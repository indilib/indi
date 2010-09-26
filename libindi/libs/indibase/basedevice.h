#ifndef INDIBASEDEVICE_H
#define INDIBASEDEVICE_H

#include <vector>
#include <map>

#include "indiapi.h"
#include "indidevapi.h"
#include "indibase.h"

#define MAXRBUF 2048

class INDI::BaseDevice
{
public:
    BaseDevice();
    virtual ~BaseDevice();

    enum { INDI_DEVICE_NOT_FOUND=-1, INDI_PROPERTY_INVALID=-2, INDI_PROPERTY_DUPLICATED = -3, INDI_DISPATCH_ERROR=-4 };

    INumberVectorProperty * getNumber(const char *name);
    ITextVectorProperty * getText(const char *name);
    ISwitchVectorProperty * getSwitch(const char *name);
    ILightVectorProperty * getLight(const char *name);
    IBLOBVectorProperty * getBLOB(const char *name);

    int removeProperty(const char *name);

    void buildSkeleton(const char *filename);

    bool isConnected();
    void setConnected(bool status);

    void setDeviceName(const char *dev);
    const char *deviceName();

    void addMessage(const char *msg);
    const char *message() { return messageQueue.c_str(); }

protected:

    virtual void ISGetProperties (const char *dev);
    virtual void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) {}
    virtual void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) {}
    virtual void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    int buildProp(XMLEle *root, char *errmsg);

    // Configuration
    bool loadConfig(bool ignoreConnection = false);
    bool saveConfig();
    bool loadDefaultConfig();

    // Simulatin & Debug
    void setDebug(bool enable);
    void setSimulation(bool enable);
    bool isDebug();
    bool isSimulation();

    // handle SetXXX commands from client
    int setValue (XMLEle *root, char * errmsg);
    int processBLOB(IBLOB *blobEL, XMLEle *ep, char * errmsg);
    void virtual BLOBReceived(unsigned char *buffer, unsigned long size, const char *format) {}
    int setBLOB(IBLOBVectorProperty *pp, XMLEle * root, char * errmsg);

    char deviceID[MAXINDINAME];

private:

    enum pType { INDI_NUMBER, INDI_SWITCH, INDI_TEXT, INDI_LIGHT, INDI_BLOB };

    typedef struct
    {
        pType type;
        void *p;
    } pOrder;

    std::vector<INumberVectorProperty *> pNumbers;
    std::vector<ITextVectorProperty *> pTexts;
    std::vector<ISwitchVectorProperty *> pSwitches;
    std::vector<ILightVectorProperty *> pLights;
    std::vector<IBLOBVectorProperty *> pBlobs;

    ISwitch DebugS[2];
    ISwitch SimulationS[2];
    ISwitch ConfigProcessS[3];

    ISwitchVectorProperty *DebugSP;
    ISwitchVectorProperty *SimulationSP;
    ISwitchVectorProperty *ConfigProcessSP;

    LilXML *lp;

    std::vector<pOrder> pAll;

    bool pDebug;
    bool pSimulation;


    std::string messageQueue;

    friend class INDI::BaseClient;

};

#endif // INDIBASEDEVICE_H
