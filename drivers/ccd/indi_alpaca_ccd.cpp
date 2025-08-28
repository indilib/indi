/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  ASCOM Alpaca Camera INDI Driver

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

#include "indi_alpaca_ccd.h"

#include "indicom.h"
#include <httplib.h>
#include <string.h>
#include <chrono>
#include <thread>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

// We declare an auto pointer to AlpacaCCD
std::unique_ptr<AlpacaCCD> alpaca_ccd(new AlpacaCCD());

AlpacaCCD::AlpacaCCD()
{
    // Set initial CCD capabilities based on ASCOM Alpaca Camera API
    SetCCDCapability(INDI::CCD::CCD_CAN_ABORT | INDI::CCD::CCD_CAN_BIN | INDI::CCD::CCD_CAN_SUBFRAME |
                     INDI::CCD::CCD_HAS_COOLER | INDI::CCD::CCD_HAS_SHUTTER);
}

bool AlpacaCCD::initProperties()
{
    // Call base class initProperties
    INDI::CCD::initProperties();

    // Cooler properties
    CoolerSP[INDI_ENABLED].fill("COOLER_ON", "ON", ISS_OFF);
    CoolerSP[INDI_DISABLED].fill("COOLER_OFF", "OFF", ISS_ON);
    CoolerSP.fill(getDeviceName(), "CCD_COOLER", "Cooler", CAMERA_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    // Setup server address properties
    ServerAddressTP[0].fill("HOST", "Host", "");  // Empty default to force configuration
    ServerAddressTP[1].fill("PORT", "Port", "");
    ServerAddressTP.fill(getDeviceName(), "SERVER_ADDRESS", "Server", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);
    ServerAddressTP.load();

    // Setup device number property
    DeviceNumberNP[0].fill("DEVICE_NUMBER", "Device Number", "%.0f", 0, 10, 1, 0);
    DeviceNumberNP.fill(getDeviceName(), "DEVICE_NUMBER", "Alpaca Device", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);
    DeviceNumberNP.load();

    // Setup connection settings properties
    ConnectionSettingsNP[0].fill("TIMEOUT", "Timeout (sec)", "%.0f", 1, 30, 1, 5);
    ConnectionSettingsNP[1].fill("RETRIES", "Max Retries", "%.0f", 1, 10, 1, 3);
    ConnectionSettingsNP[2].fill("RETRY_DELAY", "Retry Delay (ms)", "%.0f", 100, 5000, 100, 1000);
    ConnectionSettingsNP.fill(getDeviceName(), "CONNECTION_SETTINGS", "Connection", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);
    ConnectionSettingsNP.load();

    // Gain property
    GainNP[0].fill("GAIN", "Gain", "%.0f", 0, 1000, 1, 0); // Max gain can vary, set a reasonable default
    GainNP.fill(getDeviceName(), "CCD_GAIN", "Gain", CAMERA_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Offset property
    OffsetNP[0].fill("OFFSET", "Offset", "%.0f", 0, 10000, 1, 0); // Max offset can vary
    OffsetNP.fill(getDeviceName(), "CCD_OFFSET", "Offset", CAMERA_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Initialize CCDChip parameters (these will be updated from Alpaca API)
    SetCCDParams(1, 1, 16, 1.0, 1.0); // Dummy values, will be updated on connect

    addAuxControls();

    return true;
}

void AlpacaCCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);

    // Always define these properties
    defineProperty(ServerAddressTP);
    defineProperty(DeviceNumberNP);
    defineProperty(ConnectionSettingsNP);
    defineProperty(GainNP);
    defineProperty(OffsetNP);
}

bool AlpacaCCD::Connect()
{
    if (ServerAddressTP[0].getText() == nullptr || ServerAddressTP[1].getText() == nullptr)
    {
        LOG_ERROR("Server address or port is not set.");
        return false;
    }

    // Test connection by getting camera status
    nlohmann::json response;
    bool success = retryRequest([this, &response]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/connected";
        return makeAlpacaRequest(path, response);
    });

    if (!success)
    {
        LOG_ERROR("Failed to connect to Alpaca camera. Please check server address and port.");
        return false;
    }

    // Set connected state
    success = retryRequest([this, &response]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/connected";
        nlohmann::json body = {{"Connected", true}};
        return makeAlpacaRequest(path, response, true, &body);
    });

    if (!success)
    {
        LOG_ERROR("Failed to set connected state on Alpaca camera.");
        return false;
    }

    LOG_INFO("Successfully connected to Alpaca camera.");

    // Update initial camera parameters
    updateProperties();
    updateReadoutModes();

    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool AlpacaCCD::Disconnect()
{
    if (isConnected())
    {
        nlohmann::json response;
        bool success = retryRequest([this, &response]()
        {
            std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/connected";
            nlohmann::json body = {{"Connected", false}};
            return makeAlpacaRequest(path, response, true, &body);
        });

        if (!success)
        {
            LOG_ERROR("Failed to set disconnected state on Alpaca camera.");
            return false;
        }
    }

    LOG_INFO("Disconnected from Alpaca camera.");
    return true;
}

