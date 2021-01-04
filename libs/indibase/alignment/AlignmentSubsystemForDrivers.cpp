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

        bool AlignmentSubsystemForDrivers::AddAlignmentEntryEquatorial(double actualRA, double actualDec, double mountRA, double mountDec)
        {
            ln_lnlat_posn location;
            if (!GetDatabaseReferencePosition(location))
            {
                return false;
            }

            double LST = get_local_sidereal_time(location.lng);
            struct ln_equ_posn RaDec
            {
                0, 0
            };
            RaDec.ra = range360(((LST - mountRA) * 360.0) / 24.0);
            RaDec.dec = mountDec;

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

        bool AlignmentSubsystemForDrivers::SkyToTelescopeEquatorial(double actualRA, double actualDec, double &mountRA, double &mountDec)
        {
            ln_equ_posn eq{0, 0};
            TelescopeDirectionVector TDV;
            ln_lnlat_posn location;

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
                    double LST = get_local_sidereal_time(location.lng);
                    eq.ra = eq.ra * 24 / 360;
                    mountRA = range24(LST - eq.ra);
                    mountDec = eq.dec;

                    return true;
                }
            }

            return false;
        }

        bool AlignmentSubsystemForDrivers::TelescopeEquatorialToSky(double mountRA, double mountDec, double &actualRA, double &actualDec)
        {
            ln_equ_posn eq{0, 0};
            ln_lnlat_posn location;

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

                double lha, lst;
                lst = get_local_sidereal_time(location.lng);
                lha = get_local_hour_angle(lst, mountRA);

                eq.ra = lha * 360.0 / 24.0;
                eq.dec = mountDec;

                TDV = TelescopeDirectionVectorFromLocalHourAngleDeclination(eq);

                return TransformTelescopeToCelestial(TDV, actualRA, actualDec);
            }

            return false;
        }

        bool AlignmentSubsystemForDrivers::AddAlignmentEntryAltAz(double actualRA, double actualDec, double mountAlt, double mountAz)
        {
            ln_lnlat_posn location;
            if (!GetDatabaseReferencePosition(location))
            {
                return false;
            }

            struct ln_hrz_posn AltAz
            {
                0, 0
            };
            AltAz.alt = range360(mountAlt);
            AltAz.az = range360(mountAz);

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
            ln_hrz_posn altAz{0, 0};
            TelescopeDirectionVector TDV;
            ln_lnlat_posn location;

            if (!GetDatabaseReferencePosition(location))
            {
                return false;
            }

            if (GetAlignmentDatabase().size() > 1)
            {
                if (TransformCelestialToTelescope(actualRA, actualDec, 0.0, TDV))
                {
                    AltitudeAzimuthFromTelescopeDirectionVector(TDV, altAz);

                    mountAlt = range360(altAz.alt);
                    mountAz = range360(altAz.az);

                    return true;
                }
            }

            return false;
        }

        bool AlignmentSubsystemForDrivers::TelescopeAltAzToSky(double mountAlt, double mountAz, double &actualRa, double &actualDec)
        {
            ln_hrz_posn altaz{0, 0};
            ln_lnlat_posn location;

            if (!GetDatabaseReferencePosition(location))
            {
                return false;
            }

            if (GetAlignmentDatabase().size() > 1)
            {
                TelescopeDirectionVector TDV;

                altaz.alt = range360(mountAlt);
                altaz.az = range360(mountAz);

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
