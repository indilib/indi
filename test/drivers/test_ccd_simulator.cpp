#include "indilogger.h"

#include <gtest/gtest.h>

#include "ccd_simulator.h"

#include <cmath>
#include <set>
#include <utility>

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
            auto p = getNumber("SIMULATOR_SETTINGS");
            ASSERT_TRUE(p.isValid());
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

            auto defects = getNumber("SIM_DEFECTS");
            ASSERT_TRUE(defects.isValid());
            ASSERT_NE(defects.findWidgetByName("SIM_HOT_PIXELS"), nullptr);
            ASSERT_NE(defects.findWidgetByName("SIM_COLD_PIXELS"), nullptr);
            ASSERT_NE(defects.findWidgetByName("SIM_HOT_RATE"), nullptr);
            EXPECT_EQ(defects[SIM_HOT_PIXELS].getValue(), 0) << "defects must be off by default";
            EXPECT_EQ(defects[SIM_COLD_PIXELS].getValue(), 0) << "defects must be off by default";
        }

        void testGuideAPI()
        {
            EXPECT_TRUE(isnan(currentRA)) << "Field 'currentRA' is undefined when initializing CCDSim.";
            EXPECT_TRUE(isnan(currentDE)) << "Field 'currentDEC' is undefined when initializing CCDSim.";

            EXPECT_EQ(GuideRate, 7 /* arcsec/s */);

            EXPECT_EQ(guideNSOffset, 0);
            EXPECT_EQ(guideWEOffset, 0);

            double const arcsec = 1.0 / 3600.0;

            EXPECT_EQ(GuideNorth(1000.0), IPS_OK);
            EXPECT_NEAR(guideNSOffset, +7 * arcsec, 1 * arcsec);
            EXPECT_EQ(GuideSouth(1000.0), IPS_OK);
            EXPECT_NEAR(guideNSOffset, +0 * arcsec, 1 * arcsec);
            EXPECT_EQ(GuideSouth(1000.0), IPS_OK);
            EXPECT_NEAR(guideNSOffset, -7 * arcsec, 1 * arcsec);
            EXPECT_EQ(GuideNorth(1000.0), IPS_OK);
            EXPECT_NEAR(guideNSOffset, +0 * arcsec, 1 * arcsec);

            currentDE = 0;

            EXPECT_EQ(GuideWest(1000.0), IPS_OK);
            EXPECT_NEAR(guideWEOffset, +7 * arcsec, 15 * arcsec);
            EXPECT_EQ(GuideEast(1000.0), IPS_OK);
            EXPECT_NEAR(guideWEOffset, +0 * arcsec, 15 * arcsec);
            EXPECT_EQ(GuideEast(1000.0), IPS_OK);
            EXPECT_NEAR(guideWEOffset, -7 * arcsec, 15 * arcsec);
            EXPECT_EQ(GuideWest(1000.0), IPS_OK);
            EXPECT_NEAR(guideWEOffset, +0 * arcsec, 15 * arcsec);
        }
};

// ---------------------------------------------------------------------------
// SkyRenderer tests -- exercise the renderer standalone, no INDI driver.
// ---------------------------------------------------------------------------
class SkyRendererTest : public ::testing::Test
{
    protected:
        SkyRenderer renderer;
        INDI::CCDChip chip;

        void setupChip(int xres, int yres, float pixelSize = 4.6f)
        {
            chip.setResolution(xres, yres);
            chip.setPixelSize(pixelSize, pixelSize);
            chip.setFrame(0, 0, xres, yres);
            chip.setBPP(16);
            chip.setFrameBufferSize(xres * yres * 2);
            memset(chip.getFrameBuffer(), 0, chip.getFrameBufferSize());
        }

        uint16_t *fb()
        {
            return reinterpret_cast<uint16_t *>(chip.getFrameBuffer());
        }

        long sumFrame(int xres, int yres)
        {
            uint16_t *buf = fb();
            long s = 0;
            for (int i = 0; i < xres * yres; i++)
                s += buf[i];
            return s;
        }

        uint16_t peakPixel(int xres, int yres)
        {
            uint16_t *buf = fb();
            uint16_t peak = 0;
            for (int i = 0; i < xres * yres; i++)
                if (buf[i] > peak) peak = buf[i];
            return peak;
        }
};

