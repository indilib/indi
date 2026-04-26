/*******************************************************************************
 Copyright(c) 2026 Christian Kemper. All rights reserved.

 SPK Math Plugin for INDI Alignment Subsystem.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "SPKMathPlugin.h"
#include "DriverCommon.h"
#include <vector>
#include <cmath>
#include <cstring>
#include "indicom.h"
#include "libastro.h"
#include "spk/sofa.h"
#include <libnova/julian_day.h>
#include <libnova/sidereal_time.h>

namespace INDI
{
namespace AlignmentSubsystem
{

#ifndef NO_PLUGIN_HOOKS
// Standard functions required for all plugins
extern "C" {
    SPKMathPlugin *Create()
    {
        return new SPKMathPlugin;
    }

    void Destroy(SPKMathPlugin *pPlugin)
    {
        delete pPlugin;
    }

    const char *GetDisplayName()
    {
        return "SPK Math Plugin";
    }
}
#endif

SPKMathPlugin::SPKMathPlugin()
{
    // Initialize structures with defaults
    memset(&m_Obs, 0, sizeof(m_Obs));
    memset(&m_PM, 0, sizeof(m_PM));
    memset(&m_Ast, 0, sizeof(m_Ast));
    memset(&m_Opt, 0, sizeof(m_Opt));
    memset(&m_PO, 0, sizeof(m_PO));
    
    m_Opt.fl = 1000.0; // Default fl
    m_Opt.wl = 0.55;   // Default wavelength

    memset(m_NE_A, 0, sizeof(m_NE_A));
    memset(m_NE_v, 0, sizeof(m_NE_v));
    m_NE_n         = 0;
    m_NE_mountChar = 0;
    m_NE_valid     = false;
}

SPKMathPlugin::~SPKMathPlugin()
{
}

bool SPKMathPlugin::Initialise(InMemoryDatabase *pInMemoryDatabase)
{
    if (!pInMemoryDatabase) return false;
    MathPlugin::Initialise(pInMemoryDatabase);

    if (!UpdateObsConfig()) return true;  // no location yet — model unconstrained, not an error

    InMemoryDatabase::AlignmentDatabaseType &syncPoints = pInMemoryDatabase->GetAlignmentDatabase();
    int  n         = static_cast<int>(syncPoints.size());
    char mountChar = (m_Obs.mount == ALTAZ) ? 'A' : 'E';

    if (n < 1) return true;  // No sync points yet — not an error, model is simply unconstrained.

    ASSDEBUGF("SPK Initialise: n=%d mount=%c NE_valid=%d NE_n=%d",
              n, mountChar, m_NE_valid, m_NE_n);

    // Hot path: full 6-term model already fitted, exactly one new point added,
    // same mount type.
    if (m_NE_valid &&
        n         == m_NE_n + 1 &&
        mountChar == m_NE_mountChar)
    {
        ASSDEBUGF("SPK Initialise: hot path — incremental NE update for point %d", n);
        double oblon, oblat, rdem, pdem;
        if (ExtractObsRow(syncPoints.back(), oblon, oblat, rdem, pdem) &&
            AccumulateObsRow(oblon, oblat, rdem, pdem))
        {
            double pmv[6];
            if (SolveNormalEquations(pmv))
            {
                m_NE_n = n;
                ParsePmfitCoefficients(pmv, 6);
                return true;
            }
        }
        // Fall through to cold path on any failure.
        ASSDEBUG("SPK hot path failed - falling through to cold path");
    }

    // Cold path: full Pmfit rebuild.
    int nt = 0;
    std::vector<double> obsData = BuildObservationData(syncPoints, nt);

    double pmv[6] = {}, pms[6] = {}, skysig = 0;
    int js = Pmfit(m_Obs.slat, mountChar, n, obsData.data(), nt, pmv, pms, &skysig);
    if (js != 0)
    {
        ASSDEBUGF("SPK Pmfit failed: js=%d", js);
        m_NE_valid = false;
        return false;
    }

    ParsePmfitCoefficients(pmv, nt);

    // If we have a full 6-term model, build the NE accumulator so subsequent
    // single-point additions can use the hot path.
    if (nt == 6)
    {
        m_NE_mountChar = mountChar;
        memset(m_NE_A, 0, sizeof(m_NE_A));
        memset(m_NE_v, 0, sizeof(m_NE_v));
        m_NE_valid = true;

        for (const auto &point : syncPoints)
        {
            double oblon, oblat, rdem, pdem;
            if (!ExtractObsRow(point, oblon, oblat, rdem, pdem) ||
                !AccumulateObsRow(oblon, oblat, rdem, pdem))
            {
                m_NE_valid = false;
                break;
            }
        }
        m_NE_n = m_NE_valid ? n : 0;
    }
    else
    {
        m_NE_valid = false;
    }

    return true;
}

bool SPKMathPlugin::TransformCelestialToTelescope(const double RightAscension, const double Declination,
        double JulianOffset,
        TelescopeDirectionVector &ApparentTelescopeDirectionVector)
{
    if (!UpdateObsConfig()) return false;
    UpdateAstrometry(ln_get_julian_from_sys() + JulianOffset);

    spkTAR tar;
    // Input RA/Dec is in INDI's "observed" (= SOFA's "apparent") form: precession,
    // nutation, and aberration have been applied by the INDI subsystem (J2000toObserved),
    // but atmospheric refraction has not.  tar.sys = APPT matches this — it tells spkVtel
    // the coordinates are already in apparent form and not to re-apply those transforms.
    // Refraction is not modeled in this pipeline; pressure=0 in spkAIR disables
    // spkVtel's internal refraction as well.
    tar.sys = APPT;
    tar.a = HOURS_TO_RAD(RightAscension);
    tar.b = DEG_TO_RAD(Declination);

    double tara, tare, tarr, tarp, soln[5];

    // Two-pass: first AXES call with ax3={0,0,0} gives an initial encoder
    // demand; second pass re-evaluates the VD correction at the correct
    // elevation.  One refinement suffices; residual is O(pvd^2).
    spkAX3 ax3 = {0, 0, 0};
    int status = spkVtel(AXES, &m_Obs, &m_Opt, &m_PM, &m_Ast, &ax3, &tar, &m_PO,
                         &tara, &tare, &tarr, &tarp, soln);

    if (status >= 0)
    {
        ax3.a = soln[0];
        ax3.b = soln[1];
        status = spkVtel(AXES, &m_Obs, &m_Opt, &m_PM, &m_Ast, &ax3, &tar, &m_PO,
                         &tara, &tare, &tarr, &tarp, soln);
    }

    if (status >= 0)
    {
        double roll = soln[0];
        double pitch = soln[1];
        ApparentTelescopeDirectionVector = RollPitchToDirectionVector(roll, pitch);
        return true;
    }
    ASSDEBUGF("SPK C->T failed: RA=%.4fh Dec=%.4fdeg status=%d", RightAscension, Declination, status);
    return false;
}

bool SPKMathPlugin::TransformTelescopeToCelestial(const TelescopeDirectionVector &ApparentTelescopeDirectionVector,
        double &RightAscension, double &Declination, double JulianOffset)
{
    if (!UpdateObsConfig()) return false;
    UpdateAstrometry(ln_get_julian_from_sys() + JulianOffset);

    spkAX3 ax3;
    double roll, pitch;
    DirectionVectorToRollPitch(ApparentTelescopeDirectionVector, roll, pitch);
    
    ax3.a = roll;
    ax3.b = pitch;
    ax3.r = 0.0;

    spkTAR tar;
    tar.sys = APPT;
    tar.a   = 0.0;  // not used by spkVtel in TARG mode, but read unconditionally before the mode switch
    tar.b   = 0.0;

    double tara, tare, tarr, tarp, soln[5];
    int status = spkVtel(TARG, &m_Obs, &m_Opt, &m_PM, &m_Ast, &ax3, &tar, &m_PO,
                         &tara, &tare, &tarr, &tarp, soln);

    if (status >= 0)
    {
        // soln[0] is RA in radians
        RightAscension = RAD_TO_HOURS(iauAnp(soln[0]));
        Declination = RAD_TO_DEG(soln[1]);
        ASSDEBUGF("SPK T->C: roll=%.4fdeg pitch=%.4fdeg => RA=%.4fh Dec=%.4fdeg",
                  RAD_TO_DEG(roll), RAD_TO_DEG(pitch), RightAscension, Declination);
        return true;
    }
    ASSDEBUGF("SPK T->C failed: roll=%.4fdeg pitch=%.4fdeg status=%d",
              RAD_TO_DEG(roll), RAD_TO_DEG(pitch), status);
    return false;
}

bool SPKMathPlugin::UpdateObsConfig()
{
    if (!pInMemoryDatabase) return false;
    INDI::IGeographicCoordinates pos;
    if (!pInMemoryDatabase->GetDatabaseReferencePosition(pos))
        return false;
    m_Obs.slat = DEG_TO_RAD(pos.latitude);
    m_Obs.slon = DEG_TO_RAD(pos.longitude);
    m_Obs.sh   = pos.elevation;
    m_Obs.mount = (ApproximateMountAlignment == ZENITH) ? ALTAZ : EQUAT;
    return true;
}

void SPKMathPlugin::UpdateAstrometry(double JD)
{
    // Use libnova for JD to calendar conversion
    ln_date date;
    ln_get_date(JD, &date);

    spkUTC utc;
    utc.iy = date.years;
    utc.mo = date.months;
    utc.id = date.days;
    utc.ih = date.hours;
    utc.mi = date.minutes;
    utc.sec = date.seconds;

    // Compute LST first — needed both for the ERA synchronization below and
    // as a safe fallback if spkAstr fails.
    double gmst_hrs = ln_get_apparent_sidereal_time(JD);
    double lst_rad  = HOURS_TO_RAD(range24(gmst_hrs + RAD_TO_HOURS(m_Obs.slon)));
    m_LST_rad = lst_rad;

    spkEOP eop = {0, 0, 0};
    // pressure=0 disables spkVtel's internal refraction model; temperature and humidity
    // are nominal placeholders.  Atmospheric refraction is not modeled in this pipeline
    // (INDI's coordinate flow stops at "apparent" / JNow; see tar.sys = APPT above).
    spkAIR air = {0.0, 10.0, 0.5};

    // Zero m_Ast before calling spkAstr so a failure cannot leave stale data behind.
    memset(&m_Ast, 0, sizeof(m_Ast));
    int astrStatus = spkAstr(1, utc, 0.0, &eop, &m_Obs, &air, &m_Opt, &m_Ast);
    if (astrStatus < 0)
    {
        ASSDEBUGF("SPK UpdateAstrometry: spkAstr failed (status=%d), transforms will be inaccurate", astrStatus);
        // Leave m_Ast zeroed; set eral from libnova LST so subsequent spkVtel
        // calls see a minimally coherent ERA rather than garbage.
        m_Ast.astrom.eral = iauAnp(lst_rad);
        return;
    }

    // Synchronize SOFA internal "clock" (ERA) with libnova LST.
    // This ensures consistency with the simulator's view of HA/Az.
    // eral = ERA + Longitude = LST + EO
    m_Ast.astrom.eral = iauAnp(lst_rad + m_Ast.eo);
}


bool SPKMathPlugin::ExtractObsRow(const AlignmentDatabaseEntry &point,
                                   double &oblon, double &oblat,
                                   double &rdem,  double &pdem)
{
    UpdateAstrometry(point.ObservationJulianDate);

    double gmst_hrs  = ln_get_apparent_sidereal_time(point.ObservationJulianDate);
    double lst_hrs   = range24(gmst_hrs + RAD_TO_HOURS(m_Obs.slon));
    double obslon_ha = HOURS_TO_RAD(get_local_hour_angle(lst_hrs, point.RightAscension));
    double obslat_   = DEG_TO_RAD(point.Declination);

    double roll, pitch;
    DirectionVectorToRollPitch(point.TelescopeDirection, roll, pitch);

    if (m_Obs.mount == ALTAZ)
    {
        // SPK altaz roll convention: roll = pi - Az_NE (vtel.c Note 7).
        // Bfun('A') expects rdem in SOFA convention (0=N, East+), so recover
        // the SOFA azimuth via pi - roll.
        double az_cel, el_cel;
        iauHd2ae(obslon_ha, obslat_, m_Obs.slat, &az_cel, &el_cel);
        oblon = az_cel;
        oblat = el_cel;
        rdem  = iauAnp(M_PI - roll);
    }
    else
    {
        oblon = obslon_ha;
        oblat = obslat_;
        rdem  = -roll;
    }
    pdem = pitch;
    ASSDEBUGF("SPK ExtractObsRow: RA=%.4fh Dec=%.4fdeg oblon=%.4fdeg oblat=%.4fdeg rdem=%.4fdeg pdem=%.4fdeg residual=(%.1f\" %.1f\")",
              point.RightAscension, point.Declination,
              RAD_TO_DEG(oblon), RAD_TO_DEG(oblat),
              RAD_TO_DEG(rdem),  RAD_TO_DEG(pdem),
              RAD_TO_DEG(iauAnpm(oblon - rdem)) * cos(oblat) * 3600.0,
              RAD_TO_DEG(iauAnpm(oblat - pdem)) * 3600.0);
    return true;
}

bool SPKMathPlugin::AccumulateObsRow(double oblon, double oblat,
                                      double rdem,  double pdem)
{
    // Bfun is evaluated at the celestial (observed) coordinates, matching
    // Pmfit's accumulation pass.  xi/eta are the residuals with pm=0
    // (encoder demand = rdem/pdem), which is Pmfit iteration-0.
    double bf[12]; // 2 * 6 terms
    char mountChar = (m_Obs.mount == ALTAZ) ? 'A' : 'E';
    if (Bfun(6, m_Obs.slat, mountChar, oblon, oblat, bf) != 0)
        return false;

    double xi  = iauAnpm(oblon - rdem) * cos(oblat);
    double eta = iauAnpm(oblat - pdem);

    for (int i = 0; i < 6; i++)
    {
        double bfi0 = bf[i*2 + 0];
        double bfi1 = bf[i*2 + 1];
        m_NE_v[i] += bfi0 * xi + bfi1 * eta;
        for (int j = 0; j < 6; j++)
            m_NE_A[i*6 + j] += bfi0 * bf[j*2 + 0] + bfi1 * bf[j*2 + 1];
    }
    return true;
}

bool SPKMathPlugin::SolveNormalEquations(double pmv[6])
{
    // Copy both arrays: Simeqn overwrites in-place, but we need to keep the
    // accumulators intact for future incremental additions.
    double Acopy[36];
    double vcopy[6];
    memcpy(Acopy, m_NE_A, sizeof(m_NE_A));
    memcpy(vcopy, m_NE_v, sizeof(m_NE_v));

    if (Simeqn(6, Acopy, vcopy) != 0)
        return false;

    memcpy(pmv, vcopy, 6 * sizeof(double));
    return true;
}

std::vector<double> SPKMathPlugin::BuildObservationData(const InMemoryDatabase::AlignmentDatabaseType &syncPoints, int &outTermCount)
{
    // Pmfit term order: IH, ID, ME, MA, CH, TF  (equatorial)
    //                   IA, IE, AN, AW, CA, TF  (altazimuth)
    // Polar/axis-tilt terms come before collimation so that a 4-term fit on
    // 3 points (6 measurements, 4 unknowns) is well-conditioned.
    outTermCount = 6;
    if      (syncPoints.size() < 3) outTermCount = 2; // 1-2 pts: IH, ID
    else if (syncPoints.size() < 5) outTermCount = 4; // 3-4 pts: IH, ID, ME, MA
    else if (syncPoints.size() < 6) outTermCount = 5; // 5 pts:   IH, ID, ME, MA, CH
    // 6+ pts: full model

    std::vector<double> obsData;
    for (const auto &point : syncPoints)
    {
        UpdateAstrometry(point.ObservationJulianDate);
        
        double gmst_hrs = ln_get_apparent_sidereal_time(point.ObservationJulianDate);
        double lst_hrs  = range24(gmst_hrs + RAD_TO_HOURS(m_Obs.slon));

        double obslon_ha = HOURS_TO_RAD(get_local_hour_angle(lst_hrs, point.RightAscension));
        double obslat    = DEG_TO_RAD(point.Declination);

        double roll, pitch;
        DirectionVectorToRollPitch(point.TelescopeDirection, roll, pitch);

        if (m_Obs.mount == ALTAZ)
        {
            double az_cel, el_cel;
            iauHd2ae(obslon_ha, obslat, m_Obs.slat, &az_cel, &el_cel);
            obsData.push_back(az_cel);
            obsData.push_back(el_cel);
            // SPK altaz roll = pi - Az_NE, so rdem (SOFA Az) = pi - roll.
            obsData.push_back(iauAnp(M_PI - roll));
        }
        else
        {
            obsData.push_back(obslon_ha);
            obsData.push_back(obslat);
            obsData.push_back(-roll); // roll = -HA
        }
        obsData.push_back(pitch);
    }
    return obsData;
}

void SPKMathPlugin::ParsePmfitCoefficients(const double pmv[6], int terms)
{
    memset(&m_PM, 0, sizeof(m_PM));

    if (m_Obs.mount == ALTAZ)
    {
        // Pmfit order: IA, IE, AN, AW, CA, TF
        // Wallace (2002) Note 4 (altaz): IA=bf[0], IB=bf[1], AN=bf[2], AW=bf[3], CA=bf[4], VD=bf[5]
        if (terms >= 1) m_PM.pia = pmv[0];
        if (terms >= 2) m_PM.pib = pmv[1];
        if (terms >= 3) m_PM.pan = pmv[2];
        if (terms >= 4) m_PM.paw = pmv[3];
        if (terms >= 5) m_PM.pca = pmv[4];
        if (terms >= 6) m_PM.pvd = pmv[5];
    }
    else // EQUATORIAL
    {
        // Pmfit order: IH, ID, ME, MA, CH, TF
        // Wallace (2002) Note 4 (equat): IA=-bf[0], IB=bf[1], AW=-bf[3], AN=bf[2], CA=-bf[4], VD=bf[5]
        if (terms >= 1) m_PM.pia = -pmv[0];
        if (terms >= 2) m_PM.pib = pmv[1];
        if (terms >= 3) m_PM.paw = -pmv[3];
        if (terms >= 4) m_PM.pan = pmv[2];
        if (terms >= 5) m_PM.pca = -pmv[4];
        if (terms >= 6) m_PM.pvd = pmv[5];
    }
}

TelescopeDirectionVector SPKMathPlugin::RollPitchToDirectionVector(double roll, double pitch)
{
    if (m_Obs.mount == ALTAZ)
    {
        // SPK altaz roll = pi - Az_NE (vtel.c Note 7), so Az_NE = pi - roll.
        INDI::IHorizontalCoordinates hor;
        hor.azimuth = RAD_TO_DEG(iauAnp(M_PI - roll));
        hor.altitude = RAD_TO_DEG(pitch);
        return TelescopeDirectionVectorFromAltitudeAzimuth(hor);
    }
    else // EQ: roll = -HA, convert to RA = LST - HA = LST + roll
    {
        INDI::IEquatorialCoordinates eq;
        eq.rightascension = range24(RAD_TO_HOURS(iauAnp(m_LST_rad + roll)));
        eq.declination = RAD_TO_DEG(pitch);
        return TelescopeDirectionVectorFromEquatorialCoordinates(eq);
    }
}

void SPKMathPlugin::DirectionVectorToRollPitch(const TelescopeDirectionVector &v, double &roll, double &pitch)
{
    if (m_Obs.mount == ALTAZ)
    {
        // SPK altaz roll = pi - Az_NE (vtel.c Note 7).
        INDI::IHorizontalCoordinates hor;
        AltitudeAzimuthFromTelescopeDirectionVector(v, hor);
        roll  = iauAnp(M_PI - DEG_TO_RAD(hor.azimuth));
        pitch = DEG_TO_RAD(hor.altitude);
    }
    else // EQ: decode RA-based TDV, convert RA to HA, roll = -HA
    {
        INDI::IEquatorialCoordinates eq;
        EquatorialCoordinatesFromTelescopeDirectionVector(v, eq);
        double ha_rad = iauAnpm(m_LST_rad - HOURS_TO_RAD(eq.rightascension));
        roll  = -ha_rad;
        pitch = DEG_TO_RAD(eq.declination);
    }
}

} // namespace AlignmentSubsystem
} // namespace INDI
