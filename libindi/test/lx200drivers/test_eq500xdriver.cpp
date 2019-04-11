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


class MockEQ500XDriver : public EQ500X {
    public:
};


// Right ascension is normal sexagesimal mapping.
//
// NotFlipped  (LST = -6)   Flipped
// S -12.0h <-> 12:00:00 <-> -24.0h
// E -06.0h <-> 18:00:00 <-> -18.0h
// N +00.0h <-> 00:00:00 <-> -12.0h
// W +06.0h <-> 06:00:00 <-> -06.0h
// S +12.0h <-> 12:00:00 <-> +00.0h
// E +18.0h <-> 18:00:00 <-> +06.0h
// N +24.0h <-> 00:00:00 <-> +12.0h

TEST(EQ500XDriverTest, test_Unflipped_RA_Conversions)
{
    EQ500X::MechanicalPoint p;
    char b[64]= {0};

    p.setLST(+6.0);

    ASSERT_EQ(false, p.setFlipped(false));
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

TEST(EQ500XDriverTest, test_Flipped_RA_Conversions)
{
    EQ500X::MechanicalPoint p;
    char b[64]= {0};

    p.setLST(+6.0);

    ASSERT_EQ(true, p.setFlipped(true));
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
// NotFlipped                         Flipped
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

TEST(EQ500XDriverTest, test_Unflipped_DEC_Conversions)
{
    EQ500X::MechanicalPoint p;
    char b[64]= {0};

    ASSERT_EQ(false, p.setFlipped(false));
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

TEST(EQ500XDriverTest, test_Flipped_DEC_Conversions)
{
    EQ500X::MechanicalPoint p;
    char b[64]= {0};

    // Positive tests
    ASSERT_EQ(true, p.setFlipped(true));
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
