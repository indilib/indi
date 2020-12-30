#pragma once

#include <inditelescope.h>
#include <indicom.h>
#include <alignment/AlignmentSubsystemForDrivers.h>

using namespace INDI::AlignmentSubsystem;

char _me[] = "MockAlignmentScope";
char *me = _me;

class Scope : public INDI::Telescope, public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
public:
    Scope()
    {
        ISGetProperties("");
    }

    virtual const char *getDefaultName() override
    {
        return "MockAlignmentScope";
    }

    // make sure to pass new values into the alignment subsytem with all the ISNew* methods
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
        INDI::Telescope::ISGetProperties(dev);
    }
    bool ISSnoopDevice(XMLEle *root) override
    {
        return INDI::Telescope::ISSnoopDevice(root);
    }

    virtual bool initProperties() override
    {
        INDI::Telescope::initProperties();
        // initialize the alignment subsystem properties AFTER creating the base telescope properties
        InitAlignmentProperties(this);
        return true;
    }

    virtual bool Handshake() override
    {
        // should be called before Initialise
        SetApproximateMountAlignmentFromMountType(EQUATORIAL);

        // the next two lines reset the alignment database
        // skip if you want to reuse your model
        // these also need to be called before Initialise
        GetAlignmentDatabase().clear();
        UpdateSize();

        Initialise(this);

        return INDI::Telescope::Handshake();
    }

    virtual bool updateLocation(double latitude, double longitude, double elevation) override
    {
        // call UpdateLocation in the alignment subsystem
        UpdateLocation(latitude, longitude, elevation);

        // call updateObserverLocation on the telescope base class
        // this sets lnobserver values so we don't need to peek the indi properties
        updateObserverLocation(latitude, longitude, elevation);
        return true;
    }

    virtual bool ReadScopeStatus() override
    {
        // TODO: Implement your own code to read the RA/Dec from the scope.
        // mountRA should be in decimal hours.
        double mountRA = 0, mountDec = 0;
        double actualRA, actualDec;

        // use the alignment subsystem to convert where the mount thinks it is
        // to where the alignment subsystem calculates we are actually pointing
        TelescopeEquatorialToSky(range24(mountRA), rangeDec(mountDec), actualRA, actualDec);

        // tell the driver where we are pointing
        NewRaDec(actualRA, actualDec);

        return true;
    }

    virtual bool Sync(double ra, double dec) override
    {
        // In an actual driver, you would get the mounts RA/Dec and use them here.
        // For the test class, we are assuming a "perfect" sync.
        double mountRA = ra, mountDec = dec;

        return AddAlignmentEntryEquatorial(ra, dec, mountRA, mountDec);
    }

    virtual bool Goto(double ra, double dec) override
    {
        double mountRA, mountDec;
        SkyToTelescopeEquatorial(ra, dec, mountRA, mountDec);

        // In an actual driver, you would send the mount to mountRA/mountDec
        // here.

        return true;
    }
};

// static std::unique_ptr<Scope> scope(new Scope());

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
