/*
    OnStepX INDI Driver — OnStepXModelFitter.cpp
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

#include "OnStepXModelFitter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace
{
constexpr std::size_t PARAMETER_COUNT = 12;

constexpr double ARCSEC_PER_RAD = 206264.80624709636;
constexpr double RAD_PER_ARCSEC = 1.0 / ARCSEC_PER_RAD;

constexpr double FINITE_DIFFERENCE_STEP = 0.02;

constexpr double INITIAL_LAMBDA = 1.0e-3;
constexpr double MIN_LAMBDA = 1.0e-12;
constexpr double MAX_LAMBDA = 1.0e16;

constexpr std::size_t MAX_ITERATIONS = 200;

constexpr double STEP_TOLERANCE = 1.0e-7;
constexpr double COST_TOLERANCE = 1.0e-12;

struct Parameters
{
    std::array<double, PARAMETER_COUNT> value {};
};

OnStepXModelMath::ModelCoefficients toModel(const Parameters &p)
{
    OnStepXModelMath::ModelCoefficients model {};

    // The first eight parameters are represented directly in arcseconds.
    model.ax1Cor = p.value[0] * RAD_PER_ARCSEC;
    model.ax2Cor = p.value[1] * RAD_PER_ARCSEC;
    model.altCor = p.value[2] * RAD_PER_ARCSEC;
    model.azmCor = p.value[3] * RAD_PER_ARCSEC;
    model.doCor  = p.value[4] * RAD_PER_ARCSEC;
    model.pdCor  = p.value[5] * RAD_PER_ARCSEC;
    model.dfCor  = p.value[6] * RAD_PER_ARCSEC;
    model.tfCor  = p.value[7] * RAD_PER_ARCSEC;

    /*
     * The cosine terms are parameterised as:
     *
     *   Hc = hca * cos(hcp)
     *   Hs = -hca * sin(hcp)
     *
     *   Dc = dca * cos(dcp)
     *   Ds = -dca * sin(dcp)
     *
     * This avoids the phase/amplitude singularity during fitting.
     */
    model.hca = std::hypot(p.value[8], p.value[9]) * RAD_PER_ARCSEC;
    model.dca = std::hypot(p.value[10], p.value[11]) * RAD_PER_ARCSEC;

    model.hcp = std::atan2(-p.value[9], p.value[8]);
    model.dcp = std::atan2(-p.value[11], p.value[10]);

    return model;
}

bool evaluateResiduals(
    const std::vector<OnStepXModelMath::Observation> &observations,
    const Parameters &parameters,
    double latitude,
    std::vector<double> &residuals)
{
    residuals.clear();
    residuals.reserve(observations.size() * 2);

    const auto model = toModel(parameters);

    for (const auto &observation : observations)
    {
        double r1 = 0.0;
        double r2 = 0.0;

        OnStepXModelMath::residual(
            observation,
            model,
            latitude,
            r1,
            r2);

        r1 *= ARCSEC_PER_RAD;
        r2 *= ARCSEC_PER_RAD;

        if (!std::isfinite(r1) || !std::isfinite(r2))
            return false;

        residuals.push_back(r1);
        residuals.push_back(r2);
    }

    return true;
}

double squaredNorm(const std::vector<double> &values)
{
    double result = 0.0;

    for (double value : values)
        result += value * value;

    return result;
}

bool solveLinearSystem(
    std::array<std::array<double, PARAMETER_COUNT>, PARAMETER_COUNT> matrix,
    std::array<double, PARAMETER_COUNT> rhs,
    std::array<double, PARAMETER_COUNT> &solution)
{
    constexpr double PIVOT_EPSILON = 1.0e-14;

    for (std::size_t column = 0;
            column < PARAMETER_COUNT;
            ++column)
    {
        std::size_t pivot = column;
        double pivotMagnitude = std::abs(matrix[column][column]);

        for (std::size_t row = column + 1;
                row < PARAMETER_COUNT;
                ++row)
        {
            const double magnitude =
                std::abs(matrix[row][column]);

            if (magnitude > pivotMagnitude)
            {
                pivotMagnitude = magnitude;
                pivot = row;
            }
        }

        if (!std::isfinite(pivotMagnitude) ||
                pivotMagnitude <= PIVOT_EPSILON)
        {
            return false;
        }

        if (pivot != column)
        {
            std::swap(matrix[pivot], matrix[column]);
            std::swap(rhs[pivot], rhs[column]);
        }

        for (std::size_t row = column + 1;
                row < PARAMETER_COUNT;
                ++row)
        {
            const double factor =
                matrix[row][column] /
                matrix[column][column];

            if (!std::isfinite(factor))
                return false;

            matrix[row][column] = 0.0;

            for (std::size_t j = column + 1;
                    j < PARAMETER_COUNT;
                    ++j)
            {
                matrix[row][j] -=
                    factor * matrix[column][j];
            }

            rhs[row] -= factor * rhs[column];
        }
    }

    solution.fill(0.0);

    for (std::size_t i = PARAMETER_COUNT; i-- > 0;)
    {
        double value = rhs[i];

        for (std::size_t j = i + 1;
                j < PARAMETER_COUNT;
                ++j)
        {
            value -=
                matrix[i][j] * solution[j];
        }

        if (!std::isfinite(matrix[i][i]) ||
                std::abs(matrix[i][i]) <= PIVOT_EPSILON)
        {
            return false;
        }

        solution[i] =
            value / matrix[i][i];

        if (!std::isfinite(solution[i]))
            return false;
    }

    return true;
}

