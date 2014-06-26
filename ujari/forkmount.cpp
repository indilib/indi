/* Copyright 2012 Geehalel (geehalel AT gmail DOT com) */
/* This file is part of the ForkMount Protocol INDI driver.

    The ForkMount Protocol INDI driver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The ForkMount Protocol INDI driver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the ForkMount Protocol INDI driver.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <memory>
#include <wordexp.h>

#include <libnova.h>

/* Normally unused-only for debug */
#include <indidevapi.h>
#include <indicom.h>

#include "config.h"
#include "ujarierror.h"
#include "forkmount.h"
#include "ujari.h"
#include "amccontroller.h"
#include "encoder.h"

extern int DBG_SCOPE_STATUS;
extern int DBG_COMM;
extern int DBG_MOUNT;

#define GOTO_LIMIT      5                               /* Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees */
#define SLEW_LIMIT      2                               /* Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees */
#define FINE_SLEW_LIMIT 0.5                             /* Move at FINE_SLEW_RATE until distance from target is FINE_SLEW_LIMIT degrees */

#define RA_GOTO_SPEED   1.5
#define RA_SLEW_SPEED   0.5
#define RA_FINE_SPEED   0.2

#define DE_GOTO_SPEED   1.5
#define DE_SLEW_SPEED   0.5
#define DE_FINE_SPEED   0.2

#define RAGOTORESOLUTION 5        /* GOTO Resolution in arcsecs */
#define DEGOTORESOLUTION 5        /* GOTO Resolution in arcsecs */


ForkMount::ForkMount(Ujari *t)
{
  fd=-1;
  debug=false;
  //debugnextread=false;
  simulation=false;
  telescope = t;

  RAMotor  = new AMCController(AMCController::RA_MOTOR, t);
  DEMotor = new AMCController(AMCController::DEC_MOTOR, t);

  RAEncoder   = new Encoder(Encoder::RA_ENCODER, t);
  DEEncoder  = new Encoder(Encoder::DEC_ENCODER, t);

  Parkdatafile= "~/.indi/ParkData.xml";
}

ForkMount::~ForkMount(void)
{
    Disconnect();
}

bool ForkMount::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{

    RAMotor->ISNewNumber(dev, name, values, names, n);
    DEMotor->ISNewNumber(dev, name, values, names, n);

    RAEncoder->ISNewNumber(dev, name, values, names, n);
    DEEncoder->ISNewNumber(dev, name, values, names, n);

    return true;
}

bool ForkMount::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    RAMotor->ISNewSwitch(dev, name, states, names, n);
    DEMotor->ISNewSwitch(dev, name, states, names, n);

    //RAEncoder->ISNewSwitch(dev, name, states, names, n);
    //DEEncoder->ISNewSwitch(dev, name, states, names, n);

    return true;
}

bool ForkMount::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    RAMotor->ISNewText(dev, name, texts, names, n);
    DEMotor->ISNewText(dev, name, texts, names, n);

    RAEncoder->ISNewText(dev, name, texts, names, n);
    DEEncoder->ISNewText(dev, name, texts, names, n);

    return true;
}

void ForkMount::setDebug (bool enable)
{
  debug=enable;
}
bool ForkMount::isDebug ()
{
  return debug;
}


void ForkMount::setSimulation (bool enable)
{
  simulation=enable;

  RAMotor->setSimulation(enable);
  DEMotor->setSimulation(enable);
  RAEncoder->setSimulation(enable);
  DEEncoder->setSimulation(enable);

}
bool ForkMount::isSimulation ()
{
  return simulation;
}

const char *ForkMount::getDeviceName ()
{
  return telescope->getDeviceName();
}

bool ForkMount::initProperties()
{
    RAMotor->initProperties();
    DEMotor->initProperties();
    RAEncoder->initProperties();
    DEEncoder->initProperties();
}

void ForkMount::ISGetProperties(const char *dev)
{
    RAMotor->ISGetProperties();
    DEMotor->ISGetProperties();
}

bool ForkMount::updateProperties()
{
    RAMotor->updateProperties(telescope->isConnected());
    DEMotor->updateProperties(telescope->isConnected());
    RAEncoder->updateProperties(telescope->isConnected());
    DEEncoder->updateProperties(telescope->isConnected());
}

/* API */

