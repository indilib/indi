/*******************************************************************************
  Copyright(c) 2024 Frank Wang/Jérémie Klein. All rights reserved.

  WandererCover V4 Pro-EC

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

#include "wanderer_cover_v4_pro_ec.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"
#include <cstring>
#include <string>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <mutex>
#include <chrono>
#include <sstream>
#include <algorithm>

static std::unique_ptr<WandererCoverV4ProEC> wanderercoverv4ec(new WandererCoverV4ProEC());

// Protocol implementations
bool WandererCoverLegacyProtocol::supportsFeature(const std::string& feature) const
{
    // Legacy protocol supports basic features only
    static const std::vector<std::string> supportedFeatures = {
        "cover_control", "light_control",  "position_setting"
    };
    return std::find(supportedFeatures.begin(), supportedFeatures.end(), feature) != supportedFeatures.end();
}

bool WandererCoverLegacyProtocol::detectProtocol(const char* data)
{
    // Parse firmware version from data
    std::string dataStr(data);
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(dataStr);
    
    while (std::getline(tokenStream, token, 'A'))
    {
        if (!token.empty())
            tokens.push_back(token);
    }
    
    // Check if we have enough tokens and the first one is correct
    if (tokens.size() < 2)
        return false;
    
    // Enhanced device detection for legacy protocol
    std::string deviceName = tokens[0];
    
    // Check for exact match first
    if (deviceName == "WandererCoverV4Pro")
    {
        // Parse firmware version
        int firmwareVersion = std::atoi(tokens[1].c_str());
        
        // Legacy protocol for firmware < 20250405
        return firmwareVersion < 20250405 && firmwareVersion > 0;
    }
    
    return false;
}

bool WandererCoverLegacyProtocol::parseDeviceData(const char* data, WandererCoverV4ProEC* device)
{
    try
    {
        // For legacy protocol, we need to parse the data differently
        // The data comes as a single string but we need to extract individual values
        std::string dataStr(data);
        
        // Remove any trailing newlines or carriage returns
        dataStr.erase(std::remove(dataStr.begin(), dataStr.end(), '\n'), dataStr.end());
        dataStr.erase(std::remove(dataStr.begin(), dataStr.end(), '\r'), dataStr.end());
        
        // Split by 'A' separator
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(dataStr);
        
        while (std::getline(tokenStream, token, 'A'))
        {
            if (!token.empty())
                tokens.push_back(token);
        }
        
        // Debug: log all tokens
        for (size_t i = 0; i < tokens.size(); i++)
        {
            // Note: Can't use LOGF_DEBUG here as we don't have access to getDeviceName()
        }
        
        // Check if we have enough tokens for legacy protocol
        if (tokens.size() < 4)
        {
            return false;
        }
        
        // Device Model - check if it's WandererCoverV4
        std::string deviceName = tokens[0];
        if (deviceName != "WandererCoverV4Pro")
        {
            // Check for similar devices that might be compatible
            if (deviceName == "ZXWBProV3" || deviceName == "ZXWBPlusV3" || 
                deviceName == "UltimateV2" || deviceName == "PlusV2"|| deviceName == "WandererEclipse"|| deviceName == "WandererCoverV4")
            {
                // Note: Can't use LOG_WARN here as we don't have access to getDeviceName()
            }
            else
            {
                // Note: Can't use LOG_ERROR here as we don't have access to getDeviceName()
            }
            return false;
        }
        
        // Firmware version
        device->firmware = std::atoi(tokens[1].c_str());
        
        // Update firmware information in the UI
        char firmwareStr[16];
        snprintf(firmwareStr, sizeof(firmwareStr), "%d", device->firmware);
        device->FirmwareTP[device->FIRMWARE_VERSION].setText(firmwareStr);
        device->FirmwareTP.setState(IPS_OK);
        device->FirmwareTP.apply();
        
        // For legacy protocol, try to parse based on the actual data structure
        // Based on the old driver, the structure is: Device, Firmware, CloseSet, OpenSet, CurrentPos, Voltage
        if (tokens.size() >= 5)
        {
            // Standard legacy format: Device, Firmware, CloseSet, OpenSet, CurrentPos, Voltage
            device->closesetread = std::strtod(tokens[2].c_str(), NULL);
            device->opensetread = std::strtod(tokens[3].c_str(), NULL);
            device->positionread = std::strtod(tokens[4].c_str(), NULL);
            
            if (tokens.size() > 5)
            {
                device->voltageread = std::strtod(tokens[5].c_str(), NULL);
            }
            else
            {
                device->voltageread = 0.0;
            }
        }
        else
        {
            return false;
        }
        
        // Update status data
        device->statusData.firmware = device->firmware;
        device->statusData.closePositionSet = device->closesetread;
        device->statusData.openPositionSet = device->opensetread;
        device->statusData.currentPosition = device->positionread;
        device->statusData.voltage = device->voltageread;
        
        // Update the UI with the parsed data
        device->updateData(device->closesetread, device->opensetread, device->positionread, device->voltageread, device->flatpanelbrightnessread, device->asiaircontrolenabledread);
        
        // Update the Close and Open position settings with the values from the device
        device->CloseSetNP[device->CloseSet].setValue(device->closesetread);
        device->CloseSetNP.setState(IPS_OK);
        device->CloseSetNP.apply();
        
        device->OpenSetNP[device->OpenSet].setValue(device->opensetread);
        device->OpenSetNP.setState(IPS_OK);
        device->OpenSetNP.apply();
        
        return true;
    }
    catch(std::exception &e)
    {
        // Note: Can't use logging here as we don't have access to getDeviceName()
        return false;
    }
}

std::string WandererCoverLegacyProtocol::generateOpenCommand() const { return "1001"; }
std::string WandererCoverLegacyProtocol::generateCloseCommand() const { return "1000"; }
std::string WandererCoverLegacyProtocol::generateSetBrightnessCommand(uint16_t value) const { return std::to_string(value); }
std::string WandererCoverLegacyProtocol::generateTurnOffLightCommand() const { return "9999"; }
std::string WandererCoverLegacyProtocol::generateSetOpenPositionCommand(double value) const { return std::to_string((int)(value * 100 + 40000)); }
std::string WandererCoverLegacyProtocol::generateSetClosePositionCommand(double value) const { return std::to_string((int)(value * 100 + 10000)); }
std::string WandererCoverLegacyProtocol::generateASIAIRControlCommand(bool enable) const { return enable ? "1500003" : "1500004"; }
std::string WandererCoverLegacyProtocol::generateCustomBrightnessCommand(int brightness, int customNumber) const { return std::to_string(customNumber * 1000000 + brightness); }

// Modern protocol implementation
bool WandererCoverModernProtocol::supportsFeature(const std::string& feature) const
{
    // Modern protocol supports all features
    static const std::vector<std::string> supportedFeatures = {
        "cover_control", "light_control", "position_setting",
        "asiair_control", "custom_brightness", "extended_status"
    };
    return std::find(supportedFeatures.begin(), supportedFeatures.end(), feature) != supportedFeatures.end();
}

bool WandererCoverModernProtocol::detectProtocol(const char* data)
{
    // Parse firmware version from data
    std::string dataStr(data);
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(dataStr);
    
    while (std::getline(tokenStream, token, 'A'))
    {
        if (!token.empty())
            tokens.push_back(token);
    }
    
    // Check if we have enough tokens and the first one is correct
    if (tokens.size() < 2)
        return false;
    
    // Enhanced device detection for modern protocol
    std::string deviceName = tokens[0];
    
    // Check for exact match first
    if (deviceName == "WandererCoverV4Pro")
    {
        // Parse firmware version
        int firmwareVersion = std::atoi(tokens[1].c_str());
        
        // Modern protocol for firmware >= 20250405
        return firmwareVersion >= 20250405;
    }
    
    return false;
}

bool WandererCoverModernProtocol::parseDeviceData(const char* data, WandererCoverV4ProEC* device)
{
    try
    {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(data);
        
        while (std::getline(tokenStream, token, 'A'))
        {
            if (!token.empty())
                tokens.push_back(token);
        }
        
        if (tokens.size() < 8)
            return false;
        
        // Enhanced device detection for modern protocol
        std::string deviceName = tokens[0];
        if (deviceName != "WandererCoverV4Pro")
        {
            // Check for similar devices that might be compatible
            if (deviceName == "ZXWBProV3" || deviceName == "ZXWBPlusV3" || 
                deviceName == "UltimateV2" || deviceName == "PlusV2"|| deviceName == "WandererEclipse"|| deviceName == "WandererDewTerminator"|| deviceName == "WandererCoverV4")
            {
                // Note: Can't use LOG_WARN here as we don't have access to getDeviceName()
            }
            else
            {
                // Note: Can't use LOG_ERROR here as we don't have access to getDeviceName()
            }
            return false;
        }
        
        // Parse firmware version
        device->firmware = std::atoi(tokens[1].c_str());
        
        // Update firmware information in the UI
        char firmwareStr[16];
        snprintf(firmwareStr, sizeof(firmwareStr), "%d", device->firmware);
        device->FirmwareTP[device->FIRMWARE_VERSION].setText(firmwareStr);
        device->FirmwareTP.setState(IPS_OK);
        device->FirmwareTP.apply();
        
        // Parse position data
        device->closesetread = std::strtod(tokens[2].c_str(), NULL);
        device->opensetread = std::strtod(tokens[3].c_str(), NULL);
        device->positionread = std::strtod(tokens[4].c_str(), NULL);
        device->voltageread = std::strtod(tokens[5].c_str(), NULL);
        
        // Parse extended data for modern protocol
        if (tokens.size() >= 8)
        {
            device->flatpanelbrightnessread = std::atoi(tokens[6].c_str());
            device->statusData.flatPanelBrightness = device->flatpanelbrightnessread;
            
            device->asiaircontrolenabledread = (tokens.size() > 8) ? (std::atoi(tokens[8].c_str()) == 1) : false;
            device->statusData.asiairControlEnabled = device->asiaircontrolenabledread;
            

        }
        
        // Update status data
        device->statusData.firmware = device->firmware;
        device->statusData.closePositionSet = device->closesetread;
        device->statusData.openPositionSet = device->opensetread;
        device->statusData.currentPosition = device->positionread;
        device->statusData.voltage = device->voltageread;
        
        // Update the UI with the parsed data
        device->updateData(device->closesetread, device->opensetread, device->positionread, device->voltageread, device->flatpanelbrightnessread, device->asiaircontrolenabledread);
        
        // Update the Close and Open position settings with the values from the device
        device->CloseSetNP[device->CloseSet].setValue(device->closesetread);
        device->CloseSetNP.setState(IPS_OK);
        device->CloseSetNP.apply();
        
        device->OpenSetNP[device->OpenSet].setValue(device->opensetread);
        device->OpenSetNP.setState(IPS_OK);
        device->OpenSetNP.apply();
        
        return true;
    }
    catch(std::exception &e)
    {
        // Note: Can't use LOG_ERROR here as we don't have access to getDeviceName()
        return false;
    }
}

std::string WandererCoverModernProtocol::generateOpenCommand() const { return "1001"; }
std::string WandererCoverModernProtocol::generateCloseCommand() const { return "1000"; }
std::string WandererCoverModernProtocol::generateSetBrightnessCommand(uint16_t value) const { return std::to_string(value); }
std::string WandererCoverModernProtocol::generateTurnOffLightCommand() const { return "9999"; }
std::string WandererCoverModernProtocol::generateSetOpenPositionCommand(double value) const { return std::to_string((int)(value * 100 + 40000)); }
std::string WandererCoverModernProtocol::generateSetClosePositionCommand(double value) const { return std::to_string((int)(value * 100 + 10000)); }
std::string WandererCoverModernProtocol::generateASIAIRControlCommand(bool enable) const { return enable ? "1500003" : "1500004"; }
std::string WandererCoverModernProtocol::generateCustomBrightnessCommand(int brightness, int customNumber) const { return std::to_string(customNumber * 1000000 + brightness); }

WandererCoverV4ProEC::WandererCoverV4ProEC() : DustCapInterface(this), LightBoxInterface(this)
{
    setVersion(1, 3);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *WandererCoverV4ProEC::getDefaultName()
{
    return "WandererCover V4 Pro-EC";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::initProperties()
{
    INDI::DefaultDevice::initProperties();

    LI::initProperties(MAIN_CONTROL_TAB, CAN_DIM);
    DI::initProperties(MAIN_CONTROL_TAB);

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE);
    addAuxControls();

    //Data read - extended for new protocol
    DataNP[closeset_read].fill( "Closed_Position", "Closed Position Set(°)", "%4.2f", 0, 999, 100, 0);
    DataNP[openset_read].fill( "Open_Position", "Open Position Set(°)", "%4.2f", 0, 999, 100, 0);
    DataNP[position_read].fill( "Current_Position", "Current Position(°)", "%4.2f", 0, 999, 100, 0);
    DataNP[voltage_read].fill( "Voltage", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    DataNP[flat_panel_brightness_read].fill( "Flat_Panel_Brightness", "Flat Panel Brightness", "%4.2f", 0, 255, 1, 0);
    DataNP[asiair_control_enabled_read].fill( "ASIAIR_Control_Enabled", "ASIAIR Control Enabled", "%4.2f", 0, 1, 1, 0);
    DataNP.fill(getDeviceName(), "STATUS", "Real Time Status", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Firmware information
    FirmwareTP[FIRMWARE_VERSION].fill("FIRMWARE_VERSION", "Firmware Version", "Unknown");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_INFO", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    LightIntensityNP[0].setMax(255);
    LightIntensityNP[0].setValue(100);


    // Close Set
    CloseSetNP[CloseSet].fill( "CloseSet", "10-90", "%.2f", 10, 90, 0.01, 20);
    CloseSetNP.fill(getDeviceName(), "CloseSet", "Close Position(°)", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Open Set
    OpenSetNP[OpenSet].fill( "OpenSet", "100-300", "%.2f", 100, 300, 0.01, 150);
    OpenSetNP.fill(getDeviceName(), "OpenSet", "Open Position(°)", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // New protocol features
    ASIAIRControlSP[ASIAIR_ENABLE].fill("ASIAIR_ENABLE", "Enable ASIAIR Control", ISS_OFF);
    ASIAIRControlSP[ASIAIR_DISABLE].fill("ASIAIR_DISABLE", "Disable ASIAIR Control", ISS_ON);
    ASIAIRControlSP.fill(getDeviceName(), "ASIAIR_CONTROL", "ASIAIR Control", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    CustomBrightnessNP[CUSTOM_BRIGHTNESS_1].fill("CUSTOM_BRIGHTNESS_1", "Custom Brightness 1", "%1.0f", 0, 255, 1, 1);
    CustomBrightnessNP[CUSTOM_BRIGHTNESS_2].fill("CUSTOM_BRIGHTNESS_2", "Custom Brightness 2", "%1.0f", 0, 255, 1, 50);
    CustomBrightnessNP[CUSTOM_BRIGHTNESS_3].fill("CUSTOM_BRIGHTNESS_3", "Custom Brightness 3", "%1.0f", 0, 255, 1, 255);
    CustomBrightnessNP.fill(getDeviceName(), "CUSTOM_BRIGHTNESS", "Custom Brightness", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);


    setDefaultPollingPeriod(2000);

    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    serialConnection->registerHandshake([&]()
    {
        return detectProtocol();
    });
    registerConnection(serialConnection);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Update firmware information
        char firmwareStr[16];
        snprintf(firmwareStr, sizeof(firmwareStr), "%d", firmware);
        FirmwareTP[FIRMWARE_VERSION].setText(firmwareStr);
        

        // Update the Close and Open position settings with the values from the device
        CloseSetNP[CloseSet].setValue(closesetread);
        OpenSetNP[OpenSet].setValue(opensetread);

        defineProperty(DataNP);
        defineProperty(FirmwareTP);
        defineProperty(CloseSetNP);
        defineProperty(OpenSetNP);

        // Define new protocol features if supported
        if (currentProtocol && currentProtocol->supportsFeature("asiair_control"))
        {
            defineProperty(ASIAIRControlSP);
        }
        if (currentProtocol && currentProtocol->supportsFeature("custom_brightness"))
        {
            defineProperty(CustomBrightnessNP);
        }

    }
    else
    {
        deleteProperty(DataNP);
        deleteProperty(FirmwareTP);
        deleteProperty(OpenSetNP);
        deleteProperty(CloseSetNP);
        deleteProperty(ASIAIRControlSP);
        deleteProperty(CustomBrightnessNP);
    }

    DI::updateProperties();
    LI::updateProperties();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererCoverV4ProEC::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);
    LI::ISGetProperties(dev);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::ISSnoopDevice(XMLEle *root)
{
    LI::snoop(root);
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::detectProtocol()
{
    try
    {
        LOG_DEBUG("Starting protocol detection...");
        
        // Try to lock the mutex with a short timeout
        if (!serialPortMutex.try_lock_for(std::chrono::milliseconds(100)))
        {
            LOG_DEBUG("Serial port is busy during protocol detection");
            return false;
        }

        std::lock_guard<std::timed_mutex> lock(serialPortMutex, std::adopt_lock);

        PortFD = serialConnection->getPortFD();
        tcflush(PortFD, TCIOFLUSH);
        
        // Read data from the device
        char buffer[512] = {0};
        int nbytes_read = 0, rc = -1;
        
        // First, try to read any available data immediately
        int available = 0;
        if (ioctl(PortFD, FIONREAD, &available) == 0)
        {
            LOGF_DEBUG("Bytes available for reading: %d", available);
        }
        
        if ((rc = tty_read_section(PortFD, buffer, '\n', 2, &nbytes_read)) != TTY_OK)
        {
            if (rc == TTY_TIME_OUT)
            {
                LOG_DEBUG("Timeout reading from device during protocol detection");
                LOG_DEBUG("Trying to read any available data without timeout...");
                
                // Try to read without timeout to see if there's any data
                if (tty_read(PortFD, buffer, 1, 0, &nbytes_read) == TTY_OK)
                {
                    LOGF_DEBUG("Found %d bytes without timeout: '%s'", nbytes_read, buffer);
                }
                else
                {
                    LOG_DEBUG("No data available without timeout either");
                }
                
                LOG_ERROR("Protocol detection failed: No data received from device");
                return false;
            }
            
            char errorMessage[MAXRBUF];
            tty_error_msg(rc, errorMessage, MAXRBUF);
            LOGF_ERROR("Failed to read data from device during protocol detection. Error: %s", errorMessage);
            return false;
        }
        
        LOGF_DEBUG("Raw data received from device: '%s' (length: %d)", buffer, nbytes_read);
        
        // Parse firmware version from the data
        std::string dataStr(buffer);
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(dataStr);
        
        while (std::getline(tokenStream, token, 'A'))
        {
            if (!token.empty())
                tokens.push_back(token);
        }
        
        LOGF_DEBUG("Parsed %zu tokens from device data", tokens.size());
        for (size_t i = 0; i < tokens.size(); i++)
        {
            LOGF_DEBUG("Token[%zu]: '%s'", i, tokens[i].c_str());
        }
        
        // Check if we have enough tokens and the first one is correct
        if (tokens.size() < 2)
        {
            LOGF_ERROR("Invalid data format: Not enough tokens. Expected at least 2, got %zu", tokens.size());
            return false;
        }
        
        // Enhanced device detection with better error reporting
        std::string deviceName = tokens[0];
        LOGF_DEBUG("Device identification: '%s'", deviceName.c_str());
        
        // Check for exact match first
        if (deviceName == "WandererCoverV4Pro")
        {
            LOG_INFO("WandererCover V4 Pro-EC device detected");
        }
        // Check for similar devices that might be compatible
        else if (deviceName == "ZXWBProV3" || deviceName == "ZXWBPlusV3" || 
                deviceName == "UltimateV2" || deviceName == "PlusV2"|| deviceName == "WandererEclipse"|| deviceName == "WandererDewTerminator"|| deviceName == "WandererCoverV4")
        {
            LOGF_ERROR("WandererAstro products detected, but the model does not match:'%s'. This driver is designed for WandererCover V4 Pro-EC, please choose the right driver or try another serial port!", deviceName.c_str());
            return false;
        }
        else
        {
            LOGF_ERROR("Unsupported device detected: '%s'. Expected 'WandererCoverV4Pro'", deviceName.c_str());
            LOG_ERROR("This driver is specifically designed for WandererCover V4 Pro-EC devices only.");
            return false;
        }
        
        // Parse firmware version
        int firmwareVersion = std::atoi(tokens[1].c_str());
        LOGF_INFO("Detected firmware version: %d", firmwareVersion);
        
        // Validate firmware version
        if (firmwareVersion <= 0)
        {
            LOGF_ERROR("Invalid firmware version: %d. Cannot determine protocol.", firmwareVersion);
            return false;
        }
        
        // Choose protocol based on firmware version
        if (firmwareVersion >= 20250405)
        {
            std::unique_ptr<WandererCoverModernProtocol> modernProtocol = std::make_unique<WandererCoverModernProtocol>();
            setProtocol(std::move(modernProtocol));
            LOG_INFO("Using modern protocol (firmware >= 20250405) Please note that in the newer firmware, to protect dark conditions, the flat light will remain off whenever the Cover is open.");
            return true;
        }
        else if (firmwareVersion > 0)
        {
            std::unique_ptr<WandererCoverLegacyProtocol> legacyProtocol = std::make_unique<WandererCoverLegacyProtocol>();
            setProtocol(std::move(legacyProtocol));
            LOG_INFO("Using legacy protocol (firmware < 20250405) Firmware update recommended.");
            return true;
        }
        else
        {
            LOGF_ERROR("Invalid firmware version: %d. Cannot determine protocol.", firmwareVersion);
            return false;
        }
    }
    catch(std::exception &e)
    {
        LOGF_ERROR("Exception occurred during protocol detection: %s", e.what());
        return false;
    }
}

void WandererCoverV4ProEC::setProtocol(std::unique_ptr<IWandererCoverProtocol> protocol)
{
    currentProtocol = std::move(protocol);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::getData()
{
    // Try to lock the mutex with a short timeout
    if (!serialPortMutex.try_lock_for(std::chrono::milliseconds(100)))
    {
        LOG_DEBUG("Serial port is busy, skipping status update");
        return true;
    }

    std::lock_guard<std::timed_mutex> lock(serialPortMutex, std::adopt_lock);

    try
    {
        LOG_DEBUG("Reading data from device...");
        
        PortFD = serialConnection->getPortFD();
        tcflush(PortFD, TCIOFLUSH);
        
        // Read all data from the device as a single line with 'A' separators
        char buffer[512] = {0};
        int nbytes_read = 0, rc = -1;
        
        // Use a shorter timeout for reading to prevent blocking too long
        if ((rc = tty_read_section(PortFD, buffer, '\n', 2, &nbytes_read)) != TTY_OK)
        {
            // If we get a timeout, it's not necessarily an error - the device might just be busy
            if (rc == TTY_TIME_OUT)
            {
                LOG_DEBUG("Timeout reading from device, will try again later");
                return true;
            }
            
            char errorMessage[MAXRBUF];
            tty_error_msg(rc, errorMessage, MAXRBUF);
            LOGF_ERROR("Failed to read data from device. Error: %s", errorMessage);
            return false;
        }
        
        LOGF_DEBUG("Data received from device: '%s' (length: %d)", buffer, nbytes_read);
        
        // Parse the received data using the current protocol
        bool parseResult = parseDeviceData(buffer);
        LOGF_DEBUG("Data parsing result: %s", parseResult ? "success" : "failed");
        return parseResult;
    }
    catch(std::exception &e)
    {
        LOGF_ERROR("Exception occurred while reading data from device: %s", e.what());
        return false;
    }
    
    return true;
}

bool WandererCoverV4ProEC::parseDeviceData(const char *data)
{
    if (!currentProtocol)
    {
        LOG_ERROR("No protocol handler available");
        return false;
    }
    
    LOGF_DEBUG("Parsing data with protocol: %s", currentProtocol->getProtocolName().c_str());
    return currentProtocol->parseDeviceData(data, this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererCoverV4ProEC::updateData(double closesetread, double opensetread, double positionread, double voltageread, double flatpanelbrightnessread, double asiaircontrolenabledread)
{
    // Update basic data
    DataNP[closeset_read].setValue(closesetread);
    DataNP[openset_read].setValue(opensetread);
    DataNP[position_read].setValue(positionread);
    DataNP[voltage_read].setValue(voltageread);
    

    // Update extended data only if supported by current protocol
    if (currentProtocol && currentProtocol->supportsFeature("extended_status"))
    {
        DataNP[flat_panel_brightness_read].setValue(flatpanelbrightnessread);
        DataNP[asiair_control_enabled_read].setValue(asiaircontrolenabledread);
    }
    else
    {
        // For legacy protocol, set default values for unsupported features
        DataNP[flat_panel_brightness_read].setValue(-1);
        DataNP[asiair_control_enabled_read].setValue(-1);
    }
    

    DataNP.setState(IPS_OK);
    DataNP.apply();

    auto prevParked = ParkCapSP[CAP_PARK].getState() == ISS_ON;
    auto prevState = ParkCapSP.getState();

    ParkCapSP[CAP_PARK].setState((positionread - 10 <= closesetread) ? ISS_ON : ISS_OFF);
    ParkCapSP[CAP_UNPARK].setState((positionread + 10 >= opensetread) ? ISS_ON : ISS_OFF);
    ParkCapSP.setState((ParkCapSP[CAP_PARK].getState() == ISS_ON
                        || ParkCapSP[CAP_UNPARK].getState() == ISS_ON) ? IPS_OK : IPS_IDLE);

    auto currentParked = ParkCapSP[CAP_PARK].getState() == ISS_ON;
    auto currentState = ParkCapSP.getState();

    // Only update on state changes
    if ((prevParked != currentParked) || (prevState != currentState))
        ParkCapSP.apply();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (LI::processText(dev, name, texts, names, n))
        return true;

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (LI::processSwitch(dev, name, states, names, n))
        return true;

    if (DI::processSwitch(dev, name, states, names, n))
        return true;

    if (dev && !strcmp(dev, getDeviceName()))
    {
        // ASIAIR Control
        if (ASIAIRControlSP.isNameMatch(name))
        {
            if (!currentProtocol || !currentProtocol->supportsFeature("asiair_control"))
            {
                LOG_ERROR("ASIAIR control not supported by current protocol");
                ASIAIRControlSP.setState(IPS_ALERT);
                ASIAIRControlSP.apply();
                return true;
            }

            bool rc = false;
            for (int i = 0; i < n; i++)
            {
                if (strcmp(names[i], "ASIAIR_ENABLE") == 0 && states[i] == ISS_ON)
                {
                    rc = sendCommand(currentProtocol->generateASIAIRControlCommand(true));
                }
                else if (strcmp(names[i], "ASIAIR_DISABLE") == 0 && states[i] == ISS_ON)
                {
                    rc = sendCommand(currentProtocol->generateASIAIRControlCommand(false));
                }
            }

            ASIAIRControlSP.setState(rc ? IPS_OK : IPS_ALERT);
            if (ASIAIRControlSP.getState() == IPS_OK)
                ASIAIRControlSP.update(states, names, n);
            ASIAIRControlSP.apply();
            return true;
        }

        
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (LI::processNumber(dev, name, values, names, n))
        return true;

    if (dev && !strcmp(dev, getDeviceName()))
    {
        
        // Close Set
        if (CloseSetNP.isNameMatch(name))
        {
            if (!currentProtocol)
            {
                LOG_ERROR("No protocol handler available");
                CloseSetNP.setState(IPS_ALERT);
                CloseSetNP.apply();
                return true;
            }

            bool rc1 = false;

            for (int i = 0; i < n; i++)
            {
                if(values[i] < 10 || values[i] > 90)
                {
                    CloseSetNP.setState(IPS_ALERT);
                    LOG_ERROR("Out of range! Allowed closed angle: 10-90 degrees.");
                    return false;
                }
                rc1 = sendCommand(currentProtocol->generateSetClosePositionCommand(values[i]));
            }

            CloseSetNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (CloseSetNP.getState() == IPS_OK)
            {
                CloseSetNP.update(values, names, n);
                saveConfig(CloseSetNP);
            }
            CloseSetNP.apply();
            return true;
        }
        // Open Set
        if (OpenSetNP.isNameMatch(name))
        {
            if (!currentProtocol)
            {
                LOG_ERROR("No protocol handler available");
                OpenSetNP.setState(IPS_ALERT);
                OpenSetNP.apply();
                return true;
            }

            bool rc1 = false;

            for (int i = 0; i < n; i++)
            {
                if(values[i] < 100 || values[i] > 300)
                {
                    OpenSetNP.setState(IPS_ALERT);
                    LOG_ERROR("Out of range! Allowed open angle: 100-300 degrees.");
                    return false;
                }
                rc1 = sendCommand(currentProtocol->generateSetOpenPositionCommand(values[i]));
            }

            OpenSetNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (OpenSetNP.getState() == IPS_OK)
            {
                OpenSetNP.update(values, names, n);
                saveConfig(OpenSetNP);
            }
            OpenSetNP.apply();
            return true;
        }

        // Custom Brightness
        if (CustomBrightnessNP.isNameMatch(name))
        {
            if (!currentProtocol || !currentProtocol->supportsFeature("custom_brightness"))
            {
                LOG_ERROR("Custom brightness not supported by current protocol");
                CustomBrightnessNP.setState(IPS_ALERT);
                CustomBrightnessNP.apply();
                return true;
            }

            bool rc = false;
            for (int i = 0; i < n; i++)
            {
                if (strcmp(names[i], "CUSTOM_BRIGHTNESS_1") == 0)
                {
                    rc = sendCommand(currentProtocol->generateCustomBrightnessCommand(static_cast<int>(values[i]), 1));
                }
                else if (strcmp(names[i], "CUSTOM_BRIGHTNESS_2") == 0)
                {
                    rc = sendCommand(currentProtocol->generateCustomBrightnessCommand(static_cast<int>(values[i]), 2));
                }
                else if (strcmp(names[i], "CUSTOM_BRIGHTNESS_3") == 0)
                {
                    rc = sendCommand(currentProtocol->generateCustomBrightnessCommand(static_cast<int>(values[i]), 3));
                }
            }

            CustomBrightnessNP.setState(rc ? IPS_OK : IPS_ALERT);
            if (CustomBrightnessNP.getState() == IPS_OK)
                CustomBrightnessNP.update(values, names, n);
            CustomBrightnessNP.apply();
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::toggleCover(bool open)
{
    if (!currentProtocol)
    {
        LOG_ERROR("No protocol handler available");
        return false;
    }

    std::string command = open ? currentProtocol->generateOpenCommand() : currentProtocol->generateCloseCommand();
    return sendCommand(command);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState WandererCoverV4ProEC::ParkCap()
{
    // Set park status to busy
    ParkCapSP.setState(IPS_BUSY);
    ParkCapSP.apply();
    
    if (toggleCover(false))
    {
        return IPS_BUSY;
    }

    // If toggleCover failed, set back to alert
    ParkCapSP.setState(IPS_ALERT);
    ParkCapSP.apply();
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState WandererCoverV4ProEC::UnParkCap()
{
    // Set park status to busy
    ParkCapSP.setState(IPS_BUSY);
    ParkCapSP.apply();
    
    if (toggleCover(true))
    {
        return IPS_BUSY;
    }

    // If toggleCover failed, set back to alert
    ParkCapSP.setState(IPS_ALERT);
    ParkCapSP.apply();
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::SetLightBoxBrightness(uint16_t value)
{
    if (!currentProtocol)
    {
        LOG_ERROR("No protocol handler available");
        return false;
    }

    auto rc = true;
    if(value > 0)
    {
        // Only change if already enabled.
        if (LightSP[INDI_ENABLED].getState() == ISS_ON)
            rc = sendCommand(currentProtocol->generateSetBrightnessCommand(value));
    }
    else
    {
        EnableLightBox(false);
        LightSP[INDI_ENABLED].setState(ISS_OFF);
        LightSP[INDI_DISABLED].setState(ISS_ON);
        LightSP.setState(IPS_IDLE);
        LightSP.apply();
    }

    return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::EnableLightBox(bool enable)
{
    if (!currentProtocol)
    {
        LOG_ERROR("No protocol handler available");
        return false;
    }

    auto rc = false;
    if(!enable)
        rc = sendCommand(currentProtocol->generateTurnOffLightCommand());
    else
        rc = sendCommand(currentProtocol->generateSetBrightnessCommand(static_cast<int>(LightIntensityNP[0].getValue())));

    return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::sendCommand(std::string command)
{
    // Lock the mutex for the duration of this function
    std::lock_guard<std::timed_mutex> lock(serialPortMutex);
    
    int nbytes_written = 0, rc = -1;
    std::string command_termination = "\n";
    LOGF_DEBUG("CMD: %s", command.c_str());
    if ((rc = tty_write_string(PortFD, (command + command_termination).c_str(), &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }
    
    return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::setClose(double value)
{
    if (!currentProtocol)
    {
        LOG_ERROR("No protocol handler available");
        return false;
    }

    return sendCommand(currentProtocol->generateSetClosePositionCommand(value));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::setOpen(double value)
{
    if (!currentProtocol)
    {
        LOG_ERROR("No protocol handler available");
        return false;
    }

    return sendCommand(currentProtocol->generateSetOpenPositionCommand(value));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererCoverV4ProEC::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getPollingPeriod());
        return;
    }

    getData();
    SetTimer(getPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4ProEC::saveConfigItems(FILE * fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    CloseSetNP.save(fp);
    OpenSetNP.save(fp);
    CustomBrightnessNP.save(fp);

    return LI::saveConfigItems(fp);
}
