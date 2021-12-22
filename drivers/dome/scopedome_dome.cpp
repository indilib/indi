/*******************************************************************************
 ScopeDome Dome INDI Driver

 Copyright(c) 2017-2021 Jarno Paananen. All rights reserved.

 based on:

 ScopeDome Windows ASCOM driver version 5.1.30

 and

 Baader Planetarium Dome INDI Driver

 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 Baader Dome INDI Driver

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "scopedome_dome.h"
#include "scopedome_arduino.h"
#include "scopedome_sim.h"
#include "scopedome_usb21.h"

#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <wordexp.h>

// We declare an auto pointer to ScopeDome.
std::unique_ptr<ScopeDome> scopeDome(new ScopeDome());

ScopeDome::ScopeDome()
{
    setVersion(2, 0);
    targetAz = 0;
    setShutterState(SHUTTER_UNKNOWN);
    simShutterStatus = SHUTTER_CLOSED;
    status = DOME_UNKNOWN;
    targetShutter = SHUTTER_CLOSE;

    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_ABS_MOVE | DOME_CAN_REL_MOVE | DOME_CAN_PARK | DOME_HAS_SHUTTER);

    stepsPerRevolution = ~0;

    // Load dome inertia table if present
    wordexp_t wexp;
    if (wordexp("~/.indi/ScopeDome_DomeInertia_Table.txt", &wexp, 0) == 0)
    {
        FILE *inertia = fopen(wexp.we_wordv[0], "r");
        if (inertia)
        {
            // skip UTF-8 marker bytes
            fseek(inertia, 3, SEEK_SET);
            char line[100];
            int lineNum = 0;

            while (fgets(line, sizeof(line), inertia))
            {
                int step, result;
                // File format is "steps ; actual steps ;" but with varying number of spaces
                char* splitter = strchr(line, ';');
                if(splitter)
                {
                    *splitter++ = 0;

                    step = atoi(line);
                    result = atoi(splitter);
                    if (step == lineNum)
                    {
                        inertiaTable.push_back(result);
                    }
                }
                lineNum++;
            }
            fclose(inertia);
            LOGF_INFO("Read inertia file %s", wexp.we_wordv[0]);
        }
        else
        {
            LOG_WARN("Could not read inertia file, please generate one with Windows "
                     "driver setup and copy to "
                     "~/.indi/ScopeDome_DomeInertia_Table.txt");
        }
    }
    wordfree(&wexp);
}

bool ScopeDome::initProperties()
{
    INDI::Dome::initProperties();

    DomeHomePositionNP[0].fill("DH_POSITION", "AZ (deg)", "%6.2f", 0.0, 360.0, 1.0, 0.0);
    DomeHomePositionNP.fill(getDeviceName(), "DOME_HOME_POSITION",
                            "Home sensor position", SITE_TAB, IP_RW, 60, IPS_OK);

    HomeSensorPolaritySP[ScopeDomeCard::ACTIVE_HIGH].fill("ACTIVE_HIGH", "Active high", ISS_ON);
    HomeSensorPolaritySP[ScopeDomeCard::ACTIVE_LOW].fill("ACTIVE_LOW", "Active low", ISS_OFF);
    HomeSensorPolaritySP.fill(getDeviceName(), "HOME_SENSOR_POLARITY",
                              "Home sensor polarity", SITE_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

    FindHomeSP[0].fill("START", "Start", ISS_OFF);
    FindHomeSP.fill(getDeviceName(), "FIND_HOME", "Find home", MAIN_CONTROL_TAB, IP_RW,
                    ISR_ATMOST1, 0, IPS_OK);

    DerotateSP[0].fill("START", "Start", ISS_OFF);
    DerotateSP.fill(getDeviceName(), "DEROTATE", "Derotate", MAIN_CONTROL_TAB, IP_RW,
                    ISR_ATMOST1, 0, IPS_OK);

    FirmwareVersionsNP[0].fill("MAIN", "Main part", "%2.2f", 0.0, 99.0, 1.0, 0.0);
    FirmwareVersionsNP[1].fill("ROTARY", "Rotary part", "%2.1f", 0.0, 99.0, 1.0, 0.0);
    FirmwareVersionsNP.fill(getDeviceName(), "FIRMWARE_VERSION",
                            "Firmware versions", INFO_TAB, IP_RO, 60, IPS_IDLE);

    StepsPerRevolutionNP[0].fill("STEPS", "Steps per revolution", "%5.0f", 0.0, 99999.0, 1.0, 0.0);
    StepsPerRevolutionNP.fill(getDeviceName(), "CALIBRATION_VALUES",
                              "Calibration values", SITE_TAB, IP_RO, 60, IPS_IDLE);

    CalibrationNeededSP[0].fill("CALIBRATION_NEEDED", "Calibration needed", ISS_OFF);
    CalibrationNeededSP.fill(getDeviceName(), "CALIBRATION_STATUS",
                             "Calibration status", SITE_TAB, IP_RO, ISR_ATMOST1, 0, IPS_IDLE);

    StartCalibrationSP[0].fill("START", "Start", ISS_OFF);
    StartCalibrationSP.fill(getDeviceName(), "RUN_CALIBRATION", "Run calibration",
                            SITE_TAB, IP_RW, ISR_ATMOST1, 0, IPS_OK);

    char defaultUsername[MAXINDINAME] = "scopedome";
    char defaultPassword[MAXINDINAME] = "default";
    IUGetConfigText(getDeviceName(), "CREDENTIALS", "USERNAME", defaultUsername, MAXINDINAME);
    IUGetConfigText(getDeviceName(), "CREDENTIALS", "PASSWORD", defaultPassword, MAXINDINAME);
    CredentialsTP[0].fill("USERNAME", "User name", defaultUsername);
    CredentialsTP[1].fill("PASSWORD", "Password", defaultPassword);
    CredentialsTP.fill(getDeviceName(), "CREDENTIALS", "Credentials", CONNECTION_TAB, IP_RW, 0, IPS_OK);

    ISState usbcard21 = ISS_ON;
    ISState arduino = ISS_OFF;
    IUGetConfigSwitch(getDeviceName(), "CARD_TYPE", "USB_CARD_21", &usbcard21);
    IUGetConfigSwitch(getDeviceName(), "CARD_TYPE", "ARDUINO", &arduino);
    CardTypeSP[SCOPEDOME_USB21].fill("USB_CARD_21", "USB Card 2.1", usbcard21);
    CardTypeSP[SCOPEDOME_ARDUINO].fill("ARDUINO", "Arduino Card", arduino);
    CardTypeSP.fill(getDeviceName(), "CARD_TYPE", "Card type",
                    CONNECTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

    switch(CardTypeSP.findOnSwitchIndex())
    {
        case SCOPEDOME_USB21:
        default:
            serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
            break;
        case SCOPEDOME_ARDUINO:
            serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
            break;
    }

    SetParkDataType(PARK_AZ);
    addAuxControls();

    defineProperty(&CardTypeSP);
    defineProperty(CredentialsTP);

    setPollingPeriodRange(1000, 3000); // Device doesn't like too long interval
    setDefaultPollingPeriod(1000);
    return true;
}

/************************************************************************************
 *
 * ***********************************************************************************/
