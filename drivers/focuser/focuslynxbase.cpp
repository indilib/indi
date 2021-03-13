/*
  Focus Lynx INDI driver
  Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "focuslynxbase.h"

/************************************************************************************
 *
* ***********************************************************************************/
FocusLynxBase::FocusLynxBase(const char *target)
{
    INDI_UNUSED(target);
}

/************************************************************************************
 *
* ***********************************************************************************/
FocusLynxBase::FocusLynxBase()
{
    setVersion(VERSION, SUBVERSION);

    lynxModels["Optec TCF-Lynx 2"] = "OA";
    lynxModels["Optec TCF-Lynx 3"] = "OB";
    lynxModels["Optec TCF-Lynx 2 with Extended Travel"] = "OC";
    lynxModels["Optec Fast Focus Secondary Focuser"] = "OD";
    lynxModels["Optec TCF-S Classic converted"] = "OE";
    lynxModels["Optec TCF-S3 Classic converted"] = "OF";
    //  lynxModels["Optec Gemini (reserved for future use)"] = "OG";
    lynxModels["Optec Leo"] = "OI";
    lynxModels["Optec Leo High-Torque"] = "OJ";
    lynxModels["Optec Sagitta"] = "OK";
    lynxModels["FocusLynx QuickSync FT Hi-Torque"] = "FA";
    lynxModels["FocusLynx QuickSync FT Hi-Speed"] = "FB";

    //  lynxModels["FocusLynx QuickSync SV (reserved for future use)"] = "FC";
    lynxModels["DirectSync TEC with bipolar motor - higher speed"] = "FD";
    lynxModels["FocusLynx QuickSync  Long Travel Hi-Torque"] = "FE";
    lynxModels["FocusLynx QuickSync Long Travel Hi-Speed"] = "FF";

    // JM 2019-09-27: This was added after the discussion here
    // https://www.indilib.org/forum/focusers-filter-wheels/5739-starlight-instruments-focuser-boss-ii-hsm20.html
    lynxModels["FeatureTouch HSM Hi-Torque"] = "FA";
    lynxModels["FeatureTouch HSM Hi-Speed"] = "FB";
    lynxModels["FeatherTouch Motor PDMS"] = "FE";
    lynxModels["FeatherTouch Motor Hi-Speed"] = "SO";
    lynxModels["FeatherTouch Motor Hi-Torque"] = "SP";
    lynxModels["Starlight Instruments - FTM with MicroTouch"] = "SQ";
    lynxModels["Televue Focuser"] = "TA";

    ModelS = nullptr;

    focusMoveRequest = 0;
    simPosition      = 0;

    // Can move in Absolute & Relative motions, can AbortFocuser motion, can sync, and has variable speed.
    FI::SetCapability(FOCUSER_CAN_ABORT    |
                      FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_SYNC     |
                      FOCUSER_CAN_REVERSE  |
                      FOCUSER_HAS_BACKLASH);

    isAbsolute = false;
    isSynced   = false;
    isHoming   = false;

    simStatus[STATUS_MOVING]   = ISS_OFF;
    simStatus[STATUS_HOMING]   = ISS_OFF;
    simStatus[STATUS_HOMED]    = ISS_OFF;
    simStatus[STATUS_FFDETECT] = ISS_OFF;
    simStatus[STATUS_TMPPROBE] = ISS_ON;
    simStatus[STATUS_REMOTEIO] = ISS_ON;
    simStatus[STATUS_HNDCTRL]  = ISS_ON;
    simStatus[STATUS_REVERSE]  = ISS_OFF;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Enable/Disable temperature compensation
    TemperatureCompensateSP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    TemperatureCompensateSP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_ON);
    TemperatureCompensateSP.fill(getDeviceName(), "T. COMPENSATION", "T. Compensation",
                       FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Enable/Disable temperature compensation on start
    TemperatureCompensateOnStartSP[0].fill("Enable", "Enable", ISS_OFF);
    TemperatureCompensateOnStartSP[1].fill("Disable", "Disable", ISS_ON);
    TemperatureCompensateOnStartSP.fill(getDeviceName(),
                       "T. COMPENSATION @START", "T. Compensation @Start", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Enable/Disable temperature Mode
    TemperatureCompensateModeSP[0].fill("A", "A", ISS_OFF);
    TemperatureCompensateModeSP[1].fill("B", "B", ISS_OFF);
    TemperatureCompensateModeSP[2].fill("C", "C", ISS_OFF);
    TemperatureCompensateModeSP[3].fill("D", "D", ISS_OFF);
    TemperatureCompensateModeSP[4].fill("E", "E", ISS_OFF);
    TemperatureCompensateModeSP.fill(getDeviceName(), "COMPENSATE MODE",
                       "Compensate Mode", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    TemperatureParamNP[0].fill("T. Coefficient", "", "%.f", -9999, 9999, 100., 0.);
    TemperatureParamNP[1].fill("T. Intercept", "", "%.f", -32766, 32766, 100., 0.);
    TemperatureParamNP.fill(getDeviceName(), "T. PARAMETERS", "Mode Parameters",
                       FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Enable/Disable Sync Mandatory for relative focuser
    SyncMandatorySP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", isSynced == false ? ISS_ON : ISS_OFF);
    SyncMandatorySP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", isSynced == true ? ISS_ON : ISS_OFF);
    SyncMandatorySP.fill(getDeviceName(), "SYNC MANDATORY", "Sync Mandatory",
                       FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Focuser Step Size
    StepSizeNP[0].fill("10000*microns/step", "", "%.f", 0, 65535, 0., 0);
    StepSizeNP.fill(getDeviceName(), "STEP SIZE", "Step Size", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Reset to Factory setting
    ResetSP[0].fill("Factory", "Factory", ISS_OFF);
    ResetSP.fill(getDeviceName(), "RESET", "Reset", FOCUS_SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0,
                       IPS_IDLE);

    // Go to home/center
    GotoSP[GOTO_CENTER].fill("Center", "Center", ISS_OFF);
    GotoSP[GOTO_HOME].fill("Home", "Home", ISS_OFF);
    GotoSP.fill(getDeviceName(), "GOTO", "Goto", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // List all supported models
    std::map<std::string, std::string>::iterator iter;
    int nModels = 1;
    ModelS = static_cast<ISwitch *>(malloc(sizeof(ISwitch)));
    // Need to be able to select no focuser to avoid troubles with Ekos
    IUFillSwitch(ModelS, "No Focuser", "No Focuser", ISS_ON);
    for (iter = lynxModels.begin(); iter != lynxModels.end(); ++iter)
    {
        ISwitch * buffer = static_cast<ISwitch *>(realloc(ModelS, (nModels + 1) * sizeof(ISwitch)));
        if (!buffer)
        {
            free(ModelS);
            return false;
        }
        else
            ModelS = buffer;
        IUFillSwitch(ModelS + nModels, (iter->first).c_str(), (iter->first).c_str(), ISS_OFF);

        nModels++;
    }
    IUFillSwitchVector(&ModelSP, ModelS, nModels, getDeviceName(), "MODEL", "Model", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Status indicators
    StatusLP[STATUS_MOVING].fill("Is Moving", "", IPS_IDLE);
    StatusLP[STATUS_HOMING].fill("Is Homing", "", IPS_IDLE);
    StatusLP[STATUS_HOMED].fill("Is Homed", "", IPS_IDLE);
    StatusLP[STATUS_FFDETECT].fill("FF Detect", "", IPS_IDLE);
    StatusLP[STATUS_TMPPROBE].fill("Tmp Probe", "", IPS_IDLE);
    StatusLP[STATUS_REMOTEIO].fill("Remote IO", "", IPS_IDLE);
    StatusLP[STATUS_HNDCTRL].fill("Hnd Ctrl", "", IPS_IDLE);
    StatusLP[STATUS_REVERSE].fill("Reverse", "", IPS_IDLE);
    StatusLP.fill(getDeviceName(), "STATUS", "Status", FOCUS_STATUS_TAB, IPS_IDLE);

    // Focus name configure in the HUB
    HFocusNameTP[0].fill("FocusName", "Focuser name", "");
    HFocusNameTP.fill(getDeviceName(), "FOCUSNAME", "Focuser", FOCUS_SETTINGS_TAB, IP_RW, 0,
                     IPS_IDLE);

    // Led intensity value
    LedNP[0].fill("Intensity", "", "%.f", 0, 100, 5., 0.);
    LedNP.fill(getDeviceName(), "LED", "Led", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);
    //simPosition = FocusAbsPosNP[0].getValue();

    addAuxControls();

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
void FocusLynxBase::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    INDI::Focuser::ISGetProperties(dev);

    defineProperty(&ModelSP);
    if (isSimulation())
        loadConfig(true, "Model");
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::updateProperties()
{
    // For absolute focusers the vector is set to RO, as we get value from the HUB
    if (isAbsolute == false)
        FocusMaxPosNP.setPermission(IP_RW);
    else
        FocusMaxPosNP.setPermission(IP_RO);

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(HFocusNameTP);

        defineProperty(TemperatureNP);
        defineProperty(TemperatureCompensateModeSP);
        defineProperty(TemperatureParamNP);
        defineProperty(TemperatureCompensateSP);
        defineProperty(TemperatureCompensateOnStartSP);

        //        defineProperty(FocusBacklashSP);
        //        defineProperty(FocusBacklashNP);

        //defineProperty(MaxTravelNP);

        defineProperty(StepSizeNP);

        defineProperty(ResetSP);
        //defineProperty(ReverseSP);
        defineProperty(StatusLP);

        if (getFocusConfig() && getFocusTemp())
            LOG_INFO("FocusLynx parameters updated, focuser ready for use.");
        else
        {
            LOG_ERROR("Failed to retrieve focuser configuration settings...");
            return false;
        }
    }
    else
    {
        deleteProperty(TemperatureNP.getName());
        deleteProperty(TemperatureCompensateModeSP.getName());
        deleteProperty(TemperatureCompensateSP.getName());
        deleteProperty(TemperatureParamNP.getName());
        deleteProperty(TemperatureCompensateOnStartSP.getName());

        //        deleteProperty(FocusBacklashSP.getName());
        //        deleteProperty(FocusBacklashNP.getName());

        //deleteProperty(MaxTravelNP.getName());
        deleteProperty(StepSizeNP.getName());

        deleteProperty(ResetSP.getName());
        deleteProperty(GotoSP.getName());
        //deleteProperty(ReverseSP.getName());

        deleteProperty(StatusLP.getName());
        deleteProperty(HFocusNameTP.getName());
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::Handshake()
{
    if (ack())
    {
        LOG_INFO("FocusLynx is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO("Error retrieving data from FocusLynx, please ensure FocusLynxBase controller is "
             "powered and the port is correct.");
    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
const char *FocusLynxBase::getDefaultName()
{
    // Has to be overide by child instance
    return "FocusLynxBase";
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Models
        if (strcmp(ModelSP.name, name) == 0)
        {
            IUUpdateSwitch(&ModelSP, states, names, n);
            ModelSP.s = IPS_OK;
            IDSetSwitch(&ModelSP, nullptr);
            if (isConnected())
            {
                setDeviceType(IUFindOnSwitchIndex(&ModelSP));
                LOG_INFO("Focuser model set. Please disconnect and reconnect now...");
            }
            else
                LOG_INFO("Focuser model set. Please connect now...");

            // Check if we have absolute or relative focusers
            checkIfAbsoluteFocuser();
            //Read the config for this new model form the HUB
            getFocusConfig();

            return true;
        }

        // Temperature Compensation
        if (TemperatureCompensateSP.isNameMatch(name))
        {
            int prevIndex = TemperatureCompensateSP.findOnSwitchIndex();
            TemperatureCompensateSP.update(states, names, n);
            if (setTemperatureCompensation(TemperatureCompensateSP[0].getState() == ISS_ON))
            {
                TemperatureCompensateSP.setState(IPS_OK);
            }
            else
            {
                TemperatureCompensateSP.reset();
                TemperatureCompensateSP.setState(IPS_ALERT);
                TemperatureCompensateSP[prevIndex].setState(ISS_ON);
            }

            TemperatureCompensateSP.apply();
            return true;
        }

        // Temperature Compensation on Start
        if (TemperatureCompensateOnStartSP.isNameMatch(name))
        {
            int prevIndex = TemperatureCompensateOnStartSP.findOnSwitchIndex();
            TemperatureCompensateOnStartSP.update(states, names, n);
            if (setTemperatureCompensationOnStart(TemperatureCompensateOnStartSP[0].getState() == ISS_ON))
            {
                TemperatureCompensateOnStartSP.setState(IPS_OK);
            }
            else
            {
                TemperatureCompensateOnStartSP.reset();
                TemperatureCompensateOnStartSP.setState(IPS_ALERT);
                TemperatureCompensateOnStartSP[prevIndex].setState(ISS_ON);
            }

            TemperatureCompensateOnStartSP.apply();
            return true;
        }

        // Temperature Compensation Mode
        if (TemperatureCompensateModeSP.isNameMatch(name))
        {
            int prevIndex = TemperatureCompensateModeSP.findOnSwitchIndex();
            TemperatureCompensateModeSP.update(states, names, n);
            char mode = TemperatureCompensateModeSP.findOnSwitchIndex() + 'A';
            if (setTemperatureCompensationMode(mode))
            {
                TemperatureCompensateModeSP.setState(IPS_OK);
            }
            else
            {
                TemperatureCompensateModeSP.reset();
                TemperatureCompensateModeSP.setState(IPS_ALERT);
                TemperatureCompensateModeSP[prevIndex].setState(ISS_ON);
            }

            TemperatureCompensateModeSP.apply();
            return true;
        }

        // Backlash enable/disable
        //        if (FocusBacklashSP.isNameMatch(name))
        //        {
        //            int prevIndex = FocusBacklashSP.findOnSwitchIndex();
        //            FocusBacklashSP.update(states, names, n);
        //            if (setBacklashCompensation(FocusBacklashSP[INDI_ENABLED].getState() == ISS_ON))
        //            {
        //                FocusBacklashSP.setState(IPS_OK);
        //            }
        //            else
        //            {
        //                FocusBacklashSP.reset();
        //                FocusBacklashSP.setState(IPS_ALERT);
        //                FocusBacklashSP[prevIndex].setState(ISS_ON);
        //            }

        //            FocusBacklashSP.apply();
        //            return true;
        //        }

        // Reset to Factory setting
        if (ResetSP.isNameMatch(name))
        {
            ResetSP.reset();
            if (resetFactory())
                ResetSP.setState(IPS_OK);
            else
                ResetSP.setState(IPS_ALERT);

            ResetSP.apply();
            return true;
        }

        // Go to home/center
        if (GotoSP.isNameMatch(name))
        {
            GotoSP.update(states, names, n);

            if (GotoSP[GOTO_HOME].getState() == ISS_ON)
            {
                if (home())
                    GotoSP.setState(IPS_BUSY);
                else
                    GotoSP.setState(IPS_ALERT);
            }
            else
            {
                if (center())
                    GotoSP.setState(IPS_BUSY);
                else
                    GotoSP.setState(IPS_ALERT);
            }

            GotoSP.apply();
            return true;
        }

        // Reverse Direction
        //        if (ReverseSP.isNameMatch(name))
        //        {
        //            ReverseSP.update(states, names, n);

        //            if (reverse(ReverseSP[0].getState() == ISS_ON))
        //                ReverseSP.setState(IPS_OK);
        //            else
        //                ReverseSP.setState(IPS_ALERT);

        //            ReverseSP.apply();
        //            return true;
        //        }

        // Sync Mandatory
        if (SyncMandatorySP.isNameMatch(name))
        {
            SyncMandatorySP.update(states, names, n);

            if (SyncMandatory(SyncMandatorySP[0].getState() == ISS_ON))
                SyncMandatorySP.setState(IPS_OK);
            else
                SyncMandatorySP.setState(IPS_ALERT);

            SyncMandatorySP.apply();
            return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Set device nickname to the HUB itself
        if (HFocusNameTP.isNameMatch(name))
        {
            HFocusNameTP.update(texts, names, n);
            if (setDeviceNickname(HFocusNameTP[0].getText()))
                HFocusNameTP.setState(IPS_OK);
            else
                HFocusNameTP.setState(IPS_ALERT);
            HFocusNameTP.apply();
            return true;
        }
    }
    return INDI::Focuser::ISNewText(dev, name, texts, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Temperature Coefficient & Inceptions
        if (TemperatureParamNP.isNameMatch(name))
        {
            TemperatureParamNP.update(values, names, n);

            char mode = static_cast<char>(65 + TemperatureCompensateModeSP.findOnSwitchIndex());
            if (!setTemperatureCompensationCoeff(mode, TemperatureParamNP[0].getValue()) ||
                    !setTemperatureInceptions(mode, TemperatureParamNP[1].getValue()))
            {
                LOG_ERROR("Failed to write temperature coefficient or intercept");
                TemperatureParamNP.setState(IPS_ALERT);
                TemperatureParamNP.apply();
                return false;
            }

            TemperatureParamNP.setState(IPS_OK);
            getFocusTemp();

            return true;
        }

        // Backlash Value
        //        if (FocusBacklashNP.isNameMatch(name))
        //        {
        //            FocusBacklashNP.update(values, names, n);
        //            if (setFocusBacklashSteps(FocusBacklashNP[0].value) == false)
        //            {
        //                LOG_ERROR("Failed to set temperature coefficients.");
        //                FocusBacklashNP.setState(IPS_ALERT);
        //                FocusBacklashNP.apply();
        //                return false;
        //            }

        //            FocusBacklashNP.setState(IPS_OK);
        //            FocusBacklashNP.apply();
        //            return true;
        //        }

        // Sync
        //        if (SyncNP.isNameMatch(name))
        //        {
        //            SyncNP.update(values, names, n);
        //            if (sync(SyncNP[0].value) == false)
        //            {
        //                LOG_ERROR("Failed to set the actual value.");
        //                SyncNP.setState(IPS_ALERT);
        //                SyncNP.apply();
        //                return false;
        //            }
        //            else
        //                SyncNP.setState(IPS_OK);

        //            SyncNP.apply();
        //            return true;
        //        }

        // StepSize
        if (StepSizeNP.isNameMatch(name))
        {
            StepSizeNP.update(values, names, n);
            if (setStepSize(StepSizeNP[0].value) == false)
            {
                LOG_ERROR("Failed to set the actual value.");
                StepSizeNP.setState(IPS_ALERT);
                StepSizeNP.apply();
                return false;
            }
            else
                StepSizeNP.setState(IPS_OK);

            StepSizeNP.apply();
            return true;
        }

        // Max Travel if relative focusers
        //        if (MaxTravelNP.isNameMatch(name))
        //        {
        //            MaxTravelNP.update(values, names, n);

        //            if (setMaxTravel(MaxTravelNP[0].value) == false)
        //                MaxTravelNP.setState(IPS_ALERT);
        //            else
        //            {
        //                MaxTravelNP.setState(IPS_OK);
        //                FocusAbsPosNP[0].setMax(SyncNP[0].setMax(MaxTravelNP[0].value));
        //                FocusAbsPosNP[0].setStep(SyncNP[0].setStep((MaxTravelNP[0].value / 50.0)));

        //                FocusAbsPosNP.updateMinMax();
        //                SyncNP.updateMinMax();

        //                MaxTravelNP.apply();

        //                LOGF_INFO("Focuser absolute limits: min (%g) max (%g)", FocusAbsPosNP[0].min,
        //                       FocusAbsPosNP[0].max);
        //            }
        //            return true;
        //        }

        // Set LED intensity to the HUB itself via function setLedLevel()
        if (LedNP.isNameMatch(name))
        {
            LedNP.update(values, names, n);
            if (setLedLevel(LedNP[0].value))
                LedNP.setState(IPS_OK);
            else
                LedNP.setState(IPS_ALERT);
            LOGF_INFO("Focuser LED level intensity : %f", LedNP[0].getValue());
            LedNP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::ack()
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sHELLO>", getFocusTarget());
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        const char *focusName = IUFindOnSwitch(&ModelSP)->label;
        strncpy(response, focusName, LYNX_MAX);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        LOGF_INFO("%s is detected.", response);

        return true;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::getFocusConfig()
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;
    char key[16];

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sGETCONFIG>", getFocusTarget());
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        if (!strcmp(getFocusTarget(), "F1"))
            strncpy(response, "CONFIG1", 16);
        else
            strncpy(response, "CONFIG2", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        if ((strcmp(response, "CONFIG1")) && (strcmp(response, "CONFIG2")))
            return false;
    }

    memset(response, 0, sizeof(response));

    // Nickname
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "NickName=Focuser#%s\n", getFocusTarget());
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    char nickname[16];
    int rc = sscanf(response, "%16[^=]=%16[^\n]s", key, nickname);

    if (rc != 2)
        return false;

    HFocusNameTP[0].setText(nickname);
    HFocusNameTP.apply();

    HFocusNameTP.setState(IPS_OK);
    HFocusNameTP.apply();

    memset(response, 0, sizeof(response));

    // Get Max Position
    if (isSimulation())
    {
        if (isAbsolute == false)
            // Value with high limit to give freedom to user of emulation range
            snprintf(response, 32, "Max Pos = %06d\n", 100000);
        else
            // Value from the TCF-S absolute focuser
            snprintf(response, 32, "Max Pos = %06d\n", 7000);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    uint32_t maxPos = 0;
    rc = sscanf(response, "%16[^=]=%d", key, &maxPos);
    if (rc == 2)
    {
        FocusAbsPosNP[0].setMin(0);
        FocusAbsPosNP[0].setMax(maxPos);
        FocusAbsPosNP[0].setStep(maxPos / 50.0);

        FocusSyncNP[0].setMin(0);
        FocusSyncNP[0].setMax(maxPos);
        FocusSyncNP[0].setStep(maxPos / 50.0);

        FocusRelPosNP[0].setMin(0);
        FocusRelPosNP[0].setMax(maxPos / 2);
        FocusRelPosNP[0].setStep(maxPos / 100.0);

        FocusAbsPosNP.updateMinMax();
        FocusRelPosNP.updateMinMax();
        FocusSyncNP.updateMinMax();

        FocusMaxPosNP.setState(IPS_OK);
        FocusMaxPosNP[0].setValue(maxPos);
        FocusMaxPosNP.apply();

    }
    else
        return false;

    memset(response, 0, sizeof(response));

    // Get Device Type
    if (isSimulation())
    {
        // In simulation each focuser is different, one Absolute and one relative
        if (strcmp(getFocusTarget(), "F2"))
            snprintf(response, 32, "Dev Type = %s\n", "OA");
        else
            snprintf(response, 32, "Dev Type = %s\n", "SO");
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    // Don't process the response if isSimulation active, Value read from saved config
    if (!isSimulation())
    {
        //Extract the code from the response value
        std::string tmpString;
        tmpString.assign(response + 11, 2);
        int count = 0;

        //As "ZZ" is not exist in lynxModel, not need interator, 'No focuser' is known as first in ModelS
        if(tmpString != "ZZ")
        {
            // If not 'No Focuser' then do iterator
            // iterate throught all elements in std::map<std::string, std::string> and search the index from the code.
            std::map<std::string, std::string>::iterator it = lynxModels.begin();
            while(it != lynxModels.end())
            {
                count++;
                if (it->second == tmpString)
                    break;
                it++;
            }
        }

        // as different focuser could have the same code in the HUB, we are not able to find the correct name in the list of focuser.
        // The first one would be show as the item.
        IUResetSwitch(&ModelSP);
        ModelS[count].s = ISS_ON;
        IDSetSwitch(&ModelSP, nullptr);

        // If focuser is relative, we only exposure "Center" command as it cannot home
        checkIfAbsoluteFocuser();

        LOGF_DEBUG("Index focuser : %d", count);
    } // end if (!isSimulation)

    // Get Status Parameters

    memset(response, 0, sizeof(response));

    // Temperature information processed on function getFocusTemp(), do nothing with related response

    // Temperature Compensation On
    if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    // Temperature Coeff A
    if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    // Temperature Coeff B
    if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    // Temperature Coeff C
    if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    // Temperature Coeff D
    if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    // Temperature Coeff E
    if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    // Temperature Compensation Mode
    if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    // Backlash Compensation
    if (isSimulation())
    {
        snprintf(response, 32, "BLC En = %d\n", FocusBacklashSP[INDI_ENABLED].getState() == ISS_ON ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int BLCCompensate;
    rc = sscanf(response, "%16[^=]=%d", key, &BLCCompensate);
    if (rc != 2)
        return false;

    FocusBacklashSP.reset();
    FocusBacklashSP[INDI_ENABLED].setState(BLCCompensate ? ISS_ON : ISS_OFF);
    FocusBacklashSP[INDI_DISABLED].setState(BLCCompensate ? ISS_OFF : ISS_ON);
    FocusBacklashSP.setState(IPS_OK);
    FocusBacklashSP.apply();

    // Backlash Value
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "BLC Stps = %d\n", 50);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int BLCValue;
    rc = sscanf(response, "%16[^=]=%d", key, &BLCValue);
    if (rc != 2)
        return false;

    FocusBacklashNP[0].setValue(BLCValue);
    FocusBacklashNP.setState(IPS_OK);
    FocusBacklashNP.apply();

    // Led brightnesss
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "LED Brt = %d\n", 75);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int LEDBrightness;
    rc = sscanf(response, "%16[^=]=%d", key, &LEDBrightness);
    if (rc != 2)
        return false;

    LedNP[0].setValue(LEDBrightness);
    LedNP.setState(IPS_OK);
    LedNP.apply();

    // Temperature Compensation on Start
    if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    // END is reached
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        strncpy(response, "END\n", 16);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';

        // Display the response to be sure to have read the complet TTY Buffer.
        LOGF_DEBUG("RES (%s)", response);

        if (strcmp(response, "END"))
            return false;
    }

    tcflush(PortFD, TCIFLUSH);

    configurationComplete = true;

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::getFocusStatus()
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;
    char key[16];

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sGETSTATUS>", getFocusTarget());
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        if (!strcmp(getFocusTarget(), "F1"))
            strncpy(response, "STATUS1", 16);
        else
            strncpy(response, "STATUS2", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        //tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        if (!((!strcmp(response, "STATUS1")) && (!strcmp(getFocusTarget(), "F1"))) && !((!strcmp(response, "STATUS2")) && (!strcmp(getFocusTarget(), "F2"))))
        {
            tcflush(PortFD, TCIFLUSH);
            return false;
        }

        // Get Temperature
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            //strncpy(response, "Temp(C) = +21.7\n", 16); // #PS: for string literal, use strcpy
            strcpy(response, "Temp(C) = +21.7\n");
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (nbytes_read > 0)
            response[nbytes_read - 1] = '\0'; // remove last character (new line)

        LOGF_DEBUG("RES (%s)", response);

        float temperature = 0;
        int rc            = sscanf(response, "%16[^=]=%f", key, &temperature);
        if (rc == 2)
        {
            TemperatureNP[0].setValue(temperature);
            TemperatureNP.apply();
        }
        else
        {
            if (TemperatureNP.getState() != IPS_ALERT)
            {
                TemperatureNP.setState(IPS_ALERT);
                TemperatureNP.apply();
            }
        }

        // Get Current Position
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            snprintf(response, 32, "Curr Pos = %06d\n", simPosition);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        uint32_t currPos = 0;
        rc               = sscanf(response, "%16[^=]=%d", key, &currPos);
        if (rc == 2)
        {
            FocusAbsPosNP[0].setValue(currPos);
            FocusAbsPosNP.apply();
        }
        else
            return false;

        // Get Target Position
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            snprintf(response, 32, "Targ Pos = %06d\n", targetPosition);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        // Get Status Parameters

        // #1 is Moving?
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            snprintf(response, 32, "Is Moving = %d\n", (simStatus[STATUS_MOVING] == ISS_ON) ? 1 : 0);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        int isMoving;
        rc = sscanf(response, "%16[^=]=%d", key, &isMoving);
        if (rc != 2)
            return false;

        StatusLP[STATUS_MOVING].setState(isMoving ? IPS_BUSY : IPS_IDLE);

        // #2 is Homing?
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            snprintf(response, 32, "Is Homing = %d\n", (simStatus[STATUS_HOMING] == ISS_ON) ? 1 : 0);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        int _isHoming;
        rc = sscanf(response, "%16[^=]=%d", key, &_isHoming);
        if (rc != 2)
            return false;

        StatusLP[STATUS_HOMING].setState(_isHoming ? IPS_BUSY : IPS_IDLE);
        // For relative focusers home is not applicable.
        if (isAbsolute == false)
            StatusLP[STATUS_HOMING].setState(IPS_IDLE);

        // We set that isHoming in process, but we don't set it to false here it must be reset in TimerHit
        if (StatusLP[STATUS_HOMING].getState() == IPS_BUSY)
            isHoming = true;

        // #3 is Homed?
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            snprintf(response, 32, "Is Homed = %d\n", (simStatus[STATUS_HOMED] == ISS_ON) ? 1 : 0);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        int isHomed;
        rc = sscanf(response, "%16[^=]=%d", key, &isHomed);
        if (rc != 2)
            return false;

        StatusLP[STATUS_HOMED].setState(isHomed ? IPS_OK : IPS_IDLE);
        // For relative focusers home is not applicable.
        if (isAbsolute == false)
            StatusLP[STATUS_HOMED].setState(IPS_IDLE);

        // #4 FF Detected?
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            snprintf(response, 32, "FFDetect = %d\n", (simStatus[STATUS_FFDETECT] == ISS_ON) ? 1 : 0);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        int FFDetect;
        rc = sscanf(response, "%16[^=]=%d", key, &FFDetect);
        if (rc != 2)
            return false;

        StatusLP[STATUS_FFDETECT].setState(FFDetect ? IPS_OK : IPS_IDLE);

        // #5 Temperature probe?
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            snprintf(response, 32, "TmpProbe = %d\n", (simStatus[STATUS_TMPPROBE] == ISS_ON) ? 1 : 0);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        int TmpProbe;
        rc = sscanf(response, "%16[^=]=%d", key, &TmpProbe);
        if (rc != 2)
            return false;

        StatusLP[STATUS_TMPPROBE].setState(TmpProbe ? IPS_OK : IPS_IDLE);

        // #6 Remote IO?
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            snprintf(response, 32, "RemoteIO = %d\n", (simStatus[STATUS_REMOTEIO] == ISS_ON) ? 1 : 0);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        int RemoteIO;
        rc = sscanf(response, "%16[^=]=%d", key, &RemoteIO);
        if (rc != 2)
            return false;

        StatusLP[STATUS_REMOTEIO].setState(RemoteIO ? IPS_OK : IPS_IDLE);

        // #7 Hand controller?
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            snprintf(response, 32, "Hnd Ctlr = %d\n", (simStatus[STATUS_HNDCTRL] == ISS_ON) ? 1 : 0);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        int HndCtlr;
        rc = sscanf(response, "%16[^=]=%d", key, &HndCtlr);
        if (rc != 2)
            return false;

        StatusLP[STATUS_HNDCTRL].setState(HndCtlr ? IPS_OK : IPS_IDLE);

        // #8 Reverse?
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            snprintf(response, 32, "Reverse = %d\n", (simStatus[STATUS_REVERSE] == ISS_ON) ? 1 : 0);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        int reverse;
        rc = sscanf(response, "%16[^=]=%d", key, &reverse);
        if (rc != 2)
            return false;

        StatusLP[STATUS_REVERSE].setState(reverse ? IPS_OK : IPS_IDLE);

        // If reverse is enable and switch shows disabled, let's change that
        // same thing is reverse is disabled but switch is enabled
        if ((reverse && FocusReverseSP[1].s == ISS_ON) || (!reverse && FocusReverseSP[0].s == ISS_ON))
        {
            FocusReverseSP.reset();
            FocusReverseSP[0].setState((reverse == 1) ? ISS_ON : ISS_OFF);
            FocusReverseSP[1].setState((reverse == 0) ? ISS_ON : ISS_OFF);
            FocusReverseSP.apply();
        }

        StatusLP.setState(IPS_OK);
        StatusLP.apply();

        // END is reached
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            strncpy(response, "END\n", 16);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (nbytes_read > 0)
        {
            response[nbytes_read - 1] = '\0';

            // Display the response to be sure to have read the complet TTY Buffer.
            LOGF_DEBUG("RES (%s)", response);
            if (strcmp(response, "END"))
                return false;
        }

        tcflush(PortFD, TCIFLUSH);

        return true;
    }
    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::getFocusTemp()
{
    // Get value related to Temperature compensation

    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;
    char key[16];

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sGETTCI>", getFocusTarget());
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        if (!strcmp(getFocusTarget(), "F1"))
            strncpy(response, "TEMP COMP1", 16);
        else
            strncpy(response, "TEMP COMP2", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        if ((errcode = tty_write_string(PortFD, cmd,  &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        if ((strcmp(response, "TEMP COMP1")) && (strcmp(response, "TEMP COMP2")))
            return false;

        memset(response, 0, sizeof(response));

        // Temperature Compensation On?
        if (isSimulation())
        {
            snprintf(response, 32, "TComp ON = %d\n", TemperatureCompensateSP[0].getState() == ISS_ON ? 1 : 0);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        int TCompOn;
        int rc = sscanf(response, "%16[^=]=%d", key, &TCompOn);
        if (rc != 2)
            return false;

        TemperatureCompensateSP.reset();
        TemperatureCompensateSP[0].setState(TCompOn ? ISS_ON : ISS_OFF);
        TemperatureCompensateSP[1].setState(TCompOn ? ISS_OFF : ISS_ON);
        TemperatureCompensateSP.setState(IPS_OK);
        TemperatureCompensateSP.apply();

        memset(response, 0, sizeof(response));

        // Temperature Compensation Mode
        if (isSimulation())
        {
            snprintf(response, 32, "TC Mode = %c\n", 'C');
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        char compensateMode;
        rc = sscanf(response, "%16[^=]= %c", key, &compensateMode);
        if (rc != 2)
        {
            if (rc == 1 && key[0] == 'T')
            {
                //If the controller does not support this it could be null. Assume A mode in this case.
                compensateMode = 'A';
            }
            else
            {
                return false;
            }
        }

        TemperatureCompensateModeSP.reset();
        int index = compensateMode - 'A';
        if (index >= 0 && index <= 5)
        {
            TemperatureCompensateModeSP[index].setState(ISS_ON);
            TemperatureCompensateModeSP.setState(IPS_OK);
        }
        else
        {
            LOGF_ERROR("Invalid index %d for compensation mode.", index);
            TemperatureCompensateModeSP.setState(IPS_ALERT);
        }

        TemperatureCompensateModeSP.apply();


        // Temperature Compensation on Start
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            snprintf(response, 32, "TC@Start = %d\n", TemperatureCompensateOnStartSP[0].getState() == ISS_ON ? 1 : 0);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        int TCOnStart;
        rc = sscanf(response, "%16[^=]=%d", key, &TCOnStart);
        if (rc != 2)
            return false;

        TemperatureCompensateOnStartSP.reset();
        TemperatureCompensateOnStartSP[0].setState(TCOnStart ? ISS_ON : ISS_OFF);
        TemperatureCompensateOnStartSP[1].setState(TCOnStart ? ISS_OFF : ISS_ON);
        TemperatureCompensateOnStartSP.setState(IPS_OK);
        TemperatureCompensateOnStartSP.apply();

        // Temperature Coeff A
        if (isSimulation())
        {
            snprintf(response, 32, "TempCo A = %d\n", static_cast<int>(TemperatureParamNP[0].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (TemperatureCompensateModeSP[0].getState() == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TCoeff;
            rc = sscanf(response, "%16[^=]=%d", key, &TCoeff);
            if (rc != 2)
                return false;

            TemperatureParamNP[0].setValue(TCoeff);
        }
        memset(response, 0, sizeof(response));

        // Temperature Coeff B
        if (isSimulation())
        {
            snprintf(response, 32, "TempCo B = %d\n", static_cast<int>(TemperatureParamNP[0].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeSP[1].getState() == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TCoeff;
            rc = sscanf(response, "%16[^=]=%d", key, &TCoeff);
            if (rc != 2)
                return false;

            TemperatureParamNP[0].setValue(TCoeff);
        }

        memset(response, 0, sizeof(response));

        // Temperature Coeff C
        if (isSimulation())
        {
            snprintf(response, 32, "TempCo C = %d\n", static_cast<int>(TemperatureParamNP[0].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeSP[2].getState() == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TCoeff;
            rc = sscanf(response, "%16[^=]=%d", key, &TCoeff);
            if (rc != 2)
                return false;

            TemperatureParamNP[0].setValue(TCoeff);
        }

        memset(response, 0, sizeof(response));

        // Temperature Coeff D
        if (isSimulation())
        {
            snprintf(response, 32, "TempCo D = %d\n", static_cast<int>(TemperatureParamNP[0].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeSP[3].getState() == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TCoeff;
            rc = sscanf(response, "%16[^=]=%d", key, &TCoeff);
            if (rc != 2)
                return false;

            TemperatureParamNP[0].setValue(TCoeff);
        }

        memset(response, 0, sizeof(response));

        // Temperature Coeff E
        if (isSimulation())
        {
            snprintf(response, 32, "TempCo E = %d\n", static_cast<int>(TemperatureParamNP[0].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeSP[4].getState() == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TCoeff;
            rc = sscanf(response, "%16[^=]=%d", key, &TCoeff);
            if (rc != 2)
                return false;

            TemperatureParamNP[0].setValue(TCoeff);
        }

        memset(response, 0, sizeof(response));

        // Temperature intercepts A
        if (isSimulation())
        {
            snprintf(response, 32, "TempIn A = %d\n", static_cast<int>(TemperatureParamNP[1].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeSP[0].getState() == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TInter;
            rc = sscanf(response, "%16[^=]=%d", key, &TInter);
            if (rc != 2)
                return false;

            TemperatureParamNP[1].setValue(TInter);
        }

        memset(response, 0, sizeof(response));

        // Temperature intercepts B
        if (isSimulation())
        {
            snprintf(response, 32, "TempIn B = %d\n", static_cast<int>(TemperatureParamNP[1].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeSP[1].getState() == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TInter;
            rc = sscanf(response, "%16[^=]=%d", key, &TInter);
            if (rc != 2)
                return false;

            TemperatureParamNP[1].setValue(TInter);
        }

        memset(response, 0, sizeof(response));

        // Temperature intercepts C
        if (isSimulation())
        {
            snprintf(response, 32, "TempIn C = %d\n", static_cast<int>(TemperatureParamNP[1].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeSP[2].getState() == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TInter;
            rc = sscanf(response, "%16[^=]=%d", key, &TInter);
            if (rc != 2)
                return false;

            TemperatureParamNP[1].setValue(TInter);
        }

        memset(response, 0, sizeof(response));

        // Temperature intercepts D
        if (isSimulation())
        {
            snprintf(response, 32, "TempIn D = %d\n", static_cast<int>(TemperatureParamNP[1].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeSP[3].getState() == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TInter;
            rc = sscanf(response, "%16[^=]=%d", key, &TInter);
            if (rc != 2)
                return false;

            TemperatureParamNP[1].setValue(TInter);
        }

        memset(response, 0, sizeof(response));

        // Temperature intercepts E
        if (isSimulation())
        {
            snprintf(response, 32, "TempIn E = %d\n", static_cast<int>(TemperatureParamNP[1].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeSP[4].getState() == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TInter;
            rc = sscanf(response, "%16[^=]=%d", key, &TInter);
            if (rc != 2)
                return false;

            TemperatureParamNP[1].setValue(TInter);
        }

        TemperatureParamNP.setState(IPS_OK);
        TemperatureParamNP.apply();

        memset(response, 0, sizeof(response));

        // StepSize
        if (isSimulation())
        {
            snprintf(response, 32, "StepSize = %d\n", static_cast<int>(StepSizeNP[0].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        int valueStepSize;
        rc = sscanf(response, "%16[^=]=%d", key, &valueStepSize);
        if (rc != 2)
            return false;

        StepSizeNP[0].setValue(valueStepSize);
        StepSizeNP.apply();

        memset(response, 0, sizeof(response));

        // END is reached
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            strncpy(response, "END\n", 16);
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (nbytes_read > 0)
        {
            response[nbytes_read - 1] = '\0';

            // Display the response to be sure to have read the complet TTY Buffer.
            LOGF_DEBUG("RES (%s)", response);
            if (strcmp(response, "END"))
                return false;
        }

        tcflush(PortFD, TCIFLUSH);

        return true;
    }
    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::setDeviceType(int index)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<%sSCDT%s>", getFocusTarget(), index > 0 ? lynxModels[ModelS[index].name].c_str() : "ZZ");

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
            return true;
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::setLedLevel(int level)
// Write via the connected port to the HUB the selected LED intensity level

{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<FHSCLB%d>", level);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
            return true;
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::setDeviceNickname(const char *nickname)
// Write via the connected port to the HUB the choiced nikname of the focuser
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sSCNN%s>", getFocusTarget(), nickname);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
            return true;
        else
            return false;
    }

    return false;
}
/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::home()
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sHOME>", getFocusTarget());
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "H", 16);
        nbytes_read              = strlen(response) + 1;
        targetPosition           = 0;
        //FocusAbsPosNP[0].setValue(MaxTravelNP[0].getValue());
        FocusAbsPosNP.setState(IPS_OK);
        FocusAbsPosNP.apply();
        simStatus[STATUS_HOMING] = ISS_ON;
        simStatus[STATUS_HOMED] = ISS_OFF;
        simPosition = FocusAbsPosNP[0].getValue();
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        FocusAbsPosNP.setState(IPS_BUSY);
        FocusAbsPosNP.apply();

        isHoming = true;
        LOG_INFO("Focuser moving to home position...");

        tcflush(PortFD, TCIFLUSH);

        return true;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::center()
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (isAbsolute == false)
        return (MoveAbsFocuser(FocusAbsPosNP[0].getMax() / 2) != IPS_ALERT);

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sCENTER>", getFocusTarget());
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "M", 16);
        nbytes_read              = strlen(response) + 1;
        simStatus[STATUS_MOVING] = ISS_ON;
        targetPosition           = FocusAbsPosNP[0].getMax() / 2;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        LOG_INFO("Focuser moving to center position...");

        FocusAbsPosNP.setState(IPS_BUSY);
        FocusAbsPosNP.apply();

        tcflush(PortFD, TCIFLUSH);

        return true;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::setTemperatureCompensation(bool enable)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<%sSCTE%d>", getFocusTarget(), enable ? 1 : 0);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
            return true;
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::setTemperatureCompensationMode(char mode)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<%sSCTM%c>", getFocusTarget(), mode);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        // If OK, the value would be read and update UI properties
        if (!strcmp(response, "SET"))
            return getFocusTemp();
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::setTemperatureCompensationCoeff(char mode, int16_t coeff)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<%sSCTC%c%c%04d>", getFocusTarget(), mode, coeff >= 0 ? '+' : '-', static_cast<int>(std::abs(coeff)));

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
            return true;
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::setTemperatureInceptions(char mode, int32_t inter)
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sSETINT%c%c%06d>", getFocusTarget(), mode, inter >= 0 ? '+' : '-', static_cast<int>(std::abs(inter)));

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
            return true;
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::setTemperatureCompensationOnStart(bool enable)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<%sSCTS%d>", getFocusTarget(), enable ? 1 : 0);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
            return true;
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
//bool FocusLynxBase::setBacklashCompensation(bool enable)
bool FocusLynxBase::SetFocuserBacklashEnabled(bool enabled)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<%sSCBE%d>", getFocusTarget(), enabled ? 1 : 0);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
            return true;
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
//bool FocusLynxBase::setFocusBacklashSteps(uint16_t steps)
bool FocusLynxBase::SetFocuserBacklash(int32_t steps)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<%sSCBS%02d>", getFocusTarget(), steps);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
            return true;
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::ReverseFocuser(bool enabled)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<%sREVERSE%d>", getFocusTarget(), enabled ? 1 : 0);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read               = strlen(response) + 1;
        simStatus[STATUS_REVERSE] = enabled ? ISS_ON : ISS_OFF;
    }
    else
    {
        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
            return true;
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::SyncFocuser(uint32_t ticks)
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sSCCP%06d>", getFocusTarget(), ticks);
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        simPosition = ticks;
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
        {
            LOGF_INFO("Setting current position to %d", ticks);
            isSynced = true;
            return true;
        }
        else
            return false;
    }
    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
//bool FocusLynxBase::setMaxTravel(u_int16_t travel)
bool FocusLynxBase::SetFocuserMaxPosition(uint32_t ticks)
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sSETMAX%06d>", getFocusTarget(), ticks);
    LOGF_DEBUG("CMD (%s)", cmd);

    SyncPresets(ticks);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
        {
            getFocusConfig();
            return true;
        }
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::setStepSize(u_int16_t stepsize)
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sSETFSS%06d>", getFocusTarget(), stepsize);
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
        {
            getFocusConfig();
            return true;
        }
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::resetFactory()
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sRESET>", getFocusTarget());
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);
        tcflush(PortFD, TCIFLUSH);

        if (!strcmp(response, "SET"))
        {
            getFocusConfig();
            return true;
        }
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::isResponseOK()
{
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read = 0;

    memset(response, 0, sizeof(response));

    if (isSimulation())
    {
        strcpy(response, "!");
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("TTY error: %s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        if (!strcmp(response, "!"))
            return true;
        else
        {
            memset(response, 0, sizeof(response));
            while (strstr(response, "END") == nullptr)
            {
                if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
                {
                    tty_error_msg(errcode, errmsg, MAXRBUF);
                    LOGF_ERROR("TTY error: %s", errmsg);
                    return false;
                }
                response[nbytes_read - 1] = '\0';
                LOGF_ERROR("Controller error: %s", response);
            }

            return false;
        }
    }
    return true;
}

/************************************************************************************
*
* ***********************************************************************************/
IPState FocusLynxBase::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    // Relative focusers must be synced initially.
    if (isAbsolute == false && isSynced == false)
    {
        LOG_ERROR("Relative focusers must be synced. Please sync before issuing any motion commands.");
        return IPS_ALERT;
    }

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<%sM%cR%c>", getFocusTarget(), (dir == FOCUS_INWARD) ? 'I' : 'O', (speed == 0) ? '0' : '1');

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "M", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return IPS_ALERT;
        }

        if (!isResponseOK())
            return IPS_ALERT;

        gettimeofday(&focusMoveStart, nullptr);
        focusMoveRequest = duration / 1000.0;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return IPS_ALERT;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        if (duration <= getCurrentPollingPeriod())
        {
            usleep(getCurrentPollingPeriod() * 1000);
            AbortFocuser();
            return IPS_OK;
        }

        tcflush(PortFD, TCIFLUSH);

        return IPS_BUSY;
    }

    return IPS_ALERT;
}

/************************************************************************************
*
* ***********************************************************************************/
IPState FocusLynxBase::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    // Relative focusers must be synced initially.
    if (isAbsolute == false && isSynced == false)
    {
        LOG_ERROR("Relative focusers must be synced. Please sync before issuing any motion commands.");
        return IPS_ALERT;
    }

    targetPosition = targetTicks;

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sMA%06d>", getFocusTarget(), targetTicks);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "M", 16);
        nbytes_read              = strlen(response) + 1;
        simStatus[STATUS_MOVING] = ISS_ON;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return IPS_ALERT;
        }

        if (!isResponseOK())
            return IPS_ALERT;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return IPS_ALERT;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        FocusAbsPosNP.setState(IPS_BUSY);

        tcflush(PortFD, TCIFLUSH);

        return IPS_BUSY;
    }

    return IPS_ALERT;
}

/************************************************************************************
*
* ***********************************************************************************/
IPState FocusLynxBase::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t newPosition = 0;

    // Relative focusers must be synced initially.
    if (isAbsolute == false && isSynced == false)
    {
        LOG_DEBUG("Relative focusers must be synced. Please sync before issuing any motion commands.");
        return IPS_ALERT;
    }

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    return MoveAbsFocuser(newPosition);
}

/************************************************************************************
*
* ***********************************************************************************/
void FocusLynxBase::TimerHit()
{
    if (!isConnected())
        return;

    if (configurationComplete == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool statusrc = false;
    for (int i = 0; i < 2; i++)
    {
        statusrc = getFocusStatus();
        if (statusrc)
            break;
    }

    if (statusrc == false)
    {
        LOG_WARN("Unable to read focuser status....");
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (isSimulation())
        {
            if (FocusAbsPosNP[0].value < targetPosition)
                simPosition += 100;
            else
                simPosition -= 100;

            simStatus[STATUS_MOVING] = ISS_ON;

            if (std::abs(static_cast<int64_t>(simPosition) - static_cast<int64_t>(targetPosition)) < 100)
            {
                FocusAbsPosNP[0].setValue(targetPosition);
                simPosition              = FocusAbsPosNP[0].getValue();
                simStatus[STATUS_MOVING] = ISS_OFF;
                StatusLP[STATUS_MOVING].setState(IPS_IDLE);
                if (simStatus[STATUS_HOMING] == ISS_ON)
                {
                    StatusLP[STATUS_HOMED].setState(IPS_OK);
                    StatusLP[STATUS_HOMING].setState(IPS_IDLE);
                    simStatus[STATUS_HOMING] = ISS_OFF;
                    simStatus[STATUS_HOMED] = ISS_ON;
                }
            }
            else
                StatusLP[STATUS_MOVING].setState(IPS_BUSY);
            StatusLP.apply();
        }

        if (isHoming && StatusLP[STATUS_HOMED].s == IPS_OK)
        {
            isHoming = false;
            GotoSP.setState(IPS_OK);
            GotoSP.reset();
            GotoSP[GOTO_HOME].setState(ISS_ON);
            GotoSP.apply();
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.apply();
            LOG_INFO("Focuser reached home position.");
            if (isSimulation())
                center();
        }
        else if (StatusLP[STATUS_MOVING].getState() == IPS_IDLE)
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
            if (GotoSP.getState() == IPS_BUSY)
            {
                GotoSP.reset();
                GotoSP.setState(IPS_OK);
                GotoSP.apply();
            }
            LOG_INFO("Focuser reached requested position.");
        }
        else if (StatusLP[STATUS_MOVING].getState() == IPS_BUSY && focusMoveRequest > 0)
        {
            float remaining = calcTimeLeft(focusMoveStart, focusMoveRequest);

            if (remaining < getCurrentPollingPeriod())
            {
                sleep(remaining);
                AbortFocuser();
                focusMoveRequest = 0;
            }
        }
    }
    if (StatusLP[STATUS_HOMING].getState() == IPS_BUSY && GotoSP.getState() != IPS_BUSY)
    {
        GotoSP.setState(IPS_BUSY);
        GotoSP.apply();
    }

    SetTimer(getCurrentPollingPeriod());
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::AbortFocuser()
{
    char cmd[LYNX_MAX] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));
    snprintf(cmd, LYNX_MAX, "<%sHALT>", getFocusTarget());
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "HALTED", 16);
        nbytes_read              = strlen(response) + 1;
        simStatus[STATUS_MOVING] = ISS_OFF;
        simStatus[STATUS_HOMING] = ISS_OFF;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        if (FocusRelPosNP.getState() == IPS_BUSY)
        {
            FocusRelPosNP.setState(IPS_IDLE);
            FocusRelPosNP.apply();
        }

        FocusTimerNP.setState(IPS_IDLE);
        FocusAbsPosNP.setState(IPS_IDLE);
        GotoSP.setState(IPS_IDLE);
        GotoSP.reset();
        FocusTimerNP.apply();
        FocusAbsPosNP.apply();
        GotoSP.apply();

        tcflush(PortFD, TCIFLUSH);

        return true;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
float FocusLynxBase::calcTimeLeft(timeval start, float req)
{
    double timesince;
    double timeleft;
    struct timeval now
    {
        0, 0
    };
    gettimeofday(&now, nullptr);

    timesince =
        static_cast<int>((now.tv_sec * 1000.0 + now.tv_usec / 1000)) - static_cast<int>((start.tv_sec * 1000.0 + start.tv_usec / 1000));
    timesince = timesince / 1000;
    timeleft  = req - timesince;
    return timeleft;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &ModelSP);
    TemperatureCompensateSP.save(fp);
    TemperatureCompensateOnStartSP.save(fp);
    //ReverseSP.save(fp);
    TemperatureNP.save(fp);
    TemperatureCompensateModeSP.save(fp);
    //FocusBacklashSP.save(fp);
    //FocusBacklashNP.save(fp);
    StepSizeNP.save(fp);
    if (isAbsolute == false)
    {
        //MaxTravelNP.save(fp);
        SyncMandatorySP.save(fp);
    }
    return true;
}

/************************************************************************************
*
************************************************************************************/
bool FocusLynxBase::loadConfig(bool silent, const char *property)
{
    bool result = true;

    if (property == nullptr)
    {
        // Need to know the user choice for this option not store in HUB
        result = INDI::DefaultDevice::loadConfig(silent, "SYNC MANDATORY");
        result = INDI::DefaultDevice::loadConfig(silent, "Presets") && result;
        if (isSimulation())
        {
            // Only load for simulation, otherwise got from the HUB
            result = (INDI::DefaultDevice::loadConfig(silent, "MODEL") && result);
            result = (INDI::DefaultDevice::loadConfig(silent, "T. COMPENSATION") && result);
            result = (INDI::DefaultDevice::loadConfig(silent, "T. COMPENSATION @START") && result);
            result = (INDI::DefaultDevice::loadConfig(silent, "REVERSE") && result);
            result = (INDI::DefaultDevice::loadConfig(silent, "T. COEFF") && result);
            result = (INDI::DefaultDevice::loadConfig(silent, "COMPENSATE MODE") && result);
            //            result = (INDI::DefaultDevice::loadConfig(silent, "BACKLASH COMPENSATION") && result);
            //            result = (INDI::DefaultDevice::loadConfig(silent, "BACKLASH") && result);
            result = (INDI::DefaultDevice::loadConfig(silent, "MAX TRAVEL") && result);
            result = (INDI::DefaultDevice::loadConfig(silent, "STEP SIZE") && result);
            result = (INDI::DefaultDevice::loadConfig(silent, "T. PARAMETERS") && result);
        }
    }
    else
        result = INDI::DefaultDevice::loadConfig(silent, property);

    return result;
}

/************************************************************************************
 *
* ***********************************************************************************/
void FocusLynxBase::debugTriggered(bool enable)
{
    INDI_UNUSED(enable);
    //tty_set_debug(enable ? 1 : 0);
}

/************************************************************************************
 *
* ***********************************************************************************/
void FocusLynxBase::setFocusTarget(const char *target)
// Use to set the string of the private char[] focusTarget
{
    strncpy(focusTarget, target, 8);
}

/************************************************************************************
 *
* ***********************************************************************************/
const char *FocusLynxBase::getFocusTarget()
// Use to get the string of the private char[] focusTarget
{
    return focusTarget;
}

/************************************************************************************
 *
* ***********************************************************************************/
int FocusLynxBase::getVersion(int *major, int *minor, int *sub)
{
    INDI_UNUSED(major);
    INDI_UNUSED(minor);
    INDI_UNUSED(sub);
    /* For future use of implementation of new firmware 2.0.0
     * and give ability to keep compatible to actual 1.0.9
     * Will be to avoid calling to new functions
     * Not yet implemented in this version of the driver
     */
    char sMajor[8], sMinor[8], sSub[8];
    int  rc = sscanf(version, "%[^.].%[^.].%s", sMajor, sMinor, sSub);

    LOGF_DEBUG("Version major: %s, minor: %s, subversion: %s", sMajor, sMinor, sSub);
    *major = atoi(sMajor);
    *minor = atoi(sMinor);
    *sub = atoi(sSub);
    if (rc == 3)
        return *major;
    return 0;  // 0 Means error in this case
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::checkIfAbsoluteFocuser()
{
    const char *focusName = IUFindOnSwitch(&ModelSP)->label;
    deleteProperty(GotoSP.getName());
    deleteProperty(SyncMandatorySP.getName());

    // Check if we have absolute or relative focusers
    if (strstr(focusName, "TCF") ||
            strstr(focusName, "Leo") ||
            !strcmp(focusName, "FastFocus") ||
            !strcmp(focusName, "FeatherTouch Motor Hi-Speed"))
    {
        LOG_DEBUG("Absolute focuser detected.");
        GotoSP.resize(2);
        isAbsolute = true;
    }
    else
    {
        LOG_DEBUG("Relative focuser detected.");
        GotoSP.resize(1);

        SyncMandatorySP[0].setState(ISS_OFF);
        SyncMandatorySP[1].setState(ISS_ON);
        defineProperty(SyncMandatorySP);

        ISState syncEnabled = ISS_OFF;
        if (IUGetConfigSwitch(getDeviceName(), "SYNC MANDATORY", "Enable", &syncEnabled) == 0)
        {
            SyncMandatorySP[0].setState(syncEnabled);
            SyncMandatorySP[1].setState(syncEnabled == ISS_ON ? ISS_OFF : ISS_ON);
        }

        if (SyncMandatorySP[0].getState() == ISS_ON)
            isSynced = false;
        else
            isSynced = true;

        isAbsolute = false;
    }

    defineProperty(GotoSP);
    return isAbsolute;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxBase::SyncMandatory(bool enable)
{
    isSynced = !enable;
    return true;
}
