#ifndef INDIBASEDRIVER_H
#define INDIBASEDRIVER_H

#include <vector>
#include <map>
#include <string>

#include "indiapi.h"
#include "indidevapi.h"
#include "indibase.h"

#define MAXRBUF 2048

/**
 * \class INDI::BaseDriver
   \brief Class to provide basic INDI device functionality


 */
class INDI::BaseDriver
{
public:
    BaseDriver();
    virtual ~BaseDriver();

    /*! INDI error codes. */
    enum INDI_ERROR
    {
        INDI_DEVICE_NOT_FOUND=-1,       /*!< INDI Device was not found. */
        INDI_PROPERTY_INVALID=-2,       /*!< Property has an invalid syntax or attribute. */
        INDI_PROPERTY_DUPLICATED = -3,  /*!< INDI Device was not found. */
        INDI_DISPATCH_ERROR=-4          /*!< Dispatching command to driver failed. */
    };

    /** \brief INDI property type */
    enum INDI_TYPE { INDI_NUMBER, INDI_SWITCH, INDI_TEXT, INDI_LIGHT, INDI_BLOB };

    /** \return Return vector number property given its name */
    INumberVectorProperty * getNumber(const char *name);
    /** \return Return vector text property given its name */
    ITextVectorProperty * getText(const char *name);
    /** \return Return vector switch property given its name */
    ISwitchVectorProperty * getSwitch(const char *name);
    /** \return Return vector light property given its name */
    ILightVectorProperty * getLight(const char *name);
    /** \return Return vector BLOB property given its name */
    IBLOBVectorProperty * getBLOB(const char *name);

    /** \brief Remove a property
        \param name name of property to be removed
        \return 0 if successul, -1 otherwise.
    */
    int removeProperty(const char *name);

    /** \brief Return a property and its type given its name.
        \param name of property to be found.
        \param type of property found.
        \return If property is found, it is returned. To be used you must use static_cast with given the type of property
        returned.

        \note This is a low-level function and should not be called directly unless necessary. Use getXXX instead where XXX
        is the property type (Number, Text, Switch..etc).

    */
    void * getProperty(const char *name, INDI_TYPE & type);

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
        INDI_TYPE type;
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
    friend class INDI::DefaultDriver;

};

#endif // INDIBASEDRIVER_H
