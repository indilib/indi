/*******************************************************************************
 Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indiguiderinterface.h"
#include "inditelescope.h"
#include "scopesim_helper.h"

/*
static char device_str[MAXINDIDEVICE] = "Telescope Simulator";

///
/// \brief The Angle class
/// This class implements an angle type.
/// This holds an angle that is always in the range -180 to +180
/// Relational and arithmatic operators work over the -180 - +180 discontinuity
///
struct Angle
{
public:
    enum ANGLE_UNITS {DEGREES, HOURS, RADIANS};

    Angle()
    {
        angle = 0;
    }

    Angle(double value, ANGLE_UNITS type);

    Angle(double degrees)
    {
        angle = range(degrees);
    }

    //virtual ~Angle() = default;

    ///
    /// \brief Degrees
    /// \return angle in degrees, range -180 to 0 to +180
    ///
    double Degrees() {return angle; }

    ///
    /// \brief Degrees360
    /// \return angle in degrees, range 0 to 360
    ///
    double Degrees360()
    {
        if (angle >= 0) return angle;
        return 360.0 - angle;
    }

    ///
    /// \brief Hours
    /// \return angle in hours, range 0 to 24
    ///
    double Hours()
    {
        double h = angle / 15.0;
        if (h < 0.0)
            h = 24 + h;
        return h;
    }
    ///
    /// \brief HoursHa
    /// \return angle in hours, range -12 to +12
    ///
    double HoursHa() {return angle / 15.0; }

    ///
    /// \brief radians
    /// \return angle in radians, range -Pi to 0 to +PI
    ///
    double radians() { return angle * M_PI / 180.0; }

    ///
    /// \brief setDegrees
    /// set the angle in degrees
    /// \param deg angle in degrees
    ///
    void setDegrees(double deg)
    {
        angle = range(deg);
    }

    ///
    /// \brief setHours set the angle
    /// \param hrs angle in hours
    ///
    void setHours(double hrs)
    {
        angle = hrstoDeg(hrs);
    }

    Angle add(Angle a)
    {
        double total = a.Degrees() + this->Degrees();
        return Angle(total);
    }

    Angle subtract(Angle a)
    {
        return Angle(this->Degrees() - a.Degrees());
    }

    double difference(Angle a)
    {
        return range(this->Degrees() - a.Degrees());
    }

    Angle operator - ()
    {
        return Angle(-this->angle);
    }

    Angle& operator += (const Angle& a)
    {
        angle = range(angle + a.angle);
        return *this;
    }

    Angle& operator += (const double a)
    {
        angle = range(angle + a);
        return *this;
    }

    Angle& operator-= (const Angle& a)
    {
        angle = range(angle - a.angle);
        return *this;
    }

    Angle& operator-= (const double a)
    {
        angle = range(angle - a);
        return *this;
    }

    ///
    /// \brief operator *
    /// multiplies the angle by a double,
    /// used to manage tracking and slewing
    /// \param duration as a double
    /// \return Angle
    ///
    Angle operator * (const double duration)
    {
        return Angle(this->angle * duration);
    }

    bool operator== (const Angle& a)
    {
        return std::abs(difference(a)) < 10E-6;
    }

    bool operator!= (const Angle& a)
    {
        return std::abs(difference(a)) >= 10E-6;
    }

    bool operator > (const Angle& a)
    {
        return difference(a) > 0;
    }

    bool operator < (const Angle& a)
    {
        return difference(a) < 0;
    }
    bool operator >= (const Angle& a)
    {
        return difference(a) >= 0;
    }
    bool operator <= (const Angle& a)
    {
        return difference(a) <= 0;
    }

private:
    double angle;     // position in degrees, range -180 to 0 to 180

    ///
    /// \brief range
    /// \param deg
    /// \return returns an angle in degrees folded to the range -180 to 0 to 180
    ///
    static double range(double deg)
    {
        while (deg >= 180.0) deg -= 360.0;
        while (deg < -180.0) deg += 360.0;
        return deg;
    }

    static double hrstoDeg(double hrs) { return range(hrs * 15.0); }
};

Angle operator+ (Angle lhs, const Angle& rhs) { return lhs += rhs;}
Angle operator+ (Angle lhs, const double& rhs) { return lhs += rhs;}

Angle operator- (Angle lhs, const Angle& rhs) { return lhs -= rhs;}
Angle operator- (Angle lhs, const double& rhs) { return lhs -= rhs;}


class Axis
{
public:
    enum AXIS_TRACK_MODE {OFF, ALTAZ, EQ_N, EQ_S };
    enum AXIS_TRACK_RATE { SIDEREAL, LUNAR, SOLAR };

    Axis(const char * name) { axisName = name; }
    const char * axisName;

    void setDegrees(double degrees);
    void setHours(double hours);

    Angle position;         // current axis position
    Angle target;           // target axis position

    void StartSlew(Angle angle)
    {
        LOGF_DEBUG("StartSlew %s to %f", axisName, angle.Degrees());
        target = angle;
        isSlewing = true;
    }

    void AbortSlew()
    {
        target = position;
    }

    bool isSlewing;

    void Tracking(bool enabled)
    {
        trackMode = enabled ? AXIS_TRACK_MODE::EQ_N : AXIS_TRACK_MODE::OFF;
        SetModeAndRate();
    }

    bool isTracking() { return trackMode != AXIS_TRACK_MODE::OFF; }

    void TrackRate(AXIS_TRACK_RATE rate)
    {
        LOGF_DEBUG("TrackRate %i", rate);
        trackingRate = rate;
        SetModeAndRate();
    }

    void StartGuide(double rate, uint durationMs)
    {
        // rate is fraction sidereal, signed to give the direction

        guideRateDegSec = (360.0 / 86400) * rate;
        guideDuration = durationMs / 1000.0;
        LOGF_DEBUG("%s StartGuide rate %f=>%f, dur %d =>%f", axisName, rate, guideRateDegSec.Degrees(), durationMs, guideDuration);
    }

    bool IsGuiding() { return guideDuration > 0; }

    Angle moveRate;         // degrees per second, set to move mount while tracking

    int mcRate;             // int -4 to 4 sets move rate, zero is stopped

    void update();         // called about once a second to update the position and mode

    // needed for debug MACROS
    const char *getDeviceName() { return device_str;}

private:
    struct timeval lastTime { 0, 0 };

    AXIS_TRACK_RATE trackingRate { AXIS_TRACK_RATE::SIDEREAL };
    AXIS_TRACK_MODE trackMode;

    Angle trackingRateDegSec;
    Angle rotateCentre { 90.0 };

    double guideDuration;
    Angle guideRateDegSec;

    // rates are angles in degrees per second
    const Angle solarRate { 360.0 / 86400};
    const Angle siderealRate { (360.0 / 86400) * 0.99726958  };
    const Angle lunarRate { (360.0 / 86400) * 1.034 };

    Angle mcRates[5]
    {
        0,
        siderealRate,   // guide rate
        0.5,            // fine rate
        2.5,            // center rate
        6.0,            // goto rate
    };

    void SetModeAndRate()
    {
        Angle tr = 0;
        switch (trackingRate)
        {
            case AXIS_TRACK_RATE::SIDEREAL:
                tr = siderealRate;
                break;
            case AXIS_TRACK_RATE::SOLAR:
                tr = solarRate;
                break;
            case AXIS_TRACK_RATE::LUNAR:
                tr = lunarRate;
                break;
        }

        switch (trackMode)
        {
            case AXIS_TRACK_MODE::OFF:
            case AXIS_TRACK_MODE::ALTAZ:
                trackingRateDegSec = 0;
                break;
            case AXIS_TRACK_MODE::EQ_N:
                trackingRateDegSec = tr;
                break;
            case AXIS_TRACK_MODE::EQ_S:
                trackingRateDegSec = -tr;
                break;
        }
    }

};

///
/// \brief The Alignment class
/// This converts between the mount axis angles and the sky hour angle and declination angles.
/// Initially for equatorial fork and GEM mounts.
/// To start with there is a unity mount model.
/// The axis zeros correspond to the declination and hour angle zeroes and the directions match in the Northern henisphere
/// For the GEM the normal pointing state is defined as positive hour angles, ie. with the mount on the East, looking West
/// Both axis directions are mirrored in the South
///
/// A future enhancement will be to use a simple mount model based on Patrick Wallace's paper.
/// this is at http://www.tpointsw.uk/pointing.htm
///
/// Terminology is as defined in figure 1:
///
///  Apparent Ra and Dec - what is (incorrectly) called JNow.
///     apply local sidereal time
///  Apparent Ha and Dec    positions are apparentRa, apparentHa and apparentDec
///     ignore diurnal effects
///     ignore refraction (for now)
///  Observed Place  These are the mount coordinates for a perfect mount, positions are observedHa and observedDec
///     apply telescope pointing corrections
///  Instrument Place these are the mount coordinates for the mount with corrections, values are instrumentHa and instrumentDec
///     for a  GEM convert to axis cooordinates ( this isn't in the paper).
///  Mount Place these give primary (ha) and secondary (dec) positions
///
///
class Alignment
{
public:
    Alignment(){}

    enum MOUNT_TYPE { ALTAZ, EQ_FORK, EQ_GEM };

    ///
    /// \brief mountToApparentHaDec: convert mount position to apparent Ha, Dec
    /// \param primary
    /// \param Secondary
    /// \param ra
    /// \param dec
    ///
    void mountToApparentHaDec(Angle primary, Angle Secondary, Angle *ra, Angle *dec);

    ///
    /// \brief mountToApparentRaDec: convert mount position to apparent Ra, Dec
    /// \param primary
    /// \param secondary
    /// \param ra
    /// \param dec
    ///
    void mountToApparentRaDec(Angle primary, Angle secondary, Angle * ra, Angle* dec);

    void apparentHaDecToMount(Angle ha, Angle dec, Angle *primary, Angle *secondary);

    void apparentRaDecToMount(Angle ra, Angle dec, Angle *primary, Angle *secondary);

    Angle lst();            // returns the current LST as an angle
    Angle latitude = 0;
    Angle longitude = 0;
    MOUNT_TYPE mountType = EQ_FORK;

    void setCorrections(double ih, double id, double ch, double np, double ma, double me);

    // needed for debug MACROS
    const char *getDeviceName() { return device_str;}
private:
    void instrumentToObserved(Angle ham, Angle decm, Angle *hao, Angle *deco);
    void observedToInstrument(Angle ha, Angle dec, Angle * hac, Angle * decc);

    ///
    /// \brief correction: determins the correction to the instrument position to get the observed
    /// Based on Patrick Wallace's paper, see Table 1.
    ///
    /// correction parameters are:
    /// IH: The hour angle axis index error
    /// ID: The dec axis index error
    /// CH: the telescope collimation error, popularly known as cone
    /// NP: the amount that the mount dec axis is not perpendicular to the hour angle axis
    /// MA: the polar axis azimuth error
    /// ME: the polar axis elevation error
    ///
    /// \param instrumentHa
    /// \param instrumentDec
    /// \param correctionHa
    /// \param correctionDec
    ///
    void correction(Angle instrumentHa, Angle instrumentDec, Angle *correctionHa, Angle *correctionDec);
    double separation(double ha1, double dec1, double ha2, double dec2);

    // mount model, these angles are in degrees
    // the angles are small so as double to avoid
    // load of conversions
    double IH = 0;
    double ID = 0;
    double CH = 0;
    double NP = 0;
    double MA = 0;
    double ME = 0;
};
*/

