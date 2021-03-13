/*!
 * \file SkywatcherAltAzSimple.cpp
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

#include "skywatcherAltAzSimple.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <libnova/transform.h>
// libnova specifies round() on old systems and it collides with the new gcc 5.x/6.x headers
#define HAVE_ROUND
#include <libnova/utility.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

#include <sys/stat.h>

// We declare an auto pointer to SkywatcherAltAzSimple.
std::unique_ptr<SkywatcherAltAzSimple> SkywatcherAltAzSimplePtr(new SkywatcherAltAzSimple());

/* Preset Slew Speeds */
#define SLEWMODES 9
double SlewSpeeds[SLEWMODES] = { 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 600.0 };

void ISPoll(void *p);

namespace
{
bool FileExists(const std::string &name)
{
    struct stat buffer;

    return (stat (name.c_str(), &buffer) == 0);
}

std::string GetLogTimestamp()
{
    time_t Now = time(nullptr);
    struct tm TimeStruct;
    char Buffer[60];
    std::string FinalStr;

    TimeStruct = *localtime(&Now);
    strftime(Buffer, sizeof(Buffer), "%Y%m%d %H:%M:%S", &TimeStruct);
    FinalStr = Buffer;
    // Add the millisecond part
    std::chrono::system_clock::time_point NowClock = std::chrono::system_clock::now();
    std::chrono::system_clock::duration TimePassed = NowClock.time_since_epoch();

    TimePassed -= std::chrono::duration_cast<std::chrono::seconds>(TimePassed);
    FinalStr += "." + std::to_string(static_cast<unsigned>(TimePassed / std::chrono::milliseconds(1)));
    return FinalStr;
}
} // namespace

void ISGetProperties(const char *dev)
{
    SkywatcherAltAzSimplePtr->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    SkywatcherAltAzSimplePtr->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    SkywatcherAltAzSimplePtr->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    SkywatcherAltAzSimplePtr->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    SkywatcherAltAzSimplePtr->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice(XMLEle *root)
{
    SkywatcherAltAzSimplePtr->ISSnoopDevice(root);
}

SkywatcherAltAzSimple::SkywatcherAltAzSimple() : TrackLogFileName(GetHomeDirectory() + "/.indi/sw_mount_track_log.txt")
{
    // Set up the logging pointer in SkyWatcherAPI
    pChildTelescope  = this;
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION,
                           SLEWMODES);
    std::remove(TrackLogFileName.c_str());
}

bool SkywatcherAltAzSimple::Abort()
{
    DEBUG(DBG_SCOPE, "SkywatcherAltAzSimple::Abort");
    LogMessage("MOVE ABORT");
    SlowStop(AXIS1);
    SlowStop(AXIS2);
    TrackState = SCOPE_IDLE;

    if (GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY)
    {
        GuideNSNP.s = GuideWENP.s = IPS_IDLE;
        GuideNSN[0].value = GuideNSN[1].value = 0.0;
        GuideWEN[0].value = GuideWEN[1].value = 0.0;

        IDMessage(getDeviceName(), "Guide aborted.");
        IDSetNumber(&GuideNSNP, nullptr);
        IDSetNumber(&GuideWENP, nullptr);

        return true;
    }

    return true;
}

bool SkywatcherAltAzSimple::Handshake()
{
    DEBUG(DBG_SCOPE, "SkywatcherAltAzSimple::Handshake");
    SetSerialPort(PortFD);

    Connection::Interface *activeConnection = getActiveConnection();
    if (!activeConnection->name().compare("CONNECTION_TCP"))
    {
        tty_set_generic_udp_format(1);
    }

    bool Result = InitMount();

    if (getActiveConnection() == serialConnection)
    {
        SerialPortName = serialConnection->port();
    }
    else
    {
        SerialPortName = "";
    }

    RecoverAfterReconnection = false;
    DEBUGF(DBG_SCOPE, "SkywatcherAltAzSimple::Handshake - Result: %d", Result);
    return Result;
}

const char *SkywatcherAltAzSimple::getDefaultName()
{
    //DEBUG(DBG_SCOPE, "SkywatcherAltAzSimple::getDefaultName\n");
    return "Skywatcher Alt-Az Wedge";
}

bool SkywatcherAltAzSimple::Goto(double ra, double dec)
{
    DEBUG(DBG_SCOPE, "SkywatcherAltAzSimple::Goto");

    if (TrackState != SCOPE_IDLE)
        Abort();

    DEBUGF(DBG_SCOPE, "RA %lf DEC %lf", ra, dec);

    if (CoordSP.findWidgetByName("TRACK")->s == ISS_ON || CoordSP.findWidgetByName("SLEW")->s == ISS_ON)
    {
        char RAStr[32], DecStr[32];
        fs_sexa(RAStr, ra, 2, 3600);
        fs_sexa(DecStr, dec, 2, 3600);
        CurrentTrackingTarget.ra  = ra;
        CurrentTrackingTarget.dec = dec;
        DEBUGF(INDI::Logger::DBG_SESSION, "New Tracking target RA %s DEC %s", RAStr, DecStr);
    }

    ln_hrz_posn AltAz { 0, 0 };

    AltAz = GetAltAzPosition(ra, dec);
    DEBUGF(DBG_SCOPE, "New Altitude %lf degrees %ld microsteps Azimuth %lf degrees %ld microsteps", AltAz.alt,
           DegreesToMicrosteps(AXIS2, AltAz.alt), AltAz.az, DegreesToMicrosteps(AXIS1, AltAz.az));
    LogMessage("NEW GOTO TARGET: Ra %lf Dec %lf - Alt %lf Az %lf - microsteps %ld %ld", ra, dec, AltAz.alt, AltAz.az,
               DegreesToMicrosteps(AXIS2, AltAz.alt), DegreesToMicrosteps(AXIS1, AltAz.az));

    // Update the current encoder positions
    GetEncoder(AXIS1);
    GetEncoder(AXIS2);

    long AltitudeOffsetMicrosteps =
        DegreesToMicrosteps(AXIS2, AltAz.alt) + ZeroPositionEncoders[AXIS2] - CurrentEncoders[AXIS2];
    long AzimuthOffsetMicrosteps =
        DegreesToMicrosteps(AXIS1, AltAz.az) + ZeroPositionEncoders[AXIS1] - CurrentEncoders[AXIS1];

    DEBUGF(DBG_SCOPE, "Initial deltas Altitude %ld microsteps Azimuth %ld microsteps", AltitudeOffsetMicrosteps,
           AzimuthOffsetMicrosteps);
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
    if (AltitudeOffsetMicrosteps < -MicrostepsPerRevolution[AXIS2] / 2)
    {
        // Going the long way round - send it the other way
        AltitudeOffsetMicrosteps += MicrostepsPerRevolution[AXIS2];
    }
    if (AzimuthOffsetMicrosteps < -MicrostepsPerRevolution[AXIS1] / 2)
    {
        // Going the long way round - send it the other way
        AzimuthOffsetMicrosteps += MicrostepsPerRevolution[AXIS1];
    }
    DEBUGF(DBG_SCOPE, "Initial Axis2 %ld microsteps Axis1 %ld microsteps",
           ZeroPositionEncoders[AXIS2], ZeroPositionEncoders[AXIS1]);
    DEBUGF(DBG_SCOPE, "Current Axis2 %ld microsteps Axis1 %ld microsteps",
           CurrentEncoders[AXIS2], CurrentEncoders[AXIS1]);
    DEBUGF(DBG_SCOPE, "Altitude offset %ld microsteps Azimuth offset %ld microsteps",
           AltitudeOffsetMicrosteps, AzimuthOffsetMicrosteps);

    if (SlewModesSP.findWidgetByName("SLEW_NORMAL")->s == ISS_ON)
    {
        SilentSlewMode = false;
    }
    else
    {
        SilentSlewMode = true;
    }
    SlewTo(AXIS1, AzimuthOffsetMicrosteps);
    SlewTo(AXIS2, AltitudeOffsetMicrosteps);

    TrackState = SCOPE_SLEWING;

    //EqNP.setState(IPS_BUSY);

    return true;
}