bool ForkMount::Connect()  throw (UjariError)
{
  bool raMotorRC=false, decMotorRC=false, raEncoderRC=false, decEncoderRC=false;

  raMotorRC    = RAMotor->connect();
  decMotorRC   = DEMotor->connect();
  raEncoderRC  = true;//RAEncoder->connect();
  decEncoderRC = true;//DEEncoder->connect();

  if (raMotorRC && decMotorRC && raEncoderRC && decEncoderRC)
      return true;

  if (raMotorRC == false)
    throw UjariError(UjariError::ErrDisconnect, "Error connecting to RA Motor");
  else if (decMotorRC == false)
      throw UjariError(UjariError::ErrDisconnect, "Error connecting to DEC Motor");
  else if (raEncoderRC == false)
      throw UjariError(UjariError::ErrDisconnect, "Error connecting to RA Encoder");
  else if (decEncoderRC == false)
      throw UjariError(UjariError::ErrDisconnect, "Error connecting to DEC Encoder");

  return false;

}


bool ForkMount::Disconnect() throw (UjariError)
{
  if (fd < 0) return true;
  StopMotor(Axis1);
  StopMotor(Axis2);
  // Deactivate motor (for geehalel mount only)
  /*
  if (MountCode == 0xF0) {
    dispatch_command(Deactivate, Axis1, NULL);
    read_eqmod();
  }
  */

  if (!isSimulation())
  {

  tty_disconnect(fd);
  fd=-1;

  }

  return true;
}

unsigned long ForkMount::GetRAEncoder()  throw (UjariError)
{
  // Axis Position
  /*dispatch_command(GetAxisPosition, Axis1, NULL);
  read_eqmod();
  RAStep=Revu24str2long(response+1);
  gettimeofday (&lastreadmotorposition[Axis1], NULL);
  DEBUGF(DBG_SCOPE_STATUS, "%s() = %ld", __FUNCTION__, RAStep);
  */

  return RAStep;
}

unsigned long ForkMount::GetDEEncoder()  throw (UjariError)
{
  // Axis Position
  /*
  dispatch_command(GetAxisPosition, Axis2, NULL);
  read_eqmod();

  DEStep=Revu24str2long(response+1);
  gettimeofday (&lastreadmotorposition[Axis2], NULL);
  DEBUGF(DBG_SCOPE_STATUS, "%s() = %ld", __FUNCTION__, DEStep);
  */
  return DEStep;
}

unsigned long ForkMount::GetRAEncoderZero()
{
  RAStepInit = RAEncoder->GetEncoderZero();
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() = %ld", __FUNCTION__, RAStepInit);
  return RAStepInit;
}

unsigned long ForkMount::GetRAEncoderTotal()
{
  RASteps360 = RAEncoder->GetEncoderTotal();
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() = %ld", __FUNCTION__, RASteps360);
  return RASteps360;
}

unsigned long ForkMount::GetRAEncoderHome()
{
  RAStepHome = RAEncoder->GetEncoderHome();
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() = %ld", __FUNCTION__, RAStepHome);
  return RAStepHome;
}

unsigned long ForkMount::GetDEEncoderZero()
{
  DEStepInit = DEEncoder->GetEncoderZero();
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() = %ld", __FUNCTION__, DEStepInit);
  return DEStepInit;
}

unsigned long ForkMount::GetDEEncoderTotal()
{
  DESteps360 = DEEncoder->GetEncoderTotal();
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() = %ld", __FUNCTION__, DESteps360);
  return DESteps360;
}

unsigned long ForkMount::GetDEEncoderHome()
{
  DEStepHome = DEEncoder->GetEncoderHome();
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() = %ld", __FUNCTION__, DEStepHome);
  return DEStepHome;
}

void ForkMount::GetRAMotorStatus(ILightVectorProperty *motorLP) throw (UjariError)
{


  ReadMotorStatus(Axis1);
  if (!RAInitialized) {
    IUFindLight(motorLP, "RAInitialized")->s=IPS_ALERT;
    IUFindLight(motorLP, "RARunning")->s=IPS_IDLE;
    IUFindLight(motorLP, "RAGoto")->s=IPS_IDLE;
    IUFindLight(motorLP, "RAForward")->s=IPS_IDLE;
  } else {
    IUFindLight(motorLP, "RAInitialized")->s=IPS_OK;
    IUFindLight(motorLP, "RARunning")->s=(RARunning?IPS_OK:IPS_BUSY);
    IUFindLight(motorLP, "RAGoto")->s=((RAStatus.slewmode==GOTO)?IPS_OK:IPS_BUSY);
    IUFindLight(motorLP, "RAForward")->s=((RAStatus.direction==FORWARD)?IPS_OK:IPS_BUSY);
  }

}