bool ScopeDome::SetupParms()
{
    targetAz = 0;

    stepsPerRevolution = interface->getStepsPerRevolution();
    LOGF_INFO("Steps per revolution read as %d", stepsPerRevolution);
    StepsPerRevolutionNP[0].setValue(stepsPerRevolution);
    StepsPerRevolutionNP.setState(IPS_OK);
    StepsPerRevolutionNP.apply();

    if (UpdatePosition())
        IDSetNumber(&DomeAbsPosNP, nullptr);

    if (UpdateShutterStatus())
        IDSetSwitch(&DomeShutterSP, nullptr);

    UpdateSensorStatus();
    UpdateRelayStatus();

    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking
        // values.
        SetAxis1ParkDefault(0);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is
        // found.
        SetAxis1Park(0);
        SetAxis1ParkDefault(0);
    }

    bool calibrationNeeded = interface->isCalibrationNeeded();
    CalibrationNeededSP[0].setState(calibrationNeeded ? ISS_ON : ISS_OFF);
    CalibrationNeededSP.setState(IPS_OK);
    CalibrationNeededSP.apply();

    double fwMain, fwRotary;
    interface->getFirmwareVersions(fwMain, fwRotary);
    FirmwareVersionsNP[0].setValue(fwMain);
    FirmwareVersionsNP[1].setValue(fwRotary);
    FirmwareVersionsNP.setState(IPS_OK);
    FirmwareVersionsNP.apply();

    rotaryLinkEstablished = interface->getInputState(ScopeDomeCard::ROTARY_LINK);
    if(rotaryLinkEstablished == ISS_OFF)
    {
        LOG_WARN("Rotary link not established, shutter control not possible");
    }
    return true;
}

