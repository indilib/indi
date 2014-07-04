#if 0
    INDI Ujari Driver - Hitachi Inverter
    Copyright (C) 2014 Jasem Mutlaq
    
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


#define INVERTER_GROUP "Inverters"

pthread_mutex_t dome_inverter_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t shutter_inverter_mutex = PTHREAD_MUTEX_INITIALIZER;

// Wait 500ms between updates
#define MAX_THREAD_WAIT     200000

const int ERROR_MAX_COUNT = 3;
const double FREQ_DIFF_LIMIT = 0.5;
const int ERROR_TIMEOUT = 200000;

/****************************************************************
**
**
* N.B. Make sure that the stating address are correct since Modbus ref 17, for example, should be addressed as 16.
* Not sure if libmodbus takes care of that or not.
*****************************************************************/
/*Inverter::Inverter(inverterType new_type, Ujari *scope) : OPERATION_COMMAND_ADDRESS(0x01),
    DIRECTION_COMMAND_ADDRESS(0x02), FREQ_SOURCE_ADDRESS(0x01), INVERTER_STATUS_ADDRESS(0x45),
    FREQ_OUTPUT_ADDRESS(0x1001)*/

Inverter::Inverter(inverterType new_type, Ujari *scope) : OPERATION_COMMAND_ADDRESS(0x0),
    DIRECTION_COMMAND_ADDRESS(0x1), FREQ_SOURCE_ADDRESS(0x00), INVERTER_STATUS_ADDRESS(0x44),
    FREQ_OUTPUT_ADDRESS(0x1000)
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

  mb_param = NULL;

  memset(Inverter_Status_Coils, 0, sizeof(Inverter_Status_Coils));
  
  set_type(new_type);


}

