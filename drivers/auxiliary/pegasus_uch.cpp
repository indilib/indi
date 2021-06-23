/*******************************************************************************
  Copyright(c) 2021 Chrysikos Efstathios. All rights reserved.

  Pegasus USB Control Hub

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

#include "pegasus_uch.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <memory>
#include <regex>
#include <termios.h>
#include <cstring>
#include <sys/ioctl.h>
#include <chrono>
#include <math.h>
#include <iomanip>


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
PegasusUCH::PegasusUCH()
{
    setVersion(1, 0);
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUCH::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE);

    addAuxControls();

    ////////////////////////////////////////////////////////////////////////////
    /// Main Control Panel
    ////////////////////////////////////////////////////////////////////////////

    // Reboot
    IUFillSwitch(&RebootS[0], "REBOOT", "Reboot Device", ISS_OFF);
    IUFillSwitchVector(&RebootSP, RebootS, 1, getDeviceName(), "REBOOT_DEVICE", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,
                       60, IPS_IDLE);

    // Led
    IUFillSwitch(&PowerLEDS[POWER_LED_ON], "POWER_LED_ON", "On", ISS_ON);
    IUFillSwitch(&PowerLEDS[POWER_LED_OFF], "POWER_LED_OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&PowerLEDSP, PowerLEDS, 2, getDeviceName(), "POWER_LED", "LED", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    // USB
    IUFillSwitch(&USBPort1S[USB_OFF], "USBPORT1_OFF", "Off", ISS_ON);
    IUFillSwitch(&USBPort1S[USB_ON], "USBPORT1_ON", "On", ISS_OFF);
    IUFillSwitchVector(&USBPort1SP, USBPort1S, 2, getDeviceName(), "USBPort1", "", USB_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&USBPort2S[USB_OFF], "USBPORT2_OFF", "Off", ISS_ON);
    IUFillSwitch(&USBPort2S[USB_ON], "USBPORT2_ON", "On", ISS_OFF);
    IUFillSwitchVector(&USBPort2SP, USBPort2S, 2, getDeviceName(), "USBPort2", "", USB_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&USBPort3S[USB_OFF], "USBPORT3_OFF", "Off", ISS_ON);
    IUFillSwitch(&USBPort3S[USB_ON], "USBPORT3_ON", "On", ISS_OFF);
    IUFillSwitchVector(&USBPort3SP, USBPort3S, 2, getDeviceName(), "USBPort3", "", USB_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&USBPort4S[USB_OFF], "USBPORT4_OFF", "Off", ISS_ON);
    IUFillSwitch(&USBPort4S[USB_ON], "USBPORT4_ON", "On", ISS_OFF);
    IUFillSwitchVector(&USBPort4SP, USBPort4S, 2, getDeviceName(), "USBPort4", "", USB_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&USBPort5S[USB_OFF], "USBPORT5_OFF", "Off", ISS_ON);
    IUFillSwitch(&USBPort5S[USB_ON], "USBPORT5_ON", "On", ISS_OFF);
    IUFillSwitchVector(&USBPort5SP, USBPort5S, 2, getDeviceName(), "USBPort5", "", USB_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&USBPort6S[USB_OFF], "USBPORT1_OFF", "Off", ISS_ON);
    IUFillSwitch(&USBPort6S[USB_ON], "USBPORT1_ON", "On", ISS_OFF);
    IUFillSwitchVector(&USBPort6SP, USBPort6S, 2, getDeviceName(), "USBPort6", "", USB_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);



    ////////////////////////////////////////////////////////////////////////////
    /// Firmware Group
    ////////////////////////////////////////////////////////////////////////////
    IUFillText(&InfoT[INFO_VERSION], "VERSION", "Version", "NA");
    IUFillText(&InfoT[INFO_UPTIME], "UPTIME", "Uptime (h)", "NA");
    IUFillText(&InfoT[INFO_USBVOLTAGE], "USBVOLTAGE", "USB Voltage", "NA");
    IUFillTextVector(&InfoTP, InfoT, 3, getDeviceName(), "INFO", "INFO", INFO_TAB, IP_RO, 60,
                     IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Serial Connection
    ////////////////////////////////////////////////////////////////////////////
    serialConnection = new Connection::Serial(this);
     serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUCH::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Main Control
        defineProperty(&RebootSP);

        // USB
        defineProperty(&USBPort1SP);
        defineProperty(&USBPort2SP);
        defineProperty(&USBPort3SP);
        defineProperty(&USBPort4SP);
        defineProperty(&USBPort5SP);
        defineProperty(&USBPort6SP);

        // Led
        defineProperty(&PowerLEDSP);

        // Firmware
        defineProperty(&InfoTP);
        initialized = true;
    }
    else
    {
        // Main Control
        deleteProperty(RebootSP.name);

        // USB
        deleteProperty(USBPort1SP.name);
        deleteProperty(USBPort2SP.name);
        deleteProperty(USBPort3SP.name);
        deleteProperty(USBPort4SP.name);
        deleteProperty(USBPort5SP.name);
        deleteProperty(USBPort6SP.name);

        // Led
        deleteProperty(PowerLEDSP.name);

        //Firmware
        deleteProperty(InfoTP.name);

        initialized = false;
    }
    return true;
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUCH::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Reboot
        if (!strcmp(name, RebootSP.name))
        {
            RebootSP.s = reboot() ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&RebootSP, nullptr);
            LOG_INFO("Rebooting device...");
            return true;
        }



        // Power LED
        if (!strcmp(name, PowerLEDSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&PowerLEDSP);
            IUUpdateSwitch(&PowerLEDSP, states, names, n);
            if (setPowerLEDEnabled(PowerLEDS[0].s == ISS_ON))
            {
                PowerLEDSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&PowerLEDSP);
                PowerLEDS[prevIndex].s = ISS_ON;
                PowerLEDSP.s = IPS_ALERT;
            }

            IDSetSwitch(&PowerLEDSP, nullptr);
            return true;
        }

        if (!strcmp(name, USBPort1SP.name))
        {
            return  this->setUSBPort(1, USBPort1S, USBPort1SP, states, names, n);
        }
        else if(!strcmp(name, USBPort2SP.name))
        {
            return  this->setUSBPort(2, USBPort1S, USBPort1SP, states, names, n);
        }
        else if(!strcmp(name, USBPort3SP.name))
        {
            return  this->setUSBPort(3, USBPort1S, USBPort1SP, states, names, n);
        }
        else if(!strcmp(name, USBPort4SP.name))
        {
            return  this->setUSBPort(4, USBPort1S, USBPort1SP, states, names, n);
        }
        else if(!strcmp(name, USBPort5SP.name))
        {
            return  this->setUSBPort(5, USBPort1S, USBPort1SP, states, names, n);
        }
        else if(!strcmp(name, USBPort6SP.name))
        {
            return  this->setUSBPort(6, USBPort1S, USBPort1SP, states, names, n);
        }
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}


bool PegasusUCH::setUSBPort(uint8_t port, ISwitch usbPortS[2], ISwitchVectorProperty sp, ISState * states, char * names[],
                            int n)
{
    int prevIndex = IUFindOnSwitchIndex(&sp);
    IUUpdateSwitch(&sp, states, names, n);
    if (setUSBPortEnabled(port, usbPortS[1].s == ISS_ON))
    {
        sp.s = IPS_OK;
    }
    else
    {
        IUResetSwitch(&sp);
        usbPortS[prevIndex].s = ISS_ON;
        sp.s = IPS_ALERT;
    }

    IDSetSwitch(&sp, nullptr);
    return true;
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
const char * PegasusUCH::getDefaultName()
{
    return "Pegasus UCH";
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUCH::saveConfigItems(FILE * fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &PowerLEDSP);
    return true;
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void PegasusUCH::TimerHit()
{
    if (!isConnected() || initialized == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }


    this->updateUSBPower();
    this->updateUpTime();

    SetTimer(getCurrentPollingPeriod());
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
std::vector<std::string> PegasusUCH::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUCH::Handshake()
{
    PortFD = serialConnection->getPortFD();

    char response[PEGASUS_LEN] = {0};
    if(sendCommand("P#", response))
    {
        if(strstr("OK_UCH", response) != nullptr)
        {
            setFirmwareVersion();
            this->initialized = true;
            return  true;
        }
    }
    else
    {
        LOG_ERROR("Ack failed.");
    }

    this->initialized = false;
    return false;
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUCH::reboot()
{
    return sendCommand("PF", nullptr);
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUCH::setPowerLEDEnabled(bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "PL:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUCH::setUSBPortEnabled(uint8_t port, bool enabled)
{
    char cmd[PEGASUS_LEN] = {0}, expected[PEGASUS_LEN] = {0}, res[PEGASUS_LEN] = {0};
    snprintf(cmd, PEGASUS_LEN, "U%d:%d", port, enabled ? 1 : 0);
    snprintf(expected, PEGASUS_LEN, "U%d:%d", port, enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, expected));
    }

    return false;
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void PegasusUCH::setFirmwareVersion()
{
    char response[PEGASUS_LEN] = {0};

    if(sendCommand("PV", response))
    {
        IUSaveText(&InfoT[INFO_VERSION], response);
        IDSetText(&InfoTP, nullptr);
    }
    else
    {
        InfoTP.s = IPS_ALERT;
        LOG_ERROR("Error on updateFirmware.");
    }
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void PegasusUCH::updateUSBPower()
{
    char response[PEGASUS_LEN] = {0};

    if(sendCommand("PA", response))
    {
        std::vector<std::string> result = split(response, ":");


        if(result.size() != 3)
        {
            LOGF_WARN("Received wrong number (%i) of  data (%s). Retrying...", result.size(), response);
            return;
        }

        std::string usbBusVoltage = result[1];


        IUSaveText(&InfoT[INFO_USBVOLTAGE], usbBusVoltage.c_str());
        IDSetText(&InfoTP,nullptr);
    }
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void PegasusUCH::updateUpTime()
{
    char response[PEGASUS_LEN] = {0};

    if(sendCommand("PC", response))
    {
        std::vector<std::string> result = split(response, ":");


        if(result.size() != 2)
        {
            LOGF_WARN("Received wrong number (%i) of  data (%s). Retrying...", result.size(), response);
            return;
        }

        std::chrono::milliseconds uptime(std::stol(result[1]));
        using dhours = std::chrono::duration<double, std::ratio<3600>>;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) << dhours(uptime).count();
        IUSaveText(&InfoT[INFO_UPTIME], ss.str().c_str());
        IDSetText(&InfoTP,nullptr);
    }
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusUCH::sendCommand(const char * command, char * res)
{
    int nbytes_read = 0, nbytes_written = 0, rc = -1;
    LOGF_DEBUG("CMD <%s>", command);

    char errstr[MAXRBUF];

    char cmd[PEGASUS_LEN] = {0};
    snprintf(cmd, 7, "%s\n", command);

    LOGF_DEBUG("CMD <%#02X>", cmd[0]);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return false;
    }

    if ((rc = tty_read_section(PortFD, res, 0xA, 3, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return  false;
    }

    // Get rid of 0xA
    res[nbytes_read - 1] = 0;

    if(res[nbytes_read - 2] == '\r')
        res[nbytes_read - 2] = 0;

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void PegasusUCH::cleanupResponse(char *response)
{
    std::string s(response);
    s.erase(std::remove_if(s.begin(), s.end(),
                           [](unsigned char x)
    {
        return std::isspace(x);
    }), s.end());
    strncpy(response, s.c_str(), PEGASUS_LEN);
}
