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
    lynxModels["FeatherTouch Motor PDMS"] = "FE";
    lynxModels["FeatherTouch Motor Hi-Speed"] = "SO";
    lynxModels["FeatherTouch Motor Hi-Torque"] = "SP";
    lynxModels["Starlight Instruments - FTM with MicroTouch"] = "SQ";
    lynxModels["Televue Focuser"] = "TA";

    ModelS = nullptr;

    focusMoveRequest = 0;
    simPosition      = 0;

    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.
    FI::SetCapability(FOCUSER_CAN_ABORT | FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_SYNC | FOCUSER_CAN_REVERSE);

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
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Enable/Disable temperature compensation
    IUFillSwitch(&TemperatureCompensateS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&TemperatureCompensateSP, TemperatureCompensateS, 2, getDeviceName(), "T. COMPENSATION", "T. Compensation",
                       FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Enable/Disable temperature compensation on start
    IUFillSwitch(&TemperatureCompensateOnStartS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateOnStartS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&TemperatureCompensateOnStartSP, TemperatureCompensateOnStartS, 2, getDeviceName(),
                       "T. COMPENSATION @START", "T. Compensation @Start", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Enable/Disable temperature Mode
    IUFillSwitch(&TemperatureCompensateModeS[0], "A", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateModeS[1], "B", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateModeS[2], "C", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateModeS[3], "D", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateModeS[4], "E", "", ISS_OFF);
    IUFillSwitchVector(&TemperatureCompensateModeSP, TemperatureCompensateModeS, 5, getDeviceName(), "COMPENSATE MODE",
                       "Compensate Mode", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&TemperatureParamN[0], "T. Coefficient", "", "%.f", -9999, 9999, 100., 0.);
    IUFillNumber(&TemperatureParamN[1], "T. Intercept", "", "%.f", -32766, 32766, 100., 0.);
    IUFillNumberVector(&TemperatureParamNP, TemperatureParamN, 2, getDeviceName(), "T. PARAMETERS", "Mode Parameters",
                       FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Enable/Disable backlash
    IUFillSwitch(&BacklashCompensationS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&BacklashCompensationS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&BacklashCompensationSP, BacklashCompensationS, 2, getDeviceName(), "BACKLASH COMPENSATION", "Backlash Compensation",
                       FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Backlash Value
    IUFillNumber(&BacklashN[0], "Steps", "", "%.f", 0, 99, 5., 0.);
    IUFillNumberVector(&BacklashNP, BacklashN, 1, getDeviceName(), "BACKLASH", "Backlash", FOCUS_SETTINGS_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Enable/Disable Sync Mandatory for relative focuser
    IUFillSwitch(&SyncMandatoryS[0], "Enable", "", isSynced == false ? ISS_ON : ISS_OFF);
    IUFillSwitch(&SyncMandatoryS[1], "Disable", "", isSynced == true ? ISS_ON : ISS_OFF);
    IUFillSwitchVector(&SyncMandatorySP, SyncMandatoryS, 2, getDeviceName(), "SYNC MANDATORY", "Sync Mandatory",
                       FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Max Travel relative focusers
    //    IUFillNumber(&MaxTravelN[0], "Ticks", "", "%.f", 0, 100000, 0., 0.);
    //    IUFillNumberVector(&MaxTravelNP, MaxTravelN, 1, getDeviceName(), "MAX TRAVEL", "Max Travel", FOCUS_SETTINGS_TAB, IP_RW, 0,
    //                       IPS_IDLE);

    // Focuser Step Size
    IUFillNumber(&StepSizeN[0], "10000*microns/step", "", "%.f", 0, 65535, 0., 0);
    IUFillNumberVector(&StepSizeNP, StepSizeN, 1, getDeviceName(), "STEP SIZE", "Step Size", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Reset to Factory setting
    IUFillSwitch(&ResetS[0], "Factory", "", ISS_OFF);
    IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "RESET", "Reset", FOCUS_SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0,
                       IPS_IDLE);

    // Go to home/center
    IUFillSwitch(&GotoS[GOTO_CENTER], "Center", "", ISS_OFF);
    IUFillSwitch(&GotoS[GOTO_HOME], "Home", "", ISS_OFF);
    IUFillSwitchVector(&GotoSP, GotoS, 2, getDeviceName(), "GOTO", "Goto", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Reverse direction
    //    IUFillSwitch(&ReverseS[0], "Enable", "", ISS_OFF);
    //    IUFillSwitch(&ReverseS[1], "Disable", "", ISS_ON);
    //    IUFillSwitchVector(&ReverseSP, ReverseS, 2, getDeviceName(), "REVERSE", "Reverse", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY,
    //                       0, IPS_IDLE);

    // List all supported models
    std::map<std::string, std::string>::iterator iter;
    int nModels = 1;
    ModelS = static_cast<ISwitch *>(malloc(sizeof(ISwitch)));
    // Need to be able to select no focuser to avoid troubles with Ekos
    IUFillSwitch(ModelS, "No Focuser", "No Focuser", ISS_ON);
    for (iter = lynxModels.begin(); iter != lynxModels.end(); ++iter)
    {
        ModelS = static_cast<ISwitch *>(realloc(ModelS, (nModels + 1) * sizeof(ISwitch)));
        IUFillSwitch(ModelS + nModels, (iter->first).c_str(), (iter->first).c_str(), ISS_OFF);

        nModels++;
    }
    IUFillSwitchVector(&ModelSP, ModelS, nModels, getDeviceName(), "MODEL", "Model", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Sync to a particular position
    IUFillNumber(&SyncN[0], "Ticks", "", "%.f", 0, 200000, 100., 0.);
    IUFillNumberVector(&SyncNP, SyncN, 1, getDeviceName(), "SYNC", "Sync", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Status indicators
    IUFillLight(&StatusL[STATUS_MOVING], "Is Moving", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_HOMING], "Is Homing", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_HOMED], "Is Homed", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_FFDETECT], "FF Detect", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_TMPPROBE], "Tmp Probe", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_REMOTEIO], "Remote IO", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_HNDCTRL], "Hnd Ctrl", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_REVERSE], "Reverse", "", IPS_IDLE);
    IUFillLightVector(&StatusLP, StatusL, 8, getDeviceName(), "STATUS", "Status", FOCUS_STATUS_TAB, IPS_IDLE);

    // Focus name configure in the HUB
    IUFillText(&HFocusNameT[0], "FocusName", "Focuser name", "");
    IUFillTextVector(&HFocusNameTP, HFocusNameT, 1, getDeviceName(), "FOCUSNAME", "Focuser", FOCUS_SETTINGS_TAB, IP_RW, 0,
                     IPS_IDLE);

    // Led intensity value
    IUFillNumber(&LedN[0], "Intensity", "", "%.f", 0, 100, 5., 0.);
    IUFillNumberVector(&LedNP, LedN, 1, getDeviceName(), "LED", "Led", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);
    //simPosition = FocusAbsPosN[0].value;

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

    defineSwitch(&ModelSP);
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
        FocusMaxPosNP.p = IP_RW;
    else
        FocusMaxPosNP.p = IP_RO;

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineText(&HFocusNameTP);

        defineNumber(&TemperatureNP);
        defineSwitch(&TemperatureCompensateModeSP);
        defineNumber(&TemperatureParamNP);
        defineSwitch(&TemperatureCompensateSP);
        defineSwitch(&TemperatureCompensateOnStartSP);

        defineSwitch(&BacklashCompensationSP);
        defineNumber(&BacklashNP);

        //defineNumber(&MaxTravelNP);

        defineNumber(&StepSizeNP);

        defineSwitch(&ResetSP);
        //defineSwitch(&ReverseSP);
        defineLight(&StatusLP);

        if (getFocusConfig() && getFocusTemp())
            LOG_INFO("FocusLynx paramaters updated, focuser ready for use.");
        else
        {
            LOG_ERROR("Failed to retrieve focuser configuration settings...");
            return false;
        }
    }
    else
    {
        deleteProperty(TemperatureNP.name);
        deleteProperty(TemperatureCompensateModeSP.name);
        deleteProperty(TemperatureCompensateSP.name);
        deleteProperty(TemperatureParamNP.name);
        deleteProperty(TemperatureCompensateOnStartSP.name);

        deleteProperty(BacklashCompensationSP.name);
        deleteProperty(BacklashNP.name);

        //deleteProperty(MaxTravelNP.name);
        deleteProperty(StepSizeNP.name);

        deleteProperty(ResetSP.name);
        deleteProperty(GotoSP.name);
        //deleteProperty(ReverseSP.name);

        deleteProperty(StatusLP.name);
        deleteProperty(HFocusNameTP.name);
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

    LOG_INFO("Error retreiving data from FocusLynx, please ensure FocusLynxBase controller is "
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
        if (strcmp(TemperatureCompensateSP.name, name) == 0)
        {
            int prevIndex = IUFindOnSwitchIndex(&TemperatureCompensateSP);
            IUUpdateSwitch(&TemperatureCompensateSP, states, names, n);
            if (setTemperatureCompensation(TemperatureCompensateS[0].s == ISS_ON))
            {
                TemperatureCompensateSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&TemperatureCompensateSP);
                TemperatureCompensateSP.s           = IPS_ALERT;
                TemperatureCompensateS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&TemperatureCompensateSP, nullptr);
            return true;
        }

        // Temperature Compensation on Start
        if (!strcmp(TemperatureCompensateOnStartSP.name, name))
        {
            int prevIndex = IUFindOnSwitchIndex(&TemperatureCompensateOnStartSP);
            IUUpdateSwitch(&TemperatureCompensateOnStartSP, states, names, n);
            if (setTemperatureCompensationOnStart(TemperatureCompensateOnStartS[0].s == ISS_ON))
            {
                TemperatureCompensateOnStartSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&TemperatureCompensateOnStartSP);
                TemperatureCompensateOnStartSP.s           = IPS_ALERT;
                TemperatureCompensateOnStartS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&TemperatureCompensateOnStartSP, nullptr);
            return true;
        }

        // Temperature Compensation Mode
        if (!strcmp(TemperatureCompensateModeSP.name, name))
        {
            int prevIndex = IUFindOnSwitchIndex(&TemperatureCompensateModeSP);
            IUUpdateSwitch(&TemperatureCompensateModeSP, states, names, n);
            char mode = IUFindOnSwitchIndex(&TemperatureCompensateModeSP) + 'A';
            if (setTemperatureCompensationMode(mode))
            {
                TemperatureCompensateModeSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&TemperatureCompensateModeSP);
                TemperatureCompensateModeSP.s           = IPS_ALERT;
                TemperatureCompensateModeS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&TemperatureCompensateModeSP, nullptr);
            return true;
        }

        // Backlash enable/disable
        if (!strcmp(BacklashCompensationSP.name, name))
        {
            int prevIndex = IUFindOnSwitchIndex(&BacklashCompensationSP);
            IUUpdateSwitch(&BacklashCompensationSP, states, names, n);
            if (setBacklashCompensation(BacklashCompensationS[0].s == ISS_ON))
            {
                BacklashCompensationSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&BacklashCompensationSP);
                BacklashCompensationSP.s           = IPS_ALERT;
                BacklashCompensationS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&BacklashCompensationSP, nullptr);
            return true;
        }

        // Reset to Factory setting
        if (strcmp(ResetSP.name, name) == 0)
        {
            IUResetSwitch(&ResetSP);
            if (resetFactory())
                ResetSP.s = IPS_OK;
            else
                ResetSP.s = IPS_ALERT;

            IDSetSwitch(&ResetSP, nullptr);
            return true;
        }

        // Go to home/center
        if (!strcmp(GotoSP.name, name))
        {
            IUUpdateSwitch(&GotoSP, states, names, n);

            if (GotoS[GOTO_HOME].s == ISS_ON)
            {
                if (home())
                    GotoSP.s = IPS_BUSY;
                else
                    GotoSP.s = IPS_ALERT;
            }
            else
            {
                if (center())
                    GotoSP.s = IPS_BUSY;
                else
                    GotoSP.s = IPS_ALERT;
            }

            IDSetSwitch(&GotoSP, nullptr);
            return true;
        }

        // Reverse Direction
        //        if (!strcmp(ReverseSP.name, name))
        //        {
        //            IUUpdateSwitch(&ReverseSP, states, names, n);

        //            if (reverse(ReverseS[0].s == ISS_ON))
        //                ReverseSP.s = IPS_OK;
        //            else
        //                ReverseSP.s = IPS_ALERT;

        //            IDSetSwitch(&ReverseSP, nullptr);
        //            return true;
        //        }

        // Sync Mandatory
        if (!strcmp(SyncMandatorySP.name, name))
        {
            IUUpdateSwitch(&SyncMandatorySP, states, names, n);

            if (SyncMandatory(SyncMandatoryS[0].s == ISS_ON))
                SyncMandatorySP.s = IPS_OK;
            else
                SyncMandatorySP.s = IPS_ALERT;

            IDSetSwitch(&SyncMandatorySP, nullptr);
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
        if (!strcmp(name, HFocusNameTP.name))
        {
            IUUpdateText(&HFocusNameTP, texts, names, n);
            if (setDeviceNickname(HFocusNameT[0].text))
                HFocusNameTP.s = IPS_OK;
            else
                HFocusNameTP.s = IPS_ALERT;
            IDSetText(&HFocusNameTP, nullptr);
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
        if (!strcmp(TemperatureParamNP.name, name))
        {
            IUUpdateNumber(&TemperatureParamNP, values, names, n);

            char mode = static_cast<char>(65 + IUFindOnSwitchIndex(&TemperatureCompensateModeSP));
            if (!setTemperatureCompensationCoeff(mode, TemperatureParamN[0].value) ||
                    !setTemperatureInceptions(mode, TemperatureParamN[1].value))
            {
                LOG_ERROR("Failed to write temperature coefficient or intercept");
                TemperatureParamNP.s = IPS_ALERT;
                IDSetNumber(&TemperatureParamNP, nullptr);
                return false;
            }

            TemperatureParamNP.s = IPS_OK;
            getFocusTemp();

            return true;
        }

        // Backlash Value
        if (!strcmp(BacklashNP.name, name))
        {
            IUUpdateNumber(&BacklashNP, values, names, n);
            if (setBacklashCompensationSteps(BacklashN[0].value) == false)
            {
                LOG_ERROR("Failed to set temperature coefficients.");
                BacklashNP.s = IPS_ALERT;
                IDSetNumber(&BacklashNP, nullptr);
                return false;
            }

            BacklashNP.s = IPS_OK;
            IDSetNumber(&BacklashNP, nullptr);
            return true;
        }

        // Sync
        //        if (strcmp(SyncNP.name, name) == 0)
        //        {
        //            IUUpdateNumber(&SyncNP, values, names, n);
        //            if (sync(SyncN[0].value) == false)
        //            {
        //                LOG_ERROR("Failed to set the actual value.");
        //                SyncNP.s = IPS_ALERT;
        //                IDSetNumber(&SyncNP, nullptr);
        //                return false;
        //            }
        //            else
        //                SyncNP.s = IPS_OK;

        //            IDSetNumber(&SyncNP, nullptr);
        //            return true;
        //        }

        // StepSize
        if (strcmp(StepSizeNP.name, name) == 0)
        {
            IUUpdateNumber(&StepSizeNP, values, names, n);
            if (setStepSize(StepSizeN[0].value) == false)
            {
                LOG_ERROR("Failed to set the actual value.");
                StepSizeNP.s = IPS_ALERT;
                IDSetNumber(&StepSizeNP, nullptr);
                return false;
            }
            else
                StepSizeNP.s = IPS_OK;

            IDSetNumber(&StepSizeNP, nullptr);
            return true;
        }

        // Max Travel if relative focusers
        //        if (strcmp(MaxTravelNP.name, name) == 0)
        //        {
        //            IUUpdateNumber(&MaxTravelNP, values, names, n);

        //            if (setMaxTravel(MaxTravelN[0].value) == false)
        //                MaxTravelNP.s = IPS_ALERT;
        //            else
        //            {
        //                MaxTravelNP.s = IPS_OK;
        //                FocusAbsPosN[0].max = SyncN[0].max = MaxTravelN[0].value;
        //                FocusAbsPosN[0].step = SyncN[0].step = (MaxTravelN[0].value / 50.0);

        //                IUUpdateMinMax(&FocusAbsPosNP);
        //                IUUpdateMinMax(&SyncNP);

        //                IDSetNumber(&MaxTravelNP, nullptr);

        //                LOGF_INFO("Focuser absolute limits: min (%g) max (%g)", FocusAbsPosN[0].min,
        //                       FocusAbsPosN[0].max);
        //            }
        //            return true;
        //        }

        // Set LED intensity to the HUB itself via function setLedLevel()
        if (!strcmp(LedNP.name, name))
        {
            IUUpdateNumber(&LedNP, values, names, n);
            if (setLedLevel(LedN[0].value))
                LedNP.s = IPS_OK;
            else
                LedNP.s = IPS_ALERT;
            LOGF_INFO("Focuser LED level intensity : %f", LedN[0].value);
            IDSetNumber(&LedNP, nullptr);
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

    IUSaveText(&HFocusNameT[0], nickname);
    IDSetText(&HFocusNameTP, nullptr);

    HFocusNameTP.s = IPS_OK;
    IDSetText(&HFocusNameTP, nullptr);

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
        FocusAbsPosN[0].max = SyncN[0].max = maxPos;
        FocusAbsPosN[0].step = SyncN[0].step = maxPos / 50.0;
        FocusAbsPosN[0].min = SyncN[0].min = 0;

        FocusRelPosN[0].max  = maxPos / 2;
        FocusRelPosN[0].step = maxPos / 100.0;
        FocusRelPosN[0].min  = 0;

        IUUpdateMinMax(&FocusAbsPosNP);
        IUUpdateMinMax(&FocusRelPosNP);
        IUUpdateMinMax(&SyncNP);

        FocusMaxPosNP.s = IPS_OK;
        FocusMaxPosN[0].value = maxPos;
        IDSetNumber(&FocusMaxPosNP, nullptr);

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
        snprintf(response, 32, "BLC En = %d\n", BacklashCompensationS[0].s == ISS_ON ? 1 : 0);
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

    IUResetSwitch(&BacklashCompensationSP);
    BacklashCompensationS[0].s = BLCCompensate ? ISS_ON : ISS_OFF;
    BacklashCompensationS[1].s = BLCCompensate ? ISS_OFF : ISS_ON;
    BacklashCompensationSP.s   = IPS_OK;
    IDSetSwitch(&BacklashCompensationSP, nullptr);

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

    BacklashN[0].value = BLCValue;
    BacklashNP.s       = IPS_OK;
    IDSetNumber(&BacklashNP, nullptr);

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

    LedN[0].value = LEDBrightness;
    LedNP.s       = IPS_OK;
    IDSetNumber(&LedNP, nullptr);

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
            strncpy(response, "Temp(C) = +21.7\n", 16);
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

        float temperature = 0;
        int rc            = sscanf(response, "%16[^=]=%f", key, &temperature);
        if (rc == 2)
        {
            TemperatureN[0].value = temperature;
            IDSetNumber(&TemperatureNP, nullptr);
        }
        else
        {
            char np[8];
            int rc = sscanf(response, "%16[^=]= %s", key, np);

            if (rc != 2 || strcmp(np, "NP"))
            {
                if (TemperatureNP.s != IPS_ALERT)
                {
                    TemperatureNP.s = IPS_ALERT;
                    IDSetNumber(&TemperatureNP, nullptr);
                }
                return false;
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
            FocusAbsPosN[0].value = currPos;
            IDSetNumber(&FocusAbsPosNP, nullptr);
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

        StatusL[STATUS_MOVING].s = isMoving ? IPS_BUSY : IPS_IDLE;

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

        StatusL[STATUS_HOMING].s = _isHoming ? IPS_BUSY : IPS_IDLE;
        // For relative focusers home is not applicable.
        if (isAbsolute == false)
            StatusL[STATUS_HOMING].s = IPS_IDLE;

        // We set that isHoming in process, but we don't set it to false here it must be reset in TimerHit
        if (StatusL[STATUS_HOMING].s == IPS_BUSY)
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

        StatusL[STATUS_HOMED].s = isHomed ? IPS_OK : IPS_IDLE;
        // For relative focusers home is not applicable.
        if (isAbsolute == false)
            StatusL[STATUS_HOMED].s = IPS_IDLE;

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

        StatusL[STATUS_FFDETECT].s = FFDetect ? IPS_OK : IPS_IDLE;

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

        StatusL[STATUS_TMPPROBE].s = TmpProbe ? IPS_OK : IPS_IDLE;

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

        StatusL[STATUS_REMOTEIO].s = RemoteIO ? IPS_OK : IPS_IDLE;

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

        StatusL[STATUS_HNDCTRL].s = HndCtlr ? IPS_OK : IPS_IDLE;

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

        StatusL[STATUS_REVERSE].s = reverse ? IPS_OK : IPS_IDLE;

        // If reverse is enable and switch shows disabled, let's change that
        // same thing is reverse is disabled but switch is enabled
        if ((reverse && FocusReverseS[1].s == ISS_ON) || (!reverse && FocusReverseS[0].s == ISS_ON))
        {
            IUResetSwitch(&FocusReverseSP);
            FocusReverseS[0].s = (reverse == 1) ? ISS_ON : ISS_OFF;
            FocusReverseS[1].s = (reverse == 0) ? ISS_ON : ISS_OFF;
            IDSetSwitch(&FocusReverseSP, nullptr);
        }

        StatusLP.s = IPS_OK;
        IDSetLight(&StatusLP, nullptr);

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
            snprintf(response, 32, "TComp ON = %d\n", TemperatureCompensateS[0].s == ISS_ON ? 1 : 0);
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

        IUResetSwitch(&TemperatureCompensateSP);
        TemperatureCompensateS[0].s = TCompOn ? ISS_ON : ISS_OFF;
        TemperatureCompensateS[1].s = TCompOn ? ISS_OFF : ISS_ON;
        TemperatureCompensateSP.s   = IPS_OK;
        IDSetSwitch(&TemperatureCompensateSP, nullptr);

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
            if (rc == 1 && key[0] == 'T'){
                //If the controller does not support this it could be null. Assume A mode in this case.
                compensateMode = 'A';
            }
            else{
                return false;
            }
        }

        IUResetSwitch(&TemperatureCompensateModeSP);
        int index = compensateMode - 'A';
        if (index >= 0 && index <= 5)
        {
            TemperatureCompensateModeS[index].s = ISS_ON;
            TemperatureCompensateModeSP.s       = IPS_OK;
        }
        else
        {
            LOGF_ERROR("Invalid index %d for compensation mode.", index);
            TemperatureCompensateModeSP.s = IPS_ALERT;
        }

        IDSetSwitch(&TemperatureCompensateModeSP, nullptr);


        // Temperature Compensation on Start
        memset(response, 0, sizeof(response));
        if (isSimulation())
        {
            snprintf(response, 32, "TC@Start = %d\n", TemperatureCompensateOnStartS[0].s == ISS_ON ? 1 : 0);
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

        IUResetSwitch(&TemperatureCompensateOnStartSP);
        TemperatureCompensateOnStartS[0].s = TCOnStart ? ISS_ON : ISS_OFF;
        TemperatureCompensateOnStartS[1].s = TCOnStart ? ISS_OFF : ISS_ON;
        TemperatureCompensateOnStartSP.s   = IPS_OK;
        IDSetSwitch(&TemperatureCompensateOnStartSP, nullptr);

        // Temperature Coeff A
        if (isSimulation())
        {
            snprintf(response, 32, "TempCo A = %d\n", static_cast<int>(TemperatureParamN[0].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (TemperatureCompensateModeS[0].s == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TCoeff;
            rc = sscanf(response, "%16[^=]=%d", key, &TCoeff);
            if (rc != 2)
                return false;

            TemperatureParamN[0].value = TCoeff;
        }
        memset(response, 0, sizeof(response));

        // Temperature Coeff B
        if (isSimulation())
        {
            snprintf(response, 32, "TempCo B = %d\n", static_cast<int>(TemperatureParamN[0].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeS[1].s == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TCoeff;
            rc = sscanf(response, "%16[^=]=%d", key, &TCoeff);
            if (rc != 2)
                return false;

            TemperatureParamN[0].value = TCoeff;
        }

        memset(response, 0, sizeof(response));

        // Temperature Coeff C
        if (isSimulation())
        {
            snprintf(response, 32, "TempCo C = %d\n", static_cast<int>(TemperatureParamN[0].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeS[2].s == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TCoeff;
            rc = sscanf(response, "%16[^=]=%d", key, &TCoeff);
            if (rc != 2)
                return false;

            TemperatureParamN[0].value = TCoeff;
        }

        memset(response, 0, sizeof(response));

        // Temperature Coeff D
        if (isSimulation())
        {
            snprintf(response, 32, "TempCo D = %d\n", static_cast<int>(TemperatureParamN[0].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeS[3].s == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TCoeff;
            rc = sscanf(response, "%16[^=]=%d", key, &TCoeff);
            if (rc != 2)
                return false;

            TemperatureParamN[0].value = TCoeff;
        }

        memset(response, 0, sizeof(response));

        // Temperature Coeff E
        if (isSimulation())
        {
            snprintf(response, 32, "TempCo E = %d\n", static_cast<int>(TemperatureParamN[0].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeS[4].s == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TCoeff;
            rc = sscanf(response, "%16[^=]=%d", key, &TCoeff);
            if (rc != 2)
                return false;

            TemperatureParamN[0].value = TCoeff;
        }

        memset(response, 0, sizeof(response));

        // Temperature intercepts A
        if (isSimulation())
        {
            snprintf(response, 32, "TempIn A = %d\n", static_cast<int>(TemperatureParamN[1].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeS[0].s == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TInter;
            rc = sscanf(response, "%16[^=]=%d", key, &TInter);
            if (rc != 2)
                return false;

            TemperatureParamN[1].value = TInter;
        }

        memset(response, 0, sizeof(response));

        // Temperature intercepts B
        if (isSimulation())
        {
            snprintf(response, 32, "TempIn B = %d\n", static_cast<int>(TemperatureParamN[1].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeS[1].s == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TInter;
            rc = sscanf(response, "%16[^=]=%d", key, &TInter);
            if (rc != 2)
                return false;

            TemperatureParamN[1].value = TInter;
        }

        memset(response, 0, sizeof(response));

        // Temperature intercepts C
        if (isSimulation())
        {
            snprintf(response, 32, "TempIn C = %d\n", static_cast<int>(TemperatureParamN[1].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeS[2].s == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TInter;
            rc = sscanf(response, "%16[^=]=%d", key, &TInter);
            if (rc != 2)
                return false;

            TemperatureParamN[1].value = TInter;
        }

        memset(response, 0, sizeof(response));

        // Temperature intercepts D
        if (isSimulation())
        {
            snprintf(response, 32, "TempIn D = %d\n", static_cast<int>(TemperatureParamN[1].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeS[3].s == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TInter;
            rc = sscanf(response, "%16[^=]=%d", key, &TInter);
            if (rc != 2)
                return false;

            TemperatureParamN[1].value = TInter;
        }

        memset(response, 0, sizeof(response));

        // Temperature intercepts E
        if (isSimulation())
        {
            snprintf(response, 32, "TempIn E = %d\n", static_cast<int>(TemperatureParamN[1].value));
            nbytes_read = strlen(response);
        }
        else if ((errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
        if (TemperatureCompensateModeS[4].s == ISS_ON)
        {
            response[nbytes_read - 1] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            int TInter;
            rc = sscanf(response, "%16[^=]=%d", key, &TInter);
            if (rc != 2)
                return false;

            TemperatureParamN[1].value = TInter;
        }

        TemperatureParamNP.s = IPS_OK;
        IDSetNumber(&TemperatureParamNP, nullptr);

        memset(response, 0, sizeof(response));

        // StepSize
        if (isSimulation())
        {
            snprintf(response, 32, "StepSize = %d\n", static_cast<int>(StepSizeN[0].value));
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

        StepSizeN[0].value = valueStepSize;
        IDSetNumber(&StepSizeNP, nullptr);

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
        //FocusAbsPosN[0].value = MaxTravelN[0].value;
        FocusAbsPosNP.s = IPS_OK;
        IDSetNumber(&FocusAbsPosNP, nullptr);
        simStatus[STATUS_HOMING] = ISS_ON;
        simStatus[STATUS_HOMED] = ISS_OFF;
        simPosition = FocusAbsPosN[0].value;
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
        FocusAbsPosNP.s = IPS_BUSY;
        IDSetNumber(&FocusAbsPosNP, nullptr);

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
        return (MoveAbsFocuser(FocusAbsPosN[0].max / 2) != IPS_ALERT);

    memset(response, 0, sizeof(response));

    snprintf(cmd, LYNX_MAX, "<%sCENTER>", getFocusTarget());
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "M", 16);
        nbytes_read              = strlen(response) + 1;
        simStatus[STATUS_MOVING] = ISS_ON;
        targetPosition           = FocusAbsPosN[0].max / 2;
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

        FocusAbsPosNP.s = IPS_BUSY;
        IDSetNumber(&FocusAbsPosNP, nullptr);

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
bool FocusLynxBase::setBacklashCompensation(bool enable)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[LYNX_MAX] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<%sSCBE%d>", getFocusTarget(), enable ? 1 : 0);

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
bool FocusLynxBase::setBacklashCompensationSteps(uint16_t steps)
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

        if (duration <= POLLMS)
        {
            usleep(POLLMS * 1000);
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

        FocusAbsPosNP.s = IPS_BUSY;

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
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

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
        SetTimer(POLLMS);
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
        SetTimer(POLLMS);
        return;
    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (isSimulation())
        {
            if (FocusAbsPosN[0].value < targetPosition)
                simPosition += 100;
            else
                simPosition -= 100;

            simStatus[STATUS_MOVING] = ISS_ON;

            if (std::abs(static_cast<int64_t>(simPosition) - static_cast<int64_t>(targetPosition)) < 100)
            {
                FocusAbsPosN[0].value    = targetPosition;
                simPosition              = FocusAbsPosN[0].value;
                simStatus[STATUS_MOVING] = ISS_OFF;
                StatusL[STATUS_MOVING].s = IPS_IDLE;
                if (simStatus[STATUS_HOMING] == ISS_ON)
                {
                    StatusL[STATUS_HOMED].s = IPS_OK;
                    StatusL[STATUS_HOMING].s = IPS_IDLE;
                    simStatus[STATUS_HOMING] = ISS_OFF;
                    simStatus[STATUS_HOMED] = ISS_ON;
                }
            }
            else
                StatusL[STATUS_MOVING].s = IPS_BUSY;
            IDSetLight(&StatusLP, nullptr);
        }

        if (isHoming && StatusL[STATUS_HOMED].s == IPS_OK)
        {
            isHoming = false;
            GotoSP.s = IPS_OK;
            IUResetSwitch(&GotoSP);
            GotoS[GOTO_HOME].s = ISS_ON;
            IDSetSwitch(&GotoSP, nullptr);
            FocusAbsPosNP.s = IPS_OK;
            IDSetNumber(&FocusRelPosNP, nullptr);
            LOG_INFO("Focuser reached home position.");
            if (isSimulation())
                center();
        }
        else if (StatusL[STATUS_MOVING].s == IPS_IDLE)
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            IDSetNumber(&FocusRelPosNP, nullptr);
            if (GotoSP.s == IPS_BUSY)
            {
                IUResetSwitch(&GotoSP);
                GotoSP.s = IPS_OK;
                IDSetSwitch(&GotoSP, nullptr);
            }
            LOG_INFO("Focuser reached requested position.");
        }
        else if (StatusL[STATUS_MOVING].s == IPS_BUSY && focusMoveRequest > 0)
        {
            float remaining = calcTimeLeft(focusMoveStart, focusMoveRequest);

            if (remaining < POLLMS)
            {
                sleep(remaining);
                AbortFocuser();
                focusMoveRequest = 0;
            }
        }
    }
    if (StatusL[STATUS_HOMING].s == IPS_BUSY && GotoSP.s != IPS_BUSY)
    {
        GotoSP.s = IPS_BUSY;
        IDSetSwitch(&GotoSP, nullptr);
    }

    SetTimer(POLLMS);
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

        if (FocusRelPosNP.s == IPS_BUSY)
        {
            FocusRelPosNP.s = IPS_IDLE;
            IDSetNumber(&FocusRelPosNP, nullptr);
        }

        FocusTimerNP.s = FocusAbsPosNP.s = GotoSP.s = IPS_IDLE;
        IUResetSwitch(&GotoSP);
        IDSetNumber(&FocusTimerNP, nullptr);
        IDSetNumber(&FocusAbsPosNP, nullptr);
        IDSetSwitch(&GotoSP, nullptr);

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
    IUSaveConfigSwitch(fp, &TemperatureCompensateSP);
    IUSaveConfigSwitch(fp, &TemperatureCompensateOnStartSP);
    //IUSaveConfigSwitch(fp, &ReverseSP);
    IUSaveConfigNumber(fp, &TemperatureNP);
    IUSaveConfigSwitch(fp, &TemperatureCompensateModeSP);
    IUSaveConfigSwitch(fp, &BacklashCompensationSP);
    IUSaveConfigNumber(fp, &BacklashNP);
    IUSaveConfigNumber(fp, &StepSizeNP);
    if (isAbsolute == false)
    {
        //IUSaveConfigNumber(fp, &MaxTravelNP);
        IUSaveConfigSwitch(fp, &SyncMandatorySP);
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
            result = (INDI::DefaultDevice::loadConfig(silent, "BACKLASH COMPENSATION") && result);
            result = (INDI::DefaultDevice::loadConfig(silent, "BACKLASH") && result);
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
    deleteProperty(GotoSP.name);
    deleteProperty(SyncMandatorySP.name);

    // Check if we have absolute or relative focusers
    if (strstr(focusName, "TCF") || strstr(focusName, "Leo") ||
            !strcmp(focusName, "FastFocus") || !strcmp(focusName, "FeatherTouch Motor Hi-Speed"))
    {
        LOG_DEBUG("Absolute focuser detected.");
        GotoSP.nsp = 2;
        isAbsolute = true;
        deleteProperty(SyncNP.name);
    }
    else
    {
        LOG_DEBUG("Relative focuser detected.");
        GotoSP.nsp = 1;
        defineNumber(&SyncNP);
        SyncMandatoryS[0].s = ISS_OFF;
        SyncMandatoryS[1].s = ISS_ON;
        defineSwitch(&SyncMandatorySP);
        INDI::DefaultDevice::loadConfig(true, "SYNC MANDATORY");
        if (SyncMandatoryS[0].s == ISS_ON)
            isSynced = false;
        else isSynced = true;
        isAbsolute = false;
    }
    defineSwitch(&GotoSP);

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
