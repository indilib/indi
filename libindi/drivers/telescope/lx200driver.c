#if 0
    LX200 Driver
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef _WIN32
#include <termios.h>
#endif

#define LX200_TIMEOUT	5		/* FD timeout in seconds */

int controller_format;
int lx200_debug = 0;

/**************************************************************************
 Diagnostics
 **************************************************************************/
char ACK(int fd);
/*int testTelescope(void);
int testAP(void);*/
int check_lx200_connection(int fd);

/**************************************************************************
 Get Commands: store data in the supplied buffer. Return 0 on success or -1 on failure 
 **************************************************************************/
 
/* Get Double from Sexagisemal */
int getCommandSexa(int fd, double *value, const char *cmd);
/* Get String */
int getCommandString(int fd, char *data, const char* cmd);
/* Get Int */
int getCommandInt(int fd, int *value, const char* cmd);
/* Get tracking frequency */
int getTrackFreq(int fd, double * value);
/* Get site Latitude */
int getSiteLatitude(int fd, int *dd, int *mm);
/* Get site Longitude */
int getSiteLongitude(int fd, int *ddd, int *mm);
/* Get Calender data */
int getCalenderDate(int fd, char *date);
/* Get site Name */
int getSiteName(int fd, char *siteName, int siteNum);
/* Get Number of Bars */
int getNumberOfBars(int fd, int *value);
/* Get Home Search Status */
int getHomeSearchStatus(int fd, int *status);
/* Get OTA Temperature */
int getOTATemp(int fd, double * value);
/* Get time format: 12 or 24 */
int getTimeFormat(int fd, int *format);


/**************************************************************************
 Set Commands
 **************************************************************************/

/* Set Int */
int setCommandInt(int fd, int data, const char *cmd);
/* Set Sexigesimal */
int setCommandXYZ(int fd, int x, int y, int z, const char *cmd);
/* Common routine for Set commands */
int setStandardProcedure(int fd, const char * writeData);
/* Set Slew Mode */
int setSlewMode(int fd, int slewMode);
/* Set Alignment mode */
int setAlignmentMode(int fd, unsigned int alignMode);
/* Set Object RA */
int setObjectRA(int fd, double ra);
/* set Object DEC */
int setObjectDEC(int fd, double dec);
/* Set Calender date */
int setCalenderDate(int fd, int dd, int mm, int yy);
/* Set UTC offset */
int setUTCOffset(int fd, double hours);
/* Set Track Freq */
int setTrackFreq(int fd, double trackF);
/* Set current site longitude */
int setSiteLongitude(int fd, double Long);
/* Set current site latitude */
int setSiteLatitude(int fd, double Lat);
/* Set Object Azimuth */
int setObjAz(int fd, double az);
/* Set Object Altitude */
int setObjAlt(int fd, double alt);
/* Set site name */
int setSiteName(int fd, char * siteName, int siteNum);
/* Set maximum slew rate */
int setMaxSlewRate(int fd, int slewRate);
/* Set focuser motion */
int setFocuserMotion(int fd, int motionType);
/* Set focuser speed mode */
int setFocuserSpeedMode (int fd, int speedMode);
/* Set minimum elevation limit */
int setMinElevationLimit(int fd, int min);
/* Set maximum elevation limit */
int setMaxElevationLimit(int fd, int max);

/**************************************************************************
 Motion Commands
 **************************************************************************/
/* Slew to the selected coordinates */
int Slew(int fd);
/* Synchronize to the selected coordinates and return the matching object if any */
int Sync(int fd, char *matchedObject);
/* Abort slew in all axes */
int abortSlew(int fd);
/* Move into one direction, two valid directions can be stacked */
int MoveTo(int fd, int direction);
/* Half movement in a particular direction */
int HaltMovement(int fd, int direction);
/* Select the tracking mode */
int selectTrackingMode(int fd, int trackMode);
/* Select Astro-Physics tracking mode */
int selectAPTrackingMode(int fd, int trackMode);
/* Send Pulse-Guide command (timed guide move), two valid directions can be stacked */
int SendPulseCmd(int fd, int direction, int duration_msec);

/**************************************************************************
 Other Commands
 **************************************************************************/
 /* Ensures LX200 RA/DEC format is long */
int checkLX200Format(int fd);
/* Select a site from the LX200 controller */
int selectSite(int fd, int siteNum);
/* Select a catalog object */
int selectCatalogObject(int fd, int catalog, int NNNN);
/* Select a sub catalog */
int selectSubCatalog(int fd, int catalog, int subCatalog);

void setLX200Debug(int value)
{
    lx200_debug = value;
}

int check_lx200_connection(int in_fd)
{

  int i=0;
  char ack[1] = { (char) 0x06 };
  char MountAlign[64];
  int nbytes_read=0;

  if (lx200_debug)
    IDLog("%s Testing telescope's connection using ACK...\n", __FUNCTION__);

  if (in_fd <= 0) return -1;

  for (i=0; i < 2; i++)
  {
    if (write(in_fd, ack, 1) < 0) return -1;
    tty_read(in_fd, MountAlign, 1, LX200_TIMEOUT, &nbytes_read);
    if (nbytes_read == 1)
     return 0;
    usleep(50000);
  }
  
  return -1;
}


/**********************************************************************
* GET
**********************************************************************/

char ACK(int fd)
{
  char ack[1] = { (char) 0x06 };
  char MountAlign[2];
  int nbytes_write=0, nbytes_read=0, error_type;

  if (lx200_debug)
   IDLog("%s Command [%s]\n", __FUNCTION__, ack);

  nbytes_write = write(fd, ack, 1);

  if (nbytes_write < 0)
    return -1;
 
  error_type = tty_read(fd, MountAlign, 1, LX200_TIMEOUT, &nbytes_read);
  
  if (lx200_debug)
    IDLog("%s Response <%s>\n", __FUNCTION__, MountAlign);

  if (nbytes_read == 1)
    return MountAlign[0];
  else
    return error_type;
}

