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

#include <libnova/libnova.h>

#include "spectracyber.h"

#define mydev		"SpectraCyber"
#define BASIC_GROUP	"Main Control"
#define OPTIONS_GROUP	"Options"

#define current_freq FreqN[0].value
#define CONT_CHANNEL 0
#define SPEC_CHANNEL 1


const int POLLMS = 1000;

const int SPECTROMETER_READ_BUFFER = 16;
const int SPECTROMETER_ERROR_BUFFER = 128;
const int SPECTROMETER_CMD_LEN = 5;
const int SPECTROMETER_CMD_REPLY = 4;

const double SPECTROMETER_MIN_FREQ = 46.4;
const double SPECTROMETER_REST_FREQ = 48.6;
const double SPECTROMETER_MAX_FREQ = 51.2;
const double SPECTROMETER_RF_FREQ = 1371.805;

const unsigned int SPECTROMETER_OFFSET = 0x050;

const char * contFMT=".ascii_cont";
const char * specFMT=".ascii_spec";


// We declare an auto pointer to spectrometer.
auto_ptr<SpectraCyber> spectracyber(0);

void ISPoll(void *p);


void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(spectracyber.get() == 0) spectracyber.reset(new SpectraCyber());
    IEAddTimer(POLLMS, ISPoll, NULL);

}

