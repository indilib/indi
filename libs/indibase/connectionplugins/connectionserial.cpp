/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "connectionserial.h"
#include "indistandardproperty.h"
#include "indicom.h"
#include "indilogger.h"

#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>
#include <regex>
#include <random>

namespace Connection
{
extern const char *CONNECTION_TAB;

Serial::Serial(INDI::DefaultDevice *dev, IPerm permission) : Interface(dev, CONNECTION_SERIAL), m_Permission(permission)
{
    char configPort[256] = {0};
    // Try to load the port from the config file. If that fails, use default port.
    if (IUGetConfigText(dev->getDeviceName(), INDI::SP::DEVICE_PORT, "PORT", configPort, 256) == 0)
    {
        m_ConfigPort = configPort;
        IUFillText(&PortT[0], "PORT", "Port", configPort);
    }
    else
    {
#ifdef __APPLE__
        IUFillText(&PortT[0], "PORT", "Port", "/dev/cu.usbserial");
#else
        IUFillText(&PortT[0], "PORT", "Port", "/dev/ttyUSB0");
#endif
    }
    IUFillTextVector(&PortTP, PortT, 1, dev->getDeviceName(), INDI::SP::DEVICE_PORT, "Ports", CONNECTION_TAB, m_Permission, 60,
                     IPS_IDLE);

    int autoSearchIndex = 0;
    // Try to load the port from the config file. If that fails, use default port.
    IUGetConfigOnSwitchIndex(dev->getDeviceName(), INDI::SP::DEVICE_AUTO_SEARCH, &autoSearchIndex);
    IUFillSwitch(&AutoSearchS[INDI::DefaultDevice::INDI_ENABLED], "INDI_ENABLED", "Enabled",
                 autoSearchIndex == 0 ? ISS_ON : ISS_OFF);
    IUFillSwitch(&AutoSearchS[INDI::DefaultDevice::INDI_DISABLED], "INDI_DISABLED", "Disabled",
                 autoSearchIndex == 0 ? ISS_OFF : ISS_ON);
    IUFillSwitchVector(&AutoSearchSP, AutoSearchS, 2, dev->getDeviceName(), INDI::SP::DEVICE_AUTO_SEARCH, "Auto Search",
                       CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&RefreshS[0], "Scan Ports", "Scan Ports", ISS_OFF);
    IUFillSwitchVector(&RefreshSP, RefreshS, 1, dev->getDeviceName(), "DEVICE_PORT_SCAN", "Refresh", CONNECTION_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&BaudRateS[0], "9600", "", ISS_ON);
    IUFillSwitch(&BaudRateS[1], "19200", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[2], "38400", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[3], "57600", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[4], "115200", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[5], "230400", "", ISS_OFF);
    IUFillSwitchVector(&BaudRateSP, BaudRateS, 6, dev->getDeviceName(), INDI::SP::DEVICE_BAUD_RATE, "Baud Rate", CONNECTION_TAB,
                       m_Permission, ISR_1OFMANY, 60, IPS_IDLE);

    // Try to load the port from the config file. If that fails, use default port.
    IUGetConfigOnSwitchIndex(dev->getDeviceName(), INDI::SP::DEVICE_BAUD_RATE, &m_ConfigBaudRate);
    // If we have a valid config entry, se it.
    if (m_ConfigBaudRate >= 0)
    {
        IUResetSwitch(&BaudRateSP);
        BaudRateS[m_ConfigBaudRate].s = ISS_ON;
    }
}

Serial::~Serial()
{
    delete[] SystemPortS;
}

bool Serial::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev, m_Device->getDeviceName()))
    {
        // Serial Port
        if (!strcmp(name, PortTP.name))
        {
            IUUpdateText(&PortTP, texts, names, n);
            PortTP.s = IPS_OK;
            IDSetText(&PortTP, nullptr);

            auto pos = std::find_if(m_SystemPorts.begin(), m_SystemPorts.end(), [&](const std::string onePort)
            {
                return !strcmp(PortT[0].text, onePort.c_str());
            });
            if (pos != m_SystemPorts.end())
            {
                LOGF_DEBUG("Auto search is disabled because %s is not a system port.", PortT[0].text);
                AutoSearchS[0].s = ISS_OFF;
                AutoSearchS[1].s = ISS_ON;
                IDSetSwitch(&AutoSearchSP, nullptr);
            }
        }
        return true;
    }

    return false;
}

