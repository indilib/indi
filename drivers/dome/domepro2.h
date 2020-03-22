/*******************************************************************************
 Copyright(c) 2020 Jasem Mutlaq. All rights reserved.

 Astrometric Solutions DomePro2 INDI Driver

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

#include "indibase/indidome.h"

class DomePro2 : public INDI::Dome
{
    public:
        DomePro2();
        virtual ~DomePro2() override = default;

        virtual const char *getDefaultName() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;


    protected:
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool Handshake() override;
        virtual void TimerHit() override;

        virtual IPState MoveRel(double azDiff) override;
        virtual IPState MoveAbs(double az) override;
        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
        virtual IPState ControlShutter(ShutterOperation operation) override;
        virtual bool Abort() override;
        virtual bool Sync(double az) override;

        // Parking
        virtual IPState Park() override;
        virtual IPState UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

    private:

        ShutterOperation m_TargetShutter { SHUTTER_OPEN };
        double targetAz {0};

        ///////////////////////////////////////////////////////////////////////////////
        /// Setup Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool setupInitialParameters();

        ///////////////////////////////////////////////////////////////////////////////
        /// Query Functions
        ///////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////
        /// Parking, Homing, and Calibration
        ///////////////////////////////////////////////////////////////////////////////


        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////////

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * INFO_TAB = "Info";
        // 0xD is the stop char
        static const char DRIVER_STOP_CHAR { 0x0D };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {64};



};
