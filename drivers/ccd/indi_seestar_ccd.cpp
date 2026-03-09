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

#include "indi_seestar_ccd.h"
#include "indicom.h"

// We declare an auto pointer to SeestarCCD
std::unique_ptr<SeestarCCD> seestar_ccd(new SeestarCCD());

/**
 * @brief Main entry point - INDI handles this through the unique_ptr
 */

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
SeestarCCD::SeestarCCD()
{
    setVersion(1, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
const char *SeestarCCD::getDefaultName()
{
    return "Seestar CCD";
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool SeestarCCD::initProperties()
{
    // Call parent class first
    AlpacaCCD::initProperties();
    
    // Set Seestar-specific default connection parameters
    setDefaultServerAddress("seestar.local", "32323");
    
    // Remove cooler capability (Seestar doesn't have active cooling)
    uint32_t cap = GetCCDCapability();
    cap &= ~CCD_HAS_COOLER;
    SetCCDCapability(cap);
    
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool SeestarCCD::Connect()
{
    // Call base class Connect
    if (!AlpacaCCD::Connect())
        return false;
    
    // Force remove cooler capability after base class may have added it
    // (Seestar reports cansetccdtemperature=true but we don't want to expose cooler controls)
    uint32_t cap = GetCCDCapability();
    cap &= ~CCD_HAS_COOLER;
    SetCCDCapability(cap);
    
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool SeestarCCD::updateProperties()
{
    // Call base class first
    AlpacaCCD::updateProperties();
    
    // Remove cooler properties if they were added by base class
    if (isConnected())
    {
        deleteProperty("CCD_COOLER");
        deleteProperty("CCD_TEMPERATURE");
        deleteProperty("CCD_COOLER_POWER");
    }
    
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
int SeestarCCD::SetTemperature(double temperature)
{
    // Seestar doesn't have active cooling, so we don't allow temperature setting
    INDI_UNUSED(temperature);
    return -1;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
std::string SeestarCCD::buildAlpacaFormData(const nlohmann::json& request)
{
    // Seestar requires method parameters FIRST, then ClientID/ClientTransactionID
    // This is opposite of standard Alpaca ordering
    std::string form_data;
    
    // Add method-specific parameters first
    for (auto& [key, value] : request.items())
    {
        if (!form_data.empty()) form_data += "&";

        if (value.is_string())
        {
            form_data += key + "=" + value.get<std::string>();
        }
        else if (value.is_number_integer())
        {
            form_data += key + "=" + std::to_string(value.get<int>());
        }
        else if (value.is_number_float())
        {
            form_data += key + "=" + std::to_string(value.get<double>());
        }
        else if (value.is_boolean())
        {
            form_data += key + "=" + (value.get<bool>() ? "true" : "false");
        }
    }

    // Add ClientID and ClientTransactionID AFTER method parameters
    if (!form_data.empty()) form_data += "&";
    form_data += "ClientID=" + std::to_string(getpid());
    form_data += "&ClientTransactionID=" + std::to_string(getTransactionId());

    return form_data;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool SeestarCCD::parseImageBytesMetadata(ImageBytesMetadata* metadata, size_t body_size)
{
    // First, call the base class validation for basic checks
    if (metadata->MetadataVersion != 1)
    {
        LOGF_ERROR("Unsupported ImageBytes metadata version: %d", metadata->MetadataVersion);
        return false;
    }

    if (metadata->ErrorNumber != 0)
    {
        LOGF_ERROR("ImageBytes contains error: %d", metadata->ErrorNumber);
        return false;
    }

    // Log the raw metadata before corrections
    LOGF_INFO("Seestar raw metadata: Rank=%d, Dim1=%d, Dim2=%d, Dim3=%d, TransType=%d, DataStart=%d",
               metadata->Rank, metadata->Dimension1, metadata->Dimension2, metadata->Dimension3,
               metadata->TransmissionElementType, metadata->DataStart);

    // Apply Seestar-specific workarounds
    // Bug #1: Dimension1 and Dimension2 are swapped
    // - Rank field is correct (2 for monochrome)
    // - Dimension1 contains Height (1080) instead of Width
    // - Dimension2 contains Width (1920) instead of Height
    // - Dimension3 is correct (0 for 2D images)
    int32_t actual_rank = metadata->Rank;           // Rank is correct
    int32_t actual_width = metadata->Dimension2;    // Width is in Dimension2
    int32_t actual_height = metadata->Dimension1;   // Height is in Dimension1

    // Validate the corrected values
    if (actual_rank < 2 || actual_rank > 3)
    {
        LOGF_ERROR("Invalid corrected rank: %d (must be 2 or 3)", actual_rank);
        return false;
    }

    if (actual_width <= 0 || actual_height <= 0)
    {
        LOGF_ERROR("Invalid corrected dimensions: %dx%d", actual_width, actual_height);
        return false;
    }

    // Bug #2: TransmissionElementType handling
    // Seestar reports type 8 (UInt16) which is correct for 2 bytes per pixel
    // Keep it as-is (no workaround needed for this field)
    int32_t actual_transmission_type = metadata->TransmissionElementType;

    // Bug #3: DataStart is now correct (44)
    // Earlier firmware versions reported 0, but current version reports 44 correctly
    int32_t actual_data_start = metadata->DataStart;

    // Verify data size matches expectations with corrected values
    size_t bytes_per_element = 2; // Int16
    uint32_t planes = (actual_rank == 3) ? 1 : 1; // For now assume mono
    size_t expected_data_size = actual_width * actual_height * planes * bytes_per_element;
    size_t actual_data_size = body_size - actual_data_start;

    LOGF_DEBUG("Seestar ImageBytes corrected: %dx%dx%d, rank=%d, type=Int16, data_size=%zu (expected=%zu)",
               actual_width, actual_height, planes, actual_rank, actual_data_size, expected_data_size);

    // Allow some tolerance for the data size (within 1% or 1KB, whichever is larger)
    size_t tolerance = std::max(expected_data_size / 100, (size_t)1024);
    if (actual_data_size < expected_data_size - tolerance || actual_data_size > expected_data_size + tolerance)
    {
        LOGF_WARN("Data size mismatch: expected %zu bytes, got %zu bytes (tolerance: %zu)",
                  expected_data_size, actual_data_size, tolerance);
        // Don't fail, just warn - we'll use what we got
    }

    // Update metadata structure with corrected values
    metadata->Rank = actual_rank;
    metadata->Dimension1 = actual_width;
    metadata->Dimension2 = actual_height;
    metadata->Dimension3 = (actual_rank == 3) ? planes : 0;
    metadata->TransmissionElementType = actual_transmission_type;
    metadata->DataStart = actual_data_start;

    LOGF_INFO("Seestar ImageBytes: %dx%d, rank=%d, type=Int16, %zu bytes",
              actual_width, actual_height, actual_rank, actual_data_size);

    return true;
}
