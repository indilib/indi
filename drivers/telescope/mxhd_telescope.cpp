/*******************************************************************************
  Copyright(c) 2026 Makoto Kasahara All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file LICENSE.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/
#include "mxhd_telescope.h"
#include "mxhd_serial.h"

#include <algorithm>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <libnova/libnova.h>
#include <memory>
#include <thread>
#include <regex>
#include <unistd.h>
#include <vector>

static double clampDec(double d)
{
    if (d > 90.0) return 90.0;
    if (d < -90.0) return -90.0;
    return d;
}

static double normalizeHours(double h)
{
    while (h < 0.0) h += 24.0;
    while (h >= 24.0) h -= 24.0;
    return h;
}

static double normalizeSignedHours(double h)
{
    h = normalizeHours(h);
    if (h >= 12.0)
        h -= 24.0;
    return h;
}

static bool extractNumbers(const std::string &s, std::vector<double> &out)
{
    out.clear();
    static const std::regex re(R"([-+]?\d+(\.\d+)?)");
    for (auto it = std::sregex_iterator(s.begin(), s.end(), re); it != std::sregex_iterator(); ++it)
    {
        try
        {
            out.push_back(std::stod((*it).str()));
        }
        catch (...)
        {
        }
    }
    return !out.empty();
}

static bool parseRaHours(const std::string &ra, double &hours)
{
    std::vector<double> nums;
    if (!extractNumbers(ra, nums)) return false;
    if (nums.size() == 1)
    {
        hours = normalizeHours(nums[0]);
        return true;
    }
    const double hh = nums[0];
    const double mm = nums.size() >= 2 ? nums[1] : 0.0;
    const double ss = nums.size() >= 3 ? nums[2] : 0.0;
    hours = normalizeHours(hh + mm / 60.0 + ss / 3600.0);
    return true;
}

static bool parseDecDeg(const std::string &dec, double &deg)
{
    const auto firstDigit = dec.find_first_of("0123456789");
    const bool explicitNegative = (firstDigit != std::string::npos &&
                                   dec.find('-', 0) != std::string::npos &&
                                   dec.find('-', 0) < firstDigit);

    std::vector<double> nums;
    if (!extractNumbers(dec, nums)) return false;
    if (nums.size() == 1)
    {
        deg = clampDec(nums[0]);
        return true;
    }
    const double dd = nums[0];
    const double mm = nums.size() >= 2 ? std::fabs(nums[1]) : 0.0;
    const double ss = nums.size() >= 3 ? std::fabs(nums[2]) : 0.0;
    const double sign = (dd < 0.0 || explicitNegative) ? -1.0 : 1.0;
    deg = clampDec(sign * (std::fabs(dd) + mm / 60.0 + ss / 3600.0));
    return true;
}

static void normalizeSexagesimal(int &high, int &mid, int &low)
{
    if (low >= 60)
    {
        low -= 60;
        ++mid;
    }
    if (mid >= 60)
    {
        mid -= 60;
        ++high;
    }
}

static int roundUtcOffsetHours(double utcOffsetHoursEast)
{
    return static_cast<int>(std::lround(utcOffsetHoursEast));
}

static std::string formatUtcOffsetForMount(int utcOffsetHoursEast)
{
    // MX-HD / LX200 expects the opposite sign of the INDI/ASCOM east-positive convention.
    const int mountHours = std::max(-14, std::min(14, -utcOffsetHoursEast));
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%c%02d", mountHours >= 0 ? '+' : '-', std::abs(mountHours));
    return std::string(buf);
}

static std::string formatDateMdy(const ln_date &date)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d/%02d/%02d",
                  date.months,
                  date.days,
                  date.years % 100);
    return std::string(buf);
}

static bool makeLocalDateFromUtc(const ln_date *utc, double utcOffsetHoursEast, ln_date &localDate)
{
    if (!utc)
        return false;

    const double utcJd = ln_get_julian_day(const_cast<ln_date *>(utc));
    ln_get_date(utcJd + (utcOffsetHoursEast / 24.0), &localDate);
    return true;
}

static bool normalizeLocalDateTime(const ln_date &date, ln_date &normalized)
{
    std::tm tm {};
    tm.tm_year = date.years - 1900;
    tm.tm_mon = date.months - 1;
    tm.tm_mday = date.days;
    tm.tm_hour = date.hours;
    tm.tm_min = date.minutes;
    tm.tm_sec = static_cast<int>(std::lround(date.seconds));
    tm.tm_isdst = -1;

    time_t t = ::timegm(&tm);
    if (t == static_cast<time_t>(-1))
        return false;

    std::tm tmNormalized {};
    if (!::gmtime_r(&t, &tmNormalized))
        return false;

    normalized.years = tmNormalized.tm_year + 1900;
    normalized.months = tmNormalized.tm_mon + 1;
    normalized.days = tmNormalized.tm_mday;
    normalized.hours = tmNormalized.tm_hour;
    normalized.minutes = tmNormalized.tm_min;
    normalized.seconds = tmNormalized.tm_sec;
    return true;
}

static std::string formatTimeHms(const ln_date &date)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                  date.hours,
                  date.minutes,
                  static_cast<int>(std::lround(date.seconds)));
    return std::string(buf);
}

static std::string formatLatDmm(double latitudeDegrees)
{
    const double lat = std::max(-90.0, std::min(90.0, latitudeDegrees));
    const char sign = lat >= 0.0 ? '+' : '-';
    const double absLat = std::fabs(lat);
    int deg = static_cast<int>(std::floor(absLat));
    int min = static_cast<int>(std::lround((absLat - deg) * 60.0));
    if (min >= 60)
    {
        min -= 60;
        ++deg;
    }
    if (deg > 90)
    {
        deg = 90;
        min = 0;
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%c%02d*%02d", sign, deg, min);
    return std::string(buf);
}

static std::string formatLonDmmMountWestPositive(double longitudeDegreesEastPositive)
{
    double lonE = std::fmod(longitudeDegreesEastPositive, 360.0);
    if (lonE < 0.0)
        lonE += 360.0;

    // Convert INDI/ASCOM east-positive longitude to the mount's west-positive form.
    double lonW = std::fmod(360.0 - lonE, 360.0);
    if (lonW < 0.0)
        lonW += 360.0;

    int totalMinutes = static_cast<int>(std::lround(lonW * 60.0));
    totalMinutes %= (360 * 60);
    if (totalMinutes < 0)
        totalMinutes += (360 * 60);

    const int deg = totalMinutes / 60;
    const int min = totalMinutes % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "+%03d*%02d", deg, min);
    return std::string(buf);
}

static double currentUtcJulianDay()
{
    const auto now = std::chrono::system_clock::now();
    const auto epochSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();
    return 2440587.5 + (epochSeconds / 86400.0);
}

static double greenwichMeanSiderealHours(double julianDay)
{
    const double t = (julianDay - 2451545.0) / 36525.0;
    double gmstDegrees = 280.46061837 +
                         360.98564736629 * (julianDay - 2451545.0) +
                         0.000387933 * t * t -
                         (t * t * t) / 38710000.0;

    gmstDegrees = std::fmod(gmstDegrees, 360.0);
    if (gmstDegrees < 0.0)
        gmstDegrees += 360.0;

    return gmstDegrees / 15.0;
}

static std::string formatRaSr(double raHours)
{
    // LX200 :Sr HH:MM:SS#
    raHours = normalizeHours(raHours);
    int hh = static_cast<int>(std::floor(raHours));
    const double m = (raHours - hh) * 60.0;
    int mm = static_cast<int>(std::floor(m));
    int ss = static_cast<int>(std::round((m - mm) * 60.0));
    normalizeSexagesimal(hh, mm, ss);
    if (hh >= 24) hh -= 24;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss);
    return std::string(buf);
}

static std::string formatDecSd(double decDeg)
{
    // LX200 :Sd sDD*MM:SS#
    const double d = clampDec(decDeg);
    const char sign = (d < 0.0) ? '-' : '+';
    const double ad = std::fabs(d);
    int dd = static_cast<int>(std::floor(ad));
    const double m = (ad - dd) * 60.0;
    int mm = static_cast<int>(std::floor(m));
    int ss = static_cast<int>(std::round((m - mm) * 60.0));
    normalizeSexagesimal(dd, mm, ss);
    if (dd > 90)
    {
        dd = 90;
        mm = 0;
        ss = 0;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%c%02d*%02d:%02d", sign, dd, mm, ss);
    return std::string(buf);
}

MXHDTelescope::MXHDTelescope()
{
    setVersion(1, 0);
    setDefaultPollingPeriod(4000);
    uint32_t capabilities =
        TELESCOPE_CAN_GOTO |
        TELESCOPE_CAN_SYNC |
        TELESCOPE_CAN_PARK |
        TELESCOPE_CAN_ABORT |
        TELESCOPE_HAS_TIME |
        TELESCOPE_HAS_LOCATION |
        TELESCOPE_HAS_TRACK_MODE |
        TELESCOPE_CAN_CONTROL_TRACK |
        /* MX-HD supports adjustable tracking rate in firmware, but this
           driver version only exposes the standard sidereal/solar/lunar
           track modes and does not implement arbitrary numeric track-rate
           control yet. */
        /* TELESCOPE_HAS_TRACK_RATE | */
        TELESCOPE_HAS_PIER_SIDE |
        TELESCOPE_HAS_PIER_SIDE_SIMULATION |
        0;
    SetTelescopeCapability(capabilities, 4);
}