/************************************************************************************
 *
 ************************************************************************************/
bool ScopeDome::Handshake()
{
    if(reconnecting)
        return true;

    sim = isSimulation();

    if (sim)
    {
        interface.reset(static_cast<ScopeDomeCard *>(new ScopeDomeSim()));
    }
    else
    {
        switch(CardTypeSP.findOnSwitchIndex())
        {
            case SCOPEDOME_USB21:
            default:
                interface.reset(static_cast<ScopeDomeCard *>(new ScopeDomeUSB21(this, PortFD)));
                break;
            case SCOPEDOME_ARDUINO:
            interface.reset(static_cast<ScopeDomeCard *>(new ScopeDomeArduino(this, getActiveConnection())));
                break;
        }
    }
    if (interface->detect())
    {
        return true;
    }
    LOG_ERROR("Can't identify the card, check card type and baud rate (115200 for USB Card 2.1, 9600 for Arduino Card)");
    return false;
}

/************************************************************************************
 *
 ************************************************************************************/
const char *ScopeDome::getDefaultName()
{
    return (const char *)"ScopeDome Dome";
}

/************************************************************************************
 *
 ************************************************************************************/
bool ScopeDome::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        // Initialize dynamic properties
        {
            size_t numSensors = interface->getNumberOfSensors();
            SensorsNP.resize(numSensors);
            for(size_t i = 0; i < numSensors; i++)
            {
                auto info = interface->getSensorInfo(i);
                SensorsNP[i].fill(info.propName.c_str(), info.label.c_str(),
                                  info.format.c_str(), info.minValue, info.maxValue, 1.0, 0.0);
            }
            SensorsNP.fill(getDeviceName(), "SCOPEDOME_SENSORS",
                           "Sensors", INFO_TAB, IP_RO, 60, IPS_IDLE);
        }

        {
            size_t numRelays = interface->getNumberOfRelays();
            RelaysSP.resize(numRelays);
            for(size_t i = 0; i < numRelays; i++)
            {
                auto info = interface->getRelayInfo(i);
                RelaysSP[i].fill(info.propName.c_str(), info.label.c_str(), ISS_OFF);
            }
            RelaysSP.fill(getDeviceName(), "RELAYS", "Relays", MAIN_CONTROL_TAB, IP_RW,
                          ISR_NOFMANY,
                          0, IPS_IDLE);
        }

        {
            size_t numInputs = interface->getNumberOfInputs();
            InputsSP.resize(numInputs);
            for(size_t i = 0; i < numInputs; i++)
            {
                auto info = interface->getInputInfo(i);
                InputsSP[i].fill(info.propName.c_str(), info.label.c_str(), ISS_OFF);
            }
            InputsSP.fill(getDeviceName(), "INPUTS", "Inputs", INFO_TAB, IP_RO,
                          ISR_NOFMANY, 0, IPS_IDLE);
        }

        defineProperty(FindHomeSP);
        defineProperty(DerotateSP);
        defineProperty(DomeHomePositionNP);
        defineProperty(HomeSensorPolaritySP);
        defineProperty(SensorsNP);
        defineProperty(RelaysSP);
        defineProperty(InputsSP);
        defineProperty(StepsPerRevolutionNP);
        defineProperty(CalibrationNeededSP);
        defineProperty(StartCalibrationSP);
        defineProperty(FirmwareVersionsNP);
        SetupParms();
    }
    else
    {
        deleteProperty(FindHomeSP.getName());
        deleteProperty(DerotateSP.getName());
        deleteProperty(DomeHomePositionNP.getName());
        deleteProperty(HomeSensorPolaritySP.getName());
        deleteProperty(SensorsNP.getName());
        deleteProperty(RelaysSP.getName());
        deleteProperty(InputsSP.getName());
        deleteProperty(StepsPerRevolutionNP.getName());
        deleteProperty(CalibrationNeededSP.getName());
        deleteProperty(StartCalibrationSP.getName());
        deleteProperty(FirmwareVersionsNP.getName());
    }

    return true;
}

/************************************************************************************
 *
 * ***********************************************************************************/
