
#pragma once

#include "indiguiderinterface.h"
#include "inditelescope.h"
#include "alignment/AlignmentSubsystemForDrivers.h"

class ScopeSim : public INDI::Telescope, public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
  public:
    ScopeSim();

private:
    virtual bool Abort() override;
    bool canSync();
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual const char *getDefaultName() override;
    virtual bool Goto(double ra, double dec) override;
    virtual bool initProperties() override;
    friend void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                          char *formats[], char *names[], int n);
    virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                           char *formats[], char *names[], int n) override;
    friend void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    friend void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    friend void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
    virtual bool ReadScopeStatus() override;
    virtual bool Sync(double ra, double dec) override;
    virtual void TimerHit() override;
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;

    static const long MICROSTEPS_PER_REVOLUTION;
    static const double MICROSTEPS_PER_DEGREE;
    static const double DEFAULT_SLEW_RATE;
    static const long MAX_DEC;
    static const long MIN_DEC;

    enum AxisStatus
    {
        STOPPED,
        SLEWING,
        SLEWING_TO
    };
    enum AxisDirection
    {
        FORWARD,
        REVERSE
    };

    AxisStatus AxisStatusDEC;
    AxisDirection AxisDirectionDEC;
    double AxisSlewRateDEC;
    long CurrentEncoderMicrostepsDEC;
    long GotoTargetMicrostepsDEC;

    AxisStatus AxisStatusRA;
    AxisDirection AxisDirectionRA;
    double AxisSlewRateRA;
    long CurrentEncoderMicrostepsRA;
    long GotoTargetMicrostepsRA;

    // Tracking
    ln_equ_posn CurrentTrackingTarget;

    // Tracing in timer tick
    int TraceThisTickCount;
    bool TraceThisTick;

    unsigned int DBG_SIMULATOR;
};