const char *MXHDTelescope::getDefaultName()
{
    return "MX-HD Telescope";
}

bool MXHDTelescope::initProperties()
{
    INDI::Telescope::initProperties();
    setDriverInterface(TELESCOPE_INTERFACE);
    setSimulatePierSide(true);

    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");

    // Home command (custom simple button)
    IUFillSwitch(&HomeS[0], "FIND_HOME", "Home", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 1, getDeviceName(), "MXHD_HOME", "Home", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Timed guide (INDI-compatible property names)
    IUFillNumber(&GuideNSN[0], "TIMED_GUIDE_N", "North (ms)", "%.0f", 0, 600000, 0, 0);
    IUFillNumber(&GuideNSN[1], "TIMED_GUIDE_S", "South (ms)", "%.0f", 0, 600000, 0, 0);
    IUFillNumberVector(&GuideNSNP, GuideNSN, 2, getDeviceName(), "TELESCOPE_TIMED_GUIDE_NS", "Pulse Guide NS", MOTION_TAB,
                       IP_RW, 0, IPS_IDLE);

    IUFillNumber(&GuideWEN[0], "TIMED_GUIDE_W", "West (ms)", "%.0f", 0, 600000, 0, 0);
    IUFillNumber(&GuideWEN[1], "TIMED_GUIDE_E", "East (ms)", "%.0f", 0, 600000, 0, 0);
    IUFillNumberVector(&GuideWENP, GuideWEN, 2, getDeviceName(), "TELESCOPE_TIMED_GUIDE_WE", "Pulse Guide WE", MOTION_TAB,
                       IP_RW, 0, IPS_IDLE);

    return true;
}

bool MXHDTelescope::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineProperty(&HomeSP);
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
    }
    else
    {
        deleteProperty(HomeSP.name);
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
    }

    return true;
}

