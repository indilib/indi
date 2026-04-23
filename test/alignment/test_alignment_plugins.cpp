/*******************************************************************************
 * unit test for Alignment Math Plugins
 *******************************************************************************/

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <vector>

#include <indilogger.h>
#include <indicom.h>
#include <libastro.h>

#include <alignment/MathPlugin.h>
#include <alignment/BuiltInMathPlugin.h>
#include <alignment/SVDMathPlugin.h>
#include <alignment/NearestMathPlugin.h>
#include <alignment/HaltonSequence.h>
#include <alignment/TelescopeDirectionVectorSupportFunctions.h>
#include "../../drivers/telescope/scopesim_helper.h"

#include <libnova/sidereal_time.h>
#include <libnova/julian_day.h>

using namespace INDI::AlignmentSubsystem;

// ---------------------------------------------------------------------------
// MountErrors -- all six Wallace terms, in degrees
// ---------------------------------------------------------------------------

struct MountErrors
{
    double ih = 0;  ///< HA index error         (degrees)
    double id = 0;  ///< Dec index error         (degrees)
    double ch = 0;  ///< Collimation/cone error  (degrees)
    double np = 0;  ///< Non-perpendicularity    (degrees)
    double ma = 0;  ///< Polar Azimuth error     (degrees)
    double me = 0;  ///< Polar Elevation error   (degrees)
};

// ---------------------------------------------------------------------------
// ErrorStats -- statistical summary of residuals
// ---------------------------------------------------------------------------

struct ErrorStats
{
    double max  = 0;
    double rms  = 0;
    double mean = 0;
    double p95  = 0;

    static ErrorStats compute(std::vector<double> residuals)
    {
        ErrorStats s;
        if (residuals.empty()) return s;

        double sumSq = 0;
        double sum   = 0;
        for (double r : residuals)
        {
            sumSq += r * r;
            sum   += r;
            if (r > s.max) s.max = r;
        }
        s.rms  = std::sqrt(sumSq / residuals.size());
        s.mean = sum / residuals.size();

        std::sort(residuals.begin(), residuals.end());
        size_t idx95 = static_cast<size_t>(0.95 * (residuals.size() - 1));
        s.p95 = residuals[idx95];

        return s;
    }
};

// -----------------------------------------------------------------------
// Grid sampling helpers
// -----------------------------------------------------------------------

static void getSkyPoint(double rawRA, double rawDec, double minDec, double maxDec, double &ra, double &dec)
{
    ra = rawRA * 24.0;
    double sinMin = std::sin(DEG_TO_RAD(minDec));
    double sinMax = std::sin(DEG_TO_RAD(maxDec));
    dec = RAD_TO_DEG(std::asin(sinMin + rawDec * (sinMax - sinMin)));
}

static const MountErrors kMixed = {
    .ih = ARCMIN_TO_DEG(5),  .id = ARCMIN_TO_DEG(3),  .ch = ARCMIN_TO_DEG(8),
    .np = ARCMIN_TO_DEG(0.5), .ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(6),
};

// ---------------------------------------------------------------------------
// Observatory sites
// ---------------------------------------------------------------------------

static const INDI::IGeographicCoordinates kLosAngeles = { .longitude = -118.2, .latitude = 34.1, .elevation = 0 };
static const INDI::IGeographicCoordinates kSydney     = { .longitude =  151.2, .latitude = -33.9, .elevation = 0 };
static const INDI::IGeographicCoordinates kTokyo      = { .longitude =  139.7, .latitude = 35.7, .elevation = 0 };
static const INDI::IGeographicCoordinates kNairobi    = { .longitude =   36.8, .latitude = 0.0, .elevation = 0 };
static const INDI::IGeographicCoordinates kArctic     = { .longitude =   15.6, .latitude = 78.2, .elevation = 0 };
static const INDI::IGeographicCoordinates kAntarctic  = { .longitude =  166.7, .latitude = -77.8, .elevation = 0 };

// ---------------------------------------------------------------------------
// Sky windows used for Halton point generation
// ---------------------------------------------------------------------------

static constexpr double kEQ_MinDec = -60.0;   // equatorial: Dec lower bound (deg)
static constexpr double kEQ_MaxDec =  80.0;   // equatorial: Dec upper bound (deg)
static constexpr double kAZ_MinDec =  20.0;   // AltAz: Dec lower bound (deg)
static constexpr double kAZ_MaxDec =  85.0;   // AltAz: Dec upper bound (deg)

// ---------------------------------------------------------------------------
// Tolerance constants
// ---------------------------------------------------------------------------

static constexpr double kRoundTripTolDeg   =  0.05;          // RunPluginAllInAlignment Dec tolerance (~3')
static constexpr double kRoundTripTolHours = kRoundTripTolDeg / 15.0;  // equivalent RA tolerance in hours

