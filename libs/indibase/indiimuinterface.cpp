/*
    IMU Interface
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

#include "indiimuinterface.h"
#include "defaultdevice.h"
#include "indilogger.h"
#include "indipropertylight.h"

namespace INDI
{

IMUInterface::IMUInterface(DefaultDevice *defaultDevice) : m_defaultDevice(defaultDevice)
{
}

void IMUInterface::initProperties(const std::string &groupName)
{
    // Core Sensor Data Properties
    if (HasOrientation())
    {
        OrientationNP[ORIENTATION_ROLL].fill("ROLL", "Roll (deg)", "%.4f", -180, 180, 0, 0);
        OrientationNP[ORIENTATION_PITCH].fill("PITCH", "Pitch (deg)", "%.4f", -90, 90, 0, 0);
        OrientationNP[ORIENTATION_YAW].fill("YAW", "Yaw (deg)", "%.4f", 0, 360, 0, 0);
        OrientationNP[ORIENTATION_QUATERNION_W].fill("QUATERNION_W", "Quaternion W", "%.4f", -1, 1, 0, 0);
        OrientationNP.fill(m_defaultDevice->getDeviceName(), "ORIENTATION", "Orientation", groupName.c_str(), IP_RO, 0, IPS_IDLE);
    }

    if (HasAcceleration())
    {
        AccelerationNP[ACCELERATION_X].fill("ACCEL_X", "X Acceleration (m/s²)", "%.4f", -100, 100, 0, 0);
        AccelerationNP[ACCELERATION_Y].fill("ACCEL_Y", "Y Acceleration (m/s²)", "%.4f", -100, 100, 0, 0);
        AccelerationNP[ACCELERATION_Z].fill("ACCEL_Z", "Z Acceleration (m/s²)", "%.4f", -100, 100, 0, 0);
        AccelerationNP.fill(m_defaultDevice->getDeviceName(), "ACCELERATION", "Acceleration", groupName.c_str(), IP_RO, 0,
                            IPS_IDLE);
    }

    if (HasGyroscope())
    {
        GyroscopeNP[GYROSCOPE_X].fill("GYRO_X", "X Angular Velocity (rad/s)", "%.4f", -10, 10, 0, 0);
        GyroscopeNP[GYROSCOPE_Y].fill("GYRO_Y", "Y Angular Velocity (rad/s)", "%.4f", -10, 10, 0, 0);
        GyroscopeNP[GYROSCOPE_Z].fill("GYRO_Z", "Z Angular Velocity (rad/s)", "%.4f", -10, 10, 0, 0);
        GyroscopeNP.fill(m_defaultDevice->getDeviceName(), "GYROSCOPE", "Gyroscope", groupName.c_str(), IP_RO, 0, IPS_IDLE);
    }

    if (HasMagnetometer())
    {
        MagnetometerNP[MAGNETOMETER_X].fill("MAG_X", "X Magnetic Field (µT)", "%.4f", -1000, 1000, 0, 0);
        MagnetometerNP[MAGNETOMETER_Y].fill("MAG_Y", "Y Magnetic Field (µT)", "%.4f", -1000, 1000, 0, 0);
        MagnetometerNP[MAGNETOMETER_Z].fill("MAG_Z", "Z Magnetic Field (µT)", "%.4f", -1000, 1000, 0, 0);
        MagnetometerNP.fill(m_defaultDevice->getDeviceName(), "MAGNETOMETER", "Magnetometer", groupName.c_str(), IP_RO, 0,
                            IPS_IDLE);
    }

    // Calibration Properties
    if (HasCalibration())
    {
        CalibrationStatusLP[CALIBRATION_STATUS_SYS].fill("CAL_SYS", "System", IPS_IDLE);
        CalibrationStatusLP[CALIBRATION_STATUS_GYRO].fill("CAL_GYRO", "Gyroscope", IPS_IDLE);
        CalibrationStatusLP[CALIBRATION_STATUS_ACCEL].fill("CAL_ACCEL", "Accelerometer", IPS_IDLE);
        CalibrationStatusLP[CALIBRATION_STATUS_MAG].fill("CAL_MAG", "Magnetometer", IPS_IDLE);
        CalibrationStatusLP.fill(m_defaultDevice->getDeviceName(), "CALIBRATION_STATUS", "Calibration Status",
                                 CALIBRATION_TAB.c_str(), IPS_IDLE);

        CalibrationControlSP[CALIBRATION_CONTROL_START].fill("CAL_START", "Start Calibration", ISS_OFF);
        CalibrationControlSP[CALIBRATION_CONTROL_SAVE].fill("CAL_SAVE", "Save Calibration", ISS_OFF);
        CalibrationControlSP[CALIBRATION_CONTROL_LOAD].fill("CAL_LOAD", "Load Calibration", ISS_OFF);
        CalibrationControlSP[CALIBRATION_CONTROL_RESET].fill("CAL_RESET", "Reset Calibration", ISS_OFF);
        CalibrationControlSP.fill(m_defaultDevice->getDeviceName(), "CALIBRATION_CONTROL", "Calibration Control",
                                  CALIBRATION_TAB.c_str(), IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
    }

    // Configuration Properties
    PowerModeSP[POWER_MODE_NORMAL].fill("NORMAL", "Normal", ISS_ON);
    PowerModeSP[POWER_MODE_LOW_POWER].fill("LOW_POWER", "Low Power", ISS_OFF);
    PowerModeSP[POWER_MODE_SUSPEND].fill("SUSPEND", "Suspend", ISS_OFF);
    PowerModeSP.fill(m_defaultDevice->getDeviceName(), "POWER_MODE", "Power Mode", CONFIG_TAB.c_str(), IP_RW, ISR_1OFMANY, 0,
                     IPS_IDLE);

    OperationModeSP[OPERATION_MODE_IMU].fill("IMU", "IMU", ISS_ON);
    OperationModeSP[OPERATION_MODE_COMPASS].fill("COMPASS", "Compass", ISS_OFF);
    OperationModeSP[OPERATION_MODE_M4G].fill("M4G", "M4G", ISS_OFF);
    OperationModeSP[OPERATION_MODE_NDOF].fill("NDOF", "NDOF", ISS_OFF);
    OperationModeSP.fill(m_defaultDevice->getDeviceName(), "OPERATION_MODE", "Operation Mode", CONFIG_TAB.c_str(), IP_RW,
                         ISR_1OFMANY, 0, IPS_IDLE);

    UnitsSP[UNITS_METRIC].fill("METRIC", "Metric", ISS_ON);
    UnitsSP[UNITS_IMPERIAL].fill("IMPERIAL", "Imperial", ISS_OFF);
    UnitsSP[UNITS_DEGREES].fill("DEGREES", "Degrees", ISS_ON);
    UnitsSP[UNITS_RADIANS].fill("RADIANS", "Radians", ISS_OFF);
    UnitsSP.fill(m_defaultDevice->getDeviceName(), "UNITS", "Units", CONFIG_TAB.c_str(), IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    UpdateRateNP[UPDATE_RATE_RATE].fill("RATE", "Update Rate (Hz)", "%.2f", 1, 100, 1, 10);
    UpdateRateNP.fill(m_defaultDevice->getDeviceName(), "UPDATE_RATE", "Update Rate", CONFIG_TAB.c_str(), IP_RW, 0, IPS_IDLE);

    OffsetsNP[OFFSETS_X].fill("OFFSET_X", "X Offset", "%.4f", -180, 180, 0, 0);
    OffsetsNP[OFFSETS_Y].fill("OFFSET_Y", "Y Offset", "%.4f", -180, 180, 0, 0);
    OffsetsNP[OFFSETS_Z].fill("OFFSET_Z", "Z Offset", "%.4f", -180, 180, 0, 0);
    OffsetsNP.fill(m_defaultDevice->getDeviceName(), "OFFSETS", "Manual Offsets", CONFIG_TAB.c_str(), IP_RW, 0, IPS_IDLE);

    // Status and Info Properties
    DeviceInfoTP[DEVICE_INFO_CHIP_ID].fill("CHIP_ID", "Chip ID", "");
    DeviceInfoTP[DEVICE_INFO_FIRMWARE_VERSION].fill("FIRMWARE_VERSION", "Firmware Version", "");
    DeviceInfoTP[DEVICE_INFO_SENSOR_STATUS].fill("SENSOR_STATUS", "Sensor Status", "");
    DeviceInfoTP.fill(m_defaultDevice->getDeviceName(), "DEVICE_INFO", "Device Info", STATUS_TAB.c_str(), IP_RO, 0, IPS_IDLE);

    if (HasTemperature())
    {
        TemperatureNP[TEMPERATURE_VALUE].fill("TEMPERATURE", "Temperature (°C)", "%.2f", -40, 85, 0, 0);
        TemperatureNP.fill(m_defaultDevice->getDeviceName(), "TEMPERATURE", "Temperature", STATUS_TAB.c_str(), IP_RO, 0, IPS_IDLE);
    }

    // Astronomical-Specific Properties
    if (HasStabilityMonitoring())
    {
        StabilityMonitoringNP[STABILITY_MONITORING_VIBRATION_LEVEL].fill("VIBRATION_LEVEL", "Vibration Level (RMS)", "%.4f", 0, 100,
                0, 0);
        StabilityMonitoringNP[STABILITY_MONITORING_STABILITY_THRESHOLD].fill("STABILITY_MONITORING_STABILITY_THRESHOLD",
                "Stability Threshold (RMS)", "%.4f", 0, 100, 0, 5);
        StabilityMonitoringNP.fill(m_defaultDevice->getDeviceName(), "STABILITY_MONITORING", "Stability Monitoring",
                                   ASTRONOMICAL_TAB.c_str(), IP_RW, 0, IPS_IDLE);
    }
}

bool IMUInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
        // Define properties when connected
        if (HasOrientation())
            m_defaultDevice->defineProperty(OrientationNP);
        if (HasAcceleration())
            m_defaultDevice->defineProperty(AccelerationNP);
        if (HasGyroscope())
            m_defaultDevice->defineProperty(GyroscopeNP);
        if (HasMagnetometer())
            m_defaultDevice->defineProperty(MagnetometerNP);
        if (HasCalibration())
        {
            m_defaultDevice->defineProperty(CalibrationStatusLP);
            m_defaultDevice->defineProperty(CalibrationControlSP);
        }

        m_defaultDevice->defineProperty(PowerModeSP);
        m_defaultDevice->defineProperty(OperationModeSP);
        m_defaultDevice->defineProperty(UnitsSP);
        m_defaultDevice->defineProperty(UpdateRateNP);
        m_defaultDevice->defineProperty(OffsetsNP);
        m_defaultDevice->defineProperty(DeviceInfoTP);

        if (HasTemperature())
            m_defaultDevice->defineProperty(TemperatureNP);
        if (HasStabilityMonitoring())
            m_defaultDevice->defineProperty(StabilityMonitoringNP);
    }
    else
    {
        // Delete properties when disconnected
        if (HasOrientation())
            m_defaultDevice->deleteProperty(OrientationNP);
        if (HasAcceleration())
            m_defaultDevice->deleteProperty(AccelerationNP);
        if (HasGyroscope())
            m_defaultDevice->deleteProperty(GyroscopeNP);
        if (HasMagnetometer())
            m_defaultDevice->deleteProperty(MagnetometerNP);
        if (HasCalibration())
        {
            m_defaultDevice->deleteProperty(CalibrationStatusLP);
            m_defaultDevice->deleteProperty(CalibrationControlSP);
        }

        m_defaultDevice->deleteProperty(PowerModeSP);
        m_defaultDevice->deleteProperty(OperationModeSP);
        m_defaultDevice->deleteProperty(UnitsSP);
        m_defaultDevice->deleteProperty(UpdateRateNP);
        m_defaultDevice->deleteProperty(OffsetsNP);
        m_defaultDevice->deleteProperty(DeviceInfoTP);

        if (HasTemperature())
            m_defaultDevice->deleteProperty(TemperatureNP);
        if (HasStabilityMonitoring())
            m_defaultDevice->deleteProperty(StabilityMonitoringNP);
    }

    return true;
}

bool IMUInterface::processNumber(const std::string &dev, const std::string &name, double values[], char *names[], int n)
{
    if (dev != m_defaultDevice->getDeviceName())
        return false;

    if (name == UpdateRateNP.getName())
    {
        UpdateRateNP.update(values, names, n);
        UpdateRateNP.setState(IPS_OK);
        UpdateRateNP.apply();
        SetUpdateRate(UpdateRateNP[UPDATE_RATE_RATE].getValue());
        return true;
    }

    if (name == OffsetsNP.getName())
    {
        OffsetsNP.update(values, names, n);
        OffsetsNP.setState(IPS_OK);
        OffsetsNP.apply();
        SetOffsets(OffsetsNP[OFFSETS_X].getValue(), OffsetsNP[OFFSETS_Y].getValue(), OffsetsNP[OFFSETS_Z].getValue());
        return true;
    }

    if (HasStabilityMonitoring() && name == StabilityMonitoringNP.getName())
    {
        StabilityMonitoringNP.update(values, names, n);
        StabilityMonitoringNP.setState(IPS_OK);
        StabilityMonitoringNP.apply();
        SetStabilityMonitoring(StabilityMonitoringNP[STABILITY_MONITORING_VIBRATION_LEVEL].getValue(),
                               StabilityMonitoringNP[STABILITY_MONITORING_STABILITY_THRESHOLD].getValue());
        return true;
    }

    return false;
}

bool IMUInterface::processSwitch(const std::string &dev, const std::string &name, ISState *states, char *names[], int n)
{
    if (dev != m_defaultDevice->getDeviceName())
        return false;

    if (name == PowerModeSP.getName())
    {
        PowerModeSP.update(states, names, n);
        PowerModeSP.setState(IPS_OK);
        PowerModeSP.apply();

        int index = PowerModeSP.findOnSwitchIndex();
        if (index >= 0)
            SetPowerMode(PowerModeSP[index].getName());
        return true;
    }

    if (name == OperationModeSP.getName())
    {
        OperationModeSP.update(states, names, n);
        OperationModeSP.setState(IPS_OK);
        OperationModeSP.apply();

        int index = OperationModeSP.findOnSwitchIndex();
        if (index >= 0)
            SetOperationMode(OperationModeSP[index].getName());
        return true;
    }

    if (name == UnitsSP.getName())
    {
        UnitsSP.update(states, names, n);
        UnitsSP.setState(IPS_OK);
        UnitsSP.apply();

        bool metric = UnitsSP[UNITS_METRIC].getState() == ISS_ON;
        bool degrees = UnitsSP[UNITS_DEGREES].getState() == ISS_ON;
        SetUnits(metric, degrees);
        return true;
    }

    if (HasCalibration() && name == CalibrationControlSP.getName())
    {
        CalibrationControlSP.update(states, names, n);
        CalibrationControlSP.setState(IPS_BUSY);
        CalibrationControlSP.apply();

        for (int i = 0; i < n; i++)
        {
            if (states[i] == ISS_ON)
            {
                std::string switchName = names[i];
                bool result = false;

                if (switchName == CalibrationControlSP[CALIBRATION_CONTROL_START].getName())
                    result = StartCalibration();
                else if (switchName == CalibrationControlSP[CALIBRATION_CONTROL_SAVE].getName())
                    result = SaveCalibrationData();
                else if (switchName == CalibrationControlSP[CALIBRATION_CONTROL_LOAD].getName())
                    result = LoadCalibrationData();
                else if (switchName == CalibrationControlSP[CALIBRATION_CONTROL_RESET].getName())
                    result = ResetCalibration();

                CalibrationControlSP.setState(result ? IPS_OK : IPS_ALERT);
                CalibrationControlSP.reset();
                CalibrationControlSP.apply();
                break;
            }
        }
        return true;
    }

    return false;
}

bool IMUInterface::processText(const std::string &dev, const std::string &name, char *texts[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(texts);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
    return false;
}

bool IMUInterface::SetOrientationData(double roll, double pitch, double yaw, double w)
{
    if (!HasOrientation())
        return false;

    OrientationNP[ORIENTATION_ROLL].setValue(roll);
    OrientationNP[ORIENTATION_PITCH].setValue(pitch);
    OrientationNP[ORIENTATION_YAW].setValue(yaw);
    OrientationNP[ORIENTATION_QUATERNION_W].setValue(w);
    OrientationNP.setState(IPS_OK);
    OrientationNP.apply();
    return true;
}

bool IMUInterface::SetAccelerationData(double x, double y, double z)
{
    if (!HasAcceleration())
        return false;

    AccelerationNP[ACCELERATION_X].setValue(x);
    AccelerationNP[ACCELERATION_Y].setValue(y);
    AccelerationNP[ACCELERATION_Z].setValue(z);
    AccelerationNP.setState(IPS_OK);
    AccelerationNP.apply();
    return true;
}

bool IMUInterface::SetGyroscopeData(double x, double y, double z)
{
    if (!HasGyroscope())
        return false;

    GyroscopeNP[GYROSCOPE_X].setValue(x);
    GyroscopeNP[GYROSCOPE_Y].setValue(y);
    GyroscopeNP[GYROSCOPE_Z].setValue(z);
    GyroscopeNP.setState(IPS_OK);
    GyroscopeNP.apply();
    return true;
}

bool IMUInterface::SetMagnetometerData(double x, double y, double z)
{
    if (!HasMagnetometer())
        return false;

    MagnetometerNP[MAGNETOMETER_X].setValue(x);
    MagnetometerNP[MAGNETOMETER_Y].setValue(y);
    MagnetometerNP[MAGNETOMETER_Z].setValue(z);
    MagnetometerNP.setState(IPS_OK);
    MagnetometerNP.apply();
    return true;
}

bool IMUInterface::SetCalibrationStatus(int sys, int gyro, int accel, int mag)
{
    if (!HasCalibration())
        return false;

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

    CalibrationStatusLP[CALIBRATION_STATUS_SYS].setState(getCalibrationState(sys));
    CalibrationStatusLP[CALIBRATION_STATUS_GYRO].setState(getCalibrationState(gyro));
    CalibrationStatusLP[CALIBRATION_STATUS_ACCEL].setState(getCalibrationState(accel));
    CalibrationStatusLP[CALIBRATION_STATUS_MAG].setState(getCalibrationState(mag));
    CalibrationStatusLP.apply();
    return true;
}

bool IMUInterface::StartCalibration()
{
    // Default implementation - should be overridden by concrete drivers
    return false;
}

bool IMUInterface::SaveCalibrationData()
{
    // Default implementation - should be overridden by concrete drivers
    return false;
}

bool IMUInterface::LoadCalibrationData()
{
    // Default implementation - should be overridden by concrete drivers
    return false;
}

bool IMUInterface::ResetCalibration()
{
    // Default implementation - should be overridden by concrete drivers
    return false;
}

bool IMUInterface::SetPowerMode(const std::string &mode)
{
    INDI_UNUSED(mode);
    // Default implementation - should be overridden by concrete drivers
    return false;
}

bool IMUInterface::SetOperationMode(const std::string &mode)
{
    INDI_UNUSED(mode);
    // Default implementation - should be overridden by concrete drivers
    return false;
}

bool IMUInterface::SetUnits(bool metric, bool degrees)
{
    INDI_UNUSED(metric);
    INDI_UNUSED(degrees);
    // Default implementation - should be overridden by concrete drivers
    return false;
}

bool IMUInterface::SetUpdateRate(double rate)
{
    INDI_UNUSED(rate);
    // Default implementation - should be overridden by concrete drivers
    return false;
}

bool IMUInterface::SetOffsets(double x, double y, double z)
{
    INDI_UNUSED(x);
    INDI_UNUSED(y);
    INDI_UNUSED(z);
    // Default implementation - should be overridden by concrete drivers
    return false;
}

bool IMUInterface::SetDeviceInfo(const std::string &chipID, const std::string &firmwareVersion,
                                 const std::string &sensorStatus)
{
    DeviceInfoTP[DEVICE_INFO_CHIP_ID].setText(chipID.c_str());
    DeviceInfoTP[DEVICE_INFO_FIRMWARE_VERSION].setText(firmwareVersion.c_str());
    DeviceInfoTP[DEVICE_INFO_SENSOR_STATUS].setText(sensorStatus.c_str());
    DeviceInfoTP.setState(IPS_OK);
    DeviceInfoTP.apply();
    return true;
}

bool IMUInterface::SetTemperature(double temperature)
{
    if (!HasTemperature())
        return false;

    TemperatureNP[TEMPERATURE_VALUE].setValue(temperature);
    TemperatureNP.setState(IPS_OK);
    TemperatureNP.apply();
    return true;
}

bool IMUInterface::SetStabilityMonitoring(double vibrationLevel, double stabilityThreshold)
{
    if (!HasStabilityMonitoring())
        return false;

    StabilityMonitoringNP[STABILITY_MONITORING_VIBRATION_LEVEL].setValue(vibrationLevel);
    StabilityMonitoringNP[STABILITY_MONITORING_STABILITY_THRESHOLD].setValue(stabilityThreshold);
    StabilityMonitoringNP.setState(IPS_OK);
    StabilityMonitoringNP.apply();
    return true;
}

bool IMUInterface::saveConfigItems(FILE *fp)
{
    PowerModeSP.save(fp);
    OperationModeSP.save(fp);
    UnitsSP.save(fp);
    UpdateRateNP.save(fp);
    OffsetsNP.save(fp);

    if (HasStabilityMonitoring())
        StabilityMonitoringNP.save(fp);

    return true;
}

}
