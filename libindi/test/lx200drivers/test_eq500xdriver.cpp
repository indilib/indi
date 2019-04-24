#include <stdarg.h>
#include <cmath>
#include <memory>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <cassert>

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

#include "lx200generic.h"
#include "eq500x.h"

#include "indicom.h"
#include "indilogger.h"
#include "lx200driver.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using ::testing::_;
using ::testing::StrEq;

char * me = "MockEQ500XDriver";
class MockEQ500XDriver : public EQ500X
{
public:
    MockEQ500XDriver() : EQ500X()
    {
        resetSimulation();
        ISGetProperties("");
        setSimulation(true);
        setDebug(true);
        char * names[] = {"DBG_DEBUG"};
        ISState states[] = {ISS_ON};
        ISNewSwitch(getDeviceName(),"DEBUG_LEVEL",states,names,1);
        if (checkConnection())
            setConnected(true);
    }
public:
    // Default LST for this driver is 6 - RA is east when starting up
    double LST { 6 };
    double getLST() { return LST; }
    bool getMechanicalPosition(MechanicalPoint &p) { return getCurrentPosition(p); }
    TelescopeStatus getTrackState() const { return TrackState; }
    long getReadScopeStatusInterval() const { return POLLMS; }
public:
    void setLongitude(double lng) {
        /* Say it's 0h on Greenwich meridian (GHA=0) - express LST as hours */
        LST = 0.0 + lng/15.0;
        updateLocation(0,lng,0);
    }
    bool executeReadScopeStatus() { return ReadScopeStatus(); }
    bool executeGotoOffset(double ra_offset, double dec_offset) { return Goto(currentRA+ra_offset,currentDEC+dec_offset); }
    bool executeAbort() { return Abort(); }
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
    ASSERT_FALSE(d.getMechanicalPosition(p));
    ASSERT_DOUBLE_EQ(  +0.0, p.RAm());
    ASSERT_DOUBLE_EQ( +90.0, p.DECm());
    // Assign a new longitude
    d.setLongitude(5*15);
    ASSERT_FALSE(d.getMechanicalPosition(p));
    ASSERT_DOUBLE_EQ(  23.0, p.RAm());
    ASSERT_DOUBLE_EQ( +90.0, p.DECm());
    // Assign a new longitude - but this time the mount is not considered "parked" east/pole and does not sync
    d.setLongitude(7*15);
    ASSERT_FALSE(d.getMechanicalPosition(p));
    ASSERT_DOUBLE_EQ(  23.0, p.RAm());  // Expected 1h - not possible to assign longitude without restarting the mount
    ASSERT_DOUBLE_EQ( +90.0, p.DECm());
}

TEST(EQ500XDriverTest, test_Goto_NoMovement)
{
    MockEQ500XDriver d;
    struct timespec timeout = {0,100000000L};

    ASSERT_TRUE(d.isConnected());
    ASSERT_TRUE(d.executeReadScopeStatus());
    ASSERT_TRUE(d.executeGotoOffset(0,0));
    ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    for(int i = 0; i < 10; i++)
    {
        nanosleep(&timeout, nullptr);
        ASSERT_TRUE(d.executeReadScopeStatus());
        if (EQ500X::SCOPE_TRACKING == d.getTrackState()) break;
        ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    }
    ASSERT_EQ(EQ500X::SCOPE_TRACKING, d.getTrackState());
}

TEST(EQ500XDriverTest, test_Goto_AbortMovement)
{
    MockEQ500XDriver d;

    ASSERT_TRUE(d.isConnected());
    ASSERT_TRUE(d.executeReadScopeStatus());
    ASSERT_TRUE(d.executeGotoOffset(-1,-10));
    ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    for(int i = 0; i < 10; i++)
    {
        long seconds = d.getReadScopeStatusInterval()/1000;
        struct timespec timeout = {seconds, (d.getReadScopeStatusInterval()-seconds*1000)*1000000L};
        nanosleep(&timeout, nullptr);
        ASSERT_TRUE(d.executeReadScopeStatus());
        ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    }
    ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    ASSERT_TRUE(d.executeAbort());
    ASSERT_EQ(EQ500X::SCOPE_TRACKING, d.getTrackState());
    ASSERT_EQ(1000, d.getReadScopeStatusInterval());
}

TEST(EQ500XDriverTest, test_Goto_SouthMovement)
{
    MockEQ500XDriver d;

    ASSERT_TRUE(d.isConnected());
    ASSERT_TRUE(d.executeReadScopeStatus());
    ASSERT_TRUE(d.executeGotoOffset(0,-10));
    ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    for(int i = 0; i < 100; i++)
    {
        long seconds = d.getReadScopeStatusInterval()/1000;
        struct timespec timeout = {seconds, (d.getReadScopeStatusInterval()-seconds*1000)*1000000L};
        nanosleep(&timeout, nullptr);
        ASSERT_TRUE(d.executeReadScopeStatus());
        if (EQ500X::SCOPE_TRACKING == d.getTrackState()) break;
        ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    }
    ASSERT_EQ(EQ500X::SCOPE_TRACKING, d.getTrackState());
}

TEST(EQ500XDriverTest, test_Goto_NorthMovement)
{
    MockEQ500XDriver d;

    ASSERT_TRUE(d.isConnected());
    ASSERT_TRUE(d.executeReadScopeStatus());
    ASSERT_TRUE(d.executeGotoOffset(0,+10));
    ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    for(int i = 0; i < 100; i++)
    {
        long seconds = d.getReadScopeStatusInterval()/1000;
        struct timespec timeout = {seconds, (d.getReadScopeStatusInterval()-seconds*1000)*1000000L};
        nanosleep(&timeout, nullptr);
        ASSERT_TRUE(d.executeReadScopeStatus());
        if (EQ500X::SCOPE_TRACKING == d.getTrackState()) break;
        ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    }
    ASSERT_EQ(EQ500X::SCOPE_TRACKING, d.getTrackState());
}

