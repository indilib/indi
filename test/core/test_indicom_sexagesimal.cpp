/*
    Tests of getSexComponentsIIDRounded(): the seconds are rounded to the requested decimals and a rounding to
    60 seconds is carried into the minutes and degrees, so that printing the seconds with that many decimals
    never shows 60.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "indicom.h"

namespace
{

struct Components
{
    int d;
    int m;
    double s;
};

Components rounded(double value, int decimals)
{
    Components c {0, 0, 0};
    getSexComponentsIIDRounded(value, &c.d, &c.m, &c.s, decimals);
    return c;
}

}

TEST(GetSexComponentsIIDRounded, RoundsTheSeconds)
{
    // 12.3456789 degrees = 12 20' 44.444"
    Components c = rounded(12.3456789, 1);
    EXPECT_EQ(c.d, 12);
    EXPECT_EQ(c.m, 20);
    EXPECT_DOUBLE_EQ(c.s, 44.4);

    c = rounded(12.3456789, 2);
    EXPECT_DOUBLE_EQ(c.s, 44.44);

    c = rounded(12.3456789, 0);
    EXPECT_DOUBLE_EQ(c.s, 44);
}

TEST(GetSexComponentsIIDRounded, CarriesSixtySecondsIntoTheMinutesAndDegrees)
{
    // 0.99999 degrees = 0 59' 59.964", which rounds to 1 00' 00.0"
    Components c = rounded(0.99999, 1);
    EXPECT_EQ(c.d, 1);
    EXPECT_EQ(c.m, 0);
    EXPECT_DOUBLE_EQ(c.s, 0);

    // 5 59' 59.9964" with two decimals is 6 00' 00.00"
    c = rounded(5.999999, 2);
    EXPECT_EQ(c.d, 6);
    EXPECT_EQ(c.m, 0);
    EXPECT_DOUBLE_EQ(c.s, 0);

    // 10 14' 59.97" with one decimal is 10 15' 00.0"
    c = rounded(10 + 14 / 60.0 + 59.97 / 3600.0, 1);
    EXPECT_EQ(c.d, 10);
    EXPECT_EQ(c.m, 15);
    EXPECT_DOUBLE_EQ(c.s, 0);
}

TEST(GetSexComponentsIIDRounded, StoresTheSignInTheDegrees)
{
    Components c = rounded(-10.5, 1);
    EXPECT_EQ(c.d, -10);
    EXPECT_EQ(c.m, 30);
    EXPECT_DOUBLE_EQ(c.s, 0);

    c = rounded(-0.99999, 1);
    EXPECT_EQ(c.d, -1);
    EXPECT_EQ(c.m, 0);
    EXPECT_DOUBLE_EQ(c.s, 0);

    // As with getSexComponents(), the sign of values between -1 and 0 is lost in d
    c = rounded(-0.5, 1);
    EXPECT_EQ(c.d, 0);
    EXPECT_EQ(c.m, 30);
}

TEST(GetSexComponentsIIDRounded, LimitsTheDecimals)
{
    EXPECT_DOUBLE_EQ(rounded(12.3456789, -3).s, 44);
    EXPECT_DOUBLE_EQ(rounded(12.3456789, 6).s, rounded(12.3456789, 99).s);
}

namespace
{

// Prints the seconds with the given decimals and checks them against the value.
void checkPrinted(double value, int decimals)
{
    const double unit = std::pow(10.0, -decimals);
    Components c = rounded(value, decimals);
    char seconds[32];
    snprintf(seconds, sizeof(seconds), "%.*f", decimals, c.s);

    ASSERT_GE(c.m, 0) << value;
    ASSERT_LT(c.m, 60) << value;
    ASSERT_LT(atof(seconds), 60.0) << "value " << value << " seconds " << seconds;
    ASSERT_TRUE(value < 0 ? c.d <= 0 : c.d >= 0) << value;

    // The printed components must be within half a unit of the value
    const double printed = std::abs(c.d) + c.m / 60.0 + atof(seconds) / 3600.0;
    ASSERT_NEAR(printed, std::fabs(value), unit / 2 / 3600.0 + 1e-9) << value;
}

}

TEST(GetSexComponentsIIDRounded, PrintedSecondsAreNeverSixtyAndMatchTheValue)
{
    for (int decimals = 0; decimals <= 3; decimals++)
    {
        const double unit = std::pow(10.0, -decimals) / 3600.0;

        // Fine steps across two units on each side of minute boundaries, where the seconds round up to 60
        const int degrees[] = {0, 1, 44, 89};
        const int minutes[] = {0, 30, 59};
        const int signs[] = {1, -1};
        for (int d : degrees)
            for (int m : minutes)
                for (int k = -2000; k <= 2000; k++)
                    for (int sign : signs)
                    {
                        checkPrinted(sign * (d + (m + 1) / 60.0 + k * unit * 0.0011), decimals);
                        if (HasFatalFailure())
                            return;
                    }

        // Coarse steps over the whole range
        for (double value = -90.0; value <= 90.0; value += 0.000737)
        {
            checkPrinted(value, decimals);
            if (HasFatalFailure())
                return;
        }
    }
}
