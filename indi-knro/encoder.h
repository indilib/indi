/*
    Kuwait National Radio Observatory
    INDI Driver for 24 bit AMCI Encoders
    Communication: RS485 Link, Binary

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
    2009-04-26: Creating encoder class (JM)
    2009-08-24: Encoder reports absolute position. Need to establish calibration for ticks_per_degree (JM)

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

class knroEncoder
{

public:

    enum encoderType { AZ_ENCODER, ALT_ENCODER };
    enum encoderCommand { SSI_OUTPUT_MODE = 1, NUM_OF_TURNS = 2, FULL_COUNT = 3, BAUD_RATE = 4, ENCODER_TYPE = 5, POSITION_VALUE = 9 };
    enum encoderError { NO_ERROR, BAUD_RATE_ERROR, FLASH_MEMORY_ERROR, WRONG_COMMAND_ERROR, WRONG_PARAMETER_ERROR, FATAL_ERROR };

    knroEncoder(encoderType new_type);
    ~knroEncoder();

    unsigned int get_abs_encoder_count() { return abs_encoder_count; }
    double get_current_angel() { return current_angle; }

    void set_type(encoderType new_type);
    encoderType get_type() { return type; }
      
    bool connect();
    void disconnect();

    // Update
    void update_encoder_count();
    
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
    // INDI Properties
    // Encoder Absolute Position
    INumber EncoderAbsPosN[2];
    INumberVectorProperty EncoderAbsPosNP;
	
    // Encoder Port number
    ITextVectorProperty PortTP;
    IText PortT[1];

    ISwitch UpdateCountS[1];
    ISwitchVectorProperty UpdateCountSP;

    // Functions
    void init_properties();
    bool init_encoder();
    bool check_drive_connection();
    bool dispatch_command(encoderCommand command);
    encoderError get_encoder_value(encoderCommand command, char * response, double & encoder_value);
    bool openEncoderServer (const char * host, int indi_port);

    // Variables
    string type_name;
    string default_port;
		
    int connection_status; 
    bool simulation;
		
    unsigned int abs_encoder_count;
    double current_angle;
    encoderType type;

    char encoder_command[4];
    int sockfd;
    /*FILE *fp;*/

};

