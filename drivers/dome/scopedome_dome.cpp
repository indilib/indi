/*******************************************************************************
 ScopeDome Dome INDI Driver

 Copyright(c) 2017-2019 Jarno Paananen. All rights reserved.

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

#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>
#include <wordexp.h>
#include <unistd.h>

// We declare an auto pointer to ScopeDome.
std::unique_ptr<ScopeDome> scopeDome(new ScopeDome());

ScopeDome::ScopeDome()
{
    setVersion(1, 2);
    targetAz         = 0;
    m_ShutterState     = SHUTTER_UNKNOWN;
    simShutterStatus = SHUTTER_CLOSED;

    status        = DOME_UNKNOWN;
    targetShutter = SHUTTER_CLOSE;

    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_ABS_MOVE | DOME_CAN_REL_MOVE | DOME_CAN_PARK | DOME_HAS_SHUTTER);

    stepsPerTurn = -1;

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
                if (sscanf(line, "%d ;%d", &step, &result) != 2)
                {
                    sscanf(line, "%d;%d", &step, &result);
                }
                if (step == lineNum)
                {
                    inertiaTable.push_back(result);
                }
                lineNum++;
            }
            fclose(inertia);
            LOGF_INFO("Read inertia file %s", wexp.we_wordv[0]);
        }
        else
        {
            LOG_INFO("Could not read inertia file, please generate one with Windows driver setup and copy to "
                     "~/.indi/ScopeDome_DomeInertia_Table.txt");
        }
    }
    wordfree(&wexp);
}

bool ScopeDome::initProperties()
{
    INDI::Dome::initProperties();

    IUFillNumber(&DomeHomePositionN[0], "DH_POSITION", "AZ (deg)", "%6.2f", 0.0, 360.0, 1.0, 0.0);
    IUFillNumberVector(&DomeHomePositionNP, DomeHomePositionN, 1, getDeviceName(), "DOME_HOME_POSITION",
                       "Home sensor position", SITE_TAB, IP_RW, 60, IPS_OK);

    IUFillSwitch(&ParkShutterS[0], "ON", "On", ISS_ON);
    IUFillSwitch(&ParkShutterS[1], "OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&ParkShutterSP, ParkShutterS, 2, getDeviceName(), "PARK_SHUTTER", "Park controls shutter",
                       OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

    IUFillSwitch(&FindHomeS[0], "START", "Start", ISS_OFF);
    IUFillSwitchVector(&FindHomeSP, FindHomeS, 1, getDeviceName(), "FIND_HOME", "Find home", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_OK);

    IUFillSwitch(&DerotateS[0], "START", "Start", ISS_OFF);
    IUFillSwitchVector(&DerotateSP, DerotateS, 1, getDeviceName(), "DEROTATE", "Derotate", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_OK);

    IUFillSwitch(&PowerRelaysS[0], "CCD", "CCD", ISS_OFF);
    IUFillSwitch(&PowerRelaysS[1], "SCOPE", "Telescope", ISS_OFF);
    IUFillSwitch(&PowerRelaysS[2], "LIGHT", "Light", ISS_OFF);
    IUFillSwitch(&PowerRelaysS[3], "FAN", "Fan", ISS_OFF);
    IUFillSwitchVector(&PowerRelaysSP, PowerRelaysS, 4, getDeviceName(), "POWER_RELAYS", "Power relays",
                       MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    IUFillSwitch(&RelaysS[0], "RELAY_1", "Relay 1 (reset)", ISS_OFF);
    IUFillSwitch(&RelaysS[1], "RELAY_2", "Relay 2 (heater)", ISS_OFF);
    IUFillSwitch(&RelaysS[2], "RELAY_3", "Relay 3", ISS_OFF);
    IUFillSwitch(&RelaysS[3], "RELAY_4", "Relay 4", ISS_OFF);
    IUFillSwitchVector(&RelaysSP, RelaysS, 4, getDeviceName(), "RELAYS", "Relays", MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY,
                       0, IPS_IDLE);

    IUFillSwitch(&AutoCloseS[0], "CLOUD", "Cloud sensor", ISS_OFF);
    IUFillSwitch(&AutoCloseS[1], "RAIN", "Rain sensor", ISS_OFF);
    IUFillSwitch(&AutoCloseS[2], "FREE", "Free input", ISS_OFF);
    IUFillSwitch(&AutoCloseS[3], "NO_POWER", "No power", ISS_OFF);
    IUFillSwitch(&AutoCloseS[4], "DOME_LOW", "Low dome battery", ISS_OFF);
    IUFillSwitch(&AutoCloseS[5], "SHUTTER_LOW", "Low shutter battery", ISS_OFF);
    IUFillSwitch(&AutoCloseS[6], "WEATHER", "Bad weather", ISS_OFF);
    IUFillSwitch(&AutoCloseS[7], "LOST_CONNECTION", "Lost connection", ISS_OFF);
    IUFillSwitchVector(&AutoCloseSP, AutoCloseS, 8, getDeviceName(), "AUTO_CLOSE", "Close shutter automatically",
                       SITE_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    IUFillNumber(&EnvironmentSensorsN[0], "LINK_STRENGTH", "Shutter link strength", "%3.0f", 0.0, 100.0, 1.0, 0.0);
    IUFillNumber(&EnvironmentSensorsN[1], "SHUTTER_POWER", "Shutter internal power", "%2.2f", 0.0, 100.0, 1.0, 0.0);
    IUFillNumber(&EnvironmentSensorsN[2], "SHUTTER_BATTERY", "Shutter battery power", "%2.2f", 0.0, 100.0, 1.0, 0.0);
    IUFillNumber(&EnvironmentSensorsN[3], "CARD_POWER", "Card internal power", "%2.2f", 0.0, 100.0, 1.0, 0.0);
    IUFillNumber(&EnvironmentSensorsN[4], "CARD_BATTERY", "Card battery power", "%2.2f", 0.0, 100.0, 1.0, 0.0);
    IUFillNumber(&EnvironmentSensorsN[5], "TEMP_DOME_IN", "Temperature in dome", "%2.2f", -100.0, 100.0, 1.0, 0.0);
    IUFillNumber(&EnvironmentSensorsN[6], "TEMP_DOME_OUT", "Temperature outside dome", "%2.2f", -100.0, 100.0, 1.0,
                 0.0);
    IUFillNumber(&EnvironmentSensorsN[7], "TEMP_DOME_HUMIDITY", "Temperature humidity sensor", "%2.2f", -100.0, 100.0,
                 1.0, 0.0);
    IUFillNumber(&EnvironmentSensorsN[8], "HUMIDITY", "Humidity", "%3.2f", 0.0, 100.0, 1.0, 0.0);
    IUFillNumber(&EnvironmentSensorsN[9], "PRESSURE", "Pressure", "%4.1f", 0.0, 2000.0, 1.0, 0.0);
    IUFillNumber(&EnvironmentSensorsN[10], "DEW_POINT", "Dew point", "%2.2f", -100.0, 100.0, 1.0, 0.0);
    IUFillNumberVector(&EnvironmentSensorsNP, EnvironmentSensorsN, 11, getDeviceName(), "SCOPEDOME_SENSORS",
                       "Environment sensors", INFO_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&SensorsS[0], "AZ_COUNTER", "Az counter", ISS_OFF);
    IUFillSwitch(&SensorsS[1], "ROTATE_CCW", "Rotate CCW", ISS_OFF);
    IUFillSwitch(&SensorsS[2], "HOME", "Dome at home", ISS_OFF);
    IUFillSwitch(&SensorsS[3], "OPEN_1", "Shutter 1 open", ISS_OFF);
    IUFillSwitch(&SensorsS[4], "CLOSE_1", "Shutter 1 closed", ISS_OFF);
    IUFillSwitch(&SensorsS[5], "OPEN_2", "Shutter 2 open", ISS_OFF);
    IUFillSwitch(&SensorsS[6], "CLOSE_2", "Shutter 2 closed", ISS_OFF);
    IUFillSwitch(&SensorsS[7], "SCOPE_HOME", "Scope at home", ISS_OFF);
    IUFillSwitch(&SensorsS[8], "RAIN", "Rain sensor", ISS_OFF);
    IUFillSwitch(&SensorsS[9], "CLOUD", "Cloud sensor", ISS_OFF);
    IUFillSwitch(&SensorsS[10], "SAFE", "Observatory safe", ISS_OFF);
    IUFillSwitch(&SensorsS[11], "LINK", "Rotary link", ISS_OFF);
    IUFillSwitch(&SensorsS[12], "FREE", "Free input", ISS_OFF);
    IUFillSwitchVector(&SensorsSP, SensorsS, 13, getDeviceName(), "INPUTS", "Input sensors", INFO_TAB, IP_RO,
                       ISR_NOFMANY, 0, IPS_IDLE);

    IUFillNumber(&FirmwareVersionsN[0], "MAIN", "Main part", "%2.2f", 0.0, 99.0, 1.0, 0.0);
    IUFillNumber(&FirmwareVersionsN[1], "ROTARY", "Rotary part", "%2.1f", 0.0, 99.0, 1.0, 0.0);
    IUFillNumberVector(&FirmwareVersionsNP, FirmwareVersionsN, 2, getDeviceName(), "FIRMWARE_VERSION",
                       "Firmware versions", INFO_TAB, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&StepsPerRevolutionN[0], "STEPS", "Steps per revolution", "%5.0f", 0.0, 99999.0, 1.0, 0.0);
    IUFillNumberVector(&StepsPerRevolutionNP, StepsPerRevolutionN, 1, getDeviceName(), "CALIBRATION_VALUES",
                       "Calibration values", SITE_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&CalibrationNeededS[0], "CALIBRATION_NEEDED", "Calibration needed", ISS_OFF);
    IUFillSwitchVector(&CalibrationNeededSP, CalibrationNeededS, 1, getDeviceName(), "CALIBRATION_STATUS",
                       "Calibration status", SITE_TAB, IP_RO, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&StartCalibrationS[0], "START", "Start", ISS_OFF);
    IUFillSwitchVector(&StartCalibrationSP, StartCalibrationS, 1, getDeviceName(), "RUN_CALIBRATION", "Run calibration",
                       SITE_TAB, IP_RW, ISR_ATMOST1, 0, IPS_OK);

    SetParkDataType(PARK_AZ);

    addAuxControls();

    // Set serial parameters
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

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

    readU32(GetImpPerTurn, stepsPerTurn);
    LOGF_INFO("Steps per turn read as %d", stepsPerTurn);
    StepsPerRevolutionN[0].value = stepsPerTurn;
    StepsPerRevolutionNP.s       = IPS_OK;
    IDSetNumber(&StepsPerRevolutionNP, nullptr);

    readS32(GetHomeSensorPosition, homePosition);
    LOGF_INFO("Home position read as %d", homePosition);

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

    uint8_t calibrationNeeded = false;
    readU8(IsFullSystemCalReq, calibrationNeeded);
    CalibrationNeededS[0].s = calibrationNeeded ? ISS_ON : ISS_OFF;
    CalibrationNeededSP.s   = IPS_OK;
    IDSetSwitch(&CalibrationNeededSP, nullptr);

    uint16_t fwVersion;
    readU16(GetVersionFirmware, fwVersion);
    FirmwareVersionsN[0].value = fwVersion / 100.0;

    uint8_t fwVersionRotary;
    readU8(GetVersionFirmwareRotary, fwVersionRotary);
    FirmwareVersionsN[1].value = (fwVersionRotary + 9) / 10.0;
    FirmwareVersionsNP.s       = IPS_OK;
    IDSetNumber(&FirmwareVersionsNP, nullptr);
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool ScopeDome::Handshake()
{
    return Ack();
}

/************************************************************************************
 *
* ***********************************************************************************/
const char *ScopeDome::getDefaultName()
{
    return (const char *)"ScopeDome Dome";
}