const char *AlpacaCCD::getDefaultName()
{
    return "Alpaca CCD";
}

bool AlpacaCCD::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (isDeviceNameMatch(dev) && ServerAddressTP.isNameMatch(name))
    {
        ServerAddressTP.update(texts, names, n);
        ServerAddressTP.setState(IPS_OK);
        ServerAddressTP.apply();
        // Save configuration after update
        saveConfig();
        return true;
    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

bool AlpacaCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (isDeviceNameMatch(dev))
    {
        if (ConnectionSettingsNP.isNameMatch(name))
        {
            ConnectionSettingsNP.update(values, names, n);
            ConnectionSettingsNP.setState(IPS_OK);
            ConnectionSettingsNP.apply();
            saveConfig();
            LOG_INFO("Connection settings updated.");
            return true;
        }
        else if (DeviceNumberNP.isNameMatch(name))
        {
            if (isConnected())
            {
                LOG_WARN("Cannot change device number while connected.");
                return false;
            }

            DeviceNumberNP.update(values, names, n);
            DeviceNumberNP.setState(IPS_OK);
            DeviceNumberNP.apply();
            saveConfig();
            LOG_INFO("Alpaca device number updated.");
            return true;
        }
        else if (GainNP.isNameMatch(name))
        {
            nlohmann::json response;
            bool success = retryRequest([this, &response, values]()
            {
                std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/gain";
                nlohmann::json body = {{"Gain", values[0]}};
                return makeAlpacaRequest(path, response, true, &body);
            });

            if (success)
            {
                GainNP.update(values, names, n);
                GainNP.setState(IPS_OK);
                GainNP.apply();
                saveConfig();
                LOG_INFO("Gain updated.");
                return true;
            }
            LOG_ERROR("Failed to set gain.");
            return false;
        }
        else if (OffsetNP.isNameMatch(name))
        {
            nlohmann::json response;
            bool success = retryRequest([this, &response, values]()
            {
                std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/offset";
                nlohmann::json body = {{"Offset", values[0]}};
                return makeAlpacaRequest(path, response, true, &body);
            });

            if (success)
            {
                OffsetNP.update(values, names, n);
                OffsetNP.setState(IPS_OK);
                OffsetNP.apply();
                saveConfig();
                LOG_INFO("Offset updated.");
                return true;
            }
            LOG_ERROR("Failed to set offset.");
            return false;
        }
        else if (TemperatureNP.isNameMatch(name))
        {
            return SetTemperature(values[0]) != -1;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool AlpacaCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (isDeviceNameMatch(dev))
    {
        if (CoolerSP.isNameMatch(name))
        {
            CoolerSP.update(states, names, n);
            bool coolerOn = CoolerSP[0].getState() == ISS_ON; // Assuming CoolerOn is the first switch

            nlohmann::json response;
            bool success = retryRequest([this, &response, coolerOn]()
            {
                std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/cooleron";
                nlohmann::json body = {{"CoolerOn", coolerOn}};
                return makeAlpacaRequest(path, response, true, &body);
            });

            if (success)
            {
                CoolerSP.setState(IPS_OK);
                CoolerSP.apply();
                LOGF_INFO("Cooler set to %s.", coolerOn ? "ON" : "OFF");
                return true;
            }
            LOG_ERROR("Failed to set cooler state.");
            return false;
        }
        else if (CaptureFormatSP.isNameMatch(name))
        {
            int index = CaptureFormatSP.findOnSwitchIndex();
            if (index != -1)
            {
                return SetCaptureFormat(static_cast<uint8_t>(index));
            }
        }
    }
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool AlpacaCCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        LOG_INFO("Alpaca camera is ready for operation.");

        nlohmann::json response;
        bool success;

        // Get camera capabilities and update INDI properties
        int cameraXSize = 1, cameraYSize = 1;
        double pixelSizeX = 1.0, pixelSizeY = 1.0;

        success = retryRequest([this, &response]()
        {
            std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/cameraxsize";
            return makeAlpacaRequest(path, response);
        });
        if (success) cameraXSize = response["Value"].get<int>();

        success = retryRequest([this, &response]()
        {
            std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/cameraysize";
            return makeAlpacaRequest(path, response);
        });
        if (success) cameraYSize = response["Value"].get<int>();

        success = retryRequest([this, &response]()
        {
            std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/pixelsizex";
            return makeAlpacaRequest(path, response);
        });
        if (success) pixelSizeX = response["Value"].get<double>();

        success = retryRequest([this, &response]()
        {
            std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/pixelsizey";
            return makeAlpacaRequest(path, response);
        });
        if (success) pixelSizeY = response["Value"].get<double>();

        // Update CCDParams
        SetCCDParams(cameraXSize, cameraYSize, PrimaryCCD.getBPP(), pixelSizeX, pixelSizeY);

        // Get current gain
        success = retryRequest([this, &response]()
        {
            std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/gain";
            return makeAlpacaRequest(path, response);
        });
        if (success)
        {
            GainNP[0].setValue(response["Value"].get<double>());
            GainNP.setState(IPS_OK);
            GainNP.apply();
        }

        // Get current offset
        success = retryRequest([this, &response]()
        {
            std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/offset";
            return makeAlpacaRequest(path, response);
        });
        if (success)
        {
            OffsetNP[0].setValue(response["Value"].get<double>());
            OffsetNP.setState(IPS_OK);
            OffsetNP.apply();
        }

        // Cooler properties
        defineProperty(CoolerSP);
        defineProperty(TemperatureNP);
        updateCoolerStatus();

    }
    else
    {
        LOG_INFO("Alpaca camera is disconnected.");
        deleteProperty(CoolerSP);
        deleteProperty(TemperatureNP);
        deleteProperty(GainNP);
        deleteProperty(OffsetNP);
    }

    return true;
}

void AlpacaCCD::TimerHit()
{
    if (!isConnected())
        return;

    updateStatus();
    SetTimer(getCurrentPollingPeriod());
}

bool AlpacaCCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    ServerAddressTP.save(fp);
    DeviceNumberNP.save(fp);
    ConnectionSettingsNP.save(fp);
    GainNP.save(fp);
    OffsetNP.save(fp);

    return true;
}

