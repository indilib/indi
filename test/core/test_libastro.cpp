/*******************************************************************************
 * Tests for libastro coordinate functions (libnova backend)
 *
 * Tests for the libastro coordinate pipeline against external truth (IMCCE Miriade)
 * and internal round-trip consistency.
 *
 * Golden data: test/data/horizontal_golden.json
 *   8 cases: Vega and Polaris at Greenwich, Mitaka, Mauna Kea, Siding Spring.
 *   Source: IMCCE Miriade (INPOP19), -tcoor=5 (horizontal coordinates).
 *   Regenerate with: python3 tools/generate_golden_files.py
 *******************************************************************************/

#include <gtest/gtest.h>
#include <libastro.h>
#ifdef _USE_SYSTEM_JSONLIB
#include <indijson.hpp>
#else
#include <indijson.hpp>
#endif
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

// ---------------------------------------------------------------------------
// Golden data loader
// ---------------------------------------------------------------------------

struct HorizCase {
    std::string label;
    std::string object;
    double ra_j2000_h;
    double dec_j2000_deg;
    double jd;
    std::string site;
    double lon_deg;
    double lat_deg;
    double elev_m;
    double az_truth;
    double alt_truth;
};

static std::vector<HorizCase> load_golden()
{
    std::ifstream f(TEST_DATA_DIR "/horizontal_golden.json");
    if (!f.is_open()) return {};
    nlohmann::json j = nlohmann::json::parse(f);
    std::vector<HorizCase> cases;
    for (auto &e : j) {
        if (!e.contains("az_deg")) continue;
        HorizCase c;
        c.label         = e["label"];
        c.object        = e["object"];
        c.ra_j2000_h    = e["ra_j2000_h"];
        c.dec_j2000_deg = e["dec_j2000_deg"];
        c.jd            = e["jd"];
        c.site          = e["site"];
        c.lon_deg       = e["lon_deg"];
        c.lat_deg       = e["lat_deg"];
        c.elev_m        = e["elev_m"];
        c.az_truth      = e["az_deg"];
        c.alt_truth     = e["alt_deg"];
        cases.push_back(c);
    }
    return cases;
}

static double az_diff_arcsec(double a, double b) {
    double d = a - b;
    while (d >  180.0) d -= 360.0;
    while (d < -180.0) d += 360.0;
    return std::abs(d) * 3600.0;
}

// ---------------------------------------------------------------------------
// J2000toObserved -> ObservedToJ2000 round-trip
// The libnova forward and inverse paths must close to < 1 arcsecond.
// A large error here indicates a broken nutation or precession inversion.
// ---------------------------------------------------------------------------
TEST(Libastro, Reciprocity)
{
    // Use two different epochs to exercise different precession amounts.
    struct { double jd; const char *label; } epochs[] = {
        { 2459019.833333, "2020-06-19" },
        { 2461112.5,      "2026-01-01" },
    };
    INDI::IEquatorialCoordinates j2000_in = { 20.69053168, 45.28033881 }; // Deneb

    for (auto &ep : epochs) {
        INDI::IEquatorialCoordinates jnow, j2000_out;
        INDI::J2000toObserved(&j2000_in, ep.jd, &jnow);
        INDI::ObservedToJ2000(&jnow, ep.jd, &j2000_out);

        double cos_dec = std::cos(j2000_in.declination * M_PI / 180.0);
        double ra_err  = std::abs(j2000_in.rightascension - j2000_out.rightascension) * 15.0 * 3600.0 * cos_dec;
        double dec_err = std::abs(j2000_in.declination    - j2000_out.declination)    * 3600.0;

        GTEST_LOG_(INFO) << ep.label << " round-trip: RA=" << ra_err << "\"  Dec=" << dec_err << "\"";
        EXPECT_LT(ra_err,  1.0) << "Round-trip RA error at " << ep.label;
        EXPECT_LT(dec_err, 1.0) << "Round-trip Dec error at " << ep.label;
    }
}

