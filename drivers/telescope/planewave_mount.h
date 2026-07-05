/*******************************************************************************
 Copyright(c) 2023 Jasem Mutlaq. All rights reserved.

 Planewave Mount
 API Communication via PWI4 HTTP Interface

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
#include "indipropertynumber.h"
#include "indipropertyswitch.h"
#include "inicpp.h"

#include <httplib.h>
#include <memory>

class PlaneWave : public INDI::Telescope
{
    public:
        PlaneWave();
        // Destructor must be declared here (not = default) because the unique_ptr
        // to the incomplete httplib::Client type must be destroyed in the .cpp.
        virtual ~PlaneWave();

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
        /// Internal helpers
        ///////////////////////////////////////////////////////////////////////////////////////////////
        bool getStatus();
        bool enableAxis(int axis, bool enable);
        double getSlewRateArcsecPerSec() const;

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// HTTP communication
        ///////////////////////////////////////////////////////////////////////////////////////////////
        /**
         * @brief dispatch Send an HTTP GET request, parse the key=value status
         *        response body into m_Status, and return true on HTTP 200.
         */
        bool dispatch(const std::string &request);

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// INDI Properties — Info tab
        ///////////////////////////////////////////////////////////////////////////////////////////////

        /// PWI4 software version string
        INDI::PropertyText FirmwareTP {1};

        /// Human-readable mount geometry (Alt-Az, Eq Fork, GEM)
        INDI::PropertyText MountGeometryTP {1};

        /// Current azimuth and altitude (read-only, updated every poll)
        INDI::PropertyNumber AltAzNP {2};
        enum { AZ_VALUE, ALT_VALUE };

        /// Per-axis servo diagnostics (read-only, updated every poll)
        INDI::PropertyNumber Axis0TelemetryNP {4};
        INDI::PropertyNumber Axis1TelemetryNP {4};
        enum
        {
            TELEMETRY_RMS_ERROR,
            TELEMETRY_DIST_TO_TARGET,
            TELEMETRY_SERVO_ERROR,
            TELEMETRY_MOTOR_CURRENT
        };

        /// Angular distance from the current pointing direction to the Sun
        INDI::PropertyNumber DistanceToSunNP {1};

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// INDI Properties — Main Control tab
        ///////////////////////////////////////////////////////////////////////////////////////////////

        /// Enable / disable the primary (AZ / RA) axis motor
        INDI::PropertySwitch Axis0MotorSP {2};
        /// Enable / disable the secondary (ALT / Dec) axis motor
        INDI::PropertySwitch Axis1MotorSP {2};

        /// Initiate a home-finding sequence (L-series mounts)
        INDI::PropertySwitch FindHomeSP {1};

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// INDI Properties — Pointing Model tab
        ///////////////////////////////////////////////////////////////////////////////////////////////

        /// Filename of the currently-loaded pointing model (read-only)
        INDI::PropertyText  ModelFilenameTP {1};

        /// Model statistics: total points, enabled points, RMS error (read-only)
        INDI::PropertyNumber ModelInfoNP {3};
        enum { MODEL_POINTS_TOTAL, MODEL_POINTS_ENABLED, MODEL_RMS_ERROR };

        /// One-click model operations: Save as Default, Clear all points
        INDI::PropertySwitch ModelOperationSP {2};
        enum { MODEL_SAVE_AS_DEFAULT, MODEL_CLEAR_POINTS };

        /// Type a filename and Apply to load that model from PWI4's model directory
        INDI::PropertyText ModelLoadFilenameTP {1};

        /// Type a filename and Apply to save the current model under that name
        INDI::PropertyText ModelSaveFilenameTP {1};

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// INDI Properties — Options tab
        ///////////////////////////////////////////////////////////////////////////////////////////////

        /// Slew time constant (seconds) — smaller = more aggressive approach to target
        INDI::PropertyNumber SlewTimeConstantNP {1};

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Member variables
        ///////////////////////////////////////////////////////////////////////////////////////////////

        /// Parsed key=value pairs from the last PWI4 status response
        ini::IniSectionBase<std::less<std::basic_string<char>>> m_Status;

        /// Persistent HTTP client — created once in Handshake(), reused for all calls
        std::unique_ptr<httplib::Client> m_HttpClient;

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Static constants
        ///////////////////////////////////////////////////////////////////////////////////////////////

        /// Slew rates in arcsec/sec corresponding to the 4 INDI slew-rate buttons
        /// (≈ 0.5×, 2×, 8×, and 50× sidereal respectively)
        static constexpr double SLEW_RATES[4] {7.5, 30.0, 120.0, 750.0};
        /// Custom tab label for pointing-model properties
        static constexpr const char *MODEL_TAB {"Pointing Model"};
};
