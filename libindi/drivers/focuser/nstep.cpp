/*
  NStep Focuser
  Copyright (c) 2016 Cloudmakers, s. r. o.
  All Rights Reserved.

  Thanks to Gene Nolan and Leon Palmer for their support.

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
#include <memory>
#include <unistd.h>

#include <indicom.h>

#include "nstep.h"

#define POLLMS  2000

std::unique_ptr<NSTEP> nstep(new NSTEP());

void ISGetProperties(const char *dev) {  
    nstep->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
    nstep->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num) {
    nstep->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
    nstep->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root) {
  nstep->ISSnoopDevice(root);
}

NSTEP::NSTEP() {
  setDeviceName(getDefaultName());
  setVersion(1, 0);
  SetFocuserCapability(FOCUSER_CAN_ABORT | FOCUSER_CAN_REL_MOVE);
  sim_position = 0;
}

NSTEP::~NSTEP() {
  if (isConnected())
    Disconnect();
}

bool NSTEP::command(const char *request, char *response, int count) {
  DEBUGF(INDI::Logger::DBG_DEBUG,  "Write [%s]", request);
  if (isSimulation()) {
    if (!strcmp(request, ":RT")) {
      strcpy(response, "+150");
      DEBUGF(INDI::Logger::DBG_DEBUG,  "Read [%s]", response);
      return true;
    }
    if (!strcmp(request, ":RP")) {
      sprintf(response, "%+07ld", sim_position);
      DEBUGF(INDI::Logger::DBG_DEBUG,  "Read [%s]", response);
      return true;
    }
    if (!strcmp(request, ":RS")) {
      strcpy(response, "100");
      DEBUGF(INDI::Logger::DBG_DEBUG,  "Read [%s]", response);
      return true;
    }
    if (!strcmp(request, ":RO")) {
      strcpy(response, "001");
      DEBUGF(INDI::Logger::DBG_DEBUG,  "Read [%s]", response);
      return true;
    }
    if (!strcmp(request, ":RA")) {
      strcpy(response, "+010");
      DEBUGF(INDI::Logger::DBG_DEBUG,  "Read [%s]", response);
      return true;
    }
    if (!strcmp(request, ":RB")) {
      strcpy(response, "005");
      DEBUGF(INDI::Logger::DBG_DEBUG,  "Read [%s]", response);
      return true;
    }
    if (!strcmp(request, ":RG")) {
      strcpy(response, "2");
      DEBUGF(INDI::Logger::DBG_DEBUG,  "Read [%s]", response);
      return true;
    }
    if (!strcmp(request, ":RW")) {
      strcpy(response, "0");
      DEBUGF(INDI::Logger::DBG_DEBUG,  "Read [%s]", response);
      return true;
    }
    if (!strcmp(request, "S")) {
      strcpy(response, "0");
      DEBUGF(INDI::Logger::DBG_DEBUG,  "Read [%s]", response);
      return true;
    }
    return true;
  }
  int actual, total;
  pthread_mutex_lock(&lock);
  int rc = TTY_OK;
  if (request)
    rc = tty_write(PortFD, request, strlen(request), &actual);
  if (rc == TTY_OK && response) {
    total = 0;
    while (rc == TTY_OK && count > 0) {
      rc = tty_read(PortFD, response + total, count, 5, &actual);
      total += actual;
      count -= actual;
    }
    response[total] = 0;
  }
  if (rc != TTY_OK) {
    char message[MAXRBUF];
    tty_error_msg(rc, message, MAXRBUF);
    IDMessage(getDeviceName(), "%s", message);
    pthread_mutex_unlock(&lock);
    return false;
  }
  DEBUGF(INDI::Logger::DBG_DEBUG,  "Read [%s]", response);
  pthread_mutex_unlock(&lock);
  return true;
}

const char * NSTEP::getDefaultName() {
  return "NStep";
}

bool NSTEP::Connect() {
  if (isConnected()) {
    return true;
  }
  int connectrc = 0;
  bool rc;
  if (isSimulation()) {
    IDMessage(getDeviceName(), "NStep simulation is connected.");
    SetTimer(POLLMS);
    return true;
  } else {
    if ((rc = tty_connect(PortT[0].text, 9600, 8, 0, 1, &PortFD)) != TTY_OK) {
      tty_error_msg(rc, buf, MAXRBUF);
      IDMessage(getDeviceName(), "Failed to connect to NStep on %s (%s)", PortT[0].text, buf);
      return false;
    }
    char b = 0x06;
    int actual;
    int rc = tty_write(PortFD, &b, 1, &actual);
    if (rc == TTY_OK) {
      rc = tty_read(PortFD, &b, 1, 5, &actual);
      if (rc == TTY_OK && b == 'S') {
        IDMessage(getDeviceName(), "NStep is connected on %s.", PortT[0].text);
        SetTimer(POLLMS);
        return true;
      }
    }
  }
  return false;
}

bool NSTEP::Disconnect() {
  if (!isSimulation()) {
    tty_disconnect(PortFD);
    IDMessage(getDeviceName(), "NStep is disconnected.");
    return true;
  }
}

bool NSTEP::initProperties() {
  INDI::Focuser::initProperties();
  
  addDebugControl();
  addSimulationControl();

  FocusAbsPosN[0].min = -999999;
  FocusAbsPosN[0].max = 999999;
  FocusAbsPosN[0].step = 1;

  FocusRelPosN[0].min = -999;
  FocusRelPosN[0].max = 999;
  FocusRelPosN[0].step = 1;

  FocusSpeedN[0].min = 1;
  FocusSpeedN[0].max = 254;
  FocusSpeedN[0].step = 1;

  FocusMotionSP.r = ISR_1OFMANY;

  IUFillText(&PortT[0], "PORT", "Port", "/dev/ttyACM0");
  IUFillTextVector(&PortTP, PortT, 1, getDeviceName(), "DEVICE_PORT", "Ports", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

  IUFillSwitch(&TempCompS[0], "ENABLED", "Temperature compensation enabled", ISS_OFF);
  IUFillSwitch(&TempCompS[1], "DISABLED", "Temperature compensation disabled", ISS_ON);
  IUFillSwitchVector(&TempCompSP, TempCompS, 2, getDeviceName(), "COMPENSATION_MODE", "Compensation mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

  IUFillNumber(&TempCompN[0], "TEMP_CHANGE", "Temperature change", "%.1f", -99, 99, 0.1, 0);
  IUFillNumber(&TempCompN[1], "TEMP_MOVE", "Compensation move", "%.0f", 0, 999, 1, 0);
  IUFillNumberVector(&TempCompNP, TempCompN, 2, getDeviceName(), "COMPENSATION_SETTING", "Compensation settings", MAIN_CONTROL_TAB, IP_RW, 0, IPS_OK);

  IUFillNumber(&TempN[0], "TEMPERATURE", "Temperature", "%.1f", 0, 999, 0, 0);
  IUFillNumberVector(&TempNP, TempN, 1, getDeviceName(), "TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_OK);

  IUFillSwitch(&SteppingModeS[0], "WAVE", "Wave", ISS_ON);
  IUFillSwitch(&SteppingModeS[1], "HALF", "Half", ISS_OFF);
  IUFillSwitch(&SteppingModeS[2], "FULL", "Full", ISS_OFF);
  IUFillSwitchVector(&SteppingModeSP, SteppingModeS, 3, getDeviceName(), "STEPPING_MODE", "Stepping mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

  FocusAbsPosNP.p = IP_RO;
  
  return true;
}

bool NSTEP::updateProperties() {
  if (isConnected()) {
    command(":CC1", NULL, 0);
    if (command(":RP", buf, 7)) {
      sscanf(buf, "%ld", &position);
      FocusAbsPosN[0].value = position;
      defineNumber(&FocusAbsPosNP);
    } else {
      IDMessage(getDeviceName(), "Failed to read position");
    }
    if (command(":RT", buf, 4)) {
      sscanf(buf, "%d", &temperature);
      if (temperature != -888) {
        TempN[0].value = temperature/10.0;
        defineNumber(&TempNP);
        if (command(":RA", buf, 4)) {
          int value;
          sscanf(buf, "%d", &value);
          TempCompN[0].value = value/10.0;
          defineNumber(&TempCompNP);
        } else {
          IDMessage(getDeviceName(), "Failed to read temperature change for compensation");
        }
        if (command(":RB", buf, 3)) {
          int value;
          sscanf(buf, "%d", &value);
          TempCompN[1].value = value;
          defineNumber(&TempCompNP);
        } else {
          IDMessage(getDeviceName(), "Failed to read temperature step for compensation");
        }
        if (command(":RG", buf, 1)) {
          char value = *buf;
          TempCompS[0].s = value == '2' ? ISS_ON : ISS_OFF;
          TempCompS[1].s = value == '0'  ? ISS_ON : ISS_OFF;
          defineSwitch(&TempCompSP);
        } else {
          IDMessage(getDeviceName(), "Failed to read compensation mode");
        }
      } else {
        IDMessage(getDeviceName(), "Temperature sensor is not connected");
      }
    } else {
      IDMessage(getDeviceName(), "Failed to read temperature");
    }
    if (command(":RS", buf, 3)) {
      int value;
      sscanf(buf, "%d", &value);
      FocusSpeedN[0].max = value;
      if (command(":RO", buf, 3)) {
        int value;
        sscanf(buf, "%d", &value);
        FocusSpeedN[0].value = value;
        defineNumber(&FocusSpeedNP);
      } else {
        IDMessage(getDeviceName(), "Failed to read step rate");
      }
    } else {
      IDMessage(getDeviceName(), "Failed to read max step rate");
    }
    if (command(":RW", buf, 1)) {
      steppingMode = *buf;
      SteppingModeS[0].s = steppingMode == '0' ? ISS_ON : ISS_OFF;
      SteppingModeS[1].s = steppingMode == '1' ? ISS_ON : ISS_OFF;
      SteppingModeS[2].s = steppingMode == '2' ? ISS_ON : ISS_OFF;
      defineSwitch(&SteppingModeSP);
    } else {
      IDMessage(getDeviceName(), "Failed to read stepping mode");
    }
  } else {
    deleteProperty(FocusAbsPosNP.name);
    deleteProperty(TempNP.name);
    deleteProperty(TempCompNP.name);
    deleteProperty(TempCompSP.name);
    deleteProperty(FocusSpeedNP.name);
    deleteProperty(SteppingModeSP.name);
  }
  return INDI::Focuser::updateProperties();;
}

bool NSTEP::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) {
  if (strcmp(dev, getDeviceName()) == 0) {
    if (!strcmp (name, TempCompSP.name)) {
      IUUpdateSwitch(&TempCompSP, states, names, n);
      TempCompSP.s = IPS_OK;
      if (TempCompS[0].s == ISS_ON) {
        if (!command(":TA2", NULL, 0)) {
          TempCompSP.s = IPS_ALERT;
        }
        if (!command(":TC30#", NULL, 0)) {
          TempCompSP.s = IPS_ALERT;
        }
      } else {
        if (!command(":TA0", NULL, 0)) {
          TempCompSP.s = IPS_ALERT;
        }
      }
      IDSetSwitch(&TempCompSP, NULL);
      return true;
    }
    if (!strcmp (name, SteppingModeSP.name)) {
      IUUpdateSwitch(&SteppingModeSP, states, names, n);
      SteppingModeSP.s = IPS_OK;
      if (SteppingModeS[0].s == ISS_ON) {
        steppingMode = '0';
        if (!command(":CW0", NULL, 0)) {
          SteppingModeSP.s = IPS_ALERT;
        }
      } else if (SteppingModeS[1].s == ISS_ON) {
        steppingMode = '1';
        if (!command(":CW1", NULL, 0)) {
          SteppingModeSP.s = IPS_ALERT;
        }
      } else if (SteppingModeS[2].s == ISS_ON) {
        steppingMode = '2';
        if (!command(":CW2", NULL, 0)) {
          SteppingModeSP.s = IPS_ALERT;
        }
      }
      IDSetSwitch(&SteppingModeSP, NULL);
      return true;
    }
  }
  return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool NSTEP::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) {
  if(strcmp(dev, getDeviceName()) == 0) {
    if (!strcmp(name, TempCompNP.name)) {
      IUUpdateNumber(&TempCompNP, values, names, n);
      PresetNP.s = IPS_OK;
      sprintf(buf, ":TT%+04d#", (int)(TempCompN[0].value*10));
      if (!command(buf, NULL, 0)) {
        PresetNP.s = IPS_ALERT;
      }
      sprintf(buf, ":TS%03d#", (int)(TempCompN[1].value));
      if (!command(buf, NULL, 0)) {
        PresetNP.s = IPS_ALERT;
      }
      IDSetNumber(&TempCompNP, NULL);
      return true;
    }
  }
  return INDI::Focuser::ISNewNumber(dev,name,values,names,n);
}


IPState NSTEP::MoveRelFocuser(FocusDirection dir, unsigned int ticks) {
  sprintf(buf, ":F%c%c%03d#", dir == FOCUS_INWARD?'1':'0', steppingMode, ticks);
  if (command(buf, NULL, 0)) {
    FocusAbsPosNP.s = IPS_BUSY;
    IDSetNumber(&FocusAbsPosNP, NULL);
    if (isSimulation()) {
      if (dir == FOCUS_INWARD)
        sim_position -= ticks;
      else
        sim_position += ticks;
    }
    return IPS_BUSY;
  }
  FocusAbsPosNP.s = IPS_ALERT;
  IDSetNumber(&FocusAbsPosNP, NULL);
  return IPS_ALERT;
}

bool NSTEP::AbortFocuser() {
  sprintf(buf, ":F1%c000#", steppingMode);
  if (command(buf, NULL, 0)) {
    return true;
  }
  return false;
}

bool NSTEP::SetFocuserSpeed(int speed) {
  sprintf(buf, ":CS%03d#", speed);
  if (command(buf, buf, 2)) {
    return true;
  }
  return false;
}

void NSTEP::TimerHit() {
  if (isConnected()) {
    if (command(":RT", buf, 4)) {
      int tmp;
      sscanf(buf, "%d", &tmp);
      if (tmp != temperature) {
        temperature = tmp;
        TempN[0].value = temperature/10.0;
        defineNumber(&TempNP);
      }
    } else {
      IDMessage(getDeviceName(), "Failed to read temperature");
    }
    if (command(":RP", buf, 7)) {
      int tmp;
      sscanf(buf, "%d", &tmp);
      if (tmp != position) {
        position = tmp;
        FocusAbsPosN[0].value = position;
        defineNumber(&FocusAbsPosNP);
      }
    } else {
      IDMessage(getDeviceName(), "Failed to read position");
    }
    if (command("S", buf, 1)) {
      if (*buf == '0' && FocusAbsPosNP.s == IPS_BUSY) {
        FocusAbsPosNP.s = IPS_OK;
        defineNumber(&FocusAbsPosNP);
        FocusRelPosNP.s = IPS_OK;
        defineNumber(&FocusRelPosNP);
      }
    }
    SetTimer(POLLMS);
  }
}


bool NSTEP::saveConfigItems(FILE *fp) {
  IUSaveConfigSwitch(fp, &TempCompSP);
  IUSaveConfigNumber(fp, &TempCompNP);
  return INDI::Focuser::saveConfigItems(fp);
}

