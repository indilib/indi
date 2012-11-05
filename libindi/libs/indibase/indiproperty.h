#ifndef INDI_INDIPROPERTY_H
#define INDI_INDIPROPERTY_H

#include "indibase.h"



namespace INDI
{

class BaseDevice;

class Property
{
public:
    Property();
    ~Property();

    void setProperty(void *);
    void setType(INDI_TYPE t);
    void setRegistered(bool r);
    void setDynamic(bool d);
    void setBaseDevice(BaseDevice *idp);

    void *getProperty() { return pPtr; }
    INDI_TYPE getType() { return pType; }
    bool getRegistered() { return pRegistered; }
    bool isDynamic() { return pDynamic; }
    BaseDevice *getBaseDevice() { return dp; }

    // Convenience Functions
    const char *getName();
    const char *getLabel();
    const char *getGroupName();
    const char *getDeviceName();
    IPState getState();
    IPerm getPermission();

    INumberVectorProperty *getNumber();
    ITextVectorProperty   *getText();
    ISwitchVectorProperty *getSwitch();
    ILightVectorProperty  *getLight();
    IBLOBVectorProperty   *getBLOB();

private:
    void *pPtr;
    BaseDevice *dp;
    INDI_TYPE pType;
    bool pRegistered;
    bool pDynamic;
};

} // namespace INDI

#endif // INDI_INDIPROPERTY_H
