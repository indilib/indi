#include <gtest/gtest.h>
#include <gmock/gmock.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include <cstring>

#include <indilogger.h>
#include <indidevapi.h>
#include <defaultdevice.h>
#include <scopesim_helper.h>
#include <telescope_simulator.h>

using ::testing::_;
using ::testing::StrEq;

// Angle tests
TEST(AngleTest, CreateAngle)
{

	Angle a = Angle(60, Angle::DEGREES);
	EXPECT_EQ(a.Degrees(), 60);
    EXPECT_EQ(a.Hours(), 4);
    EXPECT_EQ(a.radians(), 60. * M_PI/180.);

    a = Angle(1.0, Angle::RADIANS);
    EXPECT_EQ(a.radians(), 1.0);
	
	a = Angle(2.0, Angle::HOURS);
    EXPECT_EQ(a.Hours(), 2.0);
    EXPECT_EQ(a.Degrees(), 30.0);

    EXPECT_EQ(Angle(180).Degrees(), 180);
    EXPECT_EQ(Angle(-180).Degrees(), 180);
    EXPECT_EQ(Angle(180).Hours(), 12);
    EXPECT_EQ(Angle(-180).HoursHa(), 12);
    EXPECT_EQ(Angle(360).Degrees360(), 0);
    EXPECT_EQ(Angle(-360).Degrees360(), 0);
    EXPECT_EQ(Angle(720).Hours(), 0);
    EXPECT_EQ(Angle(360).HoursHa(), 0);
    EXPECT_EQ(Angle(-345).Hours(), 1);
    EXPECT_EQ(Angle(-345).HoursHa(), 1);
    EXPECT_EQ(Angle(345).Hours(), 23);
    EXPECT_EQ(Angle(345).HoursHa(), -1);
}

TEST(AngleTest, Logic)
{
    Angle a = -60;
    EXPECT_EQ(a.HoursHa(), -4);

    EXPECT_TRUE(a == Angle(-60));
    EXPECT_FALSE(a == Angle(60));
    EXPECT_TRUE(a != Angle(-61));
    EXPECT_FALSE(a != Angle(-60));
    EXPECT_TRUE(a < Angle(119));
    EXPECT_TRUE(a > Angle(121));
    EXPECT_TRUE(Angle(350) < Angle(10));
    EXPECT_FALSE(Angle(351) > Angle(11));
}

TEST(AngleTest, Arithmetic)
{
    Angle a = -60;
    Angle b = 60;
    EXPECT_EQ((a + b).Degrees(), 0);
    EXPECT_EQ((a - b).Degrees(), -120);
    EXPECT_EQ((b - a).Degrees(), 120);

    a += 10;
    EXPECT_EQ(a.Degrees(), -50);
    a += Angle(10);
    EXPECT_EQ(a.Degrees(), -40);

    EXPECT_EQ((b * 0.5).Degrees(), 30);
    EXPECT_EQ(-a.Degrees(), 40);

}

// Vector tests

TEST(VectorTest, Constructors)
{
    Vector v = Vector();
    EXPECT_EQ(v.l(), 0);
    EXPECT_EQ(v.m(), 0);
    EXPECT_EQ(v.n(), 0);

    v = Vector(2, 3, 6);
    EXPECT_EQ(v.l(), 2./7);
    EXPECT_EQ(v.m(), 3./7);
    EXPECT_EQ(v.n(), 6./7);

    v = Vector(Angle(90), Angle(45));
    EXPECT_NEAR(v.l(), 0, 0.00001);
    EXPECT_NEAR(v.m(), 0.707107, 0.00001);
    EXPECT_NEAR(v.n(), 0.707107, 0.00001);
}

TEST(VectorTest, PriSec)
{
    Vector v = Vector(Angle(90), Angle(45));
    EXPECT_NEAR(v.primary().Degrees(), 90, 0.00001);
    EXPECT_NEAR(v.secondary().Degrees(), 45, 0.00001);
    EXPECT_EQ(v.lengthSquared(), 1.0);
}

