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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <termios.h>

#include <indicom.h>

#include "encoder.h"
#include "knro.h"

const int ENCODER_READ_BUFFER = 16;
const int ENCODER_ERROR_BUFFER = 128;
const int ENCODER_CMD_LEN = 4;

/* Azimuth TICKs (encoder absolute counter) per degree */
const double AZ_TPD = 202.5;

//**************** WARNING ###################//
//**************** WARNING ###################//
//**************** WARNING ###################//
// TODO ENABLE AGAIN FIXME

// Looking NORTH
const int AZ_HOME_TICKS = 216727;
//**************** WARNING ###################//
//**************** WARNING ###################//
//**************** WARNING ###################//

// Limits to keep wrong values out.
const int AZ_MAX_COUNT = 400000;
const int AZ_MIN_COUNT = 300000;

const double ALT_TPD = 225.9;
//const int ALT_HOME_TICKS = 234465;
const int ALT_HOME_TICKS = 229990;
const double ALT_HOME_DEGREES = 90;

const int ALT_MAX_COUNT = 260000;
const int ALT_MIN_COUNT = 200000;


// 1000ms sleep for encoder thread
const int ENCODER_POLLMS = 10000;
const int SIMULATED_ENCODER_POLLMS = 250000;

/****************************************************************
**
**
*****************************************************************/
knroEncoder::knroEncoder(encoderType new_type, knroObservatory *scope)
{

  connection_status = -1;
  
  debug = false;
  
  telescope = scope;
  simulation = false;
  simulated_forward = true;
  simulated_speed = 0;

  set_type(new_type);

  // As per RS485 AMCI Manual
  // ASCII g
  encoder_command[0] = (char) 0x67 ;
  // Parameter id is one byte to be filled by dispatch_command function
  // Carriage Return
  encoder_command[2] = (char) 0x0D ;
  // Line Feed
  encoder_command[3] = (char) 0x0A ;

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
bool knroEncoder::initProperties()
{

  IUFillNumber(&EncoderAbsPosN[0], "Value" , "", "%g", 0., 16777216., 0., 0.);
  IUFillNumber(&EncoderAbsPosN[1], "Angle" , "", "%.2f", 0., 360., 0., 0.);

   IUFillText(&PortT[0], "PORT", "Port", default_port.c_str());

  if (type == AZ_ENCODER)
  {
        IUFillNumberVector(&EncoderAbsPosNP, EncoderAbsPosN, 2, telescope->getDeviceName(), "Absolute Az", "", ENCODER_GROUP, IP_RO, 0, IPS_OK);
        IUFillTextVector(&PortTP, PortT, 1, telescope->getDeviceName(), "AZIMUTH_ENCODER_PORT", "Azimuth", ENCODER_GROUP, IP_RW, 0, IPS_IDLE);
  }
  else
  {
        IUFillTextVector(&PortTP, PortT, 1, telescope->getDeviceName(), "ALTITUDE_ENCODER_PORT", "Altitude", ENCODER_GROUP, IP_RW, 0, IPS_IDLE);
        IUFillNumberVector(&EncoderAbsPosNP, EncoderAbsPosN, 2, telescope->getDeviceName(), "Absolute Alt", "", ENCODER_GROUP, IP_RO, 0, IPS_OK);

  }

  return true;
  
}

/****************************************************************
**
**
*****************************************************************/
bool knroEncoder::updateProperties(bool connected)
{
    if (connected)
    {
        telescope->defineNumber(&EncoderAbsPosNP);
        telescope->defineText(&PortTP);
    }
    else
    {
        telescope->deleteProperty(EncoderAbsPosNP.name);
        telescope->deleteProperty(PortTP.name);
    }

    return true;
}

/****************************************************************
**
**
*****************************************************************/
void knroEncoder::reset_all_properties()
{
		EncoderAbsPosNP.s 	= IPS_IDLE;
		PortTP.s		= IPS_IDLE;
	
	
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
                        default_port = string("192.168.1.4");
	 }
	 else
	 {
	 		type_name = string("Altitude");
                        default_port = string("192.168.1.5");
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
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION,  "%s Encoder: Simulating connecting to port %s.", type_name.c_str(), PortT[0].text);
        connection_status = 0;
	return true;
    }


    /*if (tty_connect(PortT[0].text, 9600, 8, 0, 1, &fd) != TTY_OK)
    {
	EncoderAbsPosNP.s = IPS_ALERT;
	IDSetNumber (&EncoderAbsPosNP, "Error connecting to port %s. Make sure you have BOTH read and write permission to the port.", PortT[0].text);
	return false;
  }*/

      DEBUGDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "Attempting to communicate with encoder...");

    if (openEncoderServer(default_port.c_str(), 10001) == false)
    {
	    EncoderAbsPosNP.s = IPS_ALERT;
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR, "connection to %s encoder failed. Please insure encoder is online.", type_name.c_str());
        IDSetNumber (&EncoderAbsPosNP, NULL);
	    return false;
    }

    connection_status = 0;
    EncoderAbsPosNP.s = IPS_OK;
    DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s encoder is online. Retrieving positional data...", type_name.c_str());
    IDSetNumber (&EncoderAbsPosNP, NULL);

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
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s Encoder: Simulating encoder init.", type_name.c_str());
   }
   	
   return true;
	
}

