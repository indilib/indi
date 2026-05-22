/*******************************************************************************
  Copyright(c) 2013-2026 Jasem Mutlaq. All rights reserved.

  Based on code by Keith Lantz.

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

#include <iostream>
#include <functional>
#include <algorithm>
#include <fcntl.h>
#include <pthread.h>
#include <cmath>
#include <cstdint>
#include <linux/joystick.h>
#include <vector>
#include <unistd.h>

#define JOYSTICK_DEV "/dev/input/js0"

// ─────────────────────────────────────────────────────────────────────────────
// ABS event-code constants (subset of linux/input-event-codes.h).
// Duplicated so we don't require a separate evdev header.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef ABS_X
#define ABS_X        0x00
#define ABS_Y        0x01
#define ABS_Z        0x02   ///< L2 / left trigger
#define ABS_RX       0x03
#define ABS_RY       0x04
#define ABS_RZ       0x05   ///< R2 / right trigger
#define ABS_THROTTLE 0x06
#define ABS_RUDDER   0x07
#define ABS_WHEEL    0x08
#define ABS_GAS      0x09
#define ABS_BRAKE    0x0a
#define ABS_HAT0X    0x10
#define ABS_HAT0Y    0x11
#define ABS_HAT1X    0x12
#define ABS_HAT1Y    0x13
#define ABS_HAT2X    0x14
#define ABS_HAT2Y    0x15
#define ABS_HAT3X    0x16
#define ABS_HAT3Y    0x17
#endif

typedef struct
{
    float theta, r, x, y;
} joystick_position;

typedef struct
{
    std::vector<signed short> button;
    std::vector<signed short> axis;
} joystick_state;

/**
 * @brief Axis type classification inferred from JSIOCGAXMAP.
 */
enum class JoyAxisType
{
    STICK,    ///< Analog stick (ABS_X/Y, ABS_RX/RY, throttle, rudder, wheel)
    TRIGGER,  ///< Analog trigger — single-axis input (ABS_Z/RZ, gas, brake)
    HAT,      ///< Digital hat / d-pad (ABS_HAT0X/Y … ABS_HAT3Y)
    UNKNOWN
};

/**
 * @brief A matched pair of axis indices that together form a joystick.
 */
struct JoyAxisPair
{
    int     i0;     ///< Index of the horizontal (X) axis
    int     i1;     ///< Index of the vertical (Y) axis
    uint8_t absX;   ///< ABS_* code for the X axis
    uint8_t absY;   ///< ABS_* code for the Y axis
    bool    isHat;  ///< True for digital hat / d-pad pairs
    const char *name; ///< Human-readable name (e.g. "Left Stick")
};

/**
 * @brief The JoyStickDriver class provides basic functionality to read events
 * from supported game pads under Linux.
 *
 * The driver uses JSIOCGAXMAP to correctly identify analog sticks, triggers
 * and d-pads.  Joystick pairs (magnitude + angle) are only generated for
 * genuine stick/hat axis pairs; trigger axes appear only as raw axis values.
 *
 * Each joystick has a normalised magnitude [0, 1] and a compass angle
 * (N = 0°, E = 90°, S = 180°, W = 270°).
 */
class JoyStickDriver
{
    public:
        JoyStickDriver();
        ~JoyStickDriver();

        typedef std::function<void(int joystick_n, double mag, double angle)> joystickFunc;
        typedef std::function<void(int axis_n, double value)> axisFunc;
        typedef std::function<void(int button_n, int value)> buttonFunc;

        bool Connect();
        bool Disconnect();

        void setPort(const char *port);
        void setPoll(int ms);

        const char *getName();
        uint32_t getVersion();

        // ── Axis / joystick counts ────────────────────────────────────────
        uint8_t getNumOfAxes();
        uint8_t getNumrOfButtons();

        /**
         * @brief getNumOfJoysticks Returns the number of proper analog-stick /
         * hat pairs found via JSIOCGAXMAP.  Falls back to axes/2 if the ioctl
         * is unavailable.
         */
        uint8_t getNumOfJoysticks();

