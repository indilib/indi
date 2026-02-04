/*!
 * \file skywatcherAPI.cpp
 *
 * \author Roger James
 * \author Gerry Rozema
 * \author Jean-Luc Geehalel
 * \date 13th November 2013
 *
 * This file contains the definitions for a C++ implementatiom of the Skywatcher API.
 * It is based on work from four sources.
 * A C++ implementation of the API by Roger James.
 * The indi_eqmod driver by Jean-Luc Geehalel.
 * The synscanmount driver by Gerry Rozema.
 * The C# implementation published by Skywatcher/Synta
*/

#pragma once

#include <string>
#include <map>

#define INDI_DEBUG_LOGGING
#ifdef INDI_DEBUG_LOGGING
#include "inditelescope.h"
#define MYDEBUG(priority, msg) \
    INDI::Logger::getInstance().print(pChildTelescope->getDeviceName(), priority, __FILE__, __LINE__, msg)
#define MYDEBUGF(priority, msg, ...) \
    INDI::Logger::getInstance().print(pChildTelescope->getDeviceName(), priority, __FILE__, __LINE__, msg, __VA_ARGS__)
#else
#define MYDEBUG(priority, msg)
#define MYDEBUGF(priority, msg, ...)
#endif

struct AXISSTATUS
{
    AXISSTATUS()
        : FullStop(false), Slewing(false), SlewingTo(false), SlewingForward(false), HighSpeed(false),
          NotInitialized(true)
    {
    }
    bool FullStop;
    bool Slewing;
    bool SlewingTo;
    bool SlewingForward;
    bool HighSpeed;
    bool NotInitialized;

    void SetFullStop();
    void SetSlewing(bool forward, bool highspeed);
    void SetSlewingTo(bool forward, bool highspeed);
};

class SkywatcherAPI
{
    public:
        enum AXISID
        {
            AXIS1 = 0,
            AXIS2 = 1
        };

        // Types
        enum SkywatcherCommand
        {
            Initialize                = 'F',
            InquireMotorBoardVersion  = 'e',
            InquireGridPerRevolution  = 'a',
            InquireTimerInterruptFreq = 'b',
            InquireHighSpeedRatio     = 'g',
            InquirePECPeriod          = 's',
            InstantAxisStop           = 'L',
            NotInstantAxisStop        = 'K',
            SetAxisPositionCmd        = 'E',
            GetAxisPosition           = 'j',
            GetAxisStatus             = 'f',
            SetSnapPort               = 'O', // EQ8/AZEQ6/AZEQ5/EQ6-R/AZ-GTi only
            SetMotionMode             = 'G',
            SetGotoTargetIncrement    = 'H',
            SetBreakPointIncrement    = 'M',
            SetGotoTarget             = 'S',
            SetBreakStep              = 'U',
            SetStepPeriod             = 'I',
            StartMotion               = 'J',
            GetStepPeriod             = 'D', // See Merlin protocol http://www.papywizard.org/wiki/DevelopGuide
            ActivateMotor             = 'B', // See eq6direct implementation http://pierre.nerzic.free.fr/INDI/
            SetST4GuideRateCmd        = 'P',
            GetHomePosition           = 'd', // Get Home position encoder count (default at startup)
            SetFeatureCmd             = 'W', // EQ8/AZEQ6/AZEQ5 only
            GetFeatureCmd             = 'q', // EQ8/AZEQ6/AZEQ5 only
            InquireAuxEncoder         = 'd', // EQ8/AZEQ6/AZEQ5 only
            SetPolarScopeLED          = 'V',
        };

        enum SkywatcherGetFeatureCmd
        {
            GET_INDEXER_CMD  = 0x00,
            GET_FEATURES_CMD = 0x01
        };

        enum SkywatcherSetFeatureCmd
        {
            START_PPEC_TRAINING_CMD            = 0x00,
            STOP_PPEC_TRAINING_CMD             = 0x01,
            TURN_PPEC_ON_CMD                   = 0x02,
            TURN_PPEC_OFF_CMD                  = 0X03,
            ENCODER_ON_CMD                     = 0x04,
            ENCODER_OFF_CMD                    = 0x05,
            DISABLE_FULL_CURRENT_LOW_SPEED_CMD = 0x0006,
            ENABLE_FULL_CURRENT_LOW_SPEED_CMD  = 0x0106,
            RESET_HOME_INDEXER_CMD             = 0x08
        };


