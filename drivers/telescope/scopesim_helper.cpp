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

#include <cmath>

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
            angle = range(value * M_PI / 180.0);
            break;
    }
}

double Angle::radians() { return this->angle * M_PI / 180.0; }

bool Angle::operator== (const Angle& a)
{
    return std::abs(difference(a)) < 10E-6;
}

bool Angle::operator!= (const Angle& a)
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
    LOGF_EXTRA1("%s Teacking enabled %s", axisName, enabled ? "true" : "false");
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

void Axis::StartGuide(double rate, uint32_t durationMs)
{
    // rate is fraction of sidereal, signed to give the direction

    guideRateDegSec = (360.0 / 86400) * rate;
    guideDuration = durationMs / 1000.0;
    LOGF_DEBUG("%s StartGuide rate %f=>%f, dur %d =>%f", axisName, rate, guideRateDegSec.Degrees(), durationMs, guideDuration);
}

void Axis::update()         // called about once a second to update the position and mode
{
    struct timeval currentTime { 0, 0 };

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
        //LOGF_EXTRA1("%s: tracking, rate %f, position %f, target %f", axisName, TrackingRateDegSec.Degrees(), position.Degrees(), target.Degrees());
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
        const char * b;
        if (delta < -fastChange)
        {
            change = -fastChange;
            b = "--";
        }
        else if (delta < -slowChange)
        {
            change = -slowChange;
            b = "-";
        }
        else if (delta > fastChange)
        {
            change = fastChange;
            b = "++";
        }
        else if (delta > slowChange)
        {
            change = slowChange;
            b = "+";
        }
        else
        {
            position = target;
            isSlewing = false;
            //OnMoveFinished();
            b = "=";
        }
        position += change;
        //LOGF_DEBUG("move %s: change %f, position %f", b, change, position.Degrees());
    }

    // handle the motion control
    if (mcRate < 0)
    {
        change = -mcRates[-mcRate].Degrees() * interval;
        //LOGF_DEBUG("mcRate %d, rate %f, change %f", mcRate, mcRates[-mcRate].Degrees(), change);
        position += change;
    }
    else if (mcRate > 0)
    {
        change = mcRates[mcRate].Degrees() * interval;
        //LOGF_DEBUG("mcRate %d, rate %f, change %f", mcRate, mcRates[mcRate].Degrees(), change);
        position += change;
    }

    // handle guiding
    if (guideDuration > 0)
    {
        change = guideRateDegSec.Degrees() * (guideDuration > interval ? interval : guideDuration);
        guideDuration -= interval;
        //LOGF_DEBUG("guide rate %f, remaining duration %f, change %f", guideRateDegSec.Degrees(), guideDuration, change);
        position += change;
    }
}

/////////////////////////////////////////////////////////////////////////

// Alignment methods

Angle Alignment::lst()
{
    return Angle(get_local_sidereal_time(longitude.Degrees360()) * 15.0);
}

void Alignment::mountToApparentHaDec(Angle primary, Angle secondary, Angle * happarentHa, Angle* apparentDec)
{
    Angle prio, seco;
    // get instrument place
    switch (mountType)
    {
    case MOUNT_TYPE::ALTAZ:
        break;
    case MOUNT_TYPE::EQ_FORK:
        seco = (latitude >= 0) ? secondary : -secondary;
        prio = primary;
        break;
    case MOUNT_TYPE::EQ_GEM:
        seco = (latitude >= 0) ? secondary : -secondary;
        prio = primary;
        if (seco > 90 || seco < -90)
        {
            // pointing state inverted
            seco = Angle(180.0 - seco.Degrees());
            prio += 180.0;
        }
        break;
    }
    // instrument to observed, ignore apparent
    instrumentToObserved(prio, seco, happarentHa, apparentDec);
}

void Alignment::mountToApparentRaDec(Angle primary, Angle secondary, Angle * apparentRa, Angle* apparentDec)
{
    Angle ha;
    mountToApparentHaDec(primary, secondary, &ha, apparentDec);
    *apparentRa = lst() - ha;
    LOGF_EXTRA1("mountToApparentRaDec %f, %f to ha %f, ra %f, %f", primary.Degrees(), secondary.Degrees(), ha.Degrees(), apparentRa->Degrees(), apparentDec->Degrees());
}

void Alignment::apparentHaDecToMount(Angle apparentHa, Angle apparentDec, Angle* primary, Angle* secondary)
{
    // ignore diurnal abberations and refractions to get observed ha, dec
    // apply telescope pointing to get instrument
    Angle instrumentHa, instrumentDec;
    observedToInstrument(apparentHa, apparentDec, &instrumentHa, &instrumentDec);

    switch (mountType)
    {
    case MOUNT_TYPE::ALTAZ:
        break;
    case MOUNT_TYPE::EQ_FORK:
        *secondary = (latitude >= 0) ? instrumentDec : -instrumentDec;
        *primary = instrumentHa;
        break;
    case MOUNT_TYPE::EQ_GEM:
        *secondary = instrumentDec;
        *primary = instrumentHa;
        // use the instrument Ha to select the pointing state
        if (instrumentHa < flipHourAngle)
        {
            // pointing state inverted
            *primary += Angle(180);
            *secondary = Angle(180) - instrumentDec;
        }
        if (latitude < 0)
            *secondary = -*secondary;
        break;
    }
}

void Alignment::apparentRaDecToMount(Angle apparentRa, Angle apparentDec, Angle* primary, Angle* secondary)
{
    Angle ha = lst() - apparentRa;
    apparentHaDecToMount(ha, apparentDec, primary, secondary);
    LOGF_EXTRA1("apparentRaDecToMount ra %f, ha %f, %f to %f, %f", apparentRa.Degrees(), ha.Degrees(), apparentDec.Degrees(), primary->Degrees(), secondary->Degrees());
}

void Alignment::instrumentToObserved(Angle instrumentHa, Angle instrumentDec, Angle * observedHa, Angle* observedDec)
{
    Angle correctionHa, correctionDec;
    correction(instrumentHa, instrumentDec, &correctionHa, &correctionDec);
    // add the correction to the instrument to get the observed position
    *observedHa = instrumentHa + correctionHa;
    *observedDec = instrumentDec + correctionDec;
//    LOGF_EXTRA1("instrumentToObserved %f, %f to %f, %f", instrumentHa.Degrees(), instrumentDec.Degrees(),
//               observedHa->Degrees(), observedDec->Degrees());
}

void Alignment::observedToInstrument(Angle observedHa, Angle observedDec, Angle * instrumentHa, Angle *instrumentDec)
{
    Angle correctionHa, correctionDec;
    correction(observedHa, observedDec, &correctionHa, &correctionDec);
    // subtract the correction to get a first pass at the instrument
    *instrumentHa = observedHa - correctionHa;
    *instrumentDec = observedDec - correctionDec;

    static const Angle minDelta(1.0/3600.0);     // 1.0 arc seconds
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
}

// corrections based on the instrument position, add to instrument to get observed
// see Patrick Wallace's white paper for details
void Alignment::correction(Angle instrumentHa, Angle instrumentDec, Angle * correctionHa, Angle * correctionDec)
{
    // apply Ha and Dec zero offsets
    *correctionHa = IH;
    *correctionDec = ID;

    // avoid errors near dec 90 by limiting sec and tan dec to 10
    static const double minCos = 0.1;
    static const double maxTan = 10.0;

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