/*
 * Estimate rank from a 12x12 matrix using pivoted Gaussian elimination.
 *
 * The matrix supplied here is J^T J from the actual finite-difference
 * Jacobian used by the current LM iteration.
 */
std::size_t rankOf(
    const std::array <
    std::array<double, PARAMETER_COUNT>,
    PARAMETER_COUNT > &input)
{
    auto matrix = input;

    double scale = 0.0;

    for (const auto &row : matrix)
    {
        for (double value : row)
            scale = std::max(scale, std::abs(value));
    }

    if (!(scale > 0.0) || !std::isfinite(scale))
        return 0;

    const double tolerance = scale * 1.0e-12;

    std::size_t rank = 0;

    for (std::size_t column = 0;
            column < PARAMETER_COUNT &&
            rank < PARAMETER_COUNT;
            ++column)
    {
        std::size_t pivot = rank;
        double pivotMagnitude =
            std::abs(matrix[rank][column]);

        for (std::size_t row = rank + 1;
                row < PARAMETER_COUNT;
                ++row)
        {
            const double magnitude =
                std::abs(matrix[row][column]);

            if (magnitude > pivotMagnitude)
            {
                pivotMagnitude = magnitude;
                pivot = row;
            }
        }

        if (pivotMagnitude <= tolerance)
            continue;

        if (pivot != rank)
            std::swap(matrix[pivot], matrix[rank]);

        for (std::size_t row = rank + 1;
                row < PARAMETER_COUNT;
                ++row)
        {
            const double factor =
                matrix[row][column] /
                matrix[rank][column];

            for (std::size_t j = column;
                    j < PARAMETER_COUNT;
                    ++j)
            {
                matrix[row][j] -=
                    factor * matrix[rank][j];
            }
        }

        ++rank;
    }

    return rank;
}

/*
 * Parameters held at zero for a mount type, because the model cannot tell
 * them apart from another term.  On AltAz the firmware evaluates tube flex
 * at latitude 90 deg, which leaves only -tfCor*cos(alt) on axis 2: exactly
 * the cosine part of the Dec harmonic (Dc).  Holding tfCor at 0 lets Dc
 * absorb it, with identical pointing.  (The firmware's own fit never meets
 * this: it doesn't fit the harmonic terms.)
 */
using FixedMask = std::array<bool, PARAMETER_COUNT>;
using Terms = OnStepXModelFitter::Terms;

constexpr std::size_t TF_PARAMETER = 7;

// The level that first solves for each parameter (Parameters order).
constexpr Terms PARAMETER_LEVEL[PARAMETER_COUNT] =
{
    Terms::INDEX, Terms::INDEX,                         // ax1Cor, ax2Cor
    Terms::AXIS, Terms::AXIS,                           // altCor, azmCor
    Terms::CONE,                                        // doCor
    Terms::MECHANICAL, Terms::MECHANICAL, Terms::MECHANICAL,   // pdCor, dfCor, tfCor
    Terms::FULL, Terms::FULL, Terms::FULL, Terms::FULL  // Hc, Hs, Dc, Ds
};

// Parameters held at zero: those outside the level, and tfCor on AltAz at
// FULL (where the Dec harmonic duplicates it; below FULL it is separable).
FixedMask fixedParameters(OnStepXModelMath::MountType mountType, Terms terms)
{
    FixedMask fixed {};
    for (std::size_t i = 0; i < PARAMETER_COUNT; ++i)
        fixed[i] = static_cast<int>(PARAMETER_LEVEL[i]) > static_cast<int>(terms);
    if (mountType == OnStepXModelMath::MountType::ALTAZM && terms == Terms::FULL)
        fixed[TF_PARAMETER] = true;
    return fixed;
}

