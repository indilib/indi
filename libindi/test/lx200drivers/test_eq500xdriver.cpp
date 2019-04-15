#include "lx200generic.h"
#include "eq500x.h"

#include "indicom.h"
#include "lx200driver.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdarg.h>
#include <cmath>
#include <memory>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <cassert>

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

using ::testing::_;
using ::testing::StrEq;

char * me = "MockEQ500XDriver";
class MockEQ500XDriver : public EQ500X
{
public:
    MockEQ500XDriver() : EQ500X()
    {
        ISGetProperties("");
        setSimulation(true);
        setConnected(true);
    }
public:
    // Default LST for this driver is 6 - RA is east when starting up
    double LST { 6 };
    double getLST() { return LST; }
    bool getPosition(MechanicalPoint &p) { return getCurrentPosition(p); }
    void setLongitude(double lng) {
        /* Say it's 0h on Greenwich meridian (GHA=0) - express LST as hours */
        LST = 0.0 + lng/15.0;
        updateLocation(0,lng,0);
    }
};


// Right ascension is normal sexagesimal mapping.
//
// HA = LST - RA
//
// South is HA = +0,  RA = LST
// East  is HA = -6,  RA = LST+6
// North is HA = -12, RA = LST+12 on the east side
// West  is HA = +6,  RA = LST-6
// North is HA = +12, RA = LST-12 on the west side
//
// Telescope on western side of pier is 12 hours later than
// telescope on eastern side of pier.
//
// PierEast             (LST = -6)           PierWest
// E +12.0h = LST-18 <-> 12:00:00 <-> LST-18 = +00.0h W
// N +18.0h = LST-12 <-> 18:00:00 <-> LST-12 = +06.0h N
// W +00.0h = LST-6  <-> 00:00:00 <-> LST-6  = +12.0h E
// S +06.0h = LST+0  <-> 06:00:00 <-> LST+0  = +18.0h S
// E +12.0h = LST+6  <-> 12:00:00 <-> LST+6  = +00.0h W
// N +18.0h = LST+12 <-> 18:00:00 <-> LST+12 = +06.0h N
// W +00.0h = LST+18 <-> 00:00:00 <-> LST+18 = +12.0h E

TEST(EQ500XDriverTest, test_LSTSync)
{
    MockEQ500XDriver d;

    ASSERT_TRUE(d.isConnected());

    EQ500X::MechanicalPoint p;
    // Assign a longitude that makes the RA of the scope point east - default position is 90° east
    d.setLongitude(6*15);
    ASSERT_FALSE(d.getPosition(p));
    ASSERT_DOUBLE_EQ(  +0.0, p.RAm());
    ASSERT_DOUBLE_EQ( +90.0, p.DECm());
    // Assign a new longitude
    d.setLongitude(5*15);
    ASSERT_FALSE(d.getPosition(p));
    ASSERT_DOUBLE_EQ(  23.0, p.RAm());
    ASSERT_DOUBLE_EQ( +90.0, p.DECm());
    // Assign a new longitude - but this time the mount is not considered "parked" east/pole and does not sync
    d.setLongitude(7*15);
    ASSERT_FALSE(d.getPosition(p));
    ASSERT_DOUBLE_EQ(  23.0, p.RAm());  // Expected 1h - not possible to assign longitude without restarting the mount
    ASSERT_DOUBLE_EQ( +90.0, p.DECm());
}

