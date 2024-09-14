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
    try
    {
        PortFD = serialConnection->getPortFD();
        tcflush(PortFD, TCIOFLUSH);
        int nbytes_read_name = 0, rc = -1;
        char name[64] = {0};

        //Device Model//////////////////////////////////////////////////////////////////////////////////////////////////////
        if ((rc = tty_read_section(PortFD, name, 'A', 3, &nbytes_read_name)) != TTY_OK)
        {
            char errorMessage[MAXRBUF];
            tty_error_msg(rc, errorMessage, MAXRBUF);
            if(Ismoving == false)
            {
                LOGF_ERROR("No data received, the device may not be WandererCover V4-EC, please check the serial port! Error: %s",
                           errorMessage);
            }
            return false;
        }
        name[nbytes_read_name - 1] = '\0';
        if(strcmp(name, "ZXWBProV3") == 0 || strcmp(name, "ZXWBPlusV3") == 0 || strcmp(name, "UltimateV2") == 0
                || strcmp(name, "PlusV2") == 0)
        {
            LOG_WARN("The device is not WandererCover V4-EC!");
            return false;
        }

        if(strcmp(name, "WandererCoverV4") != 0)
            throw std::exception();
        // Frimware version/////////////////////////////////////////////////////////////////////////////////////////////
        int nbytes_read_version = 0;
        char version[64] = {0};
        tty_read_section(PortFD, version, 'A', 5, &nbytes_read_version);

        version[nbytes_read_version - 1] = '\0';
        firmware = std::atoi(version);
        // Close position set//////////////////////////////////////////////////////////////////////////////////////////
        char closeset[64] = {0};
        int nbytes_read_closeset = 0;
        tty_read_section(PortFD, closeset, 'A', 5, &nbytes_read_closeset);
        closeset[nbytes_read_closeset - 1] = '\0';
        closesetread = std::strtod(closeset, NULL);

        // Open position set//////////////////////////////////////////////////////////////////////////////////////////
        char openset[64] = {0};
        int nbytes_read_openset = 0;
        tty_read_section(PortFD, openset, 'A', 5, &nbytes_read_openset);
        openset[nbytes_read_openset - 1] = '\0';
        opensetread = std::strtod(openset, NULL);
        // Current Position//////////////////////////////////////////////////////////////////////////////////////////
        char position[64] = {0};
        int nbytes_read_position = 0;
        tty_read_section(PortFD, position, 'A', 5, &nbytes_read_position);
        position[nbytes_read_position - 1] = '\0';
        positionread = std::strtod(position, NULL);

        // Voltage//////////////////////////////////////////////////////////////////////////////////////////
        char voltage[64] = {0};
        int nbytes_read_voltage = 0;
        tty_read_section(PortFD, voltage, 'A', 5, &nbytes_read_voltage);
        voltage[nbytes_read_voltage - 1] = '\0';
        voltageread = std::strtod(voltage, NULL);
        updateData(closesetread, opensetread, positionread, voltageread);

        if(voltageread <= 7)
        {
            LOG_ERROR("No power input!");
        }

        Ismoving = false;
    }
    catch(std::exception &e)
    {
        //LOGF_INFO("Data read failed","failed");
    }
    return true;
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
    if (sendCommand(cmd))
    {
        Ismoving = true;
        return true;
    }

    return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState WandererCoverV4EC::ParkCap()
{
    if (toggleCover(false))
    {
        return IPS_BUSY;
    }

    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState WandererCoverV4EC::UnParkCap()
{
    if (toggleCover(true))
    {
        return IPS_BUSY;
    }

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
            rc = sendCommand(std::to_string(value));
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
        rc = sendCommand("9999");
    else
        rc = sendCommand(std::to_string(static_cast<int>(LightIntensityNP[0].getValue())));

    return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererCoverV4EC::sendCommand(std::string command)
{
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
bool WandererCoverV4EC::setDewPWM(int id, int value)
{
    char cmd[64] = {0};
    snprintf(cmd, 64, "%d%03d", id, value);
    if (sendCommand(cmd))
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
    if (sendCommand(cmd))
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
    if (sendCommand(cmd))
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
