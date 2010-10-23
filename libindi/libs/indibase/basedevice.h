#ifndef INDIBASEDEVICE_H
#define INDIBASEDEVICE_H

#include <vector>
#include <map>
#include <string>

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
    enum pType { INDI_NUMBER, INDI_SWITCH, INDI_TEXT, INDI_LIGHT, INDI_BLOB };

    INumberVectorProperty * getNumber(const char *name);
    ITextVectorProperty * getText(const char *name);
    ISwitchVectorProperty * getSwitch(const char *name);
    ILightVectorProperty * getLight(const char *name);
    IBLOBVectorProperty * getBLOB(const char *name);

    int removeProperty(const char *name);

    void * getProperty(const char *name, pType & type);

    void buildSkeleton(const char *filename);

    bool isConnected();
    virtual void setConnected(bool status);

    void setDeviceName(const char *dev);
    const char *deviceName();

    void addMessage(const char *msg);
    const char *message() { return messageQueue.c_str(); }

    void setMediator(INDI::BaseMediator *med) { mediator = med; }
    INDI::BaseMediator * getMediator() { return mediator; }

protected:

    int buildProp(XMLEle *root, char *errmsg);

    // handle SetXXX commands from client
    int setValue (XMLEle *root, char * errmsg);
    int processBLOB(IBLOB *blobEL, XMLEle *ep, char * errmsg);
    virtual void newBLOB(unsigned char *buffer, unsigned long size, const char *format) {}
    int setBLOB(IBLOBVectorProperty *pp, XMLEle * root, char * errmsg);

    char deviceID[MAXINDINAME];

private:

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

    LilXML *lp;

    std::vector<pOrder> pAll;

    std::string messageQueue;

    INDI::BaseMediator *mediator;

    friend class INDI::BaseClient;
    friend class INDI::DefaultDevice;

};

#endif // INDIBASEDEVICE_H
