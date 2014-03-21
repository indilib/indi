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

#define INVERTER_GROUP "Inverter"

const int ERROR_MAX_COUNT = 3;
//const int ERROR_TIMEOUT = 100000;

const int ERROR_TIMEOUT = 2000000;

/****************************************************************
**
**
* N.B. Make sure that the stating address are correct since Modbus ref 17, for example, should be addressed as 16.
* Not sure if libmodbus takes care of that or not.
*****************************************************************/
Inverter::Inverter(inverterType new_type, Ujari *scope) : WRITE_ADDRESS(0x02), CONTROL_ADDRESS(0x03), READ_ADDRESS(0x04)
{

  // Initially, not connected
  connection_status = -1;
  
  telescope = scope;

  // Default value
  type = DOME_INVERTER;
  
  debug = false;
  simulation = false;
  verbose    = true;
  mb_param = NULL;
  MotionCommand[0] = 0;
  
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
  default_port = string("192.168.1.3");

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
	mb_param = modbus_new_tcp(PortT[0].text, 502);
	modbus_set_slave(mb_param, SLAVE_ADDRESS);
	
	// Enable debug
    modbus_set_debug(mb_param, FALSE);
    
    if ( (connection_status = modbus_connect(mb_param)) == -1)
    {
       DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Connection failed to inverter @ port %s", type_name.c_str(), PortT[0].text);
       if (debug)
	 IDLog("%s drive: Connection failed to inverter @ port %s\n", type_name.c_str(), PortT[0].text);
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
	
	modbus_close(mb_param);
	modbus_free(mb_param);
	
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::move_forward()
{
	int ret;
	
	if (!check_drive_connection())
		return false;
	
	// Already moving forward
    if (MotionCommand[0] == INVERTER_FORWARD)
	    return true;
	
    MotionCommand[0] = INVERTER_FORWARD;

	if (simulation)
	{
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating forward command.", type_name.c_str());
		MotionControlSP.s = IPS_BUSY;
		IDSetSwitch(&MotionControlSP, "%s drive is moving %s", type_name.c_str(), forward_motion.c_str());
		return true;
     }

	for (int i=0; i < ERROR_MAX_COUNT; i++)
	{
       ret = modbus_write_bits(mb_param, CONTROL_ADDRESS, 1, MotionCommand);
	
       if (ret == 1)
	   {
            MotionControlSP.s = IPS_BUSY;

            if (verbose)
                IDSetSwitch(&MotionControlSP, "%s drive is moving %s", type_name.c_str(), forward_motion.c_str());
            else
                IDSetSwitch(&MotionControlSP, NULL);
		    return true;
 	   }

	   usleep(ERROR_TIMEOUT);
     }
	   
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Forward Command ERROR. force_multiple_coils (%d)\n", ret);
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Slave = %d, address = %d, nb = %d\n", SLAVE_ADDRESS, WRITE_ADDRESS, 1);

    MotionCommand[0] = 0;
	MotionControlSP.s = IPS_ALERT;
	IUResetSwitch(&MotionControlSP);
    MotionControlS[0].s = ISS_ON;
	IDSetSwitch(&MotionControlSP, "Error: %s drive failed to move %s", type_name.c_str(), forward_motion.c_str());
	return false;
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::move_reverse()
{
    int ret;

    if (!check_drive_connection())
        return false;

    // Already moving backwards
    if (MotionCommand[0] == INVERTER_REVERSE)
        return true;

    MotionCommand[0] = INVERTER_REVERSE;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating reverse command.", type_name.c_str());
        MotionControlSP.s = IPS_BUSY;
        IDSetSwitch(&MotionControlSP, "%s drive is moving %s", type_name.c_str(), reverse_motion.c_str());
        return true;
     }

    for (int i=0; i < ERROR_MAX_COUNT; i++)
    {
       ret = modbus_write_bits(mb_param, CONTROL_ADDRESS, 1, MotionCommand);

       if (ret == 1)
       {
            MotionControlSP.s = IPS_BUSY;

            if (verbose)
                IDSetSwitch(&MotionControlSP, "%s drive is moving %s", type_name.c_str(), reverse_motion.c_str());
            else
                IDSetSwitch(&MotionControlSP, NULL);
            return true;
       }

       usleep(ERROR_TIMEOUT);
     }

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Forward Command ERROR. force_multiple_coils (%d)\n", ret);
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Slave = %d, address = %d, nb = %d\n", SLAVE_ADDRESS, WRITE_ADDRESS, 1);

    MotionCommand[0] = 0;
    MotionControlSP.s = IPS_ALERT;
    IUResetSwitch(&MotionControlSP);
    MotionControlS[0].s = ISS_ON;
    IDSetSwitch(&MotionControlSP, "Error: %s drive failed to move %s", type_name.c_str(), reverse_motion.c_str());
    return false;

}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::stop()
{
    int ret;

    if (!check_drive_connection())
        return false;

    // Already moving backwards
    if (MotionCommand[0] == INVERTER_STOP)
        return true;

    MotionCommand[0] = INVERTER_STOP;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating stop command.", type_name.c_str());
        MotionControlSP.s = IPS_IDLE;
        IDSetSwitch(&MotionControlSP, "%s drive is stopped", type_name.c_str());
        return true;
     }

    for (int i=0; i < ERROR_MAX_COUNT; i++)
    {
       ret = modbus_write_bits(mb_param, CONTROL_ADDRESS, 1, MotionCommand);

       if (ret == 1)
       {
            MotionControlSP.s = IPS_BUSY;

            if (verbose)
                IDSetSwitch(&MotionControlSP, "%s drive is stopped", type_name.c_str());
            else
                IDSetSwitch(&MotionControlSP, NULL);
            return true;
       }

       usleep(ERROR_TIMEOUT);
     }

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Stop Command ERROR. force_multiple_coils (%d)\n", ret);
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Slave = %d, address = %d, nb = %d\n", SLAVE_ADDRESS, WRITE_ADDRESS, 1);

    MotionCommand[0] = 0;
    MotionControlSP.s = IPS_ALERT;
    IUResetSwitch(&MotionControlSP);
    MotionControlS[0].s = ISS_ON;
    IDSetSwitch(&MotionControlSP, "Error: %s drive failed to stop", type_name.c_str());
    return false;

}

/****************************************************************
**
**
*****************************************************************/   
bool Inverter::set_speed(float newHz)
{
	int ret;
	
	if (!check_drive_connection())
		return false;
	
	if (newHz < 0. || newHz > 50.)
	{
		IDLog("set_speed: newHz %g is outside boundary limits (0,50) Hz", newHz);
		return false;
    }

    unsigned short speedHz = newHz * 100;

    // Set Frequency (0) FUNC
    Hz_Speed_Register[0] = 0;
    // High Byte
    Hz_Speed_Register[1] = speedHz >> 8;
    // Low Byte
    Hz_Speed_Register[2] = speedHz & 0xFF;

    
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Requested Speed is %g", newHz);
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Hz_Speed_Register[1] = 0x%X - Hz_Speed_Register[2] = 0xX", Hz_Speed_Register[1], Hz_Speed_Register[2]);
    
    if (simulation)
    {
    DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating set speed command.", type_name.c_str());
	InverterSpeedN[0].value = newHz;
    InverterSpeedNP.s = IPS_OK;
    if (verbose)
        IDSetNumber(&InverterSpeedNP, "%s drive speed updated to %g Hz.", type_name.c_str(), InverterSpeedN[0].value);
    else
        IDSetNumber(&InverterSpeedNP, NULL);
	return true;
    }

	for (int i=0; i < ERROR_MAX_COUNT; i++)
	{

    ret = modbus_write_bits(mb_param, WRITE_ADDRESS, 3, Hz_Speed_Register);
	
    if (ret == 3)
	{
	    Hz_Speed_Register[0] = 0;
        Hz_Speed_Register[1] = 0;
        Hz_Speed_Register[2] = 0;

	    for (int j=0; i < ERROR_MAX_COUNT; j++)
   	    {
            modbus_write_bits(mb_param, READ_ADDRESS, 1, 0);
            ret = modbus_read_bits(mb_param, READ_ADDRESS, 2, Hz_Speed_Register);
		    if (ret == 2)
		    {
               DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "** READING ** Hz_Speed_Register[0] = 0x%X - Hz_Speed_Register[1] = 0x%X", Hz_Speed_Register[0], Hz_Speed_Register[1]);
    
			InverterSpeedN[0].value = newHz;
            InverterSpeedNP.s = IPS_OK;
            if (verbose)
                DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive speed updated to %g Hz.", type_name.c_str(), InverterSpeedN[0].value);

            IDSetNumber(&InverterSpeedNP, NULL);
			return true;
		    }

		   usleep(ERROR_TIMEOUT);
	   }
       }
       usleep(ERROR_TIMEOUT);
      }

      DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "set_speed ERROR! read or write holding_registers (%d)\n", ret);
      DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG,"Slave = %d, address = %d, nb = %d\n", SLAVE_ADDRESS, READ_ADDRESS, 2);
      InverterSpeedNP.s = IPS_ALERT;
      DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR,"Error: could not update speed for %s drive.", type_name.c_str());
      IDSetNumber(&InverterSpeedNP, NULL);
      return false;
	
}

