/*******************************************************************************
 Copyright(c) 2021 Jasem Mutlaq. All rights reserved.

 AstroTrac Mount Driver.

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

#include "NearestMathPlugin.h"

#include <libnova/julian_day.h>

namespace INDI
{
namespace AlignmentSubsystem
{
// Standard functions required for all plugins
extern "C" {

    //////////////////////////////////////////////////////////////////////////////////////
    ///
    //////////////////////////////////////////////////////////////////////////////////////
    NearestMathPlugin *Create()
    {
        return new NearestMathPlugin;
    }

    //////////////////////////////////////////////////////////////////////////////////////
    ///
    //////////////////////////////////////////////////////////////////////////////////////
    void Destroy(NearestMathPlugin *pPlugin)
    {
        delete pPlugin;
    }

    //////////////////////////////////////////////////////////////////////////////////////
    ///
    //////////////////////////////////////////////////////////////////////////////////////
    const char *GetDisplayName()
    {
        return "Nearest Math Plugin";
    }
}

//////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////
NearestMathPlugin::NearestMathPlugin()
{

}

//////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////
NearestMathPlugin::~NearestMathPlugin()
{

}

//////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////
bool NearestMathPlugin::Initialise(InMemoryDatabase *pInMemoryDatabase)
{
    // Call the base class to initialise to in in memory database pointer
    MathPlugin::Initialise(pInMemoryDatabase);
    const auto &SyncPoints = pInMemoryDatabase->GetAlignmentDatabase();
    // Clear all extended alignment points so we can re-create them.
    ExtendedAlignmentPoints.clear();

    IGeographicCoordinates Position;
    if (!pInMemoryDatabase->GetDatabaseReferencePosition(Position))
        return false;

    // JM: We iterate over all the sync point and compute the celestial and telescope horizontal coordinates
    // Since these are used to sort the nearest alignment points to the current target. The offsets of the
    // nearest point celestial coordintes are then applied to the current target to correct for its position.
    // No complex transformations used.
    for (auto &oneSyncPoint : SyncPoints)
    {
        ExtendedAlignmentDatabaseEntry oneEntry;
        oneEntry.RightAscension = oneSyncPoint.RightAscension;
        oneEntry.Declination = oneSyncPoint.Declination;
        oneEntry.ObservationJulianDate = oneSyncPoint.ObservationJulianDate;
        oneEntry.TelescopeDirection = oneSyncPoint.TelescopeDirection;

        INDI::IEquatorialCoordinates CelestialRADE {oneEntry.RightAscension, oneEntry.Declination};
        INDI::IHorizontalCoordinates CelestialAltAz;
        EquatorialToHorizontal(&CelestialRADE, &Position, oneEntry.ObservationJulianDate, &CelestialAltAz);

        oneEntry.CelestialAzimuth = CelestialAltAz.azimuth;
        oneEntry.CelestialAltitude = CelestialAltAz.altitude;

        INDI::IHorizontalCoordinates TelescopeAltAz;
        // Alt-Az Mounts?
        if (ApproximateMountAlignment == ZENITH)
        {
            AltitudeAzimuthFromTelescopeDirectionVector(oneEntry.TelescopeDirection, TelescopeAltAz);
        }
        // Equatorial?
        else
        {
            INDI::IEquatorialCoordinates TelescopeRADE;
            EquatorialCoordinatesFromTelescopeDirectionVector(oneEntry.TelescopeDirection, TelescopeRADE);
            EquatorialToHorizontal(&TelescopeRADE, &Position, oneEntry.ObservationJulianDate, &TelescopeAltAz);
        }

        oneEntry.TelescopeAzimuth = TelescopeAltAz.azimuth;
        oneEntry.TelescopeAltitude = TelescopeAltAz.altitude;

        ExtendedAlignmentPoints.push_back(oneEntry);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////
bool NearestMathPlugin::TransformCelestialToTelescope(const double RightAscension, const double Declination,
        double JulianOffset, TelescopeDirectionVector &ApparentTelescopeDirectionVector)
{
    // Get Position
    IGeographicCoordinates Position;
    if (!pInMemoryDatabase || !pInMemoryDatabase->GetDatabaseReferencePosition(Position))
        return false;

    // Get Julian date from system and apply Julian Offset if any.
    double JDD = ln_get_julian_from_sys() + JulianOffset;

    // Compute CURRENT horizontal coords.
    INDI::IEquatorialCoordinates CelestialRADE {RightAscension, Declination};
    INDI::IHorizontalCoordinates CelestialAltAz;
    EquatorialToHorizontal(&CelestialRADE, &Position, JDD, &CelestialAltAz);

    // Return Telescope Direction Vector directly from Celestial coordinates if we
    // do not have any sync points.
    if (ExtendedAlignmentPoints.empty())
    {
        if (ApproximateMountAlignment == ZENITH)
        {
            // Return Alt-Az Telescope Direction Vector For Alt-Az mounts.
            ApparentTelescopeDirectionVector = TelescopeDirectionVectorFromAltitudeAzimuth(CelestialAltAz);
        }
        // Equatorial?
        else
        {
            // Return RA-DE Telescope Direction Vector for Equatorial mounts.
            ApparentTelescopeDirectionVector = TelescopeDirectionVectorFromEquatorialCoordinates(CelestialRADE);
        }

        return true;
    }

    // If we have sync points, then get the Nearest Point
    ExtendedAlignmentDatabaseEntry nearest = GetNearestPoint(CelestialAltAz.azimuth, CelestialAltAz.altitude, true);

    INDI::IEquatorialCoordinates TelescopeRADE;

    // Get the nearest point in the telescope reference frame

    // Alt-Az? Transform the nearest telescope direction vector to telescope Alt-Az and then to telescope RA/DE
    if (ApproximateMountAlignment == ZENITH)
    {
        INDI::IHorizontalCoordinates TelescopeAltAz;
        AltitudeAzimuthFromTelescopeDirectionVector(nearest.TelescopeDirection, TelescopeAltAz);
        HorizontalToEquatorial(&TelescopeAltAz, &Position, nearest.ObservationJulianDate, &TelescopeRADE);
    }
    // Equatorial? Transform nearest directly to telescope RA/DE
    else
    {
        EquatorialCoordinatesFromTelescopeDirectionVector(nearest.TelescopeDirection, TelescopeRADE);
    }

    // Adjust the Celestial coordinates to account for the offset between the nearest point and the telescope
    // e.g. Celestial RA = 5. Nearest Point (Sky: 4, Telescope: 3)
    // Means Final Telescope RA = 5 - (4-3) = 4
    // So we can issue GOTO to RA ~4, and it should up near Celestial RA ~5
    INDI::IEquatorialCoordinates TransformedTelescopeRADE = CelestialRADE;
    TransformedTelescopeRADE.rightascension -= (nearest.RightAscension - TelescopeRADE.rightascension);
    TransformedTelescopeRADE.declination -= (nearest.Declination - TelescopeRADE.declination);

    // Final step is to convert transformed telescope coordinates to a direction vector
    if (ApproximateMountAlignment == ZENITH)
    {
        INDI::IHorizontalCoordinates TransformedTelescopeAltAz;
        EquatorialToHorizontal(&TransformedTelescopeRADE, &Position, JDD, &TransformedTelescopeAltAz);
        ApparentTelescopeDirectionVector = TelescopeDirectionVectorFromAltitudeAzimuth(TransformedTelescopeAltAz);
    }
    // Equatorial?
    else
    {
        ApparentTelescopeDirectionVector = TelescopeDirectionVectorFromEquatorialCoordinates(TransformedTelescopeRADE);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////
bool NearestMathPlugin::TransformTelescopeToCelestial(const TelescopeDirectionVector &ApparentTelescopeDirectionVector,
        double &RightAscension, double &Declination)
{
    IGeographicCoordinates Position;
    if (!pInMemoryDatabase || !pInMemoryDatabase->GetDatabaseReferencePosition(Position))
        return false;

    double JDD = ln_get_julian_from_sys();

    // Telescope Equatorial Coordinates
    INDI::IEquatorialCoordinates TelescopeRADE;

    // Do nothing if we don't have sync points.
    if (ExtendedAlignmentPoints.empty())
    {
        // Alt/Az Mount?
        if (ApproximateMountAlignment == ZENITH)
        {
            INDI::IHorizontalCoordinates TelescopeAltAz;
            AltitudeAzimuthFromTelescopeDirectionVector(ApparentTelescopeDirectionVector, TelescopeAltAz);
            HorizontalToEquatorial(&TelescopeAltAz, &Position, JDD, &TelescopeRADE);
        }
        // Equatorial?
        else
        {
            EquatorialCoordinatesFromTelescopeDirectionVector(ApparentTelescopeDirectionVector, TelescopeRADE);
        }

        RightAscension = TelescopeRADE.rightascension;
        Declination = TelescopeRADE.declination;
        return true;
    }

    // Need to get CURRENT Telescope horizontal coords
    INDI::IHorizontalCoordinates TelescopeAltAz;
    // Alt/Az Mount?
    if (ApproximateMountAlignment == ZENITH)
    {
        AltitudeAzimuthFromTelescopeDirectionVector(ApparentTelescopeDirectionVector, TelescopeAltAz);
        HorizontalToEquatorial(&TelescopeAltAz, &Position, JDD, &TelescopeRADE);
    }
    // Equatorial?
    else
    {
        EquatorialCoordinatesFromTelescopeDirectionVector(ApparentTelescopeDirectionVector, TelescopeRADE);
        EquatorialToHorizontal(&TelescopeRADE, &Position, JDD, &TelescopeAltAz);
    }

    // Find the nearest point to our telescope now
    ExtendedAlignmentDatabaseEntry nearest = GetNearestPoint(TelescopeAltAz.azimuth, TelescopeAltAz.altitude, false);

    // Now get the nearest telescope in equatorial coordinates.
    INDI::IEquatorialCoordinates NearestTelescopeRADE;
    if (ApproximateMountAlignment == ZENITH)
    {
        INDI::IHorizontalCoordinates NearestTelescopeAltAz {nearest.TelescopeAzimuth, nearest.TelescopeAltitude};
        HorizontalToEquatorial(&NearestTelescopeAltAz, &Position, nearest.ObservationJulianDate, &NearestTelescopeRADE);
    }
    // Equatorial?
    else
    {
        EquatorialCoordinatesFromTelescopeDirectionVector(nearest.TelescopeDirection, NearestTelescopeRADE);
    }

    // Adjust the Telescope coordinates to account for the offset between the nearest point and the telescope
    // e.g. Telescope RA = 5. Nearest Point (Target: 4, Telescope: 3)
    // Means Final Telescope RA = 5 + (4-3) = 6
    // So a telescope reporting ~5 hours should actually be pointing to ~6 hours in the sky.
    INDI::IEquatorialCoordinates TransformedCelestialRADE = TelescopeRADE;
    TransformedCelestialRADE.rightascension += (nearest.RightAscension - NearestTelescopeRADE.rightascension);
    TransformedCelestialRADE.declination += (nearest.Declination - NearestTelescopeRADE.declination);

    RightAscension = TransformedCelestialRADE.rightascension;
    Declination = TransformedCelestialRADE.declination;
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////
ExtendedAlignmentDatabaseEntry NearestMathPlugin::GetNearestPoint(const double Azimuth, const double Altitude,
        bool isCelestial)
{
    ExtendedAlignmentDatabaseEntry nearest;
    double distance = 1e6;

    for (auto &oneEntry : ExtendedAlignmentPoints)
    {
        double oneDistance = 0;

        if (isCelestial)
            oneDistance = SphereUnitDistance(Azimuth, oneEntry.CelestialAzimuth, Altitude, oneEntry.CelestialAltitude);
        else
            oneDistance = SphereUnitDistance(Azimuth, oneEntry.TelescopeAzimuth, Altitude, oneEntry.TelescopeAltitude);

        if (oneDistance < distance)
        {
            nearest = oneEntry;
            distance = oneDistance;
        }
    }

    return nearest;
}

//////////////////////////////////////////////////////////////////////////////////////
/// Using haversine: http://en.wikipedia.org/wiki/Haversine_formula
//////////////////////////////////////////////////////////////////////////////////////
double NearestMathPlugin::SphereUnitDistance(double theta1, double theta2, double phi1, double phi2)
{
    double sqrt_haversin_lat  = sin(((phi2 - phi1) / 2) * (M_PI / 180));
    double sqrt_haversin_long = sin(((theta2 - theta1) / 2) * (M_PI / 180));
    return (2 *
            asin(sqrt((sqrt_haversin_lat * sqrt_haversin_lat) + cos(phi1 * (M_PI / 180)) * cos(phi2 * (M_PI / 180)) *
                      (sqrt_haversin_long * sqrt_haversin_long))));
}
} // namespace AlignmentSubsystem
} // namespace INDI
