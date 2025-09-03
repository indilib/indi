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

#pragma once

#include "indiimu.h"
#include "connectionplugins/connectioni2c.h"

namespace INDI
{

class BNO085 : public IMU
{
    public:
        BNO085();
        virtual ~BNO085() = default;

        virtual const char *getDefaultName() override
        {
            return "BNO085";
        }

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool Handshake() override;
        virtual void TimerHit() override;

    protected:
        // Implement virtual functions from IMUInterface
        virtual bool SetOrientationData(double roll, double pitch, double yaw, double w = 0.0) override;
        virtual bool SetAccelerationData(double x, double y, double z) override;
        virtual bool SetGyroscopeData(double x, double y, double z) override;
        virtual bool SetMagnetometerData(double x, double y, double z) override;
        virtual bool SetCalibrationStatus(int sys, int gyro, int accel, int mag) override;
        virtual bool StartCalibration() override;
        virtual bool SaveCalibrationData() override;
        virtual bool LoadCalibrationData() override;
        virtual bool ResetCalibration() override;
        virtual bool SetPowerMode(const std::string &mode) override;
        virtual bool SetOperationMode(const std::string &mode) override;
        virtual bool SetUnits(bool metric, bool degrees) override;
        virtual bool SetUpdateRate(double rate) override;
        virtual bool SetOffsets(double x, double y, double z) override;
        virtual bool SetDeviceInfo(const std::string &chipID, const std::string &firmwareVersion, const std::string &sensorStatus) override;
        virtual bool SetTemperature(double temperature) override;
        virtual bool SetStabilityMonitoring(double vibrationLevel, double stabilityThreshold) override;

    private:
        // BNO085 specific registers and commands (simplified for example)
        bool readSensorData();
        bool writeRegister(uint8_t reg, uint8_t value);
        uint8_t readRegister(uint8_t reg);
};

}
