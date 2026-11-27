/*
    OnStepX INDI Driver — test_onstepxmodelmath.cpp
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
    OnStepX INDI Driver — pure model mathematics tests

    These tests deliberately include only OnStepXModelMath.h. They must not
    construct OnStepXModelBuilder, OnStepXComm, DefaultDevice, or any other
    INDI/hardware object.

    The expected values are transcribed from the firmware, not from the driver:
    GeoAlign::mountToObservedPlace() in OnStepX src/telescope/mount/coordinates/
    Align.hs.cpp (10.29c).  It adds the index terms and subtracts all the
    others.  Earlier expectations here added them, matching a misreading in
    the driver, so a fitted model was uploaded with those terms reversed.
*/

#include "OnStepXModelMath.h"

#include <gtest/gtest.h>

#include <cmath>

namespace
{
constexpr double PI = 3.14159265358979323846;

double deg(double value)
{
    return value * PI / 180.0;
}

double hours(double value)
{
    return value * PI / 12.0;
}

OnStepXModelMath::Observation baseObservation(
    OnStepXModelMath::MountType mountType,
    OnStepXModelMath::PierSide pierSide = OnStepXModelMath::PierSide::EAST)
{
    OnStepXModelMath::Observation observation;
    observation.mountType = mountType;
    observation.pierSide = pierSide;
    return observation;
}

} // namespace

TEST(OnStepXModelMath, WrapRadians)
{
    EXPECT_DOUBLE_EQ(OnStepXModelMath::wrapRadians(0.0), 0.0);
    EXPECT_DOUBLE_EQ(OnStepXModelMath::wrapRadians(PI), PI);
    EXPECT_DOUBLE_EQ(OnStepXModelMath::wrapRadians(-PI), PI);

    EXPECT_NEAR(OnStepXModelMath::wrapRadians(3.0 * PI), PI, 1e-15);
    EXPECT_NEAR(OnStepXModelMath::wrapRadians(-3.0 * PI), PI, 1e-15);
    EXPECT_NEAR(OnStepXModelMath::wrapRadians(deg(270.0)), deg(-90.0), 1e-15);
    EXPECT_NEAR(OnStepXModelMath::wrapRadians(deg(-270.0)), deg(90.0), 1e-15);
}

TEST(OnStepXModelMath, EquatorialToNativeGemUsesHourAngleAndDeclination)
{
    double axis1 = 0.0;
    double axis2 = 0.0;

    OnStepXModelMath::equatorialToNative(
        hours(3.0), deg(-20.0), deg(52.0),
        OnStepXModelMath::MountType::GEM, axis1, axis2);

    EXPECT_NEAR(axis1, hours(3.0), 1e-15);
    EXPECT_NEAR(axis2, deg(-20.0), 1e-15);
}

TEST(OnStepXModelMath, EquatorialToNativeForkUsesHourAngleAndDeclination)
{
    double axis1 = 0.0;
    double axis2 = 0.0;

    OnStepXModelMath::equatorialToNative(
        hours(-5.0), deg(35.0), deg(52.0),
        OnStepXModelMath::MountType::FORK, axis1, axis2);

    EXPECT_NEAR(axis1, hours(-5.0), 1e-15);
    EXPECT_NEAR(axis2, deg(35.0), 1e-15);
}

TEST(OnStepXModelMath, EquatorialToNativeGemWrapsHourAngle)
{
    double axis1 = 0.0;
    double axis2 = 0.0;

    OnStepXModelMath::equatorialToNative(
        hours(13.0), deg(10.0), deg(52.0),
        OnStepXModelMath::MountType::GEM, axis1, axis2);

    EXPECT_NEAR(axis1, hours(-11.0), 1e-15);
    EXPECT_NEAR(axis2, deg(10.0), 1e-15);
}

