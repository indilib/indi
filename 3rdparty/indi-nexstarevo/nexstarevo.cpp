#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include "nexstarevo.h"
#include <indicom.h>

#include "NexStarAUXScope.h"

#include <memory>

using namespace INDI::AlignmentSubsystem;

#define POLLMS 1000 // Default timer tick
#define MAX_SLEW_RATE           9
#define FIND_SLEW_RATE          7
#define CENTERING_SLEW_RATE     3
#define GUIDE_SLEW_RATE         2

// We declare an auto pointer to NexStarEvo.
std::unique_ptr<NexStarEvo> telescope_nse(new NexStarEvo());

void ISGetProperties(const char *dev)
{
    telescope_nse->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    telescope_nse->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    telescope_nse->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    telescope_nse->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    telescope_nse->ISNewBLOB (dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice (XMLEle *root)
{
    telescope_nse->ISSnoopDevice(root);
}

// One definition rule (ODR) constants
// AUX commands use 24bit integer as a representation of angle in units of 
// fractional revolutions. Thus 2^24 steps makes full revolution.
const long NexStarEvo::STEPS_PER_REVOLUTION = 16777216; 
const double NexStarEvo::STEPS_PER_DEGREE = STEPS_PER_REVOLUTION / 360.0;
const double NexStarEvo::DEFAULT_SLEW_RATE = STEPS_PER_DEGREE * 2.0;
const long NexStarEvo::MAX_ALT = 90.0 * STEPS_PER_DEGREE;
const long NexStarEvo::MIN_ALT = -90.0 * STEPS_PER_DEGREE;
const double NexStarEvo::TRACK_SCALE = 80.0/60;

NexStarEvo::NexStarEvo() : 
    AxisStatusAZ(STOPPED), AxisDirectionAZ(FORWARD),
    AxisSlewRateAZ(DEFAULT_SLEW_RATE), CurrentAZ(0),
    AxisStatusALT(STOPPED), AxisDirectionALT(FORWARD),
    AxisSlewRateALT(DEFAULT_SLEW_RATE), CurrentALT(0),
    ScopeStatus(IDLE),
    PreviousNSMotion(PREVIOUS_NS_MOTION_UNKNOWN),
    PreviousWEMotion(PREVIOUS_WE_MOTION_UNKNOWN),
    TraceThisTickCount(0),
    TraceThisTick(false),
    DBG_NSEVO(INDI::Logger::getInstance().addDebugLevel("NexStar Evo Verbose", "NSEVO"))
{
    scope = NULL;

    SetTelescopeCapability( TELESCOPE_CAN_PARK | 
                            TELESCOPE_CAN_SYNC | 
                            TELESCOPE_CAN_ABORT |
                            TELESCOPE_HAS_TIME |
                            TELESCOPE_HAS_LOCATION, 4);
    // Approach from the top left 2deg away
    ApproachALT=1.0*STEPS_PER_DEGREE;
    ApproachAZ=-1.0*STEPS_PER_DEGREE;
}

NexStarEvo::~NexStarEvo()
{

}

// Private methods

bool NexStarEvo::Abort()
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

    AxisStatusAZ = AxisStatusALT = STOPPED; 
    ScopeStatus = IDLE ;
    scope->Abort();
    AbortSP.s      = IPS_OK;
    IUResetSwitch(&AbortSP);
    IDSetSwitch(&AbortSP, NULL);
    DEBUG(INDI::Logger::DBG_SESSION, "Telescope aborted.");

    return true;
}

bool NexStarEvo::Connect()
{
    SetTimer(POLLMS);
    if (scope == NULL)
        scope = new NexStarAUXScope(IPAddressT->text,IPPortN->value);
    if (scope != NULL) {
        scope->Connect();
    }
    return true;
}

bool NexStarEvo::Disconnect()
{
    if (scope != NULL) {
        scope->Disconnect();
    }
    return true;
}

const char * NexStarEvo::getDefaultName()
{
    return (char *)"NexStar Evolution";
}


bool NexStarEvo::Park()
{
    // Park at the southern horizon 
    // This is a designated by celestron parking position
    Abort();
    scope->GoToFast(long(0),long(0),false);
    TrackState = SCOPE_PARKING;
    ParkSP.s=IPS_BUSY;
    IDSetSwitch(&ParkSP, NULL);
    DEBUG(DBG_NSEVO, "Telescope park in progress...");
    
    return true;
}

bool NexStarEvo::UnPark()
{
    SetParked(false);
    return true;
}


// TODO: Make adjustment for the approx time it takes to slew to the given pos.
bool NexStarEvo::Goto(double ra,double dec)
{

    DEBUGF(DBG_NSEVO, "Goto - Celestial reference frame target right ascension %lf(%lf) declination %lf", ra * 360.0 / 24.0, ra, dec);
    if (ISS_ON == IUFindSwitch(&CoordSP,"TRACK")->s)
    {
        char RAStr[32], DecStr[32];
        fs_sexa(RAStr, ra, 2, 3600);
        fs_sexa(DecStr, dec, 2, 3600);
        CurrentTrackingTarget.ra = ra;
        CurrentTrackingTarget.dec = dec;
        NewTrackingTarget = CurrentTrackingTarget;
        DEBUG(DBG_NSEVO, "Goto - tracking requested");
    }
    
    GoToTarget.ra=ra;
    GoToTarget.dec=dec;

    // Call the alignment subsystem to translate the celestial reference frame coordinate
    // into a telescope reference frame coordinate
    TelescopeDirectionVector TDV;
    ln_hrz_posn AltAz;
    if (TransformCelestialToTelescope(ra, dec, 0.0, TDV))
    {
        // The alignment subsystem has successfully transformed my coordinate
        AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
    }
    else
    {
        // The alignment subsystem cannot transform the coordinate.
        // Try some simple rotations using the stored observatory position if any
        bool HavePosition = false;
        ln_lnlat_posn Position;
        if ((NULL != IUFindNumber(&LocationNP, "LAT")) && ( 0 != IUFindNumber(&LocationNP, "LAT")->value)
            && (NULL != IUFindNumber(&LocationNP, "LONG")) && ( 0 != IUFindNumber(&LocationNP, "LONG")->value))
        {
            // I assume that being on the equator and exactly on the prime meridian is unlikely
            Position.lat = IUFindNumber(&LocationNP, "LAT")->value;
            Position.lng = IUFindNumber(&LocationNP, "LONG")->value;
            HavePosition = true;
        }
        struct ln_equ_posn EquatorialCoordinates;
        // libnova works in decimal degrees
        EquatorialCoordinates.ra = ra * 360.0 / 24.0;
        EquatorialCoordinates.dec = dec;
        if (HavePosition)
        {
            ln_get_hrz_from_equ(&EquatorialCoordinates, &Position, ln_get_julian_from_sys(), &AltAz);
            TDV = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
            switch (GetApproximateMountAlignment())
            {
                case ZENITH:
                    break;

                case NORTH_CELESTIAL_POLE:
                    // Rotate the TDV coordinate system clockwise (negative) around the y axis by 90 minus
                    // the (positive)observatory latitude. The vector itself is rotated anticlockwise
                    TDV.RotateAroundY(Position.lat - 90.0);
                    break;

                case SOUTH_CELESTIAL_POLE:
                    // Rotate the TDV coordinate system anticlockwise (positive) around the y axis by 90 plus
                    // the (negative)observatory latitude. The vector itself is rotated clockwise
                    TDV.RotateAroundY(Position.lat + 90.0);
                    break;
            }
            AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
        }
        else
        {
            // The best I can do is just do a direct conversion to Alt/Az
            TDV = TelescopeDirectionVectorFromEquatorialCoordinates(EquatorialCoordinates);
            AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
        }
    }

    if (ScopeStatus != APPROACH){ 
        // The scope is not in slow approach mode - target should be modified
        // for precission approach.
        // TODO: This is simplistic - it should be modified close to the zenith
        AltAz.alt+=ApproachALT/STEPS_PER_DEGREE;
        AltAz.az+=ApproachAZ/STEPS_PER_DEGREE;
    }

    if (AltAz.az < 0.0)
    {
        // Calculated azimuth may be <0 - translate to 0-360 range
        // of the encoder in the mount
        AltAz.az += 360.0;
    }


    // My altitude encoder runs -90 to +90 there is no point going outside.
    if (AltAz.alt > 90.0) AltAz.alt=90.0 ;
    if (AltAz.alt < -90.0) AltAz.alt=-90.0 ;

    DEBUGF(DBG_NSEVO, "Goto - Scope reference frame target altitude %lf azimuth %lf", AltAz.alt, AltAz.az);

    TrackState = SCOPE_SLEWING;
    if (ScopeStatus == APPROACH) {
        // We need to make a slow slew to approach the final position
        ScopeStatus = SLEWING_SLOW;
        scope->GoToSlow(long(AltAz.alt * STEPS_PER_DEGREE),
                        long(AltAz.az * STEPS_PER_DEGREE),
                        ISS_ON == IUFindSwitch(&CoordSP,"TRACK")->s);
    } else {
        // Just make a standard fast slew
        ScopeStatus = SLEWING_FAST;
        scope->GoToFast(long(AltAz.alt * STEPS_PER_DEGREE),
                        long(AltAz.az * STEPS_PER_DEGREE),
                        ISS_ON == IUFindSwitch(&CoordSP,"TRACK")->s);    
    }

    EqNP.s    = IPS_BUSY;

    return true;
}

bool NexStarEvo::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    IUFillSwitch(&SlewRateS[SLEW_GUIDE], "SLEW_GUIDE", "Guide", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_CENTERING], "SLEW_CENTERING", "Centering", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_FIND], "SLEW_FIND", "Find", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_MAX], "SLEW_MAX", "Max", ISS_ON);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 4, getDeviceName(), "SLEWMODE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    TrackState=SCOPE_IDLE;

    // Build the UI for the scope
    IUFillText(&IPAddressT[0],"ADDRESS","IP address", NSEVO_DEFAULT_IP);
    IUFillTextVector(&IPAddressTP,IPAddressT,1,getDeviceName(),"DEVICE_IP_ADDRESS","IP address",OPTIONS_TAB,IP_RW,60,IPS_IDLE);
    
    IUFillNumber(&IPPortN[0],"PORT","IP port","%g",1,65535,1, NSEVO_DEFAULT_PORT);
    IUFillNumberVector(&IPPortNP,IPPortN,1,getDeviceName(),"DEVICE_IP_PORT","IP port",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();

    // Add alignment properties
    InitAlignmentProperties(this);

    return true;
}

