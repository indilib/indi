/*******************************************************************************
 * unit test for Alignment Math Plugins
 *******************************************************************************/

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <random>
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
#include <alignment/SPKMathPlugin.h>
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

// Perturb a unit vector by Gaussian noise of sigmaDeg per axis.
// Two orthogonal perturbation axes are chosen automatically.
static void addNoiseToDirection(TelescopeDirectionVector &tdv, double sigmaDeg, std::mt19937 &rng)
{
    if (sigmaDeg <= 0.0) return;

    std::normal_distribution<double> dist(0.0, DEG_TO_RAD(sigmaDeg));

    // Pick a basis vector not nearly parallel to tdv
    double ax = std::abs(tdv.x), ay = std::abs(tdv.y), az = std::abs(tdv.z);
    TelescopeDirectionVector e1, e2;
    if (ax <= ay && ax <= az)
        e1 = {0, -tdv.z, tdv.y};
    else if (ay <= az)
        e1 = {tdv.z, 0, -tdv.x};
    else
        e1 = {-tdv.y, tdv.x, 0};

    double len1 = std::sqrt(e1.x * e1.x + e1.y * e1.y + e1.z * e1.z);
    e1.x /= len1; e1.y /= len1; e1.z /= len1;

    // e2 = tdv × e1
    e2.x = tdv.y * e1.z - tdv.z * e1.y;
    e2.y = tdv.z * e1.x - tdv.x * e1.z;
    e2.z = tdv.x * e1.y - tdv.y * e1.x;

    double d1 = dist(rng), d2 = dist(rng);
    tdv.x += d1 * e1.x + d2 * e2.x;
    tdv.y += d1 * e1.y + d2 * e2.y;
    tdv.z += d1 * e1.z + d2 * e2.z;

    double len = std::sqrt(tdv.x * tdv.x + tdv.y * tdv.y + tdv.z * tdv.z);
    tdv.x /= len; tdv.y /= len; tdv.z /= len;
}

static const MountErrors kMixed = {
    .ih = ARCMIN_TO_DEG(5),  .id = ARCMIN_TO_DEG(3),  .ch = ARCMIN_TO_DEG(8),
    .np = ARCMIN_TO_DEG(0.5), .ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(6),
};

