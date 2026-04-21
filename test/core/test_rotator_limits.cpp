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
 * The safe zone is a symmetric window of ±(limit/2) degrees centered on a
 * fixed cable-neutral reference point (m_RotatorOffset). The reference is set
 * at connect time from the hardware position and updated on every sync.
 * This prevents cable wrap regardless of how many incremental moves are made.
 * The check is disabled when limit == 0.
 *
 * Formula:
 *   delta   = target - reference          (normalized to [-180, +180])
 *   blocked = (limit > 0) && (|delta| > limit / 2)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Pure reimplementation of the formula from indirotatorinterface.cpp so the
// tests are self-contained and do not require linking the full INDI library.
// ---------------------------------------------------------------------------
static bool rotatorExceedsLimit(double target, double reference, double limit)
{
    if (limit <= 0.0)
        return false;                           // limits disabled

    double delta = target - reference;
    while (delta >  180.0) delta -= 360.0;
    while (delta < -180.0) delta += 360.0;
    return std::abs(delta) > limit / 2.0;
}

// ===========================================================================
// Test suite: disabled limits (limit == 0)
// ===========================================================================
TEST(RotatorLimits, DisabledLimits_NothingBlocked)
{
    // limit == 0 means no restriction regardless of target or reference
    EXPECT_FALSE(rotatorExceedsLimit(0.0,   0.0, 0.0));
    EXPECT_FALSE(rotatorExceedsLimit(180.0, 0.0, 0.0));
    EXPECT_FALSE(rotatorExceedsLimit(359.0, 0.0, 0.0));
    EXPECT_FALSE(rotatorExceedsLimit(270.0, 45.0, 0.0));
}