TEST(VectorTest, length)
{
    Vector v(1,4,8);
    EXPECT_EQ(v.lengthSquared(), 1);
    EXPECT_EQ(v.length(), 1);
    v.normalise();
    EXPECT_DOUBLE_EQ(v.length(), 1);
    EXPECT_EQ(v.l(), 1./9.);
    EXPECT_EQ(v.m(), 4./9.);
    EXPECT_EQ(v.n(), 8./9.);
}

TEST(VectorTest, rotateX)
{
    Vector v = Vector(Angle(90), Angle(45));
    Vector vr = v.rotateX(Angle(45));
    EXPECT_NEAR(vr.primary().Degrees(), 90, 0.00001);
    EXPECT_NEAR(vr.secondary().Degrees(), 0, 0.00001);
    v = vr.rotateX(Angle(-45));
    EXPECT_NEAR(v.primary().Degrees(), 90, 0.00001);
    EXPECT_NEAR(v.secondary().Degrees(), 45, 0.00001);
}

TEST(VectorTest, rotateY)
{
    Vector v = Vector(Angle(90), Angle(45));
    Vector vr = v.rotateY(Angle(45));
    EXPECT_NEAR(vr.primary().Degrees(), 125.26439, 0.00001);
    EXPECT_NEAR(vr.secondary().Degrees(), 30, 0.00001);
    v = vr.rotateY(Angle(-45));
    EXPECT_NEAR(v.primary().Degrees(), 90, 0.00001);
    EXPECT_NEAR(v.secondary().Degrees(), 45, 0.00001);
}

TEST(VectorTest, rotateZ)
{
    Vector v = Vector(Angle(90), Angle(45));
    Vector vr = v.rotateZ(Angle(45));
    EXPECT_NEAR(vr.primary().Degrees(), 45, 0.00001);
    EXPECT_NEAR(vr.secondary().Degrees(), 45, 0.00001);
    v = vr.rotateZ(Angle(-45));
    EXPECT_NEAR(v.primary().Degrees(), 90, 0.00001);
    EXPECT_NEAR(v.secondary().Degrees(), 45, 0.00001);
}

// Alignment tests
// the tuple contains:
// Test Ha, Ra, primary, azimuth angle
// Test Dec, altitude, secondary angle
// Expected Ha, Ra, primary, azimuth angle
// Expected Dec, altitude, secondary angle

class AlignmentTest :public ::testing::Test
{
protected:
    Alignment alignment;
    AlignmentTest()
    {
        alignment.latitude = Angle(51.6);
        alignment.longitude = Angle(-0.73);
    }
};

TEST_F(AlignmentTest, Create)
{
    EXPECT_EQ(alignment.latitude.Degrees(), 51.6);
    EXPECT_EQ(alignment.longitude.Degrees(), -0.73);

    EXPECT_EQ(alignment.mountType, Alignment::MOUNT_TYPE::EQ_FORK);
}

TEST_F(AlignmentTest, Errors)
{
    EXPECT_EQ(alignment.ih(), 0);
    EXPECT_EQ(alignment.id(), 0);
    EXPECT_EQ(alignment.np(), 0);
    EXPECT_EQ(alignment.ch(), 0);
    EXPECT_EQ(alignment.ma(), 0);
    EXPECT_EQ(alignment.me(), 0);
    alignment.setCorrections(1,2,3,4,5,6);
    EXPECT_EQ(alignment.ih(), 1);
    EXPECT_EQ(alignment.id(), 2);
    EXPECT_EQ(alignment.np(), 4);
    EXPECT_EQ(alignment.ch(), 3);
    EXPECT_EQ(alignment.ma(), 5);
    EXPECT_EQ(alignment.me(), 6);
}