bool AlpacaCCD::loadConfig(bool silent, const char *property)
{
    bool result = INDI::CCD::loadConfig(silent, property);

    if (property == nullptr)
    {
        result &= ServerAddressTP.load();
        result &= DeviceNumberNP.load();
        result &= ConnectionSettingsNP.load();
        result &= GainNP.load();
        result &= OffsetNP.load();
    }

    return result;
}

bool AlpacaCCD::StartExposure(float duration)
{
    if (!isConnected())
    {
        LOG_ERROR("Not connected to Alpaca camera.");
        return false;
    }

    m_ExposureDuration = duration;
    m_ExposureInProgress = true;

    nlohmann::json response;
    bool success = retryRequest([this, &response, duration]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/startexposure";
        nlohmann::json body =
        {
            {"Duration", duration},
            {"Light", PrimaryCCD.getFrameType() == INDI::CCDChip::LIGHT_FRAME}
        };
        return makeAlpacaRequest(path, response, true, &body);
    });

    if (success)
    {
        LOGF_INFO("Exposure started for %.2f seconds.", duration);
        PrimaryCCD.setExposureDuration(duration);
        PrimaryCCD.setExposureLeft(duration);
        return true;
    }

    m_ExposureInProgress = false;
    LOG_ERROR("Failed to start exposure.");
    return false;
}

