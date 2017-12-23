#include <gtest/gtest.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "celestrondriver.h"

using namespace Celestron;


TEST(CelestronDriverTest, trimDecAngle)
{
    ASSERT_FLOAT_EQ(0, trimDecAngle(0));
    ASSERT_FLOAT_EQ(0, trimDecAngle(180));
    ASSERT_FLOAT_EQ(0, trimDecAngle(360));

    ASSERT_FLOAT_EQ(45, trimDecAngle(45));
    ASSERT_FLOAT_EQ(90, trimDecAngle(90));
    ASSERT_FLOAT_EQ(85, trimDecAngle(95));
    ASSERT_FLOAT_EQ(5, trimDecAngle(175));

    ASSERT_FLOAT_EQ(-5, trimDecAngle(355));
    ASSERT_FLOAT_EQ(-20, trimDecAngle(200));
    ASSERT_FLOAT_EQ(-90, trimDecAngle(270));

    ASSERT_FLOAT_EQ(-5, trimDecAngle(-5));
    ASSERT_FLOAT_EQ(-20, trimDecAngle(-20));
    ASSERT_FLOAT_EQ(90, trimDecAngle(-270));

    ASSERT_FLOAT_EQ(-5, trimDecAngle(355 + 360));
}

TEST(CelestronDriverTest, dd2nex)
{
    ASSERT_EQ(0x0000, dd2nex(0.0));
    ASSERT_EQ(0x2000, dd2nex(45.0));
    ASSERT_EQ(0xc000, dd2nex(270.0));
    ASSERT_EQ(0x0000, dd2nex(360.0));
    ASSERT_EQ(0x12ce, dd2nex(26.4441));

    ASSERT_EQ(0x12ce, dd2nex(360 + 26.4441));
    ASSERT_EQ(0xc000, dd2nex(-90.0));
}

TEST(CelestronDriverTest, dd2pnex)
{
    ASSERT_EQ(0x00000000, dd2pnex(0.0));
    ASSERT_EQ(0x20000000, dd2pnex(45.0));
    ASSERT_EQ(0xc0000000, dd2pnex(270.0));
    ASSERT_EQ(0x00000000, dd2pnex(360.0));
    ASSERT_EQ(0x12ab0500, dd2pnex(26.25193834305));

    ASSERT_EQ(0x12ab0500, dd2pnex(360 + 26.25193834305));
    ASSERT_EQ(0xc0000000, dd2pnex(-90.0));
}

TEST(CelestronDriverTest, nex2dd)
{
    ASSERT_FLOAT_EQ(0.0, nex2dd(0x0000));
    ASSERT_FLOAT_EQ(45.0, nex2dd(0x2000));
    ASSERT_FLOAT_EQ(270.0, nex2dd(0xc000));
    ASSERT_FLOAT_EQ(337.5, nex2dd(0xf000));
    ASSERT_FLOAT_EQ(26.4441, nex2dd(0x12ce));
}

TEST(CelestronDriverTest, pnex2dd)
{
    ASSERT_FLOAT_EQ(0.0, pnex2dd(0x00000000));
    ASSERT_FLOAT_EQ(45.0, pnex2dd(0x20000000));
    ASSERT_FLOAT_EQ(270.0, pnex2dd(0xc0000000));
    ASSERT_FLOAT_EQ(337.5, pnex2dd(0xf0000000));
    ASSERT_FLOAT_EQ(26.25193834305, pnex2dd(0x12ab0500));
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