TEST_F(AlignmentTest, instrumentToObservedME1)
{
    Angle oHa, oDec;

    alignment.setCorrections(0,0,0,0,0,1);      // ME 1

    // looking NS
    alignment.instrumentToObserved(Angle(0), Angle(0), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 0);
    EXPECT_EQ(oDec.Degrees(), 1);

    // looking EW
    alignment.instrumentToObserved(Angle(90), Angle(0), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 90);
    EXPECT_NEAR(oDec.Degrees(), 0, 1e10);

    // on meridian, dec 80
    alignment.instrumentToObserved(Angle(0), Angle(80), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 0);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 81);

    // looking at pole
    alignment.instrumentToObserved(Angle(0), Angle(90), &oHa, &oDec);
    EXPECT_FLOAT_EQ(oHa.HoursHa(), 12);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 89);

    // 89 dec, expect move to pole
    alignment.instrumentToObserved(Angle(0), Angle(89), &oHa, &oDec);
    EXPECT_FLOAT_EQ(oHa.HoursHa(), 12);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 90);

    // 1 deg W of pole
    alignment.instrumentToObserved(Angle(90), Angle(89), &oHa, &oDec);
    EXPECT_NEAR(oHa.HoursHa(), 8.9997, 0.0001);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 88.5858);
}

TEST_F(AlignmentTest, observedToInstrumentME1)
{
    Angle oHa, oDec;

    alignment.setCorrections(0,0,0,0,0,1);      // ME 1

    // looking NS
    alignment.observedToInstrument(Angle(0), Angle(0), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 0);
    EXPECT_EQ(oDec.Degrees(), -1);

    // looking EW
    alignment.instrumentToObserved(Angle(-90), Angle(0), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), -90);
    EXPECT_NEAR(oDec.Degrees(), 0, 1e10);

    // on meridian, dec 80
    alignment.observedToInstrument(Angle(0), Angle(80), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 0);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 79);

    // looking at pole
    alignment.observedToInstrument(Angle(90), Angle(90), &oHa, &oDec);
    EXPECT_NEAR(oHa.HoursHa(), 0, 0.0001);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 89);

    // 89 dec, expect move to pole
    alignment.observedToInstrument(Angle(180), Angle(89), &oHa, &oDec);
    //EXPECT_FLOAT_EQ(oHa.HoursHa(), 0);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 90);

    // 1 deg E of pole
    alignment.observedToInstrument(Angle(-90), Angle(89), &oHa, &oDec);
    EXPECT_NEAR(oHa.HoursHa(), -3.0003, 0.00001);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 88.5858);
}


TEST_F(AlignmentTest, observedToInstrumentMEn1)
{
    Angle oHa, oDec;

    alignment.setCorrections(0,0,0,0,0,-1);      // ME -1

    // looking NS
    alignment.observedToInstrument(Angle(0), Angle(0), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 0);
    EXPECT_EQ(oDec.Degrees(), 1);

    // looking EW
    alignment.instrumentToObserved(Angle(-90), Angle(0), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), -90);
    EXPECT_NEAR(oDec.Degrees(), 0, 1e10);

    // on meridian, dec 80
    alignment.observedToInstrument(Angle(0), Angle(80), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 0);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 81);

    // looking at pole
    alignment.observedToInstrument(Angle(90), Angle(90), &oHa, &oDec);
    EXPECT_NEAR(oHa.HoursHa(), 12, 0.0001);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 89);

    // 89 dec, expect move to pole
    alignment.observedToInstrument(Angle(0), Angle(89), &oHa, &oDec);
    //EXPECT_FLOAT_EQ(oHa.HoursHa(), 0);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 90);

    // 1 deg E of pole
    alignment.observedToInstrument(Angle(-90), Angle(89), &oHa, &oDec);
    EXPECT_NEAR(oHa.HoursHa(), -8.9997, 0.00001);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 88.5858);
}

