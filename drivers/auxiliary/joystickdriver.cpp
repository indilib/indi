/*******************************************************************************
  Copyright(c) 2012-2026 Jasem Mutlaq. All rights reserved.

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

#include "joystickdriver.h"

#include <cstring>
#include <sys/ioctl.h>

// ─────────────────────────────────────────────────────────────────────────────
// Axis-pair table: each entry is (absX, absY, isHat, name).
// We search for both codes in the axis map and, if both are found, create a
// JoyAxisPair.  Order matters — we report pairs in this sequence.
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
struct PairDef
{
    uint8_t    absX, absY;
    bool       isHat;
    const char *name;
};

static const PairDef kKnownPairs[] =
{
    {ABS_X,        ABS_Y,        false, "Left Stick"},
    {ABS_RX,       ABS_RY,       false, "Right Stick"},
    {ABS_THROTTLE, ABS_RUDDER,   false, "Throttle/Rudder"},
    {ABS_WHEEL,    ABS_RUDDER,   false, "Wheel/Rudder"},
    {ABS_HAT0X,    ABS_HAT0Y,    true,  "D-pad (Hat 0)"},
    {ABS_HAT1X,    ABS_HAT1Y,    true,  "Hat 1"},
    {ABS_HAT2X,    ABS_HAT2Y,    true,  "Hat 2"},
    {ABS_HAT3X,    ABS_HAT3Y,    true,  "Hat 3"},
};
static const int kNumKnownPairs = sizeof(kKnownPairs) / sizeof(kKnownPairs[0]);
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// JoyStickDriver — constructor / destructor
// ─────────────────────────────────────────────────────────────────────────────

JoyStickDriver::JoyStickDriver()
{
    active      = false;
    pollMS      = 100;
    joystick_fd = 0;
    joystick_ev = new js_event();
    joystick_st = new joystick_state();

    strncpy(dev_path, JOYSTICK_DEV, 256);

    joystickCallbackFunc = joystickEvent;
    axisCallbackFunc     = axisEvent;
    buttonCallbackFunc   = buttonEvent;
}

JoyStickDriver::~JoyStickDriver()
{
    Disconnect();
    delete joystick_st;
    delete joystick_ev;
}

// ─────────────────────────────────────────────────────────────────────────────
// Connect / Disconnect
// ─────────────────────────────────────────────────────────────────────────────

bool JoyStickDriver::Connect()
{
    joystick_fd = open(dev_path, O_RDONLY | O_NONBLOCK);
    if (joystick_fd <= 0)
        return false;

    ioctl(joystick_fd, JSIOCGNAME(256), name);
    ioctl(joystick_fd, JSIOCGVERSION, &version);
    ioctl(joystick_fd, JSIOCGAXES,    &axes);
    ioctl(joystick_fd, JSIOCGBUTTONS, &buttons);

    joystick_st->axis.resize(axes);
    joystick_st->button.resize(buttons);

    // ── Query axis-to-ABS map ─────────────────────────────────────────────
    // IMPORTANT: JSIOCGAXMAP always writes ABS_MAX+1 = 64 bytes regardless
    // of the device's actual axis count.  Using a smaller destination buffer
    // causes heap corruption.  Use a fixed 64-byte scratch buffer and copy
    // only the relevant entries into m_axmap.
    {
        const int ABS_BUF_SIZE = 64; // ABS_MAX+1
        uint8_t axmapBuf[ABS_BUF_SIZE] = {};
        m_axmap.resize(axes, 0);
        if (ioctl(joystick_fd, JSIOCGAXMAP, axmapBuf) < 0)
        {
            // Not available on older kernels — fall back to sequential codes.
            for (int i = 0; i < axes; i++)
                m_axmap[i] = static_cast<uint8_t>(i);
        }
        else
        {
            for (int i = 0; i < axes; i++)
                m_axmap[i] = axmapBuf[i];
        }
    }

    // ── Build axis pairs ──────────────────────────────────────────────────
    buildAxisPairs();

    // ── Per-axis calibration defaults ─────────────────────────────────────
    m_calCenter.assign(axes, 0);
    m_calMin.assign(axes, -32767);
    m_calMax.assign(axes,  32767);

    // ── Per-axis reversal defaults (all normal polarity) ──────────────────
    m_axisReversed.assign(axes, false);

    active = true;
    pthread_create(&thread, nullptr, &JoyStickDriver::loop, this);
    return true;
}

bool JoyStickDriver::Disconnect()
{
    if (joystick_fd > 0)
    {
        active = false;
        pthread_join(thread, nullptr);
        close(joystick_fd);
    }
    joystick_fd = 0;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildAxisPairs
// ─────────────────────────────────────────────────────────────────────────────

void JoyStickDriver::buildAxisPairs()
{
    m_stickPairs.clear();
    m_axisToPairIndex.assign(axes, -1);

    for (int p = 0; p < kNumKnownPairs; p++)
    {
        const PairDef &def = kKnownPairs[p];
        int idx0 = -1, idx1 = -1;

        for (int i = 0; i < axes; i++)
        {
            if (m_axmap[i] == def.absX) idx0 = i;
            if (m_axmap[i] == def.absY) idx1 = i;
        }

        if (idx0 >= 0 && idx1 >= 0)
        {
            // Skip if either axis is already claimed by an earlier pair
            if (m_axisToPairIndex[idx0] >= 0 || m_axisToPairIndex[idx1] >= 0)
                continue;

            JoyAxisPair pair;
            pair.i0    = idx0;
            pair.i1    = idx1;
            pair.absX  = def.absX;
            pair.absY  = def.absY;
            pair.isHat = def.isHat;
            pair.name  = def.name;

            int pairIdx = static_cast<int>(m_stickPairs.size());
            m_stickPairs.push_back(pair);
            m_axisToPairIndex[idx0] = pairIdx;
            m_axisToPairIndex[idx1] = pairIdx;
        }
    }

    // If JSIOCGAXMAP was not available (all codes are sequential 0,1,2,…) and
    // no known pairs were found, fall back to the legacy even/odd pairing.
    if (m_stickPairs.empty())
    {
        for (int i = 0; i + 1 < axes; i += 2)
        {
            JoyAxisPair pair;
            pair.i0    = i;
            pair.i1    = i + 1;
            pair.absX  = static_cast<uint8_t>(i);
            pair.absY  = static_cast<uint8_t>(i + 1);
            pair.isHat = false;
            pair.name  = "Joystick";

            int pairIdx = static_cast<int>(m_stickPairs.size());
            m_stickPairs.push_back(pair);
            m_axisToPairIndex[i]     = pairIdx;
            m_axisToPairIndex[i + 1] = pairIdx;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// classifyAbsCode / absCodeLabel
// ─────────────────────────────────────────────────────────────────────────────

JoyAxisType JoyStickDriver::classifyAbsCode(uint8_t code)
{
    switch (code)
    {
        case ABS_X:
        case ABS_Y:
        case ABS_RX:
        case ABS_RY:
        case ABS_THROTTLE:
        case ABS_RUDDER:
        case ABS_WHEEL:
            return JoyAxisType::STICK;
        case ABS_Z:
        case ABS_RZ:
        case ABS_GAS:
        case ABS_BRAKE:
            return JoyAxisType::TRIGGER;
        case ABS_HAT0X:
        case ABS_HAT0Y:
        case ABS_HAT1X:
        case ABS_HAT1Y:
        case ABS_HAT2X:
        case ABS_HAT2Y:
        case ABS_HAT3X:
        case ABS_HAT3Y:
            return JoyAxisType::HAT;
        default:
            return JoyAxisType::UNKNOWN;
    }
}

const char *JoyStickDriver::absCodeLabel(uint8_t code)
{
    switch (code)
    {
        case ABS_X:
            return "Left-Stick X";
        case ABS_Y:
            return "Left-Stick Y";
        case ABS_Z:
            return "L2 Trigger";
        case ABS_RX:
            return "Right-Stick X";
        case ABS_RY:
            return "Right-Stick Y";
        case ABS_RZ:
            return "R2 Trigger";
        case ABS_THROTTLE:
            return "Throttle";
        case ABS_RUDDER:
            return "Rudder";
        case ABS_WHEEL:
            return "Wheel";
        case ABS_GAS:
            return "Gas";
        case ABS_BRAKE:
            return "Brake";
        case ABS_HAT0X:
            return "D-pad X";
        case ABS_HAT0Y:
            return "D-pad Y";
        case ABS_HAT1X:
            return "Hat1 X";
        case ABS_HAT1Y:
            return "Hat1 Y";
        case ABS_HAT2X:
            return "Hat2 X";
        case ABS_HAT2Y:
            return "Hat2 Y";
        case ABS_HAT3X:
            return "Hat3 X";
        case ABS_HAT3Y:
            return "Hat3 Y";
        default:
            return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public accessors
// ─────────────────────────────────────────────────────────────────────────────

void JoyStickDriver::setJoystickCallback(joystickFunc f)
{
    joystickCallbackFunc = f;
}
void JoyStickDriver::setAxisCallback(axisFunc f)
{
    axisCallbackFunc     = f;
}
void JoyStickDriver::setButtonCallback(buttonFunc f)
{
    buttonCallbackFunc   = f;
}

void JoyStickDriver::setPort(const char *port)
{
    snprintf(dev_path, 256, "%s", port);
}
void JoyStickDriver::setPoll(int ms)
{
    pollMS = ms;
}

const char *JoyStickDriver::getName()
{
    return name;
}
uint32_t    JoyStickDriver::getVersion()
{
    return version;
}
uint8_t     JoyStickDriver::getNumOfAxes()
{
    return axes;
}
uint8_t     JoyStickDriver::getNumrOfButtons()
{
    return buttons;
}

uint8_t JoyStickDriver::getNumOfJoysticks()
{
    if (!m_stickPairs.empty())
        return static_cast<uint8_t>(m_stickPairs.size());

    // Legacy fallback
    int n = axes / 2;
    if (axes % 2 != 0) n++;
    return static_cast<uint8_t>(n);
}

uint8_t JoyStickDriver::getAxisAbsCode(int axis) const
{
    if (axis < 0 || axis >= static_cast<int>(m_axmap.size()))
        return 0xFF;
    return m_axmap[axis];
}

JoyAxisType JoyStickDriver::getAxisType(int axis) const
{
    if (axis < 0 || axis >= static_cast<int>(m_axmap.size()))
        return JoyAxisType::UNKNOWN;
    return classifyAbsCode(m_axmap[axis]);
}

const char *JoyStickDriver::getAxisLabel(int axis) const
{
    if (axis < 0 || axis >= static_cast<int>(m_axmap.size()))
        return "Unknown";
    return absCodeLabel(m_axmap[axis]);
}

const JoyAxisPair *JoyStickDriver::getJoystickPair(int n) const
{
    if (n < 0 || n >= static_cast<int>(m_stickPairs.size()))
        return nullptr;
    return &m_stickPairs[n];
}

void JoyStickDriver::setAxisCalibration(int axis, int center, int minVal, int maxVal)
{
    if (axis < 0 || axis >= static_cast<int>(m_calCenter.size()))
        return;
    m_calCenter[axis] = center;
    m_calMin[axis]    = minVal;
    m_calMax[axis]    = maxVal;
}

void JoyStickDriver::setAxisReversed(int axis, bool reversed)
{
    if (axis < 0 || axis >= static_cast<int>(m_axisReversed.size()))
        return;
    m_axisReversed[axis] = reversed;
}

bool JoyStickDriver::buttonPressed(int n)
{
    return n > -1 && n < buttons ? joystick_st->button[n] : 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Event loop
// ─────────────────────────────────────────────────────────────────────────────

void *JoyStickDriver::loop(void *obj)
{
    while (reinterpret_cast<JoyStickDriver *>(obj)->active)
        reinterpret_cast<JoyStickDriver *>(obj)->readEv();
    return obj;
}

void JoyStickDriver::readEv()
{
    int bytes = read(joystick_fd, joystick_ev, sizeof(*joystick_ev));
    if (bytes > 0)
    {
        joystick_ev->type &= ~JS_EVENT_INIT;

        if (joystick_ev->type & JS_EVENT_BUTTON)
        {
            const int n = joystick_ev->number;
            if (n < static_cast<int>(joystick_st->button.size()))
            {
                joystick_st->button[n] = joystick_ev->value;
                buttonCallbackFunc(n, joystick_ev->value);
            }
        }

        if (joystick_ev->type & JS_EVENT_AXIS)
        {
            const int axIdx = joystick_ev->number;
            if (axIdx >= static_cast<int>(joystick_st->axis.size()))
                return;

            joystick_st->axis[axIdx] = joystick_ev->value;

            // Dispatch axis callback (raw value)
            axisCallbackFunc(axIdx, joystick_ev->value);

            // Dispatch joystick callback if this axis belongs to a pair
            if (axIdx < static_cast<int>(m_axisToPairIndex.size()))
            {
                const int pairIdx = m_axisToPairIndex[axIdx];
                if (pairIdx >= 0)
                {
                    joystick_position pos = joystickPosition(pairIdx);

                    // Reject noise
                    if (pos.r < 0.001f)
                    {
                        pos.r     = 0.0f;
                        pos.theta = 0.0f;
                    }

                    joystickCallbackFunc(pairIdx, pos.r, pos.theta);
                }
            }
        }
    }
    else
        usleep(pollMS * 1000);
}

// ─────────────────────────────────────────────────────────────────────────────
// normalizeAxis — piecewise linear [min..center..max] → [-1..0..+1]
// ─────────────────────────────────────────────────────────────────────────────

float JoyStickDriver::normalizeAxis(int value, int idx) const
{
    if (idx < 0 || idx >= static_cast<int>(m_calCenter.size()))
        return value / 32767.0f;

    // Apply per-axis reversal before calibration so center/min/max remain valid.
    if (idx < static_cast<int>(m_axisReversed.size()) && m_axisReversed[idx])
        value = -value;

    const int center = m_calCenter[idx];
    if (value >= center)
    {
        const int span = m_calMax[idx] - center;
        if (span <= 0) return 0.0f;
        return std::min(1.0f, static_cast<float>(value - center) / static_cast<float>(span));
    }
    else
    {
        const int span = center - m_calMin[idx];
        if (span <= 0) return 0.0f;
        return std::max(-1.0f, static_cast<float>(value - center) / static_cast<float>(span));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// joystickPosition — uses m_stickPairs for correct axis pairing
// ─────────────────────────────────────────────────────────────────────────────

joystick_position JoyStickDriver::joystickPosition(int n)
{
    joystick_position pos;
    pos.theta = pos.r = pos.x = pos.y = 0.0f;

    if (n < 0 || n >= static_cast<int>(m_stickPairs.size()))
        return pos;

    const JoyAxisPair &pair = m_stickPairs[n];
    const int i0 = pair.i0;
    const int i1 = pair.i1;

    if (i0 >= static_cast<int>(joystick_st->axis.size()) ||
            i1 >= static_cast<int>(joystick_st->axis.size()))
        return pos;

    // Apply per-axis piecewise-linear calibration → [-1, +1].
    // Y axis is negated: hardware UP = negative raw value.
    const float x0 =  normalizeAxis(joystick_st->axis[i0], i0);
    const float y0 = -normalizeAxis(joystick_st->axis[i1], i1);

    // Squircle mapping to keep diagonal magnitude ≈ 1 when both axes are
    // fully deflected.
    const float x = x0 * std::sqrt(1.0f - 0.5f * y0 * y0);
    const float y = y0 * std::sqrt(1.0f - 0.5f * x0 * x0);

    pos.x = x0;
    pos.y = y0;
    pos.r = std::sqrt(x * x + y * y);

    // Compass / bearing convention: N=0°, E=90°, S=180°, W=270°.
    if (pos.r > 0.001f)
    {
        pos.theta = std::atan2(x, y) * (180.0f / 3.141592653589793f);
        if (pos.theta < 0.0f)
            pos.theta += 360.0f;
    }
    else
        pos.theta = 0.0f;

    return pos;
}

// ─────────────────────────────────────────────────────────────────────────────
// Default (no-op) static callbacks
// ─────────────────────────────────────────────────────────────────────────────

void JoyStickDriver::joystickEvent(int joystick_n, double mag, double angle)
{
    (void)joystick_n;
    (void)mag;
    (void)angle;
}

void JoyStickDriver::axisEvent(int axis_n, int value)
{
    (void)axis_n;
    (void)value;
}

void JoyStickDriver::buttonEvent(int button_n, int button_value)
{
    (void)button_n;
    (void)button_value;
}
