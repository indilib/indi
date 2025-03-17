/*******************************************************************************
 Copyright(c) 2019 astrojolo.com
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
#include "indi_astrolink4.h"
#include "config.h"

#include "indicom.h"

#define ASTROLINK4_LEN      100
#define ASTROLINK4_TIMEOUT  3

//////////////////////////////////////////////////////////////////////
/// Delegates
//////////////////////////////////////////////////////////////////////
static std::unique_ptr<IndiAstrolink4> indiAstrolink4(new IndiAstrolink4());

//////////////////////////////////////////////////////////////////////
///Constructor
//////////////////////////////////////////////////////////////////////
IndiAstrolink4::IndiAstrolink4() : FI(this), WI(this)
{
    setVersion(ASTROLINK4_VERSION_MAJOR, ASTROLINK4_VERSION_MINOR);
}

const char * IndiAstrolink4::getDefaultName()
{
    return "AstroLink 4";
}

//////////////////////////////////////////////////////////////////////
/// Communication
//////////////////////////////////////////////////////////////////////
bool IndiAstrolink4::Handshake()
{
    PortFD = serialConnection->getPortFD();

    char res[ASTROLINK4_LEN] = {0};
    if(sendCommand("#", res))
    {
        if(strncmp(res, "#:AstroLink4mini", 15) != 0)
        {
            LOG_ERROR("Device not recognized.");
            return false;
        }
        else
        {
            SetTimer(getCurrentPollingPeriod());
            return true;
        }
    }
    return false;
}

void IndiAstrolink4::TimerHit()
{
    if(!isConnected())
        return;

    sensorRead();
    SetTimer(getCurrentPollingPeriod());
}

//////////////////////////////////////////////////////////////////////
/// Overrides
//////////////////////////////////////////////////////////////////////
bool IndiAstrolink4::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | FOCUSER_INTERFACE | WEATHER_INTERFACE);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_REVERSE  |
                      FOCUSER_CAN_SYNC     |
                      FOCUSER_CAN_ABORT    |
                      FOCUSER_HAS_BACKLASH);

    FI::initProperties(FOCUS_TAB);
    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

    addDebugControl();
    addSimulationControl();
    addConfigurationControl();

    // focuser settings
    IUFillNumber(&FocuserSettingsN[FS_SPEED], "FS_SPEED", "Speed [pps]", "%.0f", 0, 4000, 50, 250);
    IUFillNumber(&FocuserSettingsN[FS_STEP_SIZE], "FS_STEP_SIZE", "Step size [um]", "%.2f", 0, 100, 0.1, 5.0);
    IUFillNumber(&FocuserSettingsN[FS_COMPENSATION], "FS_COMPENSATION", "Compensation [steps/C]", "%.2f", -1000, 1000, 1, 0);
    IUFillNumber(&FocuserSettingsN[FS_COMP_THRESHOLD], "FS_COMP_THRESHOLD", "Compensation threshold [steps]", "%.0f", 1, 1000,
                 10, 10);
    IUFillNumberVector(&FocuserSettingsNP, FocuserSettingsN, 4, getDeviceName(), "FOCUSER_SETTINGS", "Focuser settings",
                       SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    IUFillSwitch(&FocuserModeS[FS_MODE_UNI], "FS_MODE_UNI", "Unipolar", ISS_ON);
    IUFillSwitch(&FocuserModeS[FS_MODE_BI], "FS_MODE_BI", "Bipolar", ISS_OFF);
    IUFillSwitch(&FocuserModeS[FS_MODE_MICRO], "FS_MODE_MICRO", "Microstep", ISS_OFF);
    IUFillSwitchVector(&FocuserModeSP, FocuserModeS, 3, getDeviceName(), "FOCUSER_MODE", "Focuser mode", SETTINGS_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&FocuserCompModeS[FS_COMP_AUTO], "FS_COMP_AUTO", "AUTO", ISS_OFF);
    IUFillSwitch(&FocuserCompModeS[FS_COMP_MANUAL], "FS_COMP_MANUAL", "MANUAL", ISS_ON);
    IUFillSwitchVector(&FocuserCompModeSP, FocuserCompModeS, 2, getDeviceName(), "COMP_MODE", "Compensation mode", SETTINGS_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&FocuserManualS[FS_MANUAL_ON], "FS_MANUAL_ON", "ON", ISS_ON);
    IUFillSwitch(&FocuserManualS[FS_MANUAL_OFF], "FS_MANUAL_OFF", "OFF", ISS_OFF);
    IUFillSwitchVector(&FocuserManualSP, FocuserManualS, 2, getDeviceName(), "MANUAL_CONTROLLER", "Hand controller",
                       SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // other settings
    IUFillNumber(&OtherSettingsN[SET_AREF_COEFF], "SET_AREF_COEFF", "V ref coefficient", "%.3f", 0.9, 1.2, 0.001, 1.09);
    IUFillNumber(&OtherSettingsN[SET_OVER_TIME], "SET_OVER_TIME", "Protection sensitivity [ms]", "%.0f", 10, 500, 10, 100);
    IUFillNumber(&OtherSettingsN[SET_OVER_VOLT], "SET_OVER_VOLT", "Protection voltage [V]", "%.1f", 10, 14, 0.1, 14);
    IUFillNumber(&OtherSettingsN[SET_OVER_AMP], "SET_OVER_AMP", "Protection current [A]", "%.1f", 1, 10, 0.1, 10);
    IUFillNumberVector(&OtherSettingsNP, OtherSettingsN, 4, getDeviceName(), "OTHER_SETTINGS", "Device settings", SETTINGS_TAB,
                       IP_RW, 60, IPS_IDLE);

    IUFillSwitch(&BuzzerS[0], "BUZZER", "Buzzer", ISS_OFF);
    IUFillSwitchVector(&BuzzerSP, BuzzerS, 1, getDeviceName(), "BUZZER", "ONOFF", SETTINGS_TAB, IP_RW, ISR_NOFMANY, 60,
                       IPS_IDLE);

    // focuser compensation
    IUFillNumber(&CompensationValueN[0], "COMP_VALUE", "Compensation steps", "%.0f", -10000, 10000, 1, 0);
    IUFillNumberVector(&CompensationValueNP, CompensationValueN, 1, getDeviceName(), "COMP_STEPS", "Compensation steps",
                       FOCUS_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&CompensateNowS[0], "COMP_NOW", "Compensate now", ISS_OFF);
    IUFillSwitchVector(&CompensateNowSP, CompensateNowS, 1, getDeviceName(), "COMP_NOW", "Compensate now", FOCUS_TAB, IP_RW,
                       ISR_ATMOST1, 60, IPS_IDLE);

    IUFillNumber(&FocusPosMMN[0], "FOC_POS_MM", "Position [mm]", "%.3f", 0.0, 200.0, 0.001, 0.0);
    IUFillNumberVector(&FocusPosMMNP, FocusPosMMN, 1, getDeviceName(), "FOC_POS_MM", "Position [mm]", FOCUS_TAB, IP_RO, 60,
                       IPS_IDLE);

    // power lines
    IUFillText(&PowerControlsLabelsT[0], "POWER_LABEL_1", "Port 1", "Port 1");
    IUFillText(&PowerControlsLabelsT[1], "POWER_LABEL_2", "Port 2", "Port 2");
    IUFillText(&PowerControlsLabelsT[2], "POWER_LABEL_3", "Port 3", "Port 3");
    IUFillTextVector(&PowerControlsLabelsTP, PowerControlsLabelsT, 3, getDeviceName(), "POWER_CONTROL_LABEL", "Power Labels",
                     POWER_TAB, IP_WO, 60, IPS_IDLE);

    char portLabel1[MAXINDILABEL], portLabel2[MAXINDILABEL], portLabel3[MAXINDILABEL];

    memset(portLabel1, 0, MAXINDILABEL);
    int portRC1 = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.name, PowerControlsLabelsT[0].name, portLabel1,
                                  MAXINDILABEL);

    IUFillSwitch(&Power1S[0], "PWR1BTN_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power1S[1], "PWR1BTN_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&Power1SP, Power1S, 2, getDeviceName(), "DC1", portRC1 == -1 ? "Port 1" : portLabel1, POWER_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    memset(portLabel2, 0, MAXINDILABEL);
    int portRC2 = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.name, PowerControlsLabelsT[1].name, portLabel2,
                                  MAXINDILABEL);

    IUFillSwitch(&Power2S[0], "PWR2BTN_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power2S[1], "PWR2BTN_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&Power2SP, Power2S, 2, getDeviceName(), "DC2", portRC2 == -1 ? "Port 2" : portLabel2, POWER_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    memset(portLabel3, 0, MAXINDILABEL);
    int portRC3 = IUGetConfigText(getDeviceName(), PowerControlsLabelsTP.name, PowerControlsLabelsT[2].name, portLabel3,
                                  MAXINDILABEL);

    IUFillSwitch(&Power3S[0], "PWR3BTN_ON", "ON", ISS_OFF);
    IUFillSwitch(&Power3S[1], "PWR3BTN_OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&Power3SP, Power3S, 2, getDeviceName(), "DC3", portRC3 == -1 ? "Port 3" : portLabel3, POWER_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&PowerDefaultOnS[0], "POW_DEF_ON1", "DC1", ISS_OFF);
    IUFillSwitch(&PowerDefaultOnS[1], "POW_DEF_ON2", "DC2", ISS_OFF);
    IUFillSwitch(&PowerDefaultOnS[2], "POW_DEF_ON3", "DC3", ISS_OFF);
    IUFillSwitchVector(&PowerDefaultOnSP, PowerDefaultOnS, 3, getDeviceName(), "POW_DEF_ON", "Power default ON", SETTINGS_TAB,
                       IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // pwm
    IUFillNumber(&PWMN[0], "PWM1_VAL", "A", "%3.0f", 0, 100, 10, 0);
    IUFillNumber(&PWMN[1], "PWM2_VAL", "B", "%3.0f", 0, 100, 10, 0);
    IUFillNumberVector(&PWMNP, PWMN, 2, getDeviceName(), "PWM", "PWM", POWER_TAB, IP_RW, 60, IPS_IDLE);

    // Auto pwm
    IUFillSwitch(&AutoPWMDefaultOnS[0], "PWMA_A_DEF_ON", "A", ISS_OFF);
    IUFillSwitch(&AutoPWMDefaultOnS[1], "PWMA_B_DEF_ON", "B", ISS_OFF);
    IUFillSwitchVector(&AutoPWMDefaultOnSP, AutoPWMDefaultOnS, 2, getDeviceName(), "AUTO_PWM_DEF_ON", "Auto PWM default ON",
                       SETTINGS_TAB,
                       IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    ISState pwmAutoA = ISS_OFF;
    IUGetConfigSwitch(getDeviceName(), AutoPWMDefaultOnSP.name, AutoPWMDefaultOnS[0].name, &pwmAutoA);

    ISState pwmAutoB = ISS_OFF;
    IUGetConfigSwitch(getDeviceName(), AutoPWMDefaultOnSP.name, AutoPWMDefaultOnS[1].name, &pwmAutoB);

    IUFillSwitch(&AutoPWMS[0], "PWMA_A", "A", pwmAutoA);
    IUFillSwitch(&AutoPWMS[1], "PWMA_B", "B", pwmAutoB);
    IUFillSwitchVector(&AutoPWMSP, AutoPWMS, 2, getDeviceName(), "AUTO_PWM", "Auto PWM", POWER_TAB, IP_RW, ISR_NOFMANY, 60,
                       IPS_OK);

    IUFillNumber(&PowerDataN[POW_VIN], "VIN", "Input voltage", "%.1f", 0, 15, 10, 0);
    IUFillNumber(&PowerDataN[POW_VREG], "VREG", "Regulated voltage", "%.1f", 0, 15, 10, 0);
    IUFillNumber(&PowerDataN[POW_ITOT], "ITOT", "Total current", "%.1f", 0, 15, 10, 0);
    IUFillNumber(&PowerDataN[POW_AH], "AH", "Energy consumed [Ah]", "%.1f", 0, 1000, 10, 0);
    IUFillNumber(&PowerDataN[POW_WH], "WH", "Energy consumed [Wh]", "%.1f", 0, 10000, 10, 0);
    IUFillNumberVector(&PowerDataNP, PowerDataN, 5, getDeviceName(), "POWER_DATA", "Power data", POWER_TAB, IP_RO, 60,
                       IPS_IDLE);

    // Environment Group
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -15, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15);

    // Sensor 2
    IUFillNumber(&Sensor2N[0], "TEMP_2", "Temperature (C)", "%.1f", -50, 100, 1, 0);
    IUFillNumberVector(&Sensor2NP, Sensor2N, 1, getDeviceName(), "SENSOR_2", "Sensor 2", ENVIRONMENT_TAB, IP_RO, 60, IPS_IDLE);

    // DC focuser
    IUFillNumber(&DCFocTimeN[DC_PERIOD], "DC_PERIOD", "Time [ms]", "%.0f", 10, 5000, 10, 500);
    IUFillNumber(&DCFocTimeN[DC_PWM], "DC_PWM", "PWM [%]", "%.0f", 10, 100, 10, 50);
    IUFillNumberVector(&DCFocTimeNP, DCFocTimeN, 2, getDeviceName(), "DC_FOC_TIME", "DC Focuser", DCFOCUSER_TAB, IP_RW, 60,
                       IPS_OK);

    IUFillSwitch(&DCFocDirS[0], "DIR_IN", "IN", ISS_OFF);
    IUFillSwitch(&DCFocDirS[1], "DIR_OUT", "OUT", ISS_ON);
    IUFillSwitchVector(&DCFocDirSP, DCFocDirS, 2, getDeviceName(), "DC_FOC_DIR", "DC Focuser direction", DCFOCUSER_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_OK);

    IUFillSwitch(&DCFocAbortS[0], "DC_FOC_ABORT", "STOP", ISS_OFF);
    IUFillSwitchVector(&DCFocAbortSP, DCFocAbortS, 1, getDeviceName(), "DC_FOC_ABORT", "DC Focuser stop", DCFOCUSER_TAB, IP_RW,
                       ISR_ATMOST1, 60, IPS_IDLE);

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    serialConnection->setDefaultPort("/dev/ttyUSB0");
    serialConnection->setDefaultBaudRate(serialConnection->B_115200);

    return true;
}

bool IndiAstrolink4::updateProperties()
{
    // Call parent update properties first
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(&FocusPosMMNP);
        FI::updateProperties();
        WI::updateProperties();
        defineProperty(&Power1SP);
        defineProperty(&Power2SP);
        defineProperty(&Power3SP);
        defineProperty(&AutoPWMSP);
        defineProperty(&Sensor2NP);
        defineProperty(&PWMNP);
        defineProperty(&PowerDataNP);
        defineProperty(&FocuserSettingsNP);
        defineProperty(&FocuserModeSP);
        defineProperty(&FocuserCompModeSP);
        defineProperty(&FocuserManualSP);
        defineProperty(&CompensationValueNP);
        defineProperty(&CompensateNowSP);
        defineProperty(&PowerDefaultOnSP);
        defineProperty(&AutoPWMDefaultOnSP);
        defineProperty(&OtherSettingsNP);
        defineProperty(&DCFocDirSP);
        defineProperty(&DCFocTimeNP);
        defineProperty(&DCFocAbortSP);
        defineProperty(&PowerControlsLabelsTP);
        defineProperty(&BuzzerSP);
    }
    else
    {
        deleteProperty(Power1SP.name);
        deleteProperty(Power2SP.name);
        deleteProperty(Power3SP.name);
        deleteProperty(AutoPWMSP.name);
        deleteProperty(Sensor2NP.name);
        deleteProperty(PWMNP.name);
        deleteProperty(PowerDataNP.name);
        deleteProperty(FocuserSettingsNP.name);
        deleteProperty(FocuserModeSP.name);
        deleteProperty(CompensateNowSP.name);
        deleteProperty(CompensationValueNP.name);
        deleteProperty(PowerDefaultOnSP.name);
        deleteProperty(AutoPWMDefaultOnSP.name);
        deleteProperty(OtherSettingsNP.name);
        deleteProperty(DCFocTimeNP.name);
        deleteProperty(DCFocDirSP.name);
        deleteProperty(DCFocAbortSP.name);
        deleteProperty(BuzzerSP.name);
        deleteProperty(FocuserCompModeSP.name);
        deleteProperty(FocuserManualSP.name);
        deleteProperty(FocusPosMMNP.name);
        deleteProperty(PowerControlsLabelsTP.name);
        FI::updateProperties();
        WI::updateProperties();
    }

    return true;
}

bool IndiAstrolink4::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        char cmd[ASTROLINK4_LEN] = {0};
        char res[ASTROLINK4_LEN] = {0};

        // handle PWM
        if (!strcmp(name, PWMNP.name))
        {
            bool allOk = true;
            if(PWMN[0].value != values[0])
            {
                if(AutoPWMS[0].s == ISS_OFF)
                {
                    sprintf(cmd, "B:0:%d", static_cast<uint8_t>(values[0]));
                    allOk = allOk && sendCommand(cmd, res);
                }
                else
                {
                    LOG_WARN("Cannot set PWM output, it is in AUTO mode.");
                }
            }
            if(PWMN[1].value != values[1])
            {
                if(AutoPWMS[1].s == ISS_OFF)
                {
                    sprintf(cmd, "B:1:%d", static_cast<uint8_t>(values[1]));
                    allOk = allOk && sendCommand(cmd, res);
                }
                else
                {
                    LOG_WARN("Cannot set PWM output, it is in AUTO mode.");
                }
            }
            PWMNP.s = (allOk) ? IPS_BUSY : IPS_ALERT;
            if(allOk)
                IUUpdateNumber(&PWMNP, values, names, n);
            IDSetNumber(&PWMNP, nullptr);
            IDSetSwitch(&AutoPWMSP, nullptr);
            return true;
        }

        // Focuser settings
        if(!strcmp(name, FocuserSettingsNP .name))
        {
            bool allOk = true;
            std::map<int, std::string> updates;
            updates[U_SPEED] = doubleToStr(values[FS_SPEED]);
            updates[U_ACC] = doubleToStr(values[FS_SPEED] * 2.0);
            updates[U_STEPSIZE] = doubleToStr(values[FS_STEP_SIZE] * 100.0);
            allOk = allOk && updateSettings("u", "U", updates);
            updates.clear();
            updates[E_COMP_CYCLE] = "30";  // cycle [s]
            updates[E_COMP_STEPS] = doubleToStr(values[FS_COMPENSATION] * 100.0);
            updates[E_COMP_SENSR] = "0";   // sensor
            updates[E_COMP_TRGR] = doubleToStr(values[FS_COMP_THRESHOLD]);
            allOk = allOk && updateSettings("e", "E", updates);
            if(allOk)
            {
                FocuserSettingsNP.s = IPS_BUSY;
                IUUpdateNumber(&FocuserSettingsNP, values, names, n);
                IDSetNumber(&FocuserSettingsNP, nullptr);
                LOG_INFO(values[FS_COMPENSATION] > 0 ? "Temperature compensation is enabled." : "Temperature compensation is disabled.");
                return true;
            }
            FocuserSettingsNP.s = IPS_ALERT;
            return true;
        }

        // Other settings
        if(!strcmp(name, OtherSettingsNP .name))
        {
            std::map<int, std::string> updates;
            updates[N_AREF_COEFF] = doubleToStr(values[SET_AREF_COEFF] * 1000.0);
            updates[N_OVER_VOLT] = doubleToStr(values[SET_OVER_VOLT] * 10.0);
            updates[N_OVER_AMP] = doubleToStr(values[SET_OVER_AMP] * 10.0);
            updates[N_OVER_TIME] = doubleToStr(values[SET_OVER_TIME]);
            if(updateSettings("n", "N", updates))
            {
                OtherSettingsNP.s = IPS_BUSY;
                IUUpdateNumber(&OtherSettingsNP, values, names, n);
                IDSetNumber(&OtherSettingsNP, nullptr);
                return true;
            }
            OtherSettingsNP.s = IPS_ALERT;
            return true;
        }

        // DC Focuser
        if(!strcmp(name, DCFocTimeNP.name))
        {
            IUUpdateNumber(&DCFocTimeNP, values, names, n);
            IDSetNumber(&DCFocTimeNP, nullptr);
            saveConfig(true);
            sprintf(cmd, "G:%d:%.0f:%.0f", (DCFocDirS[0].s == ISS_ON) ? 1 : 0, DCFocTimeN[DC_PWM].value, DCFocTimeN[DC_PERIOD].value);
            if(sendCommand(cmd, res))
            {
                DCFocAbortS[0].s = ISS_OFF;
                DCFocAbortSP.s = IPS_OK;
                IDSetSwitch(&DCFocAbortSP, nullptr);

                DCFocTimeNP.s = IPS_BUSY;
                return true;
            }
            DCFocTimeNP.s = IPS_ALERT;
            return true;
        }

        if (strstr(name, "FOCUS_"))
            return FI::processNumber(dev, name, values, names, n);
        if (strstr(name, "WEATHER_"))
            return WI::processNumber(dev, name, values, names, n);
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}


bool IndiAstrolink4::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
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
            if(allOk)
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
            if(allOk)
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
            if(allOk)
                IUUpdateSwitch(&Power3SP, states, names, n);

            IDSetSwitch(&Power3SP, nullptr);
            return true;
        }

        // compensate now
        if(!strcmp(name, CompensateNowSP.name))
        {
            sprintf(cmd, "S:%d", static_cast<uint8_t>(CompensationValueN[0].value));
            bool allOk = sendCommand(cmd, res);
            CompensateNowSP.s = allOk ? IPS_BUSY : IPS_ALERT;
            if(allOk)
                IUUpdateSwitch(&CompensateNowSP, states, names, n);

            IDSetSwitch(&CompensateNowSP, nullptr);
            return true;
        }

        // Auto PWM
        if (!strcmp(name, AutoPWMSP.name))
        {
            IUUpdateSwitch(&AutoPWMSP, states, names, n);
            AutoPWMSP.s = (setAutoPWM()) ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&AutoPWMSP, nullptr);
            return true;
        }

        // DC Focuser
        if (!strcmp(name, DCFocDirSP.name))
        {
            DCFocDirSP.s = IPS_OK;
            IUUpdateSwitch(&DCFocDirSP, states, names, n);
            IDSetSwitch(&DCFocDirSP, nullptr);
            return true;
        }

        if (!strcmp(name, DCFocAbortSP.name))
        {
            sprintf(cmd, "%s", "K");
            if(sendCommand(cmd, res))
            {
                DCFocAbortSP.s = IPS_BUSY;
                IUUpdateSwitch(&DCFocAbortSP, states, names, n);
                IDSetSwitch(&DCFocAbortSP, nullptr);
            }
            DCFocAbortSP.s = IPS_ALERT;
            return true;
        }

        // Power default on
        if(!strcmp(name, PowerDefaultOnSP.name))
        {
            std::map<int, std::string> updates;
            updates[U_OUT1_DEF] = (states[0] == ISS_ON) ? "1" : "0";
            updates[U_OUT2_DEF] = (states[1] == ISS_ON) ? "1" : "0";
            updates[U_OUT3_DEF] = (states[2] == ISS_ON) ? "1" : "0";
            if(updateSettings("u", "U", updates))
            {
                PowerDefaultOnSP.s = IPS_BUSY;
                IUUpdateSwitch(&PowerDefaultOnSP, states, names, n);
                IDSetSwitch(&PowerDefaultOnSP, nullptr);
                return true;
            }
            PowerDefaultOnSP.s = IPS_ALERT;
            return true;
        }

        // Auto PWM default on
        if (!strcmp(name, AutoPWMDefaultOnSP.name))
        {
            IUUpdateSwitch(&AutoPWMDefaultOnSP, states, names, n);
            AutoPWMDefaultOnSP.s = IPS_OK;
            saveConfig();
            IDSetSwitch(&AutoPWMDefaultOnSP, nullptr);
            return true;
        }

        // Buzzer
        if(!strcmp(name, BuzzerSP.name))
        {
            if(updateSettings("j", "J", 1, (states[0] == ISS_ON) ? "1" : "0"))
            {
                BuzzerSP.s = IPS_BUSY;
                IUUpdateSwitch(&BuzzerSP, states, names, n);
                IDSetSwitch(&BuzzerSP, nullptr);
                return true;
            }
            BuzzerSP.s = IPS_ALERT;
            return true;
        }

        // Manual mode
        if(!strcmp(name, FocuserManualSP.name))
        {
            sprintf(cmd, "F:%s", (strcmp(FocuserManualS[0].name, names[0])) ? "0" : "1");
            if(sendCommand(cmd, res))
            {
                FocuserManualSP.s = IPS_BUSY;
                IUUpdateSwitch(&FocuserManualSP, states, names, n);
                IDSetSwitch(&FocuserManualSP, nullptr);
                return true;
            }
            FocuserManualSP.s = IPS_ALERT;
            return true;
        }

        // Focuser Mode
        if(!strcmp(name, FocuserModeSP.name))
        {
            std::string value = "0";
            if(!strcmp(FocuserModeS[FS_MODE_UNI].name, names[0])) value = "0";
            if(!strcmp(FocuserModeS[FS_MODE_BI].name, names[0])) value = "1";
            if(!strcmp(FocuserModeS[FS_MODE_MICRO].name, names[0])) value = "2";
            if(updateSettings("u", "U", U_STEPPER_MODE, value.c_str()))
            {
                FocuserModeSP.s = IPS_BUSY;
                IUUpdateSwitch(&FocuserModeSP, states, names, n);
                IDSetSwitch(&FocuserModeSP, nullptr);
                return true;
            }
            FocuserModeSP.s = IPS_ALERT;
            return true;
        }

        // Focuser compensation mode
        if(!strcmp(name, FocuserCompModeSP.name))
        {
            std::string value = "0";
            if(!strcmp(FocuserCompModeS[FS_COMP_AUTO].name, names[0])) value = "1";
            if(updateSettings("e", "E", E_COMP_AUTO, value.c_str()))
            {
                FocuserCompModeSP.s = IPS_BUSY;
                IUUpdateSwitch(&FocuserCompModeSP, states, names, n);
                IDSetSwitch(&FocuserCompModeSP, nullptr);
                return true;
            }
            FocuserCompModeSP.s = IPS_ALERT;
            return true;
        }

        if (strstr(name, "FOCUS"))
            return FI::processSwitch(dev, name, states, names, n);

    }

    return INDI::DefaultDevice::ISNewSwitch (dev, name, states, names, n);
}

bool IndiAstrolink4::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Power Labels
        if (!strcmp(name, PowerControlsLabelsTP.name))
        {
            IUUpdateText(&PowerControlsLabelsTP, texts, names, n);
            PowerControlsLabelsTP.s = IPS_OK;
            LOG_INFO("Power port labels saved. Driver must be restarted for the labels to take effect.");
            saveConfig();
            IDSetText(&PowerControlsLabelsTP, nullptr);
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}


bool IndiAstrolink4::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    FI::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &DCFocTimeNP);
    IUSaveConfigSwitch(fp, &DCFocDirSP);
    IUSaveConfigText(fp, &PowerControlsLabelsTP);
    IUSaveConfigSwitch(fp, &AutoPWMDefaultOnSP);
    return true;
}

//////////////////////////////////////////////////////////////////////
/// PWM outputs
//////////////////////////////////////////////////////////////////////
bool IndiAstrolink4::setAutoPWM()
{
    char cmd[ASTROLINK4_LEN] = {0}, res[ASTROLINK4_LEN] = {0};
    bool allOk = true;

    uint8_t valA = (AutoPWMS[0].s == ISS_ON) ? 255 : static_cast<uint8_t>(PWMN[0].value);
    uint8_t valB = (AutoPWMS[1].s == ISS_ON) ? 255 : static_cast<uint8_t>(PWMN[1].value);

    snprintf(cmd, ASTROLINK4_LEN, "B:0:%d", valA);
    allOk = allOk && sendCommand(cmd, res);
    snprintf(cmd, ASTROLINK4_LEN, "B:1:%d", valB);
    allOk = allOk && sendCommand(cmd, res);

    return allOk;
}

//////////////////////////////////////////////////////////////////////
/// Focuser interface
//////////////////////////////////////////////////////////////////////
IPState IndiAstrolink4::MoveAbsFocuser(uint32_t targetTicks)
{
    int32_t backlash = 0;
    if(backlashEnabled)
    {
        if((targetTicks > FocusAbsPosNP[0].getValue()) == (backlashSteps > 0))
        {
            if(((int32_t)targetTicks + backlash) < 0 || (targetTicks + backlash) > FocusMaxPosNP[0].getValue())
            {
                backlash = 0;
            }
            else
            {
                backlash = backlashSteps;
                requireBacklashReturn = true;
            }
        }
    }
    char cmd[ASTROLINK4_LEN] = {0}, res[ASTROLINK4_LEN] = {0};
    snprintf(cmd, ASTROLINK4_LEN, "R:0:%u", targetTicks + backlash);
    return (sendCommand(cmd, res)) ? IPS_BUSY : IPS_ALERT;
}

IPState IndiAstrolink4::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    return MoveAbsFocuser(dir == FOCUS_INWARD ? FocusAbsPosNP[0].getValue() - ticks : FocusAbsPosNP[0].getValue() + ticks);
}

bool IndiAstrolink4::AbortFocuser()
{
    char res[ASTROLINK4_LEN] = {0};
    return (sendCommand("H", res));
}

bool IndiAstrolink4::ReverseFocuser(bool enabled)
{
    return updateSettings("u", "U", U_REVERSED, (enabled) ? "1" : "0");
}

bool IndiAstrolink4::SyncFocuser(uint32_t ticks)
{
    char cmd[ASTROLINK4_LEN] = {0}, res[ASTROLINK4_LEN] = {0};
    snprintf(cmd, ASTROLINK4_LEN, "P:0:%u", ticks);
    return sendCommand(cmd, res);
}

bool IndiAstrolink4::SetFocuserMaxPosition(uint32_t ticks)
{
    if(updateSettings("u", "U", U_MAX_POS, std::to_string(ticks).c_str()))
    {
        FocuserSettingsNP.s = IPS_BUSY;
        return true;
    }
    else
    {
        return false;
    }
}

bool IndiAstrolink4::SetFocuserBacklash(int32_t steps)
{
    backlashSteps = steps;
    return true;
}

bool IndiAstrolink4::SetFocuserBacklashEnabled(bool enabled)
{
    backlashEnabled = enabled;
    return true;
}

//////////////////////////////////////////////////////////////////////
/// Serial commands
//////////////////////////////////////////////////////////////////////
bool IndiAstrolink4::sendCommand(const char * cmd, char * res)
{
    int nbytes_read = 0, nbytes_written = 0, tty_rc = 0;
    char command[ASTROLINK4_LEN];

    if(isSimulation())
    {
        if(strcmp(cmd, "#") == 0) sprintf(res, "%s\n", "#:AstroLink4mini");
        if(strcmp(cmd, "q") == 0) sprintf(res, "%s\n",
                                              "q:1234:0:1.47:1:2.12:45.1:-12.81:1:-25.22:45:0:0:0:1:12.1:5.0:1.12:13.41:0:34:0:0");
        if(strcmp(cmd, "p") == 0) sprintf(res, "%s\n", "p:1234");
        if(strcmp(cmd, "i") == 0) sprintf(res, "%s\n", "i:0");
        if(strcmp(cmd, "n") == 0) sprintf(res, "%s\n", "n:1077:14.0:10.0:100");
        if(strcmp(cmd, "e") == 0) sprintf(res, "%s\n", "e:30:1200:1:0:20");
        if(strcmp(cmd, "u") == 0) sprintf(res, "%s\n", "u:25000:220:0:100:440:0:0:1:257:0:0:0:0:0:1:0:0");
        if(strncmp(cmd, "R", 1) == 0) sprintf(res, "%s\n", "R:");
        if(strncmp(cmd, "C", 1) == 0) sprintf(res, "%s\n", "C:");
        if(strncmp(cmd, "B", 1) == 0) sprintf(res, "%s\n", "B:");
        if(strncmp(cmd, "H", 1) == 0) sprintf(res, "%s\n", "H:");
        if(strncmp(cmd, "P", 1) == 0) sprintf(res, "%s\n", "P:");
        if(strncmp(cmd, "U", 1) == 0) sprintf(res, "%s\n", "U:");
        if(strncmp(cmd, "S", 1) == 0) sprintf(res, "%s\n", "S:");
        if(strncmp(cmd, "G", 1) == 0) sprintf(res, "%s\n", "G:");
        if(strncmp(cmd, "K", 1) == 0) sprintf(res, "%s\n", "K:");
        if(strncmp(cmd, "N", 1) == 0) sprintf(res, "%s\n", "N:");
        if(strncmp(cmd, "E", 1) == 0) sprintf(res, "%s\n", "E:");
    }
    else
    {
        tcflush(PortFD, TCIOFLUSH);
        sprintf(command, "%s\n", cmd);
        LOGF_DEBUG("CMD %s", command);
        if ( (tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
            return false;

        if (!res)
        {
            tcflush(PortFD, TCIOFLUSH);
            return true;
        }

        if ( (tty_rc = tty_nread_section(PortFD, res, ASTROLINK4_LEN, stopChar, ASTROLINK4_TIMEOUT, &nbytes_read)) != TTY_OK
                || nbytes_read == 1)
            return false;

        tcflush(PortFD, TCIOFLUSH);
        res[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES %s", res);

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

//////////////////////////////////////////////////////////////////////
/// Sensors
//////////////////////////////////////////////////////////////////////
bool IndiAstrolink4::sensorRead()
{
    char res[ASTROLINK4_LEN] = {0};
    if (sendCommand("q", res))
    {
        std::vector<std::string> result = split(res, ":");

        float focuserPosition = std::stod(result[Q_STEPPER_POS]);
        FocusAbsPosNP[0].setValue(focuserPosition);
        FocusPosMMNP[0].setValue(focuserPosition * FocuserSettingsN[FS_STEP_SIZE].value / 1000.0);
        float stepsToGo = std::stod(result[Q_STEPS_TO_GO]);
        if(stepsToGo == 0)
        {
            if(requireBacklashReturn)
            {
                requireBacklashReturn = false;
                MoveAbsFocuser(focuserPosition - backlashSteps);
            }
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusPosMMNP.s = IPS_OK;
            FocusRelPosNP.apply();
        }
        else
        {
            FocusAbsPosNP.setState(IPS_BUSY);
            FocusRelPosNP.setState(IPS_BUSY);
            FocusPosMMNP.s = IPS_BUSY;
        }
        IDSetNumber(&FocusPosMMNP, nullptr);
        FocusAbsPosNP.apply();
        PowerDataN[POW_ITOT].value = std::stod(result[Q_CURRENT]);

        if(result.size() > 5)
        {
            if(std::stod(result[Q_SENS1_TYPE]) > 0)
            {
                setParameterValue("WEATHER_TEMPERATURE", std::stod(result[Q_SENS1_TEMP]));
                setParameterValue("WEATHER_HUMIDITY", std::stod(result[Q_SENS1_HUM]));
                setParameterValue("WEATHER_DEWPOINT", std::stod(result[Q_SENS1_DEW]));
                ParametersNP.setState(IPS_OK);
                ParametersNP.apply();
            }
            else
            {
                ParametersNP.setState(IPS_IDLE);
            }

            if(std::stod(result[Q_SENS2_TYPE]) > 0)
            {
                Sensor2N[0].value = std::stod(result[Q_SENS2_TEMP]);
                Sensor2NP.s = IPS_OK;
                IDSetNumber(&Sensor2NP, nullptr);
            }
            else
            {
                Sensor2NP.s = IPS_IDLE;
            }

            PWMN[0].value = std::stod(result[Q_PWM1]);
            PWMN[1].value = std::stod(result[Q_PWM2]);
            PWMNP.s = IPS_OK;
            IDSetNumber(&PWMNP, nullptr);

            bool dcMotorMoving = (std::stod(result[Q_DC_MOVE]) > 0);
            if(dcMotorMoving)
            {
                DCFocTimeNP.s = IPS_BUSY;
                IDSetNumber(&DCFocTimeNP, nullptr);
            }
            else if (DCFocTimeNP.s == IPS_BUSY)
            {
                DCFocTimeNP.s = IPS_OK;
                DCFocAbortSP.s = IPS_IDLE;
                IDSetNumber(&DCFocTimeNP, nullptr);
                IDSetSwitch(&DCFocAbortSP, nullptr);
            }

            if(Power1SP.s != IPS_OK || Power2SP.s != IPS_OK || Power3SP.s != IPS_OK)
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

            CompensationValueN[0].value = std::stod(result[Q_COMP_DIFF]);
            CompensateNowSP.s = CompensationValueNP.s = (CompensationValueN[0].value > 0) ? IPS_OK : IPS_IDLE;
            CompensateNowS[0].s = (CompensationValueN[0].value > 0) ? ISS_OFF : ISS_ON;
            IDSetNumber(&CompensationValueNP, nullptr);
            IDSetSwitch(&CompensateNowSP, nullptr);

            PowerDataN[POW_VIN].value = std::stod(result[Q_VIN]);
            PowerDataN[POW_VREG].value = std::stod(result[Q_VREG]);
            PowerDataN[POW_AH].value = std::stod(result[Q_AH]);
            PowerDataN[POW_WH].value = std::stod(result[Q_WH]);

            if(strcmp(result[Q_OP_FLAG].c_str(), "0"))
            {
                int opFlag = std::stoi(result[Q_OP_FLAG]);
                LOGF_WARN("Protection triggered, outputs were disabled. Reason: %s was too high, value: %.1f",
                          (opFlag == 1) ? "voltage" : "current", std::stod(result[Q_OP_VALUE]));
            }
        }

        PowerDataNP.s = IPS_OK;
        IDSetNumber(&PowerDataNP, nullptr);

    }

    // update settings data if was changed
    if(FocuserSettingsNP.s != IPS_OK || FocuserModeSP.s != IPS_OK || PowerDefaultOnSP.s != IPS_OK || BuzzerSP.s != IPS_OK
            || FocuserCompModeSP.s != IPS_OK)
    {
        if (sendCommand("u", res))
        {
            std::vector<std::string> result = split(res, ":");

            FocuserModeS[FS_MODE_UNI].s = FocuserModeS[FS_MODE_BI].s = FocuserModeS[FS_MODE_MICRO].s = ISS_OFF;
            if(!strcmp("0", result[U_STEPPER_MODE].c_str())) FocuserModeS[FS_MODE_UNI].s = ISS_ON;
            if(!strcmp("1", result[U_STEPPER_MODE].c_str())) FocuserModeS[FS_MODE_BI].s = ISS_ON;
            if(!strcmp("2", result[U_STEPPER_MODE].c_str())) FocuserModeS[FS_MODE_MICRO].s = ISS_ON;
            FocuserModeSP.s = IPS_OK;
            IDSetSwitch(&FocuserModeSP, nullptr);

            PowerDefaultOnS[0].s = (std::stod(result[U_OUT1_DEF]) > 0) ? ISS_ON : ISS_OFF;
            PowerDefaultOnS[1].s = (std::stod(result[U_OUT2_DEF]) > 0) ? ISS_ON : ISS_OFF;
            PowerDefaultOnS[2].s = (std::stod(result[U_OUT3_DEF]) > 0) ? ISS_ON : ISS_OFF;
            PowerDefaultOnSP.s = IPS_OK;
            IDSetSwitch(&PowerDefaultOnSP, nullptr);

            FocuserSettingsN[FS_SPEED].value = std::stod(result[U_SPEED]);
            FocuserSettingsN[FS_STEP_SIZE].value = std::stod(result[U_STEPSIZE]) / 100.0;
            FocusMaxPosNP[0].setValue(std::stod(result[U_MAX_POS]));
            FocuserSettingsNP.s = IPS_OK;
            IDSetNumber(&FocuserSettingsNP, nullptr);
            FocusMaxPosNP.apply();
        }

        if(sendCommand("j", res))
        {
            std::vector<std::string> result = split(res, ":");
            BuzzerS[0].s = (std::stod(result[1]) > 0) ? ISS_ON : ISS_OFF;
            BuzzerSP.s = IPS_OK;
            IDSetSwitch(&BuzzerSP, nullptr);
        }

        if (sendCommand("e", res))
        {
            std::vector<std::string> result = split(res, ":");
            FocuserSettingsN[FS_COMPENSATION].value = std::stod(result[E_COMP_STEPS]) / 100.0;
            FocuserSettingsN[FS_COMP_THRESHOLD].value = std::stod(result[E_COMP_TRGR]);
            FocuserSettingsNP.s = IPS_OK;
            IDSetNumber(&FocuserSettingsNP, nullptr);

            FocuserCompModeS[FS_COMP_MANUAL].s = (std::stod(result[E_COMP_AUTO]) == 0) ? ISS_ON : ISS_OFF;
            FocuserCompModeS[FS_COMP_AUTO].s = (std::stod(result[E_COMP_AUTO]) > 0) ? ISS_ON : ISS_OFF;
            FocuserCompModeSP.s = IPS_OK;
            IDSetSwitch(&FocuserCompModeSP, nullptr);
        }
    }

    if(FocuserManualSP.s != IPS_OK)
    {
        if (sendCommand("f", res))
        {
            std::vector<std::string> result = split(res, ":");
            FocuserManualS[FS_MANUAL_OFF].s = (std::stod(result[1]) == 0) ? ISS_ON : ISS_OFF;
            FocuserManualS[FS_MANUAL_ON].s = (std::stod(result[1]) > 0) ? ISS_ON : ISS_OFF;
            FocuserManualSP.s = IPS_OK;
            IDSetSwitch(&FocuserManualSP, nullptr);
        }
    }

    if(OtherSettingsNP.s != IPS_OK)
    {
        if (sendCommand("n", res))
        {
            std::vector<std::string> result = split(res, ":");
            OtherSettingsN[SET_AREF_COEFF].value = std::stod(result[N_AREF_COEFF]) / 1000.0;
            OtherSettingsN[SET_OVER_TIME].value = std::stod(result[N_OVER_TIME]);
            OtherSettingsN[SET_OVER_VOLT].value = std::stod(result[N_OVER_VOLT]) / 10.0;
            OtherSettingsN[SET_OVER_AMP].value = std::stod(result[N_OVER_AMP]) / 10.0;
            OtherSettingsNP.s = IPS_OK;
            IDSetNumber(&OtherSettingsNP, nullptr);
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
/// Helper functions
//////////////////////////////////////////////////////////////////////
std::vector<std::string> IndiAstrolink4::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

std::string IndiAstrolink4::doubleToStr(double val)
{
    char buf[10];
    sprintf(buf, "%.0f", val);
    return std::string(buf);
}

bool IndiAstrolink4::updateSettings(const char * getCom, const char * setCom, int index, const char * value)
{
    std::map<int, std::string> values;
    values[index] = value;
    return updateSettings(getCom, setCom, values);
}

bool IndiAstrolink4::updateSettings(const char * getCom, const char * setCom, std::map<int, std::string> values)
{
    char cmd[ASTROLINK4_LEN] = {0}, res[ASTROLINK4_LEN] = {0};
    snprintf(cmd, ASTROLINK4_LEN, "%s", getCom);
    if(sendCommand(cmd, res))
    {
        std::string concatSettings = "";
        std::vector<std::string> result = split(res, ":");
        if(result.size() >= values.size())
        {
            result[0] = setCom;
            for(std::map<int, std::string>::iterator it = values.begin(); it != values.end(); ++it)
                result[it->first] = it->second;

            for (const auto &piece : result) concatSettings += piece + ":";
            snprintf(cmd, ASTROLINK4_LEN, "%s", concatSettings.c_str());
            if(sendCommand(cmd, res)) return true;
        }
    }
    return false;
}