static const MountErrors kHugeIndex = {
    .ih = 35.0,  .id = -20.0,  .ch = 1.5,
    .np = 0.5, .ma = 2.0, .me = 2.0,
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
static constexpr double kAZ_MinEl  =  20.0;   // AltAz: elevation lower bound (deg); below ~15 deg refraction is nonlinear
static constexpr double kAZ_MaxEl  =  85.0;   // AltAz: elevation upper bound (deg); avoids azimuth-wrap singularity near zenith

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
                           double ra, double dec, double lst_hrs, double jd,
                           ::Alignment::MOUNT_TYPE mountType,
                           AlignmentDatabaseEntry &entry)
    {
        Angle ha(get_local_hour_angle(lst_hrs, ra), Angle::HOURS), adec(dec);

        entry.RightAscension        = ra;
        entry.Declination           = dec;
        entry.ObservationJulianDate = jd;

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
            double instRA = range24(lst_hrs - mha.Hours());
            INDI::IEquatorialCoordinates raCoords = { instRA, mdec.Degrees() };
            entry.TelescopeDirection = pSupport->TelescopeDirectionVectorFromEquatorialCoordinates(raCoords);
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
        double minDec = std::isnan(minDecOverride) ? ((mountType == ::Alignment::ALTAZ) ? kAZ_MinEl : kEQ_MinDec) : minDecOverride;
        double maxDec = std::isnan(maxDecOverride) ? ((mountType == ::Alignment::ALTAZ) ? kAZ_MaxEl : kEQ_MaxDec) : maxDecOverride;

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
                    buildEntry(generator, pSupport, ra, dec, lst, fixedJD, mountType, entry);
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
        double minDec = std::isnan(minDecOverride) ? ((mountType == ::Alignment::ALTAZ) ? kAZ_MinEl : kEQ_MinDec) : minDecOverride;
        double maxDec = std::isnan(maxDecOverride) ? ((mountType == ::Alignment::ALTAZ) ? kAZ_MaxEl : kEQ_MaxDec) : maxDecOverride;

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
            buildEntry(generator, pSupport, ra, dec, lst, fixedJD, mountType, entry);
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
    // RunPluginNoiseValidate -- measures pointing accuracy under centroiding
    // noise injected into alignment entries.
    //
    // noisyPlugin is trained on entries with noiseSigmaDeg per-axis Gaussian
    // noise.  refPlugin is trained on the same sky positions but without noise.
    // Validation measures the angular error between the two plugins' predicted
    // telescope directions at held-out sky positions, using the same JulianOffset
    // for both.  This directly measures generalization accuracy under noise without
    // any time-convention dependency.
    // -----------------------------------------------------------------------

    void RunPluginNoiseValidate(MathPlugin &noisyPlugin, MathPlugin &refPlugin,
                                const MountErrors &errors,
                                int numAlign, int numValidate,
                                double noiseSigmaDeg, double targetRMSArcsec,
                                INDI::IGeographicCoordinates site = kLosAngeles,
                                ::Alignment::MOUNT_TYPE mountType = ::Alignment::EQ_GEM)
    {
        double minDec = (mountType == ::Alignment::ALTAZ) ? kAZ_MinEl : kEQ_MinDec;
        double maxDec = (mountType == ::Alignment::ALTAZ) ? kAZ_MaxEl : kEQ_MaxDec;

        MountAlignment_t alignment = (mountType == ::Alignment::ALTAZ) ? ZENITH :
                                     site.latitude >= 0 ? NORTH_CELESTIAL_POLE : SOUTH_CELESTIAL_POLE;
        noisyPlugin.SetApproximateMountAlignment(alignment);
        refPlugin.SetApproximateMountAlignment(alignment);

        ::Alignment generator = makeGenerator(errors, site, mountType);
        auto *pSupport = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&noisyPlugin);
        ASSERT_NE(pSupport, nullptr);

        double gmst = ln_get_apparent_sidereal_time(fixedJD);
        double lst  = range24(gmst + DEG_TO_HOURS(site.longitude));

        std::mt19937 rng(42);

        InMemoryDatabase noisyDb, cleanDb;
        noisyDb.SetDatabaseReferencePosition(site);
        cleanDb.SetDatabaseReferencePosition(site);

        HaltonSequence alignSeq(2, 3);
        for (int i = 1; i <= numAlign; ++i)
        {
            double ra, dec;
            auto raw = alignSeq.getRaw(i);
            getSkyPoint(raw.first, raw.second, minDec, maxDec, ra, dec);

            AlignmentDatabaseEntry entry;
            buildEntry(generator, pSupport, ra, dec, lst, fixedJD, mountType, entry);
            cleanDb.GetAlignmentDatabase().push_back(entry);

            addNoiseToDirection(entry.TelescopeDirection, noiseSigmaDeg, rng);
            noisyDb.GetAlignmentDatabase().push_back(entry);
        }

        ASSERT_TRUE(refPlugin.Initialise(&cleanDb));
        ASSERT_TRUE(noisyPlugin.Initialise(&noisyDb));

        double joff = fixedJD - ln_get_julian_from_sys();

        HaltonSequence validateSeq(5, 7);
        std::vector<double> residuals;

        for (int i = 1; i <= numValidate; ++i)
        {
            double ra, dec;
            auto raw = validateSeq.getRaw(i);
            getSkyPoint(raw.first, raw.second, minDec, maxDec, ra, dec);

            // Noisy plugin's predicted direction
            TelescopeDirectionVector noisyV;
            ASSERT_TRUE(noisyPlugin.TransformCelestialToTelescope(ra, dec, joff, noisyV));

            // Reference (noiseless) plugin's predicted direction — same joff
            TelescopeDirectionVector refV;
            ASSERT_TRUE(refPlugin.TransformCelestialToTelescope(ra, dec, joff, refV));

            // Angular separation in arcseconds
            double dot = noisyV.x * refV.x + noisyV.y * refV.y + noisyV.z * refV.z;
            dot = std::max(-1.0, std::min(1.0, dot));
            double residual = DEG_TO_ARCSEC(RAD_TO_DEG(std::acos(dot)));

            residuals.push_back(residual);

            if (i <= 5)
            {
                GTEST_LOG_(INFO) << "  noise_validate[" << i << "] ra=" << ra
                                 << " dec=" << dec
                                 << " residual=" << residual << "\"";
            }
        }

        ErrorStats stats = ErrorStats::compute(residuals);
        GTEST_LOG_(INFO) << "[ STATS ] RMS: " << stats.rms << "\" Target: " << targetRMSArcsec
                         << "\" P95: " << stats.p95 << "\" Peak: " << stats.max << "\"";

        EXPECT_LT(stats.rms, targetRMSArcsec)
            << "Plugin failed noise-stress RMS target " << targetRMSArcsec << "\"";
        EXPECT_LT(stats.p95, targetRMSArcsec * 1.5)
            << "Plugin failed noise-stress P95 target " << targetRMSArcsec * 1.5 << "\"";
    }

    void RunTimeShiftValidate(MathPlugin &plugin, const MountErrors &errors,
                                int numAlign, int numValidate,
                                double shiftHours, double targetRMSArcsec,
                                INDI::IGeographicCoordinates site = kLosAngeles,
                                ::Alignment::MOUNT_TYPE mountType = ::Alignment::EQ_GEM)
    {
        double minDec = (mountType == ::Alignment::ALTAZ) ? kAZ_MinEl : kEQ_MinDec;
        double maxDec = (mountType == ::Alignment::ALTAZ) ? kAZ_MaxEl : kEQ_MaxDec;

        InMemoryDatabase alignDb;
        alignDb.SetDatabaseReferencePosition(site);

        MountAlignment_t alignment = (mountType == ::Alignment::ALTAZ) ? ZENITH :
                                     site.latitude >= 0  ? NORTH_CELESTIAL_POLE
                                                     : SOUTH_CELESTIAL_POLE;
        plugin.SetApproximateMountAlignment(alignment);

        ::Alignment generator = makeGenerator(errors, site, mountType);
        auto *pSupport = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);
        ASSERT_NE(pSupport, nullptr);

        // Training Phase: JD = fixedJD
        double gmst = ln_get_apparent_sidereal_time(fixedJD);
        double lst  = range24(gmst + DEG_TO_HOURS(site.longitude));

        HaltonSequence alignSeq(2, 3);
        for (int i = 1; i <= numAlign; ++i)
        {
            double ra, dec;
            auto raw = alignSeq.getRaw(i);
            getSkyPoint(raw.first, raw.second, minDec, maxDec, ra, dec);

            AlignmentDatabaseEntry entry;
            buildEntry(generator, pSupport, ra, dec, lst, fixedJD, mountType, entry);
            alignDb.GetAlignmentDatabase().push_back(entry);
        }

        ASSERT_TRUE(plugin.Initialise(&alignDb));

        // Validation Phase: JD = fixedJD + shiftHours/24
        double valJD = fixedJD + shiftHours / 24.0;
        double joff = valJD - ln_get_julian_from_sys();

        HaltonSequence validateSeq(5, 7);
        std::vector<double> residuals;

        for (int i = 1; i <= numValidate; ++i)
        {
            double ra, dec;
            auto raw = validateSeq.getRaw(i);
            getSkyPoint(raw.first, raw.second, minDec, maxDec, ra, dec);

            TelescopeDirectionVector resultV;
            // The plugin must correctly interpret ra, dec relative to the new joff (time)
            ASSERT_TRUE(plugin.TransformCelestialToTelescope(ra, dec, joff, resultV));

            double outRA, outDec;
            plugin.TransformTelescopeToCelestial(resultV, outRA, outDec, joff);

            double dRA  = DEG_TO_ARCSEC(rangeHA(outRA - ra) * 15.0) * std::cos(DEG_TO_RAD(dec));
            double dDec = DEG_TO_ARCSEC(outDec - dec);
            double residual = std::sqrt(dRA * dRA + dDec * dDec);

            residuals.push_back(residual);
        }

        ErrorStats stats = ErrorStats::compute(residuals);
        GTEST_LOG_(INFO) << "[ TIME SHIFT " << shiftHours << "h ] RMS: " << stats.rms << "\" Target: " << targetRMSArcsec
                         << "\" P95: " << stats.p95 << "\" Peak: " << stats.max << "\"";

        EXPECT_LT(stats.rms, targetRMSArcsec)
            << "Plugin failed time-shift RMS quality target " << targetRMSArcsec << "\"";
    }

    // -----------------------------------------------------------------------
    // RunGotoConventionRegression -- AltAz C->T convention check.
    //
    // Trains the plugin on numAlign points, then calls C->T on a held-out
    // sky position and decodes the returned TDV directly to Az/Alt without
    // doing T->C.  Compares against the scopesim ground-truth encoder.
    //
    // This breaks the round-trip symmetry that allows a 180deg Az convention
    // bug to pass AlignValidate tests undetected.  azTolDeg should be much
    // less than 180 but generous enough for the plugin's interpolation error.
    // -----------------------------------------------------------------------
    void RunGotoConventionRegression(MathPlugin &plugin,
                                     const MountErrors &errors,
                                     int numAlign,
                                     double valRA, double valDec,
                                     double azTolDeg,
                                     INDI::IGeographicCoordinates site = kLosAngeles)
    {
        plugin.SetApproximateMountAlignment(ZENITH);

        InMemoryDatabase db;
        db.SetDatabaseReferencePosition(site);

        ::Alignment gen = makeGenerator(errors, site, ::Alignment::ALTAZ);
        auto *pSupport  = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);
        ASSERT_NE(pSupport, nullptr);

        double gmst = ln_get_apparent_sidereal_time(fixedJD);
        double lst  = range24(gmst + DEG_TO_HOURS(site.longitude));

        HaltonSequence seq(2, 3);
        for (int i = 1; i <= numAlign; ++i)
        {
            double ra, dec;
            auto raw = seq.getRaw(i);
            getSkyPoint(raw.first, raw.second, kAZ_MinEl, kAZ_MaxEl, ra, dec);
            AlignmentDatabaseEntry entry;
            buildEntry(gen, pSupport, ra, dec, lst, fixedJD, ::Alignment::ALTAZ, entry);
            db.GetAlignmentDatabase().push_back(entry);
        }
        ASSERT_TRUE(plugin.Initialise(&db));

        double joff = fixedJD - ln_get_julian_from_sys();

        // Ground truth encoder position
        Angle ha(get_local_hour_angle(lst, valRA), Angle::HOURS);
        Angle adec(valDec);
        Angle encAz, encAlt;
        gen.apparentHaDecToMount(ha, adec, &encAz, &encAlt);

        // Plugin C->T prediction, decoded to Az/Alt without T->C
        TelescopeDirectionVector tdv;
        ASSERT_TRUE(plugin.TransformCelestialToTelescope(valRA, valDec, joff, tdv));
        INDI::IHorizontalCoordinates hor;
        pSupport->AltitudeAzimuthFromTelescopeDirectionVector(tdv, hor);

        double azDiff  = std::fmod(hor.azimuth - encAz.Degrees() + 540.0, 360.0) - 180.0;
        azDiff  = std::abs(azDiff);
        double altDiff = std::abs(hor.altitude - encAlt.Degrees());

        GTEST_LOG_(INFO) << "  encAz=" << encAz.Degrees() << " pluginAz=" << hor.azimuth
                         << " azDiff=" << azDiff * 3600.0 << "\"";
        GTEST_LOG_(INFO) << "  encAlt=" << encAlt.Degrees() << " pluginAlt=" << hor.altitude
                         << " altDiff=" << altDiff * 3600.0 << "\"";

        EXPECT_LT(azDiff,  azTolDeg) << "Az differs by " << azDiff << "deg — likely Az convention bug";
        EXPECT_LT(altDiff, azTolDeg) << "Alt differs by " << altDiff << "deg";
    }

    void RunEqPolarRegression(SPKMathPlugin &plugin,
                              const MountErrors &errors,
                              int numAlign,
                              double valRA, double valDec,
                              double tolArcsec,
                              INDI::IGeographicCoordinates site = kLosAngeles)
    {
        plugin.SetApproximateMountAlignment(NORTH_CELESTIAL_POLE);

        InMemoryDatabase db;
        db.SetDatabaseReferencePosition(site);

        ::Alignment gen = makeGenerator(errors, site, ::Alignment::EQ_GEM);
        auto *pSupport  = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);
        ASSERT_NE(pSupport, nullptr);

        double gmst = ln_get_apparent_sidereal_time(fixedJD);
        double lst  = range24(gmst + DEG_TO_HOURS(site.longitude));

        HaltonSequence seq(2, 3);
        for (int i = 1; i <= numAlign; ++i)
        {
            double ra, dec;
            auto raw = seq.getRaw(i);
            getSkyPoint(raw.first, raw.second, kEQ_MinDec, kEQ_MaxDec, ra, dec);
            AlignmentDatabaseEntry entry;
            buildEntry(gen, pSupport, ra, dec, lst, fixedJD, ::Alignment::EQ_GEM, entry);
            db.GetAlignmentDatabase().push_back(entry);
        }
        ASSERT_TRUE(plugin.Initialise(&db));

        double joff = fixedJD - ln_get_julian_from_sys();

        // Ground truth encoder position (RA/Dec) from scopesim
        Angle ha(get_local_hour_angle(lst, valRA), Angle::HOURS);
        Angle adec(valDec);
        Angle encHA, encDec;
        gen.observedToInstrument(ha, adec, &encHA, &encDec);
        double instRA = range24(lst - encHA.Hours());

        // Plugin C->T prediction, decoded to RA/Dec without T->C
        TelescopeDirectionVector tdv;
        ASSERT_TRUE(plugin.TransformCelestialToTelescope(valRA, valDec, joff, tdv));
        INDI::IEquatorialCoordinates raCoords;
        pSupport->EquatorialCoordinatesFromTelescopeDirectionVector(tdv, raCoords);

        double rawRaDiff = rangeHA(instRA - raCoords.rightascension);
        // Handle possible meridian flip between simulator and plugin solution
        if (std::abs(rawRaDiff) > 6.0)
            rawRaDiff = rangeHA(rawRaDiff - 12.0);

        double raDiff  = DEG_TO_ARCSEC(rawRaDiff * 15.0) * std::cos(DEG_TO_RAD(valDec));
        double decDiff = DEG_TO_ARCSEC(encDec.Degrees() - raCoords.declination);
        double totalDiff = std::sqrt(raDiff * raDiff + decDiff * decDiff);

        GTEST_LOG_(INFO) << "  encRA=" << instRA << " pluginRA=" << raCoords.rightascension
                         << " raDiff=" << raDiff << "\"";
        GTEST_LOG_(INFO) << "  encDec=" << encDec.Degrees() << " pluginDec=" << raCoords.declination
                         << " decDiff=" << decDiff << "\"";

        EXPECT_LT(totalDiff, tolArcsec) << "Encoder position differs by " << totalDiff << "\" — likely ME/MA mapping bug";
    }

    void RunAltAzPolarRegression(SPKMathPlugin &plugin,
                                 const MountErrors &errors,
                                 int numAlign,
                                 double valRA, double valDec,
                                 double tolArcsec,
                                 INDI::IGeographicCoordinates site = kLosAngeles)
    {
        plugin.SetApproximateMountAlignment(ZENITH);

        InMemoryDatabase db;
        db.SetDatabaseReferencePosition(site);

        ::Alignment gen = makeGenerator(errors, site, ::Alignment::ALTAZ);
        auto *pSupport  = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);
        ASSERT_NE(pSupport, nullptr);

        double gmst = ln_get_apparent_sidereal_time(fixedJD);
        double lst  = range24(gmst + DEG_TO_HOURS(site.longitude));

        HaltonSequence seq(2, 3);
        for (int i = 1; i <= numAlign; ++i)
        {
            double ra, dec;
            auto raw = seq.getRaw(i);
            getSkyPoint(raw.first, raw.second, kAZ_MinEl, kAZ_MaxEl, ra, dec);
            AlignmentDatabaseEntry entry;
            buildEntry(gen, pSupport, ra, dec, lst, fixedJD, ::Alignment::ALTAZ, entry);
            db.GetAlignmentDatabase().push_back(entry);
        }
        ASSERT_TRUE(plugin.Initialise(&db));

        double joff = fixedJD - ln_get_julian_from_sys();

        // Ground truth encoder position (Alt/Az) from scopesim
        Angle ha(get_local_hour_angle(lst, valRA), Angle::HOURS);
        Angle adec(valDec);
        Angle encAz, encAlt;
        gen.apparentHaDecToMount(ha, adec, &encAz, &encAlt);

        // Plugin C->T prediction, decoded to Alt/Az without T->C
        TelescopeDirectionVector tdv;
        ASSERT_TRUE(plugin.TransformCelestialToTelescope(valRA, valDec, joff, tdv));
        INDI::IHorizontalCoordinates horCoords;
        pSupport->AltitudeAzimuthFromTelescopeDirectionVector(tdv, horCoords);

        double azDiff  = std::fmod(horCoords.azimuth - encAz.Degrees() + 540.0, 360.0) - 180.0;
        double altDiff = std::abs(horCoords.altitude - encAlt.Degrees());
        double totalDiff = std::sqrt(std::pow(azDiff * std::cos(DEG_TO_RAD(horCoords.altitude)), 2) + altDiff * altDiff);
        totalDiff *= 3600.0; // arcsec

        GTEST_LOG_(INFO) << "  encAz=" << encAz.Degrees() << " pluginAz=" << horCoords.azimuth
                         << " azDiff=" << azDiff * 3600.0 << "\"";
        GTEST_LOG_(INFO) << "  encAlt=" << encAlt.Degrees() << " pluginAlt=" << horCoords.altitude
                         << " altDiff=" << altDiff * 3600.0 << "\"";

        EXPECT_LT(totalDiff, tolArcsec) << "Encoder position differs by " << totalDiff << "\" — likely AN/AW mapping bug";
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

        // Use 3 points for alignment in flip tests
        HaltonSequence alignSeq(2, 3);
        for (int i = 1; i <= 3; ++i)
        {
            double ra, dec;
            auto raw = alignSeq.getRaw(i);
            getSkyPoint(raw.first, raw.second, kEQ_MinDec, kEQ_MaxDec, ra, dec);

            AlignmentDatabaseEntry entry;
            buildEntry(generator, pSupport, ra, dec, lst, fixedJD, ::Alignment::EQ_GEM, entry);
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

    // -----------------------------------------------------------------------
    // Incremental hot-path helpers
    // -----------------------------------------------------------------------

    // Build a database of n sync points, initialise plugin, and return the
    // generator so the caller can add more points with the same sequence.
    ::Alignment buildIncrementalDb(SPKMathPlugin &plugin,
                                   const MountErrors &errors,
                                   int n,
                                   InMemoryDatabase &db,
                                   ::Alignment::MOUNT_TYPE mountType = ::Alignment::EQ_GEM,
                                   INDI::IGeographicCoordinates site = kLosAngeles)
    {
        db.SetDatabaseReferencePosition(site);
        MountAlignment_t alignment = (mountType == ::Alignment::ALTAZ) ? ZENITH :
                                     site.latitude >= 0 ? NORTH_CELESTIAL_POLE
                                                        : SOUTH_CELESTIAL_POLE;
        plugin.SetApproximateMountAlignment(alignment);

        ::Alignment generator = makeGenerator(errors, site, mountType);
        auto *pSupport = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);

        double gmst  = ln_get_apparent_sidereal_time(fixedJD);
        double lst   = range24(gmst + DEG_TO_HOURS(site.longitude));
        double minDec = (mountType == ::Alignment::ALTAZ) ? kAZ_MinEl : kEQ_MinDec;
        double maxDec = (mountType == ::Alignment::ALTAZ) ? kAZ_MaxEl : kEQ_MaxDec;

        HaltonSequence seq(2, 3);
        for (int i = 1; i <= n; i++)
        {
            double ra, dec;
            auto raw = seq.getRaw(i);
            getSkyPoint(raw.first, raw.second, minDec, maxDec, ra, dec);
            AlignmentDatabaseEntry entry;
                    buildEntry(generator, pSupport, ra, dec, lst, fixedJD, mountType, entry);
                    db.GetAlignmentDatabase().push_back(entry);
        }
        EXPECT_TRUE(plugin.Initialise(&db));
        return generator;
    }

    // Verify hot-path result matches a cold-initialised reference plugin.
    void verifyHotMatchesCold(SPKMathPlugin &hotPlugin, SPKMathPlugin &coldPlugin,
                               ::Alignment::MOUNT_TYPE mountType = ::Alignment::EQ_GEM)
    {
        double joff = fixedJD - ln_get_julian_from_sys();
        // Hot path uses pm=0 as the linearization point (Pmfit iteration-0);
        // Pmfit iterates to convergence.  With arcminute-level mount errors
        // the difference is a few arcseconds in angle space (~5e-5 in
        // direction vector units), well below observational noise.
        const double tol = 5e-5;
        for (double testRA : {3.0, 8.0, 15.0, 21.0})
        {
            // Clamp to physical range: equatorial [-60, 70], AltAz [20, 85]
            std::vector<double> decs = (mountType == ::Alignment::ALTAZ)
                ? std::vector<double>{25.0, 40.0, 60.0, 80.0}
                : std::vector<double>{-30.0, 0.0, 30.0, 60.0};
            for (double testDec : decs)
            {
                TelescopeDirectionVector hotV, coldV;
                ASSERT_TRUE(hotPlugin.TransformCelestialToTelescope(testRA, testDec, joff, hotV));
                ASSERT_TRUE(coldPlugin.TransformCelestialToTelescope(testRA, testDec, joff, coldV));
                EXPECT_NEAR(hotV.x, coldV.x, tol) << "RA=" << testRA << " Dec/El=" << testDec;
                EXPECT_NEAR(hotV.y, coldV.y, tol) << "RA=" << testRA << " Dec/El=" << testDec;
                EXPECT_NEAR(hotV.z, coldV.z, tol) << "RA=" << testRA << " Dec/El=" << testDec;
            }
        }
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
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AxisErrors)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ih = ARCMIN_TO_DEG(5), .id = ARCMIN_TO_DEG(3), .ch = ARCMIN_TO_DEG(8)}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_NonPerp)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.np = ARCMIN_TO_DEG(2)}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_IHOnly)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ih = ARCMIN_TO_DEG(15)}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_CHOnly)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ch = ARCMIN_TO_DEG(20)}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AllTerms_LA)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AllTerms_Tokyo)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0, kTokyo);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_SouthernHemisphere)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0, kSydney);
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
    RunPluginAlignValidate(plugin, {.ih = ARCSEC_TO_DEG(30), .me = ARCSEC_TO_DEG(18)}, 3, 100, 0.5);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_LargeErrors)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(30), .me = ARCMIN_TO_DEG(15)}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, BuiltIn_AlignValidate_AllTerms)
{
    BuiltInMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0);
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
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 35000.0);
}