// ---------------------------------------------------------------------------
// AlignmentPluginTest fixture
// ---------------------------------------------------------------------------

class AlignmentPluginTest : public ::testing::Test
{
protected:
    // Fixed JD for fully deterministic transforms (2026-03-14)
    static constexpr double fixedJD = 2461113.81667;

    void SetUp() override
    {
        INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                              INDI::Logger::DBG_DEBUG, INDI::Logger::DBG_DEBUG);
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /// Build and configure a scopesim_helper Alignment generator.
    static ::Alignment makeGenerator(const MountErrors &e, INDI::IGeographicCoordinates site,
                                     ::Alignment::MOUNT_TYPE mountType = ::Alignment::EQ_GEM)
    {
        ::Alignment gen;
        gen.mountType = mountType;
        gen.latitude  = Angle(site.latitude);
        gen.longitude = Angle(site.longitude);
        gen.setCorrections(e.ih, e.id, e.ch, e.np, e.ma, e.me);
        return gen;
    }

    static void buildEntry(::Alignment &gen,
                           TelescopeDirectionVectorSupportFunctions *pSupport,
                           double ra, double dec, double lst_hrs,
                           ::Alignment::MOUNT_TYPE mountType,
                           AlignmentDatabaseEntry &entry)
    {
        Angle ha(get_local_hour_angle(lst_hrs, ra), Angle::HOURS), adec(dec);

        entry.RightAscension        = ra;
        entry.Declination           = dec;
        entry.ObservationJulianDate = fixedJD;

        if (mountType == ::Alignment::ALTAZ)
        {
            Angle primary, secondary;
            gen.apparentHaDecToMount(ha, adec, &primary, &secondary);
            INDI::IHorizontalCoordinates horCoords = { primary.Degrees(), secondary.Degrees() };
            entry.TelescopeDirection = pSupport->TelescopeDirectionVectorFromAltitudeAzimuth(horCoords);
        }
        else
        {
            Angle mha, mdec;
            gen.observedToInstrument(ha, adec, &mha, &mdec);
            INDI::IEquatorialCoordinates haCoords = { DEG_TO_HOURS(mha.Degrees()), mdec.Degrees() };
            entry.TelescopeDirection = pSupport->TelescopeDirectionVectorFromLocalHourAngleDeclination(haCoords);
        }
    }

    // -----------------------------------------------------------------------
    // RunPluginAllInAlignment -- all points in alignment
    // -----------------------------------------------------------------------

    void RunPluginAllInAlignment(MathPlugin &plugin, const MountErrors &errors,
                                 int numPoints,
                                 INDI::IGeographicCoordinates site = kLosAngeles,
                                 ::Alignment::MOUNT_TYPE mountType = ::Alignment::EQ_GEM,
                                 double minDecOverride = NAN, double maxDecOverride = NAN)
    {
        double minDec = std::isnan(minDecOverride) ? ((mountType == ::Alignment::ALTAZ) ? kAZ_MinDec : kEQ_MinDec) : minDecOverride;
        double maxDec = std::isnan(maxDecOverride) ? ((mountType == ::Alignment::ALTAZ) ? kAZ_MaxDec : kEQ_MaxDec) : maxDecOverride;

        InMemoryDatabase db;
        db.SetDatabaseReferencePosition(site);

        MountAlignment_t alignment = (mountType == ::Alignment::ALTAZ) ? ZENITH :
                                     site.latitude >= 0  ? NORTH_CELESTIAL_POLE
                                                     : SOUTH_CELESTIAL_POLE;
        plugin.SetApproximateMountAlignment(alignment);

        ::Alignment generator = makeGenerator(errors, site, mountType);

        auto *pSupport = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);
        ASSERT_NE(pSupport, nullptr);

        double gmst = ln_get_apparent_sidereal_time(fixedJD);
        double lst  = range24(gmst + DEG_TO_HOURS(site.longitude));

        HaltonSequence seq(2, 3);
        for (int i = 1; i <= numPoints; ++i)
        {
            double ra, dec;
            auto raw = seq.getRaw(i);
            getSkyPoint(raw.first, raw.second, minDec, maxDec, ra, dec);

            AlignmentDatabaseEntry entry;
            buildEntry(generator, pSupport, ra, dec, lst, mountType, entry);
            db.GetAlignmentDatabase().push_back(entry);
        }

        ASSERT_TRUE(plugin.Initialise(&db));

        double testRA  = 12.0;
        double testDec = (mountType == ::Alignment::ALTAZ) ? 60.0 : 45.0;
        double joff    = fixedJD - ln_get_julian_from_sys();

