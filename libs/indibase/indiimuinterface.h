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
        enum
        {
            IMU_HAS_ORIENTATION     = 1 << 0, /*!< Has orientation data (Roll, Pitch, Yaw) */
            IMU_HAS_ACCELERATION    = 1 << 1, /*!< Has acceleration data */
            IMU_HAS_GYROSCOPE       = 1 << 2, /*!< Has gyroscope data */
            IMU_HAS_MAGNETOMETER    = 1 << 3, /*!< Has magnetometer data */
            IMU_HAS_CALIBRATION     = 1 << 4, /*!< Supports calibration */
            IMU_HAS_TEMPERATURE     = 1 << 5, /*!< Has temperature sensor */
            IMU_HAS_STABILITY_MON   = 1 << 6  /*!< Supports stability monitoring */
        } IMUCapability;

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
        virtual void initProperties(const std::string &groupName);

        /**
         * @brief updateProperties Define or Delete IMU properties based on the connection status of the base device
         * @return True if successful, false otherwise.
         */
        virtual bool updateProperties();

        /** \brief Process IMU number properties */
        virtual bool processNumber(const std::string &dev, const std::string &name, double values[], char *names[], int n);

        /** \brief Process IMU switch properties */
        virtual bool processSwitch(const std::string &dev, const std::string &name, ISState *states, char *names[], int n);

        /** \brief Process IMU text properties */
        virtual bool processText(const std::string &dev, const std::string &name, char *texts[], char *names[], int n);

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
         * @param degrees True for degrees, false for radians
         * @return True if successful, false otherwise
         */
        virtual bool SetUnits(bool metric, bool degrees);

        /**
         * @brief SetUpdateRate Set the sensor polling frequency
         * @param rate Frequency in Hz
         * @return True if successful, false otherwise
         */
        virtual bool SetUpdateRate(double rate);

        /**
         * @brief SetOffsets Set manual offset corrections
         * @param x X-axis offset
         * @param y Y-axis offset
         * @param z Z-axis offset
         * @return True if successful, false otherwise
         */
        virtual bool SetOffsets(double x, double y, double z);

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

        /**
         * @brief saveConfigItems Save IMU properties in config file
         * @param fp Pointer to config file
         * @return Always return true
         */
        virtual bool saveConfigItems(FILE *fp);

        // Core Sensor Data Properties
        INDI::PropertyNumber OrientationNP {4}; // Roll, Pitch, Yaw, Quaternion W
        INDI::PropertyNumber AccelerationNP {3}; // ACCEL_X, ACCEL_Y, ACCEL_Z
        INDI::PropertyNumber GyroscopeNP {3};    // GYRO_X, GYRO_Y, GYRO_Z
        INDI::PropertyNumber MagnetometerNP {3}; // MAG_X, MAG_Y, MAG_Z

        // Calibration Properties
        INDI::PropertyLight CalibrationStatusLP {4}; // CAL_SYS, CAL_GYRO, CAL_ACCEL, CAL_MAG
        INDI::PropertySwitch CalibrationControlSP {4}; // CAL_START, CAL_SAVE, CAL_LOAD, CAL_RESET

        // Configuration Properties
        INDI::PropertySwitch PowerModeSP {3}; // Normal/Low Power/Suspend
        INDI::PropertySwitch OperationModeSP {4}; // IMU/Compass/M4G/NDOF
        INDI::PropertySwitch UnitsSP {4}; // Metric/Imperial, degrees/radians
        INDI::PropertyNumber UpdateRateNP {1}; // Sensor polling frequency (Hz)
        INDI::PropertyNumber OffsetsNP {3}; // Manual offset corrections

        // Status and Info Properties
        INDI::PropertyText DeviceInfoTP {3}; // Chip ID, firmware version, sensor status
        INDI::PropertyNumber TemperatureNP {1}; // Internal chip temperature

        // Astronomical-Specific Properties (moved to driver level)
        INDI::PropertyNumber StabilityMonitoringNP {2}; // VIBRATION_LEVEL, STABILITY_THRESHOLD

    private:
        uint32_t imuCapability = 0;
        DefaultDevice *m_defaultDevice { nullptr };

    protected:
        const std::string IMU_TAB {"IMU"};
        const std::string CALIBRATION_TAB {"Calibration"};
        const std::string CONFIG_TAB {"Configuration"};
        const std::string STATUS_TAB {"Status"};
        const std::string ASTRONOMICAL_TAB {"Astronomical"};
};

}
