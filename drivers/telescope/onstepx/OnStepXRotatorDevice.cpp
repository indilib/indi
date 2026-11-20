/*
    OnStepX INDI Driver — Rotator child device (both binaries)
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

#include "OnStepXRotatorDevice.h"
#include "OnStepXVersion.h"
#include "OnStepXComm.h"

#include <indilogger.h>
#include <timer/inditimer.h>

#include <cstdio>
#include <cstring>

OnStepXRotatorDevice::OnStepXRotatorDevice(const char *parentName)
{
    snprintf(m_name, sizeof(m_name), "%s Rotator", parentName);
    setDeviceName(m_name);
    setVersion(ONSTEPX_VERSION_MAJOR, ONSTEPX_VERSION_MINOR);

    // No Serial/TCP connection — the parent's port is shared.
    setRotatorConnection(CONNECTION_NONE);

    // Without SetCapability, RotatorInterface never defines the abort, home,
    // backlash and sync properties.
    RI::SetCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_HOME | ROTATOR_CAN_SYNC | ROTATOR_HAS_BACKLASH);

    m_rotator.setDevice(this);
}

void OnStepXRotatorDevice::setComm(OnStepXComm *comm)
{
    m_comm = comm;
    m_rotator.setComm(comm);
}

const char *OnStepXRotatorDevice::getDefaultName()
{
    return m_name;
}

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------
bool OnStepXRotatorDevice::initProperties()
{
    // RotatorInterface properties go on Main Control, presets on their own tab.
    INDI::Rotator::initProperties();
    m_rotator.initProperties(m_hasDerotator);
    addAuxControls();
    return true;
}

bool OnStepXRotatorDevice::updateProperties()
{
    INDI::Rotator::updateProperties();

    if (isConnected() && m_comm)
    {
        m_rotator.updateProperties(true, m_hasDerotator);
        applyInitialState();
    }
    else
    {
        m_rotator.updateProperties(false, false);
    }
    return true;
}

// Current angle, and the backlash read from the firmware at connect.  The
// backlash toggle is shown on when a backlash is set, since turning
// ROTATOR_BACKLASH_TOGGLE off sends 0.
void OnStepXRotatorDevice::applyInitialState()
{
    const auto init = m_rotator.readInitial();
    if (init.angleValid)
    {
        GotoRotatorNP[0].setValue(init.angle);
        GotoRotatorNP.setState(IPS_OK);
        GotoRotatorNP.apply();
    }
    if (init.backlashValid)
    {
        RotatorBacklashNP[0].setValue(static_cast<double>(init.backlash));
        RotatorBacklashNP.setState(IPS_OK);
        RotatorBacklashNP.apply();
        RotatorBacklashSP.reset();
        RotatorBacklashSP[init.backlash > 0 ? INDI_ENABLED : INDI_DISABLED].setState(ISS_ON);
        RotatorBacklashSP.setState(IPS_OK);
        RotatorBacklashSP.apply();
    }
}

bool OnStepXRotatorDevice::saveConfigItems(FILE *fp)
{
    // Not INDI::Rotator::saveConfigItems(): RI::saveConfigItems() saves the
    // backlash, which the firmware keeps in NV.  Keep the client-side travel
    // limit and the presets.
    INDI::DefaultDevice::saveConfigItems(fp);
    RotatorLimitsNP.save(fp);
    PresetNP.save(fp);
    m_rotator.saveConfig(fp);
    return true;
}

bool OnStepXRotatorDevice::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()) && isConnected() && m_rotator.handleSwitch(name, states, names, n))
    {
        // e.g. go to the parallactic angle: show the angle Busy straight away.
        if (m_rotator.takeMoveStarted())
        {
            GotoRotatorNP.setState(IPS_BUSY);
            GotoRotatorNP.apply();
        }
        return true;
    }
    return INDI::Rotator::ISNewSwitch(dev, name, states, names, n);
}

bool OnStepXRotatorDevice::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()) && isConnected() && m_rotator.handleNumber(name, values, names, n))
        return true;
    return INDI::Rotator::ISNewNumber(dev, name, values, names, n);
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------
bool OnStepXRotatorDevice::Connect()
{
    if (!m_comm || !m_comm->isOpen())
    {
        LOG_ERROR("Rotator: parent device not connected. Connect the mount/controller first.");
        return false;
    }
    return true;
}

bool OnStepXRotatorDevice::Disconnect()
{
    return true;
}

// ---------------------------------------------------------------------------
// Polling
// ---------------------------------------------------------------------------
void OnStepXRotatorDevice::pollStatus()
{
    if (!isConnected())
        return;

    const auto r = m_rotator.pollStatus();
    if (r.angleValid)
        GotoRotatorNP[0].setValue(r.angle);
    if (r.statusValid)
        GotoRotatorNP.setState(r.moving ? IPS_BUSY : IPS_OK);
    if (r.angleValid || r.statusValid)
        GotoRotatorNP.apply();

    // Home (:rC#, half travel) ends when the rotator stops.
    if (r.statusValid && !r.moving && HomeRotatorSP.getState() == IPS_BUSY)
    {
        HomeRotatorSP.reset();
        HomeRotatorSP.setState(IPS_OK);
        HomeRotatorSP.apply();
    }
}

// ---------------------------------------------------------------------------
// RotatorInterface — delegate to the protocol helper
// ---------------------------------------------------------------------------
IPState OnStepXRotatorDevice::MoveRotator(double angle)
{
    return m_rotator.moveToAngle(angle);
}

bool OnStepXRotatorDevice::AbortRotator()
{
    return m_rotator.abortRotator();
}

IPState OnStepXRotatorDevice::HomeRotator()
{
    return m_rotator.homeRotator();
}

bool OnStepXRotatorDevice::SetRotatorBacklash(int32_t steps)
{
    return m_rotator.setBacklash(steps);
}

bool OnStepXRotatorDevice::SyncRotator(double angle)
{
    return m_rotator.syncAngle(angle);
}

// ---------------------------------------------------------------------------
// Child-device lifecycle (shared by OnStepXMount and OnStepXAux)
// ---------------------------------------------------------------------------
void OnStepXRotatorDevice::connectChild(std::unique_ptr<OnStepXRotatorDevice> &child, bool hasRotator,
                                        bool hasDerotator, OnStepXComm *comm, const char *parentName)
{
    if (!hasRotator)
        return;

    const bool isNew = !child;
    if (isNew)
    {
        child = std::make_unique<OnStepXRotatorDevice>(parentName);
        // Registers the device in the global list so clients see it as a
        // separate rotator in the same process; this runs initProperties().
        child->setComm(comm);
        child->setDerotator(hasDerotator);
        child->ISGetProperties(nullptr);
    }
    child->setComm(comm);
    child->setDerotator(hasDerotator);

    // No own port — the parent's connection is shared.
    child->setConnected(true, IPS_OK);

    if (isNew)
    {
        // Defer updateProperties so clients process newDevice first.
        OnStepXRotatorDevice *raw = child.get();
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

void OnStepXRotatorDevice::disconnectChild(std::unique_ptr<OnStepXRotatorDevice> &child)
{
    if (child && child->isConnected())
    {
        child->setConnected(false, IPS_IDLE);
        child->updateProperties();
    }
}
