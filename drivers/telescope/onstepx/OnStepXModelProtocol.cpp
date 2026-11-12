/*
    OnStepX INDI Driver — OnStepXModelProtocol.cpp
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

#include <cmath>
#include <limits>
#include <string>


namespace
{
constexpr double ARCSEC_PER_RAD =
    206264.80624709636;

constexpr double DEG_PER_RAD =
    180.0 / 3.1415926535897932384626433832795;

constexpr double RAD_PER_ARCSEC =
    1.0 / ARCSEC_PER_RAD;

constexpr double RAD_PER_DEG =
    3.1415926535897932384626433832795 / 180.0;


// Matches the firmware's `Deg360` constant.
constexpr long double DEG360 =
    6.283185307179586L;

bool validFirmwareLong(std::int64_t value)
{
    // On the ESP32 target, firmware `long` is 32-bit.
    constexpr auto longMin =
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min());
    constexpr auto longMax =
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());

    return value >= longMin && value <= longMax;
}

bool validArcsecField(
    std::int64_t value,
    long double lower,
    long double upper)
{
    if (!validFirmwareLong(value))
        return false;

    // :SX0 converts the atol() result to double radians and then stores
    // it in the firmware's float AlignModel field. Reproduce that
    // conversion before applying modelRead()'s bounds.
    const float converted = static_cast<float>(
                                static_cast<double>(value) / ARCSEC_PER_RAD);

    return static_cast<long double>(converted) >= lower &&
           static_cast<long double>(converted) <= upper;
}

bool validDegreeField(
    std::int64_t value,
    long double lower,
    long double upper)
{
    if (!validFirmwareLong(value))
        return false;

    const float converted = static_cast<float>(
                                static_cast<double>(value) / DEG_PER_RAD);

    return static_cast<long double>(converted) >= lower &&
           static_cast<long double>(converted) <= upper;
}

bool validateField(
    const char *name,
    std::int64_t value,
    bool arcseconds,
    long double lower,
    long double upper,
    std::string &reason)
{
    const bool valid = arcseconds
                       ? validArcsecField(value, lower, upper)
                       : validDegreeField(value, lower, upper);

    if (!valid)
    {
        reason = name;
        return false;
    }

    return true;
}

std::int64_t roundArcsec(double radians)
{
    return static_cast<std::int64_t>(
               std::lround(radians * ARCSEC_PER_RAD));
}

std::int64_t roundDegrees(double radians)
{
    return static_cast<std::int64_t>(
               std::lround(radians * DEG_PER_RAD));
}
}

OnStepXModelProtocol::Values
OnStepXModelProtocol::quantize(
    const OnStepXModelMath::ModelCoefficients &model)
{
    Values values;

    values.ax1Cor = roundArcsec(model.ax1Cor);
    values.ax2Cor = roundArcsec(model.ax2Cor);
    values.altCor = roundArcsec(model.altCor);
    values.azmCor = roundArcsec(model.azmCor);
    values.doCor  = roundArcsec(model.doCor);
    values.pdCor  = roundArcsec(model.pdCor);
    values.dfCor  = roundArcsec(model.dfCor);
    values.tfCor  = roundArcsec(model.tfCor);

    values.hcp = roundDegrees(model.hcp);
    values.hca = roundArcsec(model.hca);

    values.dcp = roundDegrees(model.dcp);
    values.dca = roundArcsec(model.dca);

    return values;
}

OnStepXModelMath::ModelCoefficients
OnStepXModelProtocol::dequantize(
    const Values &values)
{
    OnStepXModelMath::ModelCoefficients model;

    model.ax1Cor =
        static_cast<double>(values.ax1Cor) *
        RAD_PER_ARCSEC;

    model.ax2Cor =
        static_cast<double>(values.ax2Cor) *
        RAD_PER_ARCSEC;

    model.altCor =
        static_cast<double>(values.altCor) *
        RAD_PER_ARCSEC;

    model.azmCor =
        static_cast<double>(values.azmCor) *
        RAD_PER_ARCSEC;

    model.doCor =
        static_cast<double>(values.doCor) *
        RAD_PER_ARCSEC;

    model.pdCor =
        static_cast<double>(values.pdCor) *
        RAD_PER_ARCSEC;

    model.dfCor =
        static_cast<double>(values.dfCor) *
        RAD_PER_ARCSEC;

    model.tfCor =
        static_cast<double>(values.tfCor) *
        RAD_PER_ARCSEC;

    model.hcp =
        static_cast<double>(values.hcp) *
        RAD_PER_DEG;

    model.hca =
        static_cast<double>(values.hca) *
        RAD_PER_ARCSEC;

    model.dcp =
        static_cast<double>(values.dcp) *
        RAD_PER_DEG;

    model.dca =
        static_cast<double>(values.dca) *
        RAD_PER_ARCSEC;

    return model;
}

char OnStepXModelProtocol::dfCoefficientIndex(
    OnStepXModelMath::MountType mountType)
{
    if (mountType ==
            OnStepXModelMath::MountType::FORK ||
            mountType ==
            OnStepXModelMath::MountType::ALTAZM)
    {
        return '6';
    }

    return '7';
}

bool OnStepXModelProtocol::validateForFirmware(
    const Values &values,
    std::string &reason)
{
    if (!validateField("ax1Cor", values.ax1Cor, true,
                       -DEG360, DEG360, reason)) return false;
    if (!validateField("ax2Cor", values.ax2Cor, true,
                       -DEG360, DEG360, reason)) return false;
    if (!validateField("altCor", values.altCor, true,
                       -16384.0L, 16384.0L, reason)) return false;
    if (!validateField("azmCor", values.azmCor, true,
                       -16384.0L, 16384.0L, reason)) return false;
    if (!validateField("doCor", values.doCor, true,
                       -8192.0L, 8192.0L, reason)) return false;
    if (!validateField("pdCor", values.pdCor, true,
                       -256.0L, 256.0L, reason)) return false;
    if (!validateField("dfCor", values.dfCor, true,
                       -256.0L, 256.0L, reason)) return false;
    if (!validateField("tfCor", values.tfCor, true,
                       -128.0L, 128.0L, reason)) return false;
    if (!validateField("hcp", values.hcp, false,
                       -DEG360, DEG360, reason)) return false;
    if (!validateField("hca", values.hca, true,
                       -16384.0L, 16384.0L, reason)) return false;
    if (!validateField("dcp", values.dcp, false,
                       -DEG360, DEG360, reason)) return false;
    if (!validateField("dca", values.dca, true,
                       -16384.0L, 16384.0L, reason)) return false;

    reason.clear();
    return true;
}
