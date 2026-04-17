/*******************************************************************************
 Copyright(c) 2026 INDI Contributors. All rights reserved.
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

/**
 * Unit tests for the INDI rotator interface limit-checking logic.
 *
 * The safe zone is a symmetric window of ±(limit/2) degrees centered on the
 * sync offset. Minimum arc distance is used so wrap-around is handled
 * correctly. The check is disabled when limit == 0.
 *
 * Formula:
 *   diff    = |target - offset|
 *   minDist = min(diff, 360 - diff)
 *   blocked = (limit > 0) && (minDist > limit / 2)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Pure reimplementation of the formula from indirotatorinterface.cpp so the
// tests are self-contained and do not require linking the full INDI library.
// ---------------------------------------------------------------------------
static bool rotatorExceedsLimit(double target, double offset, double limit)
{
    if (limit <= 0.0)
        return false;                           // limits disabled

    double diff    = std::abs(target - offset);
    double minDist = std::min(diff, 360.0 - diff);
    return minDist > limit / 2.0;
}

// ===========================================================================
// Test suite: disabled limits (limit == 0)
// ===========================================================================
TEST(RotatorLimits, DisabledLimits_NothingBlocked)
{
    // limit == 0 means no restriction regardless of target or offset
    EXPECT_FALSE(rotatorExceedsLimit(0.0,   0.0, 0.0));
    EXPECT_FALSE(rotatorExceedsLimit(180.0, 0.0, 0.0));
    EXPECT_FALSE(rotatorExceedsLimit(359.0, 0.0, 0.0));
    EXPECT_FALSE(rotatorExceedsLimit(270.0, 45.0, 0.0));
}

// ===========================================================================
// Test suite: full range (limit == 360) — regression for the original bug
// ===========================================================================
TEST(RotatorLimits, FullRange_NothingBlocked)
{
    // With limit=360, the safe window is ±180° — every target is within range.
    // This was the original bug: targets > 180° were incorrectly blocked.
    EXPECT_FALSE(rotatorExceedsLimit(181.0, 0.0, 360.0));  // original bug case
    EXPECT_FALSE(rotatorExceedsLimit(340.0, 0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(350.0, 0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(358.0, 0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(359.0, 0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(0.0,   0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(90.0,  0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(180.0, 0.0, 360.0));
}

// ===========================================================================
// Test suite: typical limit, offset at 0°
// ===========================================================================
TEST(RotatorLimits, Offset0_Limit180_SafeRange_270to90)
{
    // offset=0, limit=180 → safe: 270°–360° and 0°–90° (i.e. within ±90° of 0°)
    EXPECT_FALSE(rotatorExceedsLimit(0.0,   0.0, 180.0));   // at offset
    EXPECT_FALSE(rotatorExceedsLimit(90.0,  0.0, 180.0));   // boundary CW
    EXPECT_FALSE(rotatorExceedsLimit(270.0, 0.0, 180.0));   // boundary CCW (wrap)
    EXPECT_TRUE (rotatorExceedsLimit(91.0,  0.0, 180.0));   // just outside CW
    EXPECT_TRUE (rotatorExceedsLimit(269.0, 0.0, 180.0));   // just outside CCW
    EXPECT_TRUE (rotatorExceedsLimit(180.0, 0.0, 180.0));   // antipodal — blocked
}

// ===========================================================================
// Test suite: typical limit, offset at 180° — scenario from the bug report
// ===========================================================================
TEST(RotatorLimits, Offset180_Limit90_SafeRange_135to225)
{
    // offset=180, limit=90 → safe: 135°–225° (±45° around 180°)
    EXPECT_FALSE(rotatorExceedsLimit(180.0, 180.0, 90.0));  // at offset
    EXPECT_FALSE(rotatorExceedsLimit(135.0, 180.0, 90.0));  // boundary CCW
    EXPECT_FALSE(rotatorExceedsLimit(225.0, 180.0, 90.0));  // boundary CW
    EXPECT_TRUE (rotatorExceedsLimit(134.0, 180.0, 90.0));  // just outside CCW
    EXPECT_TRUE (rotatorExceedsLimit(226.0, 180.0, 90.0));  // just outside CW
    EXPECT_TRUE (rotatorExceedsLimit(0.0,   180.0, 90.0));  // far away
    EXPECT_TRUE (rotatorExceedsLimit(360.0, 180.0, 90.0));  // 360° == 0°, far away
}

// ===========================================================================
// Test suite: wrap-around — offset near 360°
// ===========================================================================
TEST(RotatorLimits, Offset350_Limit90_SafeRange_305to35)
{
    // offset=350, limit=90 → safe: 305°–35° (spans the 0°/360° boundary)
    EXPECT_FALSE(rotatorExceedsLimit(350.0, 350.0, 90.0));  // at offset
    EXPECT_FALSE(rotatorExceedsLimit(305.0, 350.0, 90.0));  // boundary CCW
    EXPECT_FALSE(rotatorExceedsLimit(35.0,  350.0, 90.0));  // boundary CW (wrapped)
    EXPECT_FALSE(rotatorExceedsLimit(10.0,  350.0, 90.0));  // inside, past 0°
    EXPECT_FALSE(rotatorExceedsLimit(0.0,   350.0, 90.0));  // inside, exactly 0°
    EXPECT_TRUE (rotatorExceedsLimit(304.0, 350.0, 90.0));  // just outside CCW
    EXPECT_TRUE (rotatorExceedsLimit(36.0,  350.0, 90.0));  // just outside CW
    EXPECT_TRUE (rotatorExceedsLimit(180.0, 350.0, 90.0));  // far away
}

// ===========================================================================
// Test suite: target exactly at 180° (previous code had a gap — neither
// branch "<180" nor ">180" fired)
// ===========================================================================
TEST(RotatorLimits, TargetAt180_NoBranchGap)
{
    // With offset=0, limit=90, a target of exactly 180° is 180° away — blocked.
    EXPECT_TRUE(rotatorExceedsLimit(180.0, 0.0, 90.0));

    // With offset=180, limit=360 (full range), target=180° must be safe.
    EXPECT_FALSE(rotatorExceedsLimit(180.0, 180.0, 360.0));

    // With offset=180, limit=90, target=180° is at the offset — safe.
    EXPECT_FALSE(rotatorExceedsLimit(180.0, 180.0, 90.0));
}

// ===========================================================================
// Test suite: small limit — only tiny window allowed
// ===========================================================================
TEST(RotatorLimits, SmallLimit_TightWindow)
{
    // offset=90, limit=10 → safe: 85°–95°
    EXPECT_FALSE(rotatorExceedsLimit(90.0, 90.0, 10.0));   // at offset
    EXPECT_FALSE(rotatorExceedsLimit(85.0, 90.0, 10.0));   // boundary
    EXPECT_FALSE(rotatorExceedsLimit(95.0, 90.0, 10.0));   // boundary
    EXPECT_TRUE (rotatorExceedsLimit(84.0, 90.0, 10.0));   // outside
    EXPECT_TRUE (rotatorExceedsLimit(96.0, 90.0, 10.0));   // outside
}
