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

    void testDrawStar()
    {
        int const xres = 65;
        int const yres = 65;
        int const maxval = pow(2,8);

        // Setup a 65x65, 16-bit depth, 4.6u square pixel sensor
        INumberVectorProperty * const p = getNumber("SIMULATOR_SETTINGS");
        ASSERT_NE(p, nullptr);
        IUFindNumber(p, "SIM_XRES")->value = (double) xres;
        IUFindNumber(p, "SIM_YRES")->value = (double) yres;
        // There is no way to set depth, it is hardcoded at 16-bit - so set maximum value instead
        IUFindNumber(p, "SIM_MAXVAL")->value = (double) maxval;
        IUFindNumber(p, "SIM_XSIZE")->value = 4.6;
        IUFindNumber(p, "SIM_YSIZE")->value = 4.6;

        // Setup some parameters to simplify verifications
        IUFindNumber(p, "SIM_SKYGLOW")->value = 0.0;
        IUFindNumber(p, "SIM_NOISE")->value = 0.0;
        this->seeing = 1.0f; // No way to control seeing from properties

        // Setup
        ASSERT_TRUE(setupParameters());

        // Assert our parameters
        ASSERT_EQ(PrimaryCCD.getBPP(), 16) << "Simulator CCD depth is hardcoded at 16 bits";
        ASSERT_EQ(PrimaryCCD.getXRes(), xres);
        ASSERT_EQ(PrimaryCCD.getYRes(), yres);
        ASSERT_EQ(PrimaryCCD.getPixelSizeX(), 4.6f);
        ASSERT_EQ(PrimaryCCD.getPixelSizeY(), 4.6f);
        ASSERT_NE(PrimaryCCD.getFrameBuffer(), nullptr) << "SetupParms creates the frame buffer";

        // Assert our simplifications
        EXPECT_EQ(this->seeing, 1.0f);
        EXPECT_EQ(this->ImageScalex, 1.0f);
        EXPECT_EQ(this->ImageScaley, 1.0f);
        EXPECT_EQ(this->m_SkyGlow, 0.0f);
        EXPECT_EQ(this->m_MaxNoise, 0.0f);

        // The CCD frame is NOT initialized after this call, so manually clear the buffer
        memset(this->PrimaryCCD.getFrameBuffer(), 0, this->PrimaryCCD.getFrameBufferSize());

        // Draw a star at the center row/column of the sensor
        // If we expose a magnitude of 1 for 1 second, we get 1 ADU at center, and zero elsewhere
        // Thus in order to verify the star profile provided by the simulator up to the third decimal, we expose 1000 seconds
        DrawImageStar(&PrimaryCCD, 0.0f, xres/2+1, xres/2+1, 1000.0f);

        // Get a pointer to the 16-bit frame buffer
        uint16_t const * const fb = reinterpret_cast<uint16_t*>(PrimaryCCD.getFrameBuffer());

        // Look at center, and up to 3 pixels away, and find activated photosites there - there is no skyglow nor noise in our parameters
        int const center = xres/2+1 + (yres/2+1)*xres;

        // Center photosite
        EXPECT_EQ(fb[center], std::min(maxval, 1000)) << "Recorded flux of magnitude 0.0 for 1000 seconds at center is 1000 ADU";

        // Up, left, right and bottom photosites at one pixel
        uint16_t const ADU_at_1pix = static_cast<uint16_t>(std::min((double)maxval, 1000.0 * exp(-1.4)));
        EXPECT_EQ(fb[center-xres], ADU_at_1pix);
        EXPECT_EQ(fb[center-1], ADU_at_1pix);
        EXPECT_EQ(fb[center+1], ADU_at_1pix);
        EXPECT_EQ(fb[center+xres], ADU_at_1pix);

        // Up, left, right and bottom photosites at two pixels
        double const ADU_at_2pix = static_cast<uint16_t>(std::min((double)maxval, 1000.0 * exp(-1.4*2*2)));
        EXPECT_EQ(fb[center-xres*2], ADU_at_2pix);
        EXPECT_EQ(fb[center-1*2], ADU_at_2pix);
        EXPECT_EQ(fb[center+1*2], ADU_at_2pix);
        EXPECT_EQ(fb[center+xres*2], ADU_at_2pix);

        // Up, left, right and bottom photosite neighbors at three pixels
        double const ADU_at_3pix = static_cast<uint16_t>(std::min((double)maxval, 1000.0 * exp(-1.4*3*3)));
        EXPECT_EQ(fb[center-xres*3], ADU_at_3pix);
        EXPECT_EQ(fb[center-1*3], ADU_at_3pix);
        EXPECT_EQ(fb[center+1*3], ADU_at_3pix);
        EXPECT_EQ(fb[center+xres*3], ADU_at_3pix);

        // Up, left, right and bottom photosite neighbors at four pixels
        EXPECT_EQ(fb[center-xres*4], 0.0);
        EXPECT_EQ(fb[center-1*4], 0.0);
        EXPECT_EQ(fb[center+1*4], 0.0);
        EXPECT_EQ(fb[center+xres*4], 0.0);

        // Conclude with a random benchmark
        auto const before = std::chrono::steady_clock::now();
        int const loops = 200000;
        for (int i = 0; i < loops; i++)
        {
            float const m = (15.0f*rand())/RAND_MAX;
            float const x = static_cast<float>(xres*rand())/RAND_MAX;
            float const y = static_cast<float>(yres*rand())/RAND_MAX;
            float const e = (100.0f*rand())/RAND_MAX;
            DrawImageStar(&PrimaryCCD, m, x, y, e);
        }
        auto const after = std::chrono::steady_clock::now();
        auto const duration = std::chrono::duration_cast <std::chrono::nanoseconds> (after - before).count() / loops;
        std::cout << "[          ] DrawStarImage - randomized no-noise no-skyglow benchmark: " << duration << "ns per call" << std::endl;
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

TEST(CCDSimulatorDriverTest, test_draw_star)
{
    MockCCDSimDriver().testDrawStar();
}

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
            INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