        // These values are in radians per second
        static constexpr double SIDEREALRATE { (2 * M_PI / 86164.09065) };
        static constexpr double MAX_SPEED { 500.0 };
        static constexpr double LOW_SPEED_MARGIN { 128.0 * SIDEREALRATE };

        SkywatcherAPI();
        virtual ~SkywatcherAPI() = default;

        unsigned long BCDstr2long(std::string &String);
        unsigned long Highstr2long(std::string &String);
        bool CheckIfDCMotor();

        /// \brief Check if the current mount is a Virtuoso (AltAz)
        /// \return True if the current mount is Virtuoso otherwise false.
        //bool IsVirtuosoMount() const;

        /// \brief Check if the current mount is a Merlin (AltAz)
        /// \return True if the current mount is Merlin otherwise false.
        //bool IsMerlinMount() const;

        /// \brief Check if the current mount is AZ GTi
        /// \return True if the current mount is AZ GTi otherwise false.
        //bool IsAZGTiMount() const;

        /// \brief Convert a slewing rate in degrees per second into the required
        /// clock ticks per microstep setting.
        /// \param[in] Axis - The axis to use.
        /// \param[in] DegreesPerSecond - Slewing rate in degrees per second
        /// \return Clock ticks per microstep for the requested rate
        long DegreesPerSecondToClocksTicksPerMicrostep(AXISID Axis, double DegreesPerSecond);

        /// \brief Convert angle in degrees to microsteps
        /// \param[in] Axis - The axis to use.
        /// \param[in] AngleInRadians - the angle in degrees.
        /// \return the number of microsteps
        long DegreesToMicrosteps(AXISID Axis, double AngleInDegrees);

        /// \brief Set the CurrentEncoders status variable to the current
        /// encoder value in microsteps for the specified axis.
        /// \return false failure
        bool GetEncoder(AXISID Axis);

        /// \brief Set the HighSpeedRatio status variable to the ratio between
        /// high and low speed stepping modes.
        bool GetHighSpeedRatio(AXISID Axis);

        /// \brief Set the MicrostepsPerRevolution status variable to the number of microsteps
        /// for a 360 degree revolution of the axis.
        /// \param[in] Axis - The axis to use.
        /// \return false failure
        bool GetMicrostepsPerRevolution(AXISID Axis);

        /// \brief Set the MicrostepsPermWormRevolution status variable to the number of microsteps
        /// for a 360 degree revolution of the worm gear.
        /// \param[in] Axis - The axis to use.
        /// \return false failure
        bool GetMicrostepsPerWormRevolution(AXISID Axis);

        bool GetMotorBoardVersion(AXISID Axis);

        typedef enum { CLOCKWISE, ANTICLOCKWISE } PositiveRotationSense_t;

        /// \brief Returns the rotation direction for a positive step on the
        /// designated axis.
        /// \param[in] Axis - The axis to use.
        /// \return The rotation sense clockwise or anticlockwise.
        ///
        /// Rotation directions are given looking down the axis towards the  motorised pier
        /// for an altitude or declination axis. Or down the pier towards the mount base
        /// for an azimuth or right ascension axis
        PositiveRotationSense_t GetPositiveRotationDirection(AXISID Axis);

        bool GetStatus(AXISID Axis);

        /// \brief Set the StepperClockFrequency status variable to fixed PIC timer interrupt
        /// frequency (ticks per second).
        /// \return false failure
        bool GetStepperClockFrequency(AXISID Axis);

        // Optional: inquire axis features from mount.
        bool InquireFeatures();

        bool InitializeMC();

        /// \brief Initialize the communication to the mount
        /// \return True if successful otherwise false
        bool InitMount();

        bool HasHomeIndexers();
        bool HasAuxEncoders();
        bool HasPPEC();
        bool HasSnapPort1();
        bool HasSnapPort2();
        bool HasPolarLed();

        void TurnEncoder(AXISID axis, bool on);
        void TurnRAEncoder(bool on);
        void TurnDEEncoder(bool on);
        void SetFeature(AXISID axis, uint32_t command);