bool MXHDTelescope::Handshake()
{
    // Best-effort: try reading status bytes. If it succeeds, we assume mount is present.
    // Some MX-HD boots or link layers need a short settle time right after port open.
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        uint8_t b1 = 0, b2 = 0, b3 = 0;
        if (mxhdReadStatus(b1, b2, b3))
        {
            decodeStatus(b1, b2, b3);
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    LOG_WARN("Handshake status read failed; continuing with connection so the client can retry.");
    return true;
}

bool MXHDTelescope::Connect()
{
    // Use base class serial connection (fd stored in PortFD)
    if (!INDI::Telescope::Connect())
        return false;

    // Wrap fd for helper I/O
    {
        std::lock_guard<std::mutex> lock(m_ioMutex);
        delete m_io;
        m_io = new MXHDSerial(PortFD);
    }

    // Default: tracking enabled on connect (we keep state, but mount behavior may differ).
    m_trackingEnabled = true;

    if (!Handshake())
        return false;

    if (!mxhdApplyCachedSiteTime())
    {
        LOG_ERROR("Failed to apply cached site/time information after connect.");
        (void)mxhdSend(":Q#");
        Disconnect();
        return false;
    }

    return true;
}

bool MXHDTelescope::Disconnect()
{
    // Invalidate any pending pulse-guide completion callbacks.
    {
        std::lock_guard<std::mutex> guideLock(m_guideMutex);
        ++m_guideTokenGen;
        m_guideActive = false;
        m_guideDirection = 0;
    }

    // Best-effort stop in case guiding/manual motion was in progress.
    (void)mxhdSend(":Q#");

    {
        std::lock_guard<std::mutex> lock(m_ioMutex);
        delete m_io;
        m_io = nullptr;
    }
    return INDI::Telescope::Disconnect();
}

bool MXHDTelescope::updateLocation(double latitude, double longitude, double elevation)
{
    bool shouldSend = false;
    {
        std::lock_guard<std::mutex> lock(m_siteTimeMutex);
        // INDI standard coordinates are latitude +N and longitude +E.
        // Skip the default 0/0 placeholder unless we've already seen a real site.
        if (!m_hasCachedSiteCoordinates &&
            std::fabs(latitude) <= 1e-9 &&
            std::fabs(longitude) <= 1e-9)
        {
            m_cachedSiteLatitudeDeg = latitude;
            m_cachedSiteLongitudeDeg = longitude;
            return true;
        }

        m_cachedSiteLatitudeDeg = latitude;
        m_cachedSiteLongitudeDeg = longitude;
        m_hasCachedSiteCoordinates = true;
        (void)elevation;
        shouldSend = isConnected();
    }

    if (!shouldSend)
        return true;

    if (!mxhdApplySiteCoordinates(latitude, longitude))
        return false;

    return true;
}

bool MXHDTelescope::updateTime(ln_date *utc, double utc_offset)
{
    if (!utc)
        return false;

    bool shouldSend = false;
    {
        std::lock_guard<std::mutex> lock(m_siteTimeMutex);
        m_cachedUtcDate = *utc;
        m_cachedUtcOffsetHoursEast = utc_offset;
        m_hasCachedTime = true;
        shouldSend = isConnected();
    }

    if (!shouldSend)
        return true;

    if (!mxhdApplyTime(utc, utc_offset))
        return false;

    return true;
}

bool MXHDTelescope::mxhdSend(const std::string &cmd)
{
    std::lock_guard<std::mutex> lock(m_ioMutex);
    if (!m_io)
        return false;

    std::string err;
    if (!m_io->writeString(cmd, err))
    {
        LOGF_ERROR("I/O send failed (%s): %s", cmd.c_str(), err.c_str());
        return false;
    }
    return true;
}

bool MXHDTelescope::mxhdQueryHash(const std::string &cmd, std::string &resp)
{
    std::lock_guard<std::mutex> lock(m_ioMutex);
    if (!m_io)
        return false;

    std::string err;
    if (!m_io->queryHash(cmd, resp, 2000, err))
    {
        LOGF_ERROR("I/O query failed (%s): %s", cmd.c_str(), err.c_str());
        return false;
    }
    return true;
}

bool MXHDTelescope::mxhdQueryAck(const std::string &cmd, char &ack, int timeoutMs)
{
    std::lock_guard<std::mutex> lock(m_ioMutex);
    if (!m_io)
        return false;

    std::string err;
    if (!m_io->queryAck(cmd, ack, timeoutMs, err))
    {
        LOGF_ERROR("I/O query failed (%s): %s", cmd.c_str(), err.c_str());
        return false;
    }
    return true;
}

bool MXHDTelescope::mxhdApplySiteCoordinates(double latitudeDeg, double longitudeDeg)
{
    const std::string cmdSt = ":St" + formatLatDmm(latitudeDeg) + "#";
    const std::string cmdSg = ":Sg" + formatLonDmmMountWestPositive(longitudeDeg) + "#";

    char ack = 0;
    if (!mxhdQueryAck(cmdSt, ack, 10000))
        return false;
    if (ack != '1')
    {
        LOGF_WARN("Unexpected :St response: '%c'", ack);
        return false;
    }

    if (!mxhdQueryAck(cmdSg, ack, 10000))
        return false;
    if (ack != '1')
    {
        LOGF_WARN("Unexpected :Sg response: '%c'", ack);
        return false;
    }

    return true;
}

bool MXHDTelescope::mxhdApplyTime(ln_date *utc, double utcOffsetHoursEast)
{
    if (!mxhdSendUtcOffset(utcOffsetHoursEast))
        return false;
    return mxhdSendLocalDateTime(utc, utcOffsetHoursEast);
}

bool MXHDTelescope::mxhdSendUtcOffset(double utcOffsetHoursEast, int timeoutMs)
{
    const int utcOffsetHoursRounded = roundUtcOffsetHours(utcOffsetHoursEast);
    if (std::fabs(utcOffsetHoursEast - utcOffsetHoursRounded) > 0.01)
    {
        LOGF_WARN("UTC offset is fractional (%.3fh); rounded to %dh for MX-HD :SG.",
                  utcOffsetHoursEast, utcOffsetHoursRounded);
    }

    const std::string cmdSg = ":SG" + formatUtcOffsetForMount(utcOffsetHoursRounded) + "#";
    char ack = 0;
    if (!mxhdQueryAck(cmdSg, ack, timeoutMs))
        return false;
    if (ack != '1')
    {
        LOGF_WARN("Unexpected :SG response: '%c'", ack);
        return false;
    }
    return true;
}

bool MXHDTelescope::mxhdSendLocalDateTime(ln_date *utc, double utcOffsetHoursEast, int timeoutMs)
{
    if (!utc)
        return false;

    ln_date localDate {};
    if (!makeLocalDateFromUtc(utc, utcOffsetHoursEast, localDate))
        return false;

    ln_date normalizedLocalDate {};
    if (!normalizeLocalDateTime(localDate, normalizedLocalDate))
    {
        LOG_ERROR("Failed to normalize local date/time for :SC/:SL.");
        return false;
    }

    const std::string date = formatDateMdy(normalizedLocalDate);
    const std::string time = formatTimeHms(normalizedLocalDate);
    if (date.empty())
    {
        LOG_ERROR("Failed to format local date for :SC.");
        return false;
    }
    if (time.empty())
    {
        LOG_ERROR("Failed to format local time for :SL.");
        return false;
    }

    const std::string cmdSc = ":SC" + date + "#";
    const std::string cmdSl = ":SL" + time + "#";

    char ack = 0;
    if (!mxhdQueryAck(cmdSc, ack, timeoutMs))
        return false;
    if (ack != '1')
    {
        LOGF_WARN("Unexpected :SC response: '%c'", ack);
        return false;
    }

    if (!mxhdQueryAck(cmdSl, ack, timeoutMs))
        return false;
    if (ack != '1')
    {
        LOGF_WARN("Unexpected :SL response: '%c'", ack);
        return false;
    }

    return true;
}

void MXHDTelescope::mxhdUpdatePierSide(double raHours)
{
    double longitudeDegEast = 0.0;
    {
        std::lock_guard<std::mutex> lock(m_siteTimeMutex);
        if (!m_hasCachedSiteCoordinates)
            return;
        longitudeDegEast = m_cachedSiteLongitudeDeg;
    }

    const double lstHours = normalizeHours(greenwichMeanSiderealHours(currentUtcJulianDay()) + (longitudeDegEast / 15.0));
    const double haHours = normalizeSignedHours(lstHours - raHours);

    // Hold the previous side very close to the meridian to avoid flapping.
    constexpr double kMeridianDeadbandHours = 1.0 / 60.0;
    if (std::fabs(haHours) < kMeridianDeadbandHours)
    {
        if (!m_hasPierSideCache)
            return;

        setPierSide(m_pierSideEast ? PIER_EAST : PIER_WEST);
        return;
    }

    // INDI definition:
    // - PIER_EAST: mount on east side of pier, telescope pointing west (HA > 0)
    // - PIER_WEST: mount on west side of pier, telescope pointing east (HA < 0)
    m_pierSideEast = (haHours > 0.0);
    m_hasPierSideCache = true;
    setPierSide(m_pierSideEast ? PIER_EAST : PIER_WEST);
}

bool MXHDTelescope::mxhdApplyCachedSiteTime()
{
    bool haveSite = false;
    bool haveTime = false;
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    ln_date utc {};
    double utcOffsetHoursEast = 0.0;

    {
        std::lock_guard<std::mutex> lock(m_siteTimeMutex);
        haveSite = m_hasCachedSiteCoordinates;
        haveTime = m_hasCachedTime;
        latitudeDeg = m_cachedSiteLatitudeDeg;
        longitudeDeg = m_cachedSiteLongitudeDeg;
        utc = m_cachedUtcDate;
        utcOffsetHoursEast = m_cachedUtcOffsetHoursEast;
    }

    // ASCOM order:
    // 1) SG (timezone)
    // 2) St/Sg (site)
    // 3) SC/SL (date/time)
    if (haveTime)
    {
        if (!mxhdSendUtcOffset(utcOffsetHoursEast, 10000))
            return false;
    }

    if (haveSite)
    {
        if (!mxhdApplySiteCoordinates(latitudeDeg, longitudeDeg))
            return false;
    }

    if (haveTime)
    {
        if (!mxhdSendLocalDateTime(&utc, utcOffsetHoursEast, 10000))
            return false;
    }

    if (!haveSite && !haveTime)
    {
        LOG_WARN("No cached INDI site/time values available at connect; skipping automatic :SG/:St/:Sg/:SC/:SL sync.");
    }

    return true;
}

bool MXHDTelescope::mxhdReadStatus(uint8_t &b1, uint8_t &b2, uint8_t &b3)
{
    std::lock_guard<std::mutex> lock(m_ioMutex);
    if (!m_io)
        return false;

    std::string err;
    if (!m_io->queryStatus3("@ST#", b1, b2, b3, 2000, err))
    {
        LOGF_WARN("Status read failed (@ST#): %s", err.c_str());
        return false;
    }
    return true;
}

void MXHDTelescope::decodeStatus(uint8_t b1, uint8_t b2, uint8_t b3)
{
    m_st1 = b1;
    m_st2 = b2;
    m_st3 = b3;

    m_isHoming = (b3 & 0x80) != 0;
    const bool slewBit = (b3 & 0x40) != 0;
    m_isSlewing = slewBit || m_isHoming;

    m_voltageV = static_cast<double>(b2) / 10.0;

    if (m_isHoming || m_isSlewing)
        TrackState = m_parkCommandActive ? SCOPE_PARKING : SCOPE_SLEWING;
    else if (isParked())
        TrackState = SCOPE_PARKED;
    else
        TrackState = m_trackingEnabled ? SCOPE_TRACKING : SCOPE_IDLE;
}

bool MXHDTelescope::mxhdQueryRaDec(double &raHours, double &decDeg)
{
    std::string raRaw, decRaw;
    if (!mxhdQueryHash(":GR#", raRaw))
        return false;
    if (!mxhdQueryHash(":GD#", decRaw))
        return false;

    if (!parseRaHours(raRaw, raHours))
    {
        LOGF_ERROR("Invalid RA response: '%s'", raRaw.c_str());
        return false;
    }
    if (!parseDecDeg(decRaw, decDeg))
    {
        LOGF_ERROR("Invalid Dec response: '%s'", decRaw.c_str());
        return false;
    }
    return true;
}

bool MXHDTelescope::mxhdSetTargetRaDec(double raHours, double decDeg)
{
    // LX200: :Sr HH:MM:SS# and :Sd sDD*MM:SS#
    const std::string ra = ":Sr" + formatRaSr(raHours) + "#";
    const std::string dec = ":Sd" + formatDecSd(decDeg) + "#";

    char ack = 0;
    if (!mxhdQueryAck(ra, ack))
        return false;
    if (ack != '1')
    {
        LOGF_WARN("Unexpected :Sr response: '%c'", ack);
        return false;
    }

    if (!mxhdQueryAck(dec, ack))
        return false;
    if (ack != '1')
    {
        LOGF_WARN("Unexpected :Sd response: '%c'", ack);
        return false;
    }
    return true;
}

bool MXHDTelescope::Goto(double ra, double dec)
{
    // INDI uses hours for RA, degrees for Dec.
    if (!mxhdSetTargetRaDec(ra, dec))
        return false;

    char ack = 0;
    if (!mxhdQueryAck(":MS#", ack))
        return false;

    // MX-HD / LX200-style slew ack: '0' on success.
    if (ack != '0')
        LOGF_WARN("Unexpected :MS# response: '%c'", ack);

    m_isSlewing = true;
    TrackState = SCOPE_SLEWING;
    return true;
}

bool MXHDTelescope::Sync(double ra, double dec)
{
    if (!mxhdSetTargetRaDec(ra, dec))
        return false;

    std::string resp;
    if (!mxhdQueryHash(":CM#", resp))
        return false;

    // Old LX200 returns "Unknown name" on sync sometimes; treat any response as non-fatal.
    NewRaDec(ra, dec);
    return true;
}

bool MXHDTelescope::Park()
{
    // MX-HD Park: @Hm#
    if (!mxhdSend("@Hm#"))
        return false;

    SetParked(false);
    m_parkCommandActive = true;
    m_unparkCommandActive = false;
    m_homeCommandActive = false;
    m_isSlewing = true;
    TrackState = SCOPE_PARKING;
    return true;
}

bool MXHDTelescope::UnPark()
{
    if (!isParked())
    {
        LOG_INFO("Mount is already unparked.");
        return true;
    }

    // MX-HD restores a valid solved pose for remote operation by returning to HOME first.
    if (!mxhdSend("@OG#"))
        return false;

    m_parkCommandActive = false;
    m_unparkCommandActive = true;
    m_homeCommandActive = false;
    m_isSlewing = true;
    m_isHoming = true;
    TrackState = SCOPE_SLEWING;
    return true;
}

bool MXHDTelescope::SetSlewRate(int index)
{
    switch (index)
    {
        case SLEW_GUIDE:
            return mxhdSend(":RG#");

        case SLEW_CENTERING:
            return mxhdSend(":RC#");

        case SLEW_FIND:
            return mxhdSend(":RM#");

        case SLEW_MAX:
            return mxhdSend(":RS#");

        default:
            LOGF_WARN("Unsupported slew rate index %d requested by client.", index);
            return false;
    }
}

bool MXHDTelescope::SetTrackEnabled(bool enabled)
{
    // MX-HD tracking on/off: @FD1# / @FD0#
    if (!mxhdSend(enabled ? "@FD1#" : "@FD0#"))
        return false;
    m_trackingEnabled = enabled;
    return true;
}

bool MXHDTelescope::SetTrackMode(uint8_t mode)
{
    TrackRateMode selectedMode = TrackRateMode::Sidereal;

    switch (mode)
    {
        case TRACK_SIDEREAL:
            selectedMode = TrackRateMode::Sidereal;
            break;

        case TRACK_SOLAR:
            selectedMode = TrackRateMode::Solar;
            break;

        case TRACK_LUNAR:
            selectedMode = TrackRateMode::Lunar;
            break;

        default:
            LOGF_WARN("Unsupported track mode index %u requested by client.", mode);
            return false;
    }

    if (!mxhdSetTrackRate(selectedMode))
        return false;

    m_trackRateMode = selectedMode;
    return true;
}

bool MXHDTelescope::mxhdSetTrackRate(TrackRateMode mode)
{
    switch (mode)
    {
        case TrackRateMode::Sidereal:
            return mxhdSend("@CE0#");
        case TrackRateMode::Solar:
            return mxhdSend("@LP1#");
        case TrackRateMode::Lunar:
            return mxhdSend("@LP4#");
        default:
            return false;
    }
}

bool MXHDTelescope::Abort()
{
    // Abort has mount-specific behavior:
    // - If homing is active, :Q# does not stop it reliably; we use @ME0# and delayed finalize.
    // - If slewing bit is set, :Q# can work.
    uint8_t b1 = 0, b2 = 0, b3 = 0;
    if (mxhdReadStatus(b1, b2, b3))
        decodeStatus(b1, b2, b3);

    m_abortRequestedAt = ::time(nullptr);
    m_abortSawHomeCleared = false;
    m_abortHomeClearedAt = 0;

    if (m_isHoming)
    {
        // Home abort: @ME0# then after (home=0 && abort+180s) -> @FD0# then 1s -> @ME1#
        if (!mxhdSend("@ME0#"))
            return false;
        if (HomeSP.s == IPS_BUSY)
        {
            HomeSP.s = IPS_ALERT;
            IDSetSwitch(&HomeSP, nullptr);
        }
        m_unparkCommandActive = false;
        m_homeCommandActive = false;
        startAbortFinalize(180);
        return true;
    }

    if (m_isSlewing)
    {
        std::string resp;
        if (!mxhdQueryHash(":Q#", resp))
            return false;
        m_unparkCommandActive = false;
        m_homeCommandActive = false;
        return true;
    }

    // Not slewing & not homing: do a safe stop sequence.
    if (!mxhdSend("@ME0#"))
        return false;
    m_unparkCommandActive = false;
    m_homeCommandActive = false;
    startAbortFinalize(90);
    return true;
}

void MXHDTelescope::startAbortFinalize(uint32_t delaySeconds)
{
    m_abortFinalizeActive = true;
    m_abortDelaySeconds = delaySeconds;
    // Keep mount "busy" from the client perspective while abort finalization runs.
    // (No direct INDI state for "aborting", so we will just keep things consistent in ReadScopeStatus.)
}

void MXHDTelescope::maybeAdvanceAbortFinalize()
{
    if (!m_abortFinalizeActive)
        return;

    // Update status first.
    uint8_t b1 = 0, b2 = 0, b3 = 0;
    if (!mxhdReadStatus(b1, b2, b3))
        return;
    decodeStatus(b1, b2, b3);

    const time_t now = ::time(nullptr);

    if (!m_abortSawHomeCleared)
    {
        if (!m_isHoming)
        {
            m_abortSawHomeCleared = true;
            m_abortHomeClearedAt = now;
        }
        // Wait until home clears at least once.
        return;
    }

    if (now < (m_abortRequestedAt + static_cast<time_t>(m_abortDelaySeconds)))
        return;

    // Finalize: @FD0# then after 1s @ME1#
    (void)mxhdSend("@FD0#");
    // Sleep is acceptable here because TimerHit runs in driver thread and this is rare; keep it short.
    usleep(1000 * 1000);
    (void)mxhdSend("@ME1#");

    m_abortFinalizeActive = false;
}

bool MXHDTelescope::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (command == MOTION_START)
    {
        // LX200 manual move: :Mn# (north) / :Ms# (south)
        return mxhdSend(dir == DIRECTION_NORTH ? ":Mn#" : ":Ms#");
    }
    else
    {
        // Stop N/S: :Qn# / :Qs#
        return mxhdSend(dir == DIRECTION_NORTH ? ":Qn#" : ":Qs#");
    }
}

