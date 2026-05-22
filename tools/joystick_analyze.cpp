/*******************************************************************************
 * joystick_analyze.cpp — Standalone gamepad axis analysis & calibration
 * diagnostic tool.  Uses JSIOCGAXMAP to correctly identify axis types
 * (analog stick, trigger, d-pad/hat) and skips non-stick axes from the
 * interactive calibration procedure.
 *
 * Compile:
 *   g++ -std=c++17 -O2 -o /tmp/joystick_analyze \
 *       /home/jasem/Projects/indi/tools/joystick_analyze.cpp -lm
 *
 * Usage:
 *   /tmp/joystick_analyze [device]
 *   e.g.  /tmp/joystick_analyze /dev/input/js2
 ******************************************************************************/

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <linux/joystick.h>
#include <pwd.h>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// ABS event-code constants (from linux/input-event-codes.h)
// Duplicated here so we don't need to pull in the full evdev header.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef ABS_X
#define ABS_X        0x00
#define ABS_Y        0x01
#define ABS_Z        0x02   // L2 / left trigger
#define ABS_RX       0x03
#define ABS_RY       0x04
#define ABS_RZ       0x05   // R2 / right trigger
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

// ─────────────────────────────────────────────────────────────────────────────
// Axis classification
// ─────────────────────────────────────────────────────────────────────────────
enum class AxisType { STICK, TRIGGER, HAT, UNKNOWN };

static AxisType classifyAxis(uint8_t absCode)
{
    switch (absCode)
    {
        case ABS_X:
        case ABS_Y:
        case ABS_RX:
        case ABS_RY:
        case ABS_THROTTLE:
        case ABS_RUDDER:
        case ABS_WHEEL:
            return AxisType::STICK;
        case ABS_Z:
        case ABS_RZ:
        case ABS_GAS:
        case ABS_BRAKE:
            return AxisType::TRIGGER;
        case ABS_HAT0X:
        case ABS_HAT0Y:
        case ABS_HAT1X:
        case ABS_HAT1Y:
        case ABS_HAT2X:
        case ABS_HAT2Y:
        case ABS_HAT3X:
        case ABS_HAT3Y:
            return AxisType::HAT;
        default:
            return AxisType::UNKNOWN;
    }
}

static const char *axisTypeName(AxisType t)
{
    switch (t)
    {
        case AxisType::STICK:
            return "STICK";
        case AxisType::TRIGGER:
            return "TRIGGER";
        case AxisType::HAT:
            return "HAT/D-PAD";
        default:
            return "UNKNOWN";
    }
}

// Human-readable label for an ABS code
static const char *absCodeLabel(uint8_t code)
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
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::string homeDir()
{
    const char *h = getenv("HOME");
    if (!h)
    {
        struct passwd *pw = getpwuid(getuid());
        if (pw) h = pw->pw_dir;
    }
    return h ? std::string(h) : "/tmp";
}

// Piecewise linear normalisation: [min..center..max] → [-1..0..+1]
static double normalise(int value, int center, int minVal, int maxVal)
{
    if (value >= center)
    {
        int span = maxVal - center;
        if (span <= 0) return 0.0;
        return std::min(1.0, static_cast<double>(value - center) / span);
    }
    else
    {
        int span = center - minVal;
        if (span <= 0) return 0.0;
        return std::max(-1.0, static_cast<double>(value - center) / span);
    }
}

// Compass angle (x=East, y=North): N=0°, E=90°, S=180°, W=270°
static double compassAngle(double x, double y)
{
    double a = std::atan2(x, y) * (180.0 / M_PI);
    if (a < 0.0) a += 360.0;
    return a;
}

// ─────────────────────────────────────────────────────────────────────────────
// Window: collect raw axis extremes over a time window
// ─────────────────────────────────────────────────────────────────────────────
struct AxisWindow
{
    int min0 {INT_MAX}, max0 {INT_MIN};
    int min1 {INT_MAX}, max1 {INT_MIN};
};

