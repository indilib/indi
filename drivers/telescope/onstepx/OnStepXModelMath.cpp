/*
    OnStepX INDI Driver — OnStepXModelMath.cpp
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
    OnStepX INDI Driver — pure PC-side pointing-model mathematics
*/

#include "OnStepXModelMath.h"

#include <cmath>

namespace
{
constexpr double PI = 3.14159265358979323846;
constexpr double DEG90 = PI / 2.0;
constexpr double DEG180 = PI;
constexpr double DEG360 = 2.0 * PI;
constexpr double TENTH_ARCSEC = PI / (180.0 * 36000.0);
constexpr double POLE_GUARD = 89.98333333 * PI / 180.0;
}

double OnStepXModelMath::wrapRadians(double radians)
{
    while (radians > PI)
        radians -= 2.0 * PI;
    while (radians <= -PI)
        radians += 2.0 * PI;
    return radians;
}

void OnStepXModelMath::equatorialToNative(double ha, double dec,
        double latitude,
        MountType mountType,
        double &axis1, double &axis2)
{
    if (mountType == MountType::ALTAZM || mountType == MountType::ALTALT)
    {
        const double cosHA = std::cos(ha);
        const double sinAlt = std::sin(dec) * std::sin(latitude) +
                              std::cos(dec) * std::cos(latitude) * cosHA;
        const double alt = std::asin(sinAlt);

        double az;
        if (std::fabs(dec - DEG90) < TENTH_ARCSEC)
            az = 0.0;
        else if (std::fabs(dec + DEG90) < TENTH_ARCSEC)
            az = DEG180;
        else
        {
            const double t1 = std::sin(ha);
            const double t2 = cosHA * std::sin(latitude) -
                              std::tan(dec) * std::cos(latitude);
            az = std::atan2(t1, t2);
            az += DEG180;
        }

        if (az > DEG180)
            az -= DEG360;

        if (mountType == MountType::ALTAZM)
        {
            axis1 = az;
            axis2 = alt;
            return;
        }

        // Exact Transform::horToAa() sequence from OnStepX.
        const double cosAzm = std::cos(az);
        const double sinAA2 = std::cos(alt) * cosAzm;
        const double aa2 = std::asin(sinAA2);
        const double t1 = std::sin(az);
        const double t2 = -std::tan(alt);
        double aa1 = std::atan2(t1, t2);
        aa1 += DEG180;
        if (aa1 > DEG180)
            aa1 -= DEG360;

        axis1 = aa1;
        axis2 = aa2;
        return;
    }

    // GEM and FORK use HA/Dec directly, exactly as GeoAlign::addStar().
    axis1 = wrapRadians(ha);
    axis2 = dec;
}

void OnStepXModelMath::predictObserved(const Observation &observation,
                                       const ModelCoefficients &model,
                                       double latitude,
                                       double &observedAxis1,
                                       double &observedAxis2)
{
    const double p = (observation.pierSide == PierSide::WEST) ? -1.0 : 1.0;

    double ax1 = observation.mountAxis1 + model.ax1Cor;
    double ax2 = observation.mountAxis2 + model.ax2Cor * -p;

    if (ax2 > DEG90) ax2 = DEG90;
    if (ax2 < -DEG90) ax2 = -DEG90;

    if (std::fabs(ax2) < POLE_GUARD)
    {
        // Inside the model the firmware replaces the site latitude for the
        // alt-az mount types (GeoAlign::init()): 90 deg for AltAz, 0 for AltAlt.
        double modelLatitude = latitude;
        if (observation.mountType == MountType::ALTAZM)
            modelLatitude = DEG90;
        else if (observation.mountType == MountType::ALTALT)
            modelLatitude = 0.0;
        const double cosLat = std::cos(modelLatitude);
        const double sinLat = std::sin(modelLatitude);

        const double sinAx2 = std::sin(ax2);
        const double cosAx2 = std::cos(ax2);
        const double sinAx1 = std::sin(ax1);
        const double cosAx1 = std::cos(ax1);

        const double doH = model.doCor * (1.0 / cosAx2) * p;
        const double pdH = -model.pdCor * (sinAx2 / cosAx2) * p;

        double dfD;
        if (observation.mountType == MountType::FORK ||
                observation.mountType == MountType::ALTAZM)
            dfD = model.dfCor * cosAx1;
        else
            dfD = -model.dfCor * (cosLat * cosAx1 + sinLat * (sinAx2 / cosAx2));

        const double tfH = model.tfCor * (cosLat * sinAx1 * (1.0 / cosAx2));
        const double tfD = model.tfCor * (cosLat * cosAx1 * sinAx2 - sinLat * cosAx2);

        const double a1 = -model.azmCor * cosAx1 * (sinAx2 / cosAx2) +
                          model.altCor * sinAx1 * (sinAx2 / cosAx2);
        const double a2 = model.azmCor * sinAx1 + model.altCor * cosAx1;

        // On the west pier the Dec harmonic runs on the mirrored axis 2.
        double cosDAx2 = ax2;
        if (p < 0.0)
            cosDAx2 = (sinLat >= 0.0) ? DEG180 - ax2 : -DEG180 - ax2;
        const double cosH = std::cos(ax1 + model.hcp) * model.hca * p;
        const double cosD = std::cos(cosDAx2 + model.dcp) * model.dca * p;

        // The firmware subtracts the terms when it reports a position
        // (GeoAlign::mountToObservedPlace(), and its own fit in doSearch()).
        // Only the index terms above are added.
        ax1 = ax1 - (a1 + pdH + doH + tfH + cosH);
        ax2 = ax2 - (a2 + dfD + tfD + cosD);
    }

    if (ax2 > DEG90) ax2 = DEG90;
    if (ax2 < -DEG90) ax2 = -DEG90;

    if (observation.mountType == MountType::ALTAZM ||
            observation.mountType == MountType::ALTALT)
    {
        while (ax1 > DEG360) ax1 -= DEG360;
        while (ax1 < -DEG360) ax1 += DEG360;
    }
    else
    {
        while (ax1 > DEG180) ax1 -= DEG360;
        while (ax1 < -DEG180) ax1 += DEG360;
    }

    observedAxis1 = ax1;
    observedAxis2 = ax2;
}

void OnStepXModelMath::residual(const Observation &observation,
                                const ModelCoefficients &model,
                                double latitude,
                                double &axis1Residual,
                                double &axis2Residual)
{
    double predictedAxis1 = 0.0;
    double predictedAxis2 = 0.0;
    predictObserved(observation, model, latitude,
                    predictedAxis1, predictedAxis2);

    axis1Residual = wrapRadians(observation.actualAxis1 - predictedAxis1) *
                    std::cos(observation.actualAxis2);
    axis2Residual = observation.actualAxis2 - predictedAxis2;
}
