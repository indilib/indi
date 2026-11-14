/*
    OnStepX INDI Driver — Focuser device
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

#include "OnStepXFocuser.h"
#include "OnStepXVersion.h"
#include "OnStepXFormat.h"

#include <indicontroller.h>
#include <timer/inditimer.h>

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

OnStepXFocuser::OnStepXFocuser(int slot, const char *parentName) : m_slot(slot)
{
    snprintf(m_name, sizeof(m_name), "%s Focuser %d", parentName, slot);

    setDeviceName(m_name);

    setVersion(ONSTEPX_VERSION_MAJOR, ONSTEPX_VERSION_MINOR);

    // No Serial/TCP connection — parent device activates us
    setSupportedConnections(CONNECTION_NONE);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE  |
                      FOCUSER_CAN_REL_MOVE  |
                      FOCUSER_CAN_ABORT     |
                      FOCUSER_CAN_SYNC      |
                      FOCUSER_HAS_BACKLASH);
}

// ---------------------------------------------------------------------------
// DefaultDevice interface
// ---------------------------------------------------------------------------
const char *OnStepXFocuser::getDefaultName()
{
    return m_name;
}

bool OnStepXFocuser::initProperties()
{
    // Focuser::initProperties -> DefaultDevice::initProperties registers the
    // Connection switch, then FI::initProperties registers focuser
    // properties.  CONNECTION_NONE: no Serial/TCP UI is added.
    INDI::Focuser::initProperties();

    // Sensible default until the firmware limits are read (100 mm).  The
    // travel limits are firmware settings (:F<n>I#/:F<n>M#), so a client
    // can't change the maximum here.
    FocusMaxPosNP[0].setValue(100000);
    FocusMaxPosNP.setPermission(IP_RO);

    m_temperatureNP[0].fill("FOCUS_TEMPERATURE", "Temperature (C)", "%.1f", -50, 100, 0, 0);
    m_temperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE_NP",
                         "Temperature", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    m_homeSP[0].fill("FOCUS_GOTO_HOME", "Go to Home",  ISS_OFF);
    m_homeSP[1].fill("FOCUS_SET_HOME",  "Set as Home", ISS_OFF);
    m_homeSP.fill(getDeviceName(), "OSX_FOCUS_HOME", "Home",
                  MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    m_tcfSP[0].fill("TCF_OFF", "Off", ISS_ON);
    m_tcfSP[1].fill("TCF_ON",  "On",  ISS_OFF);
    m_tcfSP.fill(getDeviceName(), "OSX_FOCUS_TCF", "Temp. Comp.",
                 OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    m_tcfNP[0].fill("TCF_COEFFICIENT", "Coefficient (um/C)", "%.3f", -999.999, 999.999, 1, 0);
    m_tcfNP[1].fill("TCF_DEADBAND",    "Deadband (steps)",   "%.0f", 1, 10000, 1, 1);
    m_tcfNP.fill(getDeviceName(), "OSX_FOCUS_TCF_SETTINGS", "Temp. Comp. Set",
                 OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    m_tcfDeltaNP[0].fill("TCF_DELTA", "Change (C)", "%.1f", -100, 100, 0, 0);
    m_tcfDeltaNP.fill(getDeviceName(), "OSX_FOCUS_TCF_DELTA", "Temp. Change",
                      OPTIONS_TAB, IP_RO, 60, IPS_IDLE);

    setDriverInterface(FOCUSER_INTERFACE);
    addAuxControls();
    return true;
}

bool OnStepXFocuser::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected() && m_comm)
    {
        defineProperty(m_homeSP);

        uint32_t minPos = 0, maxPos = 0;
        if (cmdGetLimits(minPos, maxPos))
        {
            FocusAbsPosNP[0].setMinMax(minPos, maxPos);
            FocusAbsPosNP.updateMinMax();
            FocusMaxPosNP[0].setValue(maxPos);
            FocusMaxPosNP.apply();
        }

        // :F<n>B# — backlash in microns.  Shown with the toggle on when set,
        // since turning FOCUS_BACKLASH_TOGGLE off sends 0.
        char reply[OnStepXComm::REPLY_BUF_SIZE];
        double backlash = 0;
        if (sendText("B", reply) && OnStepXFormat::parseNumber(reply, backlash))
        {
            FocusBacklashNP[0].setValue(backlash);
            FocusBacklashNP.setState(IPS_OK);
            FocusBacklashNP.apply();
            FocusBacklashSP.reset();
            FocusBacklashSP[backlash > 0 ? INDI_ENABLED : INDI_DISABLED].setState(ISS_ON);
            FocusBacklashSP.setState(IPS_OK);
            FocusBacklashSP.apply();
        }

        uint32_t curPos = 0;
        if (cmdGetPos(curPos))
        {
            FocusAbsPosNP[0].setValue(curPos);
            FocusAbsPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
        }

        double tempC = 0;
        m_hasTcf = cmdGetTemperature(tempC);
        if (m_hasTcf)
        {
            // Only with a temperature source: the firmware answers "nan"
            // without one, which would otherwise show as 0 C.
            m_temperatureNP[0].setValue(tempC);
            m_temperatureNP.setState(IPS_OK);
            defineProperty(m_temperatureNP);

            // Compensation needs a temperature reading.
            defineProperty(m_tcfSP);
            defineProperty(m_tcfNP);
            defineProperty(m_tcfDeltaNP);
            readTcf();
        }

        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        m_moving = false;
        deleteProperty(m_homeSP);
        if (m_hasTcf)
        {
            deleteProperty(m_temperatureNP);
            deleteProperty(m_tcfSP);
            deleteProperty(m_tcfNP);
            deleteProperty(m_tcfDeltaNP);
        }
        m_hasTcf = false;
    }

    return true;
}

bool OnStepXFocuser::saveConfigItems(FILE *fp)
{
    // Not INDI::Focuser::saveConfigItems(): its FocuserInterface part saves the
    // backlash and maximum position, which the firmware keeps in NV, and a
    // config reload on connect would overwrite changes made from the hand
    // controller or web UI.  Presets and joystick mappings are INDI-only.
    INDI::DefaultDevice::saveConfigItems(fp);
    PresetNP.save(fp);
    controller->saveConfigItems(fp);
    return true;
}

// ---------------------------------------------------------------------------
// Connection — no port; the parent calls setConnected() for us
// ---------------------------------------------------------------------------
bool OnStepXFocuser::Connect()
{
    if (!m_comm || !m_comm->isOpen())
    {
        LOG_ERROR("Focuser: parent device not connected. Connect the mount/controller first.");
        return false;
    }
    return true;
}

bool OnStepXFocuser::Disconnect()
{
    return true;
}

// ---------------------------------------------------------------------------
// TimerHit — check for move completion
// ---------------------------------------------------------------------------
void OnStepXFocuser::TimerHit()
{
    if (!isConnected())
        return;

    if (m_moving)
        pollMove();

    SetTimer(getCurrentPollingPeriod());
}

void OnStepXFocuser::pollMove()
{
    bool moving = false;
    if (!cmdGetStatus(moving))
        return;
    if (!moving)
    {
        finishMove();
        return;
    }
    uint32_t pos = 0;
    if (cmdGetPos(pos) && pos != FocusAbsPosNP[0].getValue())
    {
        FocusAbsPosNP[0].setValue(pos);
        FocusAbsPosNP.apply();
    }
}

void OnStepXFocuser::finishMove()
{
    m_moving = false;

    uint32_t pos = 0;
    if (cmdGetPos(pos))
        FocusAbsPosNP[0].setValue(pos);
    FocusAbsPosNP.setState(IPS_OK);
    FocusAbsPosNP.apply();

    if (FocusRelPosNP.getState() == IPS_BUSY)
    {
        FocusRelPosNP.setState(IPS_OK);
        FocusRelPosNP.apply();
    }
}

// ---------------------------------------------------------------------------
// pollStatus — called by the parent to refresh position and temperature
// ---------------------------------------------------------------------------
void OnStepXFocuser::pollStatus()
{
    if (!isConnected() || !m_comm)
        return;

    uint32_t pos = 0;
    if (!m_moving && cmdGetPos(pos) && pos != FocusAbsPosNP[0].getValue())
    {
        FocusAbsPosNP[0].setValue(pos);
        FocusAbsPosNP.setState(IPS_OK);
        FocusAbsPosNP.apply();
    }

    double tempC = 0;
    if (m_hasTcf && cmdGetTemperature(tempC))
    {
        m_temperatureNP[0].setValue(tempC);
        m_temperatureNP.setState(IPS_OK);
        m_temperatureNP.apply();
    }

    if (m_hasTcf)
    {
        // The hand controller or web UI may switch compensation too.
        const int on = sendBool("c") ? 1 : 0;
        if (m_tcfSP.findOnSwitchIndex() != on)
        {
            m_tcfSP.reset();
            m_tcfSP[on].setState(ISS_ON);
            m_tcfSP.setState(IPS_OK);
            m_tcfSP.apply();
        }
        readTcfDelta();
    }
}

// ---------------------------------------------------------------------------
// Custom properties: home and temperature compensation
// ---------------------------------------------------------------------------
bool OnStepXFocuser::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()) && isConnected())
    {
        if (m_homeSP.isNameMatch(name))
        {
            m_homeSP.update(states, names, n);
            const int idx = m_homeSP.findOnSwitchIndex();
            m_homeSP.reset();
            bool ok = false;
            if (idx == 0)
            {
                // :F<n>h# goes to the home position, or finds home with a sensor.
                ok = sendNoReply("h");
                if (ok)
                {
                    m_moving = true;
                    FocusAbsPosNP.setState(IPS_BUSY);
                    FocusAbsPosNP.apply();
                    LOG_INFO("Focuser going to home.");
                }
            }
            else if (idx == 1)
            {
                // :F<n>H# redefines the current position as the home position.
                ok = sendNoReply("H");
                uint32_t pos = 0;
                if (ok && cmdGetPos(pos))
                {
                    FocusAbsPosNP[0].setValue(pos);
                    FocusAbsPosNP.setState(IPS_OK);
                    FocusAbsPosNP.apply();
                    LOGF_INFO("Current position set as home (%u microns).", pos);
                }
            }
            m_homeSP.setState(ok ? IPS_OK : IPS_ALERT);
            m_homeSP.apply();
            return true;
        }

        if (m_hasTcf && m_tcfSP.isNameMatch(name))
        {
            m_tcfSP.update(states, names, n);
            const bool want = m_tcfSP[1].getState() == ISS_ON;
            const bool sent = sendBool(want ? "c1" : "c0");
            const bool on = sendBool("c");
            m_tcfSP.reset();
            m_tcfSP[on ? 1 : 0].setState(ISS_ON);
            if (!sent || on != want)
            {
                LOG_ERROR(want ? "Temperature compensation did not start (the focuser needs a temperature "
                          "reading and must not be parked)." : "Temperature compensation did not stop.");
                m_tcfSP.setState(IPS_ALERT);
            }
            else
                m_tcfSP.setState(IPS_OK);
            m_tcfSP.apply();
            readTcfDelta();
            return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool OnStepXFocuser::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()) && isConnected() && m_hasTcf && m_tcfNP.isNameMatch(name))
    {
        const double coef = m_tcfNP[0].getValue(), band = m_tcfNP[1].getValue();
        m_tcfNP.update(values, names, n);
        bool ok = true;
        char body[OnStepXComm::CMD_MAX_LEN];
        if (m_tcfNP[0].getValue() != coef)
        {
            snprintf(body, sizeof(body), "C%.3f", m_tcfNP[0].getValue());
            ok = sendBool(body) && ok;
        }
        if (m_tcfNP[1].getValue() != band)
        {
            snprintf(body, sizeof(body), "d%ld", std::lround(m_tcfNP[1].getValue()));
            ok = sendBool(body) && ok;
        }
        if (!ok)
            LOG_ERROR("Temperature compensation setting refused (coefficient under 1000 um/C, "
                      "deadband 1 to 10000 steps).");
        readTcf();
        if (!ok)
        {
            m_tcfNP.setState(IPS_ALERT);
            m_tcfNP.apply();
        }
        return true;
    }
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

// Reads the enable state, coefficient and deadband.
void OnStepXFocuser::readTcf()
{
    const int on = sendBool("c") ? 1 : 0;
    m_tcfSP.reset();
    m_tcfSP[on].setState(ISS_ON);
    m_tcfSP.setState(IPS_OK);
    m_tcfSP.apply();

    char reply[OnStepXComm::REPLY_BUF_SIZE];
    double v = 0;
    bool ok = true;
    if (sendText("C", reply) && OnStepXFormat::parseNumber(reply, v))
        m_tcfNP[0].setValue(v);
    else
        ok = false;
    if (sendText("d", reply) && OnStepXFormat::parseNumber(reply, v))
        m_tcfNP[1].setValue(v);
    else
        ok = false;
    m_tcfNP.setState(ok ? IPS_OK : IPS_ALERT);
    m_tcfNP.apply();
    readTcfDelta();
}

void OnStepXFocuser::readTcfDelta()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    double v = 0;
    if (sendText("e", reply) && OnStepXFormat::parseNumber(reply, v))
    {
        m_tcfDeltaNP[0].setValue(v);
        m_tcfDeltaNP.setState(IPS_OK);
        m_tcfDeltaNP.apply();
    }
}

// ---------------------------------------------------------------------------
// FocuserInterface virtuals
// ---------------------------------------------------------------------------
IPState OnStepXFocuser::MoveAbsFocuser(uint32_t targetTicks)
{
    char body[OnStepXComm::CMD_MAX_LEN];
    snprintf(body, sizeof(body), "S%u", targetTicks);
    if (!sendBool(body))
    {
        LOGF_ERROR("Move to %u rejected by firmware (outside limits or focuser busy?)", targetTicks);
        return IPS_ALERT;
    }
    m_moving = true;
    return IPS_BUSY;
}

IPState OnStepXFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    const long delta = (dir == FOCUS_INWARD) ? -static_cast<long>(ticks) : static_cast<long>(ticks);
    char body[OnStepXComm::CMD_MAX_LEN];
    snprintf(body, sizeof(body), "R%+ld", delta);
    if (!sendNoReply(body))
    {
        LOG_ERROR("Relative move command failed");
        return IPS_ALERT;
    }
    m_moving = true;
    FocusAbsPosNP.setState(IPS_BUSY);
    FocusAbsPosNP.apply();
    return IPS_BUSY;
}

bool OnStepXFocuser::AbortFocuser()
{
    const bool ok = sendNoReply("Q");
    m_moving = false;
    FocusAbsPosNP.setState(IPS_IDLE);
    FocusAbsPosNP.apply();
    return ok;
}

bool OnStepXFocuser::SyncFocuser(uint32_t ticks)
{
    // OnStepX can only redefine the current position as zero (:F<n>Z#) or
    // as its configured home (:F<n>H#); there is no "set position to n".
    if (ticks != 0)
    {
        LOG_ERROR("OnStepX focusers can only be synced to 0.");
        return false;
    }
    if (!sendNoReply("Z"))
        return false;

    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP.apply();
    return true;
}

bool OnStepXFocuser::SetFocuserBacklash(int32_t steps)
{
    char body[OnStepXComm::CMD_MAX_LEN];
    snprintf(body, sizeof(body), "B%d", steps);
    return sendBool(body);
}

bool OnStepXFocuser::SetFocuserBacklashEnabled(bool enabled)
{
    // OnStepX has no separate backlash-enable toggle; disabled = 0.
    if (!enabled)
        return SetFocuserBacklash(0);
    return SetFocuserBacklash(static_cast<int32_t>(FocusBacklashNP[0].getValue()));
}

bool OnStepXFocuser::SetFocuserSpeed(int speed)
{
    // Goto rate: speed 1..5 -> :F<n>5# .. :F<n>9# (0.5x, 0.66x, 1x, 1.5x, 2x).
    if (speed < 1 || speed > 5)
        return false;
    char body[4];
    snprintf(body, sizeof(body), "%d", speed + 4);
    return sendNoReply(body);
}

// ---------------------------------------------------------------------------
// Protocol helpers
// ---------------------------------------------------------------------------
bool OnStepXFocuser::formatCmd(int slot, const char *body, char *buf, size_t len)
{
    if (slot < 1 || slot > 6)
        return false;
    const int n = snprintf(buf, len, ":F%d%s#", slot, body);
    return n > 0 && static_cast<size_t>(n) < len;
}

bool OnStepXFocuser::sendText(const char *body, char *reply)
{
    char cmd[OnStepXComm::CMD_MAX_LEN];
    return m_comm && formatCmd(m_slot, body, cmd, sizeof(cmd)) && m_comm->sendText(cmd, reply);
}

bool OnStepXFocuser::sendBool(const char *body)
{
    char cmd[OnStepXComm::CMD_MAX_LEN];
    return m_comm && formatCmd(m_slot, body, cmd, sizeof(cmd)) && m_comm->sendBool(cmd);
}

bool OnStepXFocuser::sendNoReply(const char *body)
{
    char cmd[OnStepXComm::CMD_MAX_LEN];
    return m_comm && formatCmd(m_slot, body, cmd, sizeof(cmd)) && m_comm->sendNoReply(cmd);
}

static bool parseLong(const char *s, long &out)
{
    char *end;
    out = std::strtol(s, &end, 10);
    return end != s;
}

bool OnStepXFocuser::cmdGetPos(uint32_t &pos)
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    long val = 0;
    if (!sendText("G", reply) || !parseLong(reply, val))
        return false;
    // INDI absolute positions are unsigned; clamp a negative firmware position.
    pos = val < 0 ? 0 : static_cast<uint32_t>(val);
    return true;
}

bool OnStepXFocuser::cmdGetLimits(uint32_t &minPos, uint32_t &maxPos)
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    long lo = 0, hi = 0;
    if (!sendText("I", reply) || !parseLong(reply, lo))
        return false;
    if (!sendText("M", reply) || !parseLong(reply, hi) || hi <= 0)
        return false;
    minPos = lo < 0 ? 0 : static_cast<uint32_t>(lo);
    maxPos = static_cast<uint32_t>(hi);
    return maxPos > minPos;
}

bool OnStepXFocuser::cmdGetStatus(bool &moving)
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    // 'M' (moving) or 'S' (stopped), then the goto rate digit 1-5.  Anything
    // else lost a byte ("3" for "M3") and must not end a move in progress.
    if (!sendText("T", reply) || (reply[0] != 'M' && reply[0] != 'S') || reply[1] < '1' || reply[1] > '5')
        return false;
    moving = (reply[0] == 'M');
    return true;
}

bool OnStepXFocuser::cmdGetTemperature(double &tempC)
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    if (!sendText("t", reply))
        return false;
    double val = 0;
    if (!OnStepXFormat::parseNumber(reply, val) || val > 990.0)
        return false;   // "nan" when no temperature source is available
    tempC = val;
    return true;
}

// ---------------------------------------------------------------------------
// Child-device lifecycle (shared by OnStepXMount and OnStepXAux)
// ---------------------------------------------------------------------------
void OnStepXFocuser::connectChildren(Children &children, uint8_t focuserMask, OnStepXComm *comm,
                                     const char *parentName)
{
    for (int slot = 1; slot <= static_cast<int>(children.size()); slot++)
    {
        if (!(focuserMask & (1u << (slot - 1))))
            continue;

        auto &child = children[slot - 1];
        const bool isNew = !child;

        if (isNew)
        {
            child = std::make_unique<OnStepXFocuser>(slot, parentName);
            child->setComm(comm);

            // Registers the device in the global list so clients (Ekos) see
            // it as a separate focuser in the same process.
            // DefaultDevice::ISGetProperties() runs initProperties() the first time.
            child->ISGetProperties(nullptr);
        }

        // No own port — the parent's connection is shared.
        child->setConnected(true, IPS_OK);

        if (isNew)
        {
            // Defer updateProperties so clients process newDevice first.
            OnStepXFocuser *raw = child.get();
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
}

void OnStepXFocuser::disconnectChildren(Children &children)
{
    for (auto &child : children)
    {
        if (child && child->isConnected())
        {
            child->setConnected(false, IPS_IDLE);
            child->updateProperties();
        }
    }
}