void ISPoll(void *p)
{
    spectracyber->ISPoll();
    IEAddTimer(POLLMS, ISPoll, NULL);
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
  
  simulation = debug = false;
  
  init_properties();

  // Command pre-limeter 
  command[0] = '!';

  srand( time(NULL));

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

  IUFillSwitch(&ConnectS[0], "CONNECT", "Connect", ISS_OFF);
  IUFillSwitch(&ConnectS[1], "DISCONNECT", "Disconnect", ISS_ON);
  IUFillSwitchVector(&ConnectSP, ConnectS, NARRAY(ConnectS), mydev, "CONNECTION", "Connection", BASIC_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

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

  // ChannelS
  IUFillSwitch(&ChannelS[0], "Continuum", "", ISS_ON);
  IUFillSwitch(&ChannelS[1], "Spectral", "", ISS_OFF);
  IUFillSwitchVector(&ChannelSP, ChannelS, NARRAY(ChannelS), mydev, "Channels", "", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Channel Value
  IUFillNumber(&ChannelValueN[0], "Value", "", "%g",  0., 10, 0.1, 0.);
  IUFillNumberVector(&ChannelValueNP, ChannelValueN, NARRAY(ChannelValueN), mydev, "Read Out (v)", "", BASIC_GROUP, IP_RO, 0, IPS_IDLE);

  // Reset
  IUFillSwitch(&ResetS[0], "Reset", "", ISS_OFF);
  IUFillSwitchVector(&ResetSP, ResetS, NARRAY(ResetS), mydev, "Parameters", "", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Current Frequency
  IUFillNumber(&FreqN[0], "Value", "", "%.3f",  SPECTROMETER_RF_FREQ+SPECTROMETER_MIN_FREQ, SPECTROMETER_RF_FREQ+SPECTROMETER_MAX_FREQ ,
               0.1, SPECTROMETER_RF_FREQ+SPECTROMETER_REST_FREQ);
  IUFillNumberVector(&FreqNP, FreqN, NARRAY(FreqN), mydev, "Freq (Mhz)", "", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

  // Scan Range and Rate
  IUFillNumber(&ScanN[0], "Low (Khz)", "", "%g",  -2000., 0., 100., -600.);
  IUFillNumber(&ScanN[1], "High (Khz)", "", "%g",  0, 2000., 100., 600.);
  IUFillNumber(&ScanN[2], "Step (5 Khz)", "", "%g",  1., 4., 1., 1.);
  IUFillNumberVector(&ScanNP, ScanN, NARRAY(ScanN), mydev, "Scan Options", "", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

  // Scan command
  IUFillSwitch(&ScanS[0], "Start", "", ISS_OFF);
  IUFillSwitch(&ScanS[1], "Stop", "", ISS_ON);
  IUFillSwitchVector(&ScanSP, ScanS, NARRAY(ScanS), mydev, "Scan", "", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Simulation
  IUFillSwitch(&SimulationS[0], "Enable", "", ISS_OFF);
  IUFillSwitch(&SimulationS[1], "Disable", "", ISS_ON);
  IUFillSwitchVector(&SimulationSP, SimulationS, NARRAY(SimulationS), mydev, "Simulation", "", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  IUFillSwitch(&DebugS[0], "Enable", "", ISS_OFF);
  IUFillSwitch(&DebugS[1], "Disable", "", ISS_ON);
  IUFillSwitchVector(&DebugSP, DebugS, NARRAY(DebugS), mydev, "Debug", "", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  // Data Stream
  IUFillBLOB(&DataStreamB[0], "Stream", "JD Value Freq", "");
  IUFillBLOBVector(&DataStreamBP, DataStreamB, NARRAY(DataStreamB), mydev, "Data", "", BASIC_GROUP, IP_RO, 360., IPS_IDLE);

  DataStreamB[0].blob = (char *) malloc(MAXBLEN * sizeof(char));

}

/****************************************************************
**
**
*****************************************************************/
void SpectraCyber::ISGetProperties()
{
   IDDefSwitch(&ConnectSP, NULL);
   IDDefText(&PortTP, NULL);

   IDDefNumber(&FreqNP, NULL);
   IDDefNumber(&ScanNP, NULL);
   IDDefSwitch(&ChannelSP, NULL);
   IDDefSwitch(&ScanSP, NULL);
   IDDefBLOB(&DataStreamBP, NULL);

   IDDefNumber(&IFGainNP, NULL);
   IDDefSwitch(&ContGainSP, NULL);
   IDDefSwitch(&ContIntegrationSP, NULL);
   IDDefSwitch(&SpecGainSP, NULL);
   IDDefSwitch(&SpecIntegrationSP, NULL);
   IDDefNumber(&DCOffsetNP, NULL);
   IDDefSwitch(&BandwidthSP, NULL);
   IDDefSwitch(&ResetSP, NULL);
   IDDefSwitch(&SimulationSP, NULL);
   IDDefSwitch(&DebugSP, NULL);
}

/****************************************************************
**
**
*****************************************************************/
void SpectraCyber::reset_all_properties(bool reset_to_idle)
{
	if (reset_to_idle)
	{
		ConnectSP.s		= IPS_IDLE;
		PortTP.s		= IPS_IDLE;
		IFGainNP.s		= IPS_IDLE;
		ContGainSP.s		= IPS_IDLE;
		ContIntegrationSP.s     = IPS_IDLE;
		SpecGainSP.s		= IPS_IDLE;
		SpecIntegrationSP.s     = IPS_IDLE;
		DCOffsetNP.s		= IPS_IDLE;
		BandwidthSP.s 		= IPS_IDLE;
                ChannelSP.s 		= IPS_IDLE;
		ResetSP.s		= IPS_IDLE;
                FreqNP.s                = IPS_IDLE;
                ScanNP.s                = IPS_IDLE;
                ScanSP.s                = IPS_IDLE;
                DataStreamBP.s          = IPS_IDLE;
	}
	
	IDSetSwitch(&ConnectSP, NULL);
	IDSetText(&PortTP, NULL);
        IDSetNumber(&IFGainNP, NULL);
	IDSetSwitch(&ContGainSP, NULL);
	IDSetSwitch(&ContIntegrationSP, NULL);
	IDSetSwitch(&SpecGainSP, NULL);
	IDSetSwitch(&SpecIntegrationSP, NULL);
	IDSetNumber(&DCOffsetNP, NULL);
	IDSetSwitch(&BandwidthSP, NULL);
        IDSetSwitch(&ChannelSP, NULL);
	IDSetSwitch(&ResetSP, NULL);
        IDSetNumber(&FreqNP, NULL);
        IDSetNumber(&ScanNP, NULL);
        IDSetSwitch(&ScanSP, NULL);
        IDSetBLOB(&DataStreamBP, NULL);
}

/****************************************************************
**
**
*****************************************************************/   
bool SpectraCyber::connect()
{
   if (is_connected())
		return true;

    if (simulation)
    {
        connection_status = 0;
        IUResetSwitch(&ConnectSP);
        ConnectS[0].s = ISS_ON;
        ConnectSP.s = IPS_OK;
        IDSetSwitch(&ConnectSP, "%s Spectrometer: Simulating connecting to port %s.", type_name.c_str(), PortT[0].text);
	return true;
    }

    if (debug)
        IDLog("Attempting to connect to spectrometer....\n");

    if (tty_connect(PortT[0].text, 2400, 8, 0, 1, &fd) != TTY_OK)
    {
	ConnectSP.s = IPS_ALERT;
	IDSetSwitch (&ConnectSP, "Error connecting to port %s. Make sure you have BOTH read and write permission to the port.", PortT[0].text);
	return false;
    }

    // We perform initial handshake check by resetting all parameter and watching for echo reply
    if (reset() == true)
    {
        connection_status = 0;
        ConnectSP.s = IPS_OK;
        IDSetSwitch (&ConnectSP, "Spectrometer is online. Retrieving preliminary data...");
        return init_spectrometer();
   }
   else
   {
        if (debug)
            IDLog("Echo test failed.\n");
	connection_status = -1;
        ConnectSP.s = IPS_ALERT;
        IDSetSwitch (&ConnectSP, "Spectrometer echo test failed. Please recheck connection to spectrometer and try again.");
	return false;
   }
	
}

/****************************************************************
**
**
*****************************************************************/   
bool SpectraCyber::init_spectrometer()
{

   if (!is_connected())
	return false;
		   
   // Enable speed mode
   if (simulation)
   {
                IDMessage(mydev, "%s Spectrometer: Simulating spectrometer init.", type_name.c_str());
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
void SpectraCyber::enable_simulation (bool to_enable)
{
        if (simulation == to_enable)
		return;
		
         simulation = to_enable;

         if (to_enable)
         {
            IDMessage(mydev, "Notice: spectrometer simulation is enabled.");
            if (debug)
                IDLog("Notice: spectrometer simulation is enabled.\n");
        }
        else
        {
            IDMessage(mydev, "Caution: spectrometer simulation is disabled.");
            if (debug)
                IDLog("Caution: spectrometer simulation is disabled.\n");
        }


}

/****************************************************************
**
**
*****************************************************************/
void SpectraCyber::enable_debug (bool to_enable)
{
        if (debug == to_enable)
                return;

         debug = to_enable;

         if (to_enable)
         {
            IDMessage(mydev, "Notice: spectrometer debug is enabled.");
            IDLog("Notice: spectrometer debug is enabled.\n");
        }
        else
        {
            IDMessage(mydev, "Notice: spectrometer debug is disabled.");
            IDLog("Notice: spectrometer debug is disabled.\n");
        }


}

    
/****************************************************************
**
**
*****************************************************************/
bool SpectraCyber::is_connected()
{
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

        // Freq Change
        if (!strcmp(FreqNP.name, name))
        {

            update_freq(values[0]);
            return;
        }

        // Scan Options

        if (!strcmp(ScanNP.name, name))
        {
            if (IUUpdateNumber(&ScanNP, values, names, n) < 0)
              return;

            ScanNP.s = IPS_OK;
            IDSetNumber(&ScanNP, NULL);
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
		IDSetText(&PortTP, "Port updated.");

		return;
	}
	
}

/****************************************************************
**
**
*****************************************************************/
void SpectraCyber::ISNewSwitch (const char *name, ISState *states, char *names[], int n)
{
	if (!strcmp(ConnectSP.name, name))
	{
		connect();
		return;
	}

        // ===================================
        // Simulation Switch
        if (!strcmp(SimulationSP.name, name))
        {
                if (IUUpdateSwitch(&SimulationSP, states, names, n) < 0)
                        return;

                if (SimulationS[0].s == ISS_ON)
                        enable_simulation(true);
                else
                        enable_simulation(false);

                SimulationSP.s = IPS_OK;
                IDSetSwitch(&SimulationSP, NULL);

                return;
        }

        // ===================================
        // Debug Switch
        if (!strcmp(DebugSP.name, name))
        {
                if (IUUpdateSwitch(&DebugSP, states, names, n) < 0)
                        return;

                if (DebugS[0].s == ISS_ON)
                        enable_debug(true);
                else
                        enable_debug(false);

                DebugSP.s = IPS_OK;
                IDSetSwitch(&DebugSP, NULL);

                return;
        }


        if (is_connected() == false)
        {
            reset_all_properties(true);
            IDMessage(mydev, "Spectrometer is offline. Connect before issiung any commands.");
            return;
        }

        // Scan
        if (!strcmp(ScanSP.name, name))
        {

            if (IUUpdateSwitch(&ScanSP, states, names, n) < 0)
                    return;

            if (ScanS[1].s == ISS_ON)
            {
                if (ScanSP.s == IPS_BUSY)
                {
                    ScanSP.s        = IPS_IDLE;
                    FreqNP.s        = IPS_IDLE;
                    DataStreamBP.s  = IPS_IDLE;

                    IDSetNumber(&FreqNP, NULL);
                    IDSetBLOB(&DataStreamBP, NULL);
                    IDSetSwitch(&ScanSP, "Scan stopped.");
                    return;
                }

                ScanSP.s = IPS_OK;
                IDSetSwitch(&ScanSP, NULL);
                return;
            }

            ScanSP.s        = IPS_BUSY;
            DataStreamBP.s  = IPS_BUSY;

            // Compute starting freq  = base_freq - low
            if (ChannelS[SPEC_CHANNEL].s == ISS_ON)
            {
                start_freq = (SPECTROMETER_RF_FREQ + SPECTROMETER_REST_FREQ) - abs( (int) ScanN[0].value)/1000.;
                target_freq = (SPECTROMETER_RF_FREQ + SPECTROMETER_REST_FREQ) + abs( (int) ScanN[1].value)/1000.;
                sample_rate = ScanN[2].value * 5;
                FreqN[0].value = start_freq;
                FreqNP.s        = IPS_BUSY;
                IDSetNumber(&FreqNP, NULL);
                IDSetSwitch(&ScanSP, "Starting spectral scan from %g MHz to %g MHz in steps of %g KHz...", start_freq, target_freq,sample_rate);
            }
            else
                IDSetSwitch(&ScanSP, "Starting continuum scan @ %g MHz...", FreqN[0].value);

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

        // Channel selection
        if (!strcmp(ChannelSP.name, name))
        {
            static int lastChannel;

            lastChannel = get_on_switch(&ChannelSP);

            if (IUUpdateSwitch(&ChannelSP, states, names, n) < 0)
                    return;

            ChannelSP.s = IPS_OK;
            if (ScanSP.s == IPS_BUSY && lastChannel != get_on_switch(&ChannelSP))
            {
                abort_scan();
                IDSetSwitch(&ChannelSP, "Scan aborted due to change of channel selection.");
            }
            else
              IDSetSwitch(&ChannelSP, NULL);

            return;
        }

        // Reset
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
                sprintf(hex, "%d", final_value);
                command[4] = hex[0];
		break;

       // Continuum Integration
       case CONT_TIME:
		command[1] = 'I';
		command[2] = '0';
		command[3] = '0';
		final_value = get_on_switch(&ContIntegrationSP);
                sprintf(hex, "%d", final_value);
                command[4] = hex[0];
		break;

       // Spectral Gain
       case SPEC_GAIN:
		command[1] = 'K';
		command[2] = '0';
		command[3] = '0';
		final_value = get_on_switch(&SpecGainSP);
                sprintf(hex, "%d", final_value);
                command[4] = hex[0];

		break;


       // Spectral Integration
       case SPEC_TIME:
		command[1] = 'L';
		command[2] = '0';
		command[3] = '0';
		final_value = get_on_switch(&SpecIntegrationSP);
                sprintf(hex, "%d", final_value);
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

        // FREQ
        case RECV_FREQ:
               command[1] = 'F';
               // Each value increment is 5 Khz. Range is 050h to 3e8h.
               // 050h corresponds to 46.4 Mhz (min), 3e8h to 51.2 Mhz (max)
               // To compute the desired received freq, we take the diff (target - min) / 0.005
               // 0.005 Mhz = 5 Khz
               // Then we add the diff to 050h (80 in decimal) to get the final freq.
               // e.g. To set 50.00 Mhz, diff = 50 - 46.4 = 3.6 / 0.005 = 800 = 320h
               //      Freq = 320h + 050h (or 800 + 80) = 370h = 880 decimal

               final_value = (int) ((FreqN[0].value - FreqN[0].min) / 0.005 + SPECTROMETER_OFFSET) ;
               sprintf(hex, "%03x", final_value);
               if (debug)
                   IDLog("Required Freq is: %.3f --- Min Freq is: %.3f --- Spec Offset is: %d -- Final Value (Dec): %d --- Final Value (Hex): %s\n",
                         FreqN[0].value, FreqN[0].min, SPECTROMETER_OFFSET, final_value, hex);
               command[2] = hex[0];
               command[3] = hex[1];
               command[4] = hex[2];
               break;

	// Read Channel
	case READ_CHANNEL:
		command[1] = 'D';
		command[2] = '0';
		command[3] = '0';
                final_value = get_on_switch(&ChannelSP);
                command[4] = (final_value == 0) ? '0' : '1';
		break;
		
	// Bandwidth
	case BANDWIDTH:
		command[1] = 'B';
		command[2] = '0';
		command[3] = '0';
		final_value = get_on_switch(&BandwidthSP);
                //sprintf(hex, "%x", final_value);
                command[4] = (final_value == 0) ? '0' : '1';
		break;

       // Reset
       case RESET:
		command[1] = 'R';
		command[2] = '0';
		command[3] = '0';
		command[4] = '0';
		break;
   }
		
   if (debug)
        IDLog("Dispatching command #%s#\n", command);

   if (simulation)
       return true;

   if  ( (err_code = tty_write(fd, command, SPECTROMETER_CMD_LEN, &nbytes_written) != TTY_OK))
   {
	tty_error_msg(err_code, spectrometer_error, SPECTROMETER_ERROR_BUFFER);
        if (debug)
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


bool SpectraCyber::update_freq(double nFreq)
{
    double last_value = FreqN[0].value;

    if (nFreq < FreqN[0].min || nFreq > FreqN[0].max)
        return false;

    FreqN[0].value = nFreq;

    if (dispatch_command(RECV_FREQ) == false)
    {
        FreqN[0].value = last_value;
        FreqNP.s = IPS_ALERT;
        IDSetNumber(&FreqNP, "Error dispatching RECV FREQ command to spectrometer. Check logs.");
        return false;
    }

    if (ScanSP.s != IPS_BUSY)
        FreqNP.s = IPS_OK;

    IDSetNumber(&FreqNP, NULL);
    //IDSetNumber(&FreqNP, "%.3f Mhz", FreqN[0].value);
    return true;
}

bool SpectraCyber::reset()
{
   int err_code = 0, nbytes_written =0, nbytes_read=0;
   char response[4];
   char err_msg[SPECTROMETER_ERROR_BUFFER];

    if (debug)
        IDLog("Attempting to write to spectrometer....\n");

   dispatch_command(RESET);

   if (debug)
       IDLog("Attempting to read from spectrometer....\n");

   // Read echo from spectrometer, we're expecting R000
   if ( (err_code = tty_read(fd, response, SPECTROMETER_CMD_REPLY, 5, &nbytes_read)) != TTY_OK)
   {
		tty_error_msg(err_code, err_msg, 32);
                if (debug)
                    IDLog("TTY error detected: %s\n", err_msg);
   		return false;
   }

   if (debug)
       IDLog("Reponse from Spectrometer: #%c# #%c# #%c# #%c#\n", response[0], response[1], response[2], response[3]);

   if (strstr(response, "R000"))
   {
       if (debug)
            IDLog("Echo test passed.\n");
        FreqN[0].value = SPECTROMETER_MIN_FREQ;
        IFGainN[0].value = 10.0;
        DCOffsetN[0].value = DCOffsetN[1].value = 0;
        IUResetSwitch(&BandwidthSP);
        BandwidthS[0].s = ISS_ON;
        IUResetSwitch(&ContIntegrationSP);
        ContIntegrationS[0].s = ISS_ON;
        IUResetSwitch(&SpecIntegrationSP);
        SpecIntegrationS[0].s = ISS_ON;
        IUResetSwitch(&ContGainSP);
        IUResetSwitch(&SpecGainSP);
        ContGainS[0].s = ISS_ON;
        SpecGainS[0].s = ISS_ON;

        IDSetNumber(&FreqNP, NULL);
        IDSetNumber(&DCOffsetNP, NULL);
        IDSetSwitch(&BandwidthSP, NULL);
        IDSetSwitch(&ContIntegrationSP, NULL);
        IDSetSwitch(&SpecIntegrationSP, NULL);
        IDSetSwitch(&ContGainSP, NULL);
        IDSetSwitch(&SpecGainSP, NULL);

	return true;
   }

   if (debug)
       IDLog("Echo test failed.\n");
   return false;
}

void SpectraCyber::ISPoll()
{
   if (!is_connected())
       return;


   switch(ScanSP.s)
   {

      case IPS_BUSY:
         if (ChannelS[CONT_CHANNEL].s == ISS_ON)
             break;

         if (current_freq >= target_freq)
         {
            ScanSP.s = IPS_OK;
            FreqNP.s = IPS_OK;

            IDSetNumber(&FreqNP, NULL);
            IDSetSwitch(&ScanSP, "Scan complete.");
            return;
         }

         if (update_freq(current_freq) == false)
         {
             abort_scan();
             return;
         }

         current_freq += sample_rate / 1000.;
         break;
    }


   switch (DataStreamBP.s)
   {
      case IPS_BUSY:
       if (ScanSP.s != IPS_BUSY)
       {
           DataStreamBP.s = IPS_IDLE;
           IDSetBLOB(&DataStreamBP, NULL);
           break;
       }

       if (read_channel() == false)
       {
           DataStreamBP.s = IPS_ALERT;

           if (ScanSP.s == IPS_BUSY)
               abort_scan();

           IDSetBLOB(&DataStreamBP, NULL);
       }


       JD = ln_get_julian_from_sys();

       // Continuum
       if (ChannelS[0].s == ISS_ON)
           strncpy(DataStreamB[0].format, contFMT, MAXINDIBLOBFMT);
       else
           strncpy(DataStreamB[0].format, specFMT, MAXINDIBLOBFMT);


       snprintf(bLine, MAXBLEN, "%.8f %.3f %.3f", JD, chanValue, current_freq);

       DataStreamB[0].bloblen = DataStreamB[0].size = strlen(bLine);
       memcpy(DataStreamB[0].blob, bLine, DataStreamB[0].bloblen);

       //IDLog("\nSTRLEN: %d -- BLOB:'%s'\n", strlen(bLine), (char *) DataStreamB[0].blob);

       IDSetBLOB(&DataStreamBP, NULL);

      break;



  }


}

void SpectraCyber::abort_scan()
{
    FreqNP.s = IPS_IDLE;
    ScanSP.s = IPS_ALERT;

    IUResetSwitch(&ScanSP);
    ScanS[1].s = ISS_ON;

    IDSetNumber(&FreqNP, NULL);
    IDSetSwitch(&ScanSP, "Scan aborted due to errors.");
}

bool SpectraCyber::read_channel()
{
    int err_code = 0, nbytes_written =0, nbytes_read=0;
    char response[SPECTROMETER_CMD_REPLY];
    char err_msg[SPECTROMETER_ERROR_BUFFER];

    if (simulation)
    {
         chanValue = ((double )rand())/( (double) RAND_MAX) * 10.0;
         return true;
    }

    dispatch_command(READ_CHANNEL);
    if ( (err_code = tty_read(fd, response, SPECTROMETER_CMD_REPLY, 5, &nbytes_read)) != TTY_OK)
    {
            tty_error_msg(err_code, err_msg, 32);
            if (debug)
                IDLog("TTY error detected: %s\n", err_msg);
            return false;
    }

    if (debug)
        IDLog("Reponse from Spectrometer: #%s#\n", response);

    int result=0;
    sscanf(response, "D%x", &result);
    // We divide by 409.5 to scale the value to 0 - 10 VDC range
    chanValue = result / 409.5;

    return true;
}
