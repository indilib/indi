/*******************************************************************************
  Copyright(c) 2025 Jérémie Klein. All rights reserved.

  INDI UPS Driver using NUT (Network UPS Tools)
  This driver monitors a UPS through NUT and exposes battery status via SAFETY_STATUS.

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

#include <memory>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sstream>
#include <termios.h>
#include <fcntl.h>
#include "ups.h"
#include "indicom.h"

// We declare an auto pointer to UPS
std::unique_ptr<UPS> ups(new UPS());

UPS::UPS()
{
    setVersion(1, 0);
    
    // Create TCP Connection
    tcpConnection = new Connection::TCP(this);
    // Set default values for TCP connection
    tcpConnection->setDefaultHost("localhost");
    tcpConnection->setDefaultPort(3493); // Default NUT port
    // Register the handshake function
    tcpConnection->registerHandshake([&]() { return Handshake(); });
    
    // Register connection
    registerConnection(tcpConnection);
}

const char *UPS::getDefaultName()
{
    return "UPS";
}

bool UPS::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Setup UPS name property
    UPSNameTP[0].fill("NAME", "UPS Name", "ups");  // Default UPS name
    UPSNameTP.fill(getDeviceName(), "UPS_NAME", "UPS", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    // Setup connection settings properties
    ConnectionSettingsNP[0].fill("RETRIES", "Max Retries", "%.0f", 1, 10, 1, 3);
    ConnectionSettingsNP[1].fill("RETRY_DELAY", "Retry Delay (ms)", "%.0f", 100, 5000, 100, 1000);
    ConnectionSettingsNP.fill(getDeviceName(), "CONNECTION_SETTINGS", "Connection", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    // Setup update period
    UpdatePeriodNP[0].fill("PERIOD", "Period (s)", "%.1f", 1, 3600, 1, 10);
    UpdatePeriodNP.fill(getDeviceName(), "UPDATE_PERIOD", "Update", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    
    // UPS parameters
    UPSParametersNP[UPS_BATTERY_CHARGE].fill("BATTERY_CHARGE", "Battery Charge (%)", "%.1f", 0, 100, 0, 0);
    UPSParametersNP[UPS_BATTERY_VOLTAGE].fill("BATTERY_VOLTAGE", "Battery Voltage (V)", "%.2f", 0, 100, 0, 0);
    UPSParametersNP[UPS_INPUT_VOLTAGE].fill("INPUT_VOLTAGE", "Input Voltage (V)", "%.2f", 0, 300, 0, 0);
    UPSParametersNP.fill(getDeviceName(), "UPS_PARAMETERS", "Parameters", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    // Battery thresholds
    BatteryThresholdsNP[BATTERY_WARNING_THRESHOLD].fill("WARNING", "Warning Level (%)", "%.0f", 0, 100, 5, 25);
    BatteryThresholdsNP[BATTERY_CRITICAL_THRESHOLD].fill("CRITICAL", "Critical Level (%)", "%.0f", 0, 100, 5, 15);
    BatteryThresholdsNP.fill(getDeviceName(), "BATTERY_THRESHOLDS", "Battery Thresholds", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    
    // Safety Status property
    SafetyStatusLP[0].fill("SAFETY", "Safety", IPS_IDLE);
    SafetyStatusLP.fill(getDeviceName(), "SAFETY_STATUS", "Status", MAIN_CONTROL_TAB, IPS_IDLE);

    addDebugControl();
    setDriverInterface(AUX_INTERFACE);

    return true;
}

void UPS::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    // Define UPS name property
    defineProperty(UPSNameTP);
    defineProperty(ConnectionSettingsNP);
    defineProperty(BatteryThresholdsNP);
    
    loadConfig(true);
}

bool UPS::Handshake()
{
    LOG_INFO("Starting handshake with NUT server...");
    
    PortFD = tcpConnection->getPortFD();
    if (PortFD == -1)
    {
        LOG_ERROR("Invalid port file descriptor during handshake.");
        return false;
    }
    
    LOGF_DEBUG("Handshake: Using file descriptor %d", PortFD);
    
    // Check if socket is valid
    int socket_test = 0;
    if (fcntl(PortFD, F_GETFL, &socket_test) < 0) {
        LOGF_ERROR("Socket test failed: %s", strerror(errno));
        return false;
    }
    
    // Wait a bit for connection to be established
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test communication with simple command
    std::string response = makeNUTRequest("VER");
    if (response.empty())
    {
        LOG_ERROR("Handshake failed: NUT server did not respond to VER command");
        return false;
    }
    
    LOGF_DEBUG("Handshake: VER command response: %s", response.c_str());
    
    // List available UPS
    response = makeNUTRequest("LIST UPS");
    if (response.empty())
    {
        LOG_ERROR("Handshake failed: NUT server did not respond to LIST UPS command");
        return false;
    }
    
    LOGF_DEBUG("Handshake: LIST UPS command response: %s", response.c_str());
    
    // Try to query the configured UPS
    if (!queryUPSStatus())
    {
        LOG_ERROR("Handshake failed: could not query UPS status for the configured UPS name");
        return false;
    }
    
    LOG_INFO("Handshake successful, connected to NUT server");
    return true;
}

bool UPS::Connect()
{
    if (!INDI::DefaultDevice::Connect())
        return false;
    
    // Start timer for periodic updates
    m_TimerID = SetTimer(static_cast<uint32_t>(UpdatePeriodNP[0].getValue() * 1000));
    
    return true;
}

bool UPS::Disconnect()
{
    if (m_TimerID > 0)
    {
        RemoveTimer(m_TimerID);
        m_TimerID = -1;
    }
    
    LOG_INFO("Disconnected from NUT server.");
    return INDI::DefaultDevice::Disconnect();
}

bool UPS::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    
    if (isConnected())
    {
        defineProperty(UpdatePeriodNP);
        defineProperty(UPSParametersNP);
        defineProperty(SafetyStatusLP);
        
        // Initial update
        updateUPSStatus();
    }
    else
    {
        deleteProperty(UpdatePeriodNP);
        deleteProperty(UPSParametersNP);
        deleteProperty(SafetyStatusLP);
    }
    
    return true;
}

void UPS::TimerHit()
{
    if (!isConnected())
        return;
    
    updateUPSStatus();
    
    // Re-arm timer
    m_TimerID = SetTimer(static_cast<uint32_t>(UpdatePeriodNP[0].getValue() * 1000));
}

void UPS::updateUPSStatus()
{
    // Get the file descriptor from the TCP connection
    PortFD = tcpConnection->getPortFD();
    
    if (PortFD == -1)
    {
        LOG_ERROR("Connection lost, invalid file descriptor");
        SafetyStatusLP.setState(IPS_ALERT);
        SafetyStatusLP.apply();
        return;
    }

    if (!queryUPSStatus())
    {
        LastParseSuccess = false;
        SafetyStatusLP.setState(IPS_ALERT);
        SafetyStatusLP.apply();
        return;
    }

    // Update UPS parameters
    try 
    {
        double batteryCharge = 0;
        bool hasCharge = false;
        
        // Battery charge
        if (upsParameters.count("battery.charge"))
        {
            batteryCharge = std::stod(upsParameters["battery.charge"]);
            UPSParametersNP[UPS_BATTERY_CHARGE].setValue(batteryCharge);
            hasCharge = true;
        }

        // Battery voltage
        if (upsParameters.count("battery.voltage"))
        {
            double voltage = std::stod(upsParameters["battery.voltage"]);
            UPSParametersNP[UPS_BATTERY_VOLTAGE].setValue(voltage);
        }

        // Input voltage
        if (upsParameters.count("input.voltage"))
        {
            double voltage = std::stod(upsParameters["input.voltage"]);
            UPSParametersNP[UPS_INPUT_VOLTAGE].setValue(voltage);
        }
        
        UPSParametersNP.setState(IPS_OK);
        UPSParametersNP.apply();

        // Determine safety status based on battery charge
        IPState safetyState = IPS_IDLE;
        
        if (hasCharge)
        {
            double criticalThreshold = BatteryThresholdsNP[BATTERY_CRITICAL_THRESHOLD].getValue();
            double warningThreshold = BatteryThresholdsNP[BATTERY_WARNING_THRESHOLD].getValue();
            
            if (batteryCharge <= criticalThreshold)
            {
                safetyState = IPS_ALERT;
                LOGF_WARN("Battery critically low: %.1f%% (<= %.0f%%)", batteryCharge, criticalThreshold);
            }
            else if (batteryCharge <= warningThreshold)
            {
                safetyState = IPS_BUSY;
                LOGF_WARN("Battery low: %.1f%% (<= %.0f%%)", batteryCharge, warningThreshold);
            }
            else
            {
                safetyState = IPS_OK;
                LOGF_DEBUG("Battery normal: %.1f%%", batteryCharge);
            }
        }
        
        SafetyStatusLP.setState(safetyState);
        SafetyStatusLP.apply();

        LastParseSuccess = true;
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Error parsing UPS status: %s", e.what());
        LastParseSuccess = false;
        SafetyStatusLP.setState(IPS_ALERT);
        SafetyStatusLP.apply();
    }
}

bool UPS::queryUPSStatus()
{
    // Clear previous parameters
    upsParameters.clear();

    LOGF_DEBUG("Querying UPS status for '%s'", UPSNameTP[0].getText());
    
    // Get UPS variables
    std::string response = makeNUTRequest("LIST VAR " + std::string(UPSNameTP[0].getText()));
    if (response.empty())
    {
        LOG_ERROR("Failed to get UPS variables");
        
        // Try a LIST UPS command to verify if the UPS exists
        std::string upsListResponse = makeNUTRequest("LIST UPS");
        if (!upsListResponse.empty()) {
            LOGF_DEBUG("Available UPS units: %s", upsListResponse.c_str());
        }
        
        return false;
    }

    LOGF_DEBUG("Response from LIST VAR: %s", response.c_str());
    return parseUPSResponse(response);
}

bool UPS::parseUPSResponse(const std::string& response)
{
    std::istringstream iss(response);
    std::string line;
    int lineCount = 0;

    LOGF_DEBUG("Parsing response of %zu bytes", response.length());

    // Check for error first
    if (response.find("ERR ") != std::string::npos) {
        size_t pos = response.find("ERR ");
        size_t end = response.find('\n', pos);
        std::string errorMsg = (end != std::string::npos) ? 
                              response.substr(pos, end - pos) : 
                              response.substr(pos);
                              
        LOGF_ERROR("NUT server returned error: %s", errorMsg.c_str());
        return false;
    }

    while (std::getline(iss, line))
    {
        lineCount++;
        // Ignore empty lines or end markers
        if (line.empty() || line == "END") {
            continue;
        }
        
        // Parse NUT response format: VAR ups variablename "value"
        if (line.substr(0, 4) == "VAR ")
        {
            LOGF_DEBUG("Processing line: %s", line.c_str());
            
            std::istringstream lineStream(line);
            std::string var, ups, name;
            lineStream >> var >> ups >> name;
            
            // Extract value between quotes
            size_t firstQuote = line.find('"');
            size_t lastQuote = line.rfind('"');
            if (firstQuote != std::string::npos && lastQuote != std::string::npos && firstQuote < lastQuote)
            {
                std::string value = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                upsParameters[name] = value;
                LOGF_DEBUG("UPS Parameter: %s = %s", name.c_str(), value.c_str());
            }
            else {
                LOGF_DEBUG("Could not extract quoted value from line: %s", line.c_str());
            }
        }
        else {
            LOGF_DEBUG("Ignoring non-VAR line: %s", line.c_str());
        }
    }

    LOGF_DEBUG("Parsed %d lines, found %zu parameters", lineCount, upsParameters.size());
    return !upsParameters.empty();
}

std::string UPS::makeNUTRequest(const std::string& command)
{
    int retries = static_cast<int>(ConnectionSettingsNP[0].getValue());
    int retryDelay = static_cast<int>(ConnectionSettingsNP[1].getValue());
    
    LOGF_DEBUG("NUT Command: %s", command.c_str());

    while (retries > 0)
    {
        // Check if port is valid
        if (PortFD == -1)
        {
            LOG_ERROR("Invalid port file descriptor");
            return "";
        }

        try
        {
            // Send command with newline
            std::string request = command + "\n";
            int nbytes_written = 0;
            
            // Flush any lingering data before sending new command
            tcflush(PortFD, TCIOFLUSH);
            
            int rc = tty_write_string(PortFD, request.c_str(), &nbytes_written);
            
            if (rc != TTY_OK)
            {
                char errorMsg[MAXRBUF];
                tty_error_msg(rc, errorMsg, MAXRBUF);
                LOGF_ERROR("Error sending command: %s", errorMsg);
                retries--;
                if (retries > 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
                    continue;
                }
                return "";
            }
            
            LOGF_DEBUG("Sent %d bytes to NUT server", nbytes_written);

            // Collect multiline response
            std::string fullResponse;
            char buffer[4096] = {0};
            
            int readAttempts = 3;
            int timeout = 2; // seconds
            
            // Read data until timeout or receiving "END"
            while(readAttempts > 0) {
                int nbytes_read = 0;
                memset(buffer, 0, sizeof(buffer));
                
                // Read one line at a time
                rc = tty_read_section(PortFD, buffer, '\n', timeout, &nbytes_read);
                
                if (rc == TTY_OK && nbytes_read > 0) {
                    buffer[nbytes_read] = '\0';
                    LOGF_DEBUG("Read line: %s", buffer);
                    
                    fullResponse += buffer;
                    
                    // If line contains "END" or "ERR", it's the end of NUT command response
                    if (strstr(buffer, "END") != nullptr || strstr(buffer, "ERR") != nullptr) {
                        break;
                    }
                    
                    continue;
                }
                else if (rc == TTY_TIME_OUT) {
                    // If we already have data and no more to read, we're done
                    if (!fullResponse.empty()) {
                        LOGF_DEBUG("Read timeout after receiving %zu bytes total", fullResponse.length());
                        break;
                    }
                }
                else {
                    char errorMsg[MAXRBUF];
                    tty_error_msg(rc, errorMsg, MAXRBUF);
                    LOGF_ERROR("Error reading response: %s (error code: %d)", errorMsg, rc);
                    readAttempts--;
                    continue;
                }
                
                readAttempts--;
            }
            
            if (fullResponse.empty()) {
                LOGF_WARN("No response received from NUT server for command: %s", command.c_str());
                retries--;
                if (retries > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
                    continue;
                }
                return "";
            }
            
            LOGF_DEBUG("Received full response (%zu bytes): %s", fullResponse.length(), fullResponse.c_str());
            return fullResponse;
        }
        catch (const std::exception& e)
        {
            LOGF_ERROR("Request error: %s", e.what());
        }

        retries--;
        if (retries > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
        }
    }

    return "";
}

bool UPS::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (isDeviceNameMatch(dev))
    {
        if (UPSNameTP.isNameMatch(name))
        {
            UPSNameTP.update(texts, names, n);
            UPSNameTP.setState(IPS_OK);
            UPSNameTP.apply();
            saveConfig();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool UPS::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (isDeviceNameMatch(dev))
    {
        if (ConnectionSettingsNP.isNameMatch(name))
        {
            ConnectionSettingsNP.update(values, names, n);
            ConnectionSettingsNP.setState(IPS_OK);
            ConnectionSettingsNP.apply();
            saveConfig();
            return true;
        }
        else if (UpdatePeriodNP.isNameMatch(name))
        {
            UpdatePeriodNP.update(values, names, n);
            UpdatePeriodNP.setState(IPS_OK);
            UpdatePeriodNP.apply();
            
            // Restart timer with new period
            if (m_TimerID > 0)
            {
                RemoveTimer(m_TimerID);
                m_TimerID = SetTimer(static_cast<uint32_t>(UpdatePeriodNP[0].getValue() * 1000));
            }
            
            saveConfig();
            return true;
        }
        else if (BatteryThresholdsNP.isNameMatch(name))
        {
            BatteryThresholdsNP.update(values, names, n);
            BatteryThresholdsNP.setState(IPS_OK);
            BatteryThresholdsNP.apply();
            
            // Re-evaluate safety status with new thresholds
            if (isConnected())
                updateUPSStatus();
            
            saveConfig();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool UPS::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    
    UPSNameTP.save(fp);
    ConnectionSettingsNP.save(fp);
    UpdatePeriodNP.save(fp);
    BatteryThresholdsNP.save(fp);
    
    return true;
}

bool UPS::loadConfig(bool silent, const char *property)
{
    bool result = INDI::DefaultDevice::loadConfig(silent, property);

    if (property == nullptr)
    {
        UPSNameTP.load();
        ConnectionSettingsNP.load();
        UpdatePeriodNP.load();
        BatteryThresholdsNP.load();
    }
    
    return result;
}
