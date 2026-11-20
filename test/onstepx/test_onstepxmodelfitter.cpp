/*
    OnStepX INDI Driver — test_onstepxmodelfitter.cpp
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

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
constexpr double PI = 3.1415926535897932384626433832795;
constexpr double DEG = PI / 180.0;
constexpr double ARCSEC = DEG / 3600.0;

using Math = OnStepXModelMath;
using MountType = Math::MountType;
using PierSide = Math::PierSide;

Math::ModelCoefficients testModel()
{
    Math::ModelCoefficients model;

    model.ax1Cor =  2.0 * ARCSEC;
    model.ax2Cor = -3.0 * ARCSEC;
    model.altCor =  4.0 * ARCSEC;
    model.azmCor = -5.0 * ARCSEC;
    model.doCor  =  6.0 * ARCSEC;
    model.pdCor  = -4.0 * ARCSEC;
    model.dfCor  =  7.0 * ARCSEC;
    model.tfCor  = -3.0 * ARCSEC;

    model.hcp =  0.6;
    model.hca =  8.0 * ARCSEC;

    model.dcp = -0.8;
    model.dca =  6.0 * ARCSEC;

    return model;
}

Math::Observation syntheticObservation(
    double mountAxis1,
    double mountAxis2,
    MountType mountType,
    PierSide pierSide,
    const Math::ModelCoefficients &model,
    double latitude)
{
    Math::Observation observation;

    observation.mountAxis1 = mountAxis1;
    observation.mountAxis2 = mountAxis2;

    observation.mountType = mountType;
    observation.pierSide = pierSide;

    double observedAxis1 = 0.0;
    double observedAxis2 = 0.0;

    Math::predictObserved(
        observation,
        model,
        latitude,
        observedAxis1,
        observedAxis2);

    observation.actualAxis1 = observedAxis1;
    observation.actualAxis2 = observedAxis2;

    return observation;
}

std::vector<Math::Observation> makeSyntheticObservations(
    MountType mountType,
    const Math::ModelCoefficients &model,
    double latitude)
{
    std::vector<Math::Observation> observations;

    /*
     * The points deliberately cover both axes over a substantial
     * range.  GEM/FORK also use both pier sides.
     */
    constexpr std::size_t COUNT = 90;

    observations.reserve(COUNT);

    for (std::size_t i = 0; i < COUNT; ++i)
    {
        const double x =
            static_cast<double>(i);

        const double axis1 =
            -2.4 +
            0.055 * x +
            0.17 * std::sin(0.31 * x);

        const double axis2 =
            -0.95 +
            0.021 * x +
            0.23 * std::cos(0.27 * x);

        const PierSide side =
            (mountType == MountType::GEM ||
             mountType == MountType::FORK)
            ? ((i & 1)
               ? PierSide::WEST
               : PierSide::EAST)
            : PierSide::EAST;

        observations.push_back(
            syntheticObservation(
                axis1,
                axis2,
                mountType,
                side,
                model,
                latitude));
    }

    return observations;
}

