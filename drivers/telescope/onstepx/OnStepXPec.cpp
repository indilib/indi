/*
    OnStepX INDI Driver — PEC helper
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
*/

#include "OnStepXPec.h"
#include "OnStepXComm.h"

#include <defaultdevice.h>
#include <indilogger.h>

#include <cstdlib>
#include <cstring>

#define PEC_TAB "PEC"

// ---------------------------------------------------------------------------
// Momentary actions: element name -> command.  :$QZ...# commands reply
// nothing, even when refused (e.g. Record while not tracking).
// ---------------------------------------------------------------------------
namespace
{
struct PecAction
{
    const char *element;
    const char *command;
};
const PecAction PEC_ACTIONS[] =
{
    { "PLAY",         ":$QZ+#" },
    { "STOP",         ":$QZ-#" },
    { "READY_RECORD", ":$QZ/#" },
    { "CLEAR",        ":$QZZ#" },
    { "SAVE",         ":$QZ!#" },
};
}

// ---------------------------------------------------------------------------
// initProperties
// ---------------------------------------------------------------------------
void OnStepXPec::initProperties()
{
    const char *dev = m_dev ? m_dev->getDeviceName() : "";

    // State lights
    m_stateSP[0].fill("IGNORED",      "Ignored", IPS_IDLE);
    m_stateSP[1].fill("READY_PLAY",   "Ready to Play", IPS_IDLE);
    m_stateSP[2].fill("PLAYING",      "Playing", IPS_IDLE);
    m_stateSP[3].fill("READY_RECORD", "Ready to Record", IPS_IDLE);
    m_stateSP[4].fill("RECORDING",    "Recording", IPS_IDLE);
    m_stateSP.fill(dev, "OSX_PEC_STATE", "PEC State", PEC_TAB, IPS_IDLE);

    // Index detection lights
    m_indexSP[0].fill("INDEX_NO",  "Not Detected", IPS_IDLE);
    m_indexSP[1].fill("INDEX_YES", "Detected", IPS_IDLE);
    m_indexSP.fill(dev, "OSX_PEC_INDEX", "Worm Index", PEC_TAB, IPS_IDLE);

    // Momentary actions, split so each property has at most four elements:
    // KStars shows a switch with more than four as a drop-down list.
    m_controlSP[0].fill("PLAY",         "Play",   ISS_OFF);
    m_controlSP[1].fill("STOP",         "Stop",   ISS_OFF);
    m_controlSP[2].fill("READY_RECORD", "Record", ISS_OFF);
    m_controlSP.fill(dev, "OSX_PEC_CONTROL", "PEC Control", PEC_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    m_dataSP[0].fill("CLEAR", "Clear", ISS_OFF);
    m_dataSP[1].fill("SAVE",  "Save",  ISS_OFF);
    m_dataSP.fill(dev, "OSX_PEC_DATA", "PEC Data", PEC_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Worm steps
    m_wormNP[0].fill("WORM_STEPS", "Steps/Revolution", "%.0f", 0, 999999, 1, 0);
    m_wormNP.fill(dev, "OSX_PEC_WORM_STEPS", "Worm Period", PEC_TAB, IP_RO, 60, IPS_IDLE);
}

// ---------------------------------------------------------------------------
// updateProperties
// ---------------------------------------------------------------------------
void OnStepXPec::updateProperties(bool connected)
{
    if (connected)
    {
        m_dev->defineProperty(m_stateSP);
        m_dev->defineProperty(m_indexSP);
        m_dev->defineProperty(m_controlSP);
        m_dev->defineProperty(m_dataSP);
        m_dev->defineProperty(m_wormNP);
        readWormSteps();
    }
    else
    {
        m_dev->deleteProperty(m_stateSP);
        m_dev->deleteProperty(m_indexSP);
        m_dev->deleteProperty(m_controlSP);
        m_dev->deleteProperty(m_dataSP);
        m_dev->deleteProperty(m_wormNP);
    }
}

// ---------------------------------------------------------------------------
// handleSwitch
// ---------------------------------------------------------------------------
bool OnStepXPec::handleSwitch(const char *name, ISState *states, char *names[], int n)
{
    if (m_controlSP.isNameMatch(name))
        runAction(m_controlSP, states, names, n);
    else if (m_dataSP.isNameMatch(name))
        runAction(m_dataSP, states, names, n);
    else
        return false;
    return true;
}

void OnStepXPec::runAction(INDI::PropertySwitch &sp, ISState *states, char *names[], int n)
{
    sp.update(states, names, n);
    auto on = sp.findOnSwitch();
    const char *cmd = nullptr;
    for (const auto &a : PEC_ACTIONS)
        if (on && on->isNameMatch(a.element))
            cmd = a.command;

    // The result shows in the status poll that follows.
    const bool ok = cmd && m_comm->sendNoReply(cmd);
    sp.reset();   // momentary — all off after sending
    sp.setState(ok ? IPS_OK : IPS_ALERT);
    sp.apply();
    if (cmd)
        pollStatus();
}

// ---------------------------------------------------------------------------
// saveConfig
// ---------------------------------------------------------------------------
void OnStepXPec::saveConfig(FILE * /*fp*/)
{
    // Nothing persistent for PEC; state is always read from firmware on connect
}

// ---------------------------------------------------------------------------
// pollStatus — :$QZ?# -> state + index
// ---------------------------------------------------------------------------
void OnStepXPec::pollStatus()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    if (!m_comm->sendText(":$QZ?#", reply))
        return;

    char state = reply[0];
    bool indexDetected = (reply[1] == '.');

    // Clear all state lights to IDLE first
    for (int i = 0; i < 5; i++)
        m_stateSP[i].setState(IPS_IDLE);

    switch (state)
    {
        case 'I':
            m_stateSP[0].setState(IPS_OK);
            m_stateSP.setState(IPS_IDLE);
            break;
        case 'p':
            m_stateSP[1].setState(IPS_BUSY);
            m_stateSP.setState(IPS_BUSY);
            break;
        case 'P':
            m_stateSP[2].setState(IPS_BUSY);
            m_stateSP.setState(IPS_BUSY);
            break;
        case 'r':
            m_stateSP[3].setState(IPS_BUSY);
            m_stateSP.setState(IPS_BUSY);
            break;
        case 'R':
            m_stateSP[4].setState(IPS_BUSY);
            m_stateSP.setState(IPS_BUSY);
            break;
        default:
            m_stateSP.setState(IPS_ALERT);
            break;
    }
    m_stateSP.apply();

    // Clear index lights
    m_indexSP[0].setState(IPS_IDLE);
    m_indexSP[1].setState(IPS_IDLE);
    if (indexDetected)
    {
        m_indexSP[1].setState(IPS_OK);
        m_indexSP.setState(IPS_OK);
    }
    else
    {
        m_indexSP[0].setState(IPS_IDLE);
        m_indexSP.setState(IPS_IDLE);
    }
    m_indexSP.apply();
}

// ---------------------------------------------------------------------------
// readWormSteps — :VW# -> worm period steps
// ---------------------------------------------------------------------------
void OnStepXPec::readWormSteps()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    if (!m_comm->sendText(":VW#", reply))
        return;

    char *end;
    long val = std::strtol(reply, &end, 10);
    if (end == reply || val <= 0)
        return;

    m_wormNP[0].setValue(static_cast<double>(val));
    m_wormNP.setState(IPS_OK);
    m_wormNP.apply();
}
