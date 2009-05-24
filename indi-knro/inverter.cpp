/*
    Kuwait National Radio Observatory
    INDI driver for Baldor V/Hz Inverter
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

*/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#include <memory>

#include <indicom.h>

#include "inverter.h"

/****************************************************************
**
**
* N.B. Make sure that the stating address are correct since Modbus ref 17, for example, should be addressed as 16.
* Not sure if libmodbus takes care of that or not.
*****************************************************************/
knroInverter::knroInverter(inverterType new_type) : SLAVE_ADDRESS(0x01), SPEED_MODE_ADDRESS(0x03), NETWORK_COMMAND_SOURCE_ADDRESS(0x23),
 MOTION_CONTROL_ADDRESS(0x4E), DRIVE_ENABLE_ADDRESS(0x52), HZ_HOLD_ADDRESS(0x9C4D)
{

  // Initially, not connected
  connection_status = -1;
  
  // Default value
  type = AZ_INVERTER;
  simulation = false;
  
  set_type(new_type);

  init_properties();

}

/****************************************************************
**
**
*****************************************************************/
knroInverter::~knroInverter()
{




}

/****************************************************************
**
**
*****************************************************************/
void knroInverter::set_type(inverterType new_type)
{
  type = new_type;
  
  if (type == AZ_INVERTER)
  {
  	type_name = string("Azimuth");
  	forward_motion = string("East");
  	reverse_motion = string("West");
  	default_port = string("/dev/ttyUSB0");
  }
  else
  {
    type_name = string("Altitude");
    forward_motion = string("Up");
    reverse_motion = string("Down"); 
    default_port = string("/dev/ttyUSB1");
  }
}

