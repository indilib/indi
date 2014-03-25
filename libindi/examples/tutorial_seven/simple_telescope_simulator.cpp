#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include "simple_telescope_simulator.h"
#include "indicom.h"

#include <memory>

// We declare an auto pointer to ScopeSim.
std::auto_ptr<ScopeSim> telescope_sim(0);

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
    ISInit();
    telescope_sim->ISNewBLOB (dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice (XMLEle *root)
{
    INDI_UNUSED(root);
}

// static consts initialised from static const doubles cannot be inside class definitions - weird but true.
const double ScopeSim::DEFAULT_SLEW_RATE = MICROSTEPS_PER_DEGREE * 2.0;

// Private methods

bool ScopeSim::Abort()
{
    if (MovementNSSP.s == IPS_BUSY)
    {
        IUResetSwitch(&MovementNSSP);
        MovementNSSP.s = IPS_IDLE;
        IDSetSwitch(&MovementNSSP, NULL);
    }

    if (MovementWESP.s == IPS_BUSY)
    {
        MovementWESP.s = IPS_IDLE;
        IUResetSwitch(&MovementWESP);
        IDSetSwitch(&MovementWESP, NULL);
    }

    if (EqNP.s == IPS_BUSY)
    {
        EqNP.s = IPS_IDLE;
        IDSetNumber(&EqNP, NULL);
    }

    TrackState = SCOPE_IDLE;

    AxisStatusRA = AxisStatusDEC = STOPPED; // This marvelous inertia free scope can be stopped instantly!

    AbortSP.s      = IPS_OK;
    IUResetSwitch(&AbortSP);
    IDSetSwitch(&AbortSP, NULL);
    DEBUG(INDI::Logger::DBG_SESSION, "Telescope aborted.");

    return true;
}

bool ScopeSim::canSync()
{
    return true;
}

bool ScopeSim::Connect()
{
    return true;
}

bool ScopeSim::Disconnect()
{
    return true;
}

const char * ScopeSim::getDefaultName()
{
    return (char *)"Simple Telescope Simulator";
}

bool ScopeSim::Goto(double r,double d)
{
/*
    //IDLog("ScopeSim Goto\n");
    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    Parked=false;
    TrackState = SCOPE_SLEWING;

    EqNP.s    = IPS_BUSY;

    DEBUGF(INDI::Logger::DBG_SESSION,"Slewing to RA: %s - DEC: %s", RAStr, DecStr);
*/
    return true;
}

bool ScopeSim::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    // Let's simulate it to be an F/10 8" telescope
    ScopeParametersN[0].value = 203;
    ScopeParametersN[1].value = 2000;
    ScopeParametersN[2].value = 203;
    ScopeParametersN[3].value = 2000;

    TrackState=SCOPE_IDLE;

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();

    // Add alignment properties
    InitProperties(this);

    return true;
}

bool ScopeSim::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // Process alignment properties
        ProcessBlobProperties(this, name, sizes, blobsizes, blobs, formats, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool ScopeSim::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device

    if(strcmp(dev,getDeviceName())==0)
    {
        // Process alignment properties
        ProcessNumberProperties(this, name, values, names, n);

    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
}

bool ScopeSim::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // Process alignment properties
        ProcessSwitchProperties(this, name, states, names, n);
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewSwitch(dev,name,states,names,n);
}

bool ScopeSim::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // Process alignment properties
        ProcessTextProperties(this, name, texts, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
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
            IUResetSwitch(&MovementNSSP);
            MovementNSSP.s = IPS_IDLE;
            IDSetSwitch(&MovementNSSP, NULL);
        }
        break;

    case MOTION_SOUTH:
      if (last_motion != MOTION_SOUTH)
          last_motion = MOTION_SOUTH;
      else
      {
          IUResetSwitch(&MovementNSSP);
          MovementNSSP.s = IPS_IDLE;
          IDSetSwitch(&MovementNSSP, NULL);
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
            IUResetSwitch(&MovementWESP);
            MovementWESP.s = IPS_IDLE;
            IDSetSwitch(&MovementWESP, NULL);
        }
        break;

    case MOTION_EAST:
      if (last_motion != MOTION_EAST)
          last_motion = MOTION_EAST;
      else
      {
          IUResetSwitch(&MovementWESP);
          MovementWESP.s = IPS_IDLE;
          IDSetSwitch(&MovementWESP, NULL);
      }
      break;
    }

    return true;

}

