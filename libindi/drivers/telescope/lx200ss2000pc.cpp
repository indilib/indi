/*
    SkySensor2000PC
    Copyright (C) 2015 Camiel Severijns

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

#include "lx200ss2000pc.h"
#include "lx200driver.h"

#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>


const int LX200SS2000PC::ShortTimeOut = 2;  // In seconds.
const int LX200SS2000PC::LongTimeOut  = 10; // In seconds.


LX200SS2000PC::LX200SS2000PC(void)
  : LX200Generic()
{
  setVersion(1, 0);
  SetTelescopeCapability(TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION, 4);
}


const char * LX200SS2000PC::getDefaultName(void) {
  return const_cast<const char *>("SkySensor2000PC");
}


bool LX200SS2000PC::updateProperties(void) {
  bool result = LX200Generic::updateProperties();
  if (isConnected()) {
    deleteProperty(AlignmentSP.name);
    deleteProperty(FocusModeSP.name);
    deleteProperty(FocusMotionSP.name);
    deleteProperty(FocusTimerNP.name);
    deleteProperty(SiteSP.name);
    deleteProperty(SiteNameTP.name);
    deleteProperty(TrackingFreqNP.name);
    deleteProperty(TrackModeSP.name);
    deleteProperty(UsePulseCmdSP.name);
  }
  return result;
}


bool LX200SS2000PC::updateTime(ln_date * utc, double utc_offset) {
  bool result = true;
  // This method is largely identical to the one in the LX200Generic class.
  // The difference is that it ensures that updates that require planetary
  // data to be recomputed by the SkySensor2000PC are only done when really
  // necessary because this takes quite some time.
  if (!isSimulation()) {
    result = false;
    struct ln_zonedate ltm;
    ln_date_to_zonedate(utc, &ltm, static_cast<long>(utc_offset*3600.0+0.5));
    DEBUGF(INDI::Logger::DBG_DEBUG, "New zonetime is %04d-%02d-%02d %02d:%02d:%06.3f (offset=%ld)", ltm.years, ltm.months, ltm.days, ltm.hours, ltm.minutes, ltm.seconds, ltm.gmtoff);
    JD = ln_get_julian_day(utc);
    DEBUGF(INDI::Logger::DBG_DEBUG, "New JD is %f", JD);
    if (setLocalTime(PortFD, ltm.hours, ltm.minutes, static_cast<int>(ltm.seconds+0.5)) < 0) {
      DEBUG(INDI::Logger::DBG_ERROR, "Error setting local time.");
    }
    else if (!setCalenderDate(ltm.years, ltm.months, ltm.days)) {
      DEBUG(INDI::Logger::DBG_ERROR, "Error setting local date.");
    }
    // Meade defines UTC Offset as the offset ADDED to local time to yield UTC, which
    // is the opposite of the standard definition of UTC offset!
    else if (!setUTCOffset(-static_cast<int>(utc_offset))) {
      DEBUG(INDI::Logger::DBG_ERROR, "Error setting UTC Offset.");
    }
    else {
      DEBUG(INDI::Logger::DBG_SESSION, "Time updated.");
      result = true;
    }
  }
  return result;
}


void LX200SS2000PC::getBasicData(void) {
  if (!isSimulation()) checkLX200Format(PortFD);
  sendScopeLocation();
  sendScopeTime();
}


bool LX200SS2000PC::isSlewComplete(void) {
  const double dx = targetRA - currentRA;
  const double dy = targetDEC - currentDEC;
  // These tolerances seem to work well. In case it turns out that these should be
  // user definable check the FS2 driver on how to implement this.
  return ( fabs(dx) <= 0.01 && fabs(dy) <= 0.01 );
}


bool LX200SS2000PC::getCalenderDate(int& year,int& month,int& day) {
  char date[16];
  bool result = ( getCommandString(PortFD, date, ":GC#") == 0 );
  DEBUGF(INDI::Logger::DBG_DEBUG, "LX200SS2000PC::getCalenderDate():: Date string from telescope: %s", date);
  if (result) {
    result = ( sscanf(date, "%d%*c%d%*c%d", &month, &day, &year) == 3 ); // Meade format is MM/DD/YY
    DEBUGF(INDI::Logger::DBG_DEBUG, "setCalenderDate: Date retrieved from telescope: %02d/%02d/%02d.", month, day, year);
    if (result) year += ( year > 50 ? 1900 : 2000 ); // Year 50 or later is in the 20th century, anything less is in the 21st century.
  }
 return result;
}


bool LX200SS2000PC::setCalenderDate(int year, int month, int day) {
  // This method differs from the setCalenderDate function in lx200driver.cpp
  // in that it reads and checks the complete respons from the SkySensor2000PC.
  // In addition, this method only sends the date when it differs from the date
  // of the SkySensor2000PC because the resulting update of the planetary data
  // takes quite some time.
  bool result = true;
  int  ss_year, ss_month, ss_day;
  const bool send_to_skysensor = (!getCalenderDate(ss_year,ss_month,ss_day) || year != ss_year || month != ss_month || day != ss_day );
  DEBUGF(INDI::Logger::DBG_DEBUG, "LX200SS2000PC::setCalenderDate(): Driver date %02d/%02d/%02d, SS2000PC date %02d/%02d/%02d.", month, day, year, ss_month, ss_day, ss_year);
  if (send_to_skysensor) {
    char buffer[64];
    int  nbytes_written = 0;
    snprintf(buffer, sizeof(buffer), ":SC %02d/%02d/%02d#", month, day, (year%100));
    result = ( tty_write_string(PortFD, buffer, &nbytes_written) == TTY_OK && nbytes_written == strlen(buffer) );
    if (result) {
      int nbytes_read = 0;
      result = ( tty_read(PortFD, buffer, 1, ShortTimeOut, &nbytes_read) == TTY_OK && nbytes_read == 1 && buffer[0] == '1' );    
      if (result) {
	if (tty_read_section(PortFD, buffer, '#', ShortTimeOut, &nbytes_read) != TTY_OK || strncmp(buffer,"Updating        planetary data#",24) != 0) {
	  DEBUGF(INDI::Logger::DBG_ERROR, "LX200SS2000PC::setCalenderDate(): Received unexpected first line '%s'.", buffer);
	  result = false;
	}
	else if (tty_read_section(PortFD, buffer, '#', LongTimeOut, &nbytes_read) != TTY_OK && strncmp(buffer,"                              #",24) != 0) {
	  DEBUGF(INDI::Logger::DBG_ERROR, "LX200SS2000PC::setCalenderDate(): Received unexpected second line '%s'.", buffer);
	  result = false;
	}
      }
    }
  }
  return result;
}


bool LX200SS2000PC::setUTCOffset(const int offset_in_hours) {
  bool result = true;
  int ss_timezone;
  const bool send_to_skysensor = ( getUTCOffset(PortFD,&ss_timezone) != 0 || offset_in_hours != ss_timezone );
  if (send_to_skysensor) {
    char temp_string[12];
    snprintf(temp_string, sizeof(temp_string), ":SG %+03d#", offset_in_hours);
    result = ( setStandardProcedure(PortFD, temp_string) == 0 );
  }
  return result;
}
