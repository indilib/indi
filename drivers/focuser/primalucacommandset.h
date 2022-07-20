/*
    Primaluca Labs Essato-Arco-Sesto Command Set
    For USB Control Specification Document Revision 3.3 published 2020.07.08

    Copyright (C) 2022 Jasem Mutlaq

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

    JM 2022.07.16: Major refactor to using json.h and update to Essato Arco
    Document protocol revision 3.3 (8th July 2022).
*/

#pragma once

#include <cstdint>
#include "json.h"

using json = nlohmann::json;

namespace PrimalucaLabs
{
typedef struct MotorRates
{
    // Rate values: 1-10
    uint32_t accRate = 0, runSpeed = 0, decRate = 0;
} MotorRates;

typedef struct MotorCurrents
{
    // Current values: 1-10
    uint32_t accCurrent = 0, runCurrent = 0, decCurrent = 0;
    // Hold current: 1-5
    uint32_t holdCurrent = 0;
} MotorCurrents;

typedef enum
{
    MOT_1,
    MOT_2
} MotorType;

typedef enum
{
    UNIT_STEPS,
    UNIT_DEGREES,
    UNIT_ARCSECS
} Units;

class Communication
{
    public:

        explicit Communication(const std::string &name, int port) : m_DeviceName(name), m_PortFD(port) {}
        const char *getDeviceName()
        {
            return m_DeviceName.c_str();
        }

        // Communication functions
        bool sendCommand(const std::string &command, json *response = nullptr);
        template <typename T = int32_t> bool genericCommand(const std::string &motor, const std::string &type, const json &command, T *response = nullptr);
        template <typename T = int32_t> bool motorGet(MotorType type, const std::string &parameter, T &value);
        template <typename T = int32_t> bool motorSet(MotorType type, const json &command);
        template <typename T = int32_t> bool motorCommand(MotorType type, const json &command);

    private:
        std::string m_DeviceName;
        int m_PortFD {-1};

        // Maximum buffer for sending/receving.
        static constexpr const int DRIVER_LEN {1024};
        static const char DRIVER_STOP_CHAR { 0xD };
        static const char DRIVER_TIMEOUT { 5 };
};

class Esatto
{
    public:
        Esatto(const std::string &name, int port);
        const char *getDeviceName()
        {
            return m_DeviceName.c_str();
        }

        // Position
        bool getMaxPosition(uint32_t &position);
        bool isHallSensorDetected(bool &isDetected);
        bool storeAsMaxPosition();
        bool storeAsMinPosition();
        bool goOutToFindMaxPos();
        bool getAbsolutePosition(uint32_t &position);

        // Motion
        bool go(uint32_t position);
        bool stop();
        bool fastMoveOut();
        bool fastMoveIn();
        bool getCurrentSpeed(uint32_t &speed);

        // Firmware
        bool getSerialNumber(std::string &response);
        bool getFirmwareVersion(std::string &response);

        // Sensors
        bool getMotorTemp(double &value);
        bool getExternalTemp(double &value);
        bool getVoltageIn(double &value);

        // Presets
        bool applyMotorPreset(const std::string &name);
        bool setMotorUserPreset(uint32_t index, const MotorRates &rates, const MotorCurrents &currents);

        // Motor Settings
        bool getMotorSettings(struct MotorRates &rates, struct MotorCurrents &currents, bool &motorHoldActive);
        bool setMotorRates(const MotorRates &rates);
        bool setMotorCurrents(const MotorCurrents &currents);
        bool setMotorHold(bool hold);

        // Calibration
        bool initCalibration();

    private:
        std::string m_DeviceName;
        int m_PortFD {-1};
        std::unique_ptr<Communication> m_Communication;
};

class Arco
{

    public:
        explicit Arco(const std::string &name, int port);
        const char *getDeviceName()
        {
            return m_DeviceName.c_str();
        }

        // Does it work?
        bool isEnabled();

        // Motion
        bool moveAbsolutePoition(Units unit, double value);
        bool stop();

        // Position
        bool getAbsolutePosition(Units unit, double &value);
        bool sync(Units unit, double value);

        // Calibration
        bool calibrate();
        bool isCalibrating();
        bool isBusy();

        // Reverse
        bool reverse(bool enabled);
        bool isReversed();

    private:
        std::string m_DeviceName;
        int m_PortFD {-1};
        std::unique_ptr<Communication> m_Communication;
};

}
