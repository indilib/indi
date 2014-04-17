/*      hy_modbus.c
 
   By S.Alford
 
   Modified by Jasem Mutlaq for Ujari Observatory

   These library of functions are designed to enable a program send and
   receive data from a Huanyang VFD. This device does not use a standard
   Modbus function code or data structure.
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                    
 
   This code has its origins with libmodbus.
 
*/  
  
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "hy_modbus.h"   
#include "ujari.h"
 

 /* Table of CRC values for high-order byte */
static unsigned char table_crc_hi[] = {
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
  0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
  0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
  0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
  0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 
  0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
  0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 
  0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
  0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
  0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
  0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 
  0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
  0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};

/* Table of CRC values for low-order byte */
static unsigned char table_crc_lo[] = {
  0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 
  0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 
  0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 
  0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 
  0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4, 
  0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3, 
  0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 
  0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4, 
  0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 
  0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 
  0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED, 
  0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26, 
  0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 
  0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 
  0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 
  0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 
  0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 
  0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5, 
  0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 
  0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 
  0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C, 
  0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 
  0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B, 
  0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C, 
  0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 
  0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};  

/************************************************************************* 
 
	Function to treat comms errors
	
**************************************************************************/
static void error_treat(modbus_param_t *mb_param, int code, const char *string)
{
	if (!mb_param->print_errors)
		return;
	
    DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"ERROR %s (%d)", string, code);
	

}


/************************************************************************* 
 
	Fast CRC function
	
**************************************************************************/

static unsigned short crc16(unsigned char *buffer,
			    unsigned short buffer_length)
{
  unsigned char crc_hi = 0xFF; /* high CRC byte initialized */
  unsigned char crc_lo = 0xFF; /* low CRC byte initialized */
  unsigned int i; /* will index into CRC lookup */

  /* pass through message buffer */
  while (buffer_length--) {
    i = crc_hi ^ *buffer++; /* calculate the CRC  */
    crc_hi = crc_lo ^ table_crc_hi[i];
    crc_lo = table_crc_lo[i];
  }

  return (crc_hi << 8 | crc_lo);
}

/************************************************************************* 
 
	Check CRC function, returns 0 else returns INVALID_CRC
	
**************************************************************************/

static int check_crc16(modbus_param_t *mb_param, uint8_t *msg,
                       const int msg_length)
{
	int ret;
	uint16_t crc_calc;
	uint16_t crc_received;
			
	crc_calc = crc16(msg, msg_length - 2);
	crc_received = (msg[msg_length - 2] << 8) | msg[msg_length - 1];
	
	/* Check CRC of msg */
	if (crc_calc == crc_received) {
			ret = 0;
	} else {
			char s_error[64];
            DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,s_error,
					"invalid crc received %0X - crc_calc %0X", 
					crc_received, crc_calc);
			ret = INVALID_CRC;
			error_treat(mb_param, ret, s_error);
	}

	return ret;
}




/************************************************************************* 
 
	Function to compute the response length based on function code within
	the query sent to the VFD
	
**************************************************************************/

static unsigned int compute_response_length(modbus_param_t *mb_param, 
					  uint8_t *query)
{
	int resp_length;

	switch (query[1])
	{
		case 0x01:
		/* Huanyang VFD - Function Read */
		
		case 0x02:
		/* Huanyang VFD - Function Write */
		resp_length = 8;
		break;
		
		case 0x03:
		/* Huanyang VFD - Write Control Data */
		resp_length = 6;
		break;
		
		case 0x04:
		/* Huanyang VFD - Read Control Data */
		resp_length = 8;
		break;	 
		
		case 0x05:
        /* Huanyang VFD - Write Inverter Frequency Data */
		resp_length = 7;
		break;	
		
		case 0x06:
		/* Huanyang VFD - Reserved */
		
		case 0x07:
		/* Huanyang VFD - Reserved */		
		return -1;
        break;
		
		case 0x08:
        /* Huanyang VFD - Loop Test (not implemented)*/
		return -1;
        break;
		
		default:
        return -1;
		break;
	}

  return resp_length;
} 
  
/************************************************************************* 
 
	Function to add a checksum to the end of a query and send. 
 
**************************************************************************/  
  
