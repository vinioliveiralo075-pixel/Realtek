#include <libkern/libkern.h>
#include <IOKit/IOService.h>

class com_vini_driver_RTL8723BE : public IOService {
    OSDeclareDefaultStructors(com_vini_driver_RTL8723BE)

public:
    virtual bool init(OSDictionary *dictionary = nullptr) override {
        if (!IOService::init(dictionary)) return false;
        IOLog("RTL8723BE: Inicializando o driver do Vini...\n");
        return true;
    }

    virtual IOService *probe(IOService *provider, SInt32 *score) override {
        IOLog("RTL8723BE: Procurando pela placa de rede...\n");
        return IOService::probe(provider, score);
    }

    virtual bool start(IOService *provider) override {
        if (!IOService::start(provider)) return false;
        IOLog("RTL8723BE: Driver carregado com sucesso!\n");
        return true;
    }

    virtual void stop(IOService *provider) override {
        IOLog("RTL8723BE: Driver descarregado.\n");
        IOService::stop(provider);
    }
};

OSDefineMetaClassAndStructors(com_vini_driver_RTL8723BE, IOService)