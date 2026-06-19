#include "indilogger.h"

#include <gtest/gtest.h>

#include "guide_simulator.h"

char _me[] = "MockGuideSimDriver";
char *me = _me;

class MockGuideSimDriver : public GuideSim
{
    public:
        MockGuideSimDriver() : GuideSim()
        {
            initProperties();
            ISGetProperties(me);
        }

        void testProperties()
        {
            auto p = getNumber("SIMULATOR_SETTINGS");
            ASSERT_NE(p, nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_XRES"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_YRES"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_XSIZE"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_YSIZE"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_MAXVAL"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_SATURATION"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_LIMITINGMAG"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_NOISE"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_SKYGLOW"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_OAGOFFSET"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_POLAR"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_POLARDRIFT"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_KING_GAMMA"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_KING_THETA"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_RA_DRIFT"), nullptr);
            ASSERT_NE(p.findWidgetByName("SIM_DEC_DRIFT"), nullptr);
        }

        void testGuideAPI()
        {
            EXPECT_EQ(m_GuideRate, 7 /* arcsec/s */);
            EXPECT_EQ(m_GuideNSOffset, 0);
            EXPECT_EQ(m_GuideWEOffset, 0);

            double const arcsec = 1.0 / 3600.0;

            EXPECT_EQ(GuideNorth(1000.0), IPS_OK);
            EXPECT_NEAR(m_GuideNSOffset, +7 * arcsec, 1 * arcsec);
            EXPECT_EQ(GuideSouth(1000.0), IPS_OK);
            EXPECT_NEAR(m_GuideNSOffset, +0 * arcsec, 1 * arcsec);
            EXPECT_EQ(GuideSouth(1000.0), IPS_OK);
            EXPECT_NEAR(m_GuideNSOffset, -7 * arcsec, 1 * arcsec);
            EXPECT_EQ(GuideNorth(1000.0), IPS_OK);
            EXPECT_NEAR(m_GuideNSOffset, +0 * arcsec, 1 * arcsec);

            m_CurrentDEC = 0;

            EXPECT_EQ(GuideWest(1000.0), IPS_OK);
            EXPECT_NEAR(m_GuideWEOffset, +7 * arcsec, 15 * arcsec);
            EXPECT_EQ(GuideEast(1000.0), IPS_OK);
            EXPECT_NEAR(m_GuideWEOffset, +0 * arcsec, 15 * arcsec);
            EXPECT_EQ(GuideEast(1000.0), IPS_OK);
            EXPECT_NEAR(m_GuideWEOffset, -7 * arcsec, 15 * arcsec);
            EXPECT_EQ(GuideWest(1000.0), IPS_OK);
            EXPECT_NEAR(m_GuideWEOffset, +0 * arcsec, 15 * arcsec);
        }

        void prepareForRendering()
        {
            ScopeInfoNP[FOCAL_LENGTH].setValue(400.0);
            ScopeInfoNP[APERTURE].setValue(60.0);

            auto p = getNumber("SIMULATOR_SETTINGS");
            p.findWidgetByName("SIM_NOISE")->setValue(0.0);
            p.findWidgetByName("SIM_SKYGLOW")->setValue(15.0);
            p.findWidgetByName("SIM_BIAS")->setValue(0.0);

            SetupParms();
            memset(PrimaryCCD.getFrameBuffer(), 0, PrimaryCCD.getFrameBufferSize());
        }

        int peakPixel()
        {
            uint16_t *fb = reinterpret_cast<uint16_t *>(PrimaryCCD.getFrameBuffer());
            int const n  = PrimaryCCD.getXRes() * PrimaryCCD.getYRes();
            int peak = 0;
            for (int i = 0; i < n; i++)
                if (fb[i] > peak) peak = fb[i];
            return peak;
        }

        // Render a frame with renderStars=false (no GSC dependency).
        // Exercises the full DrawCcdFrame pipeline: coordinate math, config
        // setup, renderFrame, and readout noise.
        void testDrawCcdFrameSmokeTest()
        {
            prepareForRendering();

            RA  = 6.0;
            Dec = 30.0;
            pierSide = 0;

            ASSERT_TRUE(StartExposure(1.0f));

            EXPECT_GT(peakPixel(), 0)
                    << "frame must contain sky glow even without GSC stars";
        }

        // Exercise the King cone-error path. With m_KingGamma > 0 the
        // coordinate transform runs before renderFrame. Verify it does not
        // crash or produce a blank frame.
        void testKingTransformSmokeTest()
        {
            prepareForRendering();

            auto p = getNumber("SIMULATOR_SETTINGS");
            p.findWidgetByName("SIM_KING_GAMMA")->setValue(0.5);
            p.findWidgetByName("SIM_KING_THETA")->setValue(45.0);
            SetupParms();
            memset(PrimaryCCD.getFrameBuffer(), 0, PrimaryCCD.getFrameBufferSize());

            RA  = 6.0;
            Dec = 30.0;
            Longitude = -100.0;
            pierSide = 0;

            ASSERT_TRUE(StartExposure(1.0f));

            EXPECT_GT(peakPixel(), 0)
                    << "King path must still produce sky glow";
        }

        // Exercise the polar drift code path with non-zero polar error
        // and dec drift. Verifies the path runs without crashing.
        void testPolarDriftSmokeTest()
        {
            prepareForRendering();

            auto p = getNumber("SIMULATOR_SETTINGS");
            p.findWidgetByName("SIM_POLAR")->setValue(10.0);
            p.findWidgetByName("SIM_POLARDRIFT")->setValue(1.0);
            p.findWidgetByName("SIM_OAGOFFSET")->setValue(20.0);
            p.findWidgetByName("SIM_RA_DRIFT")->setValue(0.5);
            p.findWidgetByName("SIM_DEC_DRIFT")->setValue(0.3);
            SetupParms();
            memset(PrimaryCCD.getFrameBuffer(), 0, PrimaryCCD.getFrameBufferSize());

            RA  = 6.0;
            Dec = 30.0;
            pierSide = 0;

            ASSERT_TRUE(StartExposure(1.0f));

            EXPECT_GT(peakPixel(), 0)
                    << "frame must contain sky glow with polar drift active";
        }
};

TEST(GuideSimulatorDriverTest, test_properties)
{
    MockGuideSimDriver().testProperties();
}

TEST(GuideSimulatorDriverTest, test_guide_api)
{
    MockGuideSimDriver().testGuideAPI();
}

TEST(GuideSimulatorDriverTest, draw_ccd_frame_smoke_test)
{
    MockGuideSimDriver().testDrawCcdFrameSmokeTest();
}

TEST(GuideSimulatorDriverTest, king_transform_smoke_test)
{
    MockGuideSimDriver().testKingTransformSmokeTest();
}

TEST(GuideSimulatorDriverTest, polar_drift_smoke_test)
{
    MockGuideSimDriver().testPolarDriftSmokeTest();
}

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
