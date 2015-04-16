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

#include "focuslynx.h"
#include "indicom.h"

#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <memory>

#define LYNXFOCUS_MAX_RETRIES          1
#define LYNXFOCUS_TIMEOUT              1
#define LYNXFOCUS_MAXBUF               16
#define LYNXFOCUS_TEMPERATURE_FREQ     20      /* Update every 20 POLLMS cycles. For POLLMS 500ms = 10 seconds freq */
#define LYNXFOCUS_POSITION_THRESHOLD    5       /* Only send position updates to client if the diff exceeds 5 steps */

#define FOCUS_SETTINGS_TAB  "Settings"
#define FOCUS_STATUS_TAB    "Status"

#define POLLMS  500

std::auto_ptr<FocusLynx> lynxDrive(0);

void ISInit()
{
    static int isInit =0;

    if (isInit == 1)
        return;

     isInit = 1;
     if(lynxDrive.get() == 0) lynxDrive.reset(new FocusLynx());
}

void ISGetProperties(const char *dev)
{
         ISInit();
         lynxDrive->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
         ISInit();
         lynxDrive->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
         ISInit();
         lynxDrive->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
         ISInit();
         lynxDrive->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
   INDI_UNUSED(dev);
   INDI_UNUSED(name);
   INDI_UNUSED(sizes);
   INDI_UNUSED(blobsizes);
   INDI_UNUSED(blobs);
   INDI_UNUSED(formats);
   INDI_UNUSED(names);
   INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
     ISInit();
     lynxDrive->ISSnoopDevice(root);
}

FocusLynx::FocusLynx()
{

    lynxModels["OA"] = "Optec TCF-Lynx 2";
    lynxModels["OB"] = "Optec TCF-Lynx 3";
    lynxModels["OA"] = "Optec TCF-Lynx 2";
    lynxModels["OC"] = "Optec TCF-Lynx 2 with Extended Travel";
    lynxModels["OD"] = "Optec Fast Focus Secondary Focuser";
    lynxModels["OE"] = "Optec TCF-S Classic converted";
    lynxModels["OF"] = "Optec TCF-S3 Classic converted";
    lynxModels["OG"] = "Optec Gemini";
    lynxModels["FA"] = "FocusLynx QuickSync FT Hi-Torque";
    lynxModels["FB"] = "FocusLynx QuickSync FT Hi-Speed";
    lynxModels["FC"] = "FocusLynx QuickSync SV";
    lynxModels["SQ"] = "FeatherTouch Motor Hi-Speed";
    lynxModels["SP"] = "FeatherTouch Motor Hi-Torque";
    lynxModels["SQ"] = "Starlight Instruments - FTM with MicroTouch";
    lynxModels["TA"] = "Televue Focuser";

    ModelS = NULL;

    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.
    FocuserCapability cap;
    cap.canAbort=true;
    cap.canAbsMove=true;
    cap.canRelMove=true;
    cap.variableSpeed=false;

    SetFocuserCapability(&cap);

    simStatus[STATUS_MOVING]   = ISS_OFF;
    simStatus[STATUS_HOMING]   = ISS_OFF;
    simStatus[STATUS_HOMED]    = ISS_OFF;
    simStatus[STATUS_FFDETECT] = ISS_OFF;
    simStatus[STATUS_TMPPROBE] = ISS_ON;
    simStatus[STATUS_REMOTEIO] = ISS_ON;
    simStatus[STATUS_HNDCTRL]  = ISS_ON;

    //lastPos = 0;
    //lastTemperature = 0;
}

FocusLynx::~FocusLynx()
{

}