// ===========================================================================
// Test suite: full range (limit == 360) — nothing should be blocked
// ===========================================================================
TEST(RotatorLimits, FullRange_NothingBlocked)
{
    // With limit=360, the safe window is ±180° — every target is within range.
    EXPECT_FALSE(rotatorExceedsLimit(181.0, 0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(340.0, 0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(350.0, 0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(358.0, 0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(359.0, 0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(0.0,   0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(90.0,  0.0, 360.0));
    EXPECT_FALSE(rotatorExceedsLimit(180.0, 0.0, 360.0));
}

// ===========================================================================
// Test suite: cable wrap scenario — the fundamental correctness test.
// With reference=0, limit=360 (±180°), incremental moves must not bypass
// the limit by "walking" around the circle.
// ===========================================================================
TEST(RotatorLimits, CableWrap_IncrementalMovesCannotBypassLimit)
{
    // reference=0, limit=190 (±95°)
    // Move 1: 0 → 80°  (delta=80, within ±95) — allowed
    EXPECT_FALSE(rotatorExceedsLimit(80.0,  0.0, 190.0));
    // Move 2: 80 → 170° (delta from reference=0 is 170, exceeds 95) — BLOCKED
    // (Even though it's only 90° from current position 80°, it's 170° from reference)
    EXPECT_TRUE (rotatorExceedsLimit(170.0, 0.0, 190.0));
    // Move 3: 0 → 350° (delta = -10°, within ±95) — allowed (350° is 10° CCW from 0)
    EXPECT_FALSE(rotatorExceedsLimit(350.0, 0.0, 190.0));
    // Move 4: 350 → 265° — from reference 0°, 265° = -95° (exactly on boundary) — allowed
    EXPECT_FALSE(rotatorExceedsLimit(265.0, 0.0, 190.0));
    // Move 5: 350 → 264° — from reference 0°, 264° = -96° (just outside) — BLOCKED
    EXPECT_TRUE (rotatorExceedsLimit(264.0, 0.0, 190.0));
}

// ===========================================================================
// Test suite: typical limit, reference at 0°
// ===========================================================================
TEST(RotatorLimits, Reference0_Limit180_SafeRange_270to90)
{
    // reference=0, limit=180 → safe: 270°–360° and 0°–90° (i.e. within ±90° of 0°)
    EXPECT_FALSE(rotatorExceedsLimit(0.0,   0.0, 180.0));   // at reference
    EXPECT_FALSE(rotatorExceedsLimit(90.0,  0.0, 180.0));   // boundary CW
    EXPECT_FALSE(rotatorExceedsLimit(270.0, 0.0, 180.0));   // boundary CCW (wrap)
    EXPECT_TRUE (rotatorExceedsLimit(91.0,  0.0, 180.0));   // just outside CW
    EXPECT_TRUE (rotatorExceedsLimit(269.0, 0.0, 180.0));   // just outside CCW
    EXPECT_TRUE (rotatorExceedsLimit(180.0, 0.0, 180.0));   // antipodal — blocked
}

// ===========================================================================
// Test suite: typical limit, reference at 180°
// ===========================================================================
TEST(RotatorLimits, Reference180_Limit90_SafeRange_135to225)
{
    // reference=180, limit=90 → safe: 135°–225° (±45° around 180°)
    EXPECT_FALSE(rotatorExceedsLimit(180.0, 180.0, 90.0));  // at reference
    EXPECT_FALSE(rotatorExceedsLimit(135.0, 180.0, 90.0));  // boundary CCW
    EXPECT_FALSE(rotatorExceedsLimit(225.0, 180.0, 90.0));  // boundary CW
    EXPECT_TRUE (rotatorExceedsLimit(134.0, 180.0, 90.0));  // just outside CCW
    EXPECT_TRUE (rotatorExceedsLimit(226.0, 180.0, 90.0));  // just outside CW
    EXPECT_TRUE (rotatorExceedsLimit(0.0,   180.0, 90.0));  // far away
    EXPECT_TRUE (rotatorExceedsLimit(360.0, 180.0, 90.0));  // 360° == 0°, far away
}

// ===========================================================================
// Test suite: wrap-around — reference near 360°
// ===========================================================================
TEST(RotatorLimits, Reference350_Limit90_SafeRange_305to35)
{
    // reference=350, limit=90 → safe: 305°–35° (spans the 0°/360° boundary)
    EXPECT_FALSE(rotatorExceedsLimit(350.0, 350.0, 90.0));  // at reference
    EXPECT_FALSE(rotatorExceedsLimit(305.0, 350.0, 90.0));  // boundary CCW
    EXPECT_FALSE(rotatorExceedsLimit(35.0,  350.0, 90.0));  // boundary CW (wrapped)
    EXPECT_FALSE(rotatorExceedsLimit(10.0,  350.0, 90.0));  // inside, past 0°
    EXPECT_FALSE(rotatorExceedsLimit(0.0,   350.0, 90.0));  // inside, exactly 0°
    EXPECT_TRUE (rotatorExceedsLimit(304.0, 350.0, 90.0));  // just outside CCW
    EXPECT_TRUE (rotatorExceedsLimit(36.0,  350.0, 90.0));  // just outside CW
    EXPECT_TRUE (rotatorExceedsLimit(180.0, 350.0, 90.0));  // far away
}

// ===========================================================================
// Test suite: small limit — only tiny window allowed
// ===========================================================================
TEST(RotatorLimits, SmallLimit_TightWindow)
{
    // reference=90, limit=10 → safe: 85°–95°
    EXPECT_FALSE(rotatorExceedsLimit(90.0, 90.0, 10.0));   // at reference
    EXPECT_FALSE(rotatorExceedsLimit(85.0, 90.0, 10.0));   // boundary
    EXPECT_FALSE(rotatorExceedsLimit(95.0, 90.0, 10.0));   // boundary
    EXPECT_TRUE (rotatorExceedsLimit(84.0, 90.0, 10.0));   // outside
    EXPECT_TRUE (rotatorExceedsLimit(96.0, 90.0, 10.0));   // outside
}

// ===========================================================================
// Test suite: user-reported bug scenario
// limit=190, reference=0, rotator moves to 10, then 90, then commanded to 265.24
// With the fixed reference approach, 265.24° from reference 0° is -94.76° — allowed.
// But 264° from reference 0° is -96° — blocked.
// ===========================================================================
TEST(RotatorLimits, BugReport_Limit190_Reference0)
{
    // reference=0, limit=190 (±95°)
    // 265.24° from 0° = -94.76° (within ±95°) — allowed
    EXPECT_FALSE(rotatorExceedsLimit(265.24, 0.0, 190.0));
    // 264° from 0° = -96° (outside ±95°) — blocked
    EXPECT_TRUE (rotatorExceedsLimit(264.0,  0.0, 190.0));
    // The intermediate moves (10°, 90°) don't change the reference — still 0°
    EXPECT_FALSE(rotatorExceedsLimit(10.0,  0.0, 190.0));
    EXPECT_FALSE(rotatorExceedsLimit(90.0,  0.0, 190.0));
}