bool MXHDTelescope::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (command == MOTION_START)
    {
        // LX200 manual move: :Me# (east) / :Mw# (west)
        return mxhdSend(dir == DIRECTION_EAST ? ":Me#" : ":Mw#");
    }
    else
    {
        // Stop E/W: :Qe# / :Qw#
        return mxhdSend(dir == DIRECTION_EAST ? ":Qe#" : ":Qw#");
    }
}

IPState MXHDTelescope::startTimedGuidePulse(GuideAxis axis, int direction, uint32_t ms, const char *moveCmd)
{
    if (ms == 0)
        return IPS_OK;

    if (!isConnected() || moveCmd == nullptr)
        return IPS_ALERT;

    uint64_t token = 0;
    bool needStopFirst = false;
    {
        std::lock_guard<std::mutex> lock(m_guideMutex);
        needStopFirst = m_guideActive;
        token = ++m_guideTokenGen;
        m_guideActive = true;
        m_guideAxis = axis;
        m_guideDirection = direction;
    }

    if (needStopFirst && !mxhdSend(":Q#"))
    {
        std::lock_guard<std::mutex> lock(m_guideMutex);
        if (token == m_guideTokenGen.load())
        {
            m_guideActive = false;
            m_guideDirection = 0;
        }
        return IPS_ALERT;
    }

    if (!mxhdSend(moveCmd))
    {
        std::lock_guard<std::mutex> lock(m_guideMutex);
        if (token == m_guideTokenGen.load())
        {
            m_guideActive = false;
            m_guideDirection = 0;
        }
        return IPS_ALERT;
    }

    std::thread([this, axis, token, ms]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        completeTimedGuidePulse(axis, token, true);
    }).detach();

    return IPS_BUSY;
}

