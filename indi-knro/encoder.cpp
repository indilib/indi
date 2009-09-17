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
#include <termios.h>

#include <indicom.h>

#include "encoder.h"

const int ENCODER_READ_BUFFER = 16;
const int ENCODER_ERROR_BUFFER = 128;
const int ENCODER_CMD_LEN = 4;

/****************************************************************
**
**
*****************************************************************/
knroEncoder::knroEncoder(encoderType new_type)
{

  connection_status = -1;
  
  simulation = false;
  
  set_type(new_type);

  // As per RS485 AMCI Manual
  // ASCII g
  encoder_command[0] = (char) 0x67 ;
  // Parameter id is one byte to be filled by dispatch_command function
  // Carriage Return
  encoder_command[2] = (char) 0x0D ;
  // Line Feed
  encoder_command[3] = (char) 0x0A ;

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

  IUFillNumber(&EncoderAbsPosN[0], "Value" , "", "%g", 0., 16777216., 0., 0.);
  IUFillNumber(&EncoderAbsPosN[1], "Angle" , "", "%g", 0., 360., 0., 0.);

   IUFillSwitch(&UpdateCountS[0], "Update Count", "", ISS_OFF);
   IUFillSwitchVector(&UpdateCountSP, UpdateCountS, NARRAY(UpdateCountS), mydev, "Update", "", ENCODER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

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
   IDDefSwitch(&UpdateCountSP, NULL);
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
		PortTP.s		= IPS_IDLE;
		UpdateCountSP.s		= IPS_IDLE;
	}
	
	IDSetNumber(&EncoderAbsPosNP, NULL);
	IDSetText(&PortTP, NULL);
//	IDSetSwitch(&UpdateCountSP, NULL);
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
	
   if (check_drive_connection())
		return true;

    if (simulation)
    {
    	IDMessage(mydev, "%s Encoder: Simulating connecting to port %s.", type_name.c_str(), PortT[0].text);
        connection_status = 0;
	return true;
    }


    if (tty_connect(PortT[0].text, 9600, 8, 0, 1, &fd) != TTY_OK)
    {
	EncoderAbsPosNP.s = IPS_ALERT;
	IDSetNumber (&EncoderAbsPosNP, "Error connecting to port %s. Make sure you have BOTH read and write permission to the port.", PortT[0].text);
	return false;
    }

    connection_status = 0;
    EncoderAbsPosNP.s = IPS_OK;
    IDSetNumber (&EncoderAbsPosNP, "%s encoder is online. Retrieving positional data...", type_name.c_str());

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
		update_encoder_count();
   	
   return true;
	
}

/****************************************************************
**
**
*****************************************************************/   
void knroEncoder::disconnect()
{
	connection_status = -1;
	tty_disconnect(fd);
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
		IDSetText(&PortTP, "Please reconnect when ready.");

		return;
	}
	
}

/****************************************************************
**
**
*****************************************************************/
void knroEncoder::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	if (!strcmp(UpdateCountSP.name, name))
	{
		IUResetSwitch(&UpdateCountSP);
		UpdateCountSP.s = IPS_OK;

		update_encoder_count();

		IDSetSwitch(&UpdateCountSP, NULL);

		return;
	}
}

bool knroEncoder::dispatch_command(encoderCommand command)
{
   char encoder_error[ENCODER_ERROR_BUFFER];
   encoder_command[1] = command;
   int err_code = 0, nbytes_written =0;

   tcflush(fd, TCIOFLUSH);

   if  ( (err_code = tty_write(fd, encoder_command, ENCODER_CMD_LEN, &nbytes_written) != TTY_OK))
   {
	tty_error_msg(err_code, encoder_error, ENCODER_ERROR_BUFFER);
	IDLog("TTY error detected: %s\n", encoder_error);
   	return false;
   }

   return true;
}

knroEncoder::encoderError knroEncoder::get_encoder_value(encoderCommand command, char * response, double & encoder_value)
{
	unsigned short big_endian_short;
	unsigned short big_endian_int;

	unsigned int encoder_position;
	unsigned char my[4];

	// Command Response is as following:
	// Command Code - Paramter Echo - Requested Data  - Error Code - Delimiter
	//    1 byte    -     1 byte    - 1,2, or 4 bytes -   1 byte   -   2 bytes
	switch (command)
	{

		case NUM_OF_TURNS:

		memcpy(&big_endian_short, response + 2, 2);

		// Now convert big endian to little endian
		encoder_value = big_endian_short >> 8 | big_endian_short << 8;

		break;

		case POSITION_VALUE:

		my[0] = response[5];
		my[1] = response[4];
		my[2] = response[3];
		my[3] = response[2];

/*		memcpy(&big_endian_int, response + 2, 4);
		  
		// Now convert big endian to little endian
		encoder_position = (big_endian_int >>24) | ((big_endian_int <<8) & 0x00FF0000) | ((big_endian_int >>8) & 0x0000FF00) | (big_endian_int<<24);*/

		memcpy(&encoder_position, my, 4);

		if (encoder_position > EncoderAbsPosN[0].max)
			break;

		encoder_value = encoder_position;

		break;
	}

	return NO_ERROR;
}

void knroEncoder::update_encoder_count()
{
	if (simulation || !check_drive_connection())
		return;

	char encoder_read[ENCODER_READ_BUFFER];
	char encoder_junk[ENCODER_READ_BUFFER];
	char encoder_error[ENCODER_ERROR_BUFFER];

        int nbytes_read =0;
        int err_code = 0;


	IDLog("About to dispatch command for position value\n");

	if (dispatch_command(POSITION_VALUE) == false)
	{
		IDLog("Error dispatching command to encoder...\n");
		return;
	}

	IDLog("About to READ from encoder\n");

	if ( (err_code = tty_read_section(fd, encoder_read, (char) 0x0A, 5, &nbytes_read)) != TTY_OK)
	{
		tty_error_msg(err_code, encoder_error, ENCODER_ERROR_BUFFER);
		IDLog("TTY error detected: %s\n", encoder_error);
   		return;
	}


	IDLog("Received response from encoder %d bytes long and is #%s#\n", nbytes_read, encoder_read);
	for (int i=0; i < nbytes_read; i++)
		IDLog("Byte #%d=0x%X --- %d\n", i, ((unsigned char) encoder_read[i]), ((unsigned char) encoder_read[i]));


	if ( ((unsigned char) encoder_read[0]) != 0x47)
	{
		//IDLog("Invalid encoder response!\n");
		tcflush(fd, TCIOFLUSH);
		return;
	}

	if ( (err_code = get_encoder_value(POSITION_VALUE, encoder_read, EncoderAbsPosN[0].value)) != NO_ERROR)
	{
		IDLog("Encoder error is %d\n", err_code);
		return;
	}

	tcflush(fd, TCIOFLUSH);

	IDSetNumber(&EncoderAbsPosNP, NULL);

	IDLog("We got encoder test value of %g\n", EncoderAbsPosN[0].value);

}


