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

        usleep(500000);

        if (read_tcfs() == false)
            continue;
        else
        {
            if (!strcmp(response, "!"))
            {
                setConnected(true, IPS_OK, "Successfully connected to TCF-S Focuser in Manual Mode.");
                return true;
            }
        }
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

    if (isDebug())
        IDLog("Bytes Read: %d - strlen(response): %d - Reponse from TCF-S: #%s#\n", nbytes_read, strlen(response), response);

    return true;
}