TEST(OnStepXModelMath, EquatorialToNativeAltAz)
{
    double axis1 = 0.0;
    double axis2 = 0.0;

    // At latitude 0, dec=0 and HA=+6h, the exact OnStepX equToHor
    // sequence gives altitude 0 and azimuth +90 degrees.
    OnStepXModelMath::equatorialToNative(
        hours(6.0), 0.0, 0.0,
        OnStepXModelMath::MountType::ALTAZM, axis1, axis2);

    EXPECT_NEAR(axis1, deg(-90.0), 1e-14);
    EXPECT_NEAR(axis2, 0.0, 1e-14);
}

TEST(OnStepXModelMath, EquatorialToNativeAltAlt)
{
    double axis1 = 0.0;
    double axis2 = 0.0;

    // Same non-singular horizontal point, followed by the exact OnStepX
    // horToAa transformation.
    OnStepXModelMath::equatorialToNative(
        hours(6.0), 0.0, 0.0,
        OnStepXModelMath::MountType::ALTALT, axis1, axis2);

    EXPECT_NEAR(axis1, deg(90.0), 1e-14);
    EXPECT_NEAR(axis2, 0.0, 1e-14);
}

TEST(OnStepXModelMath, ForwardModelWithZeroCoefficientsReturnsMountAxes)
{
    const OnStepXModelMath::ModelCoefficients model;
    const double latitude = deg(52.0);

    for (const auto mountType :
            {
                OnStepXModelMath::MountType::GEM,
                OnStepXModelMath::MountType::FORK,
                OnStepXModelMath::MountType::ALTAZM,
                OnStepXModelMath::MountType::ALTALT
            })
    {
        for (const auto pierSide :
                {
                    OnStepXModelMath::PierSide::EAST,
                    OnStepXModelMath::PierSide::WEST
                })
        {
            auto observation = baseObservation(mountType, pierSide);
            observation.mountAxis1 = deg(23.0);
            observation.mountAxis2 = deg(31.0);

            double axis1 = 0.0;
            double axis2 = 0.0;

            OnStepXModelMath::predictObserved(
                observation, model, latitude, axis1, axis2);

            EXPECT_NEAR(axis1, observation.mountAxis1, 1e-14);
            EXPECT_NEAR(axis2, observation.mountAxis2, 1e-14);
        }
    }
}

TEST(OnStepXModelMath, ForwardModelAppliesAxisCorrections)
{
    OnStepXModelMath::ModelCoefficients model;
    model.ax1Cor = deg(1.25);
    model.ax2Cor = deg(-0.75);

    auto observation = baseObservation(
                           OnStepXModelMath::MountType::GEM,
                           OnStepXModelMath::PierSide::EAST);
    observation.mountAxis1 = deg(20.0);
    observation.mountAxis2 = deg(30.0);

    double axis1 = 0.0;
    double axis2 = 0.0;

    OnStepXModelMath::predictObserved(
        observation, model, deg(52.0), axis1, axis2);

    EXPECT_NEAR(axis1, deg(21.25), 1e-14);
    EXPECT_NEAR(axis2, deg(30.75), 1e-14);
}

TEST(OnStepXModelMath, Axis2CorrectionChangesSignOnWestPier)
{
    OnStepXModelMath::ModelCoefficients model;
    model.ax2Cor = deg(1.0);

    auto east = baseObservation(
                    OnStepXModelMath::MountType::GEM,
                    OnStepXModelMath::PierSide::EAST);
    east.mountAxis1 = deg(20.0);
    east.mountAxis2 = deg(30.0);

    auto west = baseObservation(
                    OnStepXModelMath::MountType::GEM,
                    OnStepXModelMath::PierSide::WEST);
    west.mountAxis1 = east.mountAxis1;
    west.mountAxis2 = east.mountAxis2;

    double eastAxis1 = 0.0;
    double eastAxis2 = 0.0;
    double westAxis1 = 0.0;
    double westAxis2 = 0.0;

    OnStepXModelMath::predictObserved(east, model, deg(52.0), eastAxis1, eastAxis2);
    OnStepXModelMath::predictObserved(west, model, deg(52.0), westAxis1, westAxis2);

    EXPECT_NEAR(eastAxis2, deg(29.0), 1e-14);
    EXPECT_NEAR(westAxis2, deg(31.0), 1e-14);
}

