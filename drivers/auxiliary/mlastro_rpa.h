/*******************************************************************************
  MLAstro Robotic Polar Alignment (RPA)

  Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com)

  ESP32-based serial protocol (MLAstroRPA custom protocol).

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
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*******************************************************************************/

#pragma once

#include "defaultdevice.h"
#include "indipacinterface.h"
#include "connectionplugins/connectionserial.h"

/**
 * @brief The MLAstroRPA class drives the MLAstro Robotic Polar Alignment controller
 *        via its ESP32-based serial protocol.
 *
 * Protocol summary:
 *   - Handshake:  [MLAstroRPA-TC]\n  →  ok\n
 *   - Telemetry:  ?\n  →  <STATUS|Mpos:X.XXXXX,Y.YYYYY|>DATA_SETTING
 *   - Relative move (AZ): JoRe:1, ReDe:D,ReAM:M,ReAS:S, MAzL:1 or MAzR:1
 *   - Relative move (ALT): JoRe:1, ReDe:D,ReAM:M,ReAS:S, MAlU:1 or MAlD:1
 *   - Stop:   STOP:1\n  (smooth deceleration)
 *   - Speed:  SLvl:X\n  (1–5)
 *   - Home:   SetH:1\n / RetH:1\n / RstH:1\n
 *   - Sync:   SetH:1\n  (marks current position as 0,0 reference)
 *   - Backlash: Back:X\n, AzBl:X\n, AlBl:X\n
 *   - Reverse:  AzRD:X\n, AlRD:X\n
 *   - Motor config: chained commands + Save&Reboot:1\n
 *
 * Capabilities:
 *   PAC_HAS_SPEED | PAC_CAN_REVERSE | PAC_HAS_POSITION |
 *   PAC_CAN_HOME  | PAC_HAS_BACKLASH | PAC_CAN_SYNC
 */
class MLAstroRPA : public INDI::DefaultDevice, public INDI::PACInterface
{
    public:
        MLAstroRPA();
        virtual ~MLAstroRPA() override = default;

        const char *getDefaultName() override;

        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        bool initProperties() override;
        bool updateProperties() override;
        bool saveConfigItems(FILE *fp) override;

        bool Handshake();
        void TimerHit() override;

        ///////////////////////////////////////////////////////////////////////////////
        /// PACInterface – axis movement
        ///////////////////////////////////////////////////////////////////////////////
        IPState MoveAZ(double degrees) override;
        IPState MoveALT(double degrees) override;

        ///////////////////////////////////////////////////////////////////////////////
        /// PACInterface – abort, speed and reverse
        ///////////////////////////////////////////////////////////////////////////////
        bool AbortMotion() override;
        bool SetPACSpeed(uint16_t speed) override;
        bool ReverseAZ(bool enabled) override;
        bool ReverseALT(bool enabled) override;

        ///////////////////////////////////////////////////////////////////////////////
        /// PACInterface – home management
        ///////////////////////////////////////////////////////////////////////////////
        bool    SetHome() override;
        IPState GoHome() override;
        bool    ResetHome() override;

        ///////////////////////////////////////////////////////////////////////////////
        /// PACInterface – sync
        ///////////////////////////////////////////////////////////////////////////////
        bool SyncAZ(double degrees) override;
        bool SyncALT(double degrees) override;

        ///////////////////////////////////////////////////////////////////////////////
        /// PACInterface – backlash
        ///////////////////////////////////////////////////////////////////////////////
        bool SetBacklashEnabled(bool enabled) override;
        bool SetBacklashAZ(int32_t steps) override;
        bool SetBacklashALT(int32_t steps) override;

    private:
        ///////////////////////////////////////////////////////////////////////////////
        /// Communication
        ///////////////////////////////////////////////////////////////////////////////
        /**
         * @brief sendCommand Send a string command (newline appended automatically).
         * @param cmd  Null-terminated command string without the trailing newline.
         * @param res  Buffer for the response; nullptr if no response is expected.
         * @return True on success.
         */
        bool sendCommand(const char *cmd, char *res = nullptr);

        /**
         * @brief parsetelemetry Parse the telemetry response from '?' query.
         * @param response Full telemetry string.
         * @return True if successfully parsed.
         */
        bool parseTelemetry(const char *response);

        /**
         * @brief degreesToDMS Convert a signed decimal-degrees value to integer D, M, S and direction.
         * @param degrees  Input signed degrees.
         * @param d        Output whole degrees (always ≥ 0).
         * @param m        Output arc minutes.
         * @param s        Output arc seconds.
         * @param positive Output true if the value is positive (East/Up).
         */
        static void degreesToDMS(double degrees, int &d, int &m, int &s, bool &positive);

