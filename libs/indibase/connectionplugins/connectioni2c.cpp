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
    char configBusPath[256] = {0};
    // Try to load the bus path from the config file. If that fails, use default path.
    if (IUGetConfigText(dev->getDeviceName(), "I2C_BUS", "BUS_PATH", configBusPath, 256) == 0)
    {
        m_ConfigBusPath = configBusPath;
        I2CBusTP[0].fill("BUS_PATH", "Bus Path", configBusPath);
    }
    else
    {
        I2CBusTP[0].fill("BUS_PATH", "Bus Path", "/dev/i2c-1");
    }
    I2CBusTP.fill(dev->getDeviceName(), "I2C_BUS", "I2C Bus", CONNECTION_TAB, m_Permission, 60, IPS_IDLE);

    // Try to load the address from the config file. If that fails, use default address.
    IUGetConfigOnSwitchIndex(dev->getDeviceName(), "I2C_ADDRESS", &m_ConfigAddress);
    I2CAddressNP[0].fill("ADDRESS", "Address (hex)", "0x%02X", 0x03, 0x77, 1, 0x28);
    I2CAddressNP.fill(dev->getDeviceName(), "I2C_ADDRESS", "I2C Address", CONNECTION_TAB, m_Permission, 60, IPS_IDLE);

    // If we have a valid config entry, set it.
    if (m_ConfigAddress >= 0)
    {
        I2CAddressNP[0].setValue(m_ConfigAddress);
    }
}

I2C::~I2C()
{
}

bool I2C::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev, m_Device->getDeviceName()))
    {
        // I2C Bus Path
        if (I2CBusTP.isNameMatch(name))
        {
            I2CBusTP.update(texts, names, n);
            I2CBusTP.setState(IPS_OK);
            I2CBusTP.apply();
            return true;
        }
    }

    return false;
}

bool I2C::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp(dev, m_Device->getDeviceName()))
    {
        // I2C Address
        if (I2CAddressNP.isNameMatch(name))
        {
            I2CAddressNP.update(values, names, n);
            I2CAddressNP.setState(IPS_OK);
            I2CAddressNP.apply();
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
    uint8_t addr = static_cast<uint8_t>(I2CAddressNP[0].getValue());
    if (Connect(I2CBusTP[0].getText(), addr) && processHandshake())
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
        if (m_Permission != IP_RO && std::string(I2CBusTP[0].getText()) != m_ConfigBusPath)
            m_Device->saveConfig(true, "I2C_BUS");

        if (m_Permission != IP_RO && static_cast<int>(I2CAddressNP[0].getValue()) != m_ConfigAddress)
            m_Device->saveConfig(true, "I2C_ADDRESS");
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
    m_Device->defineProperty(I2CBusTP);
    m_Device->defineProperty(I2CAddressNP);
}

void I2C::Deactivated()
{
    m_Device->deleteProperty(I2CBusTP.getName());
    m_Device->deleteProperty(I2CAddressNP.getName());
}

bool I2C::saveConfigItems(FILE *fp)
{
    if (m_Permission != IP_RO)
    {
        I2CBusTP.save(fp);
        I2CAddressNP.save(fp);
    }

    return true;
}

void I2C::setDefaultBusPath(const char *path)
{
    // Only set default bus path if configuration path was not loaded already.
    if (m_ConfigBusPath.empty())
        I2CBusTP[0].setText(path);
    if (m_Device->isInitializationComplete())
        m_Device->defineProperty(I2CBusTP);
}

void I2C::setDefaultAddress(uint8_t addr)
{
    // Only set default address if configuration address was not loaded already.
    if (m_ConfigAddress == -1)
    {
        I2CAddressNP[0].setValue(addr);
    }

    if (m_Device->isInitializationComplete())
        m_Device->defineProperty(I2CAddressNP);
}

uint8_t I2C::address()
{
    return static_cast<uint8_t>(I2CAddressNP[0].getValue());
}

}