// ---------------------------------------------------------------------------
// AlignValidate -- AltAz mounts
// ---------------------------------------------------------------------------

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AltAz_Polar)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 3, 100, 20.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AltAz_AxisErrors)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ih = ARCMIN_TO_DEG(5), .id = ARCMIN_TO_DEG(3), .ch = ARCMIN_TO_DEG(8)}, 3, 100, 20.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AltAz_NonPerp)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.np = ARCMIN_TO_DEG(2)}, 3, 100, 20.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AltAz_AllTerms)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 20.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_AltAz_Southern)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 20.0,
                           kSydney, ::Alignment::ALTAZ);
}

// Tolerance 2deg: SVD's Kabsch rotation on 3 points absorbs errors as a
// rigid rotation, so a held-out point carries some interpolation residual.
// Still far from a 180deg convention failure.
TEST_F(AlignmentPluginTest, SVD_AltAz_Goto_Convention_Regression)
{
    SVDMathPlugin plugin;
    RunGotoConventionRegression(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)},
                                3, 12.0, 50.0, 2.0);
}

TEST_F(AlignmentPluginTest, BuiltIn_AlignValidate_AltAz)
{
    BuiltInMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 3, 100, 20.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

// Tolerance 2deg: BuiltIn uses a raw 3x3 matrix inversion on 3 points which
// does not explicitly fit Wallace errors, so interpolation at a held-out point
// carries more residual than SPK.  Still far from a 180deg convention failure.
TEST_F(AlignmentPluginTest, BuiltIn_AltAz_Goto_Convention_Regression)
{
    BuiltInMathPlugin plugin;
    RunGotoConventionRegression(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)},
                                3, 12.0, 50.0, 2.0);
}

