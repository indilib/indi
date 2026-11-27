/*
    OnStepX INDI Driver — Motor driver status helper (both binaries)
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

    Plain C++ helper -- no INDI base class.  Non-owning borrowed pointers.
    Shows the status the firmware reads back from each axis's motor driver:
    TMC stepper drivers over SPI or UART, a driver fault pin, or a servo
    driver, whichever the axis's DRIVER_STATUS setting enables.

    Protocol (OnStepX v10.24c, lib/axis/Axis.command.cpp):
      :GXU[n]#  driver status for axis n (1-9): eight comma-separated fields,
                each a code or empty, in this order:
                  ST standstill, OA / OB open load on output A / B,
                  GA / GB short to ground on output A / B,
                  OT over temperature (>150 C), PW pre-warning (>120 C),
                  GF fault
                e.g. "ST,,,,,,,#".  A bare '0' when the axis has no status.
      Axes 1-2 are the mount, 3 the rotator, 4-9 focusers 1-6.

    Which axes report is probed at connect (Capabilities::driverStatusMask).
    A short to ground, over temperature or fault is logged as an error, a
    temperature pre-warning as a warning, and their clearing as info.  Open
    load is shown but not logged: an idle or unplugged motor can report it.

    INDI Properties ("OnStepX" tab, only when at least one axis reports):
      OSX_DRIVER_STATUS  IP_RO  Text[one per reporting axis]  "OK", or the
                         conditions reported; Alert while any axis has a fault.
*/

#pragma once

#include <defaultdevice.h>

#include <array>
#include <cstdint>
#include <string>

class OnStepXComm;

class OnStepXDriverStatus
{
    public:
        struct Flags
        {
            bool standstill = false;
            bool openA      = false;
            bool openB      = false;
            bool shortA     = false;
            bool shortB     = false;
            bool overTemp   = false;
            bool preWarning = false;
            bool fault      = false;

            // Conditions logged as errors.
            bool serious() const
            {
                return shortA || shortB || overTemp || fault;
            }
            bool operator==(const Flags &o) const
            {
                return standstill == o.standstill && openA == o.openA && openB == o.openB &&
                       shortA == o.shortA && shortB == o.shortB && overTemp == o.overTemp &&
                       preWarning == o.preWarning && fault == o.fault;
            }
        };

        void setDevice(INDI::DefaultDevice *dev)
        {
            m_dev = dev;
        }
        void setComm(OnStepXComm *comm)
        {
            m_comm = comm;
        }

        // On connect: define OSX_DRIVER_STATUS for the axes in mask (bit n-1 =
        // axis n), then read them once.  Nothing is defined for an empty mask.
        void discoverAndDefine(uint16_t mask);
        void deleteAll();

        // Re-read every reporting axis; log changes.
        void poll();

        INDI::PropertyText &statusTP()
        {
            return m_statusTP;
        }

        // --- Pure helpers (unit-tested) ---
        static bool parse(const char *reply, Flags &out);
        static std::string describe(const Flags &f);      // "OK" or "standstill, open load A"
        static std::string problems(const Flags &f);      // logged conditions only
        static const char *axisName(int axis);            // "Axis 1 (RA/Azm)"

    private:
        INDI::DefaultDevice *m_dev  { nullptr };
        OnStepXComm         *m_comm { nullptr };

        INDI::PropertyText   m_statusTP { 0 };
        bool                 m_defined  { false };

        struct Axis
        {
            int   number = 0;
            Flags flags;
            bool  known  = false;    // flags read at least once
        };
        std::array<Axis, 9> m_axes {};
        int                 m_count { 0 };

        void report(Axis &a, const Flags &now);

        const char *getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
