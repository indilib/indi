#include "ScopeSim.h"

IndiDevice * _create_device()
{
    IDLog("Create a mount simulator\n");
    return new ScopeSim();
}


ScopeSim::ScopeSim()
{
    //ctor
    ra=0;
    dec=90;
    Parked=true;
}

ScopeSim::~ScopeSim()
{
    //dtor
}

char * ScopeSim::getDefaultName()
{
    return (char *)"ScopeSim";
}

bool ScopeSim::Connect(char *)
{
    return true;
}

bool ScopeSim::Disconnect()
{
    return true;
}

bool ScopeSim::ReadScopeStatus()
{
    if(Parked) TrackState=PARKED;
    else TrackState=TRACKING;
    NewRaDec(ra,dec);
    return true;
}

bool ScopeSim::Goto(double r,double d)
{
    IDLog("ScopeSim Goto\n");
    ra=r;
    dec=d;
    Parked=false;
    return true;
}

bool ScopeSim::Park()
{
    ra=0;
    dec=90;
    Parked=true;
    return true;
}