// A fixed parameter has a zero Jacobian column.  Give it an identity row and
// column in J^T J, so the LM step leaves it at zero and the rank check counts
// it as determined.
void pinFixed(std::array<std::array<double, PARAMETER_COUNT>, PARAMETER_COUNT> &jtj,
              const FixedMask &fixed)
{
    double diagonal = 1.0;
    for (std::size_t i = 0; i < PARAMETER_COUNT; ++i)
        diagonal = std::max(diagonal, std::abs(jtj[i][i]));
    for (std::size_t i = 0; i < PARAMETER_COUNT; ++i)
    {
        if (!fixed[i])
            continue;
        for (std::size_t j = 0; j < PARAMETER_COUNT; ++j)
        {
            jtj[i][j] = 0.0;
            jtj[j][i] = 0.0;
        }
        jtj[i][i] = diagonal;
    }
}

bool makeJacobian(
    const std::vector<OnStepXModelMath::Observation> &observations,
    const Parameters &parameters,
    double latitude,
    const FixedMask &fixed,
    std::vector<std::array<double, PARAMETER_COUNT>> &jacobian,
    std::vector<double> &residuals)
{
    if (!evaluateResiduals(
                observations,
                parameters,
                latitude,
                residuals))
    {
        return false;
    }

    jacobian.assign(residuals.size(), {});

    for (std::size_t parameter = 0;
            parameter < PARAMETER_COUNT;
            ++parameter)
    {
        if (fixed[parameter])
            continue;                   // column stays zero

        Parameters plus = parameters;
        Parameters minus = parameters;

        plus.value[parameter] +=
            FINITE_DIFFERENCE_STEP;

        minus.value[parameter] -=
            FINITE_DIFFERENCE_STEP;

        std::vector<double> plusResiduals;
        std::vector<double> minusResiduals;

        if (!evaluateResiduals(
                    observations,
                    plus,
                    latitude,
                    plusResiduals) ||
                !evaluateResiduals(
                    observations,
                    minus,
                    latitude,
                    minusResiduals))
        {
            return false;
        }

        for (std::size_t row = 0;
                row < residuals.size();
                ++row)
        {
            jacobian[row][parameter] =
                (plusResiduals[row] -
                 minusResiduals[row]) /
                (2.0 * FINITE_DIFFERENCE_STEP);
        }
    }

    return true;
}
}

std::size_t OnStepXModelFitter::minimumObservations(Terms terms)
{
    switch (terms)
    {
        case Terms::INDEX:
            return 2;
        case Terms::AXIS:
            return 3;
        case Terms::CONE:
            return 4;
        case Terms::MECHANICAL:
            return 6;
        case Terms::FULL:
        default:
            return PARAMETER_COUNT;
    }
}

std::size_t OnStepXModelFitter::freeParameters(Terms terms, OnStepXModelMath::MountType mountType)
{
    const FixedMask fixed = fixedParameters(mountType, terms);
    return static_cast<std::size_t>(std::count(fixed.begin(), fixed.end(), false));
}

const char *OnStepXModelFitter::describe(Terms terms, OnStepXModelMath::MountType mountType)
{
    switch (terms)
    {
        case Terms::INDEX:
            return "index offsets";
        case Terms::AXIS:
            if (mountType == OnStepXModelMath::MountType::ALTAZM)
                return "azimuth axis tilt";
            if (mountType == OnStepXModelMath::MountType::ALTALT)
                return "axis alignment";
            return "polar axis alignment";
        case Terms::CONE:
            return "cone error";
        case Terms::MECHANICAL:
            return "axis non-perpendicularity and flexure";
        case Terms::FULL:
        default:
            return "periodic errors";
    }
}