bool Serial::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(dev, m_Device->getDeviceName()))
    {
        // Change Baud Rate
        if (!strcmp(name, BaudRateSP.name))
        {
            IUUpdateSwitch(&BaudRateSP, states, names, n);
            BaudRateSP.s = IPS_OK;
            IDSetSwitch(&BaudRateSP, nullptr);
            return true;
        }

        // Auto Search Devices on connection failure
        if (!strcmp(name, AutoSearchSP.name))
        {
            bool wasEnabled = (AutoSearchS[0].s == ISS_ON);

            IUUpdateSwitch(&AutoSearchSP, states, names, n);
            AutoSearchSP.s = IPS_OK;

            // Only display message if there is an actual change
            if (wasEnabled == false && AutoSearchS[0].s == ISS_ON)
                LOG_INFO("Auto search is enabled. When connecting, the driver shall attempt to "
                         "communicate with all available system ports until a connection is "
                         "established.");
            else if (wasEnabled && AutoSearchS[1].s == ISS_ON)
                LOG_INFO("Auto search is disabled.");
            IDSetSwitch(&AutoSearchSP, nullptr);

            return true;
        }

        // Refresh Serial Devices
        if (!strcmp(name, RefreshSP.name))
        {
            RefreshSP.s = Refresh() ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&RefreshSP, nullptr);
            return true;
        }

        // Check if a system port is selected.
        if (!strcmp(name, SystemPortSP.name))
        {
            IUUpdateSwitch(&SystemPortSP, states, names, n);

            int index = IUFindOnSwitchIndex(&SystemPortSP);
            if (index >= 0)
            {
                IUSaveText(&PortT[0], m_SystemPorts[index].c_str());
                IDSetText(&PortTP, nullptr);
            }

            SystemPortSP.s = IPS_OK;
            IDSetSwitch(&SystemPortSP, nullptr);
            return true;
        }
    }

    return false;
}

