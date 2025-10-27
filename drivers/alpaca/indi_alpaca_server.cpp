/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Alpaca Server

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

#include "indi_alpaca_server.h"
#include "alpaca_client.h"
#include "device_manager.h"
#include "alpaca_discovery.h"
#include "indilogger.h"
#include "inditimer.h"

#include <memory>
#include <thread>
#include <chrono>

// We declare an auto pointer to INDIAlpacaServer
std::unique_ptr<INDIAlpacaServer> indiAlpacaServer(new INDIAlpacaServer());

INDIAlpacaServer::INDIAlpacaServer()
{
    setVersion(1, 0);
}

INDIAlpacaServer::~INDIAlpacaServer()
{
    // Stop the server if it's running
    if (m_ServerRunning)
        stopAlpacaServer();
}

const char *INDIAlpacaServer::getDefaultName()
{
    return "INDI Alpaca Server";
}

bool INDIAlpacaServer::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Server settings
    ServerSettingsTP[0].fill("HOST", "Host", "0.0.0.0");
    ServerSettingsTP[1].fill("PORT", "Port", "11111");
    ServerSettingsTP.fill(getDeviceName(), "SERVER_SETTINGS", "Server", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // INDI server settings
    INDIServerSettingsTP[0].fill("HOST", "Host", "localhost");
    INDIServerSettingsTP[1].fill("PORT", "Port", "7624");
    INDIServerSettingsTP.fill(getDeviceName(), "INDI_SERVER_SETTINGS", "INDI Server", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // Server control
    ServerControlSP[0].fill("START", "Start", ISS_OFF);
    ServerControlSP[1].fill("STOP", "Stop", ISS_OFF);
    ServerControlSP.fill(getDeviceName(), "SERVER_CONTROL", "Control", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Connection settings
    ConnectionSettingsNP[0].fill("TIMEOUT", "Timeout (sec)", "%.0f", 1, 30, 1, 5);
    ConnectionSettingsNP[1].fill("RETRIES", "Max Retries", "%.0f", 1, 10, 1, 3);
    ConnectionSettingsNP[2].fill("RETRY_DELAY", "Retry Delay (ms)", "%.0f", 100, 5000, 100, 1000);
    ConnectionSettingsNP.fill(getDeviceName(), "CONNECTION_SETTINGS", "Connection", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    // Discovery settings
    DiscoverySettingsNP[0].fill("PORT", "Discovery Port", "%.0f", 1, 65535, 1, 32227);
    DiscoverySettingsNP.fill(getDeviceName(), "DISCOVERY_SETTINGS", "Discovery", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // Startup delay settings
    StartupDelayNP[0].fill("DELAY", "Startup Delay (sec)", "%.0f", 1, 60, 1, 3);
    StartupDelayNP.fill(getDeviceName(), "STARTUP_DELAY", "Startup Delay", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // Get device manager singleton
    m_DeviceManager = DeviceManager::getInstance();

    // Create INDI client
    m_Client = std::make_shared<AlpacaClient>(m_DeviceManager);

    // Set the AlpacaClient in the DeviceManager
    m_DeviceManager->setAlpacaClient(m_Client);

    addAuxControls();

    return true;
}

void INDIAlpacaServer::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineProperty(ServerSettingsTP);
    defineProperty(INDIServerSettingsTP);
    defineProperty(ServerControlSP);
    defineProperty(ConnectionSettingsNP);
    defineProperty(DiscoverySettingsNP);
    defineProperty(StartupDelayNP);
}

bool INDIAlpacaServer::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Nothing to do here
    }
    else
    {
        // Nothing to do here
    }

    return true;
}

bool INDIAlpacaServer::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Server settings
        if (ServerSettingsTP.isNameMatch(name))
        {
            ServerSettingsTP.update(texts, names, n);
            ServerSettingsTP.setState(IPS_OK);
            ServerSettingsTP.apply();
            saveConfig(true, ServerSettingsTP.getName());
            return true;
        }

        // INDI server settings
        if (INDIServerSettingsTP.isNameMatch(name))
        {
            INDIServerSettingsTP.update(texts, names, n);
            INDIServerSettingsTP.setState(IPS_OK);
            INDIServerSettingsTP.apply();
            saveConfig(true, INDIServerSettingsTP.getName());
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool INDIAlpacaServer::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Connection settings
        if (ConnectionSettingsNP.isNameMatch(name))
        {
            ConnectionSettingsNP.update(values, names, n);
            ConnectionSettingsNP.setState(IPS_OK);
            ConnectionSettingsNP.apply();
            saveConfig(true, ConnectionSettingsNP.getName());
            return true;
        }

        // Discovery settings
        if (DiscoverySettingsNP.isNameMatch(name))
        {
            DiscoverySettingsNP.update(values, names, n);
            DiscoverySettingsNP.setState(IPS_OK);
            DiscoverySettingsNP.apply();

            // Update discovery port if discovery server is running
            if (m_Discovery && m_Discovery->isRunning())
            {
                m_Discovery->setDiscoveryPort(static_cast<int>(DiscoverySettingsNP[0].getValue()));
                LOGF_INFO("Discovery port updated to %d", static_cast<int>(DiscoverySettingsNP[0].getValue()));
            }

            saveConfig(DiscoverySettingsNP);
            return true;
        }

        // Startup delay settings
        if (StartupDelayNP.isNameMatch(name))
        {
            StartupDelayNP.update(values, names, n);
            StartupDelayNP.setState(IPS_OK);
            StartupDelayNP.apply();
            LOGF_INFO("Startup delay updated to %d seconds", static_cast<int>(StartupDelayNP[0].getValue()));
            saveConfig(StartupDelayNP);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool INDIAlpacaServer::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Server control
        if (ServerControlSP.isNameMatch(name))
        {
            ServerControlSP.update(states, names, n);
            int index = ServerControlSP.findOnSwitchIndex();

            switch (index)
            {
                case 0: // Start
                    if (!m_ServerRunning)
                    {
                        if (startAlpacaServer())
                        {
                            ServerControlSP[0].setState(ISS_ON);
                            ServerControlSP[1].setState(ISS_OFF);
                            ServerControlSP.setState(IPS_OK);
                            LOG_INFO("Alpaca server started");
                        }
                        else
                        {
                            ServerControlSP[0].setState(ISS_OFF);
                            ServerControlSP[1].setState(ISS_OFF);
                            ServerControlSP.setState(IPS_ALERT);
                            LOG_ERROR("Failed to start Alpaca server");
                        }
                    }
                    else
                    {
                        LOG_INFO("Alpaca server is already running");
                        ServerControlSP[0].setState(ISS_ON);
                        ServerControlSP[1].setState(ISS_OFF);
                        ServerControlSP.setState(IPS_OK);
                    }
                    break;

                case 1: // Stop
                    if (m_ServerRunning)
                    {
                        if (stopAlpacaServer())
                        {
                            ServerControlSP[0].setState(ISS_OFF);
                            ServerControlSP[1].setState(ISS_OFF);
                            ServerControlSP.setState(IPS_IDLE);
                            LOG_INFO("Alpaca server stopped");
                        }
                        else
                        {
                            ServerControlSP[0].setState(ISS_ON);
                            ServerControlSP[1].setState(ISS_OFF);
                            ServerControlSP.setState(IPS_ALERT);
                            LOG_ERROR("Failed to stop Alpaca server");
                        }
                    }
                    else
                    {
                        LOG_INFO("Alpaca server is not running");
                        ServerControlSP[0].setState(ISS_OFF);
                        ServerControlSP[1].setState(ISS_OFF);
                        ServerControlSP.setState(IPS_IDLE);
                    }
                    break;
            }

            ServerControlSP.apply();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool INDIAlpacaServer::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    ServerSettingsTP.save(fp);
    INDIServerSettingsTP.save(fp);
    ConnectionSettingsNP.save(fp);
    DiscoverySettingsNP.save(fp);
    StartupDelayNP.save(fp);

    return true;
}

bool INDIAlpacaServer::Connect()
{
    // Get the startup delay
    int startupDelay = static_cast<int>(StartupDelayNP[0].getValue());
    LOGF_INFO("Waiting %d seconds before connecting to INDI server...", startupDelay);

    // Use a single-shot timer to delay the connection
    INDI::Timer::singleShot(startupDelay * 1000, [this]()
    {
        // Connect to INDI server
        m_Client->setServer(INDIServerSettingsTP[0].getText(), std::stoi(INDIServerSettingsTP[1].getText()));

        if (m_Client->connectServer())
        {
            LOG_INFO("Connected to INDI server");

            // Automatically start the Alpaca server when the driver is connected
            if (!m_ServerRunning)
            {
                if (startAlpacaServer())
                {
                    ServerControlSP[0].setState(ISS_ON);
                    ServerControlSP[1].setState(ISS_OFF);
                    ServerControlSP.setState(IPS_OK);
                    ServerControlSP.apply();
                    LOG_INFO("Alpaca server started automatically");
                }
                else
                {
                    LOG_ERROR("Failed to start Alpaca server automatically");
                }
            }

            SetTimer(getCurrentPollingPeriod());
            setConnected(true);
            updateProperties();
        }
        else
        {
            LOG_ERROR("Failed to connect to INDI server");
            setConnected(false);
            updateProperties();
        }
    });

    return true;
}

bool INDIAlpacaServer::Disconnect()
{
    // Stop Alpaca server if running
    if (m_ServerRunning)
        stopAlpacaServer();

    // Disconnect from INDI server
    if (m_Client->disconnectServer())
    {
        LOG_INFO("Disconnected from INDI server");
        return true;
    }
    else
    {
        LOG_ERROR("Failed to disconnect from INDI server");
        return false;
    }
}

void INDIAlpacaServer::TimerHit()
{
    // Nothing to do here for now
    SetTimer(getCurrentPollingPeriod());
}

bool INDIAlpacaServer::startAlpacaServer()
{
    if (m_ServerRunning)
    {
        LOG_INFO("Alpaca server is already running");
        return true;
    }

    // Create HTTP server
    m_Server = std::make_unique<httplib::Server>();

    // Create discovery server
    int discoveryPort = static_cast<int>(DiscoverySettingsNP[0].getValue());
    int alpacaPort = std::stoi(ServerSettingsTP[1].getText());
    m_Discovery = std::make_unique<AlpacaDiscovery>(discoveryPort, alpacaPort);

    // Set up routes
    m_Server->Get("/management/(.*)", [this](const httplib::Request & req, httplib::Response & res)
    {
        m_DeviceManager->handleAlpacaRequest(req, res);
    });

    m_Server->Get("/api/v1/(.*)", [this](const httplib::Request & req, httplib::Response & res)
    {
        m_DeviceManager->handleAlpacaRequest(req, res);
    });

    m_Server->Put("/api/v1/(.*)", [this](const httplib::Request & req, httplib::Response & res)
    {
        m_DeviceManager->handleAlpacaRequest(req, res);
    });

    // Setup API
    m_Server->Get("/setup/v1/(.*)", [this](const httplib::Request & req, httplib::Response & res)
    {
        m_DeviceManager->handleSetupRequest(req, res);
    });

    // Start server thread
    m_ServerThread = std::thread(&INDIAlpacaServer::serverThreadFunc, this);

    // Wait a bit to make sure server starts
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Start discovery server
    if (!m_Discovery->start())
    {
        LOG_ERROR("Failed to start Alpaca discovery server");
        // Continue anyway, as the HTTP server is more important
    }
    else
    {
        LOGF_INFO("Alpaca discovery server started on port %d", discoveryPort);
    }

    m_ServerRunning = true;
    return true;
}

bool INDIAlpacaServer::stopAlpacaServer()
{
    if (!m_ServerRunning)
    {
        LOG_INFO("Alpaca server is not running");
        return true;
    }

    // Stop discovery server
    if (m_Discovery)
    {
        if (m_Discovery->isRunning())
        {
            if (!m_Discovery->stop())
            {
                LOG_ERROR("Failed to stop Alpaca discovery server");
                // Continue anyway to stop the HTTP server
            }
            else
            {
                LOG_INFO("Alpaca discovery server stopped");
            }
        }
        m_Discovery.reset();
    }

    // Stop HTTP server
    if (m_Server)
    {
        m_Server->stop();

        // Wait for thread to finish
        if (m_ServerThread.joinable())
            m_ServerThread.join();

        m_Server.reset();
    }

    m_ServerRunning = false;
    return true;
}

void INDIAlpacaServer::serverThreadFunc()
{
    LOG_INFO("Starting Alpaca server thread");

    // Get host and port
    std::string host = ServerSettingsTP[0].getText();
    int port = std::stoi(ServerSettingsTP[1].getText());

    LOGF_INFO("Alpaca server listening on %s:%d", host.c_str(), port);

    // Start server
    m_Server->listen(host.c_str(), port);

    LOG_INFO("Alpaca server thread stopped");
}
