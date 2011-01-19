/*
    INDI Driver for Optec TCF-S Focuser

    Copyright (C) 2010 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <memory>
#include <indicom.h>

#include "tcfs.h"

#define mydev                   "Optec TCF-S"
#define nFocusSteps             FocusStepNP->np[0].value
#define nFocusCurrentPosition   FocusPositionNP->np[0].value
#define nFocusTargetPosition    FocusPositionRequestNP->np[0].value
#define nFocusTemperature       FocusTemperatureNP->np[0].value
#define isFocusSleep            (FocusPowerSP->sp[0].s == ISS_ON)

const int POLLMS = 1000;


// We declare an auto pointer to TCFS.
auto_ptr<TCFS> tcfs(0);

void ISPoll(void *p);


void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(tcfs.get() == 0) tcfs.reset(new TCFS());
    IEAddTimer(POLLMS, ISPoll, NULL);

}

void ISPoll(void *p)
{
    tcfs->ISPoll();
    IEAddTimer(POLLMS, ISPoll, NULL);
}

void ISGetProperties(const char *dev)
{ 
	if(dev && strcmp(mydev, dev)) return;
	ISInit();
        tcfs->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
	if(dev && strcmp (mydev, dev)) return;
	ISInit();
        tcfs->ISNewSwitch(name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
	if(dev && strcmp (mydev, dev)) return;
	ISInit();
        tcfs->ISNewText(name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
	if(dev && strcmp (mydev, dev)) return;
	ISInit();
        tcfs->ISNewNumber(name, values, names, num);
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
    INDI_UNUSED(root);
}

/****************************************************************
**
**
*****************************************************************/
TCFS::TCFS()
{
    const char *skelFileName = "/usr/share/indi/indi_tcfs_sk.xml";
    struct stat st;

    char *skel = getenv("INDISKEL");
    if (skel)
      buildSkeleton(skel);
    else if (stat(skelFileName,&st) == 0)
       buildSkeleton(skelFileName);
    else
       IDLog("No skeleton file was specified. Set environment variable INDISKEL to the skeleton path and try again.\n");

     // Optional: Add aux controls for configuration, debug & simulation
     addAuxControls();

     simulated_position    = 3000;
     simulated_temperature = 25.4;

}

/****************************************************************
**
**
*****************************************************************/
TCFS::~TCFS()
{
}

/****************************************************************
**
**
*****************************************************************/
void TCFS::ISGetProperties(const char *dev)
{
    static int propInit=0;

    INDI::DefaultDriver::ISGetProperties(dev);

    if (propInit == 0)
    {
        init_properties();

        loadConfig();

        isTCFS3 = false;

        // Set upper limit for TCF-S3 focuser
        if (!strcmp(me, "indi_tcfs3_focus"))
        {
            isTCFS3 = true;

            FocusPositionRequestNP->np[0].max = 9999;
            IUUpdateMinMax(FocusPositionRequestNP);
            if (isDebug())
               IDLog("TCF-S3 detected. Updating maximum position value to 9999.\n");
        }

        propInit = 1;      
    }

}

/****************************************************************
**
**
*****************************************************************/
void TCFS::init_properties()
{
    ConnectSP = getSwitch("CONNECTION");
    FocusStepNP = getNumber("FOCUS_STEP");
    FocusPositionNP = getNumber("FOCUS_POSITION");
    FocusPositionRequestNP = getNumber("FOCUS_POSITION_REQUEST");
    FocusTemperatureNP = getNumber("FOCUS_TEMPERATURE");
    FocusPowerSP = getSwitch("FOCUS_POWER");
    FocusModeSP  = getSwitch("FOCUS_MODE");
}