TEST_F(AlignmentTest, instrumentToObservedMA1)
{
    Angle oHa, oDec;

    alignment.setCorrections(0,0,0,0,1,0);      // MA 1

    // looking NS
    alignment.instrumentToObserved(Angle(0), Angle(0), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 0);
    EXPECT_EQ(oDec.Degrees(), 0);

    // looking WE
    alignment.instrumentToObserved(Angle(-90), Angle(0), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), -90);
    EXPECT_EQ(oDec.Degrees(), 1);

    // W, dec 80
    alignment.instrumentToObserved(Angle(90), Angle(80), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 90);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 79);

    // looking at pole
    alignment.instrumentToObserved(Angle(0), Angle(90), &oHa, &oDec);
    EXPECT_FLOAT_EQ(oHa.HoursHa(), 6);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 89);

    // 89 dec, expect move to pole
    alignment.instrumentToObserved(Angle(-90), Angle(89), &oHa, &oDec);
    //EXPECT_FLOAT_EQ(oHa.HoursHa(), -6);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 90);

    // 1 deg N of pole
    alignment.instrumentToObserved(Angle(180), Angle(89), &oHa, &oDec);
    EXPECT_NEAR(oHa.HoursHa(), 9.0003, 0.0001);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 88.5858);
}


TEST_F(AlignmentTest, instrumentToObservedMAm1)
{
    Angle oHa, oDec;

    alignment.setCorrections(0,0,0,0,-1,0);      // MA -1

    // looking NS
    alignment.instrumentToObserved(Angle(0), Angle(0), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 0);
    EXPECT_EQ(oDec.Degrees(), 0);

    // looking WE
    alignment.instrumentToObserved(Angle(-90), Angle(0), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), -90);
    EXPECT_EQ(oDec.Degrees(), -1);

    // W, dec 80
    alignment.instrumentToObserved(Angle(90), Angle(80), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 90);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 81);

    // looking at pole
    alignment.instrumentToObserved(Angle(0), Angle(90), &oHa, &oDec);
    EXPECT_FLOAT_EQ(oHa.HoursHa(), -6);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 89);

    // 89 dec, expect move to pole
    alignment.instrumentToObserved(Angle(90), Angle(89), &oHa, &oDec);
    //EXPECT_FLOAT_EQ(oHa.HoursHa(), -6);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 90);

    // 1 deg S of pole
    alignment.instrumentToObserved(Angle(0), Angle(89), &oHa, &oDec);
    EXPECT_NEAR(oHa.HoursHa(), -2.9997, 0.0001);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 88.5858);
}

TEST_F(AlignmentTest, observedToInstrumentMA1)
{
    Angle iHa, iDec;

    alignment.setCorrections(0,0,0,0,1,0);      // MA 1

    // looking NS
    alignment.observedToInstrument(Angle(0), Angle(0), &iHa, &iDec);
    EXPECT_EQ(iHa.Degrees(), 0);
    EXPECT_EQ(iDec.Degrees(), 0);

    // looking EW
    alignment.observedToInstrument(Angle(90), Angle(0), &iHa, &iDec);
    EXPECT_EQ(iHa.Degrees(), 90);
    EXPECT_EQ(iDec.Degrees(), 1);

    // E, dec 80
    alignment.observedToInstrument(Angle(-90), Angle(80), &iHa, &iDec);
    EXPECT_EQ(iHa.Degrees(), -90);
    EXPECT_FLOAT_EQ(iDec.Degrees(), 79);

    // looking at pole
    alignment.observedToInstrument(Angle(90), Angle(90), &iHa, &iDec);
    EXPECT_NEAR(iHa.HoursHa(), -6, 0.0001);
    EXPECT_FLOAT_EQ(iDec.Degrees(), 89);

    // 1 deg S of pole
    alignment.observedToInstrument(Angle(0), Angle(89), &iHa, &iDec);
    EXPECT_NEAR(iHa.HoursHa(), -2.9997, 0.00001);
    EXPECT_FLOAT_EQ(iDec.Degrees(), 88.5858);
}


