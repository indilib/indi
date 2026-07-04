/*******************************************************************************
 Copyright(c) 2026 Jasem Mutlaq. All rights reserved.

 PlaneWave Rotator Driver
 API Communication via PWI4 HTTP Interface (port 8220)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "planewave_rotator.h"

#include "indicom.h"
#include "connectionplugins/connectiontcp.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>
#include <iomanip>

// Single global instance
static std::unique_ptr<PlaneWaveRotator> sRotator(new PlaneWaveRotator());

/////////////////////////////////////////////////////////////////////////////
/// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////
PlaneWaveRotator::PlaneWaveRotator()
{
    setVersion(1, 0);

    // Capabilities: can abort and can reverse direction (software)
    RI::SetCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_REVERSE);

    // We only want TCP — disable serial
    setRotatorConnection(CONNECTION_TCP);
}

// Must be defined here so unique_ptr<httplib::Client> is destroyed
// after the complete type is known.
PlaneWaveRotator::~PlaneWaveRotator() = default;

/////////////////////////////////////////////////////////////////////////////
/// getDefaultName
/////////////////////////////////////////////////////////////////////////////
const char *PlaneWaveRotator::getDefaultName()
{
    return "PlaneWave Rotator";
}

/////////////////////////////////////////////////////////////////////////////
/// initProperties
/////////////////////////////////////////////////////////////////////////////
bool PlaneWaveRotator::initProperties()
{
    INDI::Rotator::initProperties();

    // ── TCP connection defaults ────────────────────────────────────────────
    tcpConnection->setDefaultHost("localhost");
    tcpConnection->setDefaultPort(DEFAULT_PORT);

    // ── PWI4 version ────────────────────────────────────────────────────────
    VersionTP[0].fill("VERSION", "PWI4 Version", "");
    VersionTP.fill(getDeviceName(), "FIRMWARE_INFO", "PWI4",
                   MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // ── Motor enable / disable ─────────────────────────────────────────────
    MotorStateSP[MOTOR_ENABLE].fill("MOTOR_ENABLE",  "Enable",  ISS_OFF);
    MotorStateSP[MOTOR_DISABLE].fill("MOTOR_DISABLE", "Disable", ISS_OFF);
    MotorStateSP.fill(getDeviceName(), "ROTATOR_MOTOR", "Motor",
                      MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // ── Mechanical position (read-write) ───────────────────────────────────
    MechPositionNP[0].fill("MECH_POSITION", "Mechanical (°)", "%.2f", 0.0, 360.0, 0.1, 0.0);
    MechPositionNP.fill(getDeviceName(), "ROTATOR_MECH_POSITION", "Mech. Position",
                        MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // ── Status lights ──────────────────────────────────────────────────────
    RotatorStatusLP[STATUS_CONNECTED].fill("STATUS_CONNECTED", "HW Connected", IPS_IDLE);
    RotatorStatusLP[STATUS_ENABLED].fill("STATUS_ENABLED",   "Motor Enabled",  IPS_IDLE);
    RotatorStatusLP.fill(getDeviceName(), "ROTATOR_STATUS", "Status",
                         MAIN_CONTROL_TAB, IPS_IDLE);

    // ── Rotator interface properties ───────────────────────────────────────
    // These are already filled in by INDI::RotatorInterface::initProperties(),
    // called inside INDI::Rotator::initProperties() above.
    // GotoRotatorNP represents the field angle (0-360°).

    addAuxControls();
    setDefaultPollingPeriod(1000);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// updateProperties
/////////////////////////////////////////////////////////////////////////////
bool PlaneWaveRotator::updateProperties()
{
    INDI::Rotator::updateProperties();

    if (isConnected())
    {
        defineProperty(VersionTP);
        defineProperty(MotorStateSP);
        defineProperty(MechPositionNP);
        defineProperty(RotatorStatusLP);
    }
    else
    {
        deleteProperty(VersionTP);
        deleteProperty(MotorStateSP);
        deleteProperty(MechPositionNP);
        deleteProperty(RotatorStatusLP);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Handshake — build the persistent HTTP client and verify connectivity
/////////////////////////////////////////////////////////////////////////////
bool PlaneWaveRotator::Handshake()
{
    // Create (or recreate on reconnect) the persistent HTTP client
    m_HttpClient = std::make_unique<httplib::Client>(tcpConnection->host(),
                   tcpConnection->port());
    m_HttpClient->set_connection_timeout(5, 0);
    m_HttpClient->set_read_timeout(10, 0);

    // Fetch initial status to verify PWI4 is reachable
    if (!getStatus())
    {
        LOG_ERROR("Cannot communicate with PWI4. Is PWI4 running and reachable?");
        return false;
    }

    // Check that PWI4 reports the rotator as connected to hardware
    bool rotatorConnected = m_Status["rotator.is_connected"].as<bool>();
    if (!rotatorConnected)
    {
        LOG_ERROR("PWI4 reports rotator is not connected to hardware. "
                  "Please connect the rotator in PWI4 first.");
        return false;
    }

    // Auto-enable motor if it is currently disabled
    bool motorEnabled = m_Status["rotator.is_enabled"].as<bool>();
    if (!motorEnabled)
    {
        LOG_INFO("Motor is disabled. Sending /rotator/enable …");
        dispatch("/rotator/enable");
        // Re-read status to confirm
        getStatus();
        motorEnabled = m_Status["rotator.is_enabled"].as<bool>();
    }

    // Populate version info
    std::string version = m_Status["pwi4.version"].as<std::string>();
    VersionTP[0].setText(version.c_str());
    VersionTP.setState(IPS_OK);

    // Populate motor state switch
    MotorStateSP[MOTOR_ENABLE].setState(motorEnabled  ? ISS_ON : ISS_OFF);
    MotorStateSP[MOTOR_DISABLE].setState(motorEnabled ? ISS_OFF : ISS_ON);
    MotorStateSP.setState(IPS_OK);

    // Populate status lights
    RotatorStatusLP[STATUS_CONNECTED].setState(IPS_OK);
    RotatorStatusLP[STATUS_ENABLED].setState(motorEnabled ? IPS_OK : IPS_ALERT);
    RotatorStatusLP.setState(IPS_OK);

    // Seed the position fields
    double fieldAngle = m_Status["rotator.field_angle_degs"].as<double>();
    double mechAngle  = m_Status["rotator.mech_position_degs"].as<double>();

    GotoRotatorNP[0].setValue(fieldAngle);
    GotoRotatorNP.setState(IPS_OK);

    MechPositionNP[0].setValue(mechAngle);
    MechPositionNP.setState(IPS_OK);

    m_LastFieldAngle   = fieldAngle;
    m_LastMechPosition = mechAngle;

    LOGF_INFO("Connected to PWI4 %s. Field angle: %.2f°, Mech position: %.2f°",
              version.c_str(), fieldAngle, mechAngle);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// ISNewSwitch
/////////////////////////////////////////////////////////////////////////////
bool PlaneWaveRotator::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // ── Motor enable / disable ────────────────────────────────────────
        if (MotorStateSP.isNameMatch(name))
        {
            MotorStateSP.update(states, names, n);
            bool enableRequested = (MotorStateSP[MOTOR_ENABLE].getState() == ISS_ON);

            bool ok = dispatch(enableRequested ? "/rotator/enable" : "/rotator/disable");
            if (ok)
            {
                LOGF_INFO("Rotator motor %s.", enableRequested ? "enabled" : "disabled");
                MotorStateSP.setState(IPS_OK);
                RotatorStatusLP[STATUS_ENABLED].setState(enableRequested ? IPS_OK : IPS_ALERT);
                RotatorStatusLP.apply();
            }
            else
            {
                LOG_ERROR("Failed to change motor state.");
                MotorStateSP.setState(IPS_ALERT);
            }
            MotorStateSP.apply();
            return true;
        }

        // ── RotatorInterface switches (Abort, Reverse, etc.) ──────────────
        if (INDI::RotatorInterface::processSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::Rotator::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
/// ISNewNumber
/////////////////////////////////////////////////////////////////////////////
bool PlaneWaveRotator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // ── Mechanical position goto ─────────────────────────────────────
        if (MechPositionNP.isNameMatch(name))
        {
            double target = values[0];

            std::ostringstream req;
            req << std::fixed << std::setprecision(4) << "/rotator/goto_mech?degs=" << target;

            if (dispatch(req.str()))
            {
                MechPositionNP[0].setValue(target);
                MechPositionNP.setState(IPS_BUSY);
                // Field angle goto will also update GotoRotatorNP via TimerHit
                GotoRotatorNP.setState(IPS_BUSY);
                GotoRotatorNP.apply();
                LOGF_INFO("Moving rotator to mechanical position %.2f°.", target);
            }
            else
            {
                LOG_ERROR("Failed to send /rotator/goto_mech command.");
                MechPositionNP.setState(IPS_ALERT);
            }
            MechPositionNP.apply();
            return true;
        }

        // ── RotatorInterface numbers (GotoRotatorNP handled by MoveRotator) ─
        if (INDI::RotatorInterface::processNumber(dev, name, values, names, n))
            return true;
    }

    return INDI::Rotator::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
/// MoveRotator — RotatorInterface pure-virtual
/// Moves to the given field angle (0–360°), honouring the reverse flag.
/////////////////////////////////////////////////////////////////////////////
IPState PlaneWaveRotator::MoveRotator(double angle)
{
    // Apply direction reversal in software
    double target = angle;
    if (ReverseRotatorSP[INDI_ENABLED].getState() == ISS_ON)
        target = range360(360.0 - angle);

    std::ostringstream req;
    req << std::fixed << std::setprecision(4) << "/rotator/goto_field?degs=" << target;

    if (!dispatch(req.str()))
    {
        LOG_ERROR("Failed to send /rotator/goto_field command.");
        return IPS_ALERT;
    }

    LOGF_INFO("Moving rotator to field angle %.2f°%s.", target,
              (ReverseRotatorSP[INDI_ENABLED].getState() == ISS_ON) ? " (reversed)" : "");

    return IPS_BUSY;
}

/////////////////////////////////////////////////////////////////////////////
/// AbortRotator — RotatorInterface virtual
/////////////////////////////////////////////////////////////////////////////
bool PlaneWaveRotator::AbortRotator()
{
    bool ok = dispatch("/rotator/stop");
    if (ok)
        LOG_INFO("Rotator motion aborted.");
    else
        LOG_ERROR("Failed to abort rotator motion.");
    return ok;
}

/////////////////////////////////////////////////////////////////////////////
/// ReverseRotator — RotatorInterface virtual
/// Direction reversal is handled entirely in software (in MoveRotator).
/////////////////////////////////////////////////////////////////////////////
bool PlaneWaveRotator::ReverseRotator(bool enabled)
{
    LOGF_INFO("Rotator direction %s.", enabled ? "reversed" : "normal");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// TimerHit — polling loop
/////////////////////////////////////////////////////////////////////////////
void PlaneWaveRotator::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    if (!getStatus())
    {
        LOG_WARN("Failed to get status from PWI4.");
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    // ── Read live values from status ──────────────────────────────────────
    bool hwConnected  = m_Status["rotator.is_connected"].as<bool>();
    bool motorEnabled = m_Status["rotator.is_enabled"].as<bool>();
    bool isMoving     = m_Status["rotator.is_moving"].as<bool>();
    double fieldAngle = m_Status["rotator.field_angle_degs"].as<double>();
    double mechAngle  = m_Status["rotator.mech_position_degs"].as<double>();

    // ── Status lights ─────────────────────────────────────────────────────
    IPState connLight    = hwConnected  ? IPS_OK : IPS_ALERT;
    IPState enabledLight = motorEnabled ? IPS_OK : IPS_ALERT;
    if (RotatorStatusLP[STATUS_CONNECTED].getState() != connLight ||
            RotatorStatusLP[STATUS_ENABLED].getState()   != enabledLight)
    {
        RotatorStatusLP[STATUS_CONNECTED].setState(connLight);
        RotatorStatusLP[STATUS_ENABLED].setState(enabledLight);
        RotatorStatusLP.apply();
    }

    // ── Motor state switch ────────────────────────────────────────────────
    ISState onState  = motorEnabled ? ISS_ON  : ISS_OFF;
    ISState offState = motorEnabled ? ISS_OFF : ISS_ON;
    if (MotorStateSP[MOTOR_ENABLE].getState() != onState)
    {
        MotorStateSP[MOTOR_ENABLE].setState(onState);
        MotorStateSP[MOTOR_DISABLE].setState(offState);
        MotorStateSP.apply();
    }

    // ── Field angle (GotoRotatorNP) ───────────────────────────────────────
    bool fieldChanged = std::fabs(fieldAngle - m_LastFieldAngle) > POSITION_THRESHOLD;

    if (GotoRotatorNP.getState() == IPS_BUSY)
    {
        if (!isMoving)
        {
            // Motion completed
            GotoRotatorNP[0].setValue(fieldAngle);
            GotoRotatorNP.setState(IPS_OK);
            GotoRotatorNP.apply();
            LOG_INFO("Rotator reached target field angle.");
            fieldChanged = false;
        }
        else if (fieldChanged)
        {
            // Still moving — update position readout
            GotoRotatorNP[0].setValue(fieldAngle);
            GotoRotatorNP.apply();
        }
    }
    else if (fieldChanged)
    {
        GotoRotatorNP[0].setValue(fieldAngle);
        GotoRotatorNP.apply();
    }

    if (fieldChanged)
        m_LastFieldAngle = fieldAngle;

    // ── Mechanical position (MechPositionNP) ─────────────────────────────
    bool mechChanged = std::fabs(mechAngle - m_LastMechPosition) > POSITION_THRESHOLD;

    if (MechPositionNP.getState() == IPS_BUSY)
    {
        if (!isMoving)
        {
            MechPositionNP[0].setValue(mechAngle);
            MechPositionNP.setState(IPS_OK);
            MechPositionNP.apply();
            mechChanged = false;
        }
        else if (mechChanged)
        {
            MechPositionNP[0].setValue(mechAngle);
            MechPositionNP.apply();
        }
    }
    else if (mechChanged)
    {
        MechPositionNP[0].setValue(mechAngle);
        MechPositionNP.apply();
    }

    if (mechChanged)
        m_LastMechPosition = mechAngle;

    SetTimer(getCurrentPollingPeriod());
}

/////////////////////////////////////////////////////////////////////////////
/// saveConfigItems
/////////////////////////////////////////////////////////////////////////////
bool PlaneWaveRotator::saveConfigItems(FILE *fp)
{
    INDI::Rotator::saveConfigItems(fp);
    // ReverseRotatorSP is already saved by RotatorInterface::saveConfigItems
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// getStatus — thin wrapper around dispatch("/status")
/////////////////////////////////////////////////////////////////////////////
bool PlaneWaveRotator::getStatus()
{
    return dispatch("/status");
}

/////////////////////////////////////////////////////////////////////////////
/// dispatch — single HTTP entry-point
///
/// Sends an HTTP GET request via the persistent client, verifies HTTP 200,
/// parses the flat key=value body into m_Status using inicpp, and returns
/// true on success.
/////////////////////////////////////////////////////////////////////////////
bool PlaneWaveRotator::dispatch(const std::string &request)
{
    if (!m_HttpClient)
    {
        LOG_ERROR("HTTP client not initialized. Is the rotator connected?");
        return false;
    }

    auto res = m_HttpClient->Get(request);

    if (!res)
    {
        LOGF_ERROR("HTTP GET %s failed: %s",
                   request.c_str(),
                   httplib::to_string(res.error()).c_str());
        return false;
    }

    if (res->status != 200)
    {
        LOGF_ERROR("HTTP GET %s returned HTTP %d: %s",
                   request.c_str(), res->status,
                   res->body.empty() ? "(no body)" : res->body.c_str());
        return false;
    }

    if (res->body.empty())
    {
        LOGF_ERROR("HTTP GET %s returned an empty body.", request.c_str());
        return false;
    }

    try
    {
        // PWI4 returns a flat key=value format. Wrap it in a dummy [status]
        // section so that inicpp can parse it as a standard INI file.
        ini::IniFile inif;
        inif.decode("[status]\n" + res->body);

        if (inif["status"].size() == 0)
        {
            LOGF_ERROR("Parsed status for %s is empty.", request.c_str());
            return false;
        }

        m_Status = inif["status"];
        return true;
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Failed to parse PWI4 response for %s: %s",
                   request.c_str(), e.what());
        return false;
    }
}