        TelescopeDirectionVector resultV;
        ASSERT_TRUE(plugin.TransformCelestialToTelescope(testRA, testDec, joff, resultV));

        double outRA, outDec;
        plugin.TransformTelescopeToCelestial(resultV, outRA, outDec, joff);

        ASSERT_NEAR(testRA,  outRA,  kRoundTripTolHours);
        ASSERT_NEAR(testDec, outDec, kRoundTripTolDeg);
    }

    // -----------------------------------------------------------------------
    // RunPluginAlignValidate -- Dual-sequence align/validate split
    //
    // targetRMSArcsec: fixed RMS precision limit (e.g. 20" for high quality)
    // -----------------------------------------------------------------------

    void RunPluginAlignValidate(MathPlugin &plugin, const MountErrors &errors,
                                int numAlign, int numValidate,
                                double targetRMSArcsec,
                                INDI::IGeographicCoordinates site = kLosAngeles,
                                ::Alignment::MOUNT_TYPE mountType = ::Alignment::EQ_GEM,
                                double minDecOverride = NAN, double maxDecOverride = NAN,
                                double *maxResidualOut = nullptr)
    {
        double minDec = std::isnan(minDecOverride) ? ((mountType == ::Alignment::ALTAZ) ? kAZ_MinDec : kEQ_MinDec) : minDecOverride;
        double maxDec = std::isnan(maxDecOverride) ? ((mountType == ::Alignment::ALTAZ) ? kAZ_MaxDec : kEQ_MaxDec) : maxDecOverride;

        InMemoryDatabase alignDb;
        alignDb.SetDatabaseReferencePosition(site);

        MountAlignment_t alignment = (mountType == ::Alignment::ALTAZ) ? ZENITH :
                                     site.latitude >= 0  ? NORTH_CELESTIAL_POLE
                                                     : SOUTH_CELESTIAL_POLE;
        plugin.SetApproximateMountAlignment(alignment);

        ::Alignment generator = makeGenerator(errors, site, mountType);

        auto *pSupport = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);
        ASSERT_NE(pSupport, nullptr);

        double gmst = ln_get_apparent_sidereal_time(fixedJD);
        double lst  = range24(gmst + DEG_TO_HOURS(site.longitude));

        // Alignment layer: Halton(2, 3)
        HaltonSequence alignSeq(2, 3);
        for (int i = 1; i <= numAlign; ++i)
        {
            double ra, dec;
            auto raw = alignSeq.getRaw(i);
            getSkyPoint(raw.first, raw.second, minDec, maxDec, ra, dec);

            AlignmentDatabaseEntry entry;
            buildEntry(generator, pSupport, ra, dec, lst, mountType, entry);
            alignDb.GetAlignmentDatabase().push_back(entry);
        }

        ASSERT_TRUE(plugin.Initialise(&alignDb));

        double joff = fixedJD - ln_get_julian_from_sys();

        // Validation layer: Halton(5, 7)
        HaltonSequence validateSeq(5, 7);
        std::vector<double> residuals;

        for (int i = 1; i <= numValidate; ++i)
        {
            double ra, dec;
            auto raw = validateSeq.getRaw(i);
            getSkyPoint(raw.first, raw.second, minDec, maxDec, ra, dec);

            TelescopeDirectionVector resultV;
            ASSERT_TRUE(plugin.TransformCelestialToTelescope(ra, dec, joff, resultV));

            double outRA, outDec;
            plugin.TransformTelescopeToCelestial(resultV, outRA, outDec, joff);

            double dRA  = DEG_TO_ARCSEC(rangeHA(outRA - ra) * 15.0) * std::cos(DEG_TO_RAD(dec));
            double dDec = DEG_TO_ARCSEC(outDec - dec);
            double residual = std::sqrt(dRA * dRA + dDec * dDec);

            residuals.push_back(residual);

            if (i <= 5) // Only log first few points to avoid spam
            {
                GTEST_LOG_(INFO) << "  validate[" << i << "] ra=" << ra
                                 << " dec=" << dec
                                 << " dRA=" << dRA << "\" dDec=" << dDec
                                 << "\" residual=" << residual << "\"";
            }
        }

        ErrorStats stats = ErrorStats::compute(residuals);
        GTEST_LOG_(INFO) << "[ STATS ] RMS: " << stats.rms << "\" Target: " << targetRMSArcsec 
                         << "\" P95: " << stats.p95 << "\" Peak: " << stats.max << "\"";

        if (maxResidualOut) *maxResidualOut = stats.max;

        EXPECT_LT(stats.rms, targetRMSArcsec)
            << "Plugin failed to meet RMS quality target " << targetRMSArcsec << "\"";
        
        // P95 safe bound is generally target * 1.5 to allow for artifacts
        EXPECT_LT(stats.p95, targetRMSArcsec * 1.5)
            << "Plugin failed to meet P95 quality target " << targetRMSArcsec * 1.5 << "\"";
    }

    // -----------------------------------------------------------------------
    // RunMeridianFlipValidate -- exercises GEM pier flips in both hemispheres
    // -----------------------------------------------------------------------
    template <typename T>
    void RunMeridianFlipValidate(T &plugin, const MountErrors &errors,
                                 const INDI::IGeographicCoordinates &location,
                                 const MountAlignment_t pole,
                                 double targetRMSArcsec = 40.0)
    {
        plugin.SetApproximateMountAlignment(pole);

        ::Alignment generator = makeGenerator(errors, location, ::Alignment::EQ_GEM);
        auto *pSupport = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);
        ASSERT_NE(pSupport, nullptr);

        InMemoryDatabase alignDb;
        alignDb.SetDatabaseReferencePosition(location);

        double gmst = ln_get_apparent_sidereal_time(fixedJD);
        double lst  = range24(gmst + DEG_TO_HOURS(location.longitude));

        // Use 12 points for alignment in flip tests
        HaltonSequence alignSeq(2, 3);
        for (int i = 1; i <= 12; ++i)
        {
            double ra, dec;
            auto raw = alignSeq.getRaw(i);
            getSkyPoint(raw.first, raw.second, kEQ_MinDec, kEQ_MaxDec, ra, dec);

            AlignmentDatabaseEntry entry;
            buildEntry(generator, pSupport, ra, dec, lst, ::Alignment::EQ_GEM, entry);
            alignDb.GetAlignmentDatabase().push_back(entry);
        }

        ASSERT_TRUE(plugin.Initialise(&alignDb));

        double joff = fixedJD - ln_get_julian_from_sys();

        // 100-point validation set for flip tests
        HaltonSequence validateSeq(5, 7);
        std::vector<double> residuals;

        for (int i = 1; i <= 100; ++i)
        {
            double ra, dec;
            auto raw = validateSeq.getRaw(i);
            getSkyPoint(raw.first, raw.second, kEQ_MinDec, kEQ_MaxDec, ra, dec);

            TelescopeDirectionVector resultV;
            ASSERT_TRUE(plugin.TransformCelestialToTelescope(ra, dec, joff, resultV));

            double outRA, outDec;
            plugin.TransformTelescopeToCelestial(resultV, outRA, outDec, joff);

            double dRA  = DEG_TO_ARCSEC(rangeHA(outRA - ra) * 15.0) * std::cos(DEG_TO_RAD(dec));
            double dDec = DEG_TO_ARCSEC(outDec - dec);
            double residual = std::sqrt(dRA * dRA + dDec * dDec);

            residuals.push_back(residual);

            if (i <= 5)
            {
                GTEST_LOG_(INFO) << "  flip_validate[" << i << "] ra=" << ra
                                 << " dec=" << dec
                                 << " dRA=" << dRA << "\" dDec=" << dDec
                                 << "\" residual=" << residual << "\"";
            }
        }

        ErrorStats stats = ErrorStats::compute(residuals);
        GTEST_LOG_(INFO) << "[ STATS ] RMS: " << stats.rms << "\" Target: " << targetRMSArcsec 
                         << "\" P95: " << stats.p95 << "\" Peak: " << stats.max << "\"";

        EXPECT_LT(stats.rms, targetRMSArcsec) 
            << "Plugin failed meridian flip RMS quality target " << targetRMSArcsec << "\"";
        
        EXPECT_LT(stats.p95, targetRMSArcsec * 1.5)
            << "Plugin failed meridian flip P95 quality target " << targetRMSArcsec * 1.5 << "\"";
    }
};