// --- flux behavior (tested through drawImageStar) ---

TEST_F(SkyRendererTest, saturation_mag_produces_high_adu)
{
    int const xres = 33;
    int const yres = 33;
    int const maxval = 65000;

    RenderConfig cfg;
    cfg.maxVal        = maxval;
    cfg.saturationMag = 5.0f;
    cfg.limitingMag   = 20.0f;
    cfg.seeing        = 2.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);
    setupChip(xres, yres);

    renderer.drawImageStar(&chip, cfg.saturationMag, xres / 2, yres / 2, 1.0f);

    EXPECT_GT(peakPixel(xres, yres), static_cast<uint16_t>(maxval / 4))
            << "a star at saturation magnitude for 1s should produce high ADU";
}

TEST_F(SkyRendererTest, limiting_mag_produces_minimal_adu)
{
    int const xres = 33;
    int const yres = 33;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 20.0f;
    cfg.seeing        = 2.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);
    setupChip(xres, yres);

    renderer.drawImageStar(&chip, cfg.limitingMag, xres / 2, yres / 2, 1.0f);

    EXPECT_LE(peakPixel(xres, yres), 2u)
            << "a star at limiting magnitude for 1s should produce ~1 ADU";
}

TEST_F(SkyRendererTest, degenerate_mag_range_does_not_crash)
{
    int const xres = 33;
    int const yres = 33;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.saturationMag = 10.0f;
    cfg.limitingMag   = 10.0f;
    cfg.seeing        = 2.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);
    setupChip(xres, yres);

    renderer.drawImageStar(&chip, 10.0f, xres / 2, yres / 2, 1.0f);
    // Must not crash or produce NaN; some output is acceptable
}

TEST_F(SkyRendererTest, brighter_star_produces_more_flux)
{
    int const xres = 33;
    int const yres = 33;
    int const cx   = xres / 2;
    int const cy   = yres / 2;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 20.0f;
    cfg.seeing        = 2.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);

    setupChip(xres, yres);
    renderer.drawImageStar(&chip, 8.0f, cx, cy, 1.0f);
    long const sum_bright = sumFrame(xres, yres);

    setupChip(xres, yres);
    renderer.drawImageStar(&chip, 12.0f, cx, cy, 1.0f);
    long const sum_dim = sumFrame(xres, yres);

    ASSERT_GT(sum_bright, 0L);
    ASSERT_GT(sum_dim, 0L);
    EXPECT_GT(sum_bright, sum_dim);
}

// --- pixel clamping (tested through drawImageStar) ---

TEST_F(SkyRendererTest, pixel_values_clamped_at_max_val)
{
    int const xres   = 33;
    int const yres   = 33;
    int const maxval = 1000;

    RenderConfig cfg;
    cfg.maxVal        = maxval;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 20.0f;
    cfg.seeing        = 2.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);
    setupChip(xres, yres);

    renderer.drawImageStar(&chip, 0.0f, xres / 2, yres / 2, 1000.0f);

    uint16_t *buf = fb();
    for (int i = 0; i < xres * yres; i++)
        EXPECT_LE(buf[i], static_cast<uint16_t>(maxval))
                << "no pixel may exceed maxVal";
}

// --- drawImageStar: behavioral properties ---

TEST_F(SkyRendererTest, star_center_is_brightest)
{
    int const xres = 33;
    int const yres = 33;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 20.0f;
    cfg.seeing        = 2.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);
    setupChip(xres, yres);

    int const cx = xres / 2;
    int const cy = yres / 2;
    renderer.drawImageStar(&chip, 5.0f, cx, cy, 1.0f);

    uint16_t *buf = fb();
    uint16_t const peak = buf[cy * xres + cx];
    EXPECT_GT(peak, 0u);
    EXPECT_GT(peak, buf[cy * xres + cx + 1]);
    EXPECT_GT(peak, buf[cy * xres + cx - 1]);
    EXPECT_GT(peak, buf[(cy + 1) * xres + cx]);
    EXPECT_GT(peak, buf[(cy - 1) * xres + cx]);
}