static AxisWindow collectWindow(int fd, int i0, int i1,
                                const std::vector<int> &state,
                                int durationMs)
{
    AxisWindow w;
    w.min0 = w.max0 = state[i0];
    w.min1 = w.max1 = state[i1];

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(durationMs);

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    while (std::chrono::steady_clock::now() < deadline)
    {
        js_event ev;
        int bytes = read(fd, &ev, sizeof(ev));
        if (bytes == sizeof(ev))
        {
            ev.type &= ~JS_EVENT_INIT;
            if (ev.type & JS_EVENT_AXIS)
            {
                if (ev.number == i0)
                {
                    w.min0 = std::min(w.min0, (int)ev.value);
                    w.max0 = std::max(w.max0, (int)ev.value);
                }
                else if (ev.number == i1)
                {
                    w.min1 = std::min(w.min1, (int)ev.value);
                    w.max1 = std::max(w.max1, (int)ev.value);
                }
            }
        }
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    fcntl(fd, F_SETFL, flags);
    return w;
}

// Single-axis window for triggers
struct SingleWindow
{
    int minVal {INT_MAX}, maxVal {INT_MIN};
};

static SingleWindow collectSingle(int fd, int idx,
                                  const std::vector<int> &state,
                                  int durationMs)
{
    SingleWindow w;
    w.minVal = w.maxVal = state[idx];

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(durationMs);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    while (std::chrono::steady_clock::now() < deadline)
    {
        js_event ev;
        int bytes = read(fd, &ev, sizeof(ev));
        if (bytes == sizeof(ev))
        {
            ev.type &= ~JS_EVENT_INIT;
            if ((ev.type & JS_EVENT_AXIS) && ev.number == idx)
            {
                w.minVal = std::min(w.minVal, (int)ev.value);
                w.maxVal = std::max(w.maxVal, (int)ev.value);
            }
        }
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    fcntl(fd, F_SETFL, flags);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// Drain init events
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<int> drainInitEvents(int fd, int numAxes)
{
    std::vector<int> s(numAxes, 0);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    js_event ev;
    while (read(fd, &ev, sizeof(ev)) == sizeof(ev))
        if ((ev.type & JS_EVENT_AXIS) && ev.number < numAxes)
            s[ev.number] = ev.value;
    fcntl(fd, F_SETFL, flags);
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sample for report
// ─────────────────────────────────────────────────────────────────────────────
struct Sample
{
    std::string label;
    int         raw0 {0}, raw1 {0};
    double      normX {0}, normY {0};
    double      magnitude {0};
    double      angle {0};
    double      expectedAngle {-1};
};

static void printSample(std::ostream &out, const Sample &s)
{
    out << "  " << std::left << std::setw(9) << s.label << ": "
        << "Axis0=" << std::right << std::setw(7) << s.raw0 << "  "
        << "Axis1=" << std::right << std::setw(7) << s.raw1 << "  "
        << "normX=" << std::fixed << std::setw(6) << std::setprecision(3) << s.normX << "  "
        << "normY=" << std::fixed << std::setw(6) << std::setprecision(3) << s.normY << "  "
        << "mag="   << std::fixed << std::setw(5) << std::setprecision(3) << s.magnitude << "  "
        << "angle=" << std::right << std::setw(6) << std::fixed << std::setprecision(1) << s.angle << "°";

    if (s.expectedAngle >= 0)
    {
        double err = s.angle - s.expectedAngle;
        while (err >  180.0) err -= 360.0;
        while (err < -180.0) err += 360.0;
        out << "  (expected " << std::setw(5) << s.expectedAngle
            << "°  err=" << std::showpos << std::fixed << std::setprecision(1) << err << "°"
            << std::noshowpos << ")";
    }
    out << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[])
{
    const char *devPath = "/dev/input/js0";
    if (argc > 1) devPath = argv[1];

    std::cout << "\n=== Joystick Analysis & Calibration Diagnostic ===\n";
    std::cout << "Device: " << devPath << "\n\n";

    int fd = open(devPath, O_RDONLY);
    if (fd < 0)
    {
        std::cerr << "ERROR: cannot open " << devPath << ": " << strerror(errno) << "\n";
        return 1;
    }

    // Query device info
    char name[256] = "Unknown";
    uint32_t version = 0;
    uint8_t numAxes = 0, numButtons = 0;
    ioctl(fd, JSIOCGNAME(sizeof(name)), name);
    ioctl(fd, JSIOCGVERSION, &version);
    ioctl(fd, JSIOCGAXES, &numAxes);
    ioctl(fd, JSIOCGBUTTONS, &numButtons);

    // Query axis-to-ABS map.
    // IMPORTANT: JSIOCGAXMAP always writes ABS_MAX+1 = 64 bytes into the buffer
    // regardless of how many axes the device actually has.  Passing a smaller
    // buffer causes heap corruption.  Always allocate the full 64-byte scratch
    // buffer and copy only the relevant entries afterwards.
    const int ABS_BUF_SIZE = 64; // ABS_MAX+1 from linux/input.h
    uint8_t axmapBuf[ABS_BUF_SIZE] = {};
    std::vector<uint8_t> axmap(numAxes, 0);
    if (ioctl(fd, JSIOCGAXMAP, axmapBuf) < 0)
    {
        // Fall back to sequential ABS codes if ioctl fails
        for (int i = 0; i < numAxes; i++) axmap[i] = static_cast<uint8_t>(i);
        std::cerr << "WARNING: JSIOCGAXMAP not available; axis types may be wrong.\n";
    }
    else
    {
        for (int i = 0; i < numAxes; i++)
            axmap[i] = axmapBuf[i];
    }

    std::cout << "Name    : " << name << "\n";
    std::cout << "Version : " << version << "\n";
    std::cout << "Axes    : " << (int)numAxes << "\n";
    std::cout << "Buttons : " << (int)numButtons << "\n\n";

    // Print axis map
    std::cout << "Axis map (from JSIOCGAXMAP):\n";
    for (int i = 0; i < numAxes; i++)
    {
        std::cout << "  Axis " << i << "  ABS=0x"
                  << std::right << std::hex << std::setw(2) << std::setfill('0')
                  << (int)axmap[i] << std::dec << std::setfill(' ')
                  << "  Type=" << std::left << std::setw(10) << axisTypeName(classifyAxis(axmap[i]))
                  << std::right << "  Label=" << absCodeLabel(axmap[i]) << "\n";
    }
    std::cout << "\n";

    // Drain init events
    std::vector<int> axisState = drainInitEvents(fd, numAxes);

    // ── Find analog stick pairs ───────────────────────────────────────────
    // A stick pair is (ABS_X + ABS_Y) or (ABS_RX + ABS_RY) etc.
    // We look for matching X/Y pairs based on ABS code.
    struct StickPair
    {
        int      i0, i1;        // axis indices
        uint8_t  absX, absY;    // ABS codes
        std::string name;
    };

    std::vector<StickPair> stickPairs;

    // Known X+Y pairs in order of preference
    const std::array<std::pair<uint8_t, uint8_t>, 6> knownPairs = {{
            {ABS_X,        ABS_Y},
            {ABS_RX,       ABS_RY},
            {ABS_THROTTLE, ABS_RUDDER},
            {ABS_HAT0X,    ABS_HAT0Y},
            {ABS_HAT1X,    ABS_HAT1Y},
            {ABS_HAT2X,    ABS_HAT2Y},
        }
    };

    const std::array<const char *, 6> pairNames = {{
            "Left Stick",
            "Right Stick",
            "Throttle/Rudder",
            "D-pad (Hat 0)",
            "Hat 1",
            "Hat 2",
        }
    };

    for (size_t p = 0; p < knownPairs.size(); p++)
    {
        int idx0 = -1, idx1 = -1;
        for (int i = 0; i < numAxes; i++)
        {
            if (axmap[i] == knownPairs[p].first)  idx0 = i;
            if (axmap[i] == knownPairs[p].second) idx1 = i;
        }
        if (idx0 >= 0 && idx1 >= 0)
        {
            StickPair sp;
            sp.i0 = idx0;
            sp.i1 = idx1;
            sp.absX = knownPairs[p].first;
            sp.absY = knownPairs[p].second;
            sp.name = pairNames[p];
            stickPairs.push_back(sp);
        }
    }

    // ── Find triggers ─────────────────────────────────────────────────────
    struct Trigger
    {
        int     idx;
        uint8_t absCode;
        std::string label;
    };
    std::vector<Trigger> triggers;
    for (int i = 0; i < numAxes; i++)
    {
        if (classifyAxis(axmap[i]) == AxisType::TRIGGER)
            triggers.push_back({i, axmap[i], absCodeLabel(axmap[i])});
    }

    // ── Summary of what we found ──────────────────────────────────────────
    std::cout << "Found " << stickPairs.size() << " joystick pair(s):\n";
    for (const auto &sp : stickPairs)
        std::cout << "  " << sp.name << " → Axis " << sp.i0 << " + Axis " << sp.i1 << "\n";
    std::cout << "Found " << triggers.size() << " trigger(s):\n";
    for (const auto &tr : triggers)
        std::cout << "  " << tr.label << " → Axis " << tr.idx << "\n";
    std::cout << "\n";

    // ─────────────────────────────────────────────────────────────────────
    // Per-stick interactive calibration test
    // ─────────────────────────────────────────────────────────────────────
    struct DirTest
    {
        const char *label;
        double     expectedAngle;
        bool       useMaxAxis0, useMaxAxis1;
        bool       testAxis0,   testAxis1;
    };

    const std::array<DirTest, 8> dirTests = {{
            {"UP",      0.0,   false, false, false, true},
            {"UP+RIGHT", 45.0,  true,  false, true,  true},
            {"RIGHT",   90.0,  true,  false, true,  false},
            {"DN+RIGHT", 135.0, true,  true,  true,  true},
            {"DOWN",    180.0, false, true,  false, true},
            {"DN+LEFT", 225.0, false, true,  true,  true},
            {"LEFT",    270.0, false, false, true,  false},
            {"UP+LEFT", 315.0, false, false, true,  true},
        }
    };

    struct JoyResult
    {
        StickPair sp;
        std::vector<Sample> samples;
        int calCenter0, calMin0, calMax0;
        int calCenter1, calMin1, calMax1;
        bool isHat;
    };
    std::vector<JoyResult> joyResults;

    for (const auto &sp : stickPairs)
    {
        bool isHat = (classifyAxis(sp.absX) == AxisType::HAT);

        std::cout <<
        "══════════════════════════════════════════════\n"
                  << "  " << sp.name << "  (Axis " << sp.i0 << " = " << absCodeLabel(sp.absX)
                  << ",  Axis " << sp.i1 << " = " << absCodeLabel(sp.absY) << ")\n"
                  << "══════════════════════════════════════════════\n\n";

        JoyResult res;
        res.sp = sp;
        res.isHat = isHat;

        if (isHat)
        {
            // D-pad: digital, already perfect — just snapshot the current range
            std::cout << "  D-pad/hat detected. Snapping a quick sample (press all 4 directions).\n"
                         "  Press Enter when ready, then move d-pad to all 4 directions...\n";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "  Sampling for 4 seconds... ";
            std::cout.flush();
            AxisWindow w = collectWindow(fd, sp.i0, sp.i1, axisState, 4000);
            std::cout << "done.\n\n";
            res.calCenter0 = 0;
            res.calMin0 = w.min0;
            res.calMax0 = w.max0;
            res.calCenter1 = 0;
            res.calMin1 = w.min1;
            res.calMax1 = w.max1;

            // Compute sample for center
            {
                Sample s;
                s.label = "CENTER";
                s.raw0 = 0;
                s.raw1 = 0;
                s.normX = 0;
                s.normY = 0;
                s.magnitude = 0;
                s.angle = 0;
                res.samples.push_back(s);
            }
            // Compute expected values for each direction using what we saw
            for (const auto &dt : dirTests)
            {
                int r0 = dt.testAxis0 ? (dt.useMaxAxis0 ? w.max0 : w.min0) : 0;
                int r1 = dt.testAxis1 ? (dt.useMaxAxis1 ? w.max1 : w.min1) : 0;
                double nx = normalise(r0, 0, w.min0, w.max0);
                double ny = -normalise(r1, 0, w.min1, w.max1);
                double sx = nx * std::sqrt(1.0 - 0.5 * ny * ny);
                double sy = ny * std::sqrt(1.0 - 0.5 * nx * nx);
                double mag = std::sqrt(sx * sx + sy * sy);
                double ang = (mag > 0.001) ? compassAngle(sx, sy) : 0.0;
                Sample s;
                s.label = dt.label;
                s.raw0 = r0;
                s.raw1 = r1;
                s.normX = nx;
                s.normY = ny;
                s.magnitude = mag;
                s.angle = ang;
                s.expectedAngle = dt.expectedAngle;
                res.samples.push_back(s);
            }
            joyResults.push_back(res);
            continue;
        }

        // ── Step 1: Center ────────────────────────────────────────────────
        std::cout << "  [1] Release stick to CENTER/REST, then press Enter and hold still...\n";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "      Sampling 1.5s... ";
        std::cout.flush();
        AxisWindow cw = collectWindow(fd, sp.i0, sp.i1, axisState, 1500);
        int c0 = (cw.min0 + cw.max0) / 2;
        int c1 = (cw.min1 + cw.max1) / 2;
        std::cout << "done.  Center: Axis" << sp.i0 << "=" << c0
                  << "  Axis" << sp.i1 << "=" << c1 << "\n\n";

        res.calCenter0 = c0;
        res.calCenter1 = c1;
        res.calMin0 = -32767;
        res.calMax0 = 32767;
        res.calMin1 = -32767;
        res.calMax1 = 32767;

        {
            Sample s;
            s.label = "CENTER";
            s.raw0 = c0;
            s.raw1 = c1;
            res.samples.push_back(s);
        }

        // ── Step 2: Directions ────────────────────────────────────────────
        struct DirCap
        {
            AxisWindow win;
            int raw0, raw1;
        };
        std::vector<DirCap> caps(dirTests.size());

        int omin0 = INT_MAX, omax0 = INT_MIN;
        int omin1 = INT_MAX, omax1 = INT_MIN;

        for (size_t di = 0; di < dirTests.size(); di++)
        {
            const auto &dt = dirTests[di];
            std::cout << "  [" << di + 2 << "] Push " << sp.name << " fully " << dt.label
                      << ", then press Enter and hold 1.5s...\n";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "      Sampling... ";
            std::cout.flush();

            AxisWindow w = collectWindow(fd, sp.i0, sp.i1, axisState, 1500);
            caps[di].win = w;
            caps[di].raw0 = dt.testAxis0 ? (dt.useMaxAxis0 ? w.max0 : w.min0) : c0;
            caps[di].raw1 = dt.testAxis1 ? (dt.useMaxAxis1 ? w.max1 : w.min1) : c1;

            omin0 = std::min(omin0, w.min0);
            omax0 = std::max(omax0, w.max0);
            omin1 = std::min(omin1, w.min1);
            omax1 = std::max(omax1, w.max1);

            std::cout << "done.  Axis" << sp.i0 << ": [" << w.min0 << ", " << w.max0 << "]"
                      << "  Axis" << sp.i1 << ": [" << w.min1 << ", " << w.max1 << "]\n\n";
        }

        res.calMin0 = omin0;
        res.calMax0 = omax0;
        res.calMin1 = omin1;
        res.calMax1 = omax1;

        // Compute angles with final calibration
        for (size_t di = 0; di < dirTests.size(); di++)
        {
            const auto &dt = dirTests[di];
            int r0 = caps[di].raw0, r1 = caps[di].raw1;
            double nx =  normalise(r0, c0, omin0, omax0);
            double ny = -normalise(r1, c1, omin1, omax1);
            double sx = nx * std::sqrt(1.0 - 0.5 * ny * ny);
            double sy = ny * std::sqrt(1.0 - 0.5 * nx * nx);
            double mag = std::sqrt(sx * sx + sy * sy);
            double ang = (mag > 0.001) ? compassAngle(sx, sy) : 0.0;

            Sample s;
            s.label = dt.label;
            s.raw0 = r0;
            s.raw1 = r1;
            s.normX = nx;
            s.normY = ny;
            s.magnitude = mag;
            s.angle = ang;
            s.expectedAngle = dt.expectedAngle;
            res.samples.push_back(s);
        }

        joyResults.push_back(res);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Trigger section: just observe rest and full press
    // ─────────────────────────────────────────────────────────────────────
    struct TriggerResult
    {
        Trigger   tr;
        int       restVal, pressVal;
    };
    std::vector<TriggerResult> trigResults;

    if (!triggers.empty())
    {
        std::cout <<
        "══════════════════════════════════════════════\n"
                  << "  TRIGGERS\n"
                  << "══════════════════════════════════════════════\n\n";

        for (const auto &tr : triggers)
        {
            std::cout << "  [" << tr.label << " / Axis " << tr.idx << "]\n";
            std::cout << "  Release trigger fully, then press Enter...\n";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "  Sampling 1s rest... ";
            std::cout.flush();
            SingleWindow rw = collectSingle(fd, tr.idx, axisState, 1000);
            std::cout << "done.  rest=" << rw.minVal << " to " << rw.maxVal << "\n";

            std::cout << "  Press trigger FULLY, then press Enter...\n";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "  Sampling 1s full press... ";
            std::cout.flush();
            SingleWindow pw = collectSingle(fd, tr.idx, axisState, 1000);
            std::cout << "done.  pressed=" << pw.minVal << " to " << pw.maxVal << "\n\n";

            TriggerResult tres;
            tres.tr = tr;
            tres.restVal  = rw.minVal;  // minimum = rest position
            tres.pressVal = pw.maxVal;  // maximum = fully pressed
            trigResults.push_back(tres);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // Build report
    // ─────────────────────────────────────────────────────────────────────
    std::ostringstream report;
    report << "=== Joystick Analysis Report ===\n";
    report << "Device  : " << devPath << "\n";
    report << "Name    : " << name << "\n";
    report << "Version : " << version << "\n";
    report << "Axes    : " << (int)numAxes << "  |  Buttons: " << (int)numButtons << "\n\n";

    report << "Axis map:\n";
    for (int i = 0; i < numAxes; i++)
        report << "  Axis " << i << "  ABS=0x"
               << std::right << std::hex << std::setw(2) << std::setfill('0')
               << (int)axmap[i] << std::dec << std::setfill(' ')
               << "  Type=" << std::left << std::setw(10) << axisTypeName(classifyAxis(axmap[i]))
               << std::right << "  " << absCodeLabel(axmap[i]) << "\n";
    report << "\n";

    // Joystick results
    for (const auto &res : joyResults)
    {
        report << "──────────────────────────────────────────────────────\n";
        report << res.sp.name << "  (Axis " << res.sp.i0 << " + Axis " << res.sp.i1 << ")"
               << (res.isHat ? "  [DIGITAL / D-PAD — no calibration needed]" : "") << "\n";
        if (!res.isHat)
        {
            report << "Calibration:\n";
            report << "  Axis " << res.sp.i0 << ": center=" << res.calCenter0
                   << "  min=" << res.calMin0 << "  max=" << res.calMax0 << "\n";
            report << "  Axis " << res.sp.i1 << ": center=" << res.calCenter1
                   << "  min=" << res.calMin1 << "  max=" << res.calMax1 << "\n";
        }
        report << "\n  Results:\n";
        for (const auto &s : res.samples)
            printSample(report, s);

        if (!res.isHat)
        {
            report << "\n  Suggested INDI AxisCalibrationNP values:\n";
            report << "    AXIS_" << res.sp.i0 + 1 << "_CENTER = " << res.calCenter0 << "\n";
            report << "    AXIS_" << res.sp.i0 + 1 << "_MIN    = " << res.calMin0 << "\n";
            report << "    AXIS_" << res.sp.i0 + 1 << "_MAX    = " << res.calMax0 << "\n";
            report << "    AXIS_" << res.sp.i1 + 1 << "_CENTER = " << res.calCenter1 << "\n";
            report << "    AXIS_" << res.sp.i1 + 1 << "_MIN    = " << res.calMin1 << "\n";
            report << "    AXIS_" << res.sp.i1 + 1 << "_MAX    = " << res.calMax1 << "\n";
        }
        report << "\n";
    }

    // Trigger results
    if (!trigResults.empty())
    {
        report << "──────────────────────────────────────────────────────\n";
        report << "TRIGGERS\n\n";
        for (const auto &tr : trigResults)
        {
            report << "  " << tr.tr.label << " (Axis " << tr.tr.idx << "):\n";
            report << "    rest=" << tr.restVal << "  fully-pressed=" << tr.pressVal << "\n";
            double norm = normalise(tr.pressVal, tr.restVal, tr.restVal, tr.pressVal);
            report << "    Normalised at full press = " << norm << " (should be 1.0)\n";
            report << "    AXIS_" << tr.tr.idx + 1 << "_CENTER = " << tr.restVal << "\n";
            report << "    AXIS_" << tr.tr.idx + 1 << "_MIN    = " << tr.restVal << "\n";
            report << "    AXIS_" << tr.tr.idx + 1 << "_MAX    = " << tr.pressVal << "\n\n";
        }
    }

    std::cout << "\n" << report.str();

    std::string outPath = homeDir() + "/joystick_analysis.txt";
    std::ofstream ofs(outPath);
    if (ofs)
    {
        ofs << report.str();
        std::cout << "Report saved to: " << outPath << "\n";
    }
    else
        std::cerr << "WARNING: could not write to " << outPath << "\n";

    close(fd);
    return 0;
}
