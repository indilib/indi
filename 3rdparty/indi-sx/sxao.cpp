/*
 Starlight Xpress Active Optics INDI Driver

 Copyright (c) 2012 Cloudmakers, s. r. o.
 All Rights Reserved.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

#include <stdio.h>
#include <memory>
#include <unistd.h>

#include <libindi/indicom.h>

#include "sxconfig.h"
#include "sxao.h"

#define TRACE(c) (c)

std::auto_ptr<SXAO> sxao(0);

void ISInit() {
  static bool isInit = false;

  if (!isInit) {
    isInit = 1;
    if (!sxao.get())
      sxao.reset(new SXAO());
  }
}

void ISGetProperties(const char *dev) {
  TRACE(fprintf(stderr, "-> ISGetProperties(%s)\n", dev));
  ISInit();
  sxao->ISGetProperties(dev);
  TRACE(fprintf(stderr, "<- ISGetProperties\n"));
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
  TRACE(fprintf(stderr, "-> ISNewSwitch(%s, %s...)\n", dev, name));
  ISInit();
  sxao->ISNewSwitch(dev, name, states, names, num);
  TRACE(fprintf(stderr, "<- ISNewSwitch\n"));
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num) {
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(texts);
  INDI_UNUSED(names);
  INDI_UNUSED(num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
  TRACE(fprintf(stderr, "-> ISNewNumber(%s, %s, ...)\n", dev, name));
  ISInit();
  sxao->ISNewNumber(dev, name, values, names, num);
  TRACE(fprintf(stderr, "<- ISNewNumber\n"));
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
  INDI_UNUSED(root);
}

int SXAO::aoCommand(const char *request, char *response, int nbytes) {
  TRACE(fprintf(stderr, "-> SXAO::aoCommand(\"%s\", ..., %d)\n", request, nbytes));
  int actual;
  int rc = tty_write(PortFD, request, strlen(request), &actual);
  TRACE(fprintf(stderr, "   tty_write -> %d\n", rc));
  if (rc == TTY_OK) {
    rc = tty_read(PortFD, response, nbytes, 5, &actual);
    response[actual] = 0;
    TRACE(fprintf(stderr, "   tty_read: -> %d\n", rc));
  }
  TRACE(fprintf(stderr, "<- SXAO::aoCommand \"%s\", %d\n", response, rc));
  return rc;
}

SXAO::SXAO() {
  TRACE(fprintf(stderr, "-> SXAO::SXAO()\n"));
  setDeviceName(getDefaultName());
  setVersion(VERSION_MAJOR, VERSION_MINOR);
  TRACE(fprintf(stderr, "<- SXAO::SXAO\n"));
}

SXAO::~SXAO() {
  TRACE(fprintf(stderr, "-> SXAO::~SXAO()\n"));
  TRACE(fprintf(stderr, "<- SXAO::~SXAO\n"));
}

const char * SXAO::getDefaultName() {
  return (char *) "SX AO";
}

bool SXAO::Connect() {
  TRACE(fprintf(stderr, "-> SXAO::Connect()\n"));
  if (isConnected()) {
    TRACE(fprintf(stderr, "<- SXAO::Connect 1\n"));
    return true;
  }
  int connectrc = 0;
  char buf[MAXRBUF];
  bool rc;
  const char *port = PortT[0].text;

  if ((rc = tty_connect(port, 9600, 8, 0, 1, &PortFD)) != TTY_OK) {
    tty_error_msg(rc, buf, MAXRBUF);
    IDMessage(getDeviceName(), "Failed to connect to SXAO on %s (%s)", port, buf);
    TRACE(fprintf(stderr, "<- SXAO::Connect 0\n"));
    return false;
  }

  rc = aoCommand("X", buf, 1);
  if (rc == TTY_OK) {
    if (!strcmp(buf, "Y")) {
      aoCommand("V", buf, 4);
      IDMessage(getDeviceName(), "SXAO [%s] is connected on %s.", buf, port);
      AOCenter();
      TRACE(fprintf(stderr, "<- SXAO::Connect 1\n"));
      return true;
    } else {
      IDMessage(getDeviceName(), "Not a SXAO on %s", port);
      tty_disconnect(PortFD);
      TRACE(fprintf(stderr, "<- SXAO::Connect 0\n"));
      return false;
    }
  }

  IDMessage(getDeviceName(), "Failed to connect to SXAO on %s (%d)", port, rc);
  tty_disconnect(PortFD);

  TRACE(fprintf(stderr, "<- SXAO::Connect 0\n"));
  return false;
}

bool SXAO::Disconnect() {
  TRACE(fprintf(stderr, "-> SXAO::Disconnect()\n"));
  tty_disconnect(PortFD);
  IDMessage(getDeviceName(), "SXAO is disconnected.");
  TRACE(fprintf(stderr, "<- SXAO::Disconnect 1\n"));
  return true;
}

bool SXAO::initProperties() {
  TRACE(fprintf(stderr, "-> SXAO::initProperties()\n"));
  DefaultDevice::initProperties();
  initGuiderProperties(getDeviceName(), GUIDE_CONTROL_TAB);

#if __APPLE__
  IUFillText(&PortT[0], "PORT", "Port", "/dev/cu.usbserial-FTDFZ2FW");
#else
  IUFillText(&PortT[0], "PORT", "Port", "/dev/ttyUSB0");
#endif
  IUFillTextVector(&PortTP, PortT, 1, getDeviceName(), "DEVICE_PORT", "Ports", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

  defineText(&PortTP);

  IUFillNumber(&AONS[0], "AO_N", "North (steps)", "%d", 0, 80, 1, 0);
  IUFillNumber(&AONS[1], "AO_S", "South (steps)", "%d", 0, 80, 1, 0);
  IUFillNumberVector(&AONSNP, AONS, 2, getDeviceName(), "AO_NS", "AO Tilt North/South", GUIDE_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

  IUFillNumber(&AOWE[0], "AO_E", "East (steps)", "%d", 0, 80, 1, 0);
  IUFillNumber(&AOWE[1], "AO_W", "West (steps)", "%d", 0, 80, 1, 0);
  IUFillNumberVector(&AOWENP, AOWE, 2, getDeviceName(), "AO_WE", "AO Tilt East/West", GUIDE_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

  IUFillSwitch(&Center[0], "CENTER", "Center", ISS_OFF);
  IUFillSwitch(&Center[1], "UNJAM", "Unjam", ISS_OFF);
  IUFillSwitchVector(&CenterP, Center, 2, getDeviceName(), "AO_CENTER", "AO Center", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

  TRACE(fprintf(stderr, "<- SXAO::initProperties 1"));
  return true;
}

bool SXAO::updateProperties() {
  TRACE(fprintf(stderr, "-> SXAO::updateProperties() conneced=%d\n", isConnected()));

  INDI::DefaultDevice::updateProperties();

  if (isConnected()) {
    defineNumber (&GuideNSNP);
    defineNumber (&GuideWENP);
    defineNumber(&AONSNP);
    defineNumber(&AOWENP);
    defineSwitch(&CenterP);
  } else {
    deleteProperty(GuideNSNP.name);
    deleteProperty(GuideWENP.name);
    deleteProperty(AONSNP.name);
    deleteProperty(AOWENP.name);
    deleteProperty(CenterP.name);
  }

  TRACE(fprintf(stderr, "<- SXAO::updateProperties 1\n"));
  return true;
}

void SXAO::ISGetProperties(const char *dev) {
  TRACE(fprintf(stderr, "-> SXAO::ISGetProperties()\n"));
  DefaultDevice::ISGetProperties(dev);
  TRACE(fprintf(stderr, "<- SXAO::ISGetProperties\n"));
  return;
}

bool SXAO::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {
  if (strcmp(name, AONSNP.name) == 0) {
    AONSNP.s = IPS_BUSY;
    IUUpdateNumber(&AONSNP, values, names, n);
    IDSetNumber(&AONSNP, NULL);
    if (AONS[0].value != 0)
      AONorth(AONS[0].value);
    else if (AONS[1].value != 0)
      AOSouth(AONS[1].value);
    AONS[0].value = 0;
    AONS[1].value = 0;
    AONSNP.s = IPS_OK;
    IDSetNumber(&AONSNP, NULL);
    return true;
  } else if (strcmp(name, AOWENP.name) == 0) {
    AOWENP.s = IPS_BUSY;
    IUUpdateNumber(&AOWENP, values, names, n);
    IDSetNumber(&AOWENP, NULL);
    if (AOWE[0].value != 0)
      AOEast(AOWE[0].value);
    else
      AOWest(AOWE[1].value);
    AOWE[0].value = 0;
    AOWE[1].value = 0;
    AOWENP.s = IPS_OK;
    IDSetNumber(&AOWENP, NULL);
    return true;
  } else
    processGuiderProperties(name, values, names, n);
  return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool SXAO::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) {
  if (strcmp(dev, getDeviceName()) == 0) {
    if (strcmp(name, PortTP.name) == 0) {
      int rc;
      PortTP.s = IPS_OK;
      rc = IUUpdateText(&PortTP, texts, names, n);
      IDSetText(&PortTP, NULL);
      return true;
    }
  }
  return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool SXAO::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) {
  if (strcmp(name, "AO_CENTER") == 0) {
    CenterP.s = IPS_BUSY;
    IDSetSwitch(&CenterP, NULL);
    IUUpdateSwitch(&CenterP, states, names, n);
    if (Center[0].s == ISS_ON)
      AOCenter();
    else if (Center[1].s == ISS_ON)
      AOUnjam();
    Center[0].s = ISS_OFF;
    Center[1].s = ISS_OFF;
    CenterP.s = IPS_OK;
    IDSetSwitch(&CenterP, NULL);
    IUUpdateSwitch(&CenterP, states, names, n);
    return true;
  }
  return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool SXAO::GuideNorth(float ms) {
  TRACE(fprintf(stderr, "-> SXAO::GuideNorth(%f)\n", ms));
  char buf[8];
  sprintf(buf, "MN%05d", (int) (ms / 10));
  int rc = aoCommand(buf, buf, 1);
  TRACE(fprintf(stderr, "<- SXAO::GuideNorth %d\n", rc == TTY_OK));
  return rc == TTY_OK;
}

bool SXAO::GuideSouth(float ms) {
  TRACE(fprintf(stderr, "-> SXAO::GuideSouth(%f)\n", ms));
  char buf[8];
  sprintf(buf, "MS%05d", (int) (ms / 10));
  int rc = aoCommand(buf, buf, 1);
  TRACE(fprintf(stderr, "<- SXAO::GuideSouth %d\n", rc == TTY_OK));
  return rc == TTY_OK;
}

bool SXAO::GuideEast(float ms) {
  TRACE(fprintf(stderr, "-> SXAO::GuideEast(%f)\n", ms));
  char buf[8];
  sprintf(buf, "ME%05d", (int) (ms / 10));
  int rc = aoCommand(buf, buf, 1);
  TRACE(fprintf(stderr, "<- SXAO::GuideEast %d\n", rc == TTY_OK));
  return rc == TTY_OK;
}

bool SXAO::GuideWest(float ms) {
  TRACE(fprintf(stderr, "-> SXAO::GuideWest(%f)\n", ms));
  char buf[8];
  sprintf(buf, "MW%05d", (int) (ms / 10));
  int rc = aoCommand(buf, buf, 1);
  TRACE(fprintf(stderr, "<- SXAO::GuideWest %d\n", rc == TTY_OK));
  return rc == TTY_OK;
}

bool SXAO::AONorth(int steps) {
  TRACE(fprintf(stderr, "-> SXAO::AONorth(%d)\n", steps));
  char buf[8];
  sprintf(buf, "GN%05d", steps);
  int rc = aoCommand(buf, buf, 1);
  rc = rc == TTY_OK && !strcmp(buf, "G");
  TRACE(fprintf(stderr, "<- SXAO::AONorth %d\n", rc));
  return rc;
}

bool SXAO::AOSouth(int steps) {
  TRACE(fprintf(stderr, "-> SXAO::AOSouth(%d)\n", steps));
  char buf[8];
  sprintf(buf, "GS%05d", steps);
  int rc = aoCommand(buf, buf, 1);
  rc = rc == TTY_OK && !strcmp(buf, "G");
  TRACE(fprintf(stderr, "<- SXAO::AOSouth %d\n", rc));
  return rc;
}

bool SXAO::AOEast(int steps) {
  TRACE(fprintf(stderr, "-> SXAO::AOEast(%d)\n", steps));
  char buf[8];
  sprintf(buf, "GE%05d", steps);
  int rc = aoCommand(buf, buf, 1);
  rc = rc == TTY_OK && !strcmp(buf, "G");
  TRACE(fprintf(stderr, "<- SXAO::AOEast %d\n", rc));
  return rc;
}

bool SXAO::AOWest(int steps) {
  TRACE(fprintf(stderr, "-> SXAO::AOWest(%d)\n", steps));
  char buf[8];
  sprintf(buf, "GW%05d", steps);
  int rc = aoCommand(buf, buf, 1);
  rc = rc == TTY_OK && !strcmp(buf, "G");
  TRACE(fprintf(stderr, "<- SXAO::AOWest %d\n", rc));
  return rc;
}

bool SXAO::AOCenter() {
  TRACE(fprintf(stderr, "-> SXAO::AOCenter()\n"));
  char buf[8];
  int rc = aoCommand("K", buf, 1);
  TRACE(fprintf(stderr, "<- SXAO::AOCenter %d\n", rc == TTY_OK));
  return rc == TTY_OK;
}

bool SXAO::AOUnjam() {
  TRACE(fprintf(stderr, "-> SXAO::AOUnjam()\n"));
  char buf[8];
  int rc = aoCommand("R", buf, 1);
  TRACE(fprintf(stderr, "<- SXAO::AOUnjam %d\n", rc == TTY_OK));
  return rc == TTY_OK;
}

