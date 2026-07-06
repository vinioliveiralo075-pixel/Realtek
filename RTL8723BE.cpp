#include "RTL8723BE.h"
#include "rtl8723be_fw.h" // Firmware gerado pelo script do PowerShell

// Definições de Registradores Macropesados da Realtek
#define REG_MCU_TST          0x0080
#define REG_FW_RAM_ADDR      0x00F0
#define REG_FW_RAM_DATA      0x00F4
#define REG_MCU_CTRL         0x001C
#define REG_RF_CTRL          0x001F  // Controle de RF / Rádio
#define REG_CR               0x0100  // Command Register (Ativação MAC/BB/RF)
#define REG_INT_MIG          0x0304  // Mitigação e ativação de Interrupção

OSDefineMetaClassAndStructors(RTL8723BE, IOService)

bool RTL8723BE::init(OSDictionary *dictionary) {
    if (!IOService::init(dictionary)) return false;
    pciDevice = nullptr;
    memoryMap = nullptr;
    mmioBase = nullptr;
    workLoop = nullptr;
    interruptSource = nullptr;
    subsystemID = 0;
    isRadioActive = false;
    
    IOLog("RTL8723BE: Kext integrada inicializada.\n");
    return true;
}

void RTL8723BE::free(void) {
    IOService::free();
}

IOService* RTL8723BE::probe(IOService *provider, SInt32 *score) {
    return IOService::probe(provider, score);
}

void RTL8723BE::loadAdvancedConfiguration() {
    OSBoolean* boolObj = OSDynamicCast(OSBoolean, getProperty("IEEE80211d"));
    cfg80211d = boolObj ? boolObj->isTrue() : false;

    OSNumber* numObj = OSDynamicCast(OSNumber, getProperty("BandwidthMode"));
    cfgBandwidth = numObj ? numObj->unsigned32BitValue() : 20;

    boolObj = OSDynamicCast(OSBoolean, getProperty("WakeOnMagicPacket"));
    cfgWakeOnMagicPacket = boolObj ? boolObj->isTrue() : true;

    numObj = OSDynamicCast(OSNumber, getProperty("RoamingSensitivity"));
    cfgRoamingLevel = numObj ? numObj->unsigned32BitValue() : 3;

    IOLog("RTL8723BE: Configurações Avançadas Aplicadas com Sucesso.\n");
}

bool RTL8723BE::start(IOService *provider) {
    if (!IOService::start(provider)) return false;
    
    pciDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!pciDevice) return false;
    
    // Ativa Barramento PCI e comandos elétricos
    pciDevice->setMemoryEnable(true);
    pciDevice->setBusMasterEnable(true);
    
    // Diagnóstico de IDs de Hardware capturados no Boot
    uint32_t pciID = pciDevice->configRead32(0x00);
    subsystemID = pciDevice->configRead32(0x2C);
    IOLog("RTL8723BE: Vinculado ao Hardware VEN_ID: 0x%04X | DEV_ID: 0x%04X | SUBSYS: 0x%08X\n", 
          (pciID & 0xFFFF), ((pciID >> 16) & 0xFFFF), subsystemID);

    loadAdvancedConfiguration();

    // 1. Mapear os 16KB de Janela MMIO (BAR0)
    if (!mapHardwareMemory()) {
        IOLog("RTL8723BE: Falha Crítica - Mapeamento de BAR0 falhou.\n");
        return false;
    }

    // 2. Ligar Linha de Escuta de Interrupções do Processador
    if (!setupInterrupts()) {
        IOLog("RTL8723BE: Falha Crítica - Não foi possível assinar o vetor IRQ.\n");
        stop(provider);
        return false;
    }

    // 3. Efetuar a carga do Firmware do Linux
    if (!uploadFirmware()) {
        IOLog("RTL8723BE: Falha na inicialização do Microcontrolador.\n");
        stop(provider);
        return false;
    }

    // 4. LIGAR O RÁDIO (Mudar status de Inativo para Ativo no Circuito)
    setRadioPower(true);

    // Registra o Driver no ecossistema global para torná-lo visível
    registerService();
    
    IOLog("RTL8723BE: Driver completamente ativo e operacional!\n");
    return true;
}