bool SkywatcherAltAzSimple::initProperties()
{
    IDLog("SkywatcherAltAzSimple::initProperties\n");

    // Allow the base class to initialise its visible before connection properties
    INDI::Telescope::initProperties();

    for (int i = 0; i < SlewRateSP.nsp; ++i)
    {
        sprintf(SlewRateSP.sp[i].label, "%.fx", SlewSpeeds[i]);
        SlewRateSP.sp[i].aux = (void *)&SlewSpeeds[i];
    }
    strncpy(SlewRateSP.sp[SlewRateSP.nsp - 1].name, "SLEW_MAX", MAXINDINAME);

    // Add default properties
    addDebugControl();
    addConfigurationControl();

    // Set up property variables
    BasicMountInfoTP[MOTOR_CONTROL_FIRMWARE_VERSION].fill("MOTOR_CONTROL_FIRMWARE_VERSION",
               "Motor control firmware version", "-");
    BasicMountInfoTP[MOUNT_CODE].fill("MOUNT_CODE", "Mount code", "-");
    BasicMountInfoTP[MOUNT_NAME].fill("MOUNT_NAME", "Mount name", "-");
    BasicMountInfoTP[IS_DC_MOTOR].fill("IS_DC_MOTOR", "Is DC motor", "-");
    BasicMountInfoTP.fill(getDeviceName(), "BASIC_MOUNT_INFO",
                     "Basic mount information", DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);

    AxisOneInfoNP[MICROSTEPS_PER_REVOLUTION].fill("MICROSTEPS_PER_REVOLUTION", "Microsteps per revolution",
                 "%.0f", 0, 0xFFFFFF, 1, 0);
    AxisOneInfoNP[STEPPER_CLOCK_FREQUENCY].fill("STEPPER_CLOCK_FREQUENCY", "Stepper clock frequency", "%.0f", 0,
                 0xFFFFFF, 1, 0);
    AxisOneInfoNP[HIGH_SPEED_RATIO].fill("HIGH_SPEED_RATIO", "High speed ratio", "%.0f", 0, 0xFFFFFF, 1, 0);
    AxisOneInfoNP[MICROSTEPS_PER_WORM_REVOLUTION].fill("MICROSTEPS_PER_WORM_REVOLUTION",
                 "Microsteps per worm revolution", "%.0f", 0, 0xFFFFFF, 1, 0);

    AxisOneInfoNP.fill(getDeviceName(), "AXIS_ONE_INFO", "Axis one information",
                       DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);

    AxisOneStateSP[FULL_STOP].fill("FULL_STOP", "FULL_STOP", ISS_OFF);
    AxisOneStateSP[SLEWING].fill("SLEWING", "SLEWING", ISS_OFF);
    AxisOneStateSP[SLEWING_TO].fill("SLEWING_TO", "SLEWING_TO", ISS_OFF);
    AxisOneStateSP[SLEWING_FORWARD].fill("SLEWING_FORWARD", "SLEWING_FORWARD", ISS_OFF);
    AxisOneStateSP[HIGH_SPEED].fill("HIGH_SPEED", "HIGH_SPEED", ISS_OFF);
    AxisOneStateSP[NOT_INITIALISED].fill("NOT_INITIALISED", "NOT_INITIALISED", ISS_ON);
    AxisOneStateSP.fill(getDeviceName(), "AXIS_ONE_STATE", "Axis one state",
                       DetailedMountInfoPage, IP_RO, ISR_NOFMANY, 60, IPS_IDLE);

    AxisTwoInfoNP[MICROSTEPS_PER_REVOLUTION].fill("MICROSTEPS_PER_REVOLUTION", "Microsteps per revolution",
                 "%.0f", 0, 0xFFFFFF, 1, 0);
    AxisTwoInfoNP[STEPPER_CLOCK_FREQUENCY].fill("STEPPER_CLOCK_FREQUENCY", "Step timer frequency", "%.0f", 0,
                 0xFFFFFF, 1, 0);
    AxisTwoInfoNP[HIGH_SPEED_RATIO].fill("HIGH_SPEED_RATIO", "High speed ratio", "%.0f", 0, 0xFFFFFF, 1, 0);
    AxisTwoInfoNP[MICROSTEPS_PER_WORM_REVOLUTION].fill("MICROSTEPS_PER_WORM_REVOLUTION",
                 "Mictosteps per worm revolution", "%.0f", 0, 0xFFFFFF, 1, 0);

    AxisTwoInfoNP.fill(getDeviceName(), "AXIS_TWO_INFO", "Axis two information",
                       DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);

    AxisTwoStateSP[FULL_STOP].fill("FULL_STOP", "FULL_STOP", ISS_OFF);
    AxisTwoStateSP[SLEWING].fill("SLEWING", "SLEWING", ISS_OFF);
    AxisTwoStateSP[SLEWING_TO].fill("SLEWING_TO", "SLEWING_TO", ISS_OFF);
    AxisTwoStateSP[SLEWING_FORWARD].fill("SLEWING_FORWARD", "SLEWING_FORWARD", ISS_OFF);
    AxisTwoStateSP[HIGH_SPEED].fill("HIGH_SPEED", "HIGH_SPEED", ISS_OFF);
    AxisTwoStateSP[NOT_INITIALISED].fill("NOT_INITIALISED", "NOT_INITIALISED", ISS_ON);
    AxisTwoStateSP.fill(getDeviceName(), "AXIS_TWO_STATE", "Axis two state",
                       DetailedMountInfoPage, IP_RO, ISR_NOFMANY, 60, IPS_IDLE);

    AxisOneEncoderValuesNP[RAW_MICROSTEPS].fill("RAW_MICROSTEPS", "Raw Microsteps", "%.0f", 0, 0xFFFFFF, 1, 0);
    AxisOneEncoderValuesNP[MICROSTEPS_PER_ARCSEC].fill("MICROSTEPS_PER_ARCSEC", "Microsteps/arcsecond",
                 "%.4f", 0, 0xFFFFFF, 1, 0);
    AxisOneEncoderValuesNP[OFFSET_FROM_INITIAL].fill("OFFSET_FROM_INITIAL", "Offset from initial", "%.0f", 0,
                 0xFFFFFF, 1, 0);
    AxisOneEncoderValuesNP[DEGREES_FROM_INITIAL].fill("DEGREES_FROM_INITIAL", "Degrees from initial", "%.2f",
                 -1000.0, 1000.0, 1, 0);

    AxisOneEncoderValuesNP.fill(getDeviceName(), "AXIS1_ENCODER_VALUES",
                       "Axis 1 Encoder values", DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);

    AxisTwoEncoderValuesNP[RAW_MICROSTEPS].fill("RAW_MICROSTEPS", "Raw Microsteps", "%.0f", 0, 0xFFFFFF, 1, 0);
    AxisTwoEncoderValuesNP[MICROSTEPS_PER_ARCSEC].fill("MICROSTEPS_PER_ARCSEC", "Microsteps/arcsecond",
                 "%.4f", 0, 0xFFFFFF, 1, 0);
    AxisTwoEncoderValuesNP[OFFSET_FROM_INITIAL].fill("OFFSET_FROM_INITIAL", "Offset from initial", "%.0f", 0,
                 0xFFFFFF, 1, 0);
    AxisTwoEncoderValuesNP[DEGREES_FROM_INITIAL].fill("DEGREES_FROM_INITIAL", "Degrees from initial", "%.2f",
                 -1000.0, 1000.0, 1, 0);

    AxisTwoEncoderValuesNP.fill(getDeviceName(), "AXIS2_ENCODER_VALUES",
                       "Axis 2 Encoder values", DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);
    // Register any visible before connection properties

    // Slew modes
    SlewModesSP[SLEW_SILENT].fill("SLEW_SILENT", "Silent", ISS_OFF);
    SlewModesSP[SLEW_NORMAL].fill("SLEW_NORMAL", "Normal", ISS_OFF);
    SlewModesSP.fill(getDeviceName(), "TELESCOPE_MOTION_SLEWMODE", "Slew Mode",
                       MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Wedge mode
    WedgeModeSP[WEDGE_SIMPLE].fill("WEDGE_SIMPLE", "Simple wedge", ISS_OFF);
    WedgeModeSP[WEDGE_EQ].fill("WEDGE_EQ", "EQ wedge", ISS_OFF);
    WedgeModeSP[WEDGE_DISABLED].fill("WEDGE_DISABLED", "Disabled", ISS_OFF);
    WedgeModeSP.fill(getDeviceName(), "TELESCOPE_MOTION_WEDGEMODE",
                       "Wedge Mode", MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Track logging mode
    TrackLogModeSP[TRACKLOG_ENABLED].fill("TRACKLOG_ENABLED", "Enable logging", ISS_OFF);
    TrackLogModeSP[TRACKLOG_DISABLED].fill("TRACKLOG_DISABLED", "Disabled", ISS_ON);
    TrackLogModeSP.fill(getDeviceName(), "TELESCOPE_MOTION_TRACKLOGMODE",
                       "Track Logging Mode", MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Guiding rates for RA/DEC axes
    GuidingRatesNP[0].fill("GUIDERA_RATE", "microsteps/seconds (RA)", "%1.3f", 0.00001, 100000.0, 0.00001, 1.0);
    GuidingRatesNP[1].fill("GUIDEDEC_RATE", "microsteps/seconds (Dec)", "%1.3f", 0.00001, 100000.0, 0.00001, 1.0);
    GuidingRatesNP.fill(getDeviceName(), "GUIDE_RATES", "Guide Rates", MOTION_TAB,
                       IP_RW, 60, IPS_IDLE);

    // Tracking rate
    // For Skywatcher Virtuoso:
    // Alt rate: 0.72, Az rate: 0.72, timeout: 1000 msec
    // For Skywatcher Merlin:
    // Alt rate: 0.64, Az rate: 0.64, timeout: 1000 msec
    TrackingValuesNP[0].fill("TRACKING_RATE_ALT", "rate (Alt)", "%1.3f", 0.001, 10.0, 0.000001, 0.64);
    TrackingValuesNP[1].fill("TRACKING_RATE_AZ", "rate (Az)", "%1.3f", 0.001, 10.0, 0.000001, 0.64);
    TrackingValuesNP[2].fill("TRACKING_TIMEOUT", "msec (period)", "%1.3f", 0.001, 10000.0, 0.000001, 1000.0);
    TrackingValuesNP.fill(getDeviceName(), "TRACKING_VALUES", "Tracking Values", MOTION_TAB,
                       IP_RW, 60, IPS_IDLE);

    // Park movement directions
    ParkMovementDirectionSP[PARK_COUNTERCLOCKWISE].fill("PMD_COUNTERCLOCKWISE", "Counterclockwise", ISS_ON);
    ParkMovementDirectionSP[PARK_CLOCKWISE].fill("PMD_CLOCKWISE", "Clockwise", ISS_OFF);
    ParkMovementDirectionSP.fill(getDeviceName(), "PARK_DIRECTION",
                       "Park Direction", MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Park positions
    ParkPositionSP[PARK_NORTH].fill("PARK_NORTH", "North", ISS_ON);
    ParkPositionSP[PARK_EAST].fill("PARK_EAST", "East", ISS_OFF);
    ParkPositionSP[PARK_SOUTH].fill("PARK_SOUTH", "South", ISS_OFF);
    ParkPositionSP[PARK_WEST].fill("PARK_WEST", "West", ISS_OFF);
    ParkPositionSP.fill(getDeviceName(), "PARK_POSITION", "Park Position", MOTION_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Unpark positions
    UnparkPositionSP[PARK_NORTH].fill("UNPARK_NORTH", "North", ISS_OFF);
    UnparkPositionSP[PARK_EAST].fill("UNPARK_EAST", "East", ISS_OFF);
    UnparkPositionSP[PARK_SOUTH].fill("UNPARK_SOUTH", "South", ISS_OFF);
    UnparkPositionSP[PARK_WEST].fill("UNPARK_WEST", "West", ISS_OFF);
    UnparkPositionSP.fill(getDeviceName(), "UNPARK_POSITION", "Unpark Position",
                       MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Guiding support
    initGuiderProperties(getDeviceName(), GUIDE_TAB);
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    return true;
}

void SkywatcherAltAzSimple::ISGetProperties(const char *dev)
{
    IDLog("SkywatcherAltAzSimple::ISGetProperties\n");
    INDI::Telescope::ISGetProperties(dev);

    if (isConnected())
    {
        // Fill in any real values now available MCInit should have been called already
        UpdateDetailedMountInformation(false);

        // Define our connected only properties to the base driver
        // e.g. defineProperty(MyNumberVectorPointer);
        // This will register our properties and send a IDDefXXXX mewssage to any connected clients
        defineProperty(BasicMountInfoTP);
        defineProperty(AxisOneInfoNP);
        defineProperty(AxisOneStateSP);
        defineProperty(AxisTwoInfoNP);
        defineProperty(AxisTwoStateSP);
        defineProperty(AxisOneEncoderValuesNP);
        defineProperty(AxisTwoEncoderValuesNP);
        defineProperty(SlewModesSP);
        defineProperty(WedgeModeSP);
        defineProperty(TrackLogModeSP);
        defineProperty(GuidingRatesNP);
        defineProperty(TrackingValuesNP);
        defineProperty(ParkMovementDirectionSP);
        defineProperty(ParkPositionSP);
        defineProperty(UnparkPositionSP);
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
    }
}

bool SkywatcherAltAzSimple::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                                      char *formats[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // It is for us
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool SkywatcherAltAzSimple::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "GUIDE_RATES") == 0)
        {
            ResetGuidePulses();
            GuidingRatesNP.setState(IPS_OK);
            GuidingRatesNP.update(values, names, n);
            GuidingRatesNP.apply();
            return true;
        }

        if (strcmp(name, "TRACKING_VALUES") == 0)
        {
            TrackingValuesNP.setState(IPS_OK);
            TrackingValuesNP.update(values, names, n);
            TrackingValuesNP.apply();
            return true;
        }

        // Let our driver do sync operation in park position
        if (strcmp(name, "EQUATORIAL_EOD_COORD") == 0)
        {
            double ra  = -1;
            double dec = -100;

            for (int x = 0; x < n; x++)
            {
                INumber *eqp = EqNP.findWidgetByName(names[x]);
                if (eqp == &EqNP[AXIS_RA])
                {
                    ra = values[x];
                }
                else if (eqp == &EqNP[AXIS_DE])
                {
                    dec = values[x];
                }
            }
            if ((ra >= 0) && (ra <= 24) && (dec >= -90) && (dec <= 90))
            {
                ISwitch *sw = CoordSP.findWidgetByName("SYNC");

                if (sw != nullptr && sw->s == ISS_ON && isParked())
                {
                    return Sync(ra, dec);
                }
            }
        }

        processGuiderProperties(name, values, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool SkywatcherAltAzSimple::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    ISwitchVectorProperty *svp;
    svp = getSwitch(name);
    if (svp == nullptr)
    {
        LOGF_WARN("getSwitch failed for %s", name);
    }
    else
    {
        LOGF_DEBUG("getSwitch OK %s", name);
        IUUpdateSwitch(svp, states, names, n);
    }
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // It is for us
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool SkywatcherAltAzSimple::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // It is for us
    }
    // Pass it up the chain
    bool Ret =  INDI::Telescope::ISNewText(dev, name, texts, names, n);

    // The scope config switch must be updated after the config is saved to disk
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (name && std::string(name) == "SCOPE_CONFIG_NAME")
        {
            UpdateScopeConfigSwitch();
        }
    }
    return Ret;
}

void SkywatcherAltAzSimple::UpdateScopeConfigSwitch()
{
    if (!CheckFile(ScopeConfigFileName, false))
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Can't open XML file (%s) for read", ScopeConfigFileName.c_str());
        return;
    }
    LilXML *XmlHandle      = newLilXML();
    FILE *FilePtr          = fopen(ScopeConfigFileName.c_str(), "r");
    XMLEle *RootXmlNode    = nullptr;
    XMLEle *CurrentXmlNode = nullptr;
    XMLAtt *Ap             = nullptr;
    bool DeviceFound       = false;
    char ErrMsg[512];

    RootXmlNode = readXMLFile(FilePtr, XmlHandle, ErrMsg);
    delLilXML(XmlHandle);
    XmlHandle = nullptr;
    if (!RootXmlNode)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to parse XML file (%s): %s", ScopeConfigFileName.c_str(), ErrMsg);
        return;
    }
    if (std::string(tagXMLEle(RootXmlNode)) != ScopeConfigRootXmlNode)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Not a scope config XML file (%s)", ScopeConfigFileName.c_str());
        delXMLEle(RootXmlNode);
        return;
    }
    CurrentXmlNode = nextXMLEle(RootXmlNode, 1);
    // Find the current telescope in the config file
    while (CurrentXmlNode)
    {
        if (std::string(tagXMLEle(CurrentXmlNode)) != ScopeConfigDeviceXmlNode)
        {
            CurrentXmlNode = nextXMLEle(RootXmlNode, 0);
            continue;
        }
        Ap = findXMLAtt(CurrentXmlNode, ScopeConfigNameXmlNode.c_str());
        if (Ap && !strcmp(valuXMLAtt(Ap), getDeviceName()))
        {
            DeviceFound = true;
            break;
        }
        CurrentXmlNode = nextXMLEle(RootXmlNode, 0);
    }
    if (!DeviceFound)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "No a scope config found for %s in the XML file (%s)", getDeviceName(),
               ScopeConfigFileName.c_str());
        delXMLEle(RootXmlNode);
        return;
    }
    // Read the values
    XMLEle *XmlNode       = nullptr;
    XMLEle *DeviceXmlNode = CurrentXmlNode;
    std::string ConfigName;

    for (int i = 1; i < 7; ++i)
    {
        bool Found = true;

        CurrentXmlNode = findXMLEle(DeviceXmlNode, ("config" + std::to_string(i)).c_str());
        if (CurrentXmlNode)
        {
            XmlNode = findXMLEle(CurrentXmlNode, ScopeConfigLabelApXmlNode.c_str());
            if (XmlNode)
            {
                ConfigName = pcdataXMLEle(XmlNode);
            }
        }
        else
        {
            Found = false;
        }
        // Change the switch label
        ISwitch *configSwitch = IUFindSwitch(&ScopeConfigsSP, ("SCOPE_CONFIG" + std::to_string(i)).c_str());

        if (configSwitch != nullptr)
        {
            // The config is not used yet
            if (!Found)
            {
                strncpy(configSwitch->label, ("Config #" + std::to_string(i) + " - Not used").c_str(), MAXINDILABEL);
                continue;
            }
            // Empty switch label
            if (ConfigName.empty())
            {
                strncpy(configSwitch->label, ("Config #" + std::to_string(i) + " - Untitled").c_str(), MAXINDILABEL);
                continue;
            }
            strncpy(configSwitch->label, ("Config #" + std::to_string(i) + " - " + ConfigName).c_str(), MAXINDILABEL);
        }
    }
    delXMLEle(RootXmlNode);
    // Delete the joystick control to get the telescope config switch to the bottom of the page
    deleteProperty("USEJOYSTICK");
    // Recreate the switch control
    deleteProperty(ScopeConfigsSP.name);
    defineProperty(&ScopeConfigsSP);
}