void ForkMount::GetDEMotorStatus(ILightVectorProperty *motorLP) throw (UjariError)
{

  ReadMotorStatus(Axis2);
  if (!DEInitialized) {
    IUFindLight(motorLP, "DEInitialized")->s=IPS_ALERT;
    IUFindLight(motorLP, "DERunning")->s=IPS_IDLE;
    IUFindLight(motorLP, "DEGoto")->s=IPS_IDLE;
    IUFindLight(motorLP, "DEForward")->s=IPS_IDLE;
  } else {
    IUFindLight(motorLP, "DEInitialized")->s=IPS_OK;
    IUFindLight(motorLP, "DERunning")->s=(DERunning?IPS_OK:IPS_BUSY);
    IUFindLight(motorLP, "DEGoto")->s=((DEStatus.slewmode==GOTO)?IPS_OK:IPS_BUSY);
    IUFindLight(motorLP, "DEForward")->s=((DEStatus.direction==FORWARD)?IPS_OK:IPS_BUSY);
  }
}

void ForkMount::Init(ISwitchVectorProperty *parkSP) throw (UjariError)
{
  /*
  wasinitialized=false;
  ReadMotorStatus(Axis1);
  ReadMotorStatus(Axis2);
  if (!RAInitialized && !DEInitialized)
  {
    //Read initial stepper values
    dispatch_command(GetAxisPosition, Axis1, NULL);
    read_eqmod();
    RAStepInit=Revu24str2long(response+1);
    dispatch_command(GetAxisPosition, Axis2, NULL);
    read_eqmod();
    DEStepInit=Revu24str2long(response+1);
    DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : Motors not initialized -- read Init steps RAInit=%ld DEInit = %ld",
        __FUNCTION__, RAStepInit, DEStepInit);
    // Energize motors
    DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : Powering motors", __FUNCTION__);
    dispatch_command(Initialize, Axis1, NULL);
    read_eqmod();
    dispatch_command(Initialize, Axis2, NULL);
    read_eqmod();
    RAStepHome=RAStepInit;
    DEStepHome=DEStepInit + (DESteps360 / 4);

  } else
  { // Mount already initialized by another driver / driver instance
    // use default configuration && leave unchanged encoder values
    wasinitialized=true;
    RAStepInit=0x800000;
    DEStepInit=0x800000;
    RAStepHome=RAStepInit;
    DEStepHome=DEStepInit + (DESteps360 / 4);
    DEBUGF(INDI::Logger::DBG_WARNING, "%s() : Motors already initialized", __FUNCTION__);
    DEBUGF(INDI::Logger::DBG_WARNING, "%s() : Setting default Init steps --  RAInit=%ld DEInit = %ld",
       __FUNCTION__, RAStepInit, DEStepInit);
  }
  */


  /*
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : Setting Home steps RAHome=%ld DEHome = %ld",
     __FUNCTION__, RAStepHome, DEStepHome);
  //Park status
  initPark();

  if (isParked())
  {
    //TODO get Park position, set corresponding encoder values, mark mount as parked
    //parkSP->sp[0].s==ISS_ON
    DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : Mount was parked", __FUNCTION__);
    if (wasinitialized) {
      DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : leaving encoders unchanged",
         __FUNCTION__);
    } else
    {
      char cmdarg[7];
      DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : Mount in Park position -- setting encoders RA=%ld DE = %ld",
         __FUNCTION__, RAParkPosition, DEParkPosition);
      cmdarg[6]='\0';
      long2Revu24str(RAParkPosition, cmdarg);
      dispatch_command(SetAxisPosition, Axis1, cmdarg);
      read_eqmod();
      cmdarg[6]='\0';
      long2Revu24str(DEParkPosition, cmdarg);
      dispatch_command(SetAxisPosition, Axis2, cmdarg);
      read_eqmod();
    }
  }
  else
  {
    DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : Mount was not parked", __FUNCTION__);
    if (wasinitialized)
    {
      DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : leaving encoders unchanged",
         __FUNCTION__);
    } else
    {
      //mount is supposed to be in the home position (pointing Celestial Pole)
      char cmdarg[7];
      DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : Mount in Home position -- setting encoders RA=%ld DE = %ld",
         __FUNCTION__, RAStepHome, DEStepHome);
      cmdarg[6]='\0';
      long2Revu24str(DEStepHome, cmdarg);
      dispatch_command(SetAxisPosition, Axis2, cmdarg);
      read_eqmod();
      //DEBUGF(INDI::Logger::DBG_WARNING, "%s() : Mount is supposed to point North/South Celestial Pole", __FUNCTION__);
      //TODO mark mount as unparked?
    }
  }
  */
}

bool ForkMount::IsRARunning() throw (UjariError)
{
  //CheckMotorStatus(Axis1);
  RARunning = RAMotor->isMotionActive();
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() = %s", __FUNCTION__, (RARunning?"true":"false"));
  return(RARunning);
}