/************************************************************************************
 *
* ***********************************************************************************/
bool ScopeDome::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        defineProperty(&FindHomeSP);
        defineProperty(&DerotateSP);
        defineProperty(&AutoCloseSP);
        defineProperty(&PowerRelaysSP);
        defineProperty(&RelaysSP);
        defineProperty(&DomeHomePositionNP);
        defineProperty(&EnvironmentSensorsNP);
        defineProperty(&SensorsSP);
        defineProperty(&ParkShutterSP);
        defineProperty(&StepsPerRevolutionNP);
        defineProperty(&CalibrationNeededSP);
        defineProperty(&StartCalibrationSP);
        defineProperty(&FirmwareVersionsNP);
        SetupParms();
    }
    else
    {
        deleteProperty(FindHomeSP.name);
        deleteProperty(DerotateSP.name);
        deleteProperty(PowerRelaysSP.name);
        deleteProperty(RelaysSP.name);
        deleteProperty(SensorsSP.name);
        deleteProperty(AutoCloseSP.name);
        deleteProperty(DomeHomePositionNP.name);
        deleteProperty(EnvironmentSensorsNP.name);
        deleteProperty(ParkShutterSP.name);
        deleteProperty(StepsPerRevolutionNP.name);
        deleteProperty(CalibrationNeededSP.name);
        deleteProperty(StartCalibrationSP.name);
        deleteProperty(FirmwareVersionsNP.name);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool ScopeDome::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, FindHomeSP.name) == 0)
        {
            if (status != DOME_HOMING)
            {
                LOG_INFO("Finding home sensor");
                status = DOME_HOMING;
                IUResetSwitch(&FindHomeSP);
                DomeAbsPosNP.s = IPS_BUSY;
                FindHomeSP.s   = IPS_BUSY;
                IDSetSwitch(&FindHomeSP, nullptr);
                writeCmd(FindHome);
            }
            return true;
        }

        if (strcmp(name, DerotateSP.name) == 0)
        {
            if (status != DOME_DEROTATING)
            {
                LOG_INFO("De-rotating started");
                status = DOME_DEROTATING;
                IUResetSwitch(&DerotateSP);
                DomeAbsPosNP.s = IPS_BUSY;
                DerotateSP.s   = IPS_BUSY;
                IDSetSwitch(&DerotateSP, nullptr);
            }
            return true;
        }

        if (strcmp(name, StartCalibrationSP.name) == 0)
        {
            if (status != DOME_CALIBRATING)
            {
                LOG_INFO("Calibration started");
                status = DOME_CALIBRATING;
                IUResetSwitch(&StartCalibrationSP);
                DomeAbsPosNP.s       = IPS_BUSY;
                StartCalibrationSP.s = IPS_BUSY;
                IDSetSwitch(&StartCalibrationSP, nullptr);
                writeCmd(FullSystemCal);
            }
            return true;
        }

        if (strcmp(name, PowerRelaysSP.name) == 0)
        {
            IUUpdateSwitch(&PowerRelaysSP, states, names, n);
            setOutputState(OUT_CCD, PowerRelaysS[0].s);
            setOutputState(OUT_SCOPE, PowerRelaysS[1].s);
            setOutputState(OUT_LIGHT, PowerRelaysS[2].s);
            setOutputState(OUT_FAN, PowerRelaysS[3].s);
            IDSetSwitch(&PowerRelaysSP, nullptr);
            return true;
        }

        if (strcmp(name, RelaysSP.name) == 0)
        {
            IUUpdateSwitch(&RelaysSP, states, names, n);
            setOutputState(OUT_RELAY1, RelaysS[0].s);
            setOutputState(OUT_RELAY2, RelaysS[1].s);
            setOutputState(OUT_RELAY3, RelaysS[2].s);
            setOutputState(OUT_RELAY4, RelaysS[3].s);
            IDSetSwitch(&RelaysSP, nullptr);
            return true;
        }

        if (strcmp(name, ParkShutterSP.name) == 0)
        {
            IUUpdateSwitch(&ParkShutterSP, states, names, n);
            ParkShutterSP.s = IPS_OK;
            IDSetSwitch(&ParkShutterSP, nullptr);
            return true;
        }
    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

