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
 * The Seestar CCD has bugs in its ImageBytes protocol implementation:
 * 1. Rank is stored in Dimension1 field instead of Rank field
 * 2. Width is stored in Dimension3 field instead of Dimension1
 * 3. Height is correct in Dimension2
 * 4. TransmissionElementType reports Int32 (2) but actually sends Int16 data
 * 5. DataStart reports 0 but should be 44
 * 
 * This driver applies workarounds to correctly interpret Seestar images.
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
