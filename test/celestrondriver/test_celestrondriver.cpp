#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdarg.h>
#include <math.h>
#include <stdint.h>
#include <libnova/julian_day.h>
#include "indilogger.h"
#include "celestrondriver.h"


using namespace Celestron;
using ::testing::_;
using ::testing::StrEq;


// Define a new matcher that compares two byte arrays
MATCHER_P2(MemEq, buf, n, "") {
    uint8_t *b1 = (uint8_t *)buf;
    uint8_t *b2 = (uint8_t *)arg;

    for (int i=0; i<n; i++) {
        if (b1[i] != b2[i]) {
            *result_listener << "byte number " << i + 1 << " do not mach: " \
                << (int)b1[i] << " != " << (int)b2[i];
            return false;
        }
    }
    return true;
}


class MockCelestronDriver : public CelestronDriver {
    public:
        MockCelestronDriver() { fd = 1; simulation = false; }

        void set_response(const char *fmt, ...) {
            // simulate the response for the next command
            va_list args;
            va_start(args, fmt);
            vsprintf(response, fmt, args);
            va_end(args);
        }

        MOCK_METHOD3(serial_write, int (const char *cmd, int nbytes, int *nbytes_written));
        MOCK_METHOD2(serial_read, int (int nbytes, int *nbytes_read));

        int serial_read_section(char stop_char, int *nbytes_read) {
            INDI_UNUSED(stop_char);
            INDI_UNUSED(nbytes_read);
            return 0;
        }
};


TEST(CelestronDriverTest, set_simulation) {
    MockCelestronDriver driver;
    driver.set_simulation(true);

    EXPECT_CALL(driver, serial_write(_, _, _)).Times(0);
    EXPECT_CALL(driver, serial_read(_, _)).Times(0);
    EXPECT_TRUE(driver.echo());
}

TEST(CelestronDriverTest, echoCommand) {
    MockCelestronDriver driver;
    driver.set_response("x#");

    EXPECT_CALL(driver, serial_write(_, 2, _));
    EXPECT_TRUE(driver.echo());
}

TEST(CelestronDriverTest, syncCommand) {
    MockCelestronDriver driver;
    driver.set_response("#");

    EXPECT_CALL(driver, serial_write(StrEq("S2000,2000"), 10, _));
    EXPECT_TRUE(driver.sync(3.0, 45.0, false));

    EXPECT_CALL(driver, serial_write(StrEq("s20000000,20000000"), 18, _));
    EXPECT_TRUE(driver.sync(3.0, 45.0, true));
}

TEST(CelestronDriverTest, gotoCommands) {
    MockCelestronDriver driver;
    driver.set_response("#");

    EXPECT_CALL(driver, serial_write(StrEq("R2000,2000"), 10, _));
    EXPECT_TRUE(driver.slew_radec(3.0, 45.0, false));

    EXPECT_CALL(driver, serial_write(StrEq("r20000000,20000000"), 18, _));
    EXPECT_TRUE(driver.slew_radec(3.0, 45.0, true));

    EXPECT_CALL(driver, serial_write(StrEq("B2000,2000"), 10, _));
    EXPECT_TRUE(driver.slew_azalt(45.0, 45.0, false));

    EXPECT_CALL(driver, serial_write(StrEq("b20000000,20000000"), 18, _));
    EXPECT_TRUE(driver.slew_azalt(45.0, 45.0, true));
}

TEST(CelestronDriverTest, getCoordsCommands) {
    double ra, dec, az, alt;
    MockCelestronDriver driver;

    driver.set_response("2000,2000#");
    EXPECT_CALL(driver, serial_write(StrEq("E"), 1, _));
    EXPECT_TRUE(driver.get_radec(&ra, &dec, false));
    ASSERT_FLOAT_EQ(3.0, ra);
    ASSERT_FLOAT_EQ(45.0, dec);

    driver.set_response("20000000,20000000#");
    EXPECT_CALL(driver, serial_write(StrEq("e"), 1, _));
    EXPECT_TRUE(driver.get_radec(&ra, &dec, true));
    ASSERT_FLOAT_EQ(3.0, ra);
    ASSERT_FLOAT_EQ(45.0, dec);

    driver.set_response("2000,2000#");
    EXPECT_CALL(driver, serial_write(StrEq("Z"), 1, _));
    EXPECT_TRUE(driver.get_azalt(&az, &alt, false));
    ASSERT_FLOAT_EQ(45.0, az);
    ASSERT_FLOAT_EQ(45.0, alt);

    driver.set_response("20000000,20000000#");
    EXPECT_CALL(driver, serial_write(StrEq("z"), 1, _));
    EXPECT_TRUE(driver.get_azalt(&az, &alt, true));
    ASSERT_FLOAT_EQ(45.0, az);
    ASSERT_FLOAT_EQ(45.0, alt);
}

TEST(CelestronDriverTest, slewingCommands) {
    MockCelestronDriver driver;
    driver.set_response("#");
}