bool ScopeDome::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // ignore if not ours
    if (dev == nullptr || strcmp(dev, getDeviceName()))
        return false;

    if (FindHomeSP.isNameMatch(name))
    {
        if (status != DOME_HOMING)
        {
            LOG_INFO("Finding home sensor");
            status = DOME_HOMING;
            IUResetSwitch(&FindHomeSP);
            DomeAbsPosNP.s = IPS_BUSY;
            FindHomeSP.setState(IPS_BUSY);
            FindHomeSP.apply();
            interface->findHome();
        }
        return true;
    }

    if (DerotateSP.isNameMatch(name))
    {
        if (status != DOME_DEROTATING)
        {
            LOG_INFO("De-rotating started");
            status = DOME_DEROTATING;
            IUResetSwitch(&DerotateSP);
            DomeAbsPosNP.s = IPS_BUSY;
            DerotateSP.setState(IPS_BUSY);
            DerotateSP.apply();
        }
        return true;
    }

    if (StartCalibrationSP.isNameMatch(name))
    {
        if (status != DOME_CALIBRATING)
        {
            LOG_INFO("Calibration started");
            status = DOME_CALIBRATING;
            IUResetSwitch(&StartCalibrationSP);
            DomeAbsPosNP.s       = IPS_BUSY;
            StartCalibrationSP.setState(IPS_BUSY);
            StartCalibrationSP.apply();
            interface->calibrate();
        }
        return true;
    }

    if (RelaysSP.isNameMatch(name))
    {
        RelaysSP.update(states, names, n);
        for(int i = 0; i < n; i++)
        {
            interface->setRelayState(i, RelaysSP[i].getState());
        }
        RelaysSP.apply();
        return true;
    }

    if (CardTypeSP.isNameMatch(name))
    {
        CardTypeSP.update(states, names, n);
        CardTypeSP.apply();

        if(CardTypeSP[0].getState() == ISS_ON)
        {
            // USB Card 2.1 uses 115200 bauds
            serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
            setDomeConnection(CONNECTION_SERIAL);
        }
        else
        {
            // Arduino Card uses 9600 bauds
            serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
            setDomeConnection(CONNECTION_SERIAL | CONNECTION_TCP);
        }
        return true;
    }
    if (HomeSensorPolaritySP.isNameMatch(name))
    {
        HomeSensorPolaritySP.update(states, names, n);
        HomeSensorPolaritySP.apply();

        if (HomeSensorPolaritySP[ScopeDomeCard::ACTIVE_LOW].getState() == ISS_ON)
        {
            interface->setHomeSensorPolarity(ScopeDomeCard::ACTIVE_LOW);
            LOG_DEBUG("Home sensor polarity set to ACTIVE_LOW");
        }
        else
        {
            interface->setHomeSensorPolarity(ScopeDomeCard::ACTIVE_HIGH);
            LOG_DEBUG("Home sensor polarity set to ACTIVE_HIGH");
        }
        return true;
    }

    // Pass to base class
    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

bool ScopeDome::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // ignore if not ours
    if (dev == nullptr || strcmp(dev, getDeviceName()))
        return false;


    if (DomeHomePositionNP.isNameMatch(name))
    {
        DomeHomePositionNP.update(values, names, n);
        DomeHomePositionNP.setState(IPS_OK);
        DomeHomePositionNP.apply();
        LOGF_DEBUG("Dome home position set to %f", DomeHomePositionNP[0].getValue());
        return true;
    }
    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

bool ScopeDome::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    // ignore if not ours
    if (dev == nullptr || strcmp(dev, getDeviceName()))
        return false;

    if (CredentialsTP.isNameMatch(name))
    {
        CredentialsTP.update(texts, names, n);
        CredentialsTP.apply();
        return true;
    }
    return INDI::Dome::ISNewText(dev, name, texts, names, n);
}

/************************************************************************************
 *
 * ***********************************************************************************/
