#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include "telescope_simulator.h"
#include "indicom.h"

// We declare an auto pointer to ScopeSim.
std::auto_ptr<ScopeSim> telescope_sim(0);

#define	SLEWRATE	1				/* slew rate, degrees/s */
#define	POLLMS		250				/* poll period, ms */
#define SIDRATE		0.004178			/* sidereal rate, degrees/s */


void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(telescope_sim.get() == 0) telescope_sim.reset(new ScopeSim());
    //IEAddTimer(POLLMS, ISPoll, NULL);

}

void ISGetProperties(const char *dev)
{
        ISInit();
        telescope_sim->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        telescope_sim->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        telescope_sim->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        telescope_sim->ISNewNumber(dev, name, values, names, num);
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


ScopeSim::ScopeSim()
{
    //ctor
    currentRA=15;
    currentDEC=15;
    Parked=false;
}

ScopeSim::~ScopeSim()
{
    //dtor
}

const char * ScopeSim::getDefaultName()
{
    return (char *)"Telescope Simulator";
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
    static struct timeval ltv;
    struct timeval tv;
    double dt, da, dx;
    int nlocked;


    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday (&tv, NULL);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
    ltv = tv;
    da = SLEWRATE*dt;

    /* Process per current state. We check the state of EQUATORIAL_EOD_COORDS_REQUEST and act acoordingly */
    switch (TrackState)
    {
    case SCOPE_SLEWING:
    case SCOPE_PARKING:
        /* slewing - nail it when both within one pulse @ SLEWRATE */
        nlocked = 0;

        dx = targetRA - currentRA;

        if (fabs(dx) <= da)
        {
            currentRA = targetRA;
            nlocked++;
        }
        else if (dx > 0)
            currentRA += da/15.;
        else
            currentRA -= da/15.;


        dx = targetDEC - currentDEC;
        if (fabs(dx) <= da)
        {
            currentDEC = targetDEC;
            nlocked++;
        }
        else if (dx > 0)
          currentDEC += da;
        else
          currentDEC -= da;

        if (nlocked == 2)
        {
            //eqNP.s = IPS_OK;
            //eqNPR.s = IPS_OK;
            //IDSetNumber(&eqNP, NULL);
            //IDSetNumber(&eqNPR, "Now tracking");
            if (TrackState == SCOPE_SLEWING)
            {
                TrackState = SCOPE_TRACKING;
                IDMessage(deviceName(), "Telescope slew is complete. Tracking...");
            }
            else
            {
                TrackState = SCOPE_PARKED;
                IDMessage(deviceName(), "Telescope parked successfully.");
            }
        }
        //else
          //  IDSetNumber(&eqNP, NULL);

        break;

    case SCOPE_TRACKING:
        /* tracking */
        /* RA moves at sidereal, Dec stands still */
         currentRA += (SIDRATE*dt/15.);
         break;

    default:
        break;
    }

    NewRaDec(currentRA, currentDEC);
    return true;
}

bool ScopeSim::Goto(double r,double d)
{
    //IDLog("ScopeSim Goto\n");
    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    Parked=false;
    TrackState = SCOPE_SLEWING;
    IDMessage(deviceName(), "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool ScopeSim::Park()
{
    targetRA=0;
    targetDEC=90;
    Parked=true;
    TrackState = SCOPE_PARKING;
    IDMessage(deviceName(), "Parking telescope in progress...");
    return true;
}

bool ScopeSim::Connect()
{
    //  Parent class is wanting a connection
    //IDLog("INDI::Telescope calling connect with %s\n",PortT[0].text);
    bool rc=false;

    if(isConnected()) return true;

    //IDLog("Calling Connect\n");

    rc=Connect(PortT[0].text);

    if(rc)
        SetTimer(POLLMS);
    return rc;
}



/* update the "mount" over time */
void mountSim (void *p)
{

}