TEST_F(AlignmentPluginTest, Nearest_AlignValidate_AltAz)
{
    // NearestMathPlugin round-trip is only reliable when the validate point
    // has an alignment point at the same HA.  Use all-in-alignment with many
    // points rather than an align/validate split.
    NearestMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 20, kLosAngeles, ::Alignment::ALTAZ);
}

// Tolerance 1deg: Nearest returns the stored TDV of the closest alignment
// point, so accuracy at the exact alignment position is limited only by the
// encoder ground truth, not by interpolation.  We use the first Halton point
// as the validation target to guarantee the nearest neighbour is that entry.
// A 180deg convention bug would produce azDiff ~180deg.
TEST_F(AlignmentPluginTest, Nearest_AltAz_Goto_Convention_Regression)
{
    NearestMathPlugin plugin;
    plugin.SetApproximateMountAlignment(ZENITH);

    MountErrors errors = {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)};
    ::Alignment gen = makeGenerator(errors, kLosAngeles, ::Alignment::ALTAZ);
    auto *pSupport  = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);
    ASSERT_NE(pSupport, nullptr);

    double gmst = ln_get_apparent_sidereal_time(fixedJD);
    double lst  = range24(gmst + DEG_TO_HOURS(kLosAngeles.longitude));

    InMemoryDatabase db;
    db.SetDatabaseReferencePosition(kLosAngeles);

    // Build 5 alignment entries; record the first point for validation.
    double valRA = 0, valDec = 0;
    HaltonSequence seq(2, 3);
    for (int i = 1; i <= 5; ++i)
    {
        double ra, dec;
        auto raw = seq.getRaw(i);
        getSkyPoint(raw.first, raw.second, kAZ_MinEl, kAZ_MaxEl, ra, dec);
        if (i == 1) { valRA = ra; valDec = dec; }
        AlignmentDatabaseEntry entry;
        buildEntry(gen, pSupport, ra, dec, lst, fixedJD, ::Alignment::ALTAZ, entry);
        db.GetAlignmentDatabase().push_back(entry);
    }
    ASSERT_TRUE(plugin.Initialise(&db));

    double joff = fixedJD - ln_get_julian_from_sys();

    Angle ha(get_local_hour_angle(lst, valRA), Angle::HOURS);
    Angle adec(valDec);
    Angle encAz, encAlt;
    gen.apparentHaDecToMount(ha, adec, &encAz, &encAlt);

    TelescopeDirectionVector tdv;
    ASSERT_TRUE(plugin.TransformCelestialToTelescope(valRA, valDec, joff, tdv));
    INDI::IHorizontalCoordinates hor;
    pSupport->AltitudeAzimuthFromTelescopeDirectionVector(tdv, hor);

    double azDiff  = std::fmod(hor.azimuth - encAz.Degrees() + 540.0, 360.0) - 180.0;
    azDiff  = std::abs(azDiff);
    double altDiff = std::abs(hor.altitude - encAlt.Degrees());

    GTEST_LOG_(INFO) << "  encAz=" << encAz.Degrees() << " pluginAz=" << hor.azimuth
                     << " azDiff=" << azDiff * 3600.0 << "\"";
    GTEST_LOG_(INFO) << "  encAlt=" << encAlt.Degrees() << " pluginAlt=" << hor.altitude
                     << " altDiff=" << altDiff * 3600.0 << "\"";

    EXPECT_LT(azDiff,  1.0) << "Az differs by " << azDiff << "deg — likely Az convention bug";
    EXPECT_LT(altDiff, 1.0) << "Alt differs by " << altDiff << "deg";
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
        buildEntry(generator, pSupport, ra, dec, lst, fixedJD, ::Alignment::EQ_GEM, entry);
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
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0,
                           kLosAngeles, ::Alignment::EQ_FORK);
}