bool ScopeDome::UpdateShutterStatus()
{
    int rc = 0;
    if (rc != 0)
    {
        LOGF_ERROR("Error reading input state: %d", rc);
        return false;
    }

    for(size_t i = 0; i < InputsSP.size(); i++)
    {
        InputsSP[i].setState(interface->getInputValue(i) ? ISS_ON : ISS_OFF);
    }
    InputsSP.setState(IPS_OK);
    InputsSP.apply();

    if (interface->getInputState(ScopeDomeCard::OPEN1) == ISS_ON) // shutter open switch triggered
    {
        if (m_ShutterState == SHUTTER_MOVING && targetShutter == SHUTTER_OPEN)
        {
            LOGF_INFO("%s", GetShutterStatusString(SHUTTER_OPENED));
            interface->controlShutter(ScopeDomeCard::STOP_SHUTTER);
            if (getDomeState() == DOME_UNPARKING)
            {
                SetParked(false);
            }
        }
        setShutterState(SHUTTER_OPENED);
    }
    else if (interface->getInputState(ScopeDomeCard::CLOSED1) == ISS_ON) // shutter closed switch triggered
    {
        if (m_ShutterState == SHUTTER_MOVING && targetShutter == SHUTTER_CLOSE)
        {
            LOGF_INFO("%s", GetShutterStatusString(SHUTTER_CLOSED));
            interface->controlShutter(ScopeDomeCard::STOP_SHUTTER);

            if (getDomeState() == DOME_PARKING && DomeAbsPosNP.s != IPS_BUSY)
            {
                SetParked(true);
            }
        }
        setShutterState(SHUTTER_CLOSED);
    }
    else
    {
        setShutterState(SHUTTER_MOVING);
    }

    ISState link = interface->getInputState(ScopeDomeCard::ROTARY_LINK);
    if (link != rotaryLinkEstablished)
    {
        rotaryLinkEstablished = link;
        if(rotaryLinkEstablished == ISS_OFF)
        {
            LOG_WARN("Rotary link not established, shutter control not possible");
        }
        else
        {
            LOG_WARN("Rotary link established, shutter control now possible");
        }
    }
    return true;
}

/************************************************************************************
 *
 * ***********************************************************************************/
bool ScopeDome::UpdatePosition()
{
    rotationCounter = interface->getRotationCounter();

    // We assume counter value 0 is at home sensor position and grows counter clockwise as this is
    // how USB Card 2.1 works.
    double az = ((double)rotationCounter * -360.0 / stepsPerRevolution) + DomeHomePositionNP[0].getValue();
    az        = fmod(az, 360.0);
    if (az < 0.0)
    {
        az += 360.0;
    }
    DomeAbsPosN[0].value = az;
    LOGF_DEBUG("Dome position %f, step count %d", az, rotationCounter);
    return true;
}

/************************************************************************************
 *
 * ***********************************************************************************/
bool ScopeDome::UpdateSensorStatus()
{
    for (size_t i = 0; i < SensorsNP.size(); ++i)
    {
        SensorsNP[i].setValue(interface->getSensorValue(i));
    }
    //    SensorsN[10].value = getDewPoint(SensorsN[8].value, SensorsN[7].value);
    SensorsNP.setState(IPS_OK);
    SensorsNP.apply();
    return true;
}

/************************************************************************************
 *
 * ***********************************************************************************/
bool ScopeDome::UpdateRelayStatus()
{
    for(size_t i = 0; i < RelaysSP.size(); i++)
    {
        RelaysSP[i].setState(interface->getRelayState(i));
    }
    RelaysSP.setState(IPS_OK);
    RelaysSP.apply();
    return true;
}

/************************************************************************************
 *
 * ***********************************************************************************/