bool ScopeDome::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, DomeHomePositionNP.name) == 0)
        {
            IUUpdateNumber(&DomeHomePositionNP, values, names, n);
            DomeHomePositionNP.s = IPS_OK;
            IDSetNumber(&DomeHomePositionNP, nullptr);
            return true;
        }
    }
    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool ScopeDome::Ack()
{
    sim = isSimulation();

    if (sim)
    {
        interface.reset(static_cast<ScopeDomeCard *>(new ScopeDomeSim()));
    }
    else
    {
        // TODO, detect card version and instantiate correct one
        interface.reset(static_cast<ScopeDomeCard *>(new ScopeDomeUSB21(PortFD)));
    }

    return interface->detect();
}

/************************************************************************************
 *
* ***********************************************************************************/
bool ScopeDome::UpdateShutterStatus()
{
    int rc = readBuffer(GetAllDigitalExt, 5, digitalSensorState);
    if (rc != 0)
    {
        LOGF_ERROR("Error reading input state: %d", rc);
        return false;
    }
    // LOGF_INFO("digitalext %x %x %x %x %x", digitalSensorState[0],
    // digitalSensorState[1], digitalSensorState[2], digitalSensorState[3],
    // digitalSensorState[4]);
    SensorsS[0].s  = getInputState(IN_ENCODER);
    SensorsS[1].s  = ISS_OFF; // ?
    SensorsS[2].s  = getInputState(IN_HOME);
    SensorsS[3].s  = getInputState(IN_OPEN1);
    SensorsS[4].s  = getInputState(IN_CLOSED1);
    SensorsS[5].s  = getInputState(IN_OPEN2);
    SensorsS[6].s  = getInputState(IN_CLOSED2);
    SensorsS[7].s  = getInputState(IN_S_HOME);
    SensorsS[8].s  = getInputState(IN_CLOUDS);
    SensorsS[9].s  = getInputState(IN_CLOUD);
    SensorsS[10].s = getInputState(IN_SAFE);
    SensorsS[11].s = getInputState(IN_ROT_LINK);
    SensorsS[12].s = getInputState(IN_FREE);
    SensorsSP.s    = IPS_OK;
    IDSetSwitch(&SensorsSP, nullptr);

    DomeShutterSP.s = IPS_OK;
    IUResetSwitch(&DomeShutterSP);

    if (getInputState(IN_OPEN1) == ISS_ON) // shutter open switch triggered
    {
        if (m_ShutterState == SHUTTER_MOVING && targetShutter == SHUTTER_OPEN)
        {
            LOGF_INFO("%s", GetShutterStatusString(SHUTTER_OPENED));
            setOutputState(OUT_OPEN1, ISS_OFF);
            m_ShutterState = SHUTTER_OPENED;
            if (getDomeState() == DOME_UNPARKING)
                SetParked(false);
        }
        DomeShutterS[SHUTTER_OPEN].s = ISS_ON;
    }
    else if (getInputState(IN_CLOSED1) == ISS_ON) // shutter closed switch triggered
    {
        if (m_ShutterState == SHUTTER_MOVING && targetShutter == SHUTTER_CLOSE)
        {
            LOGF_INFO("%s", GetShutterStatusString(SHUTTER_CLOSED));
            setOutputState(OUT_CLOSE1, ISS_OFF);
            m_ShutterState = SHUTTER_CLOSED;

            if (getDomeState() == DOME_PARKING && DomeAbsPosNP.s != IPS_BUSY)
            {
                SetParked(true);
            }
        }
        DomeShutterS[SHUTTER_CLOSE].s = ISS_ON;
    }
    else
    {
        m_ShutterState    = SHUTTER_MOVING;
        DomeShutterSP.s = IPS_BUSY;
    }
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool ScopeDome::UpdatePosition()
{
    //    int counter = readS32(GetCounterExt);
    readS16(GetCounter, rotationCounter);

    //    LOGF_INFO("Counters are %d - %d", counter, counter2);

    // We assume counter value 0 is at home sensor position
    double az = ((double)rotationCounter * -360.0 / stepsPerTurn) + DomeHomePositionN[0].value;
    az        = fmod(az, 360.0);
    if (az < 0.0)
    {
        az += 360.0;
    }
    DomeAbsPosN[0].value = az;
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool ScopeDome::UpdateSensorStatus()
{
    readU8(GetLinkStrength, linkStrength);
    readFloat(GetAnalog1, sensors[0]);
    readFloat(GetAnalog2, sensors[1]);
    readFloat(GetMainAnalog1, sensors[2]);
    readFloat(GetMainAnalog2, sensors[3]);
    readFloat(GetTempIn, sensors[4]);
    readFloat(GetTempOut, sensors[5]);
    readFloat(GetTempHum, sensors[6]);
    readFloat(GetHum, sensors[7]);
    readFloat(GetPressure, sensors[8]);

    EnvironmentSensorsN[0].value = linkStrength;
    for (int i = 0; i < 9; ++i)
    {
        EnvironmentSensorsN[i + 1].value = sensors[i];
    }
    EnvironmentSensorsN[10].value = getDewPoint(EnvironmentSensorsN[8].value, EnvironmentSensorsN[7].value);
    EnvironmentSensorsNP.s        = IPS_OK;

    IDSetNumber(&EnvironmentSensorsNP, nullptr);

    // My shutter unit occasionally disconnects so implement a simple watchdog
    // to check for link strength and reset the controller if link is lost for
    // more than 5 polling cycles
    static int count = 0;
    if (linkStrength == 0)
    {
        count++;
        if (count > 5)
        {
            // Issue reset
            setOutputState(OUT_RELAY1, ISS_ON);
            count = 0;
        }
    }
    else
    {
        count = 0;
    }
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool ScopeDome::UpdateRelayStatus()
{
    PowerRelaysS[0].s = getInputState(OUT_CCD);
    PowerRelaysS[1].s = getInputState(OUT_SCOPE);
    PowerRelaysS[2].s = getInputState(OUT_LIGHT);
    PowerRelaysS[3].s = getInputState(OUT_FAN);
    PowerRelaysSP.s   = IPS_OK;
    IDSetSwitch(&PowerRelaysSP, nullptr);

    RelaysS[0].s = getInputState(OUT_RELAY1);
    RelaysS[1].s = getInputState(OUT_RELAY2);
    RelaysS[2].s = getInputState(OUT_RELAY3);
    RelaysS[3].s = getInputState(OUT_RELAY4);
    RelaysSP.s   = IPS_OK;
    IDSetSwitch(&RelaysSP, nullptr);
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
void ScopeDome::TimerHit()
{
    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    readU16(GetStatus, currentStatus);
    // LOGF_INFO("Status: %x", currentStatus);
    UpdatePosition();

    UpdateShutterStatus();
    IDSetSwitch(&DomeShutterSP, nullptr);

    UpdateRelayStatus();

    if (status == DOME_HOMING)
    {
        if ((currentStatus & (STATUS_HOMING | STATUS_MOVING)) == 0)
        {
            double azDiff = DomeHomePositionN[0].value - DomeAbsPosN[0].value;

            if (azDiff > 180)
            {
                azDiff -= 360;
            }
            if (azDiff < -180)
            {
                azDiff += 360;
            }

            if (getInputState(IN_HOME) || fabs(azDiff) <= DomeParamN[0].value)
            {
                // Found home (or close enough)
                LOG_INFO("Home sensor found");
                status   = DOME_READY;
                targetAz = DomeHomePositionN[0].value;

                // Reset counters
                writeCmd(ResetCounter);
                writeCmd(ResetCounterExt);

                FindHomeSP.s   = IPS_OK;
                DomeAbsPosNP.s = IPS_OK;
                IDSetSwitch(&FindHomeSP, nullptr);
            }
            else
            {
                // We overshoot, go closer
                MoveAbs(DomeHomePositionN[0].value);
            }
        }
        IDSetNumber(&DomeAbsPosNP, nullptr);
    }
    else if (status == DOME_DEROTATING)
    {
        if ((currentStatus & STATUS_MOVING) == 0)
        {
            readS32(GetCounterExt, currentRotation);
            LOGF_INFO("Current rotation is %d", currentRotation);
            if (abs(currentRotation) < 100)
            {
                // Close enough
                LOG_INFO("De-rotation complete");
                status         = DOME_READY;
                DerotateSP.s   = IPS_OK;
                DomeAbsPosNP.s = IPS_OK;
                IDSetSwitch(&DerotateSP, nullptr);
            }
            else
            {
                if (currentRotation < 0)
                {
                    writeU16(CCWRotation, compensateInertia(-currentRotation));
                }
                else
                {
                    writeU16(CWRotation, compensateInertia(currentRotation));
                }
            }
        }
        IDSetNumber(&DomeAbsPosNP, nullptr);
    }
    else if (status == DOME_CALIBRATING)
    {
        if ((currentStatus & (STATUS_CALIBRATING | STATUS_MOVING)) == 0)
        {
            readU32(GetImpPerTurn, stepsPerTurn);
            LOGF_INFO("Calibration complete, steps per turn read as %d", stepsPerTurn);
            StepsPerRevolutionN[0].value = stepsPerTurn;
            StepsPerRevolutionNP.s       = IPS_OK;
            IDSetNumber(&StepsPerRevolutionNP, nullptr);
            StartCalibrationSP.s = IPS_OK;
            DomeAbsPosNP.s       = IPS_OK;
            IDSetSwitch(&StartCalibrationSP, nullptr);
            status = DOME_READY;
        }
    }
    else if (DomeAbsPosNP.s == IPS_BUSY)
    {
        if ((currentStatus & STATUS_MOVING) == 0)
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
                    DomeAbsPosN[0].value = targetAz;
                DomeAbsPosNP.s = IPS_OK;
                LOG_INFO("Dome reached requested azimuth angle.");

                if (getDomeState() == DOME_PARKING)
                {
                    if (ParkShutterS[0].s == ISS_ON && getInputState(IN_CLOSED1) == ISS_OFF)
                    {
                        ControlShutter(SHUTTER_CLOSE);
                    }
                    else
                    {
                        SetParked(true);
                    }
                }
                else if (getDomeState() == DOME_UNPARKING)
                    SetParked(false);
                else
                    setDomeState(DOME_SYNCED);
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
        IDSetNumber(&DomeAbsPosNP, nullptr);

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
    int rc;

    if (azDiff < 0)
    {
        uint16_t steps = (uint16_t)(-azDiff * stepsPerTurn / 360.0);
        LOGF_DEBUG("CCW (%d)", steps);
        steps = compensateInertia(steps);
        LOGF_DEBUG("CCW inertia (%d)", steps);
        if (steps == 0)
            return IPS_OK;
        rc = writeU16(CCWRotation, steps);
    }
    else
    {
        uint16_t steps = (uint16_t)(azDiff * stepsPerTurn / 360.0);
        LOGF_DEBUG("CW (%d)", steps);
        steps = compensateInertia(steps);
        LOGF_DEBUG("CW inertia (%d)", steps);
        if (steps == 0)
            return IPS_OK;
        rc = writeU16(CWRotation, steps);
    }
    if (rc != 0)
    {
        LOGF_ERROR("Error moving dome: %d", rc);
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
            setOutputState(OUT_CW, ISS_ON);
            setOutputState(OUT_CCW, ISS_OFF);
        }
        else
        {
            setOutputState(OUT_CW, ISS_OFF);
            setOutputState(OUT_CCW, ISS_ON);
        }
        return IPS_BUSY;
    }
    setOutputState(OUT_CW, ISS_OFF);
    setOutputState(OUT_CCW, ISS_OFF);
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
    if (s == IPS_OK && ParkShutterS[0].s == ISS_ON)
    {
        // Already at home, just close if needed
        return ControlShutter(SHUTTER_CLOSE);
    }
    return s;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState ScopeDome::UnPark()
{
    if (ParkShutterS[0].s == ISS_ON)
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
        if (getInputState(IN_OPEN1))
        {
            LOG_INFO("Shutter already open");
            return IPS_OK;
        }
        setOutputState(OUT_CLOSE1, ISS_OFF);
        setOutputState(OUT_OPEN1, ISS_ON);
    }
    else
    {
        LOG_INFO("Closing shutter");
        if (getInputState(IN_CLOSED1))
        {
            LOG_INFO("Shutter already closed");
            return IPS_OK;
        }
        setOutputState(OUT_OPEN1, ISS_OFF);
        setOutputState(OUT_CLOSE1, ISS_ON);
    }

    m_ShutterState = SHUTTER_MOVING;
    return IPS_BUSY;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool ScopeDome::Abort()
{
    writeCmd(Stop);
    status = DOME_READY;
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool ScopeDome::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &DomeHomePositionNP);
    IUSaveConfigSwitch(fp, &ParkShutterSP);
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
bool ScopeDome::readFloat(ScopeDomeCommand cmd, float &dst)
{
    float value;
    ScopeDomeCommand c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = interface->write(cmd);
        if (rc == 0)
            rc = interface->readBuf(c, 4, (uint8_t *)&value);
        else
            reconnect();
    } while (rc != 0 && --retryCount);
    //    LOGF_ERROR("readFloat: %d %f", cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

bool ScopeDome::readU8(ScopeDomeCommand cmd, uint8_t &dst)
{
    uint8_t value;
    ScopeDomeCommand c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = interface->write(cmd);
        if (rc == 0)
            rc = interface->readBuf(c, 1, &value);
        else
            reconnect();
    } while (rc != 0 && --retryCount);
    //    LOGF_ERROR("readU8: %d %x", cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

bool ScopeDome::readS8(ScopeDomeCommand cmd, int8_t &dst)
{
    int8_t value;
    ScopeDomeCommand c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = interface->write(cmd);
        if (rc == 0)
            rc = interface->readBuf(c, 1, (uint8_t *)&value);
        else
            reconnect();
    } while (rc != 0 && --retryCount);
    //    LOGF_ERROR("readS8: %d %x", cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

bool ScopeDome::readU16(ScopeDomeCommand cmd, uint16_t &dst)
{
    uint16_t value;
    ScopeDomeCommand c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = interface->write(cmd);
        if (rc == 0)
            rc = interface->readBuf(c, 2, (uint8_t *)&value);
        else
            reconnect();
    } while (rc != 0 && --retryCount);
    //    LOGF_ERROR("readU16: %d %x", cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

bool ScopeDome::readS16(ScopeDomeCommand cmd, int16_t &dst)
{
    int16_t value;
    ScopeDomeCommand c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = interface->write(cmd);
        if (rc == 0)
            rc = interface->readBuf(c, 2, (uint8_t *)&value);
        else
            reconnect();
    } while (rc != 0 && --retryCount);
    //    LOGF_ERROR("readS16: %d %x", cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

bool ScopeDome::readU32(ScopeDomeCommand cmd, uint32_t &dst)
{
    uint32_t value;
    ScopeDomeCommand c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = interface->write(cmd);
        if (rc == 0)
            rc = interface->readBuf(c, 4, (uint8_t *)&value);
        else
            reconnect();
    } while (rc != 0 && --retryCount);
    //    LOGF_ERROR("readU32: %d %x", cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

bool ScopeDome::readS32(ScopeDomeCommand cmd, int32_t &dst)
{
    int32_t value;
    ScopeDomeCommand c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = interface->write(cmd);
        if (rc == 0)
            rc = interface->readBuf(c, 4, (uint8_t *)&value);
        else
            reconnect();
    } while (rc != 0 && --retryCount);
    //    LOGF_ERROR("readU32: %d %x", cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

int ScopeDome::readBuffer(ScopeDomeCommand cmd, int len, uint8_t *cbuf)
{
    int rc;
    int retryCount = 2;
    ScopeDomeCommand c;
    do
    {
        rc = interface->write(cmd);
        if (rc == 0)
            rc = interface->readBuf(c, len, cbuf);
        else
            reconnect();
    } while (rc != 0 && --retryCount);
    return rc;
}

int ScopeDome::writeCmd(ScopeDomeCommand cmd)
{
    int rc = interface->write(cmd);
    if (rc != 0)
    {
        reconnect();
        return rc;
    }
    return interface->read(cmd);
}

int ScopeDome::writeU8(ScopeDomeCommand cmd, uint8_t value)
{
    int rc = interface->writeBuf(cmd, 1, &value);
    if (rc != 0)
    {
        reconnect();
        return rc;
    }
    return interface->read(cmd);
}

int ScopeDome::writeU16(ScopeDomeCommand cmd, uint16_t value)
{
    int rc = interface->writeBuf(cmd, 2, (uint8_t *)&value);
    if (rc != 0)
    {
        reconnect();
        return rc;
    }
    return interface->read(cmd);
}

int ScopeDome::writeU32(ScopeDomeCommand cmd, uint32_t value)
{
    int rc = interface->writeBuf(cmd, 4, (uint8_t *)&value);
    if (rc != 0)
    {
        reconnect();
        return rc;
    }
    return interface->read(cmd);
}

int ScopeDome::writeBuffer(ScopeDomeCommand cmd, int len, uint8_t *cbuf)
{
    int rc = interface->writeBuf(cmd, len, cbuf);
    if (rc != 0)
    {
        reconnect();
        return rc;
    }
    return interface->read(cmd);
}

void ScopeDome::reconnect()
{
    // Reconnect serial port after write error
    LOG_INFO("Reconnecting serial port");
    serialConnection->Disconnect();
    usleep(1000000); // 1s
    serialConnection->Connect();
    PortFD = serialConnection->getPortFD();
    interface->setPortFD(PortFD);
    LOG_INFO("Reconnected");
}

ISState ScopeDome::getInputState(ScopeDomeDigitalIO channel)
{
    int ch      = (int)channel;
    int byte    = ch >> 3;
    uint8_t bit = 1 << (ch & 7);
    return (digitalSensorState[byte] & bit) ? ISS_ON : ISS_OFF;
}

int ScopeDome::setOutputState(ScopeDomeDigitalIO channel, ISState onOff)
{
    return writeU8(onOff == ISS_ON ? SetDigitalChannel : ClearDigitalChannel, (int)channel);
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

uint16_t ScopeDome::compensateInertia(uint16_t steps)
{
    if (inertiaTable.size() == 0)
    {
        LOGF_INFO("inertia passthrough %d", steps);
        return steps; // pass value as such if we don't have enough data
    }

    for (uint16_t out = 0; out < inertiaTable.size(); out++)
    {
        if (inertiaTable[out] > steps)
        {
            LOGF_INFO("inertia %d -> %d", steps, out - 1);
            return out - 1;
        }
    }
    // Check difference from largest table entry and assume we have
    // similar inertia also after that
    int lastEntry = inertiaTable.size() - 1;
    int inertia   = inertiaTable[lastEntry] - lastEntry;
    int movement  = (int)steps - inertia;
    LOGF_INFO("inertia %d -> %d", steps, movement);
    if (movement <= 0)
        return 0;

    return movement;
}