// ---------------------------------------------------------------------------
// EquatorialToHorizontal accuracy vs IMCCE Miriade (INPOP19) truth.
// Per-case errors are logged; the assertion catches gross regressions
// (wrong observer, wrong sidereal time, etc.) which cause degree-level errors.
// ---------------------------------------------------------------------------
TEST(Libastro, HorizontalAccuracy_vs_IMCCE)
{
    static constexpr double TOLERANCE_ARCSEC = 60.0;

    auto cases = load_golden();
    ASSERT_GT(cases.size(), 0u) << "Could not load horizontal_golden.json";

    double max_az_err = 0, max_alt_err = 0;
    int n = 0;
    for (auto &c : cases) {
        INDI::IEquatorialCoordinates j2000 = { c.ra_j2000_h, c.dec_j2000_deg };
        INDI::IEquatorialCoordinates jnow;
        INDI::J2000toObserved(&j2000, c.jd, &jnow);

        INDI::IGeographicCoordinates obs = { c.lon_deg, c.lat_deg, c.elev_m };
        INDI::IHorizontalCoordinates hrz;
        INDI::EquatorialToHorizontal(&jnow, &obs, c.jd, &hrz);

        double az_err  = az_diff_arcsec(hrz.azimuth,  c.az_truth);
        double alt_err = std::abs(hrz.altitude - c.alt_truth) * 3600.0;
        max_az_err  = std::max(max_az_err,  az_err);
        max_alt_err = std::max(max_alt_err, alt_err);

        GTEST_LOG_(INFO) << c.object << " at " << c.site
                         << "  az_err=" << az_err << "\"  alt_err=" << alt_err << "\"";
        EXPECT_LT(az_err,  TOLERANCE_ARCSEC)
            << "Az error for " << c.object << " at " << c.site
            << ": " << az_err << "\" (truth=" << c.az_truth << " got=" << hrz.azimuth << ")";
        EXPECT_LT(alt_err, TOLERANCE_ARCSEC)
            << "Alt error for " << c.object << " at " << c.site
            << ": " << alt_err << "\" (truth=" << c.alt_truth << " got=" << hrz.altitude << ")";
        n++;
    }
    GTEST_LOG_(INFO) << "Horizontal accuracy (" << n << " cases):"
                     << " max az_err=" << max_az_err << "\""
                     << " max alt_err=" << max_alt_err << "\"";
}

// ---------------------------------------------------------------------------
// Structural: observer longitude must change altitude significantly.
// If EquatorialToHorizontal ignores the observer position, Vega has the same
// altitude at all sites. Sites span ~316 deg of longitude so the spread must
// exceed 30 deg.
// ---------------------------------------------------------------------------
TEST(Libastro, ObserverLongitudeMatters)
{
    auto cases = load_golden();
    ASSERT_GT(cases.size(), 0u);

    double alt_min = 999.0, alt_max = -999.0;
    int vega_count = 0;
    for (auto &c : cases) {
        if (c.object != "Vega") continue;
        INDI::IEquatorialCoordinates j2000 = { c.ra_j2000_h, c.dec_j2000_deg };
        INDI::IEquatorialCoordinates jnow;
        INDI::J2000toObserved(&j2000, c.jd, &jnow);
        INDI::IGeographicCoordinates obs = { c.lon_deg, c.lat_deg, c.elev_m };
        INDI::IHorizontalCoordinates hrz;
        INDI::EquatorialToHorizontal(&jnow, &obs, c.jd, &hrz);
        alt_min = std::min(alt_min, hrz.altitude);
        alt_max = std::max(alt_max, hrz.altitude);
        vega_count++;
    }
    ASSERT_GE(vega_count, 3) << "Need at least 3 Vega cases";
    EXPECT_GT(alt_max - alt_min, 30.0)
        << "Vega altitude spread=" << (alt_max - alt_min)
        << " deg — expected >30 deg. Observer longitude may be ignored.";
    GTEST_LOG_(INFO) << "Vega altitude spread: " << alt_min << " to " << alt_max
                     << " deg (spread=" << (alt_max - alt_min) << " deg)";
}