TEST(OnStepXModelMath, AltitudeAzimuthModelUsesItsMountSpecificDfTerm)
{
    OnStepXModelMath::ModelCoefficients model;
    model.dfCor = deg(1.0);

    auto observation = baseObservation(OnStepXModelMath::MountType::ALTAZM);
    observation.mountAxis1 = deg(30.0);
    observation.mountAxis2 = deg(20.0);

    double axis1 = 0.0;
    double axis2 = 0.0;

    OnStepXModelMath::predictObserved(
        observation, model, deg(52.0), axis1, axis2);

    EXPECT_NEAR(axis1, observation.mountAxis1, 1e-14);
    EXPECT_NEAR(axis2,
                observation.mountAxis2 - model.dfCor * std::cos(deg(30.0)),
                1e-14);
}

TEST(OnStepXModelMath, GemModelUsesLatitudeDependentDfTerm)
{
    OnStepXModelMath::ModelCoefficients model;
    model.dfCor = deg(1.0);

    auto observation = baseObservation(OnStepXModelMath::MountType::GEM);
    observation.mountAxis1 = deg(30.0);
    observation.mountAxis2 = deg(20.0);

    const double latitude = deg(52.0);
    double axis1 = 0.0;
    double axis2 = 0.0;

    OnStepXModelMath::predictObserved(
        observation, model, latitude, axis1, axis2);

    // DFd = -dfCor * (cosLat*cos(ax1) + sinLat*tan(ax2)), subtracted.
    const double expected =
        observation.mountAxis2 +
        model.dfCor *
        (std::cos(latitude) * std::cos(observation.mountAxis1) +
         std::sin(latitude) * std::tan(observation.mountAxis2));

    EXPECT_NEAR(axis1, observation.mountAxis1, 1e-14);
    EXPECT_NEAR(axis2, expected, 1e-14);
}

TEST(OnStepXModelMath, PureDoTermMatchesForwardEquation)
{
    OnStepXModelMath::ModelCoefficients model;
    model.doCor = deg(1.0);

    auto observation = baseObservation(OnStepXModelMath::MountType::GEM);
    observation.mountAxis1 = deg(30.0);
    observation.mountAxis2 = deg(20.0);

    double axis1 = 0.0;
    double axis2 = 0.0;
    OnStepXModelMath::predictObserved(observation, model, deg(52.0), axis1, axis2);

    const double expected =
        observation.mountAxis1 - model.doCor / std::cos(observation.mountAxis2);

    EXPECT_NEAR(axis1, expected, 1e-14);
    EXPECT_NEAR(axis2, observation.mountAxis2, 1e-14);
}

TEST(OnStepXModelMath, PurePdTermMatchesForwardEquation)
{
    OnStepXModelMath::ModelCoefficients model;
    model.pdCor = deg(1.0);

    auto observation = baseObservation(OnStepXModelMath::MountType::GEM);
    observation.mountAxis1 = deg(30.0);
    observation.mountAxis2 = deg(20.0);

    double axis1 = 0.0;
    double axis2 = 0.0;
    OnStepXModelMath::predictObserved(observation, model, deg(52.0), axis1, axis2);

    const double expected =
        observation.mountAxis1 + model.pdCor * std::tan(observation.mountAxis2);

    EXPECT_NEAR(axis1, expected, 1e-14);
    EXPECT_NEAR(axis2, observation.mountAxis2, 1e-14);
}

