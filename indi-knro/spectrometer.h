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

#include "knro_common.h"

using namespace std;

class knroSpectrometer
{

public:

    enum SpectrometerCommand { SSI_OUTPUT_MODE = 1, NUM_OF_TURNS = 2, FULL_COUNT = 3, BAUD_RATE = 4, Spectrometer_TYPE = 5, POSITION_VALUE = 9 };
    enum SpectrometerError { NO_ERROR, BAUD_RATE_ERROR, FLASH_MEMORY_ERROR, WRONG_COMMAND_ERROR, WRONG_PARAMETER_ERROR, FATAL_ERROR };

    knroSpectrometer();
    ~knroSpectrometer();
   
    bool connect();
    void disconnect();

    // Simulation
    void enable_simulation ();
    void disable_simulation();
    
    // Standard INDI interface fucntions
    void ISGetProperties();
    void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 	
    void reset_all_properties(bool reset_to_idle=false);

private: 
    // Spectrometer Port number
    ITextVectorProperty PortTP;
    IText PortT[1];

    // Functions
    void init_properties();
    bool init_spectrometer();
    bool check_spectrometer_connection();
    bool dispatch_command(SpectrometerCommand command);

    // Variables
    string type_name;
    string default_port;
		
    int connection_status; 
    bool simulation;
		
    int fd;

};