double SkywatcherAltAzSimple::GetSlewRate()
{
    ISwitch *Switch = IUFindOnSwitch(&SlewRateSP);
    double Rate     = *((double *)Switch->aux);

    return Rate;
}

bool SkywatcherAltAzSimple::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    DEBUG(DBG_SCOPE, "SkywatcherAltAzSimple::MoveNS");

    double speed =
        (dir == DIRECTION_NORTH) ? GetSlewRate() * LOW_SPEED_MARGIN / 2 : -GetSlewRate() * LOW_SPEED_MARGIN / 2;
    const char *dirStr = (dir == DIRECTION_NORTH) ? "North" : "South";

    if (IsMerlinMount())
    {
        speed = -speed;
    }

    switch (command)
    {
        case MOTION_START:
            DEBUGF(DBG_SCOPE, "Starting Slew %s", dirStr);
            // Ignore the silent mode because MoveNS() is called by the manual motion UI controls.
            Slew(AXIS2, speed, true);
            moving = true;
            break;

        case MOTION_STOP:
            DEBUGF(DBG_SCOPE, "Stopping Slew %s", dirStr);
            SlowStop(AXIS2);
            moving = false;
            break;
    }

    return true;
}

bool SkywatcherAltAzSimple::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    DEBUG(DBG_SCOPE, "SkywatcherAltAzSimple::MoveWE");

    double speed =
        (dir == DIRECTION_WEST) ? GetSlewRate() * LOW_SPEED_MARGIN / 2 : -GetSlewRate() * LOW_SPEED_MARGIN / 2;
    const char *dirStr = (dir == DIRECTION_WEST) ? "West" : "East";

    speed = -speed;

    switch (command)
    {
        case MOTION_START:
            DEBUGF(DBG_SCOPE, "Starting Slew %s", dirStr);
            // Ignore the silent mode because MoveNS() is called by the manual motion UI controls.
            Slew(AXIS1, speed, true);
            moving = true;
            break;

        case MOTION_STOP:
            DEBUGF(DBG_SCOPE, "Stopping Slew %s", dirStr);
            SlowStop(AXIS1);
            moving = false;
            break;
    }

    return true;
}