TEST(OnStepXModelMath, PureAltAzmTermsMatchForwardEquation)
{
    OnStepXModelMath::ModelCoefficients model;
    model.altCor = deg(0.8);
    model.azmCor = deg(-0.6);

    auto observation = baseObservation(OnStepXModelMath::MountType::GEM);
    observation.mountAxis1 = deg(30.0);
    observation.mountAxis2 = deg(20.0);

    double axis1 = 0.0;
    double axis2 = 0.0;
    OnStepXModelMath::predictObserved(observation, model, deg(52.0), axis1, axis2);

    const double a1 =
        -model.azmCor * std::cos(observation.mountAxis1) *
        std::tan(observation.mountAxis2) +
        model.altCor * std::sin(observation.mountAxis1) *
        std::tan(observation.mountAxis2);

    const double a2 =
        model.azmCor * std::sin(observation.mountAxis1) +
        model.altCor * std::cos(observation.mountAxis1);

    EXPECT_NEAR(axis1, observation.mountAxis1 - a1, 1e-14);
    EXPECT_NEAR(axis2, observation.mountAxis2 - a2, 1e-14);
}

TEST(OnStepXModelMath, PureCosineTermsMatchForwardEquation)
{
    OnStepXModelMath::ModelCoefficients model;
    model.hcp = deg(3.0);
    model.hca = deg(1.0);
    model.dcp = deg(-4.0);
    model.dca = deg(0.5);

    auto observation = baseObservation(OnStepXModelMath::MountType::GEM);
    observation.mountAxis1 = deg(30.0);
    observation.mountAxis2 = deg(20.0);

    double axis1 = 0.0;
    double axis2 = 0.0;
    OnStepXModelMath::predictObserved(observation, model, deg(52.0), axis1, axis2);

    const double expectedAxis1 =
        observation.mountAxis1 -
        std::cos(observation.mountAxis1 + model.hcp) * model.hca;
    const double expectedAxis2 =
        observation.mountAxis2 -
        std::cos(observation.mountAxis2 + model.dcp) * model.dca;

    EXPECT_NEAR(axis1, expectedAxis1, 1e-14);
    EXPECT_NEAR(axis2, expectedAxis2, 1e-14);
}

TEST(OnStepXModelMath, TfTermMatchesForwardEquation)
{
    OnStepXModelMath::ModelCoefficients model;
    model.tfCor = deg(1.0);

    auto observation = baseObservation(OnStepXModelMath::MountType::GEM);
    observation.mountAxis1 = deg(30.0);
    observation.mountAxis2 = deg(20.0);

    const double latitude = deg(52.0);
    double axis1 = 0.0;
    double axis2 = 0.0;

    OnStepXModelMath::predictObserved(
        observation, model, latitude, axis1, axis2);

    const double tfH =
        model.tfCor *
        (std::cos(latitude) * std::sin(observation.mountAxis1) /
         std::cos(observation.mountAxis2));

    const double tfD =
        model.tfCor *
        (std::cos(latitude) * std::cos(observation.mountAxis1) *
         std::sin(observation.mountAxis2) -
         std::sin(latitude) * std::cos(observation.mountAxis2));

    EXPECT_NEAR(axis1, observation.mountAxis1 - tfH, 1e-14);
    EXPECT_NEAR(axis2, observation.mountAxis2 - tfD, 1e-14);
}

// On the west pier the firmware evaluates the Dec harmonic on the mirrored
// axis 2: 180 deg - ax2 (or -180 - ax2 in the south), with p = -1.
TEST(OnStepXModelMath, WestPierDecHarmonicUsesMirroredAxis2)
{
    OnStepXModelMath::ModelCoefficients model;
    model.dcp = deg(-25.0);
    model.dca = deg(0.5);

    for (const double latitude :
            {
                deg(52.0), deg(-33.0)
            })
    {
        auto observation = baseObservation(OnStepXModelMath::MountType::GEM,
                                           OnStepXModelMath::PierSide::WEST);
        observation.mountAxis1 = deg(-40.0);
        observation.mountAxis2 = deg(20.0);

        double axis1 = 0.0;
        double axis2 = 0.0;
        OnStepXModelMath::predictObserved(observation, model, latitude, axis1, axis2);

        const double mirrored = (latitude >= 0.0 ? PI : -PI) - observation.mountAxis2;
        const double cosD = std::cos(mirrored + model.dcp) * model.dca * -1.0;

        EXPECT_NEAR(axis1, observation.mountAxis1, 1e-14);
        EXPECT_NEAR(axis2, observation.mountAxis2 - cosD, 1e-14);
    }
}

