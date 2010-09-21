#ifndef INDIBASEDEVICE_H
#define INDIBASEDEVICE_H

#include <vector>
#include <map>

#include "indiapi.h"
#include "indidevapi.h"

#define MAXRBUF 2048

class INDIBaseDevice
{
public:
    INDIBaseDevice();
    virtual ~INDIBaseDevice();

    INumberVectorProperty * getNumber(const char *name);
    ITextVectorProperty * getText(const char *name);
    ISwitchVectorProperty * getSwitch(const char *name);
    ILightVectorProperty * getLight(const char *name);
    IBLOBVectorProperty * getBLOB(const char *name);

    void buildSkeleton(const char *filename);

    bool isConnected();
    void setConnected(bool status);

protected:

    virtual void ISGetProperties (const char *dev);
    virtual void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) =0;
    virtual void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) =0;
    virtual void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    char deviceName[MAXINDINAME];

    bool loadConfig(bool ignoreConnection = false);
    int buildProp(XMLEle *root, char *errmsg);
    bool saveConfig();
    bool loadDefaultConfig();
    void setDebug(bool enable);
    void setSimulation(bool enable);
    bool isDebug();
    bool isSimulation();

    int setAnyCmd (XMLEle *root, char * errmsg);
    int processBlob(IBLOB *blobEL, XMLEle *ep, char * errmsg);
    int setBLOB(IBLOBVectorProperty *pp, XMLEle * root, char * errmsg);
    //int setValue (INDI_P *pp, XMLEle *root, char * errmsg);

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

    friend class INDIBaseClient;

};

#endif // INDIBASEDEVICE_H