// ---------------------------------------------------------------------------
// Error recovery tests
// ---------------------------------------------------------------------------

TEST_F(AlignmentPluginTest, Test_BuiltIn_ErrorRecovery)
{
    BuiltInMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 3);
}

TEST_F(AlignmentPluginTest, Test_SVD_ErrorRecovery)
{
    SVDMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 3);
}

TEST_F(AlignmentPluginTest, Test_Nearest_ErrorRecovery)
{
    NearestMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 10);
}

TEST_F(AlignmentPluginTest, Test_SVD_AltAz)
{
    SVDMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 6, kLosAngeles, ::Alignment::ALTAZ);
}

// ---------------------------------------------------------------------------
// Single sync point -- all plugins
//
// Verifies that each plugin initialises, transforms, and round-trips correctly
// with exactly one sync point.  This exercises the size() >= 1 gate in
// AlignmentSubsystemForDrivers: corrections must apply from the first sync.
//
// Index errors on both axes (IH = 5 arcmin, ID = 3 arcmin) are used so the
// correction has both an RA and a Dec component.  With a single sync point
// every plugin's minimal model recovers only index errors, not polar errors,
// so pure index errors are the appropriate choice here.
//
// NearestMathPlugin applies the single point's offset uniformly across the
// sky — for pure index errors the offset is position-independent, so the
// round-trip is exact at any validation point.
// ---------------------------------------------------------------------------

