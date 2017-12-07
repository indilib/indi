#include <gtest/gtest.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
//#include "../drivers/telescope/celestrondriver.h"



void hex_dump(char *buf, const char *data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", (unsigned char)data[i]);

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

inline double trimAngle(double angle)
{
    angle = angle - 360*floor(angle/360);
    if (angle < 0)
        angle += 360.0;

    if ((angle > 90.) && (angle <= 270.))
        angle = 180. - angle;
    else if ((angle > 270.) && (angle <= 360.))
        angle = angle - 360.;

    return angle;
}

// Convert decimal degrees to NexStar angle
inline uint16_t dd2nex(double angle)
{
    angle = angle - 360*floor(angle/360);
    if (angle < 0)
        angle += 360.0;

    return (uint16_t)(angle * 0x10000 / 360.0);
}

// Convert decimal degrees to NexStar angle (precise)
inline uint32_t dd2pnex(double angle)
{
    angle = angle - 360*floor(angle/360);
    if (angle < 0)
        angle += 360.0;

    return (uint32_t)(angle * 0x100000000 / 360.0);
}

// Convert NexStar angle to decimal degrees
inline double nex2dd(uint16_t value)
{
    return 360.0 * ((double)value / 0x10000);
}

// Convert NexStar angle to decimal degrees (precise)
inline double pnex2dd(uint32_t value)
{
    return 360.0 * ((double)value / 0x100000000);
}


TEST(CelestronDriverTest, trimAngle)
{
    ASSERT_FLOAT_EQ(0, trimAngle(0));
    ASSERT_FLOAT_EQ(0, trimAngle(180));
    ASSERT_FLOAT_EQ(0, trimAngle(360));

    ASSERT_FLOAT_EQ(45, trimAngle(45));
    ASSERT_FLOAT_EQ(90, trimAngle(90));
    ASSERT_FLOAT_EQ(85, trimAngle(95));
    ASSERT_FLOAT_EQ(5, trimAngle(175));

    ASSERT_FLOAT_EQ(-5, trimAngle(355));
    ASSERT_FLOAT_EQ(-20, trimAngle(200));
    ASSERT_FLOAT_EQ(-90, trimAngle(270));

    ASSERT_FLOAT_EQ(-5, trimAngle(-5));
    ASSERT_FLOAT_EQ(-20, trimAngle(-20));
    ASSERT_FLOAT_EQ(90, trimAngle(-270));

    ASSERT_FLOAT_EQ(-5, trimAngle(355 + 360));
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
