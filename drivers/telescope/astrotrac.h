/*******************************************************************************
 Copyright(c) 2021 Jasem Mutlaq. All rights reserved.

 AstroTrac Mount Driver.

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
#include "indiguiderinterface.h"

#include "alignment/AlignmentSubsystemForDrivers.h"

#include "indipropertynumber.h"
#include "indipropertytext.h"
#include "indipropertyswitch.h"
#include "indielapsedtimer.h"

class AstroTrac :
    public INDI::Telescope,
    public INDI::GuiderInterface,
    public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
    public:
        AstroTrac();
        virtual ~AstroTrac() = default;

        virtual const char *getDefaultName() override;
        virtual bool Handshake() override;
        virtual bool ReadScopeStatus() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                               char *formats[], char *names[], int n) override;

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

        // Guiding
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        // Simulation
        virtual void simulationTriggered(bool enable) override;

        // Config items
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Utility
        ///////////////////////////////////////////////////////////////////////////////////////////////
        bool getVersion();
        void getRADEFromEncoders(double haEncoder, double deEncoder, double &ra, double &de);
        void getEncodersFromRADE(double ra, double de, double &raEncoder, double &deEncoder);
        double calculateSlewTime(double distance);
        bool getTelescopeFromSkyCoordinates(double ra, double de, INDI::IEquatorialCoordinates &telescopeCoordinates);

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Acceleration
        ///////////////////////////////////////////////////////////////////////////////////////////////
        bool getAcceleration(INDI_EQ_AXIS axis);
        bool setAcceleration(INDI_EQ_AXIS axis, uint32_t a);

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Velocity
        ///////////////////////////////////////////////////////////////////////////////////////////////
        bool getVelocity(INDI_EQ_AXIS axis);

        /**
         * @brief setVelocity set motor velocity
         * @param axis Motor axis
         * @param v velocity in arcsec/sec
         * @return bool if successful, false otherwise.
         */
        bool setVelocity(INDI_EQ_AXIS axis, double value);

        bool getMaximumSlewVelocity();


        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// GOTO & SYNC
        ///////////////////////////////////////////////////////////////////////////////////////////////
        bool isSlewComplete();
        bool syncEncoder(INDI_EQ_AXIS axis, double value);
        bool slewEncoder(INDI_EQ_AXIS axis, double value);
        bool getEncoderPosition(INDI_EQ_AXIS axis);
        bool stopMotion(INDI_EQ_AXIS axis);

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Communication
        ///////////////////////////////////////////////////////////////////////////////////////////////
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
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// INDI Properties
        ///////////////////////////////////////////////////////////////////////////////////////////////
        // Mount Type
        INDI::PropertySwitch MountTypeSP {2};
        enum
        {
            MOUNT_GEM,
            MOUNT_SINGLE_ARM
        };

        // Guide Rate
        INDI::PropertyNumber GuideRateNP {2};
        // Firmware Version
        INDI::PropertyText FirmwareTP {1};
        // Acceleration
        INDI::PropertyNumber AccelerationNP {2};
        // Encoders
        INDI::PropertyNumber EncoderNP {2};

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Simulation
        ///////////////////////////////////////////////////////////////////////////////////////////////
        struct
        {
            // -90 to + 90 degrees.
            double currentMechanicalHA {0}, targetMechanicalHA {0};
            // -180 to +180 degrees.
            double currentMechanicalDE {0}, targetMechanicalDE {0};
            // arcsec/sec
            double velocity[2] = {TRACKRATE_SIDEREAL, 0};
            // arcsec/sec^2
            uint32_t acceleration[2] = {3600, 3600};
            // 1 Slewing, 0 Stopped
            uint8_t status[2] = {0, 0};
        } SimData;

        INDI::ElapsedTimer m_SimulationTimer;
        // Process very simply mount simulation. No meridian flips.
        void simulateMount();
        // Process basic commands.
        bool handleSimulationCommand(const char * cmd, char * res, int cmd_len, int res_len);

        /// Mount internal coordinates
        INDI::IEquatorialCoordinates m_MountInternalCoordinates;
        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Static Constants
        ///////////////////////////////////////////////////////////////////////////////////////////////
        // > is the stop char
        static const char DRIVER_STOP_CHAR { 0x3E };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {64};
        // Slew Modes
        static constexpr const uint8_t SLEW_MODES {10};
        // Slew Speeds
        static const std::array<uint32_t, SLEW_MODES> SLEW_SPEEDS;
        // Maximum Slew Velocity. This cannot be set now so it's considered contant until until it can be altered.
        // arcsec/sec
        static constexpr double MAX_SLEW_VELOCITY {10800.0};
        // Target threshold in degrees between mechanical target and current coordinates
        // If they are within 0.1 degrees, then we consider motion complete
        static constexpr double DIFF_THRESHOLD {0.1};
};