int getCommandSexa(int fd, double *value, const char * cmd)
{
  char temp_string[16];
  int error_type;
  int nbytes_write=0, nbytes_read=0;
  
  tcflush(fd, TCIFLUSH);

  if (lx200_debug)
   IDLog("%s Command [%s]\n", __FUNCTION__, cmd);

  if ( (error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
   return error_type;
  
  error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read);
  tcflush(fd, TCIFLUSH);
  if (error_type != TTY_OK)
    return error_type;

  temp_string[nbytes_read - 1] = '\0';
  
  if (lx200_debug)
    IDLog("%s Response <%s>\n", __FUNCTION__, temp_string);

  if (f_scansexa(temp_string, value))
  {
   if (lx200_debug)
    IDLog("%s unable to process response\n", __FUNCTION__);
   return -1;
  }
 
  if (lx200_debug)
      IDLog("%s Sexa Response <%g>\n", __FUNCTION__, *value);

   tcflush(fd, TCIFLUSH);
   return 0;
}

int getCommandInt(int fd, int *value, const char* cmd)
{
  char temp_string[16];
  float temp_number;
  int error_type;
  int nbytes_write=0, nbytes_read=0;
  
  tcflush(fd, TCIFLUSH);

  if (lx200_debug)
   IDLog("%s Command [%s]\n", __FUNCTION__, cmd);

  if ( (error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
   return error_type;
  
  error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read);
  tcflush(fd, TCIFLUSH);
  if (error_type != TTY_OK)
    return error_type;
 
  temp_string[nbytes_read - 1] = '\0';

  /* Float */
  if (strchr(temp_string, '.'))
  {
     if (sscanf(temp_string, "%f", &temp_number) != 1)
	return -1;

	*value = (int) temp_number;
   }
  /* Int */
  else if (sscanf(temp_string, "%d", value) != 1)
	return -1;

  if (lx200_debug)
   IDLog("%s Response <%d>\n", __FUNCTION__, *value);

   return 0;
}


int getCommandString(int fd, char *data, const char* cmd)
{
    char * term;
    int error_type;
    int nbytes_write=0, nbytes_read=0;
    
    if (lx200_debug)
     IDLog("%s Command [%s]\n", __FUNCTION__, cmd);

   if ( (error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
    return error_type;

    error_type = tty_read_section(fd, data, '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    if (error_type != TTY_OK)
	return error_type;

    term = strchr (data, '#');
    if (term)
      *term = '\0';

   if (lx200_debug)
    IDLog("%s Response <%s>\n", __FUNCTION__, data);

    return 0;
}

int isSlewComplete(int fd)
{
    char data[8];
    int error_type;
    int nbytes_write=0, nbytes_read=0;
    char *cmd = "#:D#";

    if (lx200_debug)
     IDLog("%s Command [%s]\n", __FUNCTION__, cmd);

   if ( (error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
    return error_type;

   error_type = tty_read_section(fd, data, '#', LX200_TIMEOUT, &nbytes_read);
   tcflush(fd, TCIOFLUSH);

    if (error_type != TTY_OK)
    return error_type;

    if (lx200_debug)
     IDLog("%s Response <%s>\n", __FUNCTION__, data);

    if (data[0] == '#')
        return 0;
    else
        return 1;

}

int getCalenderDate(int fd, char *date)
{

 int dd, mm, yy;
 int error_type;
 int nbytes_read=0;
 char mell_prefix[3];
 
 if (lx200_debug)
  IDLog("%s Command [#:GC#]\n", __FUNCTION__);

 if ( (error_type = getCommandString(fd, date, "#:GC#")) )
   return error_type;

  /* Meade format is MM/DD/YY */
  nbytes_read = sscanf(date, "%d%*c%d%*c%d", &mm, &dd, &yy);
  if (nbytes_read < 3)
   return -1;

  /* We consider years 50 or more to be in the last century, anything less in the 21st century.*/
  if (yy > 50)
	strncpy(mell_prefix, "19", 3);
  else
	strncpy(mell_prefix, "20", 3);

 /* We need to have in in YYYY/MM/DD format */
 snprintf(date, 16, "%s%02d/%02d/%02d", mell_prefix, yy, mm, dd);

 if (lx200_debug)
     IDLog("%s Response <%s>\n", __FUNCTION__, date);

 return (0);

}

int getTimeFormat(int fd, int *format)
{
  char temp_string[16];
  int error_type;
  int nbytes_write=0, nbytes_read=0;
  int tMode;

  if (lx200_debug)
   IDLog("%s Command [#:Gc#]\n", __FUNCTION__);
  
 if ( (error_type = tty_write_string(fd, "#:Gc#", &nbytes_write)) != TTY_OK)
    return error_type;

  /*read_ret = portRead(temp_string, -1, LX200_TIMEOUT);*/
  if ( (error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read)) != TTY_OK)
	return error_type;

  tcflush(fd, TCIFLUSH);
  
  if (nbytes_read < 1)
   return error_type;
   
  temp_string[nbytes_read-1] = '\0';

  if (lx200_debug)
      IDLog("%s Response <%s>\n", __FUNCTION__, temp_string);

  nbytes_read = sscanf(temp_string, "(%d)", &tMode);

  if (nbytes_read < 1)
   return -1;
  else
   *format = tMode;
   
  return 0;

}

int getSiteName(int fd, char *siteName, int siteNum)
{
  char * term;
  int error_type;
  int nbytes_write=0, nbytes_read=0;

  switch (siteNum)
  {
    case 1:
      if (lx200_debug)
       IDLog("%s Command [#:GM#]\n", __FUNCTION__);
       if ( (error_type = tty_write_string(fd, "#:GM#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
    case 2:
      if (lx200_debug)
       IDLog("%s Command [#:GN#]\n", __FUNCTION__);
	if ( (error_type = tty_write_string(fd, "#:GN#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
    case 3:
      if (lx200_debug)
       IDLog("%s Command [#:GO#]\n", __FUNCTION__);
	if ( (error_type = tty_write_string(fd, "#:GO#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
    case 4:
      if (lx200_debug)
       IDLog("%s Command [#:GP#]\n", __FUNCTION__);
	if ( (error_type = tty_write_string(fd, "#:GP#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
    default:
     return -1;
   }

   error_type = tty_read_section(fd, siteName, '#', LX200_TIMEOUT, &nbytes_read);
   tcflush(fd, TCIFLUSH);

   if (nbytes_read < 1)
     return error_type;

   siteName[nbytes_read - 1] = '\0';

   if (lx200_debug)
       IDLog("%s Response <%s>\n", __FUNCTION__, siteName);

   term = strchr (siteName, ' ');
    if (term)
      *term = '\0';

    term = strchr (siteName, '<');
    if (term)
      strcpy(siteName, "unused site");

    if (lx200_debug)
        IDLog("%s Final Site Name <%s>\n", __FUNCTION__, siteName);

    return 0;
}

int getSiteLatitude(int fd, int *dd, int *mm)
{
  char temp_string[16];
  int error_type;
  int nbytes_write=0, nbytes_read=0;

  if (lx200_debug)
   IDLog("%s Command [#:Gt#]\n", __FUNCTION__);

  if ( (error_type = tty_write_string(fd, "#:Gt#", &nbytes_write)) != TTY_OK)
    	return error_type;

  error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read);
  tcflush(fd, TCIFLUSH);
  
   if (nbytes_read < 1) 
   return error_type;
   
  temp_string[nbytes_read -1] = '\0';

  if (lx200_debug)
      IDLog("%s Response <%s>\n", __FUNCTION__, temp_string);

  if (sscanf (temp_string, "%d%*c%d", dd, mm) < 2)
  {
      if (lx200_debug)
          IDLog("%s Processing response failed\n", __FUNCTION__);
      return -1;
  }

  if (lx200_debug)
      IDLog("%s Processed response <%d:%d>\n", __FUNCTION__, *dd, *mm);

  return 0;
}

int getSiteLongitude(int fd, int *ddd, int *mm)
{
  char temp_string[16];
  int error_type;
  int nbytes_write=0, nbytes_read=0;

  if (lx200_debug)
   IDLog("%s Command [#:Gg#]\n", __FUNCTION__);

  if ( (error_type = tty_write_string(fd, "#:Gg#", &nbytes_write)) != TTY_OK)
    	return error_type;

  error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read);
  
  tcflush(fd, TCIFLUSH);
  
  if (nbytes_read < 1)
    return error_type;
    
  temp_string[nbytes_read -1] = '\0';

  if (lx200_debug)
      IDLog("%s Response <%s>\n", __FUNCTION__, temp_string);

  if (sscanf (temp_string, "%d%*c%d", ddd, mm) < 2)
  {
      if (lx200_debug)
          IDLog("%s Processing response failed\n", __FUNCTION__);

      return -1;
  }

  if (lx200_debug)
      IDLog("%s Processed response <%d:%d>\n", __FUNCTION__, *ddd, *mm);

  return 0;
}

int getTrackFreq(int fd, double *value)
{
    float Freq;
    char temp_string[16];
    int error_type;
    int nbytes_write=0, nbytes_read=0;
    
    if (lx200_debug)
     IDLog("%s Command [#:GT#]\n", __FUNCTION__);

    if ( (error_type = tty_write_string(fd, "#:GT#", &nbytes_write)) != TTY_OK)
    	return error_type;

    error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);
    
    if (nbytes_read < 1)
     return error_type;

    temp_string[nbytes_read] = '\0';

    if (lx200_debug)
        IDLog("%s Response <%s>\n", __FUNCTION__, temp_string);
       
    if (sscanf(temp_string, "%f#", &Freq) < 1)
    {
        if (lx200_debug)
            IDLog("%s Processing response failed\n", __FUNCTION__);

        return -1;
    }
   
    *value = (double) Freq;
    

    if (lx200_debug)
        IDLog("%s Processed response <%g>\n", __FUNCTION__, *value);

    return 0;
}

int getNumberOfBars(int fd, int *value)
{
   char temp_string[128];
   int error_type;
   int nbytes_write=0, nbytes_read=0;

   if ( (error_type = tty_write_string(fd, "#:D#", &nbytes_write)) != TTY_OK)
    	return error_type;
   /*if (portWrite("#:D#") < 0)
     return -1;*/

   error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read);
   tcflush(fd, TCIFLUSH);
   
   if (nbytes_read < 0)
    return error_type;

   *value = nbytes_read -1;
   
   return 0;
}

int getHomeSearchStatus(int fd, int *status)
{
  char temp_string[16];
  int error_type;
  int nbytes_write=0, nbytes_read=0;

  if (lx200_debug)
   IDLog("%s Command [#:h?#]\n", __FUNCTION__);

  if ( (error_type = tty_write_string(fd, "#:h?#", &nbytes_write)) != TTY_OK)
    	return error_type;

  error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read);
  tcflush(fd, TCIFLUSH);
  
  if (nbytes_read < 1)
   return error_type;
   
  temp_string[1] = '\0';

  if (lx200_debug)
      IDLog("%s Response <%s>\n", __FUNCTION__, temp_string);

  if (temp_string[0] == '0')
    *status = 0;
  else if (temp_string[0] == '1')
    *status = 1;
  else if (temp_string[0] == '2')
    *status = 1;
  
  return 0;
}

int getOTATemp(int fd, double *value)
{

  char temp_string[16];
  int error_type;
  int nbytes_write=0, nbytes_read=0;
  float temp;
  
  if (lx200_debug)
   IDLog("%s Command [#:fT#]\n", __FUNCTION__);

  if ( (error_type = tty_write_string(fd, "#:fT#", &nbytes_write)) != TTY_OK)
    	return error_type;

  error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read);
  
  if (nbytes_read < 1)
   return error_type;
   
  temp_string[nbytes_read - 1] = '\0';

  if (lx200_debug)
      IDLog("%s Response <%s>\n", __FUNCTION__, temp_string);

  if (sscanf(temp_string, "%f", &temp) < 1)
  {
      if (lx200_debug)
          IDLog("%s Processing response failed\n", __FUNCTION__);

   return -1;
  }
   
   *value = (double) temp;

  if (lx200_debug)
      IDLog("%s Processed response <%g>\n", __FUNCTION__, *value);

  return 0;

}

int updateSkyCommanderCoord(int fd, double *ra, double *dec)
{
  char coords[16];
  char CR[1] = { (char) 0x0D };
  float RA=0.0, DEC=0.0;
  int error_type;
  int nbytes_read=0;

  error_type = write(fd, CR, 1);

  error_type = tty_read(fd, coords, 16, LX200_TIMEOUT, &nbytes_read);
  /*read_ret = portRead(coords, 16, LX200_TIMEOUT);*/
  tcflush(fd, TCIFLUSH);

  nbytes_read = sscanf(coords, " %g %g", &RA, &DEC);

  if (nbytes_read < 2)
  {
   #ifdef INDI_DEBUG
   IDLog("Error in Sky commander number format [%s], exiting.\n", coords);
   #endif
   return error_type;
  }

  *ra  = RA;
  *dec = DEC;

  return 0;

}

int updateIntelliscopeCoord (int fd, double *ra, double *dec)
{
  char coords[16];
  char CR[1] = { (char) 0x51 };	/* "Q" */
  float RA = 0.0, DEC = 0.0;
  int error_type;
  int nbytes_read=0;

  /*IDLog ("Sending a Q\n");*/
  error_type = write (fd, CR, 1);
  /* We start at 14 bytes in case its a Sky Wizard, 
     but read one more later it if it's a intelliscope */
  /*read_ret = portRead (coords, 14, LX200_TIMEOUT);*/
  error_type = tty_read(fd, coords, 14, LX200_TIMEOUT, &nbytes_read);
  tcflush(fd, TCIFLUSH);
  /*IDLog ("portRead() = [%s]\n", coords);*/

  /* Remove the Q in the response from the Intelliscope  but not the Sky Wizard */
  if (coords[0] == 'Q') {
    coords[0] = ' ';
    /* Read one more byte if Intelliscope to get the "CR" */
    error_type = tty_read(fd, coords, 1, LX200_TIMEOUT, &nbytes_read);
    /*read_ret = portRead (coords, 1, LX200_TIMEOUT);*/
  }
  nbytes_read = sscanf (coords, " %g %g", &RA, &DEC);
  /*IDLog ("sscanf() RA = [%f]\n", RA * 0.0390625);*/
  /*IDLog ("sscanf() DEC = [%f]\n", DEC * 0.0390625);*/

  /*IDLog ("Intelliscope output [%s]", coords);*/
  if (nbytes_read < 2)
  {
    #ifdef INDI_DEBUG
    IDLog ("Error in Intelliscope number format [%s], exiting.\n", coords);
    #endif
    return -1;
  }

  *ra = RA * 0.0390625;
  *dec = DEC * 0.0390625;

  return 0;

}


/**********************************************************************
* SET
**********************************************************************/

int setStandardProcedure(int fd, const char * data)
{
 char bool_return[2];
 int error_type;
 int nbytes_write=0, nbytes_read=0;
 
 if (lx200_debug)
  IDLog("%s Command [%s]\n", __FUNCTION__, data);

 if ( (error_type = tty_write_string(fd, data, &nbytes_write)) != TTY_OK)
    	return error_type;

 error_type = tty_read(fd, bool_return, 1, LX200_TIMEOUT, &nbytes_read);
 tcflush(fd, TCIFLUSH);
 
 if (nbytes_read < 1)
   return error_type;

 if (bool_return[0] == '0')
 {
     if (lx200_debug)
         IDLog("%s %s Failed.\n", __FUNCTION__, data);
     return -1;
 }

 if (lx200_debug)
    IDLog("%s %s Successful\n", __FUNCTION__, data);

 return 0;

}

int setCommandInt(int fd, int data, const char *cmd)
{
  char temp_string[16];
  int error_type;
  int nbytes_write=0;

  snprintf(temp_string, sizeof( temp_string ), "%s%d#", cmd, data);

  if (lx200_debug)
   IDLog("%s Command [%s]\n", __FUNCTION__, temp_string);

  if ( (error_type = tty_write_string(fd, temp_string, &nbytes_write)) != TTY_OK)
  {
      if (lx200_debug)
          IDLog("%s %s Failed.\n", __FUNCTION__, temp_string);

    	return error_type;
  }

  if (lx200_debug)
     IDLog("%s %s Successful\n", __FUNCTION__, temp_string);

  return 0;
}

int setMinElevationLimit(int fd, int min)
{
    if (lx200_debug)
       IDLog("%s\n", __FUNCTION__);

 char temp_string[16];

 snprintf(temp_string, sizeof( temp_string ), "#:Sh%02d#", min);

 return (setStandardProcedure(fd, temp_string));
}

int setMaxElevationLimit(int fd, int max)
{
    if (lx200_debug)
       IDLog("%s\n", __FUNCTION__);

 char temp_string[16];

 snprintf(temp_string, sizeof( temp_string ), "#:So%02d*#", max);

 return (setStandardProcedure(fd, temp_string));

}

int setMaxSlewRate(int fd, int slewRate)
{
    if (lx200_debug)
       IDLog("%s\n", __FUNCTION__);

   char temp_string[16];

   if (slewRate < 2 || slewRate > 8)
    return -1;

   snprintf(temp_string, sizeof( temp_string ), "#:Sw%d#", slewRate);

   return (setStandardProcedure(fd, temp_string));

}

int setObjectRA(int fd, double ra)
{

 int h, m, s, frac_m;
 char temp_string[16];

 getSexComponents(ra, &h, &m, &s);

 frac_m = (s / 60.0) * 10.;

 if (controller_format == LX200_LONG_FORMAT)
 	snprintf(temp_string, sizeof( temp_string ), "#:Sr %02d:%02d:%02d#", h, m, s);
 else
	snprintf(temp_string, sizeof( temp_string ), "#:Sr %02d:%02d.%01d#", h, m, frac_m);
	
 if (lx200_debug)
    IDLog("%s Set Object RA String %s\n", __FUNCTION__, temp_string);

 return (setStandardProcedure(fd, temp_string));
}


int setObjectDEC(int fd, double dec)
{
  int d, m, s;
  char temp_string[16];

  getSexComponents(dec, &d, &m, &s);

  switch(controller_format)
  {
 
    case LX200_SHORT_FORMAT:
    /* case with negative zero */
    if (!d && dec < 0)
    	snprintf(temp_string, sizeof( temp_string ), "#:Sd -%02d*%02d#", d, m);
    else	
    	snprintf(temp_string, sizeof( temp_string ), "#:Sd %+03d*%02d#", d, m);
    break;

    case LX200_LONG_FORMAT:
    /* case with negative zero */
    if (!d && dec < 0)
    	snprintf(temp_string, sizeof( temp_string ), "#:Sd -%02d:%02d:%02d#", d, m, s);
    else	
    	snprintf(temp_string, sizeof( temp_string ), "#:Sd %+03d:%02d:%02d#", d, m, s);
    break;
  }

  if (lx200_debug)
    IDLog("%s Set Object DEC String %s\n", __FUNCTION__, temp_string);
  
  return (setStandardProcedure(fd, temp_string));

}

int setCommandXYZ(int fd, int x, int y, int z, const char *cmd)
{
    if (lx200_debug)
       IDLog("%s\n", __FUNCTION__);

  char temp_string[16];

  snprintf(temp_string, sizeof( temp_string ), "%s %02d:%02d:%02d#", cmd, x, y, z);

  return (setStandardProcedure(fd, temp_string));
}

int setAlignmentMode(int fd, unsigned int alignMode)
{
  /*fprintf(stderr , "Set alignment mode %d\n", alignMode);*/
  int error_type;
  int nbytes_write=0;

  switch (alignMode)
   {
     case LX200_ALIGN_POLAR:
      if (lx200_debug)
         IDLog("%s Command [#:AP#]\n", __FUNCTION__);
	if ( (error_type = tty_write_string(fd, "#:AP#", &nbytes_write)) != TTY_OK)
    	return error_type;
      break;
     case LX200_ALIGN_ALTAZ:
      if (lx200_debug)
         IDLog("%s Command [#:AA#]\n", __FUNCTION__);
	if ( (error_type = tty_write_string(fd, "#:AA#", &nbytes_write)) != TTY_OK)
    	return error_type;
      break;
     case LX200_ALIGN_LAND:
      if (lx200_debug)
         IDLog("%s Command [#:AL#]\n", __FUNCTION__);
	if ( (error_type = tty_write_string(fd, "#:AL#", &nbytes_write)) != TTY_OK)
    	return error_type;
       break;
   }
   
   tcflush(fd, TCIFLUSH);
   return 0;
}

int setCalenderDate(int fd, int dd, int mm, int yy)
{
   char temp_string[32];
   char dumpPlanetaryUpdateString[64];
   char bool_return[2];
   int error_type;
   int nbytes_write=0, nbytes_read=0;
   yy = yy % 100;

   snprintf(temp_string, sizeof( temp_string ), "#:SC %02d/%02d/%02d#", mm, dd, yy);

   if (lx200_debug)
       IDLog("%s Command [%s]\n", __FUNCTION__, temp_string);

   if ( (error_type = tty_write_string(fd, temp_string, &nbytes_write)) != TTY_OK)
    	return error_type;

   error_type = tty_read(fd, bool_return, 1, LX200_TIMEOUT, &nbytes_read);
   tcflush(fd, TCIFLUSH);
   
   if (nbytes_read < 1)
   {
       if (lx200_debug)
           IDLog("%s Error reading response (%d)\n", __FUNCTION__, error_type);
        return error_type;
   }

   bool_return[1] = '\0';

   if (lx200_debug)
       IDLog("%s Response <%s>\n", __FUNCTION__, bool_return);

   if (bool_return[0] == '0')
     return -1;
     
    /* Read dumped data */
    error_type = tty_read_section(fd, dumpPlanetaryUpdateString, '#', LX200_TIMEOUT, &nbytes_read);
    error_type = tty_read_section(fd, dumpPlanetaryUpdateString, '#', 5, &nbytes_read);

   return 0;
}

int setUTCOffset(int fd, double hours)
{
   char temp_string[16];

   snprintf(temp_string, sizeof( temp_string ), "#:SG %+03d#", (int) hours);

   if (lx200_debug)
    IDLog("%s UTC string is %s\n", __FUNCTION__, temp_string);

   return (setStandardProcedure(fd, temp_string));

}

int setSiteLongitude(int fd, double Long)
{
   int d, m, s;
   char temp_string[32];

   if (lx200_debug)
      IDLog("%s\n", __FUNCTION__);

   getSexComponents(Long, &d, &m, &s);

   snprintf(temp_string, sizeof( temp_string ), "#:Sg%03d:%02d#", d, m);

   return (setStandardProcedure(fd, temp_string));
}

int setSiteLatitude(int fd, double Lat)
{
   int d, m, s;
   char temp_string[32];

   if (lx200_debug)
      IDLog("%s\n", __FUNCTION__);

   getSexComponents(Lat, &d, &m, &s);

   snprintf(temp_string, sizeof( temp_string ), "#:St%+03d:%02d:%02d#", d, m, s);

   return (setStandardProcedure(fd, temp_string));
}

int setObjAz(int fd, double az)
{
   int d,m,s;
   char temp_string[16];

   if (lx200_debug)
      IDLog("%s\n", __FUNCTION__);

   getSexComponents(az, &d, &m, &s);

   snprintf(temp_string, sizeof( temp_string ), "#:Sz%03d:%02d#", d, m);

   return (setStandardProcedure(fd, temp_string));

}

int setObjAlt(int fd, double alt)
{
    int d, m, s;
    char temp_string[16];

    if (lx200_debug)
       IDLog("%s\n", __FUNCTION__);

   getSexComponents(alt, &d, &m, &s);

   snprintf(temp_string, sizeof( temp_string ), "#:Sa%+02d*%02d#", d, m);

   return (setStandardProcedure(fd, temp_string));
}


int setSiteName(int fd, char * siteName, int siteNum)
{
   char temp_string[16];

   if (lx200_debug)
      IDLog("%s\n", __FUNCTION__);

   switch (siteNum)
   {
     case 1:
      snprintf(temp_string, sizeof( temp_string ), "#:SM %s#", siteName);
      break;
     case 2:
      snprintf(temp_string, sizeof( temp_string ), "#:SN %s#", siteName);
      break;
     case 3:
      snprintf(temp_string, sizeof( temp_string ), "#:SO %s#", siteName);
      break;
    case 4:
      snprintf(temp_string, sizeof( temp_string ), "#:SP %s#", siteName);
      break;
    default:
      return -1;
    }

   return (setStandardProcedure(fd, temp_string));
}

int setSlewMode(int fd, int slewMode)
{
  int error_type;
  int nbytes_write=0;

  switch (slewMode)
  {
    case LX200_SLEW_MAX:
      if (lx200_debug)
         IDLog("%s Command [#:RS#]\n", __FUNCTION__);
   if ( (error_type = tty_write_string(fd, "#:RS#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
    case LX200_SLEW_FIND:
      if (lx200_debug)
         IDLog("%s Command [#:RM#]\n", __FUNCTION__);
     if ( (error_type = tty_write_string(fd, "#:RM#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
    case LX200_SLEW_CENTER:
      if (lx200_debug)
         IDLog("%s Command [#:RC#]\n", __FUNCTION__);
	if ( (error_type = tty_write_string(fd, "#:RC#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
    case LX200_SLEW_GUIDE:
      if (lx200_debug)
         IDLog("%s Command [#:RG#]\n", __FUNCTION__);
    if ( (error_type = tty_write_string(fd, "#:RG#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
    default:
     break;
   }
   
   tcflush(fd, TCIFLUSH);
   return 0;

}

int setFocuserMotion(int fd, int motionType)
{
  int error_type;
  int nbytes_write=0;

  switch (motionType)
  {
    case LX200_FOCUSIN:
      if (lx200_debug)
         IDLog("%s Focus In Command [#:F+#]\n", __FUNCTION__);
    if ( (error_type = tty_write_string(fd, "#:F+#", &nbytes_write)) != TTY_OK)
    	return error_type;
      break;
    case LX200_FOCUSOUT:
      if (lx200_debug)
         IDLog("%s Focus Out Command [#:F-#]\n", __FUNCTION__);
    if ( (error_type = tty_write_string(fd, "#:F-#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
  }

  tcflush(fd, TCIFLUSH);
  return 0;
}

int setFocuserSpeedMode (int fd, int speedMode)
{
  int error_type;
  int nbytes_write=0;

 switch (speedMode)
 {
    case LX200_HALTFOCUS:
     if (lx200_debug)
        IDLog("%s Halt Focus Command [#:FQ#]\n", __FUNCTION__);
    if ( (error_type = tty_write_string(fd, "#:FQ#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
   case LX200_FOCUSSLOW:
     if (lx200_debug)
        IDLog("%s Focus Slow Command [#:FS#]\n", __FUNCTION__);
    if ( (error_type = tty_write_string(fd, "#:FS#", &nbytes_write)) != TTY_OK)
    	return error_type;
      break;
    case LX200_FOCUSFAST:
     if (lx200_debug)
        IDLog("%s Focus Fast Command [#:FF#]\n", __FUNCTION__);
     if ( (error_type = tty_write_string(fd, "#:FF#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
 }

 tcflush(fd, TCIFLUSH);
 return 0;
}

int setGPSFocuserSpeed (int fd, int speed)
{
  char speed_str[8];
  int error_type;
  int nbytes_write=0;

  if (speed == 0)
  {
      if (lx200_debug)
         IDLog("%s Command [#:FQ#]\n", __FUNCTION__);
     if ( (error_type = tty_write_string(fd, "#:FQ#", &nbytes_write)) != TTY_OK)
    	return error_type;

     return 0;
  }

  snprintf(speed_str, 8, "#:F%d#", speed);

  if (lx200_debug)
     IDLog("%s Command [%s]\n", __FUNCTION__, speed_str);

  if ( (error_type = tty_write_string(fd, speed_str, &nbytes_write)) != TTY_OK)
    	return error_type;

  tcflush(fd, TCIFLUSH);
  return 0;
}

int setTrackFreq(int fd, double trackF)
{
  char temp_string[16];

  if (lx200_debug)
     IDLog("%s\n", __FUNCTION__);

  snprintf(temp_string, sizeof( temp_string ), "#:ST %04.1f#", trackF);

  return (setStandardProcedure(fd, temp_string));

}

/**********************************************************************
* Misc
*********************************************************************/

int Slew(int fd)
{
    char slewNum[2];
    int error_type;
    int nbytes_write=0, nbytes_read=0;

    if (lx200_debug)
       IDLog("%s Slew Command [#:MS#]\n", __FUNCTION__);

    if ( (error_type = tty_write_string(fd, "#:MS#", &nbytes_write)) != TTY_OK)
    	return error_type;

    error_type = tty_read(fd, slewNum, 1, LX200_TIMEOUT, &nbytes_read);
    
    if (nbytes_read < 1)
    {
        if (lx200_debug)
           IDLog("%s Response error (%d)\n", __FUNCTION__, error_type);
      return error_type;
    }

    /* We don't need to read the string message, just return corresponding error code */
    tcflush(fd, TCIFLUSH);

    if (lx200_debug)
       IDLog("%s Response <%c>\n", __FUNCTION__, slewNum[0]);

    if (slewNum[0] == '0')
     return 0;
    else if  (slewNum[0] == '1')
      return 1;
    else return 2;

}

int MoveTo(int fd, int direction)
{
  int nbytes_write=0;

  switch (direction)
  {
    case LX200_NORTH:
      if (lx200_debug)
         IDLog("%s Motion Command [#:Mn#]\n", __FUNCTION__);
    tty_write_string(fd, "#:Mn#", &nbytes_write);
    break;
    case LX200_WEST:
      if (lx200_debug)
         IDLog("%s Motion Command [#:Mw#]\n", __FUNCTION__);
    tty_write_string(fd, "#:Mw#", &nbytes_write);
    break;
    case LX200_EAST:
      if (lx200_debug)
         IDLog("%s Motion Command [#:Me#]\n", __FUNCTION__);
    tty_write_string(fd, "#:Me#", &nbytes_write);
    break;
    case LX200_SOUTH:
      if (lx200_debug)
         IDLog("%s Motion Command [#:Ms#]\n", __FUNCTION__);
    tty_write_string(fd, "#:Ms#", &nbytes_write);
    break;
    default:
    break;
  }
  
  tcflush(fd, TCIFLUSH);
  return 0;
}

int SendPulseCmd(int fd, int direction, int duration_msec)
{
  int nbytes_write=0;
  char cmd[20];
  switch (direction)
  {
    case LX200_NORTH: sprintf(cmd, "#:Mgn%04d#", duration_msec); break;
    case LX200_SOUTH: sprintf(cmd, "#:Mgs%04d#", duration_msec); break;
    case LX200_EAST:  sprintf(cmd, "#:Mge%04d#", duration_msec); break;
    case LX200_WEST:  sprintf(cmd, "#:Mgw%04d#", duration_msec); break;
    default: return 1;
  }
  
  if (lx200_debug)
     IDLog("%s Command [%s]\n", __FUNCTION__, cmd);

  tty_write_string(fd, cmd, &nbytes_write);
  
  tcflush(fd, TCIFLUSH);
  return 0;
}

int HaltMovement(int fd, int direction)
{
    int error_type;
    int nbytes_write=0;

  switch (direction)
  {
    case LX200_NORTH:
      if (lx200_debug)
         IDLog("%s Stop Motion Command [#:Qn#]\n", __FUNCTION__);
       if ( (error_type = tty_write_string(fd, "#:Qn#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
    case LX200_WEST:
      if (lx200_debug)
         IDLog("%s Stop Motion Command [#:Qw#]\n", __FUNCTION__);
     if ( (error_type = tty_write_string(fd, "#:Qw#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
    case LX200_EAST:
      if (lx200_debug)
         IDLog("%s Stop Motion Command [#:Qe#]\n", __FUNCTION__);
      if ( (error_type = tty_write_string(fd, "#:Qe#", &nbytes_write)) != TTY_OK)
    	return error_type;
     break;
    case LX200_SOUTH:
      if (lx200_debug)
         IDLog("%s Stop Motion Command [#:Qs#]\n", __FUNCTION__);
    if ( (error_type = tty_write_string(fd, "#:Qs#", &nbytes_write)) != TTY_OK)
    	return error_type;
    break;
    case LX200_ALL:
      if (lx200_debug)
         IDLog("%s Stop Motion Command [#:Q#]\n", __FUNCTION__);
	if ( (error_type = tty_write_string(fd, "#:Q#", &nbytes_write)) != TTY_OK)
    	   return error_type;
     break;
    default:
     return -1;
    break;
  }
  
  tcflush(fd, TCIFLUSH);
  return 0;

}

int abortSlew(int fd)
{
  int error_type;
  int nbytes_write=0;

  if (lx200_debug)
     IDLog("%s Command [#:Q#]\n", __FUNCTION__);

 if ( (error_type = tty_write_string(fd, "#:Q#", &nbytes_write)) != TTY_OK)
    	   return error_type;

 tcflush(fd, TCIFLUSH);
 return 0;
}

int Sync(int fd, char *matchedObject)
{
  int error_type;
  int nbytes_write=0, nbytes_read=0;

  if (lx200_debug)
     IDLog("%s Command [#:CM#]\n", __FUNCTION__);

  if ( (error_type = tty_write_string(fd, "#:CM#", &nbytes_write)) != TTY_OK)
    	   return error_type;

    error_type = tty_read_section(fd, matchedObject, '#', LX200_TIMEOUT, &nbytes_read);
  
  if (nbytes_read < 1)
   return error_type;
   
  matchedObject[nbytes_read-1] = '\0';

  if (lx200_debug)
    IDLog("%s Matched Object: %s\n", __FUNCTION__, matchedObject);
  
  /* Sleep 10ms before flushing. This solves some issues with LX200 compatible devices. */
  usleep(10000);
  tcflush(fd, TCIFLUSH);

  return 0;
}

int selectSite(int fd, int siteNum)
{
  int error_type;
  int nbytes_write=0;

  switch (siteNum)
  {
    case 1:
      if (lx200_debug)
         IDLog("%s Command [#:W1#]\n", __FUNCTION__);
  	if ( (error_type = tty_write_string(fd, "#:W1#", &nbytes_write)) != TTY_OK)
    	   return error_type;
      break;
    case 2:
      if (lx200_debug)
         IDLog("%s Motion Command [#:W2#]\n", __FUNCTION__);
	if ( (error_type = tty_write_string(fd, "#:W2#", &nbytes_write)) != TTY_OK)
    	   return error_type;
      /*if (portWrite("#:W2#") < 0)
       return -1;*/
      break;
    case 3:
      if (lx200_debug)
         IDLog("%s Motion Command [#:W3#]\n", __FUNCTION__);
	if ( (error_type = tty_write_string(fd, "#:W3#", &nbytes_write)) != TTY_OK)
    	   return error_type;
      break;
    case 4:
      if (lx200_debug)
         IDLog("%s Motion Command [#:W4#]\n", __FUNCTION__);
	if ( (error_type = tty_write_string(fd, "#:W4#", &nbytes_write)) != TTY_OK)
    	   return error_type;
      break;
    default:
    return -1;
    break;
  }
  
  tcflush(fd, TCIFLUSH);
  return 0;

}

int selectCatalogObject(int fd, int catalog, int NNNN)
{
 char temp_string[16];
 int error_type;
 int nbytes_write=0;
 
 switch (catalog)
 {
   case LX200_STAR_C:
    snprintf(temp_string, sizeof( temp_string ), "#:LS%d#", NNNN);
    break;
   case LX200_DEEPSKY_C:
    snprintf(temp_string, sizeof( temp_string ), "#:LC%d#", NNNN);
    break;
   case LX200_MESSIER_C:
    snprintf(temp_string, sizeof( temp_string ), "#:LM%d#", NNNN);
    break;
   default:
    return -1;
  }

  if ( (error_type = tty_write_string(fd, temp_string, &nbytes_write)) != TTY_OK)
    	   return error_type;
  /*if (portWrite(temp_string) < 0)
   return -1;*/

  tcflush(fd, TCIFLUSH);
  return 0;
}

int selectSubCatalog(int fd, int catalog, int subCatalog)
{
  char temp_string[16];
  switch (catalog)
  {
    case LX200_STAR_C:
    snprintf(temp_string, sizeof( temp_string ), "#:LsD%d#", subCatalog);
    break;
    case LX200_DEEPSKY_C:
    snprintf(temp_string, sizeof( temp_string ), "#:LoD%d#", subCatalog);
    break;
    case LX200_MESSIER_C:
     return 1;
    default:
     return 0;
  }

  return (setStandardProcedure(fd, temp_string));
}

int checkLX200Format(int fd)
{
  char temp_string[16];
  controller_format = LX200_LONG_FORMAT;
  int error_type;
  int nbytes_write=0, nbytes_read=0;

  if (lx200_debug)
     IDLog("%s Command [#:GR#]\n", __FUNCTION__);

  if ( (error_type = tty_write_string(fd, "#:GR#", &nbytes_write)) != TTY_OK)
    	   return error_type;

  error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read);
  
  if (nbytes_read < 1)
  {
      if (lx200_debug)
         IDLog("%s Response error (%d)\n", __FUNCTION__, error_type);
   return error_type;
  }
   
  temp_string[nbytes_read - 1] = '\0';

  if (lx200_debug)
     IDLog("%s Response <%s>\n", __FUNCTION__, temp_string);

  /* Check whether it's short or long */
  if (temp_string[5] == '.')
  {
     controller_format = LX200_SHORT_FORMAT;
      return 0;
  }
  else
     return 0;
}

int selectTrackingMode(int fd, int trackMode)
{
 int error_type;
 int nbytes_write=0;
  
   switch (trackMode)
   {
    case LX200_TRACK_SIDEREAL:
       if (lx200_debug)
          IDLog("%s Sidereal Track Command [#:TQ#]\n", __FUNCTION__);
     if ( (error_type = tty_write_string(fd, "#:TQ#", &nbytes_write)) != TTY_OK)
    	   return error_type;
     break;
   case LX200_TRACK_SOLAR:
      if (lx200_debug)
         IDLog("%s Solar Track Command [#:TS#]\n", __FUNCTION__);
     if ( (error_type = tty_write_string(fd, "#:TS#", &nbytes_write)) != TTY_OK)
          return error_type;
        break;
    case LX200_TRACK_LUNAR:
       if (lx200_debug)
          IDLog("%s Lunar Track Command [#:TL#]\n", __FUNCTION__);
      if ( (error_type = tty_write_string(fd, "#:TL#", &nbytes_write)) != TTY_OK)
    	   return error_type;
      break;
   case LX200_TRACK_MANUAL:
       if (lx200_debug)
          IDLog("%s Manual Track Command [#:TM#]\n", __FUNCTION__);
     if ( (error_type = tty_write_string(fd, "#:TM#", &nbytes_write)) != TTY_OK)
    	   return error_type;
     break;
   default:
    return -1;
    break;
   }
   
   tcflush(fd, TCIFLUSH);
   return 0;

}