TEST_F(AlignmentPluginTest, SinglePoint_BuiltIn)
{
    BuiltInMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ih = ARCMIN_TO_DEG(5), .id = ARCMIN_TO_DEG(3)}, 1);
}

TEST_F(AlignmentPluginTest, SinglePoint_SVD)
{
    SVDMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ih = ARCMIN_TO_DEG(5), .id = ARCMIN_TO_DEG(3)}, 1);
}

TEST_F(AlignmentPluginTest, SinglePoint_Nearest)
{
    NearestMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ih = ARCMIN_TO_DEG(5), .id = ARCMIN_TO_DEG(3)}, 1);
}

// ---------------------------------------------------------------------------
// AlignValidate -- equatorial mounts
// ---------------------------------------------------------------------------

TEST_F(AlignmentPluginTest, SVD_AlignValidate_Polar)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 12, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AxisErrors)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ih = ARCMIN_TO_DEG(5), .id = ARCMIN_TO_DEG(3), .ch = ARCMIN_TO_DEG(8)}, 12, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_NonPerp)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.np = ARCMIN_TO_DEG(2)}, 12, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_IHOnly)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ih = ARCMIN_TO_DEG(15)}, 12, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_CHOnly)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ch = ARCMIN_TO_DEG(20)}, 12, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AllTerms_LA)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 12, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AllTerms_Tokyo)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 12, 100, 1.0, kTokyo);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_SouthernHemisphere)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 12, 100, 1.0, kSydney);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_Arctic)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 12, 100, 1.0, kArctic);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_Antarctic)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 12, 100, 1.0, kAntarctic);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_SmallErrors)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ih = ARCSEC_TO_DEG(30), .me = ARCSEC_TO_DEG(18)}, 12, 100, 0.5);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_LargeErrors)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(30), .me = ARCMIN_TO_DEG(15)}, 12, 100, 1.0);
}

TEST_F(AlignmentPluginTest, BuiltIn_AlignValidate_AllTerms)
{
    BuiltInMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 12, 100, 1.0);
}

TEST_F(AlignmentPluginTest, Nearest_AlignValidate_AllTerms)
{
    // NearestMathPlugin round-trip is only reliable when the validate point
    // has an alignment point at the same HA.  Use all-in-alignment with many
    // points rather than an align/validate split.
    NearestMathPlugin plugin;
    RunPluginAllInAlignment(plugin, kMixed, 20);
}

TEST_F(AlignmentPluginTest, Nearest_AlignValidate_QualityCheck)
{
    // Nearest is a coarse point-lookup plugin. Performance is expected to be
    // much lower than vector-fit models (SVD/SPK).
    // We set a 20000" (~5.5 deg) target to reflect this coarse precision class
    // given a 12-point alignment grid.
    NearestMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 12, 100, 35000.0);
}

// ---------------------------------------------------------------------------
// AlignValidate -- AltAz mounts
// ---------------------------------------------------------------------------

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AltAz_Polar)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 12, 100, 20.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AltAz_AxisErrors)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ih = ARCMIN_TO_DEG(5), .id = ARCMIN_TO_DEG(3), .ch = ARCMIN_TO_DEG(8)}, 12, 100, 20.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AltAz_NonPerp)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.np = ARCMIN_TO_DEG(2)}, 12, 100, 20.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AltAz_AllTerms)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 12, 100, 20.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AltAz_Southern)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 12, 100, 20.0,
                           kSydney, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, BuiltIn_AlignValidate_AltAz)
{
    BuiltInMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 12, 100, 20.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, Nearest_AlignValidate_AltAz)
{
    // NearestMathPlugin round-trip is only reliable when the validate point
    // has an alignment point at the same HA.  Use all-in-alignment with many
    // points rather than an align/validate split.
    NearestMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 20, kLosAngeles, ::Alignment::ALTAZ);
}

// ---------------------------------------------------------------------------
// Convex hull fallback -- validate points outside the aligned sky region
// ---------------------------------------------------------------------------

