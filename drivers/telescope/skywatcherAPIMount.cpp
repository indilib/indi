/*!
 * \file skywatcherAPIMount.cpp
 *
 * \author Roger James
 * \author Jasem Mutlaq
 * \author Gerry Rozema
 * \author Jean-Luc Geehalel
 * \date 13th November 2013
 *
 * Updated on 2020-12-01 by Jasem Mutlaq
 * Updated on 2021-11-20 by Jasem Mutlaq:
 *  + Fixed tracking.
 *  + Added iterative GOTO.
 *  + Simplified driver and logging.
 *
 * This file contains an implementation in C++ of the Skywatcher API.
 * It is based on work from four sources.
 * A C++ implementation of the API by Roger James.
 * The indi_eqmod driver by Jean-Luc Geehalel.
 * The synscanmount driver by Gerry Rozema.
 * The C# implementation published by Skywatcher/Synta
 */

#include "skywatcherAPIMount.h"

#include "indicom.h"
#include "connectionplugins/connectiontcp.h"
#include "alignment/DriverCommon.h"
#include "connectionplugins/connectionserial.h"

#include <chrono>
#include <thread>

#include <sys/stat.h>

#define DEBUG_PID

using namespace INDI::AlignmentSubsystem;

// We declare an auto pointer to SkywatcherAPIMount.
static std::unique_ptr<SkywatcherAPIMount> SkywatcherAPIMountPtr(new SkywatcherAPIMount());

