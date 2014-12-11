/*!
 * \file skywatcherAPIMount.cpp
 *
 * \author Roger James
 * \author Gerry Rozema
 * \author Jean-Luc Geehalel
 * \date 13th November 2013
 *
 * This file contains the implementation in C++ of a INDI telescope driver using the Skywatcher API.
 * It is based on work from three sources.
 * A C++ implementation of the API by Roger James.
 * The indi_eqmod driver by Jean-Luc Geehalel.
 * The synscanmount driver by Gerry Rozema.
 */

#include "skywatcherAPIMount.h"
#include "libs/indicom.h"
#include "indibase/alignment/DriverCommon.h"   // For DBG_ALIGNMENT

#include <unistd.h> // for sleep
#include <memory>
#include <cmath>
#include <limits>

using namespace INDI::AlignmentSubsystem;

// We declare an auto pointer to SkywatcherAPIMount.
std::auto_ptr<SkywatcherAPIMount> SkywatcherAPIMountPtr(0);

void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(SkywatcherAPIMountPtr.get() == 0) SkywatcherAPIMountPtr.reset(new SkywatcherAPIMount());

}

void ISGetProperties(const char *dev)
{
    ISInit();
    SkywatcherAPIMountPtr->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ISInit();
    SkywatcherAPIMountPtr->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ISInit();
    SkywatcherAPIMountPtr->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ISInit();
    SkywatcherAPIMountPtr->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    ISInit();
    SkywatcherAPIMountPtr->ISNewBLOB (dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice (XMLEle *root)
{
    INDI_UNUSED(root);
}

const char * SkywatcherAPIMount::DetailedMountInfoPage = "Detailed Mount Information";

// Constructor

SkywatcherAPIMount::SkywatcherAPIMount()
{
    // Set up the logging pointer in SkyWatcherAPI
    pChildTelescope = this;
    PreviousNSMotion = PREVIOUS_NS_MOTION_UNKNOWN;
    PreviousWEMotion = PREVIOUS_WE_MOTION_UNKNOWN;
#ifdef USE_INITIAL_JULIAN_DATE
    InitialJulianDate = ln_get_julian_from_sys();
#endif
    OldTrackingTarget[0] = 0;
    OldTrackingTarget[1] = 0;

    TelescopeCapability cap;

    cap.canPark = true;
    cap.canSync = true;
    cap.canAbort = true;
    SetTelescopeCapability(&cap);
}

// destructor

SkywatcherAPIMount::~SkywatcherAPIMount()
{
}

// Public methods

bool  SkywatcherAPIMount::Abort()
{
    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::Abort");

    TrackState = SCOPE_IDLE;

    SlowStop(AXIS1);
    SlowStop(AXIS2);

    return true;
}

bool  SkywatcherAPIMount::Connect()
{
    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::Connect");

	if (!INDI::Telescope::Connect())
		return false;

    // Tell SkywatcherAPI about the serial port
    //SetSerialPort(PortFD); Hacked in ReadScopeStatus

    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::Connect - Call InitMount");
	return InitMount();
}

const char * SkywatcherAPIMount::getDefaultName()
{
    //DEBUG(DBG_SCOPE, "SkywatcherAPIMount::getDefaultName\n");
    return "skywatcherAPIMount";
}

bool SkywatcherAPIMount::Goto(double ra,double dec)
{
    DEBUG(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "SkywatcherAPIMount::Goto");

    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "RA %lf DEC %lf", ra, dec);

    if (ISS_ON == IUFindSwitch(&CoordSP,"TRACK")->s)
    {
        char RAStr[32], DecStr[32];
        fs_sexa(RAStr, ra, 2, 3600);
        fs_sexa(DecStr, dec, 2, 3600);
        CurrentTrackingTarget.ra = ra;
        CurrentTrackingTarget.dec = dec;
        DEBUGF(INDI::Logger::DBG_SESSION, "New Tracking target RA %s DEC %s", RAStr, DecStr);
    }

    TelescopeDirectionVector TDV;
    ln_hrz_posn AltAz;
    if (TransformCelestialToTelescope(ra, dec, 0.0, TDV))
    {
        AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
        DEBUG(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Conversion OK");
    }
    else
    {
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
        EquatorialCoordinates.ra = ra * 360.0 / 24.0;
        EquatorialCoordinates.dec = dec;
        if (HavePosition)
        {
#ifdef USE_INITIAL_JULIAN_DATE
            ln_get_hrz_from_equ(&EquatorialCoordinates, &Position, InitialJulianDate, &AltAz);
#else
            ln_get_hrz_from_equ(&EquatorialCoordinates, &Position, ln_get_julian_from_sys(), &AltAz);
#endif
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
        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Conversion Failed - HavePosition %d", HavePosition);
    }
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "New Altitude %lf degrees %ld microsteps Azimuth %lf degrees %ld microsteps",
                                    AltAz.alt, DegreesToMicrosteps(AXIS2, AltAz.alt), AltAz.az, DegreesToMicrosteps(AXIS1, AltAz.az));

    // Update the current encoder positions
    GetEncoder(AXIS1);
    GetEncoder(AXIS2);

    long AltitudeOffsetMicrosteps = DegreesToMicrosteps(AXIS2, AltAz.alt) + ZeroPositionEncoders[AXIS2] - CurrentEncoders[AXIS2];
    long AzimuthOffsetMicrosteps = DegreesToMicrosteps(AXIS1, AltAz.az) + ZeroPositionEncoders[AXIS1] - CurrentEncoders[AXIS1];

    // Do I need to take out any complete revolutions before I do this test?
    if (AltitudeOffsetMicrosteps > MicrostepsPerRevolution[AXIS2] / 2)
    {
        // Going the long way round - send it the other way
        AltitudeOffsetMicrosteps -= MicrostepsPerRevolution[AXIS2];
    }

    if (AzimuthOffsetMicrosteps > MicrostepsPerRevolution[AXIS1] / 2)
    {
        // Going the long way round - send it the other way
        AzimuthOffsetMicrosteps -= MicrostepsPerRevolution[AXIS1];
    }
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Initial Axis2 %ld microsteps Axis1 %ld microsteps",
                                                    ZeroPositionEncoders[AXIS2], ZeroPositionEncoders[AXIS1]);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Current Axis2 %ld microsteps Axis1 %ld microsteps",
                                                    CurrentEncoders[AXIS2], CurrentEncoders[AXIS1]);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Altitude offset %ld microsteps Azimuth offset %ld microsteps",
                                                    AltitudeOffsetMicrosteps, AzimuthOffsetMicrosteps);

    SlewTo(AXIS1, AzimuthOffsetMicrosteps);
    SlewTo(AXIS2, AltitudeOffsetMicrosteps);

    TrackState = SCOPE_SLEWING;

    EqNP.s    = IPS_BUSY;

    return true;
}