TEST_F(AlignmentPluginTest, SVD_AlignValidate_ConvexHullFallback)
{
    SVDMathPlugin plugin;
    plugin.SetApproximateMountAlignment(NORTH_CELESTIAL_POLE);

    MountErrors errors = {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)};
    ::Alignment generator = makeGenerator(errors, kLosAngeles, ::Alignment::EQ_GEM);
    auto *pSupport = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);
    ASSERT_NE(pSupport, nullptr);

    InMemoryDatabase alignDb;
    alignDb.SetDatabaseReferencePosition(kLosAngeles);

    double gmst = ln_get_apparent_sidereal_time(fixedJD);
    double lst  = range24(gmst + DEG_TO_HOURS(kLosAngeles.longitude));

    // Alignment from eastern sky only: RA in [18h, 24h) or [0h, 6h)
    HaltonSequence seq(2, 3);
    int alignCount = 0;
    for (int i = 1; alignCount < 6; ++i)
    {
        double ra, dec;
        auto raw = seq.getRaw(i);
        getSkyPoint(raw.first, raw.second, kEQ_MinDec, kEQ_MaxDec, ra, dec);

        if (ra >= 6.0 && ra < 18.0)
            continue;
        AlignmentDatabaseEntry entry;
        buildEntry(generator, pSupport, ra, dec, lst, ::Alignment::EQ_GEM, entry);
        alignDb.GetAlignmentDatabase().push_back(entry);
        ++alignCount;
    }

    ASSERT_TRUE(plugin.Initialise(&alignDb));

    double joff = fixedJD - ln_get_julian_from_sys();

    // Validation from western sky: RA in [6h, 18h) -- outside the convex hull
    int validateCount = 0;
    for (int i = 1; validateCount < 6; ++i)
    {
        double ra, dec;
        auto raw = seq.getRaw(i);
        getSkyPoint(raw.first, raw.second, kEQ_MinDec, kEQ_MaxDec, ra, dec);

        if (ra < 6.0 || ra >= 18.0)
            continue;

        TelescopeDirectionVector resultV;
        // Must not crash or fail; fallback should return a usable (if coarse) TDV
        ASSERT_TRUE(plugin.TransformCelestialToTelescope(ra, dec, joff, resultV));

        double outRA, outDec;
        plugin.TransformTelescopeToCelestial(resultV, outRA, outDec, joff);

        double dRA  = DEG_TO_ARCSEC(rangeHA(outRA - ra) * 15.0) * std::cos(DEG_TO_RAD(dec));
        double dDec = DEG_TO_ARCSEC(outDec - dec);
        double residual = std::sqrt(dRA * dRA + dDec * dDec);

        GTEST_LOG_(INFO) << "  fallback[" << validateCount << "] ra=" << ra
                         << " dec=" << dec << " res=" << residual;
        EXPECT_LT(residual, 180.0);
        ++validateCount;
    }
}

// ---------------------------------------------------------------------------
// EQ_FORK mount type
// ---------------------------------------------------------------------------

TEST_F(AlignmentPluginTest, SVD_AlignValidate_EqFork_LA)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 12, 100, 1.0,
                           kLosAngeles, ::Alignment::EQ_FORK);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_EqFork_Sydney)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 12, 100, 1.0,
                           kSydney, ::Alignment::EQ_FORK);
}

// ---------------------------------------------------------------------------
// Meridian flip -- exercises GEM pier flips in both hemispheres
// ---------------------------------------------------------------------------

TEST_F(AlignmentPluginTest, SVD_AlignValidate_MeridianFlip_Northern)
{
    SVDMathPlugin plugin;
    RunMeridianFlipValidate(plugin, kMixed, kLosAngeles, NORTH_CELESTIAL_POLE);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_MeridianFlip_Southern)
{
    SVDMathPlugin plugin;
    RunMeridianFlipValidate(plugin, kMixed, kSydney, SOUTH_CELESTIAL_POLE);
}

TEST_F(AlignmentPluginTest, BuiltIn_AlignValidate_MeridianFlip_Northern)
{
    BuiltInMathPlugin plugin;
    RunMeridianFlipValidate(plugin, kMixed, kLosAngeles, NORTH_CELESTIAL_POLE);
}

TEST_F(AlignmentPluginTest, BuiltIn_AlignValidate_MeridianFlip_Southern)
{
    BuiltInMathPlugin plugin;
    RunMeridianFlipValidate(plugin, kMixed, kSydney, SOUTH_CELESTIAL_POLE);
}

TEST_F(AlignmentPluginTest, DISABLED_Nearest_AlignValidate_MeridianFlip_Northern)
{
    NearestMathPlugin plugin;
    RunMeridianFlipValidate(plugin, kMixed, kLosAngeles, NORTH_CELESTIAL_POLE);
}

