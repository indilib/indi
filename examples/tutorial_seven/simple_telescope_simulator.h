
#pragma once

#include "indiguiderinterface.h"
#include "inditelescope.h"
#include "alignment/AlignmentSubsystemForDrivers.h"

class ScopeSim : public INDI::Telescope, public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
public:
    ScopeSim();

protected:
    bool Abort() override;
    bool canSync();
    bool Connect() override;
    bool Disconnect() override;
    const char *getDefaultName() override;
    bool Goto(double ra, double dec) override;
    bool initProperties() override;
    bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                           char *formats[], char *names[], int n) override;
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
    bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
    bool ReadScopeStatus() override;
    bool Sync(double ra, double dec) override;
    void TimerHit() override;
    bool updateLocation(double latitude, double longitude, double elevation) override;

private:
    static constexpr long MICROSTEPS_PER_REVOLUTION { 1000000 };
    static constexpr double MICROSTEPS_PER_DEGREE { MICROSTEPS_PER_REVOLUTION / 360.0 };
    static constexpr double DEFAULT_SLEW_RATE { MICROSTEPS_PER_DEGREE * 2.0 };
    static constexpr long MAX_DEC { (long)(90.0 * MICROSTEPS_PER_DEGREE) };
    static constexpr long MIN_DEC { (long)(-90.0 * MICROSTEPS_PER_DEGREE) };

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

    AxisStatus AxisStatusDEC { STOPPED };
    AxisDirection AxisDirectionDEC { FORWARD };
    double AxisSlewRateDEC { DEFAULT_SLEW_RATE };
    long CurrentEncoderMicrostepsDEC { 0 };
    long GotoTargetMicrostepsDEC { 0 };

    AxisStatus AxisStatusRA { STOPPED };
    AxisDirection AxisDirectionRA { FORWARD };
    double AxisSlewRateRA { DEFAULT_SLEW_RATE };
    long CurrentEncoderMicrostepsRA { 0 };
    long GotoTargetMicrostepsRA { 0 };

    // Tracking
    INDI::IEquatorialCoordinates CurrentTrackingTarget { 0, 0 };

    // Tracing in timer tick
    int TraceThisTickCount { 0 };
    bool TraceThisTick { false };

    unsigned int DBG_SIMULATOR { 0 };
};
