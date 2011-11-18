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

#define	GOTO_RATE	2				/* slew rate, degrees/s */
#define	SLEW_RATE	0.5				/* slew rate, degrees/s */
#define FINE_SLEW_RATE  0.1                             /* slew rate, degrees/s */
#define SID_RATE	0.004178			/* sidereal rate, degrees/s */

#define GOTO_LIMIT      5                               /* Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees */
#define SLEW_LIMIT      2                               /* Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees */
#define FINE_SLEW_LIMIT 0.5                             /* Move at FINE_SLEW_RATE until distance from target is FINE_SLEW_LIMIT degrees */

#define	POLLMS		250				/* poll period, ms */

#define RA_AXIS         0
#define DEC_AXIS        1
#define GUIDE_NORTH     0
#define GUIDE_SOUTH     1
#define GUIDE_WEST      0
#define GUIDE_EAST      1



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

    GuideNSNP = new INumberVectorProperty;
    GuideWENP = new INumberVectorProperty;
    EqPECNV = new INumberVectorProperty;

    /* initialize random seed: */
      srand ( time(NULL) );
}

ScopeSim::~ScopeSim()
{
    //dtor
}

const char * ScopeSim::getDefaultName()
{
    return (char *)"Telescope Simulator";
}

bool ScopeSim::initProperties()
{
    INDI::Telescope::initProperties();

    IUFillNumber(&GuideNSN[GUIDE_NORTH], "TIMED_GUIDE_N", "North (sec)", "%g", 0, 10, 0.001, 0);
    IUFillNumber(&GuideNSN[GUIDE_SOUTH], "TIMED_GUIDE_S", "South (sec)", "%g", 0, 10, 0.001, 0);
    IUFillNumberVector(GuideNSNP, GuideNSN, 2, deviceName(), "TELESCOPE_TIMED_GUIDE_NS", "Guide North/South", MOTION_TAB, IP_RW, 0, IPS_IDLE);


    IUFillNumber(&GuideWEN[GUIDE_WEST], "TIMED_GUIDE_W", "West (sec)", "%g", 0, 10, 0.001, 0);
    IUFillNumber(&GuideWEN[GUIDE_EAST], "TIMED_GUIDE_E", "East (sec)", "%g", 0, 10, 0.001, 0);
    IUFillNumberVector(GuideWENP, GuideWEN, 2, deviceName(), "TELESCOPE_TIMED_GUIDE_WE", "Guide West/East", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&EqPECN[RA_AXIS],"RA_PEC","RA (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqPECN[DEC_AXIS],"DEC_PEC","DEC (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(EqPECNV,EqPECN,2,deviceName(),"EQUATORIAL_PEC","Periodic Error",MOTION_TAB,IP_RO,60,IPS_IDLE);


    return true;

}

void ScopeSim::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    INDI::Telescope::ISGetProperties(dev);

    if(isConnected())
    {
        defineNumber(GuideNSNP);
        defineNumber(GuideWENP);
        defineNumber(EqPECNV);
    }

    return;
}

bool ScopeSim::updateProperties()
{

    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineNumber(GuideNSNP);
        defineNumber(GuideWENP);
        defineNumber(EqPECNV);

    }
    else
    {
        deleteProperty(GuideNSNP->name);
        deleteProperty(GuideWENP->name);
        deleteProperty(EqPECNV->name);

       // initProperties();
    }



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
    double dt, da_ra, da_dec, dx;
    int nlocked, ns_guide_dir=-1, we_guide_dir=-1;


    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday (&tv, NULL);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;


    dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
    ltv = tv;

    if ( fabs(targetRA - currentRA)*15. >= GOTO_LIMIT )
        da_ra = GOTO_RATE *dt;
    else if ( fabs(targetRA - currentRA)*15. >= SLEW_LIMIT )
        da_ra = SLEW_RATE *dt;
    else
        da_ra = FINE_SLEW_RATE *dt;

    if ( fabs(targetDEC - currentDEC) >= GOTO_LIMIT )
        da_dec = GOTO_RATE *dt;
    else if ( fabs(targetDEC - currentDEC) >= SLEW_LIMIT )
        da_dec = SLEW_RATE *dt;
    else
        da_dec = FINE_SLEW_RATE *dt;


    /* Process per current state. We check the state of EQUATORIAL_EOD_COORDS_REQUEST and act acoordingly */
    switch (TrackState)
    {
    case SCOPE_IDLE:
        EqNV->s = IPS_IDLE;
        break;
    case SCOPE_SLEWING:
    case SCOPE_PARKING:
        /* slewing - nail it when both within one pulse @ SLEWRATE */
        nlocked = 0;

        dx = targetRA - currentRA;

        if (fabs(dx)*15. <= da_ra)
        {
            currentRA = targetRA;
            nlocked++;
        }
        else if (dx > 0)
            currentRA += da_ra/15.;
        else
            currentRA -= da_ra/15.;


        dx = targetDEC - currentDEC;
        if (fabs(dx) <= da_dec)
        {
            currentDEC = targetDEC;
            nlocked++;
        }
        else if (dx > 0)
          currentDEC += da_dec;
        else
          currentDEC -= da_dec;

        EqNV->s = IPS_BUSY;

        if (nlocked == 2)
        {
            if (TrackState == SCOPE_SLEWING)
            {

                // Create random PEC in RA/DEC
                EqPECN[0].value = currentRA + ( (rand()%2 == 0 ? 1 : -1) * rand() % 10)/100.;
                EqPECN[1].value = currentDEC + ( (rand()%2 == 0 ? 1 : -1) * rand() % 10)/100.;

                IDSetNumber(EqPECNV, NULL);

                TrackState = SCOPE_TRACKING;

                EqNV->s = IPS_OK;
                IDMessage(deviceName(), "Telescope slew is complete. Tracking...");
            }
            else
            {
                TrackState = SCOPE_PARKED;
                EqNV->s = IPS_IDLE;
                IDMessage(deviceName(), "Telescope parked successfully.");
            }
        }

        break;

    case SCOPE_TRACKING:
        /* tracking */

        // N/S Guide Selection
        if (GuideNSN[GUIDE_NORTH].value > 0)
            ns_guide_dir = GUIDE_NORTH;
        else if (GuideNSN[GUIDE_SOUTH].value > 0)
            ns_guide_dir = GUIDE_SOUTH;

        // WE Guide Selection
        if (GuideWEN[GUIDE_WEST].value > 0)
            we_guide_dir = GUIDE_WEST;
        else if (GuideWEN[GUIDE_EAST].value > 0)
            we_guide_dir = GUIDE_EAST;

        if (ns_guide_dir != -1)
        {
            // If time remaining is more that dt, then decrement and
          if (GuideNSN[ns_guide_dir].value >= dt)
          {
              EqPECN[DEC_AXIS].value += da_dec * (ns_guide_dir==GUIDE_NORTH ? 1 : -1);
                GuideNSN[ns_guide_dir].value -= dt;
          }
          else
          {
                EqPECN[DEC_AXIS].value += FINE_SLEW_RATE * GuideNSN[ns_guide_dir].value * (ns_guide_dir==GUIDE_NORTH ? 1 : -1);
                GuideNSN[ns_guide_dir].value = 0;
          }


            if (GuideNSN[ns_guide_dir].value <= 0)
            {
                GuideNSN[GUIDE_NORTH].value = GuideNSN[GUIDE_SOUTH].value = 0;
                GuideNSNP->s = IPS_OK;
                IDSetNumber(GuideNSNP, NULL);
            }
            else
                IDSetNumber(GuideNSNP, NULL);
          }


        if (we_guide_dir != -1)
        {
          if (GuideWEN[we_guide_dir].value >= dt)
          {
                EqPECN[RA_AXIS].value += da_ra * (we_guide_dir==GUIDE_WEST ? 1 : -1);
                GuideWEN[we_guide_dir].value -= dt;
          }
          else
          {
                EqPECN[RA_AXIS].value += FINE_SLEW_RATE * GuideWEN[we_guide_dir].value* (we_guide_dir==GUIDE_WEST ? 1 : -1);
                GuideWEN[we_guide_dir].value = 0;
          }


            if (GuideWEN[we_guide_dir].value <= 0)
            {
                GuideWEN[GUIDE_WEST].value = GuideWEN[GUIDE_EAST].value = 0;
                GuideWENP->s = IPS_OK;
                IDSetNumber(GuideWENP, NULL);
            }
            else
                IDSetNumber(GuideWENP, NULL);
          }

        if (ns_guide_dir != -1 || we_guide_dir != -1)
            IDSetNumber(EqPECNV, NULL);

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

bool ScopeSim::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device

    if(strcmp(dev,deviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
         if(strcmp(name,"TELESCOPE_TIMED_GUIDE_NS")==0)
         {

             // Unless we're in track mode, we don't obey guide commands.
             if (TrackState != SCOPE_TRACKING)
             {
                 GuideNSNP->s = IPS_IDLE;
                 IDSetNumber(GuideNSNP, NULL);
                 return true;
             }

             IUUpdateNumber(GuideNSNP, values, names, n);

           return true;
         }

         if(strcmp(name,"TELESCOPE_TIMED_GUIDE_WE")==0)
         {
             // Unless we're in track mode, we don't obey guide commands.
             if (TrackState != SCOPE_TRACKING)
             {
                 GuideWENP->s = IPS_IDLE;
                 IDSetNumber(GuideWENP, NULL);
                 return true;
             }

             IUUpdateNumber(GuideWENP, values, names, n);

           return true;
         }

    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
}
