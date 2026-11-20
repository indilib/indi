/*
    OnStepX INDI Driver — Tracking control helper (mount binary only)
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
    Advanced tracking properties beyond the standard SetTrackMode/SetTrackRate.

    Protocol (OnStepX v10.24c):
      :To#         — tracking compensation: full (both axes)
      :Tr#         — tracking compensation: refraction only
      :Tn#         — tracking compensation: off
      :T1#         — compensation axes: single
      :T2#         — compensation axes: dual
      :T-#         — frequency nudge: decrease (blind)
      :T+#         — frequency nudge: increase (blind)
      :TR#         — frequency nudge: reset (blind)
      :SX95,0/1#   — meridian auto-flip: 0=off, 1=on (reply '1')
      :GX95#       — read auto-flip setting ('0' / '1')
      :SX96,W/E/B/A# — preferred pier side (reply '1')
      :GX96#       — read preferred pier side ('W' / 'E' / 'B' / 'A')
      :GX97#       — current goto rate, deg/s
      :GX92#       — current goto rate, us/step
      :SX92,n#     — set goto rate in us/step (reply '1'; firmware clamps to
                     0.5x..2x base, refused while slewing/guiding)
      trackComp state also derived from :GU# status string each poll.

    INDI Properties (Tracking tab):
      OSX_TRACK_COMP       IP_RW  ISR_1OFMANY  3 switches: Full / Refraction / Off
      OSX_TRACK_COMP_AXES  IP_RW  ISR_1OFMANY  2 switches: Single / Dual
      OSX_FREQ_ADJ         IP_RW  ISR_ATMOST1  3 switches: Down / Up / Reset (momentary)
      OSX_TRACK_FREQ       IP_RO  Number[1]    Current tracking frequency Hz (:GT#)
    Limits tab (with the meridian limits they act on; flipping mounts only):
      OSX_AUTO_FLIP        IP_RW  ISR_1OFMANY  2 switches: Off / On
      OSX_PREFERRED_PIER   IP_RW  ISR_1OFMANY  4 switches: West / East / Best / Auto
    Motion Control tab:
      OSX_SLEW_RATE_MAX    IP_RW  Number[1]    Max slew rate in deg/sec (:GX97# / :SX92,n#)
*/

#pragma once

#include "OnStepXStatus.h"

#include <defaultdevice.h>

#include <cmath>

class OnStepXComm;

class OnStepXTracking
{
    public:
        // INDI sidereal rate (arcsec per SI second) and SI seconds per
        // sidereal second.
        static constexpr double SIDEREAL_ARCSEC_PER_SEC = 15.041067;
        static constexpr double SI_PER_SIDEREAL_SEC     = 0.99726957;

        // OnStepX models a custom rate as offsets from the base rate
        // (:SXTR,n# / :SXTD,n#, arcsec per sidereal second; RA +ve = the mount
        // tracks slower than sidereal).  INDI's TELESCOPE_TRACK_RATE is
        // absolute, arcsec per SI second.
        static double raOffsetFromRate(double raRate)
        {
            return (SIDEREAL_ARCSEC_PER_SEC - raRate) * SI_PER_SIDEREAL_SEC;
        }
        static double deOffsetFromRate(double deRate)
        {
            return deRate * SI_PER_SIDEREAL_SEC;
        }
        static double raRateFromOffset(double raOffset)
        {
            return SIDEREAL_ARCSEC_PER_SEC - raOffset / SI_PER_SIDEREAL_SEC;
        }
        static double deRateFromOffset(double deOffset)
        {
            return deOffset / SI_PER_SIDEREAL_SEC;
        }

        // Base tracking rate from :GT# (Hz while tracking): 57.900 lunar,
        // 60.000 solar, 60.136 King, 60.164 sidereal (Mount.command.cpp).
        enum class BaseRate { SIDEREAL, LUNAR, SOLAR, KING, UNKNOWN };
        static BaseRate baseRateFromHz(double hz)
        {
            const struct
            {
                double hz;
                BaseRate rate;
            } rates[] = { { 57.900, BaseRate::LUNAR }, { 60.000, BaseRate::SOLAR }, { 60.136, BaseRate::KING },
                { 60.164, BaseRate::SIDEREAL }
            };
            for (const auto &r : rates)
                if (std::fabs(hz - r.hz) < 0.005)
                    return r.rate;
            return BaseRate::UNKNOWN;
        }
        static BaseRate baseRateFromStatus(MountStatus::TrackRate rate)
        {
            switch (rate)
            {
                case MountStatus::TrackRate::LUNAR:
                    return BaseRate::LUNAR;
                case MountStatus::TrackRate::SOLAR:
                    return BaseRate::SOLAR;
                case MountStatus::TrackRate::KING:
                    return BaseRate::KING;
                default:
                    return BaseRate::SIDEREAL;
            }
        }
        void setDevice(INDI::DefaultDevice *dev)
        {
            m_dev = dev;
        }
        void setComm(OnStepXComm *comm)
        {
            m_comm = comm;
        }

        void initProperties();
        // hasMeridianFlips: define auto flip and the pier preference.
        void updateProperties(bool connected, bool hasMeridianFlips = true);

        // Returns true if the event was consumed.
        bool handleSwitch(const char *name, ISState *states, char *names[], int n);
        bool handleNumber(const char *name, double values[], char *names[], int n);

        // Property accessors (used by tests and by OnStepXMount for state sync)
        INDI::PropertyNumber &slewRateMaxNP()
        {
            return m_slewRateMaxNP;
        }
        INDI::PropertyNumber &trackFreqNP()
        {
            return m_trackFreqNP;
        }
        INDI::PropertySwitch &trackCompSP()
        {
            return m_trackCompSP;
        }

        void saveConfig(FILE *fp);

        // Sync displayed state from a freshly polled MountStatus (trackComp field).
        // Also polls :GT# to update OSX_TRACK_FREQ.
        void syncStatus(const MountStatus &s);

        // Query :GX95# and :GX96# to initialise autoFlip and preferredPier
        // after connecting (called once from updateProperties when connected).
        void readSettings();

    private:
        INDI::DefaultDevice *m_dev  { nullptr };
        OnStepXComm         *m_comm { nullptr };

        // Tracking compensation mode: Full / Refraction / Off
        INDI::PropertySwitch m_trackCompSP    {3};
        // Compensation axis count: Single / Dual
        INDI::PropertySwitch m_trackAxisSP    {2};
        // Momentary frequency nudge buttons: − / + / Reset
        INDI::PropertySwitch m_freqAdjSP      {3};
        // Meridian auto-flip: Off / On
        INDI::PropertySwitch m_autoFlipSP     {2};
        // Preferred pier side for new gotos: West / East / Best / Auto
        INDI::PropertySwitch m_preferredPierSP{4};
        // Max slew rate (deg/sec)
        INDI::PropertyNumber m_slewRateMaxNP   {1};
        // Current tracking frequency (Hz, IP_RO)
        INDI::PropertyNumber m_trackFreqNP     {1};

        bool m_hasFlips { true };  // from updateProperties(); true until a probe says otherwise
        int m_syncCount { 0 };  // poll counter for throttling :GT# query

        bool readSlewRate(double &degPerSec);
        bool readUsPerStep(double &usPerStep);

        const char *getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