/****************************************************************
**
**
*****************************************************************/
Inverter::~Inverter()
{
	disconnect();
    pthread_join(inverter_thread, NULL);
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
  default_port = string("172.16.15.4");

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
    IUFillNumber(&OutputFreqN[0], "Freq", "Hz", "%g",  0., 50., 1., 0.);

    IUFillLight(&StatusL[0], "Ready", "", IPS_IDLE);
    IUFillLight(&StatusL[1], "Forward", "", IPS_IDLE);
    IUFillLight(&StatusL[2], "Reverse", "", IPS_IDLE);
       
  if (type == DOME_INVERTER)
  {
    IUFillTextVector(&PortTP, PortT, NARRAY(PortT), telescope->getDeviceName(), "DOME_INVERTER_PORT", "Dome Port", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);
  	
    IUFillSwitchVector(&MotionControlSP, MotionControlS, NARRAY(MotionControlS), telescope->getDeviceName(), "DOME_MOTION_CONTROL", "Dome Motion", INVERTER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  	
    IUFillNumberVector(&InverterSpeedNP, InverterSpeedN, NARRAY(InverterSpeedN), telescope->getDeviceName(), "DOME_SPEED" , "Dome Speed", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);
    IUFillNumberVector(&OutputFreqNP, OutputFreqN, NARRAY(OutputFreqN), telescope->getDeviceName(), "DOME_FREQ" , "Dome Freq", INVERTER_GROUP, IP_RO, 0, IPS_IDLE);

    IUFillLightVector(&StatusLP, StatusL, NARRAY(StatusL), telescope->getDeviceName(), "DOME_INVERTER_STATUS", "Dome Status", INVERTER_GROUP, IPS_IDLE);
  }
  else
  {
    IUFillTextVector(&PortTP, PortT, NARRAY(PortT), telescope->getDeviceName(), "SHUTTER_INVERTER_PORT", "Shutter Port", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);
  	
    IUFillSwitchVector(&MotionControlSP, MotionControlS, NARRAY(MotionControlS), telescope->getDeviceName(), "SHUTTER_MOTION_CONTROL", "Shutter Motion", INVERTER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  	
    IUFillNumberVector(&InverterSpeedNP, InverterSpeedN, NARRAY(InverterSpeedN), telescope->getDeviceName(), "SHUTTER_SPEED" , "Shutter Speed", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);
    IUFillNumberVector(&OutputFreqNP, OutputFreqN, NARRAY(OutputFreqN), telescope->getDeviceName(), "SHUTTER_FREQ" , "Shutter Freq", INVERTER_GROUP, IP_RO, 0, IPS_IDLE);

    IUFillLightVector(&StatusLP, StatusL, NARRAY(StatusL), telescope->getDeviceName(), "SHUTTER_INVERTER_STATUS", "Shutter Status", INVERTER_GROUP, IPS_IDLE);
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
bool Inverter::isDriveOnline()
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
	if (isDriveOnline())
		return true;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating connecting to port %s.", type_name.c_str(), PortT[0].text);
        connection_status = 0;
        return true;
    }
	   
    mb_param = modbus_new_tcp(PortT[0].text, 502);   
    modbus_set_slave(mb_param, SLAVE_ADDRESS);
    //modbus_set_debug(mb_param, debug ? TRUE : FALSE);

    if ( (connection_status = modbus_connect(mb_param)) == -1)
    {
       DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR, "%s drive: Connection failed to inverter @ port %s", type_name.c_str(), PortT[0].text);
       return false;
    }

    modbus_set_error_recovery(mb_param, (modbus_error_recovery_mode) (MODBUS_ERROR_RECOVERY_LINK | MODBUS_ERROR_RECOVERY_PROTOCOL));

    stop();

    update_status();

    if (is_ready())
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "%s inverter is online and ready for use.", type_name.c_str());
        return true;
    }
    else
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "%s inverter ready check failed. Check logs for errors.", type_name.c_str());
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
void * Inverter::update_helper(void *context)
{
    ((Inverter *)context)->update();
    return 0;
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::update()
{

    while (connection_status != -1)
    {
        lock_mutex();

        update_status();
        update_freq();

        if (MotionControlSP.s == IPS_ALERT)
        {
            if (StatusL[1].s == IPS_BUSY)
            {
                IUResetSwitch(&MotionControlSP);
                MotionControlS[1].s = ISS_ON;
                MotionControlSP.s = IPS_BUSY;
            }
            else if (StatusL[2].s == IPS_BUSY)
            {
                IUResetSwitch(&MotionControlSP);
                MotionControlS[2].s = ISS_ON;
                MotionControlSP.s = IPS_BUSY;
            }
            else
            {
                IUResetSwitch(&MotionControlSP);
                MotionControlS[0].s = ISS_ON;
                MotionControlSP.s = IPS_IDLE;
            }

        IDSetSwitch(&MotionControlSP, NULL);
        }

        if (fabs(OutputFreqN[0].value - InverterSpeedN[0].value) <= FREQ_DIFF_LIMIT)
             OutputFreqNP.s = IPS_OK;

        IDSetNumber(&OutputFreqNP, NULL);
        IDSetLight(&StatusLP, NULL);

        unlock_mutex();

       usleep(MAX_THREAD_WAIT);
    }

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::update_status()
{
    int ret=0;

    memset(Inverter_Status_Coils, 0, sizeof(Inverter_Status_Coils));

    if (simulation == false)
        modbus_flush(mb_param);

    for (int i=0; i < ERROR_MAX_COUNT; i++)
    {
       if (simulation)
       {
           ret = 3;
           Inverter_Status_Coils[0]=1;
           Inverter_Status_Coils[1] = (motionStatus == INVERTER_FORWARD) ? 1 : 0;
           Inverter_Status_Coils[2] = (motionStatus == INVERTER_REVERSE) ? 1 : 0;
       }
       else
            ret = modbus_read_bits(mb_param, INVERTER_STATUS_ADDRESS, 3, Inverter_Status_Coils);

       if (ret == 3)
       {

           //DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Inverter_Status_Coils (%d) (%d) (%d)", Inverter_Status_Coils[0], Inverter_Status_Coils[1],
             //      Inverter_Status_Coils[2]);
            StatusL[0].s = (Inverter_Status_Coils[0] == 1) ? IPS_OK : IPS_ALERT;
            StatusL[1].s = (Inverter_Status_Coils[1] == 1) ? IPS_BUSY : IPS_IDLE;
            StatusL[2].s = (Inverter_Status_Coils[2] == 1) ? IPS_BUSY : IPS_IDLE;
            StatusLP.s = IPS_OK;
            //IDSetLight(&StatusLP, NULL);
            return true;
       }


       //usleep(ERROR_TIMEOUT);
     }

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "%s: Inverter Status Command ERROR (%s). modbus_read_bits ret=%d", type_name.c_str(), modbus_strerror(ret), ret);
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Slave = %d, address = 0x%X, nb = %d", SLAVE_ADDRESS, INVERTER_STATUS_ADDRESS, 3);

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Failed to updated status of %s inverter", type_name.c_str());
    StatusLP.s = IPS_ALERT;
    //IDSetLight(&StatusLP, NULL);

    return false;
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::update_freq()
{
    int ret=0;
    if (!isDriveOnline())
        return false;

    /* Read to 1001h (D001) High order and 1002h (D001) Low Order registers */
    uint16_t Output_Speed_Register[2];

    if (simulation)
    {
        OutputFreqN[0].value = reqFreq;
        return true;
    }

    modbus_flush(mb_param);

    for (int i=0; i < ERROR_MAX_COUNT; i++)
    {
        ret = modbus_read_registers(mb_param, FREQ_OUTPUT_ADDRESS, 2, Output_Speed_Register);

        if (ret == 2)
        {
            double outputFreq = (Output_Speed_Register[0] << 16) | Output_Speed_Register[1];
            OutputFreqN[0].value = outputFreq / 100.0;
            //IDSetNumber(&OutputFreqNP, NULL);
            return true;
        }

        //usleep(ERROR_TIMEOUT);
  }


  DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "%s read_speed ERROR (%s) read  holding_registers (%d)", type_name.c_str(), modbus_strerror(ret), ret);
  DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG,"Slave = %d, address = 0x%X, nb = %d", SLAVE_ADDRESS, FREQ_OUTPUT_ADDRESS, 2);
  DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG,"Error: could not update speed for %s drive.", type_name.c_str());
  return false;

}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::is_ready()
{
    return (Inverter_Status_Coils[0] == 1);
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::moveForward()
{
    int ret=0;

	if (!isDriveOnline())
		return false;
	
	// Already moving forward
    if (motionStatus == INVERTER_FORWARD)
	    return true;

    Motion_Control_Coils[INVERTER_OPERATION] = MODBUS_RUN;
    Motion_Control_Coils[INVERTER_DIRECTION] = MODBUS_FORWARD;

	if (simulation)
	{
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating forward command.", type_name.c_str());
        motionStatus = INVERTER_FORWARD;
		return true;
     }

    modbus_flush(mb_param);

    for (int i=0; i < ERROR_MAX_COUNT; i++)
    {
       // For some reason, cannot write two bits to Hitachi, so one bit at a time!
       //ret = modbus_write_bits(mb_param, OPERATION_COMMAND_ADDRESS, 2, Motion_Control_Coils);

        // First Forward
        ret = modbus_write_bit(mb_param, DIRECTION_COMMAND_ADDRESS, 0);

       if (ret == 1)
       {
          // Second RUN
          ret = modbus_write_bit(mb_param, OPERATION_COMMAND_ADDRESS, 1);

          if (ret == 1)
          {

            if (verbose)
                DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "%s drive is moving %s", type_name.c_str(), forward_motion.c_str());            

            motionStatus = INVERTER_FORWARD;

            MotionControlSP.s = IPS_BUSY;
            IDSetSwitch(&MotionControlSP, NULL);

            return true;
          }
       }

       //usleep(ERROR_TIMEOUT);
     }


    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Forward Command ERROR (%s). force_multiple_coils (%d)", modbus_strerror(ret), ret);
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Slave = %d, address = 0x%X, nb = %d\n", SLAVE_ADDRESS, OPERATION_COMMAND_ADDRESS, 2);

    Motion_Control_Coils[0] = 0;
    Motion_Control_Coils[1] = 0;
    MotionControlSP.s = IPS_ALERT;
    IUResetSwitch(&MotionControlSP);
    MotionControlS[INVERTER_STOP].s = ISS_ON;

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR,  "Error: %s drive failed to move %s", type_name.c_str(), forward_motion.c_str());
    IDSetSwitch(&MotionControlSP, NULL);
    return false;

}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::moveReverse()
{
    int ret=0;
    if (!isDriveOnline())
        return false;

    // Already moving forward
    if (motionStatus == INVERTER_REVERSE)
        return true;

    Motion_Control_Coils[INVERTER_OPERATION] = MODBUS_RUN;
    Motion_Control_Coils[INVERTER_DIRECTION] = MODBUS_REVERSE;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating reverse command.", type_name.c_str());
        motionStatus = INVERTER_REVERSE;
        return true;
    }

    modbus_flush(mb_param);

    for (int i=0; i < ERROR_MAX_COUNT; i++)
    {
       // First Direction - REV
       ret = modbus_write_bit(mb_param, DIRECTION_COMMAND_ADDRESS, 1);

       if (ret == 1)
       {
           // Second RUN
           ret = modbus_write_bit(mb_param, OPERATION_COMMAND_ADDRESS, 1);

           if (ret == 1)
           {

            if (verbose)
                DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "%s drive is moving %s", type_name.c_str(), reverse_motion.c_str());

            motionStatus = INVERTER_REVERSE;

            MotionControlSP.s = IPS_BUSY;
            IDSetSwitch(&MotionControlSP, NULL);

            return true;
           }
       }

       //usleep(ERROR_TIMEOUT);
     }

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Reverse Command ERROR (%s). force_multiple_coils (%d)", modbus_strerror(ret), ret);
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Slave = %d, address = 0x%X, nb = %d", SLAVE_ADDRESS, OPERATION_COMMAND_ADDRESS, 2);

    Motion_Control_Coils[0] = 0;
    Motion_Control_Coils[1] = 0;
    MotionControlSP.s = IPS_ALERT;
    IUResetSwitch(&MotionControlSP);
    MotionControlS[INVERTER_STOP].s = ISS_ON;

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR,  "Error: %s drive failed to move %s", type_name.c_str(), reverse_motion.c_str());
    IDSetSwitch(&MotionControlSP, NULL);
    return false;
}

