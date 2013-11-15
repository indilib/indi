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

#ifndef KNRO_ENCODER_H
#define KNRO_ENCODER_H

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

class knroObservatory;

class knroEncoder
{

public:

    enum encoderType { AZ_ENCODER, ALT_ENCODER };
    enum encoderCommand { SSI_OUTPUT_MODE = 1, NUM_OF_TURNS = 2, FULL_COUNT = 3, BAUD_RATE = 4, ENCODER_TYPE = 5, POSITION_VALUE = 9 };
    enum encoderError { NO_ERROR, BAUD_RATE_ERROR, FLASH_MEMORY_ERROR, WRONG_COMMAND_ERROR, WRONG_PARAMETER_ERROR, FATAL_ERROR };

    knroEncoder(encoderType new_type, knroObservatory *scope);
    ~knroEncoder();

    unsigned int get_abs_encoder_count() { return abs_encoder_count; }
    double get_angle() { return current_angle; }

    void set_type(encoderType new_type);
    encoderType get_type() { return type; }
      
    bool connect();
    void disconnect();

    // Update
    static void * update_helper(void *context);
	
    
    // Simulation
    void enable_simulation ();
    void disable_simulation();
    
    void enable_debug() { debug = true; }
    void disable_debug() { debug = false; }
    
    // Standard INDI interface fucntions
    virtual bool initProperties();
    virtual bool updateProperties(bool connected);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 	
    void reset_all_properties();
    void   update_client(void);

    // Simulation
    void simulate_forward() { simulated_forward = true; }
    void simulate_reverse() { simulated_forward = false; }
    void simulate_stop() { simulated_speed = 0; }
    void simulate_track() { simulated_speed = 10; }
    void simulate_slow() { simulated_speed = 15; }
    void simulate_medium() { simulated_speed = 30; }
    void simulate_fast() { simulated_speed = 50; }

private: 
    // INDI Properties
    // Encoder Absolute Position
    INumber EncoderAbsPosN[2];
    INumberVectorProperty EncoderAbsPosNP;
	
    // Encoder Port number
    ITextVectorProperty PortTP;
    IText PortT[1];

    // Functions
    bool init_encoder();
    void calculate_angle();
    void * update_encoder(void);
    bool check_drive_connection();
    bool dispatch_command(encoderCommand command);
    encoderError get_encoder_value(encoderCommand command, char * response, double & encoder_value);
    bool openEncoderServer (const char * host, int indi_port);


    bool simulated_forward;
    int  simulated_speed;

    // Variables
    string type_name;
    string default_port;
		
    int connection_status; 
    bool simulation;
    bool debug;
		
    unsigned int abs_encoder_count;
    double current_angle;
    encoderType type;

    char encoder_command[4];
    int sockfd;  

    knroObservatory *telescope;

};

#endif
