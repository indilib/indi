/*
    Celestron driver

    Copyright (C) 2015 Jasem Mutlaq

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
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>

#include <libnova.h>
#include <map>

#include "indicom.h"
#include "indidevapi.h"
#include "indilogger.h"

#include "celestrondriver.h"

#define CELESTRON_TIMEOUT       5		/* FD timeout in seconds */
std::map<int, std::string> celestronModels;

bool celestron_debug = false;
bool celestron_simulation = false;
char celestron_device[MAXINDIDEVICE] = "Celestron GPS";
double currentRA, currentDEC, currentSlewRate;

struct
{
    double ra;
    double dec;
    double az;
    double alt;
    CELESTRON_GPS_STATUS gpsStatus;
    CELESTRON_SLEW_RATE slewRate;
    bool isSlewing;
} simData;

void set_celestron_debug(bool enable)
{
   celestron_debug = enable;
}

void set_celestron_simulation(bool enable)
{
    celestron_simulation = enable;
}

void set_celestron_device(const char *name)
{
    strncpy(celestron_device, name, MAXINDIDEVICE);
}

void set_sim_gps_status(CELESTRON_GPS_STATUS value)
{
    simData.gpsStatus = value;
}

void set_sim_slew_rate(CELESTRON_SLEW_RATE value)
{
    simData.slewRate = value;
}

void set_sim_slewing(bool isSlewing)
{
    simData.isSlewing = isSlewing;
}

void set_sim_ra(double ra)
{
    simData.ra = ra;
}

void set_sim_dec(double dec)
{
    simData.dec = dec;
}

void set_sim_az(double az)
{
    simData.az = az;
}

void set_sim_alt(double alt)
{
    simData.alt = alt;
}

bool check_celestron_connection(int fd)
{
  char initCMD[] = "Kx";
  int errcode = 0;
  char errmsg[MAXRBUF];
  char response[8];
  int nbytes_read=0;
  int nbytes_written=0;

  if (celestronModels.empty())
  {
      celestronModels[1]  = "GPS Series";
      celestronModels[3]  = "i-Series";
      celestronModels[4]  = "i-Series SE";
      celestronModels[5]  = "CGE";
      celestronModels[6]  = "Advanced GT";
      celestronModels[7]  = "SLT";
      celestronModels[9]  = "CPC";
      celestronModels[10] = "GT";
      celestronModels[11] = "4/5 SE";
      celestronModels[12] = "6/8 SE";
      celestronModels[14] = "CGEM DX";
  }

  DEBUGDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "Initializing Celestron using Kx CMD...");

  for (int i=0; i < 2; i++)
  {
      if (celestron_simulation)
      {
          strcpy(response, "x#");
          nbytes_read= strlen(response);
      }
      else
      {
          if ( (errcode = tty_write(fd, initCMD, 2, &nbytes_written)) != TTY_OK)
          {
              tty_error_msg(errcode, errmsg, MAXRBUF);
              DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
              usleep(50000);
              continue;
          }

          if ( (errcode = tty_read_section(fd, response, '#', CELESTRON_TIMEOUT, &nbytes_read)))
          {
              tty_error_msg(errcode, errmsg, MAXRBUF);
              DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
              usleep(50000);
              continue;
          }
      }

      if (nbytes_read > 0)
      {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        if (!strcmp(response, "x#"))
            return true;
      }

      usleep(50000);
  }

  return false;
}

bool get_celestron_firmware(int fd, FirmwareInfo *info)
{
    bool rc = false;

    DEBUGDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "Getting controller version...");
    rc = get_celestron_version(fd, info);

    if (rc == false)
        return false;

    if (info->controllerVersion >= 2.2)
    {
        DEBUGDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "Getting controller model...");
        rc = get_celestron_model(fd, info);
        if (rc == false)
            return rc;
    }
    else
        info->Model = "Unknown";

    DEBUGDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "Getting GPS firmware version...");
    rc = get_celestron_gps_firmware(fd, info);

    if (rc == false)
        return rc;

    DEBUGDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "Getting RA firmware version...");
    rc = get_celestron_ra_firmware(fd, info);

    if (rc == false)
        return rc;

    DEBUGDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "Getting DE firmware version...");
    rc = get_celestron_dec_firmware(fd, info);

    return rc;
}