bool RTL8723BE::mapHardwareMemory() {
    memoryMap = pciDevice->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    if (!memoryMap) return false;
    
    mmioBase = (volatile uint8_t*)memoryMap->getVirtualAddress();
    IOByteCount tamanhoBAR = memoryMap->getLength();
    
    IOLog("RTL8723BE: BAR0 mapeada em %p (%llu bytes alocados).\n", mmioBase, tamanhoBAR);
    return (mmioBase != nullptr);
}

bool RTL8723BE::setupInterrupts() {
    workLoop = IOWorkLoop::workLoop();
    if (!workLoop) return false;

    interruptSource = IOInterruptEventSource::interruptEventSource(
        this,
        OSMemberFunctionCast(IOInterruptEventAction, this, &RTL8723BE::handleInterrupt),
        pciDevice,
        0
    );

    if (!interruptSource || workLoop->addEventSource(interruptSource) != kIOReturnSuccess) {
        return false;
    }

    interruptSource->enable();
    return true;
}

void RTL8723BE::setRadioPower(bool enable) {
    if (enable) {
        IOLog("RTL8723BE: Despertando o rádio RF e habilitando MAC/BB...\n");
        
        // Comandos de baixo nível baseados no Linux para acordar o circuito de RF
        write8(REG_RF_CTRL, 0x03);  // Ativa canais RF e clock interno
        write8(REG_CR, 0x0C);      // Habilita as máquinas de Tx (Transmissão) e Rx (Recepção) do chip
        write32(REG_INT_MIG, 0x0000); // Zera atrasos de mitigação de IRQ
        
        isRadioActive = true;
        IOLog("RTL8723BE: Circuito de rádio online. Status: ATIVO.\n");
    } else {
        write8(REG_CR, 0x00);      // Desliga Tx/Rx
        write8(REG_RF_CTRL, 0x00);  // Coloca RF em modo de economia de energia (LPS)
        isRadioActive = false;
    }
}

bool RTL8723BE::uploadFirmware() {
    IOLog("RTL8723BE: Carregando matriz de firmware integrada (%d bytes)...\n", rtl8723be_fw_bin_len);
    
    if (rtl8723be_fw_bin_len == 0) return false;

    // Reset de segurança do MCU para aceitar firmware
    uint8_t mcu_tst = read8(REG_MCU_TST);
    write8(REG_MCU_TST, mcu_tst | 0x01);
    IOSleep(10);

    // Envio dos blocos de dados de 4 em 4 bytes (Escreve no barramento interno)
    for (unsigned int i = 0; i < rtl8723be_fw_bin_len; i += 4) {
        uint32_t dword = 0;
        dword |= rtl8723be_fw_bin[i];
        if (i + 1 < rtl8723be_fw_bin_len) dword |= (rtl8723be_fw_bin[i + 1] << 8);
        if (i + 2 < rtl8723be_fw_bin_len) dword |= (rtl8723be_fw_bin[i + 2] << 16);
        if (i + 3 < rtl8723be_fw_bin_len) dword |= (rtl8723be_fw_bin[i + 3] << 24);

        write32(REG_FW_RAM_ADDR, i);
        write32(REG_FW_RAM_DATA, dword);
    }

    // Comanda o boot do microcontrolador interno
    write8(REG_MCU_TST, mcu_tst & ~0x01);
    write8(REG_MCU_CTRL, 0x20);
    IOSleep(20);

    return true;
}

void RTL8723BE::handleInterrupt(IOInterruptEventSource* source, int count) {
    // Rotina executada em Ring 0 quando dados batem na antena
    // Lê e limpa registradores de interrupção para evitar loop infinito de IRQ
}

void RTL8723BE::teardownInterrupts() {
    if (interruptSource) {
        if (workLoop) {
            interruptSource->disable();
            workLoop->removeEventSource(interruptSource);
        }
        interruptSource->release();
        interruptSource = nullptr;
    }
    if (workLoop) {
        workLoop->release();
        workLoop = nullptr;
    }
}

void RTL8723BE::unmapHardwareMemory() {
    if (memoryMap) {
        memoryMap->release();
        memoryMap = nullptr;
    }
    mmioBase = nullptr;
}

void RTL8723BE::stop(IOService *provider) {
    setRadioPower(false);
    teardownInterrupts();
    unmapHardwareMemory();
    if (pciDevice) {
        pciDevice->setBusMasterEnable(false);
        pciDevice->setMemoryEnable(false);
    }
    IOService::stop(provider);
}

IOWorkLoop* RTL8723BE::getWorkLoop() const {
    return workLoop;
}