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
*/

#include "OnStepXMeridian.h"
#include "OnStepXComm.h"

#include <indicom.h>
#include <indilogger.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#define LIMITS_TAB "Limits"

// SI seconds per sidereal second: the hour angle advances at the sidereal rate.
static constexpr double SI_PER_SIDEREAL = 0.99726957;

// ---------------------------------------------------------------------------
// initProperties
// ---------------------------------------------------------------------------
void OnStepXMeridian::initProperties()
{
    const char *dev = m_dev ? m_dev->getDeviceName() : "";

    m_continueSP[0].fill("HOME_CONTINUE", "Continue", ISS_OFF);
    m_continueSP.fill(dev, "OSX_HOME_CONTINUE", "Paused at Home",
                      LIMITS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    m_pauseSP[0].fill("HOME_PAUSE_OFF", "Off", ISS_ON);
    m_pauseSP[1].fill("HOME_PAUSE_ON",  "On",  ISS_OFF);
    m_pauseSP.fill(dev, "OSX_HOME_PAUSE", "Pause at Home",
                   LIMITS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
}

// ---------------------------------------------------------------------------
// updateProperties
// ---------------------------------------------------------------------------
void OnStepXMeridian::updateProperties(bool connected, bool hasMeridianFlips)
{
    if (connected)
    {
        m_hasFlips    = hasMeridianFlips;
        m_noticeArmed = true;
        m_wasWaiting  = false;
        m_pollCount   = 0;
        m_lastNotice.clear();
        if (m_hasFlips && m_dev)
        {
            m_continueSP.setState(IPS_IDLE);
            m_dev->defineProperty(m_pauseSP);
            m_dev->defineProperty(m_continueSP);
        }
    }
    else
    {
        if (m_hasFlips && m_dev)
        {
            m_dev->deleteProperty(m_continueSP);
            m_dev->deleteProperty(m_pauseSP);
        }
        m_hasFlips = false;
    }
}

// ---------------------------------------------------------------------------
// handleSwitch — OSX_HOME_CONTINUE (momentary)
// ---------------------------------------------------------------------------
bool OnStepXMeridian::handleSwitch(const char *name, ISState *states, char *names[], int n)
{
    if (m_pauseSP.isNameMatch(name))
    {
        m_pauseSP.update(states, names, n);
        const bool on = m_pauseSP[1].getState() == ISS_ON;
        // :SX98,n# replies a bare '1' ('0' if the mount does not flip).
        if (m_hasFlips && m_comm->sendBool(on ? ":SX98,1#" : ":SX98,0#"))
            m_pauseSP.setState(IPS_OK);
        else
        {
            LOG_ERROR("Pause at home setting refused");
            m_pauseSP.setState(IPS_ALERT);
        }
        m_pauseSP.apply();
        return true;
    }

    if (!m_continueSP.isNameMatch(name))
        return false;

    m_continueSP.update(states, names, n);
    m_continueSP.reset();

    if (!m_wasWaiting)
    {
        LOG_INFO("No goto is waiting at home.");
        m_continueSP.setState(IPS_IDLE);
    }
    // :SX99,1# replies bare '1' (it is a no-op if nothing is waiting).
    else if (m_comm->sendBool(":SX99,1#"))
    {
        LOG_INFO("Continuing the goto from home.");
        m_continueSP.setState(IPS_BUSY);    // cleared when the status poll stops reporting the wait
    }
    else
    {
        LOG_ERROR("Continue from home failed");
        m_continueSP.setState(IPS_ALERT);
    }
    m_continueSP.apply();
    return true;
}

// ---------------------------------------------------------------------------
// update — every status poll
// ---------------------------------------------------------------------------
void OnStepXMeridian::update(const MountStatus &s, double raHours)
{
    if (!m_hasFlips)
        return;

    updateHomePause(s.waitingAtHome);
    showPauseMode(s.pauseAtHomeEnabled);

    // Meridian limits apply to GEMs only (Limits::poll()).
    if (s.mountType == MountStatus::MountType::GEM)
        checkCrossing(s, raHours);
}

void OnStepXMeridian::updateHomePause(bool waiting)
{
    if (waiting == m_wasWaiting)
        return;
    m_wasWaiting = waiting;

    if (waiting)
        LOG_WARN("Goto paused at home (meridian flip pause-at-home mode). "
                 "Press Continue (Tracking tab) to finish it.");
    m_continueSP.setState(waiting ? IPS_BUSY : IPS_IDLE);
    if (m_dev)
        m_continueSP.apply();
}

// The firmware can change the mode from the hand controller or web UI.
void OnStepXMeridian::showPauseMode(bool on)
{
    const int idx = on ? 1 : 0;
    if (m_pauseSP.findOnSwitchIndex() == idx)
        return;
    m_pauseSP.reset();
    m_pauseSP[idx].setState(ISS_ON);
    m_pauseSP.setState(IPS_OK);
    if (m_dev)
        m_pauseSP.apply();
}

void OnStepXMeridian::saveConfig(FILE *fp)
{
    // Not saved: pause at home is GOTO_SETTINGS.meridianFlipPause in the
    // firmware's NV (:SX98), and a config load would overwrite changes made
    // from the hand controller or web UI.
    (void)fp;
}

void OnStepXMeridian::checkCrossing(const MountStatus &s, double raHours)
{
    if (s.pierSide != MountStatus::PierSide::WEST)
    {
        m_noticeArmed = true;
        return;
    }
    if (s.gotoActive)
    {
        m_noticeArmed = true;   // a new target gets its own notice
        return;
    }
    if (!s.tracking)
        return;
    if (m_pollCount++ % CHECK_EVERY != 0)
        return;

    char reply[OnStepXComm::REPLY_BUF_SIZE];
    double lst = 0;
    if (!m_comm->sendText(":GSH#", reply, 2000, true) || f_scansexa(reply, &lst) < 0)
        return;
    const double ha = hourAngle(lst, raHours);

    if (!m_noticeArmed)
    {
        if (ha < REARM_HA_HOURS)
            m_noticeArmed = true;
        return;
    }
    if (ha < 0)
        return;

    if (!m_comm->sendText(":GXEA#", reply, 2000, true))
        return;     // still armed: try again on the next check
    const int westLimitMin = std::atoi(reply);
    m_noticeArmed = false;

    char pref = '?';
    if (m_comm->sendText(":GX96#", reply, 2000, true))
        pref = reply[0];

    const std::string notice = crossingNotice(ha, westLimitMin, s.autoMeridianFlip, pref);
    if (notice.empty())
        return;
    m_lastNotice = notice;
    LOGF_INFO("%s", notice.c_str());
}

// ---------------------------------------------------------------------------
// flip — INDI Flip() via :MN#
// ---------------------------------------------------------------------------
bool OnStepXMeridian::flip(double ra, double dec, double curRa, double curDec)
{
    if (!m_hasFlips)
    {
        LOG_ERROR("Flip: this mount does not do meridian flips.");
        return false;
    }

    const double offset = separationDeg(ra, dec, curRa, curDec);
    if (offset > FLIP_MAX_OFFSET_DEG)
    {
        LOGF_ERROR("Flip: the target is %.1f deg from where the mount points. The controller flips "
                   "at the current position, so slew to the target first.", offset);
        return false;
    }

    char r = '\0';
    if (!m_comm->sendChar(":MN#", r))
    {
        LOG_ERROR("Flip: no reply to :MN#");
        return false;
    }
    if (r != '0')
    {
        LOGF_ERROR("Flip rejected by firmware: %s (code '%c')", OnStepXStatus::gotoErrorText(r), r);
        return false;
    }

    LOG_INFO("Meridian flip: slewing to the other side of the pier.");
    return true;
}

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------
double OnStepXMeridian::hourAngle(double lstHours, double raHours)
{
    double ha = std::fmod(lstHours - raHours, 24.0);
    if (ha < -12.0)
        ha += 24.0;
    else if (ha >= 12.0)
        ha -= 24.0;
    return ha;
}

double OnStepXMeridian::separationDeg(double ra1Hours, double dec1, double ra2Hours, double dec2)
{
    const double d2r = M_PI / 180.0;
    const double a1 = ra1Hours * 15.0 * d2r, a2 = ra2Hours * 15.0 * d2r;
    const double d1 = dec1 * d2r, d2 = dec2 * d2r;
    // Haversine: well conditioned for the small separations that matter here.
    const double h = std::pow(std::sin((d2 - d1) / 2), 2) +
                     std::cos(d1) * std::cos(d2) * std::pow(std::sin((a2 - a1) / 2), 2);
    return 2.0 * std::asin(std::sqrt(std::fmin(1.0, h))) / d2r;
}

std::string OnStepXMeridian::crossingNotice(double haHours, int westLimitMin, bool autoFlip, char pierPref)
{
    const int pastMin  = static_cast<int>(std::lround(haHours * 60.0 * SI_PER_SIDEREAL));
    const int leftMin  = static_cast<int>(std::lround((westLimitMin / 60.0 - haHours) * 60.0 * SI_PER_SIDEREAL));
    if (leftMin <= 0)
        return "";

    char buf[512];
    snprintf(buf, sizeof(buf),
             "Meridian: tracking west of the pier, %d min past the meridian. The west meridian limit "
             "(%d min) is about %d min away; there the controller will %s.",
             pastMin, westLimitMin, leftMin,
             autoFlip ? "flip the mount automatically" : "stop tracking (auto flip is off)");
    std::string notice = buf;

    switch (pierPref)
    {
        case 'B':
            notice += " Pier preference Best: until then a goto to this target stays west of the pier, "
                      "so a meridian flip made by a goto (as Ekos does by default) will not change sides. "
                      "Use the Flip slew mode, or pier preference East.";
            break;
        case 'W':
            notice += " Pier preference West: until then a goto to this target stays west of the pier.";
            break;
        case 'E':
            notice += " Pier preference East: a goto to this target now moves to the east side.";
            break;
        case 'A':
            notice += " Pier preference Auto: a goto to this target now moves to the east side.";
            break;
        default:
            break;
    }
    return notice;
}
