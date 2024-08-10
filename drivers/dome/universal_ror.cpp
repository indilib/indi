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
bool UniversalROR::Connect()
{
    if (!m_Client || !m_Client->isConnected())
    {
        LOG_ERROR("ROR Client is not connected. Specify the input and output drivers in Options tab.");
        return false;

    }

    syncIndexes();
    SetTimer(getPollingPeriod());
    return true;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool UniversalROR::Disconnect()
{
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
            syncIndexes();
            return true;
        }

        // Output Indexes
        if (OutputTP.isNameMatch(name))
        {
            OutputTP.update(texts, names, n);
            OutputTP.setState(IPS_OK);
            OutputTP.apply();
            saveConfig(OutputTP);
            syncIndexes();
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
    }
    else
    {
        deleteProperty(InputTP);
        deleteProperty(OutputTP);
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
    if (m_FullClosedLimitSwitch == m_FullOpenLimitSwitch && m_Client && m_Client->isConnected())
    {
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

    m_Client.reset(new UniversalRORClient(input, output));
    m_Client->setFullyClosedCallback([&](bool on)
    {
        m_FullClosedLimitSwitch = on;
    });
    m_Client->setFullyOpenedCallback([&](bool on)
    {
        m_FullOpenLimitSwitch = on;
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

    m_Client->syncFullyOpenedState();
    m_Client->syncFullyClosedState();

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