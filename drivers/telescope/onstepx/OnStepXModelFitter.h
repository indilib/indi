/*
    OnStepX INDI Driver — OnStepXModelFitter.h
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
    OnStepX INDI Driver — PC-side pointing-model fitter

    Pure numerical fitting component. No INDI, OnStepXComm, logging,
    or device dependencies.
*/

#pragma once

#include "OnStepXModelMath.h"

#include <cstddef>
#include <vector>

class OnStepXModelFitter
{
    public:
        /*
         * Which model terms a fit solves for.  Each level adds to the one
         * before, like the firmware's own alignment, which adds terms as it
         * gets more stars (GeoAlign::autoModel()).  Terms outside the level
         * are held at zero.
         *
         *   INDEX       ax1Cor, ax2Cor
         *   AXIS        + altCor, azmCor (polar axis alignment; azimuth axis
         *                 tilt on alt-az mounts)
         *   CONE        + doCor
         *   MECHANICAL  + pdCor, dfCor, tfCor (non-perpendicularity, flexure)
         *   FULL        + hca/hcp, dca/dcp (periodic errors)
         *
         * On AltAz FULL also holds tfCor at 0 (see fixedParameters()).
         */
        enum class Terms { INDEX, AXIS, CONE, MECHANICAL, FULL };
        static constexpr std::size_t TERM_LEVELS = 5;

        // Observations needed before a level is tried.  Each observation
        // gives two equations; these leave some redundancy at every level.
        static std::size_t minimumObservations(Terms terms);
        // Number of terms the level solves for on this mount type.
        static std::size_t freeParameters(Terms terms, OnStepXModelMath::MountType mountType);
        // What the level adds, for messages: "cone error".
        static const char *describe(Terms terms, OnStepXModelMath::MountType mountType);

        struct FitResult
        {
            enum class Status
            {
                SUCCESS,
                INSUFFICIENT_OBSERVATIONS,
                SINGULAR,
                NON_FINITE,
                NO_CONVERGENCE
            };

            Status status { Status::NO_CONVERGENCE };

            OnStepXModelMath::ModelCoefficients model {};

            double rmsArcsec { 0.0 };
            double maxAbsResidualArcsec { 0.0 };

            std::size_t iterations { 0 };
            std::size_t rank { 0 };

            Terms terms { Terms::FULL };
            std::size_t freeParameters { 0 };
            // Sum of squared residuals (arcsec^2) over both axes.
            double sumSquaredArcsec { 0.0 };

            bool success() const
            {
                return status == Status::SUCCESS;
            }
        };

        /*
         * Fit the OnStepX pointing model, solving for the given terms.
         *
         * The input observations must already contain the actual and
         * commanded/native mount coordinates required by OnStepXModelMath.
         *
         * The latitude is radians.
         */
        static FitResult fit(
            const std::vector<OnStepXModelMath::Observation> &observations,
            double latitude,
            Terms terms = Terms::FULL);

        /*
         * Fit every level the observations allow and choose one.  A level is
         * kept over the one below only if its extra terms reduce the residuals
         * significantly (an F-test), so terms that would only fit the noise in
         * a few observations are left out.
         */
        struct Level
        {
            enum class Outcome
            {
                NOT_ENOUGH_OBSERVATIONS,   // fewer than minimumObservations()
                NOT_SEPARABLE,             // the observations can't tell its terms apart
                NOT_SIGNIFICANT,           // fitted, but no real improvement
                FAILED,                    // no convergence or non-finite
                USED                       // the chosen level, or one below it that it builds on
            };
            Terms terms { Terms::INDEX };
            Outcome outcome { Outcome::NOT_ENOUGH_OBSERVATIONS };
            FitResult fit {};
        };
        struct AdaptiveResult
        {
            Level levels[TERM_LEVELS] {};
            // Index of the chosen level in levels[], or -1 if none could be fitted.
            int chosen { -1 };

            bool success() const
            {
                return chosen >= 0;
            }
            const FitResult &best() const
            {
                return levels[chosen].fit;
            }
        };
        static AdaptiveResult fitAdaptive(
            const std::vector<OnStepXModelMath::Observation> &observations,
            double latitude);

        // Smallest F statistic for a level to count as a real improvement.
        static constexpr double SIGNIFICANCE_F = 4.0;
};
