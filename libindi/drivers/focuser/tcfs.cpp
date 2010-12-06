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

#define mydev		"Optec TCF-S"
#define nFocusSteps         FocusStepNP->np[0].value
#define nFocusPosition      FocusInfoNP->np[0].value
#define nFocusTemperature   FocusInfoNP->np[1].value


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
    FocusInfoNP = getNumber("FOCUS_INFO");

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
        fd=-1;
        IDSetSwitch(ConnectSP, "TCF-S: Simulating connection to port %s.", tProp->tp[0].text);
        ISwitchVectorProperty *pSwitch = getSwitch("FOCUS_MODE");
        if (pSwitch)
        {
            pSwitch->sp[0].s = ISS_ON;
            pSwitch->s       = IPS_OK;
            IDSetSwitch(pSwitch, NULL);
        }
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
                ISwitchVectorProperty *pSwitch = getSwitch("FOCUS_MODE");
                if (pSwitch)
                {
                    pSwitch->sp[0].s = ISS_ON;
                    pSwitch->s       = IPS_OK;
                    IDSetSwitch(pSwitch, NULL);
                }

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

    if (!strcmp(sProp->name, "FOCUS_MODE"))
    {
        IUUpdateSwitch(sProp, states, names, n);

        sProp->s = IPS_OK;

        if (!strcmp(names[0], "Manual"))
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
        else if (!strcmp(names[0], "Auto A"))
            dispatch_command(FAMODE);
        else
            dispatch_command(FBMODE);

        IDSetSwitch(sProp, NULL);
        return true;
    }

    if (!strcmp(sProp->name, "FOCUS_MOTION"))
    {
        IUUpdateSwitch(sProp, states, names, n);
        bool inward=false;


        // Inward
        if (sProp->sp[0].s == ISS_ON)
        {
            inward = true;
            dispatch_command(FIN);
        }
        // Outward
        else
            dispatch_command(FOUT);

            if (read_tcfs() == false)
            {
                IUResetSwitch(sProp);
                sProp->s = IPS_ALERT;
                IDSetSwitch(sProp, "Error reading TCF-S reply.");
                return true;
            }

            if (isSimulation())
                strncpy(response, "*", TCFS_MAX_CMD);

            if (!strcmp(response, "*"))
            {
                IUResetSwitch(sProp);
                sProp->s = IPS_OK;
                FocusInfoNP->s = IPS_BUSY;
                IDSetSwitch(sProp, "Moving focuser %s %d steps.", inward ? "inward" : "outward", ((int) nFocusSteps) );
                return true;
            }
            else
            {
                IUResetSwitch(sProp);
                sProp->s = IPS_ALERT;
                IDSetSwitch(sProp, "Failed to move focuser %s!", inward? "inward" : "outward");
                return true;
            }
        }

    if (!strcmp(sProp->name, "FOCUS_POSITION"))
    {
        int currentStep = nFocusSteps;
        IUUpdateSwitch(sProp, states, names, n);

        FocusInfoNP->s = IPS_BUSY;
        sProp->s = IPS_OK;

        // Min
        if (sProp->sp[0].s == ISS_ON)
        {
            nFocusSteps = nFocusPosition;
            dispatch_command(FIN);
            read_tcfs();
            nFocusSteps = currentStep;
            IUResetSwitch(sProp);
            IDSetSwitch(sProp, "Moving focouser to minimum position...");
        }
        // Center
        else if (sProp->sp[1].s == ISS_ON)
        {
            dispatch_command(FCENTR);
            read_tcfs();

            if (isSimulation())
                strncpy(response, "CENTER", TCFS_MAX_CMD);

            if (!strcmp(response, "CENTER"))
            {
                IUResetSwitch(sProp);
                sProp->s = IPS_OK;
                FocusInfoNP->s = IPS_BUSY;
                IDSetSwitch(sProp, "Moving focuser to center position...");
                //target_position = (unsigned int) FocusInfoNP->np[0].max / 2.;
                // Temprorary for now
                target_position = 3500;
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
        else
        {
            nFocusSteps = FocusInfoNP->np[0].max - nFocusPosition;
            dispatch_command(FOUT);
            read_tcfs();
            nFocusSteps = currentStep;
            IUResetSwitch(sProp);
            IDSetSwitch(sProp, "Moving focouser to maximum position...");
        }


        return true;
    }

    if (!strcmp(sProp->name, "FOCUS_POWER"))
    {
        IUUpdateSwitch(sProp, states, names, n);
        bool sleep = false;

        // Sleep
        if (sProp->sp[0].s == ISS_ON)
        {
               dispatch_command(FSLEEP);
               sleep = true;
               if (isSimulation())
                   strncpy(response, "ZZZ", TCFS_MAX_CMD);
        }
        else
        {
            dispatch_command(FWAKUP);
            if (isSimulation())
                strncpy(response, "WAKE", TCFS_MAX_CMD);
        }


            if (read_tcfs() == false)
            {
                IUResetSwitch(sProp);
                sProp->s = IPS_ALERT;
                IDSetSwitch(sProp, "Error reading TCF-S reply.");
                return true;
            }


            IUResetSwitch(sProp);

            if (sleep)
            {
                if (!strcmp(response, "ZZZ"))
                {
                    sProp->s = IPS_OK;
                    IDSetSwitch(sProp, "Focuser is set into sleep mode.");
                    return true;
                }
                else
                {
                    sProp->s = IPS_ALERT;
                    IDSetSwitch(sProp, "Focuser sleep mode operation failed.");
                    return true;
                }
            }
            else
            {
                if (!strcmp(response, "WAKE"))
                {
                    sProp->s = IPS_OK;
                    IDSetSwitch(sProp, "Focuser is awake.");
                    return true;
                }
                else
                {
                    sProp->s = IPS_ALERT;
                    IDSetSwitch(sProp, "Focuser wake up operation failed.");
                    return true;
                }
            }
      }

    return false;
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
                 target_position = simulated_position = nFocusPosition;

                     if ( (target_position - FocusStepNP->np[0].value) >= FocusInfoNP->np[0].min)
                            target_position -= FocusStepNP->np[0].value;
                     else
                         return false;

                 snprintf(command, TCFS_MAX_CMD, "FI%04d", ((int) nFocusSteps));
                 break;

      // Focuser Out “nnnn”
      case FOUT:
                 target_position = simulated_position = nFocusPosition;

                 if ( (target_position + FocusStepNP->np[0].value) <= FocusInfoNP->np[0].max)
                    target_position += FocusStepNP->np[0].value;
                 else
                     return false;

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
   ISwitchVectorProperty *pSwitch = getSwitch("FOCUS_MODE");

   if (pSwitch == NULL)
       return;

   // Manual Mode
   if (pSwitch->sp[0].s == ISS_ON)
   {
       // Read Position
       dispatch_command(FPOSRO);
       if (read_tcfs() == false)
           return;

       if (isSimulation())
       {
           if (FocusInfoNP->s == IPS_BUSY)
           {
               if ( static_cast<int>(target_position - simulated_position) > 0)
                  simulated_position += FocusStepNP->np[0].step;
                else if ( static_cast<int>(target_position - simulated_position) < 0)
               simulated_position -= FocusStepNP->np[0].step;
           }

           snprintf(response, TCFS_MAX_CMD, "P=%04d", simulated_position);
           if (isDebug())
               IDLog("Target Position: %d -- Simulated position: #%s#\n", target_position, response);
       }

       sscanf(response, "P=%d", &f_position);

       nFocusPosition = f_position;

       // Read Temperature
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

       if (nFocusPosition == target_position)
            FocusInfoNP->s = IPS_OK;
       IDSetNumber(FocusInfoNP, NULL);

   }
   else
   // Auto-A or Auto-B
   {
       // Read Position
       if (read_tcfs() == false)
           return;

       if (isSimulation())
       {
           if (FocusInfoNP->s == IPS_BUSY)
           {
            if ( static_cast<int>(target_position - simulated_position) > 0)
                   simulated_position += FocusStepNP->np[0].step;
            else if ( static_cast<int>(target_position - simulated_position) < 0)
                   simulated_position -= FocusStepNP->np[0].step;
           }

           snprintf(response, TCFS_MAX_CMD, "P=%04d", simulated_position);
           if (isDebug())
               IDLog("Target Position: %d -- Simulated position: #%s#\n", target_position, response);
       }

       sscanf(response, "P=%d", &f_position);

       nFocusPosition = f_position;

       // Read Temperature
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

       if (nFocusPosition == target_position)
            FocusInfoNP->s = IPS_OK;
       IDSetNumber(FocusInfoNP, NULL);

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
    if ( (err_code = tty_read_section(fd, response, 0x0D, 5, &nbytes_read)) != TTY_OK)
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