bool NexStarEvo::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);
    SaveAlignmentConfigProperties(fp);
    IUSaveConfigText(fp, &IPAddressTP);
    IUSaveConfigNumber(fp, &IPPortNP);
    
    return true;
}


void NexStarEvo::ISGetProperties (const char *dev)
{
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);

    // We need to define this before connection
    defineText(&IPAddressTP);
    defineNumber(&IPPortNP);

    loadConfig(true, "DEVICE_IP_ADDRESS");
    loadConfig(true, "DEVICE_IP_PORT");
    
    if(isConnected())
    {
    }

    return;
}


bool NexStarEvo::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // Process alignment properties
        ProcessAlignmentBLOBProperties(this, name, sizes, blobsizes, blobs, formats, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool NexStarEvo::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device

    if(strcmp(dev,getDeviceName())==0)
    {
        // Process alignment properties
        ProcessAlignmentNumberProperties(this, name, values, names, n);

    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
}

bool NexStarEvo::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // Slew mode
        if (!strcmp (name, SlewRateSP.name))
        {

          if (IUUpdateSwitch(&SlewRateSP, states, names, n) < 0)
              return false;

          SlewRateSP.s = IPS_OK;
          IDSetSwitch(&SlewRateSP, NULL);
          return true;
        }

        // Process alignment properties
        ProcessAlignmentSwitchProperties(this, name, states, names, n);
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewSwitch(dev,name,states,names,n);
}

