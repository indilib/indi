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
#include "serialportnames.h"
#include "indistandardproperty.h"
#include "indicom.h"
#include "indilogger.h"

#include <dirent.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>
#include <regex>
#include <random>
#include <map>
#include <mutex>

namespace Connection
{
extern const char *CONNECTION_TAB;

namespace
{
// Per connection state, kept outside the class so that its size stays the same for drivers built against
// an older header.
struct SerialState
{
    // Port opened by the last successful Connect(port, baud)
    std::string openedPort;
    // USB vendor:product ID of the adapter last talked to, searched first by an auto search
    std::string adapterType;
    // While an auto search runs, busy and missing ports are expected and only counted
    bool searching {false};
    int busyPorts {0};
};

std::mutex stateMutex;
std::map<const Serial *, SerialState> states;

SerialState &state(const Serial *serial)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return states[serial];
}

// True if port is one of the system ports, which are listed under their stable names, or the kernel device node
// of one of them. A custom name (e.g. a udev symlink) is not a system port.
bool isSystemPort(const std::vector<std::string> &systemPorts, const std::string &port)
{
    return std::any_of(systemPorts.begin(), systemPorts.end(), [&port](const std::string & onePort)
    {
        return onePort == port || (SerialPorts::isKernelNode(port) && SerialPorts::sameDevice(onePort, port));
    });
}

// Replace a port name that can change after a reboot (e.g. /dev/ttyUSB0) by a stable one.
void useStablePortName(INDI::PropertyText &portTP, const char *deviceName)
{
    const std::string current = portTP[0].getText();
    const std::string stable = SerialPorts::nameToSave(current);
    if (stable == current)
        return;

    DEBUGFDEVICE(deviceName, INDI::Logger::DBG_SESSION, "Using %s for %s, this name stays the same after a reboot.",
                 stable.c_str(), current.c_str());
    portTP[0].setText(stable);
    portTP.apply();
}
}

Serial::Serial(INDI::DefaultDevice *dev, IPerm permission) : Interface(dev, CONNECTION_SERIAL), m_Permission(permission)
{
    // State of a connection that used the same address before.
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        states.erase(this);
    }

    char configPort[256] = {0};
    // Try to load the port from the config file. If that fails, use default port.
    if (IUGetConfigText(dev->getDeviceName(), INDI::SP::DEVICE_PORT, "PORT", configPort, 256) == 0)
    {
        m_ConfigPort = configPort;
        PortTP[0].fill("PORT", "Port", configPort);
    }
    else
    {
#ifdef __APPLE__
        PortTP[0].fill("PORT", "Port", "/dev/cu.usbserial");
#else
        PortTP[0].fill("PORT", "Port", "/dev/ttyUSB0");
#endif
    }
    PortTP.fill(dev->getDeviceName(), INDI::SP::DEVICE_PORT, "Ports", CONNECTION_TAB, m_Permission, 60, IPS_IDLE);

    int autoSearchIndex = 0;
    // Try to load the port from the config file. If that fails, use default port.
    IUGetConfigOnSwitchIndex(dev->getDeviceName(), INDI::SP::DEVICE_AUTO_SEARCH, &autoSearchIndex);
    AutoSearchSP[INDI::DefaultDevice::INDI_ENABLED].fill("INDI_ENABLED", "Enabled", autoSearchIndex == 0 ? ISS_ON : ISS_OFF);
    AutoSearchSP[INDI::DefaultDevice::INDI_DISABLED].fill("INDI_DISABLED", "Disabled", autoSearchIndex == 0 ? ISS_OFF : ISS_ON);
    AutoSearchSP.fill(dev->getDeviceName(), INDI::SP::DEVICE_AUTO_SEARCH, "Auto Search", CONNECTION_TAB, IP_RW, ISR_1OFMANY,
                      60, IPS_IDLE);

    RefreshSP[0].fill("Scan Ports", "Scan Ports", ISS_OFF);
    RefreshSP.fill(dev->getDeviceName(), "DEVICE_PORT_SCAN", "Refresh", CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    BaudRateSP[B_1200].fill("1200", "", ISS_OFF);
    BaudRateSP[B_2400].fill("2400", "", ISS_OFF);
    BaudRateSP[B_4800].fill("4800", "", ISS_OFF);
    BaudRateSP[B_9600].fill("9600", "", ISS_ON);
    BaudRateSP[B_19200].fill("19200", "", ISS_OFF);
    BaudRateSP[B_38400].fill("38400", "", ISS_OFF);
    BaudRateSP[B_57600].fill("57600", "", ISS_OFF);
    BaudRateSP[B_115200].fill("115200", "", ISS_OFF);
    BaudRateSP[B_230400].fill("230400", "", ISS_OFF);
    BaudRateSP.fill(dev->getDeviceName(), INDI::SP::DEVICE_BAUD_RATE, "Baud Rate", CONNECTION_TAB, m_Permission, ISR_1OFMANY,
                    60, IPS_IDLE);

    // Try to load the baud rate from the config file. Look it up by name and not by index since
    // configuration files written before slower baud rates were added have a different switch order.
    char configBaudRate[MAXINDINAME] = {0};
    if (IUGetConfigOnSwitchName(dev->getDeviceName(), INDI::SP::DEVICE_BAUD_RATE, configBaudRate, MAXINDINAME) == 0)
        m_ConfigBaudRate = BaudRateSP.findWidgetIndexByName(configBaudRate);
    // If we have a valid config entry, set it.
    if (m_ConfigBaudRate >= 0)
    {
        BaudRateSP.reset();
        BaudRateSP[m_ConfigBaudRate].setState(ISS_ON);
    }
}