TEST_F(SkyRendererTest, star_profile_symmetric)
{
    int const xres = 33;
    int const yres = 33;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 20.0f;
    cfg.seeing        = 2.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);
    setupChip(xres, yres);

    int const cx = xres / 2;
    int const cy = yres / 2;
    renderer.drawImageStar(&chip, 5.0f, cx, cy, 1.0f);

    uint16_t *buf = fb();
    for (int d = 1; d <= 3; d++)
    {
        EXPECT_EQ(buf[cy * xres + cx + d], buf[cy * xres + cx - d])
                << "horizontal symmetry at d=" << d;
        EXPECT_EQ(buf[(cy + d) * xres + cx], buf[(cy - d) * xres + cx])
                << "vertical symmetry at d=" << d;
    }
}

TEST_F(SkyRendererTest, star_profile_monotonic_falloff)
{
    int const xres = 33;
    int const yres = 33;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 20.0f;
    cfg.seeing        = 2.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);
    setupChip(xres, yres);

    int const cx = xres / 2;
    int const cy = yres / 2;
    renderer.drawImageStar(&chip, 5.0f, cx, cy, 1.0f);

    uint16_t *buf = fb();
    for (int d = 0; d < 5; d++)
    {
        uint16_t const inner = buf[cy * xres + cx + d];
        uint16_t const outer = buf[cy * xres + cx + d + 1];
        EXPECT_GE(inner, outer)
                << "profile must not increase at d=" << d << " -> " << d + 1;
    }
}

TEST_F(SkyRendererTest, star_zero_beyond_render_box)
{
    int const xres = 65;
    int const yres = 65;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 20.0f;
    cfg.seeing        = 1.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);
    setupChip(xres, yres);

    int const cx = xres / 2;
    int const cy = yres / 2;
    renderer.drawImageStar(&chip, 10.0f, cx, cy, 1.0f);

    uint16_t *buf = fb();
    int const beyond = static_cast<int>(3.0f * cfg.seeing / 1.0f) + 2;
    for (int d = beyond; d < cx; d++)
    {
        EXPECT_EQ(buf[cy * xres + cx + d], 0u)
                << "pixel at d=" << d << " is beyond the render box";
    }
}

// Regression guard for indilib/indi#2465: the PSF must be centred on the true
// (sub-pixel) star position, not snapped to the truncated integer pixel. If the
// fractional part of the coordinate is discarded, a sub-pixel shift produces no
// change in the rendered centroid, which quantised guide-sim drift to a lattice
// and broke calibration.
TEST_F(SkyRendererTest, star_centroid_tracks_subpixel_shift)
{
    int const xres = 201;
    int const yres = 201;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 20.0f;
    cfg.seeing        = 3.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);

    // Flux-weighted X centroid of the whole frame.
    auto centroidX = [this](int nx, int ny) -> double
    {
        uint16_t *buf = fb();
        double sumWeighted = 0.0;
        double sumFlux     = 0.0;
        for (int y = 0; y < ny; y++)
            for (int x = 0; x < nx; x++)
            {
                double const v = buf[y * nx + x];
                sumWeighted += v * x;
                sumFlux     += v;
            }
        return sumFlux > 0 ? sumWeighted / sumFlux : -1.0;
    };

    // Integer y keeps the comparison on the X axis only.
    float const y = 100.0f;

    setupChip(xres, yres);
    renderer.drawImageStar(&chip, 6.0f, 100.3f, y, 1.0f);
    double const cxLow = centroidX(xres, yres);

    setupChip(xres, yres);
    renderer.drawImageStar(&chip, 6.0f, 100.7f, y, 1.0f);
    double const cxHigh = centroidX(xres, yres);

    ASSERT_GT(cxLow, 0.0);
    ASSERT_GT(cxHigh, 0.0);

    // Each centroid should sit near its true sub-pixel position...
    EXPECT_NEAR(cxLow, 100.3, 0.15);
    EXPECT_NEAR(cxHigh, 100.7, 0.15);
    // ...and, crucially, the 0.4 px shift must be reflected in the render.
    // The pre-fix code snapped both to pixel 100, giving a zero difference.
    EXPECT_GT(cxHigh - cxLow, 0.2);
}