/****************************************************************
**
**
*****************************************************************/   
bool TCFS::connect()
{
    ITextVectorProperty *tProp = getText("DEVICE_PORT");

   if (isConnected())
		return true;

    if (tProp == NULL)
        return false;

    if (isSimulation())
    {
        setConnected(true);

        IDSetSwitch(ConnectSP, "TCF-S: Simulating connection to port %s.", tProp->tp[0].text);

        fd=-1;
        IUResetSwitch(FocusModeSP);
        FocusModeSP->sp[0].s = ISS_ON;
        FocusModeSP->s       = IPS_OK;
        IDSetSwitch(FocusModeSP, NULL);

        FocusPositionNP->s = IPS_OK;
        IDSetNumber(FocusPositionNP, NULL);

        FocusTemperatureNP->s = IPS_OK;
        IDSetNumber(FocusTemperatureNP, NULL);

        IUResetSwitch(FocusPowerSP);
        IDSetSwitch(FocusPowerSP, NULL);
	return true;
    }

    if (isDebug())
        IDLog("Attempting to connect to TCF-S focuser....\n");

    if (tty_connect(tProp->tp[0].text, 19200, 8, 0, 1, &fd) != TTY_OK)
    {
        ConnectSP->s = IPS_ALERT;
        IDSetSwitch (ConnectSP, "Error connecting to port %s. Make sure you have BOTH read and write permission to the port.", tProp->tp[0].text);
	return false;
    }

    for (int i=0; i < TCFS_MAX_TRIES; i++)
    {
        dispatch_command(FMMODE);

        if (read_tcfs() == true)
        {
            if (!strcmp(response, "!"))
            {
                setConnected(true, IPS_OK, "Successfully connected to TCF-S Focuser in Manual Mode.");

                IUResetSwitch(FocusModeSP);
                FocusModeSP->sp[0].s = ISS_ON;
                FocusModeSP->s       = IPS_OK;
                IDSetSwitch(FocusModeSP, NULL);

                FocusPositionNP->s = IPS_OK;
                IDSetNumber(FocusPositionNP, NULL);

                FocusTemperatureNP->s = IPS_OK;
                IDSetNumber(FocusTemperatureNP, NULL);

                IUResetSwitch(FocusPowerSP);
                IDSetSwitch(FocusPowerSP, NULL);

                return true;
             }
        }

        usleep(500000);
    }

    setConnected(false, IPS_ALERT, "Error connecting to TCF-S focuser...");
    return false;

}

