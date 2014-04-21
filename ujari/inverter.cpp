#if 0
    INDI Ujari Driver - Inverter
    Copyright (C) 2014 Jasem Mutlaq

    Based on EQMod INDI Driver by Jean-Luc
    
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

#endif

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
#include "ujari.h"


// bits in Status Data
#define STATUS_SetF 		0x00
#define STATUS_OutF			0x01
#define STATUS_OutA 		0x02
#define STATUS_RoTT 		0x03
#define STATUS_DCV 			0x04
#define STATUS_ACV 			0x05
#define STATUS_Cont 		0x06
#define STATUS_Tmp 			0x07

// control commands CNTR
#define CONTROL_Run_Fwd		0x01
#define CONTROL_Run_Rev		0x11
#define CONTROL_Stop		0x08

// control responses CNST
#define CONTROL_Run			0x01
#define CONTROL_Jog			0x02
#define CONTROL_Command_rf	0x04
#define CONTROL_Running		0x08
#define CONTROL_Jogging		0x10
#define CONTROL_Running_rf	0x20
#define CONTROL_Bracking	0x40
#define CONTROL_Track_Start	0x80

#define INVERTER_GROUP "Inverter"

const int ERROR_MAX_COUNT = 3;

const int ERROR_TIMEOUT = 2000000;

/****************************************************************
**
**
* N.B. Make sure that the stating address are correct since Modbus ref 17, for example, should be addressed as 16.
* Not sure if libmodbus takes care of that or not.
*****************************************************************/
Inverter::Inverter(inverterType new_type, Ujari *scope)
{

  // Initially, not connected
  connection_status = -1;
  
  telescope = scope;

  // Default value
  type = DOME_INVERTER;
  
  debug = false;
  simulation = false;
  verbose    = true;
  motionStatus = INVERTER_STOP;
  
  set_type(new_type);


}

/****************************************************************
**
**
*****************************************************************/
Inverter::~Inverter()
{
	disconnect();
}

