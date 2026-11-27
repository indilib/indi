/*
    OnStepX INDI Driver — Focuser device (shared by both binaries)
    Copyright (C) 2026 Ed Lee

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


    One INDI::Focuser child device per physical focuser slot (slots 1-6).
    The parent device (OnStepXMount or OnStepXAux) creates instances after
    Handshake, calls setComm() to wire in the shared comm object, then
    calls ISGetProperties(nullptr) to announce the device to INDI clients,
    and setConnected() to follow its own connection state.

    1 tick = 1 micron throughout this class.

    Protocol (verified against OnStepX 10.28 Focuser.command.cpp).
    Every command addresses its slot directly as :F<slot><cmd>#, e.g. :F2G#.
    This is atomic and, unlike :FA<slot>#, leaves the firmware's active
    focuser (used by SHC/SWS) unchanged.
      G       — position, microns                     (text, may be signed)
      I / M   — minimum / maximum position, microns   (text)
      T       — status "M<rate>" moving / "S<rate>"   (text)
      t       — temperature, deg C                    (text)
      S<n>    — goto absolute position n microns      (bare '1'/'0')
      R<±n>   — goto relative by n microns            (no reply)
      Q       — stop                                  (no reply)
      B<n>    — backlash, microns                     (bare '1'/'0')
      Z       — set current position as zero          (no reply)
      h / H   — go to home / set current position as home (no reply)
      c       — temperature compensation on?          (bare '1'/'0')
      c0 / c1 — temperature compensation off / on     (bare '1'; the firmware
                quietly stays off without a temperature reading, so the
                driver reads it back)
      C / C<v> — compensation coefficient, microns per deg C (text / bare '1')
      d / d<n> — compensation deadband, steps 1..10000 (text / bare '1')
      e       — temperature change since compensation started, deg C (text)
      5..9    — goto rate 0.5x / 0.66x / 1x / 1.5x / 2x (no reply)

    Direction: INDI FOCUS_INWARD decreases the position, FOCUS_OUTWARD
    increases it.

    INDI Properties (standard FocuserInterface):
      ABS_FOCUS_POSITION    IP_RW  1 number: position (microns), updated
                            during a move as well as at its end
      REL_FOCUS_POSITION    IP_RW  1 number: relative move (microns)
      FOCUS_ABORT_MOTION    IP_RW  1 switch
      FOCUS_SYNC            IP_RW  only 0 is supported (:F<n>Z#)
      FOCUS_BACKLASH_STEPS  IP_RW  1 number: backlash (microns), read at connect;
                            the toggle off sends 0.  Not saved to the INDI
                            config (the firmware keeps it in NV).
      FOCUS_MAX             IP_RO  travel maximum from the firmware (:F<n>M#)
      FOCUS_TEMPERATURE_NP  IP_RO  1 number: temperature (deg C); only with a
                            temperature source (the firmware answers "nan")
    Custom properties:
      OSX_FOCUS_HOME        IP_RW  ISR_ATMOST1  Go to Home / Set as Home (momentary)
      OSX_FOCUS_TCF         IP_RW  ISR_1OFMANY  temperature compensation Off / On
      OSX_FOCUS_TCF_SETTINGS IP_RW 2 numbers: coefficient (um/C), deadband (steps)
      OSX_FOCUS_TCF_DELTA   IP_RO  1 number: temperature change since it started (C)
    The temperature compensation properties appear only when the focuser has
    a temperature reading.  They are kept in the firmware's NV, so they are
    not saved to the INDI config.
*/

#pragma once

#include "OnStepXComm.h"

#include <indifocuser.h>

#include <array>
#include <cstdint>
#include <memory>

class OnStepXFocuser : public INDI::Focuser
{
    public:
        // Device name "<parentName> Focuser <slot>", e.g. "OnStepX Focuser 1":
        // grouped with its controller, and unique when one server runs two.
        explicit OnStepXFocuser(int slot, const char *parentName = "OnStepX");

        // Called by parent after Handshake: wire in the shared comm object.
        void setComm(OnStepXComm *comm)
        {
            m_comm = comm;
        }

        // Periodic status update — called from the parent's poll throttle.
        void pollStatus();

        // DefaultDevice
        const char *getDefaultName() override;
        bool initProperties() override;
        bool updateProperties() override;
        bool saveConfigItems(FILE *fp) override;

        // Connection — no port to open; the parent activates us.
        bool Connect() override;
        bool Disconnect() override;

        // Timer — follows an in-progress move (pollMove())
        void TimerHit() override;

        // During a move: publish the position as it changes (state stays
        // Busy), and finish the move once the firmware reports it stopped.
        // Clients take ABS_FOCUS_POSITION as the current position, and the
        // INDI base turns a relative move into an absolute target from it.
        void pollMove();

        // FocuserInterface
        IPState MoveAbsFocuser(uint32_t targetTicks) override;
        IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        bool    AbortFocuser() override;
        bool    SyncFocuser(uint32_t ticks) override;
        bool    SetFocuserBacklash(int32_t steps) override;
        bool    SetFocuserBacklashEnabled(bool enabled) override;
        bool    SetFocuserSpeed(int speed) override;

        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

        // Build ":F<slot><body>#" into buf.  Exposed for tests.
        static bool formatCmd(int slot, const char *body, char *buf, size_t len);

        using Children = std::array<std::unique_ptr<OnStepXFocuser>, 6>;

        // Parent connected: create a child for each slot in focuserMask the
        // first time it is seen (announced, then properties defined after a
        // short delay so clients can process the new device first), and
        // reconnect existing children.
        static void connectChildren(Children &children, uint8_t focuserMask, OnStepXComm *comm,
                                    const char *parentName);

        // Parent disconnecting: mark every child disconnected.
        static void disconnectChildren(Children &children);

    private:
        int           m_slot;
        OnStepXComm  *m_comm  { nullptr };
        char          m_name[MAXINDIDEVICE] {};  // "<parent> Focuser N"
        bool          m_moving { false };

        // Custom: temperature readout (not in FocuserInterface)
        INDI::PropertyNumber m_temperatureNP {1};
        // Custom: go to home / set current position as home (momentary)
        INDI::PropertySwitch m_homeSP {2};
        // Custom: temperature compensation
        INDI::PropertySwitch m_tcfSP {2};
        INDI::PropertyNumber m_tcfNP {2};
        INDI::PropertyNumber m_tcfDeltaNP {1};
        bool m_hasTcf { false };

        void readTcf();
        void readTcfDelta();

        bool sendText(const char *body, char *reply);
        bool sendBool(const char *body);
        bool sendNoReply(const char *body);

        bool cmdGetPos(uint32_t &pos);
        bool cmdGetLimits(uint32_t &minPos, uint32_t &maxPos);
        bool cmdGetStatus(bool &moving);
        bool cmdGetTemperature(double &tempC);
        void finishMove();
};
