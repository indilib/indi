/*
    Pulsar2 INDI driver

    Copyright (C) 2016 Jasem Mutlaq and Camiel Severijns

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

#include <cmath>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "indicom.h"
#include "indilogger.h"
#include "lx200pulsar2.h"
#include "lx200driver.h"


#define PULSAR2_BUF             32
#define PULSAR2_TIMEOUT         3


extern char lx200Name[MAXINDIDEVICE];
extern unsigned int DBG_SCOPE;


namespace Pulsar2Commands {
  // Reimplement some of the standard LX200 commands to solve intermittent problems
  // with tcflush() calls on the input stream.
  
  bool send(const int fd,const char* cmd) {
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", cmd);
    const int nbytes = strlen(cmd);
    int       nbytes_written = 0;
    do {
      const int errcode = tty_write(fd,&cmd[nbytes_written],nbytes-nbytes_written,&nbytes_written);
      if (errcode != TTY_OK) {
	char errmsg[MAXRBUF];
	tty_error_msg(errcode, errmsg, MAXRBUF);
	DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Error: %s", errmsg);
	return false;
      }
    } while (nbytes_written < nbytes);
    return true;
  }


  bool confirmed(const int fd,const char* cmd,char& response) { // Single character response is expected indicating success or failure
    if (send(fd,cmd)) {
      int nbytes_read = 0;
      const int errcode = tty_read(fd,&response,1,PULSAR2_TIMEOUT,&nbytes_read);
      if (errcode != TTY_OK) {
	char errmsg[MAXRBUF];
	tty_error_msg(errcode, errmsg, MAXRBUF);
	DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Error: %s", errmsg);
	return false;
      }
      if (nbytes_read == 1) {
	DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%c>", response);
	return true;
      }
      DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Received %d bytes, expected 1.", nbytes_read);
    }
    return false;
  }


  bool receive(const int fd,char response[]) {
    int nbytes_read = 0;
    int retry_count;
    for (retry_count = 0; nbytes_read < 2; ++retry_count) { // Ignore empty response strings!!
      const int error_type = tty_read_section(fd,response,'#',PULSAR2_TIMEOUT,&nbytes_read);
      if (error_type != TTY_OK)
	return false;
    }
    response[nbytes_read-1] = '\0';
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s> (%d attempts)",response,retry_count);
    return true;
  }

  
  bool getString(const int fd,const char* cmd,char response[]) {
    return ( send(fd,cmd) && receive(fd,response) );
  }


  bool getInt(int fd,const char* cmd,int* value) {
    char response[16];
    bool success = getString(fd,cmd,response);
    if (success) {
      // if (strchr(response,'.')) {
      // 	float temp_number = 0.0;
      // 	success = ( sscanf(response, "%f", &temp_number) == 1 );
      // 	if (success)
      // 	  *value = static_cast<int>(temp_number);
      // }
      // else
      success = ( sscanf(response, "%d", value) == 1 );
      if (success)
	DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%d]", *value);
      else 
	DEBUGDEVICE(lx200Name, DBG_SCOPE, "Unable to parse response");
    }
    return success;
  }


  bool getSexa(const int fd,const char* cmd,double* value) {
    char response[16];
    bool success = getString(fd,cmd,response);
    if (success) {
      success = ( f_scansexa(response, value) == 0 );
      if (success)
	DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%d]", *value);
      else
	DEBUGDEVICE(lx200Name, DBG_SCOPE, "Unable to parse response");
    }
    return success;
  }


  bool setDegreesMinutes(const int fd,const char* cmd,const double value) {
   int degrees, minutes, seconds;
   getSexComponents(value,&degrees,&minutes,&seconds);
   char full_cmd[32];
   snprintf(full_cmd,sizeof(full_cmd),"#:%s %03d:%02d#",cmd,degrees,minutes);
   char response;
   return ( confirmed(fd,full_cmd,response) && response == '1' );
  }


  bool setTime(const int fd,const int h,const int m,const int s) {
    char full_cmd[32];
    snprintf(full_cmd,sizeof(full_cmd),"#:SL %02d:%02d:%02d#",h,m,s);
    char response;
    return ( confirmed(fd,full_cmd,response) && response == '1' );
  }


  bool setDate(const int fd,const int dd,const int mm,const int yy) {
    char cmd[64];
    snprintf(cmd,sizeof(cmd),":SC %02d/%02d/%02d#",mm,dd,(yy%100));
    char response;
    const bool success = ( confirmed(fd,cmd,response) && response == '1' );
    if (success) {
      // Read dumped data
      char dumpPlanetaryUpdateString[64];
      int  nbytes_read = 0;
      (void) tty_read_section(fd,dumpPlanetaryUpdateString,'#',1,&nbytes_read);
      (void) tty_read_section(fd,dumpPlanetaryUpdateString,'#',1,&nbytes_read);
    }
    return success;
  }

  
};




LX200Pulsar2::LX200Pulsar2(void)
  : LX200Generic(), can_pulse_guide(false), just_started_slewing(false)
{
  setVersion(1, 0);
  SetTelescopeCapability(TELESCOPE_CAN_SYNC | TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION, 4);
}


bool LX200Pulsar2::updateProperties(void) {
  LX200Generic::updateProperties();
  
  if (isConnected()) {
    defineSwitch(&PierSideSP);
    defineSwitch(&PeriodicErrorCorrectionSP);
    defineSwitch(&PoleCrossingSP);
    defineSwitch(&RefractionCorrectionSP);
    // Delete unsupported properties
    deleteProperty(AlignmentSP.name);
    deleteProperty(FocusMotionSP.name);
    deleteProperty(FocusTimerNP.name);
    deleteProperty(FocusModeSP.name);
    deleteProperty(SiteSP.name);
    deleteProperty(SiteNameTP.name);
    deleteProperty(TrackingFreqNP.name);
    deleteProperty(TrackModeSP.name);
    deleteProperty(ActiveDeviceTP.name);
    if (!can_pulse_guide) deleteProperty(UsePulseCmdSP.name);

    getBasicData();
  }
  else {
    deleteProperty(PierSideSP.name);
    deleteProperty(PeriodicErrorCorrectionSP.name);
    deleteProperty(PoleCrossingSP.name);
    deleteProperty(RefractionCorrectionSP.name);
  }

  return true;
}


bool LX200Pulsar2::initProperties(void) {
  const bool result = LX200Generic::initProperties();
  if (result) {
    IUFillSwitch(&PierSideS[0], "EAST_OF_PIER", "East", ISS_OFF);
    IUFillSwitch(&PierSideS[1], "WEST_OF_PIER", "West", ISS_ON);
    IUFillSwitchVector(&PierSideSP, PierSideS, 2, getDeviceName(), "PIER_SIDE", "Side of Pier", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    IUFillSwitch(&PeriodicErrorCorrectionS[0], "PEC_OFF", "Off", ISS_OFF);
    IUFillSwitch(&PeriodicErrorCorrectionS[1], "PEC_ON",  "On",  ISS_ON);
    IUFillSwitchVector(&PeriodicErrorCorrectionSP, PeriodicErrorCorrectionS, 2, getDeviceName(), "PE_CORRECTION", "P.E. Correction", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    IUFillSwitch(&PoleCrossingS[0], "POLE_CROSS_OFF", "Off", ISS_OFF);
    IUFillSwitch(&PoleCrossingS[1], "POLE_CROSS_ON",  "On",  ISS_ON);
    IUFillSwitchVector(&PoleCrossingSP, PoleCrossingS, 2, getDeviceName(), "POLE_CROSSING", "Pole Crossing", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    IUFillSwitch(&RefractionCorrectionS[0], "REFR_CORR_OFF", "Off", ISS_OFF);
    IUFillSwitch(&RefractionCorrectionS[1], "REFR_CORR_ON",  "On",  ISS_ON);
    IUFillSwitchVector(&RefractionCorrectionSP, RefractionCorrectionS, 2, getDeviceName(), "REFR_CORRECTION", "Refraction Corr.", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  }
  return result;
}


void LX200Pulsar2::ISGetProperties(const char *dev) {
  if (dev && strcmp(dev,getDeviceName()))
    return;

  LX200Generic::ISGetProperties(dev);
  
  if (isConnected()) {
    defineSwitch(&PierSideSP);
    defineSwitch(&PeriodicErrorCorrectionSP);
    defineSwitch(&PoleCrossingSP);
    defineSwitch(&RefractionCorrectionSP);
  }
}


bool LX200Pulsar2::Connect(void) {
  const bool success = LX200Generic::Connect();
  if (success) {
    if (isParked()) {
      DEBUGF(INDI::Logger::DBG_DEBUG, "%s", "Trying to wake up the mount.");
      UnPark();
    }
    else
      DEBUGF(INDI::Logger::DBG_DEBUG, "%s", "The mount is already tracking.");
  }
  return success;
}


bool LX200Pulsar2::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) {
  if (strcmp(dev, getDeviceName()) == 0) {
    if (strcmp(name, PierSideSP.name) == 0) {
      if (IUUpdateSwitch(&PierSideSP, states, names, n) < 0)
	return false;

      if (!isSimulation()) {
	// Define which side of the pier the telescope is.
	// Required for the sync command. This is *not*
	// related to a meridian flip.
	static char cmd[] = "#:YSN_#";
	cmd[5] = ( PierSideS[1].s == ISS_ON ? '1' : '0' );
	char response;
	if (Pulsar2Commands::confirmed(PortFD,cmd,response)) {
	  if (response == '1') {
	    PierSideSP.s = IPS_OK;
	    IDSetSwitch(&PierSideSP, NULL);
	    return true;
	  }
	  else {
	    PierSideSP.s = IPS_ALERT;
	    IDSetSwitch(&PierSideSP, "Could not set side of mount");
	    return false;
	  }
	}
	PierSideSP.s = IPS_ALERT;
	IDSetSwitch(&PierSideSP, "Unexpected response");
	return false;
      }
    }

    if (strcmp(name, PeriodicErrorCorrectionSP.name) == 0) {
      if (IUUpdateSwitch(&PeriodicErrorCorrectionSP, states, names, n) < 0)
	return false;

      if (!isSimulation()) {
	// Only control PEC in RA. PEC in decl. doesn´t seem usefull.
	static char cmd[] = "#:YSP_,0#";
	cmd[5] = ( PeriodicErrorCorrectionS[1].s == ISS_ON ? '1' : '0' );
	char response;
	if (Pulsar2Commands::confirmed(PortFD,cmd,response)) {
	  if (response == '1') {
	    PeriodicErrorCorrectionSP.s = IPS_OK;
	    IDSetSwitch(&PeriodicErrorCorrectionSP, NULL);
	    return true;
	  }
	  else {
	    PeriodicErrorCorrectionSP.s = IPS_ALERT;
	    IDSetSwitch(&PeriodicErrorCorrectionSP, "Could not change the periodic error correction");
	    return false;
	  }
	}
	PeriodicErrorCorrectionSP.s = IPS_ALERT;
	IDSetSwitch(&PeriodicErrorCorrectionSP, "Unexpected response");
	return false;
      }
    }

    if (strcmp(name, PoleCrossingSP.name) == 0) {
      if (IUUpdateSwitch(&PoleCrossingSP, states, names, n) < 0)
	return false;

      if (!isSimulation()) {
	static char cmd[] = "#:YSQ_#";
	cmd[5] = ( PoleCrossingS[1].s == ISS_ON ? '1' : '0' );
	char response;
	if (Pulsar2Commands::confirmed(PortFD,cmd,response)) {
	  if (response == '1') {
	    PoleCrossingSP.s = IPS_OK;
	    IDSetSwitch(&PoleCrossingSP, NULL);
	    return true;
	  }
	  else {
	    PoleCrossingSP.s = IPS_ALERT;
	    IDSetSwitch(&PoleCrossingSP, "Could not change the pole crossing");
	    return false;
	  }
	}
	PoleCrossingSP.s = IPS_ALERT;
	IDSetSwitch(&PoleCrossingSP, "Unexpected response");
	return false;
      }
    }

    if (strcmp(name, RefractionCorrectionSP.name) == 0) {
      if (IUUpdateSwitch(&RefractionCorrectionSP, states, names, n) < 0)
	return false;

      if (!isSimulation()) {
	// Control refraction correction in both RA and decl.
	static char cmd[] = "#:YSR_,_#";
	cmd[7] = cmd[5] = ( RefractionCorrectionS[1].s == ISS_ON ? '1' : '0' );
	char response;
	if (Pulsar2Commands::confirmed(PortFD,cmd,response)) {
	  if (response == '1') {
	    RefractionCorrectionSP.s = IPS_OK;
	    IDSetSwitch(&RefractionCorrectionSP, NULL);
	    return true;
	  }
	  else {
	    RefractionCorrectionSP.s = IPS_ALERT;
	    IDSetSwitch(&RefractionCorrectionSP, "Could not change the refraction correction");
	    return false;
	  }
	}
	RefractionCorrectionSP.s = IPS_ALERT;
	IDSetSwitch(&RefractionCorrectionSP, "Unexpected response");
	return false;
      }
    }
  }
  //  Nobody has claimed this, so pass it to the parent
  return LX200Generic::ISNewSwitch(dev,name,states,names,n);
}


bool LX200Pulsar2::ISNewText(const char* dev, const char* name, char* texts[], char* names[], int n) {
  if(strcmp(dev,getDeviceName())==0) {
    // Nothing to do yet
  }
  return LX200Generic::ISNewText(dev, name, texts, names, n);
}


const char * LX200Pulsar2::getDefaultName(void) {
  return static_cast<const char *>("Pulsar2");
}


bool LX200Pulsar2::checkConnection(void) {
  if (LX200Generic::checkConnection()) {
    DEBUG(INDI::Logger::DBG_DEBUG, "Checking Pulsar2 version ...");
    for (int i = 0; i < 2; ++i) {
      char response[PULSAR2_BUF];
      if (Pulsar2Commands::getString(PortFD,":YV#",response)) {
	// Determine which Pulsar2 version this is. Expected response similar to: 'PULSAR V2.66aR  ,2008.12.10.     #'
	char version[16];
	int  year, month, day;
	if (sscanf(response, "PULSAR V%8s ,%4d.%2d.%2d. ", version, &year, &month, &day) == 4) {
	  //TODO: Replace this with a check that indicates that this is a Pulsar2 which can pulse guide.
	  can_pulse_guide = ( version[0] == '3' );
	}
	DEBUGF(INDI::Logger::DBG_SESSION, "%s %04d.%02d.%02d", version, year, month, day);
	return true;
      }
      usleep(50000);
    }
  }
  return false;
}


bool LX200Pulsar2::Goto(double r,double d) {
  char RAStr[64], DecStr[64];
  fs_sexa(RAStr,  targetRA  = r, 2, 3600);
  fs_sexa(DecStr, targetDEC = d, 2, 3600);

  // If moving, let's stop it first.
  if (EqNP.s == IPS_BUSY) {
    if (!isSimulation() && abortSlew(PortFD) < 0) {
      AbortSP.s = IPS_ALERT;
      IDSetSwitch(&AbortSP, "Abort slew failed.");
      return false;
    }

    AbortSP.s = IPS_OK;
    EqNP.s       = IPS_IDLE;
    IDSetSwitch(&AbortSP, "Slew aborted.");
    IDSetNumber(&EqNP, NULL);

    if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY) {
      MovementNSSP.s  = MovementWESP.s =  IPS_IDLE;
      EqNP.s       = IPS_IDLE;
      IUResetSwitch(&MovementNSSP);
      IUResetSwitch(&MovementWESP);
      IDSetSwitch(&MovementNSSP, NULL);
      IDSetSwitch(&MovementWESP, NULL);
    }
    usleep(100000); // sleep for 100 mseconds
  }
  
  if (!isSimulation()) {
    if (setObjectRA(PortFD, targetRA) < 0 || (setObjectDEC(PortFD, targetDEC)) < 0) {
      EqNP.s = IPS_ALERT;
      IDSetNumber(&EqNP, "Error setting RA/DEC.");
      return false;
    }
    if (!startSlew()) {
      EqNP.s = IPS_ALERT;
      IDSetNumber(&EqNP, "Error Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
      slewError(3);
      return false;
    }
    just_started_slewing = true;
  }

  TrackState = SCOPE_SLEWING;
  EqNP.s     = IPS_BUSY;
  DEBUGF(INDI::Logger::DBG_SESSION, "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
  return true;
}


bool LX200Pulsar2::isSlewComplete(void) {
  bool result = false;
  switch (TrackState) {
  case SCOPE_SLEWING:
    result = !isSlewing();
    break;
  case SCOPE_PARKING:
    result = !isParking();
    break;
  default:
    break;
  }
  return result;
}


void LX200Pulsar2::getBasicData(void) {
  if (!isSimulation()) {
    if ( !Pulsar2Commands::getSexa(PortFD,"#:GR#",&currentRA) || !Pulsar2Commands::getSexa(PortFD,"#:GD#",&currentDEC) ) {
      EqNP.s = IPS_ALERT;
      IDSetNumber(&EqNP, "Error reading RA/DEC.");
      return;
    }
    NewRaDec(currentRA,currentDEC);

    int west_of_pier = 0;
    if (Pulsar2Commands::getInt(PortFD,"#:YGN#",&west_of_pier) < 0) {
      PierSideSP.s = IPS_ALERT;
      IDSetSwitch(&PierSideSP, "Can't check at which side of the pier the telescope is.");
    }
    PierSideS[west_of_pier].s = ISS_ON;
    IDSetSwitch(&PierSideSP, NULL);

    char periodic_error_correction[PULSAR2_BUF]; // There are separate values for RA and DEC but we only use the RA value for now
    if (!Pulsar2Commands::getString(PortFD,"#:YGP#",periodic_error_correction)) {
      PeriodicErrorCorrectionSP.s = IPS_ALERT;
      IDSetSwitch(&PeriodicErrorCorrectionSP, "Can't check whether PEC is enabled.");
    }
    PeriodicErrorCorrectionS[periodic_error_correction[0] == '1' ? 0 : 1].s = ISS_ON;
    IDSetSwitch(&PeriodicErrorCorrectionSP, NULL);

    int pole_crossing = 0;
    if (Pulsar2Commands::getInt(PortFD,"#:YGQ#",&pole_crossing) < 0) {
      PoleCrossingSP.s = IPS_ALERT;
      IDSetSwitch(&PoleCrossingSP, "Can't check whether pole crossing is enabled.");
    }
    PoleCrossingS[pole_crossing].s = ISS_ON;
    IDSetSwitch(&PoleCrossingSP, NULL);

    char refraction_correction[PULSAR2_BUF]; // There are separate values for RA and DEC but we only use the RA value for now
    if (!Pulsar2Commands::getString(PortFD,"#:YGR#",refraction_correction)) {
      RefractionCorrectionSP.s = IPS_ALERT;
      IDSetSwitch(&RefractionCorrectionSP, "Can't check whether refraction correction is enabled.");
    }
    RefractionCorrectionS[refraction_correction[0] == '1' ? 1 : 0].s = ISS_ON;
    IDSetSwitch(&RefractionCorrectionSP, NULL);
  }
  sendScopeLocation();
  sendScopeTime();
}


bool LX200Pulsar2::Sync(double ra, double dec) {
  bool result = true;
  if (!isSimulation()) {
    result = !( setObjectRA(PortFD, ra) < 0 || setObjectDEC(PortFD, dec) < 0 );
    if (!result) {
      EqNP.s = IPS_ALERT;
      IDSetNumber(&EqNP, "Error setting RA/DEC. Unable to Sync.");
    }
    else {
      result = Pulsar2Commands::send(PortFD,"#:CM#");
      if (result) {
#if 1
	// Somehow the response string is not being received. Also timeouts don't make
	// the read stop. For now, sleep a second and then flush all input that might
	// have received. The Pulsar2 controller has performed the sync anyways.
	usleep(1000000L);
	tcflush(PortFD, TCIFLUSH);
#else
	DEBUG(INDI::Logger::DBG_SESSION, "Reading sync response");
	// Pulsar2 sends coordinates separated by # characters (<RA>#<Dec>#)
	char RAresponse[PULSAR2_BUF];
	result = Pulsar2Commands::receive(PortFD,RAresponse);
	if (result) {
	  DEBUGF(INDI::Logger::DBG_DEBUG, "First synchronization string: '%s'.", RAresponse);
	  char DECresponse[PULSAR2_BUF];
	  result = Pulsar2Commands::receive(PortFD,DECresponse);
	  if (result)
	    DEBUGF(INDI::Logger::DBG_DEBUG, "Second synchronization string: '%s'.", DECresponse);
	}
	//TODO: Check that the received coordinates match the original coordinates
	if (!result) {
	  EqNP.s = IPS_ALERT;
	  IDSetNumber(&EqNP , "Synchronization failed.");
	}
#endif
      }
    }
  }
  if (result) {
    currentRA  = ra;
    currentDEC = dec;
    DEBUG(INDI::Logger::DBG_SESSION, "Synchronization successful.");
    TrackState = SCOPE_IDLE;
    EqNP.s    = IPS_OK;
    NewRaDec(currentRA, currentDEC);
  }
  return result;
}


bool LX200Pulsar2::Park(void) {
  if (!isSimulation()) {
    if (!isHomeSet()) {
      ParkSP.s = IPS_ALERT;
      IDSetSwitch(&ParkSP, "No parking position defined.");
      return false;
    }
    if (isParked()) {
      ParkSP.s = IPS_ALERT;
      IDSetSwitch(&ParkSP, "Scope has already been parked.");
      return false;
    }
  }
    
  // If scope is moving, let's stop it first.
  if (EqNP.s == IPS_BUSY) {
    if (!isSimulation() && abortSlew(PortFD) < 0) {
      AbortSP.s = IPS_ALERT;
      IDSetSwitch(&AbortSP, "Abort slew failed.");
      return false;
    }

    AbortSP.s    = IPS_OK;
    EqNP.s       = IPS_IDLE;
    IDSetSwitch(&AbortSP, "Slew aborted.");
    IDSetNumber(&EqNP, NULL);
    
    if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY) {
      MovementNSSP.s  = MovementWESP.s =  IPS_IDLE;
      EqNP.s       = IPS_IDLE;
      IUResetSwitch(&MovementNSSP);
      IUResetSwitch(&MovementWESP);
      
      IDSetSwitch(&MovementNSSP, NULL);
      IDSetSwitch(&MovementWESP, NULL);
    }
    usleep(100000); // sleep for 100 msec
  }

  if (!isSimulation()) {
    int success = 0;
    if (!Pulsar2Commands::getInt(PortFD,"#:YH#",&success) || success == 0) {
      ParkSP.s = IPS_ALERT;
      IDSetSwitch(&ParkSP, "Parking Failed.");
      return false;
    }
  }
  ParkSP.s = IPS_BUSY;
  TrackState = SCOPE_PARKING;
  IDMessage(getDeviceName(), "Parking telescope in progress...");
  return true;
}


bool LX200Pulsar2::UnPark(void) {
  if (!isSimulation()) {
    if (!isParked()) {
      ParkSP.s = IPS_ALERT;
      IDSetSwitch(&ParkSP, "Mount is not parked.");
      return false;
    }
    int success = 0;
    if (!Pulsar2Commands::getInt(PortFD,"#:YL#",&success) || success == 0) {
      ParkSP.s = IPS_ALERT;
      IDSetSwitch(&ParkSP, "Unparking failed.");
      return false;
    }
  }
  ParkSP.s = IPS_OK;
  TrackState = SCOPE_IDLE;
  SetParked(false);
  IDMessage(getDeviceName(), "Telescope has been unparked.");
  return true;
}


bool LX200Pulsar2::updateLocation(double latitude, double longitude,double elevation) {
  INDI_UNUSED(elevation);
  bool success = true;
  if (!isSimulation()) {
    success = Pulsar2Commands::setDegreesMinutes(PortFD,"Sl",360.0-longitude);
    if (success) {
      success = Pulsar2Commands::setDegreesMinutes(PortFD,"St",latitude);
      if (success) {
	char l[32], L[32];
	fs_sexa (l, latitude, 3, 3600);
	fs_sexa (L, longitude, 4, 3600);
	IDMessage(getDeviceName(), "Site location updated to Lat %.32s - Long %.32s", l, L);
      }
      else
	DEBUG(INDI::Logger::DBG_ERROR, "Error setting site latitude coordinates");
    }
    else
      DEBUG(INDI::Logger::DBG_ERROR, "Error setting site longitude coordinates");
  }
  return success;
}


bool LX200Pulsar2::updateTime(ln_date * utc, double utc_offset) {
  INDI_UNUSED(utc_offset);
  bool success = true;
  if (!isSimulation()) {
    struct ln_zonedate ltm;
    ln_date_to_zonedate(utc,&ltm,0.0);  // One should use UTC only with Pulsar2!
    JD = ln_get_julian_day(utc);
    DEBUGF(INDI::Logger::DBG_DEBUG, "New JD is %f",static_cast<float>(JD));
    success = Pulsar2Commands::setTime(PortFD,ltm.hours,ltm.minutes,ltm.seconds);
    if (success) {
      success = Pulsar2Commands::setDate(PortFD,ltm.days,ltm.months,ltm.years);
      if (success)
	DEBUG(INDI::Logger::DBG_SESSION, "Time updated, updating planetary data...");
      else 
	DEBUG(INDI::Logger::DBG_ERROR, "Error setting UTC date.");
    }
    else
      DEBUG(INDI::Logger::DBG_ERROR, "Error setting UTC time.");
    // Pulsar2 cannot set UTC offset (?)
  }
  return success;
}


bool LX200Pulsar2::isHomeSet(void) {
  bool result      = false;
  int  is_home_set = -1;
  if (Pulsar2Commands::getInt(PortFD,"#:YGh#",&is_home_set))
    result = ( is_home_set == 1 );
  return result;
}


bool LX200Pulsar2::isParked(void) {
  bool result    = false;
  int  is_parked = -1;
  if (Pulsar2Commands::getInt(PortFD,"#:YGk#",&is_parked))
    result = ( is_parked == 1 );
  return result;
}


bool LX200Pulsar2::isParking(void) {
  bool result     = false;
  int  is_parking = -1;
  if (Pulsar2Commands::getInt(PortFD,"#:YGj#",&is_parking))
    result = ( is_parking == 1 );
  return result;
}


bool LX200Pulsar2::isSlewing(void) {
  bool result = false;
  // The :YGi# command is late indicating that a slew is active by a couple of seconds.
  // So we also check whether a slew was just started.
  int is_slewing = -1;
  if (Pulsar2Commands::getInt(PortFD,"#:YGi#",&is_slewing)) {
    if (is_slewing == 1) {
      result = true;
      just_started_slewing = false;
    }
  }
  if (!result) result = just_started_slewing;
  return result;
}


bool LX200Pulsar2::startSlew(void) {
  char response[4];
  const bool success = ( Pulsar2Commands::getString(PortFD,"#:MS#",response) && response[0] == '0' );
  return success;
}