/****************************************************************
**
**
*****************************************************************/
void Inverter::set_type(inverterType new_type)
{
  type = new_type;
  
  forward_motion = string("Forward");
  reverse_motion = string("Reverse");
  default_port = string("192.168.0.20");

  if (type == DOME_INVERTER)
  {
    type_name = string("Dome");
	SLAVE_ADDRESS = 1;
  }
  else
  {
    type_name = string("Shutter");
    SLAVE_ADDRESS = 2;
  }
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::initProperties()
{

    IUFillText(&PortT[0], "PORT", "Port", default_port.c_str());
    IUFillSwitch(&MotionControlS[0], "STOP", "Stop", ISS_OFF);
    IUFillSwitch(&MotionControlS[1], forward_motion.c_str(), "", ISS_OFF);
    IUFillSwitch(&MotionControlS[2], reverse_motion.c_str(), "", ISS_OFF);
  	
    IUFillNumber(&InverterSpeedN[0], "SPEED", "Hz", "%g",  0., 50., 1., 0.);
       
  if (type == DOME_INVERTER)
  {
    IUFillTextVector(&PortTP, PortT, NARRAY(PortT), telescope->getDeviceName(), "DOME_INVERTER_PORT", "Dome Port", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);
  	
    IUFillSwitchVector(&MotionControlSP, MotionControlS, NARRAY(MotionControlS), telescope->getDeviceName(), "DOME_MOTION_CONTROL", "Dome Motion", INVERTER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  	
    IUFillNumberVector(&InverterSpeedNP, InverterSpeedN, NARRAY(InverterSpeedN), telescope->getDeviceName(), "DOME_SPEED" , "Dome Speed", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);
  }
  else
  {
    IUFillTextVector(&PortTP, PortT, NARRAY(PortT), telescope->getDeviceName(), "SHUTTER_INVERTER_PORT", "Shutter Port", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);
  	
    IUFillSwitchVector(&MotionControlSP, MotionControlS, NARRAY(MotionControlS), telescope->getDeviceName(), "SHUTTER_MOTION_CONTROL", "Shutter Motion", INVERTER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  	
    IUFillNumberVector(&InverterSpeedNP, InverterSpeedN, NARRAY(InverterSpeedN), telescope->getDeviceName(), "SHUTTER_SPEED" , "Shutter Speed", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);
  }

  return true;
 
}


/****************************************************************
**
**
*****************************************************************/
void Inverter::ISGetProperties()
{
   telescope->defineText(&PortTP);
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::check_drive_connection()
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
bool Inverter::connect()
{
	if (check_drive_connection())
		return true;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating connecting to port %s.", type_name.c_str(), PortT[0].text);
        connection_status = 0;
        return true;
    }
	
	// 19200 baud is default, no parity, 8 bits, 1 stop bit
	//modbus_init_rtu(&mb_param, SLAVE_ADDRESS, PortT[0].text, 19200, "none", 8, 1);
	//modbus_init_tcp(&mb_param, PortT[0].text, 502, SLAVE_ADDRESS);
    //mb_param = modbus_new_tcp(PortT[0].text, 502);
    //modbus_set_slave(mb_param, SLAVE_ADDRESS);
	
	// Enable debug
    //modbus_set_debug(mb_param, FALSE);

    modbus_init_rtu(&mb_param, PortT[0].text, 10001, telescope);
    mb_param.debug = 1;
    mb_param.print_errors = 1;
    
    if ( (connection_status = modbus_connect(&mb_param)) != 0)
    {
       DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Connection failed to inverter @ port %s", type_name.c_str(), PortT[0].text);
       return false;
    }
    
    if (init_drive())
    {
		MotionControlSP.s = IPS_OK;
		IDSetSwitch(&MotionControlSP, "%s inverter is online and ready for use.", type_name.c_str());
		return true;
    }
    else
    {
		MotionControlSP.s = IPS_ALERT;
		IDSetSwitch(&MotionControlSP, "%s inverter failed to initialize. Please check power and cabling.", type_name.c_str());
		return false;
    }
}

/****************************************************************
**
**
*****************************************************************/   
void Inverter::disconnect()
{
    connection_status = -1;

    if (simulation)
        return;
	
    modbus_close(&mb_param);
	
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::move_forward()
{
    int retval=0;

	if (!check_drive_connection())
		return false;
	
	// Already moving forward
    if (motionStatus == INVERTER_FORWARD)
	    return true;
	
	if (simulation)
	{
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating forward command.", type_name.c_str());
		return true;
     }

    mb_data.function = WRITE_CONTROL_DATA;
    mb_data.parameter = 0x00;
    mb_data.data = CONTROL_Run_Fwd;

    if ((retval = hy_modbusN(&mb_param, &mb_data)) != 0)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "Error: %s drive failed to move %s (%s)", type_name.c_str(), forward_motion.c_str(), modbus_strerror(retval));
        return false;
    }

    motionStatus = INVERTER_FORWARD;

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "%s drive is moving %s", type_name.c_str(), forward_motion.c_str());

    return true;

}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::move_reverse()
{
    int retval=0;

    if (!check_drive_connection())
        return false;

    // Already moving forward
    if (motionStatus == INVERTER_REVERSE)
        return true;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating reverse command.", type_name.c_str());
        return true;
     }

    mb_data.function = WRITE_CONTROL_DATA;
    mb_data.parameter = 0x00;
    mb_data.data = CONTROL_Run_Rev;

    if ((retval = hy_modbusN(&mb_param, &mb_data)) != 0)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "Error: %s drive failed to move %s (%s)", type_name.c_str(), reverse_motion.c_str(), modbus_strerror(retval));
        return false;
    }

    motionStatus = INVERTER_REVERSE;

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "%s drive is moving %s", type_name.c_str(), reverse_motion.c_str());

    return true;

}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::stop()
{
    int retval=0;

    if (!check_drive_connection())
        return false;

    // Already moving forward
    if (motionStatus == INVERTER_STOP)
        return true;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating stop command.", type_name.c_str());
        return true;
     }

    mb_data.function = WRITE_CONTROL_DATA;
    mb_data.parameter = 0x00;
    mb_data.data = CONTROL_Stop;

    if ((retval = hy_modbusN(&mb_param, &mb_data)) != 0)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "Error: %s drive failed to stop (%s)", type_name.c_str(),  modbus_strerror(retval));
        return false;
    }

    motionStatus = INVERTER_STOP;

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "%s drive stopped.", type_name.c_str());

    return true;


}