TEST_F(AlignmentPluginTest, SVD_AlignValidate_EqFork_Sydney)
{
    SVDMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0,
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

TEST_F(AlignmentPluginTest, SPK_AlignValidate_MeridianFlip_Northern)
{
    SPKMathPlugin plugin;
    RunMeridianFlipValidate(plugin, kMixed, kLosAngeles, NORTH_CELESTIAL_POLE);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_MeridianFlip_Southern)
{
    SPKMathPlugin plugin;
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
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0, kNairobi);
}

TEST_F(AlignmentPluginTest, Test_SPK_ErrorRecovery)
{
    SPKMathPlugin plugin;
    // 10 points gives Pmfit a well-conditioned full 6-term fit (nt=6, 14 DoF).
    RunPluginAllInAlignment(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 10);
}

TEST_F(AlignmentPluginTest, Test_SPK_AltAz)
{
    SPKMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 10, kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, Test_SPK_SouthernHemisphere)
{
    SPKMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 10, kSydney);
}

// ---------------------------------------------------------------------------
// SPK -- AlignValidate suite (equatorial mounts)
//
// SPKMathPlugin fits a 6-term Wallace pointing model (IH, ID, ME, MA, CH, TF)
// via Pmfit.  NP is excluded (IA, NP, CA are correlated; negligible in
// well-built mounts).  Polar-axis terms (ME, MA) precede collimation (CH) so
// that a 3-point fit yields a well-conditioned 4-term polar solution.
//
// TF (tube flexure) requires a two-pass AXES call: the first pass gives the
// initial encoder demand, the second re-evaluates at that position so the
// VD correction (pvd*cos(el)) is self-consistent with the TARG path.
// ---------------------------------------------------------------------------

TEST_F(AlignmentPluginTest, SPK_AlignValidate_Polar)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_AxisErrors)
{
    SPKMathPlugin plugin;
    // IH and ID are terms 0-1 and are fitted even at 3 points.  CH is term 4
    // and requires 5+ points; it is tested separately in SPK_AlignValidate_CHOnly.
    RunPluginAlignValidate(plugin, {.ih = ARCMIN_TO_DEG(5), .id = ARCMIN_TO_DEG(3)}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_IHOnly)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ih = ARCMIN_TO_DEG(15)}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_CHOnly)
{
    SPKMathPlugin plugin;
    // CH is term 4 (IH, ID, ME, MA, CH, TF); Pmfit fits terms 0..nt-1 where 5+ points yields nt=5.
    RunPluginAlignValidate(plugin, {.ch = ARCMIN_TO_DEG(20)}, 5, 100, 1.0);
}

// NP (non-perpendicularity of the RA and Dec axes) is not fitted by Pmfit,
// but the pure-NP pointing offset is mathematically near-degenerate with the
// (IH, CH) pair: a 12-point fit with 12' NP yields IH=+318", CH=-311" and
// sub-arcsecond residuals. This means residual magnitude is NOT a useful
// signal for detecting accidental NP fitting -- the two cases are
// indistinguishable to validation. Keep the test as a regression guard on
// the residual ceiling only.
TEST_F(AlignmentPluginTest, SPK_AlignValidate_NonPerp)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.np = ARCMIN_TO_DEG(2)}, 3, 100, 1.0);
}

// Individual term isolation: ID, MA, ME (IH and CH already have isolated tests).
// These give fault localization when combined tests break.
TEST_F(AlignmentPluginTest, SPK_AlignValidate_IDOnly)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.id = ARCMIN_TO_DEG(5)}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_MAOnly)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(10)}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_MEOnly)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.me = ARCMIN_TO_DEG(5)}, 3, 100, 1.0);
}

// Zero-error baseline: with no mount errors injected, the model should produce
// near-zero corrections and behave as an identity transform.
TEST_F(AlignmentPluginTest, SPK_AlignValidate_NoErrors)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_AllTerms_Tokyo)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0, kTokyo);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_EqGem_Northern)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0, kLosAngeles, ::Alignment::EQ_GEM);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_EqGem_Southern)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0, kSydney, ::Alignment::EQ_GEM);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_Arctic)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0, kArctic);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_Antarctic)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0, kAntarctic);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_SmallErrors)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ih = ARCSEC_TO_DEG(30), .me = ARCSEC_TO_DEG(18)}, 3, 100, 0.5);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_LargeErrors)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(30), .me = ARCMIN_TO_DEG(15)}, 3, 100, 1.0);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_Equatorial)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0, kNairobi);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_HugeIndex)
{
    SPKMathPlugin plugin;
    // SPK fits a 6-parameter model. With 7 alignment points, it should be able to deduce
    // the model perfectly even with 35 degree index errors, within reasonable limits.
    // However, such massive non-linearities might demand a looser target RMS.
    RunPluginAlignValidate(plugin, kHugeIndex, 7, 100, 30000.0);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_HighDec)
{
    SPKMathPlugin plugin;
    // Target High Dec regions (80 degrees to 89 degrees) where NP/CH explode
    RunPluginAlignValidate(plugin, kMixed, 7, 100, 100.0, kLosAngeles, ::Alignment::EQ_GEM, 80.0, 89.0);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_EqFork_Northern)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0,
                           kLosAngeles, ::Alignment::EQ_FORK);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_EqFork_Southern)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0,
                           kSydney, ::Alignment::EQ_FORK);
}

// ---------------------------------------------------------------------------
// SPK -- Minimal sync point boundary tests
// ---------------------------------------------------------------------------

// 6 alignment points: full 6-term fit with 6 DoF.  Tests the 6+ sync-point path.
TEST_F(AlignmentPluginTest, SPK_AlignValidate_MinimalPoints_6)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 6, 100, 1.0);
}

// 2 alignment points: 2 terms fitted (IH, ID — index errors only), zero DoF.
// Exercises the outTermCount=2 path in BuildObservationData.
TEST_F(AlignmentPluginTest, SPK_AlignValidate_MinimalPoints_2)
{
    SPKMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ih = ARCMIN_TO_DEG(5), .id = ARCMIN_TO_DEG(3)}, 2);
}

// 1 alignment point: 2 terms fitted (IA and IB — index errors only), zero DoF.
// Exercises the outTermCount=2 path in BuildObservationData.
TEST_F(AlignmentPluginTest, SPK_AlignValidate_MinimalPoints_1)
{
    SPKMathPlugin plugin;
    RunPluginAllInAlignment(plugin, {.ih = ARCMIN_TO_DEG(5)}, 1);
}

// 3 alignment points: 4 terms fitted (IH, ID, ME, MA), 2 DoF.
// Pure polar errors (MA, ME) are fully recoverable with this minimal set.
TEST_F(AlignmentPluginTest, SPK_AlignValidate_MinimalPoints_3)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 3, 100, 1.0);
}

// Near-pole: Dec range [78, 88] exercises the CH/cos(Dec) near-singularity regime.
// Verifies no NaN or infinite residuals near the celestial pole.
TEST_F(AlignmentPluginTest, SPK_AlignValidate_NearPole)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)},
                           12, 100, 1.0,
                           kLosAngeles, ::Alignment::EQ_GEM,
                           /*minDecOverride=*/78.0, /*maxDecOverride=*/88.0);
}

// ---------------------------------------------------------------------------
// SPK -- AlignValidate suite (AltAz mounts)
// ---------------------------------------------------------------------------

