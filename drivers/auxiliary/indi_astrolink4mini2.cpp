/*******************************************************************************
 Copyright(c) 2022 astrojolo.com
 .
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
#include "indi_astrolink4mini2.h"

#include "indicom.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 2

#define ASTROLINK4_LEN 200
#define ASTROLINK4_TIMEOUT 3

#define POLLTIME 500

//////////////////////////////////////////////////////////////////////
/// Delegates
//////////////////////////////////////////////////////////////////////
std::unique_ptr<IndiAstroLink4mini2> indiFocuserLink(new IndiAstroLink4mini2());

//////////////////////////////////////////////////////////////////////
///Constructor
//////////////////////////////////////////////////////////////////////
IndiAstroLink4mini2::IndiAstroLink4mini2() : FI(this), WI(this)
{
    setVersion(VERSION_MAJOR, VERSION_MINOR);
}

const char *IndiAstroLink4mini2::getDefaultName()
{
    return "AstroLink 4 mini II";
}

//////////////////////////////////////////////////////////////////////
/// Communication
//////////////////////////////////////////////////////////////////////
bool IndiAstroLink4mini2::Handshake()
{
    PortFD = serialConnection->getPortFD();

    char res[ASTROLINK4_LEN] = {0};
    if (sendCommand("#", res))
    {
        if (strncmp(res, "#:AstroLink4mini", 16) != 0)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Device not recognized.");
            return false;
        }
        else
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Handshake success");
            SetTimer(POLLTIME);
            return true;
        }
    }
    return false;
}

void IndiAstroLink4mini2::TimerHit()
{
    if (isConnected())
    {
        readDevice();
        SetTimer(POLLTIME);
    }
}

//////////////////////////////////////////////////////////////////////
/// Overrides
//////////////////////////////////////////////////////////////////////
bool IndiAstroLink4mini2::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | FOCUSER_INTERFACE);

    char focuserSelectLabel[15];
    memset(focuserSelectLabel, 0, 15);
    setFindex(IUGetConfigOnSwitchName(getDeviceName(), FocuserSelectSP.name, focuserSelectLabel, 15) == 0 ? 0 : 1);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_REVERSE |
                      FOCUSER_CAN_SYNC |
                      FOCUSER_CAN_ABORT);
    //FOCUSER_HAS_BACKLASH);

    FI::initProperties(FOCUS_TAB);
    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

    addDebugControl();
    addSimulationControl();
    addConfigurationControl();

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    serialConnection->setDefaultPort("/dev/ttyUSB0");
    serialConnection->setDefaultBaudRate(serialConnection->B_38400);

    IUFillSwitch(&FocuserSelectS[0], "FOC_SEL_1", "Focuser 1", (getFindex() == 0 ? ISS_ON : ISS_OFF));
    IUFillSwitch(&FocuserSelectS[1], "FOC_SEL_2", "Focuser 2", (getFindex() > 0 ? ISS_ON : ISS_OFF));
    IUFillSwitchVector(&FocuserSelectSP, FocuserSelectS, 2, getDeviceName(), "FOCUSER_SELECT", "Focuser select", FOCUS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Power readings
    IUFillNumber(&PowerDataN[POW_VIN], "VIN", "Input voltage [V]", "%.1f", 0, 15, 10, 0);
    IUFillNumber(&PowerDataN[POW_REG], "REG", "Regulated voltage [V]", "%.1f", 0, 15, 10, 0);
    IUFillNumber(&PowerDataN[POW_ITOT], "ITOT", "Total current [A]", "%.1f", 0, 15, 10, 0);
    IUFillNumber(&PowerDataN[POW_AH], "AH", "Energy consumed [Ah]", "%.1f", 0, 1000, 10, 0);
    IUFillNumber(&PowerDataN[POW_WH], "WH", "Energy consumed [Wh]", "%.1f", 0, 10000, 10, 0);
    IUFillNumberVector(&PowerDataNP, PowerDataN, 4, getDeviceName(), "POWER_DATA", "Power data", POWER_TAB, IP_RO, 60, IPS_IDLE);

    // Power lines
    IUFillSwitch(&Power1S[0], "PWR1BTN_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power1S[1], "PWR1BTN_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&Power1SP, Power1S, 2, getDeviceName(), "DC1", "Port 1", POWER_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&Power2S[0], "PWR2BTN_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power2S[1], "PWR2BTN_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&Power2SP, Power2S, 2, getDeviceName(), "DC2", "Port 2", POWER_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&Power3S[0], "PWR3BTN_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power3S[1], "PWR3BTN_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&Power3SP, Power3S, 2, getDeviceName(), "DC3", "Port 3", POWER_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&PWMN[0], "PWM1_VAL", "A", "%3.0f", 0, 100, 10, 0);
    IUFillNumber(&PWMN[1], "PWM2_VAL", "B", "%3.0f", 0, 100, 10, 0);
    IUFillNumberVector(&PWMNP, PWMN, 2, getDeviceName(), "PWM", "PWM", POWER_TAB, IP_RW, 60, IPS_IDLE);

    IUFillSwitch(&PowerDefaultOnS[0], "POW_DEF_ON1", "DC1", ISS_OFF);
    IUFillSwitch(&PowerDefaultOnS[1], "POW_DEF_ON2", "DC2", ISS_OFF);
    IUFillSwitch(&PowerDefaultOnS[2], "POW_DEF_ON3", "DC3", ISS_OFF);
    IUFillSwitchVector(&PowerDefaultOnSP, PowerDefaultOnS, 3, getDeviceName(), "POW_DEF_ON", "Power default ON", POWER_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // focuser settings
    IUFillNumber(&Focuser1SettingsN[FS1_SPEED], "FS1_SPEED", "Speed [pps]", "%.0f", 10, 200, 1, 100);
    IUFillNumber(&Focuser1SettingsN[FS1_CURRENT], "FS1_CURRENT", "Current [mA]", "%.0f", 100, 2000, 100, 400);
    IUFillNumber(&Focuser1SettingsN[FS1_HOLD], "FS1_HOLD", "Hold torque [%]", "%.0f", 0, 100, 10, 0);
    IUFillNumber(&Focuser1SettingsN[FS1_STEP_SIZE], "FS1_STEP_SIZE", "Step size [um]", "%.2f", 0, 100, 0.1, 5.0);
    IUFillNumber(&Focuser1SettingsN[FS1_COMPENSATION], "FS1_COMPENSATION", "Compensation [steps/C]", "%.2f", -1000, 1000, 1, 0);
    IUFillNumber(&Focuser1SettingsN[FS1_COMP_THRESHOLD], "FS1_COMP_THRESHOLD", "Compensation threshold [steps]", "%.0f", 1, 1000, 10, 10);
    IUFillNumberVector(&Focuser1SettingsNP, Focuser1SettingsN, 6, getDeviceName(), "FOCUSER1_SETTINGS", "Focuser 1 settings", FOC1_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&Focuser2SettingsN[FS2_SPEED], "FS2_SPEED", "Speed [pps]", "%.0f", 10, 200, 1, 100);
    IUFillNumber(&Focuser2SettingsN[FS2_CURRENT], "FS2_CURRENT", "Current [mA]", "%.0f", 100, 2000, 100, 400);
    IUFillNumber(&Focuser2SettingsN[FS2_HOLD], "FS2_HOLD", "Hold torque [%]", "%.0f", 0, 100, 10, 0);
    IUFillNumber(&Focuser2SettingsN[FS2_STEP_SIZE], "FS2_STEP_SIZE", "Step size [um]", "%.2f", 0, 100, 0.1, 5.0);
    IUFillNumber(&Focuser2SettingsN[FS2_COMPENSATION], "FS2_COMPENSATION", "Compensation [steps/C]", "%.2f", -1000, 1000, 1, 0);
    IUFillNumber(&Focuser2SettingsN[FS2_COMP_THRESHOLD], "FS2_COMP_THRESHOLD", "Compensation threshold [steps]", "%.0f", 1, 1000, 10, 10);
    IUFillNumberVector(&Focuser2SettingsNP, Focuser2SettingsN, 6, getDeviceName(), "FOCUSER2_SETTINGS", "Focuser 2 settings", FOC2_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    IUFillSwitch(&Focuser1ModeS[FS1_MODE_UNI], "FS1_MODE_UNI", "Unipolar", ISS_ON);
    IUFillSwitch(&Focuser1ModeS[FS1_MODE_MICRO_L], "FS1_MODE_MICRO_L", "Microstep 1/8", ISS_OFF);
    IUFillSwitch(&Focuser1ModeS[FS1_MODE_MICRO_H], "FS1_MODE_MICRO_H", "Microstep 1/32", ISS_OFF);
    IUFillSwitchVector(&Focuser1ModeSP, Focuser1ModeS, 3, getDeviceName(), "FOCUSER1_MODE", "Focuser mode", FOC1_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&Focuser2ModeS[FS2_MODE_UNI], "FS2_MODE_UNI", "Unipolar", ISS_ON);
    IUFillSwitch(&Focuser2ModeS[FS2_MODE_MICRO_L], "FS2_MODE_MICRO_L", "Microstep 1/8", ISS_OFF);
    IUFillSwitch(&Focuser2ModeS[FS2_MODE_MICRO_H], "FS2_MODE_MICRO_H", "Microstep 1/32", ISS_OFF);
    IUFillSwitchVector(&Focuser2ModeSP, Focuser2ModeS, 3, getDeviceName(), "FOCUSER2_MODE", "Focuser mode", FOC2_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Environment Group
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -15, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15);

    return true;
}

bool IndiAstroLink4mini2::updateProperties()
{
    // Call parent update properties first
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        FI::updateProperties();
        WI::updateProperties();
        defineProperty(&FocuserSelectSP);
        defineProperty(&Focuser1SettingsNP);
        defineProperty(&Focuser2SettingsNP);
        defineProperty(&Focuser1ModeSP);
        defineProperty(&Focuser2ModeSP);
        defineProperty(&PowerDataNP);
        defineProperty(&Power1SP);
        defineProperty(&Power2SP);
        defineProperty(&Power3SP);
        defineProperty(&PWMNP);
        defineProperty(&PowerDefaultOnSP);
    }
    else
    {
        deleteProperty(PowerDataNP.name);
        deleteProperty(Focuser1SettingsNP.name);
        deleteProperty(Focuser2SettingsNP.name);
        deleteProperty(Focuser1ModeSP.name);
        deleteProperty(Focuser2ModeSP.name);
        deleteProperty(FocuserSelectSP.name);
        deleteProperty(Power1SP.name);
        deleteProperty(Power2SP.name);
        deleteProperty(Power3SP.name);
        deleteProperty(PWMNP.name);
        deleteProperty(PowerDefaultOnSP.name);
        WI::updateProperties();
        FI::updateProperties();
    }

    return true;
}

bool IndiAstroLink4mini2::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        char cmd[ASTROLINK4_LEN] = {0};
        char res[ASTROLINK4_LEN] = {0};

        // Handle PWM
        if (!strcmp(name, PWMNP.name))
        {
            bool allOk = true;
            if (PWMN[0].value != values[0])
            {
                sprintf(cmd, "B:0:%d", static_cast<uint8_t>(values[0]));
                allOk = allOk && sendCommand(cmd, res);
            }
            if (PWMN[1].value != values[1])
            {
                sprintf(cmd, "B:1:%d", static_cast<uint8_t>(values[1]));
                allOk = allOk && sendCommand(cmd, res);
            }
            PWMNP.s = (allOk) ? IPS_BUSY : IPS_ALERT;
            if (allOk)
                IUUpdateNumber(&PWMNP, values, names, n);
            IDSetNumber(&PWMNP, nullptr);
            return true;
        }

        // Focuser settings
        if (!strcmp(name, Focuser1SettingsNP.name))
        {
            bool allOk = true;
            std::map<int, std::string> updates;
            updates[U_FOC1_STEP] = doubleToStr(values[FS1_STEP_SIZE] * 100.0);
            updates[U_FOC1_COMPSTEPS] = doubleToStr(values[FS1_COMPENSATION] * 100.0);
            updates[U_FOC1_COMPTRIGGER] = doubleToStr(values[FS1_COMP_THRESHOLD]);
            updates[U_FOC1_SPEED] = intToStr(values[FS1_SPEED]);
            updates[U_FOC1_ACC] = intToStr(values[FS1_SPEED] * 5.0);
            updates[U_FOC1_CUR] = intToStr(values[FS1_CURRENT] / 10.0);
            updates[U_FOC1_HOLD] = intToStr(values[FS1_HOLD]);
            allOk = allOk && updateSettings("u", "U", updates);
            updates.clear();
            if (allOk)
            {
                Focuser1SettingsNP.s = IPS_BUSY;
                IUUpdateNumber(&Focuser1SettingsNP, values, names, n);
                IDSetNumber(&Focuser1SettingsNP, nullptr);
                DEBUGF(INDI::Logger::DBG_SESSION, "Focuser 1 temperature compensation is %s", (values[FS1_COMPENSATION] > 0) ? "enabled" : "disabled");
                return true;
            }
            Focuser1SettingsNP.s = IPS_ALERT;
            return true;
        }

        if (!strcmp(name, Focuser2SettingsNP.name))
        {
            bool allOk = true;
            std::map<int, std::string> updates;
            updates[U_FOC2_STEP] = doubleToStr(values[FS2_STEP_SIZE] * 100.0);
            updates[U_FOC2_COMPSTEPS] = doubleToStr(values[FS2_COMPENSATION] * 100.0);
            updates[U_FOC2_COMPTRIGGER] = doubleToStr(values[FS2_COMP_THRESHOLD]);
            updates[U_FOC2_SPEED] = intToStr(values[FS2_SPEED]);
            updates[U_FOC2_ACC] = intToStr(values[FS2_SPEED] * 5.0);
            updates[U_FOC2_CUR] = intToStr(values[FS2_CURRENT] / 10.0);
            updates[U_FOC2_HOLD] = intToStr(values[FS2_HOLD]);
            allOk = allOk && updateSettings("u", "U", updates);
            updates.clear();
            if (allOk)
            {
                Focuser2SettingsNP.s = IPS_BUSY;
                IUUpdateNumber(&Focuser2SettingsNP, values, names, n);
                IDSetNumber(&Focuser2SettingsNP, nullptr);
                DEBUGF(INDI::Logger::DBG_SESSION, "Focuser 2 temperature compensation is %s", (values[FS2_COMPENSATION] > 0) ? "enabled" : "disabled");
                return true;
            }
            Focuser2SettingsNP.s = IPS_ALERT;
            return true;
        }

        if (strstr(name, "FOCUS"))
            return FI::processNumber(dev, name, values, names, n);
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool IndiAstroLink4mini2::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        char cmd[ASTROLINK4_LEN] = {0};
        char res[ASTROLINK4_LEN] = {0};

        // handle power line 1
        if (!strcmp(name, Power1SP.name))
        {
            sprintf(cmd, "C:0:%s", (strcmp(Power1S[0].name, names[0])) ? "0" : "1");
            bool allOk = sendCommand(cmd, res);
            Power1SP.s = allOk ? IPS_BUSY : IPS_ALERT;
            if (allOk)
                IUUpdateSwitch(&Power1SP, states, names, n);

            IDSetSwitch(&Power1SP, nullptr);
            return true;
        }

        // handle power line 2
        if (!strcmp(name, Power2SP.name))
        {
            sprintf(cmd, "C:1:%s", (strcmp(Power2S[0].name, names[0])) ? "0" : "1");
            bool allOk = sendCommand(cmd, res);
            Power2SP.s = allOk ? IPS_BUSY : IPS_ALERT;
            if (allOk)
                IUUpdateSwitch(&Power2SP, states, names, n);

            IDSetSwitch(&Power2SP, nullptr);
            return true;
        }

        // handle power line 3
        if (!strcmp(name, Power3SP.name))
        {
            sprintf(cmd, "C:2:%s", (strcmp(Power3S[0].name, names[0])) ? "0" : "1");
            bool allOk = sendCommand(cmd, res);
            Power3SP.s = allOk ? IPS_BUSY : IPS_ALERT;
            if (allOk)
                IUUpdateSwitch(&Power3SP, states, names, n);

            IDSetSwitch(&Power3SP, nullptr);
            return true;
        }

        // Power default on
        if (!strcmp(name, PowerDefaultOnSP.name))
        {
            std::map<int, std::string> updates;
            updates[U_OUT1_DEF] = (states[0] == ISS_ON) ? "1" : "0";
            updates[U_OUT2_DEF] = (states[1] == ISS_ON) ? "1" : "0";
            updates[U_OUT3_DEF] = (states[2] == ISS_ON) ? "1" : "0";
            if (updateSettings("u", "U", updates))
            {
                PowerDefaultOnSP.s = IPS_BUSY;
                IUUpdateSwitch(&PowerDefaultOnSP, states, names, n);
                IDSetSwitch(&PowerDefaultOnSP, nullptr);
                return true;
            }
            PowerDefaultOnSP.s = IPS_ALERT;
            return true;
        }

        // Focuser Mode
        if (!strcmp(name, Focuser1ModeSP.name))
        {
            std::string value = "0";
            if (!strcmp(Focuser1ModeS[FS1_MODE_UNI].name, names[0]))
                value = "0";
            if (!strcmp(Focuser1ModeS[FS1_MODE_MICRO_L].name, names[0]))
                value = "1";
            if (!strcmp(Focuser1ModeS[FS1_MODE_MICRO_H].name, names[0]))
                value = "2";
            if (updateSettings("u", "U", U_FOC1_MODE, value.c_str()))
            {
                Focuser1ModeSP.s = IPS_BUSY;
                IUUpdateSwitch(&Focuser1ModeSP, states, names, n);
                IDSetSwitch(&Focuser1ModeSP, nullptr);
                return true;
            }
            Focuser1ModeSP.s = IPS_ALERT;
            return true;
        }
        if (!strcmp(name, Focuser2ModeSP.name))
        {
            std::string value = "0";
            if (!strcmp(Focuser2ModeS[FS2_MODE_UNI].name, names[0]))
                value = "0";
            if (!strcmp(Focuser2ModeS[FS2_MODE_MICRO_L].name, names[0]))
                value = "1";
            if (!strcmp(Focuser2ModeS[FS2_MODE_MICRO_H].name, names[0]))
                value = "2";
            if (updateSettings("u", "U", U_FOC2_MODE, value.c_str()))
            {
                Focuser2ModeSP.s = IPS_BUSY;
                IUUpdateSwitch(&Focuser2ModeSP, states, names, n);
                IDSetSwitch(&Focuser2ModeSP, nullptr);
                return true;
            }
            Focuser2ModeSP.s = IPS_ALERT;
            return true;
        }

        // Stepper select
        if (!strcmp(name, FocuserSelectSP.name))
        {
            setFindex((strcmp(FocuserSelectS[0].name, names[0])) ? 1 : 0);
            DEBUGF(INDI::Logger::DBG_DEBUG, "Focuser index set by switch to %i", getFindex());
            FocuserSelectSP.s = FocusMaxPosNP.s = FocusReverseSP.s = FocusAbsPosNP.s = IPS_BUSY;
            IUUpdateSwitch(&FocuserSelectSP, states, names, n);
            IDSetSwitch(&FocuserSelectSP, nullptr);
            IDSetSwitch(&FocusReverseSP, nullptr);
            IDSetNumber(&FocusMaxPosNP, nullptr);
            IDSetNumber(&FocusAbsPosNP, nullptr);

            return true;
        }

        if (strstr(name, "FOCUS"))
            return FI::processSwitch(dev, name, states, names, n);
    }
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool IndiAstroLink4mini2::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool IndiAstroLink4mini2::saveConfigItems(FILE *fp)
{
    IUSaveConfigSwitch(fp, &FocuserSelectSP);
    FI::saveConfigItems(fp);
    INDI::DefaultDevice::saveConfigItems(fp);
    return true;
}

//////////////////////////////////////////////////////////////////////
/// Focuser interface
//////////////////////////////////////////////////////////////////////
IPState IndiAstroLink4mini2::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[ASTROLINK4_LEN] = {0}, res[ASTROLINK4_LEN] = {0};
    snprintf(cmd, ASTROLINK4_LEN, "R:%i:%u", getFindex(), targetTicks);
    return (sendCommand(cmd, res)) ? IPS_BUSY : IPS_ALERT;
}

IPState IndiAstroLink4mini2::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    return MoveAbsFocuser(dir == FOCUS_INWARD ? FocusAbsPosN[0].value - ticks : FocusAbsPosN[0].value + ticks);
}

bool IndiAstroLink4mini2::AbortFocuser()
{
    char cmd[ASTROLINK4_LEN] = {0}, res[ASTROLINK4_LEN] = {0};
    snprintf(cmd, ASTROLINK4_LEN, "H:%i", getFindex());
    return (sendCommand(cmd, res));
}

bool IndiAstroLink4mini2::ReverseFocuser(bool enabled)
{
    int index = getFindex() > 0 ? U_FOC2_REV : U_FOC1_REV;
    if (updateSettings("u", "U", index, (enabled) ? "1" : "0"))
    {
        FocusReverseSP.s = IPS_BUSY;
        return true;
    }
    else
    {
        return false;
    }
}

bool IndiAstroLink4mini2::SyncFocuser(uint32_t ticks)
{
    char cmd[ASTROLINK4_LEN] = {0}, res[ASTROLINK4_LEN] = {0};
    snprintf(cmd, ASTROLINK4_LEN, "P:%i:%u", getFindex(), ticks);
    if (sendCommand(cmd, res))
    {
        FocusAbsPosNP.s = IPS_BUSY;
        return true;
    }
    else
    {
        return false;
    }
}

bool IndiAstroLink4mini2::SetFocuserMaxPosition(uint32_t ticks)
{
    int index = getFindex() > 0 ? U_FOC2_MAX : U_FOC1_MAX;
    if (updateSettings("u", "U", index, std::to_string(ticks).c_str()))
    {
        FocusMaxPosNP.s = IPS_BUSY;
        return true;
    }
    else
    {
        return false;
    }
}

bool IndiAstroLink4mini2::SetFocuserBacklash(int32_t steps)
{
    INDI_UNUSED(steps);
    return true;
}

bool IndiAstroLink4mini2::SetFocuserBacklashEnabled(bool enabled)
{
    INDI_UNUSED(enabled);
    return true;
}

//////////////////////////////////////////////////////////////////////
/// Serial commands
//////////////////////////////////////////////////////////////////////
bool IndiAstroLink4mini2::sendCommand(const char *cmd, char *res)
{
    int nbytes_read = 0, nbytes_written = 0, tty_rc = 0;
    char command[ASTROLINK4_LEN];

    if (isSimulation())
    {
        if (strncmp(cmd, "#", 1) == 0)
            sprintf(res, "%s\n", "#:AstroLink4mini");
        if (strncmp(cmd, "q", 1) == 0)
            sprintf(res, "%s\n", "q:AL4MII:1234:0:5678:0:3.14:1:23.12:45:9.11:1:19.19:35:80:1:0:1:12.11:7.62:20.01:132.11:33:0:0:0:1:-10.1:7.7:1:19.19:35:8.22:1:1:18.11");
        if (strncmp(cmd, "p", 1) == 0)
            sprintf(res, "%s\n", "p:1234");
        if (strncmp(cmd, "i", 1) == 0)
            sprintf(res, "%s\n", "i:0");
        if (strncmp(cmd, "u", 1) == 0)
            sprintf(res, "%s\n", "u:1:1:80:120:30:50:200:800:200:800:0:2:10000:80000:0:0:50:18:30:15:5:10:10:0:1:0:0:0:0:0:0:0:40:90:10:1100:14000:10000:100:0");
        if (strncmp(cmd, "A", 1) == 0)
            sprintf(res, "%s\n", "A:4.5.0 mini II");
        if (strncmp(cmd, "R", 1) == 0)
            sprintf(res, "%s\n", "R:");
        if (strncmp(cmd, "C", 1) == 0)
            sprintf(res, "%s\n", "C:");
        if (strncmp(cmd, "B", 1) == 0)
            sprintf(res, "%s\n", "B:");
        if (strncmp(cmd, "H", 1) == 0)
            sprintf(res, "%s\n", "H:");
        if (strncmp(cmd, "P", 1) == 0)
            sprintf(res, "%s\n", "P:");
        if (strncmp(cmd, "U", 1) == 0)
            sprintf(res, "%s\n", "U:");
        if (strncmp(cmd, "S", 1) == 0)
            sprintf(res, "%s\n", "S:");
    }
    else
    {
        tcflush(PortFD, TCIOFLUSH);
        sprintf(command, "%s\n", cmd);
        DEBUGF(INDI::Logger::DBG_DEBUG, "CMD %s", cmd);
        if ((tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
            return false;

        if (!res)
        {
            tcflush(PortFD, TCIOFLUSH);
            return true;
        }

        if ((tty_rc = tty_nread_section(PortFD, res, ASTROLINK4_LEN, stopChar, ASTROLINK4_TIMEOUT, &nbytes_read)) != TTY_OK || nbytes_read == 1)
            return false;

        tcflush(PortFD, TCIOFLUSH);
        res[nbytes_read - 1] = '\0';
        DEBUGF(INDI::Logger::DBG_DEBUG, "RES %s", res);
        if (tty_rc != TTY_OK)
        {
            char errorMessage[MAXRBUF];
            tty_error_msg(tty_rc, errorMessage, MAXRBUF);
            LOGF_ERROR("Serial error: %s", errorMessage);
            return false;
        }
    }
    return (cmd[0] == res[0]);
}

bool IndiAstroLink4mini2::readDevice()
{
    char res[ASTROLINK4_LEN] = {0};
    if (sendCommand("q", res))
    {
        std::vector<std::string> result = split(res, ":");
        result.erase(result.begin());

        int focuserPosition = std::stoi(result[getFindex() == 1 ? Q_FOC2_POS : Q_FOC1_POS]);
        int stepsToGo = std::stod(result[getFindex() == 1 ? Q_FOC2_TO_GO : Q_FOC1_TO_GO]);
        FocusAbsPosN[0].value = focuserPosition;
        if (stepsToGo == 0)
        {
            FocusAbsPosNP.s = FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusRelPosNP, nullptr);
        }
        else
        {
            FocusAbsPosNP.s = FocusRelPosNP.s = IPS_BUSY;
        }
        IDSetNumber(&FocusAbsPosNP, nullptr);

        if (result.size() > 5)
        {
            if (std::stod(result[Q_SENS1_PRESENT]) > 0)
            {
                setParameterValue("WEATHER_TEMPERATURE", std::stod(result[Q_SENS1_TEMP]));
                setParameterValue("WEATHER_HUMIDITY", std::stod(result[Q_SENS1_HUM]));
                setParameterValue("WEATHER_DEWPOINT", std::stod(result[Q_SENS1_DEW]));
                ParametersNP.setState(IPS_OK);
            }
            else
            {
                ParametersNP.setState(IPS_IDLE);
            }

            ParametersNP.apply();

            if (Power1SP.s != IPS_OK || Power2SP.s != IPS_OK || Power3SP.s != IPS_OK)
            {
                Power1S[0].s = (std::stod(result[Q_OUT1]) > 0) ? ISS_ON : ISS_OFF;
                Power1S[1].s = (std::stod(result[Q_OUT1]) == 0) ? ISS_ON : ISS_OFF;
                Power1SP.s = IPS_OK;
                IDSetSwitch(&Power1SP, nullptr);
                Power2S[0].s = (std::stod(result[Q_OUT2]) > 0) ? ISS_ON : ISS_OFF;
                Power2S[1].s = (std::stod(result[Q_OUT2]) == 0) ? ISS_ON : ISS_OFF;
                Power2SP.s = IPS_OK;
                IDSetSwitch(&Power2SP, nullptr);
                Power3S[0].s = (std::stod(result[Q_OUT3]) > 0) ? ISS_ON : ISS_OFF;
                Power3S[1].s = (std::stod(result[Q_OUT3]) == 0) ? ISS_ON : ISS_OFF;
                Power3SP.s = IPS_OK;
                IDSetSwitch(&Power3SP, nullptr);
            }

            PWMN[0].value = std::stod(result[Q_PWM1]);
            PWMN[1].value = std::stod(result[Q_PWM2]);
            PWMNP.s = IPS_OK;
            IDSetNumber(&PWMNP, nullptr);

            PowerDataN[POW_ITOT].value = std::stod(result[Q_ITOT]);
            PowerDataN[POW_REG].value = std::stod(result[Q_VREG]);
            PowerDataN[POW_VIN].value = std::stod(result[Q_VIN]);
            PowerDataN[POW_AH].value = std::stod(result[Q_AH]);
            PowerDataN[POW_WH].value = std::stod(result[Q_WH]);
            PowerDataNP.s = IPS_OK;
            IDSetNumber(&PowerDataNP, nullptr);
        }
    }

    // update settings data if was changed
    if (PowerDefaultOnSP.s != IPS_OK || FocusMaxPosNP.s != IPS_OK || FocusReverseSP.s != IPS_OK || FocuserSelectSP.s != IPS_OK || Focuser1SettingsNP.s != IPS_OK || Focuser2SettingsNP.s != IPS_OK || Focuser1ModeSP.s != IPS_OK || Focuser2ModeSP.s != IPS_OK)
    {
        if (sendCommand("u", res))
        {
            std::vector<std::string> result = split(res, ":");

            if (PowerDefaultOnSP.s != IPS_OK)
            {
                PowerDefaultOnS[0].s = (std::stod(result[U_OUT1_DEF]) > 0) ? ISS_ON : ISS_OFF;
                PowerDefaultOnS[1].s = (std::stod(result[U_OUT2_DEF]) > 0) ? ISS_ON : ISS_OFF;
                PowerDefaultOnS[2].s = (std::stod(result[U_OUT3_DEF]) > 0) ? ISS_ON : ISS_OFF;
                PowerDefaultOnSP.s = IPS_OK;
                IDSetSwitch(&PowerDefaultOnSP, nullptr);
            }

            if (Focuser1SettingsNP.s != IPS_OK)
            {

                DEBUGF(INDI::Logger::DBG_DEBUG, "Update settings, focuser 1, res %s", res);
                Focuser1SettingsN[FS1_STEP_SIZE].value = std::stod(result[U_FOC1_STEP]) / 100.0;
                Focuser1SettingsN[FS1_COMPENSATION].value = std::stod(result[U_FOC1_COMPSTEPS]) / 100.0;
                Focuser1SettingsN[FS1_COMP_THRESHOLD].value = std::stod(result[U_FOC1_COMPTRIGGER]);
                Focuser1SettingsN[FS1_SPEED].value = std::stod(result[U_FOC1_SPEED]);
                Focuser1SettingsN[FS1_CURRENT].value = std::stod(result[U_FOC1_CUR]) * 10.0;
                Focuser1SettingsN[FS1_HOLD].value = std::stod(result[U_FOC1_HOLD]);
                Focuser1SettingsNP.s = IPS_OK;
                IDSetNumber(&Focuser1SettingsNP, nullptr);
            }

            if (Focuser2SettingsNP.s != IPS_OK)
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Update settings, focuser 2, res %s", res);
                Focuser2SettingsN[FS2_STEP_SIZE].value = std::stod(result[U_FOC2_STEP]) / 100.0;
                Focuser2SettingsN[FS2_COMPENSATION].value = std::stod(result[U_FOC2_COMPSTEPS]) / 100.0;
                Focuser2SettingsN[FS2_COMP_THRESHOLD].value = std::stod(result[U_FOC2_COMPTRIGGER]);
                Focuser2SettingsN[FS2_SPEED].value = std::stod(result[U_FOC2_SPEED]);
                Focuser2SettingsN[FS2_CURRENT].value = std::stod(result[U_FOC2_CUR]) * 10.0;
                Focuser2SettingsN[FS2_HOLD].value = std::stod(result[U_FOC2_HOLD]);
                Focuser2SettingsNP.s = IPS_OK;
                IDSetNumber(&Focuser2SettingsNP, nullptr);
            }

            if (Focuser1ModeSP.s != IPS_OK)
            {
                Focuser1ModeS[FS1_MODE_UNI].s = Focuser1ModeS[FS1_MODE_MICRO_L].s = Focuser1ModeS[FS1_MODE_MICRO_H].s = ISS_OFF;
                if (!strcmp("0", result[U_FOC1_MODE].c_str()))
                    Focuser1ModeS[FS1_MODE_UNI].s = ISS_ON;
                if (!strcmp("1", result[U_FOC1_MODE].c_str()))
                    Focuser1ModeS[FS1_MODE_MICRO_L].s = ISS_ON;
                if (!strcmp("2", result[U_FOC1_MODE].c_str()))
                    Focuser1ModeS[FS1_MODE_MICRO_H].s = ISS_ON;
                Focuser1ModeSP.s = IPS_OK;
                IDSetSwitch(&Focuser1ModeSP, nullptr);
            }

            if (Focuser2ModeSP.s != IPS_OK)
            {
                Focuser2ModeS[FS2_MODE_UNI].s = Focuser2ModeS[FS2_MODE_MICRO_L].s = Focuser2ModeS[FS2_MODE_MICRO_H].s = ISS_OFF;
                if (!strcmp("0", result[U_FOC2_MODE].c_str()))
                    Focuser2ModeS[FS2_MODE_UNI].s = ISS_ON;
                if (!strcmp("1", result[U_FOC2_MODE].c_str()))
                    Focuser2ModeS[FS2_MODE_MICRO_L].s = ISS_ON;
                if (!strcmp("2", result[U_FOC2_MODE].c_str()))
                    Focuser2ModeS[FS2_MODE_MICRO_H].s = ISS_ON;
                Focuser2ModeSP.s = IPS_OK;
                IDSetSwitch(&Focuser2ModeSP, nullptr);
            }

            if (FocusMaxPosNP.s != IPS_OK)
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Update maxpos, focuser %i, res %s", getFindex(), res);
                int index = getFindex() > 0 ? U_FOC2_MAX : U_FOC1_MAX;
                FocusMaxPosN[0].value = std::stod(result[index]);
                FocusMaxPosNP.s = IPS_OK;
                IDSetNumber(&FocusMaxPosNP, nullptr);
            }
            if (FocusReverseSP.s != IPS_OK)
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Update reverse, focuser %i, res %s", getFindex(), res);
                int index = getFindex() > 0 ? U_FOC2_REV : U_FOC1_REV;
                FocusReverseS[0].s = (std::stoi(result[index]) > 0) ? ISS_ON : ISS_OFF;
                FocusReverseS[1].s = (std::stoi(result[index]) == 0) ? ISS_ON : ISS_OFF;
                FocusReverseSP.s = IPS_OK;
                IDSetSwitch(&FocusReverseSP, nullptr);
            }
            FocuserSelectSP.s = IPS_OK;
            IDSetSwitch(&FocuserSelectSP, nullptr);
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
/// Helper functions
//////////////////////////////////////////////////////////////////////
std::vector<std::string> IndiAstroLink4mini2::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

std::string IndiAstroLink4mini2::doubleToStr(double val)
{
    char buf[10];
    sprintf(buf, "%.0f", val);
    return std::string(buf);
}

std::string IndiAstroLink4mini2::intToStr(double val)
{
    char buf[10];
    sprintf(buf, "%i", (int)val);
    return std::string(buf);
}

bool IndiAstroLink4mini2::updateSettings(const char *getCom, const char *setCom, int index, const char *value)
{
    std::map<int, std::string> values;
    values[index] = value;
    return updateSettings(getCom, setCom, values);
}

bool IndiAstroLink4mini2::updateSettings(const char *getCom, const char *setCom, std::map<int, std::string> values)
{
    char cmd[ASTROLINK4_LEN] = {0}, res[ASTROLINK4_LEN] = {0};
    snprintf(cmd, ASTROLINK4_LEN, "%s", getCom);
    if (sendCommand(cmd, res))
    {
        std::string concatSettings = "";
        std::vector<std::string> result = split(res, ":");
        if (result.size() >= values.size())
        {
            result[0] = setCom;
            for (std::map<int, std::string>::iterator it = values.begin(); it != values.end(); ++it)
                result[it->first] = it->second;

            for (const auto &piece : result)
                concatSettings += piece + ":";

            snprintf(cmd, ASTROLINK4_LEN, "%s", concatSettings.c_str());
            if (sendCommand(cmd, res))
                return true;
        }
    }
    return false;
}

int IndiAstroLink4mini2::getFindex()
{
    return focuserIndex;
}

void IndiAstroLink4mini2::setFindex(int index)
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "Focuser index set to %i", index);
    focuserIndex = index;
}