double SkywatcherAltAzSimple::GetParkDeltaAz(ParkDirection_t target_direction, ParkPosition_t target_position)
{
    double Result = 0;

    DEBUGF(DBG_SCOPE, "GetParkDeltaAz: direction %d - position: %d", (int)target_direction, (int)target_position);
    // Calculate delta degrees (target: NORTH)
    if (target_position == PARK_NORTH)
    {
        if (target_direction == PARK_COUNTERCLOCKWISE)
        {
            Result = -CurrentAltAz.az;
        }
        else
        {
            Result = 360 - CurrentAltAz.az;
        }
    }
    // Calculate delta degrees (target: EAST)
    if (target_position == PARK_EAST)
    {
        if (target_direction == PARK_COUNTERCLOCKWISE)
        {
            if (CurrentAltAz.az > 0 && CurrentAltAz.az < 90)
                Result = -270 - CurrentAltAz.az;
            else
                Result = -CurrentAltAz.az + 90;
        }
        else
        {
            if (CurrentAltAz.az > 0 && CurrentAltAz.az < 90)
                Result = 90 - CurrentAltAz.az;
            else
                Result = 360 - CurrentAltAz.az + 90;
        }
    }
    // Calculate delta degrees (target: SOUTH)
    if (target_position == PARK_SOUTH)
    {
        if (target_direction == PARK_COUNTERCLOCKWISE)
        {
            if (CurrentAltAz.az > 0 && CurrentAltAz.az < 180)
                Result = -180 - CurrentAltAz.az;
            else
                Result = -CurrentAltAz.az + 180;
        }
        else
        {
            if (CurrentAltAz.az > 0 && CurrentAltAz.az < 180)
                Result = 180 - CurrentAltAz.az;
            else
                Result = 360 - CurrentAltAz.az + 180;
        }
    }
    // Calculate delta degrees (target: WEST)
    if (target_position == PARK_WEST)
    {
        if (target_direction == PARK_COUNTERCLOCKWISE)
        {
            if (CurrentAltAz.az > 0 && CurrentAltAz.az < 270)
                Result = -90 - CurrentAltAz.az;
            else
                Result = -CurrentAltAz.az + 270;
        }
        else
        {
            if (CurrentAltAz.az > 0 && CurrentAltAz.az < 270)
                Result = 270 - CurrentAltAz.az;
            else
                Result = 360 - CurrentAltAz.az + 270;
        }
    }
    if (Result >= 360)
    {
        Result -= 360;
    }
    if (Result <= -360)
    {
        Result += 360;
    }
    return Result;
}

bool SkywatcherAltAzSimple::Park()
{
    DEBUG(DBG_SCOPE, "SkywatcherAltAzSimple::Park");
    ParkPosition_t TargetPosition   = PARK_NORTH;
    ParkDirection_t TargetDirection = PARK_COUNTERCLOCKWISE;
    double DeltaAlt                 = 0;
    double DeltaAz                  = 0;

    // Determinate the target position and direction
    if (ParkPositionSP.findWidgetByName("PARK_NORTH") != nullptr &&
            ParkPositionSP.findWidgetByName("PARK_NORTH")->s == ISS_ON)
    {
        TargetPosition = PARK_NORTH;
    }
    if (ParkPositionSP.findWidgetByName("PARK_EAST") != nullptr &&
            ParkPositionSP.findWidgetByName("PARK_EAST")->s == ISS_ON)
    {
        TargetPosition = PARK_EAST;
    }
    if (ParkPositionSP.findWidgetByName("PARK_SOUTH") != nullptr &&
            ParkPositionSP.findWidgetByName("PARK_SOUTH")->s == ISS_ON)
    {
        TargetPosition = PARK_SOUTH;
    }
    if (ParkPositionSP.findWidgetByName("PARK_WEST") != nullptr &&
            ParkPositionSP.findWidgetByName("PARK_WEST")->s == ISS_ON)
    {
        TargetPosition = PARK_WEST;
    }

    if (ParkMovementDirectionSP.findWidgetByName("PMD_COUNTERCLOCKWISE") != nullptr &&
            ParkMovementDirectionSP.findWidgetByName("PMD_COUNTERCLOCKWISE")->s == ISS_ON)
    {
        TargetDirection = PARK_COUNTERCLOCKWISE;
    }
    if (ParkMovementDirectionSP.findWidgetByName("PMD_CLOCKWISE") != nullptr &&
            ParkMovementDirectionSP.findWidgetByName("PMD_CLOCKWISE")->s == ISS_ON)
    {
        TargetDirection = PARK_CLOCKWISE;
    }
    DeltaAz = GetParkDeltaAz(TargetDirection, TargetPosition);

    // Move the telescope to the desired position
    long AltitudeOffsetMicrosteps = DegreesToMicrosteps(AXIS2, DeltaAlt);
    long AzimuthOffsetMicrosteps  = DegreesToMicrosteps(AXIS1, DeltaAz);

    DEBUGF(DBG_SCOPE, "Parking: Delta altitude %1.2f - delta azimuth %1.2f", DeltaAlt, DeltaAz);
    DEBUGF(DBG_SCOPE, "Parking: Altitude offset %ld microsteps Azimuth offset %ld microsteps",
           AltitudeOffsetMicrosteps, AzimuthOffsetMicrosteps);

    if (SlewModesSP.findWidgetByName("SLEW_NORMAL")->s == ISS_ON)
    {
        SilentSlewMode = false;
    }
    else
    {
        SilentSlewMode = true;
    }
    SlewTo(AXIS1, AzimuthOffsetMicrosteps);
    SlewTo(AXIS2, AltitudeOffsetMicrosteps);

    TrackState = SCOPE_PARKING;
    return true;
}