/****************************************************************
**
**
*****************************************************************/   
void knroEncoder::disconnect()
{
	connection_status = -1;

    if (simulation)
        return;

	tty_disconnect(sockfd);
	sockfd=-1;
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
     DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "Notice: %s encoder simulation is enabled.", type_name.c_str());
	
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
	  
     DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "Caution: %s encoder simulation is disabled.", type_name.c_str());
	
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
bool knroEncoder::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	
    return true;
	
}

/****************************************************************
**
**
*****************************************************************/
bool knroEncoder::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// Device Port Text
	if (!strcmp(PortTP.name, name))
	{
		if (IUUpdateText(&PortTP, texts, names, n) < 0)
            return false;

		PortTP.s = IPS_OK;
        DEBUGDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "Please reconnect when ready.");
        IDSetText(&PortTP, NULL);

        return true;
	}

    return false;
	
}

/****************************************************************
**
**
*****************************************************************/
bool knroEncoder::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
   return true;
}

bool knroEncoder::dispatch_command(encoderCommand command)
{
   char encoder_error[ENCODER_ERROR_BUFFER];
   encoder_command[1] = command;
   int err_code = 0, nbytes_written =0;

   if  ( (err_code = tty_write(sockfd, encoder_command, ENCODER_CMD_LEN, &nbytes_written) != TTY_OK))
   {
	tty_error_msg(err_code, encoder_error, ENCODER_ERROR_BUFFER);
    DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_WARNING, "TTY error detected: %s\n", encoder_error);
   	return false;
   }

/*   nbytes_written = fwrite(encoder_command, 1, ENCODER_CMD_LEN, fp);*/


   return true;
}

knroEncoder::encoderError knroEncoder::get_encoder_value(encoderCommand command, char * response, double & encoder_value)
{
	unsigned short big_endian_short;
	unsigned int big_endian_int;

	unsigned int encoder_position=0;

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

		memcpy(&big_endian_int, response + 2, 4);
		// Now convert big endian to little endian
        encoder_position = (big_endian_int >>24) | ((big_endian_int <<8) & 0x00FF0000) | ((big_endian_int >>8) & 0x0000FF00) | (big_endian_int<<24);

        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "**** %s encoder ****** - Big Endian INT: %d - Current encoder position is: %d\n", type_name.c_str(), big_endian_int, encoder_position);
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "response[0]: %d , response[1]: %d , response[2]: %d , response[3]: %d\n", response[0], response[1], response[2], response[3]);

		
		#if 0
		// TODO FIXME Enable this back after calibration is done!!!!!
		if (type == AZ_ENCODER)
		{
		  if (encoder_position > AZ_MAX_COUNT || encoder_position < AZ_MIN_COUNT)
		    break;
		}
		else
		{
		  if (encoder_position > ALT_MAX_COUNT || encoder_position < ALT_MIN_COUNT)
		    break;
		}
		
		  //TODO add ALT_ENCODER check

		#endif
		  
		  // Reject ridicislous values
		  if ( (encoder_value != 0) && fabs(encoder_value - encoder_position) > 2000)
		  {
            DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "Rejecting large change. Old value: %g - new Value: %d\n", encoder_value, encoder_position);
            break;
		  }
		encoder_value = encoder_position;

		break;
	}

	return NO_ERROR;
}

void   knroEncoder::update_client(void)
{
	IDSetNumber(&EncoderAbsPosNP, NULL);
}