        /// \brief Bring the axis to an immediate halt.
        /// N.B. This command could cause damage to the mount or telescope
        /// and should not normally be used except for emergency stops.
        /// \param[in] Axis - The axis to use.
        /// \return false failure
        bool InstantStop(AXISID Axis);

        void Long2BCDstr(long Number, std::string &String);

        /// \brief Convert microsteps to angle in degrees
        /// \param[in] Axis - The axis to use.
        /// \param[in] Microsteps
        /// \return the angle in degrees
        double MicrostepsToDegrees(AXISID Axis, long Microsteps);

        /// \brief Convert microsteps to angle in radians
        /// \param[in] Axis - The axis to use.
        /// \param[in] Microsteps
        /// \return the angle in radians
        double MicrostepsToRadians(AXISID Axis, long Microsteps);

        void PrepareForSlewing(AXISID Axis, double Speed);

        /// \brief Convert a slewing rate in radians per second into the required
        /// clock ticks per microstep setting.
        /// \param[in] Axis - The axis to use.
        /// \param[in] DegreesPerSecond - Slewing rate in degrees per second
        /// \return Clock ticks per microstep for the requested rate
        long RadiansPerSecondToClocksTicksPerMicrostep(AXISID Axis, double RadiansPerSecond);

        /// \brief Convert angle in radians to microsteps
        /// \param[in] Axis - The axis to use.
        /// \param[in] AngleInRadians - the angle in radians.
        /// \return the number of microsteps
        long RadiansToMicrosteps(AXISID Axis, double AngleInRadians);

        /// \brief Set axis encoder to the specified value.
        /// \param[in] Axis - The axis to use.
        /// \param[in] Microsteps - the value in microsteps.
        /// \return false failure
        bool SetEncoder(AXISID Axis, long Microsteps);

        /// \brief Set the goto target offset per the specified axis
        /// \param[in] Axis - The axis to use.
        /// \param[in] OffsetInMicrosteps - the value to use
        /// \return false failure
        bool SetGotoTargetOffset(AXISID Axis, long OffsetInMicrosteps);

        /// \brief Set the motion mode per the specified axis
        /// \param[in] Axis - The axis to use.
        /// \param[in] Func - the slewing mode
        /// - 0 = High speed SlewTo mode
        /// - 1 = Low speed Slew mode
        /// - 2 = Low speed SlewTo mode
        /// - 3 = High Speed Slew mode
        /// \param[in] Direction - the direction to slew in
        /// - 0 = Forward
        /// - 1 = Reverse
        /// \return false failure
        bool SetAxisMotionMode(AXISID Axis, char Func, char Direction);

        /// \brief Set the serail port to be usb for mount communication
        /// \param[in] port - an open file descriptor for the port to use.
        void SetSerialPort(int port)
        {
            MyPortFD = port;
        }

        /// \brief Set the PIC internal divider variable which determines
        /// how many clock interrupts have to occur between each microstep
        bool SetClockTicksPerMicrostep(AXISID Axis, long ClockTicksPerMicrostep);

        /// \brief Set the length of the deccelaration ramp for Slew mode.
        /// \param[in] Axis - The axis to use.
        /// \param[in] Microsteps - the length of the decceleration ramp in microsteps.
        /// \return false failure
        bool SetSlewModeDeccelerationRampLength(AXISID Axis, long Microsteps);

        /// \brief Set the length of the deccelaration ramp for SlewTo mode.
        /// \param[in] Axis - The axis to use.
        /// \param[in] Microsteps - the length of the decceleration ramp in microsteps.
        /// \return false failure
        bool SetSlewToModeDeccelerationRampLength(AXISID Axis, long Microsteps);

        /// \brief Toggle the snap port on or off
        /// \param[in] enabled - the state requested.
        bool toggleSnapPort(bool enabled);

        /// \brief Start the axis slewing at the given rate
        /// \param[in] Axis - The axis to use.
        /// \param[in] SpeedInRadiansPerSecond - the slewing speed
        /// \param[in] IgnoreSilentMode - ignore the silent mode even if set
        void Slew(AXISID Axis, double SpeedInRadiansPerSecond, bool IgnoreSilentMode = true);

