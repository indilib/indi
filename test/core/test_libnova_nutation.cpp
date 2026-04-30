/*******************************************************************************
 * Test: libnova nutation correctness
 *
 * Validates INDI::ln_get_equ_nut (and by extension J2000toObserved) against
 * two independent checks:
 *
 *  1. Sign check: the nutation correction in Dec for Deneb at JD 2459019.8
 *     must be negative. The DEG_TO_RAD macro bug
 *     (nut_ecliptic computed as ~23.4 instead of ~0.409 radians) flips
 *     sin/cos of the ecliptic and reverses the sign, turning a -4" Dec
 *     correction into a +11" one.
 *
 *  2. Absolute accuracy: J2000toObserved for Deneb must agree with IMCCE
 *     Miriade (INPOP19) truth to < 1 arcsecond. The macro bug causes ~15"
 *     error, dominated by the flipped Dec nutation term.
 *
 * Truth source: IMCCE Miriade, query:
 *   https://ssp.imcce.fr/webservices/miriade/api/ephemcc.php
 *   -name=HIP102098 -type=star -ep=2459019.833333 -tcoor=1 -mime=json
 *   Deneb (HIP 102098) at JD 2459019.833333 (2020-06-19 08:00 UTC)
 *******************************************************************************/

#include <gtest/gtest.h>
#include <libastro.h>
#include <libnova/nutation.h>
#include <cmath>

static constexpr double JD_TEST       = 2459019.833333;
static constexpr double RA_J2000_H    = 20.69053168;
static constexpr double DEC_J2000_DEG = 45.28033881;
static constexpr double RA_TRUTH_H    = 20.70237028;
static constexpr double DEC_TRUTH_DEG = 45.35036333;

// ---------------------------------------------------------------------------
// Structural: the Dec nutation correction for Deneb at this epoch must be
// negative.  With the correct nut_ecliptic (~0.409 rad) sin_ecliptic>0,
// giving delta_dec ≈ -4".  The DEG_TO_RAD bug yields nut_ecliptic ~23.4 rad
// where sin(23.4) < 0, flipping the sign to +11".
// ---------------------------------------------------------------------------
TEST(LibnovaNutation, DecCorrectionSign)
{
    ln_equ_posn pos_before = { RA_J2000_H * 15.0, DEC_J2000_DEG };
    ln_equ_posn pos_after  = pos_before;

    INDI::ln_get_equ_nut(&pos_after, JD_TEST, false);

    double delta_dec_arcsec = (pos_after.dec - pos_before.dec) * 3600.0;

    GTEST_LOG_(INFO) << "Nutation Dec correction: " << delta_dec_arcsec << "\"";
    GTEST_LOG_(INFO) << "Nutation RA  correction: " << (pos_after.ra - pos_before.ra) * 3600.0 << "\"";

    // The correct Dec nutation for this star/epoch is negative (~-4").
    // The DEG_TO_RAD bug produces +11", so testing the sign is sufficient
    // to distinguish correct from buggy without hard-coding the exact value.
    EXPECT_LT(delta_dec_arcsec, 0.0)
        << "Dec nutation correction is positive (" << delta_dec_arcsec
        << "\") — expected negative for Deneb at this epoch. "
           "Likely DEG_TO_RAD macro bug in ln_get_equ_nut.";
}

// ---------------------------------------------------------------------------
// Absolute accuracy: J2000toObserved for Deneb must agree with IMCCE truth
// to < 1 arcsecond.  The DEG_TO_RAD bug causes ~15" error.
// ---------------------------------------------------------------------------
TEST(LibnovaNutation, J2000toObservedAccuracy)
{
    INDI::IEquatorialCoordinates j2000 = { RA_J2000_H, DEC_J2000_DEG };
    INDI::IEquatorialCoordinates jnow;

    INDI::J2000toObserved(&j2000, JD_TEST, &jnow);

    double cos_dec   = std::cos(DEC_TRUTH_DEG * M_PI / 180.0);
    double err_ra    = std::abs(jnow.rightascension - RA_TRUTH_H)    * 15.0 * 3600.0 * cos_dec;
    double err_dec   = std::abs(jnow.declination    - DEC_TRUTH_DEG) * 3600.0;
    double err_total = std::hypot(err_ra, err_dec);

    GTEST_LOG_(INFO) << "Deneb J2000toObserved: ra=" << jnow.rightascension
                     << "h  dec=" << jnow.declination << "d";
    GTEST_LOG_(INFO) << "Error vs IMCCE: RA=" << err_ra << "\"  Dec=" << err_dec
                     << "\"  total=" << err_total << "\"";

    EXPECT_LT(err_total, 1.0)
        << "J2000toObserved error=" << err_total
        << "\" exceeds 1\" vs IMCCE truth — likely DEG_TO_RAD macro bug";
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
