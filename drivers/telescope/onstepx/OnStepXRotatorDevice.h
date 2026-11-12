/*
    OnStepX INDI Driver — Rotator child device (both binaries)
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

    The controller's rotator (Axis3) as its own INDI device,
    "<parent> Rotator", like the focusers.  Ekos picks a rotator per optical
    train from devices with ROTATOR_INTERFACE, so a separate device shows
    there under its own name, with its own tabs, config file and debug
    switch.  The parent device (OnStepXMount or OnStepXAux) creates it after
    Handshake when Capabilities::hasRotator is set, shares its OnStepXComm,
    and polls it from its own poll throttle, so all controller I/O stays on
    the parent's timer.

    The protocol and the de-rotator properties are in OnStepXRotator, which
    this device owns; see that header for the commands.

    INDI Properties (Main Control, via INDI::Rotator / RotatorInterface):
      ABS_ROTATOR_ANGLE, ROTATOR_ABORT_MOTION, SYNC_ROTATOR_ANGLE (0 deg or
      half travel only), ROTATOR_HOME, ROTATOR_BACKLASH_TOGGLE/_STEPS,
      ROTATOR_LIMITS; Presets tab: Presets, Goto.
    Custom (Main Control, de-rotator capable controllers only):
      OSX_ROT_DEROTATE, OSX_ROT_PARALLACTIC, OSX_ROT_DEROTATE_REVERSE.
    Not saved to the INDI config: the backlash, which the firmware keeps in
    NV (a config reload would overwrite changes from the hand controller).
*/

#pragma once

#include "OnStepXRotator.h"

#include <indirotator.h>

#include <memory>

class OnStepXComm;

class OnStepXRotatorDevice : public INDI::Rotator
{
    public:
        // Device name "<parentName> Rotator", e.g. "OnStepX Rotator".
        explicit OnStepXRotatorDevice(const char *parentName = "OnStepX");

        void setComm(OnStepXComm *comm);

        // Whether the de-rotator properties are shown (:GX98# 'D').  Set
        // before each connect; the firmware can change between connections.
        void setDerotator(bool hasDerotator)
        {
            m_hasDerotator = hasDerotator;
        }

        // Periodic angle/motion poll — called from the parent's poll throttle.
        void pollStatus();

        // DefaultDevice
        const char *getDefaultName() override;
        bool initProperties() override;
        bool updateProperties() override;
        bool saveConfigItems(FILE *fp) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

        // Connection — no port to open; the parent activates us.
        bool Connect() override;
        bool Disconnect() override;

        // RotatorInterface
        IPState MoveRotator(double angle) override;
        bool    AbortRotator() override;
        IPState HomeRotator() override;
        bool    SetRotatorBacklash(int32_t steps) override;   // the toggle off sends 0 (RI default)
        bool    SyncRotator(double angle) override;

        // Parent connected: create the child the first time (announced, then
        // properties defined after a short delay so clients process the new
        // device first), or reconnect it.  Does nothing without a rotator.
        static void connectChild(std::unique_ptr<OnStepXRotatorDevice> &child, bool hasRotator,
                                 bool hasDerotator, OnStepXComm *comm, const char *parentName);

        // Parent disconnecting: mark the child disconnected.
        static void disconnectChild(std::unique_ptr<OnStepXRotatorDevice> &child);

    private:
        char           m_name[MAXINDIDEVICE] {};   // "<parent> Rotator"
        OnStepXRotator m_rotator;
        OnStepXComm   *m_comm { nullptr };
        bool           m_hasDerotator { false };

        void applyInitialState();
};
