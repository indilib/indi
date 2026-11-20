/*
    OnStepX INDI Driver — test_onstepxstatus.cpp
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

/*
    OnStepX INDI Driver — Status parser unit tests (Stage 3)

    OnStepXStatus has zero INDI dependencies — it is pure data transformation.
    Tests run without any device instance or socketpair.
*/

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "OnStepXStatus.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a minimal valid :GU# string: flags + "000" (rates/error all zero).
// Usage: buildGU("nNpE") → "nNpE000"
static std::string buildGU(const char *flags)
{
    std::string s = flags;
    s += "000";   // pulseGuideRate=0, guideRate=0, errorCode=0
    return s;
}

// Build a binary :Gu# byte array. All 9 bytes default to 0x80 (valid but
// zero payload). Individual bytes can be overridden via parameters.
static void buildGu(uint8_t out[9],
                    uint8_t b0 = 0x80, uint8_t b1 = 0x80,
                    uint8_t b2 = 0x80, uint8_t b3 = 0x80,
                    uint8_t b4 = 0x80, uint8_t b5 = 0x80,
                    uint8_t b6 = 0x80, uint8_t b7 = 0x80,
                    uint8_t b8 = 0x80)
{
    out[0] = b0;
    out[1] = b1;
    out[2] = b2;
    out[3] = b3;
    out[4] = b4;
    out[5] = b5;
    out[6] = b6;
    out[7] = b7;
    out[8] = b8;
}

// ---------------------------------------------------------------------------
// :GU# tests
// ---------------------------------------------------------------------------

// Absence of 'n' → tracking = true
TEST(OnStepXStatusTest, test_parseGU_tracking)
{
    MountStatus s;
    // No 'n' in flags — mount is tracking
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("NpE").c_str(), s));
    EXPECT_TRUE(s.tracking);
}

// 'n' present → tracking = false
TEST(OnStepXStatusTest, test_parseGU_not_tracking)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("nNpE").c_str(), s));
    EXPECT_FALSE(s.tracking);
}

// 'P' → parkState = PARKED, tracking stops (implied by firmware but not parsed here)
TEST(OnStepXStatusTest, test_parseGU_parked)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("nNPE").c_str(), s));
    EXPECT_EQ(s.parkState, MountStatus::ParkState::PARKED);
}

// 'I' → parkState = PARKING
TEST(OnStepXStatusTest, test_parseGU_parking)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("nNIE").c_str(), s));
    EXPECT_EQ(s.parkState, MountStatus::ParkState::PARKING);
}

// 'F' → parkState = FAILED
TEST(OnStepXStatusTest, test_parseGU_park_failed)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("nNFE").c_str(), s));
    EXPECT_EQ(s.parkState, MountStatus::ParkState::FAILED);
}

// '~' → pecState = PLAYING
TEST(OnStepXStatusTest, test_parseGU_pec_playing)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("NpE~").c_str(), s));
    EXPECT_EQ(s.pecState, MountStatus::PecState::PLAYING);
}

// 'A' → mountType = ALTAZM
TEST(OnStepXStatusTest, test_parseGU_altazm)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("NpA").c_str(), s));
    EXPECT_EQ(s.mountType, MountStatus::MountType::ALTAZM);
}

// '(' → trackRate = LUNAR
TEST(OnStepXStatusTest, test_parseGU_lunar_rate)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("Np(E").c_str(), s));
    EXPECT_EQ(s.trackRate, MountStatus::TrackRate::LUNAR);
}

// 'k' → trackRate = KING
TEST(OnStepXStatusTest, test_parseGU_king_rate)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("NpkE").c_str(), s));
    EXPECT_EQ(s.trackRate, MountStatus::TrackRate::KING);
}

// 'T' → pierSide = EAST
TEST(OnStepXStatusTest, test_parseGU_pier_east)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("NpET").c_str(), s));
    EXPECT_EQ(s.pierSide, MountStatus::PierSide::EAST);
}