bool Serial::Connect()
{
    uint32_t baud = atoi(IUFindOnSwitch(&BaudRateSP)->name);
    if (Connect(PortT[0].text, baud) && processHandshake())
        return true;

    // Important, disconnect from port immediately
    // to release the lock, otherwise another driver will find it busy.
    tty_disconnect(PortFD);

    // Start auto-search if option was selected and IF we have system ports to try connecting to
    if (AutoSearchS[0].s == ISS_ON && SystemPortS != nullptr && SystemPortSP.nsp > 1)
    {
        LOGF_WARN("Communication with %s @ %d failed. Starting Auto Search...", PortT[0].text,
                  baud);

        std::this_thread::sleep_for(std::chrono::milliseconds(500 + (rand() % 1000)));

        // Try to connect "randomly" so that competing devices don't all try to connect to the same
        // ports at the same time.
        std::vector<std::string> systemPorts;
        for (int i = 0; i < SystemPortSP.nsp; i++)
        {
            // Only try the same port last again.
            if (!strcmp(m_SystemPorts[i].c_str(), PortT[0].text))
                continue;

            systemPorts.push_back(m_SystemPorts[i].c_str());
        }

        std::random_device rd;
        std::minstd_rand g(rd());
        std::shuffle(systemPorts.begin(), systemPorts.end(), g);

        std::vector<std::string> doubleSearch = systemPorts;

        // Try the current port as LAST port again
        systemPorts.push_back(PortT[0].text);

        // Double search just in case some items were BUSY in the first pass
        systemPorts.insert(systemPorts.end(), doubleSearch.begin(), doubleSearch.end());

        for (const auto &port : systemPorts)
        {
            LOGF_INFO("Trying connecting to %s @ %d ...", port.c_str(), baud);
            if (Connect(port.c_str(), baud) && processHandshake())
            {
                IUSaveText(&PortT[0], port.c_str());
                IDSetText(&PortTP, nullptr);

#ifdef __linux__
                // Disable auto-search on Linux if not disabled already
                if (AutoSearchS[INDI::DefaultDevice::INDI_ENABLED].s == ISS_ON)
                {
                    AutoSearchS[INDI::DefaultDevice::INDI_ENABLED].s = ISS_OFF;
                    AutoSearchS[INDI::DefaultDevice::INDI_DISABLED].s = ISS_ON;
                    IDSetSwitch(&AutoSearchSP, nullptr);
                    m_Device->saveConfig(true, AutoSearchSP.name);
                }

                // Save device port if different from configuration value and read-write.
                if (m_Permission != IP_RO && m_ConfigPort != std::string(PortT[0].text))
                    m_Device->saveConfig(true, INDI::SP::DEVICE_PORT);
#else
                // Do not overwrite custom ports because it can be actually cause
                // temporary failure. For users who use mapped named ports (e.g. /dev/mount), it's not good to override their choice.
                // So only write to config if the port was a system port.
                if (m_Permission != IP_RO && std::find(m_SystemPorts.begin(), m_SystemPorts.end(), PortT[0].text) != m_SystemPorts.end())
                    m_Device->saveConfig(true, INDI::SP::DEVICE_PORT);
#endif


                // If baud rate is different from config file, save it.
                if (m_Permission != IP_RO && IUFindOnSwitchIndex(&BaudRateSP) != m_ConfigBaudRate)
                    m_Device->saveConfig(true, INDI::SP::DEVICE_BAUD_RATE);

                return true;
            }

            tty_disconnect(PortFD);
            // sleep randomly anytime between 0.5s and ~1.5s
            // This enables different competing devices to connect
            std::this_thread::sleep_for(std::chrono::milliseconds(500 + (rand() % 1000)));
        }
    }

    return false;
}

bool Serial::processHandshake()
{
    LOG_DEBUG("Connection successful, attempting handshake...");
    bool rc = Handshake();
    if (rc)
    {
        LOGF_INFO("%s is online.", getDeviceName());
        // If permission is read-write and either the config port or baud rate are different from
        // configuration values, then save them.
        if (m_Permission != IP_RO && std::string(PortT[0].text) != m_ConfigPort)
            m_Device->saveConfig(true, INDI::SP::DEVICE_PORT);

        if (m_Permission != IP_RO && IUFindOnSwitchIndex(&BaudRateSP) != m_ConfigBaudRate)
            m_Device->saveConfig(true, INDI::SP::DEVICE_BAUD_RATE);
    }
    else
        LOG_DEBUG("Handshake failed.");

    return rc;
}

bool Serial::Connect(const char *port, uint32_t baud)
{
    if (m_Device->isSimulation())
        return true;

    int connectrc = 0;
    char errorMsg[MAXRBUF];

    LOGF_DEBUG("Connecting to %s @ %d", port, baud);

    if ((connectrc = tty_connect(port, baud, wordSize, parity, stopBits, &PortFD)) != TTY_OK)
    {
        if (connectrc == TTY_PORT_BUSY)
        {
            LOGF_WARN("Port %s is already used by another driver or process.", port);
            return false;
        }

        tty_error_msg(connectrc, errorMsg, MAXRBUF);
        LOGF_ERROR("Failed to connect to port (%s). Error: %s", port, errorMsg);
        return false;
    }

    LOGF_DEBUG("Port FD %d", PortFD);

    return true;
}

bool Serial::Disconnect()
{
    if (PortFD > 0)
    {
        tty_disconnect(PortFD);
        PortFD = -1;
    }
    return true;
}