/****************************************************************
**
**
*****************************************************************/
bool Inverter::stop()
{
    int ret=0;

    if (!isDriveOnline())
        return false;

    // Already moving forward
    //if (motionStatus == INVERTER_STOP)
        //return true;

    Motion_Control_Coils[INVERTER_OPERATION] = MODBUS_STOP;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating stop command.", type_name.c_str());
        motionStatus = INVERTER_STOP;
        return true;
     }

    modbus_flush(mb_param);

    for (int i=0; i < ERROR_MAX_COUNT; i++)
    {
       ret = modbus_write_bit(mb_param, OPERATION_COMMAND_ADDRESS, 0);

       if (ret == 1)
       {           
            if (verbose)
                DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "%s drive stoppped.", type_name.c_str());

            IDSetSwitch(&MotionControlSP, NULL);

            MotionControlSP.s = IPS_IDLE;
            motionStatus = INVERTER_STOP;

            return true;
       }

       //usleep(ERROR_TIMEOUT);
     }

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Stop Command ERROR (%s). force_multiple_coils (%d)", modbus_strerror(ret), ret);
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Slave = %d, address = 0x%X, nb = %d", SLAVE_ADDRESS, OPERATION_COMMAND_ADDRESS, 1);
    Motion_Control_Coils[0] = 0;
    Motion_Control_Coils[1] = 0;
    MotionControlSP.s = IPS_ALERT;
    IUResetSwitch(&MotionControlSP);

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR,  "Error: %s drive failed to stop", type_name.c_str());
    IDSetSwitch(&MotionControlSP, NULL);
    return false;
}

