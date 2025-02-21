/*
    IPX800 Controller
    Copyright (C) 2025 Jasem Mutlaq

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

#include "ipx800v4.h"
#include "defaultdevice.h"
#include "connectionplugins/connectiontcp.h"
#include "indipropertyswitch.h"
#include <memory>
#include <httplib.h>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

using json = nlohmann::json;

static std::unique_ptr<IPX800> device(new IPX800());

IPX800::IPX800() : InputInterface(this), OutputInterface(this)
{
    setVersion(1, 0);
}

bool IPX800::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Initialize Input Interface for 8 digital inputs and 4 analog inputs
    InputInterface::initProperties("Main Control", DIGITAL_INPUTS, ANALOG_INPUTS, "Input", "Analog");

    // Initialize Output Interface for 8 digital outputs
    OutputInterface::initProperties("Main Control", DIGITAL_OUTPUTS, "Output");

    setDriverInterface(AUX_INTERFACE | OUTPUT_INTERFACE | INPUT_INTERFACE);

    addAuxControls();

    // Set up TCP connection
    tcpConnection = new Connection::TCP(this);
    tcpConnection->setDefaultHost("192.168.1.100");  // Default IPX800 IP
    tcpConnection->setDefaultPort(80);  // HTTP port
    tcpConnection->registerHandshake([&]()
    {
        return Handshake();
    });

    registerConnection(tcpConnection);

    // API Key
    APIKeyTP[0].fill("API_KEY", "API Key", "");
    APIKeyTP.fill(getDeviceName(), "API_KEY", "API Settings", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    APIKeyTP.load();

    // Model version
    ModelVersionTP[0].fill("VERSION", "Version", "");
    ModelVersionTP.fill(getDeviceName(), "MODEL", "Model", "Main Control", IP_RO, 60, IPS_IDLE);

    setDefaultPollingPeriod(1000);

    return true;
}

bool IPX800::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    InputInterface::updateProperties();
    OutputInterface::updateProperties();

    if (isConnected())
    {
        defineProperty(APIKeyTP);
        defineProperty(ModelVersionTP);
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(APIKeyTP);
        deleteProperty(ModelVersionTP);
    }

    return true;
}

const char *IPX800::getDefaultName()
{
    return "IPX800";
}

bool IPX800::Handshake()
{
    if (strlen(APIKeyTP[0].getText()) == 0)
    {
        LOG_ERROR("API Key is not set");
        return false;
    }

    httplib::Client cli(tcpConnection->host(), tcpConnection->port());

    // Get relay status as a simple test
    std::string endpoint = "/api/xdevices.json";
    endpoint += "?key=" + std::string(APIKeyTP[0].getText());
    endpoint += "&Get=D";
    auto result = cli.Get(endpoint);
    if (!result)
    {
        LOG_ERROR("Failed to connect to device");
        return false;
    }

    try
    {
        auto j = json::parse(result->body);
        if (j.contains("product"))
        {
            std::string product = j["product"].get<std::string>();
            // Extract version (e.g. "IPX800_V3" -> "V3")
            size_t pos = product.find("_");
            if (pos != std::string::npos)
            {
                ModelVersionTP[0].setText(product.substr(pos + 1).c_str());
                ModelVersionTP.setState(IPS_OK);
            }
        }
        return true;
    }
    catch (json::parse_error &e)
    {
        LOGF_ERROR("JSON parse error: %s", e.what());
    }

    return false;
}

void IPX800::TimerHit()
{
    if (!isConnected())
        return;

    UpdateDigitalInputs();
    UpdateAnalogInputs();
    UpdateDigitalOutputs();

    SetTimer(getCurrentPollingPeriod());
}

bool IPX800::UpdateDigitalInputs()
{
    httplib::Client cli(tcpConnection->host(), tcpConnection->port());

    std::string endpoint = "/api/xdevices.json";
    endpoint += "?key=" + std::string(APIKeyTP[0].getText());
    endpoint += "&Get=D";
    auto result = cli.Get(endpoint);
    if (!result)
    {
        LOG_ERROR("Failed to get digital inputs");
        return false;
    }

    try
    {
        auto j = json::parse(result->body);

        // Parse digital inputs
        auto inputs = j.get<std::vector<int>>();
        for (size_t i = 0; i < DIGITAL_INPUTS && i < inputs.size(); i++)
        {
            auto state = inputs[i] ? ISS_ON : ISS_OFF;
            if (DigitalInputsSP[i].findOnSwitchIndex() != state)
            {
                DigitalInputsSP[i].reset();
                DigitalInputsSP[i][state].setState(ISS_ON);
                DigitalInputsSP[i].setState(IPS_OK);
                DigitalInputsSP[i].apply();
            }
        }
        return true;
    }
    catch (json::parse_error &e)
    {
        LOGF_ERROR("JSON parse error: %s", e.what());
    }

    return false;
}

bool IPX800::UpdateAnalogInputs()
{
    httplib::Client cli(tcpConnection->host(), tcpConnection->port());

    std::string endpoint = "/api/xdevices.json";
    endpoint += "?key=" + std::string(APIKeyTP[0].getText());
    endpoint += "&Get=A";
    auto result = cli.Get(endpoint);
    if (!result)
    {
        LOG_ERROR("Failed to get analog inputs");
        return false;
    }

    try
    {
        auto j = json::parse(result->body);

        // Parse analog inputs
        for (int i = 0; i < ANALOG_INPUTS; i++)
        {
            std::string key = "AN" + std::to_string(i + 1);
            if (j.contains(key))
            {
                double value = j[key].get<double>();
                if (AnalogInputsNP[i][0].getValue() != value)
                {
                    AnalogInputsNP[i][0].setValue(value);
                    AnalogInputsNP[i].setState(IPS_OK);
                    AnalogInputsNP[i].apply();
                }
            }
        }
        return true;
    }
    catch (json::parse_error &e)
    {
        LOGF_ERROR("JSON parse error: %s", e.what());
    }

    return false;
}

bool IPX800::UpdateDigitalOutputs()
{
    httplib::Client cli(tcpConnection->host(), tcpConnection->port());

    std::string endpoint = "/api/xdevices.json";
    endpoint += "?key=" + std::string(APIKeyTP[0].getText());
    endpoint += "&Get=R";
    auto result = cli.Get(endpoint);
    if (!result)
    {
        LOG_ERROR("Failed to get digital outputs");
        return false;
    }

    try
    {
        auto j = json::parse(result->body);

        // Parse digital outputs
        auto outputs = j.get<std::vector<int>>();
        for (size_t i = 0; i < DIGITAL_OUTPUTS && i < outputs.size(); i++)
        {
            auto state = outputs[i] ? ISS_ON : ISS_OFF;
            if (DigitalOutputsSP[i].findOnSwitchIndex() != state)
            {
                DigitalOutputsSP[i].reset();
                DigitalOutputsSP[i][state].setState(ISS_ON);
                DigitalOutputsSP[i].setState(IPS_OK);
                DigitalOutputsSP[i].apply();
            }
        }
        return true;
    }
    catch (json::parse_error &e)
    {
        LOGF_ERROR("JSON parse error: %s", e.what());
    }

    return false;
}

bool IPX800::CommandOutput(uint32_t index, OutputState command)
{
    httplib::Client cli(tcpConnection->host(), tcpConnection->port());

    // IPX800 uses 1-based indexing for outputs
    std::string endpoint = "/api/xdevices.json";
    endpoint += "?key=" + std::string(APIKeyTP[0].getText());
    endpoint += "&" + std::string(command == OutputState::On ? "SetR=" : "ClearR=") + std::to_string(index + 1);

    auto result = cli.Get(endpoint);
    if (!result)
    {
        LOGF_ERROR("Failed to set output %d", index + 1);
        return false;
    }

    return result->status == 200;
}

bool IPX800::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (APIKeyTP.isNameMatch(name))
        {
            APIKeyTP.update(texts, names, n);
            APIKeyTP.setState(IPS_OK);
            APIKeyTP.apply();
            saveConfig(APIKeyTP);
            return true;
        }
    }

    if (InputInterface::processText(dev, name, texts, names, n))
        return true;
    if (OutputInterface::processText(dev, name, texts, names, n))
        return true;

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool IPX800::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (OutputInterface::processSwitch(dev, name, states, names, n))
        return true;

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool IPX800::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (OutputInterface::processNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool IPX800::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    InputInterface::saveConfigItems(fp);
    OutputInterface::saveConfigItems(fp);
    APIKeyTP.save(fp);
    return true;
}