void ScopeDome::TimerHit()
{
    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    interface->updateState();

    currentStatus = interface->getStatus();

    UpdatePosition();
    UpdateShutterStatus();
    IDSetSwitch(&DomeShutterSP, nullptr);

    UpdateRelayStatus();

    if (status == DOME_HOMING)
    {
        if ((currentStatus & (ScopeDomeCard::STATUS_HOMING | ScopeDomeCard::STATUS_MOVING)) == 0)
        {
            double azDiff = DomeHomePositionNP[0].getValue() - DomeAbsPosN[0].value;

            if (azDiff > 180)
            {
                azDiff -= 360;
            }
            if (azDiff < -180)
            {
                azDiff += 360;
            }

            if (interface->getInputState(ScopeDomeCard::HOME) || fabs(azDiff) <= DomeParamN[0].value)
            {
                // Found home (or close enough)
                LOG_INFO("Home sensor found");
                status   = DOME_READY;
                targetAz = DomeHomePositionNP[0].getValue();

                // Reset counters
                interface->resetCounter();

                FindHomeSP.setState(IPS_OK);
                FindHomeSP.apply();
                setDomeState(DOME_IDLE);
            }
            else
            {
                // We overshoot, go closer
                MoveAbs(DomeHomePositionNP[0].getValue());
            }
        }
        IDSetNumber(&DomeAbsPosNP, nullptr);
    }
    else if (status == DOME_DEROTATING)
    {
        if ((currentStatus & ScopeDomeCard::STATUS_MOVING) == 0)
        {
            currentRotation = interface->getRotationCounterExt();
            LOGF_INFO("Current rotation is %d", currentRotation);
            if (abs(currentRotation) < 100)
            {
                // Close enough
                LOG_INFO("De-rotation complete");
                status = DOME_READY;
                DerotateSP.setState(IPS_OK);
                DerotateSP.apply();
                setDomeState(DOME_IDLE);
            }
            else
            {
                interface->move(currentRotation);
            }
        }
    }
    else if (status == DOME_CALIBRATING)
    {
        if ((currentStatus & (ScopeDomeCard::STATUS_CALIBRATING | ScopeDomeCard::STATUS_MOVING)) == 0)
        {
            stepsPerRevolution = interface->getStepsPerRevolution();
            LOGF_INFO("Calibration complete, steps per revolution read as %d", stepsPerRevolution);
            StepsPerRevolutionNP[0].setValue(stepsPerRevolution);
            StepsPerRevolutionNP.setState(IPS_OK);
            StepsPerRevolutionNP.apply();
            StartCalibrationSP.setState(IPS_OK);
            StartCalibrationSP.apply();
            status = DOME_READY;
            setDomeState(DOME_IDLE);
        }
    }
    else if (DomeAbsPosNP.s == IPS_BUSY)
    {
        if ((currentStatus & ScopeDomeCard::STATUS_MOVING) == 0)
        {
            // Rotation idle, are we close enough?
            double azDiff = targetAz - DomeAbsPosN[0].value;

            if (azDiff > 180)
            {
                azDiff -= 360;
            }
            if (azDiff < -180)
            {
                azDiff += 360;
            }
            if (!refineMove || fabs(azDiff) <= DomeParamN[0].value)
            {
                if (refineMove)
                {
                    DomeAbsPosN[0].value = targetAz;
                }
                DomeAbsPosNP.s = IPS_OK;
                LOG_INFO("Dome reached requested azimuth angle.");

                if (getDomeState() == DOME_PARKING)
                {
                    if (ShutterParkPolicyS[SHUTTER_CLOSE_ON_PARK].s == ISS_ON &&
                            interface->getInputState(ScopeDomeCard::CLOSED1) == ISS_OFF)
                    {
                        ControlShutter(SHUTTER_CLOSE);
                    }
                    else
                    {
                        SetParked(true);
                    }
                }
                else if (getDomeState() == DOME_UNPARKING)
                {
                    SetParked(false);
                }
                else
                {
                    setDomeState(DOME_SYNCED);
                }
            }
            else
            {
                // Refine azimuth
                MoveAbs(targetAz);
            }
        }
        IDSetNumber(&DomeAbsPosNP, nullptr);
    }
    else
    {
        IDSetNumber(&DomeAbsPosNP, nullptr);
    }

    // Read temperatures only every 10th time
    static int tmpCounter = 0;
    if (--tmpCounter <= 0)
    {
        UpdateSensorStatus();
        tmpCounter = 10;
    }

    SetTimer(getCurrentPollingPeriod());
}

/************************************************************************************
 *
 * ***********************************************************************************/
IPState ScopeDome::MoveAbs(double az)
{
    LOGF_DEBUG("MoveAbs (%f)", az);
    targetAz      = az;
    double azDiff = az - DomeAbsPosN[0].value;
    LOGF_DEBUG("azDiff = %f", azDiff);

    // Make relative (-180 - 180) regardless if it passes az 0
    if (azDiff > 180)
    {
        azDiff -= 360;
    }
    if (azDiff < -180)
    {
        azDiff += 360;
    }

    LOGF_DEBUG("azDiff rel = %f", azDiff);

    refineMove = true;
    return sendMove(azDiff);
}

/************************************************************************************
 *
 * ***********************************************************************************/
IPState ScopeDome::MoveRel(double azDiff)
{
    refineMove = false;
    return sendMove(azDiff);
}

/************************************************************************************
 *
 * ***********************************************************************************/