void Serial::Activated()
{
    if (m_Permission != IP_RO)
        Refresh(true);
    m_Device->defineProperty(&PortTP);
    m_Device->defineProperty(&BaudRateSP);
    if (m_Permission != IP_RO)
    {
        m_Device->defineProperty(&AutoSearchSP);
        m_Device->defineProperty(&RefreshSP);
    }
}

void Serial::Deactivated()
{
    m_Device->deleteProperty(SystemPortSP.name);
    delete[] SystemPortS;
    SystemPortS = nullptr;
    m_Device->deleteProperty(PortTP.name);
    m_Device->deleteProperty(BaudRateSP.name);
    if (m_Permission != IP_RO)
    {
        m_Device->deleteProperty(AutoSearchSP.name);
        m_Device->deleteProperty(RefreshSP.name);
    }
}

bool Serial::saveConfigItems(FILE *fp)
{
    if (m_Permission != IP_RO)
    {
        IUSaveConfigText(fp, &PortTP);
        IUSaveConfigSwitch(fp, &BaudRateSP);
        IUSaveConfigSwitch(fp, &AutoSearchSP);
    }

    return true;
}

void Serial::setDefaultPort(const char *port)
{
    // JM 2021.09.19: Only set default port if configuration port was not loaded already.
    if (m_ConfigPort.empty())
        IUSaveText(&PortT[0], port);
    if (m_Device->isInitializationComplete())
        IDSetText(&PortTP, nullptr);
}

void Serial::setDefaultBaudRate(BaudRate newRate)
{
    // JM 2021.09.19: Only set default baud rate if configuration baud rate was not loaded already.
    if (m_ConfigBaudRate == -1)
    {
        IUResetSwitch(&BaudRateSP);
        BaudRateS[newRate].s = ISS_ON;
    }

    if (m_Device->isInitializationComplete())
        IDSetSwitch(&BaudRateSP, nullptr);
}

uint32_t Serial::baud()
{
    return atoi(IUFindOnSwitch(&BaudRateSP)->name);
}

int serial_dev_file_select(const dirent *entry)
{
#if defined(__APPLE__)
    static const char *filter_names[] = { "cu.", nullptr };
#else
    static const char *filter_names[] = { "ttyUSB", "ttyACM", nullptr };
#endif
    const char **filter;

    for (filter = filter_names; *filter; ++filter)
    {
        if (strstr(entry->d_name, *filter) != nullptr)
        {
            return (true);
        }
    }
    return (false);
}

int usb_dev_file_select(const dirent *entry)
{
    static const char *filter_names[] = { "usb-", nullptr };
    const char **filter;

    for (filter = filter_names; *filter; ++filter)
    {
        if (strstr(entry->d_name, *filter) != nullptr)
        {
            return (true);
        }
    }
    return (false);
}

int bluetooth_dev_file_select(const dirent *entry)
{
    static const char *filter_names[] = {"rfcomm", nullptr };
    const char **filter;

    for (filter = filter_names; *filter; ++filter)
    {
        if (strstr(entry->d_name, *filter) != nullptr)
        {
            return (true);
        }
    }
    return (false);
}