bool SkywatcherAltAzSimple::UnPark()
{
    DEBUG(DBG_SCOPE, "SkywatcherAltAzSimple::UnPark");

    ParkPosition_t TargetPosition   = PARK_NORTH;
    ParkDirection_t TargetDirection = PARK_COUNTERCLOCKWISE;
    double DeltaAlt                 = 0;
    double DeltaAz                  = 0;

    // Determinate the target position and direction
    if (UnparkPositionSP.findWidgetByName("UNPARK_NORTH") != nullptr &&
            UnparkPositionSP.findWidgetByName("UNPARK_NORTH")->s == ISS_ON)
    {
        TargetPosition = PARK_NORTH;
    }
    if (UnparkPositionSP.findWidgetByName("UNPARK_EAST") != nullptr &&
            UnparkPositionSP.findWidgetByName("UNPARK_EAST")->s == ISS_ON)
    {
        TargetPosition = PARK_EAST;
    }
    if (UnparkPositionSP.findWidgetByName("UNPARK_SOUTH") != nullptr &&
            UnparkPositionSP.findWidgetByName("UNPARK_SOUTH")->s == ISS_ON)
    {
        TargetPosition = PARK_SOUTH;
    }
    if (UnparkPositionSP.findWidgetByName("UNPARK_WEST") != nullptr &&
            UnparkPositionSP.findWidgetByName("UNPARK_WEST")->s == ISS_ON)
    {
        TargetPosition = PARK_WEST;
    }

    // Note: The reverse direction is used for unparking.
    if (ParkMovementDirectionSP.findWidgetByName("PMD_COUNTERCLOCKWISE") != nullptr &&
            ParkMovementDirectionSP.findWidgetByName("PMD_COUNTERCLOCKWISE")->s == ISS_ON)
    {
        TargetDirection = PARK_CLOCKWISE;
    }
    if (ParkMovementDirectionSP.findWidgetByName("PMD_CLOCKWISE") != nullptr &&
            ParkMovementDirectionSP.findWidgetByName("PMD_CLOCKWISE")->s == ISS_ON)
    {
        TargetDirection = PARK_COUNTERCLOCKWISE;
    }
    DeltaAz = GetParkDeltaAz(TargetDirection, TargetPosition);
    // Altitude 3360 points the telescope upwards
    DeltaAlt = CurrentAltAz.alt - 3360;

    // Move the telescope to the desired position
    long AltitudeOffsetMicrosteps = DegreesToMicrosteps(AXIS2, DeltaAlt);
    long AzimuthOffsetMicrosteps  = DegreesToMicrosteps(AXIS1, DeltaAz);

    DEBUGF(DBG_SCOPE, "Unparking: Delta altitude %1.2f - delta azimuth %1.2f", DeltaAlt, DeltaAz);
    DEBUGF(DBG_SCOPE, "Unparking: Altitude offset %ld microsteps Azimuth offset %ld microsteps",
           AltitudeOffsetMicrosteps, AzimuthOffsetMicrosteps);

    if (SlewModesSP.findWidgetByName("SLEW_NORMAL")->s == ISS_ON)
    {
        SilentSlewMode = false;
    }
    else
    {
        SilentSlewMode = true;
    }
    SlewTo(AXIS1, AzimuthOffsetMicrosteps);
    SlewTo(AXIS2, AltitudeOffsetMicrosteps);

    SetParked(false);
    TrackState = SCOPE_SLEWING;
    return true;
}

bool SkywatcherAltAzSimple::ReadScopeStatus()
{
    //    DEBUG(DBG_SCOPE, "SkywatcherAltAzSimple::ReadScopeStatus");

    // leave the following stuff in for the time being it is mostly harmless

    // Quick check of the mount
    if (UpdateCount == 0 && !GetMotorBoardVersion(AXIS1))
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

    if (UpdateCount % 5 == 0)
        UpdateDetailedMountInformation(true);

    UpdateCount++;
    if (TrackState == SCOPE_PARKING)
    {
        if (!IsInMotion(AXIS1) && !IsInMotion(AXIS2))
        {
            SetParked(true);
        }
    }

    // Calculate new RA DEC
    ln_hrz_posn AltAz { 0, 0 };

    AltAz.alt = MicrostepsToDegrees(AXIS2, CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]);
    if (VerboseScopeStatus)
    {
        DEBUGF(DBG_SCOPE, "Axis2 encoder %ld initial %ld alt(degrees) %lf",
               CurrentEncoders[AXIS2], ZeroPositionEncoders[AXIS2], AltAz.alt);
    }
    AltAz.az = MicrostepsToDegrees(AXIS1, CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]);
    CurrentAltAz = AltAz;
    if (VerboseScopeStatus)
    {
        DEBUGF(DBG_SCOPE, "Axis1 encoder %ld initial %ld az(degrees) %lf",
               CurrentEncoders[AXIS1], ZeroPositionEncoders[AXIS1], AltAz.az);
    }

    ln_equ_posn RaDec { 0, 0 };

    RaDec = GetRaDecPosition(AltAz.alt, AltAz.az);
    if (VerboseScopeStatus)
    {
        DEBUGF(DBG_SCOPE, "New RA %lf (hours) DEC %lf (degrees)", RaDec.ra, RaDec.dec);
    }
    LogMessage("STATUS: Ra %lf Dec %lf - Alt %lf Az %lf - microsteps %ld %ld", RaDec.ra, RaDec.dec,
               AltAz.alt, AltAz.az, CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2],
               CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]);
    NewRaDec(RaDec.ra, RaDec.dec);
    VerboseScopeStatus = false;
    return true;
}


bool SkywatcherAltAzSimple::saveConfigItems(FILE *fp)
{
    SlewModesSP.save(fp);
    WedgeModeSP.save(fp);
    TrackLogModeSP.save(fp);
    GuidingRatesNP.save(fp);
    TrackingValuesNP.save(fp);
    ParkMovementDirectionSP.save(fp);
    ParkPositionSP.save(fp);
    UnparkPositionSP.save(fp);

    return INDI::Telescope::saveConfigItems(fp);
}


bool SkywatcherAltAzSimple::Sync(double ra, double dec)
{
    DEBUG(DBG_SCOPE, "SkywatcherAltAzSimple::Sync");

    // Compute a telescope direction vector from the current encoders
    if (!GetEncoder(AXIS1))
        return false;
    if (!GetEncoder(AXIS2))
        return false;

    ln_hrz_posn AltAz { 0, 0 };

    AltAz = GetAltAzPosition(ra, dec);
    double DeltaAz = CurrentAltAz.az - AltAz.az;
    double DeltaAlt = CurrentAltAz.alt - AltAz.alt;

    LogMessage("SYNC: Ra %lf Dec %lf", ra, dec);
    MYDEBUGF(INDI::Logger::DBG_SESSION, "Sync ra: %lf dec: %lf => CurAz: %lf -> NewAz: %lf",
             ra, dec, CurrentAltAz.az, AltAz.az);
    PolarisPositionEncoders[AXIS1] += DegreesToMicrosteps(AXIS1, DeltaAz);
    PolarisPositionEncoders[AXIS2] += DegreesToMicrosteps(AXIS2, DeltaAlt);
    ZeroPositionEncoders[AXIS1] = PolarisPositionEncoders[AXIS1];
    ZeroPositionEncoders[AXIS2] = PolarisPositionEncoders[AXIS2];

    // The tracking seconds should be reset to restart the drift compensation
    ResetTrackingSeconds = true;

    // Stop any movements
    if (TrackState != SCOPE_IDLE && TrackState != SCOPE_PARKED)
    {
        Abort();
    }

    // Might as well do this
    UpdateDetailedMountInformation(true);
    return true;
}


