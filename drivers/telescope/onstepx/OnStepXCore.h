/*
    OnStepX INDI Driver — Handshake and capability probing (shared by both binaries)
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

    Owns the shared OnStepXComm instance and the Capabilities struct.
    probeController() runs for both binaries; probeMount() is mount-only.

    Probe commands (all read-only; verified against OnStepX 10.28):
      Phase 1 (both binaries)
        :GVP#   product name, must be "On-Step"           (text)
        :GVN#   firmware version "M.mmp", major >= 10     (text)
        :GVD# :GVT# :GVC#  firmware date/time/config      (text)
        :GX98#  rotator: 'R' rotate, 'D' de-rotate, 'N'   (text; bare '0' if no rotator module)
        :F[n]a# focuser slot n present                    (bool, n = 1..6; slots may be sparse)
        :GXY0#  aux feature mask "01010101"               (text; bare '0' if no features)
        :GUY0#  USB port mask (USB plugin)                (text; bare '0' if no plugin)
        :GX9A#  temperature                               (text)
        :Gv#    site elevation                            (text)
        :GX9F#  MCU temperature                           (text; bare '0' if no sensor)
        :GXU[n]# motor driver status, axes 1-2, 3 if a rotator, 3+n per
                focuser n                                 (text; bare '0' if not reported)
      Phase 2 (mount binary)
        :GW#    mount type G/P/A/L + state                (text; bare '0' if no mount)
        :Gu#    binary status, 9 bytes >= 0x80 + '#'      (binary)
        :GU#    ASCII status; PEC state char present only with AXIS1_PEC on
        :h?#    home sense "s,off1,off2", s = 0/1         (text)
        :Gm#    pier side E/W/N                           (text)
        :GX94#  pier side 0/1/2, " N" appended if the mount
                does not do meridian flips                (text)
*/

#pragma once

#include <defaultdevice.h>
#include "OnStepXComm.h"
#include "OnStepXCapabilities.h"

namespace INDI
{
class DefaultDevice;
}

class OnStepXCore
{
    public:
        OnStepXCore() = default;

        // Called once at startup by the owning INDI device.
        void setDevice(INDI::DefaultDevice *dev);

        // Called from Handshake() after the connection plugin provides a valid fd.
        void setFd(int fd);

        // :GVP# (and :GW#) are tried this many times before the probe gives up.
        static constexpr int GVP_ATTEMPTS = 3;

        // A whole :GW# reply: mount type G/P/A/L, tracking T/N, then P/H/1/0.
        static bool isValidGwReply(const char *reply);

        bool probeController();   // Phase 1 — both binaries
        bool probeMount();        // Phase 2 — mount binary only

        OnStepXComm          &comm();
        const Capabilities   &caps() const;

    private:
        OnStepXComm          m_comm;
        Capabilities         m_cap;
        INDI::DefaultDevice *m_dev { nullptr };

        const char *getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