bool ForkMount::IsDERunning() throw (UjariError)
{
  //CheckMotorStatus(Axis2);
  DERunning = DEMotor->isMotionActive();
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() = %s", __FUNCTION__, (DERunning?"true":"false"));
  return(DERunning);
}

void ForkMount::StartMotor(ForkMountAxis axis, ForkMountAxisStatus newstatus) throw (UjariError)
{
  DEBUGF(DBG_MOUNT, "%s() : Axis = %c", __FUNCTION__, axis);

  bool rc = false;
  switch (axis)
  {
    case Axis1:
      if (newstatus.direction == FORWARD)
          rc = RAMotor->moveForward();
      else
          rc = RAMotor->moveReverse();

      if (rc == false)
          throw(UjariError::ErrCmdFailed, "RA Motor start motion failed.");
      break;

    case Axis2:
      if (newstatus.direction == FORWARD)
          rc = DEMotor->moveForward();
      else
          rc = DEMotor->moveReverse();

      if (rc == false)
          throw(UjariError::ErrCmdFailed, "RA Motor start motion failed.");
      break;
  }

  // FIXME Make sure this doesn't break anything
  RAStatus.slewmode = newstatus.slewmode;

}

void ForkMount::ReadMotorStatus(ForkMountAxis axis)  throw (UjariError)
{

  switch (axis)
  {
  case Axis1:
    RAInitialized=RAMotor->isDriveOnline();
    RARunning=RAMotor->isMotionActive();
    /* FIXME Is this necessary ?
     * RAStatus.slewmode=SLEW; else RAStatus.slewmode=GOTO;
     */
    if (RAMotor->getMotionStatus() == AMCController::MOTOR_REVERSE)
        RAStatus.direction= BACKWARD;
    else if (RAMotor->getMotionStatus() == AMCController::MOTOR_FORWARD)
        RAStatus.direction=FORWARD;
    break;
  case Axis2:
      DEInitialized=DEMotor->isDriveOnline();
      DERunning=DEMotor->isMotionActive();
      /* FIXME Is this necessary ?
       * DEStatus.slewmode=SLEW; else DECStatus.slewmode=GOTO;
       */
      if (DEMotor->getMotionStatus() == AMCController::MOTOR_REVERSE)
          DEStatus.direction= BACKWARD;
      else if (DEMotor->getMotionStatus() == AMCController::MOTOR_FORWARD)
          DEStatus.direction=FORWARD;

  default: ;
  }
  gettimeofday (&lastreadmotorstatus[axis], NULL);
}

void ForkMount::SlewRA(double rate) throw (UjariError)
{
  double absrate=fabs(rate);
  ForkMountAxisStatus newstatus;

  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : rate = %g", __FUNCTION__, rate);

  if (RARunning && (RAStatus.slewmode==GOTO || RAStatus.slewmode==TRACK))
  {
      throw  UjariError(UjariError::ErrInvalidCmd, "Can not slew while goto or tracking is in progress");
  }

  if ((absrate < get_min_rate()) || (absrate > get_max_rate()))
  {
    throw  UjariError(UjariError::ErrInvalidParameter, "Speed rate out of limits: %.2fx Sidereal (min=%.2f, max=%.2f)",
              absrate, MIN_RATE, MAX_RATE);
  }

  double rpm = absrate / FORKMOUNT_RATE_TO_RPM;

  if (rate >= 0.0)
      newstatus.direction = FORWARD;
  else
      newstatus.direction = BACKWARD;

  newstatus.slewmode=SLEW;

  SetMotion(Axis1, newstatus);
  SetSpeed(Axis1, rpm);

  if (!RARunning)
      StartMotor(Axis1, newstatus);
}

void ForkMount::SlewDE(double rate) throw (UjariError)
{
  double absrate=fabs(rate);
  ForkMountAxisStatus newstatus;

  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : rate = %g", __FUNCTION__, rate);

  if (DERunning && (DEStatus.slewmode==GOTO || DEStatus.slewmode==TRACK))
  {
      throw  UjariError(UjariError::ErrInvalidCmd, "Can not slew while goto or tracking is in progress");
  }

  if (rate >= 0.0)
      newstatus.direction = FORWARD;
  else
      newstatus.direction = BACKWARD;
  newstatus.slewmode=SLEW;

  double rpm = absrate / FORKMOUNT_RATE_TO_RPM;

  DEBUGF(INDI::Logger::DBG_DEBUG, "Slewing DE at %.2f %.2f %x %g RPM", rate, absrate, rpm);

  SetMotion(Axis2, newstatus);
  SetSpeed(Axis2, rpm);
  if (!DERunning)
      StartMotor(Axis2, newstatus);
}