static int modbus_send(modbus_param_t *mb_param, uint8_t *query, int query_length )  
{  
	int ret;
	unsigned short s_crc;
	int i;
	
	unsigned char data;
  
	
	/* calculate the CRC */
	s_crc = crc16(query, query_length);
      
    /* append the CRC to then end of the query */
	query[query_length++] = s_crc >> 8;  
    query[query_length++] = s_crc & 0x00FF;  
    
	if (mb_param->debug)
	{
        DEBUGDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"Modbus query = ");
		for (i = 0; i < query_length; i++)
            DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"[%.2X]", query[i]);
	}
	
	/* write the query to the fd */
	ret = write(mb_param->fd, query, query_length);

	/* Return the number of bytes written (0 to n)
	or PORT_SOCKET_FAILURE on error */
	if ((ret == -1) || (ret != query_length))
	{
		error_treat(mb_param, ret, "Write port/socket failure");
		ret = PORT_FAILURE;
	}
	
	return ret;
}  



  

/*********************************************************************** 
 
	Definintion to be used multiple times in receive_msg function
	
***********************************************************************/ 
  
  
#define WAIT_DATA()                                                                \
{                                                                                  \
    while ((select_ret = select(mb_param->fd+1, &rfds, NULL, NULL, &tv)) == -1) {  \
            if (errno == EINTR) {                                                  \
                    DEBUGDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"A non blocked signal was caught");                   \
                    /* Necessary after an error */                                 \
                    FD_ZERO(&rfds);                                                \
                    FD_SET(mb_param->fd, &rfds);                                   \
            } else {                                                               \
                    error_treat(mb_param, SELECT_FAILURE, "Select failure");       \
                    return SELECT_FAILURE;                                         \
            }                                                                      \
    }                                                                              \
                                                                                   \
    if (select_ret == 0) {                                                         \
            DEBUGDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"WAIT_DATA(): comms time out");   			                \
            /* Call to error_treat is done later to manage exceptions */           \
            return COMM_TIME_OUT;                                                  \
    }                                                                              \
}  

  
/*********************************************************************** 
 
	Function to monitor for the reply from the modbus slave. 
	This function blocks for timeout seconds if there is no reply. 
 
	Returns a negative number is an error occured.
	The variable msg_length is assigned th number of characters
	received.
	
***********************************************************************/  
  
int receive_msg(modbus_param_t *mb_param, int msg_length_computed,
						uint8_t *msg, int *msg_length)
{  
  	int select_ret;
	int read_ret;
	fd_set rfds;  
	struct timeval tv;
	int length_to_read;
	unsigned char *p_msg;

	unsigned char read_data;
	int i;
	
	if (mb_param->debug)
        DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"waiting for message (%d bytes)...\n",
				msg_length_computed);

	/* add a file descriptor to the set */
	FD_ZERO(&rfds);  
    FD_SET(mb_param->fd, &rfds); 
	
	/* wait for a response */
	tv.tv_sec = 0;
	tv.tv_usec = TIME_OUT_BEGIN_OF_FRAME; 

	length_to_read = msg_length_computed;
	if (mb_param->debug) {
        DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"length to read = %d", length_to_read);
	}
						
	select_ret = 0;
	WAIT_DATA();
   
	//if (mb_param->debug) {
    //	DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"we received something on the port");
    //	DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"\n");
	//}
			
	/* read the message */
    (*msg_length) = 0;
    p_msg = msg;	
		  
	while (select_ret) /* loop to receive data until end of msg	or time out */
	{
		read_ret = read(mb_param->fd, p_msg, length_to_read);
		if (mb_param->debug) {
            DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"read return = [%.2X] bytes",read_ret);
		}
		
		if (read_ret == -1) {
            error_treat(mb_param, PORT_SOCKET_FAILURE,
                        "Read port/socket failure");
            return PORT_SOCKET_FAILURE;
		}
		
		/* sum bytes received */
		(*msg_length) += read_ret;
		if (mb_param->debug) {
            DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"msg_length = [%.2X]", *msg_length);
		}
		
		/* Display the hex code of each character received */
		if (mb_param->debug) {
			int i;
            DEBUGDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"characters received =");
			for (i= 0; i < read_ret; i++)
                DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"[%.2X]", p_msg[i]);

		}
		
		if ((*msg_length) < msg_length_computed) {
			/* We can receive a shorter message than msg_length_computed as
		  		some functions return one byte in the data feild. Check against
				the received data length stored in p_msg[2] */
			if ( *msg_length > 3 )
			{
				if ( *msg_length == msg[ 2 ] + 5 )
				{
					/* we have received the whole message */
					length_to_read = 0;
				}
			} else {			
				/* Message is incomplete */
				length_to_read = msg_length_computed - (*msg_length);
		 	
		 		if (mb_param->debug) {
                DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"message was incomplete, length still to read = [%.2X]", length_to_read);
				}
			}
		} else {
			length_to_read = 0;
		}
		
		/* Moves the pointer to receive other data */
        p_msg = &(p_msg[read_ret]);

		if (length_to_read > 0) {
			/* If no character at the buffer wait
			   TIME_OUT_END_OF_TRAME before to generate an error. */
			tv.tv_sec = 0;
			tv.tv_usec = TIME_OUT_END_OF_FRAME;
			
			WAIT_DATA();
		} else {
				/* All chars are received */
				select_ret = FALSE;
		}
		
	}

	/* OK */
	return 0;
}  
   