bool ScopeSim::ReadScopeStatus()
{
/*

    DEBUGF(DBG_SCOPE, "Current RA: %s Current DEC: %s", RAStr, DecStr);

    NewRaDec(currentRA, currentDEC);
*/
    return true;
}

bool ScopeSim::Sync(double ra, double dec)
{
/*
    // For the purpose of showing how to integrate the Alignment Subsystem
    // I treat the values of the property EQUATORIAL_EOD_COORD as being in
    // the celestial reference frame and evrything else as being in the telescope
    // reference frame
    currentRA  = ra;
    currentDEC = dec;

    DEBUG(INDI::Logger::DBG_SESSION,"Sync is successful.");

    TrackState = SCOPE_IDLE;
    EqNP.s    = IPS_OK;


    NewRaDec(currentRA, currentDEC);
*/
    return true;
}

void ScopeSim::TimerHit()
{
    // Simulate mount movement

    static struct timeval ltv; // previous system time
    struct timeval tv; // new system time
    double dt; // Elapsed time in seconds since last tick


    gettimeofday (&tv, NULL);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
    ltv = tv;


    // RA axis
    int SlewSteps = dt * AxisSlewRateRA;

    DEBUGF(DBG_SCOPE, "TimerHit - RA Current Encoder %d SlewSteps %d Direction %d Target %d Status %d",
                        CurrentEncoderMicrostepsRA, SlewSteps, AxisDirectionRA, GotoTargetMicrostepsRA, AxisStatusRA);

    switch(AxisStatusRA)
    {
        case STOPPED:
            // Do nothing
            break;

        case SLEWING:
        {
            // Update the encoder
            SlewSteps = SlewSteps % MICROSTEPS_PER_REVOLUTION; // Just in case ;-)
            if (FORWARD == AxisDirectionRA)
                CurrentEncoderMicrostepsRA += SlewSteps;
            else
                CurrentEncoderMicrostepsRA -= SlewSteps;
            if (CurrentEncoderMicrostepsRA < 0)
                CurrentEncoderMicrostepsRA += MICROSTEPS_PER_REVOLUTION;
            else if (CurrentEncoderMicrostepsRA >= MICROSTEPS_PER_REVOLUTION)
                CurrentEncoderMicrostepsRA -= MICROSTEPS_PER_REVOLUTION;
            break;
        }

        case SLEWING_TO:
        {
            // Calculate steps to target
            int StepsToTarget;
            if (FORWARD == AxisDirectionRA)
            {
                if (CurrentEncoderMicrostepsRA <= GotoTargetMicrostepsRA)
                    StepsToTarget = GotoTargetMicrostepsRA - CurrentEncoderMicrostepsRA;
                else
                    StepsToTarget = CurrentEncoderMicrostepsRA - GotoTargetMicrostepsRA;
            }
            else
            {
                // Axis in reverse
                if (CurrentEncoderMicrostepsRA >= GotoTargetMicrostepsRA)
                    StepsToTarget = CurrentEncoderMicrostepsRA - GotoTargetMicrostepsRA;
                else
                    StepsToTarget = GotoTargetMicrostepsRA - CurrentEncoderMicrostepsRA;
            }
            if (StepsToTarget <= SlewSteps)
            {
                // Target was hit this tick
                AxisStatusRA = STOPPED;
                CurrentEncoderMicrostepsRA = GotoTargetMicrostepsRA;
            }
            else
            {
                if (FORWARD == AxisDirectionRA)
                    CurrentEncoderMicrostepsRA += SlewSteps;
                else
                    CurrentEncoderMicrostepsRA -= SlewSteps;
                if (CurrentEncoderMicrostepsRA < 0)
                    CurrentEncoderMicrostepsRA += MICROSTEPS_PER_REVOLUTION;
                else if (CurrentEncoderMicrostepsRA >= MICROSTEPS_PER_REVOLUTION)
                    CurrentEncoderMicrostepsRA -= MICROSTEPS_PER_REVOLUTION;
            }
        }
    }

    DEBUGF(DBG_SCOPE, "TimerHit - RA New Encoder %d New Status %d",  CurrentEncoderMicrostepsRA, AxisStatusRA);

    // DEC axis
    SlewSteps = dt * AxisSlewRateDEC;

    DEBUGF(DBG_SCOPE, "TimerHit - DEC Current Encoder %d SlewSteps %d Direction %d Target %d Status %d",
                        CurrentEncoderMicrostepsDEC, SlewSteps, AxisDirectionDEC, GotoTargetMicrostepsDEC, AxisStatusDEC);

    switch(AxisStatusDEC)
    {
        case STOPPED:
            // Do nothing
            break;

        case SLEWING:
        {
            // Update the encoder
            SlewSteps = SlewSteps % MICROSTEPS_PER_REVOLUTION; // Just in case ;-)
            if (FORWARD == AxisDirectionDEC)
                CurrentEncoderMicrostepsDEC += SlewSteps;
            else
                CurrentEncoderMicrostepsDEC -= SlewSteps;
            if (CurrentEncoderMicrostepsDEC < 0)
                CurrentEncoderMicrostepsDEC += MICROSTEPS_PER_REVOLUTION;
            else if (CurrentEncoderMicrostepsDEC >= MICROSTEPS_PER_REVOLUTION)
                CurrentEncoderMicrostepsDEC -= MICROSTEPS_PER_REVOLUTION;
            break;
        }

        case SLEWING_TO:
        {
            // Calculate steps to target
            int StepsToTarget;
            if (FORWARD == AxisDirectionDEC)
            {
                if (CurrentEncoderMicrostepsDEC <= GotoTargetMicrostepsDEC)
                    StepsToTarget = GotoTargetMicrostepsDEC - CurrentEncoderMicrostepsDEC;
                else
                    StepsToTarget = CurrentEncoderMicrostepsDEC - GotoTargetMicrostepsDEC;
            }
            else
            {
                // Axis in reverse
                if (CurrentEncoderMicrostepsDEC >= GotoTargetMicrostepsDEC)
                    StepsToTarget = CurrentEncoderMicrostepsDEC - GotoTargetMicrostepsDEC;
                else
                    StepsToTarget = GotoTargetMicrostepsDEC - CurrentEncoderMicrostepsDEC;
            }
            if (StepsToTarget <= SlewSteps)
            {
                // Target was hit this tick
                AxisStatusDEC = STOPPED;
                CurrentEncoderMicrostepsDEC = GotoTargetMicrostepsDEC;
            }
            else
            {
                if (FORWARD == AxisDirectionDEC)
                    CurrentEncoderMicrostepsDEC += SlewSteps;
                else
                    CurrentEncoderMicrostepsDEC -= SlewSteps;
                if (CurrentEncoderMicrostepsDEC < 0)
                    CurrentEncoderMicrostepsDEC += MICROSTEPS_PER_REVOLUTION;
                else if (CurrentEncoderMicrostepsDEC >= MICROSTEPS_PER_REVOLUTION)
                    CurrentEncoderMicrostepsDEC -= MICROSTEPS_PER_REVOLUTION;
            }
        }
    }

    DEBUGF(DBG_SCOPE, "TimerHit - DEC New Encoder %d New Status %d",  CurrentEncoderMicrostepsDEC, AxisStatusDEC);

    INDI::Telescope::TimerHit(); // This will call ReadScopeStatus
}

