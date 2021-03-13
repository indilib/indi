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

namespace Connection
{
extern const char *CONNECTION_TAB;

Serial::Serial(INDI::DefaultDevice *dev) : Interface(dev, CONNECTION_SERIAL)
{
    char defaultPort[MAXINDINAME] = {0};
    // Try to load the port from the config file. If that fails, use default port.
    if (IUGetConfigText(dev->getDeviceName(), INDI::SP::DEVICE_PORT, "PORT", defaultPort, MAXINDINAME) < 0)
    {
#ifdef __APPLE__
        strncpy(defaultPort, "/dev/cu.usbserial", MAXINDINAME);
#else
        strncpy(defaultPort, "/dev/ttyUSB0", MAXINDINAME);
#endif
    }
    IUFillText(&PortT[0], "PORT", "Port", defaultPort);
    IUFillTextVector(&PortTP, PortT, 1, dev->getDeviceName(), INDI::SP::DEVICE_PORT, "Ports", CONNECTION_TAB, IP_RW, 60,
                     IPS_IDLE);

    AutoSearchSP[INDI::DefaultDevice::INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_ON);
    AutoSearchSP[INDI::DefaultDevice::INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_OFF);
    AutoSearchSP.fill(dev->getDeviceName(), INDI::SP::DEVICE_AUTO_SEARCH, "Auto Search",
                       CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    RefreshSP[0].fill("Scan Ports", "Scan Ports", ISS_OFF);
    RefreshSP.fill(dev->getDeviceName(), "DEVICE_PORT_SCAN", "Refresh", CONNECTION_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    BaudRateSP[0].fill("9600", "", ISS_ON);
    BaudRateSP[1].fill("19200", "", ISS_OFF);
    BaudRateSP[2].fill("38400", "", ISS_OFF);
    BaudRateSP[3].fill("57600", "", ISS_OFF);
    BaudRateSP[4].fill("115200", "", ISS_OFF);
    BaudRateSP[5].fill("230400", "", ISS_OFF);
    BaudRateSP.fill(dev->getDeviceName(), INDI::SP::DEVICE_BAUD_RATE, "Baud Rate", CONNECTION_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
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

            if (SystemPortS)
            {
                bool isSystemPort = false;
                for (int i = 0; i < SystemPortSP.nsp; i++)
                {
                    if (!strcmp(PortT[0].text, SystemPortS[i].label))
                    {
                        isSystemPort = true;
                        break;
                    }
                }
                if (isSystemPort == false)
                {
                    LOGF_DEBUG("Auto search is disabled because %s is not a system port.", PortT[0].text);
                    AutoSearchSP[0].setState(ISS_OFF);
                    AutoSearchSP[1].setState(ISS_ON);
                    AutoSearchSP.apply();
                }
            }
            return true;
        }
    }

    return false;
}

bool Serial::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(dev, m_Device->getDeviceName()))
    {
        if (BaudRateSP.isNameMatch(name))
        {
            BaudRateSP.update(states, names, n);
            BaudRateSP.setState(IPS_OK);
            BaudRateSP.apply();
            return true;
        }

        if (AutoSearchSP.isNameMatch(name))
        {
            bool wasEnabled = (AutoSearchSP[0].getState() == ISS_ON);

            AutoSearchSP.update(states, names, n);
            AutoSearchSP.setState(IPS_OK);

            // Only display message if there is an actual change
            if (wasEnabled == false && AutoSearchSP[0].s == ISS_ON)
                LOG_INFO("Auto search is enabled. When connecting, the driver shall attempt to "
                         "communicate with all available system ports until a connection is "
                         "established.");
            else if (wasEnabled && AutoSearchSP[1].s == ISS_ON)
                LOG_INFO("Auto search is disabled.");
            AutoSearchSP.apply();

            return true;
        }

        if (RefreshSP.isNameMatch(name))
        {
            RefreshSP.setState(Refresh() ? IPS_OK : IPS_ALERT);
            RefreshSP.apply();
            return true;
        }

        if (!strcmp(name, SystemPortSP.name))
        {
            IUUpdateSwitch(&SystemPortSP, states, names, n);

            ISwitch *sp = IUFindOnSwitch(&SystemPortSP);
            if (sp)
            {
                IUSaveText(&PortT[0], sp->name);
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
    uint32_t baud = atoi(BaudRateSP.findOnSwitch()->name);
    if (Connect(PortT[0].text, baud) && processHandshake())
        return true;

    // If if the user port is one of the detected system ports.
    bool isSystemPort = false;

    if (SystemPortS != nullptr)
    {
        for (int i = 0; i < SystemPortSP.nsp; i++)
        {
            if (!strcmp(PortT[0].text, SystemPortS[i].name))
            {
                isSystemPort = true;
                break;
            }
        }
    }

    // Important, disconnect from port immediately
    // to release the lock, otherwise another driver will find it busy.
    tty_disconnect(PortFD);

    // Start auto-search if option was selected and IF we have system ports to try connecting to
    if (AutoSearchSP[0].getState() == ISS_ON && SystemPortS != nullptr && SystemPortSP.nsp > 1)
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
            if (!strcmp(SystemPortS[i].name, PortT[0].text))
                continue;

            systemPorts.push_back(SystemPortS[i].name);
        }
        std::random_shuffle (systemPorts.begin(), systemPorts.end());

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

                // Do not overwrite custom ports because it can be actually some
                // temporary failure. For users who use mapped named ports (e.g. /dev/mount), it's not good to override their choice.
                // So only write to config if the port was a system port.
                if (isSystemPort)
                    m_Device->saveConfig(true, PortTP.name);
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
        m_Device->saveConfig(true, INDI::SP::DEVICE_PORT);
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
    m_Device->defineProperty(&PortTP);
    m_Device->loadConfig(true, INDI::SP::DEVICE_PORT);

    m_Device->defineProperty(BaudRateSP);
    m_Device->loadConfig(true, INDI::SP::DEVICE_BAUD_RATE);

    m_Device->defineProperty(AutoSearchSP);
    m_Device->loadConfig(true, INDI::SP::DEVICE_AUTO_SEARCH);

    m_Device->defineProperty(RefreshSP);
    Refresh(true);
}

void Serial::Deactivated()
{
    m_Device->deleteProperty(PortTP.name);
    m_Device->deleteProperty(BaudRateSP.getName());
    m_Device->deleteProperty(AutoSearchSP.getName());

    m_Device->deleteProperty(RefreshSP.getName());
    m_Device->deleteProperty(SystemPortSP.name);
    delete[] SystemPortS;
    SystemPortS = nullptr;
}

bool Serial::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &PortTP);
    BaudRateSP.save(fp);
    AutoSearchSP.save(fp);

