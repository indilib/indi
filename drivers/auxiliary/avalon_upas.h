/*******************************************************************************
  Avalon Instruments Universal Polar Alignment System (UPAS)

  Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com)

  GRBL-based serial protocol.

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
 * @brief The AvalonUPAS class drives the Avalon Instruments Universal Polar
 *        Alignment System (UPAS) via its GRBL-based serial protocol.
 *
 * Protocol summary:
 *   - Relative jog:  $J=G91G21{X|Y}{mm} F{feed_rate}\n
 *     (X = azimuth axis, Y = altitude axis; mm = degrees × 60 × gear_ratio [mm/arcmin])
 *   - Position query: ?  →  <State|MPos:az_mm,alt_mm|...>\n
 *   - Abort (jog cancel): 0x85 byte
 *   - Axis reverse:  $3={mask}\n  (bit 0 = AZ, bit 1 = ALT)
 *
 * Backlash compensation is done in software, since GRBL has none: when enabled,
 * every move ends in the configured approach direction. A move against that
 * direction overshoots by the backlash and is followed by a second jog back
 * (GRBL queues both jogs in its planner). Backlash is set in arcminutes.
 *
 * Capabilities: PAC_CAN_REVERSE | PAC_HAS_POSITION | PAC_HAS_BACKLASH
 * Feed rate is set per axis (PAC_FEED_RATE) instead of the interface's single speed.
 */
class AvalonUPAS : public INDI::DefaultDevice, public INDI::PACInterface
{
    public:
        AvalonUPAS();
        virtual ~AvalonUPAS() override = default;

        const char *getDefaultName() override;

        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        bool initProperties() override;
        bool updateProperties() override;
        bool saveConfigItems(FILE *fp) override;

        bool Handshake();
        void TimerHit() override;

        // PACInterface – axis movement
        IPState MoveAZ(double degrees) override;
        IPState MoveALT(double degrees) override;
        IPState MoveBoth(double azDegrees, double altDegrees) override;

        // PACInterface – abort and reverse
        bool AbortMotion() override;
        bool ReverseAZ(bool enabled) override;
        bool ReverseALT(bool enabled) override;

        // PACInterface – backlash (applied in software, see class description)
        bool SetBacklashEnabled(bool enabled) override;
        bool SetBacklashAZ(int32_t steps) override;
        bool SetBacklashALT(int32_t steps) override;

    private:
        ///////////////////////////////////////////////////////////////////////////////
        /// Communication
        ///////////////////////////////////////////////////////////////////////////////
        /**
         * @brief sendCommand Send a command to the UPAS and optionally read the response.
         * @param cmd  Null-terminated command string (newline appended automatically) or
         *             raw binary buffer when cmd_len > 0.
         * @param res  Buffer for the response; nullptr if no response is expected.
         * @param cmd_len  Length of binary command; -1 to use string mode.
         * @param res_len  Expected length of binary response; -1 to read until stop char.
         * @return True on success.
         */
        bool sendCommand(const char *cmd, char *res = nullptr, int cmd_len = -1, int res_len = -1);

        /** @brief Parse a GRBL status response (from '?') and update internal state. */
        bool parseStatus(const char *response);

        void hexDump(char *buf, const char *data, int size);

        /** @brief GRBL feed rate for a jog, so that no axis exceeds its own feed rate. */
        double feedRateFor(double azMM, double altMM);

        ///////////////////////////////////////////////////////////////////////////////
        /// Motion
        ///////////////////////////////////////////////////////////////////////////////
        /** @brief Send one relative jog. An axis with 0 mm is left out of the command. */
        bool sendJog(double azMM, double altMM);

        /** @brief Jog both axes, adding a final-approach jog when backlash compensation needs one. */
        IPState jogWithBacklash(double azMM, double altMM);

        /**
         * @brief finalApproachMM Split a move so that it ends in the configured approach direction.
         * @param axis BACKLASH_AZ or BACKLASH_ALT.
         * @param mm   Requested move; extended by the backlash if it runs against the approach direction.
         * @return The final-approach move in mm, or 0 if none is needed.
         */
        double finalApproachMM(int axis, double &mm);

        ///////////////////////////////////////////////////////////////////////////////
        /// Driver properties
        ///////////////////////////////////////////////////////////////////////////////

        // Gear ratio (mm per arcminute) for each axis; converts PAC interface degrees to GRBL mm.
        INDI::PropertyNumber GearRatioNP {2};
        enum
        {
            GEAR_AZ,
            GEAR_ALT
        };

        // GRBL feed rate (mm/min) for each axis.
        INDI::PropertyNumber FeedRateNP {2};
        enum
        {
            FEED_AZ,
            FEED_ALT
        };

        // Direction in which every move ends when backlash compensation is enabled.
        INDI::PropertySwitch AZApproachSP {2};
        INDI::PropertySwitch ALTApproachSP {2};
        enum
        {
            APPROACH_POSITIVE,
            APPROACH_NEGATIVE
        };

        // Read-only firmware version string returned at connect.
        INDI::PropertyText FirmwareTP {1};

        ///////////////////////////////////////////////////////////////////////////////
        /// Internal state
        ///////////////////////////////////////////////////////////////////////////////
        bool    m_IsMoving      {false};    ///< True while at least one axis is jogging.
        uint8_t m_ReverseFlags  {0};        ///< Bitmask: bit 0 = AZ reversed, bit 1 = ALT reversed.

        int     PortFD          {-1};
        Connection::Serial *serialConnection {nullptr};

        ///////////////////////////////////////////////////////////////////////////////
        /// Constants
        ///////////////////////////////////////////////////////////////////////////////
        static constexpr uint8_t     DRIVER_TIMEOUT   {3};   ///< Serial timeout in seconds.
        static constexpr uint8_t     DRIVER_LEN       {128}; ///< Max response buffer size.
        static constexpr char        DRIVER_STOP_CHAR {'\n'};
};
