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
}

SPKMathPlugin::~SPKMathPlugin()
{
}

bool SPKMathPlugin::Initialise(InMemoryDatabase *pInMemoryDatabase)
{
    if (!pInMemoryDatabase) return false;
    this->pInMemoryDatabase = pInMemoryDatabase;

    UpdateObsConfig();

    // Collect sync points from database
    InMemoryDatabase::AlignmentDatabaseType &syncPoints = pInMemoryDatabase->GetAlignmentDatabase();

    if (syncPoints.size() < 1)
    {
        return false;
    }

    // Build observation data and determine the number of terms
    int nt = 0;
    std::vector<double> obsData = BuildObservationData(syncPoints, nt);

    double pmv[6], pms[6], skysig;
    char mountChar = (m_Obs.mount == ALTAZ) ? 'A' : 'E';
    int js = Pmfit(m_Obs.slat, mountChar, syncPoints.size(), obsData.data(), nt, pmv, pms, &skysig);

    if (js == 0)
    {
        ParsePmfitCoefficients(pmv, nt);
        return true;
    }

    return false;
}

bool SPKMathPlugin::TransformCelestialToTelescope(const double RightAscension, const double Declination,
        double JulianOffset,
        TelescopeDirectionVector &ApparentTelescopeDirectionVector)
{
    UpdateAstrometry(JulianOffset);

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

    // In Wallace's reference implementation (s2e.c), spkVtel(AXES) is called
    // with the actual current mount position in ax3.  The VD correction in
    // spk_vtel.c (pvd*cos(el)) is then evaluated at the current elevation,
    // which is already a close estimate of the demanded elevation because the
    // mount is tracking.  One call is sufficient in that workflow.
    //
    // This plugin does not have access to the current mount position, so ax3
    // is initialised to {0,0,0}.  A second pass with ax3 set to the first
    // solution makes the VD correction self-consistent with spkVtel(TARG),
    // which always receives the demanded roll/pitch as ax3.  One refinement
    // step suffices; the residual is O(pvd^2).
    spkAX3 ax3 = {0, 0, 0};
    double tara, tare, tarr, tarp, soln[5];
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
    return false;
}

bool SPKMathPlugin::TransformTelescopeToCelestial(const TelescopeDirectionVector &ApparentTelescopeDirectionVector,
        double &RightAscension, double &Declination, double JulianOffset)
{
    UpdateAstrometry(JulianOffset);

    spkAX3 ax3;
    double roll, pitch;
    DirectionVectorToRollPitch(ApparentTelescopeDirectionVector, roll, pitch);
    
    ax3.a = roll;
    ax3.b = pitch;
    ax3.r = 0.0;

    spkTAR tar;
    tar.sys = APPT;

    double tara, tare, tarr, tarp, soln[5];
    int status = spkVtel(TARG, &m_Obs, &m_Opt, &m_PM, &m_Ast, &ax3, &tar, &m_PO,
                         &tara, &tare, &tarr, &tarp, soln);

    if (status >= 0)
    {
        // soln[0] is RA in radians
        RightAscension = RAD_TO_HOURS(iauAnp(soln[0]));
        Declination = RAD_TO_DEG(soln[1]);
        return true;
    }

    return false;
}

void SPKMathPlugin::UpdateObsConfig()
{
    INDI::IGeographicCoordinates pos;
    if (pInMemoryDatabase->GetDatabaseReferencePosition(pos))
    {
        m_Obs.slat = DEG_TO_RAD(pos.latitude);
        m_Obs.slon = DEG_TO_RAD(pos.longitude);
        m_Obs.sh = pos.elevation;
    }
    
    // Set mount type based on driver's approximate alignment
    m_Obs.mount = (ApproximateMountAlignment == ZENITH) ? ALTAZ : EQUAT;
}