bool SkywatcherAPIMount::initProperties()
{
    IDLog("SkywatcherAPIMount::initProperties\n");

    // Allow the base class to initialise its visible before connection properties
    INDI::Telescope::initProperties();

    // Add default properties
    addDebugControl();
    addConfigurationControl();

    // Add alignment properties
    InitProperties(this);

    // Force the alignment system to always be on
    getSwitch("ALIGNMENT_SUBSYSTEM_ACTIVE")->sp[0].s = ISS_ON;

    // Set up property variables
    IUFillNumber(&BasicMountInfo[MOTOR_CONTROL_FIRMWARE_VERSION], "MOTOR_CONTROL_FIRMWARE_VERSION",
                                                            "Motor control fimware version",
                                                            "%g",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);
    IUFillNumber(&BasicMountInfo[MOUNT_CODE], "MOUNT_CODE",
                                                            "Mount code",
                                                            "%g",
                                                            0,
                                                            0xFF,
                                                            1,
                                                            0);
    IUFillNumber(&BasicMountInfo[IS_DC_MOTOR], "IS_DC_MOTOR",
                                                            "Is DC motor (boolean)",
                                                            "%g",
                                                            0,
                                                            1,
                                                            1,
                                                            0);
    IUFillNumberVector(&BasicMountInfoV, BasicMountInfo, 3, getDeviceName(), "BASIC_MOUNT_INFO", "Basic mount information",
                        DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&MountType[MT_EQ6], "EQ6", "EQ6", ISS_OFF);
    IUFillSwitch(&MountType[MT_HEQ5], "HEQ5", "HEQ5", ISS_OFF);
    IUFillSwitch(&MountType[MT_EQ5], "EQ5", "EQ5", ISS_OFF);
    IUFillSwitch(&MountType[MT_EQ3], "EQ3", "EQ3", ISS_OFF);
    IUFillSwitch(&MountType[MT_GT], "GT", "GT", ISS_OFF);
    IUFillSwitch(&MountType[MT_MF], "MF", "MF", ISS_OFF);
    IUFillSwitch(&MountType[MT_114GT], "114GT", "114GT", ISS_OFF);
    IUFillSwitch(&MountType[MT_DOB], "DOB", "DOB", ISS_OFF);
    IUFillSwitch(&MountType[MT_UNKNOWN], "UNKNOWN", "UNKNOWN", ISS_ON);
    IUFillSwitchVector(&MountTypeV, MountType, 9, getDeviceName(), "MOUNT_TYPE", "Mount type", DetailedMountInfoPage, IP_RO,
                        ISR_ATMOST1, 60, IPS_IDLE);

    IUFillNumber(&AxisOneInfo[MICROSTEPS_PER_REVOLUTION], "MICROSTEPS_PER_REVOLUTION",
                                                            "Microsteps per revolution",
                                                            "%.0f",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);
    IUFillNumber(&AxisOneInfo[STEPPER_CLOCK_FREQUENCY], "STEPPER_CLOCK_FREQUENCY",
                                                            "Stepper clock frequency",
                                                            "%.0f",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);
    IUFillNumber(&AxisOneInfo[HIGH_SPEED_RATIO], "HIGH_SPEED_RATIO",
                                                            "High speed ratio",
                                                            "%.0f",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);
    IUFillNumber(&AxisOneInfo[MICROSTEPS_PER_WORM_REVOLUTION], "MICROSTEPS_PER_WORM_REVOLUTION",
                                                            "Microsteps per worm revolution",
                                                            "%.0f",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);

    IUFillNumberVector(&AxisOneInfoV, AxisOneInfo, 4, getDeviceName(), "AXIS_ONE_INFO", "Axis one information",
                        DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&AxisOneState[FULL_STOP], "FULL_STOP", "FULL_STOP", ISS_OFF);
    IUFillSwitch(&AxisOneState[SLEWING], "SLEWING", "SLEWING", ISS_OFF);
    IUFillSwitch(&AxisOneState[SLEWING_TO], "SLEWING_TO", "SLEWING_TO", ISS_OFF);
    IUFillSwitch(&AxisOneState[SLEWING_FORWARD], "SLEWING_FORWARD", "SLEWING_FORWARD", ISS_OFF);
    IUFillSwitch(&AxisOneState[HIGH_SPEED], "HIGH_SPEED", "HIGH_SPEED", ISS_OFF);
    IUFillSwitch(&AxisOneState[NOT_INITIALISED], "NOT_INITIALISED", "NOT_INITIALISED", ISS_ON);
    IUFillSwitchVector(&AxisOneStateV, AxisOneState, 6, getDeviceName(), "AXIS_ONE_STATE", "Axis one state", DetailedMountInfoPage, IP_RO,
                        ISR_NOFMANY, 60, IPS_IDLE);

    IUFillNumber(&AxisTwoInfo[MICROSTEPS_PER_REVOLUTION], "MICROSTEPS_PER_REVOLUTION",
                                                            "Microsteps per revolution",
                                                            "%.0f",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);
    IUFillNumber(&AxisTwoInfo[STEPPER_CLOCK_FREQUENCY], "STEPPER_CLOCK_FREQUENCY",
                                                            "Step timer frequency",
                                                            "%.0f",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);
    IUFillNumber(&AxisTwoInfo[HIGH_SPEED_RATIO], "HIGH_SPEED_RATIO",
                                                            "High speed ratio",
                                                            "%.0f",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);
    IUFillNumber(&AxisTwoInfo[MICROSTEPS_PER_WORM_REVOLUTION], "MICROSTEPS_PER_WORM_REVOLUTION",
                                                            "Mictosteps per worm revolution",
                                                            "%.0f",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);

    IUFillNumberVector(&AxisTwoInfoV, AxisTwoInfo, 4, getDeviceName(), "AXIS_TWO_INFO", "Axis two information",
                        DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&AxisTwoState[FULL_STOP], "FULL_STOP", "FULL_STOP", ISS_OFF);
    IUFillSwitch(&AxisTwoState[SLEWING], "SLEWING", "SLEWING", ISS_OFF);
    IUFillSwitch(&AxisTwoState[SLEWING_TO], "SLEWING_TO", "SLEWING_TO", ISS_OFF);
    IUFillSwitch(&AxisTwoState[SLEWING_FORWARD], "SLEWING_FORWARD", "SLEWING_FORWARD", ISS_OFF);
    IUFillSwitch(&AxisTwoState[HIGH_SPEED], "HIGH_SPEED", "HIGH_SPEED", ISS_OFF);
    IUFillSwitch(&AxisTwoState[NOT_INITIALISED], "NOT_INITIALISED", "NOT_INITIALISED", ISS_ON);
    IUFillSwitchVector(&AxisTwoStateV, AxisTwoState, 6, getDeviceName(), "AXIS_TWO_STATE", "Axis two state", DetailedMountInfoPage, IP_RO,
                        ISR_NOFMANY, 60, IPS_IDLE);

    IUFillNumber(&AxisOneEncoderValues[RAW_MICROSTEPS], "RAW_MICROSTEPS",
                                                            "Raw Microsteps",
                                                            "%.0f",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);
    IUFillNumber(&AxisOneEncoderValues[OFFSET_FROM_INITIAL], "OFFSET_FROM_INITIAL",
                                                            "Offset from initial",
                                                            "%.0f",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);
    IUFillNumber(&AxisOneEncoderValues[DEGREES_FROM_INITIAL], "DEGREES_FROM_INITIAL",
                                                            "Degrees from initial",
                                                            "%.2f",
                                                            -1000.0,
                                                            1000.0,
                                                            1,
                                                            0);

    IUFillNumberVector(&AxisOneEncoderValuesV, AxisOneEncoderValues, 3, getDeviceName(), "AXIS1_ENCODER_VALUES", "Axis 1 Encoder values",
                        DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&AxisTwoEncoderValues[RAW_MICROSTEPS], "RAW_MICROSTEPS",
                                                            "Raw Microsteps",
                                                            "%.0f",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);
    IUFillNumber(&AxisTwoEncoderValues[OFFSET_FROM_INITIAL], "OFFSET_FROM_INITIAL",
                                                            "Offset from initial",
                                                            "%.0f",
                                                            0,
                                                            0xFFFFFF,
                                                            1,
                                                            0);
    IUFillNumber(&AxisTwoEncoderValues[DEGREES_FROM_INITIAL], "DEGREES_FROM_INITIAL",
                                                            "Degrees from initial",
                                                            "%.2f",
                                                            -1000.0,
                                                            1000.0,
                                                            1,
                                                            0);

    IUFillNumberVector(&AxisTwoEncoderValuesV, AxisTwoEncoderValues, 3, getDeviceName(), "AXIS2_ENCODER_VALUES", "Axis 2 Encoder values",
                        DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);
    // Register any visible before connection properties

    return true;
}