TEST(EQ500XDriverTest, test_PierFlip)
{
    EQ500X::MechanicalPoint p;
    char b[64] = {0};

    // Mechanical point doesn't care about LST as it assumes the mount
    // is properly synced already. It only considers the pier side.
    ASSERT_DOUBLE_EQ(  0.0, p.RAm ( +0.0));

    ASSERT_DOUBLE_EQ(+90.0, p.DECm(+90.0));
    ASSERT_EQ(INDI::Telescope::PIER_WEST, p.setPierSide(INDI::Telescope::PIER_WEST));
    ASSERT_FALSE(strncmp( "12:00:00", p.toStringRA (b,64),64));
    ASSERT_FALSE(strncmp("+00:00:00", p.toStringDEC(b,64),64));
    ASSERT_EQ(INDI::Telescope::PIER_EAST, p.setPierSide(INDI::Telescope::PIER_EAST));
    ASSERT_FALSE(strncmp( "00:00:00", p.toStringRA (b,64),64));
    ASSERT_FALSE(strncmp("+00:00:00", p.toStringDEC(b,64),64));

    ASSERT_DOUBLE_EQ(+80.0, p.DECm(+80.0));
    ASSERT_EQ(INDI::Telescope::PIER_WEST, p.setPierSide(INDI::Telescope::PIER_WEST));
    ASSERT_FALSE(strncmp( "12:00:00", p.toStringRA (b,64),64));
    ASSERT_FALSE(strncmp("-10:00:00", p.toStringDEC(b,64),64));
    ASSERT_EQ(INDI::Telescope::PIER_EAST, p.setPierSide(INDI::Telescope::PIER_EAST));
    ASSERT_FALSE(strncmp( "00:00:00", p.toStringRA (b,64),64));
    ASSERT_FALSE(strncmp("+10:00:00", p.toStringDEC(b,64),64));

    ASSERT_DOUBLE_EQ(+70.0, p.DECm(+70.0));
    ASSERT_EQ(INDI::Telescope::PIER_WEST, p.setPierSide(INDI::Telescope::PIER_WEST));
    ASSERT_FALSE(strncmp( "12:00:00", p.toStringRA (b,64),64));
    ASSERT_FALSE(strncmp("-20:00:00", p.toStringDEC(b,64),64));
    ASSERT_EQ(INDI::Telescope::PIER_EAST, p.setPierSide(INDI::Telescope::PIER_EAST));
    ASSERT_FALSE(strncmp( "00:00:00", p.toStringRA (b,64),64));
    ASSERT_FALSE(strncmp("+20:00:00", p.toStringDEC(b,64),64));
}

TEST(EQ500XDriverTest, test_EastSideOfPier_RA_Conversions)
{
    EQ500X::MechanicalPoint p;
    char b[64]= {0};

    ASSERT_EQ(INDI::Telescope::PIER_EAST, p.setPierSide(INDI::Telescope::PIER_EAST));

    ASSERT_FALSE(p.parseStringRA("00:00:00",8));
    ASSERT_DOUBLE_EQ( +0.0, p.RAm());
    ASSERT_FALSE(strncmp("00:00:00",p.toStringRA(b,64),64));

    ASSERT_FALSE(p.parseStringRA("06:00:00",8));
    ASSERT_DOUBLE_EQ( +6.0, p.RAm());
    ASSERT_FALSE(strncmp("06:00:00",p.toStringRA(b,64),64));

    ASSERT_FALSE(p.parseStringRA("12:00:00",8));
    ASSERT_DOUBLE_EQ(+12.0, p.RAm());
    ASSERT_FALSE(strncmp("12:00:00",p.toStringRA(b,64),64));

    ASSERT_FALSE(p.parseStringRA("18:00:00",8));
    ASSERT_DOUBLE_EQ(+18.0, p.RAm());
    ASSERT_FALSE(strncmp("18:00:00",p.toStringRA(b,64),64));

    ASSERT_FALSE(p.parseStringRA("24:00:00",8));
    ASSERT_DOUBLE_EQ( +0.0, p.RAm());
    ASSERT_FALSE(strncmp("00:00:00",p.toStringRA(b,64),64));

    ASSERT_FALSE(p.parseStringRA("00:00:01",8));
    ASSERT_NEAR(1/3600.0, p.RAm(), 1/3600.0);
    ASSERT_FALSE(strncmp("00:00:01",p.toStringRA(b,64),64));

    ASSERT_FALSE(p.parseStringRA("00:01:00",8));
    ASSERT_NEAR(1/60.0, p.RAm(), 1/3600.0);
    ASSERT_FALSE(strncmp("00:01:00",p.toStringRA(b,64),64));
}