// 'W' → pierSide = WEST
TEST(OnStepXStatusTest, test_parseGU_pier_west)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("NpEW").c_str(), s));
    EXPECT_EQ(s.pierSide, MountStatus::PierSide::WEST);
}

// Status.command.cpp sends the error as '0' + code, and codes run to 15:
// ERR_ALT_MAX (12) arrives as '<'.  It used to be clamped to 0 and lost.
TEST(OnStepXStatusTest, test_parseGU_error_code_above_nine)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU("NpE00<", s));
    EXPECT_EQ(s.errorCode, 12);
    ASSERT_TRUE(OnStepXStatus::parseGU("NpE00?", s));
    EXPECT_EQ(s.errorCode, 15);
    // Past GeneralErrors: a garbled reply, not an error
    ASSERT_TRUE(OnStepXStatus::parseGU("NpE00W", s));
    EXPECT_EQ(s.errorCode, 0);
}

TEST(OnStepXStatusTest, test_error_text)
{
    // Limits.cpp GeneralErrors order
    EXPECT_STREQ(OnStepXStatus::errorText(0), "none");
    EXPECT_STREQ(OnStepXStatus::errorText(7), "meridian limit reached");
    EXPECT_STREQ(OnStepXStatus::errorText(12), "above the overhead limit");
    EXPECT_STREQ(OnStepXStatus::errorText(15), "settings memory (NV) failed to start");
    EXPECT_STREQ(OnStepXStatus::errorText(40), "unknown error");

    EXPECT_TRUE(OnStepXStatus::errorStopsMotion(2));
    EXPECT_TRUE(OnStepXStatus::errorStopsMotion(7));
    EXPECT_TRUE(OnStepXStatus::errorStopsMotion(12));
    EXPECT_FALSE(OnStepXStatus::errorStopsMotion(0));
    EXPECT_FALSE(OnStepXStatus::errorStopsMotion(14));
}

TEST(OnStepXStatusTest, test_goto_error_text)
{
    EXPECT_STREQ(OnStepXStatus::gotoErrorText('1'), "below the horizon limit");
    EXPECT_STREQ(OnStepXStatus::gotoErrorText('6'), "outside limits");
    EXPECT_STREQ(OnStepXStatus::gotoErrorText('x'), "unknown error");
}

// Error code digit in last position
TEST(OnStepXStatusTest, test_parseGU_error_code)
{
    MountStatus s;
    // Manually construct with error code 5: "NpE" + "005"
    ASSERT_TRUE(OnStepXStatus::parseGU("NpE005", s));
    EXPECT_EQ(s.errorCode, 5);
    EXPECT_EQ(s.pulseGuideRateSelect, 0);
    EXPECT_EQ(s.guideRateSelect, 0);
}

// Empty string → false
TEST(OnStepXStatusTest, test_parseGU_empty)
{
    MountStatus s;
    EXPECT_FALSE(OnStepXStatus::parseGU("", s));
    EXPECT_FALSE(OnStepXStatus::parseGU(nullptr, s));
}

// Late :Gu# binary bytes read as a :GU# reply must be rejected, not parsed.
TEST(OnStepXStatusTest, test_parseGU_rejects_non_printable)
{
    MountStatus s;
    EXPECT_FALSE(OnStepXStatus::parseGU("\x83\xb0\x81\x91\x80\x80\x81\x86\x80", s));
    EXPECT_FALSE(OnStepXStatus::parseGU("nNp\x01" "E000", s));
}

// ---------------------------------------------------------------------------
// :Gu# binary tests
// ---------------------------------------------------------------------------

