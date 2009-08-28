/*
    Kuwait National Radio Observatory
    INDI driver for Baldor VS1SP V/Hz Inverter
    Communication: RS485 Link over ModBus

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
    
    2009-05-22: Start

*/

#include <modbus.h>

#include <indidevapi.h>
#include <indicom.h>
#include <string>
#include <bitset>
#include <iostream>

#include "knro_common.h"

using namespace std;

class knroInverter
{

public:

    enum inverterType { AZ_INVERTER, ALT_INVERTER };
    enum inverterMotion { INVERTER_STOP, INVERTER_FORWARD, INVERTER_REVERSE};
    
	knroInverter(inverterType new_type);
	~knroInverter();

      bool move_forward();
      bool move_reverse();
      bool stop();
      
      bool set_speed(float newHz);
      float get_speed() { return InverterSpeedN[0].value; }
      
      void set_type(inverterType new_type);
      inverterType get_type() { return type; }
      
      bool connect();
      void disconnect();

    // TODO remove later
    bool test_address();
    
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

    // Modbus functions
    bool enable_drive();
    bool disable_drive();
    bool init_drive();
    
    // INDI Properties

	// Inverter Speed (Hz)
    INumber InverterSpeedN[1];
	INumberVectorProperty InverterSpeedNP;
	
	// Motion Control
	ISwitch MotionControlS[3];
	ISwitchVectorProperty MotionControlSP;
	
	// Inverter Port
	ITextVectorProperty PortTP;
	IText PortT[1];

	// Test Function
	ISwitch TestS[1];
	ISwitchVectorProperty TestSP;
	
	// Functions
	void init_properties();
	bool check_drive_connection();

	// Variable
	int connection_status; 
	inverterType type;
	bool simulation;
	
	string type_name;
	string forward_motion;
	string reverse_motion;
	string default_port;
	
	modbus_param_t mb_param;
	
	uint SLAVE_ADDRESS;
	const uint SPEED_MODE_ADDRESS;
	const uint NETWORK_COMMAND_SOURCE_ADDRESS;
	const uint MOTION_CONTROL_ADDRESS;
	const uint DRIVE_ENABLE_ADDRESS;
	const uint REMOTE_ENABLE_ADDRESS;
	const uint FORWARD_ADDRESS;	
        const uint REVERSE_ADDRESS;
	const uint HZ_HOLD_ADDRESS;
	
	// Stop, Forward, Reverse
	uint8_t Motion_Control_Coils[3];
	uint16_t Hz_Speed_Register[2];
	
};
