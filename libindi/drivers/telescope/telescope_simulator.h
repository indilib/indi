#ifndef SCOPESIM_H
#define SCOPESIM_H

#include "indibase/indiguiderinterface.h"
#include "indibase/inditelescope.h"



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

    virtual bool MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand command);
    virtual bool MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand command);
    virtual bool Abort();

    virtual bool GuideNorth(float ms);
    virtual bool GuideSouth(float ms);
    virtual bool GuideEast(float ms);
    virtual bool GuideWest(float ms);

    bool Goto(double,double);
    bool Park();
    bool Sync(double ra, double dec);
    bool canSync();
    bool canPark();

    private:
        double currentRA;
        double currentDEC;
        double targetRA;
        double targetDEC;

        unsigned int DBG_SCOPE;

        bool Parked;

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
