#ifndef RTL8723BE_H
#define RTL8723BE_H

#include <libkern/libkern.h>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOMemoryDescriptor.h> // Substitui o IOMemoryMap.h que não existia separado
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>

class RTL8723BE : public IOService {
    OSDeclareDefaultStructors(RTL8723BE)
    
private:
    IOPCIDevice* pciDevice;
    IOMemoryMap* memoryMap; // Continua funcionando perfeitamente aqui
    volatile uint8_t* mmioBase;
    uint32_t                  subsystemID;

    // Sincronização e Interrupções (IRQ)
    IOWorkLoop* workLoop;
    IOInterruptEventSource* interruptSource;

    // Estados de Configuração (Painel Avançado)
    bool                     cfg80211d;
    uint32_t                 cfgBandwidth;
    bool                     cfgWakeOnMagicPacket;
    uint32_t                 cfgRoamingLevel;
    bool                     isRadioActive;

    // Métodos Internos de Hardware
    bool mapHardwareMemory();
    void unmapHardwareMemory();
    bool setupInterrupts();
    void teardownInterrupts();
    bool uploadFirmware();
    void loadAdvancedConfiguration();
    void setRadioPower(bool enable);

    // Manipulador de Interrupções (ISR)
    void handleInterrupt(IOInterruptEventSource* source, int count);

    // Helpers Primitivos de Acesso MMIO (Registradores de 8, 16 e 32 bits)
    inline void write8(uint32_t offset, uint8_t val)   { *(mmioBase + offset) = val; }
    inline void write16(uint32_t offset, uint16_t val) { *(volatile uint16_t*)(mmioBase + offset) = val; }
    inline void write32(uint32_t offset, uint32_t val) { *(volatile uint32_t*)(mmioBase + offset) = val; }
    
    inline uint8_t  read8(uint32_t offset)   { return *(mmioBase + offset); }
    inline uint16_t read16(uint32_t offset) { return *(volatile uint16_t*)(mmioBase + offset); }
    inline uint32_t read32(uint32_t offset) { return *(volatile uint32_t*)(mmioBase + offset); }

public:
    virtual bool init(OSDictionary *dictionary = nullptr) override;
    virtual void free(void) override;
    virtual IOService* probe(IOService *provider, SInt32 *score) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual IOWorkLoop* getWorkLoop() const override;
};

#endif
