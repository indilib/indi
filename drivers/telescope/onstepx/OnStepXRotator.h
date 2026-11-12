/*
    OnStepX INDI Driver — Rotator helper (shared by both binaries)
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

    Plain C++ helper -- not a RotatorInterface subclass.  OnStepXRotatorDevice
    (the rotator child device, an INDI::Rotator) delegates each RI virtual to
    its m_rotator member.  GotoRotatorNP / RotatorBacklashNP are protected in
    RotatorInterface; pollStatus() and readInitial() return plain structs so
    the device class can apply them to its own RI properties.
    De-rotator properties are created only when cap.hasDerotator is true.

    Protocol (verified against OnStepX 10.28 Rotator.command.cpp):
      :rG#          — angle "sDDD*MM"                       (text)
      :rT#          — status "M<rate>" moving, or "S[D][R]<rate>" stopped
                      where D = de-rotator enabled, R = reversed (text)
      :rI# :rM#     — minimum / maximum angle, degrees      (text)
      :rS[s]DDD:MM:SS# — goto angle                         (bare '1'/'0')
      :rC#          — home (or move to mid-travel)          (no reply)
      :rQ#          — stop movement                         (no reply)
      :rb#          — backlash, steps                       (text)
      :rb[n]#       — set backlash                          (bare '1'/'0')
      :r+# :r-#     — de-rotator on (AltAz only) / off      (no reply)
      :rP#          — rotate to the parallactic angle       (no reply)
      :rR#          — toggle the de-rotator direction; the state is the
                      'R' in :rT# (shown only while stopped)  (no reply)
      :rZ# :rF#     — set the current position as 0 deg / as half travel,
                      and unpark                            (no reply)
      :GX98#        — 'D' de-rotator capable, 'R' rotator only, 'N' none

    INDI Properties (Main Control of the rotator device, via RotatorInterface):
      ABS_ROTATOR_ANGLE    IP_RW  1 number: target angle (deg, 0-360)
      ROTATOR_ABORT_MOTION IP_RW  1 switch
      ROTATOR_BACKLASH_STEPS / _TOGGLE  backlash steps, and on/off (off sends 0)
      ROTATOR_HOME         IP_RW  1 switch: home (:rC#)
      SYNC_ROTATOR_ANGLE   IP_RW  only 0 deg (:rZ#) or half travel (:rF#)
      OSX_ROT_DEROTATE     IP_RW  ISR_1OFMANY  2 switches: Off / On  (AltAz only)
      OSX_ROT_PARALLACTIC  IP_RW  ISR_ATMOST1  1 switch: go to parallactic angle (AltAz only)
      OSX_ROT_DEROTATE_REVERSE IP_RW ISR_1OFMANY 2 switches: Normal / Reversed (AltAz only)
*/

#pragma once

#include <defaultdevice.h>

#include <cstdint>
#include <cstdio>

class OnStepXComm;

class OnStepXRotator
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

        // Called from device class initProperties (always)
        void initProperties(bool hasDerotator);

        // Called from device class updateProperties
        void updateProperties(bool connected, bool hasDerotator);

        // Called from device class ISNewSwitch / ISNewNumber
        bool handleSwitch(const char *name, ISState *states, char *names[], int n);
        bool handleNumber(const char *name, double *values, char *names[], int n);

        void saveConfig(FILE *fp);

        // RotatorInterface virtual implementations — device class delegates here
        IPState moveToAngle(double angle);
        bool    abortRotator();
        IPState homeRotator();
        bool    setBacklash(int32_t steps);
        bool    syncAngle(double angle);

        // Data returned by pollStatus() — device class applies to its RI properties
        struct PollResult
        {
            double  angle       { 0 };
            bool    angleValid  { false };
            bool    moving      { false };
            bool    statusValid { false };
        };

        // Data returned by readInitial() — device class applies on connect
        struct InitialState
        {
            double  angle          { 0 };
            bool    angleValid     { false };
            int32_t backlash       { 0 };
            bool    backlashValid  { false };
        };

        // Periodic status poll — call from device's poll throttle (~10 s)
        PollResult    pollStatus();

        // True once after handleSwitch() started a rotator move (e.g. go to
        // parallactic angle), so the device can show its angle property Busy
        // straight away instead of waiting for the next status poll.
        bool takeMoveStarted()
        {
            const bool started = m_moveStarted;
            m_moveStarted = false;
            return started;
        }

        // Map an INDI angle (0..360) into the firmware's travel range by
        // adding or subtracting 360.  Returns false if no equivalent fits.
        static bool mapToRange(double angleDeg, double minDeg, double maxDeg, double &out);

        // Format ":rS[s]DDD:MM:SS#" for a signed angle in degrees.
        static bool formatGoto(double angleDeg, char *buf, int bufLen);

        // Read current angle and backlash once on connect
        InitialState  readInitial();

    private:
        INDI::DefaultDevice *m_dev  { nullptr };
        OnStepXComm         *m_comm { nullptr };

        // De-rotator: enable/disable (AltAz only)
        INDI::PropertySwitch m_derotateSP       {2};
        // Momentary: rotate to the parallactic angle (:rP#)
        INDI::PropertySwitch m_parallacticSP    {1};
        // De-rotator direction: Normal / Reversed (:rR# toggles it)
        INDI::PropertySwitch m_reverseSP        {2};

        // Travel limits from :rI#/:rM# (degrees); defaults until read.
        double m_limitMin { -180.0 };
        double m_limitMax {  180.0 };
        bool   m_hasDerotator { false };
        bool   m_moveStarted  { false };

        bool parseAngle(const char *reply, double &angleDeg);
        bool readReversed(bool &reversed);      // false while moving or on no reply
        void showReversed(bool reversed);
        void readLimits();

        const char *getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