/****************************************************************
**
**
*****************************************************************/
void knroInverter::init_properties()
{

    IUFillText(&PortT[0], "PORT", "Port", default_port.c_str());
    IUFillSwitch(&MotionControlS[0], "STOP", "Stop", ISS_OFF);
    IUFillSwitch(&MotionControlS[1], forward_motion.c_str(), "", ISS_OFF);
    IUFillSwitch(&MotionControlS[2], reverse_motion.c_str(), "", ISS_OFF);
  	
    IUFillNumber(&InverterSpeedN[0], "SPEED", "Hz", "%g",  0., 50., 1., 0.);
    
    // Remove Later
    IUFillSwitch(&TestS[0], "Switch to Local", "", ISS_OFF);
    IUFillSwitchVector(&TestSP, TestS, NARRAY(TestS), mydev, string(type_name + string(" test")).c_str(), "",
     INVERTER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    
  if (type == AZ_INVERTER)
  {
  	IUFillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "AZ_INVERTER_PORT", "Az Port", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);
  	
  	IUFillSwitchVector(&MotionControlSP, MotionControlS, NARRAY(MotionControlS), mydev, "AZ_MOTION_CONTROL", "Az Motion", INVERTER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  	
    IUFillNumberVector(&InverterSpeedNP, InverterSpeedN, NARRAY(InverterSpeedN), mydev, "AZ_SPEED" , "Az Speed", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);	
  }
  else
  {
  	IUFillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "ALT_INVERTER_PORT", "Alt Port", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);
  	
  	IUFillSwitchVector(&MotionControlSP, MotionControlS, NARRAY(MotionControlS), mydev, "ALT_MOTION_CONTROL", "Alt Motion", INVERTER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  	
  	IUFillNumberVector(&InverterSpeedNP, InverterSpeedN, NARRAY(InverterSpeedN), mydev, "ALT_SPEED" , "Alt Speed", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);  	
  }
 
}

/****************************************************************
**
**
*****************************************************************/
bool knroInverter::check_drive_connection()
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
bool knroInverter::connect()
{
	
	// 19200 baud is default, no parity, 8 bits, 1 stop bit
	modbus_init_rtu(&mb_param, PortT[0].text, 19200, "none", 8, 1);
	
	// Enable debug
    modbus_set_debug(&mb_param, TRUE);
    
    if (simulation)
    {
    	IDMessage(mydev, "%s drive: Simulating connecting to port %s.", type_name.c_str(), PortT[0].text);
        connection_status = 0;
    }
    else if ( (connection_status = modbus_connect(&mb_param)) == -1)
    {
       IDMessage(mydev, "%s drive: Connection failed to inverter @ port %s", type_name.c_str(), PortT[0].text);
       IDLog("%s drive: Connection failed to inverter @ port %s\n", type_name.c_str(), PortT[0].text);
       return false;
    }
    
	return init_drive();
	
}

/****************************************************************
**
**
*****************************************************************/   
void knroInverter::disconnect()
{
	if (simulation)
		connection_status = -1;
	
	
	
}

/****************************************************************
**
** We're going to test if addressing works as expected
* by placing the drive in local mode. Modbus ref 33 corresponds to
* 0x21. 0x22 is to place drive is remote mode, while 0x20 is reserved.
*****************************************************************/
bool knroInverter::test_address()
{
	
	int ret;
	
	if (!check_drive_connection())
		return false;
		
	ret = force_single_coil(&mb_param, SLAVE_ADDRESS, 0x21, 1);

   if (ret != 1)
   {
     IDLog("Command: Set drive in local mode. ERROR force_single_coil (%d)\n", ret);
     IDLog("Slave = %d, address = %d, value = %d (0x%X)\n", SLAVE_ADDRESS, 0x21, 1, 1);
     return false;
   }
   
   return true;
	
}

/****************************************************************
**
**
*****************************************************************/
bool knroInverter::move_forward()
{
	int ret;
	
	if (!check_drive_connection())
		return false;
	
	Motion_Control_Coils[INVERTER_STOP] = 0;
	Motion_Control_Coils[INVERTER_FORWARD] = 1;
	Motion_Control_Coils[INVERTER_REVERSE] = 0;
	
	if (simulation)
	{
		ret = 3;
		IDMessage(mydev, "%s drive: Simulating forward command.", type_name.c_str());
	}
	else		
		ret = force_multiple_coils(&mb_param, SLAVE_ADDRESS, MOTION_CONTROL_ADDRESS, 3, Motion_Control_Coils);
		
    if (ret != 3)
    {
       IDLog("Forward Command ERROR. force_multiple_coils (%d)\n", ret);
       IDLog("Slave = %d, address = %d, nb = %d\n", SLAVE_ADDRESS, MOTION_CONTROL_ADDRESS, 3);
       return false;
    }

   return true;
   
}

/****************************************************************
**
**
*****************************************************************/
bool knroInverter::move_reverse()
{
	int ret;
	
	if (!check_drive_connection())
		return false;
	
	Motion_Control_Coils[INVERTER_STOP] = 0;
	Motion_Control_Coils[INVERTER_FORWARD] = 0;
	Motion_Control_Coils[INVERTER_REVERSE] = 1;
	
	if (simulation)
	{
		ret = 3;
		IDMessage(mydev, "%s drive: Simulating reverse command.", type_name.c_str());
	}
	else 
		ret = force_multiple_coils(&mb_param, SLAVE_ADDRESS, MOTION_CONTROL_ADDRESS, 3, Motion_Control_Coils);
		
    if (ret != 3)
    {
       IDLog("Reverse Command ERROR. force_multiple_coils (%d)\n", ret);
       IDLog("Slave = %d, address = %d, nb = %d\n", SLAVE_ADDRESS, MOTION_CONTROL_ADDRESS, 3);
       return false;
    }	
	
	return true;
}

/****************************************************************
**
**
*****************************************************************/
bool knroInverter::stop()
{
	int ret;
	
	if (!check_drive_connection())
		return false;
	
	Motion_Control_Coils[INVERTER_STOP] = 1;
	Motion_Control_Coils[INVERTER_FORWARD] = 0;
	Motion_Control_Coils[INVERTER_REVERSE] = 0;
	
	if (simulation)
    {
    	ret = 3;
    	IDMessage(mydev, "%s drive: Simulating stop command.", type_name.c_str());
    }
    else		
		ret = force_multiple_coils(&mb_param, SLAVE_ADDRESS, MOTION_CONTROL_ADDRESS, 3, Motion_Control_Coils);
	
    if (ret != 3)
    {
       IDLog("Stop Command ERROR force_multiple_coils (%d)\n", ret);
       IDLog("Slave = %d, address = %d, nb = %d\n", SLAVE_ADDRESS, MOTION_CONTROL_ADDRESS, 3);
       return false;
    }
    
    return true;
	
}

/****************************************************************
**
**
*****************************************************************/   
bool knroInverter::set_speed(float newHz)
{
	int ret;
	
	if (!check_drive_connection())
		return false;
	
	if (newHz < 0. || newHz > 50.)
	{
		IDLog("set_speed: newHz %g is outside boundary limits (0,50) Hz", newHz);
		return false;
	}
		
	union
    {
         float input;   // assumes sizeof(float) == sizeof(int)
         int   output;
    }    data;
	
    data.input = newHz;
    
    bitset<32>   bits(data.output);
    bitset<32>   mask(0xFFFF0000);

    Hz_Speed_Register[0] = 0;
    Hz_Speed_Register[1] = ((bits & mask) >> 16).to_ulong();
    
    cerr << "Speed bits after processing are: " << bits << endl;
    IDLog("Hz_Speed_Register[0] = %d - Hz_Speed_Register[1] = %d\n", Hz_Speed_Register[0], Hz_Speed_Register[1]);
      
    if (simulation)
    {
    	ret = 2;
    	IDMessage(mydev, "%s drive: Simulating set speed command. High register value: 0x%X", type_name.c_str(), Hz_Speed_Register[1]);
	}
	else
		ret = preset_multiple_registers(&mb_param, SLAVE_ADDRESS, HZ_HOLD_ADDRESS, 2, Hz_Speed_Register);
		
    if (ret != 2)
    {
      IDLog("set_speed ERROR! preset_multiple_registers (%d)\n", ret);
      IDLog("Slave = %d, address = %d, nb = %d\n", SLAVE_ADDRESS, HZ_HOLD_ADDRESS, 2);
      return false;
    }                    

    return true;
	
}

/****************************************************************
**
**
*****************************************************************/   
bool knroInverter::enable_drive()
{
	
	int ret;
	
	if (!check_drive_connection())
		return false;
			
	stop();

	if (simulation)
	{
		ret = 1;
		IDMessage(mydev, "%s drive: Simulating enabling drive.", type_name.c_str());
	}
	else
		ret = force_single_coil(&mb_param, SLAVE_ADDRESS, DRIVE_ENABLE_ADDRESS, 1);
	
	if (ret != 1)
   {
     IDLog("Command: Enable Drive. ERROR force_single_coil (%d)\n", ret);
     IDLog("Slave = %d, address = %d, value = %d (0x%X)\n", SLAVE_ADDRESS, DRIVE_ENABLE_ADDRESS, 1, 1);
     return false;
   }
   
   return true;
	
}

/****************************************************************
**
**
*****************************************************************/   
bool knroInverter::disable_drive()
{
	int ret;
	
	if (!check_drive_connection())
		return false;
				
	stop();

	if (simulation)
	{
		ret = 1;
		IDMessage(mydev, "%s drive: Simulating disabling drive.", type_name.c_str());
	}
	else
		ret = force_single_coil(&mb_param, SLAVE_ADDRESS, DRIVE_ENABLE_ADDRESS, 0);
	
	if (ret != 1)
   {
     IDLog("Command: Disable Drive. ERROR force_single_coil (%d)\n", ret);
     IDLog("Slave = %d, address = %d, value = %d (0x%X)\n", SLAVE_ADDRESS, DRIVE_ENABLE_ADDRESS, 0, 0);
     return false;
   }
	
	return true;
}

/****************************************************************
**
**
*****************************************************************/   
bool knroInverter::init_drive()
{
	int ret;
	
	if (!check_drive_connection())
		return false;
		   
   // Ensure that drive is disabled first	
   disable_drive();
   
   // Enable speed mode
   if (simulation)
   {
		ret = 1;
		IDMessage(mydev, "%s drive: Simulating setting motion mode to SPEED.", type_name.c_str());
   }
   else
   	ret = force_single_coil(&mb_param, SLAVE_ADDRESS, SPEED_MODE_ADDRESS, 1);
                    
   if (ret != 1)
   {
     IDLog("Command: Enable Speed Mode. ERROR force_single_coil (%d)\n", ret);
     IDLog("Slave = %d, address = %d, value = %d (0x%X)\n", SLAVE_ADDRESS, SPEED_MODE_ADDRESS, 1, 1);
     return false;
   }
   
   
  // Set Command Source
  // The inverter operates in network mode. We ask the VS1SP to read the command source data from the registers values corresponding to the current operating mode
  // which is network. This will enable the drive to pull the target frequency (Hz) value from the holding registers.
  if (simulation)
   {
		ret = 1;
		IDMessage(mydev, "%s drive: Simulating setting command source for Network registers.", type_name.c_str());
   }
   else
  		ret = force_single_coil(&mb_param, SLAVE_ADDRESS, NETWORK_COMMAND_SOURCE_ADDRESS, 1);
                    
   if (ret != 1)
   {
     IDLog("Command: Set Network Command Source. ERROR force_single_coil (%d)\n", ret);
     IDLog("Slave = %d, address = %d, value = %d (0x%X)\n", SLAVE_ADDRESS, NETWORK_COMMAND_SOURCE_ADDRESS, 1, 1);
     return false;
   }
                                     

   // Now the drive is ready to be used
   return enable_drive();
   
}
    
/****************************************************************
**
**
*****************************************************************/    
void knroInverter::ISGetProperties()
{
   IDDefSwitch(&MotionControlSP, NULL);
   IDDefNumber(&InverterSpeedNP, NULL);
   IDDefText(&PortTP, NULL);
   IDDefSwitch(&TestSP, NULL);
}

/****************************************************************
**
**
*****************************************************************/
void knroInverter::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	if (!strcmp(InverterSpeedNP.name, name))
	{
		if (set_speed(values[0]))
		{
			if (IUUpdateNumber(&InverterSpeedNP, values, names, n) < 0)
				return;
				
				InverterSpeedNP.s = IPS_OK;
				IDSetNumber(&InverterSpeedNP, "%s drive speed updated to %g Hz.", type_name.c_str(), InverterSpeedN[0].value);
		}
		else
		{
				InverterSpeedNP.s = IPS_ALERT;
				IDSetNumber(&InverterSpeedNP, "Error: could not update speed for %s drive.", type_name.c_str());
		}

		return;
	}
	
}