void SkywatcherAltAzSimple::TimerHit()
{
    static bool Slewing    = false;
    static bool Tracking   = false;
    static int ElapsedTime = 0;

    if (!ReadScopeStatus())
    {
        SetTimer(TimeoutDuration);
        return;
    }

    LogMessage("SET TIMER: %d msec", TimeoutDuration);
    SetTimer(TimeoutDuration);
    ElapsedTime += TimeoutDuration;
    if (ElapsedTime >= 5000)
    {
        ElapsedTime        = 0;
        VerboseScopeStatus = true;
    }

    switch (TrackState)
    {
        case SCOPE_SLEWING:
            if (!Slewing)
            {
                LOG_INFO("Slewing started");
                TrackingStartTimer = 0;
            }
            TrackingMsecs   = 0;
            GuideDeltaAlt   = 0;
            GuideDeltaAz    = 0;
            ResetGuidePulses();
            TimeoutDuration = 400;
            Tracking        = false;
            Slewing         = true;
            GuidingPulses.clear();
            if ((AxesStatus[AXIS1].FullStop) && (AxesStatus[AXIS2].FullStop))
            {
                TrackingStartTimer += TimeoutDuration;
                if (TrackingStartTimer < 3000)
                    return;

                if (WedgeModeSP.findWidgetByName("WEDGE_EQ")->s == ISS_ON ||
                        CoordSP.findWidgetByName("TRACK")->s == ISS_ON)

                {
                    // Goto has finished start tracking
                    TrackState = SCOPE_TRACKING;
                }
                else
                {
                    TrackState = SCOPE_IDLE;
                    break;
                }
            }
            break;

        case SCOPE_TRACKING:
        {
            if (!Tracking)
            {
                LOG_INFO("Tracking started");
                TrackingMsecs   = 0;
                TimeoutDuration = (int)TrackingValuesNP.findWidgetByName("TRACKING_TIMEOUT")->value;
                GuideDeltaAlt   = 0;
                GuideDeltaAz    = 0;
                ResetGuidePulses();
            }

            if (moving)
            {
                //                TrackedAltAz  = CurrentAltAz;
                CurrentTrackingTarget.ra = EqNP[AXIS_RA].getValue();
                CurrentTrackingTarget.dec = EqNP[AXIS_DE].getValue();
            }
            else
            {
                // Restart the drift compensation after syncing
                if (ResetTrackingSeconds)
                {
                    ResetTrackingSeconds = false;
                    TrackingMsecs        = 0;
                    GuideDeltaAlt        = 0;
                    GuideDeltaAz         = 0;
                    ResetGuidePulses();
                }
                TrackingMsecs += TimeoutDuration;
                if (TrackingMsecs % 60000 == 0)
                {
                    DEBUGF(INDI::Logger::DBG_SESSION, "Tracking in progress (%d seconds elapsed)", TrackingMsecs / 1000);
                }
                Tracking = true;
                Slewing  = false;
                // Continue or start tracking
                //            ln_hrz_posn AltAz { 0, 0 };
                ln_hrz_posn FutureAltAz { 0, 0 };

                //            AltAz.alt = MicrostepsToDegrees(AXIS2, CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]);
                //            AltAz.az = MicrostepsToDegrees(AXIS1, CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]);
                FutureAltAz = GetAltAzPosition(CurrentTrackingTarget.ra, CurrentTrackingTarget.dec,
                                               (double)TimeoutDuration / 1000);
                //            DEBUGF(DBG_SCOPE,
                //                   "Tracking AXIS1 CurrentEncoder %ld OldTrackingTarget %ld AXIS2 CurrentEncoder %ld OldTrackingTarget "
                //                   "%ld",
                //                   CurrentEncoders[AXIS1], OldTrackingTarget[AXIS1], CurrentEncoders[AXIS2], OldTrackingTarget[AXIS2]);
                //            DEBUGF(DBG_SCOPE,
                //                   "New Tracking Target Altitude %lf degrees %ld microsteps Azimuth %lf degrees %ld microsteps",
                //                   AltAz.alt, DegreesToMicrosteps(AXIS2, AltAz.alt), AltAz.az, DegreesToMicrosteps(AXIS1, AltAz.az));

                // Calculate the auto-guiding delta degrees
                for (auto pulse : GuidingPulses)
                {
                    GuideDeltaAlt += pulse.DeltaAlt;
                    GuideDeltaAz += pulse.DeltaAz;
                }
                GuidingPulses.clear();

                long AltitudeOffsetMicrosteps = DegreesToMicrosteps(AXIS2, FutureAltAz.alt - CurrentAltAz.alt + GuideDeltaAlt);
                long AzimuthOffsetMicrosteps = DegreesToMicrosteps(AXIS1, FutureAltAz.az - CurrentAltAz.az + GuideDeltaAz);

                // When the Alt/Az mount is on the top of an EQ mount, the EQ mount already tracks in
                // sidereal speed. Only autoguiding is enabled in tracking mode.
                if (WedgeModeSP.findWidgetByName("WEDGE_EQ")->s == ISS_ON)
                {
                    AltitudeOffsetMicrosteps = (long)((float)GuidingRatesNP.findWidgetByName("GUIDEDEC_RATE")->value * GuideDeltaAlt);
                    AzimuthOffsetMicrosteps = (long)((float)GuidingRatesNP.findWidgetByName("GUIDERA_RATE")->value * GuideDeltaAz);
                    GuideDeltaAlt = 0;
                    GuideDeltaAz = 0;
                    // Correct the movements of the EQ mount
                    double DeltaAz = CurrentAltAz.az - FutureAltAz.az;
                    double DeltaAlt = CurrentAltAz.alt - FutureAltAz.alt;

                    PolarisPositionEncoders[AXIS1] += DegreesToMicrosteps(AXIS1, DeltaAz);
                    PolarisPositionEncoders[AXIS2] += DegreesToMicrosteps(AXIS2, DeltaAlt);
                    ZeroPositionEncoders[AXIS1] = PolarisPositionEncoders[AXIS1];
                    ZeroPositionEncoders[AXIS2] = PolarisPositionEncoders[AXIS2];
                }

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
                if (AltitudeOffsetMicrosteps < -MicrostepsPerRevolution[AXIS2] / 2)
                {
                    // Going the long way round - send it the other way
                    AltitudeOffsetMicrosteps += MicrostepsPerRevolution[AXIS2];
                }
                if (AzimuthOffsetMicrosteps < -MicrostepsPerRevolution[AXIS1] / 2)
                {
                    // Going the long way round - send it the other way
                    AzimuthOffsetMicrosteps += MicrostepsPerRevolution[AXIS1];
                }

                AltitudeOffsetMicrosteps = (long)((double)AltitudeOffsetMicrosteps * TrackingValuesNP.findWidgetByName("TRACKING_RATE_ALT")->value);
                AzimuthOffsetMicrosteps = (long)((double)AzimuthOffsetMicrosteps * TrackingValuesNP.findWidgetByName("TRACKING_RATE_AZ")->value);

                LogMessage("TRACKING: now Alt %lf Az %lf - future Alt %lf Az %lf - microsteps_diff Alt %ld Az %ld",
                           CurrentAltAz.alt, CurrentAltAz.az, FutureAltAz.alt, FutureAltAz.az,
                           AltitudeOffsetMicrosteps, AzimuthOffsetMicrosteps);

                //            DEBUGF(DBG_SCOPE, "New Tracking Target AltitudeOffset %ld microsteps AzimuthOffset %ld microsteps",
                //                   AltitudeOffsetMicrosteps, AzimuthOffsetMicrosteps);

                if (0 != AzimuthOffsetMicrosteps)
                {
                    SlewTo(AXIS1, AzimuthOffsetMicrosteps, false);
                }
                else
                {
                    // Nothing to do - stop the axis
                    SlowStop(AXIS1);
                }

                if (0 != AltitudeOffsetMicrosteps)
                {
                    SlewTo(AXIS2, AltitudeOffsetMicrosteps, false);
                }
                else
                {
                    // Nothing to do - stop the axis
                    SlowStop(AXIS2);
                }

                DEBUGF(DBG_SCOPE, "Tracking - AXIS1 error %d (offset: %ld) AXIS2 error %d (offset: %ld)",
                       OldTrackingTarget[AXIS1] - CurrentEncoders[AXIS1], AzimuthOffsetMicrosteps,
                       OldTrackingTarget[AXIS2] - CurrentEncoders[AXIS2], AltitudeOffsetMicrosteps);

                OldTrackingTarget[AXIS1] = AzimuthOffsetMicrosteps + CurrentEncoders[AXIS1];
                OldTrackingTarget[AXIS2] = AltitudeOffsetMicrosteps + CurrentEncoders[AXIS2];
            }
            break;
        }
        break;

        default:
            if (Slewing)
            {
                LOG_INFO("Slewing stopped");
            }
            if (Tracking)
            {
                LOG_INFO("Tracking stopped");
            }
            TrackingMsecs   = 0;
            GuideDeltaAlt   = 0;
            GuideDeltaAz    = 0;
            ResetGuidePulses();
            TimeoutDuration = 1000;
            Tracking        = false;
            Slewing         = false;
            GuidingPulses.clear();
            break;
    }
}

bool SkywatcherAltAzSimple::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        // Fill in any real values now available MCInit should have been called already
        UpdateDetailedMountInformation(false);

        // Define our connected only properties to the base driver
        // e.g. defineProperty(MyNumberVectorPointer);
        // This will register our properties and send a IDDefXXXX message to any connected clients
        // I have now idea why I have to do this here as well as in ISGetProperties. It makes me
        // concerned there is a design or implementation flaw somewhere.
        defineProperty(BasicMountInfoTP);
        defineProperty(AxisOneInfoNP);
        defineProperty(AxisOneStateSP);
        defineProperty(AxisTwoInfoNP);
        defineProperty(AxisTwoStateSP);
        defineProperty(AxisOneEncoderValuesNP);
        defineProperty(AxisTwoEncoderValuesNP);
        defineProperty(SlewModesSP);
        defineProperty(WedgeModeSP);
        defineProperty(TrackLogModeSP);
        defineProperty(GuidingRatesNP);
        defineProperty(TrackingValuesNP);
        defineProperty(ParkMovementDirectionSP);
        defineProperty(ParkPositionSP);
        defineProperty(UnparkPositionSP);

        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        return true;
    }
    else
    {
        // Delete any connected only properties from the base driver's list
        // e.g. deleteProperty(MyNumberVector.name);
        deleteProperty(BasicMountInfoTP.getName());
        deleteProperty(AxisOneInfoNP.getName());
        deleteProperty(AxisOneStateSP.getName());
        deleteProperty(AxisTwoInfoNP.getName());
        deleteProperty(AxisTwoStateSP.getName());
        deleteProperty(AxisOneEncoderValuesNP.getName());
        deleteProperty(AxisTwoEncoderValuesNP.getName());
        deleteProperty(SlewModesSP.getName());
        deleteProperty(WedgeModeSP.getName());
        deleteProperty(TrackLogModeSP.getName());
        deleteProperty(GuidingRatesNP.getName());
        deleteProperty(TrackingValuesNP.getName());
        deleteProperty(ParkMovementDirectionSP.getName());
        deleteProperty(ParkPositionSP.getName());
        deleteProperty(UnparkPositionSP.getName());

        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        return true;
    }
}