bool Serial::Refresh(bool silent)
{
    std::vector<std::string> m_Ports;

    // 0 Serial Only, 1 By USB-ID, 2 Bluetooth
    auto searchPath = [&](std::string prefix, uint8_t searchType)
    {
        struct dirent **namelist;
        std::vector<std::string> detectedDevices;
        int devCount = 0;
        if (searchType == SERIAL_DEV)
            devCount = scandir(prefix.c_str(), &namelist, serial_dev_file_select, alphasort);
        else if (searchType == USB_ID_DEV)
            devCount = scandir(prefix.c_str(), &namelist, usb_dev_file_select, alphasort);
        else
            devCount = scandir(prefix.c_str(), &namelist, bluetooth_dev_file_select, alphasort);
        if (devCount > 0)
        {
            while (devCount--)
            {
                if (detectedDevices.size() < 10)
                {
                    std::string s(namelist[devCount]->d_name);
                    s.erase(s.find_last_not_of(" \n\r\t") + 1);
                    detectedDevices.push_back(prefix + s);
                }
                else
                {
                    LOGF_DEBUG("Ignoring devices over %d : %s", detectedDevices.size(),
                               namelist[devCount]->d_name);
                }
                free(namelist[devCount]);
            }
            free(namelist);
        }

        return detectedDevices;
    };

#ifdef __linux__
    // Search for serial, usb, and bluetooth devices.
    const std::vector<std::string> serialDevices = searchPath("/dev/", SERIAL_DEV);
    const std::vector<std::string> usbIDDevices = searchPath("/dev/serial/by-id/", USB_ID_DEV);
    const std::vector<std::string> btDevices = searchPath("/dev/", BLUETOOTH_DEV);
    m_Ports.insert(m_Ports.end(), btDevices.begin(), btDevices.end());
    // Linux Kernel does not add identical VID:PID adapter to serial/by-id
    // Therefore, we check if there is a 1:1 correspondence between serial/by-id and /dev/ttyUSBX nodes
    // In case we have more by-id devices then /dev/ttyUSBX, we use them since these symlinks are more reusable in subsequence
    // sessions
    if (usbIDDevices.size() >= serialDevices.size())
        m_Ports.insert(m_Ports.end(), usbIDDevices.begin(), usbIDDevices.end());
    else
        m_Ports.insert(m_Ports.end(), serialDevices.begin(), serialDevices.end());
#else
    const std::vector<std::string> serialDevices = searchPath("/dev/", SERIAL_DEV);
    m_Ports.insert(m_Ports.end(), serialDevices.begin(), serialDevices.end());
#endif

    const int pCount = m_Ports.size();

    if (pCount == 0)
    {
        if (!silent)
            LOG_WARN("No candidate ports found on the system.");
        return false;
    }
    else
    {
        if (!silent)
            LOGF_INFO("Scan complete. Found %d port(s).", pCount);
    }

    // Check if anything changed and the property is already defined then we return.
    if (m_Ports == m_SystemPorts && SystemPortS)
    {
        m_Device->defineProperty(&SystemPortSP);
        return true;
    }

    m_SystemPorts = m_Ports;

    if (SystemPortS)
        m_Device->deleteProperty(SystemPortSP.name);

    delete[] SystemPortS;

    SystemPortS = new ISwitch[pCount];
    ISwitch *sp = SystemPortS;

    for (const auto &onePort : m_Ports)
    {
        // Simplify label by removing directory prefix
        std::string name = onePort.substr(onePort.find_last_of("/\\") + 1);
        std::string label = name;

        // Remove Linux extra stuff to simplify string
#ifdef __linux__
        std::regex re("usb-(.[^-]+)");
        std::smatch match;
        if (std::regex_search(onePort, match, re))
        {
            name = label = match.str(1);
            // Simplify further by removing non-unique strings
            std::regex target("FTDI_|UART_|USB_|Bridge_Controller_|to_");
            label = std::regex_replace(label, target, "");
            // Protect against too-short of a label
            if (label.length() <= 2)
                label = match.str(1);
        }
#endif
        IUFillSwitch(sp++, name.c_str(), label.c_str(), ISS_OFF);
    }

    IUFillSwitchVector(&SystemPortSP, SystemPortS, pCount, m_Device->getDeviceName(), "SYSTEM_PORTS", "System Ports",
                       CONNECTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    m_Device->defineProperty(&SystemPortSP);

    // If we have one physical port, set the current device port to this physical port
    // in case the default config port does not exist.
    if (pCount == 1 && m_ConfigPort.empty())
        IUSaveText(&PortT[0], m_Ports[0].c_str());
    return true;
}
}