    return true;
}

void Serial::setDefaultPort(const char *defaultPort)
{
    IUSaveText(&PortT[0], defaultPort);
}

void Serial::setDefaultBaudRate(BaudRate newRate)
{
    BaudRateSP.reset();
    BaudRateSP[newRate].setState(ISS_ON);
}

uint32_t Serial::baud()
{
    return atoi(BaudRateSP.findOnSwitch()->name);
}

int dev_file_select(const dirent *entry)
{
#if defined(__APPLE__)
    static const char *filter_names[] = { "cu.", nullptr };
#else
    static const char *filter_names[] = { "ttyUSB", "ttyACM", "rfcomm", nullptr };
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

bool Serial::Refresh(bool silent)
{
    std::vector<std::string> m_CurrentPorts;
    if (SystemPortS && SystemPortSP.nsp > 0)
    {
        for (uint8_t i = 0; i < SystemPortSP.nsp; i++)
            m_CurrentPorts.push_back(SystemPortS[i].name);
    }

    std::vector<std::string> m_Ports;

    struct dirent **namelist;
    int devCount = scandir("/dev", &namelist, dev_file_select, alphasort);
    if (devCount < 0)
    {
        if (!silent)
            LOGF_ERROR("Failed to scan directory /dev. Error: %s", strerror(errno));
    }
    else
    {
        while (devCount--)
        {
            if (m_Ports.size() < 10)
            {
                std::string s(namelist[devCount]->d_name);
                s.erase(s.find_last_not_of(" \n\r\t") + 1);
                m_Ports.push_back("/dev/" + s);
            }
            else
            {
                LOGF_DEBUG("Ignoring devices over %d : %s", m_Ports.size(),
                           namelist[devCount]->d_name);
            }
            free(namelist[devCount]);
        }
        free(namelist);
    }

    int pCount = m_Ports.size();

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

    // Check if anything changed. If not we return.
    if (m_Ports == m_CurrentPorts)
        return true;

    if (SystemPortS)
        m_Device->deleteProperty(SystemPortSP.name);

    delete[] SystemPortS;

    SystemPortS = new ISwitch[pCount];
    ISwitch *sp = SystemPortS;

    for (int i = pCount - 1; i >= 0; i--)
    {
        IUFillSwitch(sp++, m_Ports[i].c_str(), m_Ports[i].c_str(), ISS_OFF);
    }

    IUFillSwitchVector(&SystemPortSP, SystemPortS, pCount, m_Device->getDeviceName(), "SYSTEM_PORTS", "System Ports",
                       CONNECTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    m_Device->defineProperty(&SystemPortSP);

    // JM 2020-08-30: If we only have ONE serial port on the system
    // We check if the current port is default port. If it is, then we
    // set the port to the discovered port. This is useful because sometimes
    // the discovered port is something like /dev/ttyUSB1, but default is /dev/ttyUSB0 would
    // fail to connect, so we set it to the discovered port only if it matches this criteria.
    if (pCount == 1)
    {
        bool match = false;
#ifdef __APPLE__
        match = !strcmp(PortT[0].text, "/dev/cu.usbserial");
#else
        match = !strcmp(PortT[0].text, "/dev/ttyUSB0");
#endif
        if (match)
        {
            IUSaveText(&PortT[0], m_Ports[0].c_str());
        }
    }
    return true;
}
}
