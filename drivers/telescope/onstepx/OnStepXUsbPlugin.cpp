/*
    OnStepX INDI Driver — Auxiliary feature helper
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
*/

#include "OnStepXUsbPlugin.h"
#include "OnStepXComm.h"

#include <defaultdevice.h>
#include <indilogger.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define USB_TAB      "USB Ports"

// ---------------------------------------------------------------------------
// discoverAndDefine — probe each active port and create INDI properties
// ---------------------------------------------------------------------------
void OnStepXUsbPlugin::discoverAndDefine(uint8_t portsMask)
{
    probe(portsMask);
    defineAll();
}

void OnStepXUsbPlugin::probe(uint8_t portsMask)
{
    for (auto &port : m_ports)
        port = Port{};           // reset from previous connection

    for (int i = 0; i < 8; i++)
    {
        if (!(portsMask & (1u << i)))
            continue;

        Port &port = m_ports[i];
        port.index = i + 1;
        if (!probePort(i + 1, port))
        {
            LOGF_WARN("Could not probe USB port %d, skipping", i + 1);
            continue;
        }
        port.active = true;
        LOGF_DEBUG("USB port %d: '%s'", i + 1, port.label);
    }
}

void OnStepXUsbPlugin::defineAll()
{
    if (m_external)
        return;
    for (auto &port : m_ports)
        if (port.active)
            definePort(port);
}

std::vector<int> OnStepXUsbPlugin::portIndices() const
{
    std::vector<int> out;
    for (const auto &port : m_ports)
        if (port.active)
            out.push_back(port.index);
    return out;
}

const char *OnStepXUsbPlugin::portLabel(int portIndex) const
{
    return (portIndex >= 1 && portIndex <= 8) ? m_ports[portIndex - 1].label : "";
}

bool OnStepXUsbPlugin::writePort(int portIndex, bool on)
{
    if (portIndex < 1 || portIndex > 8 || !m_ports[portIndex - 1].active)
        return false;
    if (!sendWriteInt(portIndex, 'V', on ? 1 : 0))
        return false;
    m_ports[portIndex - 1].value = on ? 1 : 0;
    return true;
}

int OnStepXUsbPlugin::portValue(int portIndex) const
{
    return (portIndex >= 1 && portIndex <= 8) ? m_ports[portIndex - 1].value : -1;
}

// ---------------------------------------------------------------------------
// deleteAll — remove all slot properties on disconnect
// ---------------------------------------------------------------------------
void OnStepXUsbPlugin::deleteAll()
{
    if (m_external)
        return;   // nothing defined here; the owner keeps the port mapping
    for (auto &port : m_ports)
    {
        if (port.active)
            deletePort(port);
    }
}

// ---------------------------------------------------------------------------
// handleSwitch
// ---------------------------------------------------------------------------
bool OnStepXUsbPlugin::handleSwitch(const char *name, ISState *states, char *names[], int n)
{
    for (auto &port : m_ports)
    {
        if (!port.active)
            continue;

        if (!m_external && port.switchSP.isNameMatch(name))
        {
            port.switchSP.update(states, names, n);
            int val = (port.switchSP[1].getState() == ISS_ON) ? 1 : 0;
            if (sendWriteInt(port.index, 'V', val))
                port.switchSP.setState(IPS_OK);
            else
                port.switchSP.setState(IPS_ALERT);
            port.switchSP.apply();
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// pollStatus — refresh current values from :GUX[n]#
// ---------------------------------------------------------------------------
void OnStepXUsbPlugin::pollStatus()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];

    for (auto &port : m_ports)
    {
        if (!port.active)
            continue;

        char cmd[OnStepXComm::CMD_MAX_LEN];
        snprintf(cmd, sizeof(cmd), ":GUX%d#", port.index);
        // "0#"/"1#".  Quiet: plugin builds that forget numericReply=false
        // answer a bare '1' here, which would otherwise log every poll.
        if (!m_comm->sendText(cmd, reply, 2000, true))
            continue;

        char *end;
        long val = std::strtol(reply, &end, 10);
        if (end == reply)
            continue;

        port.value = val != 0 ? 1 : 0;
        if (m_external)
            continue;
        port.switchSP[0].setState(val == 0 ? ISS_ON : ISS_OFF);
        port.switchSP[1].setState(val != 0 ? ISS_ON : ISS_OFF);
        port.switchSP.setState(IPS_OK);
        port.switchSP.apply();

    }
}

// ---------------------------------------------------------------------------
// saveConfig
// ---------------------------------------------------------------------------
void OnStepXUsbPlugin::saveConfig(FILE *fp)
{
    for (auto &port : m_ports)
    {
        if (!port.active || m_external)
            continue;

        port.switchSP.save(fp);
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool OnStepXUsbPlugin::probePort(int idx, Port &port)
{
    char cmd[OnStepXComm::CMD_MAX_LEN], reply[OnStepXComm::REPLY_BUF_SIZE];
    snprintf(cmd, sizeof(cmd), ":GUY%d#", idx);
    if (!m_comm->sendText(cmd, reply))
        return false;

    snprintf(port.label, sizeof(port.label), "%s", reply);
    makePropBase(reply, idx, port.propBase, sizeof(port.propBase));

    return true;
}

void OnStepXUsbPlugin::definePort(Port &port)
{
    const char *dev = m_dev ? m_dev->getDeviceName() : "";
    char nameBuf[64];

    snprintf(nameBuf, sizeof(nameBuf), "%s_SW", port.propBase);
    port.switchSP[0].fill("OFF", "Off", ISS_ON);
    port.switchSP[1].fill("ON",  "On",  ISS_OFF);
    port.switchSP.fill(dev, nameBuf, port.label,
                       USB_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    m_dev->defineProperty(port.switchSP);
}

void OnStepXUsbPlugin::deletePort(Port &port)
{
    if (!m_dev)
        return;
    m_dev->deleteProperty(port.switchSP);
    port.active = false;
}

bool OnStepXUsbPlugin::sendWriteInt(int portIdx, char field, int value)
{
    char cmd[OnStepXComm::CMD_MAX_LEN];
    snprintf(cmd, sizeof(cmd), ":SUX%d,%c%d#", portIdx, field, value);
    return m_comm->sendBool(cmd);   // bare '1'/'0'
}

// Produce a sanitized INDI property name base from the firmware label.
// e.g. "USB A" -> "OSX_USB_A" (port index appended as fallback if label empty)
void OnStepXUsbPlugin::makePropBase(const char *label, int portIdx, char *out, int outLen)
{
    char tmp[64];
    // Copy, replace non-alphanumeric with '_', uppercase
    int j = 0;
    for (int i = 0; label[i] && j < (int)sizeof(tmp) - 1; i++)
    {
        char c = label[i];
        if (std::isalnum((unsigned char)c))
            tmp[j++] = static_cast<char>(std::toupper((unsigned char)c));
        else if (j > 0 && tmp[j - 1] != '_')
            tmp[j++] = '_';
    }
    // Trim trailing underscores
    while (j > 0 && tmp[j - 1] == '_')
        j--;
    tmp[j] = '\0';

    if (j == 0)
        snprintf(out, outLen, "OSX_USB%d", portIdx);
    else
        snprintf(out, outLen, "OSX_%s", tmp);
}
