/*
    OnStepX INDI Driver
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

#include "OnStepXCore.h"
#include "OnStepXStatus.h"
#include "OnStepXFormat.h"

#include <defaultdevice.h>
#include <indilogger.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Helper: true when s is entirely a floating-point number.
// Firmware error replies (bare '0') never reach here: sendText() rejects them.
// ---------------------------------------------------------------------------
static bool isNumeric(const char *s)
{
    // Finite only: without a weather sensor :GX9A# can reply "nan".
    double v;
    return OnStepXFormat::parseNumber(s, v);
}

// 8-char "01010101" mask -> bit i set when char i is '1'.
static bool parseMask8(const char *reply, uint8_t &mask)
{
    if (strlen(reply) < 8)
        return false;
    mask = 0;
    for (int i = 0; i < 8; i++)
        if (reply[i] == '1')
            mask |= static_cast<uint8_t>(1u << i);
    return true;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void OnStepXCore::setDevice(INDI::DefaultDevice *dev)
{
    m_dev = dev;
    m_comm.setDevice(dev);
}

void OnStepXCore::setFd(int fd)
{
    m_comm.setFd(fd);
}

const Capabilities &OnStepXCore::caps() const
{
    return m_cap;
}

OnStepXComm &OnStepXCore::comm()
{
    return m_comm;
}

// ---------------------------------------------------------------------------
// Phase 1 — probeController()
// Runs for both binaries (mount and aux).  Read-only.
// Returns true only if the firmware identifies itself as OnStepX.
// ---------------------------------------------------------------------------
bool OnStepXCore::probeController()
{
    // Start from a clean slate so nothing survives from a previous connection.
    m_cap = Capabilities{};

    char reply[OnStepXComm::REPLY_BUF_SIZE];

    // :GVP# — product name (both OnStep and OnStepX return "On-Step").
    // Retried: the first exchange can fail on a controller that is still
    // booting, or one with junk left in its buffers; the USB serial link
    // also occasionally loses a reply byte ("n-Step").  A failed attempt
    // requests a resync, so the comm layer waits for the line to go quiet
    // before the next one.
    bool gotReply = false;
    bool identified = false;
    for (int attempt = 1; attempt <= GVP_ATTEMPTS && !identified; attempt++)
    {
        if (attempt > 1)
            LOGF_DEBUG("probeController: retrying :GVP# (attempt %d of %d)", attempt, GVP_ATTEMPTS);
        if (!m_comm.sendText(":GVP#", reply, 2000, true))
        {
            m_comm.requestResync();
            continue;
        }
        gotReply = true;
        identified = strcmp(reply, "On-Step") == 0;
        if (!identified)
        {
            LOGF_DEBUG("probeController: unexpected product string '%s'", reply);
            m_comm.requestResync();
        }
    }
    if (!identified)
    {
        if (gotReply)
            LOGF_WARN("probeController: unrecognised product string '%s'", reply);
        else
            LOG_ERROR("probeController: no response to :GVP#");
        return false;
    }

    // :GVN# — firmware version "M.mmp", e.g. "10.28q".  OnStepX starts at 10.
    if (!m_comm.sendText(":GVN#", reply))
    {
        LOG_ERROR("probeController: no response to :GVN#");
        return false;
    }
    snprintf(m_cap.firmwareVersion, sizeof(m_cap.firmwareVersion), "%s", reply);

    char *dotPos = nullptr;
    const long major = strtol(reply, &dotPos, 10);
    m_cap.isOnStepX = (dotPos != reply && major >= 10);
    if (!m_cap.isOnStepX)
    {
        LOGF_WARN("OnStep firmware v%s detected; OnStepX (v10+) required.", reply);
        return false;
    }

    // Remaining firmware strings (best-effort)
    if (m_comm.sendText(":GVD#", reply, 2000, true))
        snprintf(m_cap.firmwareDate, sizeof(m_cap.firmwareDate), "%s", reply);
    if (m_comm.sendText(":GVT#", reply, 2000, true))
        snprintf(m_cap.firmwareTime, sizeof(m_cap.firmwareTime), "%s", reply);
    if (m_comm.sendText(":GVC#", reply, 2000, true))
        snprintf(m_cap.configName, sizeof(m_cap.configName), "%s", reply);

    LOGF_INFO("OnStepX firmware: %s  date: %s %s  config: %s",
              m_cap.firmwareVersion, m_cap.firmwareDate,
              m_cap.firmwareTime, m_cap.configName);

    // :GX98# — 'R' rotator, 'D' rotator + de-rotator, 'N' none.
    if (m_comm.sendText(":GX98#", reply, 2000, true))
    {
        m_cap.hasRotator   = (reply[0] == 'R' || reply[0] == 'D');
        m_cap.hasDerotator = (reply[0] == 'D');
    }
    LOGF_DEBUG("hasRotator=%d  hasDerotator=%d", m_cap.hasRotator, m_cap.hasDerotator);

    // :F[n]a# — focuser slot n present.  Slots can be sparse (e.g. only Axis5
    // configured = slot 2), so probe all six.  Direct addressing does not
    // change the firmware's active focuser, unlike :FA[n]#.
    for (int n = 1; n <= 6; n++)
    {
        char cmd[OnStepXComm::CMD_MAX_LEN];
        snprintf(cmd, sizeof(cmd), ":F%da#", n);
        if (m_comm.sendBool(cmd, 2000, true))
            m_cap.focuserMask |= static_cast<uint8_t>(1u << (n - 1));
    }
    LOGF_DEBUG("focuserMask=0x%02X", m_cap.focuserMask);

    // :GXY0# — aux feature slots
    if (m_comm.sendText(":GXY0#", reply, 2000, true))
        parseMask8(reply, m_cap.featureMask);
    LOGF_DEBUG("featureMask=0x%02X", m_cap.featureMask);

    // :GUY0# — USB port slots (USB plugin)
    if (m_comm.sendText(":GUY0#", reply, 2000, true))
        parseMask8(reply, m_cap.portMask);
    LOGF_DEBUG("portMask=0x%02X", m_cap.portMask);

    // :GX9A# — temperature.  The firmware always answers (sensor value, or
    // the user-set value used for refraction when no sensor is fitted) and
    // offers no query for sensor presence, so readable == available.
    if (m_comm.sendText(":GX9A#", reply, 2000, true))
        m_cap.hasWeatherRead = isNumeric(reply);
    LOGF_DEBUG("hasWeatherRead=%d", m_cap.hasWeatherRead);

    // :Gv# — site elevation
    if (m_comm.sendText(":Gv#", reply, 2000, true))
        m_cap.hasElevation = isNumeric(reply);

    // :GX9F# — MCU temperature (bare '0' when the MCU has no sensor)
    if (m_comm.sendText(":GX9F#", reply, 2000, true))
        m_cap.hasMcuTemp = isNumeric(reply);

    LOGF_DEBUG("hasElevation=%d  hasMcuTemp=%d", m_cap.hasElevation, m_cap.hasMcuTemp);

    // :GXU[n]# — motor driver status for axis n (Axis.command.cpp).  A flag
    // list such as "ST,,,,,,," when the axis's driver reports status; a bare
    // '0' when it does not, or when there is no axis n.  Each bare reply costs
    // the comm layer's 200 ms bare-reply wait, so only axes that can exist are
    // asked: 1-2 (mount), 3 (rotator), 3 + n (focuser n).
    for (int axis = 1; axis <= 9; axis++)
    {
        if ((axis == 3 && !m_cap.hasRotator) || (axis >= 4 && !m_cap.hasFocuser(axis - 3)))
            continue;
        char cmd[OnStepXComm::CMD_MAX_LEN];
        snprintf(cmd, sizeof(cmd), ":GXU%d#", axis);
        if (m_comm.sendText(cmd, reply, 2000, true))
            m_cap.driverStatusMask |= static_cast<uint16_t>(1u << (axis - 1));
    }
    LOGF_DEBUG("driverStatusMask=0x%03X", m_cap.driverStatusMask);

    LOGF_INFO("Probe complete: rotator=%d  focusers=0x%02X  features=0x%02X  "
              "usb=0x%02X  weather=%d  elevation=%d  mcuTemp=%d  driverStatus=0x%03X",
              m_cap.hasRotator, m_cap.focuserMask, m_cap.featureMask, m_cap.portMask,
              m_cap.hasWeatherRead, m_cap.hasElevation, m_cap.hasMcuTemp, m_cap.driverStatusMask);
    return true;
}

bool OnStepXCore::isValidGwReply(const char *reply)
{
    return strlen(reply) == 3 && strchr("GPAL", reply[0]) && strchr("TN", reply[1]) &&
           strchr("PH10", reply[2]);
}

// ---------------------------------------------------------------------------
// Phase 2 — probeMount()
// Runs only for the mount binary (OnStepXMount::Handshake).  Read-only.
// Returns true only if a mount is present.
// ---------------------------------------------------------------------------
bool OnStepXCore::probeMount()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];

    // :GW# — "G|P|A|L" + 'T'/'N' + park/home/align char, always 3 characters
    // (Status.command.cpp).
    // Without a mount the command is unknown and the firmware replies a bare
    // '0' (sendText fails).  Retried like :GVP#: a lost byte ("T1" for "GT1")
    // must not make the mount look absent.
    bool gotReply = false;
    bool valid = false;
    for (int attempt = 1; attempt <= GVP_ATTEMPTS && !valid; attempt++)
    {
        if (attempt > 1)
            LOGF_DEBUG("probeMount: retrying :GW# (attempt %d of %d)", attempt, GVP_ATTEMPTS);
        if (!m_comm.sendText(":GW#", reply, 2000, true))
        {
            m_comm.requestResync();
            continue;
        }
        gotReply = true;
        valid = isValidGwReply(reply);
        if (!valid)
        {
            LOGF_DEBUG("probeMount: malformed :GW# reply '%s'", reply);
            m_comm.requestResync();
        }
    }
    if (!gotReply)
    {
        LOG_INFO("No mount detected. For mount-less OnStepX use indi_onstepx_aux instead.");
        m_cap.hasMount = false;
        return false;
    }

    switch (valid ? reply[0] : '\0')
    {
        case 'G':
            m_cap.mountType = MountType::GEM;
            break;
        case 'P':
            m_cap.mountType = MountType::FORK;
            break;
        case 'A':
            m_cap.mountType = MountType::ALTAZM;
            break;
        case 'L':
            m_cap.mountType = MountType::ALTALT;
            break;
        default:
            LOGF_ERROR("Unrecognised mount type in :GW# reply '%s'", reply);
            m_cap.hasMount = false;
            return false;
    }
    m_cap.hasMount = true;
    LOGF_INFO("Mount type: %c", reply[0]);

    // :Gu# — binary status: 9 bytes, each >= 0x80, then '#'.
    {
        uint8_t bin[OnStepXStatus::GU_REPLY_LEN];
        if (m_comm.sendReadN(":Gu#", bin, OnStepXStatus::GU_REPLY_LEN, 2000, true))
        {
            MountStatus probe;
            m_cap.hasBinaryStatus = OnStepXStatus::parseGuReply(bin, OnStepXStatus::GU_REPLY_LEN, probe);
        }
    }
    LOGF_DEBUG("hasBinaryStatus=%d", m_cap.hasBinaryStatus);

    // :GU# — PEC is compiled in (AXIS1_PEC) only if the status string carries
    // a PEC state char.  (:$QZ?# answers 'I' even when PEC is compiled out.)
    if (m_comm.sendText(":GU#", reply, 2000, true))
        m_cap.hasPec = (strpbrk(reply, "/,~;^") != nullptr);
    LOGF_DEBUG("hasPec=%d", m_cap.hasPec);

    // :h?# — "hasSense,offset1,offset2"
    if (m_comm.sendText(":h?#", reply, 2000, true))
        m_cap.hasHomeSense = (reply[0] == '1');
    LOGF_DEBUG("hasHomeSense=%d", m_cap.hasHomeSense);

    // :Gm# — pier side E/W/N
    if (m_comm.sendText(":Gm#", reply, 2000, true))
        m_cap.hasPierSide = (reply[0] == 'E' || reply[0] == 'W' || reply[0] == 'N');
    LOGF_DEBUG("hasPierSide=%d", m_cap.hasPierSide);

    // :GX94# — "<pier side>" with " N" appended when Transform::meridianFlips
    // is off (fork/AltAz without MOUNT_ALTERNATE_ORIENTATION).  :MN# and
    // :SX99# answer a bare '0' on such mounts, so Flip is not offered.
    if (m_comm.sendText(":GX94#", reply, 2000, true))
        m_cap.hasMeridianFlips = (strchr(reply, 'N') == nullptr);
    LOGF_DEBUG("hasMeridianFlips=%d", m_cap.hasMeridianFlips);

    LOGF_INFO("Mount probe complete: pec=%d  homeSense=%d  pierSide=%d  meridianFlips=%d  binaryStatus=%d",
              m_cap.hasPec, m_cap.hasHomeSense, m_cap.hasPierSide, m_cap.hasMeridianFlips,
              m_cap.hasBinaryStatus);
    return true;
}