TEST_F(SkyRendererTest, star_flux_scales_with_exposure)
{
    int const xres = 33;
    int const yres = 33;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 20.0f;
    cfg.seeing        = 2.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);

    int const cx = xres / 2;
    int const cy = yres / 2;

    setupChip(xres, yres);
    renderer.drawImageStar(&chip, 10.0f, cx, cy, 1.0f);
    long const sum1s = sumFrame(xres, yres);

    setupChip(xres, yres);
    renderer.drawImageStar(&chip, 10.0f, cx, cy, 4.0f);
    long const sum4s = sumFrame(xres, yres);

    ASSERT_GT(sum1s, 0L);
    EXPECT_NEAR(static_cast<double>(sum4s) / sum1s, 4.0, 0.5)
            << "total flux must scale linearly with exposure time";
}

TEST_F(SkyRendererTest, star_off_chip_draws_nothing)
{
    int const xres = 33;
    int const yres = 33;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 20.0f;
    cfg.seeing        = 2.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);
    setupChip(xres, yres);

    renderer.drawImageStar(&chip, 5.0f, -10.0f, -10.0f, 1.0f);
    renderer.drawImageStar(&chip, 5.0f, xres + 10.0f, yres + 10.0f, 1.0f);
    EXPECT_EQ(sumFrame(xres, yres), 0);
}

// --- renderFrame ---

TEST_F(SkyRendererTest, render_frame_image_scale)
{
    int const xres = 100;
    int const yres = 100;
    float const pixelSizeUM = 5.0f;
    double const focalLengthMM = 1000.0;

    RenderConfig cfg;
    cfg.maxVal  = 65000;
    cfg.skyGlow = 99.0f;
    renderer.setConfig(cfg);
    setupChip(xres, yres, pixelSizeUM);

    renderer.renderFrame(&chip, 0.0, 0.0, focalLengthMM, 0.0, 1.0f, false);

    float const expectedScale = static_cast<float>((pixelSizeUM / focalLengthMM) * 206.3);
    EXPECT_NEAR(renderer.imageScaleX(), expectedScale, 0.001f);
    EXPECT_NEAR(renderer.imageScaleY(), expectedScale, 0.001f);
}

TEST_F(SkyRendererTest, render_frame_no_stars_produces_sky_glow)
{
    int const xres = 33;
    int const yres = 33;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.skyGlow       = 15.0f;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 20.0f;
    renderer.setConfig(cfg);
    setupChip(xres, yres);

    int drawn = renderer.renderFrame(&chip, 0.0, 0.0, 500.0, 0.0, 10.0f, false);
    EXPECT_EQ(drawn, 0) << "renderStars=false must draw zero stars";
    EXPECT_GT(sumFrame(xres, yres), 0L) << "sky glow must still be rendered";
}

TEST_F(SkyRendererTest, render_frame_sky_glow_vignetting)
{
    int const xres = 65;
    int const yres = 65;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.skyGlow       = 15.0f;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 20.0f;
    renderer.setConfig(cfg);
    setupChip(xres, yres);

    renderer.renderFrame(&chip, 0.0, 0.0, 500.0, 0.0, 10.0f, false);

    uint16_t *buf = fb();
    uint16_t const center_val = buf[(yres / 2) * xres + xres / 2];
    uint16_t const corner_val = buf[0];

    EXPECT_GT(center_val, 0u);
    EXPECT_GT(center_val, corner_val)
            << "vignetting must make center brighter than corners";
}

TEST_F(SkyRendererTest, render_frame_clears_buffer)
{
    int const xres = 16;
    int const yres = 16;

    RenderConfig cfg;
    cfg.maxVal  = 65000;
    cfg.skyGlow = 99.0f;
    renderer.setConfig(cfg);
    setupChip(xres, yres);

    memset(chip.getFrameBuffer(), 0xFF, chip.getFrameBufferSize());

    renderer.renderFrame(&chip, 0.0, 0.0, 500.0, 0.0, 0.001f, false);

    uint16_t *buf = fb();
    for (int i = 0; i < xres * yres; i++)
        EXPECT_LT(buf[i], 10u) << "renderFrame must clear pre-existing data";
}

// --- applyReadoutNoise ---