/****************************************************************
**
**
*****************************************************************/
void knroInverter::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// Device Port Text
	if (!strcmp(PortTP.name, name))
	{
		if (IUUpdateText(&PortTP, texts, names, n) < 0)
			return;

		PortTP.s = IPS_OK;
		IDSetText(&PortTP, NULL);
		return;
	}
}

/****************************************************************
**
**
*****************************************************************/
void knroInverter::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	if (!strcmp(TestSP.name, name))
	{
		IUResetSwitch(&TestSP);
		
		
		if (test_address())
		{
			TestSP.s = IPS_OK;
			IDSetSwitch(&TestSP, "Test function called. Check Local/Remote light on the %s inverter", type_name.c_str());
		}
		else
		{
			TestSP.s = IPS_ALERT;
			IDSetSwitch(&TestSP, "Test function failed. Check driver log for more details.");
		}
		
		return;
	}
		
	if (!strcmp(MotionControlSP.name, name))
	{
		if (IUUpdateSwitch(&MotionControlSP, states, names, n) < 0)
			return;
			
		if (MotionControlS[INVERTER_STOP].s == ISS_ON)
		{
			if (stop() != false)
			{
				MotionControlSP.s = IPS_OK;
				IDSetSwitch(&MotionControlSP, "%s motion stopped", type_name.c_str());
			}
			else
			{
				MotionControlSP.s = IPS_ALERT;
				IDSetSwitch(&MotionControlSP, "Error stopping motion for %s drive", type_name.c_str());
			}
		}
		else if (MotionControlS[INVERTER_FORWARD].s == ISS_ON)
		{
			if (move_forward() != false)
			{
				MotionControlSP.s = IPS_BUSY;
				IDSetSwitch(&MotionControlSP, "%s drive is moving %s", type_name.c_str(), forward_motion.c_str());
			}
			else
			{
				MotionControlSP.s = IPS_ALERT;
				IUResetSwitch(&MotionControlSP);
				MotionControlS[INVERTER_STOP].s = ISS_ON;
				IDSetSwitch(&MotionControlSP, "Error: %s drive failed to move %s", type_name.c_str(), forward_motion.c_str());
			}
		}
		else if (MotionControlS[INVERTER_REVERSE].s == ISS_ON)
		{
			if (move_reverse() != false)
			{
				MotionControlSP.s = IPS_BUSY;
				IDSetSwitch(&MotionControlSP, "%s drive is moving %s", type_name.c_str(), reverse_motion.c_str());
			}
			else
			{
				MotionControlSP.s = IPS_ALERT;
				IUResetSwitch(&MotionControlSP);
				MotionControlS[INVERTER_STOP].s = ISS_ON;
				IDSetSwitch(&MotionControlSP, "Error: %s drive failed to move %s", type_name.c_str(), reverse_motion.c_str());
			}
		}
     }
     
}

