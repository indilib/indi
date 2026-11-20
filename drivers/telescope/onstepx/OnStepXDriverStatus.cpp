/*
    OnStepX INDI Driver — Motor driver status helper (both binaries)
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

#include "OnStepXDriverStatus.h"
#include "OnStepXComm.h"

#include <indilogger.h>

#include <cstdio>
#include <cstring>

#define ONSTEPX_TAB "OnStepX"

// ---------------------------------------------------------------------------
// discoverAndDefine
// ---------------------------------------------------------------------------
void OnStepXDriverStatus::discoverAndDefine(uint16_t mask)
{
    m_count = 0;
    for (int axis = 1; axis <= 9; axis++)
        if (mask & (1u << (axis - 1)))
            m_axes[m_count++] = Axis{axis, Flags{}, false};
    if (m_count == 0)
        return;

    m_statusTP.resize(m_count);
    for (int i = 0; i < m_count; i++)
    {
        char name[16];
        snprintf(name, sizeof(name), "AXIS%d", m_axes[i].number);
        m_statusTP[i].fill(name, axisName(m_axes[i].number), "");
    }
    m_statusTP.fill(m_dev ? m_dev->getDeviceName() : "", "OSX_DRIVER_STATUS", "Motor Drivers",
                    ONSTEPX_TAB, IP_RO, 60, IPS_IDLE);
    if (m_dev)
        m_dev->defineProperty(m_statusTP);
    m_defined = true;
    poll();
}

void OnStepXDriverStatus::deleteAll()
{
    if (m_defined && m_dev)
        m_dev->deleteProperty(m_statusTP);
    m_defined = false;
    m_count   = 0;
}

// ---------------------------------------------------------------------------
// poll
// ---------------------------------------------------------------------------
void OnStepXDriverStatus::poll()
{
    if (!m_defined)
        return;

    bool anySerious = false;
    for (int i = 0; i < m_count; i++)
    {
        Axis &a = m_axes[i];
        char cmd[OnStepXComm::CMD_MAX_LEN];
        char reply[OnStepXComm::REPLY_BUF_SIZE];
        snprintf(cmd, sizeof(cmd), ":GXU%d#", a.number);

        Flags now;
        if (!m_comm->sendText(cmd, reply, 2000, true) || !parse(reply, now))
        {
            // A missed read (e.g. an ESP32 stall) keeps the last state shown.
            anySerious = anySerious || (a.known && a.flags.serious());
            continue;
        }
        report(a, now);
        m_statusTP[i].setText(describe(now).c_str());
        anySerious = anySerious || now.serious();
    }
    m_statusTP.setState(anySerious ? IPS_ALERT : IPS_OK);
    m_statusTP.apply();
}

void OnStepXDriverStatus::report(Axis &a, const Flags &now)
{
    const Flags before = a.known ? a.flags : Flags{};
    a.flags = now;
    a.known = true;

    const std::string was = problems(before), is = problems(now);
    if (is == was)
        return;

    if (is.empty())
        LOGF_INFO("%s motor driver: %s cleared.", axisName(a.number), was.c_str());
    else if (now.serious())
        LOGF_ERROR("%s motor driver reports: %s.", axisName(a.number), is.c_str());
    else
        LOGF_WARN("%s motor driver reports: %s.", axisName(a.number), is.c_str());
}

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------
bool OnStepXDriverStatus::parse(const char *reply, Flags &out)
{
    // Eight fields: the firmware writes each code or nothing, then a comma
    // (none after the last).  Match the codes, not the positions.
    if (!reply || !strchr(reply, ','))
        return false;

    out = Flags{};
    const char *p = reply;
    while (*p)
    {
        const char *end = strchr(p, ',');
        const size_t len = end ? static_cast<size_t>(end - p) : strlen(p);
        if (len == 2)
        {
            if      (!strncmp(p, "ST", 2)) out.standstill = true;
            else if (!strncmp(p, "OA", 2)) out.openA      = true;
            else if (!strncmp(p, "OB", 2)) out.openB      = true;
            else if (!strncmp(p, "GA", 2)) out.shortA     = true;
            else if (!strncmp(p, "GB", 2)) out.shortB     = true;
            else if (!strncmp(p, "OT", 2)) out.overTemp   = true;
            else if (!strncmp(p, "PW", 2)) out.preWarning = true;
            else if (!strncmp(p, "GF", 2)) out.fault      = true;
            else return false;
        }
        else if (len != 0)
            return false;
        if (!end)
            break;
        p = end + 1;
    }
    return true;
}

static void append(std::string &s, bool on, const char *text)
{
    if (!on)
        return;
    if (!s.empty())
        s += ", ";
    s += text;
}

std::string OnStepXDriverStatus::problems(const Flags &f)
{
    std::string s;
    append(s, f.shortA, "short to ground on output A");
    append(s, f.shortB, "short to ground on output B");
    append(s, f.overTemp, "over temperature");
    append(s, f.fault, "fault");
    append(s, f.preWarning, "temperature pre-warning");
    return s;
}

std::string OnStepXDriverStatus::describe(const Flags &f)
{
    std::string s = problems(f);
    append(s, f.openA, "open load A");
    append(s, f.openB, "open load B");
    append(s, f.standstill, "standstill");
    if (problems(f).empty())
        s = s.empty() ? "OK" : "OK, " + s;
    return s;
}

const char *OnStepXDriverStatus::axisName(int axis)
{
    static const char *names[] =
    {
        "Axis 1 (RA/Azm)", "Axis 2 (Dec/Alt)", "Axis 3 (Rotator)",
        "Axis 4 (Focuser 1)", "Axis 5 (Focuser 2)", "Axis 6 (Focuser 3)",
        "Axis 7 (Focuser 4)", "Axis 8 (Focuser 5)", "Axis 9 (Focuser 6)"
    };
    return (axis >= 1 && axis <= 9) ? names[axis - 1] : "Axis ?";
}