TEST_F(SkyRendererTest, readout_noise_adds_bias_and_noise)
{
    int const xres = 16;
    int const yres = 16;
    int const maxval = 65000;
    int const bias = 1500;
    int const maxNoise = 20;

    RenderConfig cfg;
    cfg.maxVal = maxval;
    renderer.setConfig(cfg);
    setupChip(xres, yres);

    renderer.applyReadoutNoise(&chip, bias, maxNoise);

    uint16_t *buf = fb();
    for (int i = 0; i < xres * yres; i++)
    {
        EXPECT_GE(buf[i], static_cast<uint16_t>(bias));
        EXPECT_LE(buf[i], static_cast<uint16_t>(bias + maxNoise));
    }
}

TEST_F(SkyRendererTest, readout_noise_clamps_at_max_val)
{
    int const xres = 8;
    int const yres = 8;
    int const maxval = 100;

    RenderConfig cfg;
    cfg.maxVal = maxval;
    renderer.setConfig(cfg);
    setupChip(xres, yres);

    // Bias alone exceeds maxVal
    renderer.applyReadoutNoise(&chip, 200, 10);

    uint16_t *buf = fb();
    for (int i = 0; i < xres * yres; i++)
        EXPECT_EQ(buf[i], static_cast<uint16_t>(maxval));
}

TEST_F(SkyRendererTest, readout_noise_skipped_when_zero)
{
    int const xres = 8;
    int const yres = 8;

    RenderConfig cfg;
    cfg.maxVal = 65000;
    renderer.setConfig(cfg);
    setupChip(xres, yres);

    renderer.applyReadoutNoise(&chip, 1500, 0);

    EXPECT_EQ(sumFrame(xres, yres), 0)
            << "maxNoise <= 0 must skip noise application entirely";
}

// --- sensor defects ---

TEST_F(SkyRendererTest, defects_are_reproducible_distinct_and_in_bounds)
{
    int const xres = 200;
    int const yres = 150;

    auto const a = SkyRenderer::generateDefects(xres, yres, 300, 100, 1000.0f);
    auto const b = SkyRenderer::generateDefects(xres, yres, 300, 100, 1000.0f);
    auto const c = SkyRenderer::generateDefects(xres, yres, 300, 100, 1000.0f, 2);

    ASSERT_EQ(a.size(), 400u);
    ASSERT_EQ(b.size(), a.size());

    std::set<std::pair<int, int>> positions;
    int hot = 0, cold = 0;
    bool differsFromOtherSeed = false;
    for (size_t i = 0; i < a.size(); i++)
    {
        EXPECT_EQ(a[i].x, b[i].x);
        EXPECT_EQ(a[i].y, b[i].y);
        EXPECT_EQ(a[i].rate, b[i].rate);
        if (a[i].x != c[i].x || a[i].y != c[i].y)
            differsFromOtherSeed = true;

        EXPECT_GE(a[i].x, 0);
        EXPECT_LT(a[i].x, xres);
        EXPECT_GE(a[i].y, 0);
        EXPECT_LT(a[i].y, yres);
        positions.insert({a[i].x, a[i].y});

        if (a[i].cold)
        {
            cold++;
        }
        else
        {
            hot++;
            EXPECT_GE(a[i].rate, 500.0f);
            EXPECT_LT(a[i].rate, 1500.0f);
        }
    }
    EXPECT_EQ(positions.size(), a.size()) << "defects must not share a pixel";
    EXPECT_EQ(hot, 300);
    EXPECT_EQ(cold, 100);
    EXPECT_TRUE(differsFromOtherSeed);
}

TEST_F(SkyRendererTest, defects_are_clamped_to_sensor_size)
{
    auto const defects = SkyRenderer::generateDefects(4, 4, 10, 10, 100.0f);
    EXPECT_EQ(defects.size(), 16u);
    EXPECT_TRUE(SkyRenderer::generateDefects(0, 4, 10, 10, 100.0f).empty());
    EXPECT_TRUE(SkyRenderer::generateDefects(4, 4, 0, 0, 100.0f).empty());
}