void ForkMount::SlewTo(long targetraencoder, long targetdeencoder)
{
  ForkMountAxisStatus newstatus;

  RAEncoderTarget = targetraencoder;
  DEEncoderTarget = targetdeencoder;

  long deltaraencoder, deltadeencoder;

  deltaraencoder = RAEncoderTarget - RAStep;
  deltadeencoder = DEEncoderTarget - DEStep;

  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : deltaRA = %ld deltaDE = %ld", __FUNCTION__, deltaraencoder, deltadeencoder);

  newstatus.slewmode=GOTO;

  minrpms[Axis1] = GetGotoSpeed(Axis1);
  minrpms[Axis2] = GetGotoSpeed(Axis2);

  if (deltaraencoder >= 0)
      newstatus.direction = FORWARD;
  else
      newstatus.direction = BACKWARD;

  if (deltaraencoder < 0)
      deltaraencoder = -deltaraencoder;

  if (deltaraencoder > 0)
  {
    SetMotion(Axis1, newstatus);
    SetSpeed(Axis1, minrpms[Axis1]);
    //SetTarget(Axis1, deltaraencoder);

    StartMotor(Axis1, newstatus);
  }

  if (deltadeencoder >= 0)
      newstatus.direction = FORWARD;
  else
      newstatus.direction = BACKWARD;

  if (deltadeencoder < 0)
      deltadeencoder = -deltadeencoder;

  if (deltadeencoder > 0)
  {
    SetMotion(Axis2, newstatus);
    SetSpeed(Axis2, minrpms[Axis2]);
    //SetTarget(Axis2, deltadeencoder);
    StartMotor(Axis2, newstatus);
  }

}

void  ForkMount::SetRARate(double rate)  throw (UjariError)
{
  double absrate=fabs(rate);
  ForkMountAxisStatus newstatus;

  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : rate = %g", __FUNCTION__, rate);

  if ((absrate < get_min_rate()) || (absrate > get_max_rate()))
  {
    throw  UjariError(UjariError::ErrInvalidParameter, "Speed rate out of limits: %.2fx Sidereal (min=%.2f, max=%.2f)",
              absrate, MIN_RATE, MAX_RATE);
  }

  double rpm = absrate * FORKMOUNT_RATE_TO_RPM;
  newstatus.direction = ((rate >= 0.0)? FORWARD: BACKWARD);
  newstatus.slewmode=SLEW;
  if (RARunning)
  {
    if (newstatus.direction != RAStatus.direction)
      throw  UjariError(UjariError::ErrInvalidParameter, "Can not change rate while motor is running (direction differs).");
  }
  SetMotion(Axis1, newstatus);
  SetSpeed(Axis1, rpm);
}

void  ForkMount::SetDERate(double rate)  throw (UjariError)
{
  double absrate=fabs(rate);
  ForkMountAxisStatus newstatus;

  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : rate = %g", __FUNCTION__, rate);

  if ((absrate < get_min_rate()) || (absrate > get_max_rate()))
  {
    throw  UjariError(UjariError::ErrInvalidParameter, "Speed rate out of limits: %.2fx Sidereal (min=%.2f, max=%.2f)",
              absrate, MIN_RATE, MAX_RATE);
  }

  double rpm = absrate * FORKMOUNT_RATE_TO_RPM;
  newstatus.direction = ((rate >= 0.0)? FORWARD: BACKWARD);
  newstatus.slewmode=SLEW;

  if (DERunning)
  {
    if (newstatus.direction != DEStatus.direction)
      throw  UjariError(UjariError::ErrInvalidParameter, "Can not change rate while motor is running (direction differs).");
  }
  SetMotion(Axis2, newstatus);
  SetSpeed(Axis2, rpm);
}

void ForkMount::StartRATracking(double trackspeed) throw (UjariError)
{
  double rate;
  // FIXME TODO Find out Ujari Tracking Rate
  if (trackspeed != 0.0)
      rate = trackspeed / FORKMOUNT_STELLAR_SPEED;
  else
      rate = 0.0;
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : trackspeed = %g arcsecs/s, computed rate = %g", __FUNCTION__, trackspeed, rate);
  if (rate != 0.0)
  {
    SetRARate(rate);
    if (!RARunning)
    {
        // FIXME is this the right direction for tracking?!!
        ForkMountAxisStatus newstatus;
        newstatus.direction = FORWARD;
        // Is it still considered GOTO? Not sure, find out later
        newstatus.slewmode = TRACK;
        StartMotor(Axis1, newstatus);
    }
  }
  else
    StopMotor(Axis1);
}