bool get_celestron_version (int fd, FirmwareInfo *info)
{
    char cmd[] = "V";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (celestron_simulation)
    {
        char major = 4;
        char minor = 4;
        snprintf(response, 16, "%c%c#", major, minor);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read_section(fd, response, '#', CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%02X %02X %02X)", response[0],response[1], response[2]);

      if (nbytes_read == 3)
      {
         int major = response[0];
         int minor = response[1];

         char versionStr[8];
         snprintf(versionStr, 8, "%01d.%01d", major, minor);

         info->controllerVersion = atof(versionStr);
         info->Version  = versionStr;

         tcflush(fd, TCIFLUSH);

          return true;
      }
    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 3.", nbytes_read);
    return false;

}

bool get_celestron_model (int fd, FirmwareInfo *info)
{
    char cmd[] = "m";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (celestron_simulation)
    {
        char device = 6;
        snprintf(response, 16, "%c#", device);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read_section(fd, response, '#', CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%02X %02X)", response[0],response[1]);

      if (nbytes_read == 2)
      {
          response[1] = '\0';
          int model = response[0];

          if (model >= 1 && model <= 14)
            info->Model = celestronModels[model];
          else
          {
              DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_WARNING, "Unrecognized model (%d).", model);
              info->Model = "Unknown";
          }

          tcflush(fd, TCIFLUSH);

          return true;
      }
    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 2.", nbytes_read);
    return false;

}

bool get_celestron_ra_firmware(int fd, FirmwareInfo *info)
{
    unsigned char cmd[] = { 0x50, 0x01, 0x10, 0xFE, 0x0, 0x0, 0x0, 0x02};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%02X %02X %02X %02X %02X %02X %02X %02X)", cmd[0],cmd[1],cmd[2],cmd[3],cmd[4],cmd[5],cmd[6],cmd[7]);

    if (celestron_simulation)
    {
        char major = 1;
        char minor = 9;
        snprintf(response, 16, "%c%c#", major, minor);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, (char *) cmd, 8, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read_section(fd, response, '#', CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%02X %02X %02X)", response[0],response[1], response[2]);

      if (nbytes_read == 3)
      {
          int major = response[0];
          int minor = response[1];

          char versionStr[8];
          snprintf(versionStr, 8, "%01d.%01d", major, minor);

         info->RAFirmware  = versionStr;

         tcflush(fd, TCIFLUSH);

         return true;
      }

    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 3.", nbytes_read);
    return false;
}

bool get_celestron_dec_firmware(int fd, FirmwareInfo *info)
{
    unsigned char cmd[] = { 0x50, 0x01, 0x11, 0xFE, 0x0, 0x0, 0x0, 0x02};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%02X %02X %02X %02X %02X %02X %02X %02X)", cmd[0],cmd[1],cmd[2],cmd[3],cmd[4],cmd[5],cmd[6],cmd[7]);

    if (celestron_simulation)
    {
        char major = 1;
        char minor = 6;
        snprintf(response, 16, "%c%c#", major, minor);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, (char *) cmd, 8, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read_section(fd, response, '#', CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%02X %02X %02X)", response[0],response[1], response[2]);

      if (nbytes_read == 3)
      {          
          int major = response[0];
          int minor = response[1];

          char versionStr[8];
          snprintf(versionStr, 8, "%01d.%01d", major, minor);

         info->DEFirmware = versionStr;

         tcflush(fd, TCIFLUSH);

         return true;
      }

    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 3.", nbytes_read);
    return false;
}

bool get_celestron_gps_firmware(int fd, FirmwareInfo *info)
{
    unsigned char cmd[] = { 0x50, 0x01, 0x10, 0xFE, 0x0, 0x0, 0x0, 0x02};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%02X %02X %02X %02X %02X %02X %02X %02X)", cmd[0],cmd[1],cmd[2],cmd[3],cmd[4],cmd[5],cmd[6],cmd[7]);

    if (celestron_simulation)
    {
        char major = 1;
        char minor = 6;
        snprintf(response, 16, "%c%c#", major, minor);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, (char *) cmd, 8, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read_section(fd, response, '#', CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%02X %02X %02X)", response[0],response[1], response[2]);

      if (nbytes_read == 3)
      {
          int major = response[0];
          int minor = response[1];

          char versionStr[8];
          snprintf(versionStr, 8, "%01d.%01d", major, minor);

         info->GPSFirmware = versionStr;

         tcflush(fd, TCIFLUSH);

         return true;
      }

    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 3.", nbytes_read);
    return false;
}

bool start_celestron_motion(int fd, CELESTRON_DIRECTION dir, CELESTRON_SLEW_RATE rate)
{
    char cmd[] = { 0x50, 0x02, 0x11, 0x24, 0x09, 0x00, 0x00, 0x00};
    int errcode = 0;
    char errmsg[MAXRBUF];
    int nbytes_written=0, nbytes_read=0;
    char response[8];

    switch (dir)
    {
        case CELESTRON_N:
            cmd[2] = 0x11;
            cmd[3] = 0x24;
            cmd[4] = rate;
            break;

        case CELESTRON_S:
            cmd[2] = 0x11;
            cmd[3] = 0x25;
            cmd[4] = rate;
            break;

        case CELESTRON_W:
            cmd[2] = 0x10;
            cmd[3] = 0x24;
            cmd[4] = rate;
            break;

        case CELESTRON_E:
            cmd[2] = 0x10;
            cmd[3] = 0x25;
            cmd[4] = rate;
            break;
    }

   DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%02X %02X %02X %02X %02X %02X %02X %02X)", cmd[0],cmd[1],cmd[2],cmd[3],cmd[4],cmd[5],cmd[6],cmd[7]);

   if (celestron_simulation)
   {
       strcpy(response, "#");
       nbytes_read = strlen(response);
   }
   else
   {
       if ( (errcode = tty_write(fd, cmd, 8, &nbytes_written)) != TTY_OK)
       {
           tty_error_msg(errcode, errmsg, MAXRBUF);
           DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
           return false;
       }

       if ( (errcode = tty_read(fd, response, 1, CELESTRON_TIMEOUT, &nbytes_read)))
       {
           tty_error_msg(errcode, errmsg, MAXRBUF);
           DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
           return false;
       }

   }

   if (nbytes_read > 0)
   {
     response[nbytes_read] = '\0';
     DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

     tcflush(fd, TCIFLUSH);
     return true;
   }

   DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 1.", nbytes_read);
   return false;
}