void expectRecoveredModel(
    MountType mountType)
{
    const double latitude =
        0.72;

    const auto trueModel =
        testModel();

    const auto observations =
        makeSyntheticObservations(
            mountType,
            trueModel,
            latitude);

    const auto result =
        OnStepXModelFitter::fit(
            observations,
            latitude);

    ASSERT_TRUE(result.success())
            << "fit status = "
            << static_cast<int>(result.status);

    EXPECT_EQ(result.rank, 12u);

    EXPECT_LT(result.rmsArcsec, 1.0e-6);
    EXPECT_LT(result.maxAbsResidualArcsec, 1.0e-5);

    EXPECT_NEAR(
        result.model.ax1Cor,
        trueModel.ax1Cor,
        1.0e-12);

    EXPECT_NEAR(
        result.model.ax2Cor,
        trueModel.ax2Cor,
        1.0e-12);

    EXPECT_NEAR(
        result.model.altCor,
        trueModel.altCor,
        1.0e-12);

    EXPECT_NEAR(
        result.model.azmCor,
        trueModel.azmCor,
        1.0e-12);

    EXPECT_NEAR(
        result.model.doCor,
        trueModel.doCor,
        1.0e-12);

    EXPECT_NEAR(
        result.model.pdCor,
        trueModel.pdCor,
        1.0e-12);

    EXPECT_NEAR(
        result.model.dfCor,
        trueModel.dfCor,
        1.0e-12);

    /*
     * On AltAz the fitter holds tfCor at 0: at the firmware's model latitude
     * of 90 deg it is indistinguishable from the cosine part of the Dec
     * harmonic, which absorbs it (Dc' = Dc - tfCor) with identical pointing.
     */
    const bool altAz = (mountType == MountType::ALTAZM);

    EXPECT_NEAR(
        result.model.tfCor,
        altAz ? 0.0 : trueModel.tfCor,
        1.0e-12);

    EXPECT_NEAR(
        result.model.hca,
        trueModel.hca,
        1.0e-12);

    if (!altAz)
    {
        EXPECT_NEAR(
            result.model.dca,
            trueModel.dca,
            1.0e-12);
    }

    /*
     * Phase is periodic.  Compare the reconstructed cosine
     * components instead of relying on a particular wrapped
     * phase representation.
     */
    const double trueHc =
        trueModel.hca *
        std::cos(trueModel.hcp);

    const double trueHs =
        -trueModel.hca *
        std::sin(trueModel.hcp);

    const double fittedHc =
        result.model.hca *
        std::cos(result.model.hcp);

    const double fittedHs =
        -result.model.hca *
        std::sin(result.model.hcp);

    const double trueDc =
        trueModel.dca *
        std::cos(trueModel.dcp) -
        (altAz ? trueModel.tfCor : 0.0);

    const double trueDs =
        -trueModel.dca *
        std::sin(trueModel.dcp);

    const double fittedDc =
        result.model.dca *
        std::cos(result.model.dcp);

    const double fittedDs =
        -result.model.dca *
        std::sin(result.model.dcp);

    EXPECT_NEAR(fittedHc, trueHc, 1.0e-12);
    EXPECT_NEAR(fittedHs, trueHs, 1.0e-12);

    EXPECT_NEAR(fittedDc, trueDc, 1.0e-12);
    EXPECT_NEAR(fittedDs, trueDs, 1.0e-12);
}

// Deterministic noise, identical on every platform (std::normal_distribution
// is implementation-defined).
class TestNoise
{
    public:
        explicit TestNoise(unsigned seed) : m_state(seed * 2654435761u + 1u) {}
        double gaussian()
        {
            const double u1 = (next() + 1.0) / 4294967297.0;
            const double u2 = (next() + 1.0) / 4294967297.0;
            return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * PI * u2);
        }
    private:
        double next()
        {
            m_state = m_state * 1664525u + 1013904223u;
            return static_cast<double>(m_state);
        }
        std::uint32_t m_state;
};

/*
 * Noisy data: at the minimum, rounding makes LM steps fail to lower the cost.
 * Each was rejected and never reached the convergence test, so about one fit
 * in ten ran out its iterations and the build reported no convergence.
 */
TEST(OnStepXModelFitter, ConvergesOnNoisyData)
{
    const double latitude = 0.9;
    const auto trueModel = testModel();

    for (const auto mountType :
            {
                MountType::GEM, MountType::FORK, MountType::ALTAZM, MountType::ALTALT
            })
    {
        for (unsigned seed = 1; seed <= 40; ++seed)
        {
            TestNoise noise(seed + 100u * static_cast<unsigned>(mountType));
            auto observations = makeSyntheticObservations(mountType, trueModel, latitude);
            observations.resize(30);
            for (auto &observation : observations)
            {
                observation.actualAxis1 += 2.0 * ARCSEC * noise.gaussian() /
                                           std::max(std::cos(observation.actualAxis2), 0.1);
                observation.actualAxis2 += 2.0 * ARCSEC * noise.gaussian();
            }

            const auto result = OnStepXModelFitter::fit(observations, latitude);
            ASSERT_TRUE(result.success())
                    << "mount type " << static_cast<int>(mountType) << ", seed " << seed
                    << ": fit status " << static_cast<int>(result.status)
                    << " after " << result.iterations << " iterations";
            EXPECT_LT(result.rmsArcsec, 4.0);
        }
    }
}

