/*******************************************************************************
 Copyright(c) 2026 Jasem Mutlaq. All rights reserved.

 PlaneWave Rotator Driver
 API Communication via PWI4 HTTP Interface (port 8220)

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

#include "indirotator.h"
#include "indipropertytext.h"
#include "indipropertynumber.h"
#include "indipropertyswitch.h"
#include "indipropertylight.h"
#include "inicpp.h"

#include <httplib.h>
#include <memory>

/**
 * @brief The PlaneWaveRotator class provides an INDI driver for the PlaneWave
 *        Series 5 rotator, communicating via the PWI4 HTTP API.
 *
 * Connection: TCP (default host localhost, port 8220).
 *
 * The driver exposes two positioning modes:
 *  - Field angle  (GotoRotatorNP, 0-360°) → /rotator/goto_field
 *  - Mechanical   (MechPositionNP, 0-360°) → /rotator/goto_mech
 *
 * Both positions are updated on every poll from /status.
 */
class PlaneWaveRotator : public INDI::Rotator
{
    public:
        PlaneWaveRotator();
        // Destructor must be defined in .cpp so unique_ptr<httplib::Client>
        // is destroyed after the complete type is known.
        virtual ~PlaneWaveRotator();

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool Handshake() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        // ── RotatorInterface pure-virtual overrides ──────────────────────────
        virtual IPState MoveRotator(double angle) override;
        virtual bool    AbortRotator() override;
        virtual bool    ReverseRotator(bool enabled) override;

        // ── INDI device overrides ─────────────────────────────────────────────
        virtual void TimerHit() override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        ///////////////////////////////////////////////////////////////////////
        /// HTTP helpers
        ///////////////////////////////////////////////////////////////////////
        /**
         * @brief dispatch  Send GET @p request to the PWI4 HTTP server,
         *        check for HTTP 200, and parse the key=value body into m_Status.
         * @return true on success.
         */
        bool dispatch(const std::string &request);

        /** @brief Thin wrapper: dispatch("/status"). */
        bool getStatus();

        ///////////////////////////////////////////////////////////////////////
        /// INDI Properties — Main Control tab
        ///////////////////////////////////////////////////////////////////////

        /// PWI4 software version (read-only)
        INDI::PropertyText VersionTP {1};

        /// Motor enable / disable
        INDI::PropertySwitch MotorStateSP {2};
        enum { MOTOR_ENABLE, MOTOR_DISABLE };

        /// Mechanical position (read-write — triggers /rotator/goto_mech)
        INDI::PropertyNumber MechPositionNP {1};

        /// Live status indicators: PWI4 connected, motor enabled
        INDI::PropertyLight RotatorStatusLP {2};
        enum { STATUS_CONNECTED, STATUS_ENABLED };

        ///////////////////////////////////////////////////////////////////////
        /// Private state
        ///////////////////////////////////////////////////////////////////////

        /// Parsed key=value pairs from the last /status response
        ini::IniSectionBase<std::less<std::basic_string<char>>> m_Status;

        /// Persistent HTTP client — created once in Handshake()
        std::unique_ptr<httplib::Client> m_HttpClient;

        /// Last known mechanical position (for change-detection)
        double m_LastMechPosition {-1.0};

        /// Last known field angle (for change-detection)
        double m_LastFieldAngle {-1.0};

        ///////////////////////////////////////////////////////////////////////
        /// Constants
        ///////////////////////////////////////////////////////////////////////
        static constexpr int    DEFAULT_PORT    {8220};
        static constexpr double POSITION_THRESHOLD {0.01}; ///< degrees
};