void SkywatcherAPIMount::ISGetProperties (const char *dev)
{
    IDLog("SkywatcherAPIMount::ISGetProperties\n");
    INDI::Telescope::ISGetProperties(dev);

    if (isConnected())
    {
        // Fill in any real values now available MCInit should have been called already
        UpdateDetailedMountInformation(false);

        // Define our connected only properties to the base driver
        // e.g. defineNumber(MyNumberVectorPointer);
        // This will register our properties and send a IDDefXXXX mewssage to any connected clients
        defineNumber(&BasicMountInfoV);
        defineSwitch(&MountTypeV);
        defineNumber(&AxisOneInfoV);
        defineSwitch(&AxisOneStateV);
        defineNumber(&AxisTwoInfoV);
        defineSwitch(&AxisTwoStateV);
        defineNumber(&AxisOneEncoderValuesV);
        defineNumber(&AxisTwoEncoderValuesV);
    }
    return;
}

bool SkywatcherAPIMount::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // It is for us
        ProcessBlobProperties(this, name, sizes, blobsizes, blobs, formats, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool SkywatcherAPIMount::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // It is for us
        ProcessNumberProperties(this, name, values, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool SkywatcherAPIMount::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // It is for us
        ProcessSwitchProperties(this, name, states, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool SkywatcherAPIMount::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        ProcessTextProperties(this, name, texts, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool SkywatcherAPIMount::MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand command)
{
    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::MoveNS");

    double speed = (dir == MOTION_NORTH) ? (LOW_SPEED_MARGIN / 2) : -(LOW_SPEED_MARGIN / 2);
    const char *dirStr = (dir == MOTION_NORTH) ? "North" : "South";

    switch (command)
    {
        case MOTION_START:
        DEBUGF(DBG_SCOPE, "Starting Slew %s", dirStr);
        Slew(AXIS2, speed);
        break;

        case MOTION_STOP:
        DEBUGF(DBG_SCOPE, "Stopping Slew %s", dirStr);
        SlowStop(AXIS2);
        break;
    }

    return true;
}

bool SkywatcherAPIMount::MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand command)
{
    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::MoveWE");

    double speed = (dir == MOTION_WEST) ? (LOW_SPEED_MARGIN / 2) : -(LOW_SPEED_MARGIN / 2);
    const char *dirStr = (dir == MOTION_WEST) ? "West" : "East";

    switch (command)
    {
        case MOTION_START:
        DEBUGF(DBG_SCOPE, "Starting Slew %s", dirStr);
        Slew(AXIS1, speed);
        break;

        case MOTION_STOP:
        DEBUGF(DBG_SCOPE, "Stopping Slew %s", dirStr);
        SlowStop(AXIS1);
        break;
    }

    return true;
}

bool SkywatcherAPIMount::Park()
{
    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::Park");
    return false;
}

bool SkywatcherAPIMount::ReadScopeStatus()
{
    DEBUG(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "SkywatcherAPIMount::ReadScopeStatus");

    // Horrible hack to get over the fact that the base class calls ReadScopeStatus from inside Connect
    // before I have a chance to set up the serial port
    SetSerialPort(PortFD);

    // leave the following stuff in for the time being it is mostly harmless

    // Quick check of the mount
    if (!GetMotorBoardVersion(AXIS1))
        return false;

    if (!GetStatus(AXIS1))
        return false;

    if (!GetStatus(AXIS2))
        return false;

    // Update Axis Position
    if (!GetEncoder(AXIS1))
        return false;
    if (!GetEncoder(AXIS2))
        return false;

    UpdateDetailedMountInformation(true);

    // Calculate new RA DEC
    struct ln_hrz_posn AltAz;
    AltAz.alt = MicrostepsToDegrees(AXIS2, CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis2 encoder %ld initial %ld alt(degrees) %lf", CurrentEncoders[AXIS2], ZeroPositionEncoders[AXIS2], AltAz.alt);
    AltAz.az = MicrostepsToDegrees(AXIS1, CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis1 encoder %ld initial %ld az(degrees) %lf", CurrentEncoders[AXIS1], ZeroPositionEncoders[AXIS1], AltAz.az);
    TelescopeDirectionVector TDV = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "TDV x %lf y %lf z %lf", TDV.x, TDV.y, TDV.z);

    double RightAscension, Declination;
    if (TransformTelescopeToCelestial( TDV, RightAscension, Declination))
        DEBUG(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Conversion OK");
    else
    {
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
            TelescopeDirectionVector RotatedTDV(TDV);
            switch (GetApproximateMountAlignment())
            {
                case ZENITH:
                    break;

                case NORTH_CELESTIAL_POLE:
                    // Rotate the TDV coordinate system anticlockwise (positive) around the y axis by 90 minus
                    // the (positive)observatory latitude. The vector itself is rotated clockwise
                    RotatedTDV.RotateAroundY(90.0 - Position.lat);
                    AltitudeAzimuthFromTelescopeDirectionVector(RotatedTDV, AltAz);
                    break;

                case SOUTH_CELESTIAL_POLE:
                    // Rotate the TDV coordinate system clockwise (negative) around the y axis by 90 plus
                    // the (negative)observatory latitude. The vector itself is rotated anticlockwise
                    RotatedTDV.RotateAroundY(-90.0 - Position.lat);
                    AltitudeAzimuthFromTelescopeDirectionVector(RotatedTDV, AltAz);
                    break;
            }
#ifdef USE_INITIAL_JULIAN_DATE
            ln_get_equ_from_hrz(&AltAz, &Position, InitialJulianDate, &EquatorialCoordinates);
#else
            ln_get_equ_from_hrz(&AltAz, &Position, ln_get_julian_from_sys(), &EquatorialCoordinates);
#endif
        }
        else
            // The best I can do is just do a direct conversion to RA/DEC
            EquatorialCoordinatesFromTelescopeDirectionVector(TDV, EquatorialCoordinates);
        // libnova works in decimal degrees
        RightAscension = EquatorialCoordinates.ra * 24.0 / 360.0;
        Declination = EquatorialCoordinates.dec;
        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Conversion Failed - HavePosition %d RA (degrees) %lf DEC (degrees) %lf", HavePosition,
                                                                                            EquatorialCoordinates.ra,
                                                                                            EquatorialCoordinates.dec);
    }

    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "New RA %lf (hours) DEC %lf (degrees)", RightAscension, Declination);
    NewRaDec(RightAscension, Declination);

    return true;
}

bool SkywatcherAPIMount::saveConfigItems(FILE *fp)
{
    SaveConfigProperties(fp);

    return INDI::Telescope::saveConfigItems(fp);
}

bool SkywatcherAPIMount::Sync(double ra, double dec)
{
    DEBUG(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "SkywatcherAPIMount::Sync");

    // Compute a telescope direction vector from the current encoders
    if (!GetEncoder(AXIS1))
        return false;
    if (!GetEncoder(AXIS2))
        return false;

    // Might as well do this
    UpdateDetailedMountInformation(true);

    struct ln_hrz_posn AltAz;
    AltAz.alt = MicrostepsToDegrees(AXIS2, CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis2 encoder %ld initial %ld alt(degrees) %lf", CurrentEncoders[AXIS2], ZeroPositionEncoders[AXIS2], AltAz.alt);
    AltAz.az = MicrostepsToDegrees(AXIS1, CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis1 encoder %ld initial %ld az(degrees) %lf", CurrentEncoders[AXIS1], ZeroPositionEncoders[AXIS1], AltAz.az);

    AlignmentDatabaseEntry NewEntry;
#ifdef USE_INITIAL_JULIAN_DATE
    NewEntry.ObservationJulianDate = InitialJulianDate;
#else
    NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
#endif
    NewEntry.RightAscension = ra;
    NewEntry.Declination = dec;
    NewEntry.TelescopeDirection = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
    NewEntry.PrivateDataSize = 0;

    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "New sync point Date %lf RA %lf DEC %lf TDV(x %lf y %lf z %lf)",
                    NewEntry.ObservationJulianDate, NewEntry.RightAscension, NewEntry.Declination,
                    NewEntry.TelescopeDirection.x, NewEntry.TelescopeDirection.y, NewEntry.TelescopeDirection.z);

    if (!CheckForDuplicateSyncPoint(NewEntry))
    {

        GetAlignmentDatabase().push_back(NewEntry);

        // Tell the client about size change
        UpdateSize();

        // Tell the math plugin to reinitialise
        Initialise(this);

        return true;
    }
    return false;
}

void SkywatcherAPIMount::TimerHit()
{
    // By default this method is called every POLLMS milliseconds

    // Call the base class handler
    // This normally just calls ReadScopeStatus
    INDI::Telescope::TimerHit();

    // Do my own timer stuff assuming ReadScopeStatus has just been called

    switch(TrackState)
    {
        case SCOPE_SLEWING:
            if ((AxesStatus[AXIS1].FullStop) && (AxesStatus[AXIS2].FullStop))
            {
                if (ISS_ON == IUFindSwitch(&CoordSP,"TRACK")->s)
                {
                    // Goto has finished start tracking
                    TrackState = SCOPE_TRACKING;
                    // Fall through to tracking case
                }
                else
                {
                    TrackState = SCOPE_IDLE;
                    break;
                }
            }
            else
                break;

        case SCOPE_TRACKING:
        {
            // Continue or start tracking
            // Calculate where the mount needs to be in POLLMS time
            // POLLMS is hardcoded to be one second
            double JulianOffset = 1.0 / (24.0 * 60 * 60); // TODO may need to make this longer to get a meaningful result
            TelescopeDirectionVector TDV;
            ln_hrz_posn AltAz;
            if (TransformCelestialToTelescope(CurrentTrackingTarget.ra, CurrentTrackingTarget.dec,
#ifdef USE_INITIAL_JULIAN_DATE
                                                0, TDV))
#else
                                                JulianOffset, TDV))
#endif
                AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
            else
            {
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
                if (HavePosition)
                    ln_get_hrz_from_equ(&EquatorialCoordinates, &Position,
#ifdef USE_INITIAL_JULIAN_DATE
                                            InitialJulianDate, &AltAz);
#else
                                            ln_get_julian_from_sys() + JulianOffset, &AltAz);
#endif
                else
                {
                    // No sense in tracking in this case
                    TrackState = SCOPE_IDLE;
                    break;
                }
            }
            DEBUGF(INDI::Logger::DBG_SESSION, "Tracking AXIS1 CurrentEncoder %ld OldTrackingTarget %ld AXIS2 CurrentEncoder %ld OldTrackingTarget %ld",
                                            CurrentEncoders[AXIS1], OldTrackingTarget[AXIS1], CurrentEncoders[AXIS2], OldTrackingTarget[AXIS2]);
            DEBUGF(INDI::Logger::DBG_SESSION, "New Tracking Target Altitude %lf degrees %ld microsteps Azimuth %lf degrees %ld microsteps",
                                AltAz.alt, DegreesToMicrosteps(AXIS2, AltAz.alt), AltAz.az, DegreesToMicrosteps(AXIS1, AltAz.az));

            long AltitudeOffsetMicrosteps = DegreesToMicrosteps(AXIS2, AltAz.alt) + ZeroPositionEncoders[AXIS2] - CurrentEncoders[AXIS2];
            long AzimuthOffsetMicrosteps = DegreesToMicrosteps(AXIS1, AltAz.az) + ZeroPositionEncoders[AXIS1] - CurrentEncoders[AXIS1];

            DEBUGF(INDI::Logger::DBG_SESSION, "New Tracking Target AltitudeOffset %ld microsteps AzimuthOffset %ld microsteps",
                                AltitudeOffsetMicrosteps, AzimuthOffsetMicrosteps);

            if (AzimuthOffsetMicrosteps > MicrostepsPerRevolution[AXIS1] / 2)
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Tracking AXIS1 going long way round");
                // Going the long way round - send it the other way
                AzimuthOffsetMicrosteps -= MicrostepsPerRevolution[AXIS1];
            }
            if (0 != AzimuthOffsetMicrosteps)
            {
                // Calculate the slewing rates needed to reach that position
                // at the correct time.
                long AzimuthRate = StepperClockFrequency[AXIS1] / AzimuthOffsetMicrosteps;
                if (!AxesStatus[AXIS1].FullStop &&
                    ((AxesStatus[AXIS1].SlewingForward && (AzimuthRate < 0)) || (!AxesStatus[AXIS1].SlewingForward && (AzimuthRate > 0))))
                {
                    // Direction change whilst axis running
                    // Abandon tracking for this clock tick
                    DEBUG(INDI::Logger::DBG_SESSION, "Tracking - AXIS1 direction change");
                    SlowStop(AXIS1);
                }
                else
                {
                    char Direction = AzimuthRate > 0 ? '0' : '1';
                    AzimuthRate = std::abs(AzimuthRate);
                    SetClockTicksPerMicrostep(AXIS1, AzimuthRate < 1 ? 1 : AzimuthRate);
                    if (AxesStatus[AXIS1].FullStop)
                    {
                        DEBUG(INDI::Logger::DBG_SESSION, "Tracking - AXIS1 restart");
                        SetMotionMode(AXIS1, '1', Direction);
                        StartMotion(AXIS1);
                    }
                    DEBUGF(INDI::Logger::DBG_SESSION, "Tracking - AXIS1 offset %ld microsteps rate %ld direction %c",
                                                                AzimuthOffsetMicrosteps, AzimuthRate, Direction);
                }
            }
            else
            {
                // Nothing to do - stop the axis
                DEBUG(INDI::Logger::DBG_SESSION, "Tracking - AXIS1 zero offset");
                SlowStop(AXIS1);
            }

            // Do I need to take out any complete revolutions before I do this test?
            if (AltitudeOffsetMicrosteps > MicrostepsPerRevolution[AXIS2] / 2)
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Tracking AXIS2 going long way round");
                // Going the long way round - send it the other way
                AltitudeOffsetMicrosteps -= MicrostepsPerRevolution[AXIS2];
            }
            if (0 != AltitudeOffsetMicrosteps)
            {
                 // Calculate the slewing rates needed to reach that position
                // at the correct time.
                long AltitudeRate = StepperClockFrequency[AXIS2] / AltitudeOffsetMicrosteps;

                if (!AxesStatus[AXIS2].FullStop &&
                    ((AxesStatus[AXIS2].SlewingForward && (AltitudeRate < 0)) || (!AxesStatus[AXIS2].SlewingForward && (AltitudeRate > 0))))
                {
                    // Direction change whilst axis running
                    // Abandon tracking for this clock tick
                    DEBUG(INDI::Logger::DBG_SESSION, "Tracking - AXIS2 direction change");
                    SlowStop(AXIS2);
                }
                else
                {
                    char Direction = AltitudeRate > 0 ? '0' : '1';
                    AltitudeRate = std::abs(AltitudeRate);
                    SetClockTicksPerMicrostep(AXIS2, AltitudeRate < 1 ? 1 : AltitudeRate);
                    if (AxesStatus[AXIS2].FullStop)
                    {
                        DEBUG(INDI::Logger::DBG_SESSION, "Tracking - AXIS2 restart");
                        SetMotionMode(AXIS2, '1', Direction);
                        StartMotion(AXIS2);
                    }
                    DEBUGF(INDI::Logger::DBG_SESSION, "Tracking - AXIS2 offset %ld microsteps rate %ld direction %c",
                                                                    AltitudeOffsetMicrosteps, AltitudeRate, Direction);
                }
            }
            else
            {
                // Nothing to do - stop the axis
                DEBUG(INDI::Logger::DBG_SESSION, "Tracking - AXIS2 zero offset");
                SlowStop(AXIS2);
            }


            DEBUGF(INDI::Logger::DBG_SESSION, "Tracking - AXIS1 error %d AXIS2 error %d",
                                                                OldTrackingTarget[AXIS1] - CurrentEncoders[AXIS1],
                                                                OldTrackingTarget[AXIS2] - CurrentEncoders[AXIS2]);

            OldTrackingTarget[AXIS1] = AzimuthOffsetMicrosteps + CurrentEncoders[AXIS1];
            OldTrackingTarget[AXIS2] = AltitudeOffsetMicrosteps + CurrentEncoders[AXIS2];
            break;
        }

        default:
            break;
    }
}

