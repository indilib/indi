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

#include <indidevapi.h>
#include <indicom.h>

#include "knro_common.h"

class knroInverter
{

public:

    enum inverterType { AZ_INVERTER, ALT_INVERTER };

	knroInverter(inverterType new_type);
	~knroInverter();

      void ISGetProperties();

      void move_forward();
      void move_reverse();
      void stop();
      
      void set_peed(uint newHz);
      uint get_speed() { return speed; }
      
      void set_type(inverterType new_type) { type = new_type; }
      inverterType get_type() { return type; }

     
private: 

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

	// Functions
	void init();

	// Variable 
	uint speed;
	inverterType type;
	
};