/**
 * @brief The ScopeSim class provides a simple mount simulator of an equatorial mount.
 *
 * It supports the following features:
 * + Sideral and Custom Tracking rates.
 * + Goto & Sync
 * + NWSE Hand controller direciton key slew.
 * + Tracking On/Off.
 * + Parking & Unparking with custom parking positions.
 * + Setting Time & Location.
 *
 * On startup and by default the mount shall point to the celestial pole.
 *
 * @author Jasem Mutlaq
 */
class ScopeSim : public INDI::Telescope, public INDI::GuiderInterface
{
  public:
    ScopeSim();
    virtual ~ScopeSim() = default;

    virtual const char *getDefaultName() override;
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual bool ReadScopeStatus() override;
    virtual bool initProperties() override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool updateProperties() override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

  protected:
    // Slew Rate
    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
    virtual bool Abort() override;

    virtual IPState GuideNorth(uint32_t ms) override;
    virtual IPState GuideSouth(uint32_t ms) override;
    virtual IPState GuideEast(uint32_t ms) override;
    virtual IPState GuideWest(uint32_t ms) override;

    virtual bool SetTrackMode(uint8_t mode) override;
    virtual bool SetTrackEnabled(bool enabled) override;
    virtual bool SetTrackRate(double raRate, double deRate) override;

