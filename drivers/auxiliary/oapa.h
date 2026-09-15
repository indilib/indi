/*******************************************************************************
  OAPA - Open Automatic Polar Alignment

  Copyright (C) 2026 Michele Bergo

  OAPA wire protocol (GRBL-style text lines, 115200 8N1).

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

#include <chrono>

namespace Connection
{
class Serial;
}

/**
 * @brief The OAPA class drives an Open Automatic Polar Alignment platform running
 *        the OAPA reference firmware (github.com/michelebergo/oapa-firmware, >= 1.2.1)
 *        or any device implementing the same wire protocol.
 *
 * Protocol summary:
 *   - Status:       ?\n  ->  <Idle|MPos:x,y,0.00|V:1.2.2|>\n  followed by  ok\n
 *                   (state is Idle, Run or Home; MPos is in motor steps)
 *   - Relative jog: $J=G91G21X{steps}Y{steps}F{steps_per_sec}\n  ->  ok\n
 *                   (X = azimuth axis, Y = altitude axis; one feed per command)
 *   - Stop:         !\n  ->  ok\n  (both axes decelerate, positions stay true)
 *   - Driver:       C{X|Y}{mA}\n run current, H{X|Y}{percent}\n hold current  ->  ok\n
 *                   (type letter first; any other shape is acknowledged and ignored)
 *
 * The firmware never announces that a move has finished: the driver polls the
 * status frame and completes the move when the platform is Idle at the target.
 * The firmware has no reverse or backlash commands, so axis reversal is applied
 * in the driver and backlash is left to the client's closed loop. Driver currents
 * are not persisted by the controller, so they are sent on every connection.
 *
 * Capabilities: PAC_CAN_REVERSE | PAC_HAS_POSITION. Speed is per axis (OAPA_SPEED)
 * because PAC_SPEED holds a single value.
 */
class OAPA : public INDI::DefaultDevice, public INDI::PACInterface
{
    public:
        OAPA();
        virtual ~OAPA() override = default;

        const char *getDefaultName() override;

        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        bool initProperties() override;
        bool updateProperties() override;
        bool saveConfigItems(FILE *fp) override;

        bool Handshake();
        void TimerHit() override;

        // PACInterface - axis movement
        IPState MoveAZ(double degrees) override;
        IPState MoveALT(double degrees) override;
        IPState MoveBoth(double azDegrees, double altDegrees) override;

        // PACInterface - abort and reverse
        bool AbortMotion() override;
        bool ReverseAZ(bool enabled) override;
        bool ReverseALT(bool enabled) override;

    private:
        ///////////////////////////////////////////////////////////////////////////////
        /// Communication
        ///////////////////////////////////////////////////////////////////////////////
        struct Status
        {
            bool moving {false};
            double x {0};           ///< Azimuth motor position in steps.
            double y {0};           ///< Altitude motor position in steps.
            char version[16] {0};   ///< Firmware version from the V: field, empty if absent.
        };

        /** @brief Send one command line and read its single reply line into res. */
        bool sendCommand(const char *cmd, char *res);

        /** @brief Send ? and parse the status frame, consuming the trailing ok. */
        bool queryStatus(Status &status);

        /** @brief Parse a status frame; returns false if the line is not one. */
        static bool parseStatus(const char *line, Status &status);

        /** @brief Send one driver setting ('C' run mA or 'H' hold %) for one axis. */
        bool sendDriverSetting(char type, int axis, double value);

        /** @brief Send run and hold current for both axes; false if any was not acknowledged. */
        bool pushDriverConfig();

        ///////////////////////////////////////////////////////////////////////////////
        /// Motion
        ///////////////////////////////////////////////////////////////////////////////
        IPState startMove(double azDegrees, double altDegrees);

        /** @brief Convert a signed PAC step in degrees to motor steps; NaN if uncalibrated. */
        double degreesToSteps(double degrees, int axis);

        bool isReversed(int axis);
        void updatePosition(const Status &status);
        void checkMove(const Status &status);
        void finishMove(IPState state, const char *message);

        ///////////////////////////////////////////////////////////////////////////////
        /// Driver properties
        ///////////////////////////////////////////////////////////////////////////////
        enum
        {
            AXIS_AZ,
            AXIS_ALT
        };

        // Jog speed per axis in steps/s, sent as the F feed of each jog.
        INDI::PropertyNumber AxisSpeedNP {2};

        // Motor steps per arcminute of polar-axis correction, per axis. 0 = not calibrated.
        INDI::PropertyNumber StepsPerArcminNP {2};

        // TMC driver run current (mA) and hold current (% of run), per axis.
        INDI::PropertyNumber MotorCurrentNP {2};
        INDI::PropertyNumber MotorHoldNP {2};

        // Read-only firmware version reported in the status frame.
        INDI::PropertyText FirmwareTP {1};

        ///////////////////////////////////////////////////////////////////////////////
        /// Internal state
        ///////////////////////////////////////////////////////////////////////////////
        using Clock = std::chrono::steady_clock;

        bool m_MoveActive {false};
        double m_TargetX {0}, m_TargetY {0};
        double m_LastX {0}, m_LastY {0};
        Clock::time_point m_LastProgress;
        Clock::time_point m_Deadline;

        int PortFD {-1};
        Connection::Serial *serialConnection {nullptr};

        ///////////////////////////////////////////////////////////////////////////////
        /// Constants
        ///////////////////////////////////////////////////////////////////////////////
        static constexpr int    DRIVER_TIMEOUT       {2};      ///< Serial read timeout in seconds.
        static constexpr int    DRIVER_LEN           {128};    ///< Max line length.
        static constexpr char   DRIVER_STOP_CHAR     {'\n'};
        static constexpr int    HANDSHAKE_ATTEMPTS   {6};      ///< Probes while the board boots.
        static constexpr int    MIN_SPEED            {50};     ///< Firmware jog speed floor, steps/s.
        static constexpr int    MAX_SPEED            {3000};   ///< Firmware jog speed ceiling, steps/s.
        static constexpr int    DEFAULT_SPEED        {1000};   ///< Steps/s, as the N.I.N.A. plugin.
        static constexpr int    MIN_RUN_CURRENT      {100};    ///< mA.
        static constexpr int    MAX_RUN_CURRENT      {2000};   ///< mA, TMC2209 limit.
        static constexpr int    DEFAULT_RUN_CURRENT  {600};    ///< mA, firmware default.
        static constexpr int    DEFAULT_HOLD_PERCENT {25};     ///< Firmware default since 1.2.2.
        static constexpr double ACCELERATION         {1000};   ///< Firmware acceleration, steps/s^2.
        static constexpr double POSITION_TOLERANCE   {1.0};    ///< Steps.
        static constexpr double STALL_SECONDS        {3.0};    ///< Run without progress before alert.
};
