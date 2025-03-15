/*******************************************************************************
  Copyright(c) 2024 Frank Wang. All rights reserved.

  WandererCover V4-EC

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

#include "wanderer_cover_v4_ec.h"
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

static std::unique_ptr<WandererCoverV4EC> wanderercoverv4ec(new WandererCoverV4EC());

WandererCoverV4EC::WandererCoverV4EC() : DustCapInterface(this), LightBoxInterface(this)
{
    setVersion(1, 2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *WandererCoverV4EC::getDefaultName()
{
    return "WandererCover V4-EC";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4EC::initProperties()
{
    INDI::DefaultDevice::initProperties();

    LI::initProperties(MAIN_CONTROL_TAB, CAN_DIM);
    DI::initProperties(MAIN_CONTROL_TAB);

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE);
    addAuxControls();

    //Data read
    DataNP[closeset_read].fill( "Closed_Position", "Closed Position Set(°)", "%4.2f", 0, 999, 100, 0);
    DataNP[openset_read].fill( "Open_Position", "Open Position Set(°)", "%4.2f", 0, 999, 100, 0);
    DataNP[position_read].fill( "Current_Position", "Current Position(°)", "%4.2f", 0, 999, 100, 0);
    DataNP[voltage_read].fill( "Voltage", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    DataNP.fill(getDeviceName(), "STATUS", "Real Time Status", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    LightIntensityNP[0].setMax(255);
    LightIntensityNP[0].setValue(100);

    // Heater
    SetHeaterNP[Heat].fill( "Heater", "PWM", "%.2f", 0, 150, 50, 0);
    SetHeaterNP.fill(getDeviceName(), "Heater", "Dew Heater", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Close Set
    CloseSetNP[CloseSet].fill( "CloseSet", "10-90", "%.2f", 10, 90, 0.01, 20);
    CloseSetNP.fill(getDeviceName(), "CloseSet", "Close Position(°)", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Open Set
    OpenSetNP[OpenSet].fill( "OpenSet", "100-300", "%.2f", 100, 300, 0.01, 150);
    OpenSetNP.fill(getDeviceName(), "OpenSet", "Open Position(°)", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    setDefaultPollingPeriod(2000);

    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    serialConnection->registerHandshake([&]()
    {
        return getData();
    });
    registerConnection(serialConnection);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4EC::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        if(firmware >= 20240101)
        {
            LOGF_INFO("Firmware version: %d", firmware);
        }
        else
        {
            LOGF_INFO("Firmware version: %d", firmware);
            LOG_INFO("New firmware available!");
        }


        defineProperty(DataNP);
        defineProperty(SetHeaterNP);
        defineProperty(CloseSetNP);
        defineProperty(OpenSetNP);
    }
    else
    {
        deleteProperty(DataNP);
        deleteProperty(SetHeaterNP);
        deleteProperty(OpenSetNP);
        deleteProperty(CloseSetNP);
    }

    DI::updateProperties();
    LI::updateProperties();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererCoverV4EC::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);
    LI::ISGetProperties(dev);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4EC::ISSnoopDevice(XMLEle *root)
{
    LI::snoop(root);
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4EC::getData()
{
    // Try to lock the mutex with a short timeout
    // This allows us to skip the status update if the device is busy
    // but not wait too long which could cause the UI to freeze
    if (!serialPortMutex.try_lock_for(std::chrono::milliseconds(100)))
    {
        LOG_DEBUG("Serial port is busy, skipping status update");
        return true;
    }

    // Use RAII to ensure the mutex is unlocked when we exit this function
    std::lock_guard<std::timed_mutex> lock(serialPortMutex, std::adopt_lock);

    try
    {
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
        
        // Parse the received data
        return parseDeviceData(buffer);
    }
    catch(std::exception &e)
    {
        LOG_ERROR("Exception occurred while reading data from device");
        return false;
    }
    
    return true;
}

bool WandererCoverV4EC::parseDeviceData(const char *data)
{
    try
    {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(data);
        LOGF_INFO("Data: %s", data);
        // Split the data by 'A' separator
        while (std::getline(tokenStream, token, 'A'))
        {
            if (!token.empty())
                tokens.push_back(token);
        }
        
        // Check if we have enough tokens
        if (tokens.size() < 5)
        {
            LOG_ERROR("Insufficient data received from device");
            return false;
        }
        
        // Device Model
        if (tokens[0] != "WandererCoverV4")
        {
            if (tokens[0] == "ZXWBProV3" || tokens[0] == "ZXWBPlusV3" || 
                tokens[0] == "UltimateV2" || tokens[0] == "PlusV2")
            {
                LOG_WARN("The device is not WandererCover V4-EC!");
            }
            else
            {
                LOG_ERROR("Unknown device model");
            }
            return false;
        }
        
        // Firmware version
        firmware = std::atoi(tokens[1].c_str());
        
        // Close position set
        closesetread = std::strtod(tokens[2].c_str(), NULL);
        
        // Open position set
        opensetread = std::strtod(tokens[3].c_str(), NULL);
        
        // Current Position
        positionread = std::strtod(tokens[4].c_str(), NULL);
        
        // Voltage (if available)
        if (tokens.size() > 5)
        {
            voltageread = std::strtod(tokens[5].c_str(), NULL);
            
            if (voltageread <= 7)
            {
                LOG_ERROR("No power input!");
            }
        }
        
        // Update the UI with the parsed data
        updateData(closesetread, opensetread, positionread, voltageread);
        
        return true;
    }
    catch(std::exception &e)
    {
        LOG_ERROR("Failed to parse device data");
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererCoverV4EC::updateData(double closesetread, double opensetread, double positionread, double voltageread)
{
    DataNP[closeset_read].setValue(closesetread);
    DataNP[openset_read].setValue(opensetread);
    DataNP[position_read].setValue(positionread);
    DataNP[voltage_read].setValue(voltageread);
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
bool WandererCoverV4EC::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (LI::processText(dev, name, texts, names, n))
        return true;

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4EC::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (LI::processSwitch(dev, name, states, names, n))
        return true;

    if (DI::processSwitch(dev, name, states, names, n))
        return true;

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4EC::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (LI::processNumber(dev, name, values, names, n))
        return true;

    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Heater
        if (SetHeaterNP.isNameMatch(name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                rc1 = setDewPWM(2, static_cast<int>(values[i]));
            }

            SetHeaterNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (SetHeaterNP.getState() == IPS_OK)
                SetHeaterNP.update(values, names, n);
            SetHeaterNP.apply();
            return true;
        }
        // Close Set
        if (CloseSetNP.isNameMatch(name))
        {
            bool rc1 = false;

            for (int i = 0; i < n; i++)
            {
                if(values[i] < 10 || values[i] > 90)
                {
                    CloseSetNP.setState(IPS_ALERT);
                    LOG_ERROR("Out of range! Allowed closed angle: 10-90 degrees.");
                    return false;
                }
                rc1 = setClose(values[i]);
            }

            CloseSetNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (CloseSetNP.getState() == IPS_OK)
                CloseSetNP.update(values, names, n);
            CloseSetNP.apply();
            return true;
        }
        // Open Set
        if (OpenSetNP.isNameMatch(name))
        {
            bool rc1 = false;

            for (int i = 0; i < n; i++)
            {
                if(values[i] < 100 || values[i] > 300)
                {
                    OpenSetNP.setState(IPS_ALERT);
                    LOG_ERROR("Out of range! Allowed open angle: 100-300 degrees.");
                    return false;
                }
                rc1 = setOpen(values[i]);
            }

            OpenSetNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (OpenSetNP.getState() == IPS_OK)
                OpenSetNP.update(values, names, n);
            OpenSetNP.apply();
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4EC::toggleCover(bool open)
{
    char cmd[128] = {0};
    snprintf(cmd, 128, "100%d", open ? 1 : 0);
    // No need to wait for 'done' response as we're handling the busy state in ParkCap/UnParkCap
    // and the actual position is monitored through regular status updates
    if (sendCommand(cmd, true))
    {
        return true;
    }

    return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState WandererCoverV4EC::ParkCap()
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
IPState WandererCoverV4EC::UnParkCap()
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
bool WandererCoverV4EC::SetLightBoxBrightness(uint16_t value)
{
    auto rc = true;
    if(value > 0)
    {
        // Only change if already enabled.
        if (LightSP[INDI_ENABLED].getState() == ISS_ON)
            rc = sendCommand(std::to_string(value), false);
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
bool WandererCoverV4EC::EnableLightBox(bool enable)
{
    auto rc = false;
    if(!enable)
        rc = sendCommand("9999", false);
    else
        rc = sendCommand(std::to_string(static_cast<int>(LightIntensityNP[0].getValue())), false);

    return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4EC::sendCommand(std::string command, bool waitForDone)
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
    
    if (waitForDone)
    {
        // Wait for 'done' response for at most 30 seconds
        char buffer[512] = {0};
        int nbytes_read = 0;
        time_t startTime = time(nullptr);
        
        while (difftime(time(nullptr), startTime) < 30)
        {
            LOGF_INFO("Waiting for 'done' response : %d seconds", difftime(time(nullptr), startTime));
            if ((rc = tty_read_section(PortFD, buffer, '\n', 1, &nbytes_read)) == TTY_OK)
            {
                buffer[nbytes_read] = '\0';
                LOGF_INFO("Response: %s", buffer);
                
                // Check if the response is 'done'
                if (strstr(buffer, "done") != nullptr)
                {
                    LOG_DEBUG("Received 'done' response");
                    return true;
                }
                else
                {
                    // If not 'done', parse the data to update the GUI
                    parseDeviceData(buffer);
                }
            }
        }
        
        LOG_WARN("Timeout waiting for 'done' response");
    }
    
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4EC::setDewPWM(int id, int value)
{
    char cmd[64] = {0};
    snprintf(cmd, 64, "%d%03d", id, value);
    if (sendCommand(cmd, false))
    {
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4EC::setClose(double value)
{
    char cmd[64] = {0};
    snprintf(cmd, 64, "%d", (int)(value * 100 + 10000));
    if (sendCommand(cmd, false))
    {
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4EC::setOpen(double value)
{
    char cmd[64] = {0};
    snprintf(cmd, 64, "%d", (int)(value * 100 + 40000));
    if (sendCommand(cmd, false))
    {
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererCoverV4EC::TimerHit()
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
bool WandererCoverV4EC::saveConfigItems(FILE * fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    SetHeaterNP.save(fp);
    CloseSetNP.save(fp);
    OpenSetNP.save(fp);

    return LI::saveConfigItems(fp);
}
