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
#include <cmath>

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
    CoolerSP.fill(getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    // Setup server address properties
    ServerAddressTP[0].fill("HOST", "Host", "");
    ServerAddressTP[1].fill("PORT", "Port", "11111");
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
    GainNP.fill(getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Offset property
    OffsetNP[0].fill("OFFSET", "Offset", "%.0f", 0, 10000, 1, 0); // Max offset can vary
    OffsetNP.fill(getDeviceName(), "CCD_OFFSET", "Offset", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Cooler Power property
    CoolerPowerNP[0].fill("CCD_COOLER_VALUE", "Power (%)", "%.0f", 0, 100, 1, 0);
    CoolerPowerNP.fill(getDeviceName(), "CCD_COOLER_POWER", "Cooler Power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Device Info property
    DeviceInfoTP[0].fill("DESCRIPTION", "Description", "");
    DeviceInfoTP[1].fill("DRIVER_INFO", "Driver Info", "");
    DeviceInfoTP[2].fill("DRIVER_VERSION", "Driver Version", "");
    DeviceInfoTP[3].fill("NAME", "Name", "");
    DeviceInfoTP.fill(getDeviceName(), "DEVICE_INFO", "Device Info", CONNECTION_TAB, IP_RO, 60, IPS_IDLE);

    // Camera State property
    CameraStateTP[0].fill("STATE", "State", "Idle");
    CameraStateTP.fill(getDeviceName(), "CCD_CAMERA_STATE", "Camera State", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

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
}

bool AlpacaCCD::Connect()
{
    if (ServerAddressTP[0].getText() == nullptr || ServerAddressTP[1].getText() == nullptr)
    {
        LOG_ERROR("Server address or port is not set.");
        return false;
    }

    // Initialize HTTP client
    httpClient = std::make_unique<httplib::Client>(ServerAddressTP[0].getText(), std::stoi(ServerAddressTP[1].getText()));
    httpClient->set_connection_timeout(static_cast<int>(ConnectionSettingsNP[0].getValue()));
    httpClient->set_read_timeout(static_cast<int>(ConnectionSettingsNP[0].getValue()));

    // Test connection by getting camera status
    nlohmann::json response;
    bool success = sendAlpacaGET("/connected", response);

    if (!success)
    {
        LOG_ERROR("Failed to connect to Alpaca camera. Please check server address and port.");
        return false;
    }

    // Set connected state
    nlohmann::json body = {{"Connected", true}};
    success = sendAlpacaPUT("/connected", body, response);

    if (!success)
    {
        LOG_ERROR("Failed to set connected state on Alpaca camera.");
        return false;
    }

    LOG_INFO("Successfully connected to Alpaca camera.");

    // Get current CCD capabilities
    uint32_t cap = GetCCDCapability();

    // Check if camera supports Gain by trying to get gain value
    success = sendAlpacaGET("/gain", response);
    if (success)
    {
        m_HasGain = true;
        LOGF_INFO("Camera supports Gain: Yes (current value: %.0f)", response["Value"].get<double>());
    }
    else
    {
        LOG_INFO("Camera does not support Gain (failed to get gain value).");
        m_HasGain = false;
    }

    // Check if camera supports Offset by trying to get offset value
    success = sendAlpacaGET("/offset", response);
    if (success)
    {
        m_HasOffset = true;
        LOGF_INFO("Camera supports Offset: Yes (current value: %.0f)", response["Value"].get<double>());
    }
    else
    {
        LOG_INFO("Camera does not support Offset (failed to get offset value).");
        m_HasOffset = false;
    }

    // Check if camera supports setting CCD temperature
    success = sendAlpacaGET("/cansetccdtemperature", response);
    if (success && response["Value"].get<bool>())
    {
        cap |= INDI::CCD::CCD_HAS_COOLER;
        LOG_INFO("Camera supports CCD temperature control.");
    }
    else
    {
        cap &= ~INDI::CCD::CCD_HAS_COOLER;
        LOG_INFO("Camera does not support CCD temperature control.");
    }

    // Check pulse guide capability
    success = sendAlpacaGET("/canpulseguide", response);
    if (success)
    {
        m_CanPulseGuide = response["Value"].get<bool>();
        if (m_CanPulseGuide)
        {
            cap |= INDI::CCD::CCD_HAS_ST4_PORT;
            LOG_INFO("Camera supports pulse guiding.");
        }
        else
        {
            LOG_INFO("Camera does not support pulse guiding.");
        }
    }
    else
    {
        LOG_WARN("Failed to query CanPulseGuide, assuming no pulse guide support.");
        m_CanPulseGuide = false;
    }

    // Check can stop exposure capability
    success = sendAlpacaGET("/canstopexposure", response);
    if (success)
    {
        m_CanStopExposure = response["Value"].get<bool>();
        if (m_CanStopExposure)
        {
            LOG_INFO("Camera supports stopping exposure.");
        }
        else
        {
            LOG_INFO("Camera does not support stopping exposure.");
        }
    }
    else
    {
        LOG_WARN("Failed to query CanStopExposure, assuming no stop exposure support.");
        m_CanStopExposure = false;
    }

    // Check camera sensor type for Bayer pattern support
    success = sendAlpacaGET("/sensortype", response);
    if (success)
    {
        m_SensorType = response["Value"].get<uint8_t>();
        std::string sensorTypeStr = getSensorTypeString(m_SensorType);
        LOGF_INFO("Camera sensor type: %d (%s)", m_SensorType, sensorTypeStr.c_str());

        // Sensor types: 0=Monochrome, 1=Color, 2=RGGB Bayer, 3=CMYG Bayer, etc.
        bool isColorCamWithBayer = (m_SensorType >= 2); // Bayer patterns start at type 2

        if (isColorCamWithBayer)
        {
            cap |= INDI::CCD::CCD_HAS_BAYER;
            LOG_INFO("Camera has Bayer color sensor - enabling Bayer capability.");
        }
        else if (m_SensorType == 1)
        {
            LOG_INFO("Camera has color sensor (non-Bayer).");
        }
        else
        {
            LOG_INFO("Camera has monochrome sensor.");
        }
    }
    else
    {
        LOG_WARN("Failed to query sensor type, assuming monochrome sensor.");
    }


    // Get sensor type to determine available capture formats
    std::string sensorTypeStr = getSensorTypeString(m_SensorType);
    LOGF_INFO("Setting up capture formats for sensor type: %d (%s)", m_SensorType, sensorTypeStr.c_str());

    // Clear existing capture formats from base class
    m_CaptureFormats.clear();

    // Add capture formats based on sensor type and typical Alpaca camera capabilities
    // Most Alpaca cameras support both 8-bit and 16-bit modes
    switch (m_SensorType)
    {
        case 0: // Monochrome
            addCaptureFormat({"MONO_8", "Mono 8-bit", 8, false});
            addCaptureFormat({"MONO_16", "Mono 16-bit", 16, true});
            break;
        case 1: // Color (RGB)
            addCaptureFormat({"RGB_8", "RGB 8-bit", 8, true});
            addCaptureFormat({"RGB_16", "RGB 16-bit", 16, false});
            break;
        case 2: // RGGB Bayer
            BayerTP[CFA_TYPE].setText("RGGB");
            BayerTP.apply();
            [[fallthrough]];
        case 3: // CMYG Bayer
            addCaptureFormat({"RAW_8", "Raw 8-bit", 8, false});
            addCaptureFormat({"RAW_16", "Raw 16-bit", 16, true});
            cap |= CCD_HAS_BAYER;
            break;
        default:
            LOG_WARN("Unknown sensor type, defaulting to mono formats.");
            addCaptureFormat({"MONO_8", "Mono 8-bit", 8, true});
            addCaptureFormat({"MONO_16", "Mono 16-bit", 16, false});
            break;
    }

    // Update CCD capabilities
    SetCCDCapability(cap);

    // Start temperature monitoring timer
    mTimerTemperature.callOnTimeout(std::bind(&AlpacaCCD::temperatureTimerTimeout, this));
    mTimerTemperature.start(TEMP_TIMER_MS);

    SetTimer(getCurrentPollingPeriod());
    return true;
}

void AlpacaCCD::updateCameraCapabilities()
{
    if (!isConnected())
    {
        LOG_WARN("Not connected to Alpaca camera, cannot update camera capabilities.");
        return;
    }

    nlohmann::json response;
    bool success;

    // Get camera capabilities and update INDI properties
    success = sendAlpacaGET("/cameraxsize", response);
    if (success) m_CameraXSize = response["Value"].get<int>();

    success = sendAlpacaGET("/cameraysize", response);
    if (success) m_CameraYSize = response["Value"].get<int>();

    success = sendAlpacaGET("/pixelsizex", response);
    if (success) m_PixelSizeX = response["Value"].get<double>();

    success = sendAlpacaGET("/pixelsizey", response);
    if (success) m_PixelSizeY = response["Value"].get<double>();

    // Update CCDParams
    SetCCDParams(m_CameraXSize, m_CameraYSize, PrimaryCCD.getBPP(), m_PixelSizeX, m_PixelSizeY);

    success = sendAlpacaGET("/description", response);
    if (success) m_Description = response["Value"].get<std::string>();

    success = sendAlpacaGET("/driverinfo", response);
    if (success) m_DriverInfo = response["Value"].get<std::string>();

    success = sendAlpacaGET("/driverversion", response);
    if (success) m_DriverVersion = response["Value"].get<std::string>();

    success = sendAlpacaGET("/name", response);
    if (success) m_CameraName = response["Value"].get<std::string>();

    if (m_HasGain)
    {
        success = sendAlpacaGET("/gainmin", response);
        if (success) m_GainMin = response["Value"].get<double>();

        success = sendAlpacaGET("/gainmax", response);
        if (success) m_GainMax = response["Value"].get<double>();
    }

    success = sendAlpacaGET("/maxadu", response);
    if (success) m_MaxADU = response["Value"].get<uint32_t>();

    if (m_SensorType >= 2)
    {
        if (sendAlpacaGET("/bayeroffsetx", response))
            m_BayerOffsetX = response["Value"].get<uint8_t>();

        if (sendAlpacaGET("/bayeroffsety", response))
            m_BayerOffsetY = response["Value"].get<uint8_t>();
    }
}

bool AlpacaCCD::Disconnect()
{
    // Stop the worker thread
    mWorker.quit();

    // Stop temperature monitoring timer
    mTimerTemperature.stop();

    if (isConnected())
    {
        nlohmann::json response;
        nlohmann::json body = {{"Connected", false}};
        bool success = sendAlpacaPUT("/connected", body, response);

        if (!success)
        {
            LOG_ERROR("Failed to set disconnected state on Alpaca camera.");
            return false;
        }
    }

    LOG_INFO("Disconnected from Alpaca camera.");
    httpClient.reset(); // Destroy HTTP client
    return true;
}

const char *AlpacaCCD::getDefaultName()
{
    return "Alpaca Camera";
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
            if (!isConnected())
            {
                DeviceNumberNP.update(values, names, n);
                DeviceNumberNP.setState(IPS_OK);
                saveConfig();
                LOG_INFO("Alpaca device number updated.");
            }
            else
                DeviceNumberNP.setState(IPS_IDLE);

            DeviceNumberNP.apply();
            return true;
        }
        else if (GainNP.isNameMatch(name))
        {
            updateProperty(GainNP, values, names, n, [this, values]()
            {
                nlohmann::json response;
                nlohmann::json body = {{"Gain", values[0]}};
                return sendAlpacaPUT("/gain", body, response);
            }, true);
            return true;
        }
        else if (OffsetNP.isNameMatch(name))
        {
            updateProperty(OffsetNP, values, names, n, [this, values]()
            {
                nlohmann::json response;
                nlohmann::json body = {{"Offset", values[0]}};
                return sendAlpacaPUT("/offset", body, response);
            }, true);
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool AlpacaCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (isDeviceNameMatch(dev))
    {
        if (CoolerSP.isNameMatch(name))
        {
            updateProperty(CoolerSP, states, names, n, [this, states]()
            {
                bool coolerOn = states[0] == ISS_ON;
                nlohmann::json response;
                nlohmann::json body = {{"CoolerOn", coolerOn}};
                return sendAlpacaPUT("/cooleron", body, response);
            });
            return true;
        }
        else if (ReadoutModeSP.isNameMatch(name))
        {
            if (ReadoutModeSP.update(states, names, n) == false)
            {
                ReadoutModeSP.setState(IPS_ALERT);
                ReadoutModeSP.apply();
                return true;
            }

            int index = ReadoutModeSP.findOnSwitchIndex();
            if (index != -1)
            {
                // Set the readout mode on the Alpaca camera
                nlohmann::json response;
                nlohmann::json body = {{"ReadoutMode", index}};
                bool success = sendAlpacaPUT("/readoutmode", body, response);

                if (success)
                {
                    m_CurrentReadoutModeIndex = index;
                    ReadoutModeSP.setState(IPS_OK);
                    ReadoutModeSP.apply();
                    LOGF_DEBUG("Readout mode set to index %d: %s", index, ReadoutModeSP[index].getLabel());
                    return true;
                }
                else
                {
                    LOG_ERROR("Failed to set readout mode.");
                }
            }
            ReadoutModeSP.setState(IPS_ALERT);
            ReadoutModeSP.apply();
            return true;
        }
    }
    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool AlpacaCCD::updateProperties()
{
    // Update capabilities first
    if (isConnected())
        updateCameraCapabilities();

    INDI::CCD::updateProperties();

    if (isConnected())
    {
        nlohmann::json response;
        bool success;
        defineProperty(CameraStateTP);

        // Device Info properties
        defineProperty(DeviceInfoTP);
        DeviceInfoTP[0].setText(m_Description.c_str());
        DeviceInfoTP[1].setText(m_DriverInfo.c_str());
        DeviceInfoTP[2].setText(m_DriverVersion.c_str());
        DeviceInfoTP[3].setText(m_CameraName.c_str());
        DeviceInfoTP.setState(IPS_OK);
        DeviceInfoTP.apply();

        if (m_HasGain)
        {
            // Get current gain
            success = sendAlpacaGET("/gain", response);
            if (success)
            {
                defineProperty(GainNP);
                GainNP[0].setValue(response["Value"].get<double>());
                GainNP.setState(IPS_OK);
                GainNP.apply();
            }
            else
            {
                LOG_WARN("Failed to get gain.");
            }

            // Set gain min/max from cached values
            GainNP[0].setMin(m_GainMin);
            GainNP[0].setMax(m_GainMax);
        }

        if (m_HasOffset)
        {
            // Get current offset
            success = sendAlpacaGET("/offset", response);
            if (success)
            {
                defineProperty(OffsetNP);
                OffsetNP[0].setValue(response["Value"].get<double>());
                OffsetNP.setState(IPS_OK);
                OffsetNP.apply();
            }
            else
            {
                LOG_WARN("Failed to get offset.");
            }
        }

        // Cooler properties
        defineProperty(CoolerSP);
        defineProperty(TemperatureNP);
        defineProperty(CoolerPowerNP);

        defineProperty(ReadoutModeSP);
        updateReadoutModes();

        if (m_SensorType >= 2)
        {
            char offsetX[2], offsetY[2];
            snprintf(offsetX, sizeof(offsetX), "%d", m_BayerOffsetX);
            snprintf(offsetY, sizeof(offsetY), "%d", m_BayerOffsetY);
            BayerTP[CFA_OFFSET_X].setText(offsetX);
            BayerTP[CFA_OFFSET_Y].setText(offsetY);
            BayerTP.apply();
            LOGF_DEBUG("Bayer offsets: X=%s, Y=%s", BayerTP[CFA_OFFSET_X].getText(), BayerTP[CFA_OFFSET_Y].getText());
        }
    }
    else
    {
        LOG_INFO("Alpaca camera is disconnected.");
        deleteProperty(CoolerSP);
        deleteProperty(TemperatureNP);
        deleteProperty(GainNP);
        deleteProperty(OffsetNP);
        deleteProperty(CoolerPowerNP);
        deleteProperty(DeviceInfoTP);
        deleteProperty(ReadoutModeSP);
        deleteProperty(CameraStateTP);
    }

    return true;
}

void AlpacaCCD::TimerHit()
{
    if (!isConnected())
        return;

    // Worker thread now handles exposure timing, so we just update status
    updateStatus();
    SetTimer(getCurrentPollingPeriod());
}

bool AlpacaCCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    ServerAddressTP.save(fp);
    DeviceNumberNP.save(fp);
    ConnectionSettingsNP.save(fp);
    if (m_HasGain)
    {
        GainNP.save(fp);
    }
    if (m_HasOffset)
    {
        OffsetNP.save(fp);
    }

    return true;
}

bool AlpacaCCD::StartExposure(float duration)
{
    if (!isConnected())
    {
        LOG_ERROR("Not connected to Alpaca camera.");
        return false;
    }

    // Start the worker thread to handle the exposure
    mWorker.start(std::bind(&AlpacaCCD::workerExposure, this, std::placeholders::_1, duration));
    return true;
}

bool AlpacaCCD::AbortExposure()
{
    LOG_DEBUG("Aborting exposure...");

    // Stop the worker thread
    mWorker.quit();

    // Send abort command to Alpaca camera
    nlohmann::json response;
    bool success = sendAlpacaPUT("/abortexposure", {}, response);

    if (success)
    {
        LOG_INFO("Exposure aborted.");
    }
    else
    {
        LOG_ERROR("Failed to send abort command to Alpaca camera.");
    }

    return true;
}

int AlpacaCCD::SetTemperature(double temperature)
{
    if (!isConnected())
    {
        LOG_ERROR("Not connected to Alpaca camera.");
        return -1;
    }

    // If there difference, for example, is less than 0.25 degrees, let's immediately return OK.
    if (std::abs(temperature - m_CurrentTemperature) < TEMP_THRESHOLD)
        return 1;

    // Activate cooler if not already on
    if (CoolerSP[INDI_ENABLED].getState() != ISS_ON)
    {
        nlohmann::json response;
        nlohmann::json body = {{"CoolerOn", true}};
        bool success = sendAlpacaPUT("/cooleron", body, response);

        if (!success)
        {
            LOG_ERROR("Failed to activate cooler.");
            return -1;
        }

        CoolerSP[INDI_ENABLED].setState(ISS_ON);
        CoolerSP[INDI_DISABLED].setState(ISS_OFF);
        CoolerSP.setState(IPS_OK);
        CoolerSP.apply();
    }

    m_TargetTemperature = temperature;

    nlohmann::json response;
    nlohmann::json body = {{"SetCCDTemperature", temperature}};
    bool success = sendAlpacaPUT("/setccdtemperature", body, response);

    if (success)
    {
        LOGF_DEBUG("Setting temperature to %.2f C.", temperature);
        return 0;
    }

    LOGF_ERROR("Failed to set target temperature to %.2f C.", temperature);
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
    nlohmann::json body;
    bool success;

    body = {{"StartX", x}};
    success = sendAlpacaPUT("/startx", body, response);
    if (!success) return false;

    body = {{"StartY", y}};
    success = sendAlpacaPUT("/starty", body, response);
    if (!success) return false;

    body = {{"NumX", w / PrimaryCCD.getBinX()}};
    success = sendAlpacaPUT("/numx", body, response);
    if (!success) return false;

    body = {{"NumY", h / PrimaryCCD.getBinY()}};
    return sendAlpacaPUT("/numy", body, response);
}

bool AlpacaCCD::UpdateCCDBin(int hor, int ver)
{
    if (!isConnected())
    {
        LOG_ERROR("Not connected to Alpaca camera.");
        return false;
    }

    nlohmann::json response;
    nlohmann::json body;
    bool success;

    body = {{"BinX", hor}};
    success = sendAlpacaPUT("/binx", body, response);
    if (!success) return false;

    body = {{"BinY", ver}};
    success = sendAlpacaPUT("/biny", body, response);
    // We need to update ROI *after* setting binning in Alpaca (at least for Pegasus SmartEye for now)
    if (success)
        return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
    return false;
}

bool AlpacaCCD::SetCaptureFormat(uint8_t index)
{
    if (!isConnected())
    {
        LOG_ERROR("Not connected to Alpaca camera.");
        return false;
    }

    // Get the capture format from the base class
    if (index >= m_CaptureFormats.size())
    {
        LOGF_ERROR("Invalid capture format index: %d", index);
        return false;
    }

    const auto& format = m_CaptureFormats[index];

    // Set the bit depth based on the selected format
    uint8_t bpp = format.bitsPerPixel;
    PrimaryCCD.setBPP(bpp);

    // Update the frame buffer size
    int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    LOGF_DEBUG("Capture format set to %s (%d-bit)", format.label.c_str(), bpp);
    return true;
}

// Helper function for pulse guiding
bool AlpacaCCD::sendPulseGuide(ALPACA_GUIDE_DIRECTION direction, long duration)
{
    if (!isConnected())
    {
        LOG_ERROR("Not connected to Alpaca camera.");
        return false;
    }

    if (!m_CanPulseGuide)
    {
        LOG_ERROR("Camera does not support pulse guiding.");
        return false;
    }

    nlohmann::json response;
    nlohmann::json body;
    int alpacaDirection;

    switch (direction)
    {
        case ALPACA_GUIDE_NORTH:
            alpacaDirection = 0; // guideNorth
            break;
        case ALPACA_GUIDE_SOUTH:
            alpacaDirection = 1; // guideSouth
            break;
        case ALPACA_GUIDE_EAST:
            alpacaDirection = 2; // guideEast
            break;
        case ALPACA_GUIDE_WEST:
            alpacaDirection = 3; // guideWest
            break;
        default:
            LOG_ERROR("Invalid guide direction.");
            return false;
    }

    body = {{"Direction", alpacaDirection}, {"Duration", duration}};
    return sendAlpacaPUT("/pulseguide", body, response);

}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
IPState AlpacaCCD::GuideNorth(uint32_t ms)
{
    if (sendPulseGuide(ALPACA_GUIDE_NORTH, ms))
        return IPS_OK;
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
IPState AlpacaCCD::GuideSouth(uint32_t ms)
{
    if (sendPulseGuide(ALPACA_GUIDE_SOUTH, ms))
        return IPS_OK;
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
IPState AlpacaCCD::GuideEast(uint32_t ms)
{
    if (sendPulseGuide(ALPACA_GUIDE_EAST, ms))
        return IPS_OK;
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
IPState AlpacaCCD::GuideWest(uint32_t ms)
{
    if (sendPulseGuide(ALPACA_GUIDE_WEST, ms))
        return IPS_OK;
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
std::string AlpacaCCD::getAlpacaURL(const std::string& endpoint)
{
    return "/api/v1/camera/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + endpoint;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
std::string AlpacaCCD::getSensorTypeString(uint8_t sensorType)
{
    switch (sensorType)
    {
        case 0:
            return "Monochrome";
        case 1:
            return "Color";
        case 2:
            return "RGGB Bayer";
        case 3:
            return "CMYG Bayer";
        case 4:
            return "CMYG2 Bayer";
        case 5:
            return "LRGB Truesense";
        default:
            return "Unknown";
    }
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool AlpacaCCD::sendAlpacaGET(const std::string& endpoint, nlohmann::json& response)
{
    if (!httpClient)
    {
        LOG_ERROR("HTTP client not initialized.");
        return false;
    }

    std::string url = getAlpacaURL(endpoint);
    url += "?ClientID=" + std::to_string(getpid()) + "&ClientTransactionID=" + std::to_string(getTransactionId());

    // 5 seconds timeout
    httpClient->set_read_timeout(5, 0);
    auto result = httpClient->Get(url.c_str());

    if (!result)
    {
        LOGF_ERROR("HTTP GET failed for %s: %s", endpoint.c_str(),
                   httplib::to_string(result.error()).c_str());
        return false;
    }

    if (result->status != 200)
    {
        LOGF_ERROR("HTTP GET %s returned status %d", endpoint.c_str(), result->status);
        return false;
    }

    try
    {
        response = nlohmann::json::parse(result->body);

        // Check for Alpaca errors
        if (response.contains("ErrorNumber") && response["ErrorNumber"].get<int>() != 0)
        {
            LOGF_ERROR("Alpaca error in %s: %d - %s", endpoint.c_str(),
                       response["ErrorNumber"].get<int>(),
                       response["ErrorMessage"].get<std::string>().c_str());
            return false;
        }

        return true;

    }
    catch (const nlohmann::json::exception& e)
    {
        LOGF_ERROR("JSON parse error for %s: %s", endpoint.c_str(), e.what());
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool AlpacaCCD::sendAlpacaPUT(const std::string& endpoint, const nlohmann::json& request, nlohmann::json& response)
{
    if (!httpClient)
    {
        LOG_ERROR("HTTP client not initialized.");
        return false;
    }

    std::string url = getAlpacaURL(endpoint);

    // Convert JSON to form data for Alpaca compatibility
    std::string form_data;
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

    // Add ClientID and ClientTransactionID to form data
    if (!form_data.empty()) form_data += "&";
    form_data += "ClientID=" + std::to_string(getpid());
    form_data += "&ClientTransactionID=" + std::to_string(getTransactionId());

    httplib::Headers headers =
    {
        {"Content-Type", "application/x-www-form-urlencoded"}
    };

    auto result = httpClient->Put(url.c_str(), headers, form_data, "application/x-www-form-urlencoded");

    if (!result)
    {
        LOGF_ERROR("HTTP PUT failed for %s: %s", endpoint.c_str(),
                   httplib::to_string(result.error()).c_str());
        return false;
    }

    if (result->status != 200)
    {
        LOGF_ERROR("HTTP PUT %s returned status %d", endpoint.c_str(), result->status);
        return false;
    }

    try
    {
        response = nlohmann::json::parse(result->body);

        // Check for Alpaca errors
        if (response.contains("ErrorNumber") && response["ErrorNumber"].get<int>() != 0)
        {

            // JM 2025.08.29: For some reason, I always get error 1025 when setting temperature
            // even though value is in range. Ignore this error for now.
            // if (response["ErrorNumber"].get<int>() == 1025)
            //     return true;

            LOGF_ERROR("Alpaca error in %s: %d - %s", endpoint.c_str(),
                       response["ErrorNumber"].get<int>(),
                       response["ErrorMessage"].get<std::string>().c_str());
            return false;
        }

        return true;

    }
    catch (const nlohmann::json::exception& e)
    {
        LOGF_ERROR("JSON parse error for %s: %s", e.what());
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool AlpacaCCD::alpacaGetImageReady()
{
    nlohmann::json response;
    if (!sendAlpacaGET("/imageready", response))
    {
        return false;
    }
    return response["Value"].get<bool>();
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool AlpacaCCD::alpacaGetImageArrayImageBytes(uint8_t** buffer, size_t* buffer_size, ImageBytesMetadata* metadata)
{
    if (!httpClient)
    {
        LOG_ERROR("HTTP client not initialized.");
        return false;
    }

    // Set up headers for ImageBytes protocol as per ASCOM Alpaca API v10 section 8.5.1
    httplib::Headers headers =
    {
        {"Accept", "application/imagebytes"}
    };

    std::string url = getAlpacaURL("/imagearray");
    url += "?ClientID=" + std::to_string(getpid()) + "&ClientTransactionID=" + std::to_string(getTransactionId());

    auto result = httpClient->Get(url.c_str(), headers);

    if (!result)
    {
        LOGF_ERROR("Failed to get image array: %s", httplib::to_string(result.error()).c_str());
        return false;
    }

    if (result->status != 200)
    {
        LOGF_ERROR("Image array request returned status %d", result->status);
        return false;
    }

    // Check Content-Type header as per ASCOM Alpaca API v10 section 8.5.3
    auto content_type = result->get_header_value("Content-Type");
    if (content_type.find("application/imagebytes") == std::string::npos)
    {
        LOGF_DEBUG("Server returned Content-Type: %s, falling back to JSON", content_type.c_str());
        return false; // Caller should try JSON fallback
    }

    // Parse ImageBytes binary format as per ASCOM Alpaca API v10 section 8.6
    if (result->body.size() < sizeof(ImageBytesMetadata))
    {
        LOGF_ERROR("Response too small for ImageBytes metadata: %zu bytes", result->body.size());
        return false;
    }

    // Extract metadata (44 bytes) with little-endian byte ordering as per section 8.8.2
    std::memcpy(metadata, result->body.data(), sizeof(ImageBytesMetadata));

    // Convert from little-endian if needed (most systems are little-endian already)
    // Note: The spec requires little-endian format for maximum compatibility

    // Validate metadata version as per section 8.7.1
    if (metadata->MetadataVersion != 1)
    {
        LOGF_ERROR("Unsupported ImageBytes metadata version: %d", metadata->MetadataVersion);
        return false;
    }

    // Check for errors as per section 8.9
    if (metadata->ErrorNumber != 0)
    {
        // Extract UTF8 error message from data section
        size_t error_msg_size = result->body.size() - metadata->DataStart;
        if (static_cast<size_t>(metadata->DataStart) < result->body.size() && error_msg_size > 0)
        {
            std::string error_msg(result->body.data() + metadata->DataStart, error_msg_size);
            LOGF_ERROR("Alpaca ImageBytes error %d: %s", metadata->ErrorNumber, error_msg.c_str());
        }
        else
        {
            LOGF_ERROR("Alpaca ImageBytes error %d (no message)", metadata->ErrorNumber);
        }
        return false;
    }

    // Validate dimensions
    if (metadata->Rank < 2 || metadata->Rank > 3)
    {
        LOGF_ERROR("Invalid image rank: %d (must be 2 or 3)", metadata->Rank);
        return false;
    }

    if (metadata->Dimension1 <= 0 || metadata->Dimension2 <= 0)
    {
        LOGF_ERROR("Invalid image dimensions: %dx%d", metadata->Dimension1, metadata->Dimension2);
        return false;
    }

    // Calculate expected data size based on transmission element type
    size_t bytes_per_element;
    switch (metadata->TransmissionElementType)
    {
        case 6: // Byte
            bytes_per_element = 1;
            break;
        case 1: // Int16
        case 8: // UInt16
            bytes_per_element = 2;
            break;
        case 2: // Int32
        case 9: // UInt32
        case 4: // Single (float)
            bytes_per_element = 4;
            break;
        case 3: // Double
        case 5: // UInt64
        case 7: // Int64
            bytes_per_element = 8;
            break;
        default:
            LOGF_ERROR("Unsupported transmission element type: %d", metadata->TransmissionElementType);
            return false;
    }

    uint32_t planes = (metadata->Rank == 3) ? metadata->Dimension3 : 1;
    size_t expected_data_size = metadata->Dimension1 * metadata->Dimension2 * planes * bytes_per_element;
    size_t actual_data_size = result->body.size() - metadata->DataStart;

    if (actual_data_size != expected_data_size)
    {
        LOGF_ERROR("Image data size mismatch: expected %zu bytes, got %zu bytes",
                   expected_data_size, actual_data_size);
        return false;
    }

    // Allocate buffer for image data
    *buffer_size = expected_data_size;
    PrimaryCCD.setFrameBufferSize(*buffer_size);
    *buffer = PrimaryCCD.getFrameBuffer();
    if (!*buffer)
    {
        LOG_ERROR("Failed to allocate image buffer");
        return false;
    }

    // Copy image data from response
    std::memcpy(*buffer, result->body.data() + metadata->DataStart, *buffer_size);

    LOGF_DEBUG("ImageBytes: %dx%dx%d, type %d->%d, %zu bytes",
               metadata->Dimension1, metadata->Dimension2, planes,
               metadata->ImageElementType, metadata->TransmissionElementType, *buffer_size);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool AlpacaCCD::alpacaGetImageArrayJSON(ImageMetadata& meta, uint8_t** buffer, size_t* buffer_size)
{
    // Fallback to standard JSON ImageArray method
    nlohmann::json response;
    if (!sendAlpacaGET("/imagearray", response))
    {
        LOG_ERROR("Failed to get image array via JSON");
        return false;
    }

    if (!response.contains("Value") || !response["Value"].is_array())
    {
        LOG_ERROR("Invalid JSON image array response");
        return false;
    }

    auto imageArray = response["Value"];

    // Determine array dimensions
    if (imageArray.empty() || !imageArray[0].is_array())
    {
        LOG_ERROR("Invalid image array structure");
        return false;
    }

    meta.width = imageArray.size();
    meta.height = imageArray[0].size();
    meta.rank = 2;
    meta.planes = 0;

    // Check if it's a 3D array (color)
    if (!imageArray[0][0].is_number())
    {
        if (imageArray[0][0].is_array())
        {
            meta.rank = 3;
            meta.planes = imageArray[0][0].size();
        }
        else
        {
            LOG_ERROR("Unsupported image array element type");
            return false;
        }
    }

    // Allocate buffer (assume 32-bit integers as per ASCOM standard)
    size_t pixel_count = meta.width * meta.height * (meta.planes > 0 ? meta.planes : 1);
    *buffer_size = pixel_count * sizeof(int32_t);
    PrimaryCCD.setFrameBufferSize(*buffer_size);
    *buffer = PrimaryCCD.getFrameBuffer();
    if (!*buffer)
    {
        LOG_ERROR("Failed to allocate image buffer");
        return false;
    }

    int32_t* int_buffer = reinterpret_cast<int32_t*>(*buffer);
    size_t index = 0;

    // Copy data with proper ordering as per ASCOM specification
    if (meta.rank == 2)
    {
        // 2D monochrome array
        for (uint32_t x = 0; x < meta.width; x++)
        {
            for (uint32_t y = 0; y < meta.height; y++)
            {
                int_buffer[index++] = imageArray[x][y].get<int32_t>();
            }
        }
    }
    else
    {
        // 3D color array
        for (uint32_t x = 0; x < meta.width; x++)
        {
            for (uint32_t y = 0; y < meta.height; y++)
            {
                for (uint32_t p = 0; p < meta.planes; p++)
                {
                    int_buffer[index++] = imageArray[x][y][p].get<int32_t>();
                }
            }
        }
    }

    meta.type = 2; // Int32

    // Get additional metadata for FITS headers
    meta.max_adu = m_MaxADU;
    meta.sensor_type = m_SensorType;
    if (meta.sensor_type >= 2)
    {
        meta.bayer_offset_x = atoi(BayerTP[CFA_OFFSET_X].getText());
        meta.bayer_offset_y = atoi(BayerTP[CFA_OFFSET_Y].getText());
    }

    LOG_INFO("Downloaded image via JSON fallback");
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool AlpacaCCD::downloadImage()
{
    uint8_t* image_buffer = nullptr;
    size_t buffer_size = 0;
    bool success = false;

    LOG_DEBUG("Starting image download...");

    // Try ImageBytes protocol first (ASCOM Alpaca API v10 section 8)
    ImageBytesMetadata imagebytes_meta;
    if (alpacaGetImageArrayImageBytes(&image_buffer, &buffer_size, &imagebytes_meta))
    {
        LOGF_DEBUG("ImageBytes metadata: %dx%dx%d, rank=%d, image_type=%d, transmission_type=%d",
                   imagebytes_meta.Dimension1, imagebytes_meta.Dimension2,
                   (imagebytes_meta.Rank == 3) ? imagebytes_meta.Dimension3 : 1,
                   imagebytes_meta.Rank, imagebytes_meta.ImageElementType, imagebytes_meta.TransmissionElementType);
        LOGF_DEBUG("Raw buffer size: %zu bytes", buffer_size);

        // Convert ImageBytes metadata to our internal format
        m_CurrentImage.width = imagebytes_meta.Dimension1;
        m_CurrentImage.height = imagebytes_meta.Dimension2;
        m_CurrentImage.planes = (imagebytes_meta.Rank == 3) ? imagebytes_meta.Dimension3 : 0;
        m_CurrentImage.rank = imagebytes_meta.Rank;

        // Map transmission element type to our type system
        switch (imagebytes_meta.TransmissionElementType)
        {
            case 6: // Byte
                m_CurrentImage.type = 1;
                LOG_DEBUG("Transmission type: Byte (8-bit) -> Internal type 1");
                break;
            case 1: // Int16
            case 8: // UInt16
                m_CurrentImage.type = 2;
                LOGF_DEBUG("Transmission type: %s (16-bit) -> Internal type 2",
                           (imagebytes_meta.TransmissionElementType == 1) ? "Int16" : "UInt16");
                break;
            case 2: // Int32
            case 9: // UInt32
            case 4: // Single (float)
                m_CurrentImage.type = 3;
                LOGF_DEBUG("Transmission type: %s (32-bit) -> Internal type 3",
                           (imagebytes_meta.TransmissionElementType == 2) ? "Int32" :
                           (imagebytes_meta.TransmissionElementType == 9) ? "UInt32" : "Single");
                break;
            case 3: // Double
            case 5: // UInt64
            case 7: // Int64
                m_CurrentImage.type = 4;
                LOGF_DEBUG("Transmission type: %s (64-bit) -> Internal type 4",
                           (imagebytes_meta.TransmissionElementType == 3) ? "Double" :
                           (imagebytes_meta.TransmissionElementType == 5) ? "UInt64" : "Int64");
                break;
            default:
                m_CurrentImage.type = 3; // Default to Int32
                LOGF_WARN("Unknown transmission type %d, defaulting to Internal type 3",
                          imagebytes_meta.TransmissionElementType);
                break;
        }

        // Get additional metadata for FITS headers
        m_CurrentImage.max_adu = m_MaxADU;
        m_CurrentImage.sensor_type = m_SensorType;
        if (m_CurrentImage.sensor_type >= 2)
        {
            m_CurrentImage.bayer_offset_x = atoi(BayerTP[CFA_OFFSET_X].getText());
            m_CurrentImage.bayer_offset_y = atoi(BayerTP[CFA_OFFSET_Y].getText());
            LOGF_DEBUG("Bayer offsets: X=%d, Y=%d", m_CurrentImage.bayer_offset_x, m_CurrentImage.bayer_offset_y);
        }

        LOGF_DEBUG("Final image metadata: %dx%d, planes=%d, rank=%d, type=%d",
                   m_CurrentImage.width, m_CurrentImage.height, m_CurrentImage.planes,
                   m_CurrentImage.rank, m_CurrentImage.type);

        // Convert ImageBytes data format to match our processing expectations
        // ImageBytes uses row-major ordering as per ASCOM spec section 8.8.1
        // We need to handle the data according to transmission element type
        success = processImageBytesData(image_buffer, buffer_size, imagebytes_meta);
    }
    else
    {
        // Fallback to JSON ImageArray method
        LOG_DEBUG("ImageBytes not supported, falling back to JSON ImageArray");
        if (alpacaGetImageArrayJSON(m_CurrentImage, &image_buffer, &buffer_size))
        {
            LOGF_DEBUG("JSON image metadata: %dx%d, planes=%d, rank=%d, type=%d",
                       m_CurrentImage.width, m_CurrentImage.height, m_CurrentImage.planes,
                       m_CurrentImage.rank, m_CurrentImage.type);
            LOGF_DEBUG("JSON buffer size: %zu bytes", buffer_size);

            // Translate coordinate system (ASCOM top-left to INDI bottom-left)
            translateCoordinates(image_buffer, m_CurrentImage);

            // Process the image
            if (m_CurrentImage.planes == 0 || m_CurrentImage.planes == 1)
            {
                success = processMonoImage(image_buffer);
            }
            else
            {
                success = processColorImage(image_buffer);
            }
        }
        else
        {
            LOG_ERROR("Failed to download image via both ImageBytes and JSON methods");
        }
    }

    // Clean up
    if (image_buffer)
    {
        // Since we are using the PrimaryCCD buffer, we don't need to free it here.
        // The INDI::CCD class will handle the memory management.
    }

    m_ExposureInProgress = false;
    LOGF_DEBUG("Image download completed: %s", success ? "SUCCESS" : "FAILED");
    return success;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
void AlpacaCCD::updateReadoutModes()
{
    if (!isConnected())
    {
        LOG_WARN("Not connected to Alpaca camera, cannot update readout modes.");
        return;
    }

    nlohmann::json response;

    // Now handle readout modes separately (these are camera-specific settings)
    if (sendAlpacaGET("/readoutmodes", response))
    {
        if (response.contains("Value") && response["Value"].is_array())
        {
            auto readoutModes = response["Value"];
            if (!readoutModes.empty())
            {
                // Create switches for each readout mode
                ReadoutModeSP.resize(readoutModes.size());
                for (size_t i = 0; i < readoutModes.size(); ++i)
                {
                    std::string modeName = readoutModes[i].get<std::string>();
                    char nameBuf[MAXINDINAME];
                    snprintf(nameBuf, sizeof(nameBuf), "MODE_%zu", i);
                    ReadoutModeSP[i].fill(nameBuf, modeName.c_str(), ISS_OFF);
                }

                ReadoutModeSP.fill(getDeviceName(), "READOUT_MODE", "Readout Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

                // Get current readout mode
                if (sendAlpacaGET("/readoutmode", response))
                {
                    m_CurrentReadoutModeIndex = response["Value"].get<int>();
                    if (m_CurrentReadoutModeIndex >= 0 && static_cast<size_t>(m_CurrentReadoutModeIndex) < ReadoutModeSP.count())
                    {
                        ReadoutModeSP[m_CurrentReadoutModeIndex].setState(ISS_ON);
                    }
                }
                else
                {
                    LOG_WARN("Failed to get current readout mode, defaulting to first mode.");
                    ReadoutModeSP[0].setState(ISS_ON);
                    m_CurrentReadoutModeIndex = 0;
                }

                ReadoutModeSP.setState(IPS_OK);
                ReadoutModeSP.apply();
            }
        }
    }
    else
    {
        LOG_WARN("Failed to get readout modes from Alpaca camera.");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
void AlpacaCCD::updateStatus()
{
    if (!isConnected())
        return;

    // Update camera state
    if (PrimaryCCD.isExposing())
        updateCameraState();
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
void AlpacaCCD::workerExposure(const std::atomic_bool &isAboutToQuit, float duration)
{
    PrimaryCCD.setExposureDuration(duration);

    LOGF_DEBUG("StartExposure->setexp : %.3fs", duration);

    // Start the exposure on the Alpaca camera
    nlohmann::json response;
    nlohmann::json body =
    {
        {"Duration", duration},
        {"Light", PrimaryCCD.getFrameType() == INDI::CCDChip::LIGHT_FRAME}
    };

    bool success = sendAlpacaPUT("/startexposure", body, response);
    if (!success)
    {
        LOG_ERROR("Failed to start exposure on Alpaca camera.");
        PrimaryCCD.setExposureFailed();
        return;
    }

    INDI::ElapsedTimer exposureTimer;
    LOGF_INFO("Taking a %.3f seconds frame...", duration);

    // Wait for the exposure duration using INDI's own timer
    do
    {
        float delay = 0.1;
        float timeLeft = std::max(duration - exposureTimer.elapsed() / 1000.0, 0.0);

        // Check the status every second until the time left is about one second,
        // after which decrease the poll interval
        if (timeLeft > 1.1)
        {
            delay = std::max(timeLeft - std::trunc(timeLeft), 0.005f);
            timeLeft = std::round(timeLeft);
        }

        if (timeLeft > 0)
        {
            PrimaryCCD.setExposureLeft(timeLeft);
        }

        // Check if we should quit
        if (isAboutToQuit)
            return;

        // Sleep for the calculated delay
        usleep(delay * 1000 * 1000);
    }
    while (exposureTimer.elapsed() / 1000.0 < duration);

    // Exposure time complete, now wait for image to be ready
    PrimaryCCD.setExposureLeft(0);
    LOG_INFO("Exposure time complete, waiting for image...");

    // Poll for image ready status
    int maxWaitTime = 30; // Maximum 30 seconds to wait for image
    int waitCount = 0;

    while (waitCount < maxWaitTime * 10) // Check every 100ms
    {
        if (isAboutToQuit)
            return;

        if (alpacaGetImageReady())
        {
            LOG_INFO("Image ready, downloading...");
            downloadImage();
            return;
        }

        usleep(100 * 1000); // Wait 100ms
        waitCount++;
    }

    LOG_ERROR("Timeout waiting for image to be ready.");
    PrimaryCCD.setExposureFailed();
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
void AlpacaCCD::updateCoolerStatus()
{
    if (!isConnected())
        return;

    nlohmann::json response;
    bool success;

    // Get CoolerOn status
    success = sendAlpacaGET("/cooleron", response);
    if (success)
    {
        bool coolerOn = response["Value"].get<bool>();
        CoolerSP[INDI_ENABLED].setState(coolerOn ? ISS_ON : ISS_OFF);
        CoolerSP[INDI_DISABLED].setState(coolerOn ? ISS_OFF : ISS_ON);
        CoolerSP.setState(IPS_OK);
        CoolerSP.apply();
    }
    else
    {
        LOG_WARN("Failed to get cooler status.");
    }

    // Get CoolerPower
    success = sendAlpacaGET("/coolerpower", response);
    if (success)
    {
        CoolerPowerNP[0].setValue(response["Value"].get<double>());
        CoolerPowerNP.setState(IPS_OK);
        CoolerPowerNP.apply();
    }
    else
    {
        LOG_WARN("Failed to get cooler power.");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
void AlpacaCCD::updateCameraState()
{
    if (!isConnected())
        return;

    nlohmann::json response;
    if (sendAlpacaGET("/camerastate", response))
    {
        int state = response["Value"].get<int>();
        std::string previousState = CameraStateTP[0].getText();
        switch (state)
        {
            case 0: // CameraState_Idle
                CameraStateTP[0].setText("Idle");
                CameraStateTP.setState(IPS_IDLE);
                break;
            case 1: // CameraState_Waiting
                CameraStateTP[0].setText("Waiting");
                CameraStateTP.setState(IPS_BUSY);
                break;
            case 2: // CameraState_Exposing
                CameraStateTP[0].setText("Exposing");
                CameraStateTP.setState(IPS_BUSY);
                break;
            case 3: // CameraState_Reading
                CameraStateTP[0].setText("Reading");
                CameraStateTP.setState(IPS_BUSY);
                break;
            case 4: // CameraState_Download
                CameraStateTP[0].setText("Downloading");
                CameraStateTP.setState(IPS_BUSY);
                break;
            case 5: // CameraState_Error
                CameraStateTP[0].setText("Error");
                CameraStateTP.setState(IPS_ALERT);
                break;
            default:
                CameraStateTP[0].setText("Unknown");
                break;
        }
        // Only update if necessary
        if (previousState != CameraStateTP[0].getText())
        {
            CameraStateTP.apply();
        }
    }
    else
    {
        LOG_WARN("Failed to get camera state.");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool AlpacaCCD::processImageBytesData(uint8_t* buffer, size_t buffer_size, const ImageBytesMetadata& metadata)
{
    // Handle ImageBytes data format as per ASCOM Alpaca API v10 section 8.8
    // ImageBytes uses row-major ordering with specific element ordering rules

    LOGF_DEBUG("Processing ImageBytes data: %dx%d, rank=%d, transmission_type=%d",
               metadata.Dimension1, metadata.Dimension2, metadata.Rank, metadata.TransmissionElementType);

    INDI::CCDChip* primary_chip = &PrimaryCCD;
    primary_chip->setImageExtension("fits");

    uint32_t width = metadata.Dimension1;
    uint32_t height = metadata.Dimension2;
    uint32_t planes = (metadata.Rank == 3) ? metadata.Dimension3 : 1;
    size_t pixel_count = width * height * planes;

    LOGF_DEBUG("Image dimensions: %dx%d, planes=%d, pixel_count=%zu", width, height, planes, pixel_count);

    // Determine bytes per element based on transmission type
    size_t bytes_per_element;
    switch (metadata.TransmissionElementType)
    {
        case 6: // Byte
            bytes_per_element = 1;
            break;
        case 1: // Int16
        case 8: // UInt16
            bytes_per_element = 2;
            break;
        case 2: // Int32
        case 9: // UInt32
        case 4: // Single (float)
            bytes_per_element = 4;
            break;
        case 3: // Double
        case 5: // UInt64
        case 7: // Int64
            bytes_per_element = 8;
            break;
        default:
            LOGF_ERROR("Unsupported transmission element type: %d", metadata.TransmissionElementType);
            return false;
    }

    LOGF_DEBUG("Bytes per element: %zu", bytes_per_element);

    // Validate buffer size
    size_t expected_size = pixel_count * bytes_per_element;
    if (buffer_size != expected_size)
    {
        LOGF_ERROR("Buffer size mismatch: expected %zu, got %zu", expected_size, buffer_size);
        return false;
    }

    // Convert ImageBytes data to INDI format (typically 16-bit)
    size_t indi_buffer_size = width * height * sizeof(uint16_t);
    PrimaryCCD.setFrameBufferSize(indi_buffer_size);
    uint16_t* indi_buffer = reinterpret_cast<uint16_t*>(PrimaryCCD.getFrameBuffer());
    if (!indi_buffer)
    {
        LOG_ERROR("Failed to allocate INDI buffer");
        return false;
    }

    LOGF_DEBUG("Allocated INDI buffer: %zu bytes (%dx%d * 2 bytes/pixel)",
               indi_buffer_size, width, height);

    // ImageBytes uses row-major ordering as per section 8.8.1
    // For 2D arrays: rightmost dimension (height) changes most quickly
    // For 3D arrays: rightmost dimension (color plane) changes most quickly

    if (metadata.Rank == 2)
    {
        // 2D monochrome/Bayer image
        LOG_DEBUG("Converting 2D image data");
        convertImageBytesToINDI2D(buffer, indi_buffer, width, height, metadata.TransmissionElementType);
    }
    else if (metadata.Rank == 3)
    {
        // 3D color image - for now, just take the first plane or convert to grayscale
        // This is a simplified implementation; full color support would require more work
        LOGF_DEBUG("Converting 3D image data (averaging %d planes to grayscale)", planes);
        convertImageBytesToINDI3D(buffer, indi_buffer, width, height, planes, metadata.TransmissionElementType);
    }
    else
    {
        LOGF_ERROR("Unsupported image rank: %d", metadata.Rank);
        return false;
    }

    // ImageBytes data is already in the correct coordinate system (FITS standard)
    // No need to flip coordinates like we do for JSON data

    // Upload to INDI
    primary_chip->setFrame(0, 0, width, height);
    primary_chip->setFrameBuffer(reinterpret_cast<uint8_t*>(indi_buffer));
    primary_chip->setFrameBufferSize(indi_buffer_size, false);

    LOGF_DEBUG("Set INDI frame buffer: %dx%d, size=%zu bytes", width, height, indi_buffer_size);

    ExposureComplete(primary_chip);
    LOG_DEBUG("Image processing completed successfully");
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool AlpacaCCD::processMonoImage(uint8_t* buffer)
{
    // Set up INDI CCD buffer
    INDI::CCDChip* primary_chip = &PrimaryCCD;

    primary_chip->setImageExtension("fits");

    // Convert data type if needed (INDI typically expects 16-bit)
    uint16_t* indi_buffer = nullptr;
    size_t pixel_count = m_CurrentImage.width * m_CurrentImage.height;
    size_t indi_buffer_size = pixel_count * sizeof(uint16_t);
    PrimaryCCD.setFrameBufferSize(indi_buffer_size);
    indi_buffer = reinterpret_cast<uint16_t*>(PrimaryCCD.getFrameBuffer());

    switch (m_CurrentImage.type)
    {
        case 1: // 8-bit to 16-bit
            for (size_t i = 0; i < pixel_count; i++)
            {
                indi_buffer[i] = static_cast<uint16_t>(buffer[i]) << 8; // Scale to 16-bit
            }
            break;

        case 2: // 16-bit (direct copy)
            memcpy(indi_buffer, buffer, pixel_count * sizeof(uint16_t));
            break;

        case 3: // 32-bit to 16-bit (scale down)
        {
            uint32_t* src32 = reinterpret_cast<uint32_t*>(buffer);
            for (size_t i = 0; i < pixel_count; i++)
            {
                indi_buffer[i] = static_cast<uint16_t>(src32[i] >> 16); // Scale down
            }
            break;
        }
        default:
            LOGF_ERROR("Unsupported Alpaca image type: %d", m_CurrentImage.type);
            return false;
    }

    if (!indi_buffer)
    {
        LOG_ERROR("Failed to convert image data");
        return false;
    }

    // Upload to INDI
    primary_chip->setFrame(0, 0, m_CurrentImage.width, m_CurrentImage.height);
    primary_chip->setFrameBuffer(reinterpret_cast<uint8_t*>(indi_buffer)); // Cast to uint8_t*
    primary_chip->setFrameBufferSize(pixel_count * sizeof(uint16_t), false);

    ExposureComplete(primary_chip);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
bool AlpacaCCD::processColorImage(uint8_t* /*buffer*/)
{
    // Placeholder for color image processing
    LOG_WARN("Color image processing not yet implemented.");
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
void AlpacaCCD::translateCoordinates(uint8_t* buffer, const ImageMetadata& meta)
{
    // ASCOM: (0,0) = top-left, row-major memory
    // INDI: (0,0) = bottom-left, expects FITS standard

    size_t bytes_per_pixel = (meta.type == 1) ? 1 : (meta.type == 2) ? 2 : (meta.type == 3) ? 4 : 8;
    size_t row_size = meta.width * bytes_per_pixel;

    // For multi-plane images, flip each plane
    uint32_t planes = (meta.planes > 0) ? meta.planes : 1;

    for (uint32_t plane = 0; plane < planes; plane++)
    {
        uint8_t* plane_start = buffer + (plane * meta.width * meta.height * bytes_per_pixel);

        // Flip image vertically (reverse row order)
        for (uint32_t y = 0; y < meta.height / 2; y++)
        {
            uint8_t* top_row = plane_start + (y * row_size);
            uint8_t* bottom_row = plane_start + ((meta.height - 1 - y) * row_size);

            // Swap rows
            for (size_t i = 0; i < row_size; i++)
            {
                uint8_t temp = top_row[i];
                top_row[i] = bottom_row[i];
                bottom_row[i] = temp;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
void AlpacaCCD::temperatureTimerTimeout()
{
    if (!isConnected())
        return;

    nlohmann::json response;
    bool success;
    IPState newState = TemperatureNP.getState();

    // Get current temperature
    success = sendAlpacaGET("/ccdtemperature", response);
    if (success)
    {
        m_CurrentTemperature = response["Value"].get<double>();

        // If cooling is active and target temperature is set, show goal status
        if (TemperatureNP.getState() == IPS_BUSY && !std::isnan(m_TargetTemperature)
                && std::abs(m_CurrentTemperature - m_TargetTemperature) <= TEMP_THRESHOLD)
            newState = IPS_OK;
    }
    else
    {
        LOG_WARN("Failed to get temperature from Alpaca camera.");
        newState = IPS_ALERT;
    }

    // Update if there is a change
    if (std::abs(m_CurrentTemperature - TemperatureNP[0].getValue()) > 0.05 ||
            TemperatureNP.getState() != newState)
    {
        TemperatureNP.setState(newState);
        TemperatureNP[0].setValue(m_CurrentTemperature);
        TemperatureNP.apply();
    }

    if (HasCooler())
        updateCoolerStatus();
}

////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////
void AlpacaCCD::convertImageBytesToINDI2D(uint8_t* src_buffer, uint16_t* dst_buffer, uint32_t width, uint32_t height,
        int32_t transmission_type)
{
    // Convert ImageBytes 2D data to INDI 16-bit format
    // ImageBytes uses row-major ordering as per ASCOM Alpaca API v10 section 8.8.1
    // For 2D arrays: rightmost dimension (height) changes most quickly

    size_t dst_index = 0;

    switch (transmission_type)
    {
        case 6: // Byte (8-bit)
        {
            uint8_t* src = src_buffer;
            for (uint32_t x = 0; x < width; x++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    // Scale 8-bit to 16-bit
                    dst_buffer[dst_index++] = static_cast<uint16_t>(src[x * height + y]) << 8;
                }
            }
            break;
        }
        case 1: // Int16
        {
            int16_t* src = reinterpret_cast<int16_t*>(src_buffer);
            for (uint32_t x = 0; x < width; x++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    // Convert signed to unsigned, handle negative values
                    int16_t val = src[x * height + y];
                    dst_buffer[dst_index++] = static_cast<uint16_t>(val + 32768);
                }
            }
            break;
        }
        case 8: // UInt16
        {
            uint16_t* src = reinterpret_cast<uint16_t*>(src_buffer);
            for (uint32_t x = 0; x < width; x++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    // Direct copy
                    dst_buffer[dst_index++] = src[x * height + y];
                }
            }
            break;
        }
        case 2: // Int32
        {
            int32_t* src = reinterpret_cast<int32_t*>(src_buffer);
            for (uint32_t x = 0; x < width; x++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    // Scale down from 32-bit to 16-bit
                    int32_t val = src[x * height + y];
                    dst_buffer[dst_index++] = static_cast<uint16_t>(val);
                }
            }
            break;
        }
        case 9: // UInt32
        {
            uint32_t* src = reinterpret_cast<uint32_t*>(src_buffer);
            for (uint32_t x = 0; x < width; x++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    // Scale down from 32-bit to 16-bit
                    uint32_t val = src[x * height + y];
                    dst_buffer[dst_index++] = static_cast<uint16_t>(val);
                }
            }
            break;
        }
        case 4: // Single (float)
        {
            float* src = reinterpret_cast<float*>(src_buffer);
            for (uint32_t x = 0; x < width; x++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    // Convert float to 16-bit, assuming 0.0-1.0 range
                    float val = src[x * height + y];
                    dst_buffer[dst_index++] = static_cast<uint16_t>(val * 65535.0f);
                }
            }
            break;
        }
        default:
            LOGF_ERROR("Unsupported transmission type for 2D conversion: %d", transmission_type);
            // Fill with zeros as fallback
            memset(dst_buffer, 0, width * height * sizeof(uint16_t));
            break;
    }
}

void AlpacaCCD::convertImageBytesToINDI3D(uint8_t* src_buffer, uint16_t* dst_buffer, uint32_t width, uint32_t height,
        uint32_t planes,
        int32_t transmission_type)
{
    // Convert ImageBytes 3D data to INDI 16-bit format
    // ImageBytes uses row-major ordering as per ASCOM Alpaca API v10 section 8.8.1
    // For 3D arrays: rightmost dimension (color plane) changes most quickly

    // For now, we'll convert 3D color data to grayscale by averaging the planes
    // A full color implementation would require more complex handling

    size_t dst_index = 0;

    switch (transmission_type)
    {
        case 6: // Byte (8-bit)
        {
            uint8_t* src = src_buffer;
            for (uint32_t x = 0; x < width; x++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    uint32_t sum = 0;
                    for (uint32_t p = 0; p < planes; p++)
                    {
                        sum += src[(x * height + y) * planes + p];
                    }
                    // Average the planes and scale to 16-bit
                    uint16_t avg = static_cast<uint16_t>((sum / planes)) << 8;
                    dst_buffer[dst_index++] = avg;
                }
            }
            break;
        }
        case 1: // Int16
        {
            int16_t* src = reinterpret_cast<int16_t*>(src_buffer);
            for (uint32_t x = 0; x < width; x++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    int32_t sum = 0;
                    for (uint32_t p = 0; p < planes; p++)
                    {
                        sum += src[(x * height + y) * planes + p];
                    }
                    // Average and convert to unsigned
                    int16_t avg = static_cast<int16_t>(sum / planes);
                    dst_buffer[dst_index++] = static_cast<uint16_t>(avg + 32768);
                }
            }
            break;
        }
        case 8: // UInt16
        {
            uint16_t* src = reinterpret_cast<uint16_t*>(src_buffer);
            for (uint32_t x = 0; x < width; x++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    uint32_t sum = 0;
                    for (uint32_t p = 0; p < planes; p++)
                    {
                        sum += src[(x * height + y) * planes + p];
                    }
                    // Average the planes
                    dst_buffer[dst_index++] = static_cast<uint16_t>(sum / planes);
                }
            }
            break;
        }
        case 2: // Int32
        {
            int32_t* src = reinterpret_cast<int32_t*>(src_buffer);
            for (uint32_t x = 0; x < width; x++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    int64_t sum = 0;
                    for (uint32_t p = 0; p < planes; p++)
                    {
                        sum += src[(x * height + y) * planes + p];
                    }
                    // Average and scale down
                    int32_t avg = static_cast<int32_t>(sum / planes);
                    dst_buffer[dst_index++] = static_cast<uint16_t>(avg >> 16);
                }
            }
            break;
        }
        case 9: // UInt32
        {
            uint32_t* src = reinterpret_cast<uint32_t*>(src_buffer);
            for (uint32_t x = 0; x < width; x++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    uint64_t sum = 0;
                    for (uint32_t p = 0; p < planes; p++)
                    {
                        sum += src[(x * height + y) * planes + p];
                    }
                    // Average and scale down
                    uint32_t avg = static_cast<uint32_t>(sum / planes);
                    dst_buffer[dst_index++] = static_cast<uint16_t>(avg >> 16);
                }
            }
            break;
        }
        case 4: // Single (float)
        {
            float* src = reinterpret_cast<float*>(src_buffer);
            for (uint32_t x = 0; x < width; x++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    float sum = 0.0f;
                    for (uint32_t p = 0; p < planes; p++)
                    {
                        sum += src[(x * height + y) * planes + p];
                    }
                    // Average and convert to 16-bit
                    float avg = sum / planes;
                    dst_buffer[dst_index++] = static_cast<uint16_t>(avg * 65535.0f);
                }
            }
            break;
        }
        default:
            LOGF_ERROR("Unsupported transmission type for 3D conversion: %d", transmission_type);
            // Fill with zeros as fallback
            memset(dst_buffer, 0, width * height * sizeof(uint16_t));
            break;
    }
}

void AlpacaCCD::addFITSKeywords(INDI::CCDChip * targetChip, std::vector<INDI::FITSRecord> &fitsKeywords)
{
    // Call base class to add standard INDI FITS keywords and custom keywords
    INDI::CCD::addFITSKeywords(targetChip, fitsKeywords);

    // Add Alpaca-specific FITS keywords that are not covered by the base class or provide more precise information.

    // MAXADU (Unique to Alpaca)
    fitsKeywords.push_back({"MAXADU", static_cast<int>(m_CurrentImage.max_adu), "Maximum ADU value"});

    // Color/Bayer info (more specific than generic INDI BayerTP)
    switch (m_CurrentImage.sensor_type)
    {
        case 0: // Monochrome
            fitsKeywords.push_back({"COLORTYP", "MONOCHROME", "Sensor color type"});
            break;
        case 1: // Color
            fitsKeywords.push_back({"COLORTYP", "COLOR", "Sensor color type"});
            break;
        case 2: // RGGB Bayer
            fitsKeywords.push_back({"COLORTYP", "RGGB", "Bayer matrix pattern"});
            fitsKeywords.push_back({"XBAYROFF", static_cast<int>(m_CurrentImage.bayer_offset_x), "Bayer X offset"});
            fitsKeywords.push_back({"YBAYROFF", static_cast<int>(m_CurrentImage.bayer_offset_y), "Bayer Y offset"});
            break;
        case 3: // CMYG
            fitsKeywords.push_back({"COLORTYP", "CMYG", "Bayer matrix pattern"});
            fitsKeywords.push_back({"XBAYROFF", static_cast<int>(m_CurrentImage.bayer_offset_x), "Bayer X offset"});
            fitsKeywords.push_back({"YBAYROFF", static_cast<int>(m_CurrentImage.bayer_offset_y), "Bayer Y offset"});
            break;
        default:
            LOGF_WARN("Unknown sensor type: %d", m_CurrentImage.sensor_type);
            break;
    }

    // Alpaca-specific identifiers
    fitsKeywords.push_back({"ALPACA", "TRUE", "Image from ASCOM Alpaca camera"});
    fitsKeywords.push_back({"ALPTYPE", static_cast<int>(m_CurrentImage.type), "Alpaca data type code"});

    // Gain and Offset (if supported by the camera and not already added by base class)
    if (m_HasGain)
    {
        fitsKeywords.push_back({"GAIN", GainNP[0].getValue(), 3, "Camera Gain setting"});
    }
    if (m_HasOffset)
    {
        fitsKeywords.push_back({"OFFSET", OffsetNP[0].getValue(), 3, "Camera Offset setting"});
    }
}
