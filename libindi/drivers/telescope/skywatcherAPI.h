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

#ifndef SKYWATCHERAPI_H
#define SKYWATCHERAPI_H

#include <cmath>
#include <string>

#define INDI_DEBUG_LOGGING
#ifdef INDI_DEBUG_LOGGING
#include "indibase/inditelescope.h"
#define MYDEBUG(priority, msg) INDI::Logger::getInstance().print(pChildTelescope->getDeviceName(), priority, __FILE__, __LINE__, msg)
#define MYDEBUGF(priority, msg, ...) INDI::Logger::getInstance().print(pChildTelescope->getDeviceName(), priority, __FILE__, __LINE__,  msg, __VA_ARGS__)
#else
#define MYDEBUG(priority, msg)
#define MYDEBUGF(priority, msg, ...)
#endif

struct AXISSTATUS
{
    AXISSTATUS() : FullStop(false), Slewing(false), SlewingTo(false), SlewingForward(false), HighSpeed(false), NotInitialized(true) {}
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
    enum AXISID { AXIS1 = 0, AXIS2 = 1 };

    static const double SIDEREALRATE; // Radians per second
    static const double MAX_SPEED; // Radians per second
    static const double LOW_SPEED_MARGIN; // Radians per second

    SkywatcherAPI();
    virtual ~SkywatcherAPI();

    long BCDstr2long(std::string &String);
    bool CheckIfDCMotor();

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

    typedef enum { CLOCKWISE, ANTICLOCKWISE} PositiveRotationSense_t;

    /// \brief Returns the rotation direction for a positive step on the
    /// designated axis.
    /// \param[in] Axis - The axis to use.
    /// \return The rotation sense clockwise or anticlockwise.
    ///
    /// Rotation directions are given looking down the axis towards the  motorised pier
    /// for an altitude or declination axis. Or down the pier towards the mount base
    /// for an azimuth or right ascension axis
    const PositiveRotationSense_t GetPositiveRotationDirection(AXISID Axis);

    bool GetStatus(AXISID Axis);

    /// \brief Set the StepperClockFrequency status variable to fixed PIC timer interrupt
    /// frequency (ticks per second).
    /// \return false failure
    bool GetStepperClockFrequency(AXISID Axis);

    bool InitializeMC();
    bool InitMount();

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
    /// - 0 = Low speed SlewTo mode
    /// - 1 = Low speed Slew mode
    /// - 2 = High speed SlewTo mode
    /// - 3 = High Speed Slew mode
    /// \param[in] Direction - the direction to slew in
    /// - 0 = Forward
    /// - 1 = Reverse
    /// \return false failure
    bool SetMotionMode(AXISID Axis, char Func, char Direction);


    /// \brief Set the serail port to be usb for mount communication
    /// \param[in] port - an open file descriptor for the port to use.
    void SetSerialPort(int port) { MyPortFD = port; }

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

    /// \brief Set the camera control switch to the given state
    /// \param[in] OnOff - the state requested.
    bool SetSwitch(bool OnOff);

    /// \brief Start the axis slewing at the given rate
    /// \param[in] Axis - The axis to use.
    /// \param[in] SpeedInRadiansPerSecond - the slewiing speed
    void Slew(AXISID Axis, double SpeedInRadiansPerSecond);

    /// \brief Slew to the given offset and stop
    /// \param[in] Axis - The axis to use.
    /// \param[in] OffsetInMicrosteps - The number of microsteps to
    /// slew from the current axis position.
    void SlewTo(AXISID Axis, long OffsetInMicrosteps);

    /// \brief Bring the axis to slow stop in the distance specified
    /// by SetSlewModeDeccelerationRampLength
    /// \param[in] Axis - The axis to use.
    /// \return false failure
    bool SlowStop(AXISID Axis);

    /// \brief Start the axis slewing in the prevously selected mode
    /// \param[in] Axis - The axis to use.
    /// \return false failure
    bool StartMotion(AXISID Axis);

    bool TalkWithAxis(AXISID Axis, char Command, std::string& cmdDataStr, std::string& responseStr);

    // Skywatcher mount status variables
    long MCVersion; // Motor control board firmware version

    enum MountType { EQ6=0x00, HEQ5=0x01, EQ5=0x02, EQ3=0x03, GT=0x80, MF=0x81, _114GT=0x82, DOB=0x90 };
    long MountCode;
    bool IsDCMotor;

    // Values from mount
    long MicrostepsPerRevolution[2]; // Number of microsteps for 360 degree revolution
    long StepperClockFrequency[2]; // The stepper clock timer interrupt frequency in ticks per second
    long HighSpeedRatio[2]; // The speed multiplier for high speed mode.
    long MicrostepsPerWormRevolution[2]; // Number of microsteps for one revolution of the worm gear.

    // Calculated values
    double RadiansPerMicrostep[2];
    double MicrostepsPerRadian[2];
    double DegreesPerMicrostep[2];
    double MicrostepsPerDegree[2];
    long LowSpeedGotoMargin[2];

    // SlewTo debugging
    long LastSlewToTarget[2];

    // Encoder values
    long CurrentEncoders[2]; // Current encoder value (microsteps).
    long ZeroPositionEncoders[2]; // Zero position (initial) encoder value (microsteps).

    AXISSTATUS AxesStatus[2];
    double SlewingSpeed[2];

protected:
    // Custom debug level
    unsigned int DBG_SCOPE;

private:
    enum TTY_ERROR { TTY_OK=0, TTY_READ_ERROR=-1, TTY_WRITE_ERROR=-2, TTY_SELECT_ERROR=-3, TTY_TIME_OUT=-4, TTY_PORT_FAILURE=-5, TTY_PARAM_ERROR=-6, TTY_ERRNO = -7};
    virtual int skywatcher_tty_read(int fd, char *buf, int nbytes, int timeout, int *nbytes_read) = 0;
//    virtual int skywatcher_tty_read_section(int fd, char *buf, char stop_char, int timeout, int *nbytes_read) = 0;
    virtual int skywatcher_tty_write(int fd, const char * buffer, int nbytes, int *nbytes_written) = 0;
//    virtual int skywatcher_tty_write_string(int fd, const char * buffer, int *nbytes_written) = 0;
//    virtual int skywatcher_tty_connect(const char *device, int bit_rate, int word_size, int parity, int stop_bits, int *fd) = 0;
//    virtual int skywatcher_tty_disconnect(int fd) = 0;
//    virtual void skywatcher_tty_error_msg(int err_code, char *err_msg, int err_msg_len) = 0;
//    virtual int skywatcher_tty_timeout(int fd, int timeout) = 0;*/
    int MyPortFD;

#ifdef INDI_DEBUG_LOGGING
public:
    INDI::Telescope *pChildTelescope;
#endif
};

#endif // SKYWATCHERAPI_H