/********************************************************************* 
 
	Function to check the correct response is returned and that the 
	checksum is correct. 
	
	Returns the data byte(s) in the response.
 
**********************************************************************/  
  
static int modbus_check_response(modbus_param_t *mb_param,  
						uint8_t *query, uint8_t *response)  
{  
    int response_length_computed;
	int response_length;
	int crc_check;
	int ret;
  
	response_length_computed = compute_response_length(mb_param, query);
	if (mb_param->debug) {
        DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"response_length_computed = %d", response_length_computed);
	}
	
    ret = receive_msg(mb_param, response_length_computed, 
					  response, &response_length);  
						 
	if (ret == 0) {
		
		/* good response so check the CRC*/
		crc_check = check_crc16(mb_param, response, response_length);
				if (mb_param->debug) {
            DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"crc check = %.2d", crc_check);
		}
					  
		if (crc_check != 0)
			return crc_check;
			
		if (mb_param->debug) {
            DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"we received a message of [%.2X] bytes, with a valid crc", response_length);
		}
				
	} else if (ret == COMM_TIME_OUT) {
		error_treat(mb_param, ret, "Communication time out");
		return ret;
	} else {
		return ret;
	}
	
	return 0;
}  
  
/*********************************************************************** 
 
    The following functions construct the required query into 
    a modbus query packet. 
 
***********************************************************************/  
  
int build_query(modbus_data_t *mb_data, unsigned char *query )  
{  
	/* build Hunayang request packet based on function code and return the 
	packet length (less CRC - 2 bytes) */
	
	switch (mb_data->function)
	{
		case FUNCTION_READ:
		case FUNCTION_WRITE:
			query[0] = mb_data->slave;
			query[1] = mb_data->function;
			query[2] = 0x03;
			query[3] = mb_data->parameter;
			query[4] = mb_data->data >> 8;
			query[5] = mb_data->data & 0x00FF;		
			return 6;
			break;
		
		case WRITE_CONTROL_DATA:
		case READ_CONTROL_STATUS:
			query[0] = mb_data->slave;
			query[1] = mb_data->function;
			query[2] = 0x01;
			query[3] = mb_data->data & 0x00FF;
			return 4;
			break;	 
		
		case WRITE_FREQ_DATA:
			query[0] = mb_data->slave;
			query[1] = mb_data->function;
			query[2] = 0x02;
			query[3] = mb_data->data >> 8; 
			query[4] = mb_data->data & 0x00FF;
			return 5;
		break;	
		
		case 0x06:
			/* Huanyang VFD - Reserved */
		
		case 0x07:
			/* Huanyang VFD - Reserved */		
			return -1;
        	break;
		
		case LOOP_TEST:
  			return -1;
        	break;
		default:
        	return -1;
			break;
	}


	
}  
  

int hy_modbusN(modbus_param_t *mb_param, modbus_data_t *mb_data)
{
	int result;
	int retrycount = 3;
	while( retrycount-- )
	{
		result = hy_modbus( mb_param, mb_data );
		if ( result == 0 ) return result;
	}
	return result;
}
		
  
/************************************************************************ 
 
    hy_modbus 
 
    sends and receives "modbus" messages to and from a Huanyang VFD 
     
*************************************************************************/  