IPState ScopeDome::sendMove(double azDiff)
{
    if (azDiff < 0)
    {
        int steps = static_cast<int>(-azDiff * stepsPerRevolution / 360.0);
        LOGF_DEBUG("CCW (%d)", steps);
        if (steps <= 0)
        {
            return IPS_OK;
        }
        steps = compensateInertia(steps);
        LOGF_DEBUG("CCW inertia (%d)", steps);
        if (steps <= 0)
        {
            return IPS_OK;
        }
        interface->move(-steps);
    }
    else
    {
        int steps = static_cast<int>(azDiff * stepsPerRevolution / 360.0);
        LOGF_DEBUG("CW (%d)", steps);
        if (steps <= 0)
        {
            return IPS_OK;
        }
        steps = compensateInertia(steps);
        LOGF_DEBUG("CW inertia (%d)", steps);
        if (steps <= 0)
        {
            return IPS_OK;
        }
        interface->move(steps);
    }
    return IPS_BUSY;
}

/************************************************************************************
 *
 * ***********************************************************************************/
IPState ScopeDome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    // Map to button outputs
    if (operation == MOTION_START)
    {
        refineMove = false;
        if (dir == DOME_CW)
        {
            interface->setOutputState(ScopeDomeCard::CCW, ISS_OFF);
            interface->setOutputState(ScopeDomeCard::CW, ISS_ON);
        }
        else
        {
            interface->setOutputState(ScopeDomeCard::CW, ISS_OFF);
            interface->setOutputState(ScopeDomeCard::CCW, ISS_ON);
        }
        return IPS_BUSY;
    }
    interface->setOutputState(ScopeDomeCard::CW, ISS_OFF);
    interface->setOutputState(ScopeDomeCard::CCW, ISS_OFF);
    return IPS_OK;
}

/************************************************************************************
 *
 * ***********************************************************************************/
IPState ScopeDome::Park()
{
    // First move to park position and then optionally close shutter
    targetAz  = GetAxis1Park();
    IPState s = MoveAbs(targetAz);
    if (s == IPS_OK && ShutterParkPolicyS[SHUTTER_CLOSE_ON_PARK].s == ISS_ON)
    {
        // Already at park position, just close if needed
        return ControlShutter(SHUTTER_CLOSE);
    }
    return s;
}

/************************************************************************************
 *
 * ***********************************************************************************/
IPState ScopeDome::UnPark()
{
    if (ShutterParkPolicyS[SHUTTER_OPEN_ON_UNPARK].s == ISS_ON)
    {
        return ControlShutter(SHUTTER_OPEN);
    }
    return IPS_OK;
}

/************************************************************************************
 *
 * ***********************************************************************************/
IPState ScopeDome::ControlShutter(ShutterOperation operation)
{
    LOGF_INFO("Control shutter %d", (int)operation);
    targetShutter = operation;
    if (operation == SHUTTER_OPEN)
    {
        LOG_INFO("Opening shutter");
        if (interface->getInputState(ScopeDomeCard::OPEN1))
        {
            LOG_INFO("Shutter already open");
            return IPS_OK;
        }
        interface->controlShutter(ScopeDomeCard::OPEN_SHUTTER);
    }
    else
    {
        LOG_INFO("Closing shutter");
        if (interface->getInputState(ScopeDomeCard::CLOSED1))
        {
            LOG_INFO("Shutter already closed");
            return IPS_OK;
        }
        interface->controlShutter(ScopeDomeCard::CLOSE_SHUTTER);
    }

    m_ShutterState = SHUTTER_MOVING;
    return IPS_BUSY;
}

/************************************************************************************
 *
 * ***********************************************************************************/
bool ScopeDome::Abort()
{
    interface->abort();
    status = DOME_READY;
    return true;
}

/************************************************************************************
 *
 * ***********************************************************************************/
bool ScopeDome::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);

    DomeHomePositionNP.save(fp);
    HomeSensorPolaritySP.save(fp);
    CardTypeSP.save(fp);
    CredentialsTP.save(fp);
    return true;
}

/************************************************************************************
 *
 * ***********************************************************************************/
bool ScopeDome::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosN[0].value);
    return true;
}
/************************************************************************************
 *
 * ***********************************************************************************/

bool ScopeDome::SetDefaultPark()
{
    // By default set position to 90
    SetAxis1Park(90);
    return true;
}

/************************************************************************************
 *
 * ***********************************************************************************/