/****************************************************************
**
**
*****************************************************************/   
bool Inverter::setSpeed(float newHz)
{
    int ret=0;
	if (!isDriveOnline())
		return false;
	
	if (newHz < 0. || newHz > 50.)
	{
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "setSpeed: newHz %g is outside boundary limits (0,50) Hz", newHz);
		return false;
    }

    int invFreq = newHz * 100;

    /* Write to 0001h (F001) High order and 0002h (F001) Low Order registers */
    Hz_Speed_Register[0] = (invFreq >> 16) & 0xFFFF;
    Hz_Speed_Register[1] = invFreq & 0xFFFF;

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Requested Speed is: %f", newHz);
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "Hz_Speed_Register[0] = %X - Hz_Speed_Register[1] = %X", Hz_Speed_Register[0], Hz_Speed_Register[1]);

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating set speed command.", type_name.c_str());
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "%s drive speed updated to %g Hz.", type_name.c_str(), InverterSpeedN[0].value);
        return true;
    }

    modbus_flush(mb_param);

    for (int i=0; i < ERROR_MAX_COUNT; i++)
    {

    ret = modbus_write_registers(mb_param, FREQ_SOURCE_ADDRESS, 2, Hz_Speed_Register);

    if (ret == 2)
    {
        Hz_Speed_Register[0] = 0;
        Hz_Speed_Register[1] = 0;

        for (int j=0; i < ERROR_MAX_COUNT; j++)
        {
            ret = modbus_read_registers(mb_param, FREQ_SOURCE_ADDRESS, 2, Hz_Speed_Register);
            if (ret == 2)
            {
               DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "** READING ** Hz_Speed_Register[0] = %X - Hz_Speed_Register[1] = %X", Hz_Speed_Register[0], Hz_Speed_Register[1]);

                if (verbose)
                    DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive speed updated to %g Hz.", type_name.c_str(), InverterSpeedN[0].value);

                return true;
            }

           //usleep(ERROR_TIMEOUT);
       }

        return true;
    }

       //usleep(ERROR_TIMEOUT);
  }

    DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "setSpeed ERROR (%s) read or write holding_registers (%d)", modbus_strerror(ret), ret);
    DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG,"Slave = %d, address = 0x%X, nb = %d", SLAVE_ADDRESS, FREQ_SOURCE_ADDRESS, 2);
  DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR,"Error: could not update speed for %s drive.", type_name.c_str());
  return false;
	
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
        telescope->defineNumber(&OutputFreqNP);
        telescope->defineLight(&StatusLP);

        int err = pthread_create(&inverter_thread, NULL, &Inverter::update_helper, this);
        if (err != 0)
        {
            DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR, "%s encoder: Can't create inverter thread (%s)", type_name.c_str(), strerror(err));
            return false;
        }
    }
    else
    {
        telescope->deleteProperty(MotionControlSP.name);
        telescope->deleteProperty(InverterSpeedNP.name);
        telescope->deleteProperty(OutputFreqNP.name);
        telescope->deleteProperty(StatusLP.name);

        pthread_join(inverter_thread, NULL);
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
        bool rc = setSpeed(values[0]);

        if (rc)
        {
            IUUpdateNumber(&InverterSpeedNP, values, names, n);
            reqFreq = InverterSpeedN[0].value;
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

        lock_mutex();

		if (IUUpdateSwitch(&MotionControlSP, states, names, n) < 0)
        {
            unlock_mutex();
            return false;
        }
			
		if (MotionControlS[INVERTER_STOP].s == ISS_ON)
          rc = stop();
		else if (MotionControlS[INVERTER_FORWARD].s == ISS_ON)
          rc = moveForward();
		else if (MotionControlS[INVERTER_REVERSE].s == ISS_ON)
           rc = moveReverse();

        if (rc)
        {
            MotionControlSP.s = IPS_BUSY;
            if (MotionControlS[INVERTER_STOP].s == ISS_ON)
                     MotionControlSP.s = IPS_OK;

            if (MotionControlSP.s == IPS_BUSY)
                OutputFreqNP.s = IPS_BUSY;
            else
                OutputFreqNP.s = IPS_IDLE;

            IDSetNumber(&OutputFreqNP, NULL);

        }
        else
        {
            IUResetSwitch(&MotionControlSP);
            MotionControlS[INVERTER_STOP].s = ISS_ON;
            MotionControlSP.s = IPS_ALERT;
        }

        IDSetSwitch(&MotionControlSP, NULL);

        unlock_mutex();

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
    OutputFreqNP.s    = IPS_IDLE;
	PortTP.s 		  = IPS_IDLE;
    StatusLP.s        = IPS_IDLE;

	IUResetSwitch(&MotionControlSP);
	IDSetSwitch(&MotionControlSP, NULL);
	IDSetNumber(&InverterSpeedNP, NULL);
    IDSetNumber(&OutputFreqNP, NULL);
	IDSetText(&PortTP, NULL);
    IDSetLight(&StatusLP, NULL);
	
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

/****************************************************************
**
**
*****************************************************************/
void Inverter::setDebug(bool enable)
{
    debug = enable;
    //if (mb_param != NULL)
        //modbus_set_debug(mb_param, debug ? TRUE : FALSE);
}

/****************************************************************
**
**
*****************************************************************/
void Inverter::lock_mutex()
{
    switch (type)
    {
        case DOME_INVERTER:
          pthread_mutex_lock( &dome_inverter_mutex );
          break;

       case SHUTTER_INVERTER:
            pthread_mutex_lock( &shutter_inverter_mutex );
            break;

    }
}

/****************************************************************
**
**
*****************************************************************/
void Inverter::unlock_mutex()
{
    switch (type)
    {
        case DOME_INVERTER:
          pthread_mutex_unlock( &dome_inverter_mutex );
          break;

       case SHUTTER_INVERTER:
          pthread_mutex_unlock( &shutter_inverter_mutex );
          break;
    }
}
