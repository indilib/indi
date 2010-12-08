#ifndef SCOPESIM_H
#define SCOPESIM_H

#include "IndiTelescope.h"


class ScopeSim : public IndiTelescope
{
    protected:
    private:
        double ra;
        double dec;
        bool Parked;
    public:
        ScopeSim();
        virtual ~ScopeSim();

        virtual char *getDefaultName();
        virtual bool Connect(char *);
        virtual bool Disconnect();
        virtual bool ReadScopeStatus();

        bool Goto(double,double);
        bool Park();

};

#endif // SCOPESIM_H
