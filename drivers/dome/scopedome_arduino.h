/*******************************************************************************
 ScopeDome Dome INDI Driver

 Copyright(c) 2017-2021 Jarno Paananen. All rights reserved.

 based on:

 ScopeDome Windows ASCOM driver version 5.1.30

 and

 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 Baader Planetarium Dome INDI Driver

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "connectionplugins/connectioninterface.h"

#include "scopedome_dome.h"
#include <curl/curl.h>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * ScopeDome Arduino Card
 */
class ScopeDomeArduino final: public ScopeDomeCard
{
    public:
        /** Default constructor. */
        ScopeDomeArduino(ScopeDome *driver, Connection::Interface* interface);

        /** Destructor. */
        virtual ~ScopeDomeArduino();

        ScopeDomeArduino(const ScopeDomeArduino &) = delete;
        ScopeDomeArduino &operator=(const ScopeDomeArduino &) = delete;

        virtual bool detect() override;
        virtual void setPortFD(int fd) override
        {
            PortFD = fd;
        };

        // State polling
        virtual int updateState() override;
        virtual uint32_t getStatus() override;
        virtual int getRotationCounter() override;
        virtual int getRotationCounterExt() override;

        // Information
        virtual void getFirmwareVersions(double &main, double &rotary) override;
        virtual uint32_t getStepsPerRevolution() override;
        virtual bool isCalibrationNeeded() override;

        // Commands
        virtual void abort() override;
        virtual void calibrate() override;
        virtual void findHome() override;
        virtual void controlShutter(ShutterOperation operation) override;

        virtual void resetCounter() override;

        // negative means CCW, positive CW steps
        virtual void move(int steps) override;

        // Input/Output management
        virtual size_t getNumberOfSensors() override;
        virtual SensorInfo getSensorInfo(size_t index) override;
        virtual double getSensorValue(size_t index) override;

        virtual size_t getNumberOfRelays() override;
        virtual RelayInfo getRelayInfo(size_t index) override;
        virtual ISState getRelayState(size_t index) override;
        virtual void setRelayState(size_t index, ISState state) override;

        virtual size_t getNumberOfInputs() override;
        virtual InputInfo getInputInfo(size_t index) override;
        virtual ISState getInputValue(size_t index) override;

        virtual ISState getInputState(AbstractInput input) override;
        virtual int setOutputState(AbstractOutput output, ISState state) override;

        virtual void setHomeSensorPolarity(HomeSensorPolarity polarity) override;
    private:
        ScopeDome *parent;
        CURL *curl{ nullptr };

        bool performCommand(const std::string &command, std::string &response);
        std::vector<std::string> splitString(const std::string &src, char splitChar);

        int32_t rotaryEncoder {0};
        int32_t shutterEncoder {0};
        int32_t previousEncoder{0};
        uint32_t stepsPerRevolution {0};

        bool inputs[16];
        double sensors[28];
        bool relays[6];

        bool rotaryLink;
        bool moving;
        bool homing;
        bool moveShutterOnHome;
        bool calibrating;
        bool ethernet;
        Connection::Interface* interface;
        int PortFD;
        std::string hostName;
        int port;

        static constexpr int32_t encoderBaseValue = 32000;
};
