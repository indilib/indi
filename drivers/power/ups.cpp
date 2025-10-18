/*******************************************************************************
  Copyright(c) 2025 Jérémie Klein. All rights reserved.

  INDI UPS Driver using NUT (Network UPS Tools)
  This driver monitors a UPS through NUT and exposes its status through INDI's weather interface.
  Battery level and power status are mapped to weather parameters for compatibility.

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
    
    // Setup for TCP Connection
    setWeatherConnection(CONNECTION_TCP);
    
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
    INDI::Weather::initProperties();

    // Setup UPS name property
    UPSNameTP[0].fill("NAME", "UPS Name", "ups");  // Default UPS name
    UPSNameTP.fill(getDeviceName(), "UPS_NAME", "UPS", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    // Setup connection settings properties
    ConnectionSettingsNP[0].fill("RETRIES", "Max Retries", "%.0f", 1, 10, 1, 3);
    ConnectionSettingsNP[1].fill("RETRY_DELAY", "Retry Delay (ms)", "%.0f", 100, 5000, 100, 1000);
    ConnectionSettingsNP.fill(getDeviceName(), "CONNECTION_SETTINGS", "Connection", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    // Setup update period
    UpdatePeriodNP[0].fill("PERIOD", "Period (s)", "%.1f", 1, 3600, 1, 10);
    UpdatePeriodNP.fill(getDeviceName(), "UPDATE_PERIOD", "Update", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    // Add UPS parameters as weather parameters
    addParameter("BATTERY_CHARGE", "Battery Charge", 10, 100, 0);
    addParameter("BATTERY_VOLTAGE", "Battery Voltage", 12, 14, 0);
    addParameter("INPUT_VOLTAGE", "Input Voltage", 210, 240, 0);

    // Set critical parameters
    setCriticalParameter("BATTERY_CHARGE");

    // Load config before setting any defaults
    loadConfig(true);

    addDebugControl();

    return true;
}

void UPS::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);

    // Define UPS name property
    defineProperty(UPSNameTP);
    defineProperty(ConnectionSettingsNP);
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
    
    // Vérifier si le socket est valide avec un test simple
    int socket_test = 0;
    if (fcntl(PortFD, F_GETFL, &socket_test) < 0) {
        LOGF_ERROR("Socket test failed: %s", strerror(errno));
        return false;
    }
    
    // Attendre un peu pour que la connexion soit bien établie
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Attempt a simple command first to test communication
    std::string response = makeNUTRequest("VER");
    if (response.empty())
    {
        LOG_ERROR("Handshake failed: NUT server did not respond to VER command");
        return false;
    }
    
    LOGF_DEBUG("Handshake: VER command response: %s", response.c_str());
    
    // Liste des UPS disponibles pour vérification
    response = makeNUTRequest("LIST UPS");
    if (response.empty())
    {
        LOG_ERROR("Handshake failed: NUT server did not respond to LIST UPS command");
        return false;
    }
    
    LOGF_DEBUG("Handshake: LIST UPS command response: %s", response.c_str());
    
    // Try to communicate with the NUT server using the configured UPS
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
    if (!INDI::Weather::Connect())
        return false;
    
    // Connection is now managed by the tcpConnection object
    // After successful connection, set timer for polling
    SetTimer(UpdatePeriodNP[0].getValue() * 1000);
    
    return true;
}

bool UPS::Disconnect()
{
    // The tcpConnection object will handle closing the socket
    LOG_INFO("Disconnected from NUT server.");
    return INDI::Weather::Disconnect();
}

bool UPS::updateProperties()
{
    INDI::Weather::updateProperties();
    
    if (isConnected())
    {
        // If we've just connected, ensure status is up to date
        if (LastParseSuccess == false)
        {
            LOG_INFO("Initial connection established, updating UPS status...");
            updateWeather();
        }
    }
    
    return true;
}

IPState UPS::updateWeather()
{
    // Get the file descriptor from the TCP connection
    PortFD = tcpConnection->getPortFD();
    
    if (PortFD == -1)
    {
        LOG_ERROR("Connection lost, invalid file descriptor");
        return IPS_ALERT;
    }

    if (!queryUPSStatus())
    {
        LastParseSuccess = false;
        return IPS_ALERT;
    }

    // Update weather parameters based on UPS status
    try 
    {
        // Battery charge
        if (upsParameters.count("battery.charge"))
        {
            double charge = std::stod(upsParameters["battery.charge"]);
            setParameterValue("BATTERY_CHARGE", charge);
        }

        // Battery voltage
        if (upsParameters.count("battery.voltage"))
        {
            double voltage = std::stod(upsParameters["battery.voltage"]);
            setParameterValue("BATTERY_VOLTAGE", voltage);
        }

        // Input voltage
        if (upsParameters.count("input.voltage"))
        {
            double voltage = std::stod(upsParameters["input.voltage"]);
            setParameterValue("INPUT_VOLTAGE", voltage);
        }

        LastParseSuccess = true;
        return IPS_OK;
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Error parsing UPS status: %s", e.what());
        LastParseSuccess = false;
        return IPS_ALERT;
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

    LOGF_DEBUG("Parsing response of %d bytes", response.length());

    // Vérifions d'abord si la réponse contient une erreur
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
        // Ignorer les lignes vides ou les fins de message
        if (line.empty() || line == "END") {
            continue;
        }
        
        // Parse NUT response format: VAR ups variablename "value"
        if (line.substr(0, 4) == "VAR ")
        {
            LOGF_DEBUG("Processing line: %s", line.c_str());
            
            std::istringstream lineStream(line);
            std::string var, ups, name, value;
            lineStream >> var >> ups >> name;
            
            // Extract value between quotes
            size_t firstQuote = line.find('"');
            size_t lastQuote = line.rfind('"');
            if (firstQuote != std::string::npos && lastQuote != std::string::npos && firstQuote < lastQuote)
            {
                value = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
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

    LOGF_DEBUG("Parsed %d lines, found %d parameters", lineCount, upsParameters.size());
    return !upsParameters.empty();
}

std::string UPS::makeNUTRequest(const std::string& command)
{
    int retries = ConnectionSettingsNP[0].getValue();
    int retryDelay = ConnectionSettingsNP[1].getValue();
    
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

            // Pour collecter une réponse multiligne complète
            std::string fullResponse;
            char buffer[4096] = {0};
            
            // Contrôle du timeout et des tentatives de lecture
            int readAttempts = 3;
            int timeout = 2; // secondes
            
            // Lire les données jusqu'à timeout ou réception d'un "END" spécifique au protocole NUT
            while(readAttempts > 0) {
                int nbytes_read = 0;
                memset(buffer, 0, sizeof(buffer));
                
                // Lire une ligne à la fois
                rc = tty_read_section(PortFD, buffer, '\n', timeout, &nbytes_read);
                
                if (rc == TTY_OK && nbytes_read > 0) {
                    buffer[nbytes_read] = '\0';
                    LOGF_DEBUG("Read line: %s", buffer);
                    
                    fullResponse += buffer;
                    
                    // Si la ligne contient "END" ou "ERR", c'est généralement la fin d'une commande NUT
                    if (strstr(buffer, "END") != nullptr || strstr(buffer, "ERR") != nullptr) {
                        break;
                    }
                    
                    // Continuer à lire plus de données
                    continue;
                }
                else if (rc == TTY_TIME_OUT) {
                    // Si nous avons déjà des données mais plus rien à lire, on a probablement tout
                    if (!fullResponse.empty()) {
                        LOGF_DEBUG("Read timeout after receiving %d bytes total", fullResponse.length());
                        break;
                    }
                }
                else {
                    // Erreur de lecture
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
            
            LOGF_DEBUG("Received full response (%d bytes): %s", fullResponse.length(), fullResponse.c_str());
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

    return INDI::Weather::ISNewText(dev, name, texts, names, n);
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
            SetTimer(UpdatePeriodNP[0].getValue() * 1000); // Convert to milliseconds
            saveConfig();
            return true;
        }
    }

    return INDI::Weather::ISNewNumber(dev, name, values, names, n);
}

bool UPS::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);
    
    UPSNameTP.save(fp);
    ConnectionSettingsNP.save(fp);
    UpdatePeriodNP.save(fp);
    
    return true;
}

bool UPS::loadConfig(bool silent, const char *property)
{
    bool result = INDI::Weather::loadConfig(silent, property);

    if (property == nullptr)
    {
        result &= UPSNameTP.load();
        result &= ConnectionSettingsNP.load();
        result &= UpdatePeriodNP.load();
        if (result)
            SetTimer(UpdatePeriodNP[0].getValue() * 1000); // Convert to milliseconds
    }
    
    return result;
}

IPState UPS::checkParameterState(const std::string &name) const
{
    // Use the parent class implementation which already handles thresholds correctly
    return INDI::Weather::checkParameterState(name);
} 