void ForkMount::StartDETracking(double trackspeed) throw (UjariError)
{
  double rate;
  // FIXME TODO Find out Ujari Tracking Rate
  if (trackspeed != 0.0)
      rate = trackspeed / FORKMOUNT_STELLAR_SPEED;
  else
      rate = 0.0;
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : trackspeed = %g arcsecs/s, computed rate = %g", __FUNCTION__, trackspeed, rate);

  if (rate != 0.0)
  {
    SetDERate(rate);
    if (!DERunning)
    {
        // FIXME is this the right direction for tracking?!!
        ForkMountAxisStatus newstatus;
        newstatus.direction = FORWARD;
        // Is it still considered GOTO? Not sure, find out later
        newstatus.slewmode = TRACK;
        StartMotor(Axis2, newstatus);
    }
  }
  else
    StopMotor(Axis2);
}

void ForkMount::SetSpeed(ForkMountAxis axis, double rpm) throw (UjariError)
{
  //ForkMountAxisStatus *currentstatus;

  DEBUGF(DBG_MOUNT, "%s() : Axis = %c -- rpm=%g", __FUNCTION__, axis, rpm);

  //ReadMotorStatus(axis);
  if (axis == Axis1)
  {
      //currentstatus=&RAStatus;
      RAMotor->setSpeed(rpm);
  }
  else
  {
      //currentstatus=&DEStatus;
      DEMotor->setSpeed(rpm);
  }

}

void ForkMount::StopRA()  throw (UjariError)
{
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : calling RA StopWaitMotor", __FUNCTION__);
  StopWaitMotor(Axis1);

}

void ForkMount::StopDE()  throw (UjariError)
{
  DEBUGF(INDI::Logger::DBG_DEBUG, "%s() : calling DE StopWaitMotor", __FUNCTION__);
  StopWaitMotor(Axis2);

}


void ForkMount::StopWaitMotor(ForkMountAxis axis)  throw (UjariError)
{
  bool *motorrunning;
  struct timespec wait;

  if (axis == Axis1)
  {
      RAMotor->stop();
      // FIXME Resetting it to SLEW here, CHECK!
      RAStatus.slewmode = SLEW;
  }
  else
  {
      DEMotor->stop();
      DEStatus.slewmode = SLEW;
  }

  if (axis == Axis1)
      motorrunning=&RARunning;
  else
      motorrunning=&DERunning;
  wait.tv_sec=0; wait.tv_nsec=100000000; // 100ms
  ReadMotorStatus(axis);
  while (*motorrunning)
  {
    nanosleep(&wait, NULL);
    ReadMotorStatus(axis);
  }
}

void ForkMount::SetMotion(ForkMountAxis axis, ForkMountAxisStatus newstatus) throw (UjariError)
{
  ForkMountAxisStatus *currentstatus;

  DEBUGF(DBG_MOUNT, "%s() : Axis = %c -- dir=%s mode=%s", __FUNCTION__, axis,
     ((newstatus.direction == FORWARD)?"forward":"backward"),
     ((newstatus.slewmode == SLEW)?"slew":"goto"));

  CheckMotorStatus(axis);
  if (axis == Axis1)
      currentstatus=&RAStatus;
  else
      currentstatus=&DEStatus;

#ifdef STOP_WHEN_MOTION_CHANGED
  StopWaitMotor(axis);
#else
  // TODO Stop Motor on Motion Direction control
  if ((newstatus.direction != currentstatus->direction) || (newstatus.slewmode != currentstatus->slewmode))
  {
    StopWaitMotor(axis);
  }
#endif
}


void ForkMount::StopMotor(ForkMountAxis axis)  throw (UjariError)
{
  DEBUGF(DBG_MOUNT, "%s() : Axis = %c", __FUNCTION__, axis);

  if (axis == Axis1)
  {
      RAMotor->stop();
      RAStatus.slewmode = SLEW;
  }
  else
  {
      DEMotor->stop();
      DEStatus.slewmode = SLEW;
  }
}

/* Utilities */

void ForkMount::CheckMotorStatus(ForkMountAxis axis) throw (UjariError)
{
  struct timeval now;
  DEBUGF(DBG_SCOPE_STATUS, "%s() : Axis = %c", __FUNCTION__, axis);
  gettimeofday (&now, NULL);
  if (((now.tv_sec - lastreadmotorstatus[axis].tv_sec) + ((now.tv_usec - lastreadmotorstatus[axis].tv_usec)/1e6)) > FORKMOUNT_MAXREFRESH)
    ReadMotorStatus(axis);
}

double ForkMount::get_min_rate() {
  return MIN_RATE;
}

double ForkMount::get_max_rate() {
  return MAX_RATE;
}

