/*******************************************************************************
  Copyright(c) 2013-2026 Jasem Mutlaq. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "joystick.h"

#include "joystickdriver.h"
#include "indistandardproperty.h"

// LilXML for calibration file I/O (same approach as ParkData in inditelescope.cpp)
#include "lilxml.h"

#include <memory>
#include <cstring>
#include <cmath>
#include <climits>
#include <cerrno>
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#include <wordexp.h>

#include "indimacros.h"

// We declare an auto pointer to joystick.
std::unique_ptr<JoyStick> joystick(new JoyStick());

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

JoyStick::JoyStick()
{
    driver.reset(new JoyStickDriver());
}

// ─────────────────────────────────────────────────────────────────────────────
// INDI DefaultDevice overrides
// ─────────────────────────────────────────────────────────────────────────────

const char *JoyStick::getDefaultName()
{
    return "Joystick";
}

bool JoyStick::Connect()
{
    bool rc = driver->Connect();

    if (rc)
    {
        LOG_INFO("Joystick is online.");
        setupParams();

        // Load saved calibration for this hardware name (silent if not found).
        if (loadCalibrationData())
            LOGF_INFO("Calibration loaded for \"%s\".", driver->getName());
        else
            LOG_DEBUG("No saved calibration found for this joystick.");
    }
    else
        LOG_INFO("Error: cannot find Joystick device.");

    return rc;
}

bool JoyStick::Disconnect()
{
    LOG_INFO("Joystick is offline.");
    return driver->Disconnect();
}

// ─────────────────────────────────────────────────────────────────────────────
// setupParams — called right after a successful Connect()
// ─────────────────────────────────────────────────────────────────────────────

void JoyStick::setupParams()
{
    char propName[MAXINDINAME], propLabel[MAXINDILABEL];

    if (driver == nullptr)
        return;

    int nAxis      = driver->getNumOfAxes();
    int nJoysticks = driver->getNumOfJoysticks();
    int nButtons   = driver->getNumrOfButtons();

    // One PropertyNumber per joystick, each with 2 elements (Magnitude + Angle).
    // Use the human-readable pair name from the driver (e.g. "Left Stick", "D-pad (Hat 0)").
    JoyStickNP.clear();
    JoyStickNP.reserve(nJoysticks);
    for (int i = 0; i < nJoysticks; i++)
    {
        snprintf(propName, MAXINDINAME, "JOYSTICK_%d", i + 1);

        const JoyAxisPair *pair = driver->getJoystickPair(i);
        if (pair)
            snprintf(propLabel, MAXINDILABEL, "%s", pair->name);
        else
            snprintf(propLabel, MAXINDILABEL, "Joystick %d", i + 1);

        JoyStickNP.emplace_back(2);
        auto &jp = JoyStickNP.back();
        jp[0].fill("JOYSTICK_MAGNITUDE", "Magnitude", "%g", 0.0, 1.0, 0, 0);
        jp[1].fill("JOYSTICK_ANGLE",     "Angle",     "%g", 0.0, 360.0, 0, 0);
        jp.fill(getDeviceName(), propName, propLabel, "Monitor", IP_RO, 0, IPS_IDLE);
    }

    // Axis raw values — label each axis with its hardware type (Left-Stick X, L2 Trigger, etc.)
    AxisNP.resize(nAxis);
    for (int i = 0; i < nAxis; i++)
    {
        snprintf(propName, MAXINDINAME, "AXIS_%d", i + 1);
        // Use axis label from driver; fall back to "Axis N"
        const char *axLabel = driver->getAxisLabel(i);
        if (axLabel && axLabel[0] != '\0' && strcmp(axLabel, "Unknown") != 0)
            snprintf(propLabel, MAXINDILABEL, "%s", axLabel);
        else
            snprintf(propLabel, MAXINDILABEL, "Axis %d", i + 1);
        AxisNP[i].fill(propName, propLabel, "%.f", -32767.0, 32767.0, 0, 0);
    }
    AxisNP.fill(getDeviceName(), "JOYSTICK_AXES", "Axes", "Monitor", IP_RO, 0, IPS_IDLE);

    // Dead zone thresholds — same labels as AxisNP
    DeadZoneNP.resize(nAxis);
    for (int i = 0; i < nAxis; i++)
    {
        snprintf(propName, MAXINDINAME, "AXIS_%d", i + 1);
        const char *axLabel = driver->getAxisLabel(i);
        if (axLabel && axLabel[0] != '\0' && strcmp(axLabel, "Unknown") != 0)
            snprintf(propLabel, MAXINDILABEL, "%s", axLabel);
        else
            snprintf(propLabel, MAXINDILABEL, "Axis %d", i + 1);
        DeadZoneNP[i].fill(propName, propLabel, "%.f", 0, 5000, 500, 5);
    }
    DeadZoneNP.fill(getDeviceName(), "JOYSTICK_DEAD_ZONE", "Dead Zones", "Monitor", IP_RW, 0, IPS_IDLE);

    // Initialize EMA state vectors to zero
    m_axisEMA.assign(nAxis, 0.0);
    m_joystickAngleCosEMA.assign(nJoysticks, 0.0);
    m_joystickAngleSinEMA.assign(nJoysticks, 0.0);

    // Button states
    ButtonSP.resize(nButtons);
    for (int i = 0; i < nButtons; i++)
    {
        snprintf(propName, MAXINDINAME, "BUTTON_%d", i + 1);
        snprintf(propLabel, MAXINDILABEL, "Button %d", i + 1);
        ButtonSP[i].fill(propName, propLabel, ISS_OFF);
    }
    ButtonSP.fill(getDeviceName(), "JOYSTICK_BUTTONS", "Buttons", "Monitor", IP_RO, ISR_NOFMANY, 0, IPS_IDLE);

    // ── Per-axis calibration property ────────────────────────────────────────
    // Three elements per axis: center, min, max.
    AxisCalibrationNP.resize(nAxis * 3);
    for (int i = 0; i < nAxis; i++)
    {
        snprintf(propName,  MAXINDINAME,  "AXIS_%d_CENTER", i + 1);
        snprintf(propLabel, MAXINDILABEL, "Axis %d Center", i + 1);
        AxisCalibrationNP[i * 3 + 0].fill(propName, propLabel, "%.f", -32767.0, 32767.0, 1, 0);

        snprintf(propName,  MAXINDINAME,  "AXIS_%d_MIN", i + 1);
        snprintf(propLabel, MAXINDILABEL, "Axis %d Min",    i + 1);
        AxisCalibrationNP[i * 3 + 1].fill(propName, propLabel, "%.f", -32767.0, 32767.0, 1, -32767);

        snprintf(propName,  MAXINDINAME,  "AXIS_%d_MAX", i + 1);
        snprintf(propLabel, MAXINDILABEL, "Axis %d Max",    i + 1);
        AxisCalibrationNP[i * 3 + 2].fill(propName, propLabel, "%.f", -32767.0, 32767.0, 1,  32767);
    }
    AxisCalibrationNP.fill(getDeviceName(), "JOYSTICK_AXIS_CALIBRATION", "Axis Calibration",
                           "Calibration", IP_RW, 0, IPS_IDLE);

    // Initialise runtime calibration tracking vectors
    m_axisCalCenter.assign(nAxis, 0);
    m_axisCalMin.assign(nAxis, -32767);
    m_axisCalMax.assign(nAxis,  32767);

    // ── Per-axis reversal property ────────────────────────────────────────────
    // One switch per axis, all default OFF.  ISR_NOFMANY → each is independent.
    AxisReverseSP.resize(nAxis);
    for (int i = 0; i < nAxis; i++)
    {
        const char *axLabel = driver->getAxisLabel(i);
        if (axLabel && axLabel[0] != '\0' && strcmp(axLabel, "Unknown") != 0)
            snprintf(propLabel, MAXINDILABEL, "%s", axLabel);
        else
            snprintf(propLabel, MAXINDILABEL, "Axis %d", i + 1);

        snprintf(propName, MAXINDINAME, "AXIS_%d_REVERSE", i + 1);
        AxisReverseSP[i].fill(propName, propLabel, ISS_OFF);
    }
    AxisReverseSP.fill(getDeviceName(), "JOYSTICK_AXIS_REVERSE", "Reverse Axes",
                       "Calibration", IP_RW, ISR_NOFMANY, 0, IPS_IDLE);
}

// ─────────────────────────────────────────────────────────────────────────────
// initProperties
// ─────────────────────────────────────────────────────────────────────────────

bool JoyStick::initProperties()
{
    INDI::DefaultDevice::initProperties();

    PortTP[0].fill("PORT", "Port", "/dev/input/js0");
    PortTP.fill(getDeviceName(), INDI::SP::DEVICE_PORT, "Ports", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    JoystickInfoTP[0].fill("JOYSTICK_NAME",       "Name",        "");
    JoystickInfoTP[1].fill("JOYSTICK_VERSION",    "Version",     "");
    JoystickInfoTP[2].fill("JOYSTICK_NJOYSTICKS", "# Joysticks", "");
    JoystickInfoTP[3].fill("JOYSTICK_NAXES",      "# Axes",      "");
    JoystickInfoTP[4].fill("JOYSTICK_NBUTTONS",   "# Buttons",   "");
    JoystickInfoTP.fill(getDeviceName(), "JOYSTICK_INFO", "Joystick Info", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // EMA smoothing factor: 1.0 = pass-through (no filter), lower = more smoothing
    FilterNP[0].fill("JOYSTICK_FILTER_FACTOR", "Smoothing (α)", "%.2f", 0.01, 1.0, 0.05, 0.5);
    FilterNP.fill(getDeviceName(), "JOYSTICK_FILTER", "EMA Filter", "Options", IP_RW, 0, IPS_IDLE);

    // Calibration control (defined here; AxisCalibrationNP is defined in setupParams)
    CalibrationSP[0].fill("CALIBRATION_START", "Start Calibration", ISS_OFF);
    CalibrationSP[1].fill("CALIBRATION_END",   "End Calibration",   ISS_OFF);
    CalibrationSP.fill(getDeviceName(), "JOYSTICK_CALIBRATION", "Calibration",
                       "Calibration", IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Build calibration file path: ~/.indi/JoystickCalibrationData.xml
    {
        const char *home = getenv("HOME");
        if (!home)
        {
            struct passwd *pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (home)
            m_calibrationFile = std::string(home) + "/.indi/JoystickCalibrationData.xml";
        else
            m_calibrationFile = "/tmp/JoystickCalibrationData.xml";
    }

    addDebugControl();

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// updateProperties
// ─────────────────────────────────────────────────────────────────────────────

bool JoyStick::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        char buf[8];

        // Populate joystick info text fields
        JoystickInfoTP[0].setText(driver->getName());
        snprintf(buf, sizeof(buf), "%d", driver->getVersion());
        JoystickInfoTP[1].setText(buf);
        snprintf(buf, sizeof(buf), "%d", driver->getNumOfJoysticks());
        JoystickInfoTP[2].setText(buf);
        snprintf(buf, sizeof(buf), "%d", driver->getNumOfAxes());
        JoystickInfoTP[3].setText(buf);
        snprintf(buf, sizeof(buf), "%d", driver->getNumrOfButtons());
        JoystickInfoTP[4].setText(buf);

        defineProperty(JoystickInfoTP);

        for (auto &jp : JoyStickNP)
            defineProperty(jp);

        defineProperty(AxisNP);
        defineProperty(ButtonSP);

        // Dead zones and EMA filter
        defineProperty(DeadZoneNP);
        defineProperty(FilterNP);

        // Calibration
        defineProperty(CalibrationSP);
        defineProperty(AxisCalibrationNP);

        // Per-axis reversal — restore saved state immediately after defining
        defineProperty(AxisReverseSP);
        AxisReverseSP.load();
        applyReverseToDriver();

        // N.B. Only set callbacks AFTER we define our properties above
        // because these callbacks otherwise can be called asynchronously
        // and they mess up INDI XML output
        driver->setJoystickCallback(joystickHelper);
        driver->setAxisCallback(axisHelper);
        driver->setButtonCallback(buttonHelper);
    }
    else
    {
        deleteProperty(JoystickInfoTP);

        for (auto &jp : JoyStickNP)
            deleteProperty(jp);
        JoyStickNP.clear();

        deleteProperty(AxisNP);
        deleteProperty(DeadZoneNP);
        deleteProperty(FilterNP);
        deleteProperty(ButtonSP);
        deleteProperty(CalibrationSP);
        deleteProperty(AxisCalibrationNP);
        deleteProperty(AxisReverseSP);

        AxisNP.resize(0);
        DeadZoneNP.resize(0);
        AxisCalibrationNP.resize(0);
        AxisReverseSP.resize(0);
        ButtonSP.resize(0);
        m_axisEMA.clear();
        m_joystickAngleCosEMA.clear();
        m_joystickAngleSinEMA.clear();
        m_axisCalCenter.clear();
        m_axisCalMin.clear();
        m_axisCalMax.clear();
        m_calibrating = false;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ISGetProperties / ISSnoopDevice
// ─────────────────────────────────────────────────────────────────────────────

void JoyStick::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineProperty(PortTP);
    loadConfig(true, INDI::SP::DEVICE_PORT);
}

bool JoyStick::ISSnoopDevice(XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

// ─────────────────────────────────────────────────────────────────────────────
// ISNewText
// ─────────────────────────────────────────────────────────────────────────────

bool JoyStick::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (PortTP.isNameMatch(name))
        {
            PortTP.update(texts, names, n);
            PortTP.setState(IPS_OK);
            PortTP.apply();
            driver->setPort(PortTP[0].getText());
            return true;
        }
    }

    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

// ─────────────────────────────────────────────────────────────────────────────
// ISNewNumber
// ─────────────────────────────────────────────────────────────────────────────

bool JoyStick::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (DeadZoneNP.isNameMatch(name))
        {
            DeadZoneNP.update(values, names, n);
            DeadZoneNP.setState(IPS_OK);
            DeadZoneNP.apply();
            return true;
        }

        if (FilterNP.isNameMatch(name))
        {
            FilterNP.update(values, names, n);
            FilterNP.setState(IPS_OK);
            FilterNP.apply();
            saveConfig(FilterNP);
            // Reset EMA state so the new alpha takes effect cleanly
            std::fill(m_axisEMA.begin(), m_axisEMA.end(), 0.0);
            std::fill(m_joystickAngleCosEMA.begin(), m_joystickAngleCosEMA.end(), 0.0);
            std::fill(m_joystickAngleSinEMA.begin(), m_joystickAngleSinEMA.end(), 0.0);
            return true;
        }

        // Manual edit of calibration numbers → push to driver immediately
        if (AxisCalibrationNP.isNameMatch(name))
        {
            AxisCalibrationNP.update(values, names, n);
            AxisCalibrationNP.setState(IPS_OK);
            AxisCalibrationNP.apply();
            applyCalibrationToDriver();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

// ─────────────────────────────────────────────────────────────────────────────
// ISNewSwitch
// ─────────────────────────────────────────────────────────────────────────────

bool JoyStick::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (CalibrationSP.isNameMatch(name))
        {
            CalibrationSP.update(states, names, n);

            if (CalibrationSP[0].getState() == ISS_ON) // Start Calibration
            {
                if (!isConnected())
                {
                    LOG_WARN("Cannot start calibration: joystick is not connected.");
                    CalibrationSP.setState(IPS_ALERT);
                    CalibrationSP.reset();
                    CalibrationSP.apply();
                    return true;
                }

                const int nAxis = static_cast<int>(AxisNP.size());

                // Snapshot current raw axis values as center (user should be at rest)
                for (int i = 0; i < nAxis; i++)
                    m_axisCalCenter[i] = static_cast<int>(AxisNP[i].getValue());

                // Reset min/max tracking to extreme sentinels
                m_axisCalMin.assign(nAxis, INT_MAX);
                m_axisCalMax.assign(nAxis, INT_MIN);

                m_calibrating = true;
                CalibrationSP.setState(IPS_BUSY);
                LOG_INFO("Calibration started. Move every axis to its full extent in all directions, then click End Calibration.");
            }
            else if (CalibrationSP[1].getState() == ISS_ON) // End Calibration
            {
                m_calibrating = false;

                const int nAxis = static_cast<int>(AxisNP.size());
                bool anyUpdated = false;

                for (int i = 0; i < nAxis; i++)
                {
                    // If an axis was never moved its sentinel values remain → keep defaults
                    if (m_axisCalMin[i] == INT_MAX) m_axisCalMin[i] = -32767;
                    if (m_axisCalMax[i] == INT_MIN) m_axisCalMax[i] =  32767;

                    // Sanity: min must be < center < max
                    if (m_axisCalMin[i] >= m_axisCalCenter[i])
                        m_axisCalMin[i] = m_axisCalCenter[i] - 1;
                    if (m_axisCalMax[i] <= m_axisCalCenter[i])
                        m_axisCalMax[i] = m_axisCalCenter[i] + 1;

                    AxisCalibrationNP[i * 3 + 0].setValue(m_axisCalCenter[i]);
                    AxisCalibrationNP[i * 3 + 1].setValue(m_axisCalMin[i]);
                    AxisCalibrationNP[i * 3 + 2].setValue(m_axisCalMax[i]);
                    anyUpdated = true;
                }

                if (anyUpdated)
                {
                    AxisCalibrationNP.setState(IPS_OK);
                    AxisCalibrationNP.apply();
                    applyCalibrationToDriver();

                    if (saveCalibrationData())
                        LOGF_INFO("Calibration complete and saved for \"%s\".", driver->getName());
                    else
                        LOG_WARN("Calibration applied but could not be saved to file.");
                }

                CalibrationSP.setState(IPS_OK);
                CalibrationSP.reset();
            }

            CalibrationSP.apply();
            return true;
        }

        // Per-axis reversal toggle — update driver and persist immediately
        if (AxisReverseSP.isNameMatch(name))
        {
            AxisReverseSP.update(states, names, n);
            AxisReverseSP.setState(IPS_OK);
            AxisReverseSP.apply();
            applyReverseToDriver();
            saveConfig(AxisReverseSP);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

// ─────────────────────────────────────────────────────────────────────────────
// Static callback trampolines
// ─────────────────────────────────────────────────────────────────────────────

void JoyStick::joystickHelper(int joystick_n, double mag, double angle)
{
    joystick->joystickEvent(joystick_n, mag, angle);
}

void JoyStick::buttonHelper(int button_n, int value)
{
    joystick->buttonEvent(button_n, value);
}

void JoyStick::axisHelper(int axis_n, int value)
{
    joystick->axisEvent(axis_n, value);
}

// ─────────────────────────────────────────────────────────────────────────────
// Event handlers
// ─────────────────────────────────────────────────────────────────────────────

void JoyStick::joystickEvent(int joystick_n, double mag, double angle)
{
    if (!isConnected())
        return;

    LOGF_DEBUG("joystickEvent[%d]: %g @ %g", joystick_n, mag, angle);

    if (joystick_n < 0 || joystick_n >= static_cast<int>(JoyStickNP.size()))
        return;

    // Snap magnitude to zero when within the normalized dead-zone of the
    // corresponding axis pair so that a resting stick shows IDLE.
    const int axisIdx = joystick_n * 2;
    const double dzNorm = (axisIdx < static_cast<int>(DeadZoneNP.size()))
                          ? DeadZoneNP[axisIdx].getValue() / 32767.0
                          : 0.0;
    if (mag <= dzNorm)
    {
        mag   = 0.0;
        angle = 0.0;
        if (joystick_n < static_cast<int>(m_joystickAngleCosEMA.size()))
        {
            m_joystickAngleCosEMA[joystick_n] = 0.0;
            m_joystickAngleSinEMA[joystick_n] = 0.0;
        }
    }
    else
    {
        // Apply circular EMA to angle using unit-vector decomposition.
        if (joystick_n < static_cast<int>(m_joystickAngleCosEMA.size()))
        {
            const double alpha = FilterNP[0].getValue();
            const double rad   = DEG_TO_RAD(angle);
            m_joystickAngleCosEMA[joystick_n] = alpha * std::cos(rad) + (1.0 - alpha) * m_joystickAngleCosEMA[joystick_n];
            m_joystickAngleSinEMA[joystick_n] = alpha * std::sin(rad) + (1.0 - alpha) * m_joystickAngleSinEMA[joystick_n];
            double filtAngle = RAD_TO_DEG(std::atan2(m_joystickAngleSinEMA[joystick_n],
                                          m_joystickAngleCosEMA[joystick_n]));
            if (filtAngle < 0.0)
                filtAngle += 360.0;
            angle = filtAngle;
        }
    }

    JoyStickNP[joystick_n].setState(mag == 0 ? IPS_IDLE : IPS_BUSY);
    JoyStickNP[joystick_n][0].setValue(mag);
    JoyStickNP[joystick_n][1].setValue(angle);
    JoyStickNP[joystick_n].apply();
}

void JoyStick::axisEvent(int axis_n, int value)
{
    if (!isConnected())
        return;

    LOGF_DEBUG("axisEvent[%d]: %d", axis_n, value);

    if (axis_n < 0 || axis_n >= static_cast<int>(AxisNP.size()))
        return;

    // ── Calibration tracking (raw value, before any filtering) ───────────────
    if (m_calibrating && axis_n < static_cast<int>(m_axisCalMin.size()))
    {
        if (value < m_axisCalMin[axis_n]) m_axisCalMin[axis_n] = value;
        if (value > m_axisCalMax[axis_n]) m_axisCalMax[axis_n] = value;

        // Mirror live min/max into the calibration property so the user can
        // watch the values update in real time.
        AxisCalibrationNP[axis_n * 3 + 0].setValue(m_axisCalCenter[axis_n]);
        AxisCalibrationNP[axis_n * 3 + 1].setValue(m_axisCalMin[axis_n]);
        AxisCalibrationNP[axis_n * 3 + 2].setValue(m_axisCalMax[axis_n]);
        AxisCalibrationNP.setState(IPS_BUSY);
        AxisCalibrationNP.apply();
    }

    // Apply EMA low-pass filter to smooth out rapid jitter.
    const double alpha = FilterNP[0].getValue();
    if (axis_n < static_cast<int>(m_axisEMA.size()))
    {
        m_axisEMA[axis_n] = alpha * static_cast<double>(value) + (1.0 - alpha) * m_axisEMA[axis_n];
        value = static_cast<int>(std::round(m_axisEMA[axis_n]));
    }

    // All values within dead-zone are snapped to zero.
    if (std::abs(value) <= static_cast<int>(DeadZoneNP[axis_n].getValue()))
        value = 0;

    AxisNP.setState(value == 0 ? IPS_IDLE : IPS_BUSY);
    AxisNP[axis_n].setValue(value);
    AxisNP.apply();
}

void JoyStick::buttonEvent(int button_n, int value)
{
    if (!isConnected())
        return;

    LOGF_DEBUG("buttonEvent[%d]: %s", button_n, value > 0 ? "On" : "Off");

    if (button_n < 0 || button_n >= static_cast<int>(ButtonSP.size()))
        return;

    ButtonSP.setState(IPS_OK);
    ButtonSP[button_n].setState(value == 0 ? ISS_OFF : ISS_ON);
    ButtonSP.apply();
}

// ─────────────────────────────────────────────────────────────────────────────
// saveConfigItems
// ─────────────────────────────────────────────────────────────────────────────

bool JoyStick::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    PortTP.save(fp);
    DeadZoneNP.save(fp);
    FilterNP.save(fp);
    if (AxisReverseSP.size() > 0)
        AxisReverseSP.save(fp);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// applyCalibrationToDriver
// ─────────────────────────────────────────────────────────────────────────────

void JoyStick::applyCalibrationToDriver()
{
    const int nAxis = static_cast<int>(AxisNP.size());
    for (int i = 0; i < nAxis; i++)
    {
        const int center = static_cast<int>(AxisCalibrationNP[i * 3 + 0].getValue());
        const int minVal = static_cast<int>(AxisCalibrationNP[i * 3 + 1].getValue());
        const int maxVal = static_cast<int>(AxisCalibrationNP[i * 3 + 2].getValue());
        driver->setAxisCalibration(i, center, minVal, maxVal);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// applyReverseToDriver
// ─────────────────────────────────────────────────────────────────────────────

void JoyStick::applyReverseToDriver()
{
    const int nAxis = static_cast<int>(AxisReverseSP.size());
    for (int i = 0; i < nAxis; i++)
        driver->setAxisReversed(i, AxisReverseSP[i].getState() == ISS_ON);
}

// ─────────────────────────────────────────────────────────────────────────────
// loadCalibrationData
// ─────────────────────────────────────────────────────────────────────────────

bool JoyStick::loadCalibrationData()
{
    if (!isConnected() || driver == nullptr)
        return false;

    const char *hwName = driver->getName();
    if (!hwName || hwName[0] == '\0')
        return false;

    wordexp_t wexp;
    if (wordexp(m_calibrationFile.c_str(), &wexp, 0) != 0)
    {
        wordfree(&wexp);
        return false;
    }

    FILE *fp = fopen(wexp.we_wordv[0], "r");
    wordfree(&wexp);

    if (!fp)
        return false; // file does not exist yet — normal on first use

    LilXML *lp = newLilXML();
    static char errmsg[512];
    XMLEle *root = readXMLFile(fp, lp, errmsg);
    fclose(fp);
    delLilXML(lp);

    if (!root)
    {
        LOGF_DEBUG("loadCalibrationData: parse error: %s", errmsg);
        return false;
    }

    // Iterate <device name="…"> children looking for our hardware name
    bool found = false;
    for (XMLEle *devXml = nextXMLEle(root, 1); devXml; devXml = nextXMLEle(root, 0))
    {
        if (strcmp(tagXMLEle(devXml), "device") != 0)
            continue;

        XMLAtt *nameAtt = findXMLAtt(devXml, "name");
        if (!nameAtt || strcmp(valuXMLAtt(nameAtt), hwName) != 0)
            continue;

        // Found a matching device — read per-axis entries
        found = true;
        for (XMLEle *axXml = nextXMLEle(devXml, 1); axXml; axXml = nextXMLEle(devXml, 0))
        {
            if (strcmp(tagXMLEle(axXml), "axis") != 0)
                continue;

            XMLAtt *idxAtt = findXMLAtt(axXml, "index");
            if (!idxAtt) continue;

            const int idx = atoi(valuXMLAtt(idxAtt));
            const int nAxis = static_cast<int>(AxisNP.size());
            if (idx < 0 || idx >= nAxis)
                continue;

            XMLEle *centerXml = findXMLEle(axXml, "center");
            XMLEle *minXml    = findXMLEle(axXml, "min");
            XMLEle *maxXml    = findXMLEle(axXml, "max");

            if (!centerXml || !minXml || !maxXml)
                continue;

            const int center = atoi(pcdataXMLEle(centerXml));
            const int minVal = atoi(pcdataXMLEle(minXml));
            const int maxVal = atoi(pcdataXMLEle(maxXml));

            AxisCalibrationNP[idx * 3 + 0].setValue(center);
            AxisCalibrationNP[idx * 3 + 1].setValue(minVal);
            AxisCalibrationNP[idx * 3 + 2].setValue(maxVal);

            m_axisCalCenter[idx] = center;
            m_axisCalMin[idx]    = minVal;
            m_axisCalMax[idx]    = maxVal;
        }
        break;
    }

    delXMLEle(root);

    if (found)
    {
        AxisCalibrationNP.setState(IPS_OK);
        AxisCalibrationNP.apply();
        applyCalibrationToDriver();
    }

    return found;
}

// ─────────────────────────────────────────────────────────────────────────────
// saveCalibrationData
// ─────────────────────────────────────────────────────────────────────────────

bool JoyStick::saveCalibrationData()
{
    if (!isConnected() || driver == nullptr)
        return false;

    const char *hwName = driver->getName();
    if (!hwName || hwName[0] == '\0')
        return false;

    // ── Step 1: Load existing file so we preserve other devices' entries ────
    XMLEle *root = nullptr;
    XMLEle *existingDevXml = nullptr; // entry for THIS device, if already present

    wordexp_t wexp;
    if (wordexp(m_calibrationFile.c_str(), &wexp, 0) == 0)
    {
        FILE *fp = fopen(wexp.we_wordv[0], "r");
        if (fp)
        {
            LilXML *lp = newLilXML();
            static char errmsg[512];
            root = readXMLFile(fp, lp, errmsg);
            fclose(fp);
            delLilXML(lp);
        }
        wordfree(&wexp);
    }

    // If the file was empty / missing / corrupt, create a fresh root element
    if (!root)
        root = addXMLEle(nullptr, "joystickcalibration");

    // ── Step 2: Find (or create) the <device name="…"> child for us ─────────
    for (XMLEle *devXml = nextXMLEle(root, 1); devXml; devXml = nextXMLEle(root, 0))
    {
        if (strcmp(tagXMLEle(devXml), "device") != 0)
            continue;
        XMLAtt *nameAtt = findXMLAtt(devXml, "name");
        if (nameAtt && strcmp(valuXMLAtt(nameAtt), hwName) == 0)
        {
            existingDevXml = devXml;
            break;
        }
    }

    // Remove the old entry so we can replace it cleanly
    if (existingDevXml)
        delXMLEle(existingDevXml);

    XMLEle *devXml = addXMLEle(root, "device");
    addXMLAtt(devXml, "name", hwName);

    // ── Step 3: Write per-axis elements ─────────────────────────────────────
    const int nAxis = static_cast<int>(AxisNP.size());
    char buf[32];

    for (int i = 0; i < nAxis; i++)
    {
        snprintf(buf, sizeof(buf), "%d", i);
        XMLEle *axXml = addXMLEle(devXml, "axis");
        addXMLAtt(axXml, "index", buf);

        XMLEle *centerXml = addXMLEle(axXml, "center");
        XMLEle *minXml    = addXMLEle(axXml, "min");
        XMLEle *maxXml    = addXMLEle(axXml, "max");

        snprintf(buf, sizeof(buf), "%d", static_cast<int>(AxisCalibrationNP[i * 3 + 0].getValue()));
        editXMLEle(centerXml, buf);

        snprintf(buf, sizeof(buf), "%d", static_cast<int>(AxisCalibrationNP[i * 3 + 1].getValue()));
        editXMLEle(minXml, buf);

        snprintf(buf, sizeof(buf), "%d", static_cast<int>(AxisCalibrationNP[i * 3 + 2].getValue()));
        editXMLEle(maxXml, buf);
    }

    // ── Step 4: Write the XML file ───────────────────────────────────────────
    if (wordexp(m_calibrationFile.c_str(), &wexp, 0) != 0)
    {
        wordfree(&wexp);
        delXMLEle(root);
        LOGF_ERROR("saveCalibrationData: bad filename: %s", m_calibrationFile.c_str());
        return false;
    }

    FILE *fp = fopen(wexp.we_wordv[0], "w");
    wordfree(&wexp);

    if (!fp)
    {
        delXMLEle(root);
        LOGF_ERROR("saveCalibrationData: cannot open %s: %s",
                   m_calibrationFile.c_str(), strerror(errno));
        return false;
    }

    prXMLEle(fp, root, 0);
    fclose(fp);
    delXMLEle(root);

    return true;
}
