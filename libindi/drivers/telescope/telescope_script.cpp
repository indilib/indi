/*******************************************************************************
 Copyright(c) 2016 CloudMakers, s. r. o.. All rights reserved.
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <memory>

#include "telescope_script.h"

#define	POLLMS      1000

enum { SCRIPT_CONNECT = 1, SCRIPT_DISCONNECT, SCRIPT_STATUS, SCRIPT_GOTO, SCRIPT_SYNC, SCRIPT_PARK, SCRIPT_UNPARK, SCRIPT_MOVE_NORTH, SCRIPT_MOVE_EAST, SCRIPT_MOVE_SOUTH, SCRIPT_MOVE_WEST, SCRIPT_ABORT } scripts;

std::unique_ptr<ScopeScript> scope_script(new ScopeScript());

void ISGetProperties(const char *dev) {
  scope_script->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
  scope_script->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num) {
  scope_script->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
  scope_script->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root) {
  scope_script->ISSnoopDevice(root);
}

ScopeScript::ScopeScript() {
  SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT, 4);
}

ScopeScript::~ScopeScript() {
}

const char * ScopeScript::getDefaultName() {
  return (char *)"Telescope Scripting Gateway";
}

bool ScopeScript::initProperties() {
  INDI::Telescope::initProperties();
  
#ifdef OSX_EMBEDED_MODE
  IUFillText(&ScriptsT[0], "FOLDER", "Folder", "/usr/local/share/indi/scripts");
#else
  IUFillText(&ScriptsT[0], "FOLDER", "Folder", "/usr/share/indi/scripts");
#endif
  IUFillText(&ScriptsT[1], "SCRIPT_CONNECT", "Connect script", "connect.py");
  IUFillText(&ScriptsT[2], "SCRIPT_DISCONNECT", "Disconnect script", "disconnect.py");
  IUFillText(&ScriptsT[3], "SCRIPT_STATUS", "Get status script", "status.py");
  IUFillText(&ScriptsT[4], "SCRIPT_GOTO", "Goto script", "goto.py");
  IUFillText(&ScriptsT[5], "SCRIPT_SYNC", "Sync script", "sync.py");
  IUFillText(&ScriptsT[6], "SCRIPT_PARK", "Park script", "park.py");
  IUFillText(&ScriptsT[7], "SCRIPT_UNPARK", "Unpark script", "unpark.py");
  IUFillText(&ScriptsT[8], "SCRIPT_MOVE_NORTH", "Move north script", "move_north.py");
  IUFillText(&ScriptsT[9], "SCRIPT_MOVE_EAST", "Move east script", "move_east.py");
  IUFillText(&ScriptsT[10], "SCRIPT_MOVE_SOUTH", "Move south script", "move_south.py");
  IUFillText(&ScriptsT[11], "SCRIPT_MOVE_WEST", "Move west script", "move_west.py");
  IUFillText(&ScriptsT[12], "SCRIPT_ABORT", "Abort motion script", "abort.py");
 IUFillTextVector(&ScriptsTP, ScriptsT, 13, getDefaultName(), "SCRIPTS", "Scripts", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
  
  addDebugControl();
  setDriverInterface(getDriverInterface());
  return true;
}

bool ScopeScript::saveConfigItems(FILE *fp) {
  INDI::Telescope::saveConfigItems(fp);
  IUSaveConfigText(fp, &ScriptsTP);
  return true;
}

void ScopeScript::ISGetProperties (const char *dev) {
  INDI::Telescope::ISGetProperties (dev);
  defineText(&ScriptsTP);
}

bool ScopeScript::RunScript(int script, char *arg1, char *arg2, char *arg3) {
  char path[256];
  snprintf(path, 256, "%s/%s", ScriptsT[0].text, ScriptsT[script].text);
  //DEBUGF(INDI::Logger::DBG_SESSION, "%s %s %s", ScriptsT[script].text, arg1, arg2);
  int pid = fork();
  if (pid == -1) {
    DEBUG(INDI::Logger::DBG_ERROR, "Fork failed");
    return false;
  } else if (pid == 0) {
    char *args[]={ path, arg1, arg2, arg3, NULL };
    execvp(path, args);
    DEBUGF(INDI::Logger::DBG_ERROR, "Script %s execution failed", path);
    return false;
  } else {
    int status;
    waitpid(pid, &status, 0);
    return status == 0;
  }
}

bool ScopeScript::Connect() {
  if(isConnected())
    return true;
  bool status = RunScript(SCRIPT_CONNECT, NULL, NULL, NULL);
  if (status) {
    DEBUG(INDI::Logger::DBG_SESSION, "Succesfully connected");
    ReadScopeStatus();
    SetTimer(POLLMS);
  }
  return status;
}

bool ScopeScript::Disconnect() {
  bool status = RunScript(SCRIPT_DISCONNECT, NULL, NULL, NULL);
  if (status) {
    DEBUG(INDI::Logger::DBG_SESSION, "Succesfully disconnected");
  }
  return status;
}

bool ScopeScript::ReadScopeStatus() {
  char *name = tmpnam(NULL);
  bool status = RunScript(SCRIPT_STATUS, name, NULL, NULL);
  if (status) {
    int parked;
    float ra, dec;
    FILE *file = fopen(name, "r");
    fscanf(file, "%d %f %f", &parked, &ra, &dec);
    fclose(file);
    unlink(name);
    if (parked != 0) {
      if (!isParked())
        SetParked(true);
    } else {
      if (isParked())
        SetParked(false);
    }
    NewRaDec(ra, dec);
  } else {
    DEBUG(INDI::Logger::DBG_SESSION, "Failed to read status");
  }
  return status;
}

bool ScopeScript::Goto(double ra, double dec) {
  char _ra[16], _dec[16];
  snprintf(_ra, 16, "%f", ra);
  snprintf(_dec, 16, "%f", dec);
  bool status = RunScript(SCRIPT_GOTO, _ra, _dec, NULL);
  if (status) {
    DEBUG(INDI::Logger::DBG_SESSION, "Goto succesfully executed");
  }
  return status;
}

bool ScopeScript::Sync(double ra, double dec) {
  char _ra[16], _dec[16];
  snprintf(_ra, 16, "%f", ra);
  snprintf(_dec, 16, "%f", dec);
  bool status = RunScript(SCRIPT_SYNC, _ra, _dec, NULL);
  if (status) {
    DEBUG(INDI::Logger::DBG_SESSION, "Sync succesfully executed");
  }
  return status;
}

bool ScopeScript::Park() {
  bool status = RunScript(SCRIPT_PARK, NULL, NULL, NULL);
  if (status) {
    DEBUG(INDI::Logger::DBG_SESSION, "Park succesfully executed");
    SetParked(true);
  }
  return status;
}

bool ScopeScript::UnPark() {
  bool status = RunScript(SCRIPT_UNPARK, NULL, NULL, NULL);
  if (status) {
    DEBUG(INDI::Logger::DBG_SESSION, "Unpark succesfully executed");
    SetParked(false);
  }
  return status;
}

bool ScopeScript::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) {
  char _rate[] = { (char)('0' + IUFindOnSwitchIndex(&SlewRateSP)), 0 };
  bool status = RunScript(command == MOTION_STOP ? SCRIPT_ABORT : dir == DIRECTION_NORTH ? SCRIPT_MOVE_NORTH : SCRIPT_MOVE_SOUTH, _rate, NULL, NULL);
  return status;
}

bool ScopeScript::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) {
  char _rate[] = { (char)('0' + IUFindOnSwitchIndex(&SlewRateSP)), 0 };
  bool status = RunScript(command == MOTION_STOP ? SCRIPT_ABORT : dir == DIRECTION_NORTH ? SCRIPT_MOVE_NORTH : SCRIPT_MOVE_SOUTH, _rate, NULL, NULL);
  return status;
}

bool ScopeScript::Abort() {
  bool status = RunScript(SCRIPT_ABORT, NULL, NULL, NULL);
  if (status) {
    DEBUG(INDI::Logger::DBG_SESSION, "Succesfully aborted");
  }
  return status;
}

