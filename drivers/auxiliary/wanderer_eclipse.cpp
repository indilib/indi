/*******************************************************************************
  Copyright(c) 2024 Frank Wang/Jérémie Klein. All rights reserved.

  Wanderer Eclipse

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

#include "wanderer_eclipse.h"
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

static std::unique_ptr<WandererEclipse> wanderereclipse(new WandererEclipse());

WandererEclipse::WandererEclipse() : DustCapInterface(this)
{
    setVersion(1, 2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *WandererEclipse::getDefaultName()
{
    return "Wanderer Eclipse";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererEclipse::initProperties()
{
    INDI::DefaultDevice::initProperties();

    DI::initProperties(MAIN_CONTROL_TAB);

    setDriverInterface(AUX_INTERFACE | DUSTCAP_INTERFACE);
    addAuxControls();

    //Data read
    
    DataNP[torque_read].fill( "Motor_Torque", "Motor_Torque", "%4.2f", 0, 999, 100, 0);
    DataNP[voltage_read].fill( "Voltage", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    DataNP.fill(getDeviceName(), "STATUS", "Real Time Status", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Firmware information
    FirmwareTP[FIRMWARE_VERSION].fill("FIRMWARE_VERSION", "Firmware Version", "Unknown");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_INFO", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);


    // Torque property
    //TorqueSP[TORQUE_LOW].fill("TORQUE_LOW", "Low", ISS_OFF);
    //TorqueSP[TORQUE_MEDIUM].fill("TORQUE_MEDIUM", "Medium", ISS_ON);
    //TorqueSP[TORQUE_HIGH].fill("TORQUE_HIGH", "High", ISS_OFF);
    //TorqueSP.fill(getDeviceName(), "TORQUE", "Motor Torque", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

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
bool WandererEclipse::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Update firmware information
        char firmwareStr[16];
        snprintf(firmwareStr, sizeof(firmwareStr), "%d", firmware);
        FirmwareTP[FIRMWARE_VERSION].setText(firmwareStr);
        
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
        defineProperty(FirmwareTP);
defineProperty(TorqueSP);
    }
    else
    {
        deleteProperty(DataNP);
        deleteProperty(FirmwareTP);
 deleteProperty(TorqueSP);
    }

    DI::updateProperties();
    return true;
}


bool WandererEclipse::getData()
{
    if (!serialPortMutex.try_lock_for(std::chrono::milliseconds(100)))
    {
        LOG_DEBUG("Serial port is busy, skipping status update");
        return true;
    }

    std::lock_guard<std::timed_mutex> lock(serialPortMutex, std::adopt_lock);

    try
    {
        PortFD = serialConnection->getPortFD();
        tcflush(PortFD, TCIOFLUSH);


        const char *command = "1500001\n";
        int bytes_written = write(PortFD, command, strlen(command));
        if (bytes_written < 0)
        {
            LOG_ERROR("Failed to write command to device");
            return false;
        }


        char buffer[512] = {0};
        int nbytes_read = 0, rc = -1;

        if ((rc = tty_read_section(PortFD, buffer, '\n', 2, &nbytes_read)) != TTY_OK)
        {
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

        return parseDeviceData(buffer);
    }
    catch (std::exception &e)
    {
        LOG_ERROR("Exception occurred while reading data from device");
        return false;
    }

    return true;
}


bool WandererEclipse::parseDeviceData(const char *data)
{
    try
    {
    
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(data);
        LOGF_DEBUG("Data: %s", data);
        // Split the data by 'A' separator
        while (std::getline(tokenStream, token, 'A'))
        {
            if (!token.empty())
                tokens.push_back(token);
        }
        
        // Check if we have enough tokens
        if (tokens.size() < 3)
        {
            return false;
        }
        
        // Device Model
        if (tokens[0] != "WandererEclipse")
        {
            if (tokens[0] == "ZXWBProV3" || tokens[0] == "ZXWBPlusV3" || 
                tokens[0] == "UltimateV2" || tokens[0] == "PlusV2"|| tokens[0] == "WandererCoverV4"|| tokens[0] == "WandererCoverV4Pro")
            {
                LOG_WARN("The device is not Wanderer Eclipse!");
            }
            else
            {
                LOG_ERROR("Unknown device model");
            }
            return false;
        }
        
        // Firmware version
        firmware = std::atoi(tokens[1].c_str());
        
        // Update firmware information in the UI
        char firmwareStr[16];
        snprintf(firmwareStr, sizeof(firmwareStr), "%d", firmware);
        FirmwareTP[FIRMWARE_VERSION].setText(firmwareStr);
        FirmwareTP.setState(IPS_OK);
        FirmwareTP.apply();
        
        // Touque
        torqueLevel = std::strtod(tokens[2].c_str(), NULL);
        
        
        // Voltage 
        if (tokens.size() > 3)
        {
            voltageread = std::strtod(tokens[3].c_str(), NULL);
            
            if (voltageread <= 7)
            {
                LOG_ERROR("No power input!");
            }
        }
        
        // Update the UI with the parsed data
        updateData(torqueLevel, voltageread);
        
        
        return true;
    }
    catch(std::exception &e)
    {
        LOG_ERROR("Failed to parse device data");
        return false;
    }
}

void WandererEclipse::updateData(double torqueLevel, double voltageread)
{
    if (torqueLevel <= 0)
        return;

    DataNP[torque_read].setValue(torqueLevel);
    DataNP[voltage_read].setValue(voltageread);
    DataNP.setState(IPS_OK);
    DataNP.apply();

    int level = 0;
    if (torqueLevel <= 110)
        level = 0; // LOW
    else if (torqueLevel <= 160)
        level = 1; // MEDIUM
    else
        level = 2; // HIGH

    for (int i = 0; i < 3; i++)
        TorqueSP[i].setState(i == level ? ISS_ON : ISS_OFF);

    TorqueSP.setState(IPS_OK);
    TorqueSP.apply();
    
        auto prevParked = ParkCapSP[CAP_PARK].getState() == ISS_ON;
    auto prevState = ParkCapSP.getState();
    ParkCapSP.setState(IPS_OK);
                        
        auto currentParked = ParkCapSP[CAP_PARK].getState() == ISS_ON;
    auto currentState = ParkCapSP.getState();

    // Only update on state changes
    if ((prevParked != currentParked) || (prevState != currentState))
        ParkCapSP.apply();
}





////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererEclipse::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
     if (DI::processSwitch(dev, name, states, names, n))
        return true;

   if (dev && !strcmp(dev, getDeviceName()))
{
    // Torque
    if (TorqueSP.isNameMatch(name))
    {
        int newTorque = -1;
        const char *cmd = nullptr;

        for (int i = 0; i < n; i++)
        {
            if (states[i] == ISS_ON)
            {
                if (strcmp(names[i], "TORQUE_LOW") == 0)
                {
                    newTorque = 0;
                    cmd = "3110";
                }
                else if (strcmp(names[i], "TORQUE_MEDIUM") == 0)
                {
                    newTorque = 1;
                    cmd = "3160";
                }
                else if (strcmp(names[i], "TORQUE_HIGH") == 0)
                {
                    newTorque = 2;
                    cmd = "3190";
                }
            }
        }

        if (newTorque != -1 && newTorque != torqueLevel)
        {
            if (cmd && sendCommand(cmd))
            {
                torqueLevel = newTorque;
                for (int i = 0; i < 3; i++)
                    TorqueSP[i].setState(i == newTorque ? ISS_ON : ISS_OFF);
                TorqueSP.setState(IPS_OK);
            }
            else
            {
                TorqueSP.setState(IPS_ALERT);
            }

            TorqueSP.apply();
            return true;
        }
    }
}

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererEclipse::toggleCover(bool open)
{
    char cmd[128] = {0};
    snprintf(cmd, 128, "100%d", open ? 1 : 0);
    if (sendCommand(cmd))
    {
        return true;
    }

    return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState WandererEclipse::ParkCap()
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
IPState WandererEclipse::UnParkCap()
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
bool WandererEclipse::sendCommand(std::string command)
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
    
    return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererEclipse::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getPollingPeriod());
        return;
    }

    getData();
    SetTimer(getPollingPeriod());
}


