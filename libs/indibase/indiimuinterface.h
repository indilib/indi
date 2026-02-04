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

#pragma once

#include "indibase.h"
#include "indipropertynumber.h"
#include "indipropertyswitch.h"
#include "indipropertytext.h"
#include "indipropertylight.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace INDI
{

class IMUInterface
{
    public:
        /**
         * \struct IMUCapability
         * \brief Holds the capabilities of an IMU device
         */
        enum IMUCapability
        {
            IMU_HAS_ORIENTATION     = 1 << 0, /*!< Has orientation data (Roll, Pitch, Yaw) */
            IMU_HAS_ACCELERATION    = 1 << 1, /*!< Has acceleration data */
            IMU_HAS_GYROSCOPE       = 1 << 2, /*!< Has gyroscope data */
            IMU_HAS_MAGNETOMETER    = 1 << 3, /*!< Has magnetometer data */
            IMU_HAS_CALIBRATION     = 1 << 4, /*!< Supports calibration */
            IMU_HAS_TEMPERATURE     = 1 << 5, /*!< Has temperature sensor */
            IMU_HAS_STABILITY_MON   = 1 << 6  /*!< Supports stability monitoring */
        };

        enum OrientationProperty
        {
            ORIENTATION_ROLL = 0,
            ORIENTATION_PITCH,
            ORIENTATION_YAW,
            ORIENTATION_QUATERNION_W,
            ORIENTATION_PROPERTY_COUNT
        };

        enum AccelerationProperty
        {
            ACCELERATION_X = 0,
            ACCELERATION_Y,
            ACCELERATION_Z,
            ACCELERATION_PROPERTY_COUNT
        };

        enum GyroscopeProperty
        {
            GYROSCOPE_X = 0,
            GYROSCOPE_Y,
            GYROSCOPE_Z,
            GYROSCOPE_PROPERTY_COUNT
        };

        enum MagnetometerProperty
        {
            MAGNETOMETER_X = 0,
            MAGNETOMETER_Y,
            MAGNETOMETER_Z,
            MAGNETOMETER_PROPERTY_COUNT
        };

        enum CalibrationStatusProperty
        {
            CALIBRATION_STATUS_SYS = 0,
            CALIBRATION_STATUS_GYRO,
            CALIBRATION_STATUS_ACCEL,
            CALIBRATION_STATUS_MAG,
            CALIBRATION_STATUS_PROPERTY_COUNT
        };

        enum CalibrationControlProperty
        {
            CALIBRATION_CONTROL_START = 0,
            CALIBRATION_CONTROL_SAVE,
            CALIBRATION_CONTROL_LOAD,
            CALIBRATION_CONTROL_RESET,
            CALIBRATION_CONTROL_PROPERTY_COUNT
        };

        enum PowerModeProperty
        {
            POWER_MODE_NORMAL = 0,
            POWER_MODE_LOW_POWER,
            POWER_MODE_SUSPEND,
            POWER_MODE_PROPERTY_COUNT
        };

        enum OperationModeProperty
        {
            OPERATION_MODE_IMU = 0,
            OPERATION_MODE_COMPASS,
            OPERATION_MODE_M4G,
            OPERATION_MODE_NDOF,
            OPERATION_MODE_PROPERTY_COUNT
        };

        enum DistanceUnitsProperty
        {
            DISTANCE_UNITS_METRIC = 0,
            DISTANCE_UNITS_IMPERIAL,
            DISTANCE_UNITS_PROPERTY_COUNT
        };

        enum AngularUnitsProperty
        {
            ANGULAR_UNITS_DEGREES = 0,
            ANGULAR_UNITS_RADIANS,
            ANGULAR_UNITS_PROPERTY_COUNT
        };

        enum UpdateRateProperty
        {
            UPDATE_RATE_RATE = 0,
            UPDATE_RATE_PROPERTY_COUNT
        };

        enum OffsetsProperty
        {
            OFFSETS_X = 0,
            OFFSETS_Y,
            OFFSETS_Z,
            OFFSETS_PROPERTY_COUNT
        };

        enum DeviceInfoProperty
        {
            DEVICE_INFO_CHIP_ID = 0,
            DEVICE_INFO_FIRMWARE_VERSION,
            DEVICE_INFO_SENSOR_STATUS,
            DEVICE_INFO_PROPERTY_COUNT
        };

        enum TemperatureProperty
        {
            TEMPERATURE_VALUE = 0,
            TEMPERATURE_PROPERTY_COUNT
        };

        enum StabilityMonitoringProperty
        {
            STABILITY_MONITORING_VIBRATION_LEVEL = 0,
            STABILITY_MONITORING_STABILITY_THRESHOLD,
            STABILITY_MONITORING_PROPERTY_COUNT
        };

        /**
         * @brief GetIMUCapability returns the capability of the IMU device
         */
        uint32_t GetCapability() const
        {
            return imuCapability;
        }

        /**
         * @brief SetIMUCapability sets the IMU capabilities. All capabilities must be initialized.
         * @param cap ORed list of IMU capabilities.
         */
        void SetCapability(uint32_t cap)
        {
            imuCapability = cap;
        }

        bool HasOrientation()
        {
            return imuCapability & IMU_HAS_ORIENTATION;
        }
        bool HasAcceleration()
        {
            return imuCapability & IMU_HAS_ACCELERATION;
        }
        bool HasGyroscope()
        {
            return imuCapability & IMU_HAS_GYROSCOPE;
        }
        bool HasMagnetometer()
        {
            return imuCapability & IMU_HAS_MAGNETOMETER;
        }
        bool HasCalibration()
        {
            return imuCapability & IMU_HAS_CALIBRATION;
        }
        bool HasTemperature()
        {
            return imuCapability & IMU_HAS_TEMPERATURE;
        }
        bool HasStabilityMonitoring()
        {
            return imuCapability & IMU_HAS_STABILITY_MON;
        }

    protected:
        explicit IMUInterface(DefaultDevice *defaultDevice);
        virtual ~IMUInterface() = default;

        /**
         * \brief Initialize IMU properties. It is recommended to call this function within
         * initProperties() of your primary device
         * \param groupName Group or tab name to be used to define IMU properties.
         */
        void initProperties(const std::string &groupName);

        /**
         * @brief updateProperties Define or Delete IMU properties based on the connection status of the base device
         * @return True if successful, false otherwise.
         */
        bool updateProperties();

        /** \brief Process IMU number properties */
        bool processNumber(const std::string &dev, const std::string &name, double values[], char *names[], int n);

        /** \brief Process IMU switch properties */
        bool processSwitch(const std::string &dev, const std::string &name, ISState *states, char *names[], int n);

        /** \brief Process IMU text properties */
        bool processText(const std::string &dev, const std::string &name, char *texts[], char *names[], int n);

        /**
         * @brief saveConfigItems Save IMU properties in config file
         * @param fp Pointer to config file
         * @return Always return true
         */
        bool saveConfigItems(FILE *fp);

        /**
         * @brief SetOrientationData Set the orientation data (Roll, Pitch, Yaw, Quaternion W)
         * @param roll Roll angle in degrees
         * @param pitch Pitch angle in degrees
         * @param yaw Yaw/Azimuth angle in degrees
         * @param w Quaternion W component (optional)
         * @return True if successful, false otherwise
         */
        virtual bool SetOrientationData(double roll, double pitch, double yaw, double w = 0.0);

        /**
         * @brief SetAccelerationData Set the linear acceleration data
         * @param x X-axis acceleration in m/s²
         * @param y Y-axis acceleration in m/s²
         * @param z Z-axis acceleration in m/s²
         * @return True if successful, false otherwise
         */
        virtual bool SetAccelerationData(double x, double y, double z);

        /**
         * @brief SetGyroscopeData Set the angular velocity data
         * @param x X-axis angular velocity in rad/s or deg/s
         * @param y Y-axis angular velocity in rad/s or deg/s
         * @param z Z-axis angular velocity in rad/s or deg/s
         * @return True if successful, false otherwise
         */
        virtual bool SetGyroscopeData(double x, double y, double z);

        /**
         * @brief SetMagnetometerData Set the magnetic field strength data
         * @param x X-axis magnetic field strength in µT
         * @param y Y-axis magnetic field strength in µT
         * @param z Z-axis magnetic field strength in µT
         * @return True if successful, false otherwise
         */
        virtual bool SetMagnetometerData(double x, double y, double z);

        /**
         * @brief SetCalibrationStatus Set the calibration status for each sensor
         * @param sys System calibration status (0-3)
         * @param gyro Gyroscope calibration status (0-3)
         * @param accel Accelerometer calibration status (0-3)
         * @param mag Magnetometer calibration status (0-3)
         * @return True if successful, false otherwise
         */
        virtual bool SetCalibrationStatus(int sys, int gyro, int accel, int mag);

        /**
         * @brief StartCalibration Initiate the calibration sequence
         * @return True if successful, false otherwise
         */
        virtual bool StartCalibration();

        /**
         * @brief SaveCalibrationData Save the current calibration data
         * @return True if successful, false otherwise
         */
        virtual bool SaveCalibrationData();

        /**
         * @brief LoadCalibrationData Load previously saved calibration data
         * @return True if successful, false otherwise
         */
        virtual bool LoadCalibrationData();

        /**
         * @brief ResetCalibration Reset calibration data
         * @return True if successful, false otherwise
         */
        virtual bool ResetCalibration();

        /**
         * @brief SetPowerMode Set the power mode of the IMU sensor
         * @param mode Power mode (e.g., Normal, Low Power, Suspend)
         * @return True if successful, false otherwise
         */
        virtual bool SetPowerMode(const std::string &mode);

        /**
         * @brief SetOperationMode Set the operation mode of the IMU sensor
         * @param mode Operation mode (e.g., IMU, Compass, NDOF)
         * @return True if successful, false otherwise
         */
        virtual bool SetOperationMode(const std::string &mode);

        /**
         * @brief SetUnits Set the units for sensor data
         * @param metric True for metric, false for imperial
         * @return True if successful, false otherwise
         */
        virtual bool SetDistanceUnits(bool metric);

        /**
         * @brief SetAngularUnits Set the angular units for sensor data
         * @param degrees True for degrees, false for radians
         * @return True if successful, false otherwise
         */
        virtual bool SetAngularUnits(bool degrees);

        /**
         * @brief SetUpdateRate Set the sensor polling frequency
         * @param rate Frequency in Hz
         * @return True if successful, false otherwise
         */
        virtual bool SetUpdateRate(double rate);

        /**
         * @brief SetDeviceInfo Set chip ID, firmware version, and sensor status
         * @param chipID Chip ID string
         * @param firmwareVersion Firmware version string
         * @param sensorStatus Sensor status string
         * @return True if successful, false otherwise
         */
        virtual bool SetDeviceInfo(const std::string &chipID, const std::string &firmwareVersion, const std::string &sensorStatus);

        /**
         * @brief SetTemperature Set the internal chip temperature
         * @param temperature Temperature in Celsius
         * @return True if successful, false otherwise
         */
        virtual bool SetTemperature(double temperature);

        /**
         * @brief SetStabilityMonitoring Set vibration level and alert threshold
         * @param vibrationLevel RMS vibration measurement
         * @param stabilityThreshold Alert threshold
         * @return True if successful, false otherwise
         */
        virtual bool SetStabilityMonitoring(double vibrationLevel, double stabilityThreshold);

        // Core Sensor Data Properties
        INDI::PropertyNumber OrientationNP {ORIENTATION_PROPERTY_COUNT}; // Roll, Pitch, Yaw, Quaternion W
        INDI::PropertyNumber AccelerationNP {ACCELERATION_PROPERTY_COUNT}; // ACCEL_X, ACCEL_Y, ACCEL_Z
        INDI::PropertyNumber GyroscopeNP {GYROSCOPE_PROPERTY_COUNT};    // GYRO_X, GYRO_Y, GYRO_Z
        INDI::PropertyNumber MagnetometerNP {MAGNETOMETER_PROPERTY_COUNT}; // MAG_X, MAG_Y, MAG_Z

        // Calibration Properties
        INDI::PropertyLight CalibrationStatusLP {CALIBRATION_STATUS_PROPERTY_COUNT}; // CAL_SYS, CAL_GYRO, CAL_ACCEL, CAL_MAG
        INDI::PropertySwitch CalibrationControlSP {CALIBRATION_CONTROL_PROPERTY_COUNT}; // CAL_START, CAL_SAVE, CAL_LOAD, CAL_RESET

        // Configuration Properties
        INDI::PropertySwitch PowerModeSP {POWER_MODE_PROPERTY_COUNT}; // Normal/Low Power/Suspend
        INDI::PropertySwitch OperationModeSP {OPERATION_MODE_PROPERTY_COUNT}; // IMU/Compass/M4G/NDOF
        INDI::PropertySwitch DistanceUnitsSP {DISTANCE_UNITS_PROPERTY_COUNT}; // Metric/Imperial
        INDI::PropertySwitch AngularUnitsSP {ANGULAR_UNITS_PROPERTY_COUNT}; // degrees/radians
        INDI::PropertyNumber UpdateRateNP {UPDATE_RATE_PROPERTY_COUNT}; // Sensor polling frequency (Hz)
        INDI::PropertyNumber DataThresholdNP {1}; // Data change threshold

        // Status and Info Properties
        INDI::PropertyText DeviceInfoTP {DEVICE_INFO_PROPERTY_COUNT}; // Chip ID, firmware version, sensor status
        INDI::PropertyNumber TemperatureNP {TEMPERATURE_PROPERTY_COUNT}; // Internal chip temperature

        // Astronomical-Specific Properties (moved to driver level)
        INDI::PropertyNumber StabilityMonitoringNP {STABILITY_MONITORING_PROPERTY_COUNT}; // VIBRATION_LEVEL, STABILITY_THRESHOLD

    private:
        uint32_t imuCapability = 0;
        DefaultDevice *m_defaultDevice { nullptr };

    protected:
        const std::string IMU_TAB {"IMU"};
        const std::string CALIBRATION_TAB {"Calibration"};
        const std::string STATUS_TAB {"Status"};
        const std::string COORDINATES_TAB {"Coordinates"};
};

}