// All 0x80 = empty payload: tracking, goto active (no "no goto" flag),
// unparked, no mount-type or pier-side flag set.
TEST(OnStepXStatusTest, test_parseGu_binary_empty_payload)
{
    uint8_t buf[9];
    buildGu(buf);   // all 0x80

    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGu(buf, 9, s));
    EXPECT_TRUE(s.tracking);
    EXPECT_TRUE(s.gotoActive);
    EXPECT_EQ(s.parkState, MountStatus::ParkState::UNPARKED);
    EXPECT_EQ(s.mountType, MountStatus::MountType::UNKNOWN);
    EXPECT_EQ(s.pierSide, MountStatus::PierSide::NONE);
    EXPECT_EQ(s.trackComp, MountStatus::TrackComp::NONE);
    EXPECT_EQ(s.trackRate, MountStatus::TrackRate::SIDEREAL);
    EXPECT_EQ(s.errorCode, 0);
}

// ---------------------------------------------------------------------------
// Firmware encoders
//
// FwState plus encodeGu()/encodeGU() are transcriptions of the :Gu# and :GU#
// handlers in OnStepX src/telescope/mount/status/Status.command.cpp. Keep them
// in step with the firmware, not with the driver.
// ---------------------------------------------------------------------------
enum FwRc { RC_NONE, RC_REFRACTION, RC_REFRACTION_DUAL, RC_MODEL, RC_MODEL_DUAL };
enum FwRate { RATE_SIDEREAL, RATE_LUNAR, RATE_SOLAR, RATE_KING };
enum FwMount { FW_GEM, FW_FORK, FW_ALTAZM, FW_ALTALT };
enum FwPier { FW_PIER_NONE, FW_PIER_EAST, FW_PIER_WEST };

struct FwState
{
    bool tracking = true;
    bool gotoNone = true;
    bool pps = false;
    bool pulseGuide = false;
    bool guide = false;
    FwRc rc = RC_NONE;
    FwRate rate = RATE_SIDEREAL;
    bool syncEnc = false;
    bool atHome = false;
    bool homing = false;
    bool autoBoot = false;
    bool homePaused = false;
    bool pauseEnabled = false;
    bool sound = false;
    bool autoFlip = false;
    FwMount mount = FW_GEM;
    FwPier pier = FW_PIER_NONE;
    bool pecRecorded = false;
    int pecState = 0;          // 0 ignore .. 4 recording
    int park = 0;              // 0 unparked, 1 parking, 2 parked, 3 failed
    int pulseRate = 1;
    int guideRate = 6;
    int error = 0;
};

static bool fwEquatorial(const FwState &f)
{
    return f.mount == FW_GEM || f.mount == FW_FORK;
}

static void encodeGu(const FwState &f, uint8_t reply[9])
{
    memset(reply, 0x80, 9);
    if (!f.tracking)  reply[0] |= 0b10000001;
    if (f.gotoNone)   reply[0] |= 0b10000010;
    if (f.pps)        reply[0] |= 0b10000100;
    if (f.pulseGuide) reply[0] |= 0b10001000;
    if (f.rc == RC_REFRACTION)      reply[0] |= 0b11010000;
    if (f.rc == RC_REFRACTION_DUAL) reply[0] |= 0b10010000;
    if (f.rc == RC_MODEL)           reply[0] |= 0b11100000;
    if (f.rc == RC_MODEL_DUAL)      reply[0] |= 0b10100000;
    if (f.rc == RC_NONE)
    {
        if (f.rate == RATE_LUNAR) reply[1] |= 0b10000001;
        else if (f.rate == RATE_SOLAR) reply[1] |= 0b10000010;
        else if (f.rate == RATE_KING)  reply[1] |= 0b10000011;
    }
    if (f.syncEnc)      reply[1] |= 0b10000100;
    if (f.guide)        reply[1] |= 0b10001000;
    if (f.atHome)       reply[2] |= 0b10000001;
    if (f.homing)       reply[2] |= 0b10100000;
    if (f.autoBoot)     reply[2] |= 0b11000000;
    if (f.homePaused)   reply[2] |= 0b10000010;
    if (f.pauseEnabled) reply[2] |= 0b10000100;
    if (f.sound)        reply[2] |= 0b10001000;
    if (f.autoFlip)     reply[2] |= 0b10010000;
    if (f.mount == FW_GEM)    reply[3] |= 0b10000001;
    else if (f.mount == FW_FORK)   reply[3] |= 0b10000010;
    else if (f.mount == FW_ALTAZM) reply[3] |= 0b10001000;
    else if (f.mount == FW_ALTALT) reply[3] |= 0b10000100;
    if (f.pier == FW_PIER_NONE) reply[3] |= 0b10010000;
    else if (f.pier == FW_PIER_EAST) reply[3] |= 0b10100000;
    else if (f.pier == FW_PIER_WEST) reply[3] |= 0b11000000;
    if (fwEquatorial(f)) reply[4] = f.pecState | 0b10000000;
    if (f.pecRecorded)   reply[4] |= 0b11000000;
    reply[5] = f.park | 0b10000000;
    reply[6] = f.pulseRate | 0b10000000;
    reply[7] = f.guideRate | 0b10000000;
    reply[8] = f.error | 0b10000000;
}

