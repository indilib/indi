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
#include "indicom.h"
#include <cmath>

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
    MountOffsetNP[AXIS1_OFFSET].fill("AXIS1_OFFSET", "Axis 1 Offset (deg)", "%.2f", 0, 360, 0, 0);
    MountOffsetNP[AXIS2_OFFSET].fill("AXIS2_OFFSET", "Axis 2 Offset (deg)", "%.2f", 0, 360, 0, 0);
    MountOffsetNP[ROTATION_OFFSET].fill("ROTATION_OFFSET", "Rotation Offset (deg)", "%.2f", 0, 360, 0, 0);
    MountOffsetNP.fill(getDeviceName(), "MOUNT_OFFSET", "Mount Offset", COORDINATES_TAB.c_str(), IP_RW, 0, IPS_IDLE);

    AstroCoordinatesNP[AXIS1].fill("AXIS1", "Axis 1 (deg)", "%.2f", 0, 360, 0, 0);
    AstroCoordinatesNP[AXIS2].fill("AXIS2", "Axis 2 (deg)", "%.2f", 0, 360, 0, 0);
    AstroCoordinatesNP.fill(getDeviceName(), "COORDINATES", "Coordinates", COORDINATES_TAB.c_str(), IP_RO,
                            0, IPS_IDLE);

    AstroCoordsTypeSP[COORD_EQUATORIAL].fill("EQUATORIAL", "Equatorial (HA/DEC)", ISS_ON);
    AstroCoordsTypeSP[COORD_ALTAZ].fill("ALTAZ", "Alt-Az (AZ/ALT)", ISS_OFF);
    AstroCoordsTypeSP.fill(getDeviceName(), "COORDS_TYPE", "Coordinate Type", COORDINATES_TAB.c_str(), IP_RW,
                           ISR_1OFMANY, 0, IPS_IDLE);

    GeographicCoordNP[LOCATION_LATITUDE].fill("LAT", "Latitude", "+%08.4f", -90, 90, 0, 0);
    GeographicCoordNP[LOCATION_LONGITUDE].fill("LONG", "Longitude", "+%08.4f", 0, 360, 0, 0);
    GeographicCoordNP[LOCATION_ELEVATION].fill("ELEV", "Elevation", "%.f", 0, 10000, 0, 0);
    GeographicCoordNP.fill(getDeviceName(), "GEOGRAPHIC_COORD", "Location", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    MagneticDeclinationNP[0].fill("MAGNETIC_DECLINATION", "Magnetic Declination", "%.4f", -180, 180, 0, 0);
    MagneticDeclinationNP.fill(getDeviceName(), "MAGNETIC_DECLINATION", "Magnetic Declination", MAIN_CONTROL_TAB, IP_RW, 0,
                               IPS_IDLE);

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
        defineProperty(MountOffsetNP);
        defineProperty(AstroCoordinatesNP);
        defineProperty(AstroCoordsTypeSP);
        defineProperty(GeographicCoordNP);
        defineProperty(MagneticDeclinationNP);
    }
    else
    {
        // Delete driver-specific properties when disconnected
        deleteProperty(MountOffsetNP);
        deleteProperty(AstroCoordinatesNP);
        deleteProperty(AstroCoordsTypeSP);
        deleteProperty(GeographicCoordNP);
        deleteProperty(MagneticDeclinationNP);
    }
    return true;
}