int hy_modbus(modbus_param_t *mb_param, modbus_data_t *mb_data)
{
	int query_length;
	int query_ret;
	int response_ret;
	int msg_function_code;

	unsigned char query[MIN_QUERY_SIZE];
	unsigned char response[MAX_PACKET_SIZE];
	
	/* build the request query */
	query_length = build_query(mb_data, query);
	if (mb_param->debug) {
        DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"query_length = %d", query_length);
	} 
	
	/* add CRC to the query and send */
	query_ret = modbus_send(mb_param, query, query_length);
	if (mb_param->debug) {
        DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"query_ret = %d", query_ret);
	} 
	
	if (query_ret > 0){
		/* query was sent so get the response from the VFD */
		response_ret = modbus_check_response(mb_param, query, response);
		
		if (response_ret == 0) {
		
			msg_function_code = response[1];
			if (mb_param->debug) {
                DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"the message function code is = [%.2X]", msg_function_code);
			}
			
			/* check that the returned function code is the same as the query */
			if (msg_function_code != mb_data->function)
				return ILLEGAL_FUNCTION;
							
			/* the returned data length */
			mb_data->ret_length = response[2];
		
			switch (msg_function_code)
			{
				case FUNCTION_READ:
				case FUNCTION_WRITE:
					mb_data->ret_parameter = response[3];
					if (mb_data->ret_length == 2) {
						mb_data->ret_data = response[4];
					} else {
						mb_data->ret_data = response[4] << 8 | response[5];
					}
					break;
					
				case WRITE_CONTROL_DATA:
					mb_data->ret_parameter = 0x00;
					mb_data->ret_data = response[3];
					break;
					
				case READ_CONTROL_STATUS:
					mb_data->ret_parameter = response[3];
					mb_data->ret_data = response[4] << 8 | response[5];
					break;
					
				case WRITE_FREQ_DATA:
					mb_data->ret_parameter = response[3];
					mb_data->ret_data = response[3] << 8 | response[4];
					break;	
					
				default:
        			return -1;
					break;
			}
					
		
			if (mb_param->debug) {
                DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"response parameter = [%.2X]", mb_data->ret_parameter);
                DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_DEBUG,"response data = [%.4X]", mb_data->ret_data);
			}
		}
				 
	} else {
		response_ret = query_ret;
	}
	
	return response_ret;
}  

  

/************************************************************************ 
 
    Initializes the modbus_param_t structure for RTU over TCP/IP
    - host: IP address or hostname
    - port: port of server
**************************************************************************/  
  
void modbus_init_rtu(modbus_param_t *mb_param, const char *host, int port, Ujari *telescope)
{
        memset(mb_param, 0, sizeof(modbus_param_t));
        strncpy(mb_param->host, host, MAX_HOST_NAME);
        mb_param->port = port;
        mb_param->telescope = telescope;
        mb_param->debug = FALSE;
}  


/************************************************************************ 
 
	Closes the file descriptor in RTU mode
 
**************************************************************************/ 

void modbus_close(modbus_param_t *mb_param)
{
        close(mb_param->fd);
}
  
  
/************************************************************************ 
 
    Connect to RS485 server port for RTU communications
 
**************************************************************************/  

int modbus_connect(modbus_param_t *mb_param)
{
    struct sockaddr_in serv_addr;
    struct hostent *hp;
    int sockfd;

    /* lookup host address */
    hp = gethostbyname (mb_param->host);
    if (!hp)
    {
        DEBUGFDEVICE(mb_param->telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "gethostbyname(%s): %s", mb_param->host,  strerror(errno));
        return -1;
    }

    /* create a socket to the RS485 server */
    (void) memset ((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    serv_addr.sin_port = htons(mb_param->port);
    if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        DEBUGFDEVICE (mb_param->telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "socket(%s,%d): %s", mb_param->host, mb_param->port,strerror(errno));
        return -1;
    }

    /* connect */
    if (::connect (sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
    {
        DEBUGFDEVICE (mb_param->telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "connect(%s,%d): %s", mb_param->host,mb_param->port,strerror(errno));
        return -1;
    }

    /* ok */
    mb_param->fd = sockfd;

    return 0;
}

/************************************************************************

    Retrun error code string

    Added by Jasem Mutlaq 2014-04-17

**************************************************************************/

const char *modbus_strerror(int errnum)
{
    switch (errnum)
    {
        case 0:
            return "Modbus OK";
            break;
        case -1:
            return "Illegal Function";
            break;
        case -2:
            return "Illegal parameter";
            break;
        case -3:
            return "Illegal data value";
            break;
        case -4:
            return "Slave device failure";
            break;
         case -5:
            return "Acknowledge";
            break;
         case -6:
            return "Slave device busy";
            break;
         case -7:
            return "Negative acknowledge";
            break;
          case -8:
            return "Memory parity error";
            break;
          default:
            return "Unknown error";
            break;
    }
}