TEST(EQ500XDriverTest, test_WestSideOfPier_RA_Conversions)
{
    EQ500X::MechanicalPoint p;
    char b[64]= {0};

    ASSERT_EQ(INDI::Telescope::PIER_WEST, p.setPierSide(INDI::Telescope::PIER_WEST));

    ASSERT_FALSE(p.parseStringRA("00:00:00",8));
    ASSERT_EQ(+12.0, p.RAm());
    ASSERT_FALSE(strncmp("00:00:00",p.toStringRA(b,64),64));

    ASSERT_FALSE(p.parseStringRA("06:00:00",8));
    ASSERT_EQ(+18.0, p.RAm());
    ASSERT_FALSE(strncmp("06:00:00",p.toStringRA(b,64),64));

    ASSERT_FALSE(p.parseStringRA("12:00:00",8));
    ASSERT_EQ( +0.0, p.RAm());
    ASSERT_FALSE(strncmp("12:00:00",p.toStringRA(b,64),64));

    ASSERT_FALSE(p.parseStringRA("18:00:00",8));
    ASSERT_EQ( +6.0, p.RAm());
    ASSERT_FALSE(strncmp("18:00:00",p.toStringRA(b,64),64));

    ASSERT_FALSE(p.parseStringRA("24:00:00",8));
    ASSERT_EQ(+12.0, p.RAm());
    ASSERT_FALSE(strncmp("00:00:00",p.toStringRA(b,64),64));
}

// Declination goes from -255:59:59 to +255:59:59
//
// Tenths and hundredths share the same character:
// - 0-9 is mapped to {0,1,2,3,4,5,6,7,8,9}
// - 10-16 is mapped to {:,;,<,=,>,?,@}
// - 17-25 is mapped to {A,B,C,D,E,F,G,H,I}
//
// PierWest                           PierEast
// -165.0° <-> -255.0 = -I5:00:00 <-> +345.0°
// -135.0° <-> -225.0 = -F5:00:00 <-> +315.0°
//  -90.0° <-> -180.0 = -B0:00:00 <-> +270.0°
//  -45.0° <-> -135.0 = -=5:00:00 <-> +225.0°
//  +00.0° <->  -90.0 = -90:00:00 <-> +180.0°
//  +45.0° <->  -45.0 = -45:00:00 <-> +135.0°
//  +90.0° <->    0.0 = +00:00:00 <->  +90.0°
// +135.0° <->   45.0 = +45:00:00 <->  +45.0°
// +180.0° <->   90.0 = +90:00:00 <->  +00.0°
// +225.0° <->  135.0 = +=5:00:00 <->  -45.0°
// +270.0° <->  180.0 = +B0:00:00 <->  -90.0°
// +315.0° <->  225.0 = +F5:00:00 <-> -135.0°
// +345.0° <->  255.0 = +I5:00:00 <-> -165.0°

