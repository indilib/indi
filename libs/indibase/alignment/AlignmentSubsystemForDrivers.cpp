/*!
 * \file AlignmentSubsystemForDrivers.cpp
 *
 * \author Roger James
 * \date 13th November 2013
 *
 */

#include "AlignmentSubsystemForDrivers.h"

namespace INDI
{
namespace AlignmentSubsystem
{
AlignmentSubsystemForDrivers::AlignmentSubsystemForDrivers()
{
    // Set up the in memory database pointer for math plugins
    SetCurrentInMemoryDatabase(this);
    // Tell the built in math plugin about it
    Initialise(this);
    // Fix up the database load callback
    SetLoadDatabaseCallback(&MyDatabaseLoadCallback, this);
}

// Public methods

void AlignmentSubsystemForDrivers::InitAlignmentProperties(Telescope *pTelescope)
{
    MapPropertiesToInMemoryDatabase::InitProperties(pTelescope);
    MathPluginManagement::InitProperties(pTelescope);
}

void AlignmentSubsystemForDrivers::ProcessAlignmentBLOBProperties(Telescope *pTelescope, const char *name, int sizes[],
        int blobsizes[], char *blobs[], char *formats[],
        char *names[], int n)
{
    MapPropertiesToInMemoryDatabase::ProcessBlobProperties(pTelescope, name, sizes, blobsizes, blobs, formats, names,
            n);
}

void AlignmentSubsystemForDrivers::ProcessAlignmentNumberProperties(Telescope *pTelescope, const char *name,
        double values[], char *names[], int n)
{
    MapPropertiesToInMemoryDatabase::ProcessNumberProperties(pTelescope, name, values, names, n);
}

void AlignmentSubsystemForDrivers::ProcessAlignmentSwitchProperties(Telescope *pTelescope, const char *name,
        ISState *states, char *names[], int n)
{
    MapPropertiesToInMemoryDatabase::ProcessSwitchProperties(pTelescope, name, states, names, n);
    MathPluginManagement::ProcessSwitchProperties(pTelescope, name, states, names, n);
}

void AlignmentSubsystemForDrivers::ProcessAlignmentTextProperties(Telescope *pTelescope, const char *name,
        char *texts[], char *names[], int n)
{
    MathPluginManagement::ProcessTextProperties(pTelescope, name, texts, names, n);
}

void AlignmentSubsystemForDrivers::SaveAlignmentConfigProperties(FILE *fp)
{
    MathPluginManagement::SaveConfigProperties(fp);
}

// Helper methods

bool AlignmentSubsystemForDrivers::AddAlignmentEntryEquatorial(double actualRA, double actualDec, double mountRA,
        double mountDec)
{
    IGeographicCoordinates location;
    if (!GetDatabaseReferencePosition(location))
    {
        return false;
    }

    double LST = get_local_sidereal_time(location.longitude);
    INDI::IEquatorialCoordinates RaDec {range24(LST - mountRA), mountDec};
    AlignmentDatabaseEntry NewEntry;
    TelescopeDirectionVector TDV = TelescopeDirectionVectorFromLocalHourAngleDeclination(RaDec);

    NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
    NewEntry.RightAscension = actualRA;
    NewEntry.Declination = actualDec;
    NewEntry.TelescopeDirection = TDV;
    NewEntry.PrivateDataSize = 0;

    if (!CheckForDuplicateSyncPoint(NewEntry))
    {
        GetAlignmentDatabase().push_back(NewEntry);
        UpdateSize();

        // tell the math plugin about the new alignment point
        Initialise(this);

        return true;
    }

    return false;
}

bool AlignmentSubsystemForDrivers::SkyToTelescopeEquatorial(double actualRA, double actualDec, double &mountRA,
        double &mountDec)
{
    INDI::IEquatorialCoordinates eq{0, 0};
    TelescopeDirectionVector TDV;
    IGeographicCoordinates location;

    // by default, just return what we were given
    mountRA = actualRA;
    mountDec = actualDec;

    if (!GetDatabaseReferencePosition(location))
    {
        return false;
    }

    if (GetAlignmentDatabase().size() > 1)
    {
        if (TransformCelestialToTelescope(actualRA, actualDec, 0.0, TDV))
        {
            LocalHourAngleDeclinationFromTelescopeDirectionVector(TDV, eq);

            //  and now we have to convert from lha back to RA
            double LST = get_local_sidereal_time(location.longitude);
            mountRA = range24(LST - eq.rightascension);
            mountDec = eq.declination;

            return true;
        }
    }

    return false;
}

bool AlignmentSubsystemForDrivers::TelescopeEquatorialToSky(double mountRA, double mountDec, double &actualRA,
        double &actualDec)
{
    INDI::IEquatorialCoordinates eq{0, 0};
    IGeographicCoordinates location;

    // by default, just return what we were given
    actualRA = mountRA;
    actualDec = mountDec;

    if (!GetDatabaseReferencePosition(location))
    {
        return false;
    }

    if (GetAlignmentDatabase().size() > 1)
    {
        TelescopeDirectionVector TDV;

        double lst = get_local_sidereal_time(location.longitude);
        double lha = get_local_hour_angle(lst, mountRA);
        eq.rightascension = lha;
        eq.declination = mountDec;

        TDV = TelescopeDirectionVectorFromLocalHourAngleDeclination(eq);
        return TransformTelescopeToCelestial(TDV, actualRA, actualDec);
    }

    return false;
}

bool AlignmentSubsystemForDrivers::AddAlignmentEntryAltAz(double actualRA, double actualDec, double mountAlt,
        double mountAz)
{
    IGeographicCoordinates location;
    if (!GetDatabaseReferencePosition(location))
    {
        return false;
    }

    INDI::IHorizontalCoordinates AltAz {range360(mountAz), range360(mountAlt)};
    AlignmentDatabaseEntry NewEntry;
    TelescopeDirectionVector TDV = TelescopeDirectionVectorFromAltitudeAzimuth(AltAz);

    NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
    NewEntry.RightAscension = actualRA;
    NewEntry.Declination = actualDec;
    NewEntry.TelescopeDirection = TDV;
    NewEntry.PrivateDataSize = 0;

    if (!CheckForDuplicateSyncPoint(NewEntry))
    {
        GetAlignmentDatabase().push_back(NewEntry);
        UpdateSize();

        // tell the math plugin about the new alignment point
        Initialise(this);

        return true;
    }

    return false;
}

bool AlignmentSubsystemForDrivers::SkyToTelescopeAltAz(double actualRA, double actualDec, double &mountAlt, double &mountAz)
{
    INDI::IHorizontalCoordinates altAz{0, 0};
    TelescopeDirectionVector TDV;
    IGeographicCoordinates location;

    if (!GetDatabaseReferencePosition(location))
    {
        return false;
    }

    if (GetAlignmentDatabase().size() > 1)
    {
        if (TransformCelestialToTelescope(actualRA, actualDec, 0.0, TDV))
        {
            AltitudeAzimuthFromTelescopeDirectionVector(TDV, altAz);
            mountAz = range360(altAz.azimuth);
            mountAlt = range360(altAz.altitude);
            return true;
        }
    }

    return false;
}

bool AlignmentSubsystemForDrivers::TelescopeAltAzToSky(double mountAlt, double mountAz, double &actualRa, double &actualDec)
{
    INDI::IHorizontalCoordinates altaz{0, 0};
    IGeographicCoordinates location;

    if (!GetDatabaseReferencePosition(location))
    {
        return false;
    }

    if (GetAlignmentDatabase().size() > 1)
    {
        TelescopeDirectionVector TDV;
        altaz.azimuth  = range360(mountAz);
        altaz.altitude = range360(mountAlt);
        TDV = TelescopeDirectionVectorFromAltitudeAzimuth(altaz);
        return TransformTelescopeToCelestial(TDV, actualRa, actualDec);
    }

    return false;
}

// Private methods

void AlignmentSubsystemForDrivers::MyDatabaseLoadCallback(void *ThisPointer)
{
    ((AlignmentSubsystemForDrivers *)ThisPointer)->Initialise((AlignmentSubsystemForDrivers *)ThisPointer);
}

} // namespace AlignmentSubsystem
} // namespace INDI
