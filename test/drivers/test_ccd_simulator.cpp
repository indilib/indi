#include "indicom.h"
#include "indilogger.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using ::testing::_;
using ::testing::StrEq;

#include "ccd_simulator.h"

char _me[] = "MockCCDSimDriver";
char *me = _me;
class MockCCDSimDriver: public CCDSim
{
public:
    MockCCDSimDriver(): CCDSim()
    {
        initProperties();
        ISGetProperties(me);
    }

    void testProperties()
    {
        INumberVectorProperty * const p = getNumber("SIMULATOR_SETTINGS");
        ASSERT_NE(p, nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_XRES"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_YRES"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_XSIZE"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_YSIZE"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_MAXVAL"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_BIAS"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_SATURATION"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_LIMITINGMAG"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_NOISE"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_SKYGLOW"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_OAGOFFSET"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_POLAR"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_POLARDRIFT"), nullptr);
        ASSERT_NE(IUFindNumber(p, "SIM_ROTATION"), nullptr);
    }

    void testGuideAPI()
    {
        // At init, current RA and DEC are undefined - message will not appear because the test passes
        EXPECT_TRUE(isnan(currentRA)) << "Field 'currentRA' is undefined when initializing CCDSim.";
        EXPECT_TRUE(isnan(currentDE)) << "Field 'currentDEC' is undefined when initializing CCDSim.";

        // Guide rate is fixed
        EXPECT_EQ(GuideRate, 7 /* arcsec/s */);

        // Initial guide offsets are zero
        EXPECT_EQ(guideNSOffset, 0);
        EXPECT_EQ(guideWEOffset, 0);

        double const arcsec = 1.0/3600.0;

        // Guiding in DEC stores offset in arcsec for next simulation step
        EXPECT_EQ(GuideNorth(1000.0), IPS_OK);
        EXPECT_NEAR(guideNSOffset, +7*arcsec, 1*arcsec);
        EXPECT_EQ(GuideSouth(1000.0), IPS_OK);
        EXPECT_NEAR(guideNSOffset, +0*arcsec, 1*arcsec);
        EXPECT_EQ(GuideSouth(1000.0), IPS_OK);
        EXPECT_NEAR(guideNSOffset, -7*arcsec, 1*arcsec);
        EXPECT_EQ(GuideNorth(1000.0), IPS_OK);
        EXPECT_NEAR(guideNSOffset, +0*arcsec, 1*arcsec);

        // RA guiding rate depends on declination, we need a valid one
        currentDE = 0;

        // Guiding in RA stores offset in arcsec for next simulation step
        // There is an adjustemnt based on declination - here zero from previous test
        EXPECT_EQ(GuideWest(1000.0), IPS_OK);
        EXPECT_NEAR(guideWEOffset, +7*arcsec, 15*arcsec);
        EXPECT_EQ(GuideEast(1000.0), IPS_OK);
        EXPECT_NEAR(guideWEOffset, +0*arcsec, 15*arcsec);
        EXPECT_EQ(GuideEast(1000.0), IPS_OK);
        EXPECT_NEAR(guideWEOffset, -7*arcsec, 15*arcsec);
        EXPECT_EQ(GuideWest(1000.0), IPS_OK);
        EXPECT_NEAR(guideWEOffset, +0*arcsec, 15*arcsec);

        // TODO: verify DEC-biased RA guiding rate
        // TODO: verify property-based guiding API
    }
};

TEST(CCDSimulatorDriverTest, test_properties)
{
    MockCCDSimDriver().testProperties();
}

TEST(CCDSimulatorDriverTest, test_guide_api)
{
    MockCCDSimDriver().testGuideAPI();
}

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
            INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
