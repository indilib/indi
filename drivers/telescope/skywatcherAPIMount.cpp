/*!
 * \file skywatcherAPIMount.cpp
 *
 * \author Roger James
 * \author Gerry Rozema
 * \author Jean-Luc Geehalel
 * \date 13th November 2013
 *
 * Updated on 2020-12-01 by Jasem Mutlaq
 *
 * This file contains the implementation in C++ of a INDI telescope driver using the Skywatcher API.
 * It is based on work from three sources.
 * A C++ implementation of the API by Roger James.
 * The indi_eqmod driver by Jean-Luc Geehalel.
 * The synscanmount driver by Gerry Rozema.
 */

#include "skywatcherAPIMount.h"

#include "indicom.h"
#include "connectionplugins/connectiontcp.h"
#include "alignment/DriverCommon.h"
#include "connectionplugins/connectionserial.h"

#include <chrono>
#include <thread>

#include <sys/stat.h>

using namespace INDI::AlignmentSubsystem;

// We declare an auto pointer to SkywatcherAPIMount.
static std::unique_ptr<SkywatcherAPIMount> SkywatcherAPIMountPtr(new SkywatcherAPIMount());

/* Preset Slew Speeds */
#define SLEWMODES 9
static double SlewSpeeds[SLEWMODES] = { 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 600.0 };

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
SkywatcherAPIMount::SkywatcherAPIMount()
{
    // Set up the logging pointer in SkyWatcherAPI
    pChildTelescope  = this;
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION,
                           SLEWMODES);

    setVersion(1, 4);
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

        // Let our driver do sync operation in park position
        if (strcmp(name, "EQUATORIAL_EOD_COORD") == 0)
        {
            double ra  = -1;
            double dec = -100;

            for (int x = 0; x < n; x++)
            {
                INumber *eqp = IUFindNumber(&EqNP, names[x]);
                if (eqp == &EqN[AXIS_RA])
                {
                    ra = values[x];
                }
                else if (eqp == &EqN[AXIS_DE])
                {
                    dec = values[x];
                }
            }
            if ((ra >= 0) && (ra <= 24) && (dec >= -90) && (dec <= 90))
            {
                ISwitch *sw = IUFindSwitch(&CoordSP, "SYNC");

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

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

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

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::Goto(double ra, double dec)
{
    DEBUG(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "SkywatcherAPIMount::Goto");

    if (TrackState != SCOPE_IDLE)
        Abort();

    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "RA %lf DEC %lf", ra, dec);

    if (IUFindSwitch(&CoordSP, "TRACK")->s == ISS_ON || IUFindSwitch(&CoordSP, "SLEW")->s == ISS_ON)
    {
        char RAStr[32], DecStr[32];
        fs_sexa(RAStr, ra, 2, 3600);
        fs_sexa(DecStr, dec, 2, 3600);
        CurrentTrackingTarget.rightascension  = ra;
        CurrentTrackingTarget.declination = dec;
        LOGF_INFO("New Tracking target RA %s DEC %s", RAStr, DecStr);
    }

    INDI::IHorizontalCoordinates AltAz { 0, 0 };
    TelescopeDirectionVector TDV;

    if (TransformCelestialToTelescope(ra, dec, 0.0, TDV))
    {
        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "TDV x %lf y %lf z %lf", TDV.x, TDV.y, TDV.z);
        AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
        DEBUG(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Conversion OK");
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
           "New Altitude %lf degrees %ld microsteps Azimuth %lf degrees %ld microsteps", AltAz.altitude,
           DegreesToMicrosteps(AXIS2, AltAz.altitude), AltAz.azimuth, DegreesToMicrosteps(AXIS1, AltAz.azimuth));

    // Update the current encoder positions
    GetEncoder(AXIS1);
    GetEncoder(AXIS2);

    long AltitudeOffsetMicrosteps =
        DegreesToMicrosteps(AXIS2, AltAz.altitude) + ZeroPositionEncoders[AXIS2] - CurrentEncoders[AXIS2];
    long AzimuthOffsetMicrosteps =
        DegreesToMicrosteps(AXIS1, AltAz.azimuth) + ZeroPositionEncoders[AXIS1] - CurrentEncoders[AXIS1];

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

    TrackState = SCOPE_SLEWING;

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::initProperties()
{
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

    // Add alignment properties
    InitAlignmentProperties(this);

    // Force the alignment system to always be on
    getSwitch("ALIGNMENT_SUBSYSTEM_ACTIVE")->sp[0].s = ISS_ON;

    // Set up property variables
    IUFillText(&BasicMountInfoT[MOTOR_CONTROL_FIRMWARE_VERSION], "MOTOR_CONTROL_FIRMWARE_VERSION",
               "Motor control firmware version", "-");
    IUFillText(&BasicMountInfoT[MOUNT_CODE], "MOUNT_CODE", "Mount code", "-");
    IUFillText(&BasicMountInfoT[MOUNT_NAME], "MOUNT_NAME", "Mount name", "-");
    IUFillText(&BasicMountInfoT[IS_DC_MOTOR], "IS_DC_MOTOR", "Is DC motor", "-");
    IUFillTextVector(&BasicMountInfoTP, BasicMountInfoT, 4, getDeviceName(), "BASIC_MOUNT_INFO",
                     "Basic mount information", DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&AxisOneInfoN[MICROSTEPS_PER_REVOLUTION], "MICROSTEPS_PER_REVOLUTION", "Microsteps per revolution",
                 "%.0f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisOneInfoN[STEPPER_CLOCK_FREQUENCY], "STEPPER_CLOCK_FREQUENCY", "Stepper clock frequency", "%.0f", 0,
                 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisOneInfoN[HIGH_SPEED_RATIO], "HIGH_SPEED_RATIO", "High speed ratio", "%.0f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisOneInfoN[MICROSTEPS_PER_WORM_REVOLUTION], "MICROSTEPS_PER_WORM_REVOLUTION",
                 "Microsteps per worm revolution", "%.0f", 0, 0xFFFFFF, 1, 0);

    IUFillNumberVector(&AxisOneInfoNP, AxisOneInfoN, 4, getDeviceName(), "AXIS_ONE_INFO", "Axis one information",
                       DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&AxisOneStateS[FULL_STOP], "FULL_STOP", "FULL_STOP", ISS_OFF);
    IUFillSwitch(&AxisOneStateS[SLEWING], "SLEWING", "SLEWING", ISS_OFF);
    IUFillSwitch(&AxisOneStateS[SLEWING_TO], "SLEWING_TO", "SLEWING_TO", ISS_OFF);
    IUFillSwitch(&AxisOneStateS[SLEWING_FORWARD], "SLEWING_FORWARD", "SLEWING_FORWARD", ISS_OFF);
    IUFillSwitch(&AxisOneStateS[HIGH_SPEED], "HIGH_SPEED", "HIGH_SPEED", ISS_OFF);
    IUFillSwitch(&AxisOneStateS[NOT_INITIALISED], "NOT_INITIALISED", "NOT_INITIALISED", ISS_ON);
    IUFillSwitchVector(&AxisOneStateSP, AxisOneStateS, 6, getDeviceName(), "AXIS_ONE_STATE", "Axis one state",
                       DetailedMountInfoPage, IP_RO, ISR_NOFMANY, 60, IPS_IDLE);

    IUFillNumber(&AxisTwoInfoN[MICROSTEPS_PER_REVOLUTION], "MICROSTEPS_PER_REVOLUTION", "Microsteps per revolution",
                 "%.0f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisTwoInfoN[STEPPER_CLOCK_FREQUENCY], "STEPPER_CLOCK_FREQUENCY", "Step timer frequency", "%.0f", 0,
                 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisTwoInfoN[HIGH_SPEED_RATIO], "HIGH_SPEED_RATIO", "High speed ratio", "%.0f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisTwoInfoN[MICROSTEPS_PER_WORM_REVOLUTION], "MICROSTEPS_PER_WORM_REVOLUTION",
                 "Mictosteps per worm revolution", "%.0f", 0, 0xFFFFFF, 1, 0);

    IUFillNumberVector(&AxisTwoInfoNP, AxisTwoInfoN, 4, getDeviceName(), "AXIS_TWO_INFO", "Axis two information",
                       DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&AxisTwoStateS[FULL_STOP], "FULL_STOP", "FULL_STOP", ISS_OFF);
    IUFillSwitch(&AxisTwoStateS[SLEWING], "SLEWING", "SLEWING", ISS_OFF);
    IUFillSwitch(&AxisTwoStateS[SLEWING_TO], "SLEWING_TO", "SLEWING_TO", ISS_OFF);
    IUFillSwitch(&AxisTwoStateS[SLEWING_FORWARD], "SLEWING_FORWARD", "SLEWING_FORWARD", ISS_OFF);
    IUFillSwitch(&AxisTwoStateS[HIGH_SPEED], "HIGH_SPEED", "HIGH_SPEED", ISS_OFF);
    IUFillSwitch(&AxisTwoStateS[NOT_INITIALISED], "NOT_INITIALISED", "NOT_INITIALISED", ISS_ON);
    IUFillSwitchVector(&AxisTwoStateSP, AxisTwoStateS, 6, getDeviceName(), "AXIS_TWO_STATE", "Axis two state",
                       DetailedMountInfoPage, IP_RO, ISR_NOFMANY, 60, IPS_IDLE);

    IUFillNumber(&AxisOneEncoderValuesN[RAW_MICROSTEPS], "RAW_MICROSTEPS", "Raw Microsteps", "%.0f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisOneEncoderValuesN[MICROSTEPS_PER_ARCSEC], "MICROSTEPS_PER_ARCSEC", "Microsteps/arcsecond",
                 "%.4f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisOneEncoderValuesN[OFFSET_FROM_INITIAL], "OFFSET_FROM_INITIAL", "Offset from initial", "%.0f", 0,
                 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisOneEncoderValuesN[DEGREES_FROM_INITIAL], "DEGREES_FROM_INITIAL", "Degrees from initial", "%.2f",
                 -1000.0, 1000.0, 1, 0);

    IUFillNumberVector(&AxisOneEncoderValuesNP, AxisOneEncoderValuesN, 4, getDeviceName(), "AXIS1_ENCODER_VALUES",
                       "Axis 1 Encoder values", DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&AxisTwoEncoderValuesN[RAW_MICROSTEPS], "RAW_MICROSTEPS", "Raw Microsteps", "%.0f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisTwoEncoderValuesN[MICROSTEPS_PER_ARCSEC], "MICROSTEPS_PER_ARCSEC", "Microsteps/arcsecond",
                 "%.4f", 0, 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisTwoEncoderValuesN[OFFSET_FROM_INITIAL], "OFFSET_FROM_INITIAL", "Offset from initial", "%.0f", 0,
                 0xFFFFFF, 1, 0);
    IUFillNumber(&AxisTwoEncoderValuesN[DEGREES_FROM_INITIAL], "DEGREES_FROM_INITIAL", "Degrees from initial", "%.2f",
                 -1000.0, 1000.0, 1, 0);

    IUFillNumberVector(&AxisTwoEncoderValuesNP, AxisTwoEncoderValuesN, 4, getDeviceName(), "AXIS2_ENCODER_VALUES",
                       "Axis 2 Encoder values", DetailedMountInfoPage, IP_RO, 60, IPS_IDLE);
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

    tcpConnection->setDefaultHost("192.168.4.1");
    tcpConnection->setDefaultPort(11880);
    tcpConnection->setConnectionType(Connection::TCP::TYPE_UDP);

    SetParkDataType(PARK_AZ_ALT_ENCODER);

    // Guiding support
    initGuiderProperties(getDeviceName(), GUIDE_TAB);
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    //Set default values in parent class
    IUFindNumber(&ScopeParametersNP, "TELESCOPE_APERTURE")->value = 200;
    IUFindNumber(&ScopeParametersNP, "TELESCOPE_FOCAL_LENGTH")->value = 2000;
    IUFindNumber(&ScopeParametersNP, "GUIDER_APERTURE")->value = 30;
    IUFindNumber(&ScopeParametersNP, "GUIDER_FOCAL_LENGTH")->value = 120;

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
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void SkywatcherAPIMount::UpdateScopeConfigSwitch()
{
    if (!CheckFile(ScopeConfigFileName, false))
    {
        LOGF_INFO("Can't open XML file (%s) for read", ScopeConfigFileName.c_str());
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
        LOGF_INFO("Failed to parse XML file (%s): %s", ScopeConfigFileName.c_str(), ErrMsg);
        return;
    }
    if (std::string(tagXMLEle(RootXmlNode)) != ScopeConfigRootXmlNode)
    {
        LOGF_INFO("Not a scope config XML file (%s)", ScopeConfigFileName.c_str());
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
        LOGF_INFO("No a scope config found for %s in the XML file (%s)", getDeviceName(),
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

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
double SkywatcherAPIMount::GetSlewRate()
{
    ISwitch *Switch = IUFindOnSwitch(&SlewRateSP);
    double Rate     = *((double *)Switch->aux);

    return Rate;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::MoveNS");

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
    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::MoveWE");

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
    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::Park");
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
bool SkywatcherAPIMount::ReadScopeStatus()
{
    DEBUG(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "SkywatcherAPIMount::ReadScopeStatus");

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

    if (TrackState == SCOPE_SLEWING)
    {
        if ((AxesStatus[AXIS1].FullStop) && (AxesStatus[AXIS2].FullStop))
        {
            if (ISS_ON == IUFindSwitch(&CoordSP, "TRACK")->s)
            {
                // Goto has finished start tracking
                TrackState = SCOPE_TRACKING;

                LOG_INFO("Tracking started.");
                ResetTrackingSeconds = true;
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

    // Calculate new RA DEC
    INDI::IHorizontalCoordinates AltAz { 0, 0 };
    AltAz.altitude = MicrostepsToDegrees(AXIS2, CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis2 encoder %ld initial %ld alt(degrees) %lf",
           CurrentEncoders[AXIS2], ZeroPositionEncoders[AXIS2], AltAz.altitude);
    AltAz.azimuth = MicrostepsToDegrees(AXIS1, CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]);
    CurrentAltAz = AltAz;
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis1 encoder %ld initial %ld az(degrees) %lf",
           CurrentEncoders[AXIS1], ZeroPositionEncoders[AXIS1], AltAz.azimuth);

    INDI::IEquatorialCoordinates rade;
    getCurrentRADE(AltAz, rade);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "New RA %lf (hours) DEC %lf (degrees)", rade.rightascension,
           rade.declination);
    NewRaDec(rade.rightascension, rade.declination);
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
        altaz.altitude = MicrostepsToDegrees(AXIS2, CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]);
        altaz.azimuth = MicrostepsToDegrees(AXIS1, CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]);
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
        double OrigAlt = 0;

        if (TransformCelestialToTelescope(ra, dec, 0.0, TDV))
        {
            AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
            OrigAlt = AltAz.altitude;
            ZeroPositionEncoders[AXIS1] = PolarisPositionEncoders[AXIS1] - DegreesToMicrosteps(AXIS1, AltAz.azimuth);
            ZeroPositionEncoders[AXIS2] = PolarisPositionEncoders[AXIS2] - DegreesToMicrosteps(AXIS2, AltAz.altitude);
            LOGF_INFO("Sync (Alt: %lf Az: %lf) in park position", OrigAlt, AltAz.azimuth);
            GetAlignmentDatabase().clear();
            return true;
        }
    }

    // The tracking seconds should be reset to restart the drift compensation
    ResetTrackingSeconds = true;
    // Might as well do this
    UpdateDetailedMountInformation(true);

    INDI::IHorizontalCoordinates AltAz { 0, 0 };

    AltAz.altitude = MicrostepsToDegrees(AXIS2, CurrentEncoders[AXIS2] - ZeroPositionEncoders[AXIS2]);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis2 encoder %ld initial %ld alt(degrees) %lf",
           CurrentEncoders[AXIS2], ZeroPositionEncoders[AXIS2], AltAz.altitude);
    AltAz.azimuth = MicrostepsToDegrees(AXIS1, CurrentEncoders[AXIS1] - ZeroPositionEncoders[AXIS1]);
    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Axis1 encoder %ld initial %ld az(degrees) %lf",
           CurrentEncoders[AXIS1], ZeroPositionEncoders[AXIS1], AltAz.azimuth);

    AlignmentDatabaseEntry NewEntry;
    NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
    NewEntry.RightAscension     = ra;
    NewEntry.Declination        = dec;
    NewEntry.TelescopeDirection = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);
    NewEntry.PrivateDataSize    = 0;

    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "New sync point Date %lf RA %lf DEC %lf TDV(x %lf y %lf z %lf)",
           NewEntry.ObservationJulianDate, NewEntry.RightAscension, NewEntry.Declination, NewEntry.TelescopeDirection.x,
           NewEntry.TelescopeDirection.y, NewEntry.TelescopeDirection.z);

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

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool SkywatcherAPIMount::Abort()
{
    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::Abort");
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

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void SkywatcherAPIMount::TimerHit()
{
    // Call the base class handler
    // This normally just calls ReadScopeStatus
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
                ResetTrackingSeconds = true;
            }
            // If we're manually moving by WESN controls, update the tracking coordinates.
            if (m_ManualMotionActive)
            {
                break;
            }
            else
            {
                // Restart the drift compensation after syncing or after stopping manual motion
                if (ResetTrackingSeconds)
                {
                    m_TrackingElapsedTimer.restart();
                    ResetTrackingSeconds = false;
                    GuideDeltaAlt = 0;
                    GuideDeltaAz = 0;
                    ResetGuidePulses();
                    TrackedAltAz  = CurrentAltAz;
                    CurrentTrackingTarget.rightascension = EqN[AXIS_RA].value;
                    CurrentTrackingTarget.declination = EqN[AXIS_DE].value;
                }

                double trackingDeltaAlt = std::abs(CurrentAltAz.altitude - TrackedAltAz.altitude);
                double trackingDeltaAz = std::abs(CurrentAltAz.azimuth - TrackedAltAz.azimuth);

                if (trackingDeltaAlt + trackingDeltaAz > 50.0)
                {
                    LOGF_WARN("Abort tracking after too much margin (%1.4f > 10)", trackingDeltaAlt + trackingDeltaAz);
                    Abort();
                }

                uint32_t TrackingMsecs = m_TrackingElapsedTimer.elapsed();
                if (TrackingMsecs % 60000 == 0)
                {
                    LOGF_DEBUG("Tracking in progress (%d seconds elapsed)", TrackingMsecs / 1000);
                }

                // Continue or start tracking
                // Calculate where the mount needs to be in POLLMS time
                // TODO may need to make this longer to get a meaningful result
                double JulianOffset = 1.0 / (24.0 * 60 * 60);
                TelescopeDirectionVector TDV;
                INDI::IHorizontalCoordinates AltAz { 0, 0 };

                if (TransformCelestialToTelescope(CurrentTrackingTarget.rightascension, CurrentTrackingTarget.declination,
                                                  JulianOffset, TDV))
                {
                    DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "TDV x %lf y %lf z %lf", TDV.x, TDV.y, TDV.z);
                    AltitudeAzimuthFromTelescopeDirectionVector(TDV, AltAz);
                }
                else
                {
                    INDI::IEquatorialCoordinates EquatorialCoordinates { 0, 0 };
                    EquatorialCoordinates.rightascension  = CurrentTrackingTarget.rightascension;
                    EquatorialCoordinates.declination = CurrentTrackingTarget.declination;
                    INDI::EquatorialToHorizontal(&EquatorialCoordinates, &m_Location, ln_get_julian_from_sys(), &AltAz);

                }
                DEBUGF(DBG_SCOPE,
                       "Tracking AXIS1 CurrentEncoder %ld OldTrackingTarget %ld AXIS2 CurrentEncoder %ld OldTrackingTarget "
                       "%ld",
                       CurrentEncoders[AXIS1], OldTrackingTarget[AXIS1], CurrentEncoders[AXIS2], OldTrackingTarget[AXIS2]);
                DEBUGF(DBG_SCOPE,
                       "New Tracking Target Altitude %lf degrees %ld microsteps Azimuth %lf degrees %ld microsteps",
                       AltAz.altitude, DegreesToMicrosteps(AXIS2, AltAz.altitude), AltAz.azimuth, DegreesToMicrosteps(AXIS1, AltAz.azimuth));

                // Calculate the auto-guiding delta degrees
                double DeltaAlt = 0;
                double DeltaAz = 0;

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

                GuideDeltaAlt += DeltaAlt;
                GuideDeltaAz += DeltaAz;

                long AltitudeOffsetMicrosteps = DegreesToMicrosteps(AXIS2,
                                                AltAz.altitude + GuideDeltaAlt) + ZeroPositionEncoders[AXIS2] - CurrentEncoders[AXIS2];
                long AzimuthOffsetMicrosteps  = DegreesToMicrosteps(AXIS1,
                                                AltAz.azimuth + GuideDeltaAz) + ZeroPositionEncoders[AXIS1] - CurrentEncoders[AXIS1];

                DEBUGF(DBG_SCOPE, "New Tracking Target AltitudeOffset %ld microsteps AzimuthOffset %ld microsteps",
                       AltitudeOffsetMicrosteps, AzimuthOffsetMicrosteps);

                if (AzimuthOffsetMicrosteps > MicrostepsPerRevolution[AXIS1] / 2)
                {
                    DEBUG(DBG_SCOPE, "Tracking AXIS1 going long way round");
                    // Going the long way round - send it the other way
                    AzimuthOffsetMicrosteps -= MicrostepsPerRevolution[AXIS1];
                }
                if (0 != AzimuthOffsetMicrosteps)
                {
                    // Calculate the slewing rates needed to reach that position
                    // at the correct time.
                    long AzimuthRate = StepperClockFrequency[AXIS1] / AzimuthOffsetMicrosteps;
                    if (!AxesStatus[AXIS1].FullStop && ((AxesStatus[AXIS1].SlewingForward && (AzimuthRate < 0)) ||
                                                        (!AxesStatus[AXIS1].SlewingForward && (AzimuthRate > 0))))
                    {
                        // Direction change whilst axis running
                        // Abandon tracking for this clock tick
                        DEBUG(DBG_SCOPE, "Tracking - AXIS1 direction change");
                        SlowStop(AXIS1);
                    }
                    else
                    {
                        char Direction = AzimuthRate > 0 ? '0' : '1';
                        AzimuthRate    = std::abs(AzimuthRate) * m_AzimuthRateScale;
                        SetClockTicksPerMicrostep(AXIS1, AzimuthRate < 1 ? 1 : AzimuthRate);
                        if (AxesStatus[AXIS1].FullStop)
                        {
                            DEBUG(DBG_SCOPE, "Tracking - AXIS1 restart");
                            SetMotionMode(AXIS1, '1', Direction);
                            StartMotion(AXIS1);
                        }
                        DEBUGF(DBG_SCOPE, "Tracking - AXIS1 offset %ld microsteps rate %ld direction %c",
                               AzimuthOffsetMicrosteps, AzimuthRate, Direction);
                    }
                }
                else
                {
                    // Nothing to do - stop the axis
                    DEBUG(DBG_SCOPE, "Tracking - AXIS1 zero offset");
                    SlowStop(AXIS1);
                }

                // Do I need to take out any complete revolutions before I do this test?
                if (AltitudeOffsetMicrosteps > MicrostepsPerRevolution[AXIS2] / 2)
                {
                    DEBUG(DBG_SCOPE, "Tracking AXIS2 going long way round");
                    // Going the long way round - send it the other way
                    AltitudeOffsetMicrosteps -= MicrostepsPerRevolution[AXIS2];
                }
                if (0 != AltitudeOffsetMicrosteps)
                {
                    // Calculate the slewing rates needed to reach that position
                    // at the correct time.
                    long AltitudeRate = StepperClockFrequency[AXIS2] / AltitudeOffsetMicrosteps;

                    if (!AxesStatus[AXIS2].FullStop && ((AxesStatus[AXIS2].SlewingForward && (AltitudeRate < 0)) ||
                                                        (!AxesStatus[AXIS2].SlewingForward && (AltitudeRate > 0))))
                    {
                        // Direction change whilst axis running
                        // Abandon tracking for this clock tick
                        DEBUG(DBG_SCOPE, "Tracking - AXIS2 direction change");
                        SlowStop(AXIS2);
                    }
                    else
                    {
                        char Direction = AltitudeRate > 0 ? '0' : '1';
                        AltitudeRate   = std::abs(AltitudeRate) * m_AltitudeRateScale;
                        SetClockTicksPerMicrostep(AXIS2, AltitudeRate < 1 ? 1 : AltitudeRate);
                        if (AxesStatus[AXIS2].FullStop)
                        {
                            DEBUG(DBG_SCOPE, "Tracking - AXIS2 restart");
                            SetMotionMode(AXIS2, '1', Direction);
                            StartMotion(AXIS2);
                        }
                        DEBUGF(DBG_SCOPE, "Tracking - AXIS2 offset %ld microsteps rate %ld direction %c",
                               AltitudeOffsetMicrosteps, AltitudeRate, Direction);
                    }
                }
                else
                {
                    // Nothing to do - stop the axis
                    DEBUG(DBG_SCOPE, "Tracking - AXIS2 zero offset");
                    SlowStop(AXIS2);
                }

                DEBUGF(DBG_SCOPE, "Tracking - AXIS1 error %d AXIS2 error %d",
                       OldTrackingTarget[AXIS1] - CurrentEncoders[AXIS1],
                       OldTrackingTarget[AXIS2] - CurrentEncoders[AXIS2]);

                OldTrackingTarget[AXIS1] = AzimuthOffsetMicrosteps + CurrentEncoders[AXIS1];
                OldTrackingTarget[AXIS2] = AltitudeOffsetMicrosteps + CurrentEncoders[AXIS2];
            }

            break;
        }
        break;

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
    DEBUG(DBG_SCOPE, "SkywatcherAPIMount::updateLocation");
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
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);

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
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);

        return true;
    }
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
    Pulse.Duration = (int)ms;
    Pulse.OriginalDuration = (int)ms;
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
    Pulse.Duration = (int)ms;
    Pulse.OriginalDuration = (int)ms;
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
    Pulse.Duration = (int)ms;
    Pulse.OriginalDuration = (int)ms;
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
    Pulse.Duration = (int)ms;
    Pulse.OriginalDuration = (int)ms;
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
    const double WestRate = IUFindNumber(&GuidingRatesNP, "GUIDERA_RATE")->value / 10 * -(double)1 / 60 / 60 * 3.75 / 100;

    ConvertGuideCorrection(WestRate, 0, WestPulse.DeltaAlt, WestPulse.DeltaAz);
    WestPulse.Duration = 1;

    // Calculate the north reference delta
    // Note: By some reason, it must be multiplied by 100 to match with the RA values.
    const double NorthRate = IUFindNumber(&GuidingRatesNP, "GUIDEDEC_RATE")->value / 10 * (double)1 / 60 / 60 * 100 / 100;

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

    TransformCelestialToTelescope(CurrentTrackingTarget.rightascension, CurrentTrackingTarget.declination, 0.0, OldTDV);
    AltitudeAzimuthFromTelescopeDirectionVector(OldTDV, OldAltAz);
    TransformCelestialToTelescope(CurrentTrackingTarget.rightascension + delta_ra,
                                  CurrentTrackingTarget.declination + delta_dec, 0.0, NewTDV);
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

    if (MountCode == AZEQ6)
        IUSaveText(&BasicMountInfoT[MOUNT_NAME], "AZEQ6");
    else if (MountCode >= 128 && MountCode <= 143)
        IUSaveText(&BasicMountInfoT[MOUNT_NAME], "Az Goto");
    else if (MountCode >= 144 && MountCode <= 159)
        IUSaveText(&BasicMountInfoT[MOUNT_NAME], "Dob Goto");
    else if (MountCode >= 160)
        IUSaveText(&BasicMountInfoT[MOUNT_NAME], "AllView Goto");

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
