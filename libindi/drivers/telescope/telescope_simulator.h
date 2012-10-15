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

    virtual bool MoveNS(TelescopeMotionNS dir);
    virtual bool MoveWE(TelescopeMotionWE dir);
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

        bool Parked;

        double guiderEWTarget[2];
        double guiderNSTarget[2];

        INumber GuideRateN[2];
        INumberVectorProperty GuideRateNP;

        INumberVectorProperty EqPECNV;
        INumber EqPECN[2];

        ISwitch PECErrNSS[2];
        ISwitchVectorProperty PECErrNSSP;

        ISwitch PECErrWES[2];
        ISwitchVectorProperty PECErrWESP;




};

#endif // SCOPESIM_H