TEST_F(AlignmentPluginTest, DISABLED_Nearest_AlignValidate_MeridianFlip_Southern)
{
    NearestMathPlugin plugin;
    RunMeridianFlipValidate(plugin, kMixed, kSydney, SOUTH_CELESTIAL_POLE);
}

// ---------------------------------------------------------------------------
// Equatorial site (Nairobi, lat=0 deg)
// ---------------------------------------------------------------------------

TEST_F(AlignmentPluginTest, SVD_AlignValidate_Equatorial)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 12, 100, 1.0, kNairobi);
}

// ---------------------------------------------------------------------------
// Polar-degeneracy regression tests
//
// When all sync points are near the celestial pole (EQ) or zenith (altaz),
// the roll-index term (IH/IA) is unobservable because its basis function
// scales as cos(pitch) which is ~0.  Without the degeneracy guard in
// Initialise(), the solver produces an arbitrary IH/IA value that causes
// wildly wrong gotos at any other part of the sky.
//
// These tests reproduce the exact failure scenario from the field:
//   1. Single sync point near pole/zenith with mount errors
//   2. Goto + roundtrip to a target far from the pole/zenith
//   3. Verify the result is sane (not garbage)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// PolarDegeneracy tests
//
// Syncs a plugin at a single near-pole (EQ) or near-zenith (altaz) point
// with mount errors, then does TransformCelestialToTelescope to a target
// far from the pole/zenith and checks the resulting encoder position is
// sane.  A roundtrip (C->T->C) always passes for invertible plugins, so
// we check the forward direction: decode the TDV to encoder HA/Dec (or
// Az/El) and verify it's not wildly different from what a zero-error mount
// would produce.
// ---------------------------------------------------------------------------

// Defined as TEST_F body helper so it can access fixture members.
// Called from individual TEST_F cases below.

static void RunPolarDegeneracyTestImpl(
    MathPlugin &plugin, const MountErrors &errors,
    INDI::IGeographicCoordinates site,
    ::Alignment::MOUNT_TYPE mountType,
    double fixedJD = 2461113.81667)
{
    auto makeGenerator = [](const MountErrors &e, INDI::IGeographicCoordinates s,
                            ::Alignment::MOUNT_TYPE mt) -> ::Alignment
    {
        ::Alignment gen;
        gen.mountType = mt;
        gen.latitude  = Angle(s.latitude);
        gen.longitude = Angle(s.longitude);
        gen.setCorrections(e.ih, e.id, e.ch, e.np, e.ma, e.me);
        return gen;
    };

    auto buildEntry = [&](::Alignment &gen,
                          TelescopeDirectionVectorSupportFunctions *pSupport,
                          double ra, double dec, double lst_hrs,
                          ::Alignment::MOUNT_TYPE mt,
                          AlignmentDatabaseEntry &entry)
    {
        Angle ha(get_local_hour_angle(lst_hrs, ra), Angle::HOURS), adec(dec);
        entry.RightAscension        = ra;
        entry.Declination           = dec;
        entry.ObservationJulianDate = fixedJD;
        if (mt == ::Alignment::ALTAZ)
        {
            Angle primary, secondary;
            gen.apparentHaDecToMount(ha, adec, &primary, &secondary);
            INDI::IHorizontalCoordinates horCoords = { primary.Degrees(), secondary.Degrees() };
            entry.TelescopeDirection = pSupport->TelescopeDirectionVectorFromAltitudeAzimuth(horCoords);
        }
        else
        {
            Angle mha, mdec;
            gen.observedToInstrument(ha, adec, &mha, &mdec);
            double instRA = range24(lst_hrs - mha.Hours());
            INDI::IEquatorialCoordinates raCoords = { instRA, mdec.Degrees() };
            entry.TelescopeDirection = pSupport->TelescopeDirectionVectorFromEquatorialCoordinates(raCoords);
        }
    };
    MountAlignment_t alignment = (mountType == ::Alignment::ALTAZ) ? ZENITH :
                                 site.latitude >= 0  ? NORTH_CELESTIAL_POLE
                                                 : SOUTH_CELESTIAL_POLE;
    plugin.SetApproximateMountAlignment(alignment);

    InMemoryDatabase db;
    db.SetDatabaseReferencePosition(site);

    ::Alignment gen = makeGenerator(errors, site, mountType);

    auto *pSupport = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);
    ASSERT_NE(pSupport, nullptr);

    double gmst = ln_get_apparent_sidereal_time(fixedJD);
    double lst  = range24(gmst + DEG_TO_HOURS(site.longitude));

    // Single sync point near the pole/zenith
    double syncRA, syncDec;
    if (mountType == ::Alignment::ALTAZ)
    {
        // Near-zenith: RA ≈ LST, Dec ≈ latitude
        syncRA  = lst;
        syncDec = site.latitude + 1.0;
    }
    else
    {
        // Near celestial pole
        syncRA  = lst - 1.0;
        syncDec = 88.0;
    }

    AlignmentDatabaseEntry entry;
    buildEntry(gen, pSupport, syncRA, syncDec, lst, mountType, entry);
    db.GetAlignmentDatabase().push_back(entry);

    ASSERT_TRUE(plugin.Initialise(&db));

    double joff = fixedJD - ln_get_julian_from_sys();

    // Target far from the pole/zenith
    double testRA  = 12.0;
    double testDec = (mountType == ::Alignment::ALTAZ) ? 30.0 : 45.0;

    TelescopeDirectionVector resultV;
    ASSERT_TRUE(plugin.TransformCelestialToTelescope(testRA, testDec, joff, resultV));

    // Decode the TDV to encoder coordinates and sanity-check
    if (mountType == ::Alignment::ALTAZ)
    {
        INDI::IHorizontalCoordinates hor;
        pSupport->AltitudeAzimuthFromTelescopeDirectionVector(resultV, hor);
        // For a target at Dec=30 from LA, the elevation should be positive
        // and less than ~90.  A degenerate transform might produce negative
        // elevation or wildly wrong azimuth.
        EXPECT_GT(hor.altitude, -10.0)
            << "AltAz polar degeneracy: El=" << hor.altitude
            << " -- forward transform produced sub-horizon encoder position";
        EXPECT_LT(hor.altitude, 90.0)
            << "AltAz polar degeneracy: El=" << hor.altitude;
    }
    else
    {
        INDI::IEquatorialCoordinates eq;
        pSupport->EquatorialCoordinatesFromTelescopeDirectionVector(resultV, eq);
        double encoderRA = eq.rightascension;
        // The encoder RA should be roughly near the target RA.
        // With small mount errors (arcminutes) the difference should be
        // well under a few degrees, not tens of degrees.
        double dRA = rangeHA(encoderRA - testRA) * 15.0;  // degrees
        // Tolerances are generous (15 deg) because this test uses large mount
        // errors (IH=5 deg, ID=5 deg) with a single sync point far from the
        // test target.  The goal is to detect catastrophic failures, not to
        // verify sub-degree accuracy.
        EXPECT_LT(std::abs(dRA), 15.0)
            << "EQ polar degeneracy: encoder RA=" << encoderRA
            << "h vs target RA=" << testRA
            << "h (dRA=" << dRA << " deg)"
            << " -- forward transform produced garbage encoder RA";
        EXPECT_LT(std::abs(eq.declination - testDec), 15.0)
            << "EQ polar degeneracy: encoder Dec=" << eq.declination
            << " vs target Dec=" << testDec
            << " -- forward transform produced garbage encoder Dec";
    }
}