bool FocusLynx::initProperties()
{
    INDI::Focuser::initProperties();

    // Focus Sync
    IUFillNumber(&SyncN[0], "Position (steps)", "", "%3.0f", 0., 200000., 100., 0.);
    IUFillNumberVector(&SyncNP, SyncN, 1, getDeviceName(), "FOCUS_SYNC", "Sync", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Enable/Disable temperature compnesation
    IUFillSwitch(&TemperatureCompensateS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&TemperatureCompensateSP, TemperatureCompensateS, 2, getDeviceName(), "T. Compensation", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Enable/Disable temperature compnesation on start
    IUFillSwitch(&TemperatureCompensateOnStartS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateOnStartS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&TemperatureCompensateOnStartSP, TemperatureCompensateOnStartS, 2, getDeviceName(), "T. Compensation @Start", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Temperature Coefficient
    IUFillNumber(&TemperatureCoeffN[0], "A", "", "%.f", 0., 200000., 100., 0.);
    IUFillNumber(&TemperatureCoeffN[1], "B", "", "%.f", 0., 200000., 100., 0.);
    IUFillNumber(&TemperatureCoeffN[2], "C", "", "%.f", 0., 200000., 100., 0.);
    IUFillNumber(&TemperatureCoeffN[3], "D", "", "%.f", 0., 200000., 100., 0.);
    IUFillNumber(&TemperatureCoeffN[4], "E", "", "%.f", 0., 200000., 100., 0.);
    IUFillNumberVector(&TemperatureCoeffNP, TemperatureCoeffN, 5, getDeviceName(), "T. Coeff", "", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Enable/Disable temperature Mode
    IUFillSwitch(&TemperatureCompensateModeS[0], "A", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateModeS[1], "B", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateModeS[2], "B", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateModeS[3], "B", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateModeS[4], "B", "", ISS_OFF);
    IUFillSwitchVector(&TemperatureCompensateModeSP, TemperatureCompensateModeS, 5, getDeviceName(), "Compensate Mode", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Enable/Disable backlash
    IUFillSwitch(&BacklashCompensationS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&BacklashCompensationS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&BacklashCompensationSP, BacklashCompensationS, 2, getDeviceName(), "Backlash Compensation", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Backlash Value
    IUFillNumber(&BacklashN[0], "Value", "", "%.f", 0, 200000, 100., 0.);
    IUFillNumberVector(&BacklashNP, BacklashN, 1, getDeviceName(), "Backlash", "", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Reset to Factory setting
    IUFillSwitch(&ResetS[0], "Factory", "", ISS_OFF);
    IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "Reset", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Go to home/center
    IUFillSwitch(&GotoS[GOTO_CENTER], "Center", "", ISS_OFF);
    IUFillSwitch(&GotoS[GOTO_HOME], "Home", "", ISS_OFF);
    IUFillSwitchVector(&GotoSP, GotoS, 2, getDeviceName(), "GOTO", "", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // List all supported models
    std::map<std::string, std::string>::iterator iter;
    int nModels=1;
    ModelS = (ISwitch *) malloc(sizeof(ISwitch));
    IUFillSwitch(ModelS, "ZZ", "--", ISS_ON);
    for (iter = lynxModels.begin(); iter != lynxModels.end(); ++iter)
    {        
        ModelS = (ISwitch *) realloc(ModelS, (nModels+1)*sizeof(ISwitch));
        IUFillSwitch(ModelS+nModels, (iter->first).c_str(), (iter->second).c_str(), ISS_OFF);
        nModels++;
    }
    IUFillSwitchVector(&ModelSP, ModelS, nModels, getDeviceName(), "Models", "", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Sync to a particular position
    IUFillNumber(&SyncN[0], "Steps", "", "%.f", 0, 200000, 100., 0.);
    IUFillNumberVector(&SyncNP, SyncN, 1, getDeviceName(), "Sync", "", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Status indicators
    IUFillLight(&StatusL[STATUS_MOVING], "Is Moving", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_HOMING], "Is Homing", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_HOMED], "Is Homed", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_FFDETECT], "FF Detect", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_TMPPROBE], "Tmp Probe", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_REMOTEIO], "Remote IO", "", IPS_IDLE);
    IUFillLight(&StatusL[STATUS_HNDCTRL], "Hnd Ctrl", "", IPS_IDLE);
    IUFillLightVector(&StatusLP, StatusL, 7, getDeviceName(), "Status", "", FOCUS_STATUS_TAB, IPS_IDLE);

    //simPosition = FocusAbsPosN[0].value;
    addAuxControls();

    return true;

}

void FocusLynx::ISGetProperties(const char *dev)
{
    if(dev && strcmp(dev,getDeviceName()))
        return;

    INDI::Focuser::ISGetProperties(dev);

    defineSwitch(&ModelSP);

    loadConfig(true, "Models");
}

bool FocusLynx::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&SyncNP);

        defineNumber(&TemperatureNP);
        defineNumber(&TemperatureCoeffNP);
        defineSwitch(&TemperatureCompensateSP);
        defineSwitch(&TemperatureCompensateModeSP);
        defineSwitch(&TemperatureCompensateOnStartSP);

        defineSwitch(&BacklashCompensationSP);
        defineNumber(&BacklashNP);

        defineSwitch(&ResetSP);
        defineSwitch(&GotoSP);

        defineLight(&StatusLP);

        getInitialData();

        DEBUG(INDI::Logger::DBG_SESSION, "FocusLynx paramaters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(SyncNP.name);

        deleteProperty(TemperatureNP.name);
        deleteProperty(TemperatureCoeffNP.name);
        deleteProperty(TemperatureCompensateSP.name);
        deleteProperty(TemperatureCompensateModeSP.name);
        deleteProperty(TemperatureCompensateOnStartSP.name);

        deleteProperty(BacklashCompensationSP.name);
        deleteProperty(BacklashNP.name);

        deleteProperty(ResetSP.name);
        deleteProperty(GotoSP.name);

        deleteProperty(StatusLP.name);
    }

    return true;

}

bool FocusLynx::Connect()
{
    int connectrc=0;
    char errorMsg[MAXRBUF];

    if (!isSimulation() && (connectrc = tty_connect(PortT[0].text, 9600, 8, 0, 1, &PortFD)) != TTY_OK)
    {
        tty_error_msg(connectrc, errorMsg, MAXRBUF);

        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to connect to port %s. Error: %s", PortT[0].text, errorMsg);

        return false;

    }

    if (ack())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "FocusLynx is online. Getting focus parameters...");
        //temperatureUpdateCounter=0;
        SetTimer(POLLMS);
        return true;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Error retreiving data from FocusLynx, please ensure FocusLynx controller is powered and the port is correct.");
    return false;
}

bool FocusLynx::Disconnect()
{
    //temperatureUpdateCounter=0;
    if (!isSimulation())
        tty_disconnect(PortFD);
    DEBUG(INDI::Logger::DBG_SESSION, "FocusLynx is offline.");
    return true;
}

const char * FocusLynx::getDefaultName()
{
    return "FocusLynx";
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
#if 0
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(TemperatureCompensateSP.name, name))
        {
            int last_index = IUFindOnSwitchIndex(&TemperatureCompensateSP);
            IUUpdateSwitch(&TemperatureCompensateSP, states, names, n);

            bool rc = setTemperatureCompensation();

            if (rc == false)
            {
                TemperatureCompensateSP.s = IPS_ALERT;
                IUResetSwitch(&TemperatureCompensateSP);
                TemperatureCompensateS[last_index].s = ISS_ON;
                IDSetSwitch(&TemperatureCompensateSP, NULL);
                return false;
            }

            TemperatureCompensateSP.s = IPS_OK;
            IDSetSwitch(&TemperatureCompensateSP, NULL);
            return true;
        }

        if (!strcmp(ModelSP.name, name))
        {
            IUUpdateSwitch(&ModelSP, states, names, n);

            int i = IUFindOnSwitchIndex(&ModelSP);

            double focusMaxPos = floor(fSettings[i].maxTrip / fSettings[i].gearRatio) * 100;
            FocusAbsPosN[0].max = focusMaxPos;
            IUUpdateMinMax(&FocusAbsPosNP);

            CustomSettingN[FOCUS_MAX_TRIP].value = fSettings[i].maxTrip;
            CustomSettingN[FOCUS_GEAR_RATIO].value = fSettings[i].gearRatio;

            IDSetNumber(&CustomSettingNP, NULL);

            ModelSP.s = IPS_OK;
            IDSetSwitch(&ModelSP, NULL);

            return true;
        }

    }

#endif
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
#if 0
    if(strcmp(dev,getDeviceName())==0)
    {
        // Set Accelration
        if (!strcmp(name, AccelerationNP.name))
        {
            if (setAcceleration((int) values[0]))
            {
                IUUpdateNumber(&AccelerationNP, values, names, n);
                AccelerationNP.s = IPS_OK;
                IDSetNumber(&AccelerationNP, NULL);
                return true;
            }
            else
            {

                AccelerationNP.s = IPS_ALERT;
                IDSetNumber(&AccelerationNP, NULL);
                return false;
            }
        }

        // Set Temperature Settings
        if (!strcmp(name, TemperatureSettingNP.name))
        {
            // Coeff is only needed when we enable or disable the temperature compensation. Here we only set the # of samples
            unsigned int targetSamples;

            if (!strcmp(names[0], TemperatureSettingN[FOCUS_T_SAMPLES].name))
                targetSamples = (int) values[0];
            else
                targetSamples = (int) values[1];

            unsigned int finalSample = targetSamples;

            if (setTemperatureSamples(targetSamples, &finalSample))
            {
                IUUpdateNumber(&TemperatureSettingNP, values, names, n);
                TemperatureSettingN[FOCUS_T_SAMPLES].value = finalSample;

                if (TemperatureSettingN[FOCUS_T_COEFF].value > TemperatureSettingN[FOCUS_T_COEFF].max)
                    TemperatureSettingN[FOCUS_T_COEFF].value = TemperatureSettingN[FOCUS_T_COEFF].max;

                TemperatureSettingNP.s = IPS_OK;
                IDSetNumber(&TemperatureSettingNP, NULL);
                return true;
            }
            else
            {
                TemperatureSettingNP.s = IPS_ALERT;
                IDSetNumber(&TemperatureSettingNP, NULL);
                return true;
            }
        }

        // Set Custom Settings
        if (!strcmp(name, CustomSettingNP.name))
        {
            int i = IUFindOnSwitchIndex(&ModelSP);

            // If the model is NOT set to custom, then the values cannot be updated
            if (i != 4)
            {
                CustomSettingNP.s = IPS_IDLE;
                DEBUG(INDI::Logger::DBG_WARNING, "You can not set custom values for a non-custom focuser.");
                IDSetNumber(&CustomSettingNP, NULL);
                return false;
            }


            double maxTrip, gearRatio;
            if (!strcmp(names[0], CustomSettingN[FOCUS_MAX_TRIP].name))
            {
                maxTrip   = values[0];
                gearRatio = values[1];
            }
            else
            {
                maxTrip   = values[1];
                gearRatio = values[0];
            }

            if (setCustomSettings(maxTrip, gearRatio))
            {
                IUUpdateNumber(&CustomSettingNP, values, names, n);
                CustomSettingNP.s = IPS_OK;
                IDSetNumber(&CustomSettingNP, NULL);
            }
            else
            {
                CustomSettingNP.s = IPS_ALERT;
                IDSetNumber(&CustomSettingNP, NULL);
            }
        }

        // Set Sync Position
        if (!strcmp(name, SyncNP.name))
        {
            if (Sync( (unsigned int) values[0]))
            {
                IUUpdateNumber(&SyncNP, values, names, n);
                SyncNP.s = IPS_OK;
                IDSetNumber(&SyncNP, NULL);

                if (updatePosition())
                    IDSetNumber(&FocusAbsPosNP, NULL);

                return true;
            }
            else
            {
                SyncNP.s = IPS_ALERT;
                IDSetNumber(&SyncNP, NULL);

                return false;
            }
        }
    }

#endif
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::ack()
{
    char cmd[] = "<F1HELLO>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "Optec 2\" TCF-S", 16);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
      DEBUGF(INDI::Logger::DBG_SESSION, "%s is detected.", response);

      tcflush(PortFD, TCIFLUSH);

      return true;
     }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