bool SkywatcherAPIMount::updateLocation(double latitude, double longitude, double elevation)
{
    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::updateLocation");
    UpdateLocation(latitude, longitude, elevation);
    return true;
}

bool SkywatcherAPIMount::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        // Fill in any real values now available MCInit should have been called already
        UpdateDetailedMountInformation(false);

        // Define our connected only properties to the base driver
        // e.g. defineNumber(MyNumberVectorPointer);
        // This will register our properties and send a IDDefXXXX mewssage to any connected clients
        // I have now idea why I have to do this here as well as in ISGetProperties. It makes me
        // concerned there is a design or implementation flaw somewhere.
        defineNumber(&BasicMountInfoV);
        defineSwitch(&MountTypeV);
        defineNumber(&AxisOneInfoV);
        defineSwitch(&AxisOneStateV);
        defineNumber(&AxisTwoInfoV);
        defineSwitch(&AxisTwoStateV);
        defineNumber(&AxisOneEncoderValuesV);
        defineNumber(&AxisTwoEncoderValuesV);

        // Start the timer if we need one
        // SetTimer(POLLMS);
    }
    else
    {
        // Delete any connected only properties from the base driver's list
        // e.g. deleteProperty(MyNumberVector.name);
        deleteProperty(BasicMountInfoV.name);
        deleteProperty(MountTypeV.name);
        deleteProperty(AxisOneInfoV.name);
        deleteProperty(AxisOneStateV.name);
        deleteProperty(AxisTwoInfoV.name);
        deleteProperty(AxisTwoStateV.name);
        deleteProperty(AxisOneEncoderValuesV.name);
        deleteProperty(AxisTwoEncoderValuesV.name);
    }
}

