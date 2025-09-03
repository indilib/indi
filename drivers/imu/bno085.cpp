/*
    BNO085 IMU Driver
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

#include "bno085.h"
#include "indicom.h"
#include "indilogger.h"

namespace INDI
{

BNO085::BNO085()
{
    SetCapability(IMU_HAS_ORIENTATION | IMU_HAS_ACCELERATION | IMU_HAS_GYROSCOPE | IMU_HAS_MAGNETOMETER |
                  IMU_HAS_CALIBRATION | IMU_HAS_TEMPERATURE | IMU_HAS_STABILITY_MON);

    setSupportedConnections(INDI::IMU::CONNECTION_I2C);

    setDriverInterface(IMU_INTERFACE);
}

bool BNO085::initProperties()
{
    IMU::initProperties();
    addDebugControl();
    addPollPeriodControl();
    return true;
}

bool BNO085::updateProperties()
{
    IMU::updateProperties();
    return true;
}

bool BNO085::Handshake()
{
    if (i2cConnection->Connect())
    {
        // PortFD is now managed by the base IMU class
        // i2cFD = i2cConnection->getI2CFD(); // No longer needed here
        // TODO: Implement BNO085 initialization sequence
        // For example, check chip ID, set operation mode, etc.
        LOG_INFO("BNO085 connected successfully.");
        return true;
    }
    LOG_ERROR("BNO085 connection failed.");
    return false;
}

void BNO085::TimerHit()
{
    if (!isConnected())
        return;

    // TODO: Read sensor data and update INDI properties
    readSensorData();

    SetTimer(getPollingPeriod());
}

bool BNO085::readSensorData()
{
    // Placeholder for reading sensor data from BNO085 via I2C
    // This would involve reading multiple registers and parsing the values
    // For example:
    // uint8_t data[20];
    // if (readBytes(BNO085_ACCEL_DATA_X_LSB, data, 6)) {
    //    double accelX = (int16_t)((data[1] << 8) | data[0]) / 100.0; // Example scaling
    //    SetAccelerationData(accelX, accelY, accelZ);
    // }

    // For now, just simulate some data
    SetOrientationData(10.0, 20.0, 30.0, 1.0);
    SetAccelerationData(0.1, 0.2, 9.8);
    SetGyroscopeData(0.01, 0.02, 0.03);
    SetMagnetometerData(40.0, 50.0, 60.0);
    SetCalibrationStatus(3, 3, 3, 3);
    SetTemperature(25.5);
    SetStabilityMonitoring(0.05, 0.1);

    return true;
}

bool BNO085::writeRegister(uint8_t reg, uint8_t value)
{
    INDI_UNUSED(reg);
    INDI_UNUSED(value);
    // Placeholder for I2C write operation
    // This would use i2cFD to write to the BNO085
    return true;
}

uint8_t BNO085::readRegister(uint8_t reg)
{
    INDI_UNUSED(reg);
    // Placeholder for I2C read operation
    // This would use i2cFD to read from the BNO085
    return 0; // Return dummy value
}

// Implement virtual functions from IMUInterface
bool BNO085::SetOrientationData(double roll, double pitch, double yaw, double w)
{
    // Update INDI property and send to client
    OrientationNP[0].setValue(roll);
    OrientationNP[1].setValue(pitch);
    OrientationNP[2].setValue(yaw);
    OrientationNP[3].setValue(w);
    OrientationNP.setState(IPS_OK);
    OrientationNP.apply();
    return true;
}

bool BNO085::SetAccelerationData(double x, double y, double z)
{
    AccelerationNP[0].setValue(x);
    AccelerationNP[1].setValue(y);
    AccelerationNP[2].setValue(z);
    AccelerationNP.setState(IPS_OK);
    AccelerationNP.apply();
    return true;
}

bool BNO085::SetGyroscopeData(double x, double y, double z)
{
    GyroscopeNP[0].setValue(x);
    GyroscopeNP[1].setValue(y);
    GyroscopeNP[2].setValue(z);
    GyroscopeNP.setState(IPS_OK);
    GyroscopeNP.apply();
    return true;
}

bool BNO085::SetMagnetometerData(double x, double y, double z)
{
    MagnetometerNP[0].setValue(x);
    MagnetometerNP[1].setValue(y);
    MagnetometerNP[2].setValue(z);
    MagnetometerNP.setState(IPS_OK);
    MagnetometerNP.apply();
    return true;
}

bool BNO085::SetCalibrationStatus(int sys, int gyro, int accel, int mag)
{
    // Convert calibration values (0-3) to light states
    auto getCalibrationState = [](int value) -> IPState
    {
        switch (value)
        {
            case 0:
                return IPS_ALERT;   // Not calibrated
            case 1:
                return IPS_BUSY;    // Partially calibrated
            case 2:
                return IPS_BUSY;    // Mostly calibrated
            case 3:
                return IPS_OK;      // Fully calibrated
            default:
                return IPS_IDLE;
        }
    };

    CalibrationStatusLP[0].setState(getCalibrationState(sys));
    CalibrationStatusLP[1].setState(getCalibrationState(gyro));
    CalibrationStatusLP[2].setState(getCalibrationState(accel));
    CalibrationStatusLP[3].setState(getCalibrationState(mag));
    CalibrationStatusLP.apply();
    return true;
}

bool BNO085::StartCalibration()
{
    // TODO: Implement BNO085 calibration start
    LOG_INFO("BNO085: Starting calibration.");
    return true;
}

bool BNO085::SaveCalibrationData()
{
    // TODO: Implement BNO085 calibration save
    LOG_INFO("BNO085: Saving calibration data.");
    return true;
}

bool BNO085::LoadCalibrationData()
{
    // TODO: Implement BNO085 calibration load
    LOG_INFO("BNO085: Loading calibration data.");
    return true;
}

bool BNO085::ResetCalibration()
{
    // TODO: Implement BNO085 calibration reset
    LOG_INFO("BNO085: Resetting calibration data.");
    return true;
}

bool BNO085::SetPowerMode(const std::string &mode)
{
    // TODO: Implement BNO085 power mode setting
    LOGF_INFO("BNO085: Setting power mode to %s.", mode.c_str());
    return true;
}

bool BNO085::SetOperationMode(const std::string &mode)
{
    // TODO: Implement BNO085 operation mode setting
    LOGF_INFO("BNO085: Setting operation mode to %s.", mode.c_str());
    return true;
}

bool BNO085::SetUnits(bool metric, bool degrees)
{
    // TODO: Implement BNO085 unit setting
    LOGF_INFO("BNO085: Setting units (metric: %d, degrees: %d).", metric, degrees);
    return true;
}

bool BNO085::SetUpdateRate(double rate)
{
    // TODO: Implement BNO085 update rate setting
    LOGF_INFO("BNO085: Setting update rate to %f Hz.", rate);
    return true;
}

bool BNO085::SetOffsets(double x, double y, double z)
{
    // TODO: Implement BNO085 offset setting
    LOGF_INFO("BNO085: Setting offsets (x: %f, y: %f, z: %f).", x, y, z);
    return true;
}

bool BNO085::SetDeviceInfo(const std::string &chipID, const std::string &firmwareVersion, const std::string &sensorStatus)
{
    DeviceInfoTP[0].setText(chipID);
    DeviceInfoTP[1].setText(firmwareVersion);
    DeviceInfoTP[2].setText(sensorStatus);
    DeviceInfoTP.setState(IPS_OK);
    DeviceInfoTP.apply();
    return true;
}

bool BNO085::SetTemperature(double temperature)
{
    TemperatureNP[0].setValue(temperature);
    TemperatureNP.setState(IPS_OK);
    TemperatureNP.apply();
    return true;
}

bool BNO085::SetStabilityMonitoring(double vibrationLevel, double stabilityThreshold)
{
    StabilityMonitoringNP[0].setValue(vibrationLevel);
    StabilityMonitoringNP[1].setValue(stabilityThreshold);
    StabilityMonitoringNP.setState(IPS_OK);
    StabilityMonitoringNP.apply();
    return true;
}

}