/****************************************************************
**
**
*****************************************************************/
void knroInverter::reset_all_properties(bool reset_to_idle)
{
	
	if (reset_to_idle)
	{
		MotionControlSP.s = IPS_IDLE;
		InverterSpeedNP.s = IPS_IDLE;
		PortTP.s 		  = IPS_IDLE;
		
		// Remove Later
		TestSP.s		  = IPS_IDLE;
	}
	
	IUResetSwitch(&MotionControlSP);
	IDSetSwitch(&MotionControlSP, NULL);
	IDSetNumber(&InverterSpeedNP, NULL);
	IDSetText(&PortTP, NULL);
	
	// Remove later
	IDSetSwitch(&TestSP, NULL);
	
}

/****************************************************************
**
**
*****************************************************************/
void knroInverter::enable_simulation ()
{
	if (simulation)
		return;
		
	 simulation = true;
	 IDMessage(mydev, "Notice: %s drive simulation is enabled.", type_name.c_str());
	 IDLog("Notice: %s drive simulation is enabled.\n", type_name.c_str());
}

/****************************************************************
**
**
*****************************************************************/
void knroInverter::disable_simulation()
{
	if (!simulation) 
		return;
		
	 // Disconnect
	 disconnect();
	 
	 simulation = false;
	  
	 IDMessage(mydev, "Caution: %s drive simulation is disabled.", type_name.c_str());
	 IDLog("Caution: %s drive simulation is disabled.\n", type_name.c_str());
}
    
