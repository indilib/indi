/*
  Alluna TCS2 Focus, Dust Cover, Climate, Rotator, and Settings
  (Dust Cover and Rotator are not implemented)

  Copyright(c) 2022 Peter Englmaier. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "alluna_tcs2.h"

#include "indicom.h"

#include <cstring>
#include <termios.h>
#include <memory>
#include <thread>
#include <chrono>

// create an instance of this driver
static std::unique_ptr<AllunaTCS2> allunaTCS2(new AllunaTCS2 ());

AllunaTCS2::AllunaTCS2() //: DustCapInterface()
{
    LOG_DEBUG("Init AllunaTCS2");
    // Let's specify the driver version
    setVersion(1, 0);

    // we know only about serial (USB) connections
    setSupportedConnections(CONNECTION_SERIAL);
    // Connection parameters should be 19200 @ 8-N-1
    // FIXME: add some code to warn if settings are not ok

    // What capabilities do we support?
    FI::SetCapability(FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE ); //FIXME: maybe remove CAN_REL_MOVE
}


bool AllunaTCS2::initProperties()
{
    INDI::Focuser::initProperties();
    //INDI::DustCapInterface::initDustCapProperties(getDeviceName(), "groupname");

    // Focuser temperature / ambient temperature, ekos uses first number of "FOCUS_TEMPERATURE" property
    IUFillNumber(&TemperatureN[0], "TEMPERATURE_AMBIENT", "Focuser Temp [C]", "%6.2f", -100, 100, 0, 0);
    // Primary mirror temperature
    IUFillNumber(&TemperatureN[1], "TEMPERATURE_PRIMARY", "Primary Temp [C]", "%6.2f", -100, 100, 0, 0);
    // Secondary mirror temperature
    IUFillNumber(&TemperatureN[2], "TEMPERATURE_SECONDARY", "Secondary Temp [C]", "%6.2f", -100, 100, 0, 0);
    // Ambient humidity
    IUFillNumber(&TemperatureN[3], "HUMIDITY", "Humidity [%]", "%6.2f", 0, 100, 0, 0);

    IUFillNumberVector(&TemperatureNP, TemperatureN, 4, getDeviceName(), "FOCUS_TEMPERATURE", "Climate",
                       CLIMATE_TAB, IP_RO, 0, IPS_IDLE);

    // Climate control
    IUFillSwitch(&ClimateControlS[CLIMATECONTROL_AUTO],  "CLIMATE_AUTO",  "On", ISS_OFF);
    IUFillSwitch(&ClimateControlS[CLIMATECONTROL_MANUAL], "CLIMATE_MANUAL", "Off", ISS_ON);
    IUFillSwitchVector(&ClimateControlSP, ClimateControlS, 2, getDeviceName(), "CLIMATE_CONTROL", "Climate Control", CLIMATE_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&PrimaryDewHeaterS[DEWHEATER_ON],  "PRIMARY_HEATER_ON",  "On", ISS_OFF);
    IUFillSwitch(&PrimaryDewHeaterS[DEWHEATER_OFF], "PRIMARY_HEATER_OFF", "Off", ISS_ON);
    IUFillSwitchVector(&PrimaryDewHeaterSP, PrimaryDewHeaterS, 2, getDeviceName(), "PRIMARY_HEATER", "Heat primary", CLIMATE_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUFillSwitch(&SecondaryDewHeaterS[DEWHEATER_ON],  "SECONDARY_HEATER_ON",  "On", ISS_OFF);
    IUFillSwitch(&SecondaryDewHeaterS[DEWHEATER_OFF], "SECONDARY_HEATER_OFF", "Off", ISS_ON);
    IUFillSwitchVector(&SecondaryDewHeaterSP, SecondaryDewHeaterS, 2, getDeviceName(), "SECONDARY_HEATER", "Heat secondary", CLIMATE_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&FanPowerN[0], "FANPOWER", "Fan power [130..255]", "%3.0f", 130, 255, 1, 255);
    IUFillNumberVector(&FanPowerNP, FanPowerN, 1, getDeviceName(), "FANPOWER", "Fan Power",
                       CLIMATE_TAB, IP_RW, 60, IPS_IDLE);

    // Stepping Modes "SpeedStep" and "MicroStep"
    IUFillSwitch(&SteppingModeS[STEPPING_SPEED], "STEPPING_SPEED", "SpeedStep", ISS_ON);
    IUFillSwitch(&SteppingModeS[STEPPING_MICRO], "STEPPING_MICRO", "MicroStep", ISS_OFF);
    IUFillSwitchVector(&SteppingModeSP, SteppingModeS, 2, getDeviceName(), "STEPPING_MODE", "Mode",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);


    // Set limits as per documentation
    FocusAbsPosN[0].min  = 0;
    FocusAbsPosN[0].max  = (steppingMode == STEPPING_MICRO) ? 22400 : 1400; // 22400 in microstep mode, 1400 in speedstep mode
    FocusAbsPosN[0].step = 1;

    FocusRelPosN[0].min  = 0;
    FocusRelPosN[0].max  = 1000;
    FocusRelPosN[0].step = 1;

    // Maximum Position
    FocusMaxPosN[0].value = FocusAbsPosN[0].max;
    FocusMaxPosNP.p = IP_RO;

    // Dust Cover
    IUFillSwitch(&CoverS[COVER_OPEN], "COVER_OPEN", "Open", ISS_OFF);
    IUFillSwitch(&CoverS[COVER_CLOSED], "COVER_CLOSE", "Close", ISS_ON);
    IUFillSwitchVector(&CoverSP, CoverS, 2, getDeviceName(), "COVER_CONTROL", "Cover Control", DUSTCOVER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    setDriverInterface(FOCUSER_INTERFACE); //| DUSTCAP_INTERFACE);

    addAuxControls();

    return true;
}

const char *AllunaTCS2::getDefaultName()
{
    return "Alluna TCS2";
}

bool AllunaTCS2::updateProperties()
{
    LOG_INFO("updateProperties called");
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        // turn on green Connected-LED
        if (sendCommand("Connect 1\n"))
        {
            LOG_DEBUG("Turned on Connected-LED¨");
        }
        else
        {
            LOG_ERROR("Cannot turn on Connected-LED");
        }

        // Read these values before defining focuser interface properties
        // Only ask for values in sync, because TimerHit is not running, yet.
        getPosition();
        getStepping();
        getDustCover();
        getTemperature();
        getClimateControl();
        getFanPower();

        // Focuser
        defineProperty(&SteppingModeSP);
        defineProperty(&FocusMaxPosNP);
        defineProperty(&FocusAbsPosNP);

        // Climate
        defineProperty(&TemperatureNP);
        defineProperty(&ClimateControlSP);
        defineProperty(&PrimaryDewHeaterSP);
        defineProperty(&SecondaryDewHeaterSP);
        defineProperty(&FanPowerNP);

        // Cover
        defineProperty(&CoverSP);

        LOG_INFO("AllunaTCS2 is ready.");
    }
    else
    {
        deleteProperty(SteppingModeSP.name);
        deleteProperty(FocusMaxPosNP.name);
        deleteProperty(FocusAbsPosNP.name);

        deleteProperty(TemperatureNP.name);
        deleteProperty(ClimateControlSP.name);
        deleteProperty(PrimaryDewHeaterSP.name);
        deleteProperty(SecondaryDewHeaterSP.name);
        deleteProperty(FanPowerNP.name);

        deleteProperty(CoverSP.name);
    }

    return true;
}

bool AllunaTCS2::Handshake()
{
    char cmd[DRIVER_LEN] = "HandShake\n", res[DRIVER_LEN] = {0};

    tcs.unlock();
    bool rc = sendCommand(cmd, res, 0, 2);

    if (rc == false)
        return false;

    return res[0] == '\r' && res[1] == '\n';
}

bool AllunaTCS2::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{

   if (tcs.try_lock() ) {
     bool result;
     result = sendCommandNoLock(cmd, res, cmd_len, res_len);
     tcs.unlock();
     return result;
   } else {
     LOG_INFO("sendCommand: lock failed, abort");
     return false;
   }

}

bool AllunaTCS2::sendCommandNoLock(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    LOG_DEBUG("sendCommand: Send Command");
    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("Byte string '%s'", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("Char string '%s'", cmd);
        rc = tty_write_string(PortFD, cmd, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if (res_len > 0) {
        LOG_DEBUG("sendCommand: Read Answer Bytes");
        rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);
    } else {
        LOG_DEBUG("sendCommand: Read Answer String");
        rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("203 Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[DRIVER_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("Bytes '%s'", hex_res);
    }
    else
    {
        LOGF_DEBUG("String '%s'", res);
    }

    tcflush(PortFD, TCIOFLUSH);
    LOG_DEBUG("sendCommand: Ende");
    return true;
}


bool AllunaTCS2::sendCommandOnly(const char * cmd, int cmd_len)
{
    int nbytes_written = 0, rc = -1;

    if (! tcs.try_lock() ) {
        LOGF_INFO("sendCommandOnly: %s: lock failed, abort", cmd);
        return false;
    }
    LOG_DEBUG("sendCommandOnly: Anfang");
    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("Bytes '%s'", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("String '%s'", cmd);
        rc = tty_write_string(PortFD, cmd, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        tcs.unlock();
        return false;
    }

    LOG_DEBUG("sendCommandOnly: Ende");
    return true;
}

bool AllunaTCS2::receiveNext(char * res, int res_len)
{
    int nbytes_read = 0, rc = -1;
    LOG_DEBUG("receiveNext: Anfang");

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("285 Serial read error: %s.", errstr);
        tcs.unlock();
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[DRIVER_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("Bytes '%s'", hex_res);
    }
    else
    {
        LOGF_DEBUG("String '%s'", res);
    }
    LOG_DEBUG("receiveNext: Ende");

    return true;
}

void AllunaTCS2::receiveDone()
{
    LOG_DEBUG("receiveDone");
    tcflush(PortFD, TCIOFLUSH);
    tcs.unlock();
}

void AllunaTCS2::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

// client asks for list of all properties
void AllunaTCS2::ISGetProperties(const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);
    LOG_INFO("ISGetProperties called");
    // FIXME: do something like upclass does with controller class
}

// client wants to change switch value (i.e. click on switch in GUI)
bool AllunaTCS2::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()) )
    {        
        LOGF_INFO("ISNewSwitch called for %s", name);
        if (!strcmp(name, "CONNECTION") && !strcmp(names[0], "DISCONNECT") && states[0] == ISS_ON)
        {
            // turn off green Connected-LED
            if (sendCommand("Connect 0\n"))
                LOG_DEBUG("Turned off Connected-LED");
            else
                LOG_ERROR("Cannot turn off Connected-LED");
        }
        // Stepping Mode?
        if (!strcmp(name, SteppingModeSP.name))
        {
            IUUpdateSwitch(&SteppingModeSP, states, names, n);
            SteppingModeSP.s = IPS_OK;
            IDSetSwitch(&SteppingModeSP, nullptr);

            // write new stepping mode to tcs2
            setStepping((SteppingModeS[STEPPING_SPEED].s == ISS_ON) ? STEPPING_SPEED : STEPPING_MICRO);
            // update maximum stepping position
            FocusAbsPosN[0].max  = (steppingMode == STEPPING_MICRO) ? 22400 : 1400; // 22400 in microstep mode, 1400 in speedstep mode
            // update max position value
            FocusMaxPosN[0].value = FocusAbsPosN[0].max;
            // update maximum stepping postion for presets
            SetFocuserMaxPosition( FocusAbsPosN[0].max ); // 22400 in microstep mode, 1400 in speedstep mode
            // Update clients
            IDSetNumber(&FocusAbsPosNP, nullptr); // not sure if this is necessary, because not shown in driver panel
            IDSetNumber(&FocusMaxPosNP, nullptr );
            LOGF_INFO("Setting new max position to %d", (steppingMode == STEPPING_MICRO) ? 22400 : 1400 );

            defineProperty(&FocusMaxPosNP);
            defineProperty(&FocusAbsPosNP);
            // read focuser position (depends on stepping mode)
            getPosition();
            LOGF_INFO("Processed %s",name);
            return true;
        }

        // Cover Switch?
        if (!strcmp(name, CoverSP.name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // Do nothing, if state is already what it should be
            int currentCoverIndex = IUFindOnSwitchIndex(&CoverSP);
            if (!strcmp(actionName, CoverS[currentCoverIndex].name))
            {
               DEBUGF(INDI::Logger::DBG_SESSION, "Cover is already %s", CoverS[currentCoverIndex].label);
               CoverSP.s = IPS_IDLE;
               IDSetSwitch(&CoverSP, NULL);
               return true;
            }

            // Otherwise, let us update the switch state
            IUUpdateSwitch(&CoverSP, states, names, n);
            currentCoverIndex = IUFindOnSwitchIndex(&CoverSP);
            if ( setDustCover() ) {
                isCoverMoving = true;
                DEBUGF(INDI::Logger::DBG_SESSION, "Cover is now %s", CoverS[currentCoverIndex].label);
                CoverSP.s = IPS_OK;
                IDSetSwitch(&CoverSP, NULL);
                return true;
            } else {
                DEBUG(INDI::Logger::DBG_SESSION, "Cannot get lock, try again");
                CoverSP.s = IPS_ALERT;
                IDSetSwitch(&CoverSP, NULL);
            }
        }

        // Climate Control Switch?
        if (!strcmp(name, ClimateControlSP.name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // Do nothing, if state is already what it should be
            int currentClimateControlIndex = IUFindOnSwitchIndex(&ClimateControlSP);
            if (!strcmp(actionName, ClimateControlS[currentClimateControlIndex].name))
            {
               DEBUGF(INDI::Logger::DBG_SESSION, "Climate Control is already %s", ClimateControlS[currentClimateControlIndex].label);
               ClimateControlSP.s = IPS_IDLE;
               IDSetSwitch(&ClimateControlSP, NULL);
               return true;
            }

            // Otherwise, let us update the switch state
            IUUpdateSwitch(&ClimateControlSP, states, names, n);
            currentClimateControlIndex = IUFindOnSwitchIndex(&ClimateControlSP);
            if ( setClimateControl((currentClimateControlIndex==CLIMATECONTROL_AUTO) ? CLIMATECONTROL_MANUAL: CLIMATECONTROL_AUTO) ) {
                DEBUGF(INDI::Logger::DBG_SESSION, "ClimateControl is now %s", CoverS[currentClimateControlIndex].label);
                ClimateControlSP.s = IPS_OK;
                IDSetSwitch(&ClimateControlSP, NULL);
                return true;
            } else {
                DEBUG(INDI::Logger::DBG_SESSION, "Cannot get lock, try again");
                ClimateControlSP.s = IPS_ALERT;
                IDSetSwitch(&ClimateControlSP, NULL);
            }
        }

        // PrimaryDewHeater Switch?
        if (!strcmp(name, PrimaryDewHeaterSP.name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // Do nothing, if state is already what it should be
            int currentPrimaryDewHeaterIndex = IUFindOnSwitchIndex(&PrimaryDewHeaterSP);
            if (!strcmp(actionName, PrimaryDewHeaterS[currentPrimaryDewHeaterIndex].name))
            {
               DEBUGF(INDI::Logger::DBG_SESSION, "PrimaryDewHeater is already %s", PrimaryDewHeaterS[currentPrimaryDewHeaterIndex].label);
               PrimaryDewHeaterSP.s = IPS_IDLE;
               IDSetSwitch(&PrimaryDewHeaterSP, NULL);
               return true;
            }

            // Otherwise, let us update the switch state
            IUUpdateSwitch(&PrimaryDewHeaterSP, states, names, n);
            currentPrimaryDewHeaterIndex = IUFindOnSwitchIndex(&PrimaryDewHeaterSP);
            if ( setPrimaryDewHeater((currentPrimaryDewHeaterIndex==DEWHEATER_OFF) ? DEWHEATER_ON: DEWHEATER_OFF) ) {
                DEBUGF(INDI::Logger::DBG_SESSION, "PrimaryDewHeater is now %s", PrimaryDewHeaterS[currentPrimaryDewHeaterIndex].label);
                PrimaryDewHeaterSP.s = IPS_OK;
                IDSetSwitch(&PrimaryDewHeaterSP, NULL);
                return true;
            } else {
                DEBUG(INDI::Logger::DBG_SESSION, "Cannot get lock, try again");
                PrimaryDewHeaterSP.s = IPS_ALERT;
                IDSetSwitch(&PrimaryDewHeaterSP, NULL);
            }
        }

        // SecondaryDewHeater Switch?
        if (!strcmp(name, SecondaryDewHeaterSP.name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // Do nothing, if state is already what it should be
            int currentSecondaryDewHeaterIndex = IUFindOnSwitchIndex(&SecondaryDewHeaterSP);
            if (!strcmp(actionName, SecondaryDewHeaterS[currentSecondaryDewHeaterIndex].name))
            {
               DEBUGF(INDI::Logger::DBG_SESSION, "SecondaryDewHeater is already %s", SecondaryDewHeaterS[currentSecondaryDewHeaterIndex].label);
               SecondaryDewHeaterSP.s = IPS_IDLE;
               IDSetSwitch(&SecondaryDewHeaterSP, NULL);
               return true;
            }

            // Otherwise, let us update the switch state
            IUUpdateSwitch(&SecondaryDewHeaterSP, states, names, n);
            currentSecondaryDewHeaterIndex = IUFindOnSwitchIndex(&SecondaryDewHeaterSP);
            if ( setSecondaryDewHeater((currentSecondaryDewHeaterIndex==DEWHEATER_OFF) ? DEWHEATER_ON: DEWHEATER_OFF) ) {
                DEBUGF(INDI::Logger::DBG_SESSION, "SecondaryDewHeater is now %s", SecondaryDewHeaterS[currentSecondaryDewHeaterIndex].label);
                SecondaryDewHeaterSP.s = IPS_OK;
                IDSetSwitch(&SecondaryDewHeaterSP, NULL);
                return true;
            } else {
                DEBUG(INDI::Logger::DBG_SESSION, "Cannot get lock, try again");
                SecondaryDewHeaterSP.s = IPS_ALERT;
                IDSetSwitch(&SecondaryDewHeaterSP, NULL);
            }
        }



    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

// client wants to change number value
bool AllunaTCS2::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    LOGF_INFO("ISNewSwitch called for %s\n", name);
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Fan Power
        if (!strcmp(name, FanPowerNP.name))
        {
            // Try to update settings
            int power=values[0];
            if (power>255) power=255;
            if (power<0) power=0;
            if (setFanPower(power))
            {
                IUUpdateNumber(&FanPowerNP, values, names, n);
                FanPowerNP.s = IPS_OK;
            }
            else
            {
                FanPowerNP.s = IPS_ALERT;
            }

            IDSetNumber(&FanPowerNP, nullptr);
            return true;
        }

    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

IPState AllunaTCS2::MoveAbsFocuser(uint32_t targetTicks)
{
    LOGF_INFO("MoveAbsFocuser %d called", targetTicks);
    char cmd[DRIVER_LEN];
    snprintf(cmd, DRIVER_LEN, "FocuserGoTo %d\r\n", targetTicks);
    bool rc = sendCommandOnly(cmd);
    if (rc == false) {
        LOGF_ERROR("MoveAbsFocuser %d failed", targetTicks);
        return IPS_ALERT;
    }
    isFocuserMoving = true;

    return IPS_BUSY;
}

IPState AllunaTCS2::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    m_TargetDiff = ticks * ((dir == FOCUS_INWARD) ? -1 : 1);
    return MoveAbsFocuser(FocusAbsPosN[0].value + m_TargetDiff);
}

bool AllunaTCS2::AbortFocuser()
{
    return sendCommand("FocuserStop\n");
}

void AllunaTCS2::TimerHit()
{
    //LOG_INFO("TimerHit");
    if (!isConnected())
        return; // No need to reset timer if we are not connected anymore

    // try to read temperature, if it works no lock was present
    if (getTemperature() && getFanPower()) {
        SetTimer(getCurrentPollingPeriod());
        return;
    }
    // if we could not read temperature, a lock is set and we need to check if there is input to be processed.

    bool actionInProgress = isFocuserMoving || isCoverMoving;
    // expect and process device output while present
    char res[DRIVER_LEN] = {0};

    // read a line, if available
    while (actionInProgress && receiveNext(res))
    {
        int32_t pos;

         if ( res[1] == '#') {
            switch (res[0])
            {
                case 'A': // aux1 on (primary mirror heating)
                    LOG_INFO("Primary heater switched ON");
                    break;
                case 'B': // aux1 off (primary mirror heating)
                    LOG_INFO("Primary heater switched OFF");
                    break;
                case 'C': // aux2 on (secondary mirror heating)
                    LOG_INFO("Secondary heater switched ON");
                    break;
                case 'D': // aux2 off (secondary mirror heating)
                    LOG_INFO("Secondary heater switched OFF");
                    break;
                case 'E': // climate control ON
                    LOG_INFO("Climate Control switched ON");
                    break;
                case 'F': // climate control OFF
                    LOG_INFO("Climate Control switched OFF");
                    break;
                case 'G': // fan slider return value
                    break;
                case 'Q': // focuser home run start
                    break;
                case 'U': // back focus minimum for optic "None"
                    break;
                case 'V': // back focus maximum for optic "None"
                    break;
                case 'W': // back focus minimum for optic "Corrector"
                    break;
                case 'X': // back focus maximum for optic "Corrector"
                    break;
                case 'Y': // back focus minimum for optic "Reducer"
                    break;
                case 'Z': // back focus maximum for optic "Reducer"
                    break;
                case 'a': // ambient temperature correction value
                    break;
                case 'b': // primary temperature correction value
                    break;
                case 'c': // secondary temperature correction value
                    break;
                case 'K': // new focuser position
                    pos = 1e6;
                    sscanf(res, "K#%d", &pos);
                    //LOGF_INFO("TimerHit: new pos (%d)",pos);
                    if (pos != 1e6) {
                        FocusAbsPosN[0].value = pos;
                    }
                    FocusAbsPosNP.s = IPS_BUSY;
                    FocusRelPosNP.s = IPS_BUSY;
                    IDSetNumber(&FocusAbsPosNP, nullptr);
                    break;
                case 'I': // starting to focus
                    LOG_INFO("TimerHit: starting to focus");
                    break;
                case 'J': // end of focusing
                    LOG_INFO("TimetHit: end of focusing");
                    isFocuserMoving = false;
                    FocusAbsPosNP.s = IPS_OK;
                    IDSetNumber(&FocusAbsPosNP, nullptr);
                    receiveDone();
                    break;
                case 'O': // cover started moving
                    LOG_INFO("TimerHit: cover started moving");
                    CoverSP.s = IPS_BUSY;
                    IDSetSwitch(&CoverSP, nullptr);
                    break;
                case 'H': // cover stopped moving
                    LOG_INFO("TimetHit: cover stopped moving");
                    isCoverMoving = false;
                    receiveDone();
                    CoverSP.s = IPS_OK;
                    IDSetSwitch(&CoverSP, nullptr);
                    break;
                default: // unexpected output
                    LOGF_INFO("TimerHit: unexpected response (%s)", res);
           }
        } else {
          LOGF_INFO("TimerHit: unexpected response (%s)", res);
        }
        actionInProgress = isFocuserMoving || isCoverMoving;
    }

    // What is the last read position?
    double currentPosition = FocusAbsPosN[0].value;

    // Check if we have a pending motion
    // if isMoving() is false, then we stopped, so we need to set the Focus Absolute
    // and relative properties to OK
    if ( (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY) )
    {
        FocusAbsPosNP.s = IPS_OK;
        FocusRelPosNP.s = IPS_OK;
        IDSetNumber(&FocusAbsPosNP, nullptr);
        IDSetNumber(&FocusRelPosNP, nullptr);
    }
    // If there was a different between last and current positions, let's update all clients
    else if (currentPosition != FocusAbsPosN[0].value)
    {
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    SetTimer(getCurrentPollingPeriod());
}

bool AllunaTCS2::isMoving()
{
    return isFocuserMoving;
}

bool AllunaTCS2::getTemperature()
{
    // timestamp, when we updated temperatur
    static std::chrono::system_clock::time_point last_temp_update = std::chrono::system_clock::now();
    static bool first_run = true;

    // the command GetTemperatures will respond with 4 lines:
    // R#{ambient_temperature}<CR><LF>
    // S#{primary_mirror_tempertature}<CR><LF>
    // T#{secondary_mirror_temperature}<CR><LF>
    // d#{ambient-humidity}<CR><LF>

    std::chrono::duration<double> seconds = std::chrono::system_clock::now() - last_temp_update;
    if ( !first_run && seconds.count() < 300 ) // update every 300 seconds
    {
        if (tcs.try_lock()) {
            tcs.unlock(); // we need to get lock, to make TimerHit behave the same when we block reading temperature
            return true; // return true, if we could get the lock
        } else {
            return false; // return false, if we could not get the lock
        }
    }
    else if ( sendCommandOnly("GetTemperatures\n")  )
    {
      TemperatureNP.s = IPS_BUSY;
      isGetTemperature = true;

      // expect and process device output while present
      char res[DRIVER_LEN] = {0};
      float value;

      // read a line, if available
      while (isGetTemperature && receiveNext(res))
      {
         switch (res[0])
         {
            case 'R': // ambient temperature value
                sscanf(res, "R#%f", &value);
                TemperatureN[0].value = value;
                break;
            case 'S': // primary mirror temperature value
                sscanf(res, "S#%f", &value);
                TemperatureN[1].value = value;
                break;
            case 'T': // secondary mirror temperature value
                sscanf(res, "T#%f", &value);
                TemperatureN[2].value = value;
                break;
            case 'd': // ambient humidity value
                sscanf(res, "d#%f", &value);
                TemperatureN[3].value = value;
                receiveDone();
                isGetTemperature=false;
                TemperatureNP.s = IPS_OK;
                IDSetNumber(&TemperatureNP, nullptr);
                break;
            default: // unexpected output
                LOGF_ERROR("GetTemperatures: unexpected response (%s)", res);
          }
      }
      first_run = false;
      last_temp_update = std::chrono::system_clock::now();
      return true;
    }
    return false;
}

bool AllunaTCS2::getPosition()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand("GetFocuserPosition\n", res, 0) == false)
        return false;

    int32_t pos = 1e6;
    sscanf(res, "%d", &pos);

    if (pos == 1e6)
        return false;

    FocusAbsPosN[0].value = pos;

    FocusAbsPosNP.s = IPS_OK;
    IDSetNumber(&FocusAbsPosNP, nullptr); // display in user interface

    return true;
}

bool AllunaTCS2::getDustCover()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand("GetDustCover\n", res, 0) == false)
        return false;

    int32_t value = -1;
    sscanf(res, "%d", &value);

    if (value == -1)
        return false;

    DEBUGF(INDI::Logger::DBG_SESSION, "Cover status read to be %s (%d)", (value==1)?"open":"closed", value);
    CoverS[COVER_OPEN  ].s = (value==1)?ISS_ON:ISS_OFF;
    CoverS[COVER_CLOSED].s = (value!=1)?ISS_ON:ISS_OFF;
    CoverSP.s = IPS_OK;

    return true;
}

bool AllunaTCS2::getStepping()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand("GetFocuserMode\n", res) == false)
        return false;

    int32_t mode = 1e6;
    sscanf(res, "%d", &mode);

    if (mode == 1e6)
        return false;

    // mode=1: Microstep, mode=0: Speedstep
    steppingMode = (mode == 0) ? STEPPING_SPEED : STEPPING_MICRO;
    SteppingModeS[STEPPING_SPEED].s = (mode == 0) ? ISS_ON : ISS_OFF;
    SteppingModeS[STEPPING_MICRO].s = (mode == 0) ? ISS_OFF : ISS_ON;
    SteppingModeSP.s = IPS_OK;

    // Set limits as per documentation
    FocusAbsPosN[0].max  = (steppingMode == STEPPING_MICRO) ? 22400 : 1400; // 22400 in microstep mode, 1400 in speedstep mode
    LOGF_INFO("readStepping: set max position to %d",(int)FocusAbsPosN[0].max);
    return true;
}

bool AllunaTCS2::setStepping(SteppingMode mode)
{
    int value;
    char cmd[DRIVER_LEN] = {0};
    steppingMode=mode;
    value = (mode == STEPPING_SPEED) ? 0 : 1;
    LOGF_INFO("Setting stepping mde to: %s", (mode==STEPPING_SPEED)?"SPEED":"micro");
    LOGF_INFO("Setting stepping mode to: %d", value);
    snprintf(cmd, DRIVER_LEN, "SetFocuserMode %d\n", value);
    return sendCommand(cmd);
}

bool AllunaTCS2::setDustCover()
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "SetDustCover\n"); // opens/closes dust cover (state toggle)
    return sendCommandOnly(cmd); // response is processed in TimerHit
}

bool AllunaTCS2::getClimateControl()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand("GetClimateControl\n", res, 0) == false)
        return false;

    int32_t value = -1;
    sscanf(res, "%d", &value);

    if (value == -1)
        return false;

    DEBUGF(INDI::Logger::DBG_SESSION, "Climate Control status read to be %s (%d)", (value==1)?"automatic":"manual", value);
    ClimateControlS[CLIMATECONTROL_AUTO  ].s = (value==1)?ISS_ON:ISS_OFF;
    ClimateControlS[CLIMATECONTROL_MANUAL].s = (value!=1)?ISS_ON:ISS_OFF;
    ClimateControlSP.s = IPS_OK;

    return true;
}

bool AllunaTCS2::setClimateControl(ClimateControlMode mode)
{
    char cmd[DRIVER_LEN] = {0};
    int value;
    value = (mode == CLIMATECONTROL_AUTO) ? 1 : 0;
    snprintf(cmd, DRIVER_LEN, "SetClimateControl %d\n", value); // enable/disable climate control
    return sendCommand(cmd);
}

bool AllunaTCS2::getPrimaryDewHeater()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand("GetAux1\n", res, 0) == false)
        return false;

    int32_t value = -1;
    sscanf(res, "%d", &value);

    if (value == -1)
        return false;

    DEBUGF(INDI::Logger::DBG_SESSION, "PrimaryDewHeater status read to be %s (%d)", (value==1)?"ON":"OFF", value);
    PrimaryDewHeaterS[DEWHEATER_ON ].s = (value==1)?ISS_ON:ISS_OFF;
    PrimaryDewHeaterS[DEWHEATER_OFF].s = (value!=1)?ISS_ON:ISS_OFF;
    PrimaryDewHeaterSP.s = IPS_OK;

    return true;
}

bool AllunaTCS2::setPrimaryDewHeater(DewHeaterMode mode)
{
    char cmd[DRIVER_LEN] = {0};
    int value;
    value = (mode == DEWHEATER_ON) ? 1 : 0;
    snprintf(cmd, DRIVER_LEN, "SetAux1 %d\n", value); // enable/disable heating
    return sendCommand(cmd);
}

bool AllunaTCS2::getSecondaryDewHeater()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand("GetAux2\n", res, 0) == false)
        return false;

    int32_t value = -1;
    sscanf(res, "%d", &value);

    if (value == -1)
        return false;

    DEBUGF(INDI::Logger::DBG_SESSION, "SecondaryDewHeater status read to be %s (%d)", (value==1)?"ON":"OFF", value);
    SecondaryDewHeaterS[DEWHEATER_ON ].s = (value==1)?ISS_ON:ISS_OFF;
    SecondaryDewHeaterS[DEWHEATER_OFF].s = (value!=1)?ISS_ON:ISS_OFF;
    SecondaryDewHeaterSP.s = IPS_OK;

    return true;
}

bool AllunaTCS2::setSecondaryDewHeater(DewHeaterMode mode)
{
    char cmd[DRIVER_LEN] = {0};
    int value;
    value = (mode == DEWHEATER_ON) ? 1 : 0;
    snprintf(cmd, DRIVER_LEN, "SetAux2 %d\n", value); // enable/disable heating
    return sendCommand(cmd);
}

bool AllunaTCS2::getFanPower()
{
    // timestamp, when we updated temperatur
    static std::chrono::system_clock::time_point last_temp_update = std::chrono::system_clock::now();
    static bool first_run = true;

    char res[DRIVER_LEN] = {0};

    std::chrono::duration<double> seconds = std::chrono::system_clock::now() - last_temp_update;
    if ( !first_run && seconds.count() < 3 ) // update every 3 seconds
    {
        if (tcs.try_lock()) {
            tcs.unlock(); // we need to get lock, to make TimerHit behave the same when we block reading temperature
            return true; // return true, if we could get the lock
        } else {
            return false; // return false, if we could not get the lock
        }
    }
    if (sendCommand("GetFanPower\n", res, 0) == false)
        return false;

    int32_t value = -1;
    sscanf(res, "%d", &value);

    if (value == -1)
        return false;

    if (value != (int)FanPowerN[0].value) {
      LOGF_INFO("FanPower read to be %d", value);
      FanPowerN[0].value = (double)value;
      FanPowerNP.s = IPS_OK;
      IDSetNumber(&FanPowerNP, nullptr);
    }
    return true;
}

bool AllunaTCS2::setFanPower(int value)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "SetFanPower %d\n", value); // enable/disable heating
    return sendCommand(cmd);
}


bool AllunaTCS2::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);

    // We need to reserve and save stepping mode
    // so that the next time the driver is loaded, it is remembered and applied.
    //IUSaveConfigSwitch(fp, &SteppingModeSP);  -- not needed, because tcs2 stores state internally

    return true;
}

