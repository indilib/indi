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
const int SPECTROMETER_CMD_LEN = 5;
const int SPECTROMETER_CMD_REPLY = 4;

/****************************************************************
**
**
*****************************************************************/
knroSpectrometer::knroSpectrometer()
{

  connection_status = -1;
  
  simulation = false;
  
  init_properties();

  // Command pre-limeter 
  command[0] = '!';

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

   default_port = string("/dev/ttyUSB0");
   IUFillText(&PortT[0], "PORT", "Port", default_port.c_str());
   IUFillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "SPECTROMETER_PORT", "Spectrometer", SPECTROMETER_GROUP, IP_RW, 0, IPS_IDLE);

  // Intermediate Frequency Gain (IF)
  IUFillNumber(&IFGainN[0], "Gain (dB)", "", "%g",  10., 25.75, 0.25, 10.0);
  IUFillNumberVector(&IFGainNP, IFGainN, NARRAY(IFGainN), mydev, "70 Mhz IF", "", SPECTROMETER_GROUP, IP_RW, 0, IPS_IDLE);

  // Continuum Gain
  IUFillSwitch(&ContGainS[0], "x1", "", ISS_ON);
  IUFillSwitch(&ContGainS[1], "x5", "", ISS_OFF);
  IUFillSwitch(&ContGainS[2], "x10", "", ISS_OFF);
  IUFillSwitch(&ContGainS[3], "x20", "", ISS_OFF);
  IUFillSwitch(&ContGainS[4], "x50", "", ISS_OFF);
  IUFillSwitch(&ContGainS[5], "x60", "", ISS_OFF);
  IUFillSwitchVector(&ContGainSP, ContGainS, NARRAY(ContGainS), mydev, "Continuum Gain", "", SPECTROMETER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Continuum Integration
  IUFillSwitch(&ContIntegrationS[0], "0.3", "", ISS_ON);
  IUFillSwitch(&ContIntegrationS[1], "1", "", ISS_OFF);
  IUFillSwitch(&ContIntegrationS[2], "10", "", ISS_OFF);
  IUFillSwitchVector(&ContIntegrationSP, ContIntegrationS, NARRAY(ContIntegrationS), mydev, "Continuum Integration (s)", "", SPECTROMETER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Spectral Gain
  IUFillSwitch(&SpecGainS[0], "x1", "", ISS_ON);
  IUFillSwitch(&SpecGainS[1], "x5", "", ISS_OFF);
  IUFillSwitch(&SpecGainS[2], "x10", "", ISS_OFF);
  IUFillSwitch(&SpecGainS[3], "x20", "", ISS_OFF);
  IUFillSwitch(&SpecGainS[4], "x50", "", ISS_OFF);
  IUFillSwitch(&SpecGainS[5], "x60", "", ISS_OFF);
  IUFillSwitchVector(&SpecGainSP, SpecGainS, NARRAY(SpecGainS), mydev, "Spectral Gain", "", SPECTROMETER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Spectral Integration
  IUFillSwitch(&SpecIntegrationS[0], "0.3", "", ISS_ON);
  IUFillSwitch(&SpecIntegrationS[1], "1", "", ISS_OFF);
  IUFillSwitch(&SpecIntegrationS[2], "10", "", ISS_OFF);
  IUFillSwitchVector(&SpecIntegrationSP, SpecIntegrationS, NARRAY(SpecIntegrationS), mydev, "Spectral Integration (s)", "", SPECTROMETER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // DC Offsets
  IUFillNumber(&DCOffsetN[CONTINUUM_CHANNEL], "Continuum (v)", "", "%g",  0., 4.096, 0.001, 0.);
  IUFillNumber(&DCOffsetN[SPECTRAL_CHANNEL], "Spectral (v)", "", "%g",  0., 4.096, 0.001, 0.);
  IUFillNumberVector(&DCOffsetNP, DCOffsetN, NARRAY(DCOffsetN), mydev, "DC Offset", "", SPECTROMETER_GROUP, IP_RW, 0, IPS_IDLE);

   // Bandwidth
  IUFillSwitch(&BandwidthS[0], "15", "", ISS_ON);
  IUFillSwitch(&BandwidthS[1], "30", "", ISS_OFF);
  IUFillSwitchVector(&BandwidthSP, BandwidthS, NARRAY(BandwidthS), mydev, "Bandwidth (Khz)", "", SPECTROMETER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Commands
  IUFillSwitch(&CommandS[0], "Read Continuum", "", ISS_OFF);
  IUFillSwitch(&CommandS[1], "Read Spectral", "", ISS_OFF);
  IUFillSwitchVector(&CommandSP, CommandS, NARRAY(CommandS), mydev, "Commands", "", SPECTROMETER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Channel Value
  IUFillNumber(&ChannelValueN[0], "Value", "", "%g",  0., 10, 0.1, 0.);
  IUFillNumberVector(&ChannelValueNP, ChannelValueN, NARRAY(ChannelValueN), mydev, "Read Out (v)", "", SPECTROMETER_GROUP, IP_RO, 0, IPS_IDLE);

  // Reset
  IUFillSwitch(&ResetS[0], "Reset", "", ISS_OFF);
  IUFillSwitchVector(&ResetSP, ResetS, NARRAY(ResetS), mydev, "Parameters", "", SPECTROMETER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  
}

/****************************************************************
**
**
*****************************************************************/
void knroSpectrometer::ISGetProperties()
{
   IDDefText(&PortTP, NULL);
   IDDefNumber(&ChannelValueNP, NULL);
   IDDefNumber(&IFGainNP, NULL);
   IDDefSwitch(&ContGainSP, NULL);
   IDDefSwitch(&ContIntegrationSP, NULL);
   IDDefSwitch(&SpecGainSP, NULL);
   IDDefSwitch(&SpecIntegrationSP, NULL);
   IDDefNumber(&DCOffsetNP, NULL);
   IDDefSwitch(&BandwidthSP, NULL);
   IDDefSwitch(&CommandSP, NULL);
   IDDefSwitch(&ResetSP, NULL);


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
		IFGainNP.s		= IPS_IDLE;
		ContGainSP.s		= IPS_IDLE;
		ContIntegrationSP.s     = IPS_IDLE;
		SpecGainSP.s		= IPS_IDLE;
		SpecIntegrationSP.s     = IPS_IDLE;
		DCOffsetNP.s		= IPS_IDLE;
		BandwidthSP.s 		= IPS_IDLE;
		CommandSP.s 		= IPS_IDLE;
		ChannelValueNP.s 	= IPS_IDLE;
		ResetSP.s		= IPS_IDLE;
	}
	
	IDSetText(&PortTP, NULL);
        IDSetNumber(&IFGainNP, NULL);
	IDSetSwitch(&ContGainSP, NULL);
	IDSetSwitch(&ContIntegrationSP, NULL);
	IDSetSwitch(&SpecGainSP, NULL);
	IDSetSwitch(&SpecIntegrationSP, NULL);
	IDSetNumber(&DCOffsetNP, NULL);
	IDSetSwitch(&BandwidthSP, NULL);
	IDSetSwitch(&CommandSP, NULL);
	IDSetNumber(&ChannelValueNP, NULL);
	IDSetSwitch(&ResetSP, NULL);
}

/****************************************************************
**
**
*****************************************************************/   
bool knroSpectrometer::connect()
{
	
   int err_code = 0, nbytes_written =0, nbytes_read=0;
   char response[4];
   char err_msg[SPECTROMETER_ERROR_BUFFER];

   if (check_spectrometer_connection())
		return true;

    if (simulation)
    {
    	IDMessage(mydev, "%s Spectrometer: Simulating connecting to port %s.", type_name.c_str(), PortT[0].text);
        connection_status = 0;
	return true;
    }


    IDLog("Attempting to connect to spectrometer....\n");

    if (tty_connect(PortT[0].text, 2400, 8, 0, 1, &fd) != TTY_OK)
    {
	PortTP.s = IPS_ALERT;
	IDSetText (&PortTP, "Error connecting to port %s. Make sure you have BOTH read and write permission to the port.", PortT[0].text);
	return false;
    }


   IDLog("Attempting to write to spectrometer....\n");

   dispatch_command(RESET);

   IDLog("Attempting to read from spectrometer....\n");

   // Read echo from spectrometer, we're expecting R000
   if ( (err_code = tty_read(fd, response, SPECTROMETER_CMD_REPLY, 5, &nbytes_read)) != TTY_OK)
   {
		tty_error_msg(err_code, err_msg, 32);
		IDLog("TTY error detected: %s\n", err_msg);
   		return false;
   }

   IDLog("Reponse from Spectrometer: #%c# #%c# #%c# #%c#\n", response[0], response[1], response[2], response[3]);

   if (strstr(response, "R000"))
   {
	IDLog("Echo test passed.\n");
	connection_status = 0;
        PortTP.s = IPS_OK;
        IDSetText (&PortTP, "Spectrometer is online. Retrieving preliminary data...");
        return init_spectrometer();
   }
   else
   {
	IDLog("Echo test failed.\n");
	connection_status = -1;
        PortTP.s = IPS_ALERT;
        IDSetText (&PortTP, "Spectrometer echo test failed. Please recheck connection to spectrometer and try again.");
	return false;
   }

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
   int err_code = 0, nbytes_written =0, nbytes_read=0;
   char response[SPECTROMETER_CMD_REPLY];
   char err_msg[SPECTROMETER_ERROR_BUFFER];

	if (!strcmp(CommandSP.name, name))
	{
		if (IUUpdateSwitch(&CommandSP, states, names, n) < 0)
			return;

		dispatch_command(READ_CHANNEL);

		IUResetSwitch(&CommandSP);

		if ( (err_code = tty_read(fd, response, SPECTROMETER_CMD_REPLY, 5, &nbytes_read)) != TTY_OK)
		{
			tty_error_msg(err_code, err_msg, 32);
			IDLog("TTY error detected: %s\n", err_msg);
			CommandSP.s = IPS_ALERT;
			IDSetSwitch(&CommandSP, "Command failed. TTY error detected: %s", err_msg);
			return;
		}

		IDLog("Reponse from Spectrometer: #%s#\n", response);

		int result=0;
	        sscanf(response, "D%x", &result);

		// We divide by 409.5 to scale the value to 0 - 10 VDC range
		ChannelValueN[0].value = result / 409.5;
		CommandSP.s = IPS_OK;
		IDSetSwitch(&CommandSP, NULL);
		IDSetNumber(&ChannelValueNP, NULL);

		return;
	}

	// Testing
	if (!strcmp(ContGainSP.name, name))
	{

		if (IUUpdateSwitch(&ContGainSP, states, names, n) < 0)
			return;

		ContGainSP.s = IPS_OK;

		IDSetSwitch(&ContGainSP, "Switch #%d was selected.", get_on_switch(&ContGainSP) + 1);

		return;
	}

}

bool knroSpectrometer::dispatch_command(SpectrometerCommand command_type)
{
   char spectrometer_error[SPECTROMETER_ERROR_BUFFER];
   int err_code = 0, nbytes_written =0, final_value=0;
   // Maximum of 3 hex digits in addition to null terminator
   char hex[4];

   tcflush(fd, TCIOFLUSH);

   switch (command_type)
   {
	// Intermediate Frequency Gain
	case IF_GAIN:
		command[1] = 'A';
		command[2] = '0';
		// Equation is
		// Value = ((X - 10) * 63) / 15.75, where X is the user selection (10dB to 25.75dB)
		final_value = (int) ((IFGainN[0].value - 10) * 63) / 15.75;
		sprintf(hex, "%02x", final_value);
		command[3] = hex[0];
		command[4] = hex[1];
		break;

	// Continuum Gain
	case CONT_GAIN:
		command[1] = 'C';
		command[2] = '0';
		command[3] = '0';
		final_value = get_on_switch(&ContGainSP);
		sprintf(hex, "%x", final_value);
		command[4] = hex[0];
		break;

       // Continuum Integration
       case CONT_TIME:
		command[1] = 'I';
		command[2] = '0';
		command[3] = '0';
		final_value = get_on_switch(&ContIntegrationSP);
		sprintf(hex, "%x", final_value);
		command[4] = hex[0];
		break;

       // Spectral Gain
       case SPEC_GAIN:
		command[1] = 'K';
		command[2] = '0';
		command[3] = '0';
		final_value = get_on_switch(&SpecGainSP);
		sprintf(hex, "%x", final_value);
		command[4] = hex[0];
		break;


       // Spectral Integration
       case SPEC_TIME:
		command[1] = 'L';
		command[2] = '0';
		command[3] = '0';
		final_value = get_on_switch(&SpecIntegrationSP);
		sprintf(hex, "%x", final_value);
		command[4] = hex[0];
		break;

	// Continuum DC Offset
	case CONT_OFFSET:
		command[1] = 'O';
		final_value = (int) DCOffsetN[CONTINUUM_CHANNEL].value / 0.001; 
		sprintf(hex, "%03x", final_value);
		command[2] = hex[0];
		command[3] = hex[1];
		command[4] = hex[2];
		break;

	// Spectral DC Offset
	case SPEC_OFFSET:
		command[1] = 'J';
		final_value = (int) DCOffsetN[SPECTRAL_CHANNEL].value / 0.001; 
		sprintf(hex, "%03x", final_value);
		command[2] = hex[0];
		command[3] = hex[1];
		command[4] = hex[2];
		break;

	// Read Channel
	case READ_CHANNEL:
		command[1] = 'D';
		command[2] = '0';
		command[3] = '0';
		final_value = get_on_switch(&CommandSP);
		sprintf(hex, "%x", final_value);
		command[3] = hex[0];
		break;
		
       // Reset
       case RESET:
		command[1] = 'R';
		command[2] = '0';
		command[3] = '0';
		command[4] = '0';
		break;
   }
		

   IDLog("Dispatching command #%s#\n", command);

   if  ( (err_code = tty_write(fd, command, SPECTROMETER_CMD_LEN, &nbytes_written) != TTY_OK))
   {
	tty_error_msg(err_code, spectrometer_error, SPECTROMETER_ERROR_BUFFER);
	IDLog("TTY error detected: %s\n", spectrometer_error);
   	return false;
   }

   return true;
}

int knroSpectrometer::get_on_switch(ISwitchVectorProperty *sp)
{
 for (int i=0; i < sp->nsp ; i++)
     if (sp->sp[i].s == ISS_ON)
      return i;

 return -1;
}