TEST(EQ500XDriverTest, test_Goto_EastMovement)
{
    MockEQ500XDriver d;

    ASSERT_TRUE(d.isConnected());
    ASSERT_TRUE(d.executeReadScopeStatus());
    ASSERT_TRUE(d.executeGotoOffset(+1,0));
    ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    for(int i = 0; i < 100; i++)
    {
        long seconds = d.getReadScopeStatusInterval()/1000;
        struct timespec timeout = {seconds, (d.getReadScopeStatusInterval()-seconds*1000)*1000000L};
        nanosleep(&timeout, nullptr);
        ASSERT_TRUE(d.executeReadScopeStatus());
        if (EQ500X::SCOPE_TRACKING == d.getTrackState()) break;
        ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    }
    ASSERT_EQ(EQ500X::SCOPE_TRACKING, d.getTrackState());
}

TEST(EQ500XDriverTest, test_Goto_WestMovement)
{
    MockEQ500XDriver d;

    ASSERT_TRUE(d.isConnected());
    ASSERT_TRUE(d.executeReadScopeStatus());
    ASSERT_TRUE(d.executeGotoOffset(-1,0));
    ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    for(int i = 0; i < 100; i++)
    {
        long seconds = d.getReadScopeStatusInterval()/1000;
        struct timespec timeout = {seconds, (d.getReadScopeStatusInterval()-seconds*1000)*1000000L};
        nanosleep(&timeout, nullptr);
        ASSERT_TRUE(d.executeReadScopeStatus());
        if (EQ500X::SCOPE_TRACKING == d.getTrackState()) break;
        ASSERT_EQ(EQ500X::SCOPE_SLEWING, d.getTrackState());
    }
    ASSERT_EQ(EQ500X::SCOPE_TRACKING, d.getTrackState());
}

TEST(EQ500XDriverTest, test_MechanicalPointEquality)
{
    EQ500X::MechanicalPoint p, q;

    p.RAm(1.23456789);
    p.DECm(1.23456789);
    p.setPierSide(EQ500X::PIER_EAST);
    q.RAm(1.23456789);
    q.DECm(1.23456789);
    q.setPierSide(EQ500X::PIER_EAST);
    ASSERT_TRUE(p == q);
    ASSERT_FALSE(p != q);
    q.setPierSide(EQ500X::PIER_WEST);
    ASSERT_FALSE(p == q);
    ASSERT_TRUE(p != q);
    q.setPierSide(EQ500X::PIER_EAST);
    q.RAm(q.RAm()+15.0/3600.0);
    ASSERT_FALSE(p == q);
    ASSERT_TRUE(p != q);
    q.RAm(q.RAm()-15.0/3600.0);
    ASSERT_TRUE(p == q);
    ASSERT_FALSE(p != q);
    q.DECm(q.DECm()+1.0/3600.0);
    ASSERT_FALSE(p == q);
    ASSERT_TRUE(p != q);
    q.DECm(q.DECm()-1.0/3600.0);
    ASSERT_TRUE(p == q);
    ASSERT_FALSE(p != q);
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

TEST(EQ500XDriverTest, test_Stability_RA_Conversions)
{
    EQ500X::TelescopePierSide const sides[] = {EQ500X::PIER_EAST, EQ500X::PIER_WEST};
    for (size_t ps = 0; ps < sizeof(sides); ps++)
    {
        for (int s = 0; s < 60; s++)
        {
            for (int m = 0; m < 60; m++)
            {
                for (int h = 0; h < 24; h++)
                {
                    // Locals are on purpose - reset test material on each loop
                    EQ500X::MechanicalPoint p;
                    char b[64] = {0}, c[64] = {0};

                    p.setPierSide(sides[ps]);

                    snprintf(b, sizeof(b), "%02d:%02d:%02d", h, m, s);
                    p.parseStringRA(b, sizeof(b));
                    p.toStringRA(c, sizeof(c));

                    ASSERT_FALSE(strncmp(b,c,sizeof(b)));
                }
            }
        }
    }
}

TEST(EQ500XDriverTest, test_Stability_DEC_Conversions)
{
    EQ500X::TelescopePierSide const sides[] = {EQ500X::PIER_EAST, EQ500X::PIER_WEST};
    for (size_t ps = 0; ps < sizeof(sides); ps++)
    {
        for (int s = 0; s < 60; s++)
        {
            for (int m = 0; m < 60; m++)
            {
                for (int d = -90; d < 91; d++)
                {
                    // Locals are on purpose - reset test material on each loop
                    EQ500X::MechanicalPoint p;
                    char b[64] = {0}, c[64] = {0};

                    p.setPierSide(sides[ps]);

                    snprintf(b, sizeof(b), "%+03d:%02d:%02d", d, m, s);
                    p.parseStringDEC(b, sizeof(b));
                    p.toStringDEC(c, sizeof(c));

                    // Debug test with this block
                    if (strncmp(b,c,sizeof(b)))
                    {
                        p.parseStringDEC(b, sizeof(b));
                        p.toStringDEC(c, sizeof(c));
                    }

                    ASSERT_FALSE(strncmp(b,c,sizeof(b)));
                }
            }
        }
    }
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