static std::string encodeGU(const FwState &f)
{
    std::string r;
    if (!f.tracking) r += 'n';
    if (f.gotoNone)  r += 'N';
    if (f.park == 0) r += 'p';
    else if (f.park == 1) r += 'I';
    else if (f.park == 2) r += 'P';
    else if (f.park == 3) r += 'F';
    if (f.syncEnc)   r += 'e';
    if (f.atHome)    r += 'H';
    if (f.homing)    r += 'h';
    if (f.autoBoot)  r += 'B';
    if (f.pps)       r += 'S';
    if (f.pulseGuide) r += 'G';
    if (f.guide)     r += 'g';
    if (f.rc == RC_REFRACTION)      r += "rs";
    if (f.rc == RC_REFRACTION_DUAL) r += "r";
    if (f.rc == RC_MODEL)           r += "ts";
    if (f.rc == RC_MODEL_DUAL)      r += "t";
    if (f.rc == RC_NONE)
    {
        if (f.rate == RATE_LUNAR) r += '(';
        else if (f.rate == RATE_SOLAR) r += 'O';
        else if (f.rate == RATE_KING)  r += 'k';
    }
    if (f.homePaused)   r += 'w';
    if (f.pauseEnabled) r += 'u';
    if (f.sound)        r += 'z';
    if (f.autoFlip)     r += 'a';
    if (f.pecRecorded)  r += 'R';
    if (fwEquatorial(f)) r += "/,~;^"[f.pecState];
    if (f.mount == FW_GEM)    r += 'E';
    else if (f.mount == FW_FORK)   r += 'K';
    else if (f.mount == FW_ALTAZM) r += 'A';
    else if (f.mount == FW_ALTALT) r += 'L';
    if (f.pier == FW_PIER_NONE) r += 'o';
    else if (f.pier == FW_PIER_EAST) r += 'T';
    else if (f.pier == FW_PIER_WEST) r += 'W';
    r += static_cast<char>('0' + f.pulseRate);
    r += static_cast<char>('0' + f.guideRate);
    r += static_cast<char>('0' + f.error);
    return r;
}

