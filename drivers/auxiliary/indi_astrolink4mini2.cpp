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
    setFindex(IUGetConfigOnSwitchName(getDeviceName(), FocuserSelectSP.getName(), focuserSelectLabel, 15) == 0 ? 0 : 1);

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

    FocuserSelectSP[FOC_SEL_1].fill("FOC_SEL_1", "Focuser 1", (getFindex() == 0 ? ISS_ON : ISS_OFF));
    FocuserSelectSP[FOC_SEL_2].fill("FOC_SEL_2", "Focuser 2", (getFindex() > 0 ? ISS_ON : ISS_OFF));
    FocuserSelectSP.fill(getDeviceName(), "FOCUSER_SELECT", "Focuser select", FOCUS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Power readings
    PowerDataNP[POW_VIN].fill("VIN", "Input voltage [V]", "%.1f", 0, 15, 10, 0);
    PowerDataNP[POW_REG].fill("REG", "Regulated voltage [V]", "%.1f", 0, 15, 10, 0);
    PowerDataNP[POW_ITOT].fill("ITOT", "Total current [A]", "%.1f", 0, 15, 10, 0);
    PowerDataNP[POW_AH].fill("AH", "Energy consumed [Ah]", "%.1f", 0, 1000, 10, 0);
    PowerDataNP[POW_WH].fill("WH", "Energy consumed [Wh]", "%.1f", 0, 10000, 10, 0);
    PowerDataNP.fill(getDeviceName(), "POWER_DATA", "Power data", POWER_TAB, IP_RO, 60, IPS_IDLE);

    // Power lines
    Power1SP[PWR1BTN_ON].fill("PWR1BTN_ON", "ON", ISS_OFF);
    Power1SP[PWR1BTN_OFF].fill( "PWR1BTN_OFF", "OFF", ISS_ON);
    Power1SP.fill(getDeviceName(), "DC1", "Port 1", POWER_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    Power2SP[PWR2BTN_ON].fill("PWR2BTN_ON", "ON", ISS_OFF);
    Power2SP[PWR2BTN_OFF].fill("PWR2BTN_OFF", "OFF", ISS_ON);
    Power2SP.fill(getDeviceName(), "DC2", "Port 2", POWER_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    Power3SP[PWR3BTN_ON].fill( "PWR3BTN_ON", "ON", ISS_OFF);
    Power3SP[PWR3BTN_OFF].fill("PWR3BTN_OFF", "OFF", ISS_ON);
    Power3SP.fill(getDeviceName(), "DC3", "Port 3", POWER_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    PWMNP[PWM1_VAL].fill("PWM1_VAL", "A", "%3.0f", 0, 100, 10, 0);
    PWMNP[PWM2_VAL].fill("PWM2_VAL", "B", "%3.0f", 0, 100, 10, 0);
    PWMNP.fill(getDeviceName(), "PWM", "PWM", POWER_TAB, IP_RW, 60, IPS_IDLE);

    PowerDefaultOnSP[POW_DEF_ON1].fill("POW_DEF_ON1", "DC1", ISS_OFF);
    PowerDefaultOnSP[POW_DEF_ON2].fill("POW_DEF_ON2", "DC2", ISS_OFF);
    PowerDefaultOnSP[POW_DEF_ON3].fill("POW_DEF_ON3", "DC3", ISS_OFF);
    PowerDefaultOnSP.fill(getDeviceName(), "POW_DEF_ON", "Power default ON", POWER_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // focuser settings
    Focuser1SettingsNP[FS1_SPEED].fill("FS1_SPEED", "Speed [pps]", "%.0f", 10, 200, 1, 100);
    Focuser1SettingsNP[FS1_CURRENT].fill("FS1_CURRENT", "Current [mA]", "%.0f", 100, 2000, 100, 400);
    Focuser1SettingsNP[FS1_HOLD].fill("FS1_HOLD", "Hold torque [%]", "%.0f", 0, 100, 10, 0);
    Focuser1SettingsNP[FS1_STEP_SIZE].fill("FS1_STEP_SIZE", "Step size [um]", "%.2f", 0, 100, 0.1, 5.0);
    Focuser1SettingsNP[FS1_COMPENSATION].fill("FS1_COMPENSATION", "Compensation [steps/C]", "%.2f", -1000, 1000, 1, 0);
    Focuser1SettingsNP[FS1_COMP_THRESHOLD].fill("FS1_COMP_THRESHOLD", "Compensation threshold [steps]", "%.0f", 1, 1000, 10,
            10);
    Focuser1SettingsNP.fill(getDeviceName(), "FOCUSER1_SETTINGS", "Focuser 1 settings", FOC1_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    Focuser2SettingsNP[FS2_SPEED].fill("FS2_SPEED", "Speed [pps]", "%.0f", 10, 200, 1, 100);
    Focuser2SettingsNP[FS2_CURRENT].fill("FS2_CURRENT", "Current [mA]", "%.0f", 100, 2000, 100, 400);
    Focuser2SettingsNP[FS2_HOLD].fill("FS2_HOLD", "Hold torque [%]", "%.0f", 0, 100, 10, 0);
    Focuser2SettingsNP[FS2_STEP_SIZE].fill("FS2_STEP_SIZE", "Step size [um]", "%.2f", 0, 100, 0.1, 5.0);
    Focuser2SettingsNP[FS2_COMPENSATION].fill("FS2_COMPENSATION", "Compensation [steps/C]", "%.2f", -1000, 1000, 1, 0);
    Focuser2SettingsNP[FS2_COMP_THRESHOLD].fill("FS2_COMP_THRESHOLD", "Compensation threshold [steps]", "%.0f", 1, 1000, 10,
            10);
    Focuser2SettingsNP.fill(getDeviceName(), "FOCUSER2_SETTINGS", "Focuser 2 settings", FOC2_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    Focuser1ModeSP[FS1_MODE_UNI].fill("FS1_MODE_UNI", "Unipolar", ISS_ON);
    Focuser1ModeSP[FS1_MODE_MICRO_L].fill("FS1_MODE_MICRO_L", "Microstep 1/8", ISS_OFF);
    Focuser1ModeSP[FS1_MODE_MICRO_H].fill( "FS1_MODE_MICRO_H", "Microstep 1/32", ISS_OFF);
    Focuser1ModeSP.fill(getDeviceName(), "FOCUSER1_MODE", "Focuser mode", FOC1_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    Focuser2ModeSP[FS2_MODE_UNI].fill("FS2_MODE_UNI", "Unipolar", ISS_ON);
    Focuser2ModeSP[FS2_MODE_MICRO_L].fill("FS2_MODE_MICRO_L", "Microstep 1/8", ISS_OFF);
    Focuser2ModeSP[FS2_MODE_MICRO_H].fill("FS2_MODE_MICRO_H", "Microstep 1/32", ISS_OFF);
    Focuser2ModeSP.fill(getDeviceName(), "FOCUSER2_MODE", "Focuser mode", FOC2_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

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
        defineProperty(FocuserSelectSP);
        defineProperty(Focuser1SettingsNP);
        defineProperty(Focuser2SettingsNP);
        defineProperty(Focuser1ModeSP);
        defineProperty(Focuser2ModeSP);
        defineProperty(PowerDataNP);
        defineProperty(Power1SP);
        defineProperty(Power2SP);
        defineProperty(Power3SP);
        defineProperty(PWMNP);
        defineProperty(PowerDefaultOnSP);
    }
    else
    {
        deleteProperty(PowerDataNP);
        deleteProperty(Focuser1SettingsNP);
        deleteProperty(Focuser2SettingsNP);
        deleteProperty(Focuser1ModeSP);
        deleteProperty(Focuser2ModeSP);
        deleteProperty(FocuserSelectSP);
        deleteProperty(Power1SP);
        deleteProperty(Power2SP);
        deleteProperty(Power3SP);
        deleteProperty(PWMNP);
        deleteProperty(PowerDefaultOnSP);
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
        if (PWMNP.isNameMatch(name))
        {
            bool allOk = true;
            if (PWMNP[PWM1_VAL].getValue() != values[0])
            {
                sprintf(cmd, "B:0:%d", static_cast<uint8_t>(values[0]));
                allOk = allOk && sendCommand(cmd, res);
            }
            if (PWMNP[PWM2_VAL].getValue() != values[1])
            {
                sprintf(cmd, "B:1:%d", static_cast<uint8_t>(values[1]));
                allOk = allOk && sendCommand(cmd, res);
            }
            PWMNP.setState((allOk) ? IPS_BUSY : IPS_ALERT);
            if (allOk)
                PWMNP.update(values, names, n);
            PWMNP.apply();
            return true;
        }

        // Focuser settings
        if (Focuser1SettingsNP.isNameMatch(name))
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
                Focuser1SettingsNP.setState(IPS_BUSY);
                Focuser1SettingsNP.update(values, names, n);
                Focuser1SettingsNP.apply();
                DEBUGF(INDI::Logger::DBG_SESSION, "Focuser 1 temperature compensation is %s",
                       (values[FS1_COMPENSATION] > 0) ? "enabled" : "disabled");
                return true;
            }
            Focuser1SettingsNP.setState(IPS_ALERT);
            return true;
        }

        if (Focuser2SettingsNP.isNameMatch(name))
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
                Focuser2SettingsNP.setState(IPS_BUSY);
                Focuser2SettingsNP.update(values, names, n);
                Focuser2SettingsNP.apply();
                DEBUGF(INDI::Logger::DBG_SESSION, "Focuser 2 temperature compensation is %s",
                       (values[FS2_COMPENSATION] > 0) ? "enabled" : "disabled");
                return true;
            }
            Focuser2SettingsNP.setState(IPS_ALERT);
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
        if (Power1SP.isNameMatch(name))
        {
            sprintf(cmd, "C:0:%s", (strcmp(Power1SP[PWR1BTN_ON].getName(), names[0])) ? "0" : "1");
            bool allOk = sendCommand(cmd, res);
            Power1SP.setState(allOk ? IPS_BUSY : IPS_ALERT);
            if (allOk)
                Power1SP.update(states, names, n);

            Power1SP.apply();
            return true;
        }

        // handle power line 2
        if (Power2SP.isNameMatch(name))
        {
            sprintf(cmd, "C:1:%s", (strcmp(Power2SP[PWR2BTN_ON].getName(), names[0])) ? "0" : "1");
            bool allOk = sendCommand(cmd, res);
            Power2SP.setState(allOk ? IPS_BUSY : IPS_ALERT);
            if (allOk)
                Power2SP.update(states, names, n);

            Power2SP.apply();
            return true;
        }

        // handle power line 3
        if (Power3SP.isNameMatch(name))
        {
            sprintf(cmd, "C:2:%s", (strcmp(Power3SP[PWR3BTN_ON].getName(), names[0])) ? "0" : "1");
            bool allOk = sendCommand(cmd, res);
            Power3SP.setState(allOk ? IPS_BUSY : IPS_ALERT);
            if (allOk)
                Power3SP.update(states, names, n);

            Power3SP.apply();
            return true;
        }

        // Power default on
        if (PowerDefaultOnSP.isNameMatch(name))
        {
            std::map<int, std::string> updates;
            updates[U_OUT1_DEF] = (states[0] == ISS_ON) ? "1" : "0";
            updates[U_OUT2_DEF] = (states[1] == ISS_ON) ? "1" : "0";
            updates[U_OUT3_DEF] = (states[2] == ISS_ON) ? "1" : "0";
            if (updateSettings("u", "U", updates))
            {
                PowerDefaultOnSP.setState(IPS_BUSY);
                PowerDefaultOnSP.update(states, names, n);
                PowerDefaultOnSP.apply();
                return true;
            }
            PowerDefaultOnSP.setState(IPS_ALERT);
            return true;
        }

        // Focuser Mode
        if (Focuser1ModeSP.isNameMatch(name))
        {
            std::string value = "0";
            if (!strcmp(Focuser1ModeSP[FS1_MODE_UNI].getName(), names[0]))
                value = "0";
            if (!strcmp(Focuser1ModeSP[FS1_MODE_MICRO_L].getName(), names[0]))
                value = "1";
            if (!strcmp(Focuser1ModeSP[FS1_MODE_MICRO_H].getName(), names[0]))
                value = "2";
            if (updateSettings("u", "U", U_FOC1_MODE, value.c_str()))
            {
                Focuser1ModeSP.setState(IPS_BUSY);
                Focuser1ModeSP.update(states, names, n);
                Focuser1ModeSP.apply();
                return true;
            }
            Focuser1ModeSP.setState(IPS_ALERT);
            return true;
        }
        if (Focuser2ModeSP.isNameMatch(name))
        {
            std::string value = "0";
            if (!strcmp(Focuser2ModeSP[FS2_MODE_UNI].getName(), names[0]))
                value = "0";
            if (!strcmp(Focuser2ModeSP[FS2_MODE_MICRO_L].getName(), names[0]))
                value = "1";
            if (!strcmp(Focuser2ModeSP[FS2_MODE_MICRO_H].getName(), names[0]))
                value = "2";
            if (updateSettings("u", "U", U_FOC2_MODE, value.c_str()))
            {
                Focuser2ModeSP.setState(IPS_BUSY);
                Focuser2ModeSP.update(states, names, n);
                Focuser2ModeSP.apply();
                return true;
            }
            Focuser2ModeSP.setState(IPS_ALERT);
            return true;
        }

        // Stepper select
        if (FocuserSelectSP.isNameMatch(name))
        {
            setFindex((strcmp(FocuserSelectSP[0].getName(), names[0])) ? 1 : 0);
            DEBUGF(INDI::Logger::DBG_DEBUG, "Focuser index set by switch to %i", getFindex());
            FocusAbsPosNP.setState(IPS_BUSY);   
            FocusReverseSP.setState(FocusAbsPosNP.getState());
            FocusMaxPosNP.setState(FocusReverseSP.getState());
            FocuserSelectSP.setState(FocusMaxPosNP.getState()); 

            FocuserSelectSP.update(states, names, n);
            FocuserSelectSP.apply();
            FocusReverseSP.apply();
            FocusMaxPosNP.apply();
            FocusAbsPosNP.apply();

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
    FocuserSelectSP.save(fp);
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
    return MoveAbsFocuser(dir == FOCUS_INWARD ? FocusAbsPosNP[0].getValue() - ticks : FocusAbsPosNP[0].getValue() + ticks);
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
        FocusReverseSP.setState(IPS_BUSY);
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
        FocusAbsPosNP.setState(IPS_BUSY);
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
        FocusMaxPosNP.setState(IPS_BUSY);
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
            sprintf(res, "%s\n",
                    "q:AL4MII:1234:0:5678:0:3.14:1:23.12:45:9.11:1:19.19:35:80:1:0:1:12.11:7.62:20.01:132.11:33:0:0:0:1:-10.1:7.7:1:19.19:35:8.22:1:1:18.11");
        if (strncmp(cmd, "p", 1) == 0)
            sprintf(res, "%s\n", "p:1234");
        if (strncmp(cmd, "i", 1) == 0)
            sprintf(res, "%s\n", "i:0");
        if (strncmp(cmd, "u", 1) == 0)
            sprintf(res, "%s\n",
                    "u:1:1:80:120:30:50:200:800:200:800:0:2:10000:80000:0:0:50:18:30:15:5:10:10:0:1:0:0:0:0:0:0:0:40:90:10:1100:14000:10000:100:0");
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

        if ((tty_rc = tty_nread_section(PortFD, res, ASTROLINK4_LEN, stopChar, ASTROLINK4_TIMEOUT, &nbytes_read)) != TTY_OK
                || nbytes_read == 1)
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
        FocusAbsPosNP[0].setValue(focuserPosition);
        if (stepsToGo == 0)
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusRelPosNP.apply();
        }
        else
        {
            FocusAbsPosNP.setState(IPS_BUSY);
            FocusRelPosNP.setState(IPS_BUSY);
        }
        FocusAbsPosNP.apply();

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

            if (Power1SP.getState() != IPS_OK || Power2SP.getState() != IPS_OK || Power3SP.getState() != IPS_OK)
            {
                Power1SP[PWR1BTN_ON].setState((std::stod(result[Q_OUT1]) > 0) ? ISS_ON : ISS_OFF);
                Power1SP[PWR1BTN_OFF].setState((std::stod(result[Q_OUT1]) == 0) ? ISS_ON : ISS_OFF);
                Power1SP.setState(IPS_OK);
                Power1SP.apply();
                Power2SP[PWR2BTN_ON].setState((std::stod(result[Q_OUT2]) > 0) ? ISS_ON : ISS_OFF);
                Power2SP[PWR2BTN_OFF].setState((std::stod(result[Q_OUT2]) == 0) ? ISS_ON : ISS_OFF);
                Power2SP.setState(IPS_OK);
                Power2SP.apply();
                Power3SP[PWR3BTN_ON].setState((std::stod(result[Q_OUT3]) > 0) ? ISS_ON : ISS_OFF);
                Power3SP[PWR3BTN_OFF].setState((std::stod(result[Q_OUT3]) == 0) ? ISS_ON : ISS_OFF);
                Power3SP.setState(IPS_OK);
                Power3SP.apply();
            }

            PWMNP[PWM1_VAL].setValue(std::stod(result[Q_PWM1]));
            PWMNP[PWM2_VAL].setValue(std::stod(result[Q_PWM2]));
            PWMNP.setState(IPS_OK);
            PWMNP.apply();

            PowerDataNP[POW_ITOT].setValue(std::stod(result[Q_ITOT]));
            PowerDataNP[POW_REG].setValue(std::stod(result[Q_VREG]));
            PowerDataNP[POW_VIN].setValue(std::stod(result[Q_VIN]));
            PowerDataNP[POW_AH].setValue(std::stod(result[Q_AH]));
            PowerDataNP[POW_WH].setValue(std::stod(result[Q_WH]));
            PowerDataNP.setState(IPS_OK);
            PowerDataNP.apply();
        }
    }

    // update settings data if was changed
    if (PowerDefaultOnSP.getState() != IPS_OK || FocusMaxPosNP.getState() != IPS_OK || FocusReverseSP.getState() != IPS_OK
            || FocuserSelectSP.getState() != IPS_OK || Focuser1SettingsNP.getState() != IPS_OK
            || Focuser2SettingsNP.getState() != IPS_OK || Focuser1ModeSP.getState() != IPS_OK || Focuser2ModeSP.getState() != IPS_OK)
    {
        if (sendCommand("u", res))
        {
            std::vector<std::string> result = split(res, ":");

            if (PowerDefaultOnSP.getState() != IPS_OK)
            {
                PowerDefaultOnSP[POW_DEF_ON1].setState((std::stod(result[U_OUT1_DEF]) > 0) ? ISS_ON : ISS_OFF);
                PowerDefaultOnSP[POW_DEF_ON2].setState((std::stod(result[U_OUT2_DEF]) > 0) ? ISS_ON : ISS_OFF);
                PowerDefaultOnSP[POW_DEF_ON3].setState((std::stod(result[U_OUT3_DEF]) > 0) ? ISS_ON : ISS_OFF);
                PowerDefaultOnSP.setState(IPS_OK);
                PowerDefaultOnSP.apply();
            }

            if (Focuser1SettingsNP.getState() != IPS_OK)
            {

                DEBUGF(INDI::Logger::DBG_DEBUG, "Update settings, focuser 1, res %s", res);
                Focuser1SettingsNP[FS1_STEP_SIZE].setValue(std::stod(result[U_FOC1_STEP]) / 100.0);
                Focuser1SettingsNP[FS1_COMPENSATION].setValue(std::stod(result[U_FOC1_COMPSTEPS]) / 100.0);
                Focuser1SettingsNP[FS1_COMP_THRESHOLD].setValue(std::stod(result[U_FOC1_COMPTRIGGER]));
                Focuser1SettingsNP[FS1_SPEED].setValue(std::stod(result[U_FOC1_SPEED]));
                Focuser1SettingsNP[FS1_CURRENT].setValue(std::stod(result[U_FOC1_CUR]) * 10.0);
                Focuser1SettingsNP[FS1_HOLD].setValue(std::stod(result[U_FOC1_HOLD]));
                Focuser1SettingsNP.setState(IPS_OK);
                Focuser1SettingsNP.apply();
            }

            if (Focuser2SettingsNP.getState() != IPS_OK)
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Update settings, focuser 2, res %s", res);
                Focuser2SettingsNP[FS2_STEP_SIZE].setValue(std::stod(result[U_FOC2_STEP]) / 100.0);
                Focuser2SettingsNP[FS2_COMPENSATION].setValue(std::stod(result[U_FOC2_COMPSTEPS]) / 100.0);
                Focuser2SettingsNP[FS2_COMP_THRESHOLD].setValue(std::stod(result[U_FOC2_COMPTRIGGER]));
                Focuser2SettingsNP[FS2_SPEED].setValue(std::stod(result[U_FOC2_SPEED]));
                Focuser2SettingsNP[FS2_CURRENT].setValue(std::stod(result[U_FOC2_CUR]) * 10.0);
                Focuser2SettingsNP[FS2_HOLD].setValue(std::stod(result[U_FOC2_HOLD]));
                Focuser2SettingsNP.setState(IPS_OK);
                Focuser2SettingsNP.apply();
            }

            if (Focuser1ModeSP.getState() != IPS_OK)
            {
                Focuser1ModeSP[FS1_MODE_UNI].setState(Focuser1ModeSP[FS1_MODE_MICRO_L].s = Focuser1ModeSP[FS1_MODE_MICRO_H].s = ISS_OFF);
                if (!strcmp("0", result[U_FOC1_MODE].c_str()))
                    Focuser1ModeSP[FS1_MODE_UNI].setState(ISS_ON);
                if (!strcmp("1", result[U_FOC1_MODE].c_str()))
                    Focuser1ModeSP[FS1_MODE_MICRO_L].setState(ISS_ON);
                if (!strcmp("2", result[U_FOC1_MODE].c_str()))
                    Focuser1ModeSP[FS1_MODE_MICRO_H].setState(ISS_ON);
                Focuser1ModeSP.setState(IPS_OK);
                Focuser1ModeSP.apply();
            }

            if (Focuser2ModeSP.getState() != IPS_OK)
            {
                Focuser2ModeSP[FS2_MODE_UNI].setState(Focuser2ModeSP[FS2_MODE_MICRO_L].s = Focuser2ModeSP[FS2_MODE_MICRO_H].s = ISS_OFF);
                if (!strcmp("0", result[U_FOC2_MODE].c_str()))
                    Focuser2ModeSP[FS2_MODE_UNI].setState(ISS_ON);
                if (!strcmp("1", result[U_FOC2_MODE].c_str()))
                    Focuser2ModeSP[FS2_MODE_MICRO_L].setState(ISS_ON);
                if (!strcmp("2", result[U_FOC2_MODE].c_str()))
                    Focuser2ModeSP[FS2_MODE_MICRO_H].setState(ISS_ON);
                Focuser2ModeSP.setState(IPS_OK);
                Focuser2ModeSP.apply();
            }

            if (FocusMaxPosNP.getState() != IPS_OK)
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Update maxpos, focuser %i, res %s", getFindex(), res);
                int index = getFindex() > 0 ? U_FOC2_MAX : U_FOC1_MAX;
                FocusMaxPosNP[0].setValue(std::stod(result[index]));
                FocusMaxPosNP.setState(IPS_OK);
                FocusMaxPosNP.apply();
            }
            if (FocusReverseSP.getState() != IPS_OK)
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Update reverse, focuser %i, res %s", getFindex(), res);
                int index = getFindex() > 0 ? U_FOC2_REV : U_FOC1_REV;
                FocusReverseSP[INDI_ENABLED].setState((std::stoi(result[index]) > 0) ? ISS_ON : ISS_OFF);
                FocusReverseSP[INDI_DISABLED].setState((std::stoi(result[index]) == 0) ? ISS_ON : ISS_OFF);
                FocusReverseSP.setState(IPS_OK);
                FocusReverseSP.apply();
            }
            FocuserSelectSP.setState(IPS_OK);
            FocuserSelectSP.apply();
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
