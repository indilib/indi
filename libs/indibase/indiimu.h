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

#pragma once

#include "defaultdevice.h"
#include "indiimuinterface.h"

namespace Connection
{
class Serial;
class I2C;
}

namespace INDI
{

class IMU : public DefaultDevice, public IMUInterface
{
    public:
        IMU();
        virtual ~IMU();

        /** \struct IMUConnection
                \brief Holds the connection mode of the IMU.
            */
        enum
        {
            CONNECTION_NONE   = 1 << 0, /** Do not use any connection plugin */
            CONNECTION_SERIAL = 1 << 1, /** For regular serial and bluetooth connections */
            CONNECTION_I2C    = 1 << 2  /** For I2C connections */
        } IMUConnection;

        /**
             * @brief setConnection Set IMU connection mode. Child class should call this in the constructor before IMU registers
             * any connection interfaces
         * @param value A bitmask (ORed combination) of IMUConnection enum values (e.g., CONNECTION_SERIAL | CONNECTION_I2C).
         */
        void setSupportedConnections(const uint8_t &value);

        /**
             * @brief Get the currently configured supported connection modes.
             * @return A bitmask (ORed combination) of IMUConnection enum values.
             */
        uint8_t getSupportedConnections() const
        {
            return imuConnection;
        }

        virtual const char *getDefaultName() override
        {
            return "IMU";
        }

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        /**
         * @brief Performs a device-specific handshake after a connection is established.
         * This method should be overridden by concrete IMU drivers to implement
         * any necessary communication checks or initialization sequences with the hardware.
         * The base implementation does nothing and returns false.
         * @return true if the handshake is successful, false otherwise.
         */
        virtual bool Handshake();

    protected:
        // Helper function to convert quaternion to Euler angles (roll, pitch, yaw) in radians
        void QuaternionToEuler(double i, double j, double k, double w, double &roll, double &pitch, double &yaw);

        // Helper function to convert Euler angles (roll, pitch, yaw) in radians to a quaternion
        void EulerToQuaternion(double roll, double pitch, double yaw, double &i, double &j, double &k, double &w);

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;

        // Called by the concrete class
        bool SetOrientationData(double roll, double pitch, double yaw, double w = 0.0) override;
        bool SetAccelerationData(double x, double y, double z) override;
        bool SetGyroscopeData(double x, double y, double z) override;
        bool SetMagnetometerData(double x, double y, double z) override;

    protected:
        // Virtual functions from IMUInterface to be implemented by concrete drivers
        virtual bool SetCalibrationStatus(int sys, int gyro, int accel, int mag) override;
        virtual bool StartCalibration() override;
        virtual bool SaveCalibrationData() override;
        virtual bool LoadCalibrationData() override;
        virtual bool ResetCalibration() override;
        virtual bool SetPowerMode(const std::string &mode) override;
        virtual bool SetOperationMode(const std::string &mode) override;
        virtual bool SetDistanceUnits(bool metric) override;
        virtual bool SetAngularUnits(bool degrees) override;
        virtual bool SetUpdateRate(double rate) override;
        virtual bool SetDeviceInfo(const std::string &chipID, const std::string &firmwareVersion,
                                   const std::string &sensorStatus) override;
        virtual bool SetTemperature(double temperature) override;
        virtual bool SetStabilityMonitoring(double vibrationLevel, double stabilityThreshold) override;

        // Additional properties for astronomical coordinates and mount alignment at the driver level
        enum
        {
            AXIS1_OFFSET = 0,
            AXIS2_OFFSET,
            ROTATION_OFFSET
        };

        enum
        {
            AXIS1 = 0,
            AXIS2
        };

        enum
        {
            COORD_EQUATORIAL = 0,
            COORD_ALTAZ
        };

        enum IMUFrame
        {
            ENU = 0,
            NWU,
            SWU
        };

        enum Location
        {
            LOCATION_LATITUDE,
            LOCATION_LONGITUDE,
            LOCATION_ELEVATION
        };

        INDI::PropertyNumber AstroCoordinatesNP {2}; // HA/DEC or AZ/ALT
        INDI::PropertySwitch AstroCoordsTypeSP {2}; // Equatorial/Alt-Az
        INDI::PropertySwitch IMUFrameSP {3};

        // New properties for IMU orientation adjustment
        enum OrientationAdjustments
        {
            ROLL_MULTIPLIER = 0,
            PITCH_MULTIPLIER,
            YAW_MULTIPLIER,
            ROLL_OFFSET,
            PITCH_OFFSET,
            YAW_OFFSET
        };
        INDI::PropertyNumber OrientationAdjustmentsNP {6};

        // New properties for Sync Axis
        INDI::PropertyNumber SyncAxisNP {2};

        // New properties for Telescope pointing vector
        enum TelescopeVector
        {
            TELESCOPE_VECTOR_X = 0,
            TELESCOPE_VECTOR_Y,
            TELESCOPE_VECTOR_Z
        };
        INDI::PropertyNumber TelescopeVectorNP {3};

        // Geographic Location
        INDI::PropertyNumber GeographicCoordNP {3};
        // Magnetic Declination
        INDI::PropertyNumber MagneticDeclinationNP {1};

        Connection::Serial *serialConnection = nullptr;
        Connection::I2C *i2cConnection       = nullptr;

        int PortFD = -1;

    private:
        bool callHandshake();
        uint8_t imuConnection = CONNECTION_SERIAL | CONNECTION_I2C;

        // Last known quaternion values for recalculation
        double last_q_i = 0.0;
        double last_q_j = 0.0;
        double last_q_k = 0.0;
        double last_q_w = 1.0; // Default to identity quaternion

        // Last known raw quaternion values from the sensor
        double last_raw_q_i = 0.0;
        double last_raw_q_j = 0.0;
        double last_raw_q_k = 0.0;
        double last_raw_q_w = 1.0; // Default to identity quaternion

    protected:
        // Function to recalculate astronomical coordinates using last known quaternion
        void RecalculateAstroCoordinates();
};

}
