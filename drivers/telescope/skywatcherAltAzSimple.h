/*!
 * \file SkywatcherAltAzSimple.h
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

#pragma once

#include "indiguiderinterface.h"
#include "skywatcherAPI.h"

typedef enum { PARK_COUNTERCLOCKWISE = 0, PARK_CLOCKWISE } ParkDirection_t;
typedef enum { PARK_NORTH = 0, PARK_EAST, PARK_SOUTH, PARK_WEST } ParkPosition_t;

struct GuidingPulse
{
    float DeltaAlt { 0 };
    float DeltaAz { 0 };
};


class SkywatcherAltAzSimple : public SkywatcherAPI,
    public INDI::Telescope,
    public INDI::GuiderInterface
{
    public:
        SkywatcherAltAzSimple();
        virtual ~SkywatcherAltAzSimple() = default;

        //  overrides of base class virtual functions
        virtual bool Abort() override;
        virtual bool Handshake() override;
        virtual const char *getDefaultName() override;
        virtual bool Goto(double ra, double dec) override;
        virtual bool initProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                               char *formats[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        double GetSlewRate();
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        double GetParkDeltaAz(ParkDirection_t target_direction, ParkPosition_t target_position);
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool ReadScopeStatus() override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual bool Sync(double ra, double dec) override;
        virtual void TimerHit() override;
        virtual bool updateProperties() override;
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

    private:
        void ResetGuidePulses();
        void UpdateScopeConfigSwitch();
        int recover_tty_reconnect();
        void UpdateDetailedMountInformation(bool InformClient);
        INDI::IHorizontalCoordinates GetAltAzPosition(double ra, double dec, double offset_in_sec = 0);
        INDI::IEquatorialCoordinates GetRaDecPosition(double alt, double az);
        void LogMessage(const char* format, ...);

        // Properties

        static constexpr const char *DetailedMountInfoPage { "Detailed Mount Information" };
        enum
        {
            MOTOR_CONTROL_FIRMWARE_VERSION,
            MOUNT_CODE,
            MOUNT_NAME,
            IS_DC_MOTOR
        };
        IText BasicMountInfoT[4] {};
        ITextVectorProperty BasicMountInfoTP;

        enum
        {
            MICROSTEPS_PER_REVOLUTION,
            STEPPER_CLOCK_FREQUENCY,
            HIGH_SPEED_RATIO,
            MICROSTEPS_PER_WORM_REVOLUTION
        };
        INumber AxisOneInfoN[4];
        INumberVectorProperty AxisOneInfoNP;
        INumber AxisTwoInfoN[4];
        INumberVectorProperty AxisTwoInfoNP;
        enum
        {
            FULL_STOP,
            SLEWING,
            SLEWING_TO,
            SLEWING_FORWARD,
            HIGH_SPEED,
            NOT_INITIALISED
        };
        ISwitch AxisOneStateS[6];
        ISwitchVectorProperty AxisOneStateSP;
        ISwitch AxisTwoStateS[6];
        ISwitchVectorProperty AxisTwoStateSP;
        enum
        {
            RAW_MICROSTEPS,
            MICROSTEPS_PER_ARCSEC,
            OFFSET_FROM_INITIAL,
            DEGREES_FROM_INITIAL
        };
        INumber AxisOneEncoderValuesN[4];
        INumberVectorProperty AxisOneEncoderValuesNP;
        INumber AxisTwoEncoderValuesN[4];
        INumberVectorProperty AxisTwoEncoderValuesNP;

        // A switch for silent/highspeed slewing modes
        enum
        {
            SLEW_SILENT,
            SLEW_NORMAL
        };
        ISwitch SlewModesS[2];
        ISwitchVectorProperty SlewModesSP;

        // A switch for wedge mode
        enum
        {
            WEDGE_SIMPLE,
            WEDGE_EQ,
            WEDGE_DISABLED
        };
        ISwitch WedgeModeS[3];
        ISwitchVectorProperty WedgeModeSP;

        // A switch for tracking logging
        enum
        {
            TRACKLOG_ENABLED,
            TRACKLOG_DISABLED
        };
        ISwitch TrackLogModeS[2];
        ISwitchVectorProperty TrackLogModeSP;

        // Guiding rates (RA/Dec)
        INumber GuidingRatesN[2];
        INumberVectorProperty GuidingRatesNP;

        // Tracking values
        INumber TrackingValuesN[3];
        INumberVectorProperty TrackingValuesNP;

        // A switch for park movement directions (clockwise/counterclockwise)
        ISwitch ParkMovementDirectionS[2];
        ISwitchVectorProperty ParkMovementDirectionSP;

        // A switch for park positions
        ISwitch ParkPositionS[4];
        ISwitchVectorProperty ParkPositionSP;

        // A switch for unpark positions
        ISwitch UnparkPositionS[4];
        ISwitchVectorProperty UnparkPositionSP;

        // Tracking
        INDI::IEquatorialCoordinates CurrentTrackingTarget { 0, 0 };
        long OldTrackingTarget[2] { 0, 0 };
        INDI::IHorizontalCoordinates CurrentAltAz { 0, 0 };
        bool ResetTrackingSeconds { false };
        int TrackingMsecs { 0 };
        int TrackingStartTimer { 0 };
        double GuideDeltaAlt { 0 };
        double GuideDeltaAz { 0 };
        int TimeoutDuration { 500 };
        const std::string TrackLogFileName;
        int UpdateCount { 0 };

        /// Save the serial port name
        std::string SerialPortName;
        /// Recover after disconnection
        bool RecoverAfterReconnection { false };
        bool VerboseScopeStatus { false };

        std::vector<GuidingPulse> GuidingPulses;

        bool moving { false };
};