/****************************************************************
**
**
*****************************************************************/   
bool Inverter::set_speed(float newHz)
{
    int retval=0;
	
	if (!check_drive_connection())
		return false;
	
	if (newHz < 0. || newHz > 50.)
	{
		IDLog("set_speed: newHz %g is outside boundary limits (0,50) Hz", newHz);
		return false;
    }

    /*
    mb_data->function = WRITE_FREQ_DATA;
    mb_data->parameter = 0x00;

    hzcalc = 50.0 / *(haldata->rated_motor_rev);
    freq =  abs((int)(*(haldata->speed_command)*hzcalc*100));
    */

    // NOTE: Do I need to scale it to rated_motor_rev ??!
    int speedHz = abs(newHz * 100);

    mb_data.function = WRITE_FREQ_DATA;
    mb_data.parameter = 0x00;
    mb_data.data = speedHz;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating set speed command.", type_name.c_str());
        return true;
    }

    if ((retval = hy_modbusN(&mb_param, &mb_data)) != 0)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR, "%s driver: Error setting speed %g (%s)", type_name.c_str(), newHz, modbus_strerror(retval));
        return false;
    }

   return true;
	
}

/****************************************************************
**
**
*****************************************************************/   
bool Inverter::init_drive()
{

    mb_data.slave = SLAVE_ADDRESS;

   return true;
   
}
    
/****************************************************************
**
**
*****************************************************************/    
bool Inverter::updateProperties(bool connected)
{
    if (connected)
    {
        telescope->defineSwitch(&MotionControlSP);
        telescope->defineNumber(&InverterSpeedNP);
        telescope->defineText(&PortTP);
    }
    else
    {
        telescope->deleteProperty(MotionControlSP.name);
        telescope->deleteProperty(InverterSpeedNP.name);
        telescope->deleteProperty(PortTP.name);
    }
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	if (!strcmp(InverterSpeedNP.name, name))
	{
        bool rc = set_speed(values[0]);

        if (rc)
        {
            IUUpdateNumber(&InverterSpeedNP, values, names, n);
            InverterSpeedNP.s = IPS_OK;
        }
        else
        {
            InverterSpeedNP.s = IPS_ALERT;
        }

        IDSetNumber(&InverterSpeedNP, NULL);
		
        return true;
	}

    return false;
	
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// Device Port Text
	if (!strcmp(PortTP.name, name))
	{
		if (IUUpdateText(&PortTP, texts, names, n) < 0)
            return false;

		PortTP.s = IPS_OK;
		IDSetText(&PortTP, "Please reconnect when ready.");

        return true;
	}

    return false;
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
		
	if (!strcmp(MotionControlSP.name, name))
	{
        bool rc = false;

		if (IUUpdateSwitch(&MotionControlSP, states, names, n) < 0)
            return false;
			
		if (MotionControlS[INVERTER_STOP].s == ISS_ON)
          rc = stop();
		else if (MotionControlS[INVERTER_FORWARD].s == ISS_ON)
          rc = move_forward();
		else if (MotionControlS[INVERTER_REVERSE].s == ISS_ON)
           rc = move_reverse();

        if (rc)
        {
            MotionControlSP.s = IPS_BUSY;
            if (MotionControlS[INVERTER_STOP].s == ISS_ON)
                     MotionControlSP.s = IPS_OK;
        }
        else
        {
            IUResetSwitch(&MotionControlSP);
            MotionControlS[INVERTER_STOP].s = ISS_ON;
            MotionControlSP.s = IPS_ALERT;
        }

        IDSetSwitch(&MotionControlSP, NULL);

        return true;
     }

    return false;
     
}

/****************************************************************
**
**
*****************************************************************/
void Inverter::reset_all_properties()
{
	
	MotionControlSP.s = IPS_IDLE;
	InverterSpeedNP.s = IPS_IDLE;
	PortTP.s 		  = IPS_IDLE;

	IUResetSwitch(&MotionControlSP);
	IDSetSwitch(&MotionControlSP, NULL);
	IDSetNumber(&InverterSpeedNP, NULL);
	IDSetText(&PortTP, NULL);
	
}

/****************************************************************
**
**
*****************************************************************/
void Inverter::setSimulation(bool enable)
{
    simulation = enable;
		 
    DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive simulation is %s.", type_name.c_str(), enable ? "Enabled" : "Disabled");
}

bool Inverter::is_in_motion()
{
  if (motionStatus != INVERTER_STOP)
    return true;
  else
    return false;
}

    