// GeoAlign::init(): on AltAz the model uses latitude 90 deg, whatever the site.
// Tube flex then leaves only TFd = -tfCor * cos(alt) on axis 2.
TEST(OnStepXModelMath, AltAzModelUsesLatitude90)
{
    OnStepXModelMath::ModelCoefficients model;
    model.tfCor = deg(1.0);

    for (const double siteLatitude :
            {
                deg(52.0), deg(-20.0)
            })
    {
        auto observation = baseObservation(OnStepXModelMath::MountType::ALTAZM);
        observation.mountAxis1 = deg(30.0);
        observation.mountAxis2 = deg(40.0);

        double axis1 = 0.0;
        double axis2 = 0.0;
        OnStepXModelMath::predictObserved(observation, model, siteLatitude, axis1, axis2);

        EXPECT_NEAR(axis1, observation.mountAxis1, 1e-14);
        EXPECT_NEAR(axis2, observation.mountAxis2 + model.tfCor * std::cos(observation.mountAxis2), 1e-14);
    }
}

// GeoAlign::init(): on AltAlt the model uses latitude 0, whatever the site, and
// axis flex uses the GEM form: DFd = -dfCor * cos(ax1).
TEST(OnStepXModelMath, AltAltModelUsesLatitude0)
{
    for (const double siteLatitude :
            {
                deg(52.0), deg(-20.0)
            })
    {
        auto observation = baseObservation(OnStepXModelMath::MountType::ALTALT);
        observation.mountAxis1 = deg(30.0);
        observation.mountAxis2 = deg(40.0);
        const double a1 = observation.mountAxis1;
        const double a2 = observation.mountAxis2;

        OnStepXModelMath::ModelCoefficients tf;
        tf.tfCor = deg(1.0);
        double axis1 = 0.0;
        double axis2 = 0.0;
        OnStepXModelMath::predictObserved(observation, tf, siteLatitude, axis1, axis2);
        EXPECT_NEAR(axis1, a1 - tf.tfCor * std::sin(a1) / std::cos(a2), 1e-14);
        EXPECT_NEAR(axis2, a2 - tf.tfCor * std::cos(a1) * std::sin(a2), 1e-14);

        OnStepXModelMath::ModelCoefficients df;
        df.dfCor = deg(1.0);
        OnStepXModelMath::predictObserved(observation, df, siteLatitude, axis1, axis2);
        EXPECT_NEAR(axis1, a1, 1e-14);
        EXPECT_NEAR(axis2, a2 + df.dfCor * std::cos(a1), 1e-14);
    }
}

TEST(OnStepXModelMath, ResidualIsZeroForModelGeneratedObservation)
{
    OnStepXModelMath::ModelCoefficients model;
    model.ax1Cor = deg(0.2);
    model.ax2Cor = deg(-0.3);
    model.altCor = deg(0.1);
    model.azmCor = deg(-0.15);
    model.doCor = deg(0.05);
    model.pdCor = deg(-0.07);
    model.dfCor = deg(0.08);
    model.tfCor = deg(-0.04);
    model.hcp = deg(2.0);
    model.hca = deg(0.03);
    model.dcp = deg(-3.0);
    model.dca = deg(0.02);

    auto observation = baseObservation(OnStepXModelMath::MountType::GEM);
    observation.mountAxis1 = deg(35.0);
    observation.mountAxis2 = deg(25.0);

    double actualAxis1 = 0.0;
    double actualAxis2 = 0.0;
    OnStepXModelMath::predictObserved(
        observation, model, deg(52.0), actualAxis1, actualAxis2);

    observation.actualAxis1 = actualAxis1;
    observation.actualAxis2 = actualAxis2;

    double r1 = 0.0;
    double r2 = 0.0;
    OnStepXModelMath::residual(
        observation, model, deg(52.0), r1, r2);

    EXPECT_NEAR(r1, 0.0, 1e-14);
    EXPECT_NEAR(r2, 0.0, 1e-14);
}

