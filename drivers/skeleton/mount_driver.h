/*******************************************************************************
 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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

#include "indiguiderinterface.h"
#include "inditelescope.h"

/**
 * @brief The MountDriver class provides a simple example for development of a new
 * mount driver. Modify the driver to fit your needs.
 *
 * It supports the following features:
 * + Sideral and Custom Tracking rates.
 * + Goto & Sync
 * + NWSE Hand controller direciton key slew.
 * + Tracking On/Off.
 * + Parking & Unparking with custom parking positions.
 * + Setting Time & Location.
 *
 * On startup and by default the mount shall point to the celestial pole.
 *
 * @author Jasem Mutlaq
 */
class MountDriver : public INDI::Telescope, public INDI::GuiderInterface
{
    public:
        MountDriver();

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Commands
        ///////////////////////////////////////////////////////////////////////////////
        /**
         * @brief Handshake Attempt communication with the mount.
         * @return true if successful, false otherwise.
         */
        virtual bool Handshake() override;

        /**
         * @brief ReadScopeStatus Query the mount status, coordinate, any status indicators, pier side..etc.
         * @return True if query is successful, false otherwise.
         */
        virtual bool ReadScopeStatus() override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Motions commands.
        ///////////////////////////////////////////////////////////////////////////////

        /**
         * @brief MoveNS Start or Stop motion in the North/South DEC Axis.
         * @param dir Direction
         * @param command Start or Stop
         * @return true if successful, false otherwise.
         */
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;

        /**
         * @brief MoveWE Start or Stop motion in the East/West RA Axis.
         * @param dir Direction
         * @param command Start or Stop
         * @return true if successful, false otherwise.
         */
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

        /**
         * @brief Abort Abort all motion. If tracking, stop it.
         * @return True if successful, false otherwise.
         */
        virtual bool Abort() override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Pulse Guiding Commands
        ///////////////////////////////////////////////////////////////////////////////
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Tracking Commands
        ///////////////////////////////////////////////////////////////////////////////
        virtual bool SetTrackMode(uint8_t mode) override;
        virtual bool SetTrackEnabled(bool enabled) override;
        virtual bool SetTrackRate(double raRate, double deRate) override;

        ///////////////////////////////////////////////////////////////////////////////
        /// GOTO & Sync commands
        ///////////////////////////////////////////////////////////////////////////////
        virtual bool Goto(double RA, double DE) override;
        virtual bool Sync(double RA, double DE) override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Time, Date & Location commands.
        ///////////////////////////////////////////////////////////////////////////////
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Parking commands
        ///////////////////////////////////////////////////////////////////////////////
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Utility Functions
        ///////////////////////////////////////////////////////////////////////////////
        /**
         * @brief sendCommand Send a string command to device.
         * @param cmd Command to be sent. Can be either NULL TERMINATED or just byte buffer.
         * @param res If not nullptr, the function will wait for a response from the device. If nullptr, it returns true immediately
         * after the command is successfully sent.
         * @param cmd_len if -1, it is assumed that the @a cmd is a null-terminated string. Otherwise, it would write @a cmd_len bytes from
         * the @a cmd buffer.
         * @param res_len if -1 and if @a res is not nullptr, the function will read until it detects the default delimeter DRIVER_STOP_CHAR
         *  up to DRIVER_LEN length. Otherwise, the function will read @a res_len from the device and store it in @a res.
         * @return True if successful, false otherwise.
         */
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);

        /**
         * @brief hexDump Helper function to print non-string commands to the logger so it is easier to debug
         * @param buf buffer to format the command into hex strings.
         * @param data the command
         * @param size length of the command in bytes.
         * @note This is called internally by sendCommand, no need to call it directly.
         */
        void hexDump(char * buf, const char * data, int size);

    private:
        ///////////////////////////////////////////////////////////////////////////////
        /// Additional Properties
        ///////////////////////////////////////////////////////////////////////////////
        INumber GuideRateN[2];
        INumberVectorProperty GuideRateNP;

        ///////////////////////////////////////////////////////////////////////////////
        /// Class Variables
        ///////////////////////////////////////////////////////////////////////////////
        IGeographicCoordinates m_GeographicLocation { 0, 0 };

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        // '#' is the stop char
        static const char DRIVER_STOP_CHAR { 0x23 };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {64};

};