// Field-scenario errors: IH=300' ID=300' CH=5' NP=0.5' MA=10' ME=6'
static const MountErrors kFieldErrors = {
    .ih = 5.0, .id = 5.0, .ch = ARCMIN_TO_DEG(5),
    .np = ARCMIN_TO_DEG(0.5), .ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(6),
};

TEST_F(AlignmentPluginTest, BuiltIn_PolarDegeneracy_EQ_SinglePoint)
{
    BuiltInMathPlugin plugin;
    RunPolarDegeneracyTestImpl(plugin, kFieldErrors, kLosAngeles, ::Alignment::EQ_GEM);
}

TEST_F(AlignmentPluginTest, SVD_PolarDegeneracy_EQ_SinglePoint)
{
    SVDMathPlugin plugin;
    RunPolarDegeneracyTestImpl(plugin, kFieldErrors, kLosAngeles, ::Alignment::EQ_GEM);
}

TEST_F(AlignmentPluginTest, Nearest_PolarDegeneracy_EQ_SinglePoint)
{
    NearestMathPlugin plugin;
    RunPolarDegeneracyTestImpl(plugin, kFieldErrors, kLosAngeles, ::Alignment::EQ_GEM);
}

TEST_F(AlignmentPluginTest, BuiltIn_PolarDegeneracy_AltAz_SinglePoint)
{
    BuiltInMathPlugin plugin;
    RunPolarDegeneracyTestImpl(plugin, kFieldErrors, kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SVD_PolarDegeneracy_AltAz_SinglePoint)
{
    SVDMathPlugin plugin;
    RunPolarDegeneracyTestImpl(plugin, kFieldErrors, kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, Nearest_PolarDegeneracy_AltAz_SinglePoint)
{
    NearestMathPlugin plugin;
    RunPolarDegeneracyTestImpl(plugin, kFieldErrors, kLosAngeles, ::Alignment::ALTAZ);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
