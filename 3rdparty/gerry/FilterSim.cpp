#include "FilterSim.h"


IndiDevice * _create_device()
{
    IndiDevice *wheel;
    IDLog("Create a FilterWheel Simulator\n");
    wheel=new FilterSim();
    return wheel;
}

FilterSim::FilterSim()
{
    //ctor
}

FilterSim::~FilterSim()
{
    //dtor
}

char *FilterSim::getDefaultName()
{
    return (char *)"FilterWheelSim";
}

bool FilterSim::Connect()
{
    CurrentFilter=1;
    MinFilter=1;
    MaxFilter=7;
    return true;
}

bool FilterSim::Disconnect()
{
    return true;
}

int FilterSim::SelectFilter(int f)
{
    CurrentFilter=f;
    SetTimer(500);
    return 0;
}

void FilterSim::TimerHit()
{
    SelectFilterDone(CurrentFilter);
}