TEST_F(AlignmentTest, instrumentToObservedCH1)
{
    Angle oHa, oDec;

    alignment.setCorrections(0, 0, 1, 0, 0, 0);      // CH 1

    // looking NS
    alignment.instrumentToObserved(Angle(0), Angle(0), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 1);
    EXPECT_EQ(oDec.Degrees(), 0);

    // looking WE
    alignment.instrumentToObserved(Angle(-90), Angle(0), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), -89);
    EXPECT_EQ(oDec.Degrees(), 0);

    // W, dec 60
    alignment.instrumentToObserved(Angle(90), Angle(60), &oHa, &oDec);
    EXPECT_EQ(oHa.Degrees(), 92);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 60);

    // looking at pole
    alignment.instrumentToObserved(Angle(0), Angle(90), &oHa, &oDec);
    //EXPECT_FLOAT_EQ(oHa.HoursHa(), 6);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 90);

    // 89 dec
    alignment.instrumentToObserved(Angle(-90), Angle(89), &oHa, &oDec);
    EXPECT_NEAR(oHa.HoursHa(), -2.180087, 0.0001);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 89);

    // 1 deg N of pole
    alignment.instrumentToObserved(Angle(180), Angle(89), &oHa, &oDec);
    EXPECT_NEAR(oHa.HoursHa(), -8.180087, 0.0001);
    EXPECT_FLOAT_EQ(oDec.Degrees(), 89);
}

TEST_F(AlignmentTest, observedToInstrumentCH1)
{
    Angle iHa, iDec;

    alignment.setCorrections(0, 0, 1, 0, 0, 0);      // CH 1

    // looking NS
    alignment.observedToInstrument(Angle(0), Angle(0), &iHa, &iDec);
    EXPECT_EQ(iHa.Degrees(), -1);
    EXPECT_EQ(iDec.Degrees(), 0);

    // looking EW
    alignment.observedToInstrument(Angle(90), Angle(0), &iHa, &iDec);
    EXPECT_EQ(iHa.Degrees(), 89);
    EXPECT_EQ(iDec.Degrees(), 0);

    // E, dec 60
    alignment.observedToInstrument(Angle(-90), Angle(60), &iHa, &iDec);
    EXPECT_EQ(iHa.Degrees(), -92);
    EXPECT_FLOAT_EQ(iDec.Degrees(), 60);

    // looking at pole
    alignment.observedToInstrument(Angle(90), Angle(90), &iHa, &iDec);
    //EXPECT_NEAR(iHa.HoursHa(), -6, 0.0001);
    EXPECT_FLOAT_EQ(iDec.Degrees(), 90);

    // 1 deg S of pole
    alignment.observedToInstrument(Angle(0), Angle(89), &iHa, &iDec);
    EXPECT_NEAR(iHa.HoursHa(), -3.81991, 0.00001);
    EXPECT_FLOAT_EQ(iDec.Degrees(), 89);
}

TEST_F(AlignmentTest, roundTripMAME1)
{
    Angle oHa, oDec;
    Angle iHa, iDec;

    alignment.setCorrections(0,0,0,0,1,1);      // MA 1, ME 1

    // looking NS
    alignment.instrumentToObserved(Angle(0), Angle(0), &oHa, &oDec);
    alignment.observedToInstrument(oHa, oDec, &iHa, &iDec);
    EXPECT_NEAR(iHa.Degrees(), 0, 0.00001);
    EXPECT_NEAR(iDec.Degrees(), 0, 0.00001);

    // looking EW
    alignment.instrumentToObserved(Angle(90), Angle(0), &oHa, &oDec);
    alignment.observedToInstrument(oHa, oDec, &iHa, &iDec);
    EXPECT_NEAR(iHa.Degrees(), 90, 0.00001);
    EXPECT_NEAR(iDec.Degrees(), 0, 0.00001);

    // on meridian, dec 80
    alignment.instrumentToObserved(Angle(0), Angle(80), &oHa, &oDec);
    alignment.observedToInstrument(oHa, oDec, &iHa, &iDec);
    EXPECT_NEAR(iHa.Degrees(), 0, 0.00001);
    EXPECT_NEAR(iDec.Degrees(), 80, 0.00001);

    // E, dec 80
    alignment.instrumentToObserved(Angle(-90), Angle(80), &oHa, &oDec);
    alignment.observedToInstrument(oHa, oDec, &iHa, &iDec);
    EXPECT_NEAR(iHa.Degrees(), -90, 0.00001);
    EXPECT_NEAR(iDec.Degrees(), 80, 0.00001);

    // looking at pole
    alignment.instrumentToObserved(Angle(0), Angle(90), &oHa, &oDec);
    alignment.observedToInstrument(oHa, oDec, &iHa, &iDec);
    //EXPECT_NEAR(iHa.Degrees(), 0, 0.0001);
    EXPECT_NEAR(iDec.Degrees(), 90, 0.00001);

    // 89 dec, expect move to pole
    alignment.instrumentToObserved(Angle(0), Angle(89), &oHa, &oDec);
    alignment.observedToInstrument(oHa, oDec, &iHa, &iDec);
    EXPECT_NEAR(iHa.Degrees(), 0, 0.00001);
    EXPECT_NEAR(iDec.Degrees(), 89, 0.00001);

    // 1 deg W of pole
    alignment.instrumentToObserved(Angle(90), Angle(89), &oHa, &oDec);
    alignment.observedToInstrument(oHa, oDec, &iHa, &iDec);
    EXPECT_NEAR(iHa.Degrees(), 90, 0.00001);
    EXPECT_NEAR(iDec.Degrees(), 89, 0.00001);

    // 1 deg S of pole
    alignment.instrumentToObserved(Angle(0), Angle(89), &oHa, &oDec);
    alignment.observedToInstrument(oHa, oDec, &iHa, &iDec);
    EXPECT_NEAR(iHa.Degrees(), 0, 0.00001);
    EXPECT_NEAR(iDec.Degrees(), 89, 0.00001);
}

