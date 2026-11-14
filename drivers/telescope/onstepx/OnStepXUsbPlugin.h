/*
    OnStepX INDI Driver — USB Plugin helper (shared by both binaries)
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Handles dynamic discovery and control of OnStepX aux feature slots 1-8.
    Slot count and types are probed after connect.  INDI::OutputInterface was
    not used because it requires the output count at initProperties() time,
    before the firmware has been queried.

    Protocol (USB Plugin):
      :GUY0#        — 8-char mask ('1'=configured, '0'=not), positions 0-7 = slots 1-8
      :GUY[n]#      — port_name: "name"
      :GUX[n]#      — current value (0=OFF/1=ON integer, '#'-terminated)
      :SUX[n],V[v]# — set value (SWITCH 0=OFF/1=ON)
      All :SUX# commands reply '1' on success.

    INDI Properties (on OnStepXPowerDevice, which owns this helper):
      Ports  -> PowerInterface USB ports (external mode, see setExternal());
                otherwise OSX_[label]_SW on the "USB Ports" tab
*/

#pragma once

#include <defaultdevice.h>
#include <array>
#include <vector>
#include <cstdint>
#include <cstdio>

class OnStepXComm;

class OnStepXUsbPlugin
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
        void discoverAndDefine(uint8_t portsMask);

        // Read each port's name (:GUY[n]#); defines nothing.
        void probe(uint8_t portsMask);
        void defineAll();

        // Ports handled by the owner instead (OnStepXPowerDevice maps them to
        // PowerInterface USB ports): no property is defined, handled or saved
        // here; pollStatus() still reads their value.
        void setExternal(bool external)
        {
            m_external = external;
        }

        // Probed ports in order (1-8), their names and values.
        std::vector<int> portIndices() const;
        const char *portLabel(int portIndex) const;
        bool writePort(int portIndex, bool on);
        // Last polled value: 1 on, 0 off, -1 not read yet.
        int portValue(int portIndex) const;

        // Called from updateProperties(false): deleteProperty for all active slots
        void deleteAll();

        // Called from ISNewSwitch
        bool handleSwitch(const char *name, ISState *states, char *names[], int n);

        // Periodic poll — call every ~5 polls from TimerHit / ReadScopeStatus
        void pollStatus();

        void saveConfig(FILE *fp);

    protected:

        static void makePropBase(const char *label, int slotIdx, char *out, int outLen);

    private:

        struct Port
        {
            int         index    = 0;             // 1-8
            char        label[32] = {};           // human name from :GUY[n]#
            char        propBase[48] = {};        // sanitized INDI name root
            bool        active   = false;
            int         value    = -1;            // last polled value

            // SWITCH: single On/Off switch
            INDI::PropertySwitch switchSP   {2};
        };

        std::array<Port, 8> m_ports;

        INDI::DefaultDevice *m_dev  = nullptr;
        OnStepXComm         *m_comm = nullptr;
        bool                 m_external = false;

        bool probePort(int idx, Port &port);
        void definePort(Port &port);
        void deletePort(Port &port);

        bool sendWriteInt(int portIdx, char field, int value);

        const char* getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
