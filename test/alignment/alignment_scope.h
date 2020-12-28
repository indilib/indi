#pragma once

#include <inditelescope.h>
#include <indicom.h>
#include <alignment/AlignmentSubsystemForDrivers.h>

using namespace INDI::AlignmentSubsystem;

double decimalHoursToDecimalDegrees(double decimalHours)
{
    return (decimalHours * 360.0) / 24.0;
}

double decimalDegreesToDecimalHours(double decimalDegrees)
{
    return (decimalDegrees * 24.0) / 360.0;
}

class Scope : public INDI::Telescope, public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
public:
    Scope()
    {
    }

    virtual const char *getDefaultName() override
    {
        return "AlignmentScope";
    }

    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override
    {
        if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
            ProcessAlignmentNumberProperties(this, name, values, names, n);
        return true;
    }
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override
    {
        if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
            ProcessAlignmentTextProperties(this, name, texts, names, n);
        return true;
    }
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override
    {
        if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
            ProcessAlignmentSwitchProperties(this, name, states, names, n);
        return true;
    }
    bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                   char *formats[], char *names[], int n) override
    {
        if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
            ProcessAlignmentBLOBProperties(this, name, sizes, blobsizes, blobs, formats, names, n);
        return true;
    }
    void ISGetProperties(const char *dev) override
    {
        INDI_UNUSED(dev);
    }
    bool ISSnoopDevice(XMLEle *root) override
    {
        INDI_UNUSED(root);
        return true;
    }

    virtual bool initProperties() override
    {
        INDI::Telescope::initProperties();
        InitAlignmentProperties(this);
        return true;
    }

    virtual bool Handshake() override
    {
        Initialise(this);

        GetAlignmentDatabase().clear();
        UpdateSize();

        SetApproximateMountAlignmentFromMountType(EQUATORIAL);

        return INDI::Telescope::Handshake();
    }

    virtual bool updateLocation(double latitude, double longitude, double elevation) override
    {
        UpdateLocation(latitude, longitude, elevation);
        updateObserverLocation(latitude, longitude, elevation);
        return true;
    }

    virtual bool ReadScopeStatus() override
    {
        // TODO: Implement your own code to read the RA/Dec from the scope.
        // mountRA should be in decimal hours.
        double mountRA = 0, mountDec = 0;

        ln_equ_posn eq = TelescopeEquatorialToSky(range24(mountRA), rangeDec(mountDec));

        NewRaDec(decimalDegreesToDecimalHours(eq.ra), eq.dec);

        return true;
    }

    virtual bool Sync(double ra, double dec) override
    {
        return AddAlignmentEntry(ra, dec);
    }

    // adds an entry to the alignment database.
    // ra is in decimal hours
    // returns true if an entry was added, otherwise false
    bool AddAlignmentEntry(double ra, double dec)
    {
        double LST = get_local_sidereal_time(lnobserver.lng);

        // This should be the mounts RA/Dec, but for now we are adding a "perfect" sync point.
        struct ln_equ_posn RaDec
        {
            0, 0
        };
        RaDec.ra = range360(((LST - ra) * 360.0) / 24.0);
        RaDec.dec = dec;

        AlignmentDatabaseEntry NewEntry;
        TelescopeDirectionVector TDV = TelescopeDirectionVectorFromLocalHourAngleDeclination(RaDec);

        NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
        NewEntry.RightAscension = ra;
        NewEntry.Declination = dec;
        NewEntry.TelescopeDirection = TDV;
        NewEntry.PrivateDataSize = 0;

        if (!CheckForDuplicateSyncPoint(NewEntry))
        {
            GetAlignmentDatabase().push_back(NewEntry);
            UpdateSize();
            Initialise(this);

            return true;
        }

        return false;
    }

    ln_equ_posn SkyToTelescopeEquatorial(double ra, double dec)
    {
        ln_equ_posn eq{0, 0};
        TelescopeDirectionVector TDV;
        double RightAscension, Declination;

        if (GetAlignmentDatabase().size() > 1)
        {
            if (TransformCelestialToTelescope(ra, dec, 0.0, TDV))
            {
                LocalHourAngleDeclinationFromTelescopeDirectionVector(TDV, eq);

                //  and now we have to convert from lha back to RA
                double LST = get_local_sidereal_time(lnobserver.lng);
                eq.ra = eq.ra * 24 / 360;
                RightAscension = LST - eq.ra;
                RightAscension = range24(RightAscension);
                Declination = eq.dec;
            }
            else
            {
                RightAscension = ra;
                Declination = dec;
            }
        }
        else
        {
            RightAscension = ra;
            Declination = dec;
        }

        // again, ln_equ_posn should have ra in decimal degrees
        eq.ra = decimalHoursToDecimalDegrees(RightAscension);
        eq.dec = Declination;

        return eq;
    }

    // uses the alignment subsystem to convert a mount RA/Dec to actual RA/Dec.
    // ra should be in decimal hours
    // ra and dec are where the mount thinks it is.
    // Returns ln_eq_posn with decimal degrees for RA and Dec.
    ln_equ_posn TelescopeEquatorialToSky(double ra, double dec)
    {
        double RightAscension, Declination;
        ln_equ_posn eq{0, 0};

        if (GetAlignmentDatabase().size() > 1)
        {
            TelescopeDirectionVector TDV;

            double lha, lst;
            lst = get_local_sidereal_time(lnobserver.lng);
            lha = get_local_hour_angle(lst, ra);

            eq.ra = decimalHoursToDecimalDegrees(lha);
            eq.dec = dec;

            TDV = TelescopeDirectionVectorFromLocalHourAngleDeclination(eq);

            if (!TransformTelescopeToCelestial(TDV, RightAscension, Declination))
            {
                RightAscension = decimalDegreesToDecimalHours(eq.ra);
                Declination = eq.dec;
            }
        }
        else
        {
            // With less than 2 align points
            // Just return raw data
            RightAscension = ra;
            Declination = dec;
        }

        // again, ln_equ_posn should have ra in decimal degrees
        eq.ra = decimalHoursToDecimalDegrees(RightAscension);
        eq.dec = Declination;

        return eq;
    }
};

void ISGetProperties(const char *dev)
{
    INDI_UNUSED(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(states);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(texts);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(values);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    INDI_UNUSED(root);
}
