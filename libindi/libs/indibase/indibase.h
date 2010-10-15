#ifndef INDIBASE_H
#define INDIBASE_H

#include "indiapi.h"
#include "indidevapi.h"

#define MAXRBUF 2048

namespace INDI
{
    class BaseMediator;
    class BaseDevice;
    class BaseClient;
    class DefaultDevice;
}

class INDI::BaseMediator
{
public:

    virtual void newDevice()  =0;
    virtual void newBLOB(IBLOB *bp) =0;
    virtual void newSwitch(ISwitchVectorProperty *svp) =0;
    virtual void newNumber(INumberVectorProperty *nvp) =0;
    virtual void newText(ITextVectorProperty *tvp) =0;
    virtual void newLight(ILightVectorProperty *lvp) =0;
};

#endif // INDIBASE_H