void MXHDTelescope::setGuidePropertyIdle(GuideAxis axis, IPState state)
{
    if (axis == GuideAxis::RA)
    {
        GuideWEN[0].value = 0;
        GuideWEN[1].value = 0;
        GuideWENP.s = state;
        IDSetNumber(&GuideWENP, nullptr);
    }
    else
    {
        GuideNSN[0].value = 0;
        GuideNSN[1].value = 0;
        GuideNSNP.s = state;
        IDSetNumber(&GuideNSNP, nullptr);
    }
}

void MXHDTelescope::completeTimedGuidePulse(GuideAxis axis, uint64_t token, bool sendStop)
{
    bool shouldStop = false;
    {
        std::lock_guard<std::mutex> lock(m_guideMutex);
        if (token != m_guideTokenGen.load() || !m_guideActive || axis != m_guideAxis)
            return;

        m_guideActive = false;
        m_guideDirection = 0;
        ++m_guideTokenGen;
        shouldStop = sendStop && isConnected();
    }

    if (shouldStop)
        (void)mxhdSend(":Q#");

    setGuidePropertyIdle(axis, IPS_OK);
}

IPState MXHDTelescope::GuideNorth(uint32_t ms)
{
    return startTimedGuidePulse(GuideAxis::DEC, +1, ms, "@mN#");
}

