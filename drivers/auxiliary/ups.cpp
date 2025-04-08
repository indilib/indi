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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "ups.h"
#include "indicom.h"

// We declare an auto pointer to UPS
std::unique_ptr<UPS> ups(new UPS());

UPS::UPS()
{
    setVersion(1, 0);
    setWeatherConnection(CONNECTION_NONE);
    socketFd = -1;
}

const char *UPS::getDefaultName()
{
    return "UPS";
}

bool UPS::initProperties()
{
    INDI::Weather::initProperties();

    // Setup server address properties
    ServerAddressTP[0].fill("HOST", "Host", "localhost");
    ServerAddressTP[1].fill("PORT", "Port", "3493");  // Default NUT port
    ServerAddressTP.fill(getDeviceName(), "SERVER_ADDRESS", "NUT Server", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    // Setup UPS name property
    UPSNameTP[0].fill("NAME", "UPS Name", "ups");  // Default UPS name
    UPSNameTP.fill(getDeviceName(), "UPS_NAME", "UPS", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    // Setup connection settings properties
    ConnectionSettingsNP[0].fill("TIMEOUT", "Timeout (sec)", "%.0f", 1, 30, 1, 5);
    ConnectionSettingsNP[1].fill("RETRIES", "Max Retries", "%.0f", 1, 10, 1, 3);
    ConnectionSettingsNP[2].fill("RETRY_DELAY", "Retry Delay (ms)", "%.0f", 100, 5000, 100, 1000);
    ConnectionSettingsNP[3].fill("RECONNECT_ATTEMPTS", "Max Reconnect Attempts", "%.0f", 0, 10, 1, 3);
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

    // Always define these properties
    defineProperty(ServerAddressTP);
    defineProperty(UPSNameTP);
    defineProperty(ConnectionSettingsNP);
}

bool UPS::Connect()
{
    if (ServerAddressTP[0].getText() == nullptr || ServerAddressTP[1].getText() == nullptr)
    {
        LOG_ERROR("Server address or port is not set.");
        return false;
    }

    // Ensure socket is closed before reconnecting
    if (socketFd >= 0)
    {
        close(socketFd);
        socketFd = -1;
    }

    // Create socket
    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0)
    {
        LOGF_ERROR("Failed to create socket: %s", strerror(errno));
        return false;
    }

    // Setup address structure
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(std::stoi(ServerAddressTP[1].getText()));

    // Convert hostname to IP
    struct hostent *he = gethostbyname(ServerAddressTP[0].getText());
    if (he == nullptr)
    {
        LOGF_ERROR("Failed to resolve hostname: %s", ServerAddressTP[0].getText());
        close(socketFd);
        socketFd = -1;
        return false;
    }
    memcpy(&serverAddr.sin_addr, he->h_addr_list[0], he->h_length);

    // Set timeout for connect
    struct timeval tv;
    tv.tv_sec = static_cast<int>(ConnectionSettingsNP[0].getValue());
    tv.tv_usec = 0;
    setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(socketFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // Enable keep-alive
    int keepalive = 1;
    setsockopt(socketFd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

    // Connect to server with retries
    int retries = ConnectionSettingsNP[1].getValue();
    int retryDelay = ConnectionSettingsNP[2].getValue();
    
    while (retries > 0)
    {
        if (connect(socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == 0)
        {
            // Test connection by querying UPS status
            if (queryUPSStatus())
            {
                LOG_INFO("Successfully connected to NUT server.");
                
                // Mettre à jour l'état immédiatement
                IPState state = updateWeather();
                if (state == IPS_OK || state == IPS_BUSY)
                {
                    LOG_INFO("UPS status successfully updated.");
                }
                else
                {
                    LOG_WARN("Connected to NUT server but failed to update UPS status.");
                }
                
                SetTimer(getCurrentPollingPeriod());
                return true;
            }
            else
            {
                LOG_ERROR("Failed to query UPS status");
            }
        }
        
        LOGF_WARN("Failed to connect to NUT server: %s. Retrying in %d ms...", strerror(errno), retryDelay);
        
        // Close socket before retrying
        close(socketFd);
        
        // Create new socket for retry
        socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd < 0)
        {
            LOGF_ERROR("Failed to create socket: %s", strerror(errno));
            return false;
        }
        
        // Reapply socket options
        setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(socketFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        setsockopt(socketFd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

        retries--;
        if (retries > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
    }

    if (socketFd >= 0)
    {
        close(socketFd);
        socketFd = -1;
    }
    
    LOG_ERROR("Failed to connect to NUT server after all retries");
    return false;
}

bool UPS::checkConnection()
{
    if (socketFd < 0)
        return false;

    // Try to read from socket to check if it's still alive
    char buffer[1];
    ssize_t result = recv(socketFd, buffer, 1, MSG_PEEK | MSG_DONTWAIT);
    
    if (result == 0)  // Connection closed by peer
    {
        LOG_WARN("Connection to NUT server was closed by peer");
        return false;
    }
    else if (result < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)  // Real error, not just no data
        {
            LOGF_WARN("Connection error: %s", strerror(errno));
            return false;
        }
    }

    return true;
}

bool UPS::Disconnect()
{
    if (socketFd >= 0)
    {
        close(socketFd);
        socketFd = -1;
    }
    LOG_INFO("Disconnected from NUT server.");
    return true;
}

bool UPS::updateProperties()
{
    INDI::Weather::updateProperties();
    
    if (isConnected())
    {
        // Si on vient de se connecter, on vérifie que le statut est à jour
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
    if (!checkConnection())
    {
        LOG_WARN("Connection lost, attempting to reconnect...");
        if (reconnectAttempts < ConnectionSettingsNP[3].getValue())
        {
            reconnectAttempts++;
            if (attemptReconnect())
            {
                LOG_INFO("Successfully reconnected to NUT server");
                reconnectAttempts = 0;
            }
            else
            {
                LOGF_WARN("Reconnection attempt %d failed", reconnectAttempts);
                return IPS_ALERT;
            }
        }
        else
        {
            LOG_ERROR("Maximum reconnection attempts reached");
            return IPS_ALERT;
        }
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

    // Get UPS variables
    std::string response = makeNUTRequest("LIST VAR " + std::string(UPSNameTP[0].getText()));
    if (response.empty())
    {
        LOG_ERROR("Failed to get UPS variables");
        return false;
    }

    return parseUPSResponse(response);
}

bool UPS::parseUPSResponse(const std::string& response)
{
    std::istringstream iss(response);
    std::string line;

    while (std::getline(iss, line))
    {
        // Parse NUT response format: VAR ups variablename "value"
        if (line.substr(0, 4) == "VAR ")
        {
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
                DEBUGF(INDI::Logger::DBG_DEBUG, "UPS Parameter: %s = %s", name.c_str(), value.c_str());
            }
        }
    }

    return !upsParameters.empty();
}

std::string UPS::makeNUTRequest(const std::string& command)
{
    int retries = ConnectionSettingsNP[1].getValue();
    int retryDelay = ConnectionSettingsNP[2].getValue();

    while (retries > 0)
    {
        // Check if socket is valid
        if (socketFd < 0)
        {
            if (!Connect())  // Try to reconnect
            {
                LOG_ERROR("Socket disconnected and reconnection failed");
                return "";
            }
        }

        try
        {
            // Send command with newline
            std::string request = command + "\n";
            ssize_t sent = send(socketFd, request.c_str(), request.length(), 0);
            if (sent != static_cast<ssize_t>(request.length()))
            {
                LOGF_ERROR("Failed to send complete command: %s", strerror(errno));
                // Socket might be broken, close it and try to reconnect
                close(socketFd);
                socketFd = -1;
                retries--;
                if (retries > 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
                    continue;
                }
                return "";
            }

            // Read response
            char buffer[4096];
            std::string response;
            ssize_t received;

            // Read until we get a complete response (ends with newline)
            while ((received = recv(socketFd, buffer, sizeof(buffer) - 1, 0)) > 0)
            {
                buffer[received] = '\0';
                response += buffer;
                if (response.find('\n') != std::string::npos)
                    break;
            }

            if (received < 0)
            {
                LOGF_ERROR("Failed to receive response: %s", strerror(errno));
                // Socket might be broken, close it and try to reconnect
                close(socketFd);
                socketFd = -1;
                retries--;
                if (retries > 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
                    continue;
                }
                return "";
            }

            if (!response.empty())
                return response;
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
        if (ServerAddressTP.isNameMatch(name))
        {
            ServerAddressTP.update(texts, names, n);
            ServerAddressTP.setState(IPS_OK);
            ServerAddressTP.apply();
            saveConfig();
            return true;
        }
        else if (UPSNameTP.isNameMatch(name))
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
    
    ServerAddressTP.save(fp);
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
        result &= ServerAddressTP.load();
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

bool UPS::attemptReconnect()
{
    if (socketFd >= 0)
    {
        close(socketFd);
        socketFd = -1;
    }

    // Wait before attempting to reconnect
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(ConnectionSettingsNP[2].getValue())));

    // Attempt to establish a new connection
    bool connected = Connect();
    
    // La méthode Connect() gère déjà l'appel à updateWeather()
    
    return connected;
} 