#include "filter_simulator.h"


// We declare an auto pointer to FilterSim.
std::auto_ptr<FilterSim> filter_sim(0);

void ISPoll(void *p);


void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(filter_sim.get() == 0) filter_sim.reset(new FilterSim());
    //IEAddTimer(POLLMS, ISPoll, NULL);

}

void ISGetProperties(const char *dev)
{
        ISInit();
        filter_sim->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        filter_sim->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        filter_sim->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        filter_sim->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}
void ISSnoopDevice (XMLEle *root)
{
    INDI_UNUSED(root);
}


FilterSim::FilterSim()
{
    //ctor
}

FilterSim::~FilterSim()
{
    //dtor
}

const char *FilterSim::getDefaultName()
{
    return (char *)"Filter Simulator";
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

bool FilterSim::SelectFilter(int f)
{
    CurrentFilter=f;
    SetTimer(500);
    return true;
}

void FilterSim::TimerHit()
{
    SelectFilterDone(CurrentFilter);
}