TEST_F(SkyRendererTest, hot_pixels_scale_with_exposure_and_cold_pixels_read_low)
{
    int const xres = 32;
    int const yres = 32;
    int const bias = 1000;

    RenderConfig cfg;
    cfg.maxVal = 65000;
    renderer.setConfig(cfg);

    std::vector<SensorDefect> defects(2);
    defects[0].x = 5;
    defects[0].y = 7;
    defects[0].rate = 200.0f;
    defects[1].x = 20;
    defects[1].y = 3;
    defects[1].cold = true;

    for (float exposure : {1.0f, 3.0f})
    {
        setupChip(xres, yres);
        renderer.applyReadoutNoise(&chip, bias, 1); // flat bias, no noise
        renderer.applyDefects(&chip, defects, exposure, bias);

        uint16_t *buf = fb();
        EXPECT_EQ(buf[7 * xres + 5], static_cast<uint16_t>(bias + 200 * exposure));
        EXPECT_EQ(buf[3 * xres + 20], static_cast<uint16_t>(bias / 2));
        EXPECT_EQ(buf[0], static_cast<uint16_t>(bias)) << "other pixels must be left alone";
        EXPECT_EQ(sumFrame(xres, yres), static_cast<long>(xres * yres * bias + 200 * exposure - bias / 2));
    }
}

TEST_F(SkyRendererTest, hot_pixels_clamp_at_max_val)
{
    RenderConfig cfg;
    cfg.maxVal = 4095;
    renderer.setConfig(cfg);
    setupChip(8, 8);

    std::vector<SensorDefect> defects(1);
    defects[0].x = 2;
    defects[0].y = 2;
    defects[0].rate = 60000.0f;
    renderer.applyDefects(&chip, defects, 10.0f, 0);

    EXPECT_EQ(fb()[2 * 8 + 2], 4095u);
}

TEST_F(SkyRendererTest, defects_follow_the_subframe)
{
    int const xres = 64;
    int const yres = 64;

    RenderConfig cfg;
    cfg.maxVal = 65000;
    renderer.setConfig(cfg);
    setupChip(xres, yres);

    // 16x16 subframe starting at (10, 20)
    chip.setFrame(10, 20, 16, 16);
    memset(chip.getFrameBuffer(), 0, chip.getFrameBufferSize());

    std::vector<SensorDefect> defects(3);
    defects[0].x = 12; // inside -> subframe (2, 3)
    defects[0].y = 23;
    defects[0].rate = 100.0f;
    defects[1].x = 9;  // left of the subframe
    defects[1].y = 23;
    defects[1].rate = 100.0f;
    defects[2].x = 26; // right of the subframe (10 + 16)
    defects[2].y = 25;
    defects[2].rate = 100.0f;

    renderer.applyDefects(&chip, defects, 1.0f, 0);

    EXPECT_EQ(fb()[3 * 16 + 2], 100u);
    EXPECT_EQ(sumFrame(16, 16), 100) << "defects outside the subframe must not be drawn";
}

// --- benchmark (no assertions, informational only) ---

TEST_F(SkyRendererTest, draw_star_benchmark)
{
    int const xres = 65;
    int const yres = 65;

    RenderConfig cfg;
    cfg.maxVal        = 65000;
    cfg.saturationMag = 0.0f;
    cfg.limitingMag   = 30.0f;
    cfg.seeing        = 1.0f;
    renderer.setConfig(cfg);
    renderer.setImageScale(1.0f, 1.0f);
    setupChip(xres, yres);

    auto const before = std::chrono::steady_clock::now();
    int const loops = 200000;
    for (int i = 0; i < loops; i++)
    {
        float const m = (15.0f * rand()) / static_cast<float>(RAND_MAX);
        float const x = static_cast<float>(xres * rand()) / static_cast<float>(RAND_MAX);
        float const y = static_cast<float>(yres * rand()) / static_cast<float>(RAND_MAX);
        float const e = (100.0f * rand()) / static_cast<float>(RAND_MAX);
        renderer.drawImageStar(&chip, m, x, y, e);
    }
    auto const after = std::chrono::steady_clock::now();
    auto const duration = std::chrono::duration_cast<std::chrono::nanoseconds>(after - before).count() / loops;
    std::cout << "[          ] drawImageStar benchmark: " << duration << " ns/call" << std::endl;
}

// --- CCDSim driver-level tests ---

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
    return RUN_ALL_TESTS();
}
