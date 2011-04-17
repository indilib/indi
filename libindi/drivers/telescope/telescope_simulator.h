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
    public:
        ScopeSim();
        virtual ~ScopeSim();

        virtual const char *getDefaultName();
        virtual bool Connect();
        virtual bool Connect(char *);
        virtual bool Disconnect();
        virtual bool ReadScopeStatus();

        bool Goto(double,double);
        bool Park();

};

#endif // SCOPESIM_H