// ---------------------------------------------------------------------------
// Adaptive term selection
// ---------------------------------------------------------------------------

using Terms = OnStepXModelFitter::Terms;
using Outcome = OnStepXModelFitter::Level::Outcome;

// n observations spread over the full synthetic set, not its first n points.
std::vector<Math::Observation> spreadObservations(MountType mountType, const Math::ModelCoefficients &model,
        double latitude, std::size_t n)
{
    const auto all = makeSyntheticObservations(mountType, model, latitude);
    std::vector<Math::Observation> out;
    for (std::size_t i = 0; i < n; ++i)
        out.push_back(all[(i * all.size()) / n]);
    return out;
}

TEST(OnStepXModelFitter, FreeParametersPerLevelAndMount)
{
    EXPECT_EQ(OnStepXModelFitter::freeParameters(Terms::INDEX, MountType::GEM), 2u);
    EXPECT_EQ(OnStepXModelFitter::freeParameters(Terms::AXIS, MountType::GEM), 4u);
    EXPECT_EQ(OnStepXModelFitter::freeParameters(Terms::CONE, MountType::GEM), 5u);
    EXPECT_EQ(OnStepXModelFitter::freeParameters(Terms::MECHANICAL, MountType::GEM), 8u);
    EXPECT_EQ(OnStepXModelFitter::freeParameters(Terms::FULL, MountType::GEM), 12u);
    // AltAz: tube flex is separable without the harmonics, held at 0 with them.
    EXPECT_EQ(OnStepXModelFitter::freeParameters(Terms::MECHANICAL, MountType::ALTAZM), 8u);
    EXPECT_EQ(OnStepXModelFitter::freeParameters(Terms::FULL, MountType::ALTAZM), 11u);
}

// testModel() with only the terms up to the given level.
Math::ModelCoefficients modelUpTo(Terms terms)
{
    auto model = testModel();
    const int level = static_cast<int>(terms);
    if (level < static_cast<int>(Terms::AXIS))
        model.altCor = model.azmCor = 0.0;
    if (level < static_cast<int>(Terms::CONE))
        model.doCor = 0.0;
    if (level < static_cast<int>(Terms::MECHANICAL))
        model.pdCor = model.dfCor = model.tfCor = 0.0;
    if (level < static_cast<int>(Terms::FULL))
        model.hca = model.dca = 0.0;
    return model;
}

// Each threshold unlocks the next level once the mount has those errors.
// (With every error present, a level is only kept once the terms still
// missing are small next to its own: see AdaptiveLeavesOutTermsThatOnlyFitNoise.)
TEST(OnStepXModelFitter, AdaptiveAddsTermsAsObservationsArrive)
{
    const double latitude = 0.72;
    const struct
    {
        std::size_t observations;
        Terms expected;
    } steps[] =
    {
        { 2, Terms::INDEX }, { 3, Terms::AXIS }, { 4, Terms::CONE },
        { 6, Terms::MECHANICAL }, { 12, Terms::FULL }, { 40, Terms::FULL }
    };

    for (const auto mountType :
            {
                MountType::GEM, MountType::FORK, MountType::ALTAZM, MountType::ALTALT
            })
    {
        for (const auto &step : steps)
        {
            const auto observations = spreadObservations(mountType, modelUpTo(step.expected), latitude,
                                      step.observations);
            const auto result = OnStepXModelFitter::fitAdaptive(observations, latitude);
            ASSERT_TRUE(result.success()) << "mount " << static_cast<int>(mountType)
                                          << ", " << step.observations << " observations";
            EXPECT_EQ(result.levels[result.chosen].terms, step.expected)
                    << "mount " << static_cast<int>(mountType) << ", " << step.observations << " observations";
            for (std::size_t i = static_cast<std::size_t>(result.chosen) + 1; i < OnStepXModelFitter::TERM_LEVELS; ++i)
                EXPECT_EQ(result.levels[i].outcome, Outcome::NOT_ENOUGH_OBSERVATIONS);
        }
    }
}

