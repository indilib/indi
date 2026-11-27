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
*/

#include "OnStepXRotator.h"
#include "OnStepXComm.h"

#include <defaultdevice.h>
#include <indicom.h>   // f_scansexa
#include <indilogger.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define ROTATOR_TAB "Main Control"   // on the rotator child device, with the RotatorInterface controls

// ---------------------------------------------------------------------------
// initProperties
// ---------------------------------------------------------------------------
void OnStepXRotator::initProperties(bool hasDerotator)
{
    const char *dev = m_dev ? m_dev->getDeviceName() : "";

    m_derotateSP[0].fill("DEROTATE_OFF", "Off", ISS_ON);
    m_derotateSP[1].fill("DEROTATE_ON",  "On",  ISS_OFF);
    m_derotateSP.fill(dev, "OSX_ROT_DEROTATE",
                      "De-rotator", ROTATOR_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    m_parallacticSP[0].fill("PARALLACTIC_GOTO", "Go to Parallactic Angle", ISS_OFF);
    m_parallacticSP.fill(dev, "OSX_ROT_PARALLACTIC",
                         "Parallactic", ROTATOR_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    m_reverseSP[0].fill("DEROTATE_NORMAL",   "Normal",   ISS_ON);
    m_reverseSP[1].fill("DEROTATE_REVERSED", "Reversed", ISS_OFF);
    m_reverseSP.fill(dev, "OSX_ROT_DEROTATE_REVERSE",
                     "De-rotator Dir.", ROTATOR_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    INDI_UNUSED(hasDerotator);  // known only after Handshake
}

// ---------------------------------------------------------------------------
// updateProperties
// ---------------------------------------------------------------------------
void OnStepXRotator::updateProperties(bool connected, bool hasDerotator)
{
    m_hasDerotator = connected && hasDerotator;

    if (connected)
    {
        if (hasDerotator)
        {
            m_dev->defineProperty(m_derotateSP);
            m_dev->defineProperty(m_parallacticSP);
            m_dev->defineProperty(m_reverseSP);
            bool reversed = false;
            if (readReversed(reversed))
                showReversed(reversed);
        }
    }
    else
    {
        m_dev->deleteProperty(m_derotateSP);
        m_dev->deleteProperty(m_parallacticSP);
        m_dev->deleteProperty(m_reverseSP);
    }
}

// ---------------------------------------------------------------------------
// handleSwitch
// ---------------------------------------------------------------------------
bool OnStepXRotator::handleSwitch(const char *name, ISState *states, char *names[], int n)
{
    // De-rotator enable/disable — :r+# / :r-# reply nothing.  :r+# is
    // silently refused while parked; the next poll shows the real state.
    if (m_derotateSP.isNameMatch(name))
    {
        m_derotateSP.update(states, names, n);
        const char *cmd = (m_derotateSP[1].getState() == ISS_ON) ? ":r+#" : ":r-#";
        m_derotateSP.setState(m_comm->sendNoReply(cmd) ? IPS_OK : IPS_ALERT);
        m_derotateSP.apply();
        return true;
    }

    // De-rotator direction.  :rR# toggles, so read the state first; :rT#
    // shows it only while the rotator is stopped.
    if (m_reverseSP.isNameMatch(name))
    {
        bool want = false;
        for (int i = 0; i < n; i++)
            if (states[i] == ISS_ON && strcmp(names[i], "DEROTATE_REVERSED") == 0)
                want = true;
        bool reversed = false;
        if (!readReversed(reversed))
        {
            LOG_ERROR("De-rotator direction can be changed only while the rotator is stopped.");
            m_reverseSP.setState(IPS_ALERT);
            m_reverseSP.apply();
            return true;
        }
        if (reversed != want)
        {
            m_comm->sendNoReply(":rR#");
            readReversed(reversed);
        }
        showReversed(reversed);
        if (reversed != want)
        {
            LOG_ERROR("De-rotator direction did not change.");
            m_reverseSP.setState(IPS_ALERT);
            m_reverseSP.apply();
        }
        return true;
    }

    // Momentary: rotate to the parallactic angle
    if (m_parallacticSP.isNameMatch(name))
    {
        const bool ok = m_comm->sendNoReply(":rP#");
        m_moveStarted = ok;
        m_parallacticSP.reset();
        m_parallacticSP.setState(ok ? IPS_OK : IPS_ALERT);
        m_parallacticSP.apply();
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// handleNumber — nothing custom yet
// ---------------------------------------------------------------------------
bool OnStepXRotator::handleNumber(const char * /*name*/, double * /*values*/,
                                  char * /*names*/[], int /*n*/)
{
    return false;
}

// ---------------------------------------------------------------------------
// saveConfig — nothing saved: de-rotation is a live state, parallactic is
// a momentary action.
// ---------------------------------------------------------------------------
void OnStepXRotator::saveConfig(FILE *fp)
{
    (void)fp;
}

// ---------------------------------------------------------------------------
// RotatorInterface virtual implementations
// ---------------------------------------------------------------------------
IPState OnStepXRotator::moveToAngle(double angle)
{
    double target = 0;
    if (!mapToRange(angle, m_limitMin, m_limitMax, target))
    {
        LOGF_ERROR("MoveRotator: %.2f deg is outside the rotator range %.0f..%.0f deg",
                   angle, m_limitMin, m_limitMax);
        return IPS_ALERT;
    }

    char cmd[OnStepXComm::CMD_MAX_LEN];
    if (!formatGoto(target, cmd, sizeof(cmd)))
        return IPS_ALERT;

    if (!m_comm->sendBool(cmd))
    {
        LOGF_ERROR("MoveRotator: command '%s' rejected", cmd);
        return IPS_ALERT;
    }

    return IPS_BUSY;
}

bool OnStepXRotator::abortRotator()
{
    return m_comm->sendNoReply(":rQ#");
}

IPState OnStepXRotator::homeRotator()
{
    return m_comm->sendNoReply(":rC#") ? IPS_BUSY : IPS_ALERT;
}

// The firmware can only redefine the current position as 0 deg (:rZ#) or as
// half travel (:rF#).  Both reply nothing, so the angle is read back.
bool OnStepXRotator::syncAngle(double angle)
{
    double target = 0;
    const double half = (m_limitMin + m_limitMax) / 2.0;
    const char *cmd = nullptr;
    if (mapToRange(angle, m_limitMin, m_limitMax, target))
    {
        if (std::fabs(target) < 0.01)
            cmd = ":rZ#";
        else if (std::fabs(target - half) < 0.01)
            cmd = ":rF#";
    }
    if (!cmd)
    {
        double halfIndi = std::fmod(half, 360.0);
        if (halfIndi < 0)
            halfIndi += 360.0;
        LOGF_ERROR("OnStepX can sync the rotator only to 0 deg or to half travel (%.1f deg).", halfIndi);
        return false;
    }
    if (!m_comm->sendNoReply(cmd))
        return false;

    char reply[OnStepXComm::REPLY_BUF_SIZE];
    double now = 0, want = std::fmod(target, 360.0);
    if (want < 0)
        want += 360.0;
    if (!m_comm->sendText(":rG#", reply) || !parseAngle(reply, now) ||
            std::fabs(std::remainder(now - want, 360.0)) > 0.1)
    {
        LOG_ERROR("Rotator sync was not applied.");
        return false;
    }
    return true;
}

bool OnStepXRotator::setBacklash(int32_t steps)
{
    char cmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(cmd, sizeof(cmd), ":rb%d#", steps);
    return m_comm->sendBool(cmd);
}

// ---------------------------------------------------------------------------
// pollStatus — returns angle and motion state for the device class to apply
// ---------------------------------------------------------------------------
OnStepXRotator::PollResult OnStepXRotator::pollStatus()
{
    PollResult result;
    char reply[OnStepXComm::REPLY_BUF_SIZE];

    double angle = 0;
    if (m_comm->sendText(":rG#", reply) && parseAngle(reply, angle))
    {
        result.angle      = angle;
        result.angleValid = true;
    }

    // :rT# is 'M' (moving) or 'S' (stopped, then D/R flags), then the goto
    // rate digit 1-5.  Anything else is a reply that lost a byte ("3" for
    // "M3"), which must not end a move in progress.
    const size_t n = m_comm->sendText(":rT#", reply) ? strlen(reply) : 0;
    if (n >= 2 && (reply[0] == 'M' || reply[0] == 'S') && reply[n - 1] >= '1' && reply[n - 1] <= '5')
    {
        result.moving      = (reply[0] == 'M');
        result.statusValid = true;

        // When stopped, 'D' after the 'S' means de-rotation is enabled
        // and 'R' that it is reversed.
        if (m_hasDerotator && reply[0] == 'S')
        {
            showReversed(strchr(reply + 1, 'R') != nullptr);
            const int on = (strchr(reply + 1, 'D') != nullptr) ? 1 : 0;
            if (m_derotateSP.findOnSwitchIndex() != on)
            {
                m_derotateSP.reset();
                m_derotateSP[on].setState(ISS_ON);
                m_derotateSP.setState(IPS_OK);
                m_derotateSP.apply();
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// readInitial — called once on connect; returns angle and backlash
// ---------------------------------------------------------------------------
OnStepXRotator::InitialState OnStepXRotator::readInitial()
{
    InitialState state;
    char reply[OnStepXComm::REPLY_BUF_SIZE];

    readLimits();

    double angle = 0;
    if (m_comm->sendText(":rG#", reply) && parseAngle(reply, angle))
    {
        state.angle      = angle;
        state.angleValid = true;
    }

    if (m_comm->sendText(":rb#", reply))
    {
        char *end;
        const long bl = std::strtol(reply, &end, 10);
        if (end != reply)
        {
            state.backlash      = static_cast<int32_t>(bl);
            state.backlashValid = true;
        }
    }

    return state;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool OnStepXRotator::readReversed(bool &reversed)
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    if (!m_comm->sendText(":rT#", reply) || reply[0] != 'S')
        return false;
    reversed = strchr(reply + 1, 'R') != nullptr;
    return true;
}

void OnStepXRotator::showReversed(bool reversed)
{
    const int on = reversed ? 1 : 0;
    if (m_reverseSP.findOnSwitchIndex() == on && m_reverseSP.getState() == IPS_OK)
        return;
    m_reverseSP.reset();
    m_reverseSP[on].setState(ISS_ON);
    m_reverseSP.setState(IPS_OK);
    m_reverseSP.apply();
}

void OnStepXRotator::readLimits()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    char *end;

    if (m_comm->sendText(":rI#", reply))
    {
        const double v = std::strtod(reply, &end);
        if (end != reply)
            m_limitMin = v;
    }
    if (m_comm->sendText(":rM#", reply))
    {
        const double v = std::strtod(reply, &end);
        if (end != reply)
            m_limitMax = v;
    }
    LOGF_DEBUG("Rotator travel %.0f..%.0f deg", m_limitMin, m_limitMax);
}

// Parse "sDDD*MM" into degrees, normalised to INDI's 0..360.
bool OnStepXRotator::parseAngle(const char *reply, double &angleDeg)
{
    double val = 0;
    if (f_scansexa(reply, &val) < 0)
        return false;
    val = std::fmod(val, 360.0);
    if (val < 0)
        val += 360.0;
    angleDeg = val;
    return true;
}

bool OnStepXRotator::mapToRange(double angleDeg, double minDeg, double maxDeg, double &out)
{
    for (double candidate :
            {
                angleDeg, angleDeg - 360.0, angleDeg + 360.0
            })
    {
        if (candidate >= minDeg && candidate <= maxDeg)
        {
            out = candidate;
            return true;
        }
    }
    return false;
}

// ":rS" + optional sign + "DDD:MM:SS" (the firmware requires 3 degree digits).
bool OnStepXRotator::formatGoto(double angleDeg, char *buf, int bufLen)
{
    const char sign = angleDeg < 0 ? '-' : '+';
    long secs = std::lround(std::fabs(angleDeg) * 3600.0);
    const int s = static_cast<int>(secs % 60);
    secs /= 60;
    const int m = static_cast<int>(secs % 60);
    const int d = static_cast<int>(secs / 60);
    const int written = snprintf(buf, bufLen, ":rS%c%03d:%02d:%02d#", sign, d, m, s);
    return written > 0 && written < bufLen;
}
