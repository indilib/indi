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
#define currentPosition         FocusAbsPosN[0].value
#define isFocusSleep            (FocusPowerSP->sp[0].s == ISS_ON)
#define inAutoMode              (FocusModeSP->sp[0].s != ISS_ON)

const int POLLMS = 500;

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
    tcfs->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
	if(dev && strcmp (mydev, dev)) return;
	ISInit();
    tcfs->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
	if(dev && strcmp (mydev, dev)) return;
	ISInit();
    tcfs->ISNewNumber(dev, name, values, names, num);
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
    tcfs->ISSnoopDevice(root);
}

/****************************************************************
**
**
*****************************************************************/
TCFS::TCFS()
{   
    SetFocuserCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE);

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
bool TCFS::initProperties()
{
    INDI::Focuser::initProperties();

    // Set upper limit for TCF-S3 focuser
    if (!strcmp(me, "indi_tcfs3_focus"))
    {
        isTCFS3 = true;

        FocusAbsPosN[0].max = 9999;
        FocusRelPosN[0].max = 2000;
        FocusRelPosN[0].step = FocusAbsPosN[0].step = 100;
        FocusRelPosN[0].value = 0;
        DEBUG(INDI::Logger::DBG_DEBUG, "TCF-S3 detected. Updating maximum position value to 9999.");
    }
    else
    {
        isTCFS3 = false;

        FocusAbsPosN[0].max = 7000;
        FocusRelPosN[0].max = 2000;
        FocusRelPosN[0].step = FocusAbsPosN[0].step = 100;
        FocusRelPosN[0].value = 0;
        DEBUG(INDI::Logger::DBG_DEBUG, "TCF-S detected. Updating maximum position value to 7000.");
    }
    return true;
}

/****************************************************************
**
**
*****************************************************************/
void TCFS::ISGetProperties(const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);

    // Optional: Add aux controls for configuration, debug & simulation
    addAuxControls();
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
         buildSkeleton("indi_tcfs_sk.xml");

         FocusTemperatureNP = getNumber("FOCUS_TEMPERATURE");
         FocusPowerSP = getSwitch("FOCUS_POWER");
         FocusModeSP  = getSwitch("FOCUS_MODE");
         FocusGotoSP  = getSwitch("FOCUS_GOTO");

         FocusAbsPosNP.s = IPS_OK;
         FocusTemperatureNP->s = IPS_OK;
         FocusModeSP->sp[0].s = ISS_ON;
         defineSwitch(FocusGotoSP);
         defineNumber(FocusTemperatureNP);
         defineSwitch(FocusPowerSP);
         defineSwitch(FocusModeSP);

         loadConfig(true);

         SetTimer(POLLMS);
         return true;
    }
    else
    {
        deleteProperty(FocusGotoSP->name);
        deleteProperty(FocusTemperatureNP->name);
        deleteProperty(FocusPowerSP->name);
        deleteProperty(FocusModeSP->name);
        return false;
    }
    return true;
}

/****************************************************************
**
**
*****************************************************************/   
bool TCFS::Connect()
{
   if (isConnected())
		return true;

    if (isSimulation())
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "TCF-S: Simulating connection to port %s.", PortT[0].text);

        currentPosition = simulated_position;

        fd=-1;
        return true;
    }

    DEBUG(INDI::Logger::DBG_DEBUG, "Attempting to connect to TCF-S focuser....");

    if (tty_connect(PortT[0].text, 19200, 8, 0, 1, &fd) != TTY_OK)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Error connecting to port %s. Make sure you have BOTH read and write permission to the port.", PortT[0].text);
        return false;
    }

    dispatch_command(FWAKUP);
    read_tcfs();

    dispatch_command(FMMODE);
    read_tcfs();

    tcflush(fd, TCIOFLUSH);

    DEBUG(INDI::Logger::DBG_SESSION, "Successfully connected to TCF-S Focuser in Manual Mode.");

    return true;
}