/****************************************************************
**
**
*****************************************************************/   
void TCFS::disconnect()
{

    FocusPositionNP->s = IPS_IDLE;
    IDSetNumber(FocusPositionNP, NULL);

    FocusTemperatureNP->s = IPS_IDLE;
    IDSetNumber(FocusTemperatureNP, NULL);

    dispatch_command(FFMODE);

    tty_disconnect(fd);

    setConnected(false, IPS_OK, "Disconnected from TCF-S.");
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::ISNewNumber (const char *name, double values[], char *names[], int n)
{

  INumberVectorProperty *nProp = getNumber(name);

  if (nProp == NULL)
      return false;

  if (isConnected() == false)
  {
      resetProperties();
      IDMessage(deviceID, "TCF-S is offline. Connect before issiung any commands.");
      return false;
  }

  if (!strcmp(nProp->name, "FOCUS_STEP"))
  {
      IUUpdateNumber(nProp, values, names, n);
      nProp->s = IPS_OK;
      IDSetNumber(nProp, NULL);
      return true;
  }

  if (isFocusSleep)
  {
      nProp->s = IPS_IDLE;
      IDSetNumber(nProp, "Focuser is still in sleep mode. Wake up in order to issue commands.");
      return true;
  }

  if (!strcmp(nProp->name, "FOCUS_POSITION_REQUEST"))
  {
      int current_step = nFocusSteps;
      IUUpdateNumber(nProp, values, names, n);

      nFocusSteps = fabs(nFocusTargetPosition - nFocusCurrentPosition);

      if ( (nFocusTargetPosition - nFocusCurrentPosition) > 0)
          move_focuser(TCFS_OUTWARD);
      else
          move_focuser(TCFS_INWARD);

      nFocusSteps = current_step;

      FocusPositionNP->s = IPS_BUSY;
      nProp->s = IPS_BUSY;
      IDSetNumber(nProp, "Moving focuser to new position %g...", nFocusTargetPosition);
      return true;
  }
    
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::ISNewText (const char *name, char *texts[], char *names[], int n)
{
    ITextVectorProperty * tProp = getText(name);

    if (tProp == NULL)
        return false;

    // Device Port Text
    if (!strcmp(tProp->name, "DEVICE_PORT"))
    {
        if (IUUpdateText(tProp, texts, names, n) < 0)
                        return false;

                tProp->s = IPS_OK;
                IDSetText(tProp, "Port updated.");

                return true;
    }


    return false;
	
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::ISNewSwitch (const char *name, ISState *states, char *names[], int n)
{

    ISwitch *current_active_switch = NULL, *target_active_switch = NULL;
    // First process parent!
    if (INDI::DefaultDriver::ISNewSwitch(deviceID, name, states, names, n) == true)
        return true;

    ISwitchVectorProperty *sProp = getSwitch(name);

    if (sProp == NULL)
        return false;

    if (!strcmp(sProp->name, "CONNECTION"))
    {
        if (!strcmp(names[0], "CONNECT"))
            connect();
        else
            disconnect();
        return true;
    }


    if (isConnected() == false)
    {
      resetProperties();
      IDMessage(deviceID, "TCF-S is offline. Connect before issiung any commands.");
      return false;
    }

    // Which switch is CURRENTLY on?
    current_active_switch = IUFindOnSwitch(sProp);

    IUUpdateSwitch(sProp, states, names, n);

    // Which switch the CLIENT wants to turn on?
    target_active_switch = IUFindOnSwitch(sProp);

    if (target_active_switch == NULL)
    {
        if (isDebug())
        {
            IDLog("Error: no ON switch found in %s property.\n", sProp->name);
        }
        return true;
    }


    if (!strcmp(sProp->name, "FOCUS_POWER"))
    {
        bool sleep = false;

        // Sleep
        if (!strcmp(target_active_switch->name, "FOCUS_SLEEP"))
        {
               dispatch_command(FSLEEP);
               sleep = true;
        }
        // Wake Up
        else
            dispatch_command(FWAKUP);

        if (read_tcfs() == false)
        {
              IUResetSwitch(sProp);
              sProp->s = IPS_ALERT;
              IDSetSwitch(sProp, "Error reading TCF-S reply.");
              return true;
        }

            if (sleep)
            {
                if (isSimulation())
                    strncpy(response, "ZZZ", TCFS_MAX_CMD);

                if (!strcmp(response, "ZZZ"))
                {
                    sProp->s = IPS_OK;
                    IDSetSwitch(sProp, "Focuser is set into sleep mode.");
                    FocusPositionNP->s = IPS_IDLE;
                    IDSetNumber(FocusPositionNP, NULL);
                    FocusTemperatureNP->s = IPS_IDLE;
                    IDSetNumber(FocusTemperatureNP, NULL);
                    return true;
                }
                else
                {
                    sProp->s = IPS_ALERT;
                    IDSetSwitch(sProp, "Focuser sleep mode operation failed. Response: %s.", response);
                    return true;
                }
            }
            else
            {
                if (isSimulation())
                    strncpy(response, "WAKE", TCFS_MAX_CMD);

                if (!strcmp(response, "WAKE"))
                {
                    sProp->s = IPS_OK;
                    IDSetSwitch(sProp, "Focuser is awake.");
                    FocusPositionNP->s = IPS_OK;
                    IDSetNumber(FocusPositionNP, NULL);
                    FocusTemperatureNP->s = IPS_OK;
                    IDSetNumber(FocusTemperatureNP, NULL);
                    return true;
                }
                else
                {
                    sProp->s = IPS_ALERT;
                    IDSetSwitch(sProp, "Focuser wake up operation failed. Response: %s", response);
                    return true;
                }
            }
      }

    if (isFocusSleep)
    {
        sProp->s = IPS_IDLE;
        IUResetSwitch(sProp);

        if (!strcmp(sProp->name, "FOCUS_MODE") && current_active_switch != NULL)
            current_active_switch->s = ISS_ON;

        IDSetSwitch(sProp, "Focuser is still in sleep mode. Wake up in order to issue commands.");
        return true;
    }

    if (!strcmp(sProp->name, "FOCUS_MODE"))
    {

        sProp->s = IPS_OK;

        if (!strcmp(target_active_switch->name, "Manual"))
        {
           dispatch_command(FMMODE);
           read_tcfs();
           if (isSimulation() == false && strcmp(response, "!"))
           {
               IUResetSwitch(sProp);
               sProp->s = IPS_ALERT;
               IDSetSwitch(sProp, "Error switching to manual mode. No reply from TCF-S. Try again.");
               return true;
           }
       }
        else if (!strcmp(target_active_switch->name, "Auto A"))
        {
            dispatch_command(FAMODE);
            read_tcfs();
            if (isSimulation() == false && strcmp(response, "A"))
            {
                IUResetSwitch(sProp);
                sProp->s = IPS_ALERT;
                IDSetSwitch(sProp, "Error switching to Auto Mode A. No reply from TCF-S. Try again.");
                return true;
            }
        }
        else
        {
            dispatch_command(FBMODE);
            read_tcfs();
            if (isSimulation() == false && strcmp(response, "B"))
            {
                IUResetSwitch(sProp);
                sProp->s = IPS_ALERT;
                IDSetSwitch(sProp, "Error switching to Auto Mode B. No reply from TCF-S. Try again.");
                return true;
            }
        }

        IDSetSwitch(sProp, NULL);
        return true;
    }

    if (!strcmp(sProp->name, "FOCUS_MOTION"))
    {
        // Inward
        if (!strcmp(target_active_switch->name, "FOCUS_INWARD"))
          move_focuser(TCFS_INWARD);
        // Outward
        else
            move_focuser(TCFS_OUTWARD);

        return true;

     }

    if (!strcmp(sProp->name, "FOCUS_GOTO"))
    {
        int currentStep = nFocusSteps;

        FocusPositionNP->s = IPS_BUSY;
        sProp->s = IPS_OK;

        // Min
        if (!strcmp(target_active_switch->name, "FOCUS_MIN"))
        {
            nFocusSteps = nFocusCurrentPosition;
            move_focuser(TCFS_INWARD);
            nFocusSteps = currentStep;
            IUResetSwitch(sProp);
            IDSetSwitch(sProp, "Moving focouser to minimum position...");
        }
        // Center
        else if (!strcmp(target_active_switch->name, "FOCUS_CENTER"))
        {
            dispatch_command(FCENTR);
            read_tcfs();

            if (isSimulation())
                strncpy(response, "CENTER", TCFS_MAX_CMD);

            if (!strcmp(response, "CENTER"))
            {
                IUResetSwitch(sProp);
                sProp->s = IPS_OK;
                FocusPositionNP->s = IPS_BUSY;
                if (isTCFS3)
                    nFocusTargetPosition = 5000;
                else
                    nFocusTargetPosition = 3500;

                IDSetSwitch(sProp, "Moving focuser to center position %g...", nFocusTargetPosition);
                return true;
            }
            else
            {
                IUResetSwitch(sProp);
                sProp->s = IPS_ALERT;
                IDSetSwitch(sProp, "Failed to move focuser to center position!");
                return true;
            }
        }
        // Max
        else if (!strcmp(target_active_switch->name, "FOCUS_MAX"))
        {
            nFocusSteps = FocusPositionRequestNP->np[0].max - nFocusCurrentPosition;
            move_focuser(TCFS_OUTWARD);
            nFocusSteps = currentStep;
            IUResetSwitch(sProp);
            IDSetSwitch(sProp, "Moving focouser to maximum position %g...", FocusPositionRequestNP->np[0].max);
        }
        // Home
        else if (!strcmp(target_active_switch->name, "FOCUS_HOME"))
        {
            dispatch_command(FHOME);
            read_tcfs();

            if (isSimulation())
                strncpy(response, "DONE", TCFS_MAX_CMD);

            if (!strcmp(response, "DONE"))
            {
                IUResetSwitch(sProp);
                sProp->s = IPS_OK;
                //FocusInfoNP->s = IPS_BUSY;
                IDSetSwitch(sProp, "Moving focuser to new calculated position based on temperature...");
                return true;
            }
            else
            {
                IUResetSwitch(sProp);
                sProp->s = IPS_ALERT;
                IDSetSwitch(sProp, "Failed to move focuser to home position!");
                return true;
            }
        }

        return true;
    }



    return false;
}

bool TCFS::move_focuser(TCFSMotion dir)
{
    ISwitchVectorProperty *sProp = getSwitch("FOCUS_MOTION");

    if (sProp == NULL)
        return false;

    // Inward
    if (dir == TCFS_INWARD)
    {
        dispatch_command(FIN);
        nFocusTargetPosition = nFocusCurrentPosition - nFocusSteps;
    }
    // Outward
    else
    {
        dispatch_command(FOUT);
        nFocusTargetPosition = nFocusCurrentPosition + nFocusSteps;
    }

     if (read_tcfs() == false)
     {
          IUResetSwitch(sProp);
          sProp->s = IPS_ALERT;
          IDSetSwitch(sProp, "Error reading TCF-S reply.");
          return false;
     }

        if (isSimulation())
            strncpy(response, "*", TCFS_MAX_CMD);

        if (!strcmp(response, "*"))
        {
            IUResetSwitch(sProp);
            sProp->s = IPS_OK;
            FocusPositionNP->s = IPS_BUSY;
            IDSetSwitch(sProp, "Moving focuser %s %d steps to position %g.", (dir == TCFS_INWARD) ? "inward" : "outward", ((int) nFocusSteps), nFocusTargetPosition);
            return true;
        }
        else
        {
            IUResetSwitch(sProp);
            sProp->s = IPS_ALERT;
            IDSetSwitch(sProp, "Failed to move focuser %s!", (dir == TCFS_INWARD) ? "inward" : "outward");
            return true;
        }
}

bool TCFS::dispatch_command(TCFSCommand command_type)
{
   int err_code = 0, nbytes_written=0, nbytes_read=0;
   char tcfs_error[TCFS_ERROR_BUFFER];
   INumberVectorProperty *nProp = NULL;
   ISwitchVectorProperty *sProp = NULL;

   // Clear string
   command[0] = '\0';

   switch (command_type)
   {
        // Focuser Manual Mode
        case FMMODE:
                strncpy(command, "FMMODE", TCFS_MAX_CMD);
		break;

        // Focuser Free Mode
        case FFMODE:
                strncpy(command, "FFMODE", TCFS_MAX_CMD);
                break;
        // Focuser Auto-A Mode
        case FAMODE:
                strncpy(command, "FAMODE", TCFS_MAX_CMD);
                break;

       // Focuser Auto-A Mode
       case FBMODE:
                 strncpy(command, "FBMODE", TCFS_MAX_CMD);
                 break;

       // Focus Center
       case FCENTR:
                 strncpy(command, "FCENTR", TCFS_MAX_CMD);
                 break;

       // Focuser In “nnnn”
       case FIN:
                 simulated_position = nFocusCurrentPosition;

                    /* if ( (nFocusTargetPosition - nFocusSteps) >= FocusInfoNP->np[0].min)
                            nFocusTargetPosition -= nFocusSteps;
                     else
                         return false;*/

                 snprintf(command, TCFS_MAX_CMD, "FI%04d", ((int) nFocusSteps));
                 break;

      // Focuser Out “nnnn”
      case FOUT:
                 simulated_position = nFocusCurrentPosition;

                 /*if ( (nFocusTargetPosition + nFocusSteps) <= FocusInfoNP->np[0].max)
                    nFocusTargetPosition += nFocusSteps;
                 else
                     return false;*/

                 snprintf(command, TCFS_MAX_CMD, "FO%04d", ((int) nFocusSteps));
                 break;

       // Focuser Position Read Out
       case FPOSRO:
                 strncpy(command, "FPOSRO", TCFS_MAX_CMD);
                 break;

       // Focuser Position Read Out
       case FTMPRO:
                strncpy(command, "FTMPRO", TCFS_MAX_CMD);
                break;

       // Focuser Sleep
       case FSLEEP:
                strncpy(command, "FSLEEP", TCFS_MAX_CMD);
                break;
       // Focuser Wake Up
       case FWAKUP:
               strncpy(command, "FWAKUP", TCFS_MAX_CMD);
               break;
       // Focuser Home Command
       case FHOME:
               strncpy(command, "FHOME", TCFS_MAX_CMD);
               break;
   }
		
   if (isDebug())
        IDLog("Dispatching command #%s#\n", command);

   if (isSimulation())
       return true;

  tcflush(fd, TCIOFLUSH);

   if  ( (err_code = tty_write(fd, command, TCFS_MAX_CMD, &nbytes_written) != TTY_OK))
   {
        tty_error_msg(err_code, tcfs_error, TCFS_ERROR_BUFFER);
        if (isDebug())
            IDLog("TTY error detected: %s\n", tcfs_error);
   	return false;
   }

   return true;
}



void TCFS::ISPoll()
{
   if (!isConnected())
       return;

   int f_position=0;
   float f_temperature=0;

   if (FocusPositionNP->s != IPS_IDLE)
   {
       // Read Position
       // Manual Mode
       if (FocusModeSP->sp[0].s == ISS_ON)
             dispatch_command(FPOSRO);

       if (read_tcfs() == false)
                   return;

       if (isSimulation())
       {
            if (FocusPositionNP->s == IPS_BUSY)
            {
               if ( static_cast<int>(nFocusTargetPosition - simulated_position) > 0)
                    simulated_position += FocusStepNP->np[0].step;
               else if ( static_cast<int>(nFocusTargetPosition - simulated_position) < 0)
                    simulated_position -= FocusStepNP->np[0].step;
           }

          snprintf(response, TCFS_MAX_CMD, "P=%04d", simulated_position);
          if (isDebug())
               IDLog("Target Position: %g -- Simulated position: #%s#\n", nFocusTargetPosition, response);
       }

        sscanf(response, "P=%d", &f_position);

        nFocusCurrentPosition = f_position;

        if (nFocusCurrentPosition == nFocusTargetPosition)
        {
             FocusPositionNP->s = IPS_OK;
             FocusPositionRequestNP->s = IPS_OK;
             IDSetNumber(FocusPositionRequestNP, NULL);
         }

        IDSetNumber(FocusPositionNP, NULL);
     }


   if (FocusTemperatureNP->s != IPS_IDLE)
   {
       // Read Temperature
       // Manual Mode
       if (FocusModeSP->sp[0].s == ISS_ON)
            dispatch_command(FTMPRO);

       if (read_tcfs() == false)
           return;

       if (isSimulation())
       {
           snprintf(response, TCFS_MAX_CMD, "T=%0.1f", simulated_temperature);
           if (isDebug())
               IDLog("Simulated temperature: #%s#\n", response);
       }

       sscanf(response, "T=%f", &f_temperature);

       nFocusTemperature = f_temperature;      
       IDSetNumber(FocusTemperatureNP, NULL);

   }

}

bool TCFS::read_tcfs()
{
    int err_code = 0, nbytes_written=0, nbytes_read=0;
    char err_msg[TCFS_ERROR_BUFFER];

    // Clear string
    response[0] = '\0';

    if (isSimulation())
    {
         strncpy(response, "SIMULATION", TCFS_MAX_CMD);
         return true;
    }

    // Read until encountring a CR
    if ( (err_code = tty_read_section(fd, response, 0x0D, 15, &nbytes_read)) != TTY_OK)
    {
            tty_error_msg(err_code, err_msg, 32);
            if (isDebug())
            {
                IDLog("TTY error detected: %s\n", err_msg);
                IDMessage(mydev, "TTY error detected: %s\n", err_msg);
            }
            return false;
    }

    // Remove LF & CR
    response[nbytes_read-2] = '\0';

    if (isDebug())
        IDLog("Bytes Read: %d - strlen(response): %d - Reponse from TCF-S: #%s#\n", nbytes_read, strlen(response), response);

    return true;
}

const char * TCFS::getDefaultName()
{
    return "TCFS";
}