/* Preset Slew Speeds */
#define SLEWMODES 9
static double SlewSpeeds[SLEWMODES] = { 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 600.0 };

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
SkywatcherAPIMount::SkywatcherAPIMount() : GI(this)
{
    // Set up the logging pointer in SkyWatcherAPI
    pChildTelescope  = this;
    SetTelescopeCapability(TELESCOPE_CAN_PARK |
                           TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME |
                           TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_CAN_CONTROL_TRACK,
                           SLEWMODES);

    m_LastCustomDirection[AXIS1] = m_LastCustomDirection[AXIS2] = 0;
    setVersion(1, 8);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::Handshake()
{
    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::Handshake");
    if (!getActiveConnection()->name().compare("CONNECTION_TCP"))
    {
        tty_set_generic_udp_format(1);
        // reset connection in case of packet loss
        tty_set_auto_reset_udp_session(1);
    }

    SetSerialPort(PortFD);
    bool Result = InitMount();
    DEBUGF(DBG_SCOPE, "SkywatcherAPIMount::Handshake - Result: %d", Result);
    return Result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
const char *SkywatcherAPIMount::getDefaultName()
{
    return "Skywatcher Alt-Az";
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::initProperties()
{
    // Allow the base class to initialise its visible before connection properties
    INDI::Telescope::initProperties();

    for (size_t i = 0; i < SlewRateSP.count(); ++i)
    {
        // sprintf(SlewRateSP.sp[i].label, "%.fx", SlewSpeeds[i]);
        SlewRateSP[i].setLabel(std::to_string(SlewSpeeds[i]) + "x");
        SlewRateSP[i].setAux(&SlewSpeeds[i]);
    }
    SlewRateSP[SlewRateSP.count() - 1].setName("SLEW_MAX");

    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");

    // Add default properties
    addDebugControl();
    addConfigurationControl();

    // Add alignment properties
    InitAlignmentProperties(this);

    // Force the alignment system to always be on
    getSwitch("ALIGNMENT_SUBSYSTEM_ACTIVE")[0].setState(ISS_ON);

    // Set up property variables
    IUFillText(&BasicMountInfoT[MOTOR_CONTROL_FIRMWARE_VERSION], "MOTOR_CONTROL_FIRMWARE_VERSION",
               "Motor control firmware version", "-");
    IUFillText(&BasicMountInfoT[MOUNT_CODE], "MOUNT_CODE", "Mount code", "-");
    IUFillText(&BasicMountInfoT[MOUNT_NAME], "MOUNT_NAME", "Mount name", "-");
    IUFillText(&BasicMountInfoT[IS_DC_MOTOR], "IS_DC_MOTOR", "Is DC motor", "-");
    IUFillTextVector(&BasicMountInfoTP, BasicMountInfoT, 4, getDeviceName(), "BASIC_MOUNT_INFO",
                     "Basic mount information", MountInfoTab, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&AxisOneInfoN[MICROSTEPS_PER_REVOLUTION], "MICROSTEPS_PER_REVOLUTION", "Microsteps per revolution",
                 "%.0f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisOneInfoN[STEPPER_CLOCK_FREQUENCY], "STEPPER_CLOCK_FREQUENCY", "Stepper clock frequency", "%.0f", 0,
                 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisOneInfoN[HIGH_SPEED_RATIO], "HIGH_SPEED_RATIO", "High speed ratio", "%.0f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisOneInfoN[MICROSTEPS_PER_WORM_REVOLUTION], "MICROSTEPS_PER_WORM_REVOLUTION",
                 "Microsteps per worm revolution", "%.0f", 0, 0xFFFFFF, 1, 0);

    IUFillNumberVector(&AxisOneInfoNP, AxisOneInfoN, 4, getDeviceName(), "AXIS_ONE_INFO", "Axis one information",
                       MountInfoTab, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&AxisOneStateS[FULL_STOP], "FULL_STOP", "FULL_STOP", ISS_OFF);
    IUFillSwitch(&AxisOneStateS[SLEWING], "SLEWING", "SLEWING", ISS_OFF);
    IUFillSwitch(&AxisOneStateS[SLEWING_TO], "SLEWING_TO", "SLEWING_TO", ISS_OFF);
    IUFillSwitch(&AxisOneStateS[SLEWING_FORWARD], "SLEWING_FORWARD", "SLEWING_FORWARD", ISS_OFF);
    IUFillSwitch(&AxisOneStateS[HIGH_SPEED], "HIGH_SPEED", "HIGH_SPEED", ISS_OFF);
    IUFillSwitch(&AxisOneStateS[NOT_INITIALISED], "NOT_INITIALISED", "NOT_INITIALISED", ISS_ON);
    IUFillSwitchVector(&AxisOneStateSP, AxisOneStateS, 6, getDeviceName(), "AXIS_ONE_STATE", "Axis one state",
                       MountInfoTab, IP_RO, ISR_NOFMANY, 60, IPS_IDLE);

    IUFillNumber(&AxisTwoInfoN[MICROSTEPS_PER_REVOLUTION], "MICROSTEPS_PER_REVOLUTION", "Microsteps per revolution",
                 "%.0f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisTwoInfoN[STEPPER_CLOCK_FREQUENCY], "STEPPER_CLOCK_FREQUENCY", "Step timer frequency", "%.0f", 0,
                 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisTwoInfoN[HIGH_SPEED_RATIO], "HIGH_SPEED_RATIO", "High speed ratio", "%.0f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisTwoInfoN[MICROSTEPS_PER_WORM_REVOLUTION], "MICROSTEPS_PER_WORM_REVOLUTION",
                 "Microsteps per worm revolution", "%.0f", 0, 0xFFFFFF, 1, 0);

    IUFillNumberVector(&AxisTwoInfoNP, AxisTwoInfoN, 4, getDeviceName(), "AXIS_TWO_INFO", "Axis two information",
                       MountInfoTab, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&AxisTwoStateS[FULL_STOP], "FULL_STOP", "FULL_STOP", ISS_OFF);
    IUFillSwitch(&AxisTwoStateS[SLEWING], "SLEWING", "SLEWING", ISS_OFF);
    IUFillSwitch(&AxisTwoStateS[SLEWING_TO], "SLEWING_TO", "SLEWING_TO", ISS_OFF);
    IUFillSwitch(&AxisTwoStateS[SLEWING_FORWARD], "SLEWING_FORWARD", "SLEWING_FORWARD", ISS_OFF);
    IUFillSwitch(&AxisTwoStateS[HIGH_SPEED], "HIGH_SPEED", "HIGH_SPEED", ISS_OFF);
    IUFillSwitch(&AxisTwoStateS[NOT_INITIALISED], "NOT_INITIALISED", "NOT_INITIALISED", ISS_ON);
    IUFillSwitchVector(&AxisTwoStateSP, AxisTwoStateS, 6, getDeviceName(), "AXIS_TWO_STATE", "Axis two state",
                       MountInfoTab, IP_RO, ISR_NOFMANY, 60, IPS_IDLE);

    IUFillNumber(&AxisOneEncoderValuesN[RAW_MICROSTEPS], "RAW_MICROSTEPS", "Raw Microsteps", "%.0f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisOneEncoderValuesN[MICROSTEPS_PER_ARCSEC], "MICROSTEPS_PER_ARCSEC", "Microsteps/arcsecond",
                 "%.4f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisOneEncoderValuesN[OFFSET_FROM_INITIAL], "OFFSET_FROM_INITIAL", "Offset from initial", "%.0f", 0,
                 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisOneEncoderValuesN[DEGREES_FROM_INITIAL], "DEGREES_FROM_INITIAL", "Degrees from initial", "%.2f",
                 -1000.0, 1000.0, 1, 0);

    IUFillNumberVector(&AxisOneEncoderValuesNP, AxisOneEncoderValuesN, 4, getDeviceName(), "AXIS1_ENCODER_VALUES",
                       "Axis 1 Encoder values", MountInfoTab, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&AxisTwoEncoderValuesN[RAW_MICROSTEPS], "RAW_MICROSTEPS", "Raw Microsteps", "%.0f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisTwoEncoderValuesN[MICROSTEPS_PER_ARCSEC], "MICROSTEPS_PER_ARCSEC", "Microsteps/arcsecond",
                 "%.4f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisTwoEncoderValuesN[OFFSET_FROM_INITIAL], "OFFSET_FROM_INITIAL", "Offset from initial", "%.0f", 0,
                 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisTwoEncoderValuesN[DEGREES_FROM_INITIAL], "DEGREES_FROM_INITIAL", "Degrees from initial", "%.2f",
                 -1000.0, 1000.0, 1, 0);

    IUFillNumberVector(&AxisTwoEncoderValuesNP, AxisTwoEncoderValuesN, 4, getDeviceName(), "AXIS2_ENCODER_VALUES",
                       "Axis 2 Encoder values", MountInfoTab, IP_RO, 60, IPS_IDLE);
    // Register any visible before connection properties

    // Slew modes
    IUFillSwitch(&SlewModesS[SLEW_SILENT], "SLEW_SILENT", "Silent", ISS_OFF);
    IUFillSwitch(&SlewModesS[SLEW_NORMAL], "SLEW_NORMAL", "Normal", ISS_ON);
    IUFillSwitchVector(&SlewModesSP, SlewModesS, 2, getDeviceName(), "TELESCOPE_MOTION_SLEWMODE", "Slew Mode",
                       MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // SoftPEC modes
    IUFillSwitch(&SoftPECModesS[SOFTPEC_ENABLED], "SOFTPEC_ENABLED", "Enable for tracking", ISS_OFF);
    IUFillSwitch(&SoftPECModesS[SOFTPEC_DISABLED], "SOFTPEC_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&SoftPECModesSP, SoftPECModesS, 2, getDeviceName(), "TELESCOPE_MOTION_SOFTPECMODE",
                       "SoftPEC Mode", MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // SoftPEC value for tracking mode
    IUFillNumber(&SoftPecN, "SOFTPEC_VALUE", "degree/minute (Alt)", "%1.3f", 0.001, 1.0, 0.001, 0.009);
    IUFillNumberVector(&SoftPecNP, &SoftPecN, 1, getDeviceName(), "SOFTPEC", "SoftPEC Value", MOTION_TAB, IP_RW, 60,
                       IPS_IDLE);

    // Guiding rates for RA/DEC axes
    IUFillNumber(&GuidingRatesN[0], "GUIDERA_RATE", "arcsec/seconds (RA)", "%1.3f", 1.0, 6000.0, 1.0, 120.0);
    IUFillNumber(&GuidingRatesN[1], "GUIDEDEC_RATE", "arcsec/seconds (Dec)", "%1.3f", 1.0, 6000.0, 1.0, 120.0);
    IUFillNumberVector(&GuidingRatesNP, GuidingRatesN, 2, getDeviceName(), "GUIDE_RATES", "Guide Rates", MOTION_TAB,
                       IP_RW, 60, IPS_IDLE);

    // AUX Encoders
    AUXEncoderSP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_ON);
    AUXEncoderSP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_OFF);
    AUXEncoderSP.fill(getDeviceName(), "AUX_ENCODERS", "AUX Encoders", TRACKING_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    AUXEncoderSP.load();

    // Snap port
    SnapPortSP[INDI_ENABLED].fill("INDI_ENABLED", "On", ISS_OFF);
    SnapPortSP[INDI_DISABLED].fill("INDI_DISABLED", "Off", ISS_ON);
    SnapPortSP.fill(getDeviceName(), "SNAP_PORT", "Snap Port", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // PID Control
    Axis1PIDNP[Propotional].fill("Propotional", "Propotional", "%.2f", 0.1, 100, 1, 0.1);
    Axis1PIDNP[Derivative].fill("Derivative", "Derivative", "%.2f", 0, 500, 10, 0.05);
    Axis1PIDNP[Integral].fill("Integral", "Integral", "%.2f", 0, 500, 10, 0.05);
    Axis1PIDNP.fill(getDeviceName(), "AXIS1_PID", "Axis1 PID", TRACKING_TAB, IP_RW, 60, IPS_IDLE);

    Axis2PIDNP[Propotional].fill("Propotional", "Propotional", "%.2f", 0.1, 100, 1, 0.2);
    Axis2PIDNP[Derivative].fill("Derivative", "Derivative", "%.2f", 0, 100, 10, 0.1);
    Axis2PIDNP[Integral].fill("Integral", "Integral", "%.2f", 0, 100, 10, 0.1);
    Axis2PIDNP.fill(getDeviceName(), "AXIS2_PID", "Axis2 PID", TRACKING_TAB, IP_RW, 60, IPS_IDLE);

    // Dead Zone
    AxisDeadZoneNP[AXIS1].fill("AXIS1", "AZ (steps)", "%.f", 0, 100, 10, 10);
    AxisDeadZoneNP[AXIS2].fill("AXIS2", "AL (steps)", "%.f", 0, 100, 10, 10);
    AxisDeadZoneNP.fill(getDeviceName(), "DEAD_ZONE", "Dead Zone", TRACKING_TAB, IP_RW, 60, IPS_IDLE);

    // Clock Multiplier
    AxisClockNP[AXIS1].fill("AXIS1", "AZ %", "%.f", 1, 200, 10, 100);
    AxisClockNP[AXIS2].fill("AXIS2", "AL %", "%.f", 1, 200, 10, 100);
    AxisClockNP.fill(getDeviceName(), "AXIS_CLOCK", "Clock Rate", TRACKING_TAB, IP_RW, 60, IPS_IDLE);

    // Offsets
    AxisOffsetNP[RAOffset].fill("RAOffset", "RA (deg)", "%.2f", -1, 1, 0.05, 0);
    AxisOffsetNP[DEOffset].fill("DEOffset", "DE (deg)", "%.2f", -1, 1, 0.05, 0);
    AxisOffsetNP[AZSteps].fill("AZEncoder", "AZ (steps)", "%.f", -10000, 10000, 1000, 0);
    AxisOffsetNP[ALSteps].fill("ALEncoder", "AL (steps)", "%.f", -10000, 10000, 1000, -100.0);
    AxisOffsetNP[JulianOffset].fill("JulianOffset", "JD (s)", "%.f", -5, 5, 0.1, 0);
    AxisOffsetNP.fill(getDeviceName(), "AXIS_OFFSET", "Offsets", TRACKING_TAB, IP_RW, 60, IPS_IDLE);

    // Tracking Rate
    Axis1TrackRateNP[TrackDirection].fill("TrackDirection", "West/East", "%.f", 0, 1, 1, 0);
    Axis1TrackRateNP[TrackClockRate].fill("TrackClockRate", "Freq/Step (Hz/s)", "%.f", 0, 16000000, 500000, 0);
    Axis1TrackRateNP.fill(getDeviceName(), "AXIS1TrackRate", "Axis 1 Track", TRACKING_TAB, IP_RW, 60, IPS_IDLE);

    Axis2TrackRateNP[TrackDirection].fill("TrackDirection", "North/South", "%.f", 0, 1, 1, 0);
    Axis2TrackRateNP[TrackClockRate].fill("TrackClockRate", "Freq/Stel (Hz/s)", "%.f", 0, 16000000, 500000, 0);
    Axis2TrackRateNP.fill(getDeviceName(), "AXIS2TrackRate", "Axis 2 Track", TRACKING_TAB, IP_RW, 60, IPS_IDLE);


    MountTypeSP.reset();
    MountTypeSP[MOUNT_ALTAZ].setState(ISS_ON);

    tcpConnection->setDefaultHost("192.168.4.1");
    tcpConnection->setDefaultPort(11880);
    tcpConnection->setConnectionType(Connection::TCP::TYPE_UDP);

    if (strstr(getDeviceName(), "Wired"))
    {
        setActiveConnection(serialConnection);
    }
    else if (strstr(getDeviceName(), "GTi"))
    {
        setActiveConnection(tcpConnection);
        tcpConnection->setLANSearchEnabled(true);
    }

    SetParkDataType(PARK_AZ_ALT_ENCODER);

    // Guiding support
    GI::initProperties(GUIDE_TAB);
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                                   char *formats[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // It is for us
        ProcessAlignmentBLOBProperties(this, name, sizes, blobsizes, blobs, formats, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Check guider interface
    if (GI::processNumber(dev, name, values, names, n))
        return true;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        ProcessAlignmentNumberProperties(this, name, values, names, n);

        if (strcmp(name, "SOFTPEC") == 0)
        {
            SoftPecNP.s = IPS_OK;
            IUUpdateNumber(&SoftPecNP, values, names, n);
            IDSetNumber(&SoftPecNP, nullptr);
            return true;
        }

        if (strcmp(name, "GUIDE_RATES") == 0)
        {
            ResetGuidePulses();
            GuidingRatesNP.s = IPS_OK;
            IUUpdateNumber(&GuidingRatesNP, values, names, n);
            IDSetNumber(&GuidingRatesNP, nullptr);
            return true;
        }

        // Dead Zone
        if (AxisDeadZoneNP.isNameMatch(name))
        {
            AxisDeadZoneNP.update(values, names, n);
            AxisDeadZoneNP.setState(IPS_OK);
            AxisDeadZoneNP.apply();
            saveConfig(true, AxisDeadZoneNP.getName());
            return true;
        }

        // Clock Rate
        if (AxisClockNP.isNameMatch(name))
        {
            AxisClockNP.update(values, names, n);
            AxisClockNP.setState(IPS_OK);
            AxisClockNP.apply();
            saveConfig(true, AxisClockNP.getName());
            return true;
        }

        // Offsets
        if (AxisOffsetNP.isNameMatch(name))
        {
            AxisOffsetNP.update(values, names, n);
            AxisOffsetNP.setState(IPS_OK);
            AxisOffsetNP.apply();
            saveConfig(true, AxisOffsetNP.getName());
            return true;
        }

        // Axis 1
        if (Axis1TrackRateNP.isNameMatch(name))
        {
            Axis1TrackRateNP.update(values, names, n);
            Axis1TrackRateNP.setState(IPS_OK);
            Axis1TrackRateNP.apply();
            saveConfig(true, Axis1TrackRateNP.getName());
            return true;
        }

        // Axis 2
        if (Axis2TrackRateNP.isNameMatch(name))
        {
            Axis2TrackRateNP.update(values, names, n);
            Axis2TrackRateNP.setState(IPS_OK);
            Axis2TrackRateNP.apply();
            saveConfig(true, Axis2TrackRateNP.getName());
            return true;
        }

        // Axis1 PID
        if (Axis1PIDNP.isNameMatch(name))
        {
            Axis1PIDNP.update(values, names, n);
            Axis1PIDNP.setState(IPS_OK);
            Axis1PIDNP.apply();
            saveConfig(Axis1PIDNP);

            m_Controllers[AXIS1].reset(new PID(getPollingPeriod() / 1000, 50, -50,
                                               Axis1PIDNP[Propotional].getValue(),
                                               Axis1PIDNP[Derivative].getValue(),
                                               Axis1PIDNP[Integral].getValue()));
            return true;
        }

        // Axis2 PID
        if (Axis2PIDNP.isNameMatch(name))
        {
            Axis2PIDNP.update(values, names, n);
            Axis2PIDNP.setState(IPS_OK);
            Axis2PIDNP.apply();
            saveConfig(Axis2PIDNP);

            m_Controllers[AXIS2].reset(new PID(getPollingPeriod() / 1000, 50, -50,
                                               Axis2PIDNP[Propotional].getValue(),
                                               Axis2PIDNP[Derivative].getValue(),
                                               Axis2PIDNP[Integral].getValue()));
            return true;
        }


        // Let our driver do sync operation in park position
        if (strcmp(name, "EQUATORIAL_EOD_COORD") == 0)
        {
            double ra  = -1;
            double dec = -100;

            for (int x = 0; x < n; x++)
            {
                if (EqNP[AXIS_RA].isNameMatch(names[x]))
                    ra = values[x];
                else if (EqNP[AXIS_DE].isNameMatch(names[x]))
                    dec = values[x];
            }
            if ((ra >= 0) && (ra <= 24) && (dec >= -90) && (dec <= 90))
            {
                if (CoordSP.isSwitchOn("SYNC") && isParked())
                {
                    return Sync(ra, dec);
                }
            }
        }
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Auxiliary Encoders
        if (AUXEncoderSP.isNameMatch(name))
        {
            AUXEncoderSP.update(states, names, n);
            AUXEncoderSP.setState(IPS_OK);
            AUXEncoderSP.apply();
            auto enabled = AUXEncoderSP.findOnSwitchIndex() == INDI_ENABLED;
            TurnRAEncoder(enabled);
            TurnDEEncoder(enabled);
            saveConfig(AUXEncoderSP);
            return true;
        }

        // Snap Port
        if (SnapPortSP.isNameMatch(name))
        {
            SnapPortSP.update(states, names, n);
            auto enabled = SnapPortSP.findOnSwitchIndex() == INDI_ENABLED;
            toggleSnapPort(enabled);
            if (enabled)
                LOG_INFO("Toggling snap port on...");
            else
                LOG_INFO("Toggling snap port off...");
            SnapPortSP.setState(enabled ? IPS_OK : IPS_IDLE);
            SnapPortSP.apply();
            return true;
        }

        ProcessAlignmentSwitchProperties(this, name, states, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        ProcessAlignmentTextProperties(this, name, texts, names, n);
    }
    // Pass it up the chain
    return  INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::Goto(double ra, double dec)
{
    if (m_IterativeGOTOPending)
    {
        char RAStr[32], DecStr[32];
        fs_sexa(RAStr, m_SkyCurrentRADE.rightascension, 2, 3600);
        fs_sexa(DecStr, m_SkyCurrentRADE.declination, 2, 3600);
        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Iterative GOTO RA %lf DEC %lf (Current Sky RA %s DE %s)", ra, dec, RAStr,
               DecStr);
    }
    else
    {
        if (TrackState != SCOPE_IDLE)
            Abort();

        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "GOTO RA %lf DEC %lf", ra, dec);

        auto onSwitch = CoordSP.findOnSwitch();
        if (onSwitch && (onSwitch->isNameMatch("TRACK") || onSwitch->isNameMatch("SLEW")))
        {
            char RAStr[32], DecStr[32];
            fs_sexa(RAStr, ra, 2, 3600);
            fs_sexa(DecStr, dec, 2, 3600);
            m_SkyTrackingTarget.rightascension  = ra;
            m_SkyTrackingTarget.declination = dec;
            LOGF_INFO("Goto target RA %s DEC %s", RAStr, DecStr);
        }
    }

    INDI::IHorizontalCoordinates AltAz { 0, 0 };
    TelescopeDirectionVector TDV;

    // Transform Celestial to Telescope coordinates.
    // We have no good way to estimate how long will the mount takes to reach target (with deceleration,
    // and not just speed). So we will use iterative GOTO once the first GOTO is complete.
    if (TransformCelestialToTelescope(ra, dec, 0.0, TDV))
    {
        INDI::IEquatorialCoordinates EquatorialCoordinates { 0, 0 };
        AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
        INDI::HorizontalToEquatorial(&AltAz, &m_Location, ln_get_julian_from_sys(), &EquatorialCoordinates);

        char RAStr[32], DecStr[32];
        fs_sexa(RAStr, EquatorialCoordinates.rightascension, 2, 3600);
        fs_sexa(DecStr, EquatorialCoordinates.declination, 2, 3600);

        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Sky -> Mount RA %s DE %s (TDV x %lf y %lf z %lf)", RAStr, DecStr, TDV.x,
               TDV.y, TDV.z);
    }
    else
    {
        // Try a conversion with the stored observatory position if any
        INDI::IEquatorialCoordinates EquatorialCoordinates { 0, 0 };
        EquatorialCoordinates.rightascension  = ra;
        EquatorialCoordinates.declination = dec;
        INDI::EquatorialToHorizontal(&EquatorialCoordinates, &m_Location, ln_get_julian_from_sys(), &AltAz);
        TDV = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
        switch (GetApproximateMountAlignment())
        {
            case ZENITH:
                break;

            case NORTH_CELESTIAL_POLE:
                // Rotate the TDV coordinate system clockwise (negative) around the y axis by 90 minus
                // the (positive)observatory latitude. The vector itself is rotated anticlockwise
                TDV.RotateAroundY(m_Location.latitude - 90.0);
                break;

            case SOUTH_CELESTIAL_POLE:
                // Rotate the TDV coordinate system anticlockwise (positive) around the y axis by 90 plus
                // the (negative)observatory latitude. The vector itself is rotated clockwise
                TDV.RotateAroundY(m_Location.latitude + 90.0);
                break;
        }
        AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
    }

    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT,
           "Sky -> Mount AZ %lf° (%ld) AL %lf° (%ld)",
           AltAz.azimuth,
           DegreesToMicrosteps(AXIS1, AltAz.azimuth),
           AltAz.altitude,
           DegreesToMicrosteps(AXIS2, AltAz.altitude));

    // Update the current encoder positions
    GetEncoder(AXIS1);
    GetEncoder(AXIS2);

    long AzimuthOffsetMicrosteps  = DegreesToMicrosteps(AXIS1,
                                    AltAz.azimuth) + ZeroPositionEncoders[AXIS1] - (CurrentEncoders[AXIS1] - AxisOffsetNP[AZSteps].getValue());
    long AltitudeOffsetMicrosteps = DegreesToMicrosteps(AXIS2,
                                    AltAz.altitude) + ZeroPositionEncoders[AXIS2] - (CurrentEncoders[AXIS2] - AxisOffsetNP[ALSteps].getValue());

    if (AzimuthOffsetMicrosteps > MicrostepsPerRevolution[AXIS1] / 2)
    {
        // Going the long way round - send it the other way
        AzimuthOffsetMicrosteps -= MicrostepsPerRevolution[AXIS1];
    }

    // Do I need to take out any complete revolutions before I do this test?
    if (AltitudeOffsetMicrosteps > MicrostepsPerRevolution[AXIS2] / 2)
    {
        // Going the long way round - send it the other way
        AltitudeOffsetMicrosteps -= MicrostepsPerRevolution[AXIS2];
    }

    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Current Axis1 %ld microsteps (Zero %ld) Axis2 %ld microsteps (Zero %ld)",
           CurrentEncoders[AXIS1], ZeroPositionEncoders[AXIS1], CurrentEncoders[AXIS2], ZeroPositionEncoders[AXIS2]);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Azimuth offset %ld microsteps | Altitude offset %ld microsteps",
           AzimuthOffsetMicrosteps, AltitudeOffsetMicrosteps);

    SilentSlewMode = (IUFindSwitch(&SlewModesSP, "SLEW_SILENT") != nullptr
                      && IUFindSwitch(&SlewModesSP, "SLEW_SILENT")->s == ISS_ON);

    if(TrackState != SCOPE_SLEWING)
    {
        long deltaAz  = DegreesToMicrosteps(AXIS1, AZ_BACKLASH_DEG);
        long deltaAlt = DegreesToMicrosteps(AXIS2, ALT_BACKLASH_DEG);
        AzimuthOffsetMicrosteps -= deltaAz;
        AltitudeOffsetMicrosteps -= deltaAlt;
    }
    SlewTo(AXIS1, AzimuthOffsetMicrosteps);
    SlewTo(AXIS2, AltitudeOffsetMicrosteps);

    TrackState = SCOPE_SLEWING;

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void SkywatcherAPIMount::ISGetProperties(const char *dev)
{
    INDI::Telescope::ISGetProperties(dev);

    if (isConnected())
    {
        // Fill in any real values now available MCInit should have been called already
        UpdateDetailedMountInformation(false);

        // Define our connected only properties to the base driver
        // e.g. defineProperty(MyNumberVectorPointer);
        // This will register our properties and send a IDDefXXXX mewssage to any connected clients
        defineProperty(&BasicMountInfoTP);
        defineProperty(&AxisOneInfoNP);
        defineProperty(&AxisOneStateSP);
        defineProperty(&AxisTwoInfoNP);
        defineProperty(&AxisTwoStateSP);
        defineProperty(&AxisOneEncoderValuesNP);
        defineProperty(&AxisTwoEncoderValuesNP);
        defineProperty(&SlewModesSP);
        defineProperty(&SoftPECModesSP);
        defineProperty(&SoftPecNP);
        defineProperty(&GuidingRatesNP);
        defineProperty(GuideNSNP);
        defineProperty(GuideWENP);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
double SkywatcherAPIMount::GetSlewRate()
{
    ISwitch *Switch = SlewRateSP.findOnSwitch();
    return *(static_cast<double *>(Switch->aux));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    double speed =
        (dir == DIRECTION_NORTH) ? GetSlewRate() * LOW_SPEED_MARGIN / 2 : -GetSlewRate() * LOW_SPEED_MARGIN / 2;
    const char *dirStr = (dir == DIRECTION_NORTH) ? "North" : "South";

    switch (command)
    {
        case MOTION_START:
            DEBUGF(DBG_SCOPE, "Starting Slew %s", dirStr);
            // Ignore the silent mode because MoveNS() is called by the manual motion UI controls.
            Slew(AXIS2, speed, true);
            m_ManualMotionActive = true;
            break;

        case MOTION_STOP:
            DEBUGF(DBG_SCOPE, "Stopping Slew %s", dirStr);
            SlowStop(AXIS2);
            break;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    double speed =
        (dir == DIRECTION_WEST) ? -GetSlewRate() * LOW_SPEED_MARGIN / 2 : GetSlewRate() * LOW_SPEED_MARGIN / 2;
    const char *dirStr = (dir == DIRECTION_WEST) ? "West" : "East";

    switch (command)
    {
        case MOTION_START:
            DEBUGF(DBG_SCOPE, "Starting Slew %s", dirStr);
            // Ignore the silent mode because MoveNS() is called by the manual motion UI controls.
            Slew(AXIS1, speed, true);
            m_ManualMotionActive = true;
            break;

        case MOTION_STOP:
            DEBUGF(DBG_SCOPE, "Stopping Slew %s", dirStr);
            SlowStop(AXIS1);
            break;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::Park()
{
    // Move the telescope to the desired position
    long AltitudeOffsetMicrosteps = GetAxis2Park() - CurrentEncoders[AXIS2];
    long AzimuthOffsetMicrosteps  = GetAxis1Park() - CurrentEncoders[AXIS1];
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT,
           "Parking: Altitude offset %ld microsteps Azimuth offset %ld microsteps", AltitudeOffsetMicrosteps,
           AzimuthOffsetMicrosteps);

    if (IUFindSwitch(&SlewModesSP, "SLEW_SILENT") != nullptr && IUFindSwitch(&SlewModesSP, "SLEW_SILENT")->s == ISS_ON)
    {
        SilentSlewMode = true;
    }
    else
    {
        SilentSlewMode = false;
    }
    SlewTo(AXIS1, AzimuthOffsetMicrosteps);
    SlewTo(AXIS2, AltitudeOffsetMicrosteps);

    TrackState = SCOPE_PARKING;
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::UnPark()
{
    SetParked(false);
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::SetTrackEnabled(bool enabled)
{
    if (enabled)
    {
        TrackState = SCOPE_TRACKING;
        resetTracking();
        m_SkyTrackingTarget.rightascension = EqNP[AXIS_RA].getValue();
        m_SkyTrackingTarget.declination = EqNP[AXIS_DE].getValue();
    }
    else
    {
        TrackState = SCOPE_IDLE;
        SlowStop(AXIS1);
        SlowStop(AXIS2);
        TrackState = SCOPE_IDLE;

        if (GuideNSNP.getState() == IPS_BUSY || GuideWENP.getState() == IPS_BUSY)
        {
            GuideNSNP.setState(IPS_IDLE);
            GuideWENP.setState(IPS_IDLE);
            GuideNSNP[0].setValue(0);
            GuideNSNP[1].setValue(0);
            GuideWENP[0].setValue(0);
            GuideWENP[1].setValue(0);
            GuideNSNP.apply();
            GuideWENP.apply();
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::ReadScopeStatus()
{
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

    bool resetTrackingTimers = false;

    // Calculate new RA DEC
    INDI::IHorizontalCoordinates AltAz { 0, 0 };
    AltAz.azimuth = range360(MicrostepsToDegrees(AXIS1,
                             CurrentEncoders[AXIS1] - AxisOffsetNP[AZSteps].getValue() - ZeroPositionEncoders[AXIS1]));
    AltAz.altitude = MicrostepsToDegrees(AXIS2,
                                         CurrentEncoders[AXIS2] - AxisOffsetNP[ALSteps].getValue() - ZeroPositionEncoders[AXIS2]);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis1 encoder %ld (Zero %ld) -> AZ %lf°",
           CurrentEncoders[AXIS1], ZeroPositionEncoders[AXIS1], AltAz.azimuth);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis2 encoder %ld (Zero %ld) -> AL %lf°",
           CurrentEncoders[AXIS2], ZeroPositionEncoders[AXIS2], AltAz.altitude);

    // Update current horizontal coords.
    m_MountAltAz = AltAz;

    // Get equatorial coords.
    getCurrentRADE(AltAz, m_SkyCurrentRADE);
    char RAStr[32], DecStr[32];
    fs_sexa(RAStr, m_SkyCurrentRADE.rightascension, 2, 3600);
    fs_sexa(DecStr, m_SkyCurrentRADE.declination, 2, 3600);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Sky RA %s DE %s", RAStr, DecStr);

    if (TrackState == SCOPE_SLEWING)
    {
        if ((AxesStatus[AXIS1].FullStop) && (AxesStatus[AXIS2].FullStop))
        {
            // If iterative GOTO was already engaged, stop it.
            if (m_IterativeGOTOPending)
                m_IterativeGOTOPending = false;
            // If not, then perform the iterative GOTO once more.
            else
            {
                m_IterativeGOTOPending = true;
                return Goto(m_SkyTrackingTarget.rightascension, m_SkyTrackingTarget.declination);
            }

            auto onSwitch = CoordSP.findOnSwitch();
            if (onSwitch && (onSwitch->isNameMatch("TRACK")))
            {
                // Goto has finished start tracking
                TrackState = SCOPE_TRACKING;
                resetTrackingTimers = true;
                LOG_INFO("Tracking started.");
            }
            else
            {
                TrackState = SCOPE_IDLE;
            }
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (!IsInMotion(AXIS1) && !IsInMotion(AXIS2))
        {
            SlowStop(AXIS1);
            SlowStop(AXIS2);
            SetParked(true);
        }
    }

    if (resetTrackingTimers)
        resetTracking();

    NewRaDec(m_SkyCurrentRADE.rightascension, m_SkyCurrentRADE.declination);
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::getCurrentAltAz(INDI::IHorizontalCoordinates &altaz)
{
    // Update Axis Position
    if (GetEncoder(AXIS1) && GetEncoder(AXIS2))
    {
        altaz.azimuth = range360(MicrostepsToDegrees(AXIS1,
                                 CurrentEncoders[AXIS1] - AxisOffsetNP[AZSteps].getValue() - ZeroPositionEncoders[AXIS1]));
        altaz.altitude = MicrostepsToDegrees(AXIS2,
                                             CurrentEncoders[AXIS2] - AxisOffsetNP[ALSteps].getValue() - ZeroPositionEncoders[AXIS2]);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::getCurrentRADE(INDI::IHorizontalCoordinates altaz, INDI::IEquatorialCoordinates &rade)
{
    TelescopeDirectionVector TDV = TelescopeDirectionVectorFromAltitudeAzimuth(altaz);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "TDV x %lf y %lf z %lf", TDV.x, TDV.y, TDV.z);

    double RightAscension, Declination;
    if (!TransformTelescopeToCelestial(TDV, RightAscension, Declination))
    {
        TelescopeDirectionVector RotatedTDV(TDV);
        switch (GetApproximateMountAlignment())
        {
            case ZENITH:
                break;

            case NORTH_CELESTIAL_POLE:
                // Rotate the TDV coordinate system anticlockwise (positive) around the y axis by 90 minus
                // the (positive)observatory latitude. The vector itself is rotated clockwise
                RotatedTDV.RotateAroundY(90.0 - m_Location.latitude);
                AltitudeAzimuthFromTelescopeDirectionVector(RotatedTDV, altaz);
                break;

            case SOUTH_CELESTIAL_POLE:
                // Rotate the TDV coordinate system clockwise (negative) around the y axis by 90 plus
                // the (negative)observatory latitude. The vector itself is rotated anticlockwise
                RotatedTDV.RotateAroundY(-90.0 - m_Location.latitude);
                AltitudeAzimuthFromTelescopeDirectionVector(RotatedTDV, altaz);
                break;
        }

        INDI::IEquatorialCoordinates EquatorialCoordinates;
        INDI::HorizontalToEquatorial(&altaz, &m_Location, ln_get_julian_from_sys(), &EquatorialCoordinates);
        RightAscension = EquatorialCoordinates.rightascension;
        Declination = EquatorialCoordinates.declination;
    }

    rade.rightascension = RightAscension;
    rade.declination = Declination;
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::saveConfigItems(FILE *fp)
{
    SaveAlignmentConfigProperties(fp);

    Axis1PIDNP.save(fp);
    Axis2PIDNP.save(fp);
    AxisDeadZoneNP.save(fp);
    AxisClockNP.save(fp);
    AxisOffsetNP.save(fp);
    Axis1TrackRateNP.save(fp);
    Axis2TrackRateNP.save(fp);
    AUXEncoderSP.save(fp);

    return INDI::Telescope::saveConfigItems(fp);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::Sync(double ra, double dec)
{
    DEBUG(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "SkywatcherAPIMount::Sync");

    // Compute a telescope direction vector from the current encoders
    if (!GetEncoder(AXIS1))
        return false;
    if (!GetEncoder(AXIS2))
        return false;

    // Syncing is treated specially when the telescope position is known in park position to spare
    // "a huge-jump point" in the alignment model.
    if (isParked())
    {
        INDI::IHorizontalCoordinates AltAz { 0, 0 };
        TelescopeDirectionVector TDV;

        if (TransformCelestialToTelescope(ra, dec, 0.0, TDV))
        {
            AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
            double OrigAlt = AltAz.altitude;
            ZeroPositionEncoders[AXIS1] = PolarisPositionEncoders[AXIS1] - DegreesToMicrosteps(AXIS1, AltAz.azimuth);
            ZeroPositionEncoders[AXIS2] = PolarisPositionEncoders[AXIS2] - DegreesToMicrosteps(AXIS2, AltAz.altitude);
            LOGF_INFO("Sync (Alt: %lf Az: %lf) in park position", OrigAlt, AltAz.azimuth);
            GetAlignmentDatabase().clear();
            return true;
        }
    }

    // Might as well do this
    UpdateDetailedMountInformation(true);

    INDI::IHorizontalCoordinates AltAz { 0, 0 };

    AltAz.azimuth = range360(MicrostepsToDegrees(AXIS1,
                             CurrentEncoders[AXIS1] - AxisOffsetNP[AZSteps].getValue() - ZeroPositionEncoders[AXIS1]));
    AltAz.altitude = MicrostepsToDegrees(AXIS2,
                                         CurrentEncoders[AXIS2] - AxisOffsetNP[ALSteps].getValue() - ZeroPositionEncoders[AXIS2]);

    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis1 encoder %ld initial %ld AZ %lf°",
           CurrentEncoders[AXIS1], ZeroPositionEncoders[AXIS1], AltAz.azimuth);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis2 encoder %ld initial %ld AL %lf°",
           CurrentEncoders[AXIS2], ZeroPositionEncoders[AXIS2], AltAz.altitude);

    AlignmentDatabaseEntry NewEntry;
    NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
    NewEntry.RightAscension     = ra;
    NewEntry.Declination        = dec;
    NewEntry.TelescopeDirection = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
    NewEntry.PrivateDataSize    = 0;

    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "New sync point Date %lf RA %lf DEC %lf TDV(x %lf y %lf z %lf)",
           NewEntry.ObservationJulianDate, NewEntry.RightAscension, NewEntry.Declination, NewEntry.TelescopeDirection.x,
           NewEntry.TelescopeDirection.y, NewEntry.TelescopeDirection.z);

    m_IterativeGOTOPending = false;

    if (!CheckForDuplicateSyncPoint(NewEntry))
    {
        GetAlignmentDatabase().push_back(NewEntry);

        // Tell the client about size change
        UpdateSize();

        // Tell the math plugin to reinitialise
        Initialise(this);

        // Force read before restarting
        ReadScopeStatus();

        // The tracking seconds should be reset to restart the drift compensation
        resetTracking();

        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::Abort()
{
    //DEBUG(DBG_SCOPE, "SkywatcherAPIMount::Abort");
    m_IterativeGOTOPending = false;
    SlowStop(AXIS1);
    SlowStop(AXIS2);
    TrackState = SCOPE_IDLE;

    if (GuideNSNP.getState() == IPS_BUSY || GuideWENP.getState() == IPS_BUSY)
    {
        GuideNSNP.setState(IPS_IDLE);
        GuideWENP.setState(IPS_IDLE);
        GuideNSNP[0].setValue(0);
        GuideNSNP[1].setValue(0);
        GuideWENP[0].setValue(0);
        GuideWENP[1].setValue(0);

        LOG_INFO("Guide aborted.");
        GuideNSNP.apply();
        GuideWENP.apply();

        return true;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void SkywatcherAPIMount::TimerHit()
{
    // Call parent to read ReadScopeStatus
    INDI::Telescope::TimerHit();

    switch (TrackState)
    {
        case SCOPE_SLEWING:
            GuideDeltaAlt   = 0;
            GuideDeltaAz    = 0;
            ResetGuidePulses();
            GuidingPulses.clear();
            break;

        case SCOPE_TRACKING:
        {
            // Check if manual motion in progress but we stopped
            if (m_ManualMotionActive && !IsInMotion(AXIS1) && !IsInMotion(AXIS2))
            {
                m_ManualMotionActive = false;
                resetTracking();
                m_SkyTrackingTarget.rightascension = EqNP[AXIS_RA].getValue();
                m_SkyTrackingTarget.declination = EqNP[AXIS_DE].getValue();
            }
            // If we're manually moving by WESN controls, update the tracking coordinates.
            if (m_ManualMotionActive)
            {
                break;
            }
            else
            {
                // TODO add switch to select between them
                //trackUsingPID();
                trackUsingPredictiveRates();
            }

            break;
        }

        default:
            GuideDeltaAlt   = 0;
            GuideDeltaAz    = 0;
            ResetGuidePulses();
            GuidingPulses.clear();
            break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::updateLocation(double latitude, double longitude, double elevation)
{
    //DEBUG(DBG_SCOPE, "SkywatcherAPIMount::updateLocation");
    UpdateLocation(latitude, longitude, elevation);
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        // Update location if loaded already from config
        if (m_Location.longitude > 0)
            UpdateLocation(m_Location.latitude, m_Location.longitude, m_Location.elevation);

        // Fill in any real values now available MCInit should have been called already
        UpdateDetailedMountInformation(false);

        // Define our connected only properties to the base driver
        // e.g. defineProperty(MyNumberVectorPointer);
        // This will register our properties and send a IDDefXXXX message to any connected clients
        // I have now idea why I have to do this here as well as in ISGetProperties. It makes me
        // concerned there is a design or implementation flaw somewhere.
        defineProperty(&BasicMountInfoTP);
        defineProperty(&AxisOneInfoNP);
        defineProperty(&AxisOneStateSP);
        defineProperty(&AxisTwoInfoNP);
        defineProperty(&AxisTwoStateSP);
        defineProperty(&AxisOneEncoderValuesNP);
        defineProperty(&AxisTwoEncoderValuesNP);
        defineProperty(&SlewModesSP);
        defineProperty(&SoftPECModesSP);
        defineProperty(&SoftPecNP);
        defineProperty(&GuidingRatesNP);
        defineProperty(Axis1PIDNP);
        defineProperty(Axis2PIDNP);
        defineProperty(AxisDeadZoneNP);
        defineProperty(AxisClockNP);
        defineProperty(AxisOffsetNP);
        defineProperty(Axis1TrackRateNP);
        defineProperty(Axis2TrackRateNP);

        if (HasAuxEncoders())
        {
            // Since config is loaded, let's use this as starting point.
            // We should not force AUX encoders if the user explicitly turned them off.
            auto enabled = AUXEncoderSP[INDI_ENABLED].getState() == ISS_ON;
            LOGF_INFO("AUX encoders detected. Turning %s...", enabled ? "on" : "off");
            TurnRAEncoder(enabled);
            TurnDEEncoder(enabled);
            defineProperty(AUXEncoderSP);
        }

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(GetAxis1Park());
            SetAxis2ParkDefault(GetAxis2Park());
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(0x800000);
            SetAxis2Park(0x800000);
            SetAxis1ParkDefault(0x800000);
            SetAxis2ParkDefault(0x800000);
        }

        if (isParked())
        {
            SetEncoder(AXIS1, GetAxis1Park());
            SetEncoder(AXIS2, GetAxis2Park());

        }
        return true;
    }
    else
    {
        // Delete any connected only properties from the base driver's list
        // e.g. deleteProperty(MyNumberVector.name);
        deleteProperty(BasicMountInfoTP.name);
        deleteProperty(AxisOneInfoNP.name);
        deleteProperty(AxisOneStateSP.name);
        deleteProperty(AxisTwoInfoNP.name);
        deleteProperty(AxisTwoStateSP.name);
        deleteProperty(AxisOneEncoderValuesNP.name);
        deleteProperty(AxisTwoEncoderValuesNP.name);
        deleteProperty(SlewModesSP.name);
        deleteProperty(SoftPECModesSP.name);
        deleteProperty(SoftPecNP.name);
        deleteProperty(GuidingRatesNP.name);
        deleteProperty(Axis1PIDNP);
        deleteProperty(Axis2PIDNP);
        deleteProperty(AxisDeadZoneNP);
        deleteProperty(AxisClockNP);
        deleteProperty(AxisOffsetNP);
        deleteProperty(Axis1TrackRateNP);
        deleteProperty(Axis2TrackRateNP);

        if (HasAuxEncoders())
            deleteProperty(AUXEncoderSP);

        return true;
    }

    GI::updateProperties();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
IPState SkywatcherAPIMount::GuideNorth(uint32_t ms)
{
    GuidingPulse Pulse;

    CalculateGuidePulses();
    Pulse.DeltaAz = NorthPulse.DeltaAz;
    Pulse.DeltaAlt = NorthPulse.DeltaAlt;
    Pulse.Duration = ms;
    Pulse.OriginalDuration = ms;
    GuidingPulses.push_back(Pulse);
    return IPS_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
IPState SkywatcherAPIMount::GuideSouth(uint32_t ms)
{
    GuidingPulse Pulse;

    CalculateGuidePulses();
    Pulse.DeltaAz = -NorthPulse.DeltaAz;
    Pulse.DeltaAlt = -NorthPulse.DeltaAlt;
    Pulse.Duration = ms;
    Pulse.OriginalDuration = ms;
    GuidingPulses.push_back(Pulse);
    return IPS_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
IPState SkywatcherAPIMount::GuideWest(uint32_t ms)
{
    GuidingPulse Pulse;

    CalculateGuidePulses();
    Pulse.DeltaAz = WestPulse.DeltaAz;
    Pulse.DeltaAlt = WestPulse.DeltaAlt;
    Pulse.Duration = ms;
    Pulse.OriginalDuration = ms;
    GuidingPulses.push_back(Pulse);
    return IPS_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
IPState SkywatcherAPIMount::GuideEast(uint32_t ms)
{
    GuidingPulse Pulse;

    CalculateGuidePulses();
    Pulse.DeltaAz = -WestPulse.DeltaAz;
    Pulse.DeltaAlt = -WestPulse.DeltaAlt;
    Pulse.Duration = ms;
    Pulse.OriginalDuration = ms;
    GuidingPulses.push_back(Pulse);
    return IPS_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void SkywatcherAPIMount::CalculateGuidePulses()
{
    if (NorthPulse.Duration != 0 || WestPulse.Duration != 0)
        return;

    // Calculate the west reference delta
    // Note: The RA is multiplied by 3.75 (90/24) to be more comparable with DEC values.
    const double WestRate = IUFindNumber(&GuidingRatesNP, "GUIDERA_RATE")->value / 10 * -1.0 / 60 / 60 * 3.75 / 100;

    ConvertGuideCorrection(WestRate, 0, WestPulse.DeltaAlt, WestPulse.DeltaAz);
    WestPulse.Duration = 1;

    // Calculate the north reference delta
    // Note: By some reason, it must be multiplied by 100 to match with the RA values.
    const double NorthRate = IUFindNumber(&GuidingRatesNP, "GUIDEDEC_RATE")->value / 10 * 1.0 / 60 / 60 * 100 / 100;

    ConvertGuideCorrection(0, NorthRate, NorthPulse.DeltaAlt, NorthPulse.DeltaAz);
    NorthPulse.Duration = 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void SkywatcherAPIMount::ResetGuidePulses()
{
    NorthPulse.Duration = 0;
    WestPulse.Duration = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void SkywatcherAPIMount::ConvertGuideCorrection(double delta_ra, double delta_dec, double &delta_alt, double &delta_az)
{
    INDI::IHorizontalCoordinates OldAltAz { 0, 0 };
    INDI::IHorizontalCoordinates NewAltAz { 0, 0 };
    TelescopeDirectionVector OldTDV;
    TelescopeDirectionVector NewTDV;

    TransformCelestialToTelescope(m_SkyTrackingTarget.rightascension, m_SkyTrackingTarget.declination, 0.0, OldTDV);
    AltitudeAzimuthFromTelescopeDirectionVector(OldTDV, OldAltAz);
    TransformCelestialToTelescope(m_SkyTrackingTarget.rightascension + delta_ra,
                                  m_SkyTrackingTarget.declination + delta_dec, 0.0, NewTDV);
    AltitudeAzimuthFromTelescopeDirectionVector(NewTDV, NewAltAz);
    delta_alt = NewAltAz.altitude - OldAltAz.altitude;
    delta_az = NewAltAz.azimuth - OldAltAz.azimuth;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void SkywatcherAPIMount::SkywatcherMicrostepsFromTelescopeDirectionVector(
    const TelescopeDirectionVector TelescopeDirectionVector, long &Axis1Microsteps, long &Axis2Microsteps)
{
    // For the time being I assume that all skywathcer mounts share the same encoder conventions
    double Axis1Angle = 0;
    double Axis2Angle = 0;
    SphericalCoordinateFromTelescopeDirectionVector(TelescopeDirectionVector, Axis1Angle,
            TelescopeDirectionVectorSupportFunctions::CLOCKWISE, Axis1Angle,
            FROM_AZIMUTHAL_PLANE);

    Axis1Microsteps = RadiansToMicrosteps(AXIS1, Axis1Angle);
    Axis2Microsteps = RadiansToMicrosteps(AXIS2, Axis2Angle);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
const TelescopeDirectionVector
SkywatcherAPIMount::TelescopeDirectionVectorFromSkywatcherMicrosteps(long Axis1Microsteps, long Axis2Microsteps)
{
    // For the time being I assume that all skywathcer mounts share the same encoder conventions
    double Axis1Angle = MicrostepsToRadians(AXIS1, Axis1Microsteps);
    double Axis2Angle = MicrostepsToRadians(AXIS2, Axis2Microsteps);
    return TelescopeDirectionVectorFromSphericalCoordinate(
               Axis1Angle, TelescopeDirectionVectorSupportFunctions::CLOCKWISE, Axis2Angle, FROM_AZIMUTHAL_PLANE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void SkywatcherAPIMount::UpdateDetailedMountInformation(bool InformClient)
{
    bool BasicMountInfoHasChanged = false;

    if (std::string(BasicMountInfoT[MOTOR_CONTROL_FIRMWARE_VERSION].text) != std::to_string(MCVersion))
    {
        IUSaveText(&BasicMountInfoT[MOTOR_CONTROL_FIRMWARE_VERSION], std::to_string(MCVersion).c_str());
        BasicMountInfoHasChanged = true;
    }
    if (std::string(BasicMountInfoT[MOUNT_CODE].text) != std::to_string(MountCode))
    {
        IUSaveText(&BasicMountInfoT[MOUNT_CODE], std::to_string(MountCode).c_str());
        SetApproximateMountAlignmentFromMountType(ALTAZ);
        BasicMountInfoHasChanged = true;
    }
    if (std::string(BasicMountInfoT[IS_DC_MOTOR].text) != std::to_string(IsDCMotor))
    {
        IUSaveText(&BasicMountInfoT[IS_DC_MOTOR], std::to_string(IsDCMotor).c_str());
        BasicMountInfoHasChanged = true;
    }
    if (BasicMountInfoHasChanged && InformClient)
        IDSetText(&BasicMountInfoTP, nullptr);

    IUSaveText(&BasicMountInfoT[MOUNT_NAME], mountTypeToString(MountCode));

    bool AxisOneInfoHasChanged = false;

    if (AxisOneInfoN[MICROSTEPS_PER_REVOLUTION].value != MicrostepsPerRevolution[0])
    {
        AxisOneInfoN[MICROSTEPS_PER_REVOLUTION].value = MicrostepsPerRevolution[0];
        AxisOneInfoHasChanged                        = true;
    }
    if (AxisOneInfoN[STEPPER_CLOCK_FREQUENCY].value != StepperClockFrequency[0])
    {
        AxisOneInfoN[STEPPER_CLOCK_FREQUENCY].value = StepperClockFrequency[0];
        AxisOneInfoHasChanged                      = true;
    }
    if (AxisOneInfoN[HIGH_SPEED_RATIO].value != HighSpeedRatio[0])
    {
        AxisOneInfoN[HIGH_SPEED_RATIO].value = HighSpeedRatio[0];
        AxisOneInfoHasChanged               = true;
    }
    if (AxisOneInfoN[MICROSTEPS_PER_WORM_REVOLUTION].value != MicrostepsPerWormRevolution[0])
    {
        AxisOneInfoN[MICROSTEPS_PER_WORM_REVOLUTION].value = MicrostepsPerWormRevolution[0];
        AxisOneInfoHasChanged                             = true;
    }
    if (AxisOneInfoHasChanged && InformClient)
        IDSetNumber(&AxisOneInfoNP, nullptr);

    bool AxisOneStateHasChanged = false;
    if (AxisOneStateS[FULL_STOP].s != (AxesStatus[0].FullStop ? ISS_ON : ISS_OFF))
    {
        AxisOneStateS[FULL_STOP].s = AxesStatus[0].FullStop ? ISS_ON : ISS_OFF;
        AxisOneStateHasChanged    = true;
    }
    if (AxisOneStateS[SLEWING].s != (AxesStatus[0].Slewing ? ISS_ON : ISS_OFF))
    {
        AxisOneStateS[SLEWING].s = AxesStatus[0].Slewing ? ISS_ON : ISS_OFF;
        AxisOneStateHasChanged  = true;
    }
    if (AxisOneStateS[SLEWING_TO].s != (AxesStatus[0].SlewingTo ? ISS_ON : ISS_OFF))
    {
        AxisOneStateS[SLEWING_TO].s = AxesStatus[0].SlewingTo ? ISS_ON : ISS_OFF;
        AxisOneStateHasChanged     = true;
    }
    if (AxisOneStateS[SLEWING_FORWARD].s != (AxesStatus[0].SlewingForward ? ISS_ON : ISS_OFF))
    {
        AxisOneStateS[SLEWING_FORWARD].s = AxesStatus[0].SlewingForward ? ISS_ON : ISS_OFF;
        AxisOneStateHasChanged          = true;
    }
    if (AxisOneStateS[HIGH_SPEED].s != (AxesStatus[0].HighSpeed ? ISS_ON : ISS_OFF))
    {
        AxisOneStateS[HIGH_SPEED].s = AxesStatus[0].HighSpeed ? ISS_ON : ISS_OFF;
        AxisOneStateHasChanged     = true;
    }
    if (AxisOneStateS[NOT_INITIALISED].s != (AxesStatus[0].NotInitialized ? ISS_ON : ISS_OFF))
    {
        AxisOneStateS[NOT_INITIALISED].s = AxesStatus[0].NotInitialized ? ISS_ON : ISS_OFF;
        AxisOneStateHasChanged          = true;
    }
    if (AxisOneStateHasChanged && InformClient)
        IDSetSwitch(&AxisOneStateSP, nullptr);

    bool AxisTwoInfoHasChanged = false;
    if (AxisTwoInfoN[MICROSTEPS_PER_REVOLUTION].value != MicrostepsPerRevolution[1])
    {
        AxisTwoInfoN[MICROSTEPS_PER_REVOLUTION].value = MicrostepsPerRevolution[1];
        AxisTwoInfoHasChanged                        = true;
    }
    if (AxisTwoInfoN[STEPPER_CLOCK_FREQUENCY].value != StepperClockFrequency[1])
    {
        AxisTwoInfoN[STEPPER_CLOCK_FREQUENCY].value = StepperClockFrequency[1];
        AxisTwoInfoHasChanged                      = true;
    }
    if (AxisTwoInfoN[HIGH_SPEED_RATIO].value != HighSpeedRatio[1])
    {
        AxisTwoInfoN[HIGH_SPEED_RATIO].value = HighSpeedRatio[1];
        AxisTwoInfoHasChanged               = true;
    }
    if (AxisTwoInfoN[MICROSTEPS_PER_WORM_REVOLUTION].value != MicrostepsPerWormRevolution[1])
    {
        AxisTwoInfoN[MICROSTEPS_PER_WORM_REVOLUTION].value = MicrostepsPerWormRevolution[1];
        AxisTwoInfoHasChanged                             = true;
    }
    if (AxisTwoInfoHasChanged && InformClient)
        IDSetNumber(&AxisTwoInfoNP, nullptr);

    bool AxisTwoStateHasChanged = false;
    if (AxisTwoStateS[FULL_STOP].s != (AxesStatus[1].FullStop ? ISS_ON : ISS_OFF))
    {
        AxisTwoStateS[FULL_STOP].s = AxesStatus[1].FullStop ? ISS_ON : ISS_OFF;
        AxisTwoStateHasChanged    = true;
    }
    if (AxisTwoStateS[SLEWING].s != (AxesStatus[1].Slewing ? ISS_ON : ISS_OFF))
    {
        AxisTwoStateS[SLEWING].s = AxesStatus[1].Slewing ? ISS_ON : ISS_OFF;
        AxisTwoStateHasChanged  = true;
    }
    if (AxisTwoStateS[SLEWING_TO].s != (AxesStatus[1].SlewingTo ? ISS_ON : ISS_OFF))
    {
        AxisTwoStateS[SLEWING_TO].s = AxesStatus[1].SlewingTo ? ISS_ON : ISS_OFF;
        AxisTwoStateHasChanged     = true;
    }
    if (AxisTwoStateS[SLEWING_FORWARD].s != (AxesStatus[1].SlewingForward ? ISS_ON : ISS_OFF))
    {
        AxisTwoStateS[SLEWING_FORWARD].s = AxesStatus[1].SlewingForward ? ISS_ON : ISS_OFF;
        AxisTwoStateHasChanged          = true;
    }
    if (AxisTwoStateS[HIGH_SPEED].s != (AxesStatus[1].HighSpeed ? ISS_ON : ISS_OFF))
    {
        AxisTwoStateS[HIGH_SPEED].s = AxesStatus[1].HighSpeed ? ISS_ON : ISS_OFF;
        AxisTwoStateHasChanged     = true;
    }
    if (AxisTwoStateS[NOT_INITIALISED].s != (AxesStatus[1].NotInitialized ? ISS_ON : ISS_OFF))
    {
        AxisTwoStateS[NOT_INITIALISED].s = AxesStatus[1].NotInitialized ? ISS_ON : ISS_OFF;
        AxisTwoStateHasChanged          = true;
    }
    if (AxisTwoStateHasChanged && InformClient)
        IDSetSwitch(&AxisTwoStateSP, nullptr);

    bool AxisOneEncoderValuesHasChanged = false;
    if ((AxisOneEncoderValuesN[RAW_MICROSTEPS].value != CurrentEncoders[AXIS1]) ||
            (AxisOneEncoderValuesN[OFFSET_FROM_INITIAL].value != CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]))
    {
        AxisOneEncoderValuesN[RAW_MICROSTEPS].value = CurrentEncoders[AXIS1];
        AxisOneEncoderValuesN[MICROSTEPS_PER_ARCSEC].value = MicrostepsPerDegree[AXIS1] / 3600.0;
        AxisOneEncoderValuesN[OFFSET_FROM_INITIAL].value = CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1];
        AxisOneEncoderValuesN[DEGREES_FROM_INITIAL].value =
            MicrostepsToDegrees(AXIS1, CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]);
        AxisOneEncoderValuesHasChanged = true;
    }
    if (AxisOneEncoderValuesHasChanged && InformClient)
        IDSetNumber(&AxisOneEncoderValuesNP, nullptr);

    bool AxisTwoEncoderValuesHasChanged = false;
    if ((AxisTwoEncoderValuesN[RAW_MICROSTEPS].value != CurrentEncoders[AXIS2]) ||
            (AxisTwoEncoderValuesN[OFFSET_FROM_INITIAL].value != CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]))
    {
        AxisTwoEncoderValuesN[RAW_MICROSTEPS].value      = CurrentEncoders[AXIS2];
        AxisTwoEncoderValuesN[MICROSTEPS_PER_ARCSEC].value = MicrostepsPerDegree[AXIS2] / 3600.0;
        AxisTwoEncoderValuesN[OFFSET_FROM_INITIAL].value = CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2];
        AxisTwoEncoderValuesN[DEGREES_FROM_INITIAL].value =
            MicrostepsToDegrees(AXIS2, CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]);
        AxisTwoEncoderValuesHasChanged = true;
    }
    if (AxisTwoEncoderValuesHasChanged && InformClient)
        IDSetNumber(&AxisTwoEncoderValuesNP, nullptr);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::SetCurrentPark()
{
    SetAxis1Park(CurrentEncoders[AXIS1]);
    SetAxis2Park(CurrentEncoders[AXIS2]);
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::SetDefaultPark()
{
    // Zero azimuth looking north/south (depending on hemisphere)
    SetAxis1Park(ZeroPositionEncoders[AXIS1]);
    SetAxis2Park(ZeroPositionEncoders[AXIS2]);
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/// Restart the drift compensation after syncing or after stopping manual motion
//////////////////////////////////////////////////////////////////////////////////////////////////
void SkywatcherAPIMount::resetTracking()
{
    m_TrackingRateTimer.restart();
    GuideDeltaAlt = 0;
    GuideDeltaAz = 0;
    m_Controllers[AXIS_AZ].reset(new PID(std::max(0.001, getPollingPeriod() / 1000.), 1000, -1000,
                                         Axis1PIDNP[Propotional].getValue(),
                                         Axis1PIDNP[Derivative].getValue(),
                                         Axis1PIDNP[Integral].getValue()));
    m_Controllers[AXIS_AZ]->setIntegratorLimits(-1000, 1000);
    m_Controllers[AXIS_ALT].reset(new PID(std::max(0.001, getPollingPeriod() / 1000.), 1000, -1000,
                                          Axis2PIDNP[Propotional].getValue(),
                                          Axis2PIDNP[Derivative].getValue(),
                                          Axis2PIDNP[Integral].getValue()));
    m_Controllers[AXIS_ALT]->setIntegratorLimits(-1000, 1000);
    ResetGuidePulses();
}

/////////////////////////////////////////////////////////////////////////////////////
/// Calculate and set T1 Preset from Clock Frequency and rate in arcsecs/s
/////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::trackByRate(AXISID axis, double rate)
{
    if (std::abs(rate) > 0 && rate == m_LastTrackRate[axis])
        return true;

    m_LastTrackRate[axis] = rate;

    // If we are already stopped and rate is zero, we immediately return
    if (AxesStatus[axis].FullStop && rate == 0)
        return true;
    // If rate is zero, or direction changed then we should stop.
    else if (!AxesStatus[axis].FullStop && (rate == 0 || (AxesStatus[AXIS1].SlewingForward && rate < 0)
                                            || (!AxesStatus[AXIS1].SlewingForward && rate > 0)))
    {
        SlowStop(axis);
        LOGF_DEBUG("Tracking -> %s direction change.", (axis == AXIS1 ? "Axis 1" : "Axis 2"));
        return true;
    }

    char Direction = rate > 0 ? '0' : '1';
    auto stepsPerSecond = static_cast<uint32_t>(std::abs(rate * (axis == AXIS1 ?
                          AxisOneEncoderValuesN[MICROSTEPS_PER_ARCSEC].value : AxisTwoEncoderValuesN[MICROSTEPS_PER_ARCSEC].value)));
    auto clockRate = (StepperClockFrequency[axis] / std::max(1u, stepsPerSecond));

    SetClockTicksPerMicrostep(axis, clockRate);
    if (AxesStatus[axis].FullStop)
    {
        LOGF_DEBUG("Tracking -> %s restart.", (axis == AXIS1 ? "Axis 1" : "Axis 2"));
        SetAxisMotionMode(axis, '1', Direction);
        StartAxisMotion(axis);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::trackUsingPID()
{
    // Continue or start tracking
    // Calculate where the mount needs to be in POLLMS time
    // TODO may need to make this longer to get a meaningful result
    //double JulianOffset = (getCurrentPollingPeriod() / 1000) / (24.0 * 60 * 60);
    TelescopeDirectionVector TDV;
    INDI::IHorizontalCoordinates AltAz { 0, 0 };

    // We modify the SkyTrackingTarget for non-sidereal objects (Moon or Sun)
    // The Moon and Sun appear to move eastward (increasing RA) relative to the stars
    // because their westward motion due to Earth's rotation is slower than the sidereal rate.
    if (TrackModeSP[TRACK_LUNAR].getState() == ISS_ON)
    {
        // TRACKRATE_LUNAR: how many arcsecs/sec the Moon moves westward (apparent motion)
        // TRACKRATE_SIDEREAL: how many arcsecs/sec the stars move westward (apparent motion)
        // Since the Moon moves slower westward, it effectively moves eastward relative to stars
        double dRA = (TRACKRATE_SIDEREAL - TRACKRATE_LUNAR) * m_TrackingRateTimer.elapsed() / 1000.0;
        m_SkyTrackingTarget.rightascension += dRA / (3600.0 * 15.0);
        m_TrackingRateTimer.restart();
    }
    else if (TrackModeSP[TRACK_SOLAR].getState() == ISS_ON)
    {
        // Similar logic: Sun moves slower westward than stars, so it moves eastward relative to stars
        double dRA = (TRACKRATE_SIDEREAL - TRACKRATE_SOLAR) * m_TrackingRateTimer.elapsed() / 1000.0;
        m_SkyTrackingTarget.rightascension += dRA / (3600.0 * 15.0);
        m_TrackingRateTimer.restart();
    }

    auto ra = m_SkyTrackingTarget.rightascension + AxisOffsetNP[RAOffset].getValue() / 15.0;
    auto de = m_SkyTrackingTarget.declination + AxisOffsetNP[DEOffset].getValue();
    auto JDOffset = AxisOffsetNP[JulianOffset].getValue() / 86400.0;

    if (TransformCelestialToTelescope(ra, de, JDOffset, TDV))
    {
        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "TDV x %lf y %lf z %lf", TDV.x, TDV.y, TDV.z);
        AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
    }
    else
    {
        INDI::IEquatorialCoordinates EquatorialCoordinates { ra, de };
        INDI::EquatorialToHorizontal(&EquatorialCoordinates, &m_Location, ln_get_julian_from_sys() + JDOffset, &AltAz);
    }

    DEBUGF(DBG_SCOPE,
           "New Tracking Target AZ %lf° (%ld microsteps) AL %lf° (%ld microsteps) ",
           AltAz.azimuth,
           DegreesToMicrosteps(AXIS1, AltAz.azimuth),
           AltAz.altitude,
           DegreesToMicrosteps(AXIS2, AltAz.altitude));

    // Calculate the auto-guiding delta degrees
    double DeltaAlt = 0;
    double DeltaAz = 0;

    getGuidePulses(DeltaAz, DeltaAlt);

    long SetPoint[2] = {0, 0}, Measurement[2] = {0, 0}, Error[2] = {0, 0};
    double TrackingRate[2] = {0, 0};

    SetPoint[AXIS1] = DegreesToMicrosteps(AXIS1,  AltAz.azimuth + GuideDeltaAz);
    Measurement[AXIS1] = CurrentEncoders[AXIS1] - AxisOffsetNP[AZSteps].getValue() - ZeroPositionEncoders[AXIS1];

    SetPoint[AXIS2] = DegreesToMicrosteps(AXIS2, AltAz.altitude + GuideDeltaAlt);
    Measurement[AXIS2] = CurrentEncoders[AXIS2] - AxisOffsetNP[ALSteps].getValue() - ZeroPositionEncoders[AXIS2];

    // Going the long way round - send it the other way
    while (SetPoint[AXIS1] > MicrostepsPerRevolution[AXIS1] / 2)
        SetPoint[AXIS1] -= MicrostepsPerRevolution[AXIS1];

    while (SetPoint[AXIS2] > MicrostepsPerRevolution[AXIS2] / 2)
        SetPoint[AXIS2] -= MicrostepsPerRevolution[AXIS2];

    Error[AXIS1] = SetPoint[AXIS1] - Measurement[AXIS1];
    Error[AXIS2] = SetPoint[AXIS2] - Measurement[AXIS2];

    auto Axis1CustomClockRate = Axis1TrackRateNP[TrackClockRate].getValue();

    if (!AxesStatus[AXIS1].FullStop && (
                (Axis1CustomClockRate == 0 && ((AxesStatus[AXIS1].SlewingForward && (Error[AXIS1] < -AxisDeadZoneNP[AXIS1].getValue())) ||
                        (!AxesStatus[AXIS1].SlewingForward && (Error[AXIS1] > AxisDeadZoneNP[AXIS1].getValue())))) ||
                (Axis1CustomClockRate > 0 && Axis1TrackRateNP[TrackDirection].getValue() != m_LastCustomDirection[AXIS1])))
    {
        m_LastCustomDirection[AXIS1] = Axis1TrackRateNP[TrackDirection].getValue();
        // Direction change whilst axis running
        // Abandon tracking for this clock tick
        LOG_DEBUG("Tracking -> AXIS1 direction change.");
        LOGF_DEBUG("AXIS1 Setpoint %d Measurement %d Error %d Rate %f",
                   SetPoint[AXIS1],
                   Measurement[AXIS1],
                   Error[AXIS1],
                   TrackingRate[AXIS1]);
        SlowStop(AXIS1);
    }
    else
    {
        TrackingRate[AXIS1] = m_Controllers[AXIS1]->calculate(SetPoint[AXIS1], Measurement[AXIS1]);
        char Direction = TrackingRate[AXIS1] > 0 ? '0' : '1';
        TrackingRate[AXIS1] = std::fabs(TrackingRate[AXIS1]);
        if (TrackingRate[AXIS1] != 0)
        {
            auto clockRate = (StepperClockFrequency[AXIS1] / TrackingRate[AXIS1]) * (AxisClockNP[AXIS1].getValue() / 100.0);

            if (Axis1CustomClockRate > 0)
            {
                clockRate = Axis1CustomClockRate;
                Direction = Axis1TrackRateNP[TrackDirection].getValue() == 0 ? '0' : '1';
            }

            LOGF_DEBUG("AXIS1 Setpoint %d Measurement %d Error %d Rate %f Freq %f Dir %s",
                       SetPoint[AXIS1],
                       Measurement[AXIS1],
                       Error[AXIS1],
                       TrackingRate[AXIS1],
                       clockRate,
                       Direction == '0' ? "Forward" : "Backward");
#ifdef DEBUG_PID
            LOGF_DEBUG("Tracking AZ P: %f I: %f D: %f",
                       m_Controllers[AXIS1]->propotionalTerm(),
                       m_Controllers[AXIS1]->integralTerm(),
                       m_Controllers[AXIS1]->derivativeTerm());
#endif

            SetClockTicksPerMicrostep(AXIS1, clockRate);
            if (AxesStatus[AXIS1].FullStop)
            {
                LOG_DEBUG("Tracking -> AXIS1 restart.");
                SetAxisMotionMode(AXIS1, '1', Direction);
                StartAxisMotion(AXIS1);
            }
        }
    }


    auto Axis2CustomClockRate = Axis2TrackRateNP[TrackClockRate].getValue();

    if (!AxesStatus[AXIS2].FullStop && (
                (Axis2CustomClockRate == 0 && ((AxesStatus[AXIS2].SlewingForward && (Error[AXIS2] < -AxisDeadZoneNP[AXIS2].getValue())) ||
                        (!AxesStatus[AXIS2].SlewingForward && (Error[AXIS2] > AxisDeadZoneNP[AXIS2].getValue())))) ||
                (Axis2CustomClockRate > 0 && Axis2TrackRateNP[TrackDirection].getValue() != m_LastCustomDirection[AXIS2])))
    {
        m_LastCustomDirection[AXIS2] = Axis2TrackRateNP[TrackDirection].getValue();

        LOG_DEBUG("Tracking -> AXIS2 direction change.");
        LOGF_DEBUG("AXIS2 Setpoint %d Measurement %d Error %d Rate %f",
                   SetPoint[AXIS2],
                   Measurement[AXIS2],
                   Error[AXIS2],
                   TrackingRate[AXIS2]);
        SlowStop(AXIS2);
    }
    else
    {
        TrackingRate[AXIS2] = m_Controllers[AXIS2]->calculate(SetPoint[AXIS2], Measurement[AXIS2]);
        char Direction = TrackingRate[AXIS2] > 0 ? '0' : '1';
        TrackingRate[AXIS2] = std::fabs(TrackingRate[AXIS2]);
        if (TrackingRate[AXIS2] != 0)
        {
            auto clockRate = StepperClockFrequency[AXIS2] / TrackingRate[AXIS2] * (AxisClockNP[AXIS2].getValue() / 100.0);

            if (Axis2CustomClockRate > 0)
            {
                clockRate = Axis2CustomClockRate;
                Direction = Axis2TrackRateNP[TrackDirection].getValue() == 0 ? '0' : '1';
            }

            LOGF_DEBUG("AXIS2 Setpoint %d Measurement %d Error %d Rate %f Freq %f Dir %s",
                       SetPoint[AXIS2],
                       Measurement[AXIS2],
                       Error[AXIS2],
                       TrackingRate[AXIS2],
                       clockRate,
                       Error[AXIS2] > 0 ? "Forward" : "Backward");
#ifdef DEBUG_PID
            LOGF_DEBUG("Tracking AZ P: %f I: %f D: %f",
                       m_Controllers[AXIS2]->propotionalTerm(),
                       m_Controllers[AXIS2]->integralTerm(),
                       m_Controllers[AXIS2]->derivativeTerm());
#endif

            SetClockTicksPerMicrostep(AXIS2, clockRate);
            if (AxesStatus[AXIS2].FullStop)
            {
                LOG_DEBUG("Tracking -> AXIS2 restart.");
                SetAxisMotionMode(AXIS2, '1', Direction);
                StartAxisMotion(AXIS2);
            }
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::trackUsingPredictiveRates()
{
    TelescopeDirectionVector TDV;
    TelescopeDirectionVector futureTDV;
    TelescopeDirectionVector pastTDV;
    INDI::IHorizontalCoordinates targetMountAxisCoordinates { 0, 0 };
    INDI::IHorizontalCoordinates pastMountAxisCoordinates { 0, 0 };
    INDI::IHorizontalCoordinates futureMountAxisCoordinates { 0, 0 };
    // time step for tracking rate estimation in seconds
    double timeStep { 5.0 };
    // The same in days
    double JDoffset { timeStep / (60 * 60 * 24) } ;

    // We modify the SkyTrackingTarget for non-sidereal objects (Moon or Sun)
    // The Moon and Sun appear to move eastward (increasing RA) relative to the stars
    // because their westward motion due to Earth's rotation is slower than the sidereal rate.
    if (TrackModeSP[TRACK_LUNAR].getState() == ISS_ON)
    {
        // TRACKRATE_LUNAR: how many arcsecs/sec the Moon moves westward (apparent motion)
        // TRACKRATE_SIDEREAL: how many arcsecs/sec the stars move westward (apparent motion)
        // Since the Moon moves slower westward, it effectively moves eastward relative to stars
        double dRA = (TRACKRATE_SIDEREAL - TRACKRATE_LUNAR) * m_TrackingRateTimer.elapsed() / 1000.0;
        m_SkyTrackingTarget.rightascension += (dRA / 3600.0) / 15.0;
        m_TrackingRateTimer.restart();
    }
    else if (TrackModeSP[TRACK_SOLAR].getState() == ISS_ON)
    {
        // Similar logic: Sun moves slower westward than stars, so it moves eastward relative to stars
        double dRA = (TRACKRATE_SIDEREAL - TRACKRATE_SOLAR) * m_TrackingRateTimer.elapsed() / 1000.0;
        m_SkyTrackingTarget.rightascension += (dRA / 3600.0) / 15.0;
        m_TrackingRateTimer.restart();
    }

    // Start by transforming tracking target celestial coordinates to telescope coordinates.
    if (TransformCelestialToTelescope(m_SkyTrackingTarget.rightascension, m_SkyTrackingTarget.declination,
                                      0, TDV))
    {
        // If mount is Alt-Az then that's all we need to do
        AltitudeAzimuthFromTelescopeDirectionVector(TDV, targetMountAxisCoordinates);
        TransformCelestialToTelescope(m_SkyTrackingTarget.rightascension, m_SkyTrackingTarget.declination,
                                      JDoffset, futureTDV);
        AltitudeAzimuthFromTelescopeDirectionVector(futureTDV, futureMountAxisCoordinates);
        TransformCelestialToTelescope(m_SkyTrackingTarget.rightascension, m_SkyTrackingTarget.declination,
                                      -JDoffset, pastTDV);
        AltitudeAzimuthFromTelescopeDirectionVector(pastTDV, pastMountAxisCoordinates);

    }
    // If transformation failed.
    else
    {
        double JDnow {ln_get_julian_from_sys()};
        INDI::IEquatorialCoordinates EquatorialCoordinates { 0, 0 };
        EquatorialCoordinates.rightascension  = m_SkyTrackingTarget.rightascension;
        EquatorialCoordinates.declination = m_SkyTrackingTarget.declination;
        INDI::EquatorialToHorizontal(&EquatorialCoordinates, &m_Location, JDnow, &targetMountAxisCoordinates);
        INDI::EquatorialToHorizontal(&EquatorialCoordinates, &m_Location, JDnow + JDoffset, &futureMountAxisCoordinates);
        INDI::EquatorialToHorizontal(&EquatorialCoordinates, &m_Location, JDnow - JDoffset, &pastMountAxisCoordinates);
    }

    double azGuideOffset = 0, altGuideOffset = 0;
    getGuidePulses(azGuideOffset, altGuideOffset);

    // Now add the guiding offsets, if any.
    targetMountAxisCoordinates.azimuth += azGuideOffset;
    pastMountAxisCoordinates.azimuth += azGuideOffset;
    futureMountAxisCoordinates.azimuth += azGuideOffset;

    targetMountAxisCoordinates.altitude += altGuideOffset;
    pastMountAxisCoordinates.altitude += altGuideOffset;
    futureMountAxisCoordinates.altitude += altGuideOffset;

    // Calculate expected tracking rates
    double predRate[2] = {0, 0};
    // Central difference, error quadratic in timestep
    // Rates in deg/s
    predRate[AXIS_AZ] = range180(AzimuthToDegrees(futureMountAxisCoordinates.azimuth - pastMountAxisCoordinates.azimuth)) /
                        timeStep / 2;
    predRate[AXIS_ALT] = (futureMountAxisCoordinates.altitude - pastMountAxisCoordinates.altitude) / timeStep / 2;

    // Rates arcsec/s
    predRate[AXIS_AZ] = 3600 * predRate[AXIS_AZ];
    predRate[AXIS_ALT] = 3600 * predRate[AXIS_ALT];

    LOGF_DEBUG("Predicted positions (AZ):  %9.4f  %9.4f (now, future, degs)",
               AzimuthToDegrees(targetMountAxisCoordinates.azimuth),
               AzimuthToDegrees(futureMountAxisCoordinates.azimuth)) ;
    LOGF_DEBUG("Predicted positions (AL):  %9.4f  %9.4f (now, future, degs)", targetMountAxisCoordinates.altitude,
               futureMountAxisCoordinates.altitude);
    LOGF_DEBUG("Predicted Rates (AZ, ALT): %9.4f  %9.4f (arcsec/s)", predRate[AXIS_AZ], predRate[AXIS_ALT]);

    // If we had guiding pulses active, mark them as complete
    if (GuideWENP.getState() == IPS_BUSY)
        GuideComplete(AXIS_RA);
    if (GuideNSNP.getState() == IPS_BUSY)
        GuideComplete(AXIS_DE);

    // Next get current alt-az
    INDI::IHorizontalCoordinates currentAltAz { 0, 0 };

    // Current Azimuth
    auto Axis1Steps = CurrentEncoders[AXIS1] - AxisOffsetNP[AZSteps].getValue() - ZeroPositionEncoders[AXIS1];
    currentAltAz.azimuth = DegreesToAzimuth(MicrostepsToDegrees(AXIS1, Axis1Steps));
    // Current Altitude
    auto Axis2Steps = CurrentEncoders[AXIS2] - AxisOffsetNP[ALSteps].getValue() - ZeroPositionEncoders[AXIS2];
    currentAltAz.altitude = MicrostepsToDegrees(AXIS2, Axis2Steps);

    // Offset between target and current horizontal coordinates in arcsecs
    double offsetAngle[2] = {0, 0};
    offsetAngle[AXIS_AZ] = range180(targetMountAxisCoordinates.azimuth - currentAltAz.azimuth) * 3600;
    offsetAngle[AXIS_ALT] = (targetMountAxisCoordinates.altitude - currentAltAz.altitude) * 3600;

    int32_t targetSteps[2] = {0, 0};
    int32_t offsetSteps[2] = {0, 0};
    double trackRates[2] = {0, 0};

    // Convert offsets from arcsecs to steps
    offsetSteps[AXIS_AZ] = offsetAngle[AXIS_AZ] * AxisOneEncoderValuesN[MICROSTEPS_PER_ARCSEC].value;
    offsetSteps[AXIS_ALT] = offsetAngle[AXIS_ALT] * AxisTwoEncoderValuesN[MICROSTEPS_PER_ARCSEC].value;

    /// AZ tracking
    {
        m_OffsetSwitchSettle[AXIS_AZ] = 0;
        m_LastOffset[AXIS_AZ] = offsetSteps[AXIS_AZ];
        targetSteps[AXIS_AZ] = DegreesToMicrosteps(AXIS1, AzimuthToDegrees(targetMountAxisCoordinates.azimuth));
        // Track rate: predicted + PID controlled correction based on tracking error: offsetSteps
        trackRates[AXIS_AZ] = predRate[AXIS_AZ] + m_Controllers[AXIS_AZ]->calculate(0, -offsetAngle[AXIS_AZ]);
        //
        // make sure we never change direction of the trackRate - reduce to predRate * MIN_TRACK_RATE_FACTOR in same direction
        // since tracking direction change can lead to poor tracking
        //
        double minTrackRate = predRate[AXIS_AZ] * MIN_TRACK_RATE_FACTOR;
        if(trackRates[AXIS_AZ] * predRate[AXIS_AZ] < 0 || std::abs(trackRates[AXIS_AZ]) < std::abs(minTrackRate))
            trackRates[AXIS_AZ] = minTrackRate;

        LOGF_DEBUG("Tracking AZ Now: %8.f Target: %8d Offset: %8d Rate: %8.2f", Axis1Steps, targetSteps[AXIS_AZ],
                   offsetSteps[AXIS_AZ], trackRates[AXIS_AZ]);
#ifdef DEBUG_PID
        LOGF_DEBUG("Tracking AZ P: %8.1f I: %8.1f D: %8.1f O: %8.1f",
                   m_Controllers[AXIS_AZ]->propotionalTerm(),
                   m_Controllers[AXIS_AZ]->integralTerm(),
                   m_Controllers[AXIS_AZ]->derivativeTerm(),
                   trackRates[AXIS_AZ] - predRate[AXIS_AZ]);
#endif

        // Set the tracking rate
        trackByRate(AXIS1, trackRates[AXIS_AZ]);
    }

    /// Alt tracking
    {
        m_OffsetSwitchSettle[AXIS_ALT] = 0;
        m_LastOffset[AXIS_ALT] = offsetAngle[AXIS_ALT];
        targetSteps[AXIS_ALT]  = DegreesToMicrosteps(AXIS2, targetMountAxisCoordinates.altitude);
        // Track rate: predicted + PID controlled correction based on tracking error: offsetSteps
        trackRates[AXIS_ALT] = predRate[AXIS_ALT] + m_Controllers[AXIS_ALT]->calculate(0, -offsetAngle[AXIS_ALT]);

        //
        // make sure we never change direction of the trackRate - reduce to predRate * MIN_TRACK_RATE_FACTOR in same direction
        // since tracking direction change can lead to poor tracking
        //
        double minTrackRate = predRate[AXIS_ALT] * MIN_TRACK_RATE_FACTOR;
        if(trackRates[AXIS_ALT] * predRate[AXIS_ALT] < 0 || std::abs(trackRates[AXIS_ALT]) < std::abs(minTrackRate))
            trackRates[AXIS_ALT] = minTrackRate;

        LOGF_DEBUG("Tracking AL Now: %8.f Target: %8d Offset: %8d Rate: %8.2f", Axis2Steps,
                   targetSteps[AXIS_ALT],
                   offsetSteps[AXIS_ALT], trackRates[AXIS_ALT]);
#ifdef DEBUG_PID
        LOGF_DEBUG("Tracking AL P: %8.1f I: %8.1f D: %8.1f O: %8.1f",
                   m_Controllers[AXIS_ALT]->propotionalTerm(),
                   m_Controllers[AXIS_ALT]->integralTerm(),
                   m_Controllers[AXIS_ALT]->derivativeTerm(),
                   trackRates[AXIS_ALT] - predRate[AXIS_ALT]);
#endif
        trackByRate(AXIS2, trackRates[AXIS_ALT]);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
double SkywatcherAPIMount::AzimuthToDegrees(double degree)
{
    if (isNorthHemisphere())
        return range360(degree);
    else
        return range360(degree + 180);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
double SkywatcherAPIMount::DegreesToAzimuth(double degree)
{
    if (isNorthHemisphere())
        return range360(degree);
    else
        return range360(degree + 180);
}

void SkywatcherAPIMount::getGuidePulses(double &az, double &alt)
{
    double DeltaAz = 0, DeltaAlt = 0;

    for (auto Iter = GuidingPulses.begin(); Iter != GuidingPulses.end(); )
    {
        // We treat the guide calibration specially
        if (Iter->OriginalDuration == 1000)
        {
            DeltaAlt += Iter->DeltaAlt;
            DeltaAz += Iter->DeltaAz;
        }
        else
        {
            DeltaAlt += Iter->DeltaAlt / 2;
            DeltaAz += Iter->DeltaAz / 2;
        }
        Iter->Duration -= getCurrentPollingPeriod();

        if (Iter->Duration < static_cast<int>(getCurrentPollingPeriod()))
        {
            Iter = GuidingPulses.erase(Iter);
            if (Iter == GuidingPulses.end())
            {
                break;
            }
            continue;
        }
        ++Iter;
    }

    az = DeltaAlt;
    alt = DeltaAz;
}