IPState SkywatcherAltAzSimple::GuideNorth(uint32_t ms)
{
    GuidingPulse Pulse;

    LogMessage("GUIDE NORTH: %1.4f", ms);
    Pulse.DeltaAz = 0;
    Pulse.DeltaAlt = ms;
    GuidingPulses.push_back(Pulse);
    return IPS_OK;
}

IPState SkywatcherAltAzSimple::GuideSouth(uint32_t ms)
{
    GuidingPulse Pulse;

    LogMessage("GUIDE SOUTH: %1.4f", ms);
    Pulse.DeltaAz = 0;
    Pulse.DeltaAlt = -ms;
    GuidingPulses.push_back(Pulse);
    return IPS_OK;
}

IPState SkywatcherAltAzSimple::GuideWest(uint32_t ms)
{
    GuidingPulse Pulse;

    LogMessage("GUIDE WEST: %1.4f", ms);
    Pulse.DeltaAz = ms;
    Pulse.DeltaAlt = 0;
    GuidingPulses.push_back(Pulse);
    return IPS_OK;
}

IPState SkywatcherAltAzSimple::GuideEast(uint32_t ms)
{
    GuidingPulse Pulse;

    LogMessage("GUIDE EAST: %1.4f", ms);
    Pulse.DeltaAz = -ms;
    Pulse.DeltaAlt = 0;
    GuidingPulses.push_back(Pulse);
    return IPS_OK;
}

// Private methods

void SkywatcherAltAzSimple::ResetGuidePulses()
{
    GuidingPulses.clear();
}

int SkywatcherAltAzSimple::recover_tty_reconnect()
{
    if (!RecoverAfterReconnection && !SerialPortName.empty() && !FileExists(SerialPortName))
    {
        RecoverAfterReconnection = true;
        serialConnection->Disconnect();
        serialConnection->Refresh();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (!serialConnection->Connect())
        {
            RecoverAfterReconnection = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (!serialConnection->Connect())
            {
                RecoverAfterReconnection = false;
                return 0;
            }
        }
        SetSerialPort(serialConnection->getPortFD());
        SerialPortName           = serialConnection->port();
        RecoverAfterReconnection = false;
        return 1;
    }
    else
    {
        return -1;
    }
}

int SkywatcherAltAzSimple::skywatcher_tty_read(int fd, char *buf, int nbytes, int timeout, int *nbytes_read)
{
    if (!recover_tty_reconnect())
    {
        return 0;
    }
    return tty_read(fd, buf, nbytes, timeout, nbytes_read);
}

int SkywatcherAltAzSimple::skywatcher_tty_read_section(int fd, char *buf, char stop_char, int timeout, int *nbytes_read)
{
    if (!recover_tty_reconnect())
    {
        return 0;
    }
    return tty_read_section(fd, buf, stop_char, timeout, nbytes_read);
}

int SkywatcherAltAzSimple::skywatcher_tty_write(int fd, const char *buffer, int nbytes, int *nbytes_written)
{
    if (!recover_tty_reconnect())
    {
        return 0;
    }
    return tty_write(fd, buffer, nbytes, nbytes_written);
}