void * knroEncoder::update_encoder(void)
{
	char encoder_read[ENCODER_READ_BUFFER];
	char encoder_error[ENCODER_ERROR_BUFFER];

        int nbytes_read =0;
        int err_code = 0;
	int counter=0;
	double new_encoder_value=0;

	//IDLog("About to dispatch command for position value\n");

	if (simulation)
	{
		if (type == AZ_ENCODER)
                  EncoderAbsPosN[0].value = 217273;
		else
                  EncoderAbsPosN[0].value = 229670;
	}

	while (1)
	{

	counter=0;
	if (simulation || !check_drive_connection())
	{
		if (simulation)
		{
                                if (simulated_forward)
                                {
                                    if (type == AZ_ENCODER)
                                        EncoderAbsPosN[0].value -= simulated_speed;
                                    else
                                        EncoderAbsPosN[0].value -= simulated_speed;
                                }
                                else
                                {

                                    if (type == AZ_ENCODER)
                                        EncoderAbsPosN[0].value += simulated_speed;
                                    else
                                        EncoderAbsPosN[0].value += simulated_speed;
                                }

				calculate_angle();

				usleep(SIMULATED_ENCODER_POLLMS);

		}

		usleep(ENCODER_POLLMS);
		continue;
	      
        }

	if (dispatch_command(POSITION_VALUE) == false)
	{
        DEBUGDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR,"Error dispatching command to encoder...\n");
        usleep(ENCODER_POLLMS);
		continue;

	}

	for (int i=0; i < ENCODER_READ_BUFFER; i++)
	{
	  if ( (err_code = tty_read(sockfd, encoder_read+counter, 1, 1, &nbytes_read)) != TTY_OK)
	  {
		tty_error_msg(err_code, encoder_error, ENCODER_ERROR_BUFFER);
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR,"%s encoder: TTY error detected (%s)\n", type_name.c_str(), encoder_error);
   		break;
	  }
	   
      DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG,"%s Byte #%d=0x%X --- %d\n", type_name.c_str(), i, ((unsigned char) encoder_read[counter]), ((unsigned char) encoder_read[counter]));

	   // If encountering line feed 0xA, then break;
	   if (encoder_read[counter] == 0xA)
	        break;
	   if (encoder_read[0] == 0x47)
		counter++;
	   
	}

	tcflush(sockfd, TCIOFLUSH);

	if (counter == 0)
	{
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR, "%s encoder. Error, unable to read. Check connection.", type_name.c_str());
                usleep(ENCODER_POLLMS);
		continue;
	}

	if ( ((unsigned char) encoder_read[0]) != 0x47)
	{
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR,"%s encoder. Invalid encoder response!\n", type_name.c_str());
        usleep(ENCODER_POLLMS);
		continue;
	}

	if ( (err_code = get_encoder_value(POSITION_VALUE, encoder_read, new_encoder_value)) != NO_ERROR)
	{
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR,"%s encoder. Encoder error is %d\n", type_name.c_str(), err_code);
        usleep(ENCODER_POLLMS);
		continue;
	}

	if (fabs(EncoderAbsPosN[0].value - new_encoder_value) > ENCODER_NOISE_TOLERANCE)
	{
	    EncoderAbsPosN[0].value = new_encoder_value;
	    calculate_angle();
	}
	
     DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG,"We got encoder test value of %g, Degree %g\n", new_encoder_value, EncoderAbsPosN[1].value);
     usleep(ENCODER_POLLMS);

     }

    return 0;

}

void knroEncoder::calculate_angle()
{
  	if (type == AZ_ENCODER)
	{
            //current_angle = (AZ_HOME_TICKS - EncoderAbsPosN[0].value) / AZ_TPD + 5.1;
            //current_angle = (AZ_HOME_TICKS - EncoderAbsPosN[0].value) / AZ_TPD + 7.63;
        //current_angle = (AZ_HOME_TICKS - EncoderAbsPosN[0].value) / AZ_TPD + 4.53;
        current_angle = (AZ_HOME_TICKS - EncoderAbsPosN[0].value) / AZ_TPD + 2.697;
			if (current_angle > 360) current_angle -= 360;
			else if (current_angle < 0) current_angle += 360;
			EncoderAbsPosN[1].value = current_angle;
	}
	else
	{
            //current_angle = ALT_HOME_DEGREES - fabs((ALT_HOME_TICKS - EncoderAbsPosN[0].value) / ALT_TPD) - 1;
            //current_angle = ALT_HOME_DEGREES - fabs((ALT_HOME_TICKS - EncoderAbsPosN[0].value) / ALT_TPD) - 3.62;
            //current_angle = ALT_HOME_DEGREES - fabs((ALT_HOME_TICKS - EncoderAbsPosN[0].value) / ALT_TPD) - 3.12;
            //current_angle = ALT_HOME_DEGREES - fabs((ALT_HOME_TICKS - EncoderAbsPosN[0].value) / ALT_TPD) + 1;
            current_angle = ALT_HOME_DEGREES - fabs((ALT_HOME_TICKS - EncoderAbsPosN[0].value) / ALT_TPD) + 1.416;
			if (current_angle > ALT_HOME_DEGREES) current_angle -= ALT_HOME_DEGREES;
			else if (current_angle < 0) current_angle += ALT_HOME_DEGREES;
			EncoderAbsPosN[1].value = current_angle;
	}  
}

void * knroEncoder::update_helper(void *context)
{
  return ((knroEncoder *)context)->update_encoder();
}

bool knroEncoder::openEncoderServer (const char * host, int indi_port)
{
	struct sockaddr_in serv_addr;
	struct hostent *hp;

	/* lookup host address */
	hp = gethostbyname (host);
	if (!hp)
	{
	    fprintf (stderr, "gethostbyname(%s): %s\n", host, strerror(errno));
	    return false;
	}

	/* create a socket to the INDI server */
	(void) memset ((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr =  ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
	serv_addr.sin_port = htons(indi_port);
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
	{
	    fprintf (stderr, "socket(%s,%d): %s\n", host, indi_port,strerror(errno));
	    return false;
	}

	/* connect */
	if (::connect (sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
	{
	    fprintf (stderr, "connect(%s,%d): %s\n", host,indi_port,strerror(errno));
	    return false;
	}

    DEBUGDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG,"Successfully connected to encoder server!");

	/* ok */
	return true;
}

