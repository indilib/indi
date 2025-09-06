/*
    I2C Connection Plugin
    Copyright (C) 2025 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "connectioni2c.h"
#include "indistandardproperty.h"
#include "indicom.h"
#include "indilogger.h"

#ifdef __linux__
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#endif
#include <cerrno>
#include <cstring>

namespace Connection
{

extern const char *CONNECTION_TAB;

I2C::I2C(INDI::DefaultDevice *dev, IPerm permission) : Interface(dev, CONNECTION_I2C), m_Permission(permission)
{
    I2CConnectionTP[I2C_BUS].fill("BUS", "Bus Path", "/dev/i2c-1");
    I2CConnectionTP[I2C_ADDRESS].fill("ADDRESS", "Address (hex)", "0x28");
    I2CConnectionTP.fill(dev->getDeviceName(), "I2C_CONNECTION", "I2C Connection", CONNECTION_TAB, m_Permission, 60, IPS_IDLE);

    char configBusPath[256] = {0};
    if (IUGetConfigText(dev->getDeviceName(), "I2C_CONNECTION", "BUS", configBusPath, 256) == 0)
    {
        m_ConfigBusPath = configBusPath;
        I2CConnectionTP[I2C_BUS].setText(configBusPath);
    }

    char configAddress[256] = {0};
    if (IUGetConfigText(dev->getDeviceName(), "I2C_CONNECTION", "ADDRESS", configAddress, 256) == 0)
    {
        m_ConfigAddress = strtol(configAddress, nullptr, 16);
        I2CConnectionTP[I2C_ADDRESS].setText(configAddress);
    }
}

I2C::~I2C()
{
}

bool I2C::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    if (!strcmp(dev, m_Device->getDeviceName()))
    {
        if (I2CConnectionTP.isNameMatch(name))
        {
            I2CConnectionTP.update(texts, names, n);
            I2CConnectionTP.setState(IPS_OK);
            I2CConnectionTP.apply();
            return true;
        }
    }

    return false;
}

bool I2C::Connect()
{
#ifndef __linux__
    LOG_ERROR("I2C connection is not supported on this platform.");
    return false;
#else
    uint8_t addr = static_cast<uint8_t>(strtol(I2CConnectionTP[I2C_ADDRESS].getText(), nullptr, 16));
    if (Connect(I2CConnectionTP[I2C_BUS].getText(), addr) && processHandshake())
        return true;

    // Important, disconnect from I2C immediately
    // to release the lock, otherwise another driver will find it busy.
    if (PortFD > 0)
    {
        close(PortFD);
        PortFD = -1;
    }

    return false;
#endif
}

bool I2C::processHandshake()
{
    LOG_DEBUG("I2C connection successful, attempting handshake...");
    bool rc = Handshake();
    if (rc)
    {
        LOGF_INFO("%s is online.", getDeviceName());
        // If permission is read-write and either the config bus path or address are different from
        // configuration values, then save them.
        if (m_Permission != IP_RO && (std::string(I2CConnectionTP[I2C_BUS].getText()) != m_ConfigBusPath ||
                                      static_cast<int>(strtol(I2CConnectionTP[I2C_ADDRESS].getText(), nullptr, 16)) != m_ConfigAddress))
            m_Device->saveConfig(true, I2CConnectionTP.getName());
    }
    else
        LOG_DEBUG("Handshake failed.");

    return rc;
}

bool I2C::Connect(const char *busPath, uint8_t addr)
{
    if (m_Device->isSimulation())
        return true;

#ifndef __linux__
    INDI_UNUSED(busPath);
    INDI_UNUSED(addr);
    LOG_ERROR("I2C connection is not supported on this platform.");
    return false;
#else
    LOGF_DEBUG("Connecting to I2C device at address 0x%02X on bus %s", addr, busPath);

    PortFD = open(busPath, O_RDWR);
    if (PortFD < 0)
    {
        LOGF_ERROR("Failed to open I2C bus %s. Error: %s", busPath, strerror(errno));
        return false;
    }

    if (ioctl(PortFD, I2C_SLAVE, addr) < 0)
    {
        LOGF_ERROR("Failed to set I2C address 0x%02X on bus %s. Error: %s", addr, busPath, strerror(errno));
        close(PortFD);
        PortFD = -1;
        return false;
    }

    LOGF_DEBUG("I2C FD %d", PortFD);

    return true;
#endif
}

bool I2C::Disconnect()
{
#ifdef __linux__
    if (PortFD > 0)
    {
        close(PortFD);
        PortFD = -1;
    }
#endif
    return true;
}

void I2C::Activated()
{
    m_Device->defineProperty(I2CConnectionTP);
}

void I2C::Deactivated()
{
    m_Device->deleteProperty(I2CConnectionTP.getName());
}

bool I2C::saveConfigItems(FILE *fp)
{
    if (m_Permission != IP_RO)
    {
        I2CConnectionTP.save(fp);
    }

    return true;
}

const char *I2C::busPath()
{
    return I2CConnectionTP[I2C_BUS].getText();
}

void I2C::setDefaultBusPath(const char *path)
{
    // Only set default bus path if configuration path was not loaded already.
    if (m_ConfigBusPath.empty())
        I2CConnectionTP[I2C_BUS].setText(path);
    if (m_Device->isInitializationComplete())
        m_Device->defineProperty(I2CConnectionTP);
}

void I2C::setDefaultAddress(uint8_t addr)
{
    // Only set default address if configuration address was not loaded already.
    if (m_ConfigAddress == -1)
    {
        char hexAddr[5];
        snprintf(hexAddr, sizeof(hexAddr), "0x%02X", addr);
        I2CConnectionTP[I2C_ADDRESS].setText(hexAddr);
    }

    if (m_Device->isInitializationComplete())
        m_Device->defineProperty(I2CConnectionTP);
}

uint8_t I2C::address()
{
    return static_cast<uint8_t>(strtol(I2CConnectionTP[I2C_ADDRESS].getText(), nullptr, 16));
}

}
