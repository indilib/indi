/*
    OnStepX INDI Driver — Power child device (both binaries)
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

#include "OnStepXPowerDevice.h"
#include "OnStepXVersion.h"
#include "OnStepXComm.h"

#include <indilogger.h>
#include <timer/inditimer.h>

#include <cstdio>
#include <cstring>

OnStepXPowerDevice::OnStepXPowerDevice(const char *parentName) : INDI::PowerInterface(this)
{
    snprintf(m_name, sizeof(m_name), "%s Power", parentName);
    setDeviceName(m_name);
    setVersion(ONSTEPX_VERSION_MAJOR, ONSTEPX_VERSION_MINOR);

    // Switch outputs and USB ports are PowerInterface channels here; the
    // helpers keep the protocol and still poll them.
    m_aux.setDevice(this);
    m_aux.setExternalSwitches(true);
    m_usb.setDevice(this);
    m_usb.setExternal(true);
}

const char *OnStepXPowerDevice::getDefaultName()
{
    return m_name;
}

// ---------------------------------------------------------------------------
// configure — probe the outputs (before the first initProperties)
// ---------------------------------------------------------------------------
void OnStepXPowerDevice::configure(OnStepXComm *comm, uint8_t featureMask, uint8_t portMask)
{
    m_comm = comm;
    m_featureMask = featureMask;
    m_portMask = portMask;
    m_aux.setComm(comm);
    m_usb.setComm(comm);

    m_aux.probe(featureMask);
    m_usb.probe(portMask);

    const std::vector<int> dc = m_aux.switchSlots();
    const std::vector<int> usb = m_usb.portIndices();
    if (!m_initialised)
    {
        m_dcSlots = dc;
        m_usbPorts = usb;
    }
    else if (dc != m_dcSlots || usb != m_usbPorts)
    {
        // The PowerInterface channels were sized when the device was created.
        LOG_WARN("The controller's switch outputs or USB ports have changed; restart the driver to show them.");
    }
}

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------
bool OnStepXPowerDevice::initProperties()
{
    INDI::DefaultDevice::initProperties();

    uint32_t cap = 0;
    if (!m_dcSlots.empty())
        cap |= POWER_HAS_DC_OUT;
    if (!m_usbPorts.empty())
        cap |= POWER_HAS_USB_TOGGLE;
    PI::SetCapability(cap);
    PI::initProperties(MAIN_CONTROL_TAB, m_dcSlots.size(), 0, 0, 0, m_usbPorts.size());
    seedLabels();

    setDriverInterface(POWER_INTERFACE | AUX_INTERFACE);
    addAuxControls();
    m_initialised = true;
    return true;
}

// PowerInterface labels default to "Channel N" / "Port N" (and are loaded
// from the INDI config).  Where the default is still there, show the
// firmware's name for the output instead.
void OnStepXPowerDevice::seedLabels()
{
    auto seed = [](INDI::PropertyText & labels, INDI::PropertySwitch & sw, size_t i, const char *fmt,
                   const char *name)
    {
        char def[MAXINDILABEL];
        snprintf(def, sizeof(def), fmt, static_cast<int>(i + 1));
        if (name && *name && !strcmp(labels[i].getText(), def))
        {
            labels[i].setText(name);
            sw[i].setLabel(name);
        }
    };
    for (size_t i = 0; i < m_dcSlots.size(); i++)
        seed(PowerChannelLabelsTP, PowerChannelsSP, i, "Channel %d", m_aux.slotLabel(m_dcSlots[i]));
    for (size_t i = 0; i < m_usbPorts.size(); i++)
        seed(USBPortLabelsTP, USBPortSP, i, "Port %d", m_usb.portLabel(m_usbPorts[i]));
}

bool OnStepXPowerDevice::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    PI::updateProperties();

    if (isConnected() && m_comm)
    {
        m_aux.defineAll();   // analog outputs, dew heaters, intervalometers
        pollStatus();
    }
    else
    {
        m_aux.deleteAll();
    }
    return true;
}

bool OnStepXPowerDevice::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (PI::processSwitch(dev, name, states, names, n))
            return true;
        if (isConnected() && m_aux.handleSwitch(name, states, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool OnStepXPowerDevice::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (PI::processNumber(dev, name, values, names, n))
            return true;
        if (isConnected() && m_aux.handleNumber(name, values, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool OnStepXPowerDevice::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()) && PI::processText(dev, name, texts, names, n))
        return true;
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool OnStepXPowerDevice::saveConfigItems(FILE *fp)
{
    // Not PI::saveConfigItems(): it saves the DC output states, which the
    // firmware keeps in NV (feature values), so a config load would switch
    // outputs behind the firmware's back.  Keep the client-side labels, and
    // the USB port states: the USB plugin has no access to the firmware's NV,
    // so its ports would otherwise come up in their default state.
    INDI::DefaultDevice::saveConfigItems(fp);
    if (HasDCOutput())
        PowerChannelLabelsTP.save(fp);
    if (HasUSBPort())
    {
        USBPortSP.save(fp);
        USBPortLabelsTP.save(fp);
    }
    m_aux.saveConfig(fp);   // saves nothing: all NV
    return true;
}

// ---------------------------------------------------------------------------
// PowerInterface
// ---------------------------------------------------------------------------
bool OnStepXPowerDevice::SetPowerPort(size_t port, bool enabled)
{
    return port < m_dcSlots.size() && m_aux.writeSwitch(m_dcSlots[port], enabled);
}

bool OnStepXPowerDevice::SetUSBPort(size_t port, bool enabled)
{
    return port < m_usbPorts.size() && m_usb.writePort(m_usbPorts[port], enabled);
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------
bool OnStepXPowerDevice::Connect()
{
    if (!m_comm || !m_comm->isOpen())
    {
        LOG_ERROR("Power: parent device not connected. Connect the mount/controller first.");
        return false;
    }
    return true;
}

bool OnStepXPowerDevice::Disconnect()
{
    return true;
}

// ---------------------------------------------------------------------------
// Polling
// ---------------------------------------------------------------------------
void OnStepXPowerDevice::pollStatus()
{
    if (!isConnected() || !m_comm)
        return;
    if (m_featureMask)
        m_aux.pollStatus();   // also refreshes the custom output properties
    if (m_portMask)
        m_usb.pollStatus();
    syncChannels();
}

// Show the polled switch and USB values on the PowerInterface channels; the
// firmware state can change from the hand controller or web UI.
void OnStepXPowerDevice::syncChannels()
{
    auto sync = [](INDI::PropertySwitch & sw, const std::vector<int> &ids, auto value)
    {
        if (ids.empty())
            return;
        bool changed = false, known = false;
        for (size_t i = 0; i < ids.size(); i++)
        {
            const int v = value(ids[i]);
            if (v < 0)
                continue;
            known = true;
            const ISState s = v ? ISS_ON : ISS_OFF;
            if (sw[i].getState() != s)
            {
                sw[i].setState(s);
                changed = true;
            }
        }
        if (known && (changed || sw.getState() != IPS_OK))
        {
            sw.setState(IPS_OK);
            sw.apply();
        }
    };
    sync(PowerChannelsSP, m_dcSlots, [this](int slot)
    {
        return m_aux.switchValue(slot);
    });
    sync(USBPortSP, m_usbPorts, [this](int port)
    {
        return m_usb.portValue(port);
    });
}

// ---------------------------------------------------------------------------
// Child-device lifecycle (shared by OnStepXMount and OnStepXAux)
// ---------------------------------------------------------------------------
void OnStepXPowerDevice::connectChild(std::unique_ptr<OnStepXPowerDevice> &child, uint8_t featureMask,
                                      uint8_t portMask, OnStepXComm *comm, const char *parentName)
{
    if (!featureMask && !portMask)
        return;

    const bool isNew = !child;
    if (isNew)
        child = std::make_unique<OnStepXPowerDevice>(parentName);
    child->configure(comm, featureMask, portMask);

    if (isNew)
    {
        // Registers the device in the global list so clients see it as a
        // separate device in the same process; this runs initProperties().
        child->ISGetProperties(nullptr);
    }

    // No own port — the parent's connection is shared.
    child->setConnected(true, IPS_OK);

    if (isNew)
    {
        // Defer updateProperties so clients process newDevice first.
        OnStepXPowerDevice *raw = child.get();
        INDI::Timer::singleShot(1000, [raw]()
        {
            if (raw->isConnected())
                raw->updateProperties();
        });
    }
    else
    {
        child->updateProperties();
    }
}

void OnStepXPowerDevice::disconnectChild(std::unique_ptr<OnStepXPowerDevice> &child)
{
    if (child && child->isConnected())
    {
        child->setConnected(false, IPS_IDLE);
        child->updateProperties();
    }
}
