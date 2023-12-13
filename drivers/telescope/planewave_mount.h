/*******************************************************************************
 Copyright(c) 2023 Jasem Mutlaq. All rights reserved.

 Planewave Mount
 API Communication

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

#include "inditelescope.h"
#include "indipropertytext.h"

#include "inicpp.h"

class PlaneWave : public INDI::Telescope
{
    public:
        PlaneWave();
        virtual ~PlaneWave() = default;

        virtual const char *getDefaultName() override;
        virtual bool Handshake() override;
        virtual bool ReadScopeStatus() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        // Motion
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        virtual bool Abort() override;

        // Time & Location
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        virtual bool updateTime(ln_date *utc, double utc_offset) override;

        // GOTO & SYNC
        virtual bool Goto(double ra, double dec) override;
        virtual bool Sync(double ra, double dec) override;

        // Tracking
        virtual bool SetTrackMode(uint8_t mode) override;
        virtual bool SetTrackRate(double raRate, double deRate) override;
        virtual bool SetTrackEnabled(bool enabled) override;

        // Parking
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;
        virtual bool Park() override;
        virtual bool UnPark() override;

        // Config items
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Utility
        ///////////////////////////////////////////////////////////////////////////////////////////////
        bool getStatus();

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// GOTO & SYNC
        ///////////////////////////////////////////////////////////////////////////////////////////////


        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Communication
        ///////////////////////////////////////////////////////////////////////////////////////////////
        bool dispatch(const std::string &request);

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// INDI Properties
        ///////////////////////////////////////////////////////////////////////////////////////////////        
        // Firmware Version
        INDI::PropertyText FirmwareTP {1};

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Variables
        ///////////////////////////////////////////////////////////////////////////////////////////////
        ini::IniSectionBase<std::less<std::basic_string<char>>> m_Status;

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Static Constants
        ///////////////////////////////////////////////////////////////////////////////////////////////
        // 0xA is the stop char
        static const char DRIVER_STOP_CHAR { 0xA };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receiving.
        static constexpr const uint8_t DRIVER_LEN {128};
};
