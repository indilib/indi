/*
    ZEQ25 INDI driver

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

#include <math.h>
#include <unistd.h>
#include <termios.h>

#include "indicom.h"
#include "lx200zeq25.h"
#include "lx200driver.h"

LX200ZEQ25::LX200ZEQ25()
{
    setVersion(1, 0);
}

bool LX200ZEQ25::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        // Delete unsupported properties
        deleteProperty(AlignmentSP.name);
        deleteProperty(FocusMotionSP.name);
        deleteProperty(FocusTimerNP.name);
        deleteProperty(FocusModeSP.name);
        deleteProperty(SiteSP.name);
        deleteProperty(SiteNameTP.name);
    }

    return true;
}

const char * LX200ZEQ25::getDefaultName()
{
    return (char *)"ZEQ25";
}

bool LX200ZEQ25::checkConnection()
{
    char initCMD[] = ":V#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUG(INDI::Logger::DBG_DEBUG, "Initializing IOptron using :V# CMD...");

    for (int i=0; i < 2; i++)
    {
        if ( (errcode = tty_write(PortFD, initCMD, 3, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            usleep(50000);
            continue;
        }

        if ( (errcode = tty_read_section(PortFD, response, '#', 3, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            usleep(50000);
            continue;
        }

        if (nbytes_read > 0)
        {
            response[nbytes_read] = '\0';
            DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

            if (!strcmp(response, "V1.00#"))
                return true;
        }

        usleep(50000);
    }

    return false;
}

bool LX200ZEQ25::isSlewComplete()
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read=0;
    int nbytes_written=0;

    strncpy(cmd, ":SE#", 16);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if ( (errcode = tty_write(PortFD, cmd, 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if ( (errcode = tty_read(PortFD, response, 1, 3, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(PortFD, TCIFLUSH);

        if (response[0] == '0')
            return true;
        else
            return false;
    }

    DEBUGF(INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool LX200ZEQ25::getMountInfo()
{
    char cmd[] = ":MountInfo#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if ( (errcode = tty_read(PortFD, response, 4, 3, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        if (nbytes_read == 4)
        {
            if (!strcmp(response, "8407"))
                DEBUG(INDI::Logger::DBG_SESSION, "Detected iEQ45/iEQ30 Mount.");
            else if (!strcmp(response, "8497"))
                DEBUG(INDI::Logger::DBG_SESSION, "Detected iEQ45 AA Mount.");
            else if (!strcmp(response, "8408"))
                DEBUG(INDI::Logger::DBG_SESSION, "Detected ZEQ25 Mount.");
            else if (!strcmp(response, "8498"))
                DEBUG(INDI::Logger::DBG_SESSION, "Detected SmartEQ Mount.");
            else
                DEBUG(INDI::Logger::DBG_SESSION, "Unknown mount detected.");

            tcflush(PortFD, TCIFLUSH);

            return true;
        }
    }

    DEBUGF(INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 4.", nbytes_read);
    return false;

}

void LX200ZEQ25::getBasicData()
{
    getMountInfo();
}

bool LX200ZEQ25::Goto(double r,double d)
{
    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    // If moving, let's stop it first.
    if (EqNP.s == IPS_BUSY)
    {
         if (!isSimulation() && abortSlew(PortFD) < 0)
         {
            AbortSP.s = IPS_ALERT;
            IDSetSwitch(&AbortSP, "Abort slew failed.");
            return false;
         }

         AbortSP.s = IPS_OK;
         EqNP.s       = IPS_IDLE;
         IDSetSwitch(&AbortSP, "Slew aborted.");
         IDSetNumber(&EqNP, NULL);

         if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
         {
                MovementNSSP.s  = MovementWESP.s =  IPS_IDLE;
                EqNP.s       = IPS_IDLE;
                IUResetSwitch(&MovementNSSP);
                IUResetSwitch(&MovementWESP);
                IDSetSwitch(&MovementNSSP, NULL);
                IDSetSwitch(&MovementWESP, NULL);
          }

       // sleep for 100 mseconds
       usleep(100000);
    }

    if (isSimulation() == false)
    {
        if (setObjectRA(PortFD, targetRA) < 0 || (setObjectDEC(PortFD, targetDEC)) < 0)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error setting RA/DEC.");
            return false;
        }

        if (ZEQ25Slew() == 0)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
            slewError(1);
            return  false;
        }
    }

    TrackState = SCOPE_SLEWING;
    EqNP.s    = IPS_BUSY;

    IDMessage(getDeviceName(), "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

int LX200ZEQ25::ZEQ25Slew()
{
    DEBUGF(DBG_SCOPE, "<%s>", __FUNCTION__);
    char slewNum[2];
    int error_type;
    int nbytes_write=0, nbytes_read=0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", ":MS#");

    if ( (error_type = tty_write_string(PortFD, ":MS#", &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read(PortFD, slewNum, 1, 3, &nbytes_read);

    if (nbytes_read < 1)
    {
        DEBUGF(DBG_SCOPE, "RES ERROR <%d>", error_type);
        return error_type;
    }

    /* We don't need to read the string message, just return corresponding error code */
    tcflush(PortFD, TCIFLUSH);

    DEBUGF(DBG_SCOPE, "RES <%c>", slewNum[0]);

    return slewNum[0];
}