static void expectSameStatus(const MountStatus &a, const MountStatus &b)
{
    EXPECT_EQ(a.tracking, b.tracking);
    EXPECT_EQ(a.gotoActive, b.gotoActive);
    EXPECT_EQ(a.trackComp, b.trackComp);
    EXPECT_EQ(a.trackRate, b.trackRate);
    EXPECT_EQ(a.parkState, b.parkState);
    EXPECT_EQ(a.atHome, b.atHome);
    EXPECT_EQ(a.homing, b.homing);
    EXPECT_EQ(a.autoHomeAtBoot, b.autoHomeAtBoot);
    EXPECT_EQ(a.waitingAtHome, b.waitingAtHome);
    EXPECT_EQ(a.pauseAtHomeEnabled, b.pauseAtHomeEnabled);
    EXPECT_EQ(a.guideActive, b.guideActive);
    EXPECT_EQ(a.pulseGuideActive, b.pulseGuideActive);
    EXPECT_EQ(a.pierSide, b.pierSide);
    EXPECT_EQ(a.pecRecorded, b.pecRecorded);
    EXPECT_EQ(a.pecState, b.pecState);
    EXPECT_EQ(a.mountType, b.mountType);
    EXPECT_EQ(a.ppsSynced, b.ppsSynced);
    EXPECT_EQ(a.buzzerEnabled, b.buzzerEnabled);
    EXPECT_EQ(a.autoMeridianFlip, b.autoMeridianFlip);
    EXPECT_EQ(a.syncToEncoders, b.syncToEncoders);
    EXPECT_EQ(a.pulseGuideRateSelect, b.pulseGuideRateSelect);
    EXPECT_EQ(a.guideRateSelect, b.guideRateSelect);
    EXPECT_EQ(a.errorCode, b.errorCode);
}

// Decode the same firmware state through both paths and require identical output.
static void checkBothPaths(const FwState &f, MountStatus &out)
{
    uint8_t bin[9];
    encodeGu(f, bin);
    MountStatus fromBin, fromAscii;
    ASSERT_TRUE(OnStepXStatus::parseGu(bin, 9, fromBin));
    ASSERT_TRUE(OnStepXStatus::parseGU(encodeGU(f).c_str(), fromAscii));
    expectSameStatus(fromBin, fromAscii);
    out = fromBin;
}

// Regression: an active pulse guide used to decode as PARKED.
TEST(OnStepXStatusTest, test_parseGu_pulse_guide_is_not_parked)
{
    FwState f;
    f.pulseGuide = true;
    MountStatus s;
    checkBothPaths(f, s);
    EXPECT_TRUE(s.pulseGuideActive);
    EXPECT_EQ(s.parkState, MountStatus::ParkState::UNPARKED);
}

// Regression: a GEM used to always decode as pier side EAST.
TEST(OnStepXStatusTest, test_parseGu_gem_pier_sides)
{
    for (FwPier p :
            {
                FW_PIER_NONE, FW_PIER_EAST, FW_PIER_WEST
            })
    {
        FwState f;
        f.mount = FW_GEM;
        f.pier = p;
        MountStatus s;
        checkBothPaths(f, s);
        EXPECT_EQ(s.mountType, MountStatus::MountType::GEM);
        EXPECT_EQ(static_cast<int>(s.pierSide), static_cast<int>(p));
    }
}

TEST(OnStepXStatusTest, test_parseGu_mount_types)
{
    const std::pair<FwMount, MountStatus::MountType> cases[] =
    {
        { FW_GEM,    MountStatus::MountType::GEM },
        { FW_FORK,   MountStatus::MountType::FORK },
        { FW_ALTAZM, MountStatus::MountType::ALTAZM },
        { FW_ALTALT, MountStatus::MountType::ALTALT },
    };
    for (const auto &c : cases)
    {
        FwState f;
        f.mount = c.first;
        MountStatus s;
        checkBothPaths(f, s);
        EXPECT_EQ(s.mountType, c.second);
    }
}

TEST(OnStepXStatusTest, test_parseGu_park_states)
{
    const MountStatus::ParkState expected[] =
    {
        MountStatus::ParkState::UNPARKED, MountStatus::ParkState::PARKING,
        MountStatus::ParkState::PARKED,   MountStatus::ParkState::FAILED,
    };
    for (int p = 0; p < 4; p++)
    {
        FwState f;
        f.park = p;
        MountStatus s;
        checkBothPaths(f, s);
        EXPECT_EQ(s.parkState, expected[p]);
    }
}