bool NexStarEvo::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // Process IP address
        if(!strcmp(name,IPAddressTP.name))
        {
            //  This is IP of the scope, so, lets process it
            int rc;
            IPAddressTP.s=IPS_OK;
            rc=IUUpdateText(&IPAddressTP,texts,names,n);
            //  Update client display
            IDSetText(&IPAddressTP,NULL);
            //  We processed this one, so, tell the world we did it
            return true;
        }
        // Process alignment properties
        ProcessAlignmentTextProperties(this, name, texts, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool NexStarEvo::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    int rate=IUFindOnSwitchIndex(&SlewRateSP);
    DEBUGF(DBG_NSEVO, "MoveNS dir:%d, cmd:%d, rate:%d", dir, command, rate);
    AxisDirectionALT = (dir == DIRECTION_NORTH) ? FORWARD : REVERSE ;
    AxisStatusALT = (command == MOTION_START) ? SLEWING : STOPPED ;
    ScopeStatus=SLEWING_MANUAL;
    TrackState=SCOPE_SLEWING;
    if (command == MOTION_START)
    {
        switch (rate)
        {
            case SLEW_GUIDE:
                rate  = GUIDE_SLEW_RATE;
                break;

            case SLEW_CENTERING:
                rate  = CENTERING_SLEW_RATE;
                break;

            case SLEW_FIND:
                rate  = FIND_SLEW_RATE;
                break;

            default:
                rate = MAX_SLEW_RATE;
                break;
        }
        return scope->SlewALT(((AxisDirectionALT==FORWARD)? 1 : -1)*rate);
    }
    else
        return scope->SlewALT(0);     
    
}