        /// \brief Slew to the given offset and stop
        /// \param[in] Axis - The axis to use.
        /// \param[in] OffsetInMicrosteps - The number of microsteps to
        /// \param[in] verbose - Verbose mode
        /// slew from the current axis position.
        void SlewTo(AXISID Axis, long OffsetInMicrosteps, bool verbose = true);

        /// \brief Bring the axis to slow stop in the distance specified
        /// by SetSlewModeDeccelerationRampLength
        /// \param[in] Axis - The axis to use.
        /// \return false failure
        bool SlowStop(AXISID Axis);

        /// \brief Start the axis slewing in the previously selected mode
        /// \param[in] Axis - The axis to use.
        /// \return false failure
        bool StartAxisMotion(AXISID Axis);

        bool TalkWithAxis(AXISID Axis, SkywatcherCommand Command, std::string &cmdDataStr, std::string &responseStr);

        /// \brief Check if an axis is moving
        /// \param[in] Axis - The axis to check.
        /// \return True if the axis is moving otherwise false.
        bool IsInMotion(AXISID Axis);

        // Skywatcher mount status variables
        unsigned long MCVersion { 0 }; // Motor control board firmware version

        static const char *mountTypeToString(uint8_t type);

        enum MountType
        {
            EQ6              = 0x00,
            HEQ5             = 0x01,
            EQ5              = 0x02,
            EQ3              = 0x03,
            EQ8              = 0x04,
            AZEQ6            = 0x05,
            AZEQ5            = 0x06,
            STAR_ADVENTURER  = 0x0A,
            EQ8R_PRO         = 0x20,
            AZEQ6_PRO        = 0x22,
            EQ6_PRO          = 0x23,
            EQ5_PRO          = 0x31,
            WAVE_150I        = 0x45,
            GT               = 0x80,
            MF               = 0x81,
            _114GT           = 0x82,
            DOB              = 0x90,
            AZGTE            = 0xA2,
            AZGTI            = 0xA5,
        };

        typedef struct SkyWatcherFeatures
        {
            bool inPPECTraining = false;
            bool inPPEC = false;
            bool hasEncoder = false;
            bool hasPPEC = false;
            bool hasHomeIndexer = false;
            bool isAZEQ = false;
            bool hasPolarLed = false;
            bool hasCommonSlewStart = false; // supports :J3
            bool hasHalfCurrentTracking = false;
            bool hasWifi = false;
        } SkyWatcherFeatures;


        unsigned long MountCode { 0 };
        bool IsDCMotor { false };
        bool SilentSlewMode { true };

        // Values from mount
        long MicrostepsPerRevolution[2];     // Number of microsteps for 360 degree revolution
        long StepperClockFrequency[2];       // The stepper clock timer interrupt frequency in ticks per second
        long HighSpeedRatio[2];              // The speed multiplier for high speed mode.
        long MicrostepsPerWormRevolution[2]; // Number of microsteps for one revolution of the worm gear.
        SkyWatcherFeatures AxisFeatures[2];

        // Calculated values
        double RadiansPerMicrostep[2];
        double MicrostepsPerRadian[2];
        double DegreesPerMicrostep[2];
        double MicrostepsPerDegree[2];
        long LowSpeedGotoMargin[2];

        // SlewTo debugging
        long LastSlewToTarget[2];

        /// Current encoder values (microsteps).
        long CurrentEncoders[2];
        /// Polaris position (initial) encoder values (microsteps).
        long PolarisPositionEncoders[2];
        /// Zero position encoder values (microsteps).
        long ZeroPositionEncoders[2];

        AXISSTATUS AxesStatus[2];
        double SlewingSpeed[2];

    protected:
        // Custom debug level
        unsigned int DBG_SCOPE { 0 };

    private:
        int MyPortFD { 0 };
        /// default timeout in synscan app os 200ms and 2 retransmissions, so put there little bit more
        static constexpr uint8_t SKYWATCHER_MAX_RETRTY {5};
        static constexpr uint8_t SKYWATCHER_TIMEOUT_S  {0};
        static constexpr long    SKYWATCHER_TIMEOUT_US {300000};
        static constexpr uint8_t SKYWATCHER_MAX_CMD {16};

        static const std::map<int, std::string> errorCodes;

#ifdef INDI_DEBUG_LOGGING
    public:
        INDI::Telescope *pChildTelescope { nullptr };
#endif
};
