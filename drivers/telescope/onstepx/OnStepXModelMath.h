/*
    OnStepX INDI Driver — OnStepXModelMath.h
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

    This component deliberately has no INDI, OnStepXComm, or device
    dependencies. It contains only the coordinate/model mathematics so that
    it can be unit tested without connecting to an OnStepX controller.
*/

#pragma once

class OnStepXModelMath
{
    public:
        enum class MountType
        {
            GEM,
            FORK,
            ALTAZM,
            ALTALT
        };

        enum class PierSide
        {
            EAST,
            WEST
        };

        struct Observation
        {
            double actualAxis1 { 0.0 };
            double actualAxis2 { 0.0 };
            double mountAxis1  { 0.0 };
            double mountAxis2  { 0.0 };
            double actualRAHours { 0.0 };
            double actualDecDeg { 0.0 };
            double mountRAHours { 0.0 };
            double mountDecDeg  { 0.0 };
            double lstHours      { 0.0 };
            PierSide pierSide { PierSide::EAST };
            MountType mountType { MountType::GEM };
        };

        struct ModelCoefficients
        {
            // All angular quantities are radians, matching GeoAlign's
            // internal model representation. Protocol conversion belongs
            // outside this class.
            double ax1Cor { 0.0 };
            double ax2Cor { 0.0 };
            double altCor { 0.0 };
            double azmCor { 0.0 };
            double doCor  { 0.0 };
            double pdCor  { 0.0 };
            double dfCor  { 0.0 };
            double tfCor  { 0.0 };
            double hcp    { 0.0 };
            double hca    { 0.0 };
            double dcp    { 0.0 };
            double dca    { 0.0 };
        };

        static double wrapRadians(double radians);

        static void equatorialToNative(double ha, double dec,
                                       double latitude,
                                       MountType mountType,
                                       double &axis1, double &axis2);

        // The position the firmware reports for the mount axes with this model:
        // GeoAlign::mountToObservedPlace() in Align.hs.cpp (HIGH_SPEED_ALIGN,
        // always defined).  The index terms are added; every other term is
        // subtracted.  latitude is the site latitude (radians); for AltAz and
        // AltAlt the model uses 90 and 0 degrees instead, as the firmware does.
        static void predictObserved(const Observation &observation,
                                    const ModelCoefficients &model,
                                    double latitude,
                                    double &observedAxis1,
                                    double &observedAxis2);

        static void residual(const Observation &observation,
                             const ModelCoefficients &model,
                             double latitude,
                             double &axis1Residual,
                             double &axis2Residual);
};
