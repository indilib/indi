/*
  Optec Gemini Focuser Rotator INDI driver
  Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "gemini.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <termios.h>
#include <unistd.h>

#define GEMINI_MAX_RETRIES        1
#define GEMINI_TIMEOUT            3
#define GEMINI_MAXBUF             16
#define GEMINI_TEMPERATURE_FREQ   20 /* Update every 20 POLLMS cycles. For POLLMS 500ms = 10 seconds freq */
#define GEMINI_POSITION_THRESHOLD 5  /* Only send position updates to client if the diff exceeds 5 steps */

#define FOCUS_SETTINGS_TAB "Settings"
#define STATUS_TAB   "Status"
#define ROTATOR_TAB "Rotator"
#define HUB_TAB "Hub"

static std::unique_ptr<Gemini> geminiFR(new Gemini());

/************************************************************************************
 *
* ***********************************************************************************/
Gemini::Gemini() : RotatorInterface(this)
{
    focusMoveRequest = 0;
    focuserSimPosition      = 0;

    // Can move in Absolute & Relative motions and can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABORT | FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_HAS_BACKLASH);

    // Rotator capabilities
    RI::SetCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_HOME | ROTATOR_CAN_REVERSE | ROTATOR_HAS_BACKLASH);

    isFocuserAbsolute = true;
    isFocuserHoming   = false;

    focuserSimStatus[STATUS_MOVING]   = ISS_OFF;
    focuserSimStatus[STATUS_HOMING]   = ISS_OFF;
    focuserSimStatus[STATUS_HOMED]    = ISS_OFF;
    focuserSimStatus[STATUS_FFDETECT] = ISS_OFF;
    focuserSimStatus[STATUS_TMPPROBE] = ISS_ON;
    focuserSimStatus[STATUS_REMOTEIO] = ISS_ON;
    focuserSimStatus[STATUS_HNDCTRL]  = ISS_ON;
    focuserSimStatus[STATUS_REVERSE]  = ISS_OFF;

    DBG_FOCUS = INDI::Logger::getInstance().addDebugLevel("Verbose", "Verbose");
}