        /**
         * @brief sendRelativeMove Send a relative angle move command for one axis.
         * @param degrees  Signed step in degrees.
         * @param cmdPos   Protocol command for positive direction (e.g. "MAzR:1").
         * @param cmdNeg   Protocol command for negative direction (e.g. "MAzL:1").
         * @return IPS_BUSY on success, IPS_ALERT on error.
         */
        IPState sendRelativeMove(double degrees, const char *cmdPos, const char *cmdNeg);

        void hexDump(char *buf, const char *data, int size);

        ///////////////////////////////////////////////////////////////////////////////
        /// Telemetry / status tracking
        ///////////////////////////////////////////////////////////////////////////////
        /** @brief Update ManualAdjustmentNP state based on m_IsMoving. Called from TimerHit. */
        void checkMotionComplete();

        ///////////////////////////////////////////////////////////////////////////////
        /// Driver properties
        ///////////////////////////////////////////////////////////////////////////////

        // ── Main Control tab ──────────────────────────────────────────────
        /// Current machine state (READY / MOVING / HOMING / …)
        INDI::PropertyText StatusTP {1};

        /// WiFi connection indicator (read-only)
        INDI::PropertyLight WiFiLP {1};

        /// Homed indicator (read-only)
        INDI::PropertyLight HomedLP {1};

        // ── Options tab ───────────────────────────────────────────────────
        /// Azimuth soft limits [min, max] in degrees
        INDI::PropertyNumber AzSoftLimitsNP {2};
        enum { AZ_LIMIT_MIN, AZ_LIMIT_MAX };

        /// Altitude soft limits [min, max] in degrees
        INDI::PropertyNumber AltSoftLimitsNP {2};
        enum { ALT_LIMIT_MIN, ALT_LIMIT_MAX };

        // ── Motor Config tab ──────────────────────────────────────────────
        /// Azimuth motor configuration (9 parameters)
        INDI::PropertyNumber AzMotorNP {9};
        enum
        {
            AZ_RUN_CURRENT,   ///< AzIR – Run current in mA
            AZ_HOLD_CURRENT,  ///< AzIH – Hold current in mA
            AZ_MICROSTEPS,    ///< AzMS – Microsteps (8/16/32/64)
            AZ_ACCEL,         ///< AzAc – Acceleration
            AZ_DECEL,         ///< AzDec – Deceleration
            AZ_STEPS_PER_DEG, ///< AzSD – Steps per degree
            AZ_START_BOOST,   ///< AzSB – Startup booster (%)
            AZ_COOL_STEP,     ///< AzSC – Soft CoolStep (%)
            AZ_RUN_MODE,      ///< AzRM – Run mode (0=StealthChop, 1=SpreadCycle)
        };

        /// Altitude motor configuration (9 parameters)
        INDI::PropertyNumber AltMotorNP {9};
        enum
        {
            ALT_RUN_CURRENT,
            ALT_HOLD_CURRENT,
            ALT_MICROSTEPS,
            ALT_ACCEL,
            ALT_DECEL,
            ALT_STEPS_PER_DEG,
            ALT_START_BOOST,
            ALT_COOL_STEP,
            ALT_RUN_MODE,
        };

        /// Save settings to FRAM and reboot the controller
        INDI::PropertySwitch SaveRebootSP {1};

        // ── Info tab ──────────────────────────────────────────────────────
        /// WiFi / network information (read-only)
        INDI::PropertyText WiFiInfoTP {4};
        enum { WIFI_INFO_AP_SSID, WIFI_INFO_AP_IP, WIFI_INFO_STA_SSID, WIFI_INFO_STA_IP };

        ///////////////////////////////////////////////////////////////////////////////
        /// Internal state
        ///////////////////////////////////////////////////////////////////////////////
        bool m_IsMoving        {false};   ///< True while an axis is in motion.
        bool m_IsHoming        {false};   ///< True while returning to home.
        bool m_IsHomed         {false};   ///< True when a home position is set.

        int  PortFD            {-1};
        Connection::Serial *serialConnection {nullptr};

        ///////////////////////////////////////////////////////////////////////////////
        /// Constants
        ///////////////////////////////////////////////////////////////////////////////
        static constexpr uint8_t  DRIVER_TIMEOUT   {5};     ///< Serial timeout in seconds.
        static constexpr uint16_t DRIVER_LEN        {512};   ///< Max response buffer size.
        static constexpr char     DRIVER_STOP_CHAR  {'\n'};  ///< Response terminator.
        static constexpr const char *MOTOR_CONFIG_TAB {"Motor Config"};
        static constexpr const char *INFO_TAB         {"Info"};
};
