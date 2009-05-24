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

*/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#include <indicom.h>

#include "encoder.h"

/****************************************************************
**
**
*****************************************************************/
knroEncoder::knroEncoder(encoderType new_type)
{

  // Default value
  type = AZ_ENCODER;

  connection_status = -1;
  
  simulation = false;
  
  set_type(new_type);

  init_properties();



}

/****************************************************************
**
**
*****************************************************************/
knroEncoder::~knroEncoder()
{




}

/****************************************************************
**
**
*****************************************************************/
void knroEncoder::init_properties()
{

  IUFillNumber(&EncoderAbsPosN[0], "Value" , "", "%d", 0., 500000., 0., 0.);
  IUFillNumber(&EncoderAbsPosN[1], "Angle" , "", "%g", 0., 360., 0., 0.);

  IUFillText(&PortT[0], "PORT", "Port", default_port.c_str());

  if (type == AZ_ENCODER)
  {
    	IUFillNumberVector(&EncoderAbsPosNP, EncoderAbsPosN, NARRAY(EncoderAbsPosN), mydev, "Absolute Az", "", ENCODER_GROUP, IP_RO, 0, IPS_OK);
    	IUFillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "AZIMUTH_ENCODER_PORT", "Azimuth", ENCODER_GROUP, IP_RW, 0, IPS_IDLE);
  }
  else
  {
    	IUFillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "ALTITUDE_ENCODER_PORT", "Altitude", ENCODER_GROUP, IP_RW, 0, IPS_IDLE);
    	IUFillNumberVector(&EncoderAbsPosNP, EncoderAbsPosN, NARRAY(EncoderAbsPosN), mydev, "Absolute Alt", "", ENCODER_GROUP, IP_RO, 0, IPS_OK);
  }
   
}

/****************************************************************
**
**
*****************************************************************/
void knroEncoder::ISGetProperties()
{
   IDDefNumber(&EncoderAbsPosNP, NULL);
   IDDefText(&PortTP, NULL);
}

/****************************************************************
**
**
*****************************************************************/
void knroEncoder::reset_all_properties(bool reset_to_idle)
{
	if (reset_to_idle)
	{
		EncoderAbsPosNP.s 	= IPS_IDLE;
		PortTP.s			= IPS_IDLE;
	}
	
	IDSetNumber(&EncoderAbsPosNP, NULL);
	IDSetText(&PortTP, NULL);
	
}

/****************************************************************
**
**
*****************************************************************/
void knroEncoder::set_type(encoderType new_type)
{
	 type = new_type;
	 
	 if (type == AZ_ENCODER)
	 {
	 		type_name = string("Azimuth");
	 		default_port = string("/dev/ttyUSB0");
	 }
	 else
	 {
	 		type_name = string("Altitude");
	 		default_port = string("/dev/ttyUSB1");
	 }		
}

/****************************************************************
**
**
*****************************************************************/   
bool knroEncoder::connect()
{
	
    if (simulation)
    {
    	IDMessage(mydev, "%s Encoder: Simulating connecting to port %s.", type_name.c_str(), PortT[0].text);
        connection_status = 0;
    }
    /*else if ( (connection_status = modbus_connect(&mb_param)) == -1)
    {
       IDMessage(mydev, "%s drive: Connection failed to inverter @ port %s", type_name.c_str(), PortT[0].text);
       IDLog("%s drive: Connection failed to inverter @ port %s\n", type_name.c_str(), PortT[0].text);
       return false;
    }*/
    
	return init_encoder();
	
}

/****************************************************************
**
**
*****************************************************************/   
bool knroEncoder::init_encoder()
{
	if (!check_drive_connection())
		return false;
		   
   // Enable speed mode
   if (simulation)
   {
		IDMessage(mydev, "%s Encoder: Simulating encoder init.", type_name.c_str());
   }
   else
   //FIXME add real code
   		return false;
   		
   	return true;
	
}

/****************************************************************
**
**
*****************************************************************/   
void knroEncoder::disconnect()
{
	if (simulation)
		connection_status = -1;
}

/****************************************************************
**
**
*****************************************************************/
void knroEncoder::enable_simulation ()
{
	if (simulation)
		return;
		
	 simulation = true;
	 IDMessage(mydev, "Notice: %s encoder simulation is enabled.", type_name.c_str());
	 IDLog("Notice: %s drive encoder is enabled.\n", type_name.c_str());
}

/****************************************************************
**
**
*****************************************************************/
void knroEncoder::disable_simulation()
{
	if (!simulation) 
		return;
		
	 // Disconnect
	 disconnect();
	 
	 simulation = false;
	  
	 IDMessage(mydev, "Caution: %s encoder simulation is disabled.", type_name.c_str());
	 IDLog("Caution: %s drive encoder is disabled.\n", type_name.c_str());
}
    
/****************************************************************
**
**
*****************************************************************/
bool knroEncoder::check_drive_connection()
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
void knroEncoder::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	
	
}

/****************************************************************
**
**
*****************************************************************/
void knroEncoder::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
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
void knroEncoder::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	
	
	
	
}