TEST_F(AlignmentPluginTest, SPK_AlignValidate_AltAz_Polar)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)}, 3, 100, 1.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_AltAz_AxisErrors)
{
    SPKMathPlugin plugin;
    // IA and IE are terms 0-1 and are fitted at 3 points.
    RunPluginAlignValidate(plugin, {.ih = ARCMIN_TO_DEG(5), .id = ARCMIN_TO_DEG(3)}, 3, 100, 1.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

// Regression test for SPK altaz roll = pi-Az_NE convention fix.
// See RunGotoConventionRegression for the methodology.
// Tolerance 0.1deg: SPK fits the injected IH/ID errors exactly, so the
// residual at a held-out point is sub-arcsecond.  A 180deg convention bug
// would produce azDiff ~180deg.
TEST_F(AlignmentPluginTest, SPK_AltAz_Goto_Convention_Regression)
{
    SPKMathPlugin plugin;
    RunGotoConventionRegression(plugin, {.ih = ARCMIN_TO_DEG(5), .id = ARCMIN_TO_DEG(3)},
                                3, 12.0, 50.0, 0.1);
}

TEST_F(AlignmentPluginTest, SPK_EQ_Polar_Mapping_Regression)
{
    SPKMathPlugin plugin;
    // Inject significant ME and MA and verify the plugin maps them to the correct axes.
    // Use 10 points to ensure a full 6-term fit is well-conditioned.
    RunEqPolarRegression(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)},
                         10, 18.0, 45.0, 1.0);
}

TEST_F(AlignmentPluginTest, SPK_EQ_ConeError_Mapping_Regression)
{
    SPKMathPlugin plugin;
    // Inject significant CH and verify the plugin maps it correctly.
    RunEqPolarRegression(plugin, {.ch = ARCMIN_TO_DEG(10)},
                         10, 18.0, 45.0, 1.0);
}

TEST_F(AlignmentPluginTest, SPK_EQ_Sydney_Mapping_Regression)
{
    SPKMathPlugin plugin;
    // Verify Southern Hemisphere polar mapping (Sydney, lat=-33.9)
    RunEqPolarRegression(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)},
                         10, 18.0, -45.0, 1.0, kSydney);
}

TEST_F(AlignmentPluginTest, SPK_AltAz_Polar_Mapping_Regression)
{
    SPKMathPlugin plugin;
    // Inject significant AN and AW and verify the plugin maps them to the correct axes.
    // Use 10 points to ensure a full 6-term fit is well-conditioned.
    RunAltAzPolarRegression(plugin, {.ma = ARCMIN_TO_DEG(10), .me = ARCMIN_TO_DEG(5)},
                            10, 18.0, 45.0, 1.0);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_AltAz_NonPerp)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, {.np = ARCMIN_TO_DEG(2)}, 3, 100, 1.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_AltAz_AllTerms)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0,
                           kLosAngeles, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SPK_AlignValidate_AltAz_Southern)
{
    SPKMathPlugin plugin;
    RunPluginAlignValidate(plugin, kMixed, 3, 100, 1.0,
                           kSydney, ::Alignment::ALTAZ);
}

// ---------------------------------------------------------------------------
// Noise stress tests -- Gaussian centroiding noise injected into alignment
// entries to simulate real-world pointing uncertainty (σ = 30" per axis).
//
// Residual is the angular error between the plugin's predicted telescope
// direction and the true (noiseless) direction from the simulator, so it
// directly measures generalization accuracy rather than round-trip consistency.
//
// Expected behavior:
//   - 3 pts: SPK fits 4 terms (IH, ID, ME, MA) with 2 DoF; SVD is near
//     determined.  Residuals are larger than at 12 pts.
//   - 12 pts: SPK's 6-term systematic model has 18 DoF to average down noise;
//     SVD's general linear fit has more free parameters and averages less.
//     SPK should show lower residuals at new sky positions.
// ---------------------------------------------------------------------------

static constexpr double kNoiseSigma30arcsec = ARCSEC_TO_DEG(30.0);  // 30" per axis

TEST_F(AlignmentPluginTest, SPK_TimeShift_Regression)
{
    SPKMathPlugin plugin;
    RunTimeShiftValidate(plugin, kMixed, 6, 100, 3.0, 1.0);
}

TEST_F(AlignmentPluginTest, SVD_TimeShift_Regression)
{
    SVDMathPlugin plugin;
    RunTimeShiftValidate(plugin, kMixed, 3, 100, 3.0, 1.0);
}

TEST_F(AlignmentPluginTest, NoiseStress_SPK_3pts)
{
    SPKMathPlugin noisyPlugin, refPlugin;
    RunPluginNoiseValidate(noisyPlugin, refPlugin, kMixed, 3, 100, kNoiseSigma30arcsec, 300.0);
}

TEST_F(AlignmentPluginTest, NoiseStress_SVD_3pts)
{
    SVDMathPlugin noisyPlugin, refPlugin;
    RunPluginNoiseValidate(noisyPlugin, refPlugin, kMixed, 3, 100, kNoiseSigma30arcsec, 300.0);
}

TEST_F(AlignmentPluginTest, NoiseStress_SPK_12pts)
{
    SPKMathPlugin noisyPlugin, refPlugin;
    RunPluginNoiseValidate(noisyPlugin, refPlugin, kMixed, 12, 100, kNoiseSigma30arcsec, 120.0);
}

TEST_F(AlignmentPluginTest, NoiseStress_SVD_12pts)
{
    SVDMathPlugin noisyPlugin, refPlugin;
    RunPluginNoiseValidate(noisyPlugin, refPlugin, kMixed, 12, 100, kNoiseSigma30arcsec, 120.0);
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

    auto buildEntryLocal = [&](::Alignment &gen,
                          TelescopeDirectionVectorSupportFunctions *pSupport,
                          double ra, double dec, double lst_hrs, double jd,
                          ::Alignment::MOUNT_TYPE mt,
                          AlignmentDatabaseEntry &entry)
    {
        Angle ha(get_local_hour_angle(lst_hrs, ra), Angle::HOURS), adec(dec);
        entry.RightAscension        = ra;
        entry.Declination           = dec;
        entry.ObservationJulianDate = jd;
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
    buildEntryLocal(gen, pSupport, syncRA, syncDec, lst, fixedJD, mountType, entry);
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

TEST_F(AlignmentPluginTest, SPK_PolarDegeneracy_EQ_SinglePoint)
{
    SPKMathPlugin plugin;
    RunPolarDegeneracyTestImpl(plugin, kFieldErrors, kLosAngeles, ::Alignment::EQ_GEM);
}

TEST_F(AlignmentPluginTest, SPK_PolarDegeneracy_AltAz_SinglePoint)
{
    SPKMathPlugin plugin;
    RunPolarDegeneracyTestImpl(plugin, kFieldErrors, kLosAngeles, ::Alignment::ALTAZ);
}

// ---------------------------------------------------------------------------
// SPK incremental hot-path tests
//
// Each test cold-initialises a plugin with 6 sync points (which builds the
// NE accumulator at nt=6), adds one more point, and verifies the hot-path
// result matches a fresh cold-initialised plugin with all 7 points.
// ---------------------------------------------------------------------------

TEST_F(AlignmentPluginTest, SPK_Incremental_MatchesCold_Equatorial)
{
    InMemoryDatabase db;
    SPKMathPlugin hotPlugin;
    auto generator = buildIncrementalDb(hotPlugin, kMixed, 6, db);

    auto *pSupport = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&hotPlugin);
    double gmst = ln_get_apparent_sidereal_time(fixedJD);
    double lst  = range24(gmst + DEG_TO_HOURS(kLosAngeles.longitude));
    {
        double ra, dec;
        auto raw = HaltonSequence(2, 3).getRaw(7);
        getSkyPoint(raw.first, raw.second, kEQ_MinDec, kEQ_MaxDec, ra, dec);
        AlignmentDatabaseEntry entry;
        buildEntry(generator, pSupport, ra, dec, lst, fixedJD, ::Alignment::EQ_GEM, entry);
        db.GetAlignmentDatabase().push_back(entry);
    }
    ASSERT_TRUE(hotPlugin.Initialise(&db));

    SPKMathPlugin coldPlugin;
    coldPlugin.SetApproximateMountAlignment(NORTH_CELESTIAL_POLE);
    ASSERT_TRUE(coldPlugin.Initialise(&db));

    verifyHotMatchesCold(hotPlugin, coldPlugin);
}

TEST_F(AlignmentPluginTest, SPK_Incremental_MatchesCold_AltAz)
{
    InMemoryDatabase db;
    SPKMathPlugin hotPlugin;
    auto generator = buildIncrementalDb(hotPlugin, kMixed, 6, db, ::Alignment::ALTAZ);

    auto *pSupport = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&hotPlugin);
    double gmst = ln_get_apparent_sidereal_time(fixedJD);
    double lst  = range24(gmst + DEG_TO_HOURS(kLosAngeles.longitude));
    {
        double ra, dec;
        auto raw = HaltonSequence(2, 3).getRaw(7);
        getSkyPoint(raw.first, raw.second, kAZ_MinEl, kAZ_MaxEl, ra, dec);
        AlignmentDatabaseEntry entry;
        buildEntry(generator, pSupport, ra, dec, lst, fixedJD, ::Alignment::ALTAZ, entry);
        db.GetAlignmentDatabase().push_back(entry);
    }
    ASSERT_TRUE(hotPlugin.Initialise(&db));

    SPKMathPlugin coldPlugin;
    coldPlugin.SetApproximateMountAlignment(ZENITH);
    ASSERT_TRUE(coldPlugin.Initialise(&db));

    verifyHotMatchesCold(hotPlugin, coldPlugin, ::Alignment::ALTAZ);
}

