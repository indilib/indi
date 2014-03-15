/*!
 * \file skywatcherAPIMount.h
 *
 * \author Roger James
 * \author Gerry Rozema
 * \author Jean-Luc Geehalel
 * \date 13th November 2013
 *
 * This file contains the definitions for a C++ implementatiom of a INDI telescope driver using the Skywatcher API.
 * It is based on work from three sources.
 * A C++ implementation of the API by Roger James.
 * The indi_eqmod driver by Jean-Luc Geehalel.
 * The synscanmount driver by Gerry Rozema.
 */

#ifndef SKYWATCHERAPIMOUNT_H
#define SKYWATCHERAPIMOUNT_H

#include "indibase/alignment/AlignmentSubsystemForDrivers.h"

#include "skywatcherAPI.h"

class SkywatcherAPIMount : public SkywatcherAPI, public INDI::Telescope, public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
public:
    SkywatcherAPIMount();
    virtual ~SkywatcherAPIMount();

    //  overrides of base class virtual functions
    virtual bool Abort();
    virtual bool canSync();
    virtual bool Connect();
    virtual const char *getDefaultName();
    virtual bool Goto(double,double);
    virtual bool initProperties();
    virtual void ISGetProperties (const char *dev);
    virtual bool ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool MoveNS(TelescopeMotionNS dir);
    virtual bool MoveWE(TelescopeMotionWE dir);
    virtual bool Park();
    virtual bool ReadScopeStatus();
    virtual bool saveConfigItems(FILE *fp);
    virtual bool Sync(double ra, double dec);
    virtual void TimerHit();
    virtual bool updateLocation(double latitude, double longitude, double elevation);
    virtual bool updateProperties();

private:
    // Overrides for the pure virtual functions in SkyWatcherAPI
    int skywatcher_tty_read(int fd, char *buf, int nbytes, int timeout, int *nbytes_read);
    int skywatcher_tty_write(int fd, const char * buffer, int nbytes, int *nbytes_written);

    void SkywatcherMicrostepsFromTelescopeDirectionVector(const INDI::AlignmentSubsystem::TelescopeDirectionVector TelescopeDirectionVector,
                                                            long& Axis1Microsteps, long& Axis2Microsteps);
    const INDI::AlignmentSubsystem::TelescopeDirectionVector TelescopeDirectionVectorFromSkywatcherMicrosteps(long Axis1Microsteps, long Axis2Microsteps);

    void UpdateDetailedMountInformation(bool InformClient);

    // Properties

    static const char* DetailedMountInfoPage;
    enum { MOTOR_CONTROL_FIRMWARE_VERSION, MOUNT_CODE, IS_DC_MOTOR };
    INumber BasicMountInfo[3];
    INumberVectorProperty BasicMountInfoV;
    enum { MT_EQ6, MT_HEQ5, MT_EQ5, MT_EQ3, MT_GT, MT_MF, MT_114GT, MT_DOB, MT_UNKNOWN };
    ISwitch MountType[9];
    ISwitchVectorProperty MountTypeV;
    enum { MICROSTEPS_PER_REVOLUTION, STEPPER_CLOCK_FREQUENCY, HIGH_SPEED_RATIO, MICROSTEPS_PER_WORM_REVOLUTION };
    INumber AxisOneInfo[4];
    INumberVectorProperty AxisOneInfoV;
    INumber AxisTwoInfo[4];
    INumberVectorProperty AxisTwoInfoV;
    enum { FULL_STOP, SLEWING, SLEWING_TO, SLEWING_FORWARD, HIGH_SPEED, NOT_INITIALISED };
    ISwitch AxisOneState[6];
    ISwitchVectorProperty AxisOneStateV;
    ISwitch AxisTwoState[6];
    ISwitchVectorProperty AxisTwoStateV;
    enum { RAW_MICROSTEPS, OFFSET_FROM_INITIAL, DEGREES_FROM_INITIAL };
    INumber AxisOneEncoderValues[3];
    INumberVectorProperty AxisOneEncoderValuesV;
    INumber AxisTwoEncoderValues[3];
    INumberVectorProperty AxisTwoEncoderValuesV;

    // Previous motion direction
    typedef enum { PREVIOUS_NS_MOTION_NORTH = MOTION_NORTH,
                    PREVIOUS_NS_MOTION_SOUTH = MOTION_SOUTH,
                    PREVIOUS_NS_MOTION_UNKNOWN = -1} PreviousNSMotion_t;
    PreviousNSMotion_t PreviousNSMotion;
    typedef enum { PREVIOUS_WE_MOTION_WEST = MOTION_WEST,
                    PREVIOUS_WE_MOTION_EAST = MOTION_EAST,
                    PREVIOUS_WE_MOTION_UNKNOWN = -1} PreviousWEMotion_t;
    PreviousWEMotion_t PreviousWEMotion;

    // Tracking
    ln_equ_posn CurrentTrackingTarget;
    long OldTrackingTarget[2];

#ifdef USE_INITIAL_JULIAN_DATE
    double InitialJulianDate;
#endif
};

#endif // SKYWATCHERAPIMOUNT_H