    virtual bool Goto(double, double) override;
    virtual bool Park() override;
    virtual bool UnPark() override;
    virtual bool Sync(double ra, double dec) override;

    // Parking
    virtual bool SetCurrentPark() override;
    virtual bool SetDefaultPark() override;
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;

    virtual bool saveConfigItems(FILE *fp) override;

private:
    double currentRA { 0 };
    double currentDEC { 90 };
    double targetRA { 0 };
    double targetDEC { 0 };

    /// used by GoTo and Park
    void StartSlew(double ra, double dec, TelescopeStatus status);

    bool forceMeridianFlip { false };
    unsigned int DBG_SCOPE { 0 };

    int mcRate = 0;

//    double guiderEWTarget[2];
//    double guiderNSTarget[2];

    bool guidingNS = false;
    bool guidingEW = false;

    INumber GuideRateN[2];
    INumberVectorProperty GuideRateNP;

    Axis axisPrimary { "HaAxis" };         // hour angle mount axis
    Axis axisSecondary { "DecAxis" };       // declination mount axis

    // Scope type and alignment
    ISwitch mountTypeS[3];
    ISwitchVectorProperty mountTypeSP;
    ISwitch simPierSideS[2];
    ISwitchVectorProperty simPierSideSP;
    void updateMountAndPierSide();

    INumber mountModelN[6];
    INumberVectorProperty mountModelNP;

    Alignment alignment;

#ifdef USE_EQUATORIAL_PE
    INumberVectorProperty EqPENV;
    INumber EqPEN[2];

    ISwitch PEErrNSS[2];
    ISwitchVectorProperty PEErrNSSP;

    ISwitch PEErrWES[2];
    ISwitchVectorProperty PEErrWESP;
#endif
};