TEST(OnStepXModelMath, ResidualWrapsAxis1AcrossPi)
{
    auto observation = baseObservation(OnStepXModelMath::MountType::GEM);
    observation.mountAxis1 = deg(179.0);
    observation.mountAxis2 = deg(20.0);
    observation.actualAxis1 = deg(-179.0);
    observation.actualAxis2 = observation.mountAxis2;

    const OnStepXModelMath::ModelCoefficients model;
    double r1 = 0.0;
    double r2 = 0.0;

    OnStepXModelMath::residual(
        observation, model, deg(52.0), r1, r2);

    EXPECT_NEAR(r1, deg(2.0) * std::cos(deg(20.0)), 1e-14);
    EXPECT_NEAR(r2, 0.0, 1e-14);
}

TEST(OnStepXModelMath, ResidualAxis1IsWeightedByCosActualAxis2)
{
    auto observation = baseObservation(OnStepXModelMath::MountType::GEM);
    observation.mountAxis1 = 0.0;
    observation.mountAxis2 = deg(60.0);
    observation.actualAxis1 = deg(1.0);
    observation.actualAxis2 = deg(60.0);

    const OnStepXModelMath::ModelCoefficients model;
    double r1 = 0.0;
    double r2 = 0.0;

    OnStepXModelMath::residual(
        observation, model, deg(52.0), r1, r2);

    EXPECT_NEAR(r1, deg(1.0) * 0.5, 1e-14);
    EXPECT_NEAR(r2, 0.0, 1e-14);
}

TEST(OnStepXModelMath, ForwardModelPreservesAxisBounds)
{
    OnStepXModelMath::ModelCoefficients model;
    model.ax1Cor = deg(10.0);
    model.ax2Cor = deg(20.0);
    model.altCor = deg(10.0);
    model.azmCor = deg(10.0);
    model.doCor = deg(5.0);
    model.pdCor = deg(5.0);
    model.dfCor = deg(5.0);
    model.tfCor = deg(5.0);
    model.hca = deg(2.0);
    model.dca = deg(2.0);

    auto observation = baseObservation(OnStepXModelMath::MountType::GEM);
    observation.mountAxis1 = deg(170.0);
    observation.mountAxis2 = deg(80.0);

    double axis1 = 0.0;
    double axis2 = 0.0;
    OnStepXModelMath::predictObserved(
        observation, model, deg(52.0), axis1, axis2);

    EXPECT_LE(axis2, deg(90.0));
    EXPECT_GE(axis2, deg(-90.0));
    EXPECT_LE(axis1, deg(180.0));
    EXPECT_GE(axis1, deg(-180.0));
}

TEST(OnStepXModelMath, PoleGuardSkipsSingularModelTerms)
{
    OnStepXModelMath::ModelCoefficients model;
    model.doCor = deg(10.0);
    model.pdCor = deg(10.0);
    model.dfCor = deg(10.0);
    model.tfCor = deg(10.0);
    model.altCor = deg(10.0);
    model.azmCor = deg(10.0);
    model.hca = deg(10.0);
    model.dca = deg(10.0);

    auto observation = baseObservation(OnStepXModelMath::MountType::GEM);
    observation.mountAxis1 = deg(30.0);
    observation.mountAxis2 = deg(89.99);

    double axis1 = 0.0;
    double axis2 = 0.0;
    OnStepXModelMath::predictObserved(
        observation, model, deg(52.0), axis1, axis2);

    // The current implementation intentionally skips the model terms inside
    // the pole guard, while retaining the axis corrections and bounds.
    EXPECT_NEAR(axis1, observation.mountAxis1, 1e-14);
    EXPECT_NEAR(axis2, observation.mountAxis2, 1e-14);
}
