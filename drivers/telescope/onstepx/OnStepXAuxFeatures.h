/*
    OnStepX INDI Driver — Auxiliary feature helper (shared by both binaries)
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Handles dynamic discovery and control of OnStepX aux feature slots 1-8.
    Slot count and types are probed after connect.  INDI::OutputInterface was
    not used because it requires the output count at initProperties() time,
    before the firmware has been queried.

    Protocol (OnStepX v10.24c):
      :GXY0#        — 8-char mask ('1'=configured, '0'=not), positions 0-7 = slots 1-8
      :GXY[n]#      — slot info: "name,T", T = firmware purpose: 1 SWITCH (momentary
                      and cover switches also report 1), 2 ANALOG, 3 DEW, 4 IVO,
                      6 HIDDEN_SWITCH.  A hidden switch only sets a pin's state at
                      boot: :GXX# answers it with a bare '0', so it is skipped.
      :GXX[n]#      — values, '#'-terminated, optionally followed by a
                      power-monitor suffix ";volts,amps,flags":
                        SWITCH          "v"                 (0/1)
                        ANALOG          "v"                 (0-255)
                        DEW_HEATER      "en,zero,span,dT"   (dT = temp above dew point, may be NAN)
                        INTERVALOMETER  "current,exposure,delay,count"
      :SXX[n],V[v]# — SWITCH 0/1, ANALOG 0-255, DEW enable 0/1, IVO start 0/1
      :SXX[n],Z[f]# — DEW zero point, deg C (-5..20)
      :SXX[n],S[f]# — DEW span, deg C (-5..20)
      :SXX[n],E[f]# — IVO exposure, seconds (0..3600)
      :SXX[n],D[f]# — IVO delay between frames, seconds (1..3600)
      :SXX[n],C[n]# — IVO frame count (0..255)
      All :SXX# commands reply a bare '1'/'0'.

    INDI Properties (on OnStepXPowerDevice, which owns this helper):
      SWITCH slots            -> PowerInterface DC outputs (external mode, see
                                 setExternalSwitches()); otherwise OSX_[label]_SW
      ANALOG slots            -> "Power" tab        (OSX_[label]_AN)
      DEW_HEATER slots        -> "Dew Heaters" tab  (OSX_DEW_[label])
      INTERVALOMETER slots    -> "Intervalometer" tab (OSX_IVO_[label])
*/

#pragma once

#include <defaultdevice.h>
#include <array>
#include <vector>
#include <cstdint>
#include <cstdio>

class OnStepXComm;

class OnStepXAuxFeatures
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

        // Called from updateProperties(true): probe each slot and defineProperty
        // (probe() then defineAll()).
        void discoverAndDefine(uint8_t featureMask);

        // Read each slot's name and purpose (:GXY[n]#); defines nothing.
        void probe(uint8_t featureMask);

        // Define the properties of every probed slot.
        void defineAll();

        // Switch slots handled by the owner instead (OnStepXPowerDevice maps
        // them to PowerInterface DC outputs): no property is defined, handled
        // or saved for them here; pollStatus() still reads their value.
        void setExternalSwitches(bool external)
        {
            m_externalSwitches = external;
        }

        // Switch slots in slot order (1-8), their firmware names and values.
        std::vector<int> switchSlots() const;
        const char *slotLabel(int slotIndex) const;
        bool writeSwitch(int slotIndex, bool on);
        // Last polled value: 1 on, 0 off, -1 not read yet.
        int switchValue(int slotIndex) const;

        // Called from updateProperties(false): deleteProperty for all active slots
        void deleteAll();

        // Called from ISNewSwitch / ISNewNumber
        bool handleSwitch(const char *name, ISState *states, char *names[], int n);
        bool handleNumber(const char *name, double *values, char *names[], int n);

        // Periodic poll — call every ~5 polls from TimerHit / ReadScopeStatus
        void pollStatus();

        void saveConfig(FILE *fp);

    protected:
        enum class FeatureType { SWITCH, ANALOG, DEW_HEATER, INTERVALOMETER, UNKNOWN };

        static void makePropBase(const char *label, int slotIdx, char *out, int outLen);
        static FeatureType parseType(char t);

    private:

        struct Slot
        {
            int         index    = 0;             // 1-8
            char        label[32] = {};           // human name from :GXY[n]#
            char        propBase[48] = {};        // sanitized INDI name root
            FeatureType type     = FeatureType::UNKNOWN;
            bool        active   = false;
            bool        hidden   = false;         // purpose 6 (HIDDEN_SWITCH): skipped
            int         value    = -1;            // SWITCH: last polled value

            // SWITCH: single On/Off switch
            INDI::PropertySwitch switchSP   {2};

            // ANALOG: 0-255 value
            INDI::PropertyNumber analogNP   {1};

            // DEW_HEATER: enable switch, zero/span settings, read-only dT
            INDI::PropertySwitch dewEnSP     {2};
            INDI::PropertyNumber dewNP       {2};  // [0]=zero [1]=span (deg C)
            INDI::PropertyNumber dewStatusNP {1};  // [0]=temp above dew point (deg C)

            // INTERVALOMETER: start/stop switch, settings, read-only progress
            INDI::PropertySwitch ivoEnSP     {2};
            INDI::PropertyNumber ivoNP       {3};  // [0]=exposure s [1]=delay s [2]=count
            INDI::PropertyNumber ivoStatusNP {1};  // [0]=frames taken
        };

        std::array<Slot, 8> m_slots;

        INDI::DefaultDevice *m_dev  = nullptr;
        OnStepXComm         *m_comm = nullptr;
        bool                 m_externalSwitches = false;

        bool probeSlot(int idx, Slot &slot);
        void defineSlot(Slot &slot);
        void deleteSlot(Slot &slot);

        bool sendWrite(int slotIdx, char field, double value);

        const char *getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