/*
class AlignmentParametersTest :public ::testing::TestWithParam<std::tuple<double, double, double, double>>
{
protected:
    Alignment alignment;
    AlignmentParametersTest()
    {
        alignment.latitude = Angle(51.6);
        alignment.longitude = Angle(-0.73);
    }
};

INSTANTIATE_TEST_SUITE_P
(
    ZeroErrors,
    AlignmentParametersTest,
    ::testing::Values
    (
            // test Ha, Dec, expected Ha, Dec - all in degrees
            std::make_tuple(0, 0, 0, 0),
            std::make_tuple(0, 30, 0, 30),
            std::make_tuple(-90, 0, -90, 0),
            std::make_tuple(90, 30, 90, 30),
            std::make_tuple(-179, 60, -179, 60),
            std::make_tuple(-180, 90, 180, 90)
    )
);

TEST_P(AlignmentParametersTest, observed2Instrument)
{
    Angle iHa, iDec;

    double testHa = std::get<0>(GetParam());
    double testDec = std::get<1>(GetParam());
    double expectedHa = std::get<2>(GetParam());
    double expectedDec = std::get<3>(GetParam());

    alignment.observedToInstrument(Angle(testHa), Angle(testDec), &iHa, &iDec);
    EXPECT_EQ(iHa.Degrees(), expectedHa);
    EXPECT_EQ(iDec.Degrees(), expectedDec);
}

TEST_P(AlignmentParametersTest, InstrumentToObserved)
{
    Angle iHa, iDec;

    double testHa = std::get<0>(GetParam());
    double testDec = std::get<1>(GetParam());
    double expectedHa = std::get<2>(GetParam());
    double expectedDec = std::get<3>(GetParam());

    alignment.instrumentToObserved(Angle(testHa), Angle(testDec), &iHa, &iDec);
    EXPECT_FLOAT_EQ(iHa.Degrees(), expectedHa);
    EXPECT_FLOAT_EQ(iDec.Degrees(), expectedDec);
}

INSTANTIATE_TEST_SUITE_P
(
    TestME1,
    AlignmentParametersTest,
    ::testing::Values
    (
            // test Ha, Dec, expected Ha, Dec - all in degrees
            std::make_tuple(0, 0, 0, 0),
            std::make_tuple(0, 30, 0, 30),
            std::make_tuple(-90, 0, -90, 0),
            std::make_tuple(90, 30, 90, 30),
            std::make_tuple(-179, 60, -179, 60),
            std::make_tuple(-180, 90, 180, 90)
    )
);
*/

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
            INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}