bool IMU::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (IMUInterface::processNumber(dev, name, values, names, n))
        return true;

    if (MountOffsetNP.isNameMatch(name))
    {
        updateProperty(MountOffsetNP, values, names, n, [this]()
        {
            RecalculateAstroCoordinates();
            return true;
        }, true);
        return true;
    }

    if (GeographicCoordNP.isNameMatch(name))
    {
        updateProperty(GeographicCoordNP, values, names, n, []()
        {
            return true;
        }, true);
        return true;
    }

    if (MagneticDeclinationNP.isNameMatch(name))
    {
        updateProperty(MagneticDeclinationNP, values, names, n, [&]()
        {
            RecalculateAstroCoordinates();
            return true;
        }, true);
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
        updateProperty(AstroCoordsTypeSP, states, names, n, []()
        {
            return true;
        }, true);
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
    MountOffsetNP.save(fp);
    AstroCoordsTypeSP.save(fp);
    GeographicCoordNP.save(fp);
    MagneticDeclinationNP.save(fp);

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

// Helper function to convert quaternion to Euler angles (roll, pitch, yaw)
// Angles are in radians
void IMU::QuaternionToEuler(double i, double j, double k, double w, double &roll, double &pitch, double &yaw)
{
    // Roll (x-axis rotation)
    double sinr_cosp = 2 * (w * i + j * k);
    double cosr_cosp = 1 - 2 * (i * i + j * j);
    roll             = std::atan2(sinr_cosp, cosr_cosp);

    // Pitch (y-axis rotation)
    double sinp = 2 * (w * j - k * i);
    if (std::abs(sinp) >= 1)
        pitch = std::copysign(M_PI / 2, sinp); // Use 90 degrees if out of range
    else
        pitch = std::asin(sinp);

    // Yaw (z-axis rotation)
    double siny_cosp = 2 * (w * k + i * j);
    double cosy_cosp = 1 - 2 * (j * j + k * k);
    yaw              = std::atan2(siny_cosp, cosy_cosp);
}

// Helper function to convert Euler angles (roll, pitch, yaw) in radians to a quaternion
void IMU::EulerToQuaternion(double roll, double pitch, double yaw, double &i, double &j, double &k, double &w)
{
    double cy = cos(yaw * 0.5);
    double sy = sin(yaw * 0.5);
    double cp = cos(pitch * 0.5);
    double sp = sin(pitch * 0.5);
    double cr = cos(roll * 0.5);
    double sr = sin(roll * 0.5);

    w = cr * cp * cy + sr * sp * sy;
    i = sr * cp * cy - cr * sp * sy;
    j = cr * sp * cy + sr * cp * sy;
    k = cr * cp * sy - sr * sp * cy;
}

void IMU::RecalculateAstroCoordinates()
{
    // Log raw quaternion values
    DEBUGF(Logger::DBG_DEBUG, "IMU: Recalculating Astro Coordinates from stored Quaternion: i=%.4f, j=%.4f, k=%.4f, w=%.4f",
           last_q_i, last_q_j, last_q_k, last_q_w);

    double rollRad, pitchRad, yawRad;
    QuaternionToEuler(last_q_i, last_q_j, last_q_k, last_q_w, rollRad, pitchRad, yawRad);

    // Convert radians to degrees for INDI properties
    double rollDeg  = rollRad * 180.0 / M_PI;
    double pitchDeg = pitchRad * 180.0 / M_PI;
    double yawDeg   = yawRad * 180.0 / M_PI;

    // Log Euler angles
    DEBUGF(Logger::DBG_DEBUG, "IMU: Euler Angles (deg): Roll=%.2f, Pitch=%.2f, Yaw=%.2f", rollDeg, pitchDeg, yawDeg);

    // For now, we only support Alt-Az coordinates
    if (AstroCoordsTypeSP[COORD_ALTAZ].s != ISS_ON)
    {
        DEBUG(Logger::DBG_DEBUG, "IMU: AstroCoordsType is not Alt-Az, skipping coordinate recalculation.");
        return;
    }

    // Apply mount alignment offsets
    double axis1Offset    = MountOffsetNP[AXIS1_OFFSET].getValue(); // Corresponds to Azimuth offset
    double axis2Offset    = MountOffsetNP[AXIS2_OFFSET].getValue(); // Corresponds to Altitude offset
    double rotationOffset =
        MountOffsetNP[ROTATION_OFFSET].getValue();
    double magneticDeclination = MagneticDeclinationNP[0].getValue();

    // Log offsets
    DEBUGF(Logger::DBG_DEBUG, "IMU: Magnetic Declination=%.4f, Axis1Offset=%.2f, Axis2Offset=%.2f, RotationOffset=%.2f",
           magneticDeclination, axis1Offset, axis2Offset, rotationOffset);

    // Convert current IMU orientation to a quaternion
    double imu_q_i = last_q_i;
    double imu_q_j = last_q_j;
    double imu_q_k = last_q_k;
    double imu_q_w = last_q_w;

    // Convert offsets (including magnetic declination) to radians
    double offsetRollRad  = rotationOffset * M_PI / 180.0;
    double offsetPitchRad = axis2Offset * M_PI / 180.0;
    double offsetYawRad   = (axis1Offset + magneticDeclination) * M_PI / 180.0;

    // Log offsets in radians
    DEBUGF(Logger::DBG_DEBUG, "IMU: Offsets (rad): Roll=%.4f, Pitch=%.4f, Yaw=%.4f", offsetRollRad, offsetPitchRad,
           offsetYawRad);

    // Convert offset Euler angles to a quaternion
    double offset_q_i, offset_q_j, offset_q_k, offset_q_w;
    EulerToQuaternion(offsetRollRad, offsetPitchRad, offsetYawRad, offset_q_i, offset_q_j, offset_q_k, offset_q_w);

    // Perform quaternion multiplication: result_q = imu_q * offset_q
    double result_q_w = imu_q_w * offset_q_w - imu_q_i * offset_q_i - imu_q_j * offset_q_j - imu_q_k * offset_q_k;
    double result_q_i = imu_q_w * offset_q_i + imu_q_i * offset_q_w + imu_q_j * offset_q_k - imu_q_k * offset_q_j;
    double result_q_j = imu_q_w * offset_q_j - imu_q_i * offset_q_k + imu_q_j * offset_q_w + imu_q_k * offset_q_i;
    double result_q_k = imu_q_w * offset_q_k + imu_q_i * offset_q_j - imu_q_j * offset_q_i + imu_q_k * offset_q_w;

    // Convert the resulting quaternion back to Euler angles
    double finalRollRad, finalPitchRad, finalYawRad;
    QuaternionToEuler(result_q_i, result_q_j, result_q_k, result_q_w, finalRollRad, finalPitchRad, finalYawRad);

    // Convert final Euler angles to degrees
    double finalRollDeg  = finalRollRad * 180.0 / M_PI;
    double finalPitchDeg = finalPitchRad * 180.0 / M_PI;
    double finalYawDeg   = finalYawRad * 180.0 / M_PI;

    // Log final Euler angles after quaternion rotation
    DEBUGF(Logger::DBG_DEBUG, "IMU: Final Euler Angles (deg) after quaternion rotation: Roll=%.2f, Pitch=%.2f, Yaw=%.2f",
           finalRollDeg, finalPitchDeg, finalYawDeg);

    // Adjust Yaw to be 0-360 degrees and align with Azimuth (North=0, East=90)
    double azimuth = range360(finalYawDeg);

    // Adjust Pitch to be Altitude (-90 to +90, or 0-90 for visible sky)
    double altitude = range180(finalPitchDeg);

    // Update AstroCoordinatesNP (Azimuth, Altitude)
    AstroCoordinatesNP[AXIS1].setValue(azimuth);
    AstroCoordinatesNP[AXIS2].setValue(altitude);
    AstroCoordinatesNP.setState(IPS_OK);
    AstroCoordinatesNP.apply();

    // Log final Alt/Az values
    DEBUGF(Logger::DBG_DEBUG, "IMU: Final Alt/Az (deg): Azimuth=%.2f, Altitude=%.2f", azimuth, altitude);
}

// Implement virtual functions from IMUInterface
bool IMU::SetOrientationData(double i, double j, double k, double w)
{
    // Log raw quaternion values
    DEBUGF(Logger::DBG_DEBUG, "IMU: Raw Quaternion: i=%.4f, j=%.4f, k=%.4f, w=%.4f", i, j, k, w);

    double rollRad, pitchRad, yawRad;
    QuaternionToEuler(i, j, k, w, rollRad, pitchRad, yawRad);

    // Convert radians to degrees for INDI properties
    double rollDeg  = rollRad * 180.0 / M_PI;
    double pitchDeg = pitchRad * 180.0 / M_PI;
    double yawDeg   = yawRad * 180.0 / M_PI;

    // Log Euler angles
    DEBUGF(Logger::DBG_DEBUG, "IMU: Euler Angles (deg): Roll=%.2f, Pitch=%.2f, Yaw=%.2f", rollDeg, pitchDeg, yawDeg);

    // Update INDI Orientation properties (Roll, Pitch, Yaw in degrees)
    OrientationNP[ORIENTATION_ROLL].setValue(rollDeg);
    OrientationNP[ORIENTATION_PITCH].setValue(pitchDeg);
    OrientationNP[ORIENTATION_YAW].setValue(yawDeg);
    OrientationNP[ORIENTATION_QUATERNION_W].setValue(w); // Keep quaternion 'w' for completeness if needed elsewhere
    OrientationNP.setState(IPS_OK);
    OrientationNP.apply();

    // Store raw quaternion values
    last_q_i = i;
    last_q_j = j;
    last_q_k = k;
    last_q_w = w;

    // Recalculate astronomical coordinates
    RecalculateAstroCoordinates();

    return true;
}

bool IMU::SetAccelerationData(double x, double y, double z)
{
    AccelerationNP[ACCELERATION_X].setValue(x);
    AccelerationNP[ACCELERATION_Y].setValue(y);
    AccelerationNP[ACCELERATION_Z].setValue(z);
    AccelerationNP.setState(IPS_OK);
    AccelerationNP.apply();
    return true;
}

bool IMU::SetGyroscopeData(double x, double y, double z)
{
    GyroscopeNP[GYROSCOPE_X].setValue(x);
    GyroscopeNP[GYROSCOPE_Y].setValue(y);
    GyroscopeNP[GYROSCOPE_Z].setValue(z);
    GyroscopeNP.setState(IPS_OK);
    GyroscopeNP.apply();
    return true;
}

bool IMU::SetMagnetometerData(double x, double y, double z)
{
    MagnetometerNP[MAGNETOMETER_X].setValue(x);
    MagnetometerNP[MAGNETOMETER_Y].setValue(y);
    MagnetometerNP[MAGNETOMETER_Z].setValue(z);
    MagnetometerNP.setState(IPS_OK);
    MagnetometerNP.apply();
    return true;
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
