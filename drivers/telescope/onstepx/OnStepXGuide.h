/*
    OnStepX INDI Driver — Pulse guiding and guide rate helper (mount binary only)
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


    Plain C++ helper -- no INDI base class.

    Protocol (verified against OnStepX 10.28 Guide.command.cpp):
      :Mg[n|s|e|w][ms]#  — pulse guide for ms milliseconds (0..32767).
                           Replies nothing.  (:MG... replies a bare '1'/'0'.)
      :GX90#             — pulse-guide rate, x sidereal ("0.50#")
      :R0# :R1# :R2#     — select 0.25x / 0.5x / 1x.  Replies nothing.
                           NOTE: :R[n]# also sets the manual-move rate, so the
                           mount driver re-applies its slew rate afterwards.
                           With GUIDE_SEPARATE_PULSE_RATE ON (the default)
                           only rates <= 1x are stored as the pulse rate.

    INDI Properties (Motion Control tab):
      GUIDE_RATE      IP_RW  Number[2]  INDI standard: GUIDE_RATE_WE / GUIDE_RATE_NS
                              (x sidereal).  The firmware has one rate for both
                              axes; writes are snapped to the nearest of 0.25,
                              0.5 and 1.0.

    Pulse completion is reported to the GuiderInterface from an INDI::Timer
    per axis when the pulse duration has elapsed.
*/

#pragma once

#include <indipropertynumber.h>
#include <indiguiderinterface.h>
#include <timer/inditimer.h>

#include <cstdint>

class OnStepXComm;
namespace INDI
{
class DefaultDevice;
}

class OnStepXGuide
{
    public:
        OnStepXGuide();

        void setDevice(INDI::DefaultDevice *dev)
        {
            m_dev = dev;
        }
        void setComm(OnStepXComm *comm)
        {
            m_comm = comm;
        }
        void setGuiderInterface(INDI::GuiderInterface *gi)
        {
            m_gi = gi;
        }

        void initProperties();
        void updateProperties(bool connected);
        bool handleNumber(const char *name, double values[], char *names[], int n);
        void saveConfig(FILE *fp);

        // GuiderInterface delegations
        IPState guideNorth(uint32_t ms);
        IPState guideSouth(uint32_t ms);
        IPState guideEast(uint32_t ms);
        IPState guideWest(uint32_t ms);

        // Query :GX90# and update GUIDE_RATE
        void readGuideRate();

        // Firmware rate selection for a requested rate (x sidereal):
        // 0 -> 0.25x, 1 -> 0.5x, 2 -> 1x.
        static int rateToIndex(double rate);
        static double indexToRate(int index);

        // Longest pulse the firmware accepts (int16 milliseconds).
        static constexpr uint32_t MAX_PULSE_MS = 32767;

    private:
        INDI::DefaultDevice   *m_dev  { nullptr };
        OnStepXComm           *m_comm { nullptr };
        INDI::GuiderInterface *m_gi   { nullptr };

        // A pulse ended: zero both lengths of its TELESCOPE_TIMED_GUIDE_*
        // property, then GuideComplete().
        void pulseDone(INDI_EQ_AXIS axis);

        INDI::PropertyNumber m_guideRateNP { 2 };

        INDI::Timer m_timerNS;
        INDI::Timer m_timerWE;

        IPState pulse(char dir, uint32_t ms, INDI::Timer &timer);

        const char *getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
