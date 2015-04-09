#ifndef SCOPESIM_H
#define SCOPESIM_H

#include "indibase/indiguiderinterface.h"
#include "indibase/inditelescope.h"
#include "indicontroller.h"

class ScopeSim : public INDI::Telescope, public INDI::GuiderInterface
{
public:
    ScopeSim();
    virtual ~ScopeSim();

    virtual const char *getDefaultName();
    virtual bool Connect();
    virtual bool Connect(char *);
    virtual bool Disconnect();
    virtual bool ReadScopeStatus();
    virtual bool initProperties();
    virtual void ISGetProperties (const char *dev);
    virtual bool updateProperties();

    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    protected:

    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command);
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command);
    virtual bool Abort();

    virtual IPState GuideNorth(float ms);
    virtual IPState GuideSouth(float ms);
    virtual IPState GuideEast(float ms);
    virtual IPState GuideWest(float ms);
    virtual bool updateLocation(double latitude, double longitude, double elevation);

    bool Goto(double,double);
    bool Park();
    bool UnPark();
    bool Sync(double ra, double dec);

    private:

    double range24(double r);
    double range360(double r);

    double currentRA;
    double currentDEC;
    double targetRA;
    double targetDEC;

    ln_lnlat_posn lnobserver;
    ln_hrz_posn lnaltaz;
    bool forceMeridianFlip;
    unsigned int DBG_SCOPE;

    double guiderEWTarget[2];
    double guiderNSTarget[2];

    INumber GuideRateN[2];
    INumberVectorProperty GuideRateNP;

    INumberVectorProperty EqPENV;
    INumber EqPEN[2];

    ISwitch PEErrNSS[2];
    ISwitchVectorProperty PEErrNSSP;

    ISwitch PEErrWES[2];
    ISwitchVectorProperty PEErrWESP;

};

#endif // SCOPESIM_H
