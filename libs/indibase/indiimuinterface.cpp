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
        OrientationNP[0].fill("ROLL", "Roll", "deg", -180, 180, 0, 0);
        OrientationNP[1].fill("PITCH", "Pitch", "deg", -90, 90, 0, 0);
        OrientationNP[2].fill("YAW", "Yaw", "deg", 0, 360, 0, 0);
        OrientationNP[3].fill("QUATERNION_W", "Quaternion W", "", -1, 1, 0, 0);
        OrientationNP.fill(m_defaultDevice->getDeviceName(), "ORIENTATION", "Orientation", groupName.c_str(), IP_RO, 0, IPS_IDLE);
    }

    if (HasAcceleration())
    {
        AccelerationNP[0].fill("ACCEL_X", "X Acceleration", "m/s²", -100, 100, 0, 0);
        AccelerationNP[1].fill("ACCEL_Y", "Y Acceleration", "m/s²", -100, 100, 0, 0);
        AccelerationNP[2].fill("ACCEL_Z", "Z Acceleration", "m/s²", -100, 100, 0, 0);
        AccelerationNP.fill(m_defaultDevice->getDeviceName(), "ACCELERATION", "Acceleration", groupName.c_str(), IP_RO, 0, IPS_IDLE);
    }

    if (HasGyroscope())
    {
        GyroscopeNP[0].fill("GYRO_X", "X Angular Velocity", "rad/s", -10, 10, 0, 0);
        GyroscopeNP[1].fill("GYRO_Y", "Y Angular Velocity", "rad/s", -10, 10, 0, 0);
        GyroscopeNP[2].fill("GYRO_Z", "Z Angular Velocity", "rad/s", -10, 10, 0, 0);
        GyroscopeNP.fill(m_defaultDevice->getDeviceName(), "GYROSCOPE", "Gyroscope", groupName.c_str(), IP_RO, 0, IPS_IDLE);
    }

    if (HasMagnetometer())
    {
        MagnetometerNP[0].fill("MAG_X", "X Magnetic Field", "µT", -1000, 1000, 0, 0);
        MagnetometerNP[1].fill("MAG_Y", "Y Magnetic Field", "µT", -1000, 1000, 0, 0);
        MagnetometerNP[2].fill("MAG_Z", "Z Magnetic Field", "µT", -1000, 1000, 0, 0);
        MagnetometerNP.fill(m_defaultDevice->getDeviceName(), "MAGNETOMETER", "Magnetometer", groupName.c_str(), IP_RO, 0, IPS_IDLE);
    }

    // Calibration Properties
    if (HasCalibration())
    {
        CalibrationStatusLP[0].fill("CAL_SYS", "System", IPS_IDLE);
        CalibrationStatusLP[1].fill("CAL_GYRO", "Gyroscope", IPS_IDLE);
        CalibrationStatusLP[2].fill("CAL_ACCEL", "Accelerometer", IPS_IDLE);
        CalibrationStatusLP[3].fill("CAL_MAG", "Magnetometer", IPS_IDLE);
        CalibrationStatusLP.fill(m_defaultDevice->getDeviceName(), "CALIBRATION_STATUS", "Calibration Status", CALIBRATION_TAB.c_str(), IPS_IDLE);

        CalibrationControlSP[0].fill("CAL_START", "Start Calibration", ISS_OFF);
        CalibrationControlSP[1].fill("CAL_SAVE", "Save Calibration", ISS_OFF);
        CalibrationControlSP[2].fill("CAL_LOAD", "Load Calibration", ISS_OFF);
        CalibrationControlSP[3].fill("CAL_RESET", "Reset Calibration", ISS_OFF);
        CalibrationControlSP.fill(m_defaultDevice->getDeviceName(), "CALIBRATION_CONTROL", "Calibration Control", CALIBRATION_TAB.c_str(), IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
    }

    // Configuration Properties
    PowerModeSP[0].fill("NORMAL", "Normal", ISS_ON);
    PowerModeSP[1].fill("LOW_POWER", "Low Power", ISS_OFF);
    PowerModeSP[2].fill("SUSPEND", "Suspend", ISS_OFF);
    PowerModeSP.fill(m_defaultDevice->getDeviceName(), "POWER_MODE", "Power Mode", CONFIG_TAB.c_str(), IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    OperationModeSP[0].fill("IMU", "IMU", ISS_ON);
    OperationModeSP[1].fill("COMPASS", "Compass", ISS_OFF);
    OperationModeSP[2].fill("M4G", "M4G", ISS_OFF);
    OperationModeSP[3].fill("NDOF", "NDOF", ISS_OFF);
    OperationModeSP.fill(m_defaultDevice->getDeviceName(), "OPERATION_MODE", "Operation Mode", CONFIG_TAB.c_str(), IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    UnitsSP[0].fill("METRIC", "Metric", ISS_ON);
    UnitsSP[1].fill("IMPERIAL", "Imperial", ISS_OFF);
    UnitsSP[2].fill("DEGREES", "Degrees", ISS_ON);
    UnitsSP[3].fill("RADIANS", "Radians", ISS_OFF);
    UnitsSP.fill(m_defaultDevice->getDeviceName(), "UNITS", "Units", CONFIG_TAB.c_str(), IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    UpdateRateNP[0].fill("RATE", "Update Rate", "Hz", 1, 100, 1, 10);
    UpdateRateNP.fill(m_defaultDevice->getDeviceName(), "UPDATE_RATE", "Update Rate", CONFIG_TAB.c_str(), IP_RW, 0, IPS_IDLE);

    OffsetsNP[0].fill("OFFSET_X", "X Offset", "", -180, 180, 0, 0);
    OffsetsNP[1].fill("OFFSET_Y", "Y Offset", "", -180, 180, 0, 0);
    OffsetsNP[2].fill("OFFSET_Z", "Z Offset", "", -180, 180, 0, 0);
    OffsetsNP.fill(m_defaultDevice->getDeviceName(), "OFFSETS", "Manual Offsets", CONFIG_TAB.c_str(), IP_RW, 0, IPS_IDLE);

    // Status and Info Properties
    DeviceInfoTP[0].fill("CHIP_ID", "Chip ID", "");
    DeviceInfoTP[1].fill("FIRMWARE_VERSION", "Firmware Version", "");
    DeviceInfoTP[2].fill("SENSOR_STATUS", "Sensor Status", "");
    DeviceInfoTP.fill(m_defaultDevice->getDeviceName(), "DEVICE_INFO", "Device Info", STATUS_TAB.c_str(), IP_RO, 0, IPS_IDLE);

    if (HasTemperature())
    {
        TemperatureNP[0].fill("TEMPERATURE", "Temperature", "°C", -40, 85, 0, 0);
        TemperatureNP.fill(m_defaultDevice->getDeviceName(), "TEMPERATURE", "Temperature", STATUS_TAB.c_str(), IP_RO, 0, IPS_IDLE);
    }

    // Astronomical-Specific Properties
    if (HasStabilityMonitoring())
    {
        StabilityMonitoringNP[0].fill("VIBRATION_LEVEL", "Vibration Level", "RMS", 0, 100, 0, 0);
        StabilityMonitoringNP[1].fill("STABILITY_THRESHOLD", "Stability Threshold", "RMS", 0, 100, 0, 5);
        StabilityMonitoringNP.fill(m_defaultDevice->getDeviceName(), "STABILITY_MONITORING", "Stability Monitoring", ASTRONOMICAL_TAB.c_str(), IP_RW, 0, IPS_IDLE);
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
        SetUpdateRate(UpdateRateNP[0].getValue());
        return true;
    }

    if (name == OffsetsNP.getName())
    {
        OffsetsNP.update(values, names, n);
        OffsetsNP.setState(IPS_OK);
        OffsetsNP.apply();
        SetOffsets(OffsetsNP[0].getValue(), OffsetsNP[1].getValue(), OffsetsNP[2].getValue());
        return true;
    }

    if (HasStabilityMonitoring() && name == StabilityMonitoringNP.getName())
    {
        StabilityMonitoringNP.update(values, names, n);
        StabilityMonitoringNP.setState(IPS_OK);
        StabilityMonitoringNP.apply();
        SetStabilityMonitoring(StabilityMonitoringNP[0].getValue(), StabilityMonitoringNP[1].getValue());
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

        bool metric = UnitsSP[0].getState() == ISS_ON;
        bool degrees = UnitsSP[2].getState() == ISS_ON;
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

                if (switchName == "CAL_START")
                    result = StartCalibration();
                else if (switchName == "CAL_SAVE")
                    result = SaveCalibrationData();
                else if (switchName == "CAL_LOAD")
                    result = LoadCalibrationData();
                else if (switchName == "CAL_RESET")
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

    OrientationNP[0].setValue(roll);
    OrientationNP[1].setValue(pitch);
    OrientationNP[2].setValue(yaw);
    OrientationNP[3].setValue(w);
    OrientationNP.setState(IPS_OK);
    OrientationNP.apply();
    return true;
}

bool IMUInterface::SetAccelerationData(double x, double y, double z)
{
    if (!HasAcceleration())
        return false;

    AccelerationNP[0].setValue(x);
    AccelerationNP[1].setValue(y);
    AccelerationNP[2].setValue(z);
    AccelerationNP.setState(IPS_OK);
    AccelerationNP.apply();
    return true;
}

bool IMUInterface::SetGyroscopeData(double x, double y, double z)
{
    if (!HasGyroscope())
        return false;

    GyroscopeNP[0].setValue(x);
    GyroscopeNP[1].setValue(y);
    GyroscopeNP[2].setValue(z);
    GyroscopeNP.setState(IPS_OK);
    GyroscopeNP.apply();
    return true;
}

bool IMUInterface::SetMagnetometerData(double x, double y, double z)
{
    if (!HasMagnetometer())
        return false;

    MagnetometerNP[0].setValue(x);
    MagnetometerNP[1].setValue(y);
    MagnetometerNP[2].setValue(z);
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

    CalibrationStatusLP[0].setState(getCalibrationState(sys));
    CalibrationStatusLP[1].setState(getCalibrationState(gyro));
    CalibrationStatusLP[2].setState(getCalibrationState(accel));
    CalibrationStatusLP[3].setState(getCalibrationState(mag));
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

bool IMUInterface::SetDeviceInfo(const std::string &chipID, const std::string &firmwareVersion, const std::string &sensorStatus)
{
    DeviceInfoTP[0].setText(chipID.c_str());
    DeviceInfoTP[1].setText(firmwareVersion.c_str());
    DeviceInfoTP[2].setText(sensorStatus.c_str());
    DeviceInfoTP.setState(IPS_OK);
    DeviceInfoTP.apply();
    return true;
}

bool IMUInterface::SetTemperature(double temperature)
{
    if (!HasTemperature())
        return false;

    TemperatureNP[0].setValue(temperature);
    TemperatureNP.setState(IPS_OK);
    TemperatureNP.apply();
    return true;
}

bool IMUInterface::SetStabilityMonitoring(double vibrationLevel, double stabilityThreshold)
{
    if (!HasStabilityMonitoring())
        return false;

    StabilityMonitoringNP[0].setValue(vibrationLevel);
    StabilityMonitoringNP[1].setValue(stabilityThreshold);
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
