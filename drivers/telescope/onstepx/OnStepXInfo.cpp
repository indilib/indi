/*
    OnStepX INDI Driver — Firmware info, mount status text, reticle helper
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

#include "OnStepXInfo.h"
#include "OnStepXComm.h"

#include <defaultdevice.h>
#include <indilogger.h>

#include <cstdio>

#define ONSTEPX_TAB "OnStepX"

// ---------------------------------------------------------------------------
// initProperties
// ---------------------------------------------------------------------------
void OnStepXInfo::initProperties()
{
    const char *dev = m_dev ? m_dev->getDeviceName() : "";

    // --- OSX_FIRMWARE ---
    m_firmwareTP[0].fill("FIRMWARE_VERSION", "Version", "");
    m_firmwareTP[1].fill("FIRMWARE_DATE",    "Date",    "");
    m_firmwareTP[2].fill("FIRMWARE_TIME",    "Time",    "");
    m_firmwareTP[3].fill("FIRMWARE_CONFIG",  "Config",  "");
    m_firmwareTP.fill(dev, "OSX_FIRMWARE", "Firmware Info",
                      ONSTEPX_TAB, IP_RO, 60, IPS_IDLE);

    // --- OSX_STATUS ---
    m_statusTP[0].fill("STATUS_TRACKING",   "Tracking",   "");
    m_statusTP[1].fill("STATUS_TRACK_RATE", "Rate",       "");
    m_statusTP[2].fill("STATUS_TRACK_COMP", "Comp",       "");
    m_statusTP[3].fill("STATUS_PARK",       "Park",       "");
    m_statusTP[4].fill("STATUS_PIER",       "Pier Side",  "");
    m_statusTP[5].fill("STATUS_PEC",        "PEC",        "");
    m_statusTP[6].fill("STATUS_SLEWING",    "Slewing",    "");
    m_statusTP[7].fill("STATUS_GUIDING",    "Guiding",    "");
    m_statusTP[8].fill("STATUS_AT_HOME",    "At Home",    "");
    m_statusTP[9].fill("STATUS_HOMING",     "Homing",     "");
    m_statusTP[10].fill("STATUS_ERROR",     "Error",      "");
    m_statusTP.fill(dev, "OSX_STATUS", "Mount Status",
                    ONSTEPX_TAB, IP_RO, 60, IPS_IDLE);

    // --- OSX_RETICLE ---
    m_reticleSP[0].fill("RETICLE_PLUS",  "Brighter", ISS_OFF);
    m_reticleSP[1].fill("RETICLE_MINUS", "Dimmer",   ISS_OFF);
    // Momentary buttons (ISR_ATMOST1); ISR_NOFMANY would show as tick boxes.
    m_reticleSP.fill(dev, "OSX_RETICLE", "Reticle",
                     ONSTEPX_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
}

// ---------------------------------------------------------------------------
// updateProperties
// ---------------------------------------------------------------------------
void OnStepXInfo::updateProperties(bool connected, const Capabilities &caps)
{
    m_lastError    = 0;     // report an error already present at connect
    m_pendingError = 0;

    if (connected)
    {
        m_dev->defineProperty(m_firmwareTP);
        m_dev->defineProperty(m_statusTP);

        // Populate firmware fields immediately from probed capabilities
        m_firmwareTP[0].setText(caps.firmwareVersion);
        m_firmwareTP[1].setText(caps.firmwareDate);
        m_firmwareTP[2].setText(caps.firmwareTime);
        m_firmwareTP[3].setText(caps.configName);
        m_firmwareTP.setState(IPS_OK);
        m_firmwareTP.apply();
    }
    else
    {
        m_dev->deleteProperty(m_firmwareTP);
        m_dev->deleteProperty(m_statusTP);
    }
}

// Defined separately so the device can put MCU temperature between the
// status and the reticle control.
void OnStepXInfo::updateReticle(bool connected)
{
    if (connected)
        m_dev->defineProperty(m_reticleSP);
    else
        m_dev->deleteProperty(m_reticleSP);
}

// ---------------------------------------------------------------------------
// handleSwitch — OSX_RETICLE
// ---------------------------------------------------------------------------
bool OnStepXInfo::handleSwitch(const char *name, ISState *states, char *names[], int n)
{
    if (!m_reticleSP.isNameMatch(name))
        return false;

    m_reticleSP.update(states, names, n);

    // :B+# / :B-# reply nothing
    bool ok = false;
    if (m_reticleSP[0].getState() == ISS_ON)
        ok = m_comm->sendNoReply(":B+#");
    else if (m_reticleSP[1].getState() == ISS_ON)
        ok = m_comm->sendNoReply(":B-#");

    // Momentary: reset immediately after sending
    m_reticleSP.reset();
    m_reticleSP.setState(ok ? IPS_OK : IPS_ALERT);
    m_reticleSP.apply();
    return true;
}

// ---------------------------------------------------------------------------
// saveConfig — nothing to persist (all fields are IP_RO or momentary)
// ---------------------------------------------------------------------------
void OnStepXInfo::saveConfig(FILE *fp)
{
    (void)fp;
}

// ---------------------------------------------------------------------------
// updateStatus — populate OSX_STATUS from a freshly polled MountStatus
// ---------------------------------------------------------------------------
void OnStepXInfo::updateStatus(const MountStatus &s)
{
    // Tracking on/off
    m_statusTP[0].setText(s.tracking ? "Tracking" : "Idle");

    // Track rate
    switch (s.trackRate)
    {
        case MountStatus::TrackRate::SIDEREAL:
            m_statusTP[1].setText("Sidereal");
            break;
        case MountStatus::TrackRate::LUNAR:
            m_statusTP[1].setText("Lunar");
            break;
        case MountStatus::TrackRate::SOLAR:
            m_statusTP[1].setText("Solar");
            break;
        case MountStatus::TrackRate::KING:
            m_statusTP[1].setText("King");
            break;
        case MountStatus::TrackRate::CUSTOM:
            m_statusTP[1].setText("Custom");
            break;
        default:
            m_statusTP[1].setText("--");
            break;
    }

    // Track compensation
    switch (s.trackComp)
    {
        case MountStatus::TrackComp::REFRACTION_SINGLE:
        case MountStatus::TrackComp::REFRACTION_DUAL:
            m_statusTP[2].setText("Refraction");
            break;
        case MountStatus::TrackComp::ONTRACK_SINGLE:
        case MountStatus::TrackComp::ONTRACK_DUAL:
            m_statusTP[2].setText("Full");
            break;
        default:
            m_statusTP[2].setText("Off");
            break;
    }

    // Park state
    switch (s.parkState)
    {
        case MountStatus::ParkState::PARKED:
            m_statusTP[3].setText("Parked");
            break;
        case MountStatus::ParkState::PARKING:
            m_statusTP[3].setText("Parking");
            break;
        case MountStatus::ParkState::FAILED:
            m_statusTP[3].setText("Failed");
            break;
        default:
            m_statusTP[3].setText("Unparked");
            break;
    }

    // Pier side
    switch (s.pierSide)
    {
        case MountStatus::PierSide::EAST:
            m_statusTP[4].setText("East");
            break;
        case MountStatus::PierSide::WEST:
            m_statusTP[4].setText("West");
            break;
        default:
            m_statusTP[4].setText("--");
            break;
    }

    // PEC state
    switch (s.pecState)
    {
        case MountStatus::PecState::PLAYING:
            m_statusTP[5].setText("Playing");
            break;
        case MountStatus::PecState::READY_PLAY:
            m_statusTP[5].setText("Ready-Play");
            break;
        case MountStatus::PecState::RECORDING:
            m_statusTP[5].setText("Recording");
            break;
        case MountStatus::PecState::READY_RECORD:
            m_statusTP[5].setText("Ready-Record");
            break;
        default:
            m_statusTP[5].setText("Ignored");
            break;
    }

    m_statusTP[6].setText(s.gotoActive                           ? "Yes" : "No");
    m_statusTP[7].setText((s.guideActive || s.pulseGuideActive)  ? "Yes" : "No");
    m_statusTP[8].setText(s.atHome                               ? "Yes" : "No");
    m_statusTP[9].setText(s.homing                               ? "Yes" : "No");

    char errbuf[64];
    if (s.errorCode == 0)
        snprintf(errbuf, sizeof(errbuf), "None");
    else
        snprintf(errbuf, sizeof(errbuf), "%d: %s", s.errorCode, OnStepXStatus::errorText(s.errorCode));
    m_statusTP[10].setText(errbuf);

    reportError(s);

    m_statusTP.setState(s.errorCode == 0 ? IPS_OK : IPS_ALERT);
    m_statusTP.apply();
}

// ---------------------------------------------------------------------------
// reportError — log the firmware's general error when it changes.
//
// The firmware keeps the error set for as long as the condition lasts (a
// limit stays exceeded until the mount is moved back), so it is logged once
// on the change, not every poll.
// ---------------------------------------------------------------------------
const char *OnStepXInfo::getDeviceName() const
{
    return m_dev ? m_dev->getDeviceName() : "Unknown";
}

void OnStepXInfo::reportError(const MountStatus &s)
{
    // Act on a change only once it is seen on two polls in a row, so a
    // garbled status reply does not log a bogus error and its clearing.
    const bool confirmed = (s.errorCode == m_pendingError);
    m_pendingError = s.errorCode;
    if (s.errorCode == m_lastError || !confirmed)
        return;

    const int previous = m_lastError;
    m_lastError = s.errorCode;

    if (s.errorCode == 0)
    {
        LOGF_INFO("Controller error cleared (was: %s).", OnStepXStatus::errorText(previous));
        return;
    }

    if (!OnStepXStatus::errorStopsMotion(s.errorCode))
    {
        LOGF_WARN("Controller reports: %s.", OnStepXStatus::errorText(s.errorCode));
        return;
    }

    // Meridian limits apply to the side the mount is on.  At the west limit
    // (west of the pier, past the meridian) the firmware stops tracking.
    if (s.errorCode == 7 && s.pierSide == MountStatus::PierSide::WEST)
        LOG_ERROR("Controller stopped the mount: west meridian limit reached and tracking is off. "
                  "Flip the mount (the Flip slew mode, or a goto with pier preference East), "
                  "or enable Meridian Auto Flip so the controller flips at the limit.");
    else if (s.errorCode == 7 && s.pierSide == MountStatus::PierSide::EAST)
        // Limits::poll() stops only motion further east here; tracking goes on.
        LOG_ERROR("Controller reached the east meridian limit: motion further east is stopped.");
    else
        LOGF_ERROR("Controller stopped the mount: %s.", OnStepXStatus::errorText(s.errorCode));
}