void ScopeDome::reconnect()
{
    // Reconnect serial port after write error
    LOG_INFO("Reconnecting serial port");
    reconnecting = true;
    serialConnection->Disconnect();
    usleep(1000000); // 1s
    serialConnection->Connect();
    PortFD = serialConnection->getPortFD();
    interface->setPortFD(PortFD);
    LOG_INFO("Reconnected");
    reconnecting = false;
}

/*
 * Saturation Vapor Pressure formula for range -100..0 Deg. C.
 * This is taken from
 *   ITS-90 Formulations for Vapor Pressure, Frostpoint Temperature,
 *   Dewpoint Temperature, and Enhancement Factors in the Range 100 to +100 C
 * by Bob Hardy
 * as published in "The Proceedings of the Third International Symposium on
 * Humidity & Moisture",
 * Teddington, London, England, April 1998
 */
static const float k0 = -5.8666426e3;
static const float k1 = 2.232870244e1;
static const float k2 = 1.39387003e-2;
static const float k3 = -3.4262402e-5;
static const float k4 = 2.7040955e-8;
static const float k5 = 6.7063522e-1;

static float pvsIce(float T)
{
    float lnP = k0 / T + k1 + (k2 + (k3 + (k4 * T)) * T) * T + k5 * log(T);
    return exp(lnP);
}

/**
 * Saturation Vapor Pressure formula for range 273..678 Deg. K.
 * This is taken from the
 *   Release on the IAPWS Industrial Formulation 1997
 *   for the Thermodynamic Properties of Water and Steam
 * by IAPWS (International Association for the Properties of Water and Steam),
 * Erlangen, Germany, September 1997.
 *
 * This is Equation (30) in Section 8.1 "The Saturation-Pressure Equation (Basic
 * Equation)"
 */

static const float n1  = 0.11670521452767e4;
static const float n6  = 0.14915108613530e2;
static const float n2  = -0.72421316703206e6;
static const float n7  = -0.48232657361591e4;
static const float n3  = -0.17073846940092e2;
static const float n8  = 0.40511340542057e6;
static const float n4  = 0.12020824702470e5;
static const float n9  = -0.23855557567849;
static const float n5  = -0.32325550322333e7;
static const float n10 = 0.65017534844798e3;

static float pvsWater(float T)
{
    float th = T + n9 / (T - n10);
    float A  = (th + n1) * th + n2;
    ;
    float B = (n3 * th + n4) * th + n5;
    float C = (n6 * th + n7) * th + n8;
    ;

    float p = 2.0f * C / (-B + sqrt(B * B - 4.0f * A * C));
    p *= p;
    p *= p;
    return p * 1e6;
}

static const float C_OFFSET = 273.15f;
static const float minT     = 173; // -100 Deg. C.
static const float maxT     = 678;

static float PVS(float T)
{
    if (T < minT || T > maxT)
        return 0;
    else if (T < C_OFFSET)
        return pvsIce(T);
    else
        return pvsWater(T);
}

static float solve(float (*f)(float), float y, float x0)
{
    float x      = x0;
    int maxCount = 10;
    int count    = 0;
    while (++count < maxCount)
    {
        float xNew;
        float dx = x / 1000;
        float z  = f(x);
        xNew     = x + dx * (y - z) / (f(x + dx) - z);
        if (fabs((xNew - x) / xNew) < 0.0001f)
            return xNew;
        x = xNew;
    }
    return 0;
}

float ScopeDome::getDewPoint(float RH, float T)
{
    T = T + C_OFFSET;
    return solve(PVS, RH / 100 * PVS(T), T) - C_OFFSET;
}

int ScopeDome::compensateInertia(int steps)
{
    if (inertiaTable.size() == 0)
    {
        LOGF_DEBUG("inertia passthrough %d", steps);
        return steps; // pass value as such if we don't have enough data
    }

    for (uint16_t out = 0; out < inertiaTable.size(); out++)
    {
        if (inertiaTable[out] > steps)
        {
            LOGF_DEBUG("inertia %d -> %d", steps, out - 1);
            return out - 1;
        }
    }
    // Check difference from largest table entry and assume we have
    // similar inertia also after that
    int lastEntry = inertiaTable.size() - 1;
    int inertia   = inertiaTable[lastEntry] - lastEntry;
    int movement  = steps - inertia;
    LOGF_DEBUG("inertia %d -> %d", steps, movement);
    if (movement <= 0)
        return 0;

    return movement;
}