bool stop_celestron_motion(int fd, CELESTRON_DIRECTION dir)
{
    char cmd[] = { 0x50, 0x02, 0x11, 0x24, 0x00, 0x00, 0x00, 0x00};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read=0;
    int nbytes_written=0;  

    switch (dir)
    {
        case CELESTRON_N:
            cmd[2] = 0x11;
            cmd[3] = 0x24;
            break;

        case CELESTRON_S:
            cmd[2] = 0x10;
            cmd[3] = 0x25;
            break;

       case CELESTRON_W:
            cmd[2] = 0x11;
            cmd[3] = 0x25;

       case CELESTRON_E:
            cmd[2] = 0x10;
            cmd[3] = 0x24;
            break;
    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%02X %02X %02X %02X %02X %02X %02X %02X)", cmd[0],cmd[1],cmd[2],cmd[3],cmd[4],cmd[5],cmd[6],cmd[7]);

    if (celestron_simulation)
    {
        strcpy(response, "#");
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, 8, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read(fd, response, 1, CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      tcflush(fd, TCIFLUSH);
      return true;
    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool abort_celestron(int fd)
{
    char cmd[] = "M";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (celestron_simulation)
    {
        strcpy(response, "#");
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read(fd, response, 1, CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      tcflush(fd, TCIFLUSH);
      return true;
    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool slew_celestron(int fd, double ra, double dec)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read=0;
    int nbytes_written=0;

    int ra_int, de_int;

    ra_int = get_ra_fraction(ra);
    de_int = get_de_fraction(dec);

    char RAStr[16], DecStr[16];
    fs_sexa(RAStr, ra, 2, 3600);
    fs_sexa(DecStr, dec, 2, 3600);

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "Goto (%s,%s)", RAStr, DecStr);

    snprintf(cmd, 16, "R%04X,%04X", ra_int, de_int);

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (celestron_simulation)
    {
        strcpy(response, "#");
        nbytes_read = strlen(response);
        set_sim_slewing(true);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read(fd, response, 1, CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      if (!strcmp(response, "#"))
      {
          tcflush(fd, TCIFLUSH);
          return true;
      }
      else
      {
          DEBUGDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Requested object is below horizon.");
          tcflush(fd, TCIFLUSH);
          return false;
      }

    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool slew_celestron_azalt(int fd, double az, double alt)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read=0;
    int nbytes_written=0;

    int az_int, alt_int;

    az_int  = get_angle_fraction(az);
    alt_int = get_angle_fraction(alt);

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, az, 3, 3600);
    fs_sexa(AltStr, alt, 2, 3600);

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "Goto AZM-ALT (%s,%s)", AzStr, AltStr);

    snprintf(cmd, 16, "B%04X,%04X", az_int, alt_int);

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (celestron_simulation)
    {
        strcpy(response, "#");
        nbytes_read = strlen(response);
        set_sim_slewing(true);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read(fd, response, 1, CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      if (!strcmp(response, "#"))
      {
          tcflush(fd, TCIFLUSH);
          return true;
      }
      else
      {
          DEBUGDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Requested object is below horizon.");
          tcflush(fd, TCIFLUSH);
          return false;
      }

    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool sync_celestron(int fd, double ra, double dec)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read=0;
    int nbytes_written=0;

    int ra_int, de_int;

    char RAStr[16], DecStr[16];
    fs_sexa(RAStr, ra, 2, 3600);
    fs_sexa(DecStr, dec, 2, 3600);
    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "Sync (%s,%s)", RAStr, DecStr);

    ra_int = get_ra_fraction(ra);
    de_int = get_de_fraction(dec);

    snprintf(cmd, 16, "S%04X,%04X", ra_int, de_int);

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (celestron_simulation)
    {
        simData.ra  = ra;
        simData.dec = dec;
        strcpy(response, "#");
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read(fd, response, 1, CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      if (!strcmp(response, "#"))
      {
          tcflush(fd, TCIFLUSH);
          return true;
      }
      else
      {
          DEBUGDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Requested object is below horizon.");
          tcflush(fd, TCIFLUSH);
          return false;
      }

    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool get_celestron_coords(int fd, double *ra, double *dec)
{
    char cmd[] = "E";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_EXTRA_1, "CMD (%s)", cmd);

    if (celestron_simulation)
    {
        int ra_int = get_ra_fraction(simData.ra);
        int de_int = get_de_fraction(simData.dec);
        snprintf(response, 16, "%04X,%04X#", ra_int, de_int);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read_section(fd, response, '#', CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      tcflush(fd, TCIFLUSH);
      response[nbytes_read] = '\0';      

      char ra_str[16], de_str[16];

      strncpy(ra_str, response, 4);
      strncpy(de_str, response+5, 4);

      int CELESTRONRA = strtol(ra_str, NULL, 16);
      int CELESTRONDE = strtol(de_str, NULL, 16);

      *ra  = (CELESTRONRA / 65536.0) * (360.0 / 15.0);
      *dec = (CELESTRONDE / 65536.0) * 360.0;

      /* Account for the quadrant in declination
       * Author:  John Kielkopf (kielkopf@louisville.edu)  */
        /* 90 to 180 */
        if ( (*dec > 90.) && (*dec <= 180.) )
          *dec = 180. - *dec;
        /* 180 to 270 */
        if ( (*dec > 180.) && (*dec <= 270.) )
          *dec = *dec - 270.;
        /* 270 to 360 */
        if ( (*dec > 270.) && (*dec <= 360.) )
          *dec = *dec - 360.;

        char RAStr[16], DecStr[16];
        fs_sexa(RAStr, *ra, 2, 3600);
        fs_sexa(DecStr, *dec, 2, 3600);
        DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_EXTRA_1, "RES (%s) ==> (%s,%s)", response, RAStr, DecStr);

        return true;
    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 10.", nbytes_read);
    return false;
}

bool get_celestron_coords_azalt(int fd, double *az, double *alt)
{
    char cmd[] = "Z";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_EXTRA_1, "CMD (%s)", cmd);

    if (celestron_simulation)
    {
        int az_int = get_angle_fraction(simData.az);
        int alt_int = get_angle_fraction(simData.alt);
        snprintf(response, 16, "%04X,%04X#", az_int, alt_int);
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read_section(fd, response, '#', CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      tcflush(fd, TCIFLUSH);
      response[nbytes_read] = '\0';

      char az_str[16], alt_str[16];

      strncpy(az_str, response, 4);
      strncpy(alt_str, response+5, 4);

      int CELESTRONAZ  = strtol(az_str, NULL, 16);
      int CELESTRONALT = strtol(alt_str, NULL, 16);

      *az  = (CELESTRONAZ / 65536.0) * 360.0;
      *alt = (CELESTRONALT / 65536.0) * 360.0;

      if (*alt > 90)
          *alt -= 360;

      char AzStr[16], AltStr[16];
      fs_sexa(AzStr, *az, 3, 3600);
      fs_sexa(AltStr, *alt, 2, 3600);
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_EXTRA_1, "RES (%s) ==> AZM-ALT (%s,%s)", response, AzStr, AltStr);

      return true;
    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 10.", nbytes_read);
    return false;
}

bool set_celestron_location(int fd, double longitude, double latitude)
{    
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read=0;
    int nbytes_written=0;

    int lat_d, lat_m, lat_s;
    int long_d, long_m ,long_s;

    // Convert from INDI standard to regular east/west -180 to 180
    if (longitude > 180)
        longitude -= 360;

    getSexComponents(latitude, &lat_d, &lat_m, &lat_s);
    getSexComponents(longitude, &long_d, &long_m, &long_s);

    cmd[0] = 'W';
    cmd[1] = abs(lat_d);
    cmd[2] = lat_m;
    cmd[3] = lat_s;
    cmd[4] = lat_d > 0 ? 0 : 1;
    cmd[5] = abs(long_d);
    cmd[6] = long_m;
    cmd[7] = long_s;
    cmd[8] = long_d > 0 ? 0 : 1;

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%02X %02X %02X %02X %02X %02X %02X %02X %02X)", cmd[0],cmd[1],cmd[2],cmd[3],cmd[4],cmd[5],cmd[6],cmd[7], cmd[8]);

    if (celestron_simulation)
    {
        strcpy(response, "#");
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, 9, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read(fd, response, 1, CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      tcflush(fd, TCIFLUSH);
      return true;
    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 1.", nbytes_read);
    return false;

}

bool set_celestron_datetime(int fd, struct ln_date *utc, double utc_offset)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read=0;
    int nbytes_written=0;

    struct ln_zonedate local_date;

    // Celestron takes local time
    ln_date_to_zonedate(utc, &local_date, utc_offset*3600);

    cmd[0] = 'H';
    cmd[1] = local_date.hours;
    cmd[2] = local_date.minutes;
    cmd[3] = local_date.seconds;
    cmd[4] = local_date.months;
    cmd[5] = local_date.days;
    cmd[6] = local_date.years - 2000;

    if (utc_offset < 0)
        cmd[7] = 256 - ((unsigned int) fabs(utc_offset));
    else
        cmd[7] = ((unsigned int) fabs(utc_offset));

    // Always assume standard time
    cmd[8] = 0;

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%02X %02X %02X %02X %02X %02X %02X %02X %02X)", cmd[0],cmd[1],cmd[2],cmd[3],cmd[4],cmd[5],cmd[6],cmd[7], cmd[8]);

    if (celestron_simulation)
    {
        strcpy(response, "#");
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, 9, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read(fd, response, 1, CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      tcflush(fd, TCIFLUSH);
      return true;
    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool get_celestron_utc_date_time(int fd, double *utc_hours, int *yy, int *mm, int *dd, int *hh, int *minute, int *ss)
{
    char cmd[] = "h";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[32];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (celestron_simulation)
    {
        // HH MM SS MONTH DAY YEAR OFFSET DAYLIGHT
        response[0] = 17;
        response[1] = 30;
        response[2] = 10;
        response[3] = 4;
        response[4] = 1;
        response[5] = 15;
        response[6] = 3;
        response[7] = 0;
        response[8] = '#';
        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read_section(fd, response, '#', CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      tcflush(fd, TCIFLUSH);
      response[nbytes_read] = '\0';
      unsigned char *res = (unsigned char *) response;
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%02X %02X %02X %02X %02X %02X %02X %02X)", res[0],res[1],res[2],res[3],res[4],res[5],res[6],res[7]);

      // HH MM SS MONTH DAY YEAR OFFSET DAYLIGHT
      *hh        = res[0];
      *minute    = res[1];
      *ss        = res[2];
      *mm        = res[3];
      *dd        = res[4];
      *yy        = res[5] + 2000;
      *utc_hours = res[6];

      if (*utc_hours > 12)
          *utc_hours -= 256;

      ln_zonedate localTime;
      ln_date     utcTime;

      localTime.years   = *yy;
      localTime.months  = *mm;
      localTime.days    = *dd;
      localTime.hours   = *hh;
      localTime.minutes = *minute;
      localTime.seconds = *ss;
      localTime.gmtoff  = *utc_hours * 3600;

      ln_zonedate_to_date(&localTime, &utcTime);

      *yy = utcTime.years;
      *mm = utcTime.months;
      *dd = utcTime.days;
      *hh = utcTime.hours;
      *minute = utcTime.minutes;
      *ss = utcTime.seconds;

      return true;

    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool is_scope_slewing(int fd)
{
    char cmd[] = "L";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (celestron_simulation)
    {
        if (simData.isSlewing)
            strcpy(response, "1#");
        else
            strcpy(response, "0#");

        nbytes_read = strlen(response);
    }
    else
    {
        if ( (errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ( (errcode = tty_read(fd, response, 2, CELESTRON_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
      response[nbytes_read] = '\0';
      DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

      tcflush(fd, TCIFLUSH);

      if (response[0] == '0')
          return false;
      else
          return true;

    }

    DEBUGFDEVICE(celestron_device, INDI::Logger::DBG_ERROR, "Received #%d bytes, expected 1.", nbytes_read);
    return false;
}

unsigned int get_angle_fraction(double angle)
{
    if (angle >= 0)
        return ((unsigned int) (angle * 65536 / 360.0));
    else
        return ((unsigned int) ((angle+360) * 65536 / 360.0));
}

unsigned int get_ra_fraction(double ra)
{
    return ((unsigned int) (ra * 15.0 * 65536 / 360.0));
}

unsigned int get_de_fraction(double de)
{
    unsigned int de_int=0;

    if (de >= 0)
        de_int = (unsigned int) (de * 65536 / 360.0);
    else
        de_int = (unsigned int) ((de+360.0) * 65536 / 360.0);

    return de_int;
}


