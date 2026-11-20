/*
    OnStepX INDI Driver — test_onstepxformat.cpp
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
    OnStepX INDI Driver — OnStepXFormat unit tests

    Expected strings follow the OnStepX firmware parsers:
      :Sr  Convert::hmsToDouble  PM_HIGHEST  "HH:MM:SS.SSSS"
      :Sd  Convert::dmsToDouble  PM_HIGHEST  "sDD:MM:SS.SSS" (sign mandatory)
    Both parsers reject space padding and single-digit leading fields.
*/

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <string>

#include "OnStepXFormat.h"

static std::string ra(double h)
{
    char buf[32];
    EXPECT_TRUE(OnStepXFormat::formatRA(h, buf, sizeof(buf)));
    return buf;
}

static std::string dec(double d)
{
    char buf[32];
    EXPECT_TRUE(OnStepXFormat::formatDec(d, buf, sizeof(buf)));
    return buf;
}

TEST(OnStepXFormatTest, ra_single_digit_hours_zero_padded)
{
    EXPECT_EQ(ra(5.5), "05:30:00.0000");
    EXPECT_EQ(ra(0.25), "00:15:00.0000");
    EXPECT_EQ(ra(0.0), "00:00:00.0000");
}

TEST(OnStepXFormatTest, ra_two_digit_hours)
{
    EXPECT_EQ(ra(12.25), "12:15:00.0000");
    EXPECT_EQ(ra(23.5), "23:30:00.0000");
}

TEST(OnStepXFormatTest, ra_fractional_seconds)
{
    // 1h 2m 3.4567s
    EXPECT_EQ(ra(1.0 + 2.0 / 60.0 + 3.4567 / 3600.0), "01:02:03.4567");
}

TEST(OnStepXFormatTest, ra_rounding_carries_into_next_field)
{
    // 59.99996 s rounds up to the next minute, never "60" seconds
    EXPECT_EQ(ra(1.0 + 59.0 / 60.0 + 59.99996 / 3600.0), "02:00:00.0000");
}

TEST(OnStepXFormatTest, ra_wraps_into_0_24)
{
    EXPECT_EQ(ra(24.0), "00:00:00.0000");
    EXPECT_EQ(ra(23.99999999999), "00:00:00.0000");
    EXPECT_EQ(ra(-1.0), "23:00:00.0000");
    EXPECT_EQ(ra(25.5), "01:30:00.0000");
}

TEST(OnStepXFormatTest, ra_always_13_chars)
{
    for (double h = 0.0; h < 24.0; h += 0.37)
        EXPECT_EQ(ra(h).size(), 13u) << h;
}

TEST(OnStepXFormatTest, dec_positive_has_explicit_plus)
{
    EXPECT_EQ(dec(45.5), "+45:30:00.000");
    EXPECT_EQ(dec(0.0), "+00:00:00.000");
    EXPECT_EQ(dec(90.0), "+90:00:00.000");
}

TEST(OnStepXFormatTest, dec_small_values_zero_padded)
{
    EXPECT_EQ(dec(5.5), "+05:30:00.000");
    EXPECT_EQ(dec(-5.5), "-05:30:00.000");
    EXPECT_EQ(dec(-0.5), "-00:30:00.000");
}

TEST(OnStepXFormatTest, dec_negative)
{
    EXPECT_EQ(dec(-45.5), "-45:30:00.000");
    EXPECT_EQ(dec(-90.0), "-90:00:00.000");
}

TEST(OnStepXFormatTest, dec_rounding_carries_into_next_field)
{
    // 59.9996" rounds up to the next arcminute
    EXPECT_EQ(dec(10.0 + 59.0 / 60.0 + 59.9996 / 3600.0), "+11:00:00.000");
    EXPECT_EQ(dec(-(10.0 + 59.0 / 60.0 + 59.9996 / 3600.0)), "-11:00:00.000");
}

TEST(OnStepXFormatTest, dec_always_13_chars)
{
    for (double d = -90.0; d <= 90.0; d += 0.73)
        EXPECT_EQ(dec(d).size(), 13u) << d;
}

TEST(OnStepXFormatTest, rejects_invalid_input)
{
    char buf[32];
    EXPECT_FALSE(OnStepXFormat::formatRA(NAN, buf, sizeof(buf)));
    EXPECT_FALSE(OnStepXFormat::formatRA(INFINITY, buf, sizeof(buf)));
    EXPECT_FALSE(OnStepXFormat::formatDec(NAN, buf, sizeof(buf)));
    EXPECT_FALSE(OnStepXFormat::formatDec(90.0001, buf, sizeof(buf)));
    EXPECT_FALSE(OnStepXFormat::formatDec(-90.0001, buf, sizeof(buf)));
}

// Firmware prints "nan" for unavailable sensor values (seen on hardware:
// :GX9A# -> "nan#" with no weather sensor).  strtod() accepts it.
TEST(OnStepXFormatTest, parse_number_finite_only)
{
    double v = 0;
    EXPECT_TRUE(OnStepXFormat::parseNumber("12.5", v));
    EXPECT_DOUBLE_EQ(v, 12.5);
    EXPECT_TRUE(OnStepXFormat::parseNumber("-3", v));
    EXPECT_DOUBLE_EQ(v, -3.0);
    EXPECT_TRUE(OnStepXFormat::parseNumber(" 9.0", v));      // "%3.1f" padding
    EXPECT_TRUE(OnStepXFormat::parseNumber("0", v));

    EXPECT_FALSE(OnStepXFormat::parseNumber("nan", v));
    EXPECT_FALSE(OnStepXFormat::parseNumber("NAN", v));
    EXPECT_FALSE(OnStepXFormat::parseNumber("inf", v));
    EXPECT_FALSE(OnStepXFormat::parseNumber("", v));
    EXPECT_FALSE(OnStepXFormat::parseNumber("12.5x", v));
    EXPECT_FALSE(OnStepXFormat::parseNumber(nullptr, v));
}

TEST(OnStepXFormatTest, rejects_small_buffer)
{
    char buf[13];   // needs 14 including terminator
    EXPECT_FALSE(OnStepXFormat::formatRA(5.5, buf, sizeof(buf)));
    EXPECT_FALSE(OnStepXFormat::formatDec(5.5, buf, sizeof(buf)));
}
