/*
    OnStepX INDI Driver — test_onstepxmodelprotocol.cpp
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

#include "OnStepXModelProtocol.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace
{
constexpr double PI =
    3.1415926535897932384626433832795;

constexpr double DEG =
    PI / 180.0;

constexpr double ARCSEC =
    DEG / 3600.0;

using Math = OnStepXModelMath;
using MountType = Math::MountType;
using Protocol = OnStepXModelProtocol;
}

TEST(OnStepXModelProtocol, QuantizesArcsecondCoefficients)
{
    Math::ModelCoefficients model;

    model.ax1Cor =  12.49 * ARCSEC;
    model.ax2Cor = -12.51 * ARCSEC;
    model.altCor =   1.50 * ARCSEC;
    model.azmCor =  -1.50 * ARCSEC;
    model.doCor  =   0.49 * ARCSEC;
    model.pdCor  =  -0.51 * ARCSEC;
    model.dfCor  = 123.4 * ARCSEC;
    model.tfCor  = -77.6 * ARCSEC;

    const auto values =
        Protocol::quantize(model);

    EXPECT_EQ(values.ax1Cor, 12);
    EXPECT_EQ(values.ax2Cor, -13);
    EXPECT_EQ(values.altCor, 2);
    EXPECT_EQ(values.azmCor, -2);
    EXPECT_EQ(values.doCor, 0);
    EXPECT_EQ(values.pdCor, -1);
    EXPECT_EQ(values.dfCor, 123);
    EXPECT_EQ(values.tfCor, -78);
}

TEST(OnStepXModelProtocol, QuantizesPhaseInDegrees)
{
    Math::ModelCoefficients model;

    model.hcp = 12.49 * DEG;
    model.dcp = -12.51 * DEG;

    const auto values =
        Protocol::quantize(model);

    EXPECT_EQ(values.hcp, 12);
    EXPECT_EQ(values.dcp, -13);
}

TEST(OnStepXModelProtocol, QuantizesAmplitudeInArcseconds)
{
    Math::ModelCoefficients model;

    model.hca = 8.49 * ARCSEC;
    model.dca = -6.51 * ARCSEC;

    const auto values =
        Protocol::quantize(model);

    EXPECT_EQ(values.hca, 8);
    EXPECT_EQ(values.dca, -7);
}

TEST(OnStepXModelProtocol, DequantizesArcseconds)
{
    Protocol::Values values;

    values.ax1Cor = 12;
    values.ax2Cor = -13;
    values.altCor = 4;
    values.azmCor = -5;
    values.doCor = 6;
    values.pdCor = -4;
    values.dfCor = 7;
    values.tfCor = -3;

    const auto model =
        Protocol::dequantize(values);

    EXPECT_NEAR(
        model.ax1Cor,
        12.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        model.ax2Cor,
        -13.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        model.altCor,
        4.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        model.azmCor,
        -5.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        model.doCor,
        6.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        model.pdCor,
        -4.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        model.dfCor,
        7.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        model.tfCor,
        -3.0 * ARCSEC,
        1.0e-15);
}

TEST(OnStepXModelProtocol, DequantizesPhaseInDegrees)
{
    Protocol::Values values;

    values.hcp = 12;
    values.dcp = -13;

    const auto model =
        Protocol::dequantize(values);

    EXPECT_NEAR(
        model.hcp,
        12.0 * DEG,
        1.0e-15);

    EXPECT_NEAR(
        model.dcp,
        -13.0 * DEG,
        1.0e-15);
}

TEST(OnStepXModelProtocol, QuantizeThenDequantizeProducesFirmwareModel)
{
    Math::ModelCoefficients model;

    model.ax1Cor =  2.4 * ARCSEC;
    model.ax2Cor = -3.6 * ARCSEC;
    model.altCor =  4.4 * ARCSEC;
    model.azmCor = -5.6 * ARCSEC;

    model.doCor = 6.4 * ARCSEC;
    model.pdCor = -4.4 * ARCSEC;
    model.dfCor = 7.4 * ARCSEC;
    model.tfCor = -3.4 * ARCSEC;

    model.hcp = 0.6 * DEG;
    model.hca = 8.4 * ARCSEC;

    model.dcp = -0.8 * DEG;
    model.dca = 6.4 * ARCSEC;

    const auto values =
        Protocol::quantize(model);

    const auto firmwareModel =
        Protocol::dequantize(values);

    EXPECT_NEAR(
        firmwareModel.ax1Cor,
        2.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        firmwareModel.ax2Cor,
        -4.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        firmwareModel.altCor,
        4.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        firmwareModel.azmCor,
        -6.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        firmwareModel.doCor,
        6.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        firmwareModel.pdCor,
        -4.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        firmwareModel.dfCor,
        7.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        firmwareModel.tfCor,
        -3.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        firmwareModel.hcp,
        1.0 * DEG,
        1.0e-15);

    EXPECT_NEAR(
        firmwareModel.hca,
        8.0 * ARCSEC,
        1.0e-15);

    EXPECT_NEAR(
        firmwareModel.dcp,
        -1.0 * DEG,
        1.0e-15);

    EXPECT_NEAR(
        firmwareModel.dca,
        6.0 * ARCSEC,
        1.0e-15);
}

TEST(OnStepXModelProtocol, DfUsesCoefficient6ForFork)
{
    EXPECT_EQ(
        Protocol::dfCoefficientIndex(
            MountType::FORK),
        '6');
}

TEST(OnStepXModelProtocol, DfUsesCoefficient6ForAltAzm)
{
    EXPECT_EQ(
        Protocol::dfCoefficientIndex(
            MountType::ALTAZM),
        '6');
}

TEST(OnStepXModelProtocol, DfUsesCoefficient7ForGem)
{
    EXPECT_EQ(
        Protocol::dfCoefficientIndex(
            MountType::GEM),
        '7');
}

TEST(OnStepXModelProtocol, DfUsesCoefficient7ForAltAlt)
{
    EXPECT_EQ(
        Protocol::dfCoefficientIndex(
            MountType::ALTALT),
        '7');
}

TEST(OnStepXModelProtocol, CoefficientIndicesMatchFirmware)
{
    const auto indices =
        Protocol::coefficientIndices();

    ASSERT_EQ(indices.size(), 12u);

    EXPECT_EQ(indices[0], '0');
    EXPECT_EQ(indices[1], '1');
    EXPECT_EQ(indices[2], '2');
    EXPECT_EQ(indices[3], '3');
    EXPECT_EQ(indices[4], '4');
    EXPECT_EQ(indices[5], '5');
    EXPECT_EQ(indices[6], '6');
    EXPECT_EQ(indices[7], '8');
    EXPECT_EQ(indices[8], 'a');
    EXPECT_EQ(indices[9], 'b');
    EXPECT_EQ(indices[10], 'c');
    EXPECT_EQ(indices[11], 'd');
}

TEST(OnStepXModelProtocol, AcceptsFirmwareBoundaryValues)
{
    Protocol::Values values;
    std::string reason;

    EXPECT_TRUE(Protocol::validateForFirmware(values, reason));
    EXPECT_TRUE(reason.empty());

    values.ax1Cor = 1295999;
    EXPECT_TRUE(Protocol::validateForFirmware(values, reason));

    values.ax1Cor = 1296000;
    EXPECT_FALSE(Protocol::validateForFirmware(values, reason));
    EXPECT_EQ(reason, "ax1Cor");

    values = Protocol::Values {};
    values.hcp = 359;
    EXPECT_TRUE(Protocol::validateForFirmware(values, reason));

    values.hcp = 360;
    EXPECT_FALSE(Protocol::validateForFirmware(values, reason));
    EXPECT_EQ(reason, "hcp");
}

TEST(OnStepXModelProtocol, RejectsFirmwareModelLimits)
{
    Protocol::Values values;
    std::string reason;

    values.doCor = 1689721393;
    EXPECT_TRUE(Protocol::validateForFirmware(values, reason));
    values.doCor = 1689721394;
    EXPECT_FALSE(Protocol::validateForFirmware(values, reason));
    EXPECT_EQ(reason, "doCor");

    values = Protocol::Values {};
    values.pdCor = 52803793;
    EXPECT_TRUE(Protocol::validateForFirmware(values, reason));
    values.pdCor = 52803794;
    EXPECT_FALSE(Protocol::validateForFirmware(values, reason));
    EXPECT_EQ(reason, "pdCor");

    values = Protocol::Values {};
    values.dfCor = 52803793;
    EXPECT_TRUE(Protocol::validateForFirmware(values, reason));
    values.dfCor = 52803794;
    EXPECT_FALSE(Protocol::validateForFirmware(values, reason));
    EXPECT_EQ(reason, "dfCor");

    values = Protocol::Values {};
    values.tfCor = 26401896;
    EXPECT_TRUE(Protocol::validateForFirmware(values, reason));
    values.tfCor = 26401897;
    EXPECT_FALSE(Protocol::validateForFirmware(values, reason));
    EXPECT_EQ(reason, "tfCor");
}

TEST(OnStepXModelProtocol, RejectsValuesOutsideFirmwareLongRange)
{
    Protocol::Values values;
    std::string reason;

    values.altCor =
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1;

    EXPECT_FALSE(Protocol::validateForFirmware(values, reason));
    EXPECT_EQ(reason, "altCor");
}