/************************************************************************************
 *
* ***********************************************************************************/
Gemini::~Gemini()
{
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::initProperties()
{
    INDI::Focuser::initProperties();

    ////////////////////////////////////////////////////////////
    // Focuser Properties
    ///////////////////////////////////////////////////////////

    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Enable/Disable temperature compensation
    TemperatureCompensateSP[INDI_ENABLED].fill("Enable", "", ISS_OFF);
    TemperatureCompensateSP[INDI_DISABLED].fill("Disable", "", ISS_ON);
    TemperatureCompensateSP.fill(getDeviceName(), "T. Compensation", "",
                                 FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Enable/Disable temperature compensation on start
    TemperatureCompensateOnStartSP[INDI_ENABLED].fill("Enable", "", ISS_OFF);
    TemperatureCompensateOnStartSP[INDI_DISABLED].fill("Disable", "", ISS_ON);
    TemperatureCompensateOnStartSP.fill(getDeviceName(),
                                        "T. Compensation @Start", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Temperature Coefficient
    TemperatureCoeffNP[A].fill("A", "", "%.f", -9999, 9999, 100., 0.);
    TemperatureCoeffNP[B].fill("B", "", "%.f", -9999, 9999, 100., 0.);
    TemperatureCoeffNP[C].fill("C", "", "%.f", -9999, 9999, 100., 0.);
    TemperatureCoeffNP[D].fill("D", "", "%.f", -9999, 9999, 100., 0.);
    TemperatureCoeffNP[E].fill("E", "", "%.f", -9999, 9999, 100., 0.);
    TemperatureCoeffNP.fill(getDeviceName(), "T. Coeff", "", FOCUS_SETTINGS_TAB,
                            IP_RW, 0, IPS_IDLE);

    // Enable/Disable Home on Start
    FocuserHomeOnStartSP[INDI_ENABLED].fill("Enable", "", ISS_OFF);
    FocuserHomeOnStartSP[INDI_DISABLED].fill("Disable", "", ISS_ON);
    FocuserHomeOnStartSP.fill(getDeviceName(), "FOCUSER_HOME_ON_START", "Home on Start",
                              FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Enable/Disable temperature Mode
    TemperatureCompensateModeSP[A].fill("A", "", ISS_OFF);
    TemperatureCompensateModeSP[B].fill("B", "", ISS_OFF);
    TemperatureCompensateModeSP[C].fill( "C", "", ISS_OFF);
    TemperatureCompensateModeSP[D].fill( "D", "", ISS_OFF);
    TemperatureCompensateModeSP[E].fill( "E", "", ISS_OFF);
    TemperatureCompensateModeSP.fill(getDeviceName(), "Compensate Mode",
                                     "", FOCUS_SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    //    // Enable/Disable backlash
    //    IUFillSwitch(&FocusBacklashS[0], "Enable", "", ISS_OFF);
    //    IUFillSwitch(&FocusBacklashS[1], "Disable", "", ISS_ON);
    //    IUFillSwitchVector(&FocusBacklashSP, FocusBacklashS, 2, getDeviceName(), "FOCUSER_BACKLASH_COMPENSATION", "Backlash Compensation",
    //                       FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    //    // Backlash Value
    //    IUFillNumber(&FocusBacklashN[0], "Value", "", "%.f", 0, 99, 5., 0.);
    //    IUFillNumberVector(&FocusBacklashNP, FocusBacklashN, 1, getDeviceName(), "FOCUSER_BACKLASH", "Backlash", FOCUS_SETTINGS_TAB, IP_RW, 0,
    //                       IPS_IDLE);

    // Go to home/center
    FocuserGotoSP[GOTO_CENTER].fill("Center", "", ISS_OFF);
    FocuserGotoSP[GOTO_HOME].fill("Home", "", ISS_OFF);
    FocuserGotoSP.fill(getDeviceName(), "FOCUSER_GOTO", "Goto", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Focus Status indicators
    FocuserStatusLP[STATUS_MOVING].fill("Is Moving", "", IPS_IDLE);
    FocuserStatusLP[STATUS_HOMING].fill("Is Homing", "", IPS_IDLE);
    FocuserStatusLP[STATUS_HOMED].fill("Is Homed", "", IPS_IDLE);
    FocuserStatusLP[STATUS_FFDETECT].fill("FF Detect", "", IPS_IDLE);
    FocuserStatusLP[STATUS_TMPPROBE].fill("Tmp Probe", "", IPS_IDLE);
    FocuserStatusLP[STATUS_REMOTEIO].fill("Remote IO", "", IPS_IDLE);
    FocuserStatusLP[STATUS_HNDCTRL].fill("Hnd Ctrl", "", IPS_IDLE);
    FocuserStatusLP[STATUS_REVERSE].fill("Reverse", "", IPS_IDLE);
    FocuserStatusLP.fill(getDeviceName(), "FOCUSER_STATUS", "Focuser", STATUS_TAB, IPS_IDLE);

    ////////////////////////////////////////////////////////////
    // Rotator Properties
    ///////////////////////////////////////////////////////////

    // Enable/Disable Home on Start
    RotatorHomeOnStartSP[INDI_ENABLED].fill("Enable", "", ISS_OFF);
    RotatorHomeOnStartSP[INDI_DISABLED].fill("Disable", "", ISS_ON);
    RotatorHomeOnStartSP.fill(getDeviceName(), "ROTATOR_HOME_ON_START", "Home on Start",
                              ROTATOR_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Rotator Status indicators
    RotatorStatusLP[STATUS_MOVING].fill("Is Moving", "", IPS_IDLE);
    RotatorStatusLP[STATUS_HOMING].fill("Is Homing", "", IPS_IDLE);
    RotatorStatusLP[STATUS_HOMED].fill("Is Homed", "", IPS_IDLE);
    RotatorStatusLP[STATUS_FFDETECT].fill("FF Detect", "", IPS_IDLE);
    RotatorStatusLP[STATUS_TMPPROBE].fill("Tmp Probe", "", IPS_IDLE);
    RotatorStatusLP[STATUS_REMOTEIO].fill("Remote IO", "", IPS_IDLE);
    RotatorStatusLP[STATUS_HNDCTRL].fill("Hnd Ctrl", "", IPS_IDLE);
    RotatorStatusLP[STATUS_REVERSE].fill("Reverse", "", IPS_IDLE);
    RotatorStatusLP.fill(getDeviceName(), "ROTATOR_STATUS", "Rotator", STATUS_TAB, IPS_IDLE);

    INDI::RotatorInterface::initProperties(ROTATOR_TAB);

    // Rotator Ticks
    RotatorAbsPosNP[0].fill("ROTATOR_ABSOLUTE_POSITION", "Ticks", "%.f", 0., 0., 0., 0.);
    RotatorAbsPosNP.fill(getDeviceName(), "ABS_ROTATOR_POSITION", "Goto", ROTATOR_TAB, IP_RW,
                         0, IPS_IDLE );
#if 0


    // Rotator Degree
    IUFillNumber(&RotatorAbsAngleN[0], "ANGLE", "Degrees", "%.2f", 0, 360., 10., 0.);
    IUFillNumberVector(&RotatorAbsAngleNP, RotatorAbsAngleN, 1, getDeviceName(), "ABS_ROTATOR_ANGLE", "Angle", ROTATOR_TAB,
                       IP_RW, 0, IPS_IDLE );

    // Abort Rotator
    IUFillSwitch(&AbortRotatorS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&AbortRotatorSP, AbortRotatorS, 1, getDeviceName(), "ROTATOR_ABORT_MOTION", "Abort Motion", ROTATOR_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Rotator Go to home/center
    IUFillSwitch(&RotatorGotoS[GOTO_CENTER], "Center", "", ISS_OFF);
    IUFillSwitch(&RotatorGotoS[GOTO_HOME], "Home", "", ISS_OFF);
    IUFillSwitchVector(&RotatorGotoSP, RotatorGotoS, 2, getDeviceName(), "ROTATOR_GOTO", "Goto", ROTATOR_TAB, IP_RW,
                       ISR_1OFMANY, 0,
                       IPS_IDLE);
#endif

    ////////////////////////////////////////////////////////////
    // Hub Properties
    ///////////////////////////////////////////////////////////

    // Focus name configure in the HUB
    HFocusNameTP[DEVICE_FOCUSER].fill("FocusName", "Focuser name", "");
    HFocusNameTP[DEVICE_ROTATOR].fill("RotatorName", "Rotator name", "");
    HFocusNameTP.fill(getDeviceName(), "HUBNAMES", "HUB", HUB_TAB, IP_RW, 0,
                      IPS_IDLE);

    // Led intensity value
    LedNP[0].fill("Intensity", "", "%.f", 0, 100, 5., 0.);
    LedNP.fill(getDeviceName(), "Led", "", HUB_TAB, IP_RW, 0, IPS_IDLE);

    // Reset to Factory setting
    ResetSP[0].fill("Factory", "", ISS_OFF);
    ResetSP.fill(getDeviceName(), "Reset", "", HUB_TAB, IP_RW, ISR_ATMOST1, 0,
                 IPS_IDLE);

    addAuxControls();

    setDriverInterface(getDriverInterface() | ROTATOR_INTERFACE);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        // Focuser Properties
        defineProperty(TemperatureNP);
        defineProperty(TemperatureCoeffNP);
        defineProperty(TemperatureCompensateModeSP);
        defineProperty(TemperatureCompensateSP);
        defineProperty(TemperatureCompensateOnStartSP);
        //        defineProperty(&FocusBacklashSP);
        //        defineProperty(&FocusBacklashNP);
        defineProperty(FocuserHomeOnStartSP);
        defineProperty(FocuserGotoSP);
        defineProperty(FocuserStatusLP);

        // Rotator Properties
        INDI::RotatorInterface::updateProperties();
        /*

        defineProperty(&RotatorAbsAngleNP);
        defineProperty(&AbortRotatorSP);
        defineProperty(&RotatorGotoSP);
        defineProperty(&ReverseRotatorSP);
        */
        defineProperty(RotatorAbsPosNP);
        defineProperty(RotatorHomeOnStartSP);
        defineProperty(RotatorStatusLP);

        // Hub Properties
        defineProperty(HFocusNameTP);
        defineProperty(ResetSP);
        defineProperty(LedNP);

        if (getFocusConfig() && getRotatorConfig())
            LOG_INFO("Gemini parameters updated, rotating focuser ready for use.");
        else
        {
            LOG_ERROR("Failed to retrieve rotating focuser configuration settings...");
            return false;
        }
    }
    else
    {
        // Focuser Properties
        deleteProperty(TemperatureNP);
        deleteProperty(TemperatureCoeffNP);
        deleteProperty(TemperatureCompensateModeSP);
        deleteProperty(TemperatureCompensateSP);
        deleteProperty(TemperatureCompensateOnStartSP);
        //        deleteProperty(FocusBacklashSP.name);
        //        deleteProperty(FocusBacklashNP.name);
        deleteProperty(FocuserGotoSP);
        deleteProperty(FocuserHomeOnStartSP);
        deleteProperty(FocuserStatusLP);

        // Rotator Properties
        INDI::RotatorInterface::updateProperties();
        /*
        deleteProperty(RotatorAbsAngleNP.name);
        deleteProperty(AbortRotatorSP.name);
        deleteProperty(RotatorGotoSP.name);
        deleteProperty(ReverseRotatorSP.name);
        */

        deleteProperty(RotatorAbsPosNP);
        deleteProperty(RotatorHomeOnStartSP);

        deleteProperty(RotatorStatusLP);

        // Hub Properties
        deleteProperty(HFocusNameTP);
        deleteProperty(LedNP);
        deleteProperty(ResetSP);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::Handshake()
{
    if (ack())
    {
        LOG_INFO("Gemini is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO("Error retrieving data from Gemini, please ensure Gemini controller is "
             "powered and the port is correct.");
    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
const char *Gemini::getDefaultName()
{
    // Has to be overridden by child instance
    return "Gemini Focusing Rotator";
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Temperature Compensation
        if (TemperatureCompensateSP.isNameMatch(name))
        {
            int prevIndex = TemperatureCompensateSP.findOnSwitchIndex();
            TemperatureCompensateSP.update(states, names, n);
            if (setTemperatureCompensation(TemperatureCompensateSP[INDI_ENABLED].getState() == ISS_ON))
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
            if (setTemperatureCompensationOnStart(TemperatureCompensateOnStartSP[INDI_ENABLED].getState() == ISS_ON))
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

        // Focuser Home on Start Enable/Disable
        if (FocuserHomeOnStartSP.isNameMatch(name))
        {
            int prevIndex = FocuserHomeOnStartSP.findOnSwitchIndex();
            FocuserHomeOnStartSP.update(states, names, n);
            if (homeOnStart(DEVICE_FOCUSER, FocuserHomeOnStartSP[INDI_ENABLED].getState() == ISS_ON))
            {
                FocuserHomeOnStartSP.setState(IPS_OK);
            }
            else
            {
                FocuserHomeOnStartSP.reset();
                FocuserHomeOnStartSP.setState(IPS_ALERT);
                FocuserHomeOnStartSP[prevIndex].setState(ISS_ON);
            }

            FocuserHomeOnStartSP.apply();
            return true;
        }

        // Rotator Home on Start Enable/Disable
        if (RotatorHomeOnStartSP.isNameMatch(name))
        {
            int prevIndex = RotatorHomeOnStartSP.findOnSwitchIndex();
            RotatorHomeOnStartSP.update(states, names, n);
            if (homeOnStart(DEVICE_ROTATOR, RotatorHomeOnStartSP[INDI_ENABLED].getState() == ISS_ON))
            {
                RotatorHomeOnStartSP.setState(IPS_OK);
            }
            else
            {
                RotatorHomeOnStartSP.reset();
                RotatorHomeOnStartSP.setState(IPS_ALERT);
                RotatorHomeOnStartSP[prevIndex].setState(ISS_ON);
            }

            RotatorHomeOnStartSP.apply();
            return true;
        }

        // Focuser Backlash enable/disable
        //        if (!strcmp(FocusBacklashSP.name, name))
        //        {
        //            int prevIndex = IUFindOnSwitchIndex(&FocusBacklashSP);
        //            IUUpdateSwitch(&FocusBacklashSP, states, names, n);
        //            if (setBacklashCompensation(DEVICE_FOCUSER, FocusBacklashS[0].s == ISS_ON))
        //            {
        //                FocusBacklashSP.s = IPS_OK;
        //            }
        //            else
        //            {
        //                IUResetSwitch(&FocusBacklashSP);
        //                FocusBacklashSP.s           = IPS_ALERT;
        //                FocusBacklashS[prevIndex].s = ISS_ON;
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

        // Focser Go to home/center
        if (FocuserGotoSP.isNameMatch(name))
        {
            FocuserGotoSP.update(states, names, n);

            if (FocuserGotoSP[GOTO_HOME].getState() == ISS_ON)
            {
                if (home(DEVICE_FOCUSER))
                {
                    FocuserGotoSP.setState(IPS_BUSY);
                    FocusAbsPosNP.setState(IPS_BUSY);
                    FocusAbsPosNP.apply();
                    isFocuserHoming = true;
                    LOG_INFO("Focuser moving to home position...");
                }
                else
                    FocuserGotoSP.setState(IPS_ALERT);
            }
            else
            {
                if (center(DEVICE_FOCUSER))
                {
                    FocuserGotoSP.setState(IPS_BUSY);
                    LOG_INFO("Focuser moving to center position...");
                    FocusAbsPosNP.setState(IPS_BUSY);
                    FocusAbsPosNP.apply();
                }
                else
                    FocuserGotoSP.setState(IPS_ALERT);
            }

            FocuserGotoSP.apply();
            return true;
        }

        // Process all rotator properties
        if (strstr(name, "ROTATOR"))
        {
            if (INDI::RotatorInterface::processSwitch(dev, name, states, names, n))
                return true;
        }

        // Rotator Go to home/center
#if 0
        if (!strcmp(RotatorGotoSP.name, name))
        {
            IUUpdateSwitch(&RotatorGotoSP, states, names, n);

            if (RotatorGotoS[GOTO_HOME].s == ISS_ON)
            {
                if (home(DEVICE_ROTATOR))
                {
                    RotatorGotoSP.setState(IPS_BUSY);
                    RotatorAbsPosNP.setState(IPS_BUSY);
                    RotatorAbsPosNP.apply();
                    isRotatorHoming = true;
                    LOG_INFO("Rotator moving to home position...");
                }
                else
                    RotatorGotoSP.setState(IPS_ALERT);
            }
            else
            {
                if (center(DEVICE_ROTATOR))
                {
                    RotatorGotoSP.setState(IPS_BUSY);
                    LOG_INFO("Rotator moving to center position...");
                    RotatorAbsPosNP.setState(IPS_BUSY);
                    RotatorAbsPosNP.apply();
                }
                else
                    RotatorGotoSP.setState(IPS_ALERT);
            }

            IDSetSwitch(&RotatorGotoSP, nullptr);
            return true;
        }

        // Reverse Direction
        if (!strcmp(ReverseRotatorSP.name, name))
        {
            IUUpdateSwitch(&ReverseRotatorSP, states, names, n);

            if (reverseRotator(ReverseRotatorS[0].s == ISS_ON))
                ReverseRotatorSP.setState(IPS_OK);
            else
                ReverseRotatorSP.setState(IPS_ALERT);

            IDSetSwitch(&ReverseRotatorSP, nullptr);
            return true;
        }

        // Halt Rotator
        if (!strcmp(AbortRotatorSP.name, name))
        {
            if (halt(DEVICE_ROTATOR))
            {
                RotatorAbsPosNP.s = RotatorAbsAngleNP.s = RotatorGotoSP.setState(IPS_IDLE);
                RotatorAbsPosNP.apply();
                RotatorAbsAngleNP.apply();
                IUResetSwitch(&RotatorGotoSP);
                IDSetSwitch(&RotatorGotoSP, nullptr);

                AbortRotatorSP.setState(IPS_OK);
            }
            else
                AbortRotatorSP.setState(IPS_ALERT);

            IDSetSwitch(&AbortRotatorSP, nullptr);
            return true;
        }
#endif
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Set device nickname to the HUB itself
        if (HFocusNameTP.isNameMatch(name))
        {
            HFocusNameTP.update(texts, names, n);
            if (setNickname(DEVICE_FOCUSER, HFocusNameTP[DEVICE_FOCUSER].getText())
                    && setNickname(DEVICE_ROTATOR, HFocusNameTP[DEVICE_ROTATOR].getText()))
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
bool Gemini::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Temperature Coefficient
        if (TemperatureCoeffNP.isNameMatch(name))
        {
            TemperatureCoeffNP.update(values, names, n);
            for (int i = 0; i < n; i++)
            {
                if (setTemperatureCompensationCoeff('A' + i, TemperatureCoeffNP[i].getValue()) == false)
                {
                    LOG_ERROR("Failed to set temperature coefficients.");
                    TemperatureCoeffNP.setState(IPS_ALERT);
                    TemperatureCoeffNP.apply();
                    return false;
                }
            }

            TemperatureCoeffNP.setState(IPS_OK);
            TemperatureCoeffNP.apply();
            return true;
        }

        // Focuser Backlash Value
        //        if (!strcmp(FocusBacklashNP.name, name))
        //        {
        //            IUUpdateNumber(&FocusBacklashNP, values, names, n);
        //            if (setBacklashCompensationSteps(DEVICE_FOCUSER, FocusBacklashNP.apply();) == false)
        //            {
        //                LOG_ERROR("Failed to set focuser backlash value.");
        //                FocusBacklashNP.s = IPS_ALERT;
        //                IDSetNumber(&FocusBacklashNP, nullptr);
        //                return false;
        //            }

        //            FocusBacklashNP.s = IPS_OK;
        //            IDSetNumber(&FocusBacklashNP, nullptr);
        //            return true;
        //        }

        // Rotator Backlash Value
        if (RotatorBacklashNP.isNameMatch(name))
        {
            RotatorBacklashNP.update(values, names, n);
            if (setBacklashCompensationSteps(DEVICE_ROTATOR, RotatorBacklashNP[0].getValue()) == false)
            {
                LOG_ERROR("Failed to set rotator backlash value.");
                RotatorBacklashNP.setState(IPS_ALERT);
                RotatorBacklashNP.apply();
                return false;
            }

            RotatorBacklashNP.setState(IPS_OK);
            RotatorBacklashNP.apply();
            return true;
        }

        // Set LED intensity to the HUB itself via function setLedLevel()
        if (LedNP.isNameMatch(name))
        {
            LedNP.update(values, names, n);
            if (setLedLevel(LedNP[0].getValue()))
                LedNP.setState(IPS_OK);
            else
                LedNP.setState(IPS_ALERT);
            LOGF_INFO("Focuser LED level intensity : %f", LedNP[0].getValue());
            LedNP.apply();
            return true;
        }

        // Set Rotator Absolute Steps
        if (RotatorAbsPosNP.isNameMatch(name))
        {
            RotatorAbsPosNP.update(values, names, n);
            RotatorAbsPosNP.setState(MoveAbsRotatorTicks(static_cast<uint32_t>(RotatorAbsPosNP[0].value)));
            RotatorAbsPosNP.apply();
            return true;
        }

        if (strstr(name, "ROTATOR"))
        {
            if (INDI::RotatorInterface::processNumber(dev, name, values, names, n))
                return true;
        }

#if 0
        // Set Rotator Absolute Steps
        if (!strcmp(RotatorAbsPosNP.name, name))
        {
            IUUpdateNumber(&RotatorAbsPosNP, values, names, n);
            RotatorAbsPosNP.s = MoveAbsRotatorTicks(static_cast<uint32_t>(RotatorAbsPosN[0].value));
            RotatorAbsPosNP.apply();
            return true;
        }

        // Set Rotator Absolute Angle
        if (!strcmp(RotatorAbsAngleNP.name, name))
        {
            IUUpdateNumber(&RotatorAbsAngleNP, values, names, n);
            RotatorAbsAngleNP.s = MoveAbsRotatorAngle(RotatorAbsAngleN[0].value);
            RotatorAbsAngleNP.apply();
            return true;
        }
#endif

    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::ack()
{
    const char *cmd = "<F100GETDNN>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "Castor", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
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

        // Read 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);

        tcflush(PortFD, TCIFLUSH);

        return true;
    }

    tcflush(PortFD, TCIFLUSH);
    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::getFocusConfig()
{
    const char *cmd = "<F100GETCFG>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[64];
    int nbytes_read    = 0;
    int nbytes_written = 0;
    char key[16];

    memset(response, 0, sizeof(response));

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "!00", sizeof(response));
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        /*if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }*/
    }

    /*if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        if ((strcmp(response, "CONFIG1")) && (strcmp(response, "CONFIG2")))
            return false;
    }*/

    memset(response, 0, sizeof(response));

    // Nickname
    if (isSimulation())
    {
        strncpy(response, "NickName=Tommy\n", sizeof(response));
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    char nickname[16];
    int rc = sscanf(response, "%15[^=]=%15[^\n]s", key, nickname);

    if (rc != 2)
        return false;

    HFocusNameTP.setName(nickname);
    HFocusNameTP.apply();

    HFocusNameTP.setState(IPS_OK);
    HFocusNameTP.apply();

    memset(response, 0, sizeof(response));

    // Get Max Position
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "Max Pos = %06d\n", 100000);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    uint32_t maxPos = 0;
    rc = sscanf(response, "%15[^=]=%d", key, &maxPos);
    if (rc == 2)
    {
        FocusAbsPosNP[0].setMax(maxPos);
        FocusAbsPosNP[0].setStep(maxPos / 50.0);
        FocusAbsPosNP[0].setMin(0);

        FocusRelPosNP[0].setMax(maxPos / 2);
        FocusRelPosNP[0].setStep(maxPos / 100.0);
        FocusRelPosNP[0].setMin(0);

        FocusAbsPosNP.updateMinMax();
        FocusRelPosNP.updateMinMax();

        maxControllerTicks = maxPos;
    }
    else
        return false;

    memset(response, 0, sizeof(response));

    // Get Device Type
    if (isSimulation())
    {
        strncpy(response, "Dev Typ = A\n", sizeof(response));
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    // Get Status Parameters
    memset(response, 0, sizeof(response));

    // Temperature Compensation On?
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "TComp ON = %d\n", TemperatureCompensateSP[INDI_ENABLED].getState() == ISS_ON ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int TCompOn;
    rc = sscanf(response, "%15[^=]=%d", key, &TCompOn);
    if (rc != 2)
        return false;

    TemperatureCompensateSP.reset();
    TemperatureCompensateSP[INDI_ENABLED].setState(TCompOn ? ISS_ON : ISS_OFF);
    TemperatureCompensateSP[INDI_ENABLED].setState(TCompOn ? ISS_OFF : ISS_ON);
    TemperatureCompensateSP.setState(IPS_OK);
    TemperatureCompensateSP.apply();

    memset(response, 0, sizeof(response));

    // Temperature Coeff A
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "TempCo A = %d\n", (int)TemperatureCoeffNP[FOCUS_A_COEFF].getValue());
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int TCoeffA;
    rc = sscanf(response, "%15[^=]=%d", key, &TCoeffA);
    if (rc != 2)
        return false;

    TemperatureCoeffNP[FOCUS_A_COEFF].setValue(TCoeffA);

    memset(response, 0, sizeof(response));

    // Temperature Coeff B
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "TempCo B = %d\n", (int)TemperatureCoeffNP[FOCUS_B_COEFF].getValue());
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int TCoeffB;
    rc = sscanf(response, "%15[^=]=%d", key, &TCoeffB);
    if (rc != 2)
        return false;

    TemperatureCoeffNP[FOCUS_B_COEFF].setValue(TCoeffB);

    memset(response, 0, sizeof(response));

    // Temperature Coeff C
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "TempCo C = %d\n", (int)TemperatureCoeffNP[FOCUS_C_COEFF].getValue());
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int TCoeffC;
    rc = sscanf(response, "%15[^=]=%d", key, &TCoeffC);
    if (rc != 2)
        return false;

    TemperatureCoeffNP[FOCUS_C_COEFF].setValue(TCoeffC);

    memset(response, 0, sizeof(response));

    // Temperature Coeff D
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "TempCo D = %d\n", (int)TemperatureCoeffNP[FOCUS_D_COEFF].getValue());
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int TCoeffD;
    rc = sscanf(response, "%15[^=]=%d", key, &TCoeffD);
    if (rc != 2)
        return false;

    TemperatureCoeffNP[FOCUS_D_COEFF].setValue(TCoeffD);

    memset(response, 0, sizeof(response));

    // Temperature Coeff E
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "TempCo E = %d\n", (int)TemperatureCoeffNP[FOCUS_E_COEFF].getValue());
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int TCoeffE;
    rc = sscanf(response, "%15[^=]=%d", key, &TCoeffE);
    if (rc != 2)
        return false;

    TemperatureCoeffNP[FOCUS_E_COEFF].setValue(TCoeffE);

    TemperatureCoeffNP.setState(IPS_OK);
    TemperatureCoeffNP.apply();

    memset(response, 0, sizeof(response));

    // Temperature Compensation Mode
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "TC Mode = %c\n", 'C');
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    char compensateMode;
    rc = sscanf(response, "%15[^=]= %c", key, &compensateMode);
    if (rc != 2)
        return false;

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

    // Backlash Compensation
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "BLC En = %d\n", FocusBacklashSP[INDI_ENABLED].getState() == ISS_ON ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int BLCCompensate;
    rc = sscanf(response, "%15[^=]=%d", key, &BLCCompensate);
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
        snprintf(response, sizeof(response), "BLC Stps = %d\n", 50);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int BLCValue;
    rc = sscanf(response, "%15[^=]=%d", key, &BLCValue);
    if (rc != 2)
        return false;

    FocusBacklashNP[0].setValue(BLCValue);
    FocusBacklashNP.setState(IPS_OK);
    FocusBacklashNP.apply();

    // Temperature Compensation on Start
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "TC Start = %d\n",
                 TemperatureCompensateOnStartSP[INDI_ENABLED].getState() == ISS_ON ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int TCOnStart;
    rc = sscanf(response, "%15[^=]=%d", key, &TCOnStart);
    if (rc != 2)
        return false;

    TemperatureCompensateOnStartSP.reset();
    TemperatureCompensateOnStartSP[INDI_ENABLED].setState(TCOnStart ? ISS_ON : ISS_OFF);
    TemperatureCompensateOnStartSP[INDI_DISABLED].setState(TCOnStart ? ISS_OFF : ISS_ON);
    TemperatureCompensateOnStartSP.setState(IPS_OK);
    TemperatureCompensateOnStartSP.apply();

    // Get Status Parameters
    memset(response, 0, sizeof(response));

    // Home on start on?
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "HOnStart = %d\n", FocuserHomeOnStartSP[INDI_ENABLED].getState() == ISS_ON ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int StartOnHome;
    rc = sscanf(response, "%15[^=]=%d", key, &StartOnHome);
    if (rc != 2)
        return false;

    FocuserHomeOnStartSP.reset();
    FocuserHomeOnStartSP[INDI_ENABLED].setState(StartOnHome ? ISS_ON : ISS_OFF);
    FocuserHomeOnStartSP[INDI_DISABLED].setState(StartOnHome ? ISS_OFF : ISS_ON);
    FocuserHomeOnStartSP.setState(IPS_OK);
    FocuserHomeOnStartSP.apply();

    // Added By Philippe Besson the 28th of June for 'END' evaluation
    // END is reached
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        strncpy(response, "END\n", 16);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';

        // Display the response to be sure to have read the complete TTY Buffer.
        LOGF_DEBUG("RES (%s)", response);

        if (strcmp(response, "END"))
            return false;
    }
    // End of added code by Philippe Besson

    tcflush(PortFD, TCIFLUSH);

    focuserConfigurationComplete = true;

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::getRotatorStatus()
{
    const char *cmd = "<R100GETSTA>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[32];
    int nbytes_read    = 0;
    int nbytes_written = 0;
    char key[16];

    memset(response, 0, sizeof(response));

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "!00", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        /*if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }*/
    }

    ///////////////////////////////////////
    // #1 Get Current Position
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "CurrStep = %06d\n", rotatorSimPosition);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    int currPos = 0;
    int rc = sscanf(response, "%15[^=]=%d", key, &currPos);
    if (rc == 2)
    {
        // Do not spam unless there is an actual change
        if (RotatorAbsPosNP[0].getValue() != currPos)
        {
            RotatorAbsPosNP[0].setValue(currPos);
            RotatorAbsPosNP.apply();
        }
    }
    else
        return false;

    ///////////////////////////////////////
    // #2 Get Target Position
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "TargStep = %06d\n", targetFocuserPosition);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    ///////////////////////////////////////
    // #3 Get Current PA
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "CurenPA = %06d\n", rotatorSimPA);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    int currPA = 0;
    rc = sscanf(response, "%15[^=]=%d", key, &currPA);
    if (rc == 2)
    {
        // Only send when above a threshold
        double diffPA = fabs(GotoRotatorNP[0].getValue() - currPA / 1000.0);
        if (diffPA >= 0.01)
        {
            GotoRotatorNP[0].setValue(currPA / 1000.0);
            GotoRotatorNP.apply();
        }
    }
    else
        return false;

    ///////////////////////////////////////
    // #3 Get Target PA
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "TargetPA = %06d\n", targetFocuserPosition);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    // Get Status Parameters

    ///////////////////////////////////////
    // #5 is Moving?
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "IsMoving = %d\n", (rotatorSimStatus[STATUS_MOVING] == ISS_ON) ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    int isMoving;
    rc = sscanf(response, "%15[^=]=%d", key, &isMoving);
    if (rc != 2)
        return false;

    RotatorStatusLP[STATUS_MOVING].setState(isMoving ? IPS_BUSY : IPS_IDLE);

    ///////////////////////////////////////
    // #6 is Homing?
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "IsHoming = %d\n", (rotatorSimStatus[STATUS_HOMING] == ISS_ON) ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    int _isHoming;
    rc = sscanf(response, "%15[^=]=%d", key, &_isHoming);
    if (rc != 2)
        return false;

    RotatorStatusLP[STATUS_HOMING].setState(_isHoming ? IPS_BUSY : IPS_IDLE);

    // We set that isHoming in process, but we don't set it to false here it must be reset in TimerHit
    if (RotatorStatusLP[STATUS_HOMING].getState() == IPS_BUSY)
        isRotatorHoming = true;

    ///////////////////////////////////////
    // #6 is Homed?
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "IsHomed = %d\n", (rotatorSimStatus[STATUS_HOMED] == ISS_ON) ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    int isHomed;
    rc = sscanf(response, "%15[^=]=%d", key, &isHomed);
    if (rc != 2)
        return false;

    RotatorStatusLP[STATUS_HOMED].setState(isHomed ? IPS_OK : IPS_IDLE);
    RotatorStatusLP.apply();

    // Added By Philippe Besson the 28th of June for 'END' evaluation
    // END is reached
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        strncpy(response, "END\n", 16);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';

        // Display the response to be sure to have read the complete TTY Buffer.
        LOGF_DEBUG("RES (%s)", response);

        if (strcmp(response, "END"))
        {
            LOG_WARN("Invalid END response.");
            return false;
        }
    }
    // End of added code by Philippe Besson

    tcflush(PortFD, TCIFLUSH);

    return true;

}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::getRotatorConfig()
{
    const char *cmd = "<R100GETCFG>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[64];
    int nbytes_read    = 0;
    int nbytes_written = 0;
    char key[16];

    memset(response, 0, sizeof(response));

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "!00", sizeof(response));
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        /*if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }*/
    }

    /*if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        if ((strcmp(response, "CONFIG1")) && (strcmp(response, "CONFIG2")))
            return false;
    }*/

    memset(response, 0, sizeof(response));
    ////////////////////////////////////////////////////////////
    // Nickname
    ////////////////////////////////////////////////////////////
    if (isSimulation())
    {
        strncpy(response, "NickName=Juli\n", sizeof(response));
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    char nickname[16];
    int rc = sscanf(response, "%15[^=]=%15[^\n]s", key, nickname);

    if (rc != 2)
        return false;

    HFocusNameTP[DEVICE_ROTATOR].setText(nickname);
    HFocusNameTP.setState(IPS_OK);
    HFocusNameTP.apply();

    memset(response, 0, sizeof(response));

    ////////////////////////////////////////////////////////////
    // Get Max steps
    ////////////////////////////////////////////////////////////
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "MaxSteps = %06d\n", 100000);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    uint32_t maxPos = 0;
    rc = sscanf(response, "%15[^=]=%d", key, &maxPos);
    if (rc == 2)
    {
        RotatorAbsPosNP[0].setMin(0);
        RotatorAbsPosNP[0].setMax(maxPos);
        RotatorAbsPosNP[0].setStep(maxPos / 50.0);
        RotatorAbsPosNP.updateMinMax();
    }
    else
        return false;

    memset(response, 0, sizeof(response));

    ////////////////////////////////////////////////////////////
    // Get Device Type
    ////////////////////////////////////////////////////////////
    if (isSimulation())
    {
        strncpy(response, "Dev Type = B\n", sizeof(response));
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    // Get Status Parameters
    memset(response, 0, sizeof(response));

    ////////////////////////////////////////////////////////////
    // Backlash Compensation
    ////////////////////////////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "BLCSteps = %d\n", RotatorBacklashSP[INDI_ENABLED].getState() == ISS_ON ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int BLCCompensate;
    rc = sscanf(response, "%15[^=]=%d", key, &BLCCompensate);
    if (rc != 2)
        return false;

    RotatorBacklashSP.reset();
    RotatorBacklashSP[INDI_ENABLED].setState(BLCCompensate ? ISS_ON : ISS_OFF);
    RotatorBacklashSP[INDI_DISABLED].setState(BLCCompensate ? ISS_OFF : ISS_ON);
    RotatorBacklashSP.setState(IPS_OK);
    RotatorBacklashSP.apply();

    ////////////////////////////////////////////////////////////
    // Backlash Value
    ////////////////////////////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "BLCSteps = %d\n", 50);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int BLCValue;
    rc = sscanf(response, "%15[^=]=%d", key, &BLCValue);
    if (rc != 2)
        return false;

    RotatorBacklashNP[0].setValue(BLCValue);
    RotatorBacklashNP.setState(IPS_OK);
    RotatorBacklashNP.apply();

    ////////////////////////////////////////////////////////////
    // Home on start on?
    ////////////////////////////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, sizeof(response), "HOnStart = %d\n", RotatorHomeOnStartSP[INDI_ENABLED].getState() == ISS_ON ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES (%s)", response);

    int StartOnHome;
    rc = sscanf(response, "%15[^=]=%d", key, &StartOnHome);
    if (rc != 2)
        return false;

    RotatorHomeOnStartSP.reset();
    RotatorHomeOnStartSP[INDI_ENABLED].setState( StartOnHome ? ISS_ON : ISS_OFF);
    RotatorHomeOnStartSP[INDI_DISABLED].setState(StartOnHome ? ISS_OFF : ISS_ON);
    RotatorHomeOnStartSP.setState(IPS_OK);
    RotatorHomeOnStartSP.apply();

    ////////////////////////////////////////////////////////////
    // Reverse?
    ////////////////////////////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "Reverse = %d\n", (rotatorSimStatus[STATUS_REVERSE] == ISS_ON) ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    int reverse;
    rc = sscanf(response, "%15[^=]=%d", key, &reverse);
    if (rc != 2)
        return false;

    RotatorStatusLP[STATUS_REVERSE].setState(reverse ? IPS_OK : IPS_IDLE);

    // If reverse is enable and switch shows disabled, let's change that
    // same thing is reverse is disabled but switch is enabled
    if ((reverse && ReverseRotatorSP[1].getState() == ISS_ON) || (!reverse && ReverseRotatorSP[0].getState() == ISS_ON))
    {
        ReverseRotatorSP.reset();
        ReverseRotatorSP[0].setState((reverse == 1) ? ISS_ON : ISS_OFF);
        ReverseRotatorSP[1].setState((reverse == 0) ? ISS_ON : ISS_OFF);
        ReverseRotatorSP.apply();
    }

    RotatorStatusLP.setState(IPS_OK);
    RotatorStatusLP.apply();

    ////////////////////////////////////////////////////////////
    // Max Speed - Not used
    ////////////////////////////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "MaxSpeed = %d\n", 800);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    // Added By Philippe Besson the 28th of June for 'END' evaluation
    // END is reached
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        strncpy(response, "END\n", 16);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';

        // Display the response to be sure to have read the complete TTY Buffer.
        LOGF_DEBUG("RES (%s)", response);

        if (strcmp(response, "END"))
            return false;
    }
    // End of added code by Philippe Besson

    tcflush(PortFD, TCIFLUSH);

    rotatorConfigurationComplete = true;

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::getFocusStatus()
{
    const char *cmd = "<F100GETSTA>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[32];
    int nbytes_read    = 0;
    int nbytes_written = 0;
    char key[16];

    memset(response, 0, sizeof(response));

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "!00", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        /*if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }*/
    }

    // Get Temperature
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        //strncpy(response, "CurrTemp = +21.7\n", 16); // #PS: incorrect, lost last character
        strcpy(response, "CurrTemp = +21.7\n");
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
        response[nbytes_read - 1] = '\0';

    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    float temperature = 0;
    int rc = sscanf(response, "%15[^=]=%f", key, &temperature);
    if (rc == 2)
    {
        TemperatureNP[0].setValue(temperature);
        TemperatureNP.apply();
    }
    else
    {
        char np[8];
        int rc = sscanf(response, "%15[^=]= %s", key, np);

        if (rc != 2 || strcmp(np, "NP"))
        {
            if (TemperatureNP.getState() != IPS_ALERT)
            {
                TemperatureNP.setState(IPS_ALERT);
                TemperatureNP.apply();
            }
            return false;
        }
    }

    ///////////////////////////////////////
    // #1 Get Current Position
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "CurrStep = %06d\n", focuserSimPosition);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    uint32_t currPos = 0;
    rc = sscanf(response, "%15[^=]=%d", key, &currPos);
    if (rc == 2)
    {
        FocusAbsPosNP[0].setValue(currPos);
        FocusAbsPosNP.apply();
    }
    else
        return false;

    ///////////////////////////////////////
    // #2 Get Target Position
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "TargStep = %06d\n", targetFocuserPosition);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    // Get Status Parameters

    ///////////////////////////////////////
    // #3 is Moving?
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "IsMoving = %d\n", (focuserSimStatus[STATUS_MOVING] == ISS_ON) ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    int isMoving;
    rc = sscanf(response, "%15[^=]=%d", key, &isMoving);
    if (rc != 2)
        return false;

    FocuserStatusLP[STATUS_MOVING].setState(isMoving ? IPS_BUSY : IPS_IDLE);

    ///////////////////////////////////////
    // #4 is Homing?
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "IsHoming = %d\n", (focuserSimStatus[STATUS_HOMING] == ISS_ON) ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    int _isHoming;
    rc = sscanf(response, "%15[^=]=%d", key, &_isHoming);
    if (rc != 2)
        return false;

    FocuserStatusLP[STATUS_HOMING].setState(_isHoming ? IPS_BUSY : IPS_IDLE);
    // For relative focusers home is not applicable.
    if (isFocuserAbsolute == false)
        FocuserStatusLP[STATUS_HOMING].setState(IPS_IDLE);

    // We set that isHoming in process, but we don't set it to false here it must be reset in TimerHit
    if (FocuserStatusLP[STATUS_HOMING].getState() == IPS_BUSY)
        isFocuserHoming = true;

    ///////////////////////////////////////
    // #6 is Homed?
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "IsHomed = %d\n", (focuserSimStatus[STATUS_HOMED] == ISS_ON) ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    int isHomed;
    rc = sscanf(response, "%15[^=]=%d", key, &isHomed);
    if (rc != 2)
        return false;

    FocuserStatusLP[STATUS_HOMED].setState(isHomed ? IPS_OK : IPS_IDLE);
    // For relative focusers home is not applicable.
    if (isFocuserAbsolute == false)
        FocuserStatusLP[STATUS_HOMED].setState(IPS_IDLE);

    ///////////////////////////////////////
    // #7 Temperature probe?
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "TempProb = %d\n", (focuserSimStatus[STATUS_TMPPROBE] == ISS_ON) ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    int TmpProbe;
    rc = sscanf(response, "%15[^=]=%d", key, &TmpProbe);
    if (rc != 2)
        return false;

    FocuserStatusLP[STATUS_TMPPROBE].setState(TmpProbe ? IPS_OK : IPS_IDLE);

    ///////////////////////////////////////
    // #8 Remote IO?
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "RemoteIO = %d\n", (focuserSimStatus[STATUS_REMOTEIO] == ISS_ON) ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    int RemoteIO;
    rc = sscanf(response, "%15[^=]=%d", key, &RemoteIO);
    if (rc != 2)
        return false;

    FocuserStatusLP[STATUS_REMOTEIO].setState(RemoteIO ? IPS_OK : IPS_IDLE);

    ///////////////////////////////////////
    // #9 Hand controller?
    ///////////////////////////////////////
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        snprintf(response, 32, "HCStatus = %d\n", (focuserSimStatus[STATUS_HNDCTRL] == ISS_ON) ? 1 : 0);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    DEBUGF(DBG_FOCUS, "RES (%s)", response);

    int HndCtlr;
    rc = sscanf(response, "%15[^=]=%d", key, &HndCtlr);
    if (rc != 2)
        return false;

    FocuserStatusLP[STATUS_HNDCTRL].setState( HndCtlr ? IPS_OK : IPS_IDLE);

    FocuserStatusLP.setState(IPS_OK);
    FocuserStatusLP.apply();

    // Added By Philippe Besson the 28th of June for 'END' evaluation
    // END is reached
    memset(response, 0, sizeof(response));
    if (isSimulation())
    {
        strncpy(response, "END\n", 16);
        nbytes_read = strlen(response);
    }
    else if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';

        // Display the response to be sure to have read the complete TTY Buffer.
        LOGF_DEBUG("RES (%s)", response);

        if (strcmp(response, "END"))
        {
            LOG_WARN("Invalid END response.");
            return false;
        }
    }
    // End of added code by Philippe Besson

    tcflush(PortFD, TCIFLUSH);

    return true;

}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::setLedLevel(int level)
// Write via the serial port to the HUB the selected LED intensity level