void FocusLynx::getInitialData()
{

}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::getFocusConfig()
{
    char cmd[] = "<F1GETCONFIG>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[32];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "CONFIG", 16);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      if (strcmp(response, "CONFIG"))
        return false;

      // Nickname
      if (isSimulation())
      {
          strncpy(response, "Optec 2\" TCF-S", 16);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      // Get Max Position
      if (isSimulation())
      {
          snprintf(response, 32, "Max Pos = %06d", 100000);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      uint32_t maxPos=0;
      int rc = sscanf(response, "%*s = %d", &maxPos);
      if (rc == 1)
      {
          FocusAbsPosN[0].max = maxPos;
          FocusAbsPosN[0].step = maxPos/50.0;
          FocusAbsPosN[0].min = 0;

          FocusRelPosN[0].max = maxPos/2;
          FocusRelPosN[0].step = maxPos/100.0;
          FocusRelPosN[0].min = 0;

          IUUpdateMinMax(&FocusAbsPosNP);
          IUUpdateMinMax(&FocusRelPosNP);
      }
      else
          return false;

      // Get Device Type
      if (isSimulation())
      {
          snprintf(response, 32, "Dev Typ = %s", "OA");
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      // Get Status Parameters

      // Temperature Compensation On?
      if (isSimulation())
      {
          snprintf(response, 32, "TComp ON = %d", TemperatureCompensateS[0].s == ISS_ON ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCompOn;
      rc = sscanf(response, "%*s = %d", &TCompOn);
      if (rc != 1)
          return false;

      IUResetSwitch(&TemperatureCompensateSP);
      TemperatureCompensateS[0].s = TCompOn ? ISS_ON : ISS_OFF;
      TemperatureCompensateS[0].s = TCompOn ? ISS_OFF : ISS_ON;
      TemperatureCompensateSP.s = IPS_OK;
      IDSetSwitch(&TemperatureCompensateSP, NULL);

      // Temperature Coeff A
      if (isSimulation())
      {
          snprintf(response, 32, "TempCo A = %d", (int) TemperatureCoeffN[FOCUS_A_COEFF].value);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCoeffA;
      rc = sscanf(response, "%*s = %d", &TCoeffA);
      if (rc != 1)
          return false;

      TemperatureCoeffN[FOCUS_A_COEFF].value = TCoeffA;

      // Temperature Coeff B
      if (isSimulation())
      {
          snprintf(response, 32, "TempCo B = %d", (int) TemperatureCoeffN[FOCUS_B_COEFF].value);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCoeffB;
      rc = sscanf(response, "%*s = %d", &TCoeffB);
      if (rc != 1)
          return false;

      TemperatureCoeffN[FOCUS_B_COEFF].value = TCoeffB;

      // Temperature Coeff C
      if (isSimulation())
      {
          snprintf(response, 32, "TempCo C = %d", (int) TemperatureCoeffN[FOCUS_C_COEFF].value);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCoeffC;
      rc = sscanf(response, "%*s = %d", &TCoeffC);
      if (rc != 1)
          return false;

      TemperatureCoeffN[FOCUS_C_COEFF].value = TCoeffC;

      // Temperature Coeff D
      if (isSimulation())
      {
          snprintf(response, 32, "TempCo D = %d", (int) TemperatureCoeffN[FOCUS_D_COEFF].value);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCoeffD;
      rc = sscanf(response, "%*s = %d", &TCoeffD);
      if (rc != 1)
          return false;

      TemperatureCoeffN[FOCUS_D_COEFF].value = TCoeffD;

      // Temperature Coeff E
      if (isSimulation())
      {
          snprintf(response, 32, "TempCo E = %d", (int) TemperatureCoeffN[FOCUS_E_COEFF].value);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCoeffE;
      rc = sscanf(response, "%*s = %d", &TCoeffE);
      if (rc != 1)
          return false;

      TemperatureCoeffN[FOCUS_E_COEFF].value = TCoeffE;

      TemperatureCoeffNP.s = IPS_OK;
      IDSetNumber(&TemperatureCoeffNP, NULL);


      // Temperature Compensation Mode
      tcflush(PortFD, TCIFLUSH);
      if (isSimulation())
      {
          snprintf(response, 32, "TC Mode = %c", 'C');
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      char compensateMode;
      rc = sscanf(response, "%*s = %c", &compensateMode);
      if (rc != 1)
          return false;

      IUResetSwitch(&TemperatureCompensateModeSP);
      int index = compensateMode - 'A';
      if (index >= 0 && index <= 5)
      {
          TemperatureCompensateModeS[index].s = ISS_ON;
          TemperatureCompensateModeSP.s = IPS_OK;
      }
      else
      {
          DEBUGF(INDI::Logger::DBG_ERROR, "Invalid index %d for compensation mode.", index);
          TemperatureCompensateModeSP.s = IPS_ALERT;
      }

      IDSetSwitch(&TemperatureCompensateModeSP, NULL);

      // Backlash Compensation
      tcflush(PortFD, TCIFLUSH);
      if (isSimulation())
      {
          snprintf(response, 32, "BLC En = %d", BacklashCompensationS[0].s == ISS_ON ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int BLCCompensate;
      rc = sscanf(response, "%*s = %d", &BLCCompensate);
      if (rc != 1)
          return false;

      IUResetSwitch(&BacklashCompensationSP);
      BacklashCompensationS[0].s = BLCCompensate ? ISS_ON : ISS_OFF;
      BacklashCompensationS[1].s = BLCCompensate ? ISS_OFF : ISS_ON;
      BacklashCompensationSP.s = IPS_OK;
      IDSetSwitch(&BacklashCompensationSP, NULL);


      // Backlash Value
      tcflush(PortFD, TCIFLUSH);
      if (isSimulation())
      {
          snprintf(response, 32, "BLC Stps = %d", 50);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int BLCValue;
      rc = sscanf(response, "%*s = %d", &BLCValue);
      if (rc != 1)
          return false;

      BacklashN[0].value = BLCValue;
      BacklashNP.s = IPS_OK;
      IDSetNumber(&BacklashNP, NULL);

      // Led brightnesss
      tcflush(PortFD, TCIFLUSH);
      if (isSimulation())
      {
          snprintf(response, 32, "LED Brt = %d", 75);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int LEDBrightness;
      rc = sscanf(response, "%*s = %d", &LEDBrightness);
      if (rc != 1)
          return false;

      // Temperature Compensation on Start
      tcflush(PortFD, TCIFLUSH);
      if (isSimulation())
      {
          snprintf(response, 32, "TC@Start = %d", TemperatureCompensateOnStartS[0].s == ISS_ON ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCOnStart;
      rc = sscanf(response, "%*s = %d", &TCOnStart);
      if (rc != 1)
          return false;

      IUResetSwitch(&TemperatureCompensateOnStartSP);
      TemperatureCompensateOnStartS[0].s = TCOnStart ? ISS_ON : ISS_OFF;
      TemperatureCompensateOnStartS[1].s = TCOnStart ? ISS_OFF : ISS_ON;
      TemperatureCompensateOnStartSP.s = IPS_OK;
      IDSetSwitch(&TemperatureCompensateOnStartSP, NULL);

      return true;
     }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::getFocusStatus()
{
    char cmd[] = "<F1GETSTATUS>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[32];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "STATUS1", 16);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      if (strcmp(response, "STATUS1"))
        return false;

      // Get Temperature
      if (isSimulation())
      {
          strncpy(response, "Temp(C) = +21.7", 16);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      float temperature=0;
      int rc = sscanf(response, "%*s = %g", &temperature);
      if (rc == 1)
      {
          TemperatureN[0].value = temperature;
          IDSetNumber(&TemperatureNP, NULL);
      }
      else
          return false;


      // Get Current Position
      if (isSimulation())
      {
          snprintf(response, 32, "Curr Pos = %06d", simPosition);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      uint32_t currPos=0;
      rc = sscanf(response, "%*s = %d", &currPos);
      if (rc == 1)
      {
          FocusAbsPosN[0].value = currPos;
          IDSetNumber(&FocusAbsPosNP, NULL);
      }
      else
          return false;

      // Get Target Position
      if (isSimulation())
      {
          snprintf(response, 32, "Targ Pos = %06d", simPosition);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      // Get Status Parameters

      // #1 is Moving?
      if (isSimulation())
      {
          snprintf(response, 32, "Is Moving = %d", (simStatus[STATUS_MOVING] == ISS_ON) ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int isMoving;
      rc = sscanf(response, "%*s = %d", &isMoving);
      if (rc != 1)
          return false;

      StatusL[STATUS_MOVING].s = isMoving ? IPS_BUSY : IPS_IDLE;

      // #2 is Homing?
      if (isSimulation())
      {
          snprintf(response, 32, "Is Homing = %d", (simStatus[STATUS_HOMING] == ISS_ON) ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int isHoming;
      rc = sscanf(response, "%*s = %d", &isHoming);
      if (rc != 1)
          return false;

      StatusL[STATUS_HOMING].s = isHoming ? IPS_BUSY : IPS_IDLE;

      // #3 is Homed?
      if (isSimulation())
      {
          snprintf(response, 32, "Is Homed = %d", (simStatus[STATUS_HOMED] == ISS_ON) ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int isHomed;
      rc = sscanf(response, "%*s = %d", &isHomed);
      if (rc != 1)
          return false;

      StatusL[STATUS_HOMED].s = isHomed ? IPS_OK : IPS_IDLE;

      // #4 FF Detected?
      if (isSimulation())
      {
          snprintf(response, 32, "FFDetect = %d", (simStatus[STATUS_FFDETECT] == ISS_ON) ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int FFDetect;
      rc = sscanf(response, "%*s = %d", &FFDetect);
      if (rc != 1)
          return false;

      StatusL[STATUS_FFDETECT].s = FFDetect ? IPS_OK : IPS_IDLE;

      // #5 Temperature probe?
      if (isSimulation())
      {
          snprintf(response, 32, "TmpProbe = %d", (simStatus[STATUS_TMPPROBE] == ISS_ON) ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TmpProbe;
      rc = sscanf(response, "%*s = %d", &TmpProbe);
      if (rc != 1)
          return false;

      StatusL[STATUS_TMPPROBE].s = TmpProbe ? IPS_OK : IPS_IDLE;

      // #6 Remote IO?
      if (isSimulation())
      {
          snprintf(response, 32, "RemoteIO = %d", (simStatus[STATUS_REMOTEIO] == ISS_ON) ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int RemoteIO;
      rc = sscanf(response, "%*s = %d", &RemoteIO);
      if (rc != 1)
          return false;

      StatusL[STATUS_REMOTEIO].s = RemoteIO ? IPS_OK : IPS_IDLE;

      // #7 Hand controller?
      if (isSimulation())
      {
          snprintf(response, 32, "Hnd Ctlr = %d", (simStatus[STATUS_HNDCTRL] == ISS_ON) ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int HndCtlr;
      rc = sscanf(response, "%*s = %d", &HndCtlr);
      if (rc != 1)
          return false;

      StatusL[STATUS_HNDCTRL].s = HndCtlr ? IPS_OK : IPS_IDLE;

      IDSetLight(&StatusLP, NULL);

      tcflush(PortFD, TCIFLUSH);

      return true;
     }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::setDeviceType(int index)
{
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::home()
{
    char cmd[] = "<F1HOME>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "H", 16);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      tcflush(PortFD, TCIFLUSH);

      return true;
     }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::center()
{
    char cmd[] = "<F1CENTER>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "M", 16);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      tcflush(PortFD, TCIFLUSH);

      return true;
     }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::setFocusPosition(u_int16_t position)
{
#if 0
    int nbytes_written=0, rc=-1;
    char errstr[MAXRBUF];
    char cmd[LYNXFOCUS_MAXBUF];

    if (position < FocusAbsPosN[0].min || position > FocusAbsPosN[0].max)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Requested position value out of bound: %d", position);
        return false;
    }

    if (FocusAbsPosNP.s == IPS_BUSY)
        AbortFocuser();

    snprintf(cmd, LYNXFOCUS_CMD_LONG+1, ":F9%07d#", position);

    tcflush(PortPortFD, TCIOFLUSH);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    // Goto absolute step
    if (!isSimulation() && (rc = tty_write(PortPortFD, cmd, LYNXFOCUS_CMD_LONG, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "setPosition error: %s.", errstr);
        return false;
    }

    targetPos = position;
#endif
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::setTemperatureCompensation(bool enable)
{
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::setTemperatureCompensationMode(char mode)
{
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::setTemperatureCompensationCoeff(u_int16_t coeff)
{
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::setTemperatureCompensationOnStart(bool enable)
{
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::setBacklashCompensation(bool enable)
{
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::setBacklashCompensationSteps(u_int16_t steps)
{
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::sync(u_int16_t position)
{
    //simPosition = position;
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::resetFactory()
{
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::isResponseOK()
{
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[32];
    int nbytes_read=0;

    if (isSimulation())
    {
        strcpy(response, "!");
        nbytes_read= strlen(response);
    }
    else
    {
        if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT , &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "TTY error: %s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      if (!strcmp(response, "!"))
          return true;
      else
      {
          DEBUGF(INDI::Logger::DBG_ERROR, "Controller error: %s", response);
          return false;
      }
    }
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::startMotion(FocusDirection dir)
{
#if 0
    int nbytes_written=0, rc=-1;
    char errstr[MAXRBUF];
    char cmd[LYNXFOCUS_MAXBUF];

    // inward  --> decreasing value --> DOWN
    // outward --> increasing value --> UP
    strncpy(cmd, (dir == FOCUS_INWARD) ? ":F2MDOW0#" : ":F1MUP00#", LYNXFOCUS_CMD);

    tcflush(PortPortFD, TCIOFLUSH);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (!isSimulation() && (rc = tty_write(PortPortFD, cmd, LYNXFOCUS_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "StartMotion error: %s.", errstr);
        return false;
    }
#endif
    return true;
}


/************************************************************************************
*
* ***********************************************************************************/
#if 0
IPState FocusLynx::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    //targetPos = targetTicks;

    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    snprintf(cmd, 16, "F1M%cR%c", (dir == FOCUS_INWARD) ? 'I' : 'O', (speed == 0) ? '0' : '1');

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "M", 16);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return IPS_ALERT;
        }

        if (isResponseOK() == false)
            return IPS_ALERT;

        gettimeofday(&focusMoveStart,NULL);
        //focusMoveRequest = duration/1000.0;

        if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return IPS_ALERT;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

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
#endif


/************************************************************************************
*
* ***********************************************************************************/
IPState FocusLynx::MoveAbsFocuser(uint32_t targetTicks)
{
    //targetPos = targetTicks;

    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    snprintf(cmd, 16, "F1MA%06d", targetTicks);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "M", 16);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return IPS_ALERT;
        }

        if (isResponseOK() == false)
            return IPS_ALERT;

        if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return IPS_ALERT;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      tcflush(PortFD, TCIFLUSH);

      return IPS_BUSY;
     }

    return IPS_ALERT;
}

/************************************************************************************
*
* ***********************************************************************************/
IPState FocusLynx::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t newPosition=0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;


    return MoveAbsFocuser(newPosition);
}

void FocusLynx::TimerHit()
{
#if 0
    if (isConnected() == false)
        return;

    bool rc = updatePosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > STEELDIVE_POSITION_THRESHOLD)
        {
            IDSetNumber(&FocusAbsPosNP, NULL);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    if (temperatureUpdateCounter++ > LYNXFOCUS_TEMPERATURE_FREQ)
    {
        temperatureUpdateCounter = 0;
        rc = updateTemperature();
        if (rc)
        {
            if (fabs(lastTemperature - TemperatureN[0].value) >= 0.5)
                lastTemperature = TemperatureN[0].value;
        }

        IDSetNumber(&TemperatureNP, NULL);
    }


    if (FocusTimerNP.s == IPS_BUSY)
    {
        float remaining = CalcTimeLeft(focusMoveStart, focusMoveRequest);

        if (isSimulation())
        {
            if (FocusMotionS[FOCUS_INWARD].s == ISS_ON)
            {
                FocusAbsPosN[0].value -= FocusSpeedN[0].value;
                if (FocusAbsPosN[0].value <= 0)
                    FocusAbsPosN[0].value =0;
                simPosition = FocusAbsPosN[0].value;
            }
            else
            {
                FocusAbsPosN[0].value += FocusSpeedN[0].value;
                if (FocusAbsPosN[0].value >= FocusAbsPosN[0].max)
                    FocusAbsPosN[0].value=FocusAbsPosN[0].max;
                simPosition = FocusAbsPosN[0].value;
            }
        }

        // If we exceed focus abs values, stop timer and motion
        if (FocusAbsPosN[0].value <= 0 || FocusAbsPosN[0].value >= FocusAbsPosN[0].max)
        {
            AbortFocuser();

            if (FocusAbsPosN[0].value <= 0)
                FocusAbsPosN[0].value =0;
            else
                FocusAbsPosN[0].value=FocusAbsPosN[0].max;

            FocusTimerN[0].value=0;
            FocusTimerNP.s = IPS_IDLE;
        }
        else if (remaining <= 0)
        {
            FocusTimerNP.s = IPS_OK;
            FocusTimerN[0].value = 0;
            AbortFocuser();
        }
        else
            FocusTimerN[0].value = remaining*1000.0;

        IDSetNumber(&FocusTimerNP, NULL);
    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (isSimulation())
        {
            if (FocusAbsPosN[0].value < targetPos)
                simPosition += 100;
            else
                simPosition -= 100;

            if (fabs(simPosition - targetPos) < 100)
            {
                FocusAbsPosN[0].value = targetPos;
                simPosition = FocusAbsPosN[0].value;
            }
        }

        // Set it OK to within 5 steps
        if (fabs(targetPos - FocusAbsPosN[0].value) < 5)
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, NULL);
            IDSetNumber(&FocusRelPosNP, NULL);
            lastPos = FocusAbsPosN[0].value;
            DEBUG(INDI::Logger::DBG_SESSION, "Focuser reached requested position.");
        }
    }

    SetTimer(POLLMS);

#endif
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::AbortFocuser()
{
    char cmd[] = "<F1HALT>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "HALTED", 16);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if (isResponseOK() == false)
            return false;

        if ( (errcode = tty_read_section(PortFD, response, 0x10, LYNXFOCUS_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      if (FocusRelPosNP.s == IPS_BUSY)
      {
          FocusRelPosNP.s = IPS_IDLE;
          IDSetNumber(&FocusRelPosNP, NULL);
      }

      FocusTimerNP.s = FocusAbsPosNP.s = IPS_IDLE;
      IDSetNumber(&FocusTimerNP, NULL);
      IDSetNumber(&FocusAbsPosNP, NULL);

      tcflush(PortFD, TCIFLUSH);

      return true;
     }

    return false;

}

/************************************************************************************
 *
* ***********************************************************************************/
float FocusLynx::calcTimeLeft(timeval start,float req)
{  
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec/1000);
    timesince=timesince/1000;
    timeleft=req-timesince;
    return timeleft;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::saveConfigItems(FILE *fp)
{
#if 0
    INDI::Focuser::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &TemperatureSettingNP);
    IUSaveConfigSwitch(fp, &TemperatureCompensateSP);
    IUSaveConfigNumber(fp, &FocusSpeedNP);
    IUSaveConfigNumber(fp, &AccelerationNP);
    IUSaveConfigNumber(fp, &CustomSettingNP);
    IUSaveConfigSwitch(fp, &ModelSP);

    return saveFocuserConfig();
#endif
}

/************************************************************************************
 *
* ***********************************************************************************/
void FocusLynx::debugTriggered(bool enable)
{
    //tty_set_debug(enable ? 1 : 0);
}
