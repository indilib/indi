/*
    Askar-WAF Focuser
    Copyright (C) 2026 Jiaxing Ruixing Optical Instrument Co., Ltd.

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
   USA

*/

#include "askar_waf.h"

#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

static std::unique_ptr<AskarWAF> askarWAF(new AskarWAF());

AskarWAF::AskarWAF() {
  setVersion(1, 0);

  // Absolute & relative motion, abort, sync, reverse and backlash compensation
  // are all natively supported by the Askar-WAF "F...#" command set.
  FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE |
                    FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC | FOCUSER_CAN_REVERSE |
                    FOCUSER_HAS_BACKLASH);
}

bool AskarWAF::initProperties() {
  INDI::Focuser::initProperties();

  FirmwareVersionTP[VERSION].fill("VERSION", "Firmware", "Unknown");
  FirmwareVersionTP[MODEL].fill("MODEL", "Model", "Unknown");
  FirmwareVersionTP.fill(getDeviceName(), "FOCUS_FIRMWARE", "Firmware",
                         MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

  // Max travel: firmware accepts 100..1,000,000 logical steps (FMn#)
  FocusMaxPosNP[0].setMin(100);
  FocusMaxPosNP[0].setMax(1000000);
  FocusMaxPosNP[0].setStep(1000);
  FocusMaxPosNP[0].setValue(100000);

  FocusAbsPosNP[0].setMin(0);
  FocusAbsPosNP[0].setMax(100000);
  FocusAbsPosNP[0].setStep(1000);
  FocusAbsPosNP[0].setValue(0);

  FocusRelPosNP[0].setMin(0);
  FocusRelPosNP[0].setMax(50000);
  FocusRelPosNP[0].setStep(1000);
  FocusRelPosNP[0].setValue(0);

  FocusSyncNP[0].setMin(0);
  FocusSyncNP[0].setMax(100000);
  FocusSyncNP[0].setStep(1000);

  // Backlash: firmware accepts 0..10000 steps (FBn#); 0 disables compensation
  FocusBacklashNP[0].setMin(0);
  FocusBacklashNP[0].setMax(10000);
  FocusBacklashNP[0].setStep(10);
  FocusBacklashNP[0].setValue(0);

  setDefaultPollingPeriod(500);
  addDebugControl();

  // Baud rate is irrelevant over USB CDC-ACM, but a value is still required to
  // open the port.
  serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

  return true;
}

bool AskarWAF::updateProperties() {
  INDI::Focuser::updateProperties();

  if (isConnected()) {
    defineProperty(FirmwareVersionTP);

    bool ok = true;

    if (!readMaxPosition()) {
      FocusMaxPosNP.setState(IPS_ALERT);
      ok = false;
    }

    if (!readPosition()) {
      FocusAbsPosNP.setState(IPS_ALERT);
      ok = false;
    }

    if (!readBacklash()) {
      FocusBacklashNP.setState(IPS_ALERT);
      FocusBacklashSP.setState(IPS_ALERT);
      ok = false;
    }

    if (!readReverse()) {
      FocusReverseSP.setState(IPS_ALERT);
      ok = false;
    }

    lastPosition = static_cast<uint32_t>(FocusAbsPosNP[0].getValue());

    FocusMaxPosNP.apply();
    FocusAbsPosNP.apply();
    FocusBacklashNP.apply();
    FocusBacklashSP.apply();
    FocusReverseSP.apply();

    if (ok)
      LOG_INFO("Askar-WAF parameters updated, focuser ready for use.");
    else
      LOG_ERROR("Failed to read one or more Askar-WAF parameters after "
                "connect.");
  } else {
    deleteProperty(FirmwareVersionTP);
  }

  return true;
}

bool AskarWAF::Handshake() {
  char res[WAF_RES] = {0};

  for (int i = 0; i < 3; i++) {
    if (sendCommand("FV#", res, true)) {
      char version[16] = {0};
      if (sscanf(res, "FV%15[^#]#", version) == 1) {
        FirmwareVersionTP[VERSION].setText(version);
        LOGF_INFO("Askar-WAF is online. Firmware version %s.", version);

        char model[32] = {0};
        if (sendCommand("FI#", res, true) &&
            sscanf(res, "FI%31[^#]#", model) == 1)
          FirmwareVersionTP[MODEL].setText(model);

        return true;
      }
    }

    sleep(1);
  }

  LOG_ERROR("Error retrieving data from Askar-WAF, please ensure the focuser "
            "is powered and the port is correct.");
  return false;
}

const char *AskarWAF::getDefaultName() { return "Askar WAF"; }

bool AskarWAF::sendCommand(const char *cmd, char *res, bool silent) {
  int nbytes_written = 0, nbytes_read = 0, rc = TTY_OK;

  tcflush(PortFD, TCIOFLUSH);

  LOGF_DEBUG("CMD <%s>", cmd);

  if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK) {
    char errstr[MAXRBUF];
    tty_error_msg(rc, errstr, MAXRBUF);
    if (!silent)
      LOGF_ERROR("Serial write error: %s.", errstr);
    return false;
  }

  if (res == nullptr) {
    tcdrain(PortFD);
    return true;
  }

  // Leave one byte for the trailing '\0': tty_nread_section can return
  // nbytes_read == nsize when the stop char is the last byte read.
  if ((rc = tty_nread_section(PortFD, res, WAF_RES - 1, WAF_DEL, WAF_TIMEOUT,
                              &nbytes_read)) != TTY_OK) {
    char errstr[MAXRBUF];
    tty_error_msg(rc, errstr, MAXRBUF);
    if (!silent)
      LOGF_ERROR("Serial read error: %s.", errstr);
    return false;
  }

  // nbytes_read includes the trailing '#'
  res[nbytes_read] = '\0';

  LOGF_DEBUG("RES <%s>", res);

  if (strcmp(res, "FE#") == 0) {
    if (!silent)
      LOGF_ERROR("Askar-WAF rejected command <%s>.", cmd);
    return false;
  }

  return true;
}