TEST(EQ500XDriverTest, test_WestSideOfPier_DEC_Conversions)
{
    EQ500X::MechanicalPoint p;
    char b[64]= {0};

    ASSERT_EQ(INDI::Telescope::PIER_WEST, p.setPierSide(INDI::Telescope::PIER_WEST));
    ASSERT_FALSE(p.parseStringDEC("-I5:00:00",9));
    ASSERT_EQ(-165.0, p.DECm());
    ASSERT_FALSE(strncmp("-I5:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("-F5:00:00",9));
    ASSERT_EQ(-135.0, p.DECm());
    ASSERT_FALSE(strncmp("-F5:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("-B0:00:00",9));
    ASSERT_EQ( -90.0, p.DECm());
    ASSERT_FALSE(strncmp("-B0:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("-=5:00:00",9));
    ASSERT_EQ( -45.0, p.DECm());
    ASSERT_FALSE(strncmp("-=5:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("-90:00:00",9));
    ASSERT_EQ(  +0.0, p.DECm());
    ASSERT_FALSE(strncmp("-90:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("-45:00:00",9));
    ASSERT_EQ( +45.0, p.DECm());
    ASSERT_FALSE(strncmp("-45:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+00:00:00",9));
    ASSERT_EQ( +90.0, p.DECm());
    ASSERT_FALSE(strncmp("+00:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+45:00:00",9));
    ASSERT_EQ(+135.0, p.DECm());
    ASSERT_FALSE(strncmp("+45:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+90:00:00",9));
    ASSERT_EQ(+180.0, p.DECm());
    ASSERT_FALSE(strncmp("+90:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+=5:00:00",9));
    ASSERT_EQ(+225.0, p.DECm());
    ASSERT_FALSE(strncmp("+=5:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+B0:00:00",9));
    ASSERT_EQ(+270.0, p.DECm());
    ASSERT_FALSE(strncmp("+B0:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+F5:00:00",9));
    ASSERT_EQ(+315.0, p.DECm());
    ASSERT_FALSE(strncmp("+F5:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+I5:00:00",9));
    ASSERT_EQ(+345.0, p.DECm());
    ASSERT_FALSE(strncmp("+I5:00:00",p.toStringDEC(b,64),64));

    ASSERT_FALSE(p.parseStringDEC("+00:00:01",9));
    ASSERT_NEAR(90.0 + 1/3600.0, p.DECm(), 1/3600.0);
    ASSERT_FALSE(strncmp("+00:00:01",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+00:01:00",9));
    ASSERT_NEAR(90.0 + 1/60.0, p.DECm(), 1/3600.0);
    ASSERT_FALSE(strncmp("+00:01:00",p.toStringDEC(b,64),64));

    // Negative tests
    ASSERT_TRUE(p.parseStringDEC("+J0:00:00",9));
    ASSERT_TRUE(p.parseStringDEC("-J0:00:00",9));
}

TEST(EQ500XDriverTest, test_EastSideOfPier_DEC_Conversions)
{
    EQ500X::MechanicalPoint p;
    char b[64]= {0};

    // Positive tests
    ASSERT_EQ(INDI::Telescope::PIER_EAST, p.setPierSide(INDI::Telescope::PIER_EAST));
    ASSERT_FALSE(p.parseStringDEC("-I5:00:00",9));
    ASSERT_EQ(+345.0, p.DECm());
    ASSERT_FALSE(strncmp("-I5:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("-F5:00:00",9));
    ASSERT_EQ(+315.0, p.DECm());
    ASSERT_FALSE(strncmp("-F5:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("-B0:00:00",9));
    ASSERT_EQ(+270.0, p.DECm());
    ASSERT_FALSE(strncmp("-B0:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("-=5:00:00",9));
    ASSERT_EQ(+225.0, p.DECm());
    ASSERT_FALSE(strncmp("-=5:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("-90:00:00",9));
    ASSERT_EQ(+180.0, p.DECm());
    ASSERT_FALSE(strncmp("-90:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("-45:00:00",9));
    ASSERT_EQ(+135.0, p.DECm());
    ASSERT_FALSE(strncmp("-45:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+00:00:00",9));
    ASSERT_EQ( +90.0, p.DECm());
    ASSERT_FALSE(strncmp("+00:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+45:00:00",9));
    ASSERT_EQ( +45.0, p.DECm());
    ASSERT_FALSE(strncmp("+45:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+90:00:00",9));
    ASSERT_EQ( +0.0, p.DECm());
    ASSERT_FALSE(strncmp("+90:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+=5:00:00",9));
    ASSERT_EQ( -45.0, p.DECm());
    ASSERT_FALSE(strncmp("+=5:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+B0:00:00",9));
    ASSERT_EQ( -90.0, p.DECm());
    ASSERT_FALSE(strncmp("+B0:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+F5:00:00",9));
    ASSERT_EQ(-135.0, p.DECm());
    ASSERT_FALSE(strncmp("+F5:00:00",p.toStringDEC(b,64),64));
    ASSERT_FALSE(p.parseStringDEC("+I5:00:00",9));
    ASSERT_EQ(-165.0, p.DECm());
    ASSERT_FALSE(strncmp("+I5:00:00",p.toStringDEC(b,64),64));

    // Negative tests
    ASSERT_TRUE(p.parseStringDEC("+J0:00:00",9));
    ASSERT_TRUE(p.parseStringDEC("-J0:00:00",9));
}

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
            INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