bool AlpacaCCD::AbortExposure()
{
    if (!isConnected())
    {
        LOG_ERROR("Not connected to Alpaca camera.");
        return false;
    }

    if (!m_ExposureInProgress)
    {
        LOG_INFO("No exposure in progress to abort.");
        return true;
    }

    nlohmann::json response;
    bool success = retryRequest([this, &response]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/abortexposure";
        return makeAlpacaRequest(path, response, true);
    });

    if (success)
    {
        LOG_INFO("Exposure aborted.");
        m_ExposureInProgress = false;
        PrimaryCCD.setExposureLeft(0);
        return true;
    }

    LOG_ERROR("Failed to abort exposure.");
    return false;
}

int AlpacaCCD::SetTemperature(double temperature)
{
    if (!isConnected())
    {
        LOG_ERROR("Not connected to Alpaca camera.");
        return -1;
    }

    m_TargetTemperature = temperature;

    nlohmann::json response;
    bool success = retryRequest([this, &response, temperature]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) +
                           "/setccdtemperature";
        nlohmann::json body = {{"SetCCDTemperature", temperature}};
        return makeAlpacaRequest(path, response, true, &body);
    });

    if (success)
    {
        LOGF_INFO("Target temperature set to %.2f C.", temperature);
        TemperatureNP[0].setValue(temperature); // Update INDI property immediately
        TemperatureNP.setState(IPS_BUSY);
        TemperatureNP.apply();
        return 0; // Indicate that setting temperature takes time
    }

    LOG_ERROR("Failed to set target temperature.");
    return -1;
}

bool AlpacaCCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    if (!isConnected())
    {
        LOG_ERROR("Not connected to Alpaca camera.");
        return false;
    }

    nlohmann::json response;
    bool success = retryRequest([this, &response, x, y, w, h]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/startx";
        nlohmann::json body = {{"StartX", x}};
        if (!makeAlpacaRequest(path, response, true, &body)) return false;

        path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/starty";
        body = {{"StartY", y}};
        if (!makeAlpacaRequest(path, response, true, &body)) return false;

        path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/numx";
        body = {{"NumX", w}};
        if (!makeAlpacaRequest(path, response, true, &body)) return false;

        path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/numy";
        body = {{"NumY", h}};
        return makeAlpacaRequest(path, response, true, &body);
    });

    if (success)
    {
        PrimaryCCD.setX(x);
        PrimaryCCD.setY(y);
        PrimaryCCD.setW(w);
        PrimaryCCD.setH(h);
        LOGF_INFO("Subframe set to X:%d Y:%d W:%d H:%d", x, y, w, h);
        return true;
    }

    LOG_ERROR("Failed to set subframe.");
    return false;
}

bool AlpacaCCD::UpdateCCDBin(int hor, int ver)
{
    if (!isConnected())
    {
        LOG_ERROR("Not connected to Alpaca camera.");
        return false;
    }

    nlohmann::json response;
    bool success = retryRequest([this, &response, hor, ver]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/binx";
        nlohmann::json body = {{"BinX", hor}};
        if (!makeAlpacaRequest(path, response, true, &body)) return false;

        path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/biny";
        body = {{"BinY", ver}};
        return makeAlpacaRequest(path, response, true, &body);
    });

    if (success)
    {
        PrimaryCCD.setBinX(hor);
        PrimaryCCD.setBinY(ver);
        LOGF_INFO("Binning set to %dx%d", hor, ver);
        return true;
    }

    LOG_ERROR("Failed to set binning.");
    return false;
}

bool AlpacaCCD::SetCaptureFormat(uint8_t index)
{
    if (!isConnected())
    {
        LOG_ERROR("Not connected to Alpaca camera.");
        return false;
    }

    nlohmann::json response;
    bool success = retryRequest([this, &response, index]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/readoutmode";
        nlohmann::json body = {{"ReadoutMode", index}};
        return makeAlpacaRequest(path, response, true, &body);
    });

    if (success)
    {
        m_CurrentReadoutModeIndex = index;
        // Update CaptureFormatSP to reflect the new active mode
        for (int i = 0; i < CaptureFormatSP.get  Number(); ++i)
        {
            CaptureFormatSP[i].setState(i == index ? ISS_ON : ISS_OFF);
        }
        CaptureFormatSP.setState(IPS_OK);
        CaptureFormatSP.apply();
        LOGF_INFO("Capture format set to index %d.", index);
        return true;
    }

    LOG_ERROR("Failed to set capture format.");
    return false;
}

