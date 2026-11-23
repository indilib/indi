/*******************************************************************************
  Copyright(c) 2026 Gord Tulloch. All rights reserved.

  Seestar CCD INDI Driver

  This driver inherits from the generic ASCOM Alpaca CCD driver and applies
  workarounds for Seestar CCD's non-standard ImageBytes implementation.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#pragma once

#include "indi_alpaca_ccd.h"

/**
 * @brief The SeestarCCD class provides Seestar specific workarounds for
 * the ASCOM Alpaca CCD driver.
 *
 * Early Seestar firmware sent wrong ImageBytes metadata (rank, dimensions, element type and
 * DataStart). A Seestar S50 with camera driver "Camera V3" 1.3.1-1 sends metadata as the Alpaca
 * specification defines it (Dimension1 = X = 1080, Dimension2 = Y = 1920, UInt16, DataStart 44);
 * this driver checks and logs it and keeps the form-data ordering the Seestar requires.
 */
class SeestarCCD : public AlpacaCCD
{
    public:
        SeestarCCD();
        virtual ~SeestarCCD() = default;

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool Connect() override;
        virtual bool updateProperties() override;

        // Disable cooler controls (Seestar doesn't have active cooling)
        virtual int SetTemperature(double temperature) override;

        /**
         * @brief Build Alpaca form data with Seestar-specific parameter ordering
         * 
         * Seestar requires method parameters BEFORE ClientID/ClientTransactionID
         * (opposite of standard Alpaca ordering)
         */
        virtual std::string buildAlpacaFormData(const nlohmann::json& request) override;

    protected:
        /**
         * @brief Parse ImageBytes metadata with Seestar-specific workarounds
         * 
         * Overrides the base class method to apply Seestar quirk fixes:
         * - Swap dimension fields to correct locations
         * - Force Int16 element type regardless of what's reported
         * - Fix DataStart offset
         * 
         * @param metadata Pointer to ImageBytesMetadata structure (will be modified)
         * @param body_size Total size of HTTP response body
         * @return true if metadata is valid after corrections
         */
        virtual bool parseImageBytesMetadata(ImageBytesMetadata* metadata, size_t body_size) override;
};