// Private methods

int SkywatcherAPIMount::skywatcher_tty_read(int fd, char *buf, int nbytes, int timeout, int *nbytes_read)
{
    return tty_read(fd, buf, nbytes, timeout, nbytes_read);
}

int SkywatcherAPIMount::skywatcher_tty_write(int fd, const char * buffer, int nbytes, int *nbytes_written)
{
    return tty_write(fd, buffer, nbytes, nbytes_written);
}

void SkywatcherAPIMount::SkywatcherMicrostepsFromTelescopeDirectionVector(const TelescopeDirectionVector TelescopeDirectionVector,
                                                            long& Axis1Microsteps, long& Axis2Microsteps)
{
    // For the time being I assume that all skywathcer mounts share the same encoder conventions
    double Axis1Angle;
    double Axis2Angle;
    SphericalCoordinateFromTelescopeDirectionVector(TelescopeDirectionVector, Axis1Angle, TelescopeDirectionVectorSupportFunctions::CLOCKWISE, Axis1Angle, FROM_AZIMUTHAL_PLANE);

    Axis1Microsteps = RadiansToMicrosteps(AXIS1, Axis1Angle);
    Axis2Microsteps = RadiansToMicrosteps(AXIS2, Axis2Angle);
}

const TelescopeDirectionVector SkywatcherAPIMount::TelescopeDirectionVectorFromSkywatcherMicrosteps(long Axis1Microsteps, long Axis2Microsteps)
{
    // For the time being I assume that all skywathcer mounts share the same encoder conventions
    double Axis1Angle = MicrostepsToRadians(AXIS1, Axis1Microsteps);
    double Axis2Angle = MicrostepsToRadians(AXIS2, Axis2Microsteps);
    return TelescopeDirectionVectorFromSphericalCoordinate(Axis1Angle, TelescopeDirectionVectorSupportFunctions::CLOCKWISE, Axis2Angle, FROM_AZIMUTHAL_PLANE);
}