unsigned long ForkMount::Revu24str2long(char *s) {
   unsigned long res = 0;
   res=HEX(s[4]); res <<= 4;
   res|=HEX(s[5]); res <<= 4;
   res|=HEX(s[2]); res <<= 4;
   res|=HEX(s[3]); res <<= 4;
   res|=HEX(s[0]); res <<= 4;
   res|=HEX(s[1]);
   return res;
}

unsigned long ForkMount::Highstr2long(char *s) {
   unsigned long res = 0;
   res =HEX(s[0]); res <<= 4;
   res|=HEX(s[1]);
   return res;
}



void ForkMount::long2Revu24str(unsigned long n, char *str) {
char hexa[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                 '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
   str[0]=hexa[(n & 0xF0) >> 4];
   str[1]=hexa[(n & 0x0F)];
   str[2]=hexa[(n & 0xF000) >> 12];
   str[3]=hexa[(n & 0x0F00) >> 8];
   str[4]=hexa[(n & 0xF00000) >> 20];
   str[5]=hexa[(n & 0x0F0000) >> 16];
   str[6]='\0';
}

// Park
unsigned long ForkMount::GetRAEncoderPark()
{
  return RAParkPosition;
}
unsigned long ForkMount::GetRAEncoderParkDefault()
{
  return RADefaultParkPosition;
}
unsigned long ForkMount::GetDEEncoderPark()
{
  return DEParkPosition;
}
unsigned long ForkMount::GetDEEncoderParkDefault()
{
  return DEDefaultParkPosition;
}
unsigned long ForkMount::SetRAEncoderPark(unsigned long steps)
{
  RAParkPosition=steps;
}
unsigned long ForkMount::SetRAEncoderParkDefault(unsigned long steps)
{
  RADefaultParkPosition=steps;
}
unsigned long ForkMount::SetDEEncoderPark(unsigned long steps)
{
  DEParkPosition=steps;
}
unsigned long ForkMount::SetDEEncoderParkDefault(unsigned long steps)
{
  DEDefaultParkPosition=steps;
}
void ForkMount::SetParked(bool isparked)
{
  parked=isparked;
  WriteParkData();
}
bool ForkMount::isParked()
{
  return parked;
}

void ForkMount::initPark()
{
  char *loadres;
  loadres=LoadParkData(Parkdatafile);
  if (loadres)
  {
    DEBUGF(INDI::Logger::DBG_SESSION, "initPark: No Park data in file %s: %s", Parkdatafile, loadres);
    RAParkPosition = RAStepHome;
    RADefaultParkPosition = RAStepHome;
    DEParkPosition = DEStepHome;
    DEDefaultParkPosition = DEStepHome;
    parked=false;
  }
}

char *ForkMount::LoadParkData(const char *filename)
{
  wordexp_t wexp;
  FILE *fp;
  LilXML *lp;
  static char errmsg[512];

  XMLEle *parkxml, *devicexml;
  XMLAtt *ap;
  bool devicefound=false;

  ParkDeviceName = getDeviceName();
  ParkstatusXml=NULL;
  ParkdeviceXml=NULL;
  ParkpositionXml = NULL;
  ParkpositionRAXml = NULL;
  ParkpositionDEXml = NULL;

  if (wordexp(filename, &wexp, 0)) {
    wordfree(&wexp);
    return (char *)("Badly formed filename");
  }

  if (!(fp=fopen(wexp.we_wordv[0], "r"))) {
    wordfree(&wexp);
    return strerror(errno);
  }
  wordfree(&wexp);

 lp = newLilXML();
 if (ParkdataXmlRoot)
    delXMLEle(ParkdataXmlRoot);
  ParkdataXmlRoot = readXMLFile(fp, lp, errmsg);
  delLilXML(lp);
  if (!ParkdataXmlRoot) return errmsg;
  if (!strcmp(tagXMLEle(nextXMLEle(ParkdataXmlRoot, 1)), "parkdata")) return (char *)("Not a park data file");
  parkxml=nextXMLEle(ParkdataXmlRoot, 1);
  while (parkxml) {
    if (strcmp(tagXMLEle(parkxml), "device")) { parkxml=nextXMLEle(ParkdataXmlRoot, 0); continue;}
    ap = findXMLAtt(parkxml, "name");
    if (ap && (!strcmp(valuXMLAtt(ap), ParkDeviceName))) { devicefound = true; break; }
    parkxml=nextXMLEle(ParkdataXmlRoot, 0);
  }
  if (!devicefound) return (char *)"No park data found for this device";
  ParkdeviceXml=parkxml;
  ParkstatusXml = findXMLEle(parkxml, "parkstatus");
  ParkpositionXml = findXMLEle(parkxml, "parkposition");
  ParkpositionRAXml = findXMLEle(ParkpositionXml, "raencoder");
  ParkpositionDEXml = findXMLEle(ParkpositionXml, "deencoder");
  parked=false;
  if (!strcmp(pcdataXMLEle(ParkstatusXml), "true")) parked=true;
  sscanf(pcdataXMLEle(ParkpositionRAXml), "%ld", &RAParkPosition);
  sscanf(pcdataXMLEle(ParkpositionDEXml), "%ld", &DEParkPosition);
  RADefaultParkPosition = RAStepHome;
  DEDefaultParkPosition = DEStepHome;
  return NULL;
}

bool ForkMount::WriteParkData()
{
  char *writeres;
  writeres=WriteParkData(Parkdatafile);
  if (writeres) {
    DEBUGF(INDI::Logger::DBG_SESSION, "WriteParkData: can not write file %s: %s", Parkdatafile, writeres);
    return false;
  } else {
    return true;
  }
}

char *ForkMount::WriteParkData(const char *filename)
{
  wordexp_t wexp;
  FILE *fp;
  char pcdata[30];

  if (wordexp(filename, &wexp, 0)) {
    wordfree(&wexp);
    return (char *)("Badly formed filename");
  }

  if (!(fp=fopen(wexp.we_wordv[0], "w"))) {
    wordfree(&wexp);
    return strerror(errno);
  }

  if (!ParkdataXmlRoot) ParkdataXmlRoot=addXMLEle(NULL, "parkdata");
  if (!ParkdeviceXml) {
    ParkdeviceXml=addXMLEle(ParkdataXmlRoot, "device");
    addXMLAtt(ParkdeviceXml, "name", ParkDeviceName);
  }
  if (!ParkstatusXml) ParkstatusXml=addXMLEle(ParkdeviceXml, "parkstatus");
  if (!ParkpositionXml) ParkpositionXml=addXMLEle(ParkdeviceXml, "parkposition");
  if (!ParkpositionRAXml) ParkpositionRAXml=addXMLEle(ParkpositionXml, "raencoder");
  if (!ParkpositionDEXml) ParkpositionDEXml=addXMLEle(ParkpositionXml, "deencoder");
  editXMLEle(ParkstatusXml, (parked?"true":"false"));
  snprintf(pcdata, sizeof(pcdata), "%ld", RAParkPosition);
  editXMLEle(ParkpositionRAXml, pcdata);
  snprintf(pcdata, sizeof(pcdata), "%ld", DEParkPosition);
  editXMLEle(ParkpositionDEXml, pcdata);

  prXMLEle(fp, ParkdataXmlRoot, 0);
  fclose(fp);
  return NULL;

}

bool ForkMount::update()
{
    double separation, speed;
    bool raMotorRC    = RAMotor->update();
    bool decMotorRC   = DEMotor->update();

    RAStep = RAEncoder->getEncoderValue();
    DEStep = DEEncoder->getEncoderValue();

    // Now check how far RAEncoderTarget is from RAStep
    separation = fabs(RAStep - RAEncoderTarget)/RAEncoder->getTicksToDegreeRatio();
    if (fabs(separation) * 3600 <= RAGOTORESOLUTION)
        RAMotor->stop();
    else
    {
        speed = GetGotoSpeed(Axis1);
        if (speed != RAMotor->getSpeed())
            RAMotor->setSpeed(speed);
    }

    separation = fabs(DEStep - DEEncoderTarget)/DEEncoder->getTicksToDegreeRatio();
    if (fabs(separation) * 3600 <= DEGOTORESOLUTION)
        DEMotor->stop();
    else
    {
        speed = GetGotoSpeed(Axis2);
        if (speed != DEMotor->getSpeed())
            DEMotor->setSpeed(speed);
    }

    return (raMotorRC && decMotorRC);
}

double ForkMount::GetGotoSpeed(ForkMountAxis axis)
{
    double speed;
    double separation;

    switch (axis)
    {
        case Axis1:
        separation = fabs(RAStep - RAEncoderTarget)/RAEncoder->getTicksToDegreeRatio();
            if (separation > GOTO_LIMIT)
                speed = RA_GOTO_SPEED;
            else if (separation > SLEW_LIMIT)
                speed = RA_SLEW_SPEED;
            else
                speed = RA_FINE_SPEED;
        break;

        case Axis2:
        separation = fabs(DEStep - DEEncoderTarget)/DEEncoder->getTicksToDegreeRatio();
            if (separation > GOTO_LIMIT)
                speed = DE_GOTO_SPEED;
            else if (separation > SLEW_LIMIT)
                speed = DE_SLEW_SPEED;
            else
                speed = DE_FINE_SPEED;
        break;
    }

    return speed;
}
