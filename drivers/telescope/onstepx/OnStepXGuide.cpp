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
*/

#include "OnStepXGuide.h"
#include "OnStepXComm.h"
#include "OnStepXFormat.h"

#include <indilogger.h>

#include <cstdio>
#include <cstdlib>

// "Motion Control" — the standard INDI tab shared with GuiderInterface properties
#define MOTION_TAB "Motion Control"

OnStepXGuide::OnStepXGuide()
{
    m_timerNS.setSingleShot(true);
    m_timerWE.setSingleShot(true);
    // Zero both pulse lengths when a pulse ends, as other INDI drivers do.
    // GuiderInterface acts on the first non-zero element, so a client that
    // sends only TIMED_GUIDE_E after a West pulse would otherwise get
    // another West pulse.
    m_timerNS.callOnTimeout([this]()
    {
        pulseDone(INDI_EQ_AXIS::AXIS_DE);
    });
    m_timerWE.callOnTimeout([this]()
    {
        pulseDone(INDI_EQ_AXIS::AXIS_RA);
    });
}

void OnStepXGuide::pulseDone(INDI_EQ_AXIS axis)
{
    if (m_dev)
    {
        auto np = m_dev->getNumber(axis == INDI_EQ_AXIS::AXIS_DE ? "TELESCOPE_TIMED_GUIDE_NS"
                                   : "TELESCOPE_TIMED_GUIDE_WE");
        if (np.isValid())
            for (auto &e : np)
                e.setValue(0);
    }
    if (m_gi)
        m_gi->GuideComplete(axis);
}

// ---------------------------------------------------------------------------
// initProperties
// ---------------------------------------------------------------------------
void OnStepXGuide::initProperties()
{
    const char *dev = m_dev ? m_dev->getDeviceName() : "";

    // INDI's standard guide-rate property, so guiding software can read it.
    m_guideRateNP[0].fill("GUIDE_RATE_WE", "W/E Rate (x sidereal)", "%.2f", 0.25, 1.0, 0.25, 0.5);
    m_guideRateNP[1].fill("GUIDE_RATE_NS", "N/S Rate (x sidereal)", "%.2f", 0.25, 1.0, 0.25, 0.5);
    m_guideRateNP.fill(dev, "GUIDE_RATE", "Guide Rate",
                       MOTION_TAB, IP_RW, 60, IPS_IDLE);
}

// ---------------------------------------------------------------------------
// updateProperties
// ---------------------------------------------------------------------------
void OnStepXGuide::updateProperties(bool connected)
{
    if (connected)
    {
        m_dev->defineProperty(m_guideRateNP);
        readGuideRate();
    }
    else
    {
        m_timerNS.stop();
        m_timerWE.stop();
        m_dev->deleteProperty(m_guideRateNP);
    }
}

// ---------------------------------------------------------------------------
// Rate mapping
// ---------------------------------------------------------------------------
int OnStepXGuide::rateToIndex(double rate)
{
    if (rate < 0.375)
        return 0;   // 0.25x
    if (rate < 0.75)
        return 1;   // 0.5x
    return 2;       // 1x
}

double OnStepXGuide::indexToRate(int index)
{
    static const double rates[] = { 0.25, 0.5, 1.0 };
    return (index >= 0 && index <= 2) ? rates[index] : 0.5;
}

// ---------------------------------------------------------------------------
// handleNumber — GUIDE_RATE write.  Either element may be edited; the
// firmware has a single pulse rate, so both are set to the same value.
// ---------------------------------------------------------------------------
bool OnStepXGuide::handleNumber(const char *name, double values[], char *names[], int n)
{
    if (!m_guideRateNP.isNameMatch(name))
        return false;

    const double prevRa = m_guideRateNP[0].getValue();
    m_guideRateNP.update(values, names, n);
    const double requested = (m_guideRateNP[0].getValue() != prevRa)
                             ? m_guideRateNP[0].getValue()
                             : m_guideRateNP[1].getValue();

    const int idx = rateToIndex(requested);
    char cmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(cmd, sizeof(cmd), ":R%d#", idx);

    const bool ok = m_comm && m_comm->sendNoReply(cmd);

    const double actual = indexToRate(idx);
    m_guideRateNP[0].setValue(actual);
    m_guideRateNP[1].setValue(actual);
    m_guideRateNP.setState(ok ? IPS_OK : IPS_ALERT);
    m_guideRateNP.apply();
    return true;
}

// ---------------------------------------------------------------------------
// saveConfig
// ---------------------------------------------------------------------------
void OnStepXGuide::saveConfig(FILE *fp)
{
    // Not saved: :R<n># stores the pulse-guide rate in the firmware's NV
    // (GUIDE_SETTINGS), and a config load would overwrite it.
    (void)fp;
}

// ---------------------------------------------------------------------------
// Guide pulses
// ---------------------------------------------------------------------------
IPState OnStepXGuide::pulse(char dir, uint32_t ms, INDI::Timer &timer)
{
    if (ms > MAX_PULSE_MS)
    {
        LOGF_WARN("Guide pulse of %u ms exceeds the firmware limit; clamped to %u ms",
                  ms, MAX_PULSE_MS);
        ms = MAX_PULSE_MS;
    }

    char cmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(cmd, sizeof(cmd), ":Mg%c%u#", dir, ms);
    if (!m_comm || !m_comm->sendNoReply(cmd))
        return IPS_ALERT;

    timer.start(static_cast<int>(ms));
    return IPS_BUSY;
}

IPState OnStepXGuide::guideNorth(uint32_t ms)
{
    return pulse('n', ms, m_timerNS);
}

IPState OnStepXGuide::guideSouth(uint32_t ms)
{
    return pulse('s', ms, m_timerNS);
}

IPState OnStepXGuide::guideEast(uint32_t ms)
{
    return pulse('e', ms, m_timerWE);
}

IPState OnStepXGuide::guideWest(uint32_t ms)
{
    return pulse('w', ms, m_timerWE);
}

// ---------------------------------------------------------------------------
// readGuideRate — :GX90# -> "n.nn#" pulse-guide rate (x sidereal)
// ---------------------------------------------------------------------------
void OnStepXGuide::readGuideRate()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    if (!m_comm || !m_comm->sendText(":GX90#", reply))
        return;

    double rate = 0;
    if (!OnStepXFormat::parseNumber(reply, rate) || rate <= 0.0)
        return;

    m_guideRateNP[0].setValue(rate);
    m_guideRateNP[1].setValue(rate);
    m_guideRateNP.setState(IPS_OK);
    m_guideRateNP.apply();
}