bool NexStarEvo::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    int rate=IUFindOnSwitchIndex(&SlewRateSP);
    DEBUGF(DBG_NSEVO, "MoveWE dir:%d, cmd:%d, rate:%d", dir, command, rate);
    AxisDirectionAZ = (dir == DIRECTION_WEST) ? FORWARD : REVERSE ;
    AxisStatusAZ = (command == MOTION_START) ? SLEWING : STOPPED ;
    ScopeStatus=SLEWING_MANUAL;
    TrackState=SCOPE_SLEWING;
    if (command == MOTION_START)
    {
        switch (rate)
        {
            case SLEW_GUIDE:
                rate  = GUIDE_SLEW_RATE;
                break;

            case SLEW_CENTERING:
                rate  = CENTERING_SLEW_RATE;
                break;

            case SLEW_FIND:
                rate  = FIND_SLEW_RATE;
                break;

            default:
                rate = MAX_SLEW_RATE;
                break;
        }
        return scope->SlewAZ(((AxisDirectionAZ==FORWARD)? -1 : 1)*rate);
    }
    else
        return scope->SlewAZ(0);     
}

bool NexStarEvo::trackingRequested()
{
    return (ISS_ON == IUFindSwitch(&CoordSP,"TRACK")->s);
}

bool NexStarEvo::ReadScopeStatus()
{
    struct ln_hrz_posn AltAz;
    double RightAscension, Declination;

    AltAz.alt = double(scope->GetALT()) / STEPS_PER_DEGREE;
    // libnova indexes Az from south while Celestron controllers index from north
    // Never mix two controllers/drivers they will never agree perfectly.
    // Furthermore the celestron hand controler resets the position encoders
    // on alignment and this will mess-up all arientation in the driver.
    // Here we are not attempting to make the driver agree with the hand
    // controller (That would involve adding 180deg here to the azimuth -
    // this way the celestron nexstar driver and this would agree in some
    // situations but not in other - better not to attepmpt impossible!).
    AltAz.az = double(scope->GetAZ()) / STEPS_PER_DEGREE;
    TelescopeDirectionVector TDV = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
    
    if (TraceThisTick)
        DEBUGF(DBG_NSEVO, "ReadScopeStatus - Alt %lf deg ; Az %lf deg", AltAz.alt, AltAz.az);
    
    
    if (!TransformTelescopeToCelestial( TDV, RightAscension, Declination))
    {
        if (TraceThisTick)
            DEBUG(DBG_NSEVO, "ReadScopeStatus - TransformTelescopeToCelestial failed");

        bool HavePosition = false;
        ln_lnlat_posn Position;
        if ((NULL != IUFindNumber(&LocationNP, "LAT")) && ( 0 != IUFindNumber(&LocationNP, "LAT")->value)
            && (NULL != IUFindNumber(&LocationNP, "LONG")) && ( 0 != IUFindNumber(&LocationNP, "LONG")->value))
        {
            // I assume that being on the equator and exactly on the prime meridian is unlikely
            Position.lat = IUFindNumber(&LocationNP, "LAT")->value;
            Position.lng = IUFindNumber(&LocationNP, "LONG")->value;
            HavePosition = true;
        }
        struct ln_equ_posn EquatorialCoordinates;
        if (HavePosition)
        {
            if (TraceThisTick)
                DEBUG(DBG_NSEVO, "ReadScopeStatus - HavePosition true");
            TelescopeDirectionVector RotatedTDV(TDV);
            switch (GetApproximateMountAlignment())
            {
                case ZENITH:
                    if (TraceThisTick)
                        DEBUG(DBG_NSEVO, "ReadScopeStatus - ApproximateMountAlignment ZENITH");
                    break;

                case NORTH_CELESTIAL_POLE:
                    if (TraceThisTick)
                        DEBUG(DBG_NSEVO, "ReadScopeStatus - ApproximateMountAlignment NORTH_CELESTIAL_POLE");
                    // Rotate the TDV coordinate system anticlockwise (positive) around the y axis by 90 minus
                    // the (positive)observatory latitude. The vector itself is rotated clockwise
                    RotatedTDV.RotateAroundY(90.0 - Position.lat);
                    AltitudeAzimuthFromTelescopeDirectionVector(RotatedTDV, AltAz);
                    break;

                case SOUTH_CELESTIAL_POLE:
                    if (TraceThisTick)
                        DEBUG(DBG_NSEVO, "ReadScopeStatus - ApproximateMountAlignment SOUTH_CELESTIAL_POLE");
                    // Rotate the TDV coordinate system clockwise (negative) around the y axis by 90 plus
                    // the (negative)observatory latitude. The vector itself is rotated anticlockwise
                    RotatedTDV.RotateAroundY(-90.0 - Position.lat);
                    AltitudeAzimuthFromTelescopeDirectionVector(RotatedTDV, AltAz);
                    break;
            }
            if (TraceThisTick)
                DEBUGF(DBG_NSEVO, "After rotations: Alt %lf deg ; Az %lf deg", AltAz.alt, AltAz.az);

            ln_get_equ_from_hrz(&AltAz, &Position, ln_get_julian_from_sys(), &EquatorialCoordinates);
        }
        else
        {
            if (TraceThisTick)
                DEBUG(DBG_NSEVO, "ReadScopeStatus - HavePosition false");

            // The best I can do is just do a direct conversion to RA/DEC
            EquatorialCoordinatesFromTelescopeDirectionVector(TDV, EquatorialCoordinates);
        }
        // libnova works in decimal degrees
        RightAscension = EquatorialCoordinates.ra * 24.0 / 360.0;
        Declination = EquatorialCoordinates.dec;
    }

    if (TraceThisTick)
        DEBUGF(DBG_NSEVO, "ReadScopeStatus - RA %lf hours DEC %lf degrees", RightAscension, Declination);
    
    // In case we are slewing while tracking update the potential target
    NewTrackingTarget.ra=RightAscension;
    NewTrackingTarget.dec=Declination;
    NewRaDec(RightAscension, Declination);

    return true;
}