{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<H100SETLED%d>", level);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
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
bool Gemini::setNickname(DeviceType type, const char *nickname)
// Write via the serial port to the HUB the choiced nikname of he focuser
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 32, "<%c100SETDNN%s>", (type == DEVICE_FOCUSER ? 'F' : 'R'), nickname);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!isSimulation())
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        // Read the 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);
    }

    tcflush(PortFD, TCIFLUSH);
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::halt(DeviceType type)
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 32, "<%c100DOHALT>", (type == DEVICE_FOCUSER ? 'F' : 'R'));
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        if (type == DEVICE_FOCUSER)
            focuserSimStatus[STATUS_MOVING] = ISS_OFF;
        else
        {
            rotatorSimStatus[STATUS_MOVING] = ISS_OFF;
            isRotatorHoming = false;
        }
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        // Read the 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);
    }

    isRotatorHoming = false;

    tcflush(PortFD, TCIFLUSH);

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::home(DeviceType type)
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 32, "<%c100DOHOME>", (type == DEVICE_FOCUSER ? 'F' : 'R'));
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        if (type == DEVICE_FOCUSER)
        {
            focuserSimStatus[STATUS_HOMING] = ISS_ON;
            targetFocuserPosition = 0;
        }
        else
        {
            rotatorSimStatus[STATUS_HOMING] = ISS_ON;
            targetRotatorPosition = 0;
        }
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        // Read the 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);
    }

    tcflush(PortFD, TCIFLUSH);

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::homeOnStart(DeviceType type, bool enable)
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 32, "<%c100SETHOS%d>", (type == DEVICE_FOCUSER ? 'F' : 'R'), enable ? 1 : 0);
    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation() == false)
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        // Read the 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);
    }

    tcflush(PortFD, TCIFLUSH);

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::center(DeviceType type)
{
    if (type == DEVICE_ROTATOR)
        return MoveAbsRotatorTicks(RotatorAbsPosNP[0].getMax() / 2);

    const char * cmd = "<F100CENTER>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        if (type == DEVICE_FOCUSER)
        {
            focuserSimStatus[STATUS_MOVING] = ISS_ON;
            targetFocuserPosition = FocusAbsPosNP[0].getMax() / 2;
        }
        else
        {
            rotatorSimStatus[STATUS_MOVING] = ISS_ON;
            targetRotatorPosition = RotatorAbsPosNP[0].getMax() / 2;
        }

    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        // Read the 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);
    }

    tcflush(PortFD, TCIFLUSH);

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::setTemperatureCompensation(bool enable)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<F100SETTCE%d>", enable ? 1 : 0);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation() == false)
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        // Read the 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::setTemperatureCompensationMode(char mode)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<F100SETTCM%c>", mode);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation() == false)
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        // Read the 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::setTemperatureCompensationCoeff(char mode, int16_t coeff)
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 32, "<F100SETTCC%c%c%04d>", mode, coeff >= 0 ? '+' : '-', (int)std::abs(coeff));

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation() == false)
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        // Read the 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::setTemperatureCompensationOnStart(bool enable)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<F100SETTCS%d>", enable ? 1 : 0);

    LOGF_DEBUG("CMD (%s)", cmd);

    tcflush(PortFD, TCIFLUSH);

    if (isSimulation() == false)
    {
        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        // Read the 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::SetRotatorBacklash(int32_t steps)
{
    return setBacklashCompensationSteps(DEVICE_ROTATOR, steps);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::SetRotatorBacklashEnabled(bool enabled)
{
    return setBacklashCompensation(DEVICE_ROTATOR, enabled);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::setBacklashCompensation(DeviceType type, bool enable)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<%c100SETBCE%d>", (type == DEVICE_FOCUSER ? 'F' : 'R'), enable ? 1 : 0);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation() == false)
    {
        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        // Read the 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::setBacklashCompensationSteps(DeviceType type, uint16_t steps)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<%c100SETBCS%02d>", (type == DEVICE_FOCUSER ? 'F' : 'R'), steps);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation() == false)
    {
        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        // Read the 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::reverseRotator(bool enable)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<R100SETREV%d>", enable ? 1 : 0);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation() == false)
    {
        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        // Read the 'END'
        tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::resetFactory()
{
    const char *cmd = "<H100RESETH>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
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
            //return true;
            getFocusConfig();
            getRotatorConfig();
        }
        else
            return false;
    }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::isResponseOK()
{
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[32];
    int nbytes_read = 0;

    memset(response, 0, sizeof(response));

    if (isSimulation())
    {
        strcpy(response, "!00");
        nbytes_read = strlen(response) + 1;
    }
    else
    {
        if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
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

        if (!strcmp(response, "!00"))
            return true;
        else
        {
            memset(response, 0, sizeof(response));
            while (strstr(response, "END") == nullptr)
            {
                if ((errcode = tty_read_section(PortFD, response, 0xA, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
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
IPState Gemini::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_written = 0;

    INDI_UNUSED(speed);

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<F100DOMOVE%c>", (dir == FOCUS_INWARD) ? '0' : '1');

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!isSimulation())
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return IPS_ALERT;
        }

        if (isResponseOK() == false)
            return IPS_ALERT;

        gettimeofday(&focusMoveStart, nullptr);
        focusMoveRequest = duration / 1000.0;
    }

    if (duration <= getCurrentPollingPeriod())
    {
        usleep(getCurrentPollingPeriod() * 1000);
        AbortFocuser();
        return IPS_OK;
    }

    tcflush(PortFD, TCIFLUSH);

    return IPS_BUSY;
}

/************************************************************************************
*
* ***********************************************************************************/
IPState Gemini::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_written = 0;

    targetFocuserPosition = targetTicks;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 32, "<F100MOVABS%06d>", targetTicks);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!isSimulation())
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return IPS_ALERT;
        }

        if (isResponseOK() == false)
            return IPS_ALERT;
    }

    FocusAbsPosNP.setState(IPS_BUSY);

    tcflush(PortFD, TCIFLUSH);

    return IPS_BUSY;
}

/************************************************************************************
*
* ***********************************************************************************/
IPState Gemini::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    return MoveAbsFocuser(newPosition);
}

/************************************************************************************
*
* ***********************************************************************************/
void Gemini::TimerHit()
{
    if (!isConnected())
        return;

    if (focuserConfigurationComplete == false || rotatorConfigurationComplete == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    // Focuser Status
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
            if (FocusAbsPosNP[0].getValue() < targetFocuserPosition)
                focuserSimPosition += 100;
            else
                focuserSimPosition -= 100;

            focuserSimStatus[STATUS_MOVING] = ISS_ON;

            if (std::abs((int64_t)focuserSimPosition - (int64_t)targetFocuserPosition) < 100)
            {
                FocusAbsPosNP[0].setValue(targetFocuserPosition);
                focuserSimPosition              = FocusAbsPosNP[0].getValue();
                focuserSimStatus[STATUS_MOVING] = ISS_OFF;
                FocuserStatusLP[STATUS_MOVING].setState(IPS_IDLE);
                if (focuserSimStatus[STATUS_HOMING] == ISS_ON)
                {
                    FocuserStatusLP[STATUS_HOMED].setState(IPS_OK);
                    focuserSimStatus[STATUS_HOMING] = ISS_OFF;
                }
            }
        }

        if (isFocuserHoming && FocuserStatusLP[STATUS_HOMED].getState() == IPS_OK)
        {
            isFocuserHoming = false;
            FocuserGotoSP.setState(IPS_OK);
            FocuserGotoSP.reset();
            FocuserGotoSP[GOTO_HOME].setState(ISS_ON);
            FocuserGotoSP.apply();
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.apply();
            LOG_INFO("Focuser reached home position.");
        }
        else if (FocuserStatusLP[STATUS_MOVING].getState() == IPS_IDLE)
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
            if (FocuserGotoSP.getState() == IPS_BUSY)
            {
                FocuserGotoSP.reset();
                FocuserGotoSP.setState(IPS_OK);
                FocuserGotoSP.apply();
            }
            LOG_INFO("Focuser reached requested position.");
        }
    }
    if (FocuserStatusLP[STATUS_HOMING].getState() == IPS_BUSY && FocuserGotoSP.getState() != IPS_BUSY)
    {
        FocuserGotoSP.setState(IPS_BUSY);
        FocuserGotoSP.apply();
    }

    // Rotator Status
    statusrc = false;
    for (int i = 0; i < 2; i++)
    {
        statusrc = getRotatorStatus();
        if (statusrc)
            break;
    }

    if (statusrc == false)
    {
        LOG_WARN("Unable to read rotator status....");
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    if (RotatorAbsPosNP.getState() == IPS_BUSY || GotoRotatorNP.getState() == IPS_BUSY)
    {
        /*if (isSimulation())
        {
            if (RotatorAbsPosN[0].value < targetRotatorPosition)
                RotatorSimPosition += 100;
            else
                RotatorSimPosition -= 100;

            RotatorSimStatus[STATUS_MOVING] = ISS_ON;

            if (std::abs((int64_t)RotatorSimPosition - (int64_t)targetRotatorPosition) < 100)
            {
                RotatorAbsPosN[0].value    = targetRotatorPosition;
                RotatorSimPosition              = RotatorAbsPosN[0].value;
                RotatorSimStatus[STATUS_MOVING] = ISS_OFF;
                RotatorStatusL[STATUS_MOVING].setState(IPS_IDLE);
                if (RotatorSimStatus[STATUS_HOMING] == ISS_ON)
                {
                    RotatorStatusL[STATUS_HOMED].setState(IPS_OK);
                    RotatorSimStatus[STATUS_HOMING] = ISS_OFF;
                }
            }
        }*/

        if (isRotatorHoming && RotatorStatusLP[STATUS_HOMED].getState() == IPS_OK)
        {
            isRotatorHoming = false;
            HomeRotatorSP.setState(IPS_OK);
            HomeRotatorSP.reset();
            HomeRotatorSP.apply();
            RotatorAbsPosNP.setState(IPS_OK);
            RotatorAbsPosNP.apply();
            GotoRotatorNP.setState(IPS_OK);
            GotoRotatorNP.apply();
            LOG_INFO("Rotator reached home position.");
        }
        else if (RotatorStatusLP[STATUS_MOVING].getState() == IPS_IDLE)
        {
            RotatorAbsPosNP.setState(IPS_OK);
            RotatorAbsPosNP.apply();
            GotoRotatorNP.setState(IPS_OK);
            GotoRotatorNP.apply();
            if (HomeRotatorSP.getState() == IPS_BUSY)
            {
                HomeRotatorSP.reset();
                HomeRotatorSP.setState(IPS_OK);
                HomeRotatorSP.apply();
            }
            LOG_INFO("Rotator reached requested position.");
        }
    }
    if (RotatorStatusLP[STATUS_HOMING].getState() == IPS_BUSY && HomeRotatorSP.getState() != IPS_BUSY)
    {
        HomeRotatorSP.setState(IPS_BUSY);
        HomeRotatorSP.apply();
    }

    SetTimer(getCurrentPollingPeriod());
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::AbortFocuser()
{
    const char *cmd = "<F100DOHALT>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_written = 0;

    memset(response, 0, sizeof(response));

    LOGF_DEBUG("CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "!00", 16);
        focuserSimStatus[STATUS_MOVING] = ISS_OFF;
        focuserSimStatus[STATUS_HOMING] = ISS_OFF;
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;
    }


    if (FocusRelPosNP.getState() == IPS_BUSY)
    {
        FocusRelPosNP.setState(IPS_IDLE);
        FocusRelPosNP.apply();
    }

    FocusTimerNP.setState(IPS_IDLE);
    FocusAbsPosNP.setState(IPS_IDLE);
    FocuserGotoSP.setState(IPS_IDLE);
    FocuserGotoSP.reset();
    FocusAbsPosNP.apply();
    FocuserGotoSP.apply();

    tcflush(PortFD, TCIFLUSH);

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
float Gemini::calcTimeLeft(timeval start, float req)
{
    double timesince;
    double timeleft;
    struct timeval now
    {
        0, 0
    };
    gettimeofday(&now, nullptr);

    timesince =
        (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec / 1000);
    timesince = timesince / 1000;
    timeleft  = req - timesince;
    return timeleft;
}

/************************************************************************************
*
* ***********************************************************************************/
IPState Gemini::MoveAbsRotatorTicks(uint32_t targetTicks)
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_written = 0;

    targetRotatorPosition = targetTicks;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 32, "<R100MOVABS%06d>", targetTicks);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!isSimulation())
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return IPS_ALERT;
        }

        if (isResponseOK() == false)
            return IPS_ALERT;
    }

    RotatorAbsPosNP.setState(IPS_BUSY);

    tcflush(PortFD, TCIFLUSH);

    return IPS_BUSY;
}