TEST(OnStepXStatusTest, test_parseGu_tracking_compensation)
{
    const std::pair<FwRc, MountStatus::TrackComp> cases[] =
    {
        { RC_NONE,            MountStatus::TrackComp::NONE },
        { RC_REFRACTION,      MountStatus::TrackComp::REFRACTION_SINGLE },
        { RC_REFRACTION_DUAL, MountStatus::TrackComp::REFRACTION_DUAL },
        { RC_MODEL,           MountStatus::TrackComp::ONTRACK_SINGLE },
        { RC_MODEL_DUAL,      MountStatus::TrackComp::ONTRACK_DUAL },
    };
    for (const auto &c : cases)
    {
        FwState f;
        f.rc = c.first;
        MountStatus s;
        checkBothPaths(f, s);
        EXPECT_EQ(s.trackComp, c.second);
    }
}

TEST(OnStepXStatusTest, test_parseGu_tracking_rates)
{
    const std::pair<FwRate, MountStatus::TrackRate> cases[] =
    {
        { RATE_SIDEREAL, MountStatus::TrackRate::SIDEREAL },
        { RATE_LUNAR,    MountStatus::TrackRate::LUNAR },
        { RATE_SOLAR,    MountStatus::TrackRate::SOLAR },
        { RATE_KING,     MountStatus::TrackRate::KING },
    };
    for (const auto &c : cases)
    {
        FwState f;
        f.rate = c.first;
        MountStatus s;
        checkBothPaths(f, s);
        EXPECT_EQ(s.trackRate, c.second);
    }
}

TEST(OnStepXStatusTest, test_parseGu_pec_states)
{
    for (int p = 0; p <= 4; p++)
    {
        for (bool recorded :
                {
                    false, true
                })
        {
            FwState f;
            f.pecState = p;
            f.pecRecorded = recorded;
            MountStatus s;
            checkBothPaths(f, s);
            EXPECT_EQ(static_cast<int>(s.pecState), p);
            EXPECT_EQ(s.pecRecorded, recorded);
        }
    }
}

// Every single-bit flag on its own, so a bit landing in the wrong field shows up.
TEST(OnStepXStatusTest, test_parseGu_individual_flags)
{
    struct Case
    {
        const char *name;
        void (*set)(FwState &);
        bool (*get)(const MountStatus &);
    };
    const Case cases[] =
    {
        {
            "notTracking",  [](FwState & f) { f.tracking = false; },    [](const MountStatus & s)
            {
                return !s.tracking;
            }
        },
        {
            "gotoActive",   [](FwState & f) { f.gotoNone = false; },    [](const MountStatus & s)
            {
                return s.gotoActive;
            }
        },
        {
            "pps",          [](FwState & f) { f.pps = true; },          [](const MountStatus & s)
            {
                return s.ppsSynced;
            }
        },
        {
            "pulseGuide",   [](FwState & f) { f.pulseGuide = true; },   [](const MountStatus & s)
            {
                return s.pulseGuideActive;
            }
        },
        {
            "guide",        [](FwState & f) { f.guide = true; },        [](const MountStatus & s)
            {
                return s.guideActive;
            }
        },
        {
            "syncEnc",      [](FwState & f) { f.syncEnc = true; },      [](const MountStatus & s)
            {
                return s.syncToEncoders;
            }
        },
        {
            "atHome",       [](FwState & f) { f.atHome = true; },       [](const MountStatus & s)
            {
                return s.atHome;
            }
        },
        {
            "homing",       [](FwState & f) { f.homing = true; },       [](const MountStatus & s)
            {
                return s.homing;
            }
        },
        {
            "autoBoot",     [](FwState & f) { f.autoBoot = true; },     [](const MountStatus & s)
            {
                return s.autoHomeAtBoot;
            }
        },
        {
            "homePaused",   [](FwState & f) { f.homePaused = true; },   [](const MountStatus & s)
            {
                return s.waitingAtHome;
            }
        },
        {
            "pauseEnabled", [](FwState & f) { f.pauseEnabled = true; }, [](const MountStatus & s)
            {
                return s.pauseAtHomeEnabled;
            }
        },
        {
            "sound",        [](FwState & f) { f.sound = true; },        [](const MountStatus & s)
            {
                return s.buzzerEnabled;
            }
        },
        {
            "autoFlip",     [](FwState & f) { f.autoFlip = true; },     [](const MountStatus & s)
            {
                return s.autoMeridianFlip;
            }
        },
    };

    for (const auto &c : cases)
    {
        SCOPED_TRACE(c.name);
        FwState f;
        c.set(f);
        MountStatus s;
        checkBothPaths(f, s);
        EXPECT_TRUE(c.get(s));
        // Nothing else should look parked, homing or slewing as a side effect.
        if (std::string(c.name) != "gotoActive")
        {
            EXPECT_FALSE(s.gotoActive);
        }
        EXPECT_EQ(s.parkState, MountStatus::ParkState::UNPARKED);
        EXPECT_EQ(s.mountType, MountStatus::MountType::GEM);
        EXPECT_EQ(s.pierSide, MountStatus::PierSide::NONE);
    }
}

