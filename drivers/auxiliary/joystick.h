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

#pragma once

#include "defaultdevice.h"
#include "indipropertynumber.h"
#include "indipropertyswitch.h"
#include "indipropertytext.h"

#include <memory>
#include <string>
#include <vector>
#include <cmath>

class JoyStickDriver;

/**
 * @brief The JoyStick class provides an INDI driver that displays event data from game pads. The INDI driver can be encapsulated in any other driver
 * via snooping on properties of interesting.
 *
 * Per-axis calibration is supported: record the center (rest) position and the
 * physical min/max of each axis so that the reported magnitude and angle are
 * deterministic regardless of hardware variation.  Calibration data is stored
 * per joystick name in ~/.indi/JoystickCalibrationData.xml and is reloaded
 * automatically when the same device reconnects.
 */
class JoyStick : public INDI::DefaultDevice
{
    public:
        JoyStick();

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        static void joystickHelper(int joystick_n, double mag, double angle);
        static void axisHelper(int axis_n, int value);
        static void buttonHelper(int button_n, int value);

    protected:
        //  Generic indi device entries
        virtual bool Connect() override;
        virtual bool Disconnect() override;
        virtual const char *getDefaultName() override;

        bool saveConfigItems(FILE *fp) override;

        void setupParams();

        void joystickEvent(int joystick_n, double mag, double angle);
        void axisEvent(int axis_n, int value);
        void buttonEvent(int button_n, int value);

        // One PropertyNumber per joystick (each has 2 elements: Magnitude + Angle)
        std::vector<INDI::PropertyNumber> JoyStickNP;

        // Axis raw values and dead zones (sized to number of axes at connect time)
        INDI::PropertyNumber AxisNP {0};
        INDI::PropertyNumber DeadZoneNP {0};

        // EMA smoothing factor α ∈ (0, 1]: 1 = no filter, lower = more smoothing
        INDI::PropertyNumber FilterNP {1};

        // Buttons
        INDI::PropertySwitch ButtonSP {0};

        // Device port
        INDI::PropertyText PortTP {1};

        // Joystick information (name, version, counts)
        INDI::PropertyText JoystickInfoTP {5};

        // ── Calibration ──────────────────────────────────────────────────────────
        /**
         * @brief CalibrationSP Start / End the calibration sweep.
         *
         * CALIBRATION_START  – snapshot current axis values as center; begin
         *                      tracking min/max.  User should be at rest when
         *                      clicking this.
         * CALIBRATION_END    – stop tracking, save results to XML file, apply
         *                      values to the low-level driver.
         */
        INDI::PropertySwitch CalibrationSP {2};

        /**
         * @brief AxisCalibrationNP Per-axis calibration values (IP_RW).
         *
         * Three elements per axis, laid out sequentially:
         *   AXIS_N_CENTER – raw value at rest / center.
         *   AXIS_N_MIN    – raw value at maximum negative deflection.
         *   AXIS_N_MAX    – raw value at maximum positive deflection.
         *
         * Allocated dynamically in setupParams() to nAxes×3 elements.
         * The user can also hand-edit these values at any time.
         */
        INDI::PropertyNumber AxisCalibrationNP {0};

        /**
         * @brief AxisReverseSP Per-axis polarity reversal toggles (IP_RW).
         *
         * One switch element per axis using ISR_NOFMANY so each toggle is
         * independent.  When a switch is ON, the raw axis value is negated
         * before calibration, effectively mirroring that axis.
         *
         * Reversing both axes of a joystick pair produces the same effect as
         * a full 180° reversal of the stick.
         *
         * Allocated dynamically in setupParams() to nAxes elements.
         * Settings are saved to the standard INDI config file (~/.indi/Joystick.xml).
         */
        INDI::PropertySwitch AxisReverseSP {0};

        std::unique_ptr<JoyStickDriver> driver;

        // Per-axis EMA state (allocated at connect time)
        std::vector<double> m_axisEMA;

        // Circular EMA state for joystick angle (cos/sin components to handle 0°/360° wraparound)
        std::vector<double> m_joystickAngleCosEMA;
        std::vector<double> m_joystickAngleSinEMA;

    private:
        // ── Calibration helpers ───────────────────────────────────────────────────

        /// True while a calibration sweep is in progress.
        bool m_calibrating {false};

        /// Per-axis live min/max accumulated during a calibration sweep.
        std::vector<int> m_axisCalCenter;
        std::vector<int> m_axisCalMin;
        std::vector<int> m_axisCalMax;

        /// Full path to the XML calibration file (set in initProperties).
        std::string m_calibrationFile;

        /**
         * @brief loadCalibrationData Read the XML calibration file.
         *
         * If the file contains an entry whose device name attribute matches the
         * connected hardware name exactly, the stored center/min/max values are
         * applied to AxisCalibrationNP and pushed to the low-level driver.
         *
         * @return true if a matching entry was found and applied.
         */
        bool loadCalibrationData();

        /**
         * @brief saveCalibrationData Persist current calibration to the XML file.
         *
         * Existing entries for other joystick names are preserved.  The entry for
         * the currently-connected joystick is created or overwritten.
         *
         * @return true on success.
         */
        bool saveCalibrationData();

        /**
         * @brief applyCalibrationToDriver Push the current AxisCalibrationNP
         *        values to the low-level driver for every axis.
         */
        void applyCalibrationToDriver();

        /**
         * @brief applyReverseToDriver Push the current AxisReverseSP state
         *        to the low-level driver for every axis.
         */
        void applyReverseToDriver();
};
