#ifndef SCOPESIM_H
#define SCOPESIM_H

#include "indibase/inditelescope.h"


class ScopeSim : public INDI::Telescope
{
    protected:
    private:
        double currentRA;
        double currentDEC;
        double targetRA;
        double targetDEC;

        bool Parked;

        INumber GuideNSN[2];
        INumberVectorProperty *GuideNSNP;


        INumber GuideWEN[2];
        INumberVectorProperty *GuideWENP;

        INumberVectorProperty *EqPECNV;
        INumber EqPECN[2];

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

        virtual bool MoveNS(TelescopeMotionNS dir);
        virtual bool MoveWE(TelescopeMotionWE dir);

        bool Goto(double,double);
        bool Park();

};

#endif // SCOPESIM_H