bool AlpacaCCD::makeAlpacaRequest(const std::string& path, nlohmann::json& response, bool isPut, const nlohmann::json* body)
{
    if (ServerAddressTP[0].getText() == nullptr || ServerAddressTP[1].getText() == nullptr)
    {
        LOG_ERROR("Server address or port is not set.");
        return false;
    }

    try
    {
        httplib::Client cli(ServerAddressTP[0].getText(), std::stoi(ServerAddressTP[1].getText()));
        cli.set_connection_timeout(static_cast<int>(ConnectionSettingsNP[0].getValue()));
        cli.set_read_timeout(static_cast<int>(ConnectionSettingsNP[0].getValue()));

        std::string fullPath = path;
        if (isPut && body)
        {
            // For PUT requests, parameters are typically in the query string or body.
            // ASCOM Alpaca uses query string for PUT.
            // Convert JSON body to query string parameters
            std::string queryString = "?ClientID=" + std::to_string(getpid()); // Add ClientID
            for (auto const& [key, val] : body->items())
            {
                queryString += "&" + key + "=" + val.dump();
            }
            fullPath += queryString;
        }
        else
        {
            // For GET requests, add ClientID to query string
            fullPath += "?ClientID=" + std::to_string(getpid());
        }

        auto result = isPut ? cli.Put(fullPath.c_str()) : cli.Get(fullPath.c_str());

        if (!result)
        {
            LOG_ERROR("Failed to connect to Alpaca server.");
            return false;
        }

        if (result->status != 200)
        {
            LOGF_ERROR("HTTP error: %d - %s", result->status, result->body.c_str());
            return false;
        }

        response = nlohmann::json::parse(result->body);

        if (response["ErrorNumber"].get<int>() != 0)
        {
            LOGF_ERROR("Alpaca error: %s", response["ErrorMessage"].get<std::string>().c_str());
            return false;
        }

        return true;
    }
    catch(const std::exception &e)
    {
        LOGF_ERROR("Request error: %s", e.what());
        return false;
    }
}

bool AlpacaCCD::retryRequest(const std::function<bool()> &request)
{
    int maxRetries = static_cast<int>(ConnectionSettingsNP[1].getValue());
    int retryDelay = static_cast<int>(ConnectionSettingsNP[2].getValue());

    for (int attempt = 1; attempt <= maxRetries; attempt++)
    {
        try
        {
            if (request())
                return true;
        }
        catch(const std::exception &e)
        {
            LOGF_ERROR("Request attempt %d failed: %s", attempt, e.what());
        }

        if (attempt < maxRetries)
        {
            LOGF_DEBUG("Retrying request in %d ms (attempt %d/%d)",
                       retryDelay, attempt, maxRetries);
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
        }
    }
    return false;
}

void AlpacaCCD::updateStatus()
{
    updateExposureProgress();
    updateCoolerStatus();
    updateImageReady();
    updateCameraState();
}

void AlpacaCCD::updateExposureProgress()
{
    if (!m_ExposureInProgress)
        return;

    nlohmann::json response;
    bool success = retryRequest([this, &response]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) +
                           "/exposureremaining";
        return makeAlpacaRequest(path, response);
    });

    if (success)
    {
        double remaining = response["Value"].get<double>();
        PrimaryCCD.setExposureLeft(remaining);

        if (remaining <= 0.0)
        {
            m_ExposureInProgress = false;
            ExposureComplete(&PrimaryCCD); // Signal INDI that exposure is complete
        }
    }
    else
    {
        LOG_ERROR("Failed to get exposure remaining.");
        m_ExposureInProgress = false; // Assume exposure failed or completed if status cannot be retrieved
    }
}

void AlpacaCCD::updateCoolerStatus()
{
    if (!HasCooler())
        return;

    nlohmann::json response;
    bool success;

    // Get CoolerOn status
    success = retryRequest([this, &response]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/cooleron";
        return makeAlpacaRequest(path, response);
    });

    if (success)
    {
        bool coolerOn = response["Value"].get<bool>();
        CoolerSP[0].setState(coolerOn ? ISS_ON : ISS_OFF);
        CoolerSP[1].setState(coolerOn ? ISS_OFF : ISS_ON);
        CoolerSP.setState(IPS_OK);
        CoolerSP.apply();
    }

    // Get CCDTemperature
    success = retryRequest([this, &response]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/ccdtemperature";
        return makeAlpacaRequest(path, response);
    });

    if (success)
    {
        double ccdTemp = response["Value"].get<double>();
        TemperatureNP[0].setValue(ccdTemp);
        if (std::abs(ccdTemp - m_TargetTemperature) < 0.1) // Within 0.1 degree C
        {
            TemperatureNP.setState(IPS_OK);
        }
        else
        {
            TemperatureNP.setState(IPS_BUSY);
        }
        TemperatureNP.apply();
    }
}