OnStepXModelFitter::FitResult OnStepXModelFitter::fit(
    const std::vector<OnStepXModelMath::Observation> &observations,
    double latitude,
    Terms terms)
{
    FitResult result;
    result.terms = terms;

    if (observations.empty() || observations.size() < minimumObservations(terms))
    {
        result.status =
            FitResult::Status::INSUFFICIENT_OBSERVATIONS;

        return result;
    }

    if (!std::isfinite(latitude))
    {
        result.status =
            FitResult::Status::NON_FINITE;

        return result;
    }

    Parameters parameters;

    // The builder rejects observations whose mount type differs, so the
    // first one stands for all.
    const FixedMask fixed = fixedParameters(observations.front().mountType, terms);
    result.freeParameters = freeParameters(terms, observations.front().mountType);

    std::vector <
    std::array<double, PARAMETER_COUNT >> jacobian;

    std::vector<double> residuals;

    if (!makeJacobian(
                observations,
                parameters,
                latitude,
                fixed,
                jacobian,
                residuals))
    {
        result.status =
            FitResult::Status::NON_FINITE;

        return result;
    }

    double cost = squaredNorm(residuals);
    double lambda = INITIAL_LAMBDA;

    std::size_t finalRank = 0;
    bool converged = false;

    /*
     * The Jacobian was already computed by makeJacobian(), so establish
     * its rank before testing the initial residual. A zero residual is
     * already a converged solution and must not enter the LM step logic,
     * where a candidate with equal cost would never satisfy candidateCost
     * < cost.
     */
    {
        std::array <
        std::array<double, PARAMETER_COUNT>,
            PARAMETER_COUNT > jtj {};

        for (std::size_t row = 0;
                row < residuals.size();
                ++row)
        {
            for (std::size_t i = 0;
                    i < PARAMETER_COUNT;
                    ++i)
            {
                for (std::size_t j = 0;
                        j < PARAMETER_COUNT;
                        ++j)
                {
                    jtj[i][j] +=
                        jacobian[row][i] *
                        jacobian[row][j];
                }
            }
        }

        pinFixed(jtj, fixed);
        finalRank = rankOf(jtj);

        if (cost <= COST_TOLERANCE)
            converged = true;
    }

    for (std::size_t iteration = 0;
            !converged && iteration < MAX_ITERATIONS;
            ++iteration)
    {
        result.iterations = iteration + 1;

        /*
         * Build J^T J and J^T r from the actual finite-difference
         * Jacobian at this iteration.
         */
        std::array <
        std::array<double, PARAMETER_COUNT>,
            PARAMETER_COUNT > jtj {};

        std::array<double, PARAMETER_COUNT> jtr {};

        for (std::size_t row = 0;
                row < residuals.size();
                ++row)
        {
            for (std::size_t i = 0;
                    i < PARAMETER_COUNT;
                    ++i)
            {
                jtr[i] +=
                    jacobian[row][i] *
                    residuals[row];

                for (std::size_t j = 0;
                        j < PARAMETER_COUNT;
                        ++j)
                {
                    jtj[i][j] +=
                        jacobian[row][i] *
                        jacobian[row][j];
                }
            }
        }

        /*
         * Rank is deliberately calculated here, from the same
         * Jacobian that produced this LM normal matrix.
         */
        pinFixed(jtj, fixed);
        finalRank = rankOf(jtj);

        auto normal = jtj;
        std::array<double, PARAMETER_COUNT> rhs {};

        for (std::size_t i = 0;
                i < PARAMETER_COUNT;
                ++i)
        {
            normal[i][i] +=
                lambda *
                std::max(jtj[i][i], 1.0);

            rhs[i] = -jtr[i];
        }

        std::array<double, PARAMETER_COUNT> step {};

        if (!solveLinearSystem(
                    normal,
                    rhs,
                    step))
        {
            lambda =
                std::min(lambda * 100.0,
                         MAX_LAMBDA);

            if (lambda >= MAX_LAMBDA)
            {
                result.status =
                    FitResult::Status::SINGULAR;

                result.rank = finalRank;
                return result;
            }

            continue;
        }

        double stepNormSquared = 0.0;

        for (double value : step)
            stepNormSquared += value * value;

        const double stepNorm =
            std::sqrt(stepNormSquared);

        if (!std::isfinite(stepNorm))
        {
            result.status =
                FitResult::Status::NON_FINITE;

            result.rank = finalRank;
            return result;
        }

        Parameters candidate = parameters;

        for (std::size_t i = 0;
                i < PARAMETER_COUNT;
                ++i)
        {
            candidate.value[i] += step[i];
        }

        std::vector<double> candidateResiduals;

        if (!evaluateResiduals(
                    observations,
                    candidate,
                    latitude,
                    candidateResiduals))
        {
            lambda =
                std::min(lambda * 10.0,
                         MAX_LAMBDA);

            continue;
        }

        const double candidateCost =
            squaredNorm(candidateResiduals);

        if (!std::isfinite(candidateCost))
        {
            lambda =
                std::min(lambda * 10.0,
                         MAX_LAMBDA);

            continue;
        }

        if (candidateCost < cost)
        {
            const double relativeImprovement =
                (cost - candidateCost) /
                std::max(cost, 1.0);

            parameters = candidate;
            residuals =
                std::move(candidateResiduals);

            cost = candidateCost;

            lambda =
                std::max(lambda * 0.3,
                         MIN_LAMBDA);

            if (stepNorm <= STEP_TOLERANCE ||
                    relativeImprovement <= COST_TOLERANCE)
            {
                converged = true;
                break;
            }
        }
        else
        {
            /*
             * At the minimum, rounding makes every step fail to lower the
             * cost, so it's rejected and never reaches the accepted-step
             * test above.  Once even a negligible step can't improve the
             * fit, it has converged.  Without this, about one fit in ten
             * on noisy data ran out its iterations.
             */
            if (stepNorm <= STEP_TOLERANCE)
            {
                converged = true;
                break;
            }

            lambda =
                std::min(lambda * 10.0,
                         MAX_LAMBDA);
        }

        /*
         * Rebuild the Jacobian at the accepted parameter vector
         * before the next LM iteration.
         */
        if (!makeJacobian(
                    observations,
                    parameters,
                    latitude,
                    fixed,
                    jacobian,
                    residuals))
        {
            result.status =
                FitResult::Status::NON_FINITE;

            result.rank = finalRank;
            return result;
        }
    }

    result.rank = finalRank;

    if (!converged)
    {
        result.status =
            FitResult::Status::NO_CONVERGENCE;

        return result;
    }

    if (result.rank < PARAMETER_COUNT)
    {
        result.status =
            FitResult::Status::SINGULAR;

        return result;
    }

    double sumSquared = 0.0;
    double maximum = 0.0;

    for (double residual : residuals)
    {
        sumSquared +=
            residual * residual;

        maximum =
            std::max(maximum,
                     std::abs(residual));
    }

    result.sumSquaredArcsec = sumSquared;
    result.rmsArcsec =
        std::sqrt(
            sumSquared /
            static_cast<double>(residuals.size()));

    result.maxAbsResidualArcsec =
        maximum;

    result.model =
        toModel(parameters);

    result.status =
        FitResult::Status::SUCCESS;

    return result;
}

