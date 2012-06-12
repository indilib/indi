#ifndef INDI_INDIPROPERTY_H
#define INDI_INDIPROPERTY_H

#include "indibase.h"

namespace INDI
{

class Property
{
public:
    Property();
    ~Property();

    void setProperty(void *);
    void setType(INDI_TYPE t);
    void setRegistered(bool r);
    void setDynamic(bool d);

    void *getProperty() { return pPtr; }
    INDI_TYPE getType() { return pType; }
    bool getRegistered() { return pRegistered; }
    bool isDynamic() { return pDynamic; }

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
    INDI_TYPE pType;
    bool pRegistered;
    bool pDynamic;
};

} // namespace INDI

#endif // INDI_INDIPROPERTY_H