IPState MXHDTelescope::GuideSouth(uint32_t ms)
{
    return startTimedGuidePulse(GuideAxis::DEC, -1, ms, "@mS#");
}

IPState MXHDTelescope::GuideEast(uint32_t ms)
{
    return startTimedGuidePulse(GuideAxis::RA, +1, ms, "@mE#");
}

IPState MXHDTelescope::GuideWest(uint32_t ms)
{
    return startTimedGuidePulse(GuideAxis::RA, -1, ms, "@mW#");
}

bool MXHDTelescope::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && std::strcmp(dev, getDeviceName()) != 0)
        return false;

    if (std::strcmp(name, HomeSP.name) == 0)
    {
        IUUpdateSwitch(&HomeSP, states, names, n);
        if (HomeS[0].s == ISS_ON)
        {
            if (!mxhdSend("@OG#"))
            {
                HomeSP.s = IPS_ALERT;
            }
            else
            {
                HomeSP.s = IPS_BUSY;
                m_homeCommandActive = true;
                m_unparkCommandActive = false;
                m_isSlewing = true;
                m_isHoming = true;
                TrackState = SCOPE_SLEWING;
            }
            HomeS[0].s = ISS_OFF;
            IDSetSwitch(&HomeSP, nullptr);
        }
        return true;
    }

    if (std::strcmp(name, "SIMULATE_PIER_SIDE") == 0)
    {
        setSimulatePierSide(true);
        LOG_INFO("MX-HD uses HA-based pier side, so SIMULATE_PIER_SIDE is forced to enabled.");
        return true;
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool MXHDTelescope::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && std::strcmp(dev, getDeviceName()) != 0)
        return false;

    if (std::strcmp(name, GuideNSNP.name) == 0)
    {
        IUUpdateNumber(&GuideNSNP, values, names, n);
        const uint32_t northMs = static_cast<uint32_t>(GuideNSN[0].value < 0 ? 0 : GuideNSN[0].value);
        const uint32_t southMs = static_cast<uint32_t>(GuideNSN[1].value < 0 ? 0 : GuideNSN[1].value);

        if (northMs > 0 && southMs > 0)
        {
            setGuidePropertyIdle(GuideAxis::DEC, IPS_ALERT);
            return true;
        }

        if (northMs > 0)
            GuideNSNP.s = GuideNorth(northMs);
        else if (southMs > 0)
            GuideNSNP.s = GuideSouth(southMs);
        else
            GuideNSNP.s = IPS_IDLE;

        if (GuideNSNP.s != IPS_BUSY)
            setGuidePropertyIdle(GuideAxis::DEC, GuideNSNP.s);
        else
            IDSetNumber(&GuideNSNP, nullptr);
        return true;
    }

    if (std::strcmp(name, GuideWENP.name) == 0)
    {
        IUUpdateNumber(&GuideWENP, values, names, n);
        const uint32_t westMs = static_cast<uint32_t>(GuideWEN[0].value < 0 ? 0 : GuideWEN[0].value);
        const uint32_t eastMs = static_cast<uint32_t>(GuideWEN[1].value < 0 ? 0 : GuideWEN[1].value);

        if (westMs > 0 && eastMs > 0)
        {
            setGuidePropertyIdle(GuideAxis::RA, IPS_ALERT);
            return true;
        }

        if (westMs > 0)
            GuideWENP.s = GuideWest(westMs);
        else if (eastMs > 0)
            GuideWENP.s = GuideEast(eastMs);
        else
            GuideWENP.s = IPS_IDLE;

        if (GuideWENP.s != IPS_BUSY)
            setGuidePropertyIdle(GuideAxis::RA, GuideWENP.s);
        else
            IDSetNumber(&GuideWENP, nullptr);
        return true;
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool MXHDTelescope::ReadScopeStatus()
{
    // Status bytes
    uint8_t b1 = 0, b2 = 0, b3 = 0;
    if (mxhdReadStatus(b1, b2, b3))
        decodeStatus(b1, b2, b3);

    // Abort finalization sequence advance (if any)
    maybeAdvanceAbortFinalize();

    if (m_abortFinalizeActive)
        TrackState = SCOPE_SLEWING;
    else if (m_isHoming || m_isSlewing)
        TrackState = m_parkCommandActive ? SCOPE_PARKING : SCOPE_SLEWING;
    else if (isParked())
        TrackState = SCOPE_PARKED;
    else
        TrackState = m_trackingEnabled ? SCOPE_TRACKING : SCOPE_IDLE;

    // During slew/home motions MX-HD can temporarily stop answering :GR#/:GD#.
    // Avoid polling coordinates until the mount returns to an idle/tracking state.
    if (!m_abortFinalizeActive && !m_isHoming && !m_isSlewing)
    {
        double ra = 0.0, dec = 0.0;
        if (mxhdQueryRaDec(ra, dec))
        {
            // Push latest coordinates through the INDI telescope base helper.
            NewRaDec(ra, dec);
            mxhdUpdatePierSide(ra);
        }
    }

    // Park state: when mount becomes idle after a park command, mark parked.
    if (m_parkCommandActive && !m_isSlewing && !m_isHoming)
    {
        SetParked(true);
        m_parkCommandActive = false;
        TrackState = SCOPE_PARKED;
    }

    // Unpark is modeled as "go home, then clear parked" so the mount regains a known pose.
    if (m_unparkCommandActive && !m_isSlewing && !m_isHoming)
    {
        SetParked(false);
        m_unparkCommandActive = false;
        TrackState = m_trackingEnabled ? SCOPE_TRACKING : SCOPE_IDLE;
    }

    // Home state: treat end of homing as OK.
    if (m_homeCommandActive && HomeSP.s == IPS_BUSY && !m_isHoming && !m_isSlewing)
    {
        m_homeCommandActive = false;
        HomeSP.s = IPS_OK;
        IDSetSwitch(&HomeSP, nullptr);
    }

    return true;
}

void MXHDTelescope::TimerHit()
{
    if (!isConnected())
        return;

    ReadScopeStatus();
    SetTimer(getCurrentPollingPeriod());
}

// INDI boilerplate entry points
static std::unique_ptr<MXHDTelescope> s_telescope;

void ISGetProperties(const char *dev)
{
    if (!s_telescope)
        s_telescope.reset(new MXHDTelescope());
    s_telescope->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (s_telescope)
        s_telescope->ISNewSwitch(dev, name, states, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (s_telescope)
        s_telescope->ISNewNumber(dev, name, values, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (s_telescope)
        s_telescope->ISNewText(dev, name, texts, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    if (s_telescope)
        s_telescope->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice(XMLEle *root)
{
    if (s_telescope)
        s_telescope->ISSnoopDevice(root);
}
