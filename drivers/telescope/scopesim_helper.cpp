/*
 * scopesim_helper.cpp
 *
 * Copyright 2020 Chris Rowland <chris.rowland@cherryfield.me.uk>
 *
 * implementation of telescope simulator helper classes Angle, Axis and Alignment
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "scopesim_helper.h"

#include "indilogger.h"

/////////////////////////////////////////////////////////////////////

// Angle implementation

Angle::Angle(double value, ANGLE_UNITS type)
{
    switch(type)
    {
        case DEGREES:
            angle = range(value);
            break;
        case HOURS:
            angle = range(value * 15.0);
            break;
        case RADIANS:
            angle = range(value * 180.0 / M_PI);
            break;
    }
}

double Angle::radians()
{
    return this->angle * M_PI / 180.0;
}

bool Angle::operator== (const Angle &a)
{
    return std::abs(difference(a)) < 10E-6;
}

bool Angle::operator!= (const Angle &a)
{
    return std::abs(difference(a)) >= 10E-6;
}

////////////////////////////////////////////////////////////////////

// Axis Implementation

void Axis::setDegrees(double degrees)
{
    this->position = degrees;
    this->target = degrees;
}

void Axis::setHours(double hours)
{
    this->position = hours * 15.0;
    this->target = hours * 15.0;
}

void Axis::StartSlew(Angle angle)
{
    LOGF_DEBUG("%s StartSlew to %f", axisName, angle.Degrees());
    target = angle;
    isSlewing = true;
}

void Axis::Tracking(bool enabled)
{
    tracking = enabled;
    LOGF_EXTRA1("%s Tracking enabled %s", axisName, enabled ? "true" : "false");
}

void Axis::TrackRate(AXIS_TRACK_RATE rate)
{
    trackingRate = rate;
    switch (trackingRate)
    {
        case AXIS_TRACK_RATE::OFF:
            TrackingRateDegSec = 0;
            break;
        case AXIS_TRACK_RATE::SIDEREAL:
            TrackingRateDegSec = siderealRate;
            break;
        case AXIS_TRACK_RATE::SOLAR:
            TrackingRateDegSec = solarRate;
            break;
        case AXIS_TRACK_RATE::LUNAR:
            TrackingRateDegSec = lunarRate;
            break;
    }
    LOGF_EXTRA1("%s: TrackRate %i, trackingRateDegSec %f arcsec", axisName, trackingRate, TrackingRateDegSec.Degrees() * 3600);
}

Axis::AXIS_TRACK_RATE Axis::TrackRate()
{
    return trackingRate;
}

double Axis::getTrackingRateDegSec()
{
    return TrackingRateDegSec.Degrees();
}

void Axis::StartGuide(double rate, uint32_t durationMs)
{
    // rate is fraction of sidereal, signed to give the direction

    guideRateDegSec = (360.0 / 86400) * rate;
    guideDuration = durationMs / 1000.0;
    LOGF_DEBUG("%s StartGuide rate %f=>%f, dur %d =>%f", axisName, rate, guideRateDegSec.Degrees(), durationMs, guideDuration);
}

void Axis::update()         // called about once a second to update the position and mode
{
    struct timeval currentTime
    {
        0, 0
    };

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday(&currentTime, nullptr);

    if (lastTime.tv_sec == 0 && lastTime.tv_usec == 0)
        lastTime = currentTime;

    // Time diff in seconds
    double interval  = currentTime.tv_sec - lastTime.tv_sec + (currentTime.tv_usec - lastTime.tv_usec) / 1e6;
    lastTime = currentTime;
    double change = 0;

    //LOGF_DEBUG("%s: position %f, target %f, interval %f", axisName, position.Degrees(), target.Degrees(), interval);

    // tracking
    if (isTracking())
    {
        position += TrackingRateDegSec * interval;
        target += TrackingRateDegSec * interval;
        LOGF_EXTRA1("%s: tracking, rate %f, position %f, target %f", axisName, TrackingRateDegSec.Degrees(), position.Degrees(), target.Degrees());
    }

    // handle the slew
    if (isSlewing)
    {
        // get positions relative to the rotate centre
        Angle trc = target - rotateCentre;
        Angle prc = position - rotateCentre;
        // get the change, don't use Angle so the change goes through the rotate centre
        double delta = trc.Degrees() - prc.Degrees();
        double fastChange = mcRates[4].Degrees() * interval;
        double slowChange = fastChange / 5;
        //LOGF_DEBUG("slew: trc %f prc %f, delta %f", trc.Degrees(), prc.Degrees(), delta);
        // apply the change to the relative position
        if (delta < -fastChange)
        {
            change = -fastChange;
        }
        else if (delta < -slowChange)
        {
            change = -slowChange;
        }
        else if (delta > fastChange)
        {
            change = fastChange;
        }
        else if (delta > slowChange)
        {
            change = slowChange;
        }
        else
        {
            position = target;
            isSlewing = false;
            //OnMoveFinished();
        }
        position += change;
        //LOGF_DEBUG("move %s: change %f, position %f", b, change, position.Degrees());
    }

    int rate = std::max(-4,std::min(4,mcRate));
    if(rate != mcRate) {
        LOGF_ERROR("Invalid mcRate Detected = %d",mcRate);
    }

    // handle the motion control
    if (rate < 0)
    {
        change = -mcRates[-rate].Degrees() * interval;
        //LOGF_DEBUG("mcRate %d, rate %f, change %f", mcRate, mcRates[-mcRate].Degrees(), change);
        position += change;
    }
    else if (rate > 0)
    {
        change = mcRates[rate].Degrees() * interval;
        //LOGF_DEBUG("mcRate %d, rate %f, change %f", mcRate, mcRates[mcRate].Degrees(), change);
        position += change;
    }

    // handle guiding
    if (guideDuration > 0)
    {
        change = guideRateDegSec.Degrees() * (guideDuration > interval ? interval : guideDuration);
        guideDuration -= interval;
        LOGF_DEBUG("guide rate %f, remaining duration %f, change %f", guideRateDegSec.Degrees(), guideDuration, change);
        position += change;
    }
}

/////////////////////////////////////////////////////////////////////////

// Alignment methods

Angle Alignment::lst()
{
    return Angle(get_local_sidereal_time(longitude.Degrees360()) * 15.0);
}

void Alignment::mountToApparentHaDec(Angle primary, Angle secondary, Angle * apparentHa, Angle* apparentDec)
{

    Angle prio, seco;
    // get instrument place
    switch (mountType)
    {
        case MOUNT_TYPE::ALTAZ:
            // Primary instrument axis: Negative PA-system looking down from Zenith:Nadir, origin "HA-like"!!!
            // Secondary instrument axis: Positive PA-system looking at E, origin "DEC-like"
        case MOUNT_TYPE::EQ_FORK:
            // Primary instrument axis: ??
            // Secondary instrument axis: ??
            seco = (latitude >= 0) ? secondary : -secondary;
            prio = primary;
            break;
        case MOUNT_TYPE::EQ_GEM:
            seco = (latitude >= 0) ? secondary : -secondary; // northern : southern hemisphere
            if (seco > 90 || seco < -90) // pierside west/looking east (cf. apparentHaDecToMount())
            {
                // Primary instrument axis: Negative PA-system looking down from NCP:SCP, origin opposite HA-like
                // Secondary instrument axis: Negative PA-system looking at E, origin opposite DEC-like
                prio = primary + Angle(180.0); // Primary to Ha transformation to get negative PA-system with origin HA-like
                seco = Angle(180.0) - seco;    // Secondary to Dec transformation to get positive PA-system origin DEC-like
            }
            else
            {
                prio = primary;    // Primary already rotated by 180, so no transformation to get origin HA-like
                seco = secondary;  // Secondary already rotated by 180, so no transformation to get origin DEC-like
            }
            break;
    }
    // instrument to observed, ignore apparent
    instrumentToObserved(prio, seco, apparentHa, apparentDec);
    // finally rotate an AltAzm mount to the Ha/Dec coordinates
    if (mountType == MOUNT_TYPE::ALTAZ)
    {
        Angle rot = latitude - Angle(90);
        Vector haDec = Vector(prio, seco).rotateY(rot);
        // Primary instrument axis: Negative PA-system looking down from Zenith:Nadir, origin "HA-like" ...
        *apparentHa = haDec.primary(); // ... so there is no transformation needed!
        // Secondary instrument axis: Positive PA-system looking east, origin "DEC-like" ...
        *apparentDec = haDec.secondary(); // ... so there is no transformation needed!
        LOGF_EXTRA1("ALTAZ to apparent HaDec: pri %f, sec %f to ha %f, dec %f  rot %f", prio.Degrees(), seco.Degrees(), apparentHa->Degrees(),
                    apparentDec->Degrees(), rot.Degrees());
    }
    else
        LOGF_EXTRA1("EQ to apparent HaDec: pri %f, sec %f to ha %f, dec %f", prio.Degrees(), seco.Degrees(), apparentHa->Degrees(),
                    apparentDec->Degrees());
}

void Alignment::mountToApparentRaDec(Angle primary, Angle secondary, Angle * apparentRa, Angle* apparentDec)
{
    Angle ha;
    mountToApparentHaDec(primary, secondary, &ha, apparentDec);
    *apparentRa = lst() - ha;
    LOGF_EXTRA1("mount to apparent RaDec: pri %f, sec %f to ha %f, ra %f, dec %f", primary.Degrees(), secondary.Degrees(), ha.Degrees(),
                apparentRa->Degrees(), apparentDec->Degrees());
}

void Alignment::apparentHaDecToMount(Angle apparentHa, Angle apparentDec, Angle* primary, Angle* secondary)
{
    // convert to Alt Azm first
    if (mountType == MOUNT_TYPE::ALTAZ)
    {
        // rotate the apparent HaDec vector to the vertical
        // TODO sort out Southern Hemisphere
        Vector altAzm = Vector(apparentHa, apparentDec).rotateY(Angle(90) - latitude);
        // for now we are making no mount corrections
        // this all leaves me wondering if the GEM corrections should be done before the mount model
        // Ha axis: Negative PA-system looking down from Zenith:Nadir, origin "HA-like" ...
        *primary = altAzm.primary(); // ... so there is no tranformation needed!
        // Dec axis: Positive PA-system looking at east, origin "DEC-like" ...
        *secondary = altAzm.secondary(); // ... so there is no tranformation needed!
        LOGF_EXTRA1("apparent HaDec to ALTAZ: ha %f, dec %f  to pri %f, sec %f", apparentHa.Degrees(), apparentDec.Degrees(), primary->Degrees(),
                    secondary->Degrees() );
    }
    // Ha is negative PA-system looking down to NCP:SCP
    // Dec is positive PA-system looking E
    Angle instrumentHa, instrumentDec;
    // ignore diurnal aberrations and refractions to get observed ha, dec
    // apply telescope pointing to get instrument
    observedToInstrument(apparentHa, apparentDec, &instrumentHa, &instrumentDec);

    switch (mountType)
    {
        case MOUNT_TYPE::ALTAZ:
            // Ha axis: Negative PA-system looking down to Zenith:Nadir pole, origin "HA-like"
            // Dec axis: Positive PA-system looking at east, origin "DEC-like"
            break;
        case MOUNT_TYPE::EQ_FORK:
            // Primary instrument axis: ??
            // Secondary instrument axis: ??
            *primary = instrumentHa;
            *secondary = (latitude >= 0) ? instrumentDec : -instrumentDec;  // northern : southern hemisphere
            break;
        case MOUNT_TYPE::EQ_GEM:
            if (instrumentHa < flipHourAngle)  // pierside west (looking east)
            {
                // Ha axis: Negative PA-system looking down from NCP:SCP, origin HA-like
                // Dec axis: Positive PA-system looking at E, origin DEC-like
                *primary = instrumentHa + Angle(180);    // Ha to Primary to get negative PA-system with origin opposite HA-like
                *secondary = Angle(180) - instrumentDec; // Dec to Secondary to get positive PA-system with origin DEC-like
            }
            else
            {
                *primary = instrumentHa;    // Ha already rotated by 180, so no transformation to get origin opposite HA-like
                *secondary = instrumentDec; // Dec already rotated by 180, so no transformation to get origin opposite DEC-like
            }
            if (latitude < 0)  // southern hemisphere
                *secondary = -*secondary;
            break;
    }
    if (mountType != MOUNT_TYPE::ALTAZ)
        LOGF_EXTRA1("apparent HaDec to EQ: ha %f, dec %f to pri %f, sec %f", apparentHa.Degrees(), apparentDec.Degrees(), primary->Degrees(),
                    secondary->Degrees() );
}

void Alignment::apparentRaDecToMount(Angle apparentRa, Angle apparentDec, Angle* primary, Angle* secondary)
{
    Angle ha = lst() - apparentRa;
    apparentHaDecToMount(ha, apparentDec, primary, secondary);
    LOGF_EXTRA1("apparent RaDec to mount: ra %f, ha %f, dec %f to pri %f, sec %f", apparentRa.Degrees(), ha.Degrees(), apparentDec.Degrees(),
                primary->Degrees(), secondary->Degrees());
}

#define NEW

void Alignment::instrumentToObserved(Angle instrumentHa, Angle instrumentDec, Angle * observedHa, Angle* observedDec)
{
    static Angle maxDec = Angle(89);

    // do the corrections consecutively
    // apply Ha and Dec zero offsets
    *observedHa = instrumentHa + IH;
    *observedDec = instrumentDec + ID;

    // apply collimation (cone) error, limited to CH * 10
    double cosDec = *observedDec < maxDec ? std::cos(observedDec->radians()) : std::cos(maxDec.radians());
    double tanDec = *observedDec < maxDec ? std::tan(observedDec->radians()) : std::tan(maxDec.radians());

    *observedHa += (CH / cosDec);
    // apply Ha and Dec axis non perpendiculary, limited to NP * 10
    *observedHa += (NP * tanDec);

    // Use rotations so the corrections work at the pole
    // apply polar axis Azimuth error using rotation in the East, West, Pole plane (X)
    Vector vMa = Vector(*observedHa, *observedDec).rotateX(MA);

    // apply polar axis elevation error using rotation in the North, South, Pole plane (Y)
    Vector vMe = vMa.rotateY(Angle(ME));

    *observedHa = vMe.primary();
    *observedDec = vMe.secondary();
}

void Alignment::observedToInstrument(Angle observedHa, Angle observedDec, Angle * instrumentHa, Angle *instrumentDec)
{
#ifdef NEW

    // avoid errors near dec 90 by limiting sec and tan dec to 57
    static Angle maxDec = Angle(89);

    // do the corrections consecutively
    // use vector rotations for MA and ME so they work close to the pole
    // rotate about the EW axis (Y)
    Vector vMe = Vector(observedHa, observedDec).rotateY(Angle(-ME));

    // apply polar axis Azimuth error
    Vector vMa = vMe.rotateX(-MA);

    *instrumentHa = vMa.primary();
    *instrumentDec = vMa.secondary();

    // apply Ha and Dec axis non perpendiculary, limited to maxDec
    double tanDec = *instrumentDec < maxDec ? std::tan(instrumentDec->radians()) : std::tan(maxDec.radians());
    *instrumentHa -= (NP * tanDec);
    // apply collimation (cone) error, limited to maxDec
    double cosDec = *instrumentDec < maxDec ? std::cos(instrumentDec->radians()) : std::cos(maxDec.radians());
    *instrumentHa -= (CH / cosDec);

    // apply Ha and Dec zero offsets
    *instrumentHa -= IH;
    *instrumentDec -= ID;

#else


    Angle correctionHa, correctionDec;
    correction(observedHa, observedDec, &correctionHa, &correctionDec);
    // subtract the correction to get a first pass at the instrument
    *instrumentHa = observedHa - correctionHa;
    *instrumentDec = observedDec - correctionDec;

    static const Angle minDelta(1.0 / 3600.0);   // 1.0 arc seconds
    Angle dHa(180);
    Angle dDec(180);
    int num = 0;
    while ( num < 10)
    {
        // use the previously calculated instrument to get a new position
        Angle newHa, newDec;
        correction(*instrumentHa, *instrumentDec, &correctionHa, &correctionDec);
        newHa = *instrumentHa + correctionHa;
        newDec = *instrumentDec + correctionDec;
        // get the deltas
        dHa = observedHa - newHa;
        dDec = observedDec - newDec;
        //            LOGF_DEBUG("observedToInstrument %i: observed %f, %f, new %f, %f, delta %f, %f", num,
        //                       observedHa.Degrees(), observedDec.Degrees(),
        //                       newHa.Degrees(), newDec.Degrees(), dHa.Degrees(), dDec.Degrees());
        if (Angle(fabs(dHa.Degrees())) < minDelta && Angle(fabs(dDec.Degrees())) < minDelta)
        {
            return;
        }
        // apply the delta
        *instrumentHa += dHa;
        *instrumentDec += dDec;
        num++;
    }
    LOGF_DEBUG("mountToObserved iterations %i, delta %f, %f", num, dHa.Degrees(), dDec.Degrees());
#endif
}

// corrections based on the instrument position, add to instrument to get observed
// see Patrick Wallace's white paper for details
void Alignment::correction(Angle instrumentHa, Angle instrumentDec, Angle * correctionHa, Angle * correctionDec)
{
    // apply Ha and Dec zero offsets
    *correctionHa = IH;
    *correctionDec = ID;

    // avoid errors near dec 90 by limiting sec and tan dec to 100
    static const double minCos = 0.01;
    static const double maxTan = 100.0;

    double cosDec = std::cos(instrumentDec.radians());
    if (cosDec >= 0 && cosDec < minCos) cosDec = minCos;
    else if (cosDec <= 0 && cosDec > -minCos) cosDec = -minCos;

    double tanDec = std::tan(instrumentDec.radians());
    if (tanDec > maxTan) tanDec = maxTan;
    if (tanDec < -maxTan) tanDec = -maxTan;

    double cosHa = std::cos(instrumentHa.radians());
    double sinHa = std::sin(instrumentHa.radians());

    // apply collimation (cone) error, limited to CH * 10
    *correctionHa += (CH / cosDec);
    // apply Ha and Dec axis non perpendiculary, limited to NP * 10
    *correctionHa += (NP * tanDec);

    // apply polar axis Azimuth error
    *correctionHa += (-MA * cosHa * tanDec);
    *correctionDec += (MA * sinHa);

    // apply polar axis elevation error
    *correctionHa += (ME * sinHa * tanDec);
    *correctionDec += (ME * cosHa);

    LOGF_EXTRA1("correction %f, %f", correctionHa->Degrees(), correctionDec->Degrees());
}

#ifdef FALSE
Alignment::apparentHaDecToMount(Vector HaDec, Vector *mount)
{
    // rotate by IH and ID
    Vector vIh = HaDec.rotateZ(Angle(IH));
    Vector vId = vIh.rotateX(Angle(ID));
}
#endif

void Alignment::setCorrections(double ih, double id, double ch, double np, double ma, double me)
{
    IH = ih;
    ID = id;
    CH = ch;
    NP = np;
    MA = ma;
    ME = me;
    LOGF_DEBUG("setCorrections IH %f, ID %f, CH %f, NP %f, MA %f, ME %f", IH, ID, CH, NP, MA, ME);
}

/////////////////////////////////////////////////////////////////////////

// Vector methods

Vector::Vector(double x, double y, double z)
{
    double len = std::sqrt(x * x + y * y + z * z);
    L = x / len;
    M = y / len;
    N = z / len;
}

Vector::Vector(Angle primary, Angle secondary)
{
    double sp = std::sin(primary.radians());
    double cp = std::cos(primary.radians());
    double ss = std::sin(secondary.radians());
    double cs = std::cos(secondary.radians());

    L = cs * cp;
    M = cs * sp;
    N = ss;
}

void Vector::normalise()
{
    double len = length();
    L /= len;
    M /= len;
    N /= len;
}

Angle Vector::primary()
{
    Angle a = Angle(atan2(M, L), Angle::RADIANS);
    //IDLog("primary %f", a.Degrees());
    return a;
}

Angle Vector::secondary()
{
    Angle a = Angle(std::asin(N), Angle::RADIANS);
    //IDLog("secondary %f", a.Degrees());
    return a;
}

Vector Vector::rotateX(Angle angle)
{
    double ca = std::cos(angle.radians());
    double sa = std::sin(angle.radians());
    return Vector(L, M * ca + N * sa, N * ca - M * sa);
}

Vector Vector::rotateY(Angle angle)
{
    double ca = std::cos(angle.radians());
    double sa = std::sin(angle.radians());
    return Vector(L * ca - N * sa, M, L * sa + N * ca);
}

Vector Vector::rotateZ(Angle angle)
{
    double ca = std::cos(angle.radians());
    double sa = std::sin(angle.radians());
    return Vector(L * ca + M * sa, M * ca - L * sa, N);
}