bool LX200ZEQ25::updateTime(ln_date * utc, double utc_offset)
{
    struct ln_zonedate ltm;

    if (isSimulation())
        return true;

    ln_date_to_zonedate(utc, &ltm, utc_offset*3600.0);

    JD = ln_get_julian_day(utc);

    DEBUGF(INDI::Logger::DBG_DEBUG, "New JD is %f", (float) JD);

    // Set Local Time
    if (setLocalTime(PortFD, ltm.hours, ltm.minutes, ltm.seconds) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting local time.");
        return false;
    }

    if (setCalenderDate(PortFD, ltm.days, ltm.months, ltm.years) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting local date.");
        return false;
    }

    if (setZEQ25UTCOffset(utc_offset) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting UTC Offset.");
        return false;
    }

    return true;
}

bool LX200ZEQ25::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (isSimulation())
        return true;

    double final_longitude;

    if (longitude > 180)
        final_longitude = longitude - 360.0;
    else
        final_longitude = longitude;

    if (isSimulation() == false && setZEQ25Longitude(final_longitude) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting site longitude coordinates");
        return false;
    }

    if (isSimulation() == false && setZEQ25Latitude(latitude) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting site latitude coordinates");
        return false;
    }

    char l[32], L[32];
    fs_sexa (l, latitude, 3, 3600);
    fs_sexa (L, longitude, 4, 3600);

    IDMessage(getDeviceName(), "Site location updated to Lat %.32s - Long %.32s", l, L);

    return true;
}

int LX200ZEQ25::setZEQ25Longitude(double Long)
{
   int d, m, s;
   char sign;
   char temp_string[32];

   if (Long > 0)
       sign = '+';
   else
       sign = '-';

   getSexComponents(Long, &d, &m, &s);

   snprintf(temp_string, sizeof( temp_string ), ":Sg %c%03d:%02d:%02d#", sign, abs(d), m,s);

   return (setZEQ25StandardProcedure(PortFD, temp_string));
}

int LX200ZEQ25::setZEQ25Latitude(double Lat)
{
   int d, m, s;
   char sign;
   char temp_string[32];

   if (Lat > 0)
       sign = '+';
   else
       sign = '-';

   getSexComponents(Lat, &d, &m, &s);

   snprintf(temp_string, sizeof( temp_string ), ":St %c%02d:%02d:%02d#", sign, abs(d), m,s);

   return (setZEQ25StandardProcedure(PortFD, temp_string));
}

int LX200ZEQ25::setZEQ25UTCOffset(double hours)
{
   char temp_string[16];
   char sign;
   int h=0,m=0,s=0;

   if (hours > 0)
       sign = '+';
   else
       sign = '-';

   getSexComponents(hours, &h, &m, &s);

   snprintf(temp_string, sizeof( temp_string ), ":SG %c%02d:%02d#", sign, abs(h), m);

   return (setZEQ25StandardProcedure(PortFD, temp_string));

}

int LX200ZEQ25::setZEQ25StandardProcedure(int fd, const char * data)
{
 char bool_return[2];
 int error_type;
 int nbytes_write=0, nbytes_read=0;

 DEBUGF(DBG_SCOPE, "CMD <%s>", data);

 if ( (error_type = tty_write_string(fd, data, &nbytes_write)) != TTY_OK)
        return error_type;

error_type = tty_read(fd, bool_return, 1, 5, &nbytes_read);

// JM: Hack from Jon in the INDI forums to fix longitude/latitude settings failure on ZEQ25
usleep(10000);
tcflush(fd, TCIFLUSH);
usleep(10000);

 if (nbytes_read < 1)
   return error_type;

 DEBUGF(DBG_SCOPE, "RES <%c>", bool_return[0]);

 if (bool_return[0] == '0')
 {
     DEBUGF(DBG_SCOPE, "CMD <%s> failed.", data);
     return -1;
 }

 DEBUGF(DBG_SCOPE, "CMD <%s> successful.", data);

 return 0;

}

