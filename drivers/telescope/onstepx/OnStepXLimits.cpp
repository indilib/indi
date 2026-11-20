/*
    OnStepX INDI Driver — Limits and Home helper (mount binary only)
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

#include "OnStepXLimits.h"
#include "OnStepXComm.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define LIMITS_TAB "Limits"
#define HOME_TAB   "Site Management"   // INDI::Telescope SITE_TAB, with the park settings
#define MOTION_CONTROL_TAB "Motion Control"

// ---------------------------------------------------------------------------
// initProperties
// ---------------------------------------------------------------------------
void OnStepXLimits::initProperties()
{
    // --- Auto-home on boot switch ---
    m_autoBootSP[0].fill("AUTO_HOME_ON",  "Enabled",  ISS_OFF);
    m_autoBootSP[1].fill("AUTO_HOME_OFF", "Disabled", ISS_ON);
    m_autoBootSP.fill(m_dev ? m_dev->getDeviceName() : "",
                      "AUTO_HOME", "Home at Boot", HOME_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // --- Home axis offsets ---
    m_homeOffsetNP[0].fill("HOME_OFFSET_AX1", "Axis 1 (arcsec)", "%.1f", -3600, 3600, 1, 0);
    m_homeOffsetNP[1].fill("HOME_OFFSET_AX2", "Axis 2 (arcsec)", "%.1f", -3600, 3600, 1, 0);
    m_homeOffsetNP.fill(m_dev ? m_dev->getDeviceName() : "",
                        "HOME_OFFSETS", "Home Offsets", HOME_TAB, IP_RW, 0, IPS_IDLE);

    // --- Horizon limits ---
    m_horizonLimitNP[0].fill("HORIZON_MIN", "Minimum Alt (deg)", "%.1f", -30, 30, 0.5, -10);
    m_horizonLimitNP[1].fill("HORIZON_MAX", "Maximum Alt (deg)", "%.1f",  60, 90, 0.5,  89);
    m_horizonLimitNP.fill(m_dev ? m_dev->getDeviceName() : "",
                          "HORIZON_LIMITS", "Horizon Limits", LIMITS_TAB, IP_RW, 0, IPS_IDLE);

    // --- Meridian limits ---
    m_meridianLimitNP[0].fill("MERIDIAN_EAST", "E of Meridian (min)", "%.0f", -600, 600, 1, 0);
    m_meridianLimitNP[1].fill("MERIDIAN_WEST", "W of Meridian (min)", "%.0f", -600, 600, 1, 0);
    m_meridianLimitNP.fill(m_dev ? m_dev->getDeviceName() : "",
                           "MERIDIAN_LIMITS", "Meridian Limits", LIMITS_TAB, IP_RW, 0, IPS_IDLE);

    // --- Mount backlash ---
    m_backlashNP[0].fill("MOUNT_BACKLASH_AXIS1", "Axis1 RA/Az (arcsec)",  "%.0f", 0, 3600, 1, 0);
    m_backlashNP[1].fill("MOUNT_BACKLASH_AXIS2", "Axis2 Dec/Alt (arcsec)", "%.0f", 0, 3600, 1, 0);
    m_backlashNP.fill(m_dev ? m_dev->getDeviceName() : "",
                      "OSX_MOUNT_BACKLASH", "Mount Backlash", MOTION_CONTROL_TAB, IP_RW, 0, IPS_IDLE);
}

// ---------------------------------------------------------------------------
// updateProperties
// ---------------------------------------------------------------------------
void OnStepXLimits::updateProperties(bool connected, bool hasHomeSense, bool isGem)
{
    if (!m_dev) return;

    m_hasHomeSense = connected && hasHomeSense;
    if (connected)
        m_isGem = isGem;   // the firmware applies meridian limits to a GEM only

    if (connected)
    {
        m_dev->defineProperty(m_autoBootSP);
        if (hasHomeSense)
            m_dev->defineProperty(m_homeOffsetNP);
        m_dev->defineProperty(m_horizonLimitNP);
        if (m_isGem)
            m_dev->defineProperty(m_meridianLimitNP);
        m_dev->defineProperty(m_backlashNP);

        // Populate limits from device
        readLimits();
    }
    else
    {
        m_dev->deleteProperty(m_autoBootSP);
        m_dev->deleteProperty(m_homeOffsetNP);
        m_dev->deleteProperty(m_horizonLimitNP);
        m_dev->deleteProperty(m_meridianLimitNP);
        m_dev->deleteProperty(m_backlashNP);
    }
}

// ---------------------------------------------------------------------------
// handleSwitch
// ---------------------------------------------------------------------------
bool OnStepXLimits::handleSwitch(const char *name, ISState *states, char *names[], int n)
{
    if (m_autoBootSP.isNameMatch(name))
    {
        m_autoBootSP.update(states, names, n);
        bool enable = (m_autoBootSP[0].getState() == ISS_ON);
        bool ok = setAutoHome(enable);
        m_autoBootSP.setState(ok ? IPS_OK : IPS_ALERT);
        m_autoBootSP.apply();
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// handleNumber
// ---------------------------------------------------------------------------
bool OnStepXLimits::handleNumber(const char *name, double values[], char *names[], int n)
{
    if (m_homeOffsetNP.isNameMatch(name))
    {
        m_homeOffsetNP.update(values, names, n);
        bool ok = writeHomeOffsets(m_homeOffsetNP[0].getValue(),
                                   m_homeOffsetNP[1].getValue());
        m_homeOffsetNP.setState(ok ? IPS_OK : IPS_ALERT);
        m_homeOffsetNP.apply();
        return true;
    }

    if (m_horizonLimitNP.isNameMatch(name))
    {
        m_horizonLimitNP.update(values, names, n);
        char cmd[OnStepXComm::CMD_MAX_LEN];
        bool ok = true;
        snprintf(cmd, sizeof(cmd), ":Sh%d#", static_cast<int>(m_horizonLimitNP[0].getValue()));
        if (!m_comm->sendBool(cmd)) ok = false;
        snprintf(cmd, sizeof(cmd), ":So%d#", static_cast<int>(m_horizonLimitNP[1].getValue()));
        if (!m_comm->sendBool(cmd)) ok = false;
        m_horizonLimitNP.setState(ok ? IPS_OK : IPS_ALERT);
        m_horizonLimitNP.apply();
        return true;
    }

    if (m_meridianLimitNP.isNameMatch(name))
    {
        if (!m_isGem)
            return true;   // not offered: the firmware ignores them on this mount
        m_meridianLimitNP.update(values, names, n);
        char cmd[OnStepXComm::CMD_MAX_LEN];
        bool ok = true;
        snprintf(cmd, sizeof(cmd), ":SXE9,%d#",
                 static_cast<int>(m_meridianLimitNP[0].getValue()));
        if (!m_comm->sendBool(cmd)) ok = false;
        snprintf(cmd, sizeof(cmd), ":SXEA,%d#",
                 static_cast<int>(m_meridianLimitNP[1].getValue()));
        if (!m_comm->sendBool(cmd)) ok = false;
        m_meridianLimitNP.setState(ok ? IPS_OK : IPS_ALERT);
        m_meridianLimitNP.apply();
        return true;
    }

    if (m_backlashNP.isNameMatch(name))
    {
        m_backlashNP.update(values, names, n);
        char cmd[OnStepXComm::CMD_MAX_LEN];
        bool ok = true;
        snprintf(cmd, sizeof(cmd), ":$BR%d#", static_cast<int>(m_backlashNP[0].getValue()));
        if (!m_comm->sendBool(cmd)) ok = false;
        snprintf(cmd, sizeof(cmd), ":$BD%d#", static_cast<int>(m_backlashNP[1].getValue()));
        if (!m_comm->sendBool(cmd)) ok = false;
        m_backlashNP.setState(ok ? IPS_OK : IPS_ALERT);
        m_backlashNP.apply();
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// saveConfig
// ---------------------------------------------------------------------------
void OnStepXLimits::saveConfig(FILE *fp)
{
    // Nothing is saved: limits, backlash and home settings live in the
    // controller's NV memory and are read back on connect.  Restoring them
    // from the INDI config would overwrite changes made from the hand
    // controller or web UI.
    (void)fp;
}

// ---------------------------------------------------------------------------
// readLimits
// ---------------------------------------------------------------------------
bool OnStepXLimits::readLimits()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    bool ok = true;

    // Horizon minimum (:Gh# returns integer degrees)
    if (m_comm->sendText(":Gh#", reply))
    {
        double v = std::atof(reply);
        m_horizonLimitNP[0].setValue(v);
    }
    else ok = false;

    // Horizon maximum (:Go# returns integer degrees)
    if (m_comm->sendText(":Go#", reply))
    {
        double v = std::atof(reply);
        m_horizonLimitNP[1].setValue(v);
    }
    else ok = false;

    m_horizonLimitNP.setState(ok ? IPS_OK : IPS_ALERT);
    m_horizonLimitNP.apply();

    // Meridian limits (:GXE9# east, :GXEA# west, minutes past the meridian);
    // GEM only.
    if (m_isGem)
    {
        bool ok2 = true;
        if (m_comm->sendText(":GXE9#", reply))
            m_meridianLimitNP[0].setValue(std::atof(reply));
        else ok2 = false;
        if (m_comm->sendText(":GXEA#", reply))
            m_meridianLimitNP[1].setValue(std::atof(reply));
        else ok2 = false;
        m_meridianLimitNP.setState(ok2 ? IPS_OK : IPS_ALERT);
        m_meridianLimitNP.apply();
        ok = ok && ok2;
    }

    // Mount backlash (:%BR# -> Axis1, :%BD# -> Axis2)
    bool ok3 = true;
    if (m_comm->sendText(":%BR#", reply))
        m_backlashNP[0].setValue(std::atof(reply));
    else
        ok3 = false;

    if (m_comm->sendText(":%BD#", reply))
        m_backlashNP[1].setValue(std::atof(reply));
    else
        ok3 = false;

    m_backlashNP.setState(ok3 ? IPS_OK : IPS_ALERT);
    m_backlashNP.apply();

    // Home sensor offsets (:h?# -> "hasSense,offset1,offset2", arcseconds)
    if (m_hasHomeSense)
    {
        long off1 = 0, off2 = 0;
        int sense = 0;
        if (m_comm->sendText(":h?#", reply) &&
                std::sscanf(reply, "%d,%ld,%ld", &sense, &off1, &off2) == 3)
        {
            m_homeOffsetNP[0].setValue(static_cast<double>(off1));
            m_homeOffsetNP[1].setValue(static_cast<double>(off2));
            m_homeOffsetNP.setState(IPS_OK);
        }
        else
            m_homeOffsetNP.setState(IPS_ALERT);
        m_homeOffsetNP.apply();
    }

    return ok && ok3;
}

// ---------------------------------------------------------------------------
// homeFind
// ---------------------------------------------------------------------------
bool OnStepXLimits::homeFind()
{
    // :hC# replies nothing (even on failure); homing runs asynchronously and
    // its progress is reported by the 'h'/'H' status flags.
    if (!m_comm->sendNoReply(":hC#"))
    {
        if (m_dev)
            LOG_ERROR("Home Find (:hC#) failed");
        return false;
    }
    if (m_dev)
        LOG_DEBUG("Homing started -- watch status for 'h' flag");
    return true;
}

// ---------------------------------------------------------------------------
// homeSet
// ---------------------------------------------------------------------------
bool OnStepXLimits::homeSet()
{
    // :hF# replies nothing (even on failure).
    if (!m_comm->sendNoReply(":hF#"))
    {
        if (m_dev)
            LOG_ERROR("Home Set (:hF#) failed");
        return false;
    }
    if (m_dev)
        LOG_DEBUG("Home position set to current position");
    return true;
}

// ---------------------------------------------------------------------------
// setAutoHome
// ---------------------------------------------------------------------------
bool OnStepXLimits::setAutoHome(bool enabled)
{
    // :hA0#/:hA1# reply nothing.
    return m_comm->sendNoReply(enabled ? ":hA1#" : ":hA0#");
}

// ---------------------------------------------------------------------------
// syncAutoHome
// ---------------------------------------------------------------------------
void OnStepXLimits::syncAutoHome(bool enabled)
{
    if ((m_autoBootSP[0].getState() == ISS_ON) == enabled)
        return;

    m_autoBootSP[0].setState(enabled ? ISS_ON : ISS_OFF);
    m_autoBootSP[1].setState(enabled ? ISS_OFF : ISS_ON);
    m_autoBootSP.setState(IPS_OK);
    m_autoBootSP.apply();
}

// ---------------------------------------------------------------------------
// writeHomeOffsets
// ---------------------------------------------------------------------------
bool OnStepXLimits::writeHomeOffsets(double axis1, double axis2)
{
    char cmd[OnStepXComm::CMD_MAX_LEN];

    // :hC1,n# / :hC2,n# (arcseconds) reply nothing.
    snprintf(cmd, sizeof(cmd), ":hC1,%d#", static_cast<int>(std::lround(axis1)));
    bool ok = m_comm->sendNoReply(cmd);

    snprintf(cmd, sizeof(cmd), ":hC2,%d#", static_cast<int>(std::lround(axis2)));
    ok = m_comm->sendNoReply(cmd) && ok;

    return ok;
}
