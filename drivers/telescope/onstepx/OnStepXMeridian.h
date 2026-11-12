/*
    OnStepX INDI Driver — Meridian flip helper (mount binary only)
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
    Only active on mounts that do meridian flips (Capabilities::hasMeridianFlips).
    Auto flip and the pier preference are set in OnStepXTracking.

    How the firmware flips (Goto.cpp, Limits.cpp):
      - A goto picks its pier side from the pier preference.  With Best (the
        default) it stays on the current side while the target is reachable,
        so a "flip" made by re-sending a goto to the same target (as Ekos does
        unless told to use the Flip command) does not change sides until the
        target is past the west meridian limit.
      - While tracking west of the pier, the west meridian limit either starts
        an automatic flip (auto flip on) or stops tracking.
      - A goto that changes sides may pass through home, and with the
        pause-at-home mode it waits there for :SX99,1#.

    Protocol (OnStepX v10.24c):
      :MN#      goto the current position on the other side of the pier
                (one unframed digit like :MS#, '0' = started)
      :SX99,1#  continue a goto paused at home (bare '1')
      :SX98,n#  pause at home on a side-changing goto, 1 on / 0 off (bare
                '1'; on newer firmware 0 means visit home without pausing).
                The state is the pause-at-home flag of the status poll.
      :GSH#     sidereal time, for the hour angle (text, read every
                CHECK_EVERY polls, only while tracking west of the pier)
      :GXEA#    west meridian limit, minutes past the meridian (text)
      :GX96#    pier preference E/W/B/A (text)
      Waiting-at-home comes from the status poll (MountStatus::waitingAtHome).

    Meridian notice: once per crossing (and after each goto), when a GEM
    tracking west of the pier passes the meridian, an info message says when the west limit is reached,
    what the firmware will do there, and whether a goto to the same target
    will change sides with the current pier preference.

    INDI Properties (Limits tab, flipping mounts only):
      OSX_HOME_PAUSE     IP_RW  ISR_1OFMANY  Switch[2]  Pause at home: Off / On
                         (not saved in the INDI config: the firmware keeps it in NV)
      OSX_HOME_CONTINUE  IP_RW  ISR_ATMOST1  Switch[1]  Continue a goto paused
                         at home; Busy while the firmware waits.
*/

#pragma once

#include "OnStepXStatus.h"

#include <defaultdevice.h>

#include <string>

class OnStepXComm;

class OnStepXMeridian
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

        void initProperties();
        void updateProperties(bool connected, bool hasMeridianFlips);
        bool handleSwitch(const char *name, ISState *states, char *names[], int n);
        void saveConfig(FILE *fp);

        // Every status poll.  raHours is the mount's current RA.
        void update(const MountStatus &s, double raHours);

        // INDI Flip(): :MN# from where the mount points.  Refused if (ra, dec)
        // is more than FLIP_MAX_OFFSET_DEG from (curRa, curDec), since the
        // firmware flips at the current position, not at the requested one.
        bool flip(double ra, double dec, double curRa, double curDec);

        // The last meridian notice logged (empty if none since connect).
        const std::string &lastNotice() const
        {
            return m_lastNotice;
        }

        INDI::PropertySwitch &continueSP()
        {
            return m_continueSP;
        }
        INDI::PropertySwitch &pauseSP()
        {
            return m_pauseSP;
        }

        // --- Pure helpers (unit-tested) ---

        // Hour angle in hours, -12 <= ha < 12.
        static double hourAngle(double lstHours, double raHours);

        // Angular separation in degrees.
        static double separationDeg(double ra1Hours, double dec1, double ra2Hours, double dec2);

        // The notice for a mount tracking west of the pier at haHours, or ""
        // if the west limit has already been passed.
        static std::string crossingNotice(double haHours, int westLimitMin, bool autoFlip, char pierPref);

        static constexpr double FLIP_MAX_OFFSET_DEG = 1.0;
        static constexpr int    CHECK_EVERY         = 10;     // polls between :GSH# reads
        static constexpr double REARM_HA_HOURS      = -0.05;  // 3 min east of the meridian

    private:
        INDI::DefaultDevice *m_dev  { nullptr };
        OnStepXComm         *m_comm { nullptr };

        INDI::PropertySwitch m_continueSP { 1 };
        INDI::PropertySwitch m_pauseSP    { 2 };

        bool        m_hasFlips     { false };
        bool        m_noticeArmed  { true };
        bool        m_wasWaiting   { false };
        int         m_pollCount    { 0 };
        std::string m_lastNotice;

        void updateHomePause(bool waiting);
        void showPauseMode(bool on);
        void checkCrossing(const MountStatus &s, double raHours);

        const char *getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
