/*
    OnStepX INDI Driver — PEC helper (mount binary only)
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Protocol (OnStepX v10.24c):
      :$QZ?#   — get PEC status: one char from {I,p,P,r,R}, optional '.' suffix = index detected
                 I=Ignore, p=ready-to-play, P=Playing, r=ready-to-record, R=Recording
      :$QZ+#   — enable PEC playback (no reply, even if refused)
      :$QZ-#   — disable PEC playback (no reply, even if refused)
      :$QZ/#   — ready to record PEC (no reply, even if refused)
      :$QZZ#   — clear PEC buffer (no reply, even if refused)
      :$QZ!#   — write PEC to EEPROM (no reply, even if refused)
      :VW#     — worm period (steps per revolution, integer)

    INDI Properties (PEC tab):
      OSX_PEC_STATE       IP_RO  5 lights: Ignored / Ready-to-Play / Playing / Ready-to-Record / Recording
      OSX_PEC_INDEX       IP_RO  2 lights: Not Detected / Detected
      OSX_PEC_CONTROL     IP_RW  ISR_ATMOST1  3 momentary switches: Play / Stop / Record (ready to record)
      OSX_PEC_DATA        IP_RW  ISR_ATMOST1  2 momentary switches: Clear buffer / Save to EEPROM
      OSX_PEC_WORM_STEPS  IP_RO  1 number: worm steps per revolution
*/

#pragma once

#include <defaultdevice.h>
#include <cstdio>

class OnStepXComm;

class OnStepXPec
{
    public:
        void setDevice(INDI::DefaultDevice *dev)
        {
            m_dev = dev;
        }
        void setComm(OnStepXComm *comm)
        {
            m_comm = comm;
        }

        // Called from device class initProperties
        void initProperties();

        // Called from device class updateProperties
        void updateProperties(bool connected);

        // Called from ISNewSwitch
        bool handleSwitch(const char *name, ISState *states, char *names[], int n);

        void saveConfig(FILE *fp);

        // Periodic status poll — call from ReadScopeStatus throttle (every ~10 polls)
        void pollStatus();

        // Read worm steps once on connect
        void readWormSteps();

    private:
        INDI::DefaultDevice *m_dev  { nullptr };
        OnStepXComm         *m_comm { nullptr };

        // PEC state lights: Ignored / Ready-to-Play / Playing / Ready-to-Record / Recording
        INDI::PropertyLight  m_stateSP  {5};
        // Index detection lights
        INDI::PropertyLight  m_indexSP  {2};
        // Control switches: Play / Stop / Ready-Record / Clear / Save-EEPROM
        INDI::PropertySwitch m_controlSP {3};
        INDI::PropertySwitch m_dataSP    {2};

        // Send the action whose element is on, then reset (momentary).
        void runAction(INDI::PropertySwitch &sp, ISState *states, char *names[], int n);
        // Worm period (read-only)
        INDI::PropertyNumber m_wormNP    {1};

        const char* getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
