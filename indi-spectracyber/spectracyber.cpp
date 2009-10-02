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
#include <memory>
#include <indicom.h>

#include "spectracyber.h"

#define mydev		"SpectraCyber"
#define BASIC_GROUP	"Main Control"
#define OPTIONS_GROUP	"Options"

const int SPECTROMETER_READ_BUFFER = 16;
const int SPECTROMETER_ERROR_BUFFER = 128;
const int SPECTROMETER_CMD_LEN = 5;
const int SPECTROMETER_CMD_REPLY = 4;

// We declare an auto pointer to spectrometer.
auto_ptr<SpectraCyber> spectracyber(0);

void ISInit()
{
	if(spectracyber.get() == 0) spectracyber.reset(new SpectraCyber());
}

void ISGetProperties(const char *dev)
{ 
	if(dev && strcmp(mydev, dev)) return;
	ISInit();
	spectracyber->ISGetProperties();
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
	if(dev && strcmp (mydev, dev)) return;
	ISInit();
	spectracyber->ISNewSwitch(name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
	if(dev && strcmp (mydev, dev)) return;
	ISInit();
	spectracyber->ISNewText(name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
	if(dev && strcmp (mydev, dev)) return;
	ISInit();
	spectracyber->ISNewNumber(name, values, names, num);
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
SpectraCyber::SpectraCyber()
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
SpectraCyber::~SpectraCyber()
{




}

/****************************************************************
**
**
*****************************************************************/
void SpectraCyber::init_properties()
{

   default_port = string("/dev/ttyUSB0");
   IUFillText(&PortT[0], "PORT", "Port", default_port.c_str());
   IUFillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "SPECTROMETER_PORT", "Spectrometer", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

  // Intermediate Frequency Gain (IF)
  IUFillNumber(&IFGainN[0], "Gain (dB)", "", "%g",  10., 25.75, 0.25, 10.0);
  IUFillNumberVector(&IFGainNP, IFGainN, NARRAY(IFGainN), mydev, "70 Mhz IF", "", OPTIONS_GROUP, IP_RW, 0, IPS_IDLE);

  // Continuum Gain
  IUFillSwitch(&ContGainS[0], "x1", "", ISS_ON);
  IUFillSwitch(&ContGainS[1], "x5", "", ISS_OFF);
  IUFillSwitch(&ContGainS[2], "x10", "", ISS_OFF);
  IUFillSwitch(&ContGainS[3], "x20", "", ISS_OFF);
  IUFillSwitch(&ContGainS[4], "x50", "", ISS_OFF);
  IUFillSwitch(&ContGainS[5], "x60", "", ISS_OFF);
  IUFillSwitchVector(&ContGainSP, ContGainS, NARRAY(ContGainS), mydev, "Continuum Gain", "", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Continuum Integration
  IUFillSwitch(&ContIntegrationS[0], "0.3", "", ISS_ON);
  IUFillSwitch(&ContIntegrationS[1], "1", "", ISS_OFF);
  IUFillSwitch(&ContIntegrationS[2], "10", "", ISS_OFF);
  IUFillSwitchVector(&ContIntegrationSP, ContIntegrationS, NARRAY(ContIntegrationS), mydev, "Continuum Integration (s)", "", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Spectral Gain
  IUFillSwitch(&SpecGainS[0], "x1", "", ISS_ON);
  IUFillSwitch(&SpecGainS[1], "x5", "", ISS_OFF);
  IUFillSwitch(&SpecGainS[2], "x10", "", ISS_OFF);
  IUFillSwitch(&SpecGainS[3], "x20", "", ISS_OFF);
  IUFillSwitch(&SpecGainS[4], "x50", "", ISS_OFF);
  IUFillSwitch(&SpecGainS[5], "x60", "", ISS_OFF);
  IUFillSwitchVector(&SpecGainSP, SpecGainS, NARRAY(SpecGainS), mydev, "Spectral Gain", "", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Spectral Integration
  IUFillSwitch(&SpecIntegrationS[0], "0.3", "", ISS_ON);
  IUFillSwitch(&SpecIntegrationS[1], "0.5", "", ISS_OFF);
  IUFillSwitch(&SpecIntegrationS[2], "1", "", ISS_OFF);
  IUFillSwitchVector(&SpecIntegrationSP, SpecIntegrationS, NARRAY(SpecIntegrationS), mydev, "Spectral Integration (s)", "", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // DC Offsets
  IUFillNumber(&DCOffsetN[CONTINUUM_CHANNEL], "Continuum (v)", "", "%g",  0., 4.096, 0.001, 0.);
  IUFillNumber(&DCOffsetN[SPECTRAL_CHANNEL], "Spectral (v)", "", "%g",  0., 4.096, 0.001, 0.);
  IUFillNumberVector(&DCOffsetNP, DCOffsetN, NARRAY(DCOffsetN), mydev, "DC Offset", "", OPTIONS_GROUP, IP_RW, 0, IPS_IDLE);

   // Bandwidth
  IUFillSwitch(&BandwidthS[0], "15", "", ISS_ON);
  IUFillSwitch(&BandwidthS[1], "30", "", ISS_OFF);
  IUFillSwitchVector(&BandwidthSP, BandwidthS, NARRAY(BandwidthS), mydev, "Bandwidth (Khz)", "", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Commands
  IUFillSwitch(&CommandS[0], "Read Continuum", "", ISS_OFF);
  IUFillSwitch(&CommandS[1], "Read Spectral", "", ISS_OFF);
  IUFillSwitchVector(&CommandSP, CommandS, NARRAY(CommandS), mydev, "Commands", "", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Channel Value
  IUFillNumber(&ChannelValueN[0], "Value", "", "%g",  0., 10, 0.1, 0.);
  IUFillNumberVector(&ChannelValueNP, ChannelValueN, NARRAY(ChannelValueN), mydev, "Read Out (v)", "", BASIC_GROUP, IP_RO, 0, IPS_IDLE);

  // Reset
  IUFillSwitch(&ResetS[0], "Reset", "", ISS_OFF);
  IUFillSwitchVector(&ResetSP, ResetS, NARRAY(ResetS), mydev, "Parameters", "", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  
}

/****************************************************************
**
**
*****************************************************************/
void SpectraCyber::ISGetProperties()
{
   IDDefText(&PortTP, NULL);
   IDDefSwitch(&CommandSP, NULL);
   IDDefSwitch(&ResetSP, NULL);
   IDDefNumber(&ChannelValueNP, NULL);

   IDDefNumber(&IFGainNP, NULL);
   IDDefSwitch(&ContGainSP, NULL);
   IDDefSwitch(&ContIntegrationSP, NULL);
   IDDefSwitch(&SpecGainSP, NULL);
   IDDefSwitch(&SpecIntegrationSP, NULL);
   IDDefNumber(&DCOffsetNP, NULL);
   IDDefSwitch(&BandwidthSP, NULL);
}

/****************************************************************
**
**
*****************************************************************/
void SpectraCyber::reset_all_properties(bool reset_to_idle)
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
bool SpectraCyber::connect()
{
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

    // We perform initial handshake check by resetting all parameter and watching for echo reply
    if (reset() == true)
    {
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
bool SpectraCyber::init_spectrometer()
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
void SpectraCyber::disconnect()
{
	connection_status = -1;
	tty_disconnect(fd);
}

/****************************************************************
**
**
*****************************************************************/
void SpectraCyber::enable_simulation ()
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
void SpectraCyber::disable_simulation()
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
bool SpectraCyber::check_spectrometer_connection()
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
void SpectraCyber::ISNewNumber (const char *name, double values[], char *names[], int n)
{
  static double cont_offset=0;
  static double spec_offset=0;
  
        // IF Gain
	if (!strcmp(IFGainNP.name, name))
	{
	  double last_value = IFGainN[0].value;
	  
	  if (IUUpdateNumber(&IFGainNP, values, names, n) < 0)
	    return;
	  
	  if (dispatch_command(IF_GAIN) == false)
	  {
	    IFGainN[0].value = last_value;
	    IFGainNP.s = IPS_ALERT;
	    IDSetNumber(&IFGainNP, "Error dispatching IF gain command to spectrometer. Check logs.");
	    return;
	  }
	  
	  IFGainNP.s = IPS_OK;
	  IDSetNumber(&IFGainNP, NULL);
	 
	  return;
	}
	
	// DC Offset
	if (!strcmp(DCOffsetNP.name, name))
	{
	  
	  if (IUUpdateNumber(&DCOffsetNP, values, names, n) < 0)
	    return;
	  
	  // Check which offset change, if none, return gracefully
	  if (DCOffsetN[CONTINUUM_CHANNEL].value != cont_offset)
	  {
	     if (dispatch_command(CONT_OFFSET) == false)
	    {
	      DCOffsetN[CONTINUUM_CHANNEL].value = cont_offset;
	      DCOffsetNP.s = IPS_ALERT;
	      IDSetNumber(&DCOffsetNP, "Error dispatching continuum DC offset command to spectrometer. Check logs.");
	      return;
	    }
	    
	    cont_offset = DCOffsetN[CONTINUUM_CHANNEL].value;
	    DCOffsetNP.s = IPS_OK;
	    IDSetNumber(&DCOffsetNP, NULL);
	    return;
	  }
	  
	  if (DCOffsetN[SPECTRAL_CHANNEL].value != spec_offset)
	  {
	     if (dispatch_command(SPEC_OFFSET) == false)
	    {
	      DCOffsetN[SPECTRAL_CHANNEL].value = spec_offset;
	      DCOffsetNP.s = IPS_ALERT;
	      IDSetNumber(&DCOffsetNP, "Error dispatching spectral DC offset command to spectrometer. Check logs.");
	      return;
	    }
	    
	    spec_offset = DCOffsetN[SPECTRAL_CHANNEL].value;
	    DCOffsetNP.s = IPS_OK;
	    IDSetNumber(&DCOffsetNP, NULL);
	    return;
	  }

	  // No Change, return
	  DCOffsetNP.s = IPS_OK;
	  IDSetNumber(&DCOffsetNP, NULL);
	 
	  return;
	}
    
}

/****************************************************************
**
**
*****************************************************************/
void SpectraCyber::ISNewText (const char *name, char *texts[], char *names[], int n)
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
void SpectraCyber::ISNewSwitch (const char *name, ISState *states, char *names[], int n)
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

	// Continuum Gain Control
	if (!strcmp(ContGainSP.name, name))
	{
	        int last_switch = get_on_switch(&ContGainSP);
		
		if (IUUpdateSwitch(&ContGainSP, states, names, n) < 0)
			return;

		if (dispatch_command(CONT_GAIN) == false)
		{
		  ContGainSP.s = IPS_ALERT;
		  IUResetSwitch(&ContGainSP);
		  ContGainS[last_switch].s = ISS_ON;
		  IDSetSwitch(&ContGainSP, "Error dispatching continuum gain command to spectrometer. Check logs.");
		  return;
		}

		ContGainSP.s = IPS_OK;
		IDSetSwitch(&ContGainSP, NULL);

		return;
	}
	
	// Spectral Gain Control
	if (!strcmp(SpecGainSP.name, name))
	{
	        int last_switch = get_on_switch(&SpecGainSP);
		
		if (IUUpdateSwitch(&SpecGainSP, states, names, n) < 0)
			return;

		if (dispatch_command(SPEC_GAIN) == false)
		{
		  SpecGainSP.s = IPS_ALERT;
		  IUResetSwitch(&SpecGainSP);
		  SpecGainS[last_switch].s = ISS_ON;
		  IDSetSwitch(&SpecGainSP, "Error dispatching spectral gain command to spectrometer. Check logs.");
		  return;
		}

		SpecGainSP.s = IPS_OK;
		IDSetSwitch(&SpecGainSP, NULL);
		
		return;
	}
	
	// Continuum Integration Control
	if (!strcmp(ContIntegrationSP.name, name))
	{
	        int last_switch = get_on_switch(&ContIntegrationSP);
		
		if (IUUpdateSwitch(&ContIntegrationSP, states, names, n) < 0)
			return;

		if (dispatch_command(CONT_TIME) == false)
		{
		  ContIntegrationSP.s = IPS_ALERT;
		  IUResetSwitch(&ContIntegrationSP);
		  ContIntegrationS[last_switch].s = ISS_ON;
		  IDSetSwitch(&ContIntegrationSP, "Error dispatching continuum integration command to spectrometer. Check logs.");
		  return;
		}

		ContIntegrationSP.s = IPS_OK;
		IDSetSwitch(&ContIntegrationSP, NULL);

		return;
	}
	
	// Spectral Integration Control
	if (!strcmp(SpecIntegrationSP.name, name))
	{
	        int last_switch = get_on_switch(&SpecIntegrationSP);
		
		if (IUUpdateSwitch(&SpecIntegrationSP, states, names, n) < 0)
			return;

		if (dispatch_command(SPEC_TIME) == false)
		{
		  SpecIntegrationSP.s = IPS_ALERT;
		  IUResetSwitch(&SpecIntegrationSP);
		  SpecIntegrationS[last_switch].s = ISS_ON;
		  IDSetSwitch(&SpecIntegrationSP, "Error dispatching spectral integration command to spectrometer. Check logs.");
		  return;
		}

		SpecIntegrationSP.s = IPS_OK;
		IDSetSwitch(&SpecIntegrationSP, NULL);

		return;
	}
	
	// Bandwidth Control
	if (!strcmp(BandwidthSP.name, name))
	{
	        int last_switch = get_on_switch(&BandwidthSP);
		
		if (IUUpdateSwitch(&BandwidthSP, states, names, n) < 0)
			return;

		if (dispatch_command(BANDWIDTH) == false)
		{
		  BandwidthSP.s = IPS_ALERT;
		  IUResetSwitch(&BandwidthSP);
		  BandwidthS[last_switch].s = ISS_ON;
		  IDSetSwitch(&BandwidthSP, "Error dispatching bandwidth change command to spectrometer. Check logs.");
		  return;
		}

		BandwidthSP.s = IPS_OK;
		IDSetSwitch(&BandwidthSP, NULL);

		return;
	}


	if (!strcmp(ResetSP.name, name))
	{
	    if (reset() == true)
	    {
	      ResetSP.s = IPS_OK;
	      IDSetSwitch(&ResetSP, NULL);
	    }
	    else
	    {
	      ResetSP.s = IPS_ALERT;
	      IDSetSwitch(&ResetSP, "Error dispatching reset parameter command to spectrometer. Check logs.");
	    }
	    return;
	}
}

bool SpectraCyber::dispatch_command(SpectrometerCommand command_type)
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
		
		
	// Bandwidth
	case BANDWIDTH:
		command[1] = 'B';
		command[2] = '0';
		command[3] = '0';
		final_value = get_on_switch(&BandwidthSP);
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

int SpectraCyber::get_on_switch(ISwitchVectorProperty *sp)
{
 for (int i=0; i < sp->nsp ; i++)
     if (sp->sp[i].s == ISS_ON)
      return i;

 return -1;
}

bool SpectraCyber::reset()
{
   int err_code = 0, nbytes_written =0, nbytes_read=0;
   char response[4];
   char err_msg[SPECTROMETER_ERROR_BUFFER];

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
	return true;
   }

   IDLog("Echo test failed.\n");
   return false;
}
