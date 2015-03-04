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
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISSnoopDevice(XMLEle *root);

    static void joystickHelper(const char * joystick_n, double mag, double angle, void *context);
    static void buttonHelper(const char * button_n, ISState state, void *context);

    protected:

    virtual bool MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand command);
    virtual bool MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand command);
    virtual bool Abort();

    virtual bool GuideNorth(float ms);
    virtual bool GuideSouth(float ms);
    virtual bool GuideEast(float ms);
    virtual bool GuideWest(float ms);
    virtual bool updateLocation(double latitude, double longitude, double elevation);

    bool Goto(double,double);
    bool Park();
    bool UnPark();
    bool Sync(double ra, double dec);

    virtual void processNSWE(double mag, double angle);
    virtual void processJoystick(const char * joystick_n, double mag, double angle);
    virtual void processButton(const char * button_n, ISState state);

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
    bool Parked;

    double guiderEWTarget[2];
    double guiderNSTarget[2];

    ISwitchVectorProperty SlewModeSP;
    ISwitch SlewModeS[4];

    INumber GuideRateN[2];
    INumberVectorProperty GuideRateNP;

    INumberVectorProperty EqPENV;
    INumber EqPEN[2];

    ISwitch PEErrNSS[2];
    ISwitchVectorProperty PEErrNSSP;

    ISwitch PEErrWES[2];
    ISwitchVectorProperty PEErrWESP;

    INDI::Controller *controller;

};

#endif // SCOPESIM_H