bool NexStarEvo::Sync(double ra, double dec)
    {
        struct ln_hrz_posn AltAz;
        AltAz.alt = double(scope->GetALT()) / STEPS_PER_DEGREE;
        AltAz.az = double(scope->GetAZ()) / STEPS_PER_DEGREE;

        AlignmentDatabaseEntry NewEntry;
        NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
        NewEntry.RightAscension = ra;
        NewEntry.Declination = dec;
        NewEntry.TelescopeDirection = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
        NewEntry.PrivateDataSize = 0;

        DEBUGF(DBG_NSEVO, "Sync - Celestial reference frame target right ascension %lf(%lf) declination %lf", ra * 360.0 / 24.0, ra, dec);

        if (!CheckForDuplicateSyncPoint(NewEntry))
        {

            GetAlignmentDatabase().push_back(NewEntry);

            // Tell the client about size change
            UpdateSize();

            // Tell the math plugin to reinitialise
            Initialise(this);
            DEBUGF(DBG_NSEVO, "Sync - new entry added RA: %lf(%lf) DEC: %lf", ra * 360.0 / 24.0, ra, dec);
            ReadScopeStatus();
            return true;
        }
        DEBUGF(DBG_NSEVO, "Sync - duplicate entry RA: %lf(%lf) DEC: %lf", ra * 360.0 / 24.0, ra, dec);
        return false;
    }

