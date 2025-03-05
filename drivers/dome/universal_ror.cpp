/*******************************************************************************
 Universal Roll Off Roof driver.
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "universal_ror.h"
#include <memory>
#include <functional>
#include <regex>

// We declare an auto pointer to UniversalROR.
std::unique_ptr<UniversalROR> ror(new UniversalROR());

UniversalROR::UniversalROR()
{
    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool UniversalROR::initProperties()
{
    INDI::Dome::initProperties();
    SetParkDataType(PARK_NONE);
    addAuxControls();

    // Input Indexes
    InputTP[FullyOpened].fill("FULLY_OPENED", "Fully Opened", "Comma separated indexes");
    InputTP[FullyClosed].fill("FULLY_CLOSED", "Fully Closed", "Comma separated indexes");
    InputTP.fill(getDeviceName(), "INPUT_INDEX", "Input Indexes", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    InputTP.load();

    // Limit Switch Indicators
    LimitSwitchLP[FullyOpened].fill("FULLY_OPENED", "Fully Opened", IPS_IDLE);
    LimitSwitchLP[FullyClosed].fill("FULLY_CLOSED", "Fully Closed", IPS_IDLE);
    LimitSwitchLP.fill(getDeviceName(), "LIMIT_SWITCHES", "Limit Switches", MAIN_CONTROL_TAB, IPS_IDLE);

    // Output Indexes
    OutputTP[OpenRoof].fill("OPEN_ROOF", "Open Roof", "Comma separated indexes");
    OutputTP[CloseRoof].fill("CLOSE_ROOF", "Close Roof", "Comma separated indexes");
    OutputTP.fill(getDeviceName(), "OUTPUT_INDEX", "Output Indexes", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    OutputTP.load();

    return true;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool UniversalROR::ISSnoopDevice(XMLEle *root)
{
    return INDI::Dome::ISSnoopDevice(root);
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool UniversalROR::setupParms()
{
    // If we have parking data
    InitPark();

    // If limit switches are not identical then we have a known
    // parking state that we should set.
    if (m_FullClosedLimitSwitch != m_FullOpenLimitSwitch)
    {
        if (m_FullClosedLimitSwitch && !isParked())
            SetParked(true);
        else if (m_FullOpenLimitSwitch && isParked())
            SetParked(false);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
// Connection status monitoring function
void UniversalROR::checkConnectionStatus()
{
    if (!m_Client)
        return;

    if (!m_Client->isConnected())
    {
        m_ConnectionAttempts++;
        if (m_ConnectionAttempts >= MAX_CONNECTION_ATTEMPTS)
        {
            LOG_ERROR("Devices did not connect within the timeout period. Please check your configuration.");
            m_ConnectionAttempts = 0;
            // Stop the connection timer
            m_ConnectionTimer.stop();
        }
        else
        {
            // Continue checking every 5 seconds
            m_ConnectionTimer.start(5000);
        }
    }
    else
    {
        // We're connected, stop the timer
        m_ConnectionAttempts = 0;
        m_ConnectionTimer.stop();
    }
}

bool UniversalROR::Connect()
{
    // Reset connection attempts counter
    m_ConnectionAttempts = 0;

    // Check if client is initialized and connected
    if (!m_Client || !m_Client->isConnected())
    {
        // If the client is already initialized, let's connect server again
        if (m_Client)
            m_Client->connectServer();
        // Otherwise need to initialize the client again completely
        else
            ActiveDevicesUpdated();

        // Check again if that worked.
        if (!m_Client)
        {
            LOG_ERROR("ROR Client is not initialized. Specify the input and output drivers in Options tab.");
            return false;
        }

        // If client is initialized but not connected, we'll continue anyway
        // The connection callback will handle syncing indexes when devices connect
        if (!m_Client->isConnected())
        {
            LOG_INFO("ROR Client initialized but devices not connected yet. Will sync when devices connect.");

            // Start a separate timer to monitor connection status
            m_ConnectionTimer.stop(); // Stop any existing timer
            m_ConnectionTimer.callOnTimeout(std::bind(&UniversalROR::checkConnectionStatus, this));
            m_ConnectionTimer.start(5000);
            return true;
        }
    }

    // Only if all three conditions are met (client connected, input device connected, output device connected)
    // which is what m_Client->isConnected() checks, then we can sync indexes
    if (m_Client->isConnected())
    {
        LOG_INFO("All devices connected. Syncing indexes...");
        syncIndexes();
        SetTimer(getPollingPeriod());
    }
    else
    {
        LOG_INFO("Waiting for devices to connect...");
        // Don't set the regular timer yet - it will be set when devices connect
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool UniversalROR::Disconnect()
{
    // Stop the connection timer if it's running
    m_ConnectionTimer.stop();

    m_InputFullyOpened.clear();
    m_InputFullyClosed.clear();
    return true;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
const char *UniversalROR::getDefaultName()
{
    return "Universal ROR";
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool UniversalROR::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool UniversalROR::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Input Indexes
        if (InputTP.isNameMatch(name))
        {
            InputTP.update(texts, names, n);
            InputTP.setState(IPS_OK);
            InputTP.apply();
            saveConfig(InputTP);

            // Only sync indexes if all devices are connected
            if (m_Client && m_Client->isConnected())
                syncIndexes();
            else
                LOG_INFO("Indexes updated. Will sync when devices connect.");

            return true;
        }

        // Output Indexes
        if (OutputTP.isNameMatch(name))
        {
            OutputTP.update(texts, names, n);
            OutputTP.setState(IPS_OK);
            OutputTP.apply();
            saveConfig(OutputTP);

            // Only sync indexes if all devices are connected
            if (m_Client && m_Client->isConnected())
                syncIndexes();
            else
                LOG_INFO("Indexes updated. Will sync when devices connect.");

            return true;
        }
    }

    return INDI::Dome::ISNewText(dev, name, texts, names, n);
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool UniversalROR::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        setupParms();

        defineProperty(InputTP);
        defineProperty(OutputTP);
        defineProperty(LimitSwitchLP);
    }
    else
    {
        deleteProperty(InputTP);
        deleteProperty(OutputTP);
        deleteProperty(LimitSwitchLP);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void UniversalROR::TimerHit()
{
    if (!isConnected())
        return;

    // In case the limit switch status is unknown, try to sync them up from client.
    // Only do this if the client is fully connected to both input and output devices
    if (m_FullClosedLimitSwitch == m_FullOpenLimitSwitch && m_Client && m_Client->isConnected())
    {
        // We know the client is connected, but we'll still check the return values
        // to avoid logging errors for expected conditions
        m_Client->syncFullyOpenedState();
        m_Client->syncFullyClosedState();
    }

    if (DomeMotionSP.getState() == IPS_BUSY)
    {
        // Roll off is opening
        if (DomeMotionSP[DOME_CW].getState() == ISS_ON)
        {
            if (m_FullOpenLimitSwitch)
            {
                LOG_INFO("Roof is open.");
                SetParked(false);
                // Make sure the relays are off
                if (m_Client)
                    m_Client->stop();
            }
        }
        // Roll Off is closing
        else if (DomeMotionSP[DOME_CCW].getState() == ISS_ON)
        {
            if (m_FullClosedLimitSwitch)
            {
                LOG_INFO("Roof is closed.");
                SetParked(true);
                // Make sure the relays are off
                if (m_Client)
                    m_Client->stop();
            }
        }
    }
    // If state is not known and park is set, unpark.
    else if (isParked() && m_FullClosedLimitSwitch == false && m_FullOpenLimitSwitch == false)
        SetParked(false);

    SetTimer(getPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool UniversalROR::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);
    InputTP.save(fp);
    OutputTP.save(fp);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
IPState UniversalROR::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        // DOME_CW --> OPEN. If can we are ask to "open" while we are fully opened as the limit switch indicates, then we simply return false.
        if (dir == DOME_CW && m_FullOpenLimitSwitch)
        {
            LOG_WARN("Roof is already fully opened.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && m_FullClosedLimitSwitch)
        {
            LOG_WARN("Roof is already fully closed.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && INDI::Dome::isLocked())
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Cannot close dome when mount is locking. See: Telescope parking policy, in options tab");
            return IPS_ALERT;
        }

        if (dir == DOME_CW)
        {
            if (m_Client)
            {
                m_Client->openRoof();
            }
            else
            {
                LOG_ERROR("Failed to open roof. ROR Client is not connected!");
                return IPS_ALERT;
            }
        }
        else
        {
            if (m_Client)
            {
                m_Client->closeRoof();
            }
            else
            {
                LOG_ERROR("Failed to close roof. ROR Client is not connected!");
                return IPS_ALERT;
            }
        }

        return IPS_BUSY;
    }

    return (Dome::Abort() ? IPS_OK : IPS_ALERT);
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
IPState UniversalROR::Park()
{
    IPState rc = INDI::Dome::Move(DOME_CCW, MOTION_START);
    if (rc == IPS_BUSY)
    {
        LOG_INFO("Roll off is parking...");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
IPState UniversalROR::UnPark()
{
    IPState rc = INDI::Dome::Move(DOME_CW, MOTION_START);
    if (rc == IPS_BUSY)
    {
        LOG_INFO("Roll off is unparking...");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool UniversalROR::Abort()
{
    if (m_Client)
        return m_Client->stop();

    return false;
}


////////////////////////////////////////////////////////////////////////////////
/// Drivers are assumed to be on localhost running at port 7624
/// Clients connects to localhost:7624 server.
////////////////////////////////////////////////////////////////////////////////
void UniversalROR::ActiveDevicesUpdated()
{
    auto input = std::string(ActiveDeviceTP[ACTIVE_INPUT].getText());
    auto output = std::string(ActiveDeviceTP[ACTIVE_OUTPUT].getText());

    // If input OR output drivers are missing, do not initialize client.
    if (input.empty() || output.empty())
        return;

    // If there is no change, return
    if (m_Client && m_Client->inputDevice() == input && m_Client->outputDevice() == output)
        return;

    m_Client.reset(new UniversalRORClient(input, output));
    m_Client->setFullyClosedCallback([&](bool on)
    {
        m_FullClosedLimitSwitch = on;
        LimitSwitchLP[FullyClosed].setState(on ? IPS_OK : IPS_IDLE);
        LimitSwitchLP.apply();
    });
    m_Client->setFullyOpenedCallback([&](bool on)
    {
        m_FullOpenLimitSwitch = on;
        LimitSwitchLP[FullyOpened].setState(on ? IPS_OK : IPS_IDLE);
        LimitSwitchLP.apply();
    });
    m_Client->setConnectionCallback([&](bool connected)
    {
        if (connected)
        {
            LOG_INFO("Devices connected. Syncing indexes...");
            syncIndexes();
            SetTimer(getPollingPeriod());
        }
    });
    m_Client->watchDevice(input.c_str());
    m_Client->watchDevice(output.c_str());
    m_Client->connectServer();

}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool UniversalROR::syncIndexes()
{
    if (!m_Client)
        return false;

    // Input --> Fully Opened Indexes
    auto fullyOpenedList = extract(InputTP[FullyOpened].getText());
    if (!fullyOpenedList.empty() && fullyOpenedList != m_InputFullyOpened)
    {
        m_InputFullyOpened = fullyOpenedList;
        m_Client->setInputFullyOpened(m_InputFullyOpened);
    }

    // Input --> Fully Closed Indexes
    auto fullyClosedList = extract(InputTP[FullyClosed].getText());
    if (!fullyClosedList.empty() &&  fullyClosedList != m_InputFullyClosed)
    {
        m_InputFullyClosed = fullyClosedList;
        m_Client->setInputFullyClosed(m_InputFullyClosed);
    }

    // Output --> Open Roof Indexes
    auto openRoofList = extract(OutputTP[OpenRoof].getText());
    if (!openRoofList.empty() &&  openRoofList != m_OutputOpenRoof)
    {
        m_OutputOpenRoof = openRoofList;
        m_Client->setOutputOpenRoof(m_OutputOpenRoof);
    }

    // Output --> Close Roof Indexes
    auto closeRoofList = extract(OutputTP[CloseRoof].getText());
    if (!closeRoofList.empty() &&  closeRoofList != m_OutputCloseRoof)
    {
        m_OutputCloseRoof = closeRoofList;
        m_Client->setOutputCloseRoof(m_OutputCloseRoof);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
std::vector<uint8_t> UniversalROR::extract(const std::string &text)
{
    std::vector<uint8_t> result;
    std::regex pattern(R"(\d+)");
    std::smatch matches;
    std::string remaining = text;

    while (std::regex_search(remaining, matches, pattern))
    {
        result.push_back(std::stoi(matches[0]));
        remaining = matches.suffix().str();
    }

    return result;
}