void AlpacaCCD::updateImageReady()
{
    nlohmann::json response;
    bool success = retryRequest([this, &response]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/imageready";
        return makeAlpacaRequest(path, response);
    });

    if (success)
    {
        bool imageReady = response["Value"].get<bool>();
        if (imageReady && !m_ExposureInProgress)
        {
            // Image is ready, retrieve it
            // This part needs to be implemented to fetch the image data
            // For now, just log that an image is ready.
            LOG_INFO("Image is ready on Alpaca camera. Fetching image data...");

            // Example: Fetching image array (this is a simplified placeholder)
            // In a real implementation, you would fetch the image data,
            // convert it to a format INDI understands (e.g., FITS),
            // and then call ExposureComplete with the actual image data.
            // For now, we'll just simulate completion.
            // ExposureComplete(&PrimaryCCD); // This would be called after image data is fetched and processed
        }
    }
}

void AlpacaCCD::updateCameraState()
{
    nlohmann::json response;
    bool success = retryRequest([this, &response]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/camerastate";
        return makeAlpacaRequest(path, response);
    });

    if (success)
    {
        int cameraState = response["Value"].get<int>();
        // ASCOM CameraState: 0=Idle, 1=Waiting, 2=Exposing, 3=Reading, 4=Download, 5=Error
        switch (cameraState)
        {
            case 0: // Idle
                if (PrimaryCCD.ImageExposureNP.getState() != IPS_IDLE)
                {
                    PrimaryCCD.ImageExposureNP.setState(IPS_IDLE);
                }
                break;
            case 1: // Waiting
            case 2: // Exposing
            case 3: // Reading
            case 4: // Download
                if (PrimaryCCD.ImageExposureNP.getState() != IPS_BUSY)
                {
                    PrimaryCCD.ImageExposureNP.setState(IPS_BUSY);
                }
                break;
            case 5: // Error
                if (PrimaryCCD.ImageExposureNP.getState() != IPS_ERROR)
                {
                    PrimaryCCD.ImageExposureNP.setState(IPS_ERROR);
                }
                break;
        }
    }
}

void AlpacaCCD::updateReadoutModes()
{
    nlohmann::json response;
    bool success = retryRequest([this, &response]()
    {
        std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/readoutmodes";
        return makeAlpacaRequest(path, response);
    });

    if (success)
    {
        // Clear existing capture formats
        m_CaptureFormats.clear();
        deleteProperty(m_CaptureFormats);

        nlohmann::json readoutModes = response["Value"];
        for (int i = 0; i < readoutModes.size(); ++i)
        {
            std::string modeName = readoutModes[i].get<std::string>();
            CaptureFormat format = {modeName, modeName, 16, (i == 0)}; // Assume 16-bit, first mode is default
            addCaptureFormat(format);
        }

        // Re-define CaptureFormatSP with updated formats
        CaptureFormatSP.fill(getDeviceName(), "CCD_CAPTURE_FORMAT", "Capture Format", CAMERA_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60,
                             IPS_IDLE);
        defineProperty(CaptureFormatSP);

        // Get current readout mode
        success = retryRequest([this, &response]()
        {
            std::string path = "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/readoutmode";
            return makeAlpacaRequest(path, response);
        });

        if (success)
        {
            m_CurrentReadoutModeIndex = response["Value"].get<int>();
            if (m_CurrentReadoutModeIndex >= 0 && m_CurrentReadoutModeIndex < CaptureFormatSP.count())
            {
                CaptureFormatSP[m_CurrentReadoutModeIndex].setState(ISS_ON);
                CaptureFormatSP.setState(IPS_OK);
                CaptureFormatSP.apply();
            }
        }
    }
    else
    {
        LOG_ERROR("Failed to retrieve readout modes.");
    }
}