void NexStarEvo::TimerHit()
{

    TraceThisTickCount++;
    if (60 == TraceThisTickCount)
    {
        TraceThisTick = true;
        TraceThisTickCount = 0;
    }
    // Simulate mount movement

    static struct timeval ltv; // previous system time
    struct timeval tv; // new system time
    double dt; // Elapsed time in seconds since last tick


    gettimeofday (&tv, NULL);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
    ltv = tv;

    scope->TimerTick(dt);

    INDI::Telescope::TimerHit(); // This will call ReadScopeStatus

    // OK I have updated the celestial reference frame RA/DEC in ReadScopeStatus
    // Now handle the tracking state
    switch(TrackState)
    {
        case SCOPE_PARKING:
            if (!scope->slewing()) SetParked(true);
            break;

        case SCOPE_SLEWING:
            if (scope->slewing())
                break; // The scope is still slewing
            else {   
                // The slew has finished check if that was a coarse slew
                // or precission approach
                if (ScopeStatus == SLEWING_FAST) {
                    // This was coarse slew. Execute precise approach.
                    ScopeStatus = APPROACH;
                    Goto(GoToTarget.ra, GoToTarget.dec);
                    break;
                } 
                else if (trackingRequested()) {
                    // Precise Goto or manual slew has finished. 
                    // Start tracking if requested.
                    if (ScopeStatus == SLEWING_MANUAL) {
                        // We have been slewing manually. 
                        // Update the tracking target.
                        CurrentTrackingTarget=NewTrackingTarget;
                    }
                    DEBUGF(DBG_NSEVO, "Goto finished start tracking TargetRA: %f TargetDEC: %f", 
                            CurrentTrackingTarget.ra, CurrentTrackingTarget.dec);
                    TrackState = SCOPE_TRACKING;
                    // Fall through to tracking case
                }
                else {
                    DEBUG(DBG_NSEVO, "Goto finished. No tracking requested");
                    // Precise goto or manual slew finished.
                    // No tracking requested -> go idle.
                    TrackState = SCOPE_IDLE;
                    break;
                }
            }


        case SCOPE_TRACKING:
        {
            // Continue or start tracking
            // Calculate where the mount needs to be in a minute
            // TODO may need to make this better defined
            double JulianOffset = 60.0 / (24.0 * 60 * 60); 
            TelescopeDirectionVector TDV;
            ln_hrz_posn AltAz, AAzero;
            
            /* 
            TODO 
            The tracking should take into account movement of the scope
            by the hand controller (we can hear the commands)
            and the movements made by the joystick.
            Right now when we move the scope by HC it returns to the
            designated target by corrective tracking.
            */
            if (TransformCelestialToTelescope(CurrentTrackingTarget.ra, 
                                                CurrentTrackingTarget.dec,
                                                JulianOffset, TDV)) {
                AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
                
                // Just for debugging
                TransformCelestialToTelescope(CurrentTrackingTarget.ra, 
                                                CurrentTrackingTarget.dec,
                                                0, TDV);
                AltitudeAzimuthFromTelescopeDirectionVector(TDV, AAzero);
                if (TraceThisTick)
                    DEBUGF(DBG_NSEVO, "Tracking - Calculated Alt %lf deg ; Az %lf deg", AltAz.alt, AltAz.az);
            }
            else {
                // Try a conversion with the stored observatory position if any
                bool HavePosition = false;
                ln_lnlat_posn Position;
                if ((NULL != IUFindNumber(&LocationNP, "LAT")) && ( 0 != IUFindNumber(&LocationNP, "LAT")->value)
                    && (NULL != IUFindNumber(&LocationNP, "LONG")) && ( 0 != IUFindNumber(&LocationNP, "LONG")->value))
                {
                    // I assume that being on the equator and exactly on the prime meridian is unlikely
                    Position.lat = IUFindNumber(&LocationNP, "LAT")->value;
                    Position.lng = IUFindNumber(&LocationNP, "LONG")->value;
                    HavePosition = true;
                }
                struct ln_equ_posn EquatorialCoordinates;
                // libnova works in decimal degrees
                EquatorialCoordinates.ra = CurrentTrackingTarget.ra * 360.0 / 24.0;
                EquatorialCoordinates.dec = CurrentTrackingTarget.dec;
                if (HavePosition) {
                    ln_get_hrz_from_equ(&EquatorialCoordinates, &Position,
                                            ln_get_julian_from_sys() + JulianOffset, &AltAz);
                    // Just for debugging
                    ln_get_hrz_from_equ(&EquatorialCoordinates, &Position,
                                            ln_get_julian_from_sys(), &AAzero);
                }
                else
                {
                    // No sense in tracking in this case
                    TrackState = SCOPE_IDLE;
                    break;
                }
                if (TraceThisTick)
                    DEBUGF(DBG_NSEVO, "Tracking, aligmend failed, Clculated Alt %lf deg ; Az %lf deg", AltAz.alt, AltAz.az);
            }


            if (AltAz.az < 0.0)
            {
                // DEBUG(DBG_NSEVO, "TimerHit tracking - Azimuth negative");
                // Calculated azimuth may be <0 - translate to 0-360 range
                // of the encoder in the mount
                AltAz.az += 360.0;
            }

            {
                long altRate, azRate;
                altRate=long(TRACK_SCALE*(AltAz.alt * STEPS_PER_DEGREE - scope->GetALT()));
                azRate=long(TRACK_SCALE*(AltAz.az * STEPS_PER_DEGREE - scope->GetAZ()));

                if (TraceThisTick) 
                    DEBUGF(DBG_NSEVO, "Target (AltAz): %f  %f  Scope  (AltAz)  %f  %f", 
                        AltAz.alt, 
                        AltAz.az,
                        scope->GetALT()/ STEPS_PER_DEGREE,
                        scope->GetAZ()/ STEPS_PER_DEGREE);
                
                if (abs(azRate)>TRACK_SCALE*STEPS_PER_DEGREE*180) {
                    // Crossing the meridian. AZ skips from 350+ to 0+
                    // Correct for wrap-around
                    azRate+=TRACK_SCALE*STEPS_PER_DEGREE*360;
                }
                scope->Track(altRate,azRate);

                if (TraceThisTick) DEBUGF(DBG_NSEVO, "TimerHit - Tracking AltRate %d AzRate %d ; Pos diff (deg): Alt: %f Az: %f",
                        altRate, azRate, AltAz.alt-AAzero.alt, AltAz.az- AAzero.az);
            }
            break;
        }

        default:
            break;
    }

    TraceThisTick = false;
}

bool NexStarEvo::updateLocation(double latitude, double longitude, double elevation)
{
    UpdateLocation(latitude, longitude, elevation);
    scope->UpdateLocation(latitude, longitude, elevation);
    return true;
}



