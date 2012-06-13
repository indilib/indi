#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include "telescope_simulator.h"
#include "indicom.h"

#include <memory>

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
    GuideRateNP = new INumberVectorProperty;

    PECErrNSSP = new ISwitchVectorProperty;
    PECErrWESP = new ISwitchVectorProperty;

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
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    /* Property for guider support. How many seconds to guide either northward or southward? */
    IUFillNumber(&GuideNSN[GUIDE_NORTH], "TIMED_GUIDE_N", "North (sec)", "%g", 0, 10, 0.001, 0);
    IUFillNumber(&GuideNSN[GUIDE_SOUTH], "TIMED_GUIDE_S", "South (sec)", "%g", 0, 10, 0.001, 0);
    IUFillNumberVector(GuideNSNP, GuideNSN, 2, getDeviceName(), "TELESCOPE_TIMED_GUIDE_NS", "Guide North/South", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    /* Property for guider support. How many seconds to guide either westward or eastward? */
    IUFillNumber(&GuideWEN[GUIDE_WEST], "TIMED_GUIDE_W", "West (sec)", "%g", 0, 10, 0.001, 0);
    IUFillNumber(&GuideWEN[GUIDE_EAST], "TIMED_GUIDE_E", "East (sec)", "%g", 0, 10, 0.001, 0);
    IUFillNumberVector(GuideWENP, GuideWEN, 2, getDeviceName(), "TELESCOPE_TIMED_GUIDE_WE", "Guide West/East", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    /* Simulated periodic error in RA, DEC */
    IUFillNumber(&EqPECN[RA_AXIS],"RA_PEC","RA (hh:mm:ss)","%010.6m",0,24,0,15.);
    IUFillNumber(&EqPECN[DEC_AXIS],"DEC_PEC","DEC (dd:mm:ss)","%010.6m",-90,90,0,15.);
    IUFillNumberVector(EqPECNV,EqPECN,2,getDeviceName(),"EQUATORIAL_PEC","Periodic Error",MOTION_TAB,IP_RO,60,IPS_IDLE);

    /* Enable client to manually add periodic error northward or southward for simulation purposes */
    IUFillSwitch(&PECErrNSS[MOTION_NORTH], "PEC_N", "North", ISS_OFF);
    IUFillSwitch(&PECErrNSS[MOTION_SOUTH], "PEC_S", "South", ISS_OFF);
    IUFillSwitchVector(PECErrNSSP, PECErrNSS, 2, getDeviceName(),"PEC_NS", "PE N/S", MOTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    /* Enable client to manually add periodic error westward or easthward for simulation purposes */
    IUFillSwitch(&PECErrWES[MOTION_WEST], "PEC_W", "West", ISS_OFF);
    IUFillSwitch(&PECErrWES[MOTION_EAST], "PEC_E", "East", ISS_OFF);
    IUFillSwitchVector(PECErrWESP, PECErrWES, 2, getDeviceName(),"PEC_WE", "PE W/E", MOTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    /* How fast do we guide compared to sideral rate */
    IUFillNumber(&GuideRateN[RA_AXIS], "GUIDE_RATE_WE", "W/E Rate", "%g", 0, 1, 0.1, 0.3);
    IUFillNumber(&GuideRateN[DEC_AXIS], "GUIDE_RATE_NS", "N/S Rate", "%g", 0, 1, 0.1, 0.3);
    IUFillNumberVector(GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();

    return true;
}

void ScopeSim::ISGetProperties (const char *dev)
{
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);

    if(isConnected())
    {
        defineNumber(GuideNSNP);
        defineNumber(GuideWENP);
        defineNumber(GuideRateNP);
        defineNumber(EqPECNV);
        defineSwitch(PECErrNSSP);
        defineSwitch(PECErrWESP);
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
        defineNumber(GuideRateNP);
        defineNumber(EqPECNV);
        defineSwitch(PECErrNSSP);
        defineSwitch(PECErrWESP);

    }
    else
    {
        deleteProperty(GuideNSNP->name);
        deleteProperty(GuideWENP->name);
        deleteProperty(EqPECNV->name);
        deleteProperty(PECErrNSSP->name);
        deleteProperty(PECErrWESP->name);
        deleteProperty(GuideRateNP->name);
    }

    return true;
}

bool ScopeSim::Connect()
{
    bool rc=false;

    if(isConnected()) return true;

    rc=Connect(PortT[0].text);

    if(rc)
        SetTimer(POLLMS);

    return rc;
}

bool ScopeSim::Connect(char *port)
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
    double dt=0, da_ra=0, da_dec=0, dx=0, dy=0, ra_guide_dt=0, dec_guide_dt=0;
    static double last_dx=0, last_dy=0;
    int nlocked, ns_guide_dir=-1, we_guide_dir=-1;
    char RA_DISP[64], DEC_DISP[64], RA_GUIDE[64], DEC_GUIDE[64], RA_PEC[64], DEC_PEC[64], RA_TARGET[64], DEC_TARGET[64];


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

    switch (MovementNSSP->s)
    {
       case IPS_BUSY:
        if (MovementNSS[MOTION_NORTH].s == ISS_ON)
            currentDEC += da_dec;
        else if (MovementNSS[MOTION_SOUTH].s == ISS_ON)
            currentDEC -= da_dec;

        NewRaDec(currentRA, currentDEC);
        return true;
        break;
    }

    switch (MovementWESP->s)
    {
        case IPS_BUSY:

        if (MovementWES[MOTION_WEST].s == ISS_ON)
            currentRA += da_ra/15.;
        else if (MovementWES[MOTION_EAST].s == ISS_ON)
            currentRA -= da_ra/15.;

        NewRaDec(currentRA, currentDEC);
        return true;
        break;

    }

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


        dy = targetDEC - currentDEC;
        if (fabs(dy) <= da_dec)
        {
            currentDEC = targetDEC;
            nlocked++;
        }
        else if (dy > 0)
          currentDEC += da_dec;
        else
          currentDEC -= da_dec;

        EqNV->s = IPS_BUSY;

        if (nlocked == 2)
        {
            if (TrackState == SCOPE_SLEWING)
            {

                // Initially no PEC in both axis.
                EqPECN[0].value = currentRA;
                EqPECN[1].value = currentDEC;

                IDSetNumber(EqPECNV, NULL);

                TrackState = SCOPE_TRACKING;

                EqNV->s = IPS_OK;
                IDMessage(getDeviceName(), "Telescope slew is complete. Tracking...");
            }
            else
            {
                TrackState = SCOPE_PARKED;
                EqNV->s = IPS_IDLE;
                IDMessage(getDeviceName(), "Telescope parked successfully.");
            }
        }

        break;

    case SCOPE_TRACKING:
        /* tracking */

        if (GuideNSN[GUIDE_NORTH].value > 0)
        {
            if (isDebug())
                IDLog("  ****** Commanded to GUIDE NORTH for %g ms *****\n", GuideNSN[GUIDE_NORTH].value*1000);
            ns_guide_dir = GUIDE_NORTH;
        }
        else if (GuideNSN[GUIDE_SOUTH].value > 0)
        {
            if (isDebug())
                IDLog("  ****** Commanded to GUIDE SOUTH for %g ms *****\n", GuideNSN[GUIDE_SOUTH].value*1000);
            ns_guide_dir = GUIDE_SOUTH;
        }

        // WE Guide Selection
        if (GuideWEN[GUIDE_WEST].value > 0)
        {
            we_guide_dir = GUIDE_WEST;
            if (isDebug())
            IDLog("  ****** Commanded to GUIDE WEST for %g ms ****** \n", GuideWEN[GUIDE_WEST].value*1000);
        }
        else if (GuideWEN[GUIDE_EAST].value > 0)
        {
            we_guide_dir = GUIDE_EAST;
            if (isDebug())
               IDLog(" ****** Commanded to GUIDE EAST for %g ms  ****** \n", GuideWEN[GUIDE_EAST].value*1000);
        }

        if (ns_guide_dir != -1)
        {

            dec_guide_dt =  SID_RATE * GuideRateN[DEC_AXIS].value * GuideNSN[ns_guide_dir].value * (ns_guide_dir==GUIDE_NORTH ? 1 : -1);

            // If time remaining is more that dt, then decrement and
          if (GuideNSN[ns_guide_dir].value >= dt)
              GuideNSN[ns_guide_dir].value -= dt;
          else              
              GuideNSN[ns_guide_dir].value = 0;

          EqPECN[DEC_AXIS].value += dec_guide_dt;

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

            ra_guide_dt = SID_RATE/15.0 * GuideRateN[RA_AXIS].value * GuideWEN[we_guide_dir].value* (we_guide_dir==GUIDE_WEST ? -1 : 1);

          if (GuideWEN[we_guide_dir].value >= dt)
                GuideWEN[we_guide_dir].value -= dt;
          else
                GuideWEN[we_guide_dir].value = 0;

          EqPECN[RA_AXIS].value += ra_guide_dt;

            if (GuideWEN[we_guide_dir].value <= 0)
            {
                GuideWEN[GUIDE_WEST].value = GuideWEN[GUIDE_EAST].value = 0;
                GuideWENP->s = IPS_OK;
                IDSetNumber(GuideWENP, NULL);
            }
            else
                IDSetNumber(GuideWENP, NULL);
          }


        //Mention the followng:
        // Current RA displacemet and direction
        // Current DEC displacement and direction
        // Amount of RA GUIDING correction and direction
        // Amount of DEC GUIDING correction and direction

        dx = EqPECN[RA_AXIS].value - targetRA;
        dy = EqPECN[DEC_AXIS].value - targetDEC;
        fs_sexa(RA_DISP, fabs(dx), 2, 3600 );
        fs_sexa(DEC_DISP, fabs(dy), 2, 3600 );

        fs_sexa(RA_GUIDE, fabs(ra_guide_dt), 2, 3600 );
        fs_sexa(DEC_GUIDE, fabs(dec_guide_dt), 2, 3600 );

        fs_sexa(RA_PEC, EqPECN[RA_AXIS].value, 2, 3600);
        fs_sexa(DEC_PEC, EqPECN[DEC_AXIS].value, 2, 3600);

        fs_sexa(RA_TARGET, targetRA, 2, 3600);
        fs_sexa(DEC_TARGET, targetDEC, 2, 3600);


        if (isDebug() && (dx!=last_dx || dy!=last_dy || ra_guide_dt || dec_guide_dt))
        {
            last_dx=dx;
            last_dy=dy;
            IDLog("#########################################\n");
            IDLog("dt is %g\n", dt);
            IDLog("RA Displacement (%c%s) %s -- %s of target RA %s\n", dx >= 0 ? '+' : '-', RA_DISP, RA_PEC,  (EqPECN[RA_AXIS].value - targetRA) > 0 ? "East" : "West", RA_TARGET);
            IDLog("DEC Displacement (%c%s) %s -- %s of target RA %s\n", dy >= 0 ? '+' : '-', DEC_DISP, DEC_PEC, (EqPECN[DEC_AXIS].value - targetDEC) > 0 ? "North" : "South", DEC_TARGET);
            IDLog("RA Guide Correction (%g) %s -- Direction %s\n", ra_guide_dt, RA_GUIDE, ra_guide_dt > 0 ? "East" : "West");
            IDLog("DEC Guide Correction (%g) %s -- Direction %s\n", dec_guide_dt, DEC_GUIDE, dec_guide_dt > 0 ? "North" : "South");
            IDLog("#########################################\n");
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

    EqReqNV->s = IPS_BUSY;
    EqNV->s    = IPS_BUSY;

    IDMessage(getDeviceName(), "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool ScopeSim::Sync(double ra, double dec)
{
    currentRA  = ra;
    currentDEC = dec;

    EqPECN[RA_AXIS].value = ra;
    EqPECN[DEC_AXIS].value = dec;
    IDSetNumber(EqPECNV, NULL);

    IDMessage(getDeviceName(), "Sync is successful.");

    TrackState = SCOPE_IDLE;
    EqReqNV->s = IPS_OK;
    EqNV->s    = IPS_OK;


    NewRaDec(currentRA, currentDEC);
}

bool ScopeSim::Park()
{
    targetRA=0;
    targetDEC=90;
    Parked=true;
    TrackState = SCOPE_PARKING;
    IDMessage(getDeviceName(), "Parking telescope in progress...");
    return true;
}

bool ScopeSim::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device

    if(strcmp(dev,getDeviceName())==0)
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

         if(strcmp(name,"GUIDE_RATE")==0)
         {
             IUUpdateNumber(GuideRateNP, values, names, n);
             GuideRateNP->s = IPS_OK;
             IDSetNumber(GuideRateNP, NULL);
             return true;
         }

    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
}

bool ScopeSim::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    //IDLog("Enter IsNewSwitch for %s\n",name);
    //for(int x=0; x<n; x++) {
    //    IDLog("Switch %s %d\n",names[x],states[x]);
    //}

    if(strcmp(dev,getDeviceName())==0)
    {
        if(strcmp(name,"PEC_NS")==0)
        {
            IUUpdateSwitch(PECErrNSSP,states,names,n);

            PECErrNSSP->s = IPS_OK;

            if (PECErrNSS[MOTION_NORTH].s == ISS_ON)
            {
                EqPECN[DEC_AXIS].value += SID_RATE * GuideRateN[DEC_AXIS].value;
                if (isDebug())
                    IDLog("$$$$$ Simulating PE in NORTH direction for value of %g $$$$$\n", SID_RATE);
            }
            else
            {
                EqPECN[DEC_AXIS].value -= SID_RATE * GuideRateN[DEC_AXIS].value;
                if (isDebug())
                    IDLog("$$$$$ Simulating PE in SOUTH direction for value of %g $$$$$\n", SID_RATE);
            }

            IUResetSwitch(PECErrNSSP);
            IDSetSwitch(PECErrNSSP, NULL);
            IDSetNumber(EqPECNV, NULL);

            return true;

        }

        if(strcmp(name,"PEC_WE")==0)
        {
            IUUpdateSwitch(PECErrWESP,states,names,n);

            PECErrWESP->s = IPS_OK;

            if (PECErrWES[MOTION_WEST].s == ISS_ON)
            {
                EqPECN[RA_AXIS].value -= SID_RATE/15. * GuideRateN[RA_AXIS].value;
                if (isDebug())
                    IDLog("$$$$$ Simulator PE in WEST direction for value of %g $$$$$$\n", SID_RATE);
            }
            else
            {
                EqPECN[RA_AXIS].value += SID_RATE/15. * GuideRateN[RA_AXIS].value;
                if (isDebug())
                    IDLog("$$$$$$ Simulator PE in EAST direction for value of %g $$$$$$\n", SID_RATE);
            }

            IUResetSwitch(PECErrWESP);
            IDSetSwitch(PECErrWESP, NULL);
            IDSetNumber(EqPECNV, NULL);

            return true;

        }

    }

    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewSwitch(dev,name,states,names,n);
}



bool ScopeSim::MoveNS(TelescopeMotionNS dir)
{
    static int last_motion=-1;

    switch (dir)
    {
      case MOTION_NORTH:
        if (last_motion != MOTION_NORTH)
            last_motion = MOTION_NORTH;
        else
        {
            IUResetSwitch(MovementNSSP);
            MovementNSSP->s = IPS_IDLE;
            IDSetSwitch(MovementNSSP, NULL);
        }
        break;

    case MOTION_SOUTH:
      if (last_motion != MOTION_SOUTH)
          last_motion = MOTION_SOUTH;
      else
      {
          IUResetSwitch(MovementNSSP);
          MovementNSSP->s = IPS_IDLE;
          IDSetSwitch(MovementNSSP, NULL);
      }
      break;
    }

    return true;
}

bool ScopeSim::MoveWE(TelescopeMotionWE dir)
{
    static int last_motion=-1;

    switch (dir)
    {
      case MOTION_WEST:
        if (last_motion != MOTION_WEST)
            last_motion = MOTION_WEST;
        else
        {
            IUResetSwitch(MovementWESP);
            MovementWESP->s = IPS_IDLE;
            IDSetSwitch(MovementWESP, NULL);
        }
        break;

    case MOTION_EAST:
      if (last_motion != MOTION_EAST)
          last_motion = MOTION_EAST;
      else
      {
          IUResetSwitch(MovementWESP);
          MovementWESP->s = IPS_IDLE;
          IDSetSwitch(MovementWESP, NULL);
      }
      break;
    }

    return true;

}

