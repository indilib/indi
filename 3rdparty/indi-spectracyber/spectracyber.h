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
    2009-09-17: Creating spectrometer class (JM)

*/

#ifndef SPECTRACYBER_H
#define SPECTRACYBER_H

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#include <string>

#include <indidevapi.h>
#include <indicom.h>

using namespace std;

#define MAXBLEN 32

class SpectraCyber
{

public:

    enum SpectrometerCommand { IF_GAIN ,	// IF Gain
			       CONT_GAIN ,	// Continuum Gain
			       SPEC_GAIN ,	// Spectral Gain
			       CONT_TIME ,	// Continuum Channel Integration Constant
			       SPEC_TIME ,      // Spectral Channel Integration Constant
                               NOISE_SOURCE,    // Noise Source Control
                               CONT_OFFSET,     // Continuum DC Offset
                               SPEC_OFFSET,     // Spectral DC Offset
                               RECV_FREQ,       // Receive Frequency
                               READ_CHANNEL,    // Read Channel Value
			       BANDWIDTH,	// Bandwidth
			       RESET		// Reset All
			     };

    enum SpectrometerChannel { CONTINUUM_CHANNEL, SPECTRAL_CHANNEL };
    enum SpectrometerError { NO_ERROR, BAUD_RATE_ERROR, FLASH_MEMORY_ERROR, WRONG_COMMAND_ERROR, WRONG_PARAMETER_ERROR, FATAL_ERROR };

    SpectraCyber();
    ~SpectraCyber();
   
    bool connect();
    void disconnect();

    // Simulation
    void enable_simulation (bool to_enable);
    void enable_debug(bool to_enable);

    bool is_connected();
    
    // Standard INDI interface fucntions
    void ISGetProperties();
    void ISNewNumber (const char *name, double values[], char *names[], int n);
    void ISNewText (const char *name, char *texts[], char *names[], int n);
    void ISNewSwitch (const char *name, ISState *states, char *names[], int n);
 	
    void reset_all_properties(bool reset_to_idle=false);
    bool update_freq(double nFreq);
    void ISPoll();

private: 

    // Connect
    ISwitch ConnectS[2];
    ISwitchVectorProperty ConnectSP;				

    // Spectrometer Port number
    ITextVectorProperty PortTP;
    IText PortT[1];

    // Current Frequency
    INumber FreqN[1];
    INumberVectorProperty FreqNP;

    // Scan Range and Rate
    INumber ScanN[3];
    INumberVectorProperty ScanNP;

    // Scan command
    ISwitch ScanS[2];
    ISwitchVectorProperty ScanSP;

    // IF 70 Mhz Gain
    INumber IFGainN[1];
    INumberVectorProperty IFGainNP;

    // Continuum Gain
    ISwitch ContGainS[6];
    ISwitchVectorProperty ContGainSP;

    // Continuum Integration
    ISwitch ContIntegrationS[3];
    ISwitchVectorProperty ContIntegrationSP;

    // Spectral Gain
    ISwitch SpecGainS[6];
    ISwitchVectorProperty SpecGainSP;

    // Spectral Integration
    ISwitch SpecIntegrationS[3];
    ISwitchVectorProperty SpecIntegrationSP;

    // DC Offsets
    INumber DCOffsetN[2];
    INumberVectorProperty DCOffsetNP;

    // Channels
    ISwitch ChannelS[2];
    ISwitchVectorProperty ChannelSP;

   // Bandwidth
   ISwitch BandwidthS[2];
   ISwitchVectorProperty BandwidthSP;

   // 12 bit binary read value
   INumber ChannelValueN[1];
   INumberVectorProperty ChannelValueNP;

   // Reset Options
   ISwitch ResetS[1];
   ISwitchVectorProperty ResetSP;

   // Simulation
   ISwitch SimulationS[2];
   ISwitchVectorProperty SimulationSP;

    ISwitch DebugS[2];
    ISwitchVectorProperty DebugSP;

   // Stream Blob
   IBLOB DataStreamB[1];
   IBLOBVectorProperty DataStreamBP;

    // Functions
    void init_properties();
    bool init_spectrometer();
    void abort_scan();
    bool read_channel();
    bool dispatch_command(SpectrometerCommand command);
    int get_on_switch(ISwitchVectorProperty *sp);
    bool reset();

    // Variables
    string type_name;
    string default_port;
		
    int connection_status; 
    bool simulation, debug;
		
    int fd;
    char bLine[MAXBLEN];
    char command[5];
    double start_freq, target_freq, sample_rate, JD, chanValue;


};

#endif


