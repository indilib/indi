#pragma once

#include <inditelescope.h>
#include <indicom.h>
#include <alignment/AlignmentSubsystemForDrivers.h>

using namespace INDI::AlignmentSubsystem;

class Scope : public INDI::Telescope, public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
public:
    virtual const char *getDefaultName() override
    {
        return "AlignmentScope";
    }

    virtual bool ReadScopeStatus() override
    {
        return true;
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

protected:
    virtual bool Sync(double ra, double dec) override
    {
        // These values should be the location of where the mount thinks it is.
        double mountRa = ra, mountDec = dec;

        AlignmentDatabaseEntry NewEntry;
        struct ln_equ_posn RaDec;
        RaDec.ra = range360((mountRa * 360.0) / 24.0); // Convert decimal hours to decimal degrees
        RaDec.dec = rangeDec(mountDec);

        TelescopeDirectionVector TDV = TelescopeDirectionVectorFromEquatorialCoordinates(RaDec);

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
            ReadScopeStatus();
        }

        return true;
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