// A mount with only index errors and 2" of noise: the extra terms would only
// fit the noise, so the F-test keeps them out.
TEST(OnStepXModelFitter, AdaptiveLeavesOutTermsThatOnlyFitNoise)
{
    const double latitude = 0.72;
    Math::ModelCoefficients indexOnly;
    indexOnly.ax1Cor = 120.0 * ARCSEC;
    indexOnly.ax2Cor = -80.0 * ARCSEC;

    auto observations = spreadObservations(MountType::GEM, indexOnly, latitude, 30);
    TestNoise noise(7);
    for (auto &observation : observations)
    {
        observation.actualAxis1 += 2.0 * ARCSEC * noise.gaussian() / std::cos(observation.actualAxis2);
        observation.actualAxis2 += 2.0 * ARCSEC * noise.gaussian();
    }

    const auto result = OnStepXModelFitter::fitAdaptive(observations, latitude);
    ASSERT_TRUE(result.success());
    EXPECT_EQ(result.levels[result.chosen].terms, Terms::INDEX);
    for (std::size_t i = 1; i < OnStepXModelFitter::TERM_LEVELS; ++i)
        EXPECT_EQ(result.levels[i].outcome, Outcome::NOT_SIGNIFICANT) << "level " << i;
    EXPECT_NEAR(result.best().model.ax1Cor, indexOnly.ax1Cor, 2.0 * ARCSEC);
    EXPECT_NEAR(result.best().model.ax2Cor, indexOnly.ax2Cor, 2.0 * ARCSEC);
}

// Observations at one place can fix the index offsets but nothing else.
TEST(OnStepXModelFitter, AdaptiveReportsTermsTheGeometryCannotSeparate)
{
    Math::Observation observation;
    observation.mountType = MountType::GEM;
    observation.pierSide = PierSide::EAST;
    observation.mountAxis1 = 0.4;
    observation.mountAxis2 = 0.3;
    observation.actualAxis1 = 0.4 + 30.0 * ARCSEC;
    observation.actualAxis2 = 0.3;
    const std::vector<Math::Observation> observations(8, observation);

    const auto result = OnStepXModelFitter::fitAdaptive(observations, 0.72);
    ASSERT_TRUE(result.success());
    EXPECT_EQ(result.levels[result.chosen].terms, Terms::INDEX);
    EXPECT_EQ(result.levels[1].outcome, Outcome::NOT_SEPARABLE);
    EXPECT_EQ(result.levels[4].outcome, Outcome::NOT_ENOUGH_OBSERVATIONS);
}

TEST(OnStepXModelFitter, AdaptiveNeedsTwoObservations)
{
    const auto observations = spreadObservations(MountType::GEM, testModel(), 0.72, 1);
    const auto result = OnStepXModelFitter::fitAdaptive(observations, 0.72);
    EXPECT_FALSE(result.success());
    EXPECT_EQ(result.levels[0].outcome, Outcome::NOT_ENOUGH_OBSERVATIONS);
}

TEST(OnStepXModelFitter, RecoversGemModel)
{
    expectRecoveredModel(MountType::GEM);
}

TEST(OnStepXModelFitter, RecoversForkModel)
{
    expectRecoveredModel(MountType::FORK);
}

TEST(OnStepXModelFitter, RecoversAltAzmModel)
{
    expectRecoveredModel(MountType::ALTAZM);
}

TEST(OnStepXModelFitter, RecoversAltAltModel)
{
    expectRecoveredModel(MountType::ALTALT);
}