TEST(CelestronDriverTest, getVersionCommands) {
    char version[8];
    MockCelestronDriver driver;

    driver.set_response("\x04\x29#");
    EXPECT_CALL(driver, serial_write(StrEq("V"), 1, _));
    EXPECT_CALL(driver, serial_read(3, _));
    EXPECT_TRUE(driver.get_version(version, 8));
    ASSERT_STREQ(version, "4.41");

    driver.set_response("\x05\x07#");
    uint8_t cmd1[] = {'P', 1, 0x10, 0xfe, 0, 0, 0, 2};
    EXPECT_CALL(driver, serial_write(MemEq(cmd1, 8), 8, _));
    EXPECT_CALL(driver, serial_read(3, _));
    EXPECT_TRUE(driver.get_dev_firmware(CELESTRON_DEV_RA, version, 8));
    ASSERT_STREQ(version, "5.07");

    driver.set_response("\x03\x26#");
    uint8_t cmd2[] = {'P', 1, 0x11, 0xfe, 0, 0, 0, 2};
    EXPECT_CALL(driver, serial_write(MemEq(cmd2, 8), 8, _));
    EXPECT_CALL(driver, serial_read(3, _));
    EXPECT_TRUE(driver.get_dev_firmware(CELESTRON_DEV_DEC, version, 8));
    ASSERT_STREQ(version, "3.38");
}

TEST(CelestronDriverTest, setDateTime) {
    MockCelestronDriver driver;

    struct ln_date utc;
    utc.years = 2017;
    utc.months = 12;
    utc.days = 18;
    utc.hours = 10;
    utc.minutes = 35;
    utc.seconds = 43.1;

    driver.set_response("#");
    uint8_t cmd1[] = {'H', 8, 35, 43, 12, 18, 17, 254, 0};
    EXPECT_CALL(driver, serial_write(MemEq(cmd1, 9), 9, _));
    EXPECT_TRUE(driver.set_datetime(&utc, -2.0));
}

TEST(CelestronDriverTest, setLocation) {
    MockCelestronDriver driver;

    driver.set_response("#");
    uint8_t cmd1[] = {'W', 40, 25, 0, 0, 3, 42, 1, 1};
    EXPECT_CALL(driver, serial_write(MemEq(cmd1, 9), 9, _));
    EXPECT_TRUE(driver.set_location(-3.7003, 40.4167));
}

TEST(CelestronDriverTest, hibernate) {
    MockCelestronDriver driver;

    EXPECT_CALL(driver, serial_write(StrEq("x"), 1, _));
    EXPECT_CALL(driver, serial_read(_, _)).Times(0);
    EXPECT_TRUE(driver.hibernate());
}

/*
TEST(CelestronDriverTest, getLocation) {
    MockCelestronDriver driver;

    driver.set_response("%c%c%c%c%c%c%c%c#", 40, 25, 0, 0, 3, 42, 1, 1);
    EXPECT_CALL(driver, serial_write(StrEq("w"), 1, _));
    double lon, lat;
    EXPECT_TRUE(driver.get_location(&lon, &lat));
    ASSERT_FLOAT_EQ(-3.7003, lon);
    ASSERT_FLOAT_EQ(40.4167, lat);
}
*/

TEST(CelestronDriverTest, trimDecAngle) {
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

TEST(CelestronDriverTest, dd2nex) {
    ASSERT_EQ(0x0000U, dd2nex(0.0));
    ASSERT_EQ(0x2000U, dd2nex(45.0));
    ASSERT_EQ(0xc000U, dd2nex(270.0));
    ASSERT_EQ(0x0000U, dd2nex(360.0));
    ASSERT_EQ(0x12ceU, dd2nex(26.4441));

    ASSERT_EQ(0x12ceU, dd2nex(360 + 26.4441));
    ASSERT_EQ(0xc000U, dd2nex(-90.0));
}

TEST(CelestronDriverTest, dd2pnex) {
    ASSERT_EQ(0x00000000U, dd2pnex(0.0));
    ASSERT_EQ(0x20000000U, dd2pnex(45.0));
    ASSERT_EQ(0xc0000000U, dd2pnex(270.0));
    ASSERT_EQ(0x00000000U, dd2pnex(360.0));
    ASSERT_EQ(0x12ab0500U, dd2pnex(26.25193834305));

    ASSERT_EQ(0x12ab0500U, dd2pnex(360 + 26.25193834305));
    ASSERT_EQ(0xc0000000U, dd2pnex(-90.0));
}

TEST(CelestronDriverTest, nex2dd) {
    ASSERT_FLOAT_EQ(0.0, nex2dd(0x0000));
    ASSERT_FLOAT_EQ(45.0, nex2dd(0x2000));
    ASSERT_FLOAT_EQ(270.0, nex2dd(0xc000));
    ASSERT_FLOAT_EQ(337.5, nex2dd(0xf000));
    ASSERT_FLOAT_EQ(26.4441, nex2dd(0x12ce));
}

TEST(CelestronDriverTest, pnex2dd) {
    ASSERT_FLOAT_EQ(0.0, pnex2dd(0x00000000));
    ASSERT_FLOAT_EQ(45.0, pnex2dd(0x20000000));
    ASSERT_FLOAT_EQ(270.0, pnex2dd(0xc0000000));
    ASSERT_FLOAT_EQ(337.5, pnex2dd(0xf0000000));
    ASSERT_FLOAT_EQ(26.25193834305, pnex2dd(0x12ab0500));
}


int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
            INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
