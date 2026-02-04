/*!
 * \file skywatcherAPI.cpp
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

#include "skywatcherAPI.h"
#include "indicom.h"

#include <cmath>
#include <iomanip>
#include <memory>
#include <thread>
#include <termios.h>

void AXISSTATUS::SetFullStop()
{
    FullStop  = true;
    SlewingTo = Slewing = false;
}

void AXISSTATUS::SetSlewing(bool forward, bool highspeed)
{
    FullStop = SlewingTo = false;
    Slewing              = true;

    SlewingForward = forward;
    HighSpeed      = highspeed;
}

void AXISSTATUS::SetSlewingTo(bool forward, bool highspeed)
{
    FullStop = Slewing = false;
    SlewingTo          = true;

    SlewingForward = forward;
    HighSpeed      = highspeed;
}

const std::map<int, std::string> SkywatcherAPI::errorCodes
{
    {0, "Unknown command"},
    {1, "Command length error"},
    {2, "Motor not stopped"},
    {3, "Invalid character"},
    {4, "Not initialized"},
    {5, "Driver sleeping"}
};

const char *SkywatcherAPI::mountTypeToString(uint8_t type)
{
    switch(type)
    {
        case EQ6:
            return "EQ6";
        case HEQ5:
            return "HEQ5";
        case EQ5:
            return "EQ5";
        case EQ3:
            return "EQ3";
        case EQ8:
            return "EQ8";
        case AZEQ6:
            return "AZ-EQ6";
        case AZEQ5:
            return "AZ-EQ5";
        case STAR_ADVENTURER:
            return "Star Adventurer";
        case EQ8R_PRO:
            return "EQ8R Pro";
        case AZEQ6_PRO:
            return "AZ-EQ6 Pro";
        case EQ6_PRO:
            return "EQ6 Pro";
        case EQ5_PRO:
            return "EQ5 Pro";
        case WAVE_150I:
            return "Wave 150i";
        case GT:
            return "GT";
        case MF:
            return "MF";
        case _114GT:
            return "114 GT";
        case DOB:
            return "Dob";
        case AZGTE:
            return "AZ-GTe";
        case AZGTI:
            return "AZ-GTi";
        default:
            return "Unknown";
    }
}

SkywatcherAPI::SkywatcherAPI()
{
    // I add an additional debug level so I can log verbose scope status
    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    RadiansPerMicrostep[AXIS1] = RadiansPerMicrostep[AXIS2] = 0;
    MicrostepsPerRadian[AXIS1] = MicrostepsPerRadian[AXIS2] = 0;
    DegreesPerMicrostep[AXIS1] = DegreesPerMicrostep[AXIS2] = 0;
    MicrostepsPerDegree[AXIS1] = MicrostepsPerDegree[AXIS2] = 0;
    CurrentEncoders[AXIS1] = CurrentEncoders[AXIS2] = 0;
    PolarisPositionEncoders[AXIS1] = PolarisPositionEncoders[AXIS2] = 0;
    ZeroPositionEncoders[AXIS1] = ZeroPositionEncoders[AXIS2] = 0;
    SlewingSpeed[AXIS1] = SlewingSpeed[AXIS2] = 0;
}

unsigned long SkywatcherAPI::BCDstr2long(std::string &String)
{
    if (String.size() != 6)
    {
        return 0;
    }
    unsigned long value = 0;

#define HEX(c) (((c) < 'A') ? ((c) - '0') : ((c) - 'A') + 10)

    value = HEX(String[4]);
    value <<= 4;
    value |= HEX(String[5]);
    value <<= 4;
    value |= HEX(String[2]);
    value <<= 4;
    value |= HEX(String[3]);
    value <<= 4;
    value |= HEX(String[0]);
    value <<= 4;
    value |= HEX(String[1]);

#undef HEX

    return value;
}

unsigned long SkywatcherAPI::Highstr2long(std::string &String)
{
    if (String.size() < 2)
    {
        return 0;
    }
    unsigned long res = 0;

#define HEX(c) (((c) < 'A') ? ((c) - '0') : ((c) - 'A') + 10)

    res = HEX(String[0]);
    res <<= 4;
    res |= HEX(String[1]);

#undef HEX

    return res;
}

bool SkywatcherAPI::CheckIfDCMotor()
{
    MYDEBUG(DBG_SCOPE, "CheckIfDCMotor");
    // Flush the tty read buffer
    char input[20];
    int rc;
    int nbytes;

    while (true)
    {
        rc = tty_read(MyPortFD, input, 20, 1, &nbytes);
        if (TTY_TIME_OUT == rc)
            break;
        if (TTY_OK != rc)
            return false;
    }

    if (TTY_OK != tty_write(MyPortFD, ":", 1, &nbytes))
        return false;

    rc = tty_read(MyPortFD, input, 1, 1, &nbytes);

    if ((TTY_OK == rc) && (1 == nbytes) && (':' == input[0]))
    {
        IsDCMotor = true;
        return true;
    }
    if (TTY_TIME_OUT == rc)
    {
        IsDCMotor = false;
        return true;
    }

    return false;
}

// bool SkywatcherAPI::IsMerlinMount() const
// {
//     return MountCode >= 0x80 && MountCode < 0x90;
// }

long SkywatcherAPI::DegreesPerSecondToClocksTicksPerMicrostep(AXISID Axis, double DegreesPerSecond)
{
    double MicrostepsPerSecond = DegreesPerSecond * MicrostepsPerDegree[Axis];

    return long((double(StepperClockFrequency[Axis]) / MicrostepsPerSecond));
}

long SkywatcherAPI::DegreesToMicrosteps(AXISID Axis, double AngleInDegrees)
{
    return (long)(AngleInDegrees * MicrostepsPerDegree[(int)Axis]);
}

bool SkywatcherAPI::GetEncoder(AXISID Axis)
{
    //    MYDEBUG(DBG_SCOPE, "GetEncoder");
    std::string Parameters, Response;
    if (!TalkWithAxis(Axis, GetAxisPosition, Parameters, Response))
        return false;

    long Microsteps = BCDstr2long(Response);
    // Only accept valid data
    if (Microsteps > 0)
        CurrentEncoders[Axis] = Microsteps;
    return true;
}

bool SkywatcherAPI::GetHighSpeedRatio(AXISID Axis)
{
    MYDEBUG(DBG_SCOPE, "GetHighSpeedRatio");
    std::string Parameters, Response;

    if (!TalkWithAxis(Axis, InquireHighSpeedRatio, Parameters, Response))
        return false;

    unsigned long highSpeedRatio = Highstr2long(Response);

    if (highSpeedRatio == 0)
    {
        MYDEBUG(INDI::Logger::DBG_ERROR, "Invalid highspeed ratio value from mount. Cycle power and reconnect again.");
        return false;
    }

    HighSpeedRatio[Axis]    = highSpeedRatio;

    return true;
}

bool SkywatcherAPI::GetMicrostepsPerRevolution(AXISID Axis)
{
    MYDEBUG(DBG_SCOPE, "GetMicrostepsPerRevolution");
    std::string Parameters, Response;

    if (!TalkWithAxis(Axis, InquireGridPerRevolution, Parameters, Response))
        return false;

    long tmpMicrostepsPerRevolution = BCDstr2long(Response);

    if (tmpMicrostepsPerRevolution == 0)
    {
        MYDEBUG(INDI::Logger::DBG_ERROR, "Invalid microstep value from mount. Cycle power and reconnect again.");
        return false;
    }

    if (MountCode == _114GT)
        tmpMicrostepsPerRevolution = 0x205318; // for 114GT mount

    // if (IsMerlinMount())
    //     tmpMicrostepsPerRevolution = (long)((double)tmpMicrostepsPerRevolution * 0.655);

    MicrostepsPerRevolution[(int)Axis] = tmpMicrostepsPerRevolution;

    MicrostepsPerRadian[(int)Axis] = tmpMicrostepsPerRevolution / (2 * M_PI);
    RadiansPerMicrostep[(int)Axis] = 2 * M_PI / tmpMicrostepsPerRevolution;
    MicrostepsPerDegree[(int)Axis] = tmpMicrostepsPerRevolution / 360.0;
    DegreesPerMicrostep[(int)Axis] = 360.0 / tmpMicrostepsPerRevolution;

    MYDEBUGF(DBG_SCOPE, "Axis %d: %lf microsteps/degree, %lf microsteps/arcsec", Axis,
             (double)tmpMicrostepsPerRevolution / 360.0, (double)tmpMicrostepsPerRevolution / 360.0 / 60 / 60);

    return true;
}

bool SkywatcherAPI::GetMicrostepsPerWormRevolution(AXISID Axis)
{
    MYDEBUG(DBG_SCOPE, "GetMicrostepsPerWormRevolution");
    std::string Parameters, Response;

    if (!TalkWithAxis(Axis, InquirePECPeriod, Parameters, Response))
        return false;

    uint32_t value = BCDstr2long(Response);
    if (value == 0)
        MYDEBUGF(INDI::Logger::DBG_WARNING, "Zero Microsteps per worm revolution for Axis %d. Possible corrupted data.", Axis);

    MicrostepsPerWormRevolution[Axis] = value;

    return true;
}

bool SkywatcherAPI::GetMotorBoardVersion(AXISID Axis)
{
    MYDEBUG(DBG_SCOPE, "GetMotorBoardVersion");
    std::string Parameters, Response;

    if (!TalkWithAxis(Axis, InquireMotorBoardVersion, Parameters, Response))
        return false;

    unsigned long tmpMCVersion = BCDstr2long(Response);

    MCVersion = ((tmpMCVersion & 0xFF) << 16) | ((tmpMCVersion & 0xFF00)) | ((tmpMCVersion & 0xFF0000) >> 16);

    MYDEBUGF(INDI::Logger::DBG_DEBUG, "Motor Board Version: %#X", MCVersion);

    return true;
}

SkywatcherAPI::PositiveRotationSense_t SkywatcherAPI::GetPositiveRotationDirection(AXISID Axis)
{
    INDI_UNUSED(Axis);
    if (MountCode == _114GT)
        return CLOCKWISE;

    return ANTICLOCKWISE;
}

bool SkywatcherAPI::GetStepperClockFrequency(AXISID Axis)
{
    MYDEBUG(DBG_SCOPE, "GetStepperClockFrequency");
    std::string Parameters, Response;

    if (!TalkWithAxis(Axis, InquireTimerInterruptFreq, Parameters, Response))
        return false;

    uint32_t value = BCDstr2long(Response);

    if (value == 0)
    {
        MYDEBUG(INDI::Logger::DBG_ERROR,
                "Invalid Stepper Clock Frequency value from mount. Cycle power and reconnect again.");
        return false;
    }

    StepperClockFrequency[Axis] = value;

    return true;
}

bool SkywatcherAPI::GetStatus(AXISID Axis)
{
    //    MYDEBUG(DBG_SCOPE, "GetStatus");
    std::string Parameters, Response;

    if (!TalkWithAxis(Axis, GetAxisStatus, Parameters, Response))
        return false;

    if ((Response[1] & 0x01) != 0)
    {
        // Axis is running
        AxesStatus[(int)Axis].FullStop = false;
        if ((Response[0] & 0x01) != 0)
        {
            AxesStatus[(int)Axis].Slewing   = true; // Axis in slewing(AstroMisc speed) mode.
            AxesStatus[(int)Axis].SlewingTo = false;
        }
        else
        {
            AxesStatus[(int)Axis].SlewingTo = true; // Axis in SlewingTo mode.
            AxesStatus[(int)Axis].Slewing   = false;
        }
    }
    else
    {
        // SlewTo Debugging
        if (AxesStatus[(int)Axis].SlewingTo)
        {
            // If the mount was doing a slew to
            GetEncoder(Axis);
            //            MYDEBUGF(INDI::Logger::DBG_SESSION,
            //                     "Axis %s SlewTo complete - offset to target %ld microsteps %lf arc seconds "
            //                     "LastSlewToTarget %ld CurrentEncoder %ld",
            //                     Axis == AXIS1 ? "AXIS1" : "AXIS2", LastSlewToTarget[Axis] - CurrentEncoders[Axis],
            //                     MicrostepsToDegrees(Axis, LastSlewToTarget[Axis] - CurrentEncoders[Axis]) * 3600,
            //                     LastSlewToTarget[Axis], CurrentEncoders[Axis]);
        }

        AxesStatus[(int)Axis].FullStop  = true; // FullStop = 1;	// Axis is fully stop.
        AxesStatus[(int)Axis].Slewing   = false;
        AxesStatus[(int)Axis].SlewingTo = false;
    }

    if ((Response[0] & 0x02) == 0)
        AxesStatus[(int)Axis].SlewingForward = true; // Angle increase = 1;
    else
        AxesStatus[(int)Axis].SlewingForward = false;

    if ((Response[0] & 0x04) != 0)
        AxesStatus[(int)Axis].HighSpeed = true; // HighSpeed running mode = 1;
    else
        AxesStatus[(int)Axis].HighSpeed = false;

    if ((Response[2] & 1) == 0)
        AxesStatus[(int)Axis].NotInitialized = true; // MC is not initialized.
    else
        AxesStatus[(int)Axis].NotInitialized = false;

    return true;
}

// Set initialization done ":F3", where '3'= Both CH1 and CH2.
bool SkywatcherAPI::InitializeMC()
{
    MYDEBUG(DBG_SCOPE, "InitializeMC");
    std::string Parameters, Response;

    if (!TalkWithAxis(AXIS1, Initialize, Parameters, Response))
        return false;
    if (!TalkWithAxis(AXIS2, Initialize, Parameters, Response))
        return false;
    return true;
}

bool SkywatcherAPI::InquireFeatures()
{
    std::string Parameters, Response;
    uint32_t rafeatures = 0, defeatures = 0;

    Long2BCDstr(GET_FEATURES_CMD, Parameters);
    if (!TalkWithAxis(AXIS1, GetFeatureCmd, Parameters, Response))
        return false;
    else
        rafeatures = BCDstr2long(Response);

    Long2BCDstr(GET_FEATURES_CMD, Parameters);
    if (!TalkWithAxis(AXIS2, GetFeatureCmd, Parameters, Response))
        return false;
    else
        defeatures = BCDstr2long(Response);

    if ((rafeatures & 0x000000F0) != (defeatures & 0x000000F0))
    {
        MYDEBUGF(INDI::Logger::DBG_WARNING, "%s(): Found different features for RA (%d) and DEC (%d)", __FUNCTION__,
                 rafeatures, defeatures);
    }
    if (rafeatures & 0x00000010)
    {
        MYDEBUGF(INDI::Logger::DBG_WARNING, "%s(): Found RA PPEC training on", __FUNCTION__);
    }
    if (defeatures & 0x00000010)
    {
        MYDEBUGF(INDI::Logger::DBG_WARNING, "%s(): Found DE PPEC training on", __FUNCTION__);
    }

    // RA
    AxisFeatures[AXIS1].inPPECTraining         = rafeatures & 0x00000010;
    AxisFeatures[AXIS1].inPPEC                 = rafeatures & 0x00000020;
    AxisFeatures[AXIS1].hasEncoder             = rafeatures & 0x00000001;
    AxisFeatures[AXIS1].hasPPEC                = rafeatures & 0x00000002;
    AxisFeatures[AXIS1].hasHomeIndexer         = rafeatures & 0x00000004;
    AxisFeatures[AXIS1].isAZEQ                 = rafeatures & 0x00000008;
    AxisFeatures[AXIS1].hasPolarLed            = rafeatures & 0x00001000;
    AxisFeatures[AXIS1].hasCommonSlewStart     = rafeatures & 0x00002000; // supports :J3
    AxisFeatures[AXIS1].hasHalfCurrentTracking = rafeatures & 0x00004000;
    AxisFeatures[AXIS1].hasWifi                = rafeatures & 0x00008000;

    // DE
    AxisFeatures[AXIS2].inPPECTraining         = defeatures & 0x00000010;
    AxisFeatures[AXIS2].inPPEC                 = defeatures & 0x00000020;
    AxisFeatures[AXIS2].hasEncoder             = defeatures & 0x00000001;
    AxisFeatures[AXIS2].hasPPEC                = defeatures & 0x00000002;
    AxisFeatures[AXIS2].hasHomeIndexer         = defeatures & 0x00000004;
    AxisFeatures[AXIS2].isAZEQ                 = defeatures & 0x00000008;
    AxisFeatures[AXIS2].hasPolarLed            = defeatures & 0x00001000;
    AxisFeatures[AXIS2].hasCommonSlewStart     = defeatures & 0x00002000; // supports :J3
    AxisFeatures[AXIS2].hasHalfCurrentTracking = defeatures & 0x00004000;
    AxisFeatures[AXIS2].hasWifi                = defeatures & 0x00008000;

    return true;
}

bool SkywatcherAPI::InitMount()
{
    //MYDEBUG(DBG_SCOPE, "InitMount");

    if (!CheckIfDCMotor())
        return false;

    if (!GetMotorBoardVersion(AXIS1))
        return false;

    MountCode = MCVersion & 0xFF;

    MYDEBUGF(DBG_SCOPE, "Mount Code: %d (%s)", MountCode, mountTypeToString(MountCode));

    // Disable EQ mounts
    // 0x22 is code for AZEQ6 which is added as an exception as proposed by Dirk Tetzlaff
    if (MountCode < 0x80 && MountCode != AZEQ6 && MountCode != AZEQ5 && MountCode != AZEQ6_PRO && MountCode != WAVE_150I)
    {
        MYDEBUGF(DBG_SCOPE, "Mount type not supported. %d", MountCode);
        return false;
    }

    // Inquire Features
    InquireFeatures();

    // Inquire Gear Rate
    if (!GetMicrostepsPerRevolution(AXIS1))
        return false;
    if (!GetMicrostepsPerRevolution(AXIS2))
        return false;

    // Get stepper clock frequency
    if (!GetStepperClockFrequency(AXIS1))
        return false;
    if (!GetStepperClockFrequency(AXIS2))
        return false;

    // Inquire motor high speed ratio
    if (!GetHighSpeedRatio(AXIS1))
        return false;
    if (!GetHighSpeedRatio(AXIS2))
        return false;

    // Inquire PEC period
    // DC motor controller does not support PEC
    if (!IsDCMotor)
    {
        GetMicrostepsPerWormRevolution(AXIS1);
        GetMicrostepsPerWormRevolution(AXIS2);
    }

    GetStatus(AXIS1);
    GetStatus(AXIS2);

    // In case not init, let's do that
    if (AxesStatus[AXIS1].NotInitialized && AxesStatus[AXIS2].NotInitialized)
    {
        // Inquire Axis Position
        if (!GetEncoder(AXIS1))
            return false;
        if (!GetEncoder(AXIS2))
            return false;
        MYDEBUGF(DBG_SCOPE, "Encoders before init AXIS1 %ld AXIS2 %ld", CurrentEncoders[AXIS1], CurrentEncoders[AXIS2]);

        PolarisPositionEncoders[AXIS1] = CurrentEncoders[AXIS1];
        PolarisPositionEncoders[AXIS2] = CurrentEncoders[AXIS2];
        ZeroPositionEncoders[AXIS1] = PolarisPositionEncoders[AXIS1];
        ZeroPositionEncoders[AXIS2] = PolarisPositionEncoders[AXIS2];

        if (!InitializeMC())
            return false;

    }
    // Mount already initialized
    else
    {
        PolarisPositionEncoders[AXIS1] = 0x800000;
        PolarisPositionEncoders[AXIS2] = 0x800000;
        ZeroPositionEncoders[AXIS1] = PolarisPositionEncoders[AXIS1];
        ZeroPositionEncoders[AXIS2] = PolarisPositionEncoders[AXIS2];
    }

    // These two LowSpeedGotoMargin are calculate from slewing for 5 seconds in 128x sidereal rate
    LowSpeedGotoMargin[(int)AXIS1] = (long)(640 * SIDEREALRATE * MicrostepsPerRadian[(int)AXIS1]);
    LowSpeedGotoMargin[(int)AXIS2] = (long)(640 * SIDEREALRATE * MicrostepsPerRadian[(int)AXIS2]);

    return true;
}

bool SkywatcherAPI::InstantStop(AXISID Axis)
{
    // Request a slow stop
    MYDEBUG(DBG_SCOPE, "InstantStop");
    std::string Parameters, Response;
    if (!TalkWithAxis(Axis, InstantAxisStop, Parameters, Response))
        return false;
    AxesStatus[(int)Axis].SetFullStop();
    return true;
}

void SkywatcherAPI::Long2BCDstr(long Number, std::string &String)
{
    std::stringstream Temp;
    Temp << std::hex << std::setfill('0') << std::uppercase << std::setw(2) << (Number & 0xff) << std::setw(2)
         << ((Number & 0xff00) >> 8) << std::setw(2) << ((Number & 0xff0000) >> 16);
    String = Temp.str();
}

double SkywatcherAPI::MicrostepsToDegrees(AXISID Axis, long Microsteps)
{
    return Microsteps * DegreesPerMicrostep[(int)Axis];
}

double SkywatcherAPI::MicrostepsToRadians(AXISID Axis, long Microsteps)
{
    return Microsteps * RadiansPerMicrostep[(int)Axis];
}

void SkywatcherAPI::PrepareForSlewing(AXISID Axis, double Speed)
{
    // Update the axis status
    if (!GetStatus(Axis))
        return;

    if (!AxesStatus[Axis].FullStop)
    {
        // Axis is running
        if ((AxesStatus[Axis].SlewingTo)                            // slew to (GOTO) in progress
                || (AxesStatus[Axis].HighSpeed)                         // currently high speed slewing
                || (std::abs(Speed) >= LOW_SPEED_MARGIN)                // I am about to request high speed
                || ((AxesStatus[Axis].SlewingForward) && (Speed < 0))   // Direction change
                || (!(AxesStatus[Axis].SlewingForward) && (Speed > 0))) // Direction change
        {
            // I need to stop the axis first
            SlowStop(Axis);
        }
        else
            return; // NO need change motion mode

        // Horrible bit A POLLING LOOP !!!!!!!!!!
        while (true)
        {
            // Update status
            GetStatus(Axis);

            if (AxesStatus[Axis].FullStop)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep for 1/10 second
        }
    }

    char Direction;
    if (Speed > 0.0)
        Direction = '0';
    else
    {
        Direction = '1';
        Speed     = -Speed;
    }

    if (Speed > LOW_SPEED_MARGIN)
        SetAxisMotionMode(Axis, '3', Direction);
    else
        SetAxisMotionMode(Axis, '1', Direction);
}

long SkywatcherAPI::RadiansPerSecondToClocksTicksPerMicrostep(AXISID Axis, double RadiansPerSecond)
{
    double MicrostepsPerSecond = RadiansPerSecond * MicrostepsPerRadian[Axis];

    return long((double(StepperClockFrequency[Axis]) / MicrostepsPerSecond));
}

long SkywatcherAPI::RadiansToMicrosteps(AXISID Axis, double AngleInRadians)
{
    return (long)(AngleInRadians * MicrostepsPerRadian[(int)Axis]);
}

bool SkywatcherAPI::SetEncoder(AXISID Axis, long Microsteps)
{
    MYDEBUG(DBG_SCOPE, "SetEncoder");
    std::string Parameters, Response;

    Long2BCDstr(Microsteps, Parameters);

    return TalkWithAxis(Axis, SetAxisPositionCmd, Parameters, Response);
}

bool SkywatcherAPI::SetGotoTargetOffset(AXISID Axis, long OffsetInMicrosteps)
{
    //    MYDEBUG(DBG_SCOPE, "SetGotoTargetOffset");
    std::string Parameters, Response;

    Long2BCDstr(OffsetInMicrosteps, Parameters);

    return TalkWithAxis(Axis, SetGotoTargetIncrement, Parameters, Response);
}

/// Func - 0 High speed slew to mode (goto)
/// Func - 1 Low speed slew mode
/// Func - 2 Low speed slew to mode (goto)
/// Func - 3 High speed slew mode
bool SkywatcherAPI::SetAxisMotionMode(AXISID Axis, char Func, char Direction)
{
    //    MYDEBUG(DBG_SCOPE, "SetMotionMode");
    std::string Parameters, Response;

    Parameters.push_back(Func);
    Parameters.push_back(Direction);

    return TalkWithAxis(Axis, SetMotionMode, Parameters, Response);
}

bool SkywatcherAPI::SetClockTicksPerMicrostep(AXISID Axis, long ClockTicksPerMicrostep)
{
    //MYDEBUG(DBG_SCOPE, "SetClockTicksPerMicrostep");
    std::string Parameters, Response;

    Long2BCDstr(ClockTicksPerMicrostep, Parameters);

    return TalkWithAxis(Axis, SetStepPeriod, Parameters, Response);
}

bool SkywatcherAPI::SetSlewModeDeccelerationRampLength(AXISID Axis, long Microsteps)
{
    //    MYDEBUG(DBG_SCOPE, "SetSlewModeDeccelerationRampLength");
    std::string Parameters, Response;

    Long2BCDstr(Microsteps, Parameters);

    return TalkWithAxis(Axis, SetBreakStep, Parameters, Response);
}

bool SkywatcherAPI::SetSlewToModeDeccelerationRampLength(AXISID Axis, long Microsteps)
{
    //    MYDEBUG(DBG_SCOPE, "SetSlewToModeDeccelerationRampLength");
    std::string Parameters, Response;

    Long2BCDstr(Microsteps, Parameters);

    return TalkWithAxis(Axis, SetBreakPointIncrement, Parameters, Response);
}

bool SkywatcherAPI::toggleSnapPort(bool enabled)
{
    std::string Response;
    std::string Parameters = enabled ? "1" : "0";
    return TalkWithAxis(AXIS1, SetSnapPort, Parameters, Response);
}

void SkywatcherAPI::Slew(AXISID Axis, double SpeedInRadiansPerSecond, bool IgnoreSilentMode)
{
    MYDEBUGF(DBG_SCOPE, "Slew axis: %d speed: %1.6f", Axis, SpeedInRadiansPerSecond);
    // Clamp to MAX_SPEED
    if (SpeedInRadiansPerSecond > MAX_SPEED)
        SpeedInRadiansPerSecond = MAX_SPEED;
    else if (SpeedInRadiansPerSecond < -MAX_SPEED)
        SpeedInRadiansPerSecond = -MAX_SPEED;

    double InternalSpeed = SpeedInRadiansPerSecond;

    if (std::abs(InternalSpeed) <= SIDEREALRATE / 1000.0)
    {
        SlowStop(Axis);
        return;
    }

    // Stop motor and set motion mode if necessary
    PrepareForSlewing(Axis, InternalSpeed);

    bool Forward;
    if (InternalSpeed > 0.0)
        Forward = true;
    else
    {
        InternalSpeed = -InternalSpeed;
        Forward       = false;
    }

    bool HighSpeed = false;

    if (InternalSpeed > LOW_SPEED_MARGIN && (IgnoreSilentMode || !SilentSlewMode))
    {
        InternalSpeed = InternalSpeed / (double)HighSpeedRatio[Axis];
        HighSpeed     = true;
    }
    long SpeedInt = RadiansPerSecondToClocksTicksPerMicrostep(Axis, InternalSpeed);
    if ((MCVersion == 0x010600) || (MCVersion == 0x0010601)) // Cribbed from Mount_Skywatcher.cs
        SpeedInt -= 3;
    if (SpeedInt < 6)
        SpeedInt = 6;
    SetClockTicksPerMicrostep(Axis, SpeedInt);

    StartAxisMotion(Axis);

    AxesStatus[Axis].SetSlewing(Forward, HighSpeed);
    SlewingSpeed[Axis] = SpeedInRadiansPerSecond;
}

void SkywatcherAPI::SlewTo(AXISID Axis, long OffsetInMicrosteps, bool verbose)
{
    // Nothing to do
    if (0 == OffsetInMicrosteps)
        return;

    // Debugging
    LastSlewToTarget[Axis] = CurrentEncoders[Axis] + OffsetInMicrosteps;
    if (verbose)
    {
        MYDEBUGF(INDI::Logger::DBG_DEBUG, "SlewTo Axis %d Offset %ld CurrentEncoder %ld SlewToTarget %ld", Axis,
                 OffsetInMicrosteps, CurrentEncoders[Axis], LastSlewToTarget[Axis]);
    }

    char Direction = '0';
    bool Forward = true;

    if (OffsetInMicrosteps < 0)
    {
        Forward            = false;
        Direction          = '1';
        OffsetInMicrosteps = -OffsetInMicrosteps;
    }

    bool HighSpeed = (OffsetInMicrosteps > LowSpeedGotoMargin[Axis] && !SilentSlewMode);

    if (!GetStatus(Axis))
        return;

    if (!AxesStatus[Axis].FullStop)
    {
        // Axis is running
        if ((AxesStatus[Axis].SlewingTo)                        // slew to (GOTO) in progress
                || (AxesStatus[Axis].HighSpeed)                     // currently high speed slewing
                || HighSpeed                                        // I am about to request high speed
                || ((AxesStatus[Axis].SlewingForward) && !Forward)  // Direction change
                || (!(AxesStatus[Axis].SlewingForward) && Forward)) // Direction change
        {
            // I need to stop the axis first
            SlowStop(Axis);
            // Horrible bit A POLLING LOOP !!!!!!!!!!
            while (true)
            {
                // Update status
                GetStatus(Axis);

                if (AxesStatus[Axis].FullStop)
                    break;

                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep for 1/10 second
            }
        }
    }

    if (HighSpeed)
        SetAxisMotionMode(Axis, '0', Direction);
    else
        SetAxisMotionMode(Axis, '2', Direction);

    SetGotoTargetOffset(Axis, OffsetInMicrosteps);

    if (HighSpeed)
        SetSlewToModeDeccelerationRampLength(Axis, OffsetInMicrosteps > 3200 ? 3200 : OffsetInMicrosteps);
    else
        SetSlewToModeDeccelerationRampLength(Axis, OffsetInMicrosteps > 200 ? 200 : OffsetInMicrosteps);

    StartAxisMotion(Axis);

    AxesStatus[Axis].SetSlewingTo(Forward, HighSpeed);
}

bool SkywatcherAPI::SlowStop(AXISID Axis)
{
    // Request a slow stop
    std::string Parameters, Response;

    return TalkWithAxis(Axis, NotInstantAxisStop, Parameters, Response);
}

bool SkywatcherAPI::StartAxisMotion(AXISID Axis)
{
    std::string Parameters, Response;

    return TalkWithAxis(Axis, StartMotion, Parameters, Response);
}

bool SkywatcherAPI::TalkWithAxis(AXISID Axis, SkywatcherCommand Command, std::string &cmdDataStr, std::string &responseStr)
{
    int bytesWritten = 0;
    int bytesRead = 0;
    int errorCode = 0;
    char command[SKYWATCHER_MAX_CMD] = {0};
    char response[SKYWATCHER_MAX_CMD] = {0};

    snprintf(command, SKYWATCHER_MAX_CMD, ":%c%c%s", Command, Axis == AXIS1 ? '1' : '2', cmdDataStr.c_str());

    MYDEBUGF(DBG_SCOPE, "CMD <%s>", command + 1);

    // Now add the trailing 0xD
    command[strlen(command)] = 0xD;

    for (int retries = 0; retries < SKYWATCHER_MAX_RETRTY; retries++)
    {
        tcflush(MyPortFD, TCIOFLUSH);
        if ( (errorCode = tty_write_string(MyPortFD, command, &bytesWritten)) != TTY_OK)
        {
            if (retries == SKYWATCHER_MAX_RETRTY - 1)
            {
                char errorMessage[MAXRBUF] = {0};
                tty_error_msg(errorCode, errorMessage, MAXRBUF);
                MYDEBUGF(INDI::Logger::DBG_ERROR, "Communication error: %s", errorMessage);
                return false;
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        bool isResponseReceived = false;
        while (true)
        {
            memset(response, '\0', SKYWATCHER_MAX_CMD);
            // If we get less than 2 bytes then it must be an error (=\r is a valid response).
            if ( (errorCode = tty_read_section_expanded(MyPortFD, response, 0x0D, SKYWATCHER_TIMEOUT_S, SKYWATCHER_TIMEOUT_US, &bytesRead)) != TTY_OK
                    || bytesRead < 2)
            {
                if (retries == SKYWATCHER_MAX_RETRTY - 1)
                {
                    char errorMessage[MAXRBUF] = {0};
                    tty_error_msg(errorCode, errorMessage, MAXRBUF);
                    if (bytesRead < 2)
                        return false;
                    else
                        MYDEBUGF(INDI::Logger::DBG_ERROR, "Communication error: %s", errorMessage);
                    return false;
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    break;
                }
            }
            else
            {
                // Remove CR (0x0D)
                response[bytesRead - 1] = '\0';
                if(response[0] == '=' || response[0] == '!')
                {
                    isResponseReceived = true;
                    break;
                }
                else
                {
                    //Skip invalid response
                }
            }
        }
        if(isResponseReceived) break;
    }

    // If it is not empty, log it.
    if (response[1] != 0)
        MYDEBUGF(DBG_SCOPE, "RES <%s>", response + 1);
    // Skip first = or !
    responseStr = response + 1;

    if (response[0] == '!')
    {
        // char to int
        uint8_t code = response[1] - 0x30;
        if (errorCodes.count(code) > 0)
            MYDEBUGF(INDI::Logger::DBG_ERROR, "Mount error: %s", errorCodes.at(code).c_str());
        return false;
    }

    // Response starting to ! is abnormal response, while = is OK.
    return true;
}

bool SkywatcherAPI::IsInMotion(AXISID Axis)
{
    MYDEBUG(DBG_SCOPE, "IsInMotion");

    return AxesStatus[(int)Axis].Slewing || AxesStatus[(int)Axis].SlewingTo;
}

bool SkywatcherAPI::HasHomeIndexers()
{
    return (AxisFeatures[AXIS1].hasHomeIndexer) && (AxisFeatures[AXIS2].hasHomeIndexer);
}

bool SkywatcherAPI::HasAuxEncoders()
{
    return (AxisFeatures[AXIS1].hasEncoder) && (AxisFeatures[AXIS2].hasEncoder);
}

bool SkywatcherAPI::HasPPEC()
{
    return AxisFeatures[AXIS1].hasPPEC;
}

bool SkywatcherAPI::HasSnapPort1()
{
    return MountCode == 0x04 ||  MountCode == 0x05 ||  MountCode == 0x06 ||  MountCode == 0x0A || MountCode == 0x23
           || MountCode == 0xA5;
}

bool SkywatcherAPI::HasSnapPort2()
{
    return MountCode == 0x06;
}

bool SkywatcherAPI::HasPolarLed()
{
    return (AxisFeatures[AXIS1].hasPolarLed) && (AxisFeatures[AXIS2].hasPolarLed);
}

void SkywatcherAPI::TurnEncoder(AXISID axis, bool on)
{
    uint32_t command;
    if (on)
        command = ENCODER_ON_CMD;
    else
        command = ENCODER_OFF_CMD;
    SetFeature(axis, command);
}

void SkywatcherAPI::TurnRAEncoder(bool on)
{
    TurnEncoder(AXIS1, on);
}

void SkywatcherAPI::TurnDEEncoder(bool on)
{
    TurnEncoder(AXIS2, on);
}

void SkywatcherAPI::SetFeature(AXISID axis, uint32_t command)
{
    std::string cmd, response;
    Long2BCDstr(command, cmd);
    TalkWithAxis(axis, SetFeatureCmd, cmd, response);
}
