/*
    Waveshare ModBUS POE Output
    Copyright (C) 2024 Jasem Mutlaq
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

#include "waveshare_modbus_relay.h"
#include "../../libs/modbus/platform.h"

#include <memory>
#include <unistd.h>
#include <iomanip>
#include <connectionplugins/connectiontcp.h>

static std::unique_ptr<WaveshareRelay> sesto(new WaveshareRelay());

WaveshareRelay::WaveshareRelay() : OutputInterface(this)
{
    setVersion(1, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WaveshareRelay::initProperties()
{
    INDI::DefaultDevice::initProperties();

    INDI::OutputInterface::initProperties(MAIN_CONTROL_TAB, 8, "Output");

    setDriverInterface(AUX_INTERFACE | OUTPUT_INTERFACE);

    addAuxControls();

    setDefaultPollingPeriod(2000);
    tcpConnection = new Connection::TCP(this);
    tcpConnection->setDefaultHost("192.168.1.1");
    tcpConnection->setDefaultPort(502);
    tcpConnection->registerHandshake([&]()
    {
        return Handshake();
    });

    registerConnection(tcpConnection);

    FirmwareVersionTP[0].fill("VERSION", "Version", "1.00");
    FirmwareVersionTP.fill(getDeviceName(), "FIRMWARE", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WaveshareRelay::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    INDI::OutputInterface::updateProperties();

    if (isConnected())
    {
        defineProperty(FirmwareVersionTP);
        SetTimer(getPollingPeriod());
    }
    else
    {
        deleteProperty(FirmwareVersionTP);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WaveshareRelay::Handshake()
{
    PortFD = tcpConnection->getPortFD();

    nmbs_platform_conf platform_conf;
    platform_conf.transport = NMBS_TRANSPORT_TCP;
    platform_conf.read = read_fd_linux;
    platform_conf.write = write_fd_linux;
    platform_conf.arg = &PortFD;

    // Create the modbus client
    nmbs_error err = nmbs_client_create(&nmbs, &platform_conf);
    if (err != NMBS_ERROR_NONE)
    {
        LOGF_ERROR("Error creating modbus client: %d", err);
        if (!nmbs_error_is_exception(err))
            return false;
    }

    // Set only the response timeout. Byte timeout will be handled by the TCP connection
    nmbs_set_read_timeout(&nmbs, 1000);

    uint16_t output;
    err = nmbs_read_holding_registers(&nmbs, 0x8000, 1, &output);
    if (err == NMBS_ERROR_NONE)
    {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << output / 100.0;
        FirmwareVersionTP[0].setText(ss.str().c_str());
        FirmwareVersionTP.setState(IPS_OK);
        return true;
    }

    LOGF_ERROR("Failed to query device firmware version: %d", err);
    return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *WaveshareRelay::getDefaultName()
{
    return "Waveshare Relay";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WaveshareRelay::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    // Check Output Properties
    if (INDI::OutputInterface::processText(dev, name, texts, names, n))
        return true;

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WaveshareRelay::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (INDI::OutputInterface::processSwitch(dev, name, states, names, n))
        return true;

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WaveshareRelay::saveConfigItems(FILE * fp)
{
    INDI::OutputInterface::saveConfigItems(fp);
    return INDI::DefaultDevice::saveConfigItems(fp);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WaveshareRelay::TimerHit()
{
    if (!isConnected())
        return;

    UpdateDigitalOutputs();

    SetTimer(getCurrentPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WaveshareRelay::UpdateDigitalOutputs()
{
    nmbs_bitfield coils = {0};
    auto err = nmbs_read_coils(&nmbs, 0, 8, coils);
    if (err != NMBS_ERROR_NONE)
    {
        LOGF_ERROR("Error reading coils at address 0: %s", nmbs_strerror(err));
        return false;
    }
    else
    {
        for (size_t i = 0; i < DigitalOutputsSP.size(); i++)
        {
            auto oldState = DigitalOutputsSP[i].findOnSwitchIndex();
            auto newState = nmbs_bitfield_read(coils, i);
            if (oldState != newState)
            {
                DigitalOutputsSP[i].reset();
                DigitalOutputsSP[i][newState].setState(ISS_ON);
                DigitalOutputsSP[i].setState(IPS_OK);
                DigitalOutputsSP[i].apply();
            }
        }
        return true;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WaveshareRelay::CommandOutput(uint32_t index, OutputState command)
{
    uint16_t value = (command == On) ? 0xFF00 : 0;

    auto err = nmbs_write_single_coil(&nmbs, index, value);
    if (err != NMBS_ERROR_NONE)
    {
        LOGF_ERROR("Error reading coils at address 0: %s", nmbs_strerror(err));
        return false;
    }

    return true;
}
