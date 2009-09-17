/*
    Kuwait National Radio Observatory
    INDI Driver for SpectraCyber Hydrogen Line Spectrometer
    Communication: RS232 <---> USB

    Copyright (C) 2009 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

    Change Log:

*/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <termios.h>

#include <indicom.h>

#include "spectrometer.h"

const int SPECTROMETER_READ_BUFFER = 16;
const int SPECTROMETER_ERROR_BUFFER = 128;
const int SPECTROMETER_CMD_LEN = 4;

/****************************************************************
**
**
*****************************************************************/
knroSpectrometer::knroSpectrometer()
{

  connection_status = -1;
  
  simulation = false;
  
  init_properties();



}

/****************************************************************
**
**
*****************************************************************/
knroSpectrometer::~knroSpectrometer()
{




}

/****************************************************************
**
**
*****************************************************************/
void knroSpectrometer::init_properties()
{

   IUFillText(&PortT[0], "PORT", "Port", default_port.c_str());
   IUFillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "SPECTROMETER_PORT", "Spectrometer", SPECTROMETER_GROUP, IP_RW, 0, IPS_IDLE);
  
}

/****************************************************************
**
**
*****************************************************************/
void knroSpectrometer::ISGetProperties()
{
   IDDefText(&PortTP, NULL);
}

/****************************************************************
**
**
*****************************************************************/
void knroSpectrometer::reset_all_properties(bool reset_to_idle)
{
	if (reset_to_idle)
	{
		PortTP.s		= IPS_IDLE;
	}
	
	IDSetText(&PortTP, NULL);
}

/****************************************************************
**
**
*****************************************************************/   
bool knroSpectrometer::connect()
{
	
   if (check_spectrometer_connection())
		return true;

    if (simulation)
    {
    	IDMessage(mydev, "%s Spectrometer: Simulating connecting to port %s.", type_name.c_str(), PortT[0].text);
        connection_status = 0;
	return true;
    }


    if (tty_connect(PortT[0].text, 2400, 8, 0, 1, &fd) != TTY_OK)
    {
	PortTP.s = IPS_ALERT;
	IDSetText (&PortTP, "Error connecting to port %s. Make sure you have BOTH read and write permission to the port.", PortT[0].text);
	return false;
    }

    connection_status = 0;
    PortTP.s = IPS_OK;
    IDSetText (&PortTP, "Spectrometer is online. Retrieving preliminary data...");

    return init_spectrometer();
	
}

/****************************************************************
**
**
*****************************************************************/   
bool knroSpectrometer::init_spectrometer()
{

   if (!check_spectrometer_connection())
	return false;
		   
   // Enable speed mode
   if (simulation)
   {
		IDMessage(mydev, "%s Spectrometer: Simulating encoder init.", type_name.c_str());
   }
   	
   return true;
	
}

/****************************************************************
**
**
*****************************************************************/   
void knroSpectrometer::disconnect()
{
	connection_status = -1;
	tty_disconnect(fd);
}

/****************************************************************
**
**
*****************************************************************/
void knroSpectrometer::enable_simulation ()
{
	if (simulation)
		return;
		
	 simulation = true;
	 IDMessage(mydev, "Notice: spectrometer simulation is enabled.");
	 IDLog("Notice: spectrometer simulation is enabled.\n");
}

/****************************************************************
**
**
*****************************************************************/
void knroSpectrometer::disable_simulation()
{
	if (!simulation) 
		return;
		
	 // Disconnect
	 disconnect();
	 
	 simulation = false;
	  
	 IDMessage(mydev, "Caution: spectrometer simulation is disabled.");
	 IDLog("Caution: spectrometer simulation is disabled.\n");
}
    
/****************************************************************
**
**
*****************************************************************/
bool knroSpectrometer::check_spectrometer_connection()
{
	if (simulation)
		return true;
	
	if (connection_status == -1)
		return false;
		
	return true;
}

/****************************************************************
**
**
*****************************************************************/
void knroSpectrometer::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	
	
}

/****************************************************************
**
**
*****************************************************************/
void knroSpectrometer::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// Device Port Text
	if (!strcmp(PortTP.name, name))
	{
		if (IUUpdateText(&PortTP, texts, names, n) < 0)
			return;

		PortTP.s = IPS_OK;
		IDSetText(&PortTP, "Please reconnect when ready.");

		return;
	}
	
}

/****************************************************************
**
**
*****************************************************************/
void knroSpectrometer::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
}

bool knroSpectrometer::dispatch_command(SpectrometerCommand command)
{
/*   char encoder_error[SPECTROMETER_ERROR_BUFFER];
   encoder_command[1] = command;
   int err_code = 0, nbytes_written =0;

   tcflush(fd, TCIOFLUSH);

   if  ( (err_code = tty_write(fd, encoder_command, SPECTROMETER_CMD_LEN, &nbytes_written) != TTY_OK))
   {
	tty_error_msg(err_code, encoder_error, SPECTROMETER_ERROR_BUFFER);
	IDLog("TTY error detected: %s\n", encoder_error);
   	return false;
   }

*/
   return true;
}