bool AskarWAF::readPosition() {
  char res[WAF_RES] = {0};

  if (!sendCommand("Fp#", res))
    return false;

  int32_t pos = 0;
  if (sscanf(res, "Fp%d#", &pos) != 1) {
    LOGF_ERROR("Invalid position response <%s>.", res);
    return false;
  }

  FocusAbsPosNP[0].setValue(pos);
  return true;
}

bool AskarWAF::readMaxPosition() {
  char res[WAF_RES] = {0};

  if (!sendCommand("Fm#", res))
    return false;

  int32_t maxStep = 0;
  if (sscanf(res, "Fm%d#", &maxStep) != 1) {
    LOGF_ERROR("Invalid max travel response <%s>.", res);
    return false;
  }

  FocusMaxPosNP[0].setValue(maxStep);

  FocusAbsPosNP[0].setMax(maxStep);
  FocusAbsPosNP[0].setMin(0);
  FocusAbsPosNP[0].setStep(maxStep / 50.0);

  FocusSyncNP[0].setMax(maxStep);
  FocusSyncNP[0].setMin(0);
  FocusSyncNP[0].setStep(maxStep / 50.0);

  FocusRelPosNP[0].setMax(maxStep / 2.0);
  FocusRelPosNP[0].setMin(0);
  FocusRelPosNP[0].setStep(maxStep / 100.0);

  FocusAbsPosNP.updateMinMax();
  FocusSyncNP.updateMinMax();
  FocusRelPosNP.updateMinMax();
  SyncPresets(static_cast<uint32_t>(maxStep));

  return true;
}

bool AskarWAF::readBacklash() {
  char res[WAF_RES] = {0};

  if (!sendCommand("Fb#", res))
    return false;

  int32_t backlash = 0;
  if (sscanf(res, "Fb%d#", &backlash) != 1) {
    LOGF_ERROR("Invalid backlash response <%s>.", res);
    return false;
  }

  FocusBacklashNP[0].setValue(backlash);

  FocusBacklashSP.reset();
  FocusBacklashSP[INDI_ENABLED].setState(backlash > 0 ? ISS_ON : ISS_OFF);
  FocusBacklashSP[INDI_DISABLED].setState(backlash > 0 ? ISS_OFF : ISS_ON);

  return true;
}

bool AskarWAF::readReverse() {
  char res[WAF_RES] = {0};

  if (!sendCommand("Fr#", res))
    return false;

  bool reversed = (strcmp(res, "Fr1#") == 0);

  FocusReverseSP.reset();
  FocusReverseSP[INDI_ENABLED].setState(reversed ? ISS_ON : ISS_OFF);
  FocusReverseSP[INDI_DISABLED].setState(reversed ? ISS_OFF : ISS_ON);

  return true;
}