// ---------------------------------------------------------------------------
// Structural: observer latitude must determine whether Polaris is above the
// horizon. Polaris is circumpolar from Greenwich (lat 51.5 N) and below the
// horizon from Siding Spring (lat 31.3 S). If latitude is ignored, the sign
// of the altitude is wrong at one or both sites.
// ---------------------------------------------------------------------------
TEST(Libastro, ObserverLatitudeMatters)
{
    auto cases = load_golden();
    ASSERT_GT(cases.size(), 0u);

    double alt_greenwich = 999.0, alt_siding = 999.0;
    for (auto &c : cases) {
        if (c.object != "Polaris") continue;
        INDI::IEquatorialCoordinates j2000 = { c.ra_j2000_h, c.dec_j2000_deg };
        INDI::IEquatorialCoordinates jnow;
        INDI::J2000toObserved(&j2000, c.jd, &jnow);
        INDI::IGeographicCoordinates obs = { c.lon_deg, c.lat_deg, c.elev_m };
        INDI::IHorizontalCoordinates hrz;
        INDI::EquatorialToHorizontal(&jnow, &obs, c.jd, &hrz);
        if (c.site == "Greenwich")     alt_greenwich = hrz.altitude;
        if (c.site == "Siding Spring") alt_siding    = hrz.altitude;
    }
    ASSERT_NE(alt_greenwich, 999.0) << "Greenwich Polaris case missing";
    ASSERT_NE(alt_siding,    999.0) << "Siding Spring Polaris case missing";

    EXPECT_GT(alt_greenwich, 0.0)
        << "Polaris should be above horizon at Greenwich (got " << alt_greenwich << " deg)";
    EXPECT_LT(alt_siding, 0.0)
        << "Polaris should be below horizon at Siding Spring (got " << alt_siding << " deg)";
    GTEST_LOG_(INFO) << "Polaris: Greenwich=" << alt_greenwich
                     << " deg, Siding Spring=" << alt_siding << " deg";
}

// ---------------------------------------------------------------------------
// Round-trip: EquatorialToHorizontal -> HorizontalToEquatorial must recover
// the original JNow coordinates to floating-point precision (sub-arcsecond).
// Skips cases below 5 deg altitude where spherical trig is less stable.
// ---------------------------------------------------------------------------
TEST(Libastro, RoundTrip_HorizontalToEquatorial)
{
    auto cases = load_golden();
    ASSERT_GT(cases.size(), 0u);

    double max_ra_err = 0, max_dec_err = 0;
    for (auto &c : cases) {
        INDI::IEquatorialCoordinates j2000 = { c.ra_j2000_h, c.dec_j2000_deg };
        INDI::IEquatorialCoordinates jnow_in;
        INDI::J2000toObserved(&j2000, c.jd, &jnow_in);

        INDI::IGeographicCoordinates obs = { c.lon_deg, c.lat_deg, c.elev_m };
        INDI::IHorizontalCoordinates hrz;
        INDI::EquatorialToHorizontal(&jnow_in, &obs, c.jd, &hrz);

        if (hrz.altitude < 5.0) continue;

        INDI::IEquatorialCoordinates jnow_out;
        INDI::HorizontalToEquatorial(&hrz, &obs, c.jd, &jnow_out);

        double cos_dec = std::cos(jnow_in.declination * M_PI / 180.0);
        double ra_err  = std::abs(jnow_in.rightascension - jnow_out.rightascension) * 15.0 * 3600.0 * cos_dec;
        double dec_err = std::abs(jnow_in.declination    - jnow_out.declination)    * 3600.0;
        max_ra_err  = std::max(max_ra_err,  ra_err);
        max_dec_err = std::max(max_dec_err, dec_err);

        GTEST_LOG_(INFO) << c.object << " at " << c.site
                         << "  ra_err=" << ra_err << "\"  dec_err=" << dec_err << "\"";
        EXPECT_LT(ra_err,  1.0) << "Round-trip RA error for "  << c.object << " at " << c.site;
        EXPECT_LT(dec_err, 1.0) << "Round-trip Dec error for " << c.object << " at " << c.site;
    }
    GTEST_LOG_(INFO) << "Round-trip max: RA=" << max_ra_err << "\" Dec=" << max_dec_err << "\"";
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
