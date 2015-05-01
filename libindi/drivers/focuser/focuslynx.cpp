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
#define LYNXFOCUS_TIMEOUT              2
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

    focusMoveRequest = 0;
    simPosition=0;

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
    IUFillNumber(&TemperatureCoeffN[0], "A", "", "%.f", -9999, 9999, 100., 0.);
    IUFillNumber(&TemperatureCoeffN[1], "B", "", "%.f", -9999, 9999, 100., 0.);
    IUFillNumber(&TemperatureCoeffN[2], "C", "", "%.f", -9999, 9999, 100., 0.);
    IUFillNumber(&TemperatureCoeffN[3], "D", "", "%.f", -9999, 9999, 100., 0.);
    IUFillNumber(&TemperatureCoeffN[4], "E", "", "%.f", -9999, 9999, 100., 0.);
    IUFillNumberVector(&TemperatureCoeffNP, TemperatureCoeffN, 5, getDeviceName(), "T. Coeff", "", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Enable/Disable temperature Mode
    IUFillSwitch(&TemperatureCompensateModeS[0], "A", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateModeS[1], "B", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateModeS[2], "C", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateModeS[3], "D", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateModeS[4], "E", "", ISS_OFF);
    IUFillSwitchVector(&TemperatureCompensateModeSP, TemperatureCompensateModeS, 5, getDeviceName(), "Compensate Mode", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Enable/Disable backlash
    IUFillSwitch(&BacklashCompensationS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&BacklashCompensationS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&BacklashCompensationSP, BacklashCompensationS, 2, getDeviceName(), "Backlash Compensation", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Backlash Value
    IUFillNumber(&BacklashN[0], "Value", "", "%.f", 0, 99, 5., 0.);
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
    IUFillSwitchVector(&ModelSP, ModelS, nModels, getDeviceName(), "Model", "", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Sync to a particular position
    IUFillNumber(&SyncN[0], "Ticks", "", "%.f", 0, 200000, 100., 0.);
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
        defineSwitch(&TemperatureCompensateModeSP);
        defineSwitch(&TemperatureCompensateSP);        
        defineSwitch(&TemperatureCompensateOnStartSP);

        defineSwitch(&BacklashCompensationSP);
        defineNumber(&BacklashNP);

        defineSwitch(&ResetSP);
        defineSwitch(&GotoSP);

        defineLight(&StatusLP);

        if (getFocusConfig())
            DEBUG(INDI::Logger::DBG_SESSION, "FocusLynx paramaters updated, focuser ready for use.");
        else
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Failed to retrieve focuser configuration settings...");
            return false;
        }
    }
    else
    {
        deleteProperty(SyncNP.name);

        deleteProperty(TemperatureNP.name);
        deleteProperty(TemperatureCoeffNP.name);
        deleteProperty(TemperatureCompensateModeSP.name);
        deleteProperty(TemperatureCompensateSP.name);        
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

    configurationComplete = false;

    int modelIndex = IUFindOnSwitchIndex(&ModelSP);
    if (modelIndex == 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "You must select a model before establishing connection");
        return false;
    }

    if (!isSimulation() && (connectrc = tty_connect(PortT[0].text, 115200, 8, 0, 1, &PortFD)) != TTY_OK)
    {
        tty_error_msg(connectrc, errorMsg, MAXRBUF);

        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to connect to port %s. Error: %s", PortT[0].text, errorMsg);

        return false;

    }

    if (ack())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "FocusLynx is online. Getting focus parameters...");
        setDeviceType(modelIndex);
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
    if(strcmp(dev,getDeviceName())==0)
    {        
        // Models
        if (!strcmp(ModelSP.name, name))
        {
            IUUpdateSwitch(&ModelSP, states, names, n);            
            ModelSP.s = IPS_OK;
            IDSetSwitch(&ModelSP, NULL);
            DEBUG(INDI::Logger::DBG_SESSION, "Focuser model set. Please connect now...");
            return true;
        }

        // Temperature Compensation
        if (!strcmp(TemperatureCompensateSP.name, name))
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
                TemperatureCompensateSP.s = IPS_ALERT;
                TemperatureCompensateS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&TemperatureCompensateSP, NULL);
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
                TemperatureCompensateOnStartSP.s = IPS_ALERT;
                TemperatureCompensateOnStartS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&TemperatureCompensateOnStartSP, NULL);
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
                TemperatureCompensateModeSP.s = IPS_ALERT;
                TemperatureCompensateModeS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&TemperatureCompensateModeSP, NULL);
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
                BacklashCompensationSP.s = IPS_ALERT;
                BacklashCompensationS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&BacklashCompensationSP, NULL);
            return true;
        }

        // Reset to Factory setting
        if (!strcmp(ResetSP.name, name))
        {
            IUResetSwitch(&ResetSP);
            if (resetFactory())
                ResetSP.s = IPS_OK;
            else
                ResetSP.s = IPS_ALERT;

            IDSetSwitch(&ResetSP, NULL);
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

            IDSetSwitch(&GotoSP, NULL);
            return true;
        }

    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{

    if(strcmp(dev,getDeviceName())==0)
    {
        // Temperature Coefficient
        if (!strcmp(TemperatureCoeffNP.name, name))
        {
            IUUpdateNumber(&TemperatureCoeffNP, values, names, n);
            for (int i=0; i < n; i++)
            {
                if (setTemperatureCompensationCoeff('A' + i, TemperatureCoeffN[i].value) == false)
                {
                    DEBUG(INDI::Logger::DBG_ERROR, "Failed to set temperature coefficeints.");
                    TemperatureCoeffNP.s = IPS_ALERT;
                    IDSetNumber(&TemperatureCoeffNP, NULL);
                    return false;
                }
            }

            TemperatureCoeffNP.s = IPS_OK;
            IDSetNumber(&TemperatureCoeffNP, NULL);
            return true;
        }

        // Backlash Value
        if (!strcmp(BacklashNP.name, name))
        {
            IUUpdateNumber(&BacklashNP, values, names, n);
            if (setBacklashCompensationSteps(BacklashN[0].value))
            {
                    DEBUG(INDI::Logger::DBG_ERROR, "Failed to set temperature coefficeints.");
                    BacklashNP.s = IPS_ALERT;
                    IDSetNumber(&BacklashNP, NULL);
                    return false;
            }

            BacklashNP.s = IPS_OK;
            IDSetNumber(&BacklashNP, NULL);
            return true;
        }

        // Sync
        if (!strcmp(SyncNP.name, name))
        {
            IUUpdateNumber(&SyncNP, values, names, n);
            if (sync(SyncN[0].value) == false)
                SyncNP.s = IPS_ALERT;
            else
                SyncNP.s = IPS_OK;

            IDSetNumber(&SyncNP, NULL);
            return true;
        }
    }

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

    memset(response, 0, sizeof(response));

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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
      DEBUGF(INDI::Logger::DBG_SESSION, "%s is detected.", response);

      return true;
     }

    return false;
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
    char key[16];

    memset(response, 0, sizeof(response));

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "CONFIG1\n", 16);
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      if (strcmp(response, "CONFIG1"))
        return false;
    }

     memset(response, 0, sizeof(response));

      // Nickname
      if (isSimulation())
      {
          strncpy(response, "Optec 2\" TCF-S\n", 16);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      memset(response, 0, sizeof(response));

      // Get Max Position
      if (isSimulation())
      {
          snprintf(response, 32, "Max Pos = %06d\n", 100000);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      uint32_t maxPos=0;
      int rc = sscanf(response, "%16[^=]=%d", key, &maxPos);
      if (rc == 2)
      {
          FocusAbsPosN[0].max = SyncN[0].max = maxPos;
          FocusAbsPosN[0].step = SyncN[0].step = maxPos/50.0;
          FocusAbsPosN[0].min = SyncN[0].min = 0;

          FocusRelPosN[0].max = maxPos/2;
          FocusRelPosN[0].step = maxPos/100.0;
          FocusRelPosN[0].min = 0;

          IUUpdateMinMax(&FocusAbsPosNP);
          IUUpdateMinMax(&FocusRelPosNP);
          IUUpdateMinMax(&SyncNP);
      }
      else
          return false;

      memset(response, 0, sizeof(response));

      // Get Device Type
      if (isSimulation())
      {
          snprintf(response, 32, "Dev Typ = %s\n", "OA");
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      // Get Status Parameters

      memset(response, 0, sizeof(response));

      // Temperature Compensation On?
      if (isSimulation())
      {
          snprintf(response, 32, "TComp ON = %d\n", TemperatureCompensateS[0].s == ISS_ON ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCompOn;
      rc = sscanf(response, "%16[^=]=%d", key, &TCompOn);
      if (rc != 2)
          return false;

      IUResetSwitch(&TemperatureCompensateSP);
      TemperatureCompensateS[0].s = TCompOn ? ISS_ON : ISS_OFF;
      TemperatureCompensateS[0].s = TCompOn ? ISS_OFF : ISS_ON;
      TemperatureCompensateSP.s = IPS_OK;
      IDSetSwitch(&TemperatureCompensateSP, NULL);

      memset(response, 0, sizeof(response));

      // Temperature Coeff A
      if (isSimulation())
      {
          snprintf(response, 32, "TempCo A = %d\n", (int) TemperatureCoeffN[FOCUS_A_COEFF].value);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCoeffA;
      rc = sscanf(response, "%16[^=]=%d", key, &TCoeffA);
      if (rc != 2)
          return false;

      TemperatureCoeffN[FOCUS_A_COEFF].value = TCoeffA;

      memset(response, 0, sizeof(response));

      // Temperature Coeff B
      if (isSimulation())
      {
          snprintf(response, 32, "TempCo B = %d\n", (int) TemperatureCoeffN[FOCUS_B_COEFF].value);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCoeffB;
      rc = sscanf(response, "%16[^=]=%d", key, &TCoeffB);
      if (rc != 2)
          return false;

      TemperatureCoeffN[FOCUS_B_COEFF].value = TCoeffB;

      memset(response, 0, sizeof(response));

      // Temperature Coeff C
      if (isSimulation())
      {
          snprintf(response, 32, "TempCo C = %d\n", (int) TemperatureCoeffN[FOCUS_C_COEFF].value);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCoeffC;
      rc = sscanf(response, "%16[^=]=%d", key, &TCoeffC);
      if (rc != 2)
          return false;

      TemperatureCoeffN[FOCUS_C_COEFF].value = TCoeffC;

      memset(response, 0, sizeof(response));

      // Temperature Coeff D
      if (isSimulation())
      {
          snprintf(response, 32, "TempCo D = %d\n", (int) TemperatureCoeffN[FOCUS_D_COEFF].value);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCoeffD;
      rc = sscanf(response, "%16[^=]=%d", key, &TCoeffD);
      if (rc != 2)
          return false;

      TemperatureCoeffN[FOCUS_D_COEFF].value = TCoeffD;

      memset(response, 0, sizeof(response));

      // Temperature Coeff E
      if (isSimulation())
      {
          snprintf(response, 32, "TempCo E = %d\n", (int) TemperatureCoeffN[FOCUS_E_COEFF].value);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCoeffE;
      rc = sscanf(response, "%16[^=]=%d", key, &TCoeffE);
      if (rc != 2)
          return false;

      TemperatureCoeffN[FOCUS_E_COEFF].value = TCoeffE;

      TemperatureCoeffNP.s = IPS_OK;
      IDSetNumber(&TemperatureCoeffNP, NULL);

      memset(response, 0, sizeof(response));

      // Temperature Compensation Mode
      if (isSimulation())
      {
          snprintf(response, 32, "TC Mode = %c\n", 'C');
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      char compensateMode;
      rc = sscanf(response, "%16[^=]=%c", key, &compensateMode);
      if (rc != 2)
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
      memset(response, 0, sizeof(response));
      if (isSimulation())
      {
          snprintf(response, 32, "BLC En = %d\n", BacklashCompensationS[0].s == ISS_ON ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int BLCCompensate;
      rc = sscanf(response, "%16[^=]=%d", key, &BLCCompensate);
      if (rc != 2)
          return false;

      IUResetSwitch(&BacklashCompensationSP);
      BacklashCompensationS[0].s = BLCCompensate ? ISS_ON : ISS_OFF;
      BacklashCompensationS[1].s = BLCCompensate ? ISS_OFF : ISS_ON;
      BacklashCompensationSP.s = IPS_OK;
      IDSetSwitch(&BacklashCompensationSP, NULL);


      // Backlash Value
      memset(response, 0, sizeof(response));
      if (isSimulation())
      {
          snprintf(response, 32, "BLC Stps = %d\n", 50);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int BLCValue;
      rc = sscanf(response, "%16[^=]=%d", key, &BLCValue);
      if (rc != 2)
          return false;

      BacklashN[0].value = BLCValue;
      BacklashNP.s = IPS_OK;
      IDSetNumber(&BacklashNP, NULL);

      // Led brightnesss
      memset(response, 0, sizeof(response));
      if (isSimulation())
      {
          snprintf(response, 32, "LED Brt = %d\n", 75);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int LEDBrightness;
      rc = sscanf(response, "%16[^=]=%d", key, &LEDBrightness);
      if (rc != 2)
          return false;

      // Temperature Compensation on Start
      memset(response, 0, sizeof(response));
      if (isSimulation())
      {
          snprintf(response, 32, "TC@Start = %d\n", TemperatureCompensateOnStartS[0].s == ISS_ON ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int TCOnStart;
      rc = sscanf(response, "%16[^=]=%d", key, &TCOnStart);
      if (rc != 2)
          return false;

      IUResetSwitch(&TemperatureCompensateOnStartSP);
      TemperatureCompensateOnStartS[0].s = TCOnStart ? ISS_ON : ISS_OFF;
      TemperatureCompensateOnStartS[1].s = TCOnStart ? ISS_OFF : ISS_ON;
      TemperatureCompensateOnStartSP.s = IPS_OK;
      IDSetSwitch(&TemperatureCompensateOnStartSP, NULL);

      tcflush(PortFD, TCIFLUSH);

      configurationComplete = true;

      return true;
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
    char key[16];

    memset(response, 0, sizeof(response));

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "STATUS1\n", 16);
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      if (strcmp(response, "STATUS1"))
        return false;

      // Get Temperature
      memset(response, 0, sizeof(response));
      if (isSimulation())
      {
          strncpy(response, "Temp(C) = +21.7\n", 16);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      float temperature=0;
      int rc = sscanf(response, "%16[^=]=%f", key, &temperature);
      if (rc == 2)
      {
          TemperatureN[0].value = temperature;
          IDSetNumber(&TemperatureNP, NULL);
      }
      else
          return false;


      // Get Current Position
      memset(response, 0, sizeof(response));
      if (isSimulation())
      {
          snprintf(response, 32, "Curr Pos = %06d\n", simPosition);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      uint32_t currPos=0;
      rc = sscanf(response, "%16[^=]=%d", key, &currPos);
      if (rc == 2)
      {
          FocusAbsPosN[0].value = currPos;
          IDSetNumber(&FocusAbsPosNP, NULL);
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
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      // Get Status Parameters

      // #1 is Moving?
      memset(response, 0, sizeof(response));
      if (isSimulation())
      {
          snprintf(response, 32, "Is Moving = %d\n", (simStatus[STATUS_MOVING] == ISS_ON) ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

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
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int isHoming;
      rc = sscanf(response, "%16[^=]=%d", key, &isHoming);
      if (rc != 2)
          return false;

      StatusL[STATUS_HOMING].s = isHoming ? IPS_BUSY : IPS_IDLE;

      // #3 is Homed?
      memset(response, 0, sizeof(response));
      if (isSimulation())
      {
          snprintf(response, 32, "Is Homed = %d\n", (simStatus[STATUS_HOMED] == ISS_ON) ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int isHomed;
      rc = sscanf(response, "%16[^=]=%d", key, &isHomed);
      if (rc != 2)
          return false;

      StatusL[STATUS_HOMED].s = isHomed ? IPS_OK : IPS_IDLE;

      // #4 FF Detected?
      memset(response, 0, sizeof(response));
      if (isSimulation())
      {
          snprintf(response, 32, "FFDetect = %d\n", (simStatus[STATUS_FFDETECT] == ISS_ON) ? 1 : 0);
          nbytes_read = strlen(response);
      }
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

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
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

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
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

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
      else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
      {
          tty_error_msg(errcode, errmsg, MAXRBUF);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
          return false;
      }
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      int HndCtlr;
      rc = sscanf(response, "%16[^=]=%d", key, &HndCtlr);
      if (rc != 2)
          return false;

      StatusL[STATUS_HNDCTRL].s = HndCtlr ? IPS_OK : IPS_IDLE;

      StatusLP.s = IPS_OK;
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
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<F1SCDT%s>", ModelS[index].name);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
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
bool FocusLynx::home()
{
    char cmd[] = "<F1HOME>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    memset(response, 0, sizeof(response));

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "H", 16);
        nbytes_read = strlen(response);
        simStatus[STATUS_HOMING] = ISS_ON;
        targetPosition = 0;
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
      FocusAbsPosNP.s = IPS_BUSY;
      IDSetNumber(&FocusAbsPosNP, NULL);

      DEBUG(INDI::Logger::DBG_SESSION, "Focuser moving to home position...");

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

    memset(response, 0, sizeof(response));

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "M", 16);
        nbytes_read = strlen(response);
        simStatus[STATUS_MOVING] = ISS_ON;
        targetPosition = FocusAbsPosN[0].max/2;
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      DEBUG(INDI::Logger::DBG_SESSION, "Focuser moving to center position...");

      FocusAbsPosNP.s = IPS_BUSY;
      IDSetNumber(&FocusAbsPosNP, NULL);

      tcflush(PortFD, TCIFLUSH);

      return true;
     }

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynx::setTemperatureCompensation(bool enable)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<F1SCTE%d>", enable ? 1 : 0);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
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
bool FocusLynx::setTemperatureCompensationMode(char mode)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<F1SCTM%c>", mode);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
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
bool FocusLynx::setTemperatureCompensationCoeff(char mode, int16_t coeff)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<F1SCTC%c%c%04d>", mode, coeff >= 0 ? '+' : '-', abs(coeff));

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
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
bool FocusLynx::setTemperatureCompensationOnStart(bool enable)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<F1SCTS%d>", enable ? 1 : 0);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
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
bool FocusLynx::setBacklashCompensation(bool enable)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<F1SCBE%d>", enable ? 1 : 0);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
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
bool FocusLynx::setBacklashCompensationSteps(uint16_t steps)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "<F1SCBS%02d>", steps);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
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
bool FocusLynx::sync(u_int32_t position)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "F1SCCP%06d", position);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
        nbytes_read = strlen(response);
        simPosition = position;
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
            return false;

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
      tcflush(PortFD, TCIFLUSH);

      if (!strcmp(response, "SET"))
      {
          DEBUGF(INDI::Logger::DBG_SESSION, "Setting current position to %d", position);
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
bool FocusLynx::resetFactory()
{
    char cmd[] = "<F1RESET>";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    memset(response, 0, sizeof(response));

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "SET", 16);
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
      tcflush(PortFD, TCIFLUSH);

      if (!strcmp(response, "SET"))
      {
        return true;
        getFocusConfig();
      }
      else
        return false;
     }

    return false;
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

    memset(response, 0, sizeof(response));

    if (isSimulation())
    {
        strcpy(response, "!");
        nbytes_read= strlen(response);
    }
    else
    {
        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT , &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "TTY error: %s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      if (response[0] == '!')
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
IPState FocusLynx::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{    
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    memset(response, 0, sizeof(response));

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
        focusMoveRequest = duration/1000.0;

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return IPS_ALERT;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
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

/************************************************************************************
*
* ***********************************************************************************/
IPState FocusLynx::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    targetPosition = targetTicks;

    memset(response, 0, sizeof(response));

    snprintf(cmd, 16, "F1MA%06d", targetTicks);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "M", 16);
        nbytes_read = strlen(response);
        simStatus[STATUS_MOVING] = ISS_ON;
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return IPS_ALERT;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
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
    if (isConnected() == false)
        return;

    if (configurationComplete == false)
    {
       SetTimer(POLLMS);
       return;
    }

    getFocusStatus();

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (isSimulation())
        {
            if (FocusAbsPosN[0].value < targetPosition)
                simPosition += 100;
            else
                simPosition -= 100;

            simStatus[STATUS_MOVING] = ISS_ON;

            if (fabs(simPosition - targetPosition) < 100)
            {
                FocusAbsPosN[0].value = targetPosition;
                simPosition = FocusAbsPosN[0].value;
                simStatus[STATUS_MOVING] = ISS_OFF;
                StatusL[STATUS_MOVING].s = IPS_IDLE;
                if (simStatus[STATUS_HOMING] == ISS_ON)
                {
                    StatusL[STATUS_HOMED].s = IPS_OK;
                    simStatus[STATUS_HOMING] == ISS_OFF;
                }
            }
        }

        if (StatusL[STATUS_HOMED].s == IPS_OK)
        {
            GotoSP.s = IPS_OK;
            IUResetSwitch(&GotoSP);
            GotoS[GOTO_HOME].s = ISS_ON;
            IDSetSwitch(&GotoSP, NULL);
            FocusAbsPosNP.s = IPS_OK;
            IDSetNumber(&FocusRelPosNP, NULL);
            DEBUG(INDI::Logger::DBG_SESSION, "Focuser reached home position.");
        }
        else if (StatusL[STATUS_MOVING].s == IPS_IDLE)
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, NULL);
            IDSetNumber(&FocusRelPosNP, NULL);
            if (GotoSP.s == IPS_BUSY)
            {
                IUResetSwitch(&GotoSP);
                GotoSP.s = IPS_OK;
                IDSetSwitch(&GotoSP, NULL);
            }
            DEBUG(INDI::Logger::DBG_SESSION, "Focuser reached requested position.");
        }
        else if (StatusL[STATUS_MOVING].s == IPS_BUSY && focusMoveRequest > 0)
        {
            float remaining = calcTimeLeft(focusMoveStart, focusMoveRequest);

            if (remaining < POLLMS)
            {
                sleep(remaining);
                AbortFocuser();
                focusMoveRequest=0;
            }
        }
    }

    if (StatusL[STATUS_HOMING].s == IPS_BUSY && GotoSP.s != IPS_BUSY)
    {
        GotoSP.s = IPS_BUSY;
        IDSetSwitch(&GotoSP, NULL);
    }


    SetTimer(POLLMS);

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

    memset(response, 0, sizeof(response));

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (isSimulation())
    {
        strncpy(response, "HALTED", 16);
        nbytes_read = strlen(response);
        simStatus[STATUS_MOVING] = ISS_OFF;
        simStatus[STATUS_HOMING] = ISS_OFF;
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

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read-1] = '\0';
      DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      if (FocusRelPosNP.s == IPS_BUSY)
      {
          FocusRelPosNP.s = IPS_IDLE;
          IDSetNumber(&FocusRelPosNP, NULL);
      }

      FocusTimerNP.s = FocusAbsPosNP.s = GotoSP.s = IPS_IDLE;
      IUResetSwitch(&GotoSP);
      IDSetNumber(&FocusTimerNP, NULL);
      IDSetNumber(&FocusAbsPosNP, NULL);
      IDSetSwitch(&GotoSP, NULL);

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
    INDI::Focuser::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &ModelSP);
    IUSaveConfigSwitch(fp, &TemperatureCompensateSP);
    IUSaveConfigSwitch(fp, &TemperatureCompensateOnStartSP);
    IUSaveConfigNumber(fp, &TemperatureCoeffNP);
    IUSaveConfigSwitch(fp, &TemperatureCompensateModeSP);
    IUSaveConfigSwitch(fp, &BacklashCompensationSP);
    IUSaveConfigNumber(fp, &BacklashNP);

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
void FocusLynx::debugTriggered(bool enable)
{
    //tty_set_debug(enable ? 1 : 0);
}