TEST(OnStepXModelFitter, RejectsInsufficientObservations)
{
    const double latitude = 0.72;

    std::vector<Math::Observation> observations;

    for (std::size_t i = 0; i < 11; ++i)
    {
        Math::Observation observation;

        observation.mountAxis1 =
            0.1 * static_cast<double>(i);

        observation.mountAxis2 =
            0.2 * static_cast<double>(i);

        observation.actualAxis1 =
            observation.mountAxis1;

        observation.actualAxis2 =
            observation.mountAxis2;

        observation.mountType =
            MountType::GEM;

        observation.pierSide =
            PierSide::EAST;

        observations.push_back(observation);
    }

    const auto result =
        OnStepXModelFitter::fit(
            observations,
            latitude);

    EXPECT_FALSE(result.success());

    EXPECT_EQ(
        result.status,
        OnStepXModelFitter::FitResult::Status::
        INSUFFICIENT_OBSERVATIONS);
}

TEST(OnStepXModelFitter, RejectsRankDeficientGeometry)
{
    const double latitude =
        0.72;

    const auto model =
        testModel();

    std::vector<Math::Observation> observations;

    /*
     * Every observation is identical.  This cannot provide
     * independent information for all twelve fitted parameters.
     */
    for (std::size_t i = 0; i < 30; ++i)
    {
        observations.push_back(
            syntheticObservation(
                0.8,
                0.3,
                MountType::GEM,
                PierSide::EAST,
                model,
                latitude));
    }

    const auto result =
        OnStepXModelFitter::fit(
            observations,
            latitude);

    EXPECT_FALSE(result.success());

    EXPECT_LT(result.rank, 12u);
}

TEST(OnStepXModelFitter, RecoversZeroModel)
{
    const double latitude =
        0.72;

    Math::ModelCoefficients zeroModel;

    const auto observations =
        makeSyntheticObservations(
            MountType::GEM,
            zeroModel,
            latitude);

    const auto result =
        OnStepXModelFitter::fit(
            observations,
            latitude);

    ASSERT_TRUE(result.success())
            << "fit status = "
            << static_cast<int>(result.status);

    EXPECT_EQ(result.rank, 12u);

    EXPECT_LT(result.rmsArcsec, 1.0e-8);
    EXPECT_LT(result.maxAbsResidualArcsec, 1.0e-7);

    EXPECT_NEAR(result.model.ax1Cor, 0.0, 1.0e-10);
    EXPECT_NEAR(result.model.ax2Cor, 0.0, 1.0e-10);
    EXPECT_NEAR(result.model.altCor, 0.0, 1.0e-10);
    EXPECT_NEAR(result.model.azmCor, 0.0, 1.0e-10);
    EXPECT_NEAR(result.model.doCor,  0.0, 1.0e-10);
    EXPECT_NEAR(result.model.pdCor,  0.0, 1.0e-10);
    EXPECT_NEAR(result.model.dfCor,  0.0, 1.0e-10);
    EXPECT_NEAR(result.model.tfCor,  0.0, 1.0e-10);
    EXPECT_NEAR(result.model.hca,   0.0, 1.0e-10);
    EXPECT_NEAR(result.model.dca,   0.0, 1.0e-10);
}

TEST(OnStepXModelFitter, ReportsResidualStatistics)
{
    const double latitude =
        0.72;

    const auto model =
        testModel();

    const auto observations =
        makeSyntheticObservations(
            MountType::GEM,
            model,
            latitude);

    const auto result =
        OnStepXModelFitter::fit(
            observations,
            latitude);

    ASSERT_TRUE(result.success());

    EXPECT_TRUE(
        std::isfinite(result.rmsArcsec));

    EXPECT_TRUE(
        std::isfinite(result.maxAbsResidualArcsec));

    EXPECT_GE(
        result.rmsArcsec,
        0.0);

    EXPECT_GE(
        result.maxAbsResidualArcsec,
        result.rmsArcsec);
}
}