#if 0
    LX200 Astro-Physics Driver
    Copyright (C) 2007 Markus Wildi

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

/*ToDo: compare the routes with the new ones from lx200driver.c r111 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "indicom.h"
#include "indidevapi.h"
#include "lx200driver.h"
#include "lx200apdriver.h"

#ifndef _WIN32
#include <termios.h>
#endif

#define LX200_TIMEOUT	5		/* FD timeout in seconds */
int check_lx200ap_connection(int fd)
{

  int i=0;
/*  char ack[1] = { (char) 0x06 }; does not work for AP moung */
  char temp_string[64];
  int error_type;
  int nbytes_write=0;
  int nbytes_read=0;

  #ifdef INDI_DEBUG
  IDLog("Testing telescope's connection using #:GG#...\n");
  #endif

  if (fd <= 0)
  {
      #ifdef INDI_DEBUG
      IDLog("check_lx200ap_connection: not a valid file descriptor received\n");
      #endif
      return -1;
  }
  for (i=0; i < 2; i++)
  {
      if ( (error_type = tty_write_string(fd, "#:GG#", &nbytes_write)) != TTY_OK)
      {
	  #ifdef INDI_DEBUG
	  IDLog("check_lx200ap_connection: unsuccessful write to telescope, %d\n", nbytes_write);
          #endif

	  return error_type;
      }
      error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read) ;
      tcflush(fd, TCIFLUSH);
      if (nbytes_read > 1)
      {
	  temp_string[ nbytes_read -1] = '\0';
	  #ifdef INDI_DEBUG
	  IDLog("check_lx200ap_connection: received bytes %d, [%s]\n", nbytes_write, temp_string);
          #endif
	  return 0;
      }
    usleep(50000);
  }
  
  #ifdef INDI_DEBUG
  IDLog("check_lx200ap_connection: wrote, but nothing received\n");
  #endif
  return -1;
}
int getAPUTCOffset(int fd, double *value)
{
    int error_type;
    int nbytes_write=0;
    int nbytes_read=0;

    char temp_string[16];

    if ( (error_type = tty_write_string(fd, "#:GG#", &nbytes_write)) != TTY_OK)
	return error_type;

    if(( error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
	#ifdef INDI_DEBUG
	IDLog("getAPUTCOffset: saying good bye %d, %d\n", error_type, nbytes_read);
        #endif

	return error_type ;
    }
    tcflush(fd, TCIFLUSH);

/* Negative offsets, see AP keypad manual p. 77 */
    if((temp_string[0]== 'A') || ((temp_string[0]== '0')&&(temp_string[1]== '0')) ||(temp_string[0]== '@'))
    {
	int i ;
	for( i=nbytes_read; i > 0; i--)
	{
	    temp_string[i]= temp_string[i-1] ; 
	}
	temp_string[0] = '-' ;
	temp_string[nbytes_read + 1] = '\0' ;

	if( temp_string[1]== 'A') 
	{
	    temp_string[1]= '0' ;
	    switch (temp_string[2])
	    {
		case '5':
		    
		    temp_string[2]= '1' ;
		    break ;
		case '4':
		    
		    temp_string[2]= '2' ;
		    break ;
		case '3':
		    
		    temp_string[2]= '3' ;
		    break ;
		case '2':
		    
		    temp_string[2]= '4' ;
		    break ;
		case '1':
		    
		    temp_string[2]= '5' ;
		    break ;
		default:
		    #ifdef INDI_DEBUG
		    IDLog("getAPUTCOffset: string not handled %s\n", temp_string);
                    #endif
		    return -1 ;
		    break ;
	    }
	}    
	else if( temp_string[1]== '0')
	{
	    temp_string[1]= '0' ;
	    temp_string[2]= '6' ;
                   #ifdef INDI_DEBUG
	    IDLog("getAPUTCOffset: done here %s\n", temp_string);
                    #endif

	}
	else if( temp_string[1]== '@')
	{
	    temp_string[1]= '0' ;
	    switch (temp_string[2])
	    {
		case '9':
		    
		    temp_string[2]= '7' ;
		    break ;
		case '8':
		    
		    temp_string[2]= '8' ;
		    break ;
		case '7':
		    
		    temp_string[2]= '9' ;
		    break ;
		case '6':
		    
		    temp_string[2]= '0' ;
		    break ;
		case '5':
		    temp_string[1]= '1' ;
		    temp_string[2]= '1' ;
		    break ;
		case '4':
		    
		    temp_string[1]= '1' ;
		    temp_string[2]= '2' ;
		    break ;
		default:
		    #ifdef INDI_DEBUG
		    IDLog("getAPUTCOffset: string not handled %s\n", temp_string);
                    #endif
		    return -1 ;
		    break;
	    }    
	}
	else
	{
	    #ifdef INDI_DEBUG
	    IDLog("getAPUTCOffset: string not handled %s\n", temp_string);
            #endif
	}
    }
    else
    {
	temp_string[nbytes_read - 1] = '\0' ;
    }
/*    #ifdef INDI_DEBUG
      IDLog("getAPUTCOffset: received string %s\n", temp_string);
      #endif
*/
    if (f_scansexa(temp_string, value))
    {
	fprintf(stderr, "getAPUTCOffset: unable to process [%s]\n", temp_string);
	return -1;
    }
    return 0;
}
int setAPObjectAZ(int fd, double az)
{
    int h, m, s;
    char temp_string[16];

    getSexComponents(az, &h, &m, &s);

    snprintf(temp_string, sizeof( temp_string ), "#:Sz %03d*%02d:%02d#", h, m, s);
    #ifdef INDI_DEBUG
    IDLog("setAPObjectAZ: Set Object AZ String %s\n", temp_string);
    #endif

    return (setStandardProcedure(fd, temp_string));
}

/* wildi Valid set Values are positive, add error condition */

int setAPObjectAlt(int fd, double alt)
{
    int d, m, s;
    char temp_string[16];
    
    getSexComponents(alt, &d, &m, &s);

    /* case with negative zero */
    if (!d && alt < 0)
    {
	snprintf(temp_string, sizeof( temp_string ), "#:Sa -%02d*%02d:%02d#", d, m, s) ;
    }
    else
    {
	snprintf(temp_string, sizeof( temp_string ), "#:Sa %+02d*%02d:%02d#", d, m, s) ;
    }	

    #ifdef INDI_DEBUG
    IDLog("setAPObjectAlt: Set Object Alt String %s\n", temp_string);
    #endif
    return (setStandardProcedure(fd, temp_string));
}
int setAPUTCOffset(int fd, double hours)
{
    int h, m, s ;

    char temp_string[16];
/* To avoid the peculiar output format of AP controller, see p. 77 key pad manual */
    if( hours < 0.)
    {
	hours += 24. ;
    }

    getSexComponents(hours, &h, &m, &s);
    
    snprintf(temp_string, sizeof( temp_string ), "#:SG %+03d:%02d:%02d#", h, m, s);
    #ifdef INDI_DEBUG
    IDLog("setAPUTCOffset: %s\n", temp_string);
    #endif


    return (setStandardProcedure(fd, temp_string));
}
int APSyncCM(int fd, char *matchedObject)
{
    int error_type;
    int nbytes_write=0;
    int nbytes_read=0;

    #ifdef INDI_DEBUG
    IDLog("APSyncCM\n");
    #endif
    if ( (error_type = tty_write_string(fd, "#:CM#", &nbytes_write)) != TTY_OK)
	return error_type ;
  
    if(( error_type = tty_read_section(fd, matchedObject, '#', LX200_TIMEOUT, &nbytes_read)) != TTY_OK)
	return error_type ;
   
    matchedObject[nbytes_read-1] = '\0';
  
    /* Sleep 10ms before flushing. This solves some issues with LX200 compatible devices. */
    usleep(10000);
  
    tcflush(fd, TCIFLUSH);

    return 0;
}
int APSyncCMR(int fd, char *matchedObject)
{
    int error_type;
    int nbytes_write=0;
    int nbytes_read=0;
    
    #ifdef INDI_DEBUG
    IDLog("APSyncCMR\n");
    #endif
    if ( (error_type = tty_write_string(fd, "#:CMR#", &nbytes_write)) != TTY_OK)
	return error_type;
 
    /* read_ret = portRead(matchedObject, -1, LX200_TIMEOUT); */
    if(( error_type = tty_read_section(fd, matchedObject, '#', LX200_TIMEOUT, &nbytes_read))  != TTY_OK)
	return error_type ;

    matchedObject[nbytes_read-1] = '\0';
  
    /* Sleep 10ms before flushing. This solves some issues with LX200 compatible devices. */
    usleep(10000);
  
    tcflush(fd, TCIFLUSH);

    return 0;
}
int selectAPMoveToRate(int fd, int moveToRate)
{
    int error_type;
    int nbytes_write=0;

    switch (moveToRate)
    {
	/* 1200x */
	case 0:
            #ifdef INDI_DEBUG
	    IDLog("selectAPMoveToRate: Setting move to rate to 1200x.\n");
	    #endif
	    if ( (error_type = tty_write_string(fd, "#:RC3#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;
    
	    /* 600x */
	case 1:
            #ifdef INDI_DEBUG
	    IDLog("selectAPMoveToRate: Setting move to rate to 600x.\n");
            #endif
	    if ( (error_type = tty_write_string(fd, "#:RC2#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;

	    /* 64x */
	case 2:
            #ifdef INDI_DEBUG
	    IDLog("selectAPMoveToRate: Setting move to rate to 64x.\n");
            #endif
	    if ( (error_type = tty_write_string(fd, "#:RC1#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;
	    /* 12x*/
	case 3:
            #ifdef INDI_DEBUG
	    IDLog("selectAPMoveToRate: Setting move to rate to 12x.\n");
            #endif
	    if ( (error_type = tty_write_string(fd, "#:RC0#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;

	default:
	    return -1;
	    break;
   }
   return 0;
}
int selectAPSlewRate(int fd, int slewRate)
{
    int error_type;
    int nbytes_write=0;
    switch (slewRate)
    {
	/* 1200x */
	case 0:
            #ifdef INDI_DEBUG
	    IDLog("selectAPSlewRate: Setting slew rate to 1200x.\n");
            #endif
	    if ( (error_type = tty_write_string(fd, "#:RS2#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;
    
	    /* 900x */
	case 1:
            #ifdef INDI_DEBUG
	    IDLog("selectAPSlewRate: Setting slew rate to 900x.\n");
            #endif
	    if ( (error_type = tty_write_string(fd, "#:RS1#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;

	    /* 600x */
	case 2:
            #ifdef INDI_DEBUG
	    IDLog("selectAPSlewRate: Setting slew rate to 600x.\n");
            #endif
	    if ( (error_type = tty_write_string(fd, "#:RS0#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;

	default:
	    return -1;
	    break;
    }
    return 0;
}
int selectAPTrackingMode(int fd, int trackMode)
{
    int error_type;
    int nbytes_write=0;

    switch (trackMode)
    {
	/* Lunar */
	case 0:
            #ifdef INDI_DEBUG
	    IDLog("selectAPTrackingMode: Setting tracking mode to lunar.\n");
            #endif
	    if ( (error_type = tty_write_string(fd, "#:RT0#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;
    
	    /* Solar */
	case 1:
            #ifdef INDI_DEBUG
	    IDLog("selectAPTrackingMode: Setting tracking mode to solar.\n");
            #endif
	    if ( (error_type = tty_write_string(fd, "#:RT1#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;

	    /* Sidereal */
	case 2:
            #ifdef INDI_DEBUG
            IDLog("selectAPTrackingMode: Setting tracking mode to sidereal.\n");
            #endif
	    if ( (error_type = tty_write_string(fd, "#:RT2#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;

	    /* Zero */
	case 3:
            #ifdef INDI_DEBUG
            IDLog("selectAPTrackingMode: Setting tracking mode to zero.\n");
            #endif
	    if ( (error_type = tty_write_string(fd, "#:RT9#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;
 
	default:
	    return -1;
	    break;
    }
    return 0;
}
int swapAPButtons(int fd, int currentSwap)
{
    int error_type;
    int nbytes_write=0;

    switch (currentSwap)
    {
	case 0:
            #ifdef INDI_DEBUG
            IDLog("#:NS#\n");
            #endif
	    if ( (error_type = tty_write_string(fd, "#:NS#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;
	    
	case 1:
            #ifdef INDI_DEBUG
            IDLog("#:EW#\n");
            #endif
	    if ( (error_type = tty_write_string(fd, "#:EW#", &nbytes_write)) != TTY_OK)
		return error_type;
	    break;

	default:
	    return -1;
	    break;
    }
    return 0;
}
int setAPObjectRA(int fd, double ra)
{
/*ToDo AP accepts "#:Sr %02d:%02d:%02d.%1d#"*/
 int h, m, s;
 char temp_string[16];

 getSexComponents(ra, &h, &m, &s);

 snprintf(temp_string, sizeof( temp_string ), "#:Sr %02d:%02d:%02d#", h, m, s);

 #ifdef INDI_DEBUG
 IDLog("setAPObjectRA: Set Object RA String %s, %f\n", temp_string, ra);
 #endif
 return (setStandardProcedure(fd, temp_string));
}

int setAPObjectDEC(int fd, double dec)
{
  int d, m, s;
  char temp_string[16];

  getSexComponents(dec, &d, &m, &s);
  /* case with negative zero */
  if (!d && dec < 0)
  {
      snprintf(temp_string, sizeof( temp_string ), "#:Sd -%02d*%02d:%02d#", d, m, s);
  }
  else
  {
      snprintf(temp_string, sizeof( temp_string ),   "#:Sd %+03d*%02d:%02d#", d, m, s);
  }
  #ifdef INDI_DEBUG
  IDLog("setAPObjectDEC: Set Object DEC String %s\n", temp_string) ;
  #endif
  return (setStandardProcedure(fd, temp_string));
}
int setAPSiteLongitude(int fd, double Long)
{
   int d, m, s;
   char temp_string[32];

   getSexComponents(Long, &d, &m, &s);
   snprintf(temp_string, sizeof( temp_string ), "#:Sg %03d*%02d:%02d#", d, m, s);
   return (setStandardProcedure(fd, temp_string));
}

int setAPSiteLatitude(int fd, double Lat)
{
   int d, m, s;
   char temp_string[32];

   getSexComponents(Lat, &d, &m, &s);
   snprintf(temp_string, sizeof( temp_string ), "#:St %+03d*%02d:%02d#", d, m, s);
   return (setStandardProcedure(fd, temp_string));
}