bool AskarWAF::isMoving(bool *ok) {
  char res[WAF_RES] = {0};

  if (!sendCommand("FQ#", res)) {
    if (ok)
      *ok = false;
    return true;
  }

  if (strcmp(res, "FQ1#") == 0) {
    if (ok)
      *ok = true;
    return true;
  }

  if (strcmp(res, "FQ0#") == 0) {
    if (ok)
      *ok = true;
    return false;
  }

  LOGF_ERROR("Unknown isMoving response <%s>.", res);
  if (ok)
    *ok = false;
  return true;
}

IPState AskarWAF::MoveAbsFocuser(uint32_t targetTicks) {
  char cmd[WAF_RES] = {0};
  char res[WAF_RES] = {0};
  snprintf(cmd, sizeof(cmd), "FP%u#", targetTicks);

  if (!sendCommand(cmd, res))
    return IPS_ALERT;

  m_MotionQueryFailures = 0;
  return IPS_BUSY;
}

IPState AskarWAF::MoveRelFocuser(FocusDirection dir, uint32_t ticks) {
  char cmd[WAF_RES] = {0};
  char res[WAF_RES] = {0};
  snprintf(cmd, sizeof(cmd), "FT%c%u#", (dir == FOCUS_INWARD) ? '-' : '+',
           ticks);

  if (!sendCommand(cmd, res))
    return IPS_ALERT;

  m_MotionQueryFailures = 0;
  return IPS_BUSY;
}

bool AskarWAF::AbortFocuser() {
  char res[WAF_RES] = {0};

  if (!sendCommand("FS#", res))
    return false;

  m_MotionQueryFailures = 0;
  return true;
}

bool AskarWAF::SyncFocuser(uint32_t ticks) {
  char cmd[WAF_RES] = {0};
  char res[WAF_RES] = {0};
  snprintf(cmd, sizeof(cmd), "FY%u#", ticks);

  return sendCommand(cmd, res);
}

bool AskarWAF::ReverseFocuser(bool enabled) {
  char res[WAF_RES] = {0};

  return sendCommand(enabled ? "FR1#" : "FR0#", res);
}

bool AskarWAF::SetFocuserBacklash(int32_t steps) {
  char cmd[WAF_RES] = {0};
  char res[WAF_RES] = {0};
  snprintf(cmd, sizeof(cmd), "FB%d#", steps);

  return sendCommand(cmd, res);
}

bool AskarWAF::SetFocuserBacklashEnabled(bool enabled) {
  if (!enabled)
    return SetFocuserBacklash(0);

  int32_t steps = static_cast<int32_t>(FocusBacklashNP[0].getValue());
  return SetFocuserBacklash(steps > 0 ? steps : 1);
}

bool AskarWAF::SetFocuserMaxPosition(uint32_t ticks) {
  char cmd[WAF_RES] = {0};
  char res[WAF_RES] = {0};
  snprintf(cmd, sizeof(cmd), "FM%u#", ticks);

  // The firmware rejects FMn# (FE#) if the current position exceeds the new max
  // travel.
  if (!sendCommand(cmd, res))
    return false;

  SyncPresets(ticks);
  return true;
}

void AskarWAF::TimerHit() {
  if (!isConnected())
    return;

  const bool positionOk = readPosition();
  if (positionOk) {
    if (static_cast<uint32_t>(FocusAbsPosNP[0].getValue()) != lastPosition) {
      FocusAbsPosNP.apply();
      lastPosition = static_cast<uint32_t>(FocusAbsPosNP[0].getValue());
    }
  }

  if (FocusAbsPosNP.getState() == IPS_BUSY ||
      FocusRelPosNP.getState() == IPS_BUSY) {
    bool queryOk = false;
    const bool moving = isMoving(&queryOk);

    if (!queryOk) {
      if (++m_MotionQueryFailures >= WAF_MOTION_QUERY_MAX_FAILURES) {
        FocusAbsPosNP.setState(IPS_ALERT);
        FocusRelPosNP.setState(IPS_ALERT);
        FocusAbsPosNP.apply();
        FocusRelPosNP.apply();
        m_MotionQueryFailures = 0;
        LOG_ERROR("Lost contact with Askar-WAF while waiting for motion to "
                  "complete.");
      }
    } else {
      m_MotionQueryFailures = 0;

      // Only clear BUSY once motion has stopped and we have a fresh position.
      if (!moving && positionOk) {
        FocusAbsPosNP.setState(IPS_OK);
        FocusRelPosNP.setState(IPS_OK);
        FocusAbsPosNP.apply();
        FocusRelPosNP.apply();
        LOG_INFO("Focuser reached requested position.");
      }
    }
  }

  SetTimer(getCurrentPollingPeriod());
}
