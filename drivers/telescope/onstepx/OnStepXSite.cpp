/*
    OnStepX INDI Driver — Site/time helper (mount binary only)
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

#include "OnStepXSite.h"
#include "OnStepXComm.h"
#include "OnStepXFormat.h"

#include <indicom.h>                    // f_scansexa, getSexComponentsIID

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define SITES_TAB "Site Management"   // INDI::Telescope SITE_TAB

// ---------------------------------------------------------------------------
// initProperties
// ---------------------------------------------------------------------------
void OnStepXSite::initProperties()
{
    const char *dev = m_dev ? m_dev->getDeviceName() : "";

    // --- OSX_SITE_SELECT ---
    m_siteSelectSP[0].fill("SITE_1", "Site 1", ISS_ON);
    m_siteSelectSP[1].fill("SITE_2", "Site 2", ISS_OFF);
    m_siteSelectSP[2].fill("SITE_3", "Site 3", ISS_OFF);
    m_siteSelectSP[3].fill("SITE_4", "Site 4", ISS_OFF);
    m_siteSelectSP.fill(dev, "OSX_SITE_SELECT", "Site Profile",
                        SITES_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // --- OSX_SITE_NAME ---
    m_siteNameTP[0].fill("SITE_NAME", "Name", "");
    m_siteNameTP.fill(dev, "OSX_SITE_NAME", "Site Name",
                      SITES_TAB, IP_RW, 60, IPS_IDLE);
}

// ---------------------------------------------------------------------------
// updateProperties
// ---------------------------------------------------------------------------
void OnStepXSite::updateProperties(bool connected)
{
    if (connected)
    {
        m_dev->defineProperty(m_siteSelectSP);
        m_dev->defineProperty(m_siteNameTP);
        readActiveSite();
        readSiteNames();
    }
    else
    {
        m_dev->deleteProperty(m_siteSelectSP);
        m_dev->deleteProperty(m_siteNameTP);
    }
}

// ---------------------------------------------------------------------------
// handleSwitch — OSX_SITE_SELECT
// ---------------------------------------------------------------------------
bool OnStepXSite::handleSwitch(const char *name, ISState *states, char *names[], int n)
{
    if (!m_siteSelectSP.isNameMatch(name))
        return false;

    m_siteSelectSP.update(states, names, n);

    // Determine which site was selected (1-based)
    int site = 1;
    for (int i = 0; i < 4; i++)
        if (m_siteSelectSP[i].getState() == ISS_ON)
        {
            site = i + 1;
            break;
        }

    double lat = 0, lon = 0, elev = 0;
    if (selectSite(site, lat, lon, elev))
    {
        m_lastLat         = lat;
        m_lastLon         = lon;
        m_lastElev        = elev;
        m_locationUpdated = true;
        m_siteSelectSP.setState(IPS_OK);
        readSiteNames();
    }
    else
    {
        m_siteSelectSP.setState(IPS_ALERT);
    }
    m_siteSelectSP.apply();
    return true;
}

// ---------------------------------------------------------------------------
// handleText — OSX_SITE_NAME
// ---------------------------------------------------------------------------
bool OnStepXSite::handleText(const char *name, char *texts[], char *names[], int n)
{
    if (!m_siteNameTP.isNameMatch(name))
        return false;

    m_siteNameTP.update(texts, names, n);

    // The firmware's command buffer drops every space, so "Dark Site" would be
    // stored as "DarkSite".  Store an underscore instead to keep the words apart.
    std::string newName = m_siteNameTP[0].getText();
    std::replace(newName.begin(), newName.end(), ' ', '_');

    // The firmware limit is 15 characters.  Longer names would also overflow
    // the command buffer and lose the terminator, and '#' would end it early.
    if (newName.size() > 15 || newName.find('#') != std::string::npos)
    {
        if (m_dev)
            LOG_ERROR("Site names are limited to 15 characters and cannot contain '#'");
        m_siteNameTP.setState(IPS_ALERT);
        m_siteNameTP.apply();
        return true;
    }

    // Command depends on active site slot
    const char *cmds[4] = { ":SM", ":SN", ":SO", ":SP" };
    int idx = m_activeSite - 1;
    if (idx < 0 || idx > 3) idx = 0;

    char cmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(cmd, sizeof(cmd), "%s%s#", cmds[idx], newName.c_str());
    // :SM/:SN/:SO/:SP reply bare '1'/'0' ('0' if longer than 15 chars).
    if (m_comm->sendBool(cmd))
    {
        m_siteNameTP.setState(IPS_OK);
        // Refresh switch labels too
        readSiteNames();
    }
    else
    {
        m_siteNameTP.setState(IPS_ALERT);
    }
    m_siteNameTP.apply();
    return true;
}

// ---------------------------------------------------------------------------
// saveConfig — site names are stored in firmware; nothing to persist here
// ---------------------------------------------------------------------------
void OnStepXSite::saveConfig(FILE *fp)
{
    (void)fp;
}

// ---------------------------------------------------------------------------
// readActiveSite — :W?# replies the firmware site number 0..3
// ---------------------------------------------------------------------------
void OnStepXSite::readActiveSite()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    if (!m_comm->sendText(":W?#", reply) || reply[0] < '0' || reply[0] > '3')
        return;

    m_activeSite = reply[0] - '0' + 1;
    m_siteSelectSP.reset();
    m_siteSelectSP[m_activeSite - 1].setState(ISS_ON);
}

// ---------------------------------------------------------------------------
// readSiteNames — query :GM#/:GN#/:GO#/:GP#; update labels and name field
// ---------------------------------------------------------------------------
void OnStepXSite::readSiteNames()
{
    const char *cmds[4]  = { ":GM#", ":GN#", ":GO#", ":GP#" };
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    char names[4][64]    = { "Site 1", "Site 2", "Site 3", "Site 4" };

    for (int i = 0; i < 4; i++)
    {
        if (m_comm->sendText(cmds[i], reply) && reply[0] != '\0')
            snprintf(names[i], sizeof(names[i]), "%s", reply);
        m_siteSelectSP[i].setLabel(names[i]);
    }

    m_siteSelectSP.setState(IPS_OK);
    m_siteSelectSP.apply();

    // Populate name field with active site's name
    int idx = m_activeSite - 1;
    if (idx < 0 || idx > 3) idx = 0;
    m_siteNameTP[0].setText(names[idx]);
    m_siteNameTP.setState(IPS_OK);
    m_siteNameTP.apply();
}

// ---------------------------------------------------------------------------
// selectSite — send :W[n]#, then re-read location
// ---------------------------------------------------------------------------
bool OnStepXSite::selectSite(int n, double &lat, double &lon, double &elev)
{
    // n is 1-based (switch index + 1); the firmware numbers sites 0..3.
    // :W[n]# replies nothing.
    if (n < 1 || n > 4)
        return false;

    char cmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(cmd, sizeof(cmd), ":W%d#", n - 1);
    if (!m_comm->sendNoReply(cmd))
        return false;

    m_activeSite = n;

    // Re-read location so caller can update GEOGRAPHIC_COORD
    return readLocation(lat, lon, elev);
}

// ---------------------------------------------------------------------------
// writeLocation
// ---------------------------------------------------------------------------
bool OnStepXSite::writeLocation(double latitude, double longitude, double elevation)
{

    // --- Latitude  (+DD:MM:SS, signed) ---
    int latD, latM;
    double latS;

    const char latSign = latitude < 0.0 ? '-' : '+';

    getSexComponentsIID(std::fabs(latitude), &latD, &latM, &latS);
    normaliseDMS(&latD, &latM, &latS);

    char latCmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(latCmd, sizeof(latCmd), ":St%c%02d:%02d:%.2f#",
             latSign, latD, latM, latS);

    if (!m_comm->sendBool(latCmd))
    {
        if (m_dev)
            LOG_ERROR("Failed to set site latitude");
        return false;
    }

    // --- Longitude  (DDD:MM:SS, West-positive as OnStepX expects) ---
    // INDI sends 0-360 East; OnStepX :Sg# expects West-positive (old LX200 convention).
    double osx_lon = 360.0 - longitude;
    if (osx_lon >= 360.0) osx_lon -= 360.0;
    if (osx_lon <    0.0) osx_lon += 360.0;

    int lonD, lonM;
    double lonS;
    getSexComponentsIID(osx_lon, &lonD, &lonM, &lonS);
    normaliseDMS(&lonD, &lonM, &lonS);
    if (lonD >= 360) lonD -= 360;
    char lonCmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(lonCmd, sizeof(lonCmd), ":Sg%.03d:%02d:%.02f#", lonD, lonM, lonS);

    if (!m_comm->sendBool(lonCmd))
    {
        if (m_dev)
            LOG_ERROR("Failed to set site longitude");
        return false;
    }

    // --- Elevation (integer metres) ---
    char elvCmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(elvCmd, sizeof(elvCmd), ":Sv%d#", static_cast<int>(std::round(elevation)));
    if (!m_comm->sendBool(elvCmd))
    {
        if (m_dev)
            LOG_WARN("Failed to set site elevation (non-critical)");
        // Elevation write failure is non-critical -- continue.
    }

    if (m_dev)
        LOG_DEBUG("Site location sent to controller");

    return true;
}

// ---------------------------------------------------------------------------
// readLocation
// ---------------------------------------------------------------------------
bool OnStepXSite::readLocation(double &latitude, double &longitude, double &elevation)
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];

    // Latitude -- :GtH# returns "+DD:MM:SS.S"
    if (!m_comm->sendText(":GtH#", reply))
        return false;
    double lat = 0;
    if (f_scansexa(reply, &lat) < 0)
        return false;
    latitude = lat;

    // Longitude -- :GgH# returns West-positive "DDD:MM:SS.S"; convert to East-positive.
    if (!m_comm->sendText(":GgH#", reply))
        return false;
    double osx_lon = 0;
    if (f_scansexa(reply, &osx_lon) < 0)
        return false;
    longitude = 360.0 - osx_lon;
    if (longitude >= 360.0) longitude -= 360.0;
    if (longitude <    0.0) longitude += 360.0;

    // Elevation -- :Gv# returns integer string (metres)
    if (!m_comm->sendText(":Gv#", reply))
    {
        elevation = 0;  // non-critical -- use zero if unavailable
    }
    else if (!OnStepXFormat::parseNumber(reply, elevation))
    {
        elevation = 0;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Civil date <-> seconds since 1970-01-01 (proleptic Gregorian, no leap
// seconds).  Howard Hinnant's days_from_civil / civil_from_days algorithms.
// ---------------------------------------------------------------------------
namespace
{
long long daysFromCivil(long long y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

void civilFromDays(long long z, int &y, int &m, int &d)
{
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp  = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    y = static_cast<int>(static_cast<long long>(yoe) + era * 400 + (m <= 2));
}
}

long long OnStepXSite::civilToEpochSeconds(int y, int mo, int d, int h, int mi, double s)
{
    return daysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 86400LL +
           h * 3600LL + mi * 60LL + std::llround(s);
}

void OnStepXSite::epochSecondsToCivil(long long t, int &y, int &mo, int &d,
                                      int &h, int &mi, int &s)
{
    long long days = t / 86400;
    long long rem  = t % 86400;
    if (rem < 0)
    {
        rem += 86400;
        days--;
    }
    civilFromDays(days, y, mo, d);
    h  = static_cast<int>(rem / 3600);
    mi = static_cast<int>((rem % 3600) / 60);
    s  = static_cast<int>(rem % 60);
}

// ---------------------------------------------------------------------------
// writeTime
// ---------------------------------------------------------------------------
bool OnStepXSite::writeTime(const ln_date *utc, double utc_offset)
{

    // 1. Set the timezone.
    //    OnStepX accepts ":SG[sHH]#" or ":SG[sHH:MM]#" where MM is 00, 30, or 45.
    //    The value is the number of hours ADDED TO LOCAL TIME TO YIELD UT
    //    (firmware Site::localToUT1: UT = local + timezone), i.e. the LX200
    //    convention.  INDI's utc_offset is the opposite (local = UTC + offset),
    //    so the sign must be inverted: UTC+1 (CET/BST) is sent as ":SG-01#".
    const double tz      = -utc_offset;
    const char   tzSign  = (tz < 0.0) ? '-' : '+';
    const double tzAbs   = std::fabs(tz);
    int          tzH     = static_cast<int>(tzAbs);
    int          tzMin   = static_cast<int>(std::round((tzAbs - tzH) * 60.0));
    // Snap minutes to the values the firmware accepts (0, 30, 45).
    if      (tzMin < 15) tzMin = 0;
    else if (tzMin < 38) tzMin = 30;
    else if (tzMin < 53) tzMin = 45;
    else
    {
        tzMin = 0;
        tzH++;
    }

    char tzCmd[OnStepXComm::CMD_MAX_LEN];
    if (tzMin == 0)
        snprintf(tzCmd, sizeof(tzCmd), ":SG%c%02d#", tzSign, tzH);
    else
        snprintf(tzCmd, sizeof(tzCmd), ":SG%c%02d:%02d#", tzSign, tzH, tzMin);

    if (!m_comm->sendBool(tzCmd))
    {
        if (m_dev)
            LOG_ERROR("Failed to set UTC offset (:SG#)");
        return false;
    }

    // 2. Compute local time and date = UTC - timezone, using the SNAPPED
    //    timezone just sent so the firmware's UT = local + timezone is exact.
    //    Integer-second arithmetic: ln_date_to_zonedate() round-trips through
    //    a floating-point Julian Day and yields e.g. 59.9999 s, which was
    //    truncated to the previous second (and could roll the date back).
    const long long tzSeconds = (tzSign == '-' ? -1LL : 1LL) *
                                (tzH * 3600LL + tzMin * 60LL);
    const long long utcSeconds = civilToEpochSeconds(utc->years, utc->months, utc->days,
                                 utc->hours, utc->minutes, utc->seconds);
    int ly, lmo, ld, lh, lmi, ls;
    epochSecondsToCivil(utcSeconds - tzSeconds, ly, lmo, ld, lh, lmi, ls);

    // 3. Send local time  ":SLHH:MM:SS#"
    char timeCmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(timeCmd, sizeof(timeCmd), ":SL%02d:%02d:%02d#", lh, lmi, ls);
    if (!m_comm->sendBool(timeCmd))
    {
        if (m_dev)
            LOG_ERROR("Failed to set local time (:SL#)");
        return false;
    }

    // 4. Send local date  ":SCMM/DD/YY#"
    //    Replies bare '1'/'0'.
    char dateCmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(dateCmd, sizeof(dateCmd), ":SC%02d/%02d/%02d#",
             lmo, ld, ly % 100);
    if (!m_comm->sendBool(dateCmd))
    {
        if (m_dev)
            LOG_ERROR("Failed to set local date (:SC#)");
        return false;
    }

    if (m_dev)
        LOG_DEBUG("Date/time sent to controller");

    return true;
}

void OnStepXSite::normaliseDMS(int *d, int *m, double *s)
{
    // Round to 2dp first, matching the precision of the format string
    *s = round(*s * 100.0) / 100.0;

    if (*s >= 60.0)
    {
        *s -= 60.0;
        (*m)++;
    }
    if (*m >= 60)
    {
        *m -= 60;
        (*d)++;
    }
}