/****************************************************************
**
**
*****************************************************************/   
bool TCFS::Disconnect()
{
    FocusTemperatureNP->s = IPS_IDLE;
    IDSetNumber(FocusTemperatureNP, NULL);

    dispatch_command(FFMODE);

    tty_disconnect(fd);

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    ISwitch *current_active_switch = NULL, *target_active_switch = NULL;
    // First process parent!
    if (INDI::DefaultDevice::ISNewSwitch(getDeviceName(), name, states, names, n) == true)
        return true;

    ISwitchVectorProperty *sProp = getSwitch(name);

    if (sProp == NULL)
        return false;

    // Which switch is CURRENTLY on?
    current_active_switch = IUFindOnSwitch(sProp);

    IUUpdateSwitch(sProp, states, names, n);

    // Which switch the CLIENT wants to turn on?
    target_active_switch = IUFindOnSwitch(sProp);

    if (target_active_switch == NULL)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error: no ON switch found in %s property.", sProp->name);
        return false;
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
                    FocusAbsPosNP.s = IPS_IDLE;
                    IDSetNumber(&FocusAbsPosNP, NULL);
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
                    FocusAbsPosNP.s = IPS_OK;
                    IDSetNumber(&FocusAbsPosNP, NULL);
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

    if (!strcmp(sProp->name, "FOCUS_GOTO"))
    {
        if (inAutoMode)
        {
            sProp->s = IPS_IDLE;
            IDSetSwitch(sProp, NULL);
            DEBUG(INDI::Logger::DBG_WARNING, "The focuser can only be moved in Manual mode.");
            return false;
        }

        sProp->s = IPS_BUSY;

        // Min
        if (!strcmp(target_active_switch->name, "FOCUS_MIN"))
        {
            targetTicks = currentPosition;
            MoveRelFocuser(FOCUS_INWARD, currentPosition);
            IDSetSwitch(sProp, "Moving focuser to minimum position...");
        }
        // Center
        else if (!strcmp(target_active_switch->name, "FOCUS_CENTER"))
        {
            dispatch_command(FCENTR);
            FocusAbsPosNP.s = FocusRelPosNP.s = IPS_BUSY;
            IDSetNumber(&FocusAbsPosNP, NULL);
            IDSetNumber(&FocusRelPosNP, NULL);
            IDSetSwitch(sProp, "Moving focuser to center position %d...", isTCFS3 ? 5000 : 3500);
            return true;
        }
        // Max
        else if (!strcmp(target_active_switch->name, "FOCUS_MAX"))
        {
            unsigned int delta = 0;
            delta = FocusAbsPosN[0].max - currentPosition;
            MoveRelFocuser(FOCUS_OUTWARD, delta);
            IDSetSwitch(sProp, "Moving focuser to maximum position %g...", FocusAbsPosN[0].max);
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

        IDSetSwitch(sProp, NULL);
        return true;
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

IPState TCFS::MoveAbsFocuser(uint32_t ticks)
{
    int delta=0;

    delta = ticks - currentPosition;

    if (delta < 0)
        return MoveRelFocuser(FOCUS_INWARD, (uint32_t) fabs(delta));
    else
        return MoveRelFocuser(FOCUS_OUTWARD, (uint32_t) fabs(delta));

}

IPState TCFS::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{

    if (inAutoMode)
    {
        DEBUG(INDI::Logger::DBG_WARNING, "The focuser can only be moved in Manual mode.");
        return IPS_ALERT;
    }

    targetTicks = ticks;
    targetPosition = currentPosition;

    // Inward
    if (dir == FOCUS_INWARD)
    {
        targetPosition -= targetTicks;
        dispatch_command(FIN);
    }
    // Outward
    else
    {
        targetPosition += targetTicks;
        dispatch_command(FOUT);
    }

    FocusAbsPosNP.s = IPS_BUSY;
    FocusRelPosNP.s = IPS_BUSY;
    IDSetNumber(&FocusAbsPosNP, NULL);
    IDSetNumber(&FocusRelPosNP, NULL);

    simulated_position = targetPosition;

    return IPS_BUSY;

}

bool TCFS::dispatch_command(TCFSCommand command_type)
{
   int err_code = 0, nbytes_written=0;
   char tcfs_error[TCFS_ERROR_BUFFER];

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
                 simulated_position = currentPosition;

                 snprintf(command, TCFS_MAX_CMD, "FI%04d", targetTicks);
                 break;

      // Focuser Out “nnnn”
      case FOUT:
                 simulated_position = currentPosition;

                 snprintf(command, TCFS_MAX_CMD, "FO%04d", targetTicks);
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
		
   DEBUGF(INDI::Logger::DBG_DEBUG, "Dispatching command #%s#", command);

   currentCommand = command_type;

   if (isSimulation())
       return true;

  tcflush(fd, TCIOFLUSH);

   if  ( (err_code = tty_write(fd, command, TCFS_MAX_CMD, &nbytes_written) != TTY_OK))
   {
        tty_error_msg(err_code, tcfs_error, TCFS_ERROR_BUFFER);
        DEBUGF(INDI::Logger::DBG_ERROR, "TTY error detected: %s", tcfs_error);
        return false;
   }

   return true;
}



void TCFS::TimerHit()
{
   static double lastPosition=-1, lastTemperature=-1000;

   if (isConnected() == false)
   {
       SetTimer(POLLMS);
       return;
   }

   int f_position=0;
   float f_temperature=0;

   if (FocusGotoSP->s == IPS_BUSY)
   {
       ISwitch *sp = IUFindOnSwitch(FocusGotoSP);

       if (sp && !strcmp(sp->name, "FOCUS_CENTER"))
       {
           bool rc = read_tcfs(true);

           if (rc == false)
           {
               SetTimer(POLLMS);
               return;
           }

           if (isSimulation())
               strncpy(response, "CENTER", TCFS_MAX_CMD);

           if (!strcmp(response, "CENTER"))
           {
               IUResetSwitch(FocusGotoSP);
               FocusGotoSP->s = IPS_OK;
               FocusAbsPosNP.s = IPS_OK;

               IDSetSwitch(FocusGotoSP, NULL);
               IDSetNumber(&FocusAbsPosNP, NULL);

               DEBUG(INDI::Logger::DBG_SESSION, "Focuser moved to center position.");
           }
       }
   }

   switch (FocusAbsPosNP.s)
   {
       case IPS_OK:
       if (FocusModeSP->sp[0].s == ISS_ON)
        dispatch_command(FPOSRO);

       if (read_tcfs() == false)
       {
           SetTimer(POLLMS);
           return;
       }

       if (isSimulation())
          snprintf(response, TCFS_MAX_CMD, "P=%04d", (int) simulated_position);

       sscanf(response, "P=%d", &f_position);
       currentPosition = f_position;

       if (lastPosition != currentPosition)
       {
           lastPosition = currentPosition;
           IDSetNumber(&FocusAbsPosNP, NULL);
       }
       break;

       case IPS_BUSY:
       if (read_tcfs(true) == false)
       {
           SetTimer(POLLMS);
           return;
       }

       // Ignore error
       if (strstr(response, "ER"))
       {
           DEBUGF(INDI::Logger::DBG_DEBUG, "Received error: %s", response);
           SetTimer(POLLMS);
           return;
       }

       if (isSimulation())
           strncpy(response, "*", TCFS_MAX_CMD);

       if (!strcmp(response, "*"))
       {
           DEBUGF(INDI::Logger::DBG_DEBUG, "Moving focuser %d steps to position %d.", targetTicks, targetPosition);
           FocusAbsPosNP.s = IPS_OK;
           FocusRelPosNP.s = IPS_OK;
           FocusGotoSP->s  = IPS_OK;
           IDSetNumber(&FocusAbsPosNP, NULL);
           IDSetNumber(&FocusRelPosNP, NULL);
           IDSetSwitch(FocusGotoSP, NULL);
       }
       else
       {
           FocusAbsPosNP.s = IPS_ALERT;
           DEBUGF(INDI::Logger::DBG_ERROR, "Unable to read response from focuser #%s#.", response);
           IDSetNumber(&FocusAbsPosNP, NULL);
       }
       break;

   default:
       break;

   }

   if (FocusTemperatureNP->s != IPS_IDLE)
   {
       // Read Temperature
       // Manual Mode
       if (FocusModeSP->sp[0].s == ISS_ON)
            dispatch_command(FTMPRO);

       if (read_tcfs() == false)
       {
           SetTimer(POLLMS);
           return;
       }

       if (isSimulation())
           snprintf(response, TCFS_MAX_CMD, "T=%0.1f", simulated_temperature);

       sscanf(response, "T=%f", &f_temperature);

       FocusTemperatureNP->np[0].value  = f_temperature;

       if (lastTemperature != FocusTemperatureNP->np[0].value)
       {
           lastTemperature = FocusTemperatureNP->np[0].value;
           IDSetNumber(FocusTemperatureNP, NULL);
       }
   }

   SetTimer(POLLMS);

}

bool TCFS::read_tcfs(bool silent)
{
    int err_code = 0, nbytes_read=0;
    char err_msg[TCFS_ERROR_BUFFER];

    // Clear string
    response[0] = '\0';

    if (isSimulation())
    {
         strncpy(response, "SIMULATION", TCFS_MAX_CMD);
         return true;
    }

    // Read until encountring a CR
    if ( (err_code = tty_read_section(fd, response, 0x0D, 5, &nbytes_read)) != TTY_OK)
    {
        if (silent == false)
        {
            tty_error_msg(err_code, err_msg, 32);
            DEBUGF(INDI::Logger::DBG_ERROR, "TTY error detected: %s", err_msg);
        }

        return false;
    }

    // Remove LF & CR
    response[nbytes_read-2] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "Bytes Read: %d - strlen(response): %ld - Response from TCF-S: #%s#", nbytes_read, strlen(response), response);

    return true;
}

const char * TCFS::getDefaultName()
{
    return mydev;
}