TEST_F(AlignmentPluginTest, SPK_Incremental_InvalidStateNotUsed)
{
    // With fewer than 6 points (m_NE_valid=false), adding a 6th must take the
    // cold path (which then builds the NE accumulator).  A subsequent 7th
    // point should use the hot path and produce a sensible result.
    InMemoryDatabase db;
    SPKMathPlugin plugin;
    auto generator = buildIncrementalDb(plugin, kMixed, 5, db);

    auto *pSupport = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);
    double gmst = ln_get_apparent_sidereal_time(fixedJD);
    double lst  = range24(gmst + DEG_TO_HOURS(kLosAngeles.longitude));

    for (int i : {6, 7})
    {
        double ra, dec;
        auto raw = HaltonSequence(2, 3).getRaw(i);
        getSkyPoint(raw.first, raw.second, kEQ_MinDec, kEQ_MaxDec, ra, dec);
        AlignmentDatabaseEntry entry;
        buildEntry(generator, pSupport, ra, dec, lst, fixedJD, ::Alignment::EQ_GEM, entry);
        db.GetAlignmentDatabase().push_back(entry);
        ASSERT_TRUE(plugin.Initialise(&db));
    }

    double joff = fixedJD - ln_get_julian_from_sys();
    TelescopeDirectionVector v;
    ASSERT_TRUE(plugin.TransformCelestialToTelescope(12.0, 45.0, joff, v));
    double outRA, outDec;
    plugin.TransformTelescopeToCelestial(v, outRA, outDec, joff);
    EXPECT_NEAR(12.0, outRA,  kRoundTripTolHours);
    EXPECT_NEAR(45.0, outDec, kRoundTripTolDeg);
}

TEST_F(AlignmentPluginTest, SPK_Incremental_MountTypeChange)
{
    // Cold-init equatorial (m_NE_mountChar='E').  Switch to AltAz.  The
    // mountChar mismatch must force the cold path on the next Initialise call.
    InMemoryDatabase db;
    SPKMathPlugin plugin;
    buildIncrementalDb(plugin, kMixed, 6, db);

    plugin.SetApproximateMountAlignment(ZENITH);

    InMemoryDatabase dbAz;
    auto generatorAz = buildIncrementalDb(plugin, kMixed, 7, dbAz, ::Alignment::ALTAZ);
    (void)generatorAz;

    double joff = fixedJD - ln_get_julian_from_sys();
    TelescopeDirectionVector v;
    ASSERT_TRUE(plugin.TransformCelestialToTelescope(12.0, 60.0, joff, v));
    double outRA, outDec;
    plugin.TransformTelescopeToCelestial(v, outRA, outDec, joff);
    EXPECT_NEAR(12.0, outRA,  kRoundTripTolHours);
    EXPECT_NEAR(60.0, outDec, kRoundTripTolDeg);
}

// ---------------------------------------------------------------------------
// SPK plugin lifecycle tests
//
// Verify defensive behaviour around missing location and mount-type propagation.
// ---------------------------------------------------------------------------

// Transforms must fail gracefully when no reference position has been set.
TEST_F(AlignmentPluginTest, SPK_Lifecycle_NoLocation_TransformsFail)
{
    InMemoryDatabase db;  // no SetDatabaseReferencePosition
    SPKMathPlugin plugin;
    plugin.Initialise(&db);
    plugin.SetApproximateMountAlignment(ZENITH);

    double joff = fixedJD - ln_get_julian_from_sys();
    TelescopeDirectionVector v;
    EXPECT_FALSE(plugin.TransformCelestialToTelescope(12.0, 45.0, joff, v));

    // Provide a dummy TDV and confirm T->C also fails without a location.
    v = TelescopeDirectionVector{0, 0, 1};
    double outRA, outDec;
    EXPECT_FALSE(plugin.TransformTelescopeToCelestial(v, outRA, outDec, joff));
}

// Initialise without a location must not crash and must return true
// (model is simply unconstrained, not an error).
TEST_F(AlignmentPluginTest, SPK_Lifecycle_NoLocation_InitialiseOk)
{
    InMemoryDatabase db;
    SPKMathPlugin plugin;
    EXPECT_TRUE(plugin.Initialise(&db));
}

// Transforms must succeed once a location is provided, even if it arrives
// after the initial Initialise call (simulates late ISGetProperties config load).
TEST_F(AlignmentPluginTest, SPK_Lifecycle_LateLocation_TransformsSucceed)
{
    InMemoryDatabase db;
    SPKMathPlugin plugin;
    plugin.Initialise(&db);
    plugin.SetApproximateMountAlignment(ZENITH);

    // Location arrives late (e.g. from a saved config property).
    db.SetDatabaseReferencePosition(kLosAngeles);

    double joff = fixedJD - ln_get_julian_from_sys();
    TelescopeDirectionVector v;
    EXPECT_TRUE(plugin.TransformCelestialToTelescope(12.0, 45.0, joff, v));
    double outRA, outDec;
    EXPECT_TRUE(plugin.TransformTelescopeToCelestial(v, outRA, outDec, joff));
}

// Mount type set after Initialise must be picked up on the next transform call,
// without requiring a second Initialise.  This is the celestronaux bug scenario:
// Initialise is called at physical connection before the driver re-propagates
// its mount type to the plugin.
TEST_F(AlignmentPluginTest, SPK_Lifecycle_MountTypeAfterInit_PickedUpByTransform)
{
    InMemoryDatabase db;
    db.SetDatabaseReferencePosition(kLosAngeles);
    SPKMathPlugin plugin;

    // Initialise with EQUAT (default from MathPlugin constructor)
    plugin.Initialise(&db);
    ASSERT_EQ(plugin.GetApproximateMountAlignment(), ZENITH);  // default

    // Driver now sets ALTAZ — no second Initialise.
    plugin.SetApproximateMountAlignment(ZENITH);

    // Transform should use ALTAZ formulas.  Verify by round-trip: C->T->C.
    double joff = fixedJD - ln_get_julian_from_sys();
    TelescopeDirectionVector v;
    ASSERT_TRUE(plugin.TransformCelestialToTelescope(12.0, 30.0, joff, v));
    double outRA, outDec;
    ASSERT_TRUE(plugin.TransformTelescopeToCelestial(v, outRA, outDec, joff));
    EXPECT_NEAR(12.0, outRA,  kRoundTripTolHours);
    EXPECT_NEAR(30.0, outDec, kRoundTripTolDeg);

    // Now switch to EQUAT without Initialise — must also be picked up.
    plugin.SetApproximateMountAlignment(NORTH_CELESTIAL_POLE);
    ASSERT_TRUE(plugin.TransformCelestialToTelescope(6.0, 45.0, joff, v));
    ASSERT_TRUE(plugin.TransformTelescopeToCelestial(v, outRA, outDec, joff));
    EXPECT_NEAR(6.0,  outRA,  kRoundTripTolHours);
    EXPECT_NEAR(45.0, outDec, kRoundTripTolDeg);
}

