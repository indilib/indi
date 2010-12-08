#ifndef TESTCLIENT_H
#define TESTCLIENT_H

//#include <indidevapi.h>
//#include <indicom.h>
//#include <indibase/baseclient.h>
#include <libindi/indidevapi.h>
#include <libindi/indicom.h>
#include <libindi/baseclient.h>
#include <libindi/basedriver.h>

class testclient : public INDI::BaseClient
{
    protected:
    private:
    public:
        testclient();
        virtual ~testclient();

        bool Connected;

        INDI::BaseDriver * mycam;

    virtual void newDevice(const char *device_name);
    virtual void newProperty(const char *device_name, const char *property_name);
    virtual void newBLOB(IBLOB *bp);
    virtual void newSwitch(ISwitchVectorProperty *svp);
    virtual void newNumber(INumberVectorProperty *nvp);
    virtual void newText(ITextVectorProperty *tvp);
    virtual void newLight(ILightVectorProperty *lvp);
    virtual void serverConnected();
    virtual void serverDisconnected();

    void setnumber(char *,float);

    char *PrintProperties(char *);
    int dotest();
    int gotblob;
};

#endif // TESTCLIENT_H
