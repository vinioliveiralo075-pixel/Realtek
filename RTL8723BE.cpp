#include "RTL8723BE.h"
#include <IOKit/IOLib.h>
#include "rtl8723be_fw.h" // Cabeçalho que contém a array do firmware bruto

OSDefineMetaClassAndStructors(RTL8723BE, IOService)

bool RTL8723BE::init(OSDictionary *dictionary) {
    if (!IOService::init(dictionary)) return false;
    
    pciDevice = nullptr;
    memoryMap = nullptr;
    mmioBase = nullptr;
    subsystemID = 0;
    workLoop = nullptr;
    interruptSource = nullptr;
    isRadioActive = false;
    
    return true;
}

void RTL8723BE::free(void) {
    IOService::free();
}

IOService* RTL8723BE::probe(IOService *provider, SInt32 *score) {
    if (!IOService::probe(provider, score)) return nullptr;
    return this;
}

bool RTL8723BE::start(IOService *provider) {
    if (!IOService::start(provider)) return false;

    pciDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!pciDevice) return false;

    // Ativando barramento PCI com comandos modernos do macOS Monterey/Ventura
    pciDevice->setMemoryEnable(true);
    pciDevice->setBusLeadEnable(true); 

    // Lendo IDs de hardware dos registradores PCI
    uint32_t pciID = pciDevice->configRead32(0x00);
    subsystemID = pciDevice->configRead32(0x2C);
    IOLog("RTL8723BE: Placa PCI ID: 0x%08X carregada. Subsystem ID: 0x%08X\n", pciID, subsystemID);

    // Mapeando os registradores MMIO da Realtek
    if (!mapHardwareMemory()) {
        IOLog("RTL8723BE: Erro crítico ao mapear memória MMIO.\n");
        stop(provider);
        return false;
    }

    loadAdvancedConfiguration();

    // Configurando barramento de interrupções (IRQ)
    if (!setupInterrupts()) {
        IOLog("RTL8723BE: Erro ao registrar manipulador de interrupções.\n");
        stop(provider);
        return false;
    }

    // Injetando os blocos de firmware da array rtl8723be_fw_bin
    if (!uploadFirmware()) {
        IOLog("RTL8723BE: Falha no upload do firmware para o chip.\n");
        stop(provider);
        return false;
    }

    setRadioPower(true);
    IOLog("RTL8723BE: Driver do Vini ativado com sucesso!\n");
    return true;
}

void RTL8723BE::stop(IOService *provider) {
    setRadioPower(false);
    teardownInterrupts();
    unmapHardwareMemory();

    if (pciDevice) {
        // Desligando barramento PCI com comandos atualizados
        pciDevice->setBusLeadEnable(false);
        pciDevice->setMemoryEnable(false);
    }

    IOService::stop(provider);
}

IOWorkLoop* RTL8723BE::getWorkLoop() const {
    return workLoop;
}

bool RTL8723BE::mapHardwareMemory() {
    // Localiza a BAR0 da placa PCI para comunicação direta por registradores
    memoryMap = pciDevice->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    if (!memoryMap) return false;

    mmioBase = reinterpret_cast<volatile uint8_t*>(memoryMap->getVirtualAddress());
    return (mmioBase != nullptr);
}

void RTL8723BE::unmapHardwareMemory() {
    if (memoryMap) {
        memoryMap->release();
        memoryMap = nullptr;
    }
    mmioBase = nullptr;
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

void RTL8723BE::teardownInterrupts() {
    if (interruptSource) {
        interruptSource->disable();
        if (workLoop) {
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

bool RTL8723BE::uploadFirmware() {
    IOLog("RTL8723BE: Iniciando streaming de firmware para a placa...\n");
    
    for (uint32_t i = 0; i < rtl8723be_fw_bin_len; i += 4) {
        uint32_t dword = rtl8723be_fw_bin[i];
        
        // Cast explícito via static_cast impede os warnings de sinal de bit
        if (i + 1 < rtl8723be_fw_bin_len) dword |= (static_cast<uint32_t>(rtl8723be_fw_bin[i + 1]) << 8);
        if (i + 2 < rtl8723be_fw_bin_len) dword |= (static_cast<uint32_t>(rtl8723be_fw_bin[i + 2]) << 16);
        if (i + 3 < rtl8723be_fw_bin_len) dword |= (static_cast<uint32_t>(rtl8723be_fw_bin[i + 3]) << 24);
        
        // Desloca o firmware escrevendo nos blocos de registradores MMIO
        write32(0x1F80 + (i / 4) * 4, dword);
    }
    
    return true;
}

void RTL8723BE::loadAdvancedConfiguration() {
    cfg80211d = true;
    cfgBandwidth = 40; 
    cfgWakeOnMagicPacket = false;
    cfgRoamingLevel = 3;
}

void RTL8723BE::setRadioPower(bool enable) {
    isRadioActive = enable;
    if (enable) {
        write8(0x05, 0x01); // Aciona energia nos circuitos de rádio da Realtek
    } else {
        write8(0x05, 0x00); // Coloca em modo de suspensão
    }
}

void RTL8723BE::handleInterrupt(IOInterruptEventSource* source, int count) {
    // Lê o status de interrupção PCI da Realtek
    uint32_t intStatus = read32(0x100); 
    if (intStatus) {
        write32(0x100, intStatus); // Limpa e confirma recepção da IRQ no chip
    }
}
