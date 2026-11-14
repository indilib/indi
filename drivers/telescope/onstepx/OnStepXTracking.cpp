/*
    OnStepX INDI Driver — Tracking control helper
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
*/

#include "OnStepXTracking.h"
#include "OnStepXComm.h"

#include <indicom.h>
#include <indistandardproperty.h>
#include <indilogger.h>

#include <cstdio>
#include <cstdlib>

#define TRACKING_TAB "Tracking"
#define LIMITS_TAB   "Limits"
#define MOTION_CONTROL_TAB "Motion Control"

// ---------------------------------------------------------------------------
// initProperties
// ---------------------------------------------------------------------------
void OnStepXTracking::initProperties()
{
    const char *dev = m_dev ? m_dev->getDeviceName() : "";

    // --- OSX_TRACK_COMP ---
    m_trackCompSP[0].fill("TRACK_COMP_FULL",       "Full Compensation", ISS_OFF);
    m_trackCompSP[1].fill("TRACK_COMP_REFRACTION",  "Refraction Only",   ISS_OFF);
    m_trackCompSP[2].fill("TRACK_COMP_OFF",         "Off",               ISS_ON);
    m_trackCompSP.fill(dev, "OSX_TRACK_COMP",
                       "Track Comp.", TRACKING_TAB, IP_RW, ISR_1OFMANY, 60, IPS_OK);

    // --- OSX_TRACK_COMP_AXES ---
    m_trackAxisSP[0].fill("TRACK_AXIS_SINGLE", "Single Axis", ISS_ON);
    m_trackAxisSP[1].fill("TRACK_AXIS_DUAL",   "Dual Axis",   ISS_OFF);
    m_trackAxisSP.fill(dev, "OSX_TRACK_COMP_AXES",
                       "Comp Axes", TRACKING_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // --- OSX_FREQ_ADJ ---
    m_freqAdjSP[0].fill("FREQ_ADJ_DEC",   "Decrease", ISS_OFF);
    m_freqAdjSP[1].fill("FREQ_ADJ_INC",   "Increase", ISS_OFF);
    m_freqAdjSP[2].fill("FREQ_ADJ_RESET", "Reset",    ISS_OFF);
    m_freqAdjSP.fill(dev, "OSX_FREQ_ADJ",
                     "Freq Adjust", TRACKING_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // --- OSX_AUTO_FLIP ---
    m_autoFlipSP[0].fill("AUTO_FLIP_OFF", "Disabled", ISS_ON);
    m_autoFlipSP[1].fill("AUTO_FLIP_ON",  "Enabled",  ISS_OFF);
    m_autoFlipSP.fill(dev, "OSX_AUTO_FLIP",
                      "Meridian Auto Flip", LIMITS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // --- OSX_PREFERRED_PIER ---
    m_preferredPierSP[0].fill("PIER_WEST", "West", ISS_OFF);
    m_preferredPierSP[1].fill("PIER_EAST", "East", ISS_OFF);
    m_preferredPierSP[2].fill("PIER_BEST", "Best", ISS_ON);
    m_preferredPierSP[3].fill("PIER_AUTO", "Auto", ISS_OFF);
    m_preferredPierSP.fill(dev, "OSX_PREFERRED_PIER",
                           "Preferred Pier Side", LIMITS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // --- OSX_SLEW_RATE_MAX ---
    m_slewRateMaxNP[0].fill("SLEW_RATE_MAX", "Max Rate (deg/s)", "%.1f", 0.1, 90.0, 0.5, 1.0);
    m_slewRateMaxNP.fill(dev, "OSX_SLEW_RATE_MAX", "Max Slew Rate",
                         MOTION_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // --- OSX_TRACK_FREQ ---
    m_trackFreqNP[0].fill("TRACK_FREQ", "Frequency (Hz)", "%.4f", 0.0, 100.0, 0.0001, 0.0);
    m_trackFreqNP.fill(dev, "OSX_TRACK_FREQ", "Track Frequency",
                       TRACKING_TAB, IP_RO, 60, IPS_IDLE);
}

// ---------------------------------------------------------------------------
// updateProperties
// ---------------------------------------------------------------------------
void OnStepXTracking::updateProperties(bool connected, bool hasMeridianFlips)
{
    if (connected)
    {
        // Auto flip and the pier preference only mean something on a mount
        // that flips (:GX94# without " N"); an Alt-Az mount does not.
        m_hasFlips = hasMeridianFlips;
        m_dev->defineProperty(m_trackCompSP);
        m_dev->defineProperty(m_trackAxisSP);
        m_dev->defineProperty(m_freqAdjSP);
        if (m_hasFlips)
        {
            m_dev->defineProperty(m_autoFlipSP);
            m_dev->defineProperty(m_preferredPierSP);
        }
        m_dev->defineProperty(m_slewRateMaxNP);
        m_dev->defineProperty(m_trackFreqNP);
        readSettings();
    }
    else
    {
        m_dev->deleteProperty(m_trackCompSP);
        m_dev->deleteProperty(m_trackAxisSP);
        m_dev->deleteProperty(m_freqAdjSP);
        m_dev->deleteProperty(m_autoFlipSP);
        m_dev->deleteProperty(m_preferredPierSP);
        m_dev->deleteProperty(m_slewRateMaxNP);
        m_dev->deleteProperty(m_trackFreqNP);
    }
}

// ---------------------------------------------------------------------------
// handleSwitch
// ---------------------------------------------------------------------------
bool OnStepXTracking::handleSwitch(const char *name, ISState *states, char *names[], int n)
{
    // --- OSX_TRACK_COMP ---
    if (m_trackCompSP.isNameMatch(name))
    {
        m_trackCompSP.update(states, names, n);
        const char *cmd = nullptr;
        if      (m_trackCompSP[0].getState() == ISS_ON) cmd = ":To#";
        else if (m_trackCompSP[1].getState() == ISS_ON) cmd = ":Tr#";
        else                                             cmd = ":Tn#";

        // :To# :Tr# :Tn# reply bare '1'/'0'
        if (m_comm->sendBool(cmd))
            m_trackCompSP.setState(IPS_OK);
        else
            m_trackCompSP.setState(IPS_ALERT);

        m_trackCompSP.apply();
        return true;
    }

    // --- OSX_TRACK_COMP_AXES ---
    if (m_trackAxisSP.isNameMatch(name))
    {
        m_trackAxisSP.update(states, names, n);
        const char *cmd = (m_trackAxisSP[0].getState() == ISS_ON) ? ":T1#" : ":T2#";

        // :T1# :T2# reply bare '1'/'0'
        if (m_comm->sendBool(cmd))
            m_trackAxisSP.setState(IPS_OK);
        else
            m_trackAxisSP.setState(IPS_ALERT);

        m_trackAxisSP.apply();
        return true;
    }

    // --- OSX_FREQ_ADJ (momentary; :T-# :T+# :TR# reply nothing) ---
    if (m_freqAdjSP.isNameMatch(name))
    {
        m_freqAdjSP.update(states, names, n);
        const char *cmd = nullptr;
        if      (m_freqAdjSP[0].getState() == ISS_ON) cmd = ":T-#";
        else if (m_freqAdjSP[1].getState() == ISS_ON) cmd = ":T+#";
        else if (m_freqAdjSP[2].getState() == ISS_ON) cmd = ":TR#";

        if (cmd)
            m_comm->sendNoReply(cmd);

        m_freqAdjSP.reset();
        m_freqAdjSP.setState(IPS_IDLE);
        m_freqAdjSP.apply();
        return true;
    }

    // --- OSX_AUTO_FLIP ---
    // Not offered on a mount that does not flip; a stale INDI config entry
    // must not send :SX95/:SX96 to it.
    if (!m_hasFlips && (m_autoFlipSP.isNameMatch(name) || m_preferredPierSP.isNameMatch(name)))
        return true;

    if (m_autoFlipSP.isNameMatch(name))
    {
        m_autoFlipSP.update(states, names, n);
        const char *cmd = (m_autoFlipSP[1].getState() == ISS_ON) ? ":SX95,1#" : ":SX95,0#";

        // :SX95,n# replies bare '1'/'0' ('0' on non-equatorial mounts)
        if (m_comm->sendBool(cmd))
            m_autoFlipSP.setState(IPS_OK);
        else
            m_autoFlipSP.setState(IPS_ALERT);

        m_autoFlipSP.apply();
        return true;
    }

    // --- OSX_PREFERRED_PIER ---
    if (m_preferredPierSP.isNameMatch(name))
    {
        m_preferredPierSP.update(states, names, n);
        const char *cmd = nullptr;
        if      (m_preferredPierSP[0].getState() == ISS_ON) cmd = ":SX96,W#";
        else if (m_preferredPierSP[1].getState() == ISS_ON) cmd = ":SX96,E#";
        else if (m_preferredPierSP[3].getState() == ISS_ON) cmd = ":SX96,A#";
        else                                                 cmd = ":SX96,B#";

        // :SX96,c# replies bare '1'/'0'
        if (m_comm->sendBool(cmd))
            m_preferredPierSP.setState(IPS_OK);
        else
            m_preferredPierSP.setState(IPS_ALERT);

        m_preferredPierSP.apply();
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// handleNumber
// ---------------------------------------------------------------------------
bool OnStepXTracking::handleNumber(const char *name, double values[], char *names[], int n)
{
    if (!m_slewRateMaxNP.isNameMatch(name))
        return false;

    m_slewRateMaxNP.update(values, names, n);
    const double requested = m_slewRateMaxNP[0].getValue();

    // OnStepX sets the goto rate as microseconds per step (:SX92,n#, bare
    // '1'/'0'; refused while slewing or guiding).  Rate is inversely
    // proportional to us/step, so scale the current value.  The firmware
    // clamps the result to 0.5x..2x of its base rate.
    bool ok = false;
    double currentDegs = 0, currentUs = 0;
    if (requested > 0 && readSlewRate(currentDegs) && readUsPerStep(currentUs) && currentDegs > 0)
    {
        char cmd[OnStepXComm::CMD_MAX_LEN];
        snprintf(cmd, sizeof(cmd), ":SX92,%.3f#", currentUs * currentDegs / requested);
        ok = m_comm->sendBool(cmd);
        if (!ok)
            LOG_ERROR("Max slew rate change refused (not allowed while slewing or guiding)");
    }

    // Show what the firmware actually applied.
    double actual = 0;
    if (readSlewRate(actual))
        m_slewRateMaxNP[0].setValue(actual);
    m_slewRateMaxNP.setState(ok ? IPS_OK : IPS_ALERT);
    m_slewRateMaxNP.apply();
    return true;
}

// :GX97# — current goto rate, deg/s ("n.n#")
bool OnStepXTracking::readSlewRate(double &degPerSec)
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    if (!m_comm->sendText(":GX97#", reply))
        return false;
    char *end;
    degPerSec = std::strtod(reply, &end);
    return end != reply && degPerSec > 0;
}

// :GX92# — current goto rate, microseconds per step ("n.nnn#")
bool OnStepXTracking::readUsPerStep(double &usPerStep)
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    if (!m_comm->sendText(":GX92#", reply))
        return false;
    char *end;
    usPerStep = std::strtod(reply, &end);
    return end != reply && usPerStep > 0;
}

// ---------------------------------------------------------------------------
// saveConfig
// ---------------------------------------------------------------------------
void OnStepXTracking::saveConfig(FILE *fp)
{
    // Nothing is saved.  Each setting here is in the firmware's NV, and a
    // config load (Ekos sends one on connect) would overwrite changes made
    // from the hand controller or web UI:
    //   compensation and axes  MOUNT_SETTINGS.rc            (:To/:Tr/:Tn, :T1/:T2)
    //   auto flip, pier side   GOTO_SETTINGS                (:SX95, :SX96)
    //   max slew rate          GOTO_SETTINGS.usPerStepCurrent (:SX92)
    (void)fp;
}

// ---------------------------------------------------------------------------
// syncStatus — update displayed state from a freshly polled MountStatus
//
// TrackComp enum encodes both compensation mode and axis count:
//   NONE             → comp=Off,        axis=n/a
//   REFRACTION_SINGLE → comp=Refraction, axis=Single
//   REFRACTION_DUAL   → comp=Refraction, axis=Dual
//   ONTRACK_SINGLE    → comp=Full,       axis=Single
//   ONTRACK_DUAL      → comp=Full,       axis=Dual
// ---------------------------------------------------------------------------
void OnStepXTracking::syncStatus(const MountStatus &s)
{
    // Compensation mode
    int comp = 2;   // Off
    switch (s.trackComp)
    {
        case MountStatus::TrackComp::ONTRACK_SINGLE:
        case MountStatus::TrackComp::ONTRACK_DUAL:
            comp = 0;
            break;
        case MountStatus::TrackComp::REFRACTION_SINGLE:
        case MountStatus::TrackComp::REFRACTION_DUAL:
            comp = 1;
            break;
        default:
            break;
    }
    // Publish only on change: this runs on every status poll.
    if (m_trackCompSP.findOnSwitchIndex() != comp || m_trackCompSP.getState() != IPS_OK)
    {
        m_trackCompSP.reset();
        m_trackCompSP[comp].setState(ISS_ON);
        m_trackCompSP.setState(IPS_OK);
        m_trackCompSP.apply();
    }

    // Axis count (only meaningful when compensation is on)
    const int axes = (s.trackComp == MountStatus::TrackComp::REFRACTION_DUAL ||
                      s.trackComp == MountStatus::TrackComp::ONTRACK_DUAL) ? 1 : 0;
    if (m_trackAxisSP.findOnSwitchIndex() != axes || m_trackAxisSP.getState() != IPS_OK)
    {
        m_trackAxisSP.reset();
        m_trackAxisSP[axes].setState(ISS_ON);
        m_trackAxisSP.setState(IPS_OK);
        m_trackAxisSP.apply();
    }

    // Poll :GT# every 10 calls to update tracking frequency display
    if ((++m_syncCount % 10) == 0)
    {
        char reply[OnStepXComm::REPLY_BUF_SIZE];
        if (m_comm && m_comm->sendText(":GT#", reply))
        {
            char *end;
            double freq = std::strtod(reply, &end);
            if (end != reply && freq > 0.0)
            {
                m_trackFreqNP[0].setValue(freq);
                m_trackFreqNP.setState(IPS_OK);
                m_trackFreqNP.apply();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// readSettings — query :GX95# and :GX96# to initialise autoFlip and
// preferredPier after connect
// ---------------------------------------------------------------------------
void OnStepXTracking::readSettings()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];

    // Auto flip
    if (m_hasFlips && m_comm->sendText(":GX95#", reply))
    {
        m_autoFlipSP.reset();
        if (reply[0] == '1')
            m_autoFlipSP[1].setState(ISS_ON);
        else
            m_autoFlipSP[0].setState(ISS_ON);
        m_autoFlipSP.setState(IPS_OK);
        m_autoFlipSP.apply();
    }

    // Preferred pier side
    if (m_hasFlips && m_comm->sendText(":GX96#", reply))
    {
        m_preferredPierSP.reset();
        switch (reply[0])
        {
            case 'W':
                m_preferredPierSP[0].setState(ISS_ON);
                break;
            case 'E':
                m_preferredPierSP[1].setState(ISS_ON);
                break;
            case 'A':
                m_preferredPierSP[3].setState(ISS_ON);
                break;
            default:
                m_preferredPierSP[2].setState(ISS_ON);
                break; // 'B' or unknown
        }
        m_preferredPierSP.setState(IPS_OK);
        m_preferredPierSP.apply();
    }

    // Max slew rate
    double rate = 0;
    if (readSlewRate(rate))
    {
        m_slewRateMaxNP[0].setValue(rate);
        m_slewRateMaxNP.setState(IPS_OK);
        m_slewRateMaxNP.apply();
    }

    // Tracking frequency
    if (m_comm->sendText(":GT#", reply))
    {
        char *end;
        double freq = std::strtod(reply, &end);
        if (end != reply && freq > 0.0)
        {
            m_trackFreqNP[0].setValue(freq);
            m_trackFreqNP.setState(IPS_OK);
            m_trackFreqNP.apply();
        }
    }
}