void SkywatcherAPIMount::UpdateDetailedMountInformation(bool InformClient)
{
    bool BasicMountInfoHasChanged = false;
    if (BasicMountInfo[MOTOR_CONTROL_FIRMWARE_VERSION].value != MCVersion)
    {
        BasicMountInfo[MOTOR_CONTROL_FIRMWARE_VERSION].value = MCVersion;
        BasicMountInfoHasChanged = true;
    }
    if (BasicMountInfo[MOUNT_CODE].value != MountCode)
    {
        BasicMountInfo[MOUNT_CODE].value = MountCode;
        // Also tell the alignment subsystem
        switch (MountCode)
        {
            case _114GT:
            case DOB:
                SetApproximateMountAlignmentFromMountType(ALTAZ);
                break;

            default:
                SetApproximateMountAlignmentFromMountType(EQUATORIAL);
                break;
        }
        BasicMountInfoHasChanged = true;
    }
    if (BasicMountInfo[IS_DC_MOTOR].value != IsDCMotor)
    {
        BasicMountInfo[IS_DC_MOTOR].value = IsDCMotor;
        BasicMountInfoHasChanged = true;
    }
    if (BasicMountInfoHasChanged && InformClient)
        IDSetNumber(&BasicMountInfoV, NULL);

    int OldMountType = IUFindOnSwitchIndex(&MountTypeV);
    int NewMountType;
    switch (MountCode)
    {
        case 0x00:
            NewMountType = MT_EQ6;
            break;
        case 0x01:
            NewMountType = MT_HEQ5;
            break;
        case 0x02:
            NewMountType = MT_EQ5;
            break;
        case 0x03:
            NewMountType = MT_EQ3;
            break;
        case 0x80:
            NewMountType = MT_GT;
            break;
        case 0x81:
            NewMountType = MT_MF;
            break;
        case 0x82:
            NewMountType = MT_114GT;
            break;
        case 0x90:
            NewMountType = MT_DOB;
            break;
        default:
            NewMountType = MT_UNKNOWN;
            break;
    }
    if (OldMountType != NewMountType)
    {
        IUResetSwitch(&MountTypeV);
        MountType[NewMountType].s = ISS_ON;
        if (InformClient)
            IDSetSwitch(&MountTypeV, NULL);
    }

    bool AxisOneInfoHasChanged = false;
    if (AxisOneInfo[MICROSTEPS_PER_REVOLUTION].value != MicrostepsPerRevolution[0])
    {
        AxisOneInfo[MICROSTEPS_PER_REVOLUTION].value = MicrostepsPerRevolution[0];
        AxisOneInfoHasChanged = true;
    }
    if (AxisOneInfo[STEPPER_CLOCK_FREQUENCY].value != StepperClockFrequency[0])
    {
        AxisOneInfo[STEPPER_CLOCK_FREQUENCY].value = StepperClockFrequency[0];
        AxisOneInfoHasChanged = true;
    }
    if (AxisOneInfo[HIGH_SPEED_RATIO].value != HighSpeedRatio[0])
    {
        AxisOneInfo[HIGH_SPEED_RATIO].value = HighSpeedRatio[0];
        AxisOneInfoHasChanged = true;
    }
    if (AxisOneInfo[MICROSTEPS_PER_WORM_REVOLUTION].value != MicrostepsPerWormRevolution[0])
    {
        AxisOneInfo[MICROSTEPS_PER_WORM_REVOLUTION].value = MicrostepsPerWormRevolution[0];
        AxisOneInfoHasChanged = true;
    }
    if (AxisOneInfoHasChanged && InformClient)
        IDSetNumber(&AxisOneInfoV, NULL);

    bool AxisOneStateHasChanged = false;
    if (AxisOneState[FULL_STOP].s != (AxesStatus[0].FullStop ? ISS_ON : ISS_OFF))
    {
        AxisOneState[FULL_STOP].s = AxesStatus[0].FullStop ? ISS_ON : ISS_OFF;
        AxisOneStateHasChanged = true;
    }
    if (AxisOneState[SLEWING].s != (AxesStatus[0].Slewing ? ISS_ON : ISS_OFF))
    {
        AxisOneState[SLEWING].s = AxesStatus[0].Slewing ? ISS_ON : ISS_OFF;
        AxisOneStateHasChanged = true;
    }
    if (AxisOneState[SLEWING_TO].s != (AxesStatus[0].SlewingTo ? ISS_ON : ISS_OFF))
    {
        AxisOneState[SLEWING_TO].s = AxesStatus[0].SlewingTo ? ISS_ON : ISS_OFF;
        AxisOneStateHasChanged = true;
    }
    if (AxisOneState[SLEWING_FORWARD].s != (AxesStatus[0].SlewingForward ? ISS_ON : ISS_OFF))
    {
        AxisOneState[SLEWING_FORWARD].s = AxesStatus[0].SlewingForward ? ISS_ON : ISS_OFF;
        AxisOneStateHasChanged = true;
    }
    if (AxisOneState[HIGH_SPEED].s != (AxesStatus[0].HighSpeed ? ISS_ON : ISS_OFF))
    {
        AxisOneState[HIGH_SPEED].s = AxesStatus[0].HighSpeed ? ISS_ON : ISS_OFF;
        AxisOneStateHasChanged = true;
    }
    if (AxisOneState[NOT_INITIALISED].s != (AxesStatus[0].NotInitialized ? ISS_ON : ISS_OFF))
    {
        AxisOneState[NOT_INITIALISED].s = AxesStatus[0].NotInitialized ? ISS_ON : ISS_OFF;
        AxisOneStateHasChanged = true;
    }
    if (AxisOneStateHasChanged && InformClient)
            IDSetSwitch(&AxisOneStateV, NULL);

    bool AxisTwoInfoHasChanged = false;
    if (AxisTwoInfo[MICROSTEPS_PER_REVOLUTION].value != MicrostepsPerRevolution[1])
    {
        AxisTwoInfo[MICROSTEPS_PER_REVOLUTION].value = MicrostepsPerRevolution[1];
        AxisTwoInfoHasChanged = true;
    }
    if (AxisTwoInfo[STEPPER_CLOCK_FREQUENCY].value != StepperClockFrequency[1])
    {
        AxisTwoInfo[STEPPER_CLOCK_FREQUENCY].value = StepperClockFrequency[1];
        AxisTwoInfoHasChanged = true;
    }
    if (AxisTwoInfo[HIGH_SPEED_RATIO].value != HighSpeedRatio[1])
    {
        AxisTwoInfo[HIGH_SPEED_RATIO].value = HighSpeedRatio[1];
        AxisTwoInfoHasChanged = true;
    }
    if (AxisTwoInfo[MICROSTEPS_PER_WORM_REVOLUTION].value != MicrostepsPerWormRevolution[1])
    {
        AxisTwoInfo[MICROSTEPS_PER_WORM_REVOLUTION].value = MicrostepsPerWormRevolution[1];
        AxisTwoInfoHasChanged = true;
    }
    if (AxisTwoInfoHasChanged && InformClient)
        IDSetNumber(&AxisTwoInfoV, NULL);

    bool AxisTwoStateHasChanged = false;
    if (AxisTwoState[FULL_STOP].s != (AxesStatus[1].FullStop ? ISS_ON : ISS_OFF))
    {
        AxisTwoState[FULL_STOP].s = AxesStatus[1].FullStop ? ISS_ON : ISS_OFF;
        AxisTwoStateHasChanged = true;
    }
    if (AxisTwoState[SLEWING].s != (AxesStatus[1].Slewing ? ISS_ON : ISS_OFF))
    {
        AxisTwoState[SLEWING].s = AxesStatus[1].Slewing ? ISS_ON : ISS_OFF;
        AxisTwoStateHasChanged = true;
    }
    if (AxisTwoState[SLEWING_TO].s != (AxesStatus[1].SlewingTo ? ISS_ON : ISS_OFF))
    {
        AxisTwoState[SLEWING_TO].s = AxesStatus[1].SlewingTo ? ISS_ON : ISS_OFF;
        AxisTwoStateHasChanged = true;
    }
    if (AxisTwoState[SLEWING_FORWARD].s != (AxesStatus[1].SlewingForward ? ISS_ON : ISS_OFF))
    {
        AxisTwoState[SLEWING_FORWARD].s = AxesStatus[1].SlewingForward ? ISS_ON : ISS_OFF;
        AxisTwoStateHasChanged = true;
    }
    if (AxisTwoState[HIGH_SPEED].s != (AxesStatus[1].HighSpeed ? ISS_ON : ISS_OFF))
    {
        AxisTwoState[HIGH_SPEED].s = AxesStatus[1].HighSpeed ? ISS_ON : ISS_OFF;
        AxisTwoStateHasChanged = true;
    }
    if (AxisTwoState[NOT_INITIALISED].s != (AxesStatus[1].NotInitialized ? ISS_ON : ISS_OFF))
    {
        AxisTwoState[NOT_INITIALISED].s = AxesStatus[1].NotInitialized ? ISS_ON : ISS_OFF;
        AxisTwoStateHasChanged = true;
    }
    if (AxisTwoStateHasChanged && InformClient)
            IDSetSwitch(&AxisTwoStateV, NULL);

    bool AxisOneEncoderValuesHasChanged = false;
    if ((AxisOneEncoderValues[RAW_MICROSTEPS].value != CurrentEncoders[AXIS1])
    || (AxisOneEncoderValues[OFFSET_FROM_INITIAL].value != CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]))
    {
        AxisOneEncoderValues[RAW_MICROSTEPS].value = CurrentEncoders[AXIS1];
        AxisOneEncoderValues[OFFSET_FROM_INITIAL].value = CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1];
        AxisOneEncoderValues[DEGREES_FROM_INITIAL].value = MicrostepsToDegrees(AXIS1, CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]);
        AxisOneEncoderValuesHasChanged = true;
    }
    if (AxisOneEncoderValuesHasChanged && InformClient)
        IDSetNumber(&AxisOneEncoderValuesV, NULL);

        bool AxisTwoEncoderValuesHasChanged = false;
    if ((AxisTwoEncoderValues[RAW_MICROSTEPS].value != CurrentEncoders[AXIS2])
    || (AxisTwoEncoderValues[OFFSET_FROM_INITIAL].value != CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]))
    {
        AxisTwoEncoderValues[RAW_MICROSTEPS].value = CurrentEncoders[AXIS2];
        AxisTwoEncoderValues[OFFSET_FROM_INITIAL].value = CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2];
        AxisTwoEncoderValues[DEGREES_FROM_INITIAL].value = MicrostepsToDegrees(AXIS2, CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]);
        AxisTwoEncoderValuesHasChanged = true;
    }
    if (AxisTwoEncoderValuesHasChanged && InformClient)
        IDSetNumber(&AxisTwoEncoderValuesV, NULL);
}