TEST(OnStepXStatusTest, test_parseGu_rates_and_error)
{
    FwState f;
    f.pulseRate = 2;
    f.guideRate = 8;
    f.error = 5;
    MountStatus s;
    checkBothPaths(f, s);
    EXPECT_EQ(s.pulseGuideRateSelect, 2);
    EXPECT_EQ(s.guideRateSelect, 8);
    EXPECT_EQ(s.errorCode, 5);
}

// A realistic busy state: GEM, west, parked-failed, PEC playing, refraction single.
TEST(OnStepXStatusTest, test_parseGu_combined_state)
{
    FwState f;
    f.tracking = false;
    f.mount = FW_GEM;
    f.pier = FW_PIER_WEST;
    f.park = 3;
    f.pecState = 2;
    f.pecRecorded = true;
    f.rc = RC_REFRACTION;
    f.autoFlip = true;
    f.pulseGuide = true;
    MountStatus s;
    checkBothPaths(f, s);
    EXPECT_FALSE(s.tracking);
    EXPECT_EQ(s.pierSide, MountStatus::PierSide::WEST);
    EXPECT_EQ(s.parkState, MountStatus::ParkState::FAILED);
    EXPECT_EQ(s.pecState, MountStatus::PecState::PLAYING);
    EXPECT_EQ(s.trackComp, MountStatus::TrackComp::REFRACTION_SINGLE);
}

// Short buffer (8 bytes) → false
TEST(OnStepXStatusTest, test_parseGu_binary_short)
{
    uint8_t buf[8];
    memset(buf, 0x80, sizeof(buf));
    MountStatus s;
    EXPECT_FALSE(OnStepXStatus::parseGu(buf, 8, s));
}

// Any byte < 0x80 (high-bit not set) → false
TEST(OnStepXStatusTest, test_parseGu_binary_unmasked)
{
    uint8_t buf[9];
    buildGu(buf);
    buf[4] = 0x7F;   // byte 4 has high bit clear — invalid
    MountStatus s;
    EXPECT_FALSE(OnStepXStatus::parseGu(buf, 9, s));
}

// ---------------------------------------------------------------------------
// Additional coverage: tracking-comp and rate parsing
// ---------------------------------------------------------------------------

// 'r' alone → REFRACTION_DUAL
TEST(OnStepXStatusTest, test_parseGU_refraction_dual)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("NprE").c_str(), s));
    EXPECT_EQ(s.trackComp, MountStatus::TrackComp::REFRACTION_DUAL);
}

// 'r'+'s' → REFRACTION_SINGLE
TEST(OnStepXStatusTest, test_parseGU_refraction_single)
{
    MountStatus s;
    ASSERT_TRUE(OnStepXStatus::parseGU(buildGU("NprsE").c_str(), s));
    EXPECT_EQ(s.trackComp, MountStatus::TrackComp::REFRACTION_SINGLE);
}

// ---------------------------------------------------------------------------
// main() — defined here to override indidrivermain.c from libindidriver.so.
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