void SkywatcherAltAzSimple::UpdateDetailedMountInformation(bool InformClient)
{
    bool BasicMountInfoHasChanged = false;

    if (std::string(BasicMountInfoTP[MOTOR_CONTROL_FIRMWARE_VERSION].getText()) != std::to_string(MCVersion))
    {
        BasicMountInfoTP[MOTOR_CONTROL_FIRMWARE_VERSION].setText(std::to_string(MCVersion));
        BasicMountInfoHasChanged = true;
    }
    if (std::string(BasicMountInfoTP[MOUNT_CODE].getText()) != std::to_string(MountCode))
    {
        BasicMountInfoTP[MOUNT_CODE].setText(std::to_string(MountCode));
        BasicMountInfoHasChanged = true;
    }
    if (std::string(BasicMountInfoTP[IS_DC_MOTOR].getText()) != std::to_string(IsDCMotor))
    {
        BasicMountInfoTP[IS_DC_MOTOR].setText(std::to_string(IsDCMotor));
        BasicMountInfoHasChanged = true;
    }
    if (BasicMountInfoHasChanged && InformClient)
        BasicMountInfoTP.apply();

    if (MountCode == 128)
        BasicMountInfoTP[MOUNT_NAME].setText("Merlin");
    else if (MountCode >= 129 && MountCode <= 143)
        BasicMountInfoTP[MOUNT_NAME].setText("Az Goto");
    else if (MountCode >= 144 && MountCode <= 159)
        BasicMountInfoTP[MOUNT_NAME].setText("Dob Goto");
    else if (MountCode == 161)
        BasicMountInfoTP[MOUNT_NAME].setText("Virtuoso");
    else if (MountCode >= 160)
        BasicMountInfoTP[MOUNT_NAME].setText("AllView Goto");

    bool AxisOneInfoHasChanged = false;

    if (AxisOneInfoNP[MICROSTEPS_PER_REVOLUTION].value != MicrostepsPerRevolution[0])
    {
        AxisOneInfoNP[MICROSTEPS_PER_REVOLUTION].setValue(MicrostepsPerRevolution[0]);
        AxisOneInfoHasChanged                        = true;
    }
    if (AxisOneInfoNP[STEPPER_CLOCK_FREQUENCY].value != StepperClockFrequency[0])
    {
        AxisOneInfoNP[STEPPER_CLOCK_FREQUENCY].setValue(StepperClockFrequency[0]);
        AxisOneInfoHasChanged                      = true;
    }
    if (AxisOneInfoNP[HIGH_SPEED_RATIO].value != HighSpeedRatio[0])
    {
        AxisOneInfoNP[HIGH_SPEED_RATIO].setValue(HighSpeedRatio[0]);
        AxisOneInfoHasChanged               = true;
    }
    if (AxisOneInfoNP[MICROSTEPS_PER_WORM_REVOLUTION].value != MicrostepsPerWormRevolution[0])
    {
        AxisOneInfoNP[MICROSTEPS_PER_WORM_REVOLUTION].setValue(MicrostepsPerWormRevolution[0]);
        AxisOneInfoHasChanged                             = true;
    }
    if (AxisOneInfoHasChanged && InformClient)
        AxisOneInfoNP.apply();

    bool AxisOneStateHasChanged = false;
    if (AxisOneStateSP[FULL_STOP].getState() != (AxesStatus[0].FullStop ? ISS_ON : ISS_OFF))
    {
        AxisOneStateSP[FULL_STOP].setState(AxesStatus[0].FullStop ? ISS_ON : ISS_OFF);
        AxisOneStateHasChanged    = true;
    }
    if (AxisOneStateSP[SLEWING].getState() != (AxesStatus[0].Slewing ? ISS_ON : ISS_OFF))
    {
        AxisOneStateSP[SLEWING].setState(AxesStatus[0].Slewing ? ISS_ON : ISS_OFF);
        AxisOneStateHasChanged  = true;
    }
    if (AxisOneStateSP[SLEWING_TO].getState() != (AxesStatus[0].SlewingTo ? ISS_ON : ISS_OFF))
    {
        AxisOneStateSP[SLEWING_TO].setState(AxesStatus[0].SlewingTo ? ISS_ON : ISS_OFF);
        AxisOneStateHasChanged     = true;
    }
    if (AxisOneStateSP[SLEWING_FORWARD].getState() != (AxesStatus[0].SlewingForward ? ISS_ON : ISS_OFF))
    {
        AxisOneStateSP[SLEWING_FORWARD].setState(AxesStatus[0].SlewingForward ? ISS_ON : ISS_OFF);
        AxisOneStateHasChanged          = true;
    }
    if (AxisOneStateSP[HIGH_SPEED].getState() != (AxesStatus[0].HighSpeed ? ISS_ON : ISS_OFF))
    {
        AxisOneStateSP[HIGH_SPEED].setState(AxesStatus[0].HighSpeed ? ISS_ON : ISS_OFF);
        AxisOneStateHasChanged     = true;
    }
    if (AxisOneStateSP[NOT_INITIALISED].getState() != (AxesStatus[0].NotInitialized ? ISS_ON : ISS_OFF))
    {
        AxisOneStateSP[NOT_INITIALISED].setState(AxesStatus[0].NotInitialized ? ISS_ON : ISS_OFF);
        AxisOneStateHasChanged          = true;
    }
    if (AxisOneStateHasChanged && InformClient)
        AxisOneStateSP.apply();

    bool AxisTwoInfoHasChanged = false;
    if (AxisTwoInfoNP[MICROSTEPS_PER_REVOLUTION].value != MicrostepsPerRevolution[1])
    {
        AxisTwoInfoNP[MICROSTEPS_PER_REVOLUTION].setValue(MicrostepsPerRevolution[1]);
        AxisTwoInfoHasChanged                        = true;
    }
    if (AxisTwoInfoNP[STEPPER_CLOCK_FREQUENCY].value != StepperClockFrequency[1])
    {
        AxisTwoInfoNP[STEPPER_CLOCK_FREQUENCY].setValue(StepperClockFrequency[1]);
        AxisTwoInfoHasChanged                      = true;
    }
    if (AxisTwoInfoNP[HIGH_SPEED_RATIO].value != HighSpeedRatio[1])
    {
        AxisTwoInfoNP[HIGH_SPEED_RATIO].setValue(HighSpeedRatio[1]);
        AxisTwoInfoHasChanged               = true;
    }
    if (AxisTwoInfoNP[MICROSTEPS_PER_WORM_REVOLUTION].value != MicrostepsPerWormRevolution[1])
    {
        AxisTwoInfoNP[MICROSTEPS_PER_WORM_REVOLUTION].setValue(MicrostepsPerWormRevolution[1]);
        AxisTwoInfoHasChanged                             = true;
    }
    if (AxisTwoInfoHasChanged && InformClient)
        AxisTwoInfoNP.apply();

    bool AxisTwoStateHasChanged = false;
    if (AxisTwoStateSP[FULL_STOP].getState() != (AxesStatus[1].FullStop ? ISS_ON : ISS_OFF))
    {
        AxisTwoStateSP[FULL_STOP].setState(AxesStatus[1].FullStop ? ISS_ON : ISS_OFF);
        AxisTwoStateHasChanged    = true;
    }
    if (AxisTwoStateSP[SLEWING].getState() != (AxesStatus[1].Slewing ? ISS_ON : ISS_OFF))
    {
        AxisTwoStateSP[SLEWING].setState(AxesStatus[1].Slewing ? ISS_ON : ISS_OFF);
        AxisTwoStateHasChanged  = true;
    }
    if (AxisTwoStateSP[SLEWING_TO].getState() != (AxesStatus[1].SlewingTo ? ISS_ON : ISS_OFF))
    {
        AxisTwoStateSP[SLEWING_TO].setState(AxesStatus[1].SlewingTo ? ISS_ON : ISS_OFF);
        AxisTwoStateHasChanged     = true;
    }
    if (AxisTwoStateSP[SLEWING_FORWARD].getState() != (AxesStatus[1].SlewingForward ? ISS_ON : ISS_OFF))
    {
        AxisTwoStateSP[SLEWING_FORWARD].setState(AxesStatus[1].SlewingForward ? ISS_ON : ISS_OFF);
        AxisTwoStateHasChanged          = true;
    }
    if (AxisTwoStateSP[HIGH_SPEED].getState() != (AxesStatus[1].HighSpeed ? ISS_ON : ISS_OFF))
    {
        AxisTwoStateSP[HIGH_SPEED].setState(AxesStatus[1].HighSpeed ? ISS_ON : ISS_OFF);
        AxisTwoStateHasChanged     = true;
    }
    if (AxisTwoStateSP[NOT_INITIALISED].getState() != (AxesStatus[1].NotInitialized ? ISS_ON : ISS_OFF))
    {
        AxisTwoStateSP[NOT_INITIALISED].setState(AxesStatus[1].NotInitialized ? ISS_ON : ISS_OFF);
        AxisTwoStateHasChanged          = true;
    }
    if (AxisTwoStateHasChanged && InformClient)
        AxisTwoStateSP.apply();

    bool AxisOneEncoderValuesHasChanged = false;
    if ((AxisOneEncoderValuesNP[RAW_MICROSTEPS].value != CurrentEncoders[AXIS1]) ||
            (AxisOneEncoderValuesNP[OFFSET_FROM_INITIAL].value != CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]))
    {
        AxisOneEncoderValuesNP[RAW_MICROSTEPS].setValue(CurrentEncoders[AXIS1]);
        AxisOneEncoderValuesNP[MICROSTEPS_PER_ARCSEC].setValue(MicrostepsPerDegree[AXIS1] / 3600.0);
        AxisOneEncoderValuesNP[OFFSET_FROM_INITIAL].setValue(CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]);
        AxisOneEncoderValuesNP[DEGREES_FROM_INITIAL].setValue(MicrostepsToDegrees(AXIS1, CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]));
        AxisOneEncoderValuesHasChanged = true;
    }
    if (AxisOneEncoderValuesHasChanged && InformClient)
        AxisOneEncoderValuesNP.apply();

    bool AxisTwoEncoderValuesHasChanged = false;
    if ((AxisTwoEncoderValuesNP[RAW_MICROSTEPS].value != CurrentEncoders[AXIS2]) ||
            (AxisTwoEncoderValuesNP[OFFSET_FROM_INITIAL].value != CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]))
    {
        AxisTwoEncoderValuesNP[RAW_MICROSTEPS].setValue(CurrentEncoders[AXIS2]);
        AxisTwoEncoderValuesNP[MICROSTEPS_PER_ARCSEC].setValue(MicrostepsPerDegree[AXIS2] / 3600.0);
        AxisTwoEncoderValuesNP[OFFSET_FROM_INITIAL].setValue(CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]);
        AxisTwoEncoderValuesNP[DEGREES_FROM_INITIAL].setValue(MicrostepsToDegrees(AXIS2, CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]));
        AxisTwoEncoderValuesHasChanged = true;
    }
    if (AxisTwoEncoderValuesHasChanged && InformClient)
        AxisTwoEncoderValuesNP.apply();
}


ln_hrz_posn SkywatcherAltAzSimple::GetAltAzPosition(double ra, double dec, double offset_in_sec)
{
    ln_lnlat_posn Location { 0, 0 };
    ln_equ_posn Eq { 0, 0 };
    ln_hrz_posn AltAz { 0, 0 };
    double JulianOffset = offset_in_sec / (24.0 * 60 * 60);

    // Set the current location
    if (WedgeModeSP.findWidgetByName("WEDGE_SIMPLE")->s == ISS_OFF &&
            WedgeModeSP.findWidgetByName("WEDGE_EQ")->s == ISS_OFF)
    {
        Location.lat = LocationNP[LOCATION_LATITUDE].getValue();
        Location.lng = LocationNP[LOCATION_LONGITUDE].getValue();
    }
    else
    {
        if (LocationNP[LOCATION_LATITUDE].value > 0)
        {
            Location.lat = 90;
            Location.lng = 0;
        }
        else
        {
            Location.lat = -90;
            Location.lng = 0;
        }
    }
    Eq.ra  = ra * 360.0 / 24.0;
    Eq.dec = dec;
    ln_get_hrz_from_equ(&Eq, &Location, ln_get_julian_from_sys() + JulianOffset, &AltAz);
    AltAz.az -= 180;
    if (AltAz.az < 0)
        AltAz.az += 360;

    return AltAz;
}


ln_equ_posn SkywatcherAltAzSimple::GetRaDecPosition(double alt, double az)
{
    ln_lnlat_posn Location { 0, 0 };
    ln_equ_posn Eq { 0, 0 };
    ln_hrz_posn AltAz { az, alt };

    // Set the current location
    if (WedgeModeSP.findWidgetByName("WEDGE_SIMPLE")->s == ISS_OFF &&
            WedgeModeSP.findWidgetByName("WEDGE_EQ")->s == ISS_OFF)
    {
        Location.lat = LocationNP[LOCATION_LATITUDE].getValue();
        Location.lng = LocationNP[LOCATION_LONGITUDE].getValue();
    }
    else
    {
        if (LocationNP[LOCATION_LATITUDE].value > 0)
        {
            Location.lat = 90;
            Location.lng = 0;
        }
        else
        {
            Location.lat = -90;
            Location.lng = 0;
        }
    }
    AltAz.az -= 180;
    if (AltAz.az < 0)
        AltAz.az += 360;

    ln_get_equ_from_hrz(&AltAz, &Location, ln_get_julian_from_sys(), &Eq);
    Eq.ra = Eq.ra / 360.0 * 24.0;
    return Eq;
}


void SkywatcherAltAzSimple::LogMessage(const char* format, ...)
{
    if (!format || TrackLogModeSP.findWidgetByName("TRACKLOG_ENABLED")->s == ISS_OFF)
        return;

    va_list Ap;
    va_start(Ap, format);

    char TempStr[512];
    std::ofstream LogFile;

    LogFile.open(TrackLogFileName.c_str(), std::ios::out | std::ios::app);
    if (!LogFile.is_open())
    {
        return;
    }
    vsnprintf(TempStr, sizeof(TempStr), format, Ap);
    LogFile << GetLogTimestamp() << " | " << TempStr << "\n";
    LogFile.close();
    va_end(Ap);
}
