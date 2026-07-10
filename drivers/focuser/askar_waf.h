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

#pragma once

#include "indifocuser.h"

class AskarWAF : public INDI::Focuser {
public:
  AskarWAF();
  virtual ~AskarWAF() override = default;

  const char *getDefaultName() override;
  virtual bool initProperties() override;
  virtual bool updateProperties() override;

protected:
  /**
   * @brief Handshake Try to communicate with the focuser and read its firmware
   * version.
   * @return True if communication is successful, false otherwise.
   */
  virtual bool Handshake() override;

  virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
  virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
  virtual bool AbortFocuser() override;
  virtual bool SyncFocuser(uint32_t ticks) override;
  virtual bool ReverseFocuser(bool enabled) override;
  virtual bool SetFocuserBacklash(int32_t steps) override;
  virtual bool SetFocuserBacklashEnabled(bool enabled) override;
  virtual bool SetFocuserMaxPosition(uint32_t ticks) override;
  virtual void TimerHit() override;

private:
  /**
   * @brief sendCommand Send a command frame (e.g. "Fp#") and optionally read
   * the reply.
   * @param cmd Command to send, must already include the leading 'F' and
   * trailing '#'.
   * @param res If not nullptr, the reply (up to and including the trailing '#')
   * is stored here. Must be at least WAF_RES bytes (WAF_RES-1 payload + '\0').
   * @param silent If true, do not log errors.
   * @return True if the command was sent (and, if res is set, a valid non-error
   * reply was received).
   */
  bool sendCommand(const char *cmd, char *res = nullptr, bool silent = false);

  bool readPosition();
  bool readMaxPosition();
  bool readBacklash();
  bool readReverse();
  /**
   * @brief isMoving Query whether the focuser is currently moving.
   * @param ok If not nullptr, set to true when a valid FQ0#/FQ1# reply was
   * received, false on communication or parse failure.
   * @return True if moving (or query failed — caller should use ok to tell).
   */
  bool isMoving(bool *ok = nullptr);

  uint32_t lastPosition{0};
  uint8_t m_MotionQueryFailures{0};

  // Firmware version & model name, read once at connection time
  INDI::PropertyText FirmwareVersionTP{2};
  enum { VERSION, MODEL };

  // Askar-WAF response buffer size (includes room for trailing '\0')
  static const uint8_t WAF_RES{32};
  // Askar-WAF frame delimiter
  static const char WAF_DEL{'#'};
  // Askar-WAF read timeout in seconds
  static const uint8_t WAF_TIMEOUT{3};
  // Consecutive FQ# failures before aborting a busy move as IPS_ALERT
  static const uint8_t WAF_MOTION_QUERY_MAX_FAILURES{5};
};