OnStepXModelFitter::AdaptiveResult OnStepXModelFitter::fitAdaptive(
    const std::vector<OnStepXModelMath::Observation> &observations,
    double latitude)
{
    AdaptiveResult result;
    const double equations = 2.0 * static_cast<double>(observations.size());

    for (std::size_t i = 0; i < TERM_LEVELS; ++i)
    {
        Level &level = result.levels[i];
        level.terms = static_cast<Terms>(i);

        if (observations.size() < minimumObservations(level.terms))
        {
            level.outcome = Level::Outcome::NOT_ENOUGH_OBSERVATIONS;
            continue;
        }

        level.fit = fit(observations, latitude, level.terms);
        if (!level.fit.success())
        {
            level.outcome = (level.fit.status == FitResult::Status::SINGULAR)
                            ? Level::Outcome::NOT_SEPARABLE
                            : Level::Outcome::FAILED;
            continue;
        }

        if (result.chosen < 0)
        {
            level.outcome = Level::Outcome::USED;
            result.chosen = static_cast<int>(i);
            continue;
        }

        /*
         * The levels are nested, so compare with the chosen one by an F-test:
         * the reduction in squared residuals per extra term, relative to the
         * residual variance left by this level.  A level that only fits the
         * noise gives F around 1.
         */
        const FitResult &lower = result.levels[result.chosen].fit;
        const double extraTerms = static_cast<double>(level.fit.freeParameters) -
                                  static_cast<double>(lower.freeParameters);
        const double freedom = equations - static_cast<double>(level.fit.freeParameters);
        const double reduction = lower.sumSquaredArcsec - level.fit.sumSquaredArcsec;

        bool significant = false;
        if (extraTerms > 0.0 && freedom > 0.0 && reduction > 0.0)
        {
            const double variance = level.fit.sumSquaredArcsec / freedom;
            // An exact fit (no noise) is significant if it removed anything real.
            significant = (variance <= 1.0e-18)
                          ? reduction > 1.0e-12
                          : (reduction / extraTerms) / variance > SIGNIFICANCE_F;
        }

        if (significant)
        {
            level.outcome = Level::Outcome::USED;
            result.chosen = static_cast<int>(i);
        }
        else
            level.outcome = Level::Outcome::NOT_SIGNIFICANT;
    }

    return result;
}
