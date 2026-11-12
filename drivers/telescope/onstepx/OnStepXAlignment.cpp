/*
    OnStepX INDI Driver — Alignment helper
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
*/

#include "OnStepXAlignment.h"
#include "OnStepXComm.h"

#include <defaultdevice.h>
#include <indilogger.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "OnStepXStatus.h"

#define ALIGN_TAB "Alignment"

// Control switch indices
enum { CTRL_START = 0, CTRL_ACCEPT = 1 };

// ---------------------------------------------------------------------------
// initProperties
// ---------------------------------------------------------------------------
void OnStepXAlignment::initProperties()
{
    const char *dev = m_dev ? m_dev->getDeviceName() : "";

    // Star count selector
    m_starsSP[0].fill("STARS_1", "1 Star",  ISS_ON);
    m_starsSP[1].fill("STARS_2", "2 Stars", ISS_OFF);
    m_starsSP[2].fill("STARS_3", "3 Stars", ISS_OFF);
    m_starsSP[3].fill("STARS_4", "4 Stars", ISS_OFF);
    m_starsSP[4].fill("STARS_5", "5 Stars", ISS_OFF);
    m_starsSP[5].fill("STARS_6", "6 Stars", ISS_OFF);
    m_starsSP[6].fill("STARS_7", "7 Stars", ISS_OFF);
    m_starsSP[7].fill("STARS_8", "8 Stars", ISS_OFF);
    m_starsSP[8].fill("STARS_9", "9 Stars", ISS_OFF);
    m_starsSP.fill(dev, "OSX_ALIGN_STARS", "Star Count", ALIGN_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Start / Accept Star (momentary)
    m_controlSP[CTRL_START].fill("START",  "Start",       ISS_OFF);
    m_controlSP[CTRL_ACCEPT].fill("ACCEPT", "Accept Star",  ISS_OFF);
    m_controlSP.fill(dev, "OSX_ALIGN_CONTROL", "Alignment Control", ALIGN_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Write to EEPROM (momentary)
    m_writeSP[0].fill("WRITE", "Write to EEPROM", ISS_OFF);
    m_writeSP.fill(dev, "OSX_ALIGN_WRITE", "Save Alignment", ALIGN_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Status display
    m_statusTP[0].fill("STATUS",       "Status",       "Not started");
    m_statusTP[1].fill("MAX_STARS",    "Max Stars",    "");
    m_statusTP[2].fill("CURRENT_STAR", "Current Star", "");
    m_statusTP[3].fill("TARGET_STARS", "Target Stars", "");
    m_statusTP.fill(dev, "OSX_ALIGN_STATUS", "Alignment Status", ALIGN_TAB, IP_RO, 60, IPS_IDLE);

    // Polar error display
    m_errorTP[0].fill("POLAR_ALT", "Polar Error Alt (\")", "");
    m_errorTP[1].fill("POLAR_AZ",  "Polar Error Az (\")",  "");
    m_errorTP.fill(dev, "OSX_ALIGN_ERROR", "Polar Error", ALIGN_TAB, IP_RO, 60, IPS_IDLE);

    // Refine polar alignment (momentary)
    m_refineSP[0].fill("REFINE", "Refine Polar Align", ISS_OFF);
    m_refineSP.fill(dev, "OSX_POLAR_REFINE", "Polar Align", ALIGN_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
}

// ---------------------------------------------------------------------------
// updateProperties
// ---------------------------------------------------------------------------
void OnStepXAlignment::updateProperties(bool connected, bool equatorial)
{
    if (connected)
    {
        m_equatorial = equatorial;
        if (m_equatorial)
            m_dev->defineProperty(m_refineSP);
        m_dev->defineProperty(m_starsSP);
        m_dev->defineProperty(m_controlSP);
        m_dev->defineProperty(m_writeSP);
        m_dev->defineProperty(m_statusTP);
        m_dev->defineProperty(m_errorTP);
        updateStatus();
        updatePolarError();
    }
    else
    {
        m_dev->deleteProperty(m_starsSP);
        m_dev->deleteProperty(m_controlSP);
        m_dev->deleteProperty(m_writeSP);
        m_dev->deleteProperty(m_statusTP);
        m_dev->deleteProperty(m_errorTP);
        if (m_equatorial)
            m_dev->deleteProperty(m_refineSP);
        m_equatorial = false;
    }
}

// ---------------------------------------------------------------------------
// handleSwitch
// ---------------------------------------------------------------------------
bool OnStepXAlignment::handleSwitch(const char *name, ISState *states, char *names[], int n)
{
    // Star count selector
    if (m_starsSP.isNameMatch(name))
    {
        m_starsSP.update(states, names, n);
        m_starsSP.setState(IPS_OK);
        m_starsSP.apply();
        return true;
    }

    // Start / Accept
    if (m_controlSP.isNameMatch(name))
    {
        m_controlSP.update(states, names, n);
        int idx = m_controlSP.findOnSwitchIndex();
        bool ok = false;

        if (idx == CTRL_START)
        {
            int stars = m_starsSP.findOnSwitchIndex() + 1;  // 1-based
            ok = startAlignment(stars);
        }
        else if (idx == CTRL_ACCEPT)
        {
            ok = acceptStar();
        }

        m_controlSP.reset();
        m_controlSP.setState(ok ? IPS_OK : IPS_ALERT);
        m_controlSP.apply();
        if (ok)
            updateStatus();
        return true;
    }

    // Write to EEPROM
    if (m_writeSP.isNameMatch(name))
    {
        m_writeSP.update(states, names, n);
        bool ok = writeAlignment();
        m_writeSP.reset();
        m_writeSP.setState(ok ? IPS_OK : IPS_ALERT);
        m_writeSP.apply();
        return true;
    }

    // Refine polar alignment
    if (m_refineSP.isNameMatch(name))
    {
        m_refineSP.update(states, names, n);
        const bool ok = refinePolarAlign();
        m_refineSP.reset();
        m_refineSP.setState(ok ? IPS_OK : IPS_ALERT);
        m_refineSP.apply();
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// saveConfig — nothing persistent; firmware holds alignment state
// ---------------------------------------------------------------------------
void OnStepXAlignment::saveConfig(FILE * /*fp*/)
{
}

// ---------------------------------------------------------------------------
// updateStatus — :A?# -> status text fields
// ---------------------------------------------------------------------------
bool OnStepXAlignment::updateStatus()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE] {};
    if (!m_comm->sendText(":A?#", reply) || strlen(reply) < 3)
        return false;

    // Reply "mno", each '0' + n (so ':' is 10):
    //   m = maximum stars, n = current star (1 more than the number
    //   accepted), o = last star of the alignment in progress (0 if none).
    const int maxStars    = reply[0] - '0';
    const int currentStar = reply[1] - '0';
    const int targetStars = reply[2] - '0';

    const char *statusStr;
    if (targetStars == 0)
        statusStr = "Not started";
    else if (currentStar <= targetStars)
        statusStr = "In progress";
    else
        statusStr = "Complete";

    char maxBuf[8], curBuf[8], tgtBuf[8];
    snprintf(maxBuf, sizeof(maxBuf), "%d", maxStars);
    snprintf(curBuf, sizeof(curBuf), "%d", currentStar);
    snprintf(tgtBuf, sizeof(tgtBuf), "%d", targetStars);

    m_statusTP[0].setText(statusStr);
    m_statusTP[1].setText(maxBuf);
    m_statusTP[2].setText(curBuf);
    m_statusTP[3].setText(tgtBuf);
    m_statusTP.setState(IPS_OK);
    m_statusTP.apply();

    return true;
}

// ---------------------------------------------------------------------------
// updatePolarError — :GX02# and :GX03#
// ---------------------------------------------------------------------------
void OnStepXAlignment::updatePolarError()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];

    if (m_comm->sendText(":GX02#", reply))
    {
        m_errorTP[0].setText(reply);
    }

    if (m_comm->sendText(":GX03#", reply))
    {
        m_errorTP[1].setText(reply);
    }

    m_errorTP.setState(IPS_OK);
    m_errorTP.apply();
}

// ---------------------------------------------------------------------------
// startAlignment — :A[n]# replies bare '1' on success
// ---------------------------------------------------------------------------
bool OnStepXAlignment::startAlignment(int stars)
{
    char cmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(cmd, sizeof(cmd), ":A%d#", stars);
    const bool ok = m_comm->sendBool(cmd);   // bare '1' on success
    if (!ok)
        LOGF_ERROR("Start alignment with %d stars failed", stars);
    return ok;
}

// ---------------------------------------------------------------------------
// acceptStar — :A+# replies bare '1' on success
// ---------------------------------------------------------------------------
bool OnStepXAlignment::acceptStar()
{
    const bool ok = m_comm->sendBool(":A+#");   // bare '1' on success
    if (!ok)
        LOG_ERROR("Accept star command rejected by firmware");
    return ok;
}

// ---------------------------------------------------------------------------
// writeAlignment — :AW# reply '1' on success
// ---------------------------------------------------------------------------
bool OnStepXAlignment::writeAlignment()
{
    const bool ok = m_comm->sendBool(":AW#");
    if (ok)
        LOG_INFO("Alignment written to EEPROM");
    else
        LOG_ERROR("Write alignment to EEPROM failed");
    return ok;
}

// ---------------------------------------------------------------------------
// refinePolarAlign — :MP# (Goto.command.cpp), one unframed digit like :MS#
//
// The firmware zeroes the model's polar error terms (altCor/azmCor) and
// re-slews to the current position, so the star moves off centre by the
// polar error; the user then centres it with the mount's adjusters.
// ---------------------------------------------------------------------------
bool OnStepXAlignment::refinePolarAlign()
{
    if (!m_equatorial)
    {
        LOG_ERROR("Refine polar alignment needs an equatorial mount.");
        return false;
    }
    if (m_modelBuildActive && m_modelBuildActive())
    {
        LOG_ERROR("Refine polar alignment is not available while a pointing model is being built.");
        return false;
    }

    // Without a measured polar error the command would only re-slew in place.
    char alt[OnStepXComm::REPLY_BUF_SIZE], azm[OnStepXComm::REPLY_BUF_SIZE];
    if (!m_comm->sendText(":GX02#", alt) || !m_comm->sendText(":GX03#", azm) ||
            (std::atol(alt) == 0 && std::atol(azm) == 0))
    {
        LOG_ERROR("Refine polar alignment: no polar error has been measured. "
                  "Run a star alignment with 2 or more stars first.");
        return false;
    }

    char r = '\0';
    if (!m_comm->sendChar(":MP#", r))
    {
        LOG_ERROR("Refine polar alignment: no reply to :MP#");
        return false;
    }
    if (r != '0')
    {
        LOGF_ERROR("Refine polar alignment rejected by firmware: %s (code '%c')",
                   OnStepXStatus::gotoErrorText(r), r);
        return false;
    }

    LOGF_INFO("Refine polar alignment: removing a polar error of %s\" in altitude and %s\" in azimuth. "
              "When the slew ends, centre the star with the mount's altitude and azimuth adjusters "
              "(not the motors), then run a new star alignment.", alt, azm);
    updatePolarError();
    return true;
}
