#ifndef NEXSTAREVO_H
#define NEXSTAREVO_H

#include <indiguiderinterface.h>
#include <inditelescope.h>
#include <alignment/AlignmentSubsystemForDrivers.h>
#include "NexStarAUXScope.h"


class NexStarEvo : public INDI::Telescope, public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
public:
    NexStarEvo();
    ~NexStarEvo();

    virtual void ISGetProperties (const char *dev);
    virtual bool ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

protected:
    virtual bool initProperties();
    virtual bool saveConfigItems(FILE *fp);
    virtual bool Connect();
    virtual bool Disconnect();

    virtual const char *getDefaultName();

    virtual bool Sync(double ra, double dec);
    virtual bool Goto(double ra,double dec);
    virtual bool Abort();
    virtual bool Park();
    virtual bool UnPark();


    // TODO: Switch to AltAz from N-S/W-E
    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command);
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command);

    virtual bool ReadScopeStatus();
    virtual void TimerHit();

    virtual bool updateLocation(double latitude, double longitude, double elevation);
    
    virtual bool trackingRequested();

private:
    static const long STEPS_PER_REVOLUTION;
    static const double STEPS_PER_DEGREE;
    static const double DEFAULT_SLEW_RATE;
    static const double TRACK_SCALE;
    static const long MAX_ALT;
    static const long MIN_ALT;

    NexStarAUXScope *scope;

    enum ScopeStatus_t {IDLE, SLEWING_FAST, APPROACH, SLEWING_SLOW, SLEWING_MANUAL, TRACKING};
    ScopeStatus_t ScopeStatus;

    enum AxisStatus { STOPPED, SLEWING };
    enum AxisDirection { FORWARD, REVERSE };
    
    AxisStatus AxisStatusALT;
    AxisDirection AxisDirectionALT;
    double AxisSlewRateALT;
    long CurrentALT;
    long GotoTargetALT;
    long ApproachALT;

    AxisStatus AxisStatusAZ;
    AxisDirection AxisDirectionAZ;
    double AxisSlewRateAZ;
    long CurrentAZ;
    long GotoTargetAZ;
    long ApproachAZ;

    // Previous motion direction
    // TODO: Switch to AltAz from N-S/W-E
    typedef enum { PREVIOUS_NS_MOTION_NORTH = DIRECTION_NORTH,
                    PREVIOUS_NS_MOTION_SOUTH = DIRECTION_SOUTH,
                    PREVIOUS_NS_MOTION_UNKNOWN = -1} PreviousNSMotion_t;
    PreviousNSMotion_t PreviousNSMotion;
    typedef enum { PREVIOUS_WE_MOTION_WEST = DIRECTION_WEST,
                    PREVIOUS_WE_MOTION_EAST = DIRECTION_EAST,
                    PREVIOUS_WE_MOTION_UNKNOWN = -1} PreviousWEMotion_t;
    PreviousWEMotion_t PreviousWEMotion;

    // GoTo
    ln_equ_posn GoToTarget;

    // Tracking
    ln_equ_posn CurrentTrackingTarget;
    ln_equ_posn NewTrackingTarget;
    long OldTrackingTarget[2];

    // Tracing in timer tick
    int TraceThisTickCount;
    bool TraceThisTick;

    unsigned int DBG_NSEVO;
    
    // Device IP port
    INumberVectorProperty IPPortNP;
    INumber IPPortN[1];

    // Device IP address
    ITextVectorProperty IPAddressTP;
    IText IPAddressT[1];


};

#endif // NEXSTAREVO_H