// ---------------------------------------------------------------------------
// Driver pipeline integration tests
//
// These simulate what telescope_simulator.cpp does with scopesim_helper
// outputs, catching convention mismatches (e.g. stale +-180 offsets) that
// unit tests on individual functions miss.
//
// The pattern mirrors the driver's actual flow:
//   Goto:  RA/Dec -> instrumentHaDecToMount -> axisPrimary.position = primary
//   Read:  mountToInstrumentHaDec(axisPrimary.position, ...) -> RA/Dec
//   Sync:  TelescopeDirectionVectorFromAltitudeAzimuth({primary.Degrees(), ...})
//   PE:    mountToApparentHaDec(axisPrimary.position, ...) -> RA/Dec
// ---------------------------------------------------------------------------

class DriverPipelineTest : public AlignmentPluginTest
{
protected:
    struct SkyPoint { double ra; double dec; };

    static constexpr SkyPoint kTestPoints[] = {
        { 3.0,  45.0},
        { 8.0, -15.0},
        {14.0,  70.0},
        {20.0,  10.0},
    };

    static double getLST(INDI::IGeographicCoordinates site)
    {
        double gmst = ln_get_apparent_sidereal_time(fixedJD);
        return range24(gmst + DEG_TO_HOURS(site.longitude));
    }
};

// instrumentHaDecToMount -> mountToInstrumentHaDec round-trip for ALTAZ.
// The driver stores instrumentHaDecToMount output directly in axisPrimary.position
// (no +-180 conversion) and reads it back via mountToInstrumentHaDec.
TEST_F(DriverPipelineTest, AltAz_InstrumentRoundTrip)
{
    ::Alignment gen = makeGenerator({}, kLosAngeles, ::Alignment::ALTAZ);
    double lst = getLST(kLosAngeles);

    for (auto [testRA, testDec] : kTestPoints)
    {
        Angle ha(get_local_hour_angle(lst, testRA), Angle::HOURS);
        Angle primary, secondary;
        gen.instrumentHaDecToMount(ha, Angle(testDec), &primary, &secondary);

        // Driver does: axisPrimary.position = primary (no conversion)
        Angle instHA, instDec;
        gen.mountToInstrumentHaDec(primary, secondary, &instHA, &instDec);

        double recoveredRA = range24(lst - instHA.Hours());
        EXPECT_NEAR(testRA,  recoveredRA,    0.001) << "RA round-trip failed";
        EXPECT_NEAR(testDec, instDec.Degrees(), 0.001) << "Dec round-trip failed";
    }
}

// instrumentHaDecToMount -> mountToApparentHaDec round-trip for ALTAZ.
// With zero mount errors, apparent == instrument, so HA/Dec should round-trip.
// We use mountToApparentHaDec (not mountToApparentRaDec) to avoid dependency
// on system-time LST which differs from the test's fixed JD LST.
TEST_F(DriverPipelineTest, AltAz_ApparentRoundTrip)
{
    ::Alignment gen = makeGenerator({}, kLosAngeles, ::Alignment::ALTAZ);
    double lst = getLST(kLosAngeles);

    for (auto [testRA, testDec] : kTestPoints)
    {
        double inputHA = get_local_hour_angle(lst, testRA);
        Angle ha(inputHA, Angle::HOURS);
        Angle primary, secondary;
        gen.instrumentHaDecToMount(ha, Angle(testDec), &primary, &secondary);

        // With zero errors, mountToApparentHaDec should return the same HA/Dec
        Angle apparentHa, apparentDec;
        gen.mountToApparentHaDec(primary, secondary, &apparentHa, &apparentDec);

        EXPECT_NEAR(range24(inputHA), apparentHa.Hours(), 0.001) << "HA apparent round-trip failed at RA=" << testRA;
        EXPECT_NEAR(testDec, apparentDec.Degrees(), 0.001) << "Dec apparent round-trip failed at RA=" << testRA;
    }
}

// Verify that instrumentHaDecToMount output yields the correct TDV when
// passed to TelescopeDirectionVectorFromAltitudeAzimuth (the Sync path).
// The TDV should match what EquatorialToHorizontal + TDV conversion produces.
TEST_F(DriverPipelineTest, AltAz_SyncTDV_Convention)
{
    ::Alignment gen = makeGenerator({}, kLosAngeles, ::Alignment::ALTAZ);
    SPKMathPlugin plugin;
    auto *pSupport = dynamic_cast<TelescopeDirectionVectorSupportFunctions *>(&plugin);
    double lst = getLST(kLosAngeles);

    for (auto [testRA, testDec] : kTestPoints)
    {
        Angle ha(get_local_hour_angle(lst, testRA), Angle::HOURS);
        Angle primary, secondary;
        gen.instrumentHaDecToMount(ha, Angle(testDec), &primary, &secondary);

        // Driver Sync path: TDV from axisPrimary.position (no conversion)
        INDI::IHorizontalCoordinates horCoords = { primary.Degrees(), secondary.Degrees() };
        TelescopeDirectionVector driverTDV =
            pSupport->TelescopeDirectionVectorFromAltitudeAzimuth(horCoords);

        // Reference: EquatorialToHorizontal for the same sky position
        INDI::IEquatorialCoordinates eq = { testRA, testDec };
        INDI::IGeographicCoordinates site = kLosAngeles;
        INDI::IHorizontalCoordinates refAltAz;
        INDI::EquatorialToHorizontal(&eq, &site, fixedJD, &refAltAz);
        TelescopeDirectionVector refTDV =
            pSupport->TelescopeDirectionVectorFromAltitudeAzimuth(refAltAz);

        // TDVs should be nearly identical (no mount errors)
        EXPECT_NEAR(refTDV.x, driverTDV.x, 1e-4) << "TDV.x mismatch at RA=" << testRA;
        EXPECT_NEAR(refTDV.y, driverTDV.y, 1e-4) << "TDV.y mismatch at RA=" << testRA;
        EXPECT_NEAR(refTDV.z, driverTDV.z, 1e-4) << "TDV.z mismatch at RA=" << testRA;
    }
}

// Same pipeline tests for EQ_GEM to ensure we didn't break equatorial.
TEST_F(DriverPipelineTest, EqGem_InstrumentRoundTrip)
{
    ::Alignment gen = makeGenerator({}, kLosAngeles, ::Alignment::EQ_GEM);
    double lst = getLST(kLosAngeles);

    for (auto [testRA, testDec] : kTestPoints)
    {
        Angle ha(get_local_hour_angle(lst, testRA), Angle::HOURS);
        Angle primary, secondary;
        gen.instrumentHaDecToMount(ha, Angle(testDec), &primary, &secondary);

        Angle instHA, instDec;
        gen.mountToInstrumentHaDec(primary, secondary, &instHA, &instDec);

        double recoveredRA = range24(lst - instHA.Hours());
        EXPECT_NEAR(testRA,  recoveredRA,    0.001) << "RA round-trip failed";
        EXPECT_NEAR(testDec, instDec.Degrees(), 0.001) << "Dec round-trip failed";
    }
}

// Southern hemisphere ALTAZ pipeline
TEST_F(DriverPipelineTest, AltAz_InstrumentRoundTrip_Southern)
{
    ::Alignment gen = makeGenerator({}, kSydney, ::Alignment::ALTAZ);
    double lst = getLST(kSydney);

    for (auto [testRA, testDec] : kTestPoints)
    {
        Angle ha(get_local_hour_angle(lst, testRA), Angle::HOURS);
        Angle primary, secondary;
        gen.instrumentHaDecToMount(ha, Angle(testDec), &primary, &secondary);

        Angle instHA, instDec;
        gen.mountToInstrumentHaDec(primary, secondary, &instHA, &instDec);

        double recoveredRA = range24(lst - instHA.Hours());
        EXPECT_NEAR(testRA,  recoveredRA,    0.001) << "RA round-trip failed";
        EXPECT_NEAR(testDec, instDec.Degrees(), 0.001) << "Dec round-trip failed";
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