Serial::~Serial() = default;

bool Serial::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev, m_Device->getDeviceName()))
    {
        // Serial Port
        if (PortTP.isNameMatch(name))
        {
            PortTP.update(texts, names, n);
            PortTP.setState(IPS_OK);
            PortTP.apply();

            // Auto search only makes sense when the user is not using a custom (non-system) port.
            // Only act (and report it) when auto search is currently enabled, so that no redundant
            // message is sent and no property update is pushed when it is already disabled.
            if (!isSystemPort(m_SystemPorts, PortTP[0].getText())
                    && AutoSearchSP[INDI::DefaultDevice::INDI_ENABLED].getState() == ISS_ON)
            {
                LOGF_DEBUG("Auto search is disabled because %s is not a system port.", PortTP[0].getText());
                AutoSearchSP[INDI::DefaultDevice::INDI_ENABLED].setState(ISS_OFF);
                AutoSearchSP[INDI::DefaultDevice::INDI_DISABLED].setState(ISS_ON);
                AutoSearchSP.apply();
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
        if (BaudRateSP.isNameMatch(name))
        {
            BaudRateSP.update(states, names, n);
            BaudRateSP.setState(IPS_OK);
            BaudRateSP.apply();
            return true;
        }

        // Auto Search Devices on connection failure
        if (AutoSearchSP.isNameMatch(name))
        {
            bool wasEnabled = (AutoSearchSP[INDI::DefaultDevice::INDI_ENABLED].getState() == ISS_ON);

            AutoSearchSP.update(states, names, n);
            AutoSearchSP.setState(IPS_OK);

            // Only display message if there is an actual change
            if (wasEnabled == false && AutoSearchSP[INDI::DefaultDevice::INDI_ENABLED].getState() == ISS_ON)
                LOG_INFO("Auto search is enabled. When connecting, the driver shall attempt to "
                         "communicate with all available system ports until a connection is "
                         "established.");
            else if (wasEnabled && AutoSearchSP[INDI::DefaultDevice::INDI_DISABLED].getState() == ISS_ON)
                LOG_INFO("Auto search is disabled.");
            AutoSearchSP.apply();

            return true;
        }

        // Refresh Serial Devices
        if (RefreshSP.isNameMatch(name))
        {
            RefreshSP.setState(Refresh() ? IPS_OK : IPS_ALERT);
            RefreshSP.apply();
            return true;
        }

        // Check if a system port is selected.
        if (SystemPortSP.isNameMatch(name))
        {
            SystemPortSP.update(states, names, n);

            int index = SystemPortSP.findOnSwitchIndex();
            if (index >= 0)
            {
                PortTP[0].setText(m_SystemPorts[index]);
                PortTP.apply();
            }

            SystemPortSP.setState(IPS_OK);
            SystemPortSP.apply();
            return true;
        }
    }

    return false;
}

bool Serial::Connect()
{
    uint32_t baud = this->baud();
    if (Connect(PortTP[0].getText(), baud) && processHandshake())
        return true;

    // Important, disconnect from port immediately
    // to release the lock, otherwise another driver will find it busy.
    tty_disconnect(PortFD);

    // Collect the alternative system ports to try, excluding the port we just attempted.
    std::vector<std::string> systemPorts;
    for (const auto &onePort : m_SystemPorts)
    {
        // Only try the same port last again.
        if (onePort == PortTP[0].getText() || SerialPorts::sameDevice(onePort, PortTP[0].getText()))
            continue;

        systemPorts.push_back(onePort);
    }

    // Start auto-search if option was selected and IF we have system ports to try connecting to
    if (AutoSearchSP[INDI::DefaultDevice::INDI_ENABLED].getState() == ISS_ON && !SystemPortSP.isEmpty() && !systemPorts.empty())
    {
        LOGF_WARN("Communication with %s @ %d failed. Starting Auto Search...", PortTP[0].getText(),
                  baud);

        std::this_thread::sleep_for(std::chrono::milliseconds(500 + (rand() % 1000)));

        // Try to connect "randomly" so that competing devices don't all try to connect to the same
        // ports at the same time.
        std::random_device rd;
        std::minstd_rand g(rd());
        std::shuffle(systemPorts.begin(), systemPorts.end(), g);

        // Look for the same kind of adapter first (see SerialPorts::searchOrder).
        systemPorts = SerialPorts::searchOrder(systemPorts, PortTP[0].getText(), state(this).adapterType);

        std::vector<std::string> doubleSearch = systemPorts;

        // Try the current port as LAST port again
        systemPorts.push_back(PortTP[0].getText());

        // Double search just in case some items were BUSY in the first pass
        systemPorts.insert(systemPorts.end(), doubleSearch.begin(), doubleSearch.end());

        SerialState &search = state(this);
        search.searching = true;
        search.busyPorts = 0;
        for (const auto &port : systemPorts)
        {
            LOGF_DEBUG("Trying connecting to %s @ %d ...", port.c_str(), baud);
            if (Connect(port.c_str(), baud) && processHandshake())
            {
                search.searching = false;
                PortTP[0].setText(port);
                PortTP.apply();

#ifdef __linux__
                // Save the port under a name that survives a reboot. Auto search stays enabled (also in the
                // configuration), so the device is found again if it is moved to another USB socket.
                if (m_Permission != IP_RO)
                {
                    useStablePortName(PortTP, m_Device->getDeviceName());
                    if (m_ConfigPort != PortTP[0].getText())
                    {
                        m_Device->saveConfig(true, INDI::SP::DEVICE_PORT);
                        m_ConfigPort = PortTP[0].getText();
                    }
                    m_Device->saveConfig(true, AutoSearchSP.getName());
                }
#else
                // Do not overwrite custom ports because it can be actually cause
                // temporary failure. For users who use mapped named ports (e.g. /dev/mount), it's not good to override their choice.
                // So only write to config if the port was a system port.
                if (m_Permission != IP_RO
                        && std::find(m_SystemPorts.begin(), m_SystemPorts.end(), PortTP[0].getText()) != m_SystemPorts.end())
                    m_Device->saveConfig(true, INDI::SP::DEVICE_PORT);
#endif


                // If baud rate is different from config file, save it.
                if (m_Permission != IP_RO && BaudRateSP.findOnSwitchIndex() != m_ConfigBaudRate)
                    m_Device->saveConfig(true, INDI::SP::DEVICE_BAUD_RATE);

                return true;
            }

            tty_disconnect(PortFD);
            // sleep randomly anytime between 0.5s and ~1.5s
            // This enables different competing devices to connect
            std::this_thread::sleep_for(std::chrono::milliseconds(500 + (rand() % 1000)));
        }
        search.searching = false;

        LOGF_WARN("Auto Search: %s was not found on %d port(s) (%d attempt(s) found a port in use by another driver). "
                  "Check that the device is powered and plugged in.", getDeviceName(),
                  static_cast<int>(doubleSearch.size() + 1), search.busyPorts);
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
        SerialState &connection = state(this);
        connection.adapterType = SerialPorts::adapterType(connection.openedPort);
        // The configured port answered (during an auto search Connect() handles the port it found): store it
        // under a name that survives a reboot.
        if (m_Permission != IP_RO && connection.openedPort == PortTP[0].getText())
            useStablePortName(PortTP, m_Device->getDeviceName());
        // If permission is read-write and either the config port or baud rate are different from
        // configuration values, then save them.
        if (m_Permission != IP_RO && PortTP[0].getText() != m_ConfigPort)
        {
            m_Device->saveConfig(true, INDI::SP::DEVICE_PORT);
            m_ConfigPort = PortTP[0].getText();
        }

        if (m_Permission != IP_RO && BaudRateSP.findOnSwitchIndex() != m_ConfigBaudRate)
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

    SerialState &connection = state(this);
    if ((connectrc = tty_connect(port, baud, wordSize, parity, stopBits, &PortFD)) != TTY_OK)
    {
        // During an auto search busy and missing ports are expected, Connect() reports the result once.
        if (connectrc == TTY_PORT_BUSY)
        {
            if (connection.searching)
            {
                connection.busyPorts++;
                LOGF_DEBUG("Port %s is already used by another driver or process.", port);
            }
            else
                LOGF_WARN("Port %s is already used by another driver or process.", port);
            return false;
        }

        tty_error_msg(connectrc, errorMsg, MAXRBUF);
        if (connection.searching)
            LOGF_DEBUG("Failed to connect to port (%s). Error: %s", port, errorMsg);
        else
            LOGF_ERROR("Failed to connect to port (%s). Error: %s", port, errorMsg);
        return false;
    }

    LOGF_DEBUG("Port FD %d", PortFD);
    connection.openedPort = port;

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
    m_Device->defineProperty(PortTP);
    m_Device->defineProperty(BaudRateSP);
    if (m_Permission != IP_RO)
    {
        m_Device->defineProperty(AutoSearchSP);
        m_Device->defineProperty(RefreshSP);
    }
}

void Serial::Deactivated()
{
    if (!SystemPortSP.isEmpty())
    {
        m_Device->deleteProperty(SystemPortSP);
        SystemPortSP.resize(0);
    }
    m_Device->deleteProperty(PortTP);
    m_Device->deleteProperty(BaudRateSP);
    if (m_Permission != IP_RO)
    {
        m_Device->deleteProperty(AutoSearchSP);
        m_Device->deleteProperty(RefreshSP);
    }
}

bool Serial::saveConfigItems(FILE *fp)
{
    if (m_Permission != IP_RO)
    {
        PortTP.save(fp);
        BaudRateSP.save(fp);
        AutoSearchSP.save(fp);
    }

    return true;
}

void Serial::setDefaultPort(const char *port)
{
    // JM 2021.09.19: Only set default port if configuration port was not loaded already.
    if (m_ConfigPort.empty())
        PortTP[0].setText(port);
    if (m_Device->isInitializationComplete())
        PortTP.apply();
}

void Serial::setDefaultBaudRate(BaudRate newRate)
{
    // JM 2021.09.19: Only set default baud rate if configuration baud rate was not loaded already.
    if (m_ConfigBaudRate == -1)
    {
        BaudRateSP.reset();
        BaudRateSP[newRate].setState(ISS_ON);
    }

    if (m_Device->isInitializationComplete())
        BaudRateSP.apply();
}

void Serial::setPortMatchPattern(const std::string &pattern)
{
    m_PortMatchPattern = pattern;
}

uint32_t Serial::baud()
{
    auto onSwitch = BaudRateSP.findOnSwitch();
    return onSwitch ? atoi(onSwitch->getName()) : 9600;
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
    // Bluetooth and USB serial devices, the latter under names that stay the same after a reboot.
    const std::vector<std::string> btDevices = searchPath("/dev/", BLUETOOTH_DEV);
    const std::vector<std::string> usbDevices = SerialPorts::list();
    m_Ports.insert(m_Ports.end(), btDevices.begin(), btDevices.end());
    m_Ports.insert(m_Ports.end(), usbDevices.begin(), usbDevices.end());
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
    if (m_Ports == m_SystemPorts && !SystemPortSP.isEmpty())
    {
        m_Device->defineProperty(SystemPortSP);
        return true;
    }

    m_SystemPorts = m_Ports;

    if (!SystemPortSP.isEmpty())
        m_Device->deleteProperty(SystemPortSP);

    SystemPortSP.resize(0);

    for (const auto &onePort : m_Ports)
    {
        // Simplify label by removing directory prefix
        std::string name = onePort.substr(onePort.find_last_of("/\\") + 1);
        std::string label = name;

        // Remove Linux extra stuff to simplify string
#ifdef __linux__
        std::regex re("usb-(.[^-]+)");
        std::smatch match;
        const std::string socketLabel = SerialPorts::label(onePort);
        if (!socketLabel.empty())
            label = socketLabel;
        else if (std::regex_search(onePort, match, re))
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
        INDI::WidgetSwitch node;
        node.fill(name, label, ISS_OFF);
        SystemPortSP.push(std::move(node));
    }

    SystemPortSP.fill(m_Device->getDeviceName(), "SYSTEM_PORTS", "System Ports", CONNECTION_TAB, IP_RW, ISR_ATMOST1, 60,
                      IPS_IDLE);

    m_Device->defineProperty(SystemPortSP);

    // Check whether the saved config port is still usable: it is either among the detected system
    // ports, or it still exists on disk (e.g. a custom or mapped port like /dev/mount).
    const bool configPortAvailable =
        !m_ConfigPort.empty() &&
        (std::find(m_SystemPorts.begin(), m_SystemPorts.end(), m_ConfigPort) != m_SystemPorts.end() ||
         access(m_ConfigPort.c_str(), F_OK) == 0);

    // If we have one physical port, set the current device port to this physical port
    // in case the config port is either not defined or no longer available.
    if (pCount == 1 && !configPortAvailable)
    {
        if (!m_ConfigPort.empty())
            LOGF_INFO("Saved port %s is no longer available. Using the only detected port %s instead.",
                      m_ConfigPort.c_str(), m_Ports[0].c_str());

        PortTP[0].setText(m_Ports[0]);
        if (m_Device->isInitializationComplete())
            PortTP.apply();
    }

    // If a port match pattern is registered, try to auto-select the best matching port.
    // This is only done when no previously-saved config port is present among the discovered
    // ports (e.g. first run, or device plugged into a different USB slot).
    // Devices like Pegasus Astro embed their product name in the /dev/serial/by-id/ symlink,
    // which makes it possible to pick the right port without blind handshake attempts.
    if (!m_PortMatchPattern.empty())
    {
        if (!configPortAvailable)
        {
            try
            {
                const std::regex portRe(m_PortMatchPattern, std::regex::icase);
                for (const auto &onePort : m_SystemPorts)
                {
                    if (std::regex_search(onePort, portRe))
                    {
                        LOGF_INFO("Auto-selected port %s based on device name pattern \"%s\".",
                                  onePort.c_str(), m_PortMatchPattern.c_str());
                        PortTP[0].setText(onePort);
                        PortTP.apply();
                        break;
                    }
                }
            }
            catch (const std::regex_error &e)
            {
                LOGF_WARN("Port match pattern \"%s\" is not a valid regex: %s",
                          m_PortMatchPattern.c_str(), e.what());
            }
        }
    }

    return true;
}
}
