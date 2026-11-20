/*
    OnStepX INDI Driver — OnStepXModelProtocol.h
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

#pragma once

#include "OnStepXModelMath.h"

#include <array>
#include <cstdint>
#include <string>

class OnStepXModelProtocol
{
    public:
        struct Values
        {
            std::int64_t ax1Cor { 0 };
            std::int64_t ax2Cor { 0 };
            std::int64_t altCor { 0 };
            std::int64_t azmCor { 0 };
            std::int64_t doCor  { 0 };
            std::int64_t pdCor  { 0 };
            std::int64_t dfCor  { 0 };
            std::int64_t tfCor  { 0 };

            std::int64_t hcp { 0 };
            std::int64_t hca { 0 };
            std::int64_t dcp { 0 };
            std::int64_t dca { 0 };
        };

        /*
         * Convert the internal floating-point model to the integer
         * representation used by :SX0.
         *
         * Angular corrections:
         *   arcseconds -> integer arcseconds
         *
         * Phase coefficients:
         *   radians -> integer degrees
         *
         * Amplitude coefficients:
         *   radians -> integer arcseconds
         */
        static Values quantize(
            const OnStepXModelMath::ModelCoefficients &model);

        /*
         * Convert the integer protocol representation back into the
         * floating-point representation used by OnStepXModelMath.
         *
         * This represents the model that actually exists in firmware
         * after :SX0 has been applied, rather than the original
         * unquantized fit.
         */
        static OnStepXModelMath::ModelCoefficients dequantize(
            const Values &values);

        /*
         * Return the coefficient index used by :SX0/:GX0 for dfCor.
         *
         * Firmware uses:
         *   6 for FORK and ALTAZM
         *   7 otherwise
         */
        static char dfCoefficientIndex(
            OnStepXModelMath::MountType mountType);

        /*
         * Return the twelve protocol indices in the order used by
         * Values and by the firmware.
         */
        static constexpr std::array<char, 12> coefficientIndices()
        {
            return
            {
                '0', '1', '2', '3',
                '4', '5', '6', '8',
                'a', 'b', 'c', 'd'
            };
        };

        /*
         * Validate the quantised values against the numeric limits
         * enforced when OnStepX reads its persisted AlignModel.
         *
         * This also enforces the 32-bit firmware `long` range used by
         * atol() in the :SX0 handler on the ESP32 target.
         */
        static bool validateForFirmware(
            const Values &values,
            std::string &reason);
};