/************************************************************************************
*
* ***********************************************************************************/
IPState Gemini::MoveAbsRotatorAngle(double angle)
{
    char cmd[32];
    char response[16];
    int nbytes_written = 0;

    targetRotatorAngle = angle * 1000.0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 32, "<R100MOVEPA%06ud>", targetRotatorAngle);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!isSimulation())
    {
        int errcode = 0;
        char errmsg[MAXRBUF];
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return IPS_ALERT;
        }

        if (isResponseOK() == false)
            return IPS_ALERT;
    }

    GotoRotatorNP.setState(IPS_BUSY);

    tcflush(PortFD, TCIFLUSH);

    return IPS_BUSY;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::saveConfigItems(FILE *fp)
{
    // Save Focuser configs
    INDI::Focuser::saveConfigItems(fp);
    // Rotator Configs
    RI::saveConfigItems(fp);

    TemperatureCompensateSP.save(fp);
    TemperatureCompensateOnStartSP.save(fp);
    TemperatureCoeffNP.save(fp);
    TemperatureCompensateModeSP.save(fp);
    FocuserHomeOnStartSP.apply();
    RotatorHomeOnStartSP.save(fp);

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState Gemini::MoveRotator(double angle)
{
    IPState state = MoveAbsRotatorAngle(angle);
    RotatorAbsPosNP.setState(state);
    RotatorAbsPosNP.apply();

    return state;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState Gemini::HomeRotator()
{
    return (home(DEVICE_ROTATOR) ? IPS_BUSY : IPS_ALERT);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::ReverseRotator(bool enabled)
{
    return reverseRotator(enabled);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::SetFocuserBacklash(int32_t steps)
{
    return setBacklashCompensationSteps(DEVICE_FOCUSER, steps);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Gemini::SetFocuserBacklashEnabled(bool enabled)
{
    return setBacklashCompensation(DEVICE_FOCUSER, enabled);
}
