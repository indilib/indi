/*
    IMU Driver Base Class
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

#include "indiimu.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectioni2c.h"

namespace INDI
{

IMU::IMU() : IMUInterface(this)
{
    // Constructor only initializes the interface base class.
    // Properties will be initialized in initProperties().
    serialConnection = nullptr;
    i2cConnection    = nullptr;
}

IMU::~IMU()
{
    delete serialConnection;
    delete i2cConnection;
}

void IMU::setSupportedConnections(const uint8_t &value)
{
    uint8_t mask = CONNECTION_SERIAL | CONNECTION_I2C | CONNECTION_NONE;

    if (value == 0 || (mask & value) == 0)
    {
        DEBUGF(Logger::DBG_ERROR, "Invalid connection mode %d", value);
        return;
    }

    imuConnection = value;
}

bool IMU::initProperties()
{
    DefaultDevice::initProperties();
    IMUInterface::initProperties(IMU_TAB);

    // Initialize driver-specific properties
    MountAlignmentNP[AXIS1_OFFSET].fill("AXIS1_OFFSET", "Axis 1 Offset (deg)", "%.2f", 0, 360, 0, 0);
    MountAlignmentNP[AXIS2_OFFSET].fill("AXIS2_OFFSET", "Axis 2 Offset (deg)", "%.2f", 0, 360, 0, 0);
    MountAlignmentNP[ROTATION_OFFSET].fill("ROTATION_OFFSET", "Rotation Offset (deg)", "%.2f", 0, 360, 0, 0);
    MountAlignmentNP.fill(getDeviceName(), "MOUNT_ALIGNMENT", "Mount Alignment", COORDINATES_TAB.c_str(), IP_RW, 0, IPS_IDLE);

    AstroCoordinatesNP[AXIS1].fill("AXIS1", "Axis 1 (deg)", "%.2f", 0, 360, 0, 0);
    AstroCoordinatesNP[AXIS2].fill("AXIS2", "Axis 2 (deg)", "%.2f", 0, 360, 0, 0);
    AstroCoordinatesNP.fill(getDeviceName(), "COORDINATES", "Coordinates", COORDINATES_TAB.c_str(), IP_RO,
                            0, IPS_IDLE);

    AstroCoordsTypeSP[COORD_EQUATORIAL].fill("EQUATORIAL", "Equatorial (HA/DEC)", ISS_ON);
    AstroCoordsTypeSP[COORD_ALTAZ].fill("ALTAZ", "Alt-Az (AZ/ALT)", ISS_OFF);
    AstroCoordsTypeSP.fill(getDeviceName(), "COORDS_TYPE", "Coordinate Type", COORDINATES_TAB.c_str(), IP_RW,
                           ISR_1OFMANY, 0, IPS_IDLE);

    if (imuConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(serialConnection);
    }

    if (imuConnection & CONNECTION_I2C)
    {
        i2cConnection = new Connection::I2C(this);
        i2cConnection->setDefaultBusPath("/dev/i2c-1");
        i2cConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(i2cConnection);
    }

    return true;
}

bool IMU::updateProperties()
{
    DefaultDevice::updateProperties();
    IMUInterface::updateProperties();

    if (isConnected())
    {
        // Define driver-specific properties when connected
        defineProperty(MountAlignmentNP);
        defineProperty(AstroCoordinatesNP);
        defineProperty(AstroCoordsTypeSP);
    }
    else
    {
        // Delete driver-specific properties when disconnected
        deleteProperty(MountAlignmentNP);
        deleteProperty(AstroCoordinatesNP);
        deleteProperty(AstroCoordsTypeSP);
    }
    return true;
}

bool IMU::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (IMUInterface::processNumber(dev, name, values, names, n))
        return true;

    if (strcmp(name, MountAlignmentNP.getName()) == 0)
    {
        // Process Mount Alignment properties
        // This will be handled by the concrete driver
        return true;
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool IMU::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (IMUInterface::processSwitch(dev, name, states, names, n))
        return true;

    if (AstroCoordsTypeSP.isNameMatch(name))
    {
        // Process Astro Coordinates Type switch
        // This will be handled by the concrete driver
        return true;
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool IMU::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (IMUInterface::processText(dev, name, texts, names, n))
        return true;

    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool IMU::saveConfigItems(FILE *fp)
{
    IMUInterface::saveConfigItems(fp);
    DefaultDevice::saveConfigItems(fp);

    // Save driver-specific properties
    MountAlignmentNP.save(fp);
    AstroCoordsTypeSP.save(fp);

    return true;
}

bool IMU::Handshake()
{
    // Default implementation does nothing and returns false
    // Concrete drivers should override this method
    return false;
}

bool IMU::callHandshake()
{
    if (imuConnection > 0)
    {
        if (getActiveConnection() == serialConnection)
            PortFD = serialConnection->getPortFD();
        else if (getActiveConnection() == i2cConnection)
            PortFD = i2cConnection->getPortFD();
    }

    return Handshake();
}

// Default implementations for IMUInterface virtual functions
bool IMU::SetOrientationData(double roll, double pitch, double yaw, double w)
{
    INDI_UNUSED(roll);
    INDI_UNUSED(pitch);
    INDI_UNUSED(yaw);
    INDI_UNUSED(w);
    return false;
}
bool IMU::SetAccelerationData(double x, double y, double z)
{
    INDI_UNUSED(x);
    INDI_UNUSED(y);
    INDI_UNUSED(z);
    return false;
}
bool IMU::SetGyroscopeData(double x, double y, double z)
{
    INDI_UNUSED(x);
    INDI_UNUSED(y);
    INDI_UNUSED(z);
    return false;
}
bool IMU::SetMagnetometerData(double x, double y, double z)
{
    INDI_UNUSED(x);
    INDI_UNUSED(y);
    INDI_UNUSED(z);
    return false;
}
bool IMU::SetCalibrationStatus(int sys, int gyro, int accel, int mag)
{
    INDI_UNUSED(sys);
    INDI_UNUSED(gyro);
    INDI_UNUSED(accel);
    INDI_UNUSED(mag);
    return false;
}
bool IMU::StartCalibration()
{
    return false;
}
bool IMU::SaveCalibrationData()
{
    return false;
}
bool IMU::LoadCalibrationData()
{
    return false;
}
bool IMU::ResetCalibration()
{
    return false;
}
bool IMU::SetPowerMode(const std::string &mode)
{
    INDI_UNUSED(mode);
    return false;
}
bool IMU::SetOperationMode(const std::string &mode)
{
    INDI_UNUSED(mode);
    return false;
}
bool IMU::SetDistanceUnits(bool metric)
{
    INDI_UNUSED(metric);
    return false;
}
bool IMU::SetAngularUnits(bool degrees)
{
    INDI_UNUSED(degrees);
    return false;
}
bool IMU::SetUpdateRate(double rate)
{
    INDI_UNUSED(rate);
    return false;
}
bool IMU::SetOffsets(double x, double y, double z)
{
    INDI_UNUSED(x);
    INDI_UNUSED(y);
    INDI_UNUSED(z);
    return false;
}
bool IMU::SetDeviceInfo(const std::string &chipID, const std::string &firmwareVersion, const std::string &sensorStatus)
{
    INDI_UNUSED(chipID);
    INDI_UNUSED(firmwareVersion);
    INDI_UNUSED(sensorStatus);
    return false;
}
bool IMU::SetTemperature(double temperature)
{
    INDI_UNUSED(temperature);
    return false;
}
bool IMU::SetStabilityMonitoring(double vibrationLevel, double stabilityThreshold)
{
    INDI_UNUSED(vibrationLevel);
    INDI_UNUSED(stabilityThreshold);
    return false;
}

}