        // ── Per-axis type information ─────────────────────────────────────
        /**
         * @brief getAxisAbsCode Returns the ABS_* event code for @p axis, or
         * 0xFF if the axis map is not available.
         */
        uint8_t getAxisAbsCode(int axis) const;

        /**
         * @brief getAxisType Returns the classified type of @p axis.
         */
        JoyAxisType getAxisType(int axis) const;

        /**
         * @brief getAxisLabel Returns a human-readable label for @p axis
         * (e.g. "Left-Stick X", "L2 Trigger", "D-pad X").
         */
        const char *getAxisLabel(int axis) const;

        /**
         * @brief getJoystickPair Returns the axis-pair descriptor for joystick
         * @p n, or nullptr if out of range.
         */
        const JoyAxisPair *getJoystickPair(int n) const;

        // ── Position / button queries ─────────────────────────────────────
        joystick_position joystickPosition(int n);
        bool buttonPressed(int n);

        // ── Callbacks ─────────────────────────────────────────────────────
        void setJoystickCallback(joystickFunc joystickCallback);
        void setAxisCallback(axisFunc axisCallback);
        void setButtonCallback(buttonFunc buttonCallback);

        // ── Calibration ───────────────────────────────────────────────────
        /**
         * @brief setAxisCalibration Set piecewise-linear calibration for one axis.
         *
         * Maps [minVal .. center .. maxVal] → [-1 .. 0 .. +1].
         *
         * @param axis     Zero-based axis index.
         * @param center   Raw value at rest / center.
         * @param minVal   Raw value at full negative deflection.
         * @param maxVal   Raw value at full positive deflection.
         */
        void setAxisCalibration(int axis, int center, int minVal, int maxVal);

        /**
         * @brief setAxisReversed Enable or disable reversal for a single axis.
         *
         * When reversed, the raw axis value is negated before calibration is
         * applied.  This mirrors the physical axis so that, for example, pushing
         * a stick UP produces a positive normalised value even if the hardware
         * reports negative values for that direction.
         *
         * @param axis     Zero-based axis index.
         * @param reversed true to negate the raw value, false for normal polarity.
         */
        void setAxisReversed(int axis, bool reversed);

    protected:
        static void joystickEvent(int joystick_n, double mag, double angle);
        static void axisEvent(int axis_n, int value);
        static void buttonEvent(int button_n, int value);

        static void *loop(void *obj);
        void readEv();

        joystickFunc joystickCallbackFunc;
        buttonFunc   buttonCallbackFunc;
        axisFunc     axisCallbackFunc;

    private:
        pthread_t thread;
        bool active;
        int joystick_fd;
        js_event      *joystick_ev;
        joystick_state *joystick_st;
        uint32_t version;
        uint8_t  axes;
        uint8_t  buttons;
        char name[256];
        char dev_path[256];
        int  pollMS;

        // ── Axis map (from JSIOCGAXMAP) ───────────────────────────────────
        /// ABS_* code for each axis (length = axes).  Empty if ioctl failed.
        std::vector<uint8_t> m_axmap;

        /// Ordered list of joystick axis pairs (analog sticks + hats).
        std::vector<JoyAxisPair> m_stickPairs;

        /// For each axis, the index into m_stickPairs (-1 = not part of any pair).
        std::vector<int> m_axisToPairIndex;

        // ── Per-axis calibration ──────────────────────────────────────────
        std::vector<int>  m_calCenter;
        std::vector<int>  m_calMin;
        std::vector<int>  m_calMax;

        // ── Per-axis reversal ─────────────────────────────────────────────
        /// When true for axis i, the raw value is negated before calibration.
        std::vector<bool> m_axisReversed;

        // ── Helpers ───────────────────────────────────────────────────────
        float normalizeAxis(int value, int idx) const;

        /// Classify an ABS_* code
        static JoyAxisType classifyAbsCode(uint8_t absCode);

        /// Human-readable label for an ABS_* code
        static const char *absCodeLabel(uint8_t code);

        /// Build m_stickPairs and m_axisToPairIndex from m_axmap.
        void buildAxisPairs();
};