void SPKMathPlugin::UpdateAstrometry(double JulianOffset)
{
    double jd = ln_get_julian_from_sys() + JulianOffset;

    // Use libnova for JD to calendar conversion
    ln_date date;
    ln_get_date(jd, &date);

    spkUTC utc;
    utc.iy = date.years;
    utc.mo = date.months;
    utc.id = date.days;
    utc.ih = date.hours;
    utc.mi = date.minutes;
    utc.sec = date.seconds;

    spkEOP eop = {0, 0, 0};
    // pressure=0 disables spkVtel's internal refraction model; temperature and humidity
    // are nominal placeholders.  Atmospheric refraction is not modeled in this pipeline
    // (INDI's coordinate flow stops at "apparent" / JNow; see tar.sys = APPT above).
    spkAIR air = {0.0, 10.0, 0.5};
    
    // Use official spkAstr
    spkAstr(1, utc, 0.0, &eop, &m_Obs, &air, &m_Opt, &m_Ast);

    // Synchronize SOFA internal "clock" (ERA) with libnova LST
    // This ensures consistency with the simulator's view of HA/Az.
    double gmst_hrs = ln_get_apparent_sidereal_time(jd);
    double lst_rad  = HOURS_TO_RAD(range24(gmst_hrs + RAD_TO_HOURS(m_Obs.slon)));
    // eral = ERA + Longitude = LST + EO
    m_Ast.astrom.eral = iauAnp(lst_rad + m_Ast.eo);
}

std::vector<double> SPKMathPlugin::BuildObservationData(const InMemoryDatabase::AlignmentDatabaseType &syncPoints, int &outTermCount)
{
    // The SPK model evaluates terms rigidly: IH, ID, CH, ME, MA, TF.
    // "Aggressive" progression attempts early polar correction by fitting 
    // up through MA, but risks virtual collimation hallucination on small sets.
    outTermCount = 6;
    if (syncPoints.size() < 3) outTermCount = 2; // 1-2 points: Fit IH, ID (Offset only)
    else if (syncPoints.size() < 6) outTermCount = 5; // 3-5 points: Fit IH, ID, CH, ME, MA (Aggressive Polar)

    std::vector<double> obsData;
    for (const auto &point : syncPoints)
    {
        UpdateAstrometry(point.ObservationJulianDate - ln_get_julian_from_sys());
        
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
            obsData.push_back(iauAnp(-roll)); // roll = -Az
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
        // ALTAZ: IA -> bf[0], IB -> bf[1], CA -> bf[2], AN -> bf[3], AW -> bf[4], VD -> bf[5]
        if (terms >= 1) m_PM.pia = pmv[0];
        if (terms >= 2) m_PM.pib = pmv[1];
        if (terms >= 3) m_PM.pca = pmv[2];
        if (terms >= 4) m_PM.pan = pmv[3];
        if (terms >= 5) m_PM.paw = pmv[4];
        if (terms >= 6) m_PM.pvd = pmv[5];
    }
    else // EQUATORIAL
    {
        // Wallace (2002) Note 4: IA -> -bf[0], IB -> bf[1], CA -> -bf[2], AW -> -bf[3], AN -> bf[4], VD -> bf[5]
        if (terms >= 1) m_PM.pia = -pmv[0];
        if (terms >= 2) m_PM.pib = pmv[1];
        if (terms >= 3) m_PM.pca = -pmv[2];
        if (terms >= 4) m_PM.paw = -pmv[3];
        if (terms >= 5) m_PM.pan = pmv[4];
        if (terms >= 6) m_PM.pvd = pmv[5];
    }
}

TelescopeDirectionVector SPKMathPlugin::RollPitchToDirectionVector(double roll, double pitch)
{
    if (m_Obs.mount == ALTAZ)
    {
        INDI::IHorizontalCoordinates hor;
        hor.azimuth = RAD_TO_DEG(iauAnp(-roll));
        hor.altitude = RAD_TO_DEG(pitch);
        return TelescopeDirectionVectorFromAltitudeAzimuth(hor);
    }
    else
    {
        INDI::IEquatorialCoordinates eq;
        eq.rightascension = RAD_TO_HOURS(iauAnp(-roll));
        eq.declination = RAD_TO_DEG(pitch);
        return TelescopeDirectionVectorFromLocalHourAngleDeclination(eq);
    }
}

void SPKMathPlugin::DirectionVectorToRollPitch(const TelescopeDirectionVector &v, double &roll, double &pitch)
{
    double az, dec;
    SphericalCoordinateFromTelescopeDirectionVector(v, az, CLOCKWISE, dec, FROM_AZIMUTHAL_PLANE);
    
    if (m_Obs.mount == ALTAZ)
        roll = iauAnp(-az);
    else
        roll = -az;
        
    pitch = dec;
}

} // namespace AlignmentSubsystem
} // namespace INDI