/****************************************************************
**
**
*****************************************************************/   
bool Inverter::init_drive()
{

    // TODO

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
		set_speed(values[0]);
		
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
		if (IUUpdateSwitch(&MotionControlSP, states, names, n) < 0)
            return false;
			
		if (MotionControlS[INVERTER_STOP].s == ISS_ON)
		  stop();
		else if (MotionControlS[INVERTER_FORWARD].s == ISS_ON)
		  move_forward();
		else if (MotionControlS[INVERTER_REVERSE].s == ISS_ON)
		{
			if (move_reverse() != false)
			{
				MotionControlSP.s = IPS_BUSY;
                if (verbose)
                    DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive is moving %s", type_name.c_str(), reverse_motion.c_str());

                IDSetSwitch(&MotionControlSP, NULL);
			}
			else
			{
				MotionControlSP.s = IPS_ALERT;
				IUResetSwitch(&MotionControlSP);
				MotionControlS[INVERTER_STOP].s = ISS_ON;
                DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR, "Error: %s drive failed to move %s", type_name.c_str(), reverse_motion.c_str());
                IDSetSwitch(&MotionControlSP, NULL);
			}
		}

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
  if (MotionCommand[0] == INVERTER_FORWARD || MotionCommand[0] == INVERTER_REVERSE)
    return true;
  else
    return false;
}

    




