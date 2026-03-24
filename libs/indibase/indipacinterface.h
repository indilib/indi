/*
    PAC Interface (Polar Alignment Correction)
    Copyright (C) 2026 Joaquin Rodriguez (jrhuerta@gmail.com)
    Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#pragma once

#include "indibase.h"
#include "indidriver.h"
#include "indipropertyswitch.h"
#include "indipropertynumber.h"
#include <cstdint>

/**
 * \class PACInterface
 * \brief Provides interface to implement automated polar alignment correction (PAC).
 *
 * The interface exposes manual step controls via the PAC_MANUAL_ADJUSTMENT number property.
 * A client (e.g. Ekos) can nudge the mount by a signed number of degrees on either axis:
 *
 * \b Sign convention for manual adjustments:
 *   - Azimuth  (MANUAL_AZ_STEP):  positive value → East,  negative value → West
 *   - Altitude (MANUAL_ALT_STEP): positive value → North, negative value → South
 *
 * The ManualAdjustmentNP property state reflects the overall motion status:
 *   - IPS_BUSY  – either axis is still moving
 *   - IPS_ALERT – either axis returned an error
 *   - IPS_OK    – both axes finished successfully
 *
 * Optional capability-gated properties:
 *   - PAC_HAS_POSITION : PositionNP reports the current AZ/ALT offset in degrees (read-only)
 *   - PAC_HAS_SPEED    : SpeedNP controls the motor speed
 *   - PAC_CAN_REVERSE  : AZReverseSP / ALTReverseSP reverse individual axis direction
 *
 * \e IMPORTANT: initProperties() must be called before any other function.
 * \e IMPORTANT: updateProperties() must be called in your driver's updateProperties().
 * \e IMPORTANT: processSwitch() must be called in your driver's ISNewSwitch().
 * \e IMPORTANT: processNumber() must be called in your driver's ISNewNumber().
 *
 * \author Joaquin Rodriguez, Jasem Mutlaq
 */

namespace INDI
{

class PACInterface
{
    public:
        /** Indices for the ManualAdjustmentNP number property. */
        enum
        {
            MANUAL_AZ,   ///< Azimuth step in degrees:  positive = East,  negative = West
            MANUAL_ALT   ///< Altitude step in degrees: positive = North, negative = South
        };

        /** Indices for the PositionNP number property. */
        enum
        {
            POSITION_AZ,   ///< Current azimuth position in degrees
            POSITION_ALT   ///< Current altitude position in degrees
        };

        /**
         * @brief PAC capability flags passed to SetCapability().
         */
        enum
        {
            PAC_HAS_SPEED    = 1 << 0,  /*!< Device supports variable motor speed. */
            PAC_CAN_REVERSE  = 1 << 1,  /*!< Device supports reversing axis direction. */
            PAC_HAS_POSITION = 1 << 2,  /*!< Device can report current axis position. */
            PAC_CAN_HOME     = 1 << 3,  /*!< Device supports Set/Return/Reset home. */
            PAC_HAS_BACKLASH = 1 << 4,  /*!< Device supports backlash compensation. */
            PAC_CAN_SYNC     = 1 << 5,  /*!< Device can sync AZ/ALT position reference. */
        } PACCapability;

        /** @return Current capability bitmask. */
        uint32_t GetCapability() const
        {
            return m_Capability;
        }

        /**
         * @brief SetCapability sets the PAC device capabilities. Call this in the driver constructor.
         * @param cap Bitmask of PAC_HAS_SPEED, PAC_CAN_REVERSE, and/or PAC_HAS_POSITION.
         */
        void SetCapability(uint32_t cap)
        {
            m_Capability = cap;
        }

        /** @return True if the device supports variable speed. */
        bool HasSpeed()
        {
            return m_Capability & PAC_HAS_SPEED;
        }

        /** @return True if the device supports reversing axis direction. */
        bool CanReverse()
        {
            return m_Capability & PAC_CAN_REVERSE;
        }

        /** @return True if the device can report its current axis position. */
        bool HasPosition()
        {
            return m_Capability & PAC_HAS_POSITION;
        }

        /** @return True if the device supports Set/Return/Reset home. */
        bool CanHome()
        {
            return m_Capability & PAC_CAN_HOME;
        }

        /** @return True if the device supports backlash compensation. */
        bool HasBacklash()
        {
            return m_Capability & PAC_HAS_BACKLASH;
        }

        /** @return True if the device can sync AZ/ALT position reference. */
        bool CanSync()
        {
            return m_Capability & PAC_CAN_SYNC;
        }

    protected:
        explicit PACInterface(DefaultDevice *device);
        virtual ~PACInterface() = default;

        /**
         * @brief Move the azimuth axis by the given number of degrees.
         *
         * Sign convention: positive = East, negative = West.
         *
         * The default implementation returns IPS_ALERT. Drivers with motorised
         * azimuth adjustment must override this method.
         *
         * @param degrees Signed step size in degrees.
         * @return IPS_OK if completed immediately, IPS_BUSY if in progress, IPS_ALERT on error.
         */
        virtual IPState MoveAZ(double degrees);

        /**
         * @brief Move the altitude axis by the given number of degrees.
         *
         * Sign convention: positive = North (increase altitude), negative = South.
         *
         * The default implementation returns IPS_ALERT. Drivers with motorised
         * altitude adjustment must override this method.
         *
         * @param degrees Signed step size in degrees.
         * @return IPS_OK if completed immediately, IPS_BUSY if in progress, IPS_ALERT on error.
         */
        virtual IPState MoveALT(double degrees);

        /**
         * @brief AbortMotion Abort all axis motion immediately.
         *
         * The default implementation logs an error and returns false. Drivers that support
         * hardware abort should override this method.
         *
         * @return True if motion was successfully aborted, false otherwise.
         */
        virtual bool AbortMotion();

        /**
         * @brief SetPACSpeed Set the motor speed used for axis movements.
         *
         * Only called when PAC_HAS_SPEED capability is set. The default implementation
         * logs an error and returns false; drivers with variable-speed hardware must override.
         *
         * @param speed Speed value (range defined by the driver, default 1–10).
         * @return True if the speed was applied successfully, false otherwise.
         */
        virtual bool SetPACSpeed(uint16_t speed);

        /**
         * @brief ReverseAZ Reverse the direction of azimuth axis movement.
         *
         * Only called when PAC_CAN_REVERSE capability is set.
         *
         * @param enabled True to reverse, false to restore normal direction.
         * @return True if successful, false otherwise.
         */
        virtual bool ReverseAZ(bool enabled);

        /**
         * @brief ReverseALT Reverse the direction of altitude axis movement.
         *
         * Only called when PAC_CAN_REVERSE capability is set.
         *
         * @param enabled True to reverse, false to restore normal direction.
         * @return True if successful, false otherwise.
         */
        virtual bool ReverseALT(bool enabled);

        // ──────────────────────────────────────────────────────────────────
        // PAC_CAN_HOME
        // ──────────────────────────────────────────────────────────────────

        /**
         * @brief SetHome Mark the current position as the home (zero) reference.
         *
         * Only called when PAC_CAN_HOME is set. Default implementation returns false.
         *
         * @return True if successful, false otherwise.
         */
        virtual bool SetHome();

        /**
         * @brief GoHome Slew both axes back to the home (0, 0) position.
         *
         * Only called when PAC_CAN_HOME is set. Default returns IPS_ALERT.
         *
         * @return IPS_OK if completed immediately, IPS_BUSY if in progress, IPS_ALERT on error.
         */
        virtual IPState GoHome();

        /**
         * @brief ResetHome Clear the stored home reference without moving.
         *
         * Only called when PAC_CAN_HOME is set. Default implementation returns false.
         *
         * @return True if successful, false otherwise.
         */
        virtual bool ResetHome();

        // ──────────────────────────────────────────────────────────────────
        // PAC_CAN_SYNC
        // ──────────────────────────────────────────────────────────────────

        /**
         * @brief SyncAZ Sync the azimuth position reference to the given value without moving.
         *
         * Only called when PAC_CAN_SYNC is set. Default returns false.
         *
         * @param degrees Desired current azimuth reading in degrees.
         * @return True if successful, false otherwise.
         */
        virtual bool SyncAZ(double degrees);

        /**
         * @brief SyncALT Sync the altitude position reference to the given value without moving.
         *
         * Only called when PAC_CAN_SYNC is set. Default returns false.
         *
         * @param degrees Desired current altitude reading in degrees.
         * @return True if successful, false otherwise.
         */
        virtual bool SyncALT(double degrees);

        // ──────────────────────────────────────────────────────────────────
        // PAC_HAS_BACKLASH
        // ──────────────────────────────────────────────────────────────────

        /**
         * @brief SetBacklashEnabled Enable or disable hardware backlash compensation.
         *
         * Only called when PAC_HAS_BACKLASH is set. Default returns false.
         *
         * @param enabled True to enable backlash compensation, false to disable.
         * @return True if successful, false otherwise.
         */
        virtual bool SetBacklashEnabled(bool enabled);

        /**
         * @brief SetBacklashAZ Set the azimuth axis backlash compensation value in steps.
         *
         * Only called when PAC_HAS_BACKLASH is set. Default returns false.
         *
         * @param steps Number of motor steps used to compensate azimuth backlash.
         * @return True if successful, false otherwise.
         */
        virtual bool SetBacklashAZ(int32_t steps);

        /**
         * @brief SetBacklashALT Set the altitude axis backlash compensation value in steps.
         *
         * Only called when PAC_HAS_BACKLASH is set. Default returns false.
         *
         * @param steps Number of motor steps used to compensate altitude backlash.
         * @return True if successful, false otherwise.
         */
        virtual bool SetBacklashALT(int32_t steps);

        /**
         * @brief initProperties Initialize PAC properties.
         *        Call this from your driver's initProperties().
         * @param group Group or tab name for the properties.
         */
        void initProperties(const char *group);

        /**
         * @brief updateProperties Define or delete properties based on connection state.
         *        Call this from your driver's updateProperties().
         * @return true on success.
         */
        bool updateProperties();

        /** @brief Process PAC switch properties. */
        bool processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        /** @brief Process PAC number properties. */
        bool processNumber(const char *dev, const char *name, double values[], char *names[], int n);

        /**
         * @brief saveConfigItems Save PAC interface properties to config file.
         *        Call this from your driver's saveConfigItems().
         * @param fp Pointer to the open config file.
         * @return True on success.
         */
        bool saveConfigItems(FILE *fp);

        /**
         * Manual adjustment property.
         * Element [MANUAL_AZ]  : azimuth step in degrees  (+East / −West).
         * Element [MANUAL_ALT] : altitude step in degrees (+North / −South).
         * Writing a non-zero value to either element immediately triggers the corresponding move.
         *
         * State reflects overall motion:
         *   IPS_BUSY  – one or both axes still moving
         *   IPS_ALERT – one or both axes encountered an error
         *   IPS_OK    – both axes completed successfully
         */
        INDI::PropertyNumber ManualAdjustmentNP {2};

        /**
         * Abort motion property. Pressing Abort calls AbortMotion() and resets
         * ManualAdjustmentNP and PositionNP to IPS_IDLE.
         */
        INDI::PropertySwitch AbortSP {1};

        /**
         * Current axis position in degrees. Only defined when PAC_HAS_POSITION is set.
         * Drivers should update this property periodically (e.g. from TimerHit).
         */
        INDI::PropertyNumber PositionNP {2};

        /**
         * Motor speed. Only defined when PAC_HAS_SPEED capability is set.
         * Default range 1–10; drivers may adjust min/max/step in their initProperties()
         * after calling PACI::initProperties().
         */
        INDI::PropertyNumber SpeedNP {1};

        /**
         * Azimuth axis reverse switch. Only defined when PAC_CAN_REVERSE is set.
         */
        INDI::PropertySwitch AZReverseSP {2};

        /**
         * Altitude axis reverse switch. Only defined when PAC_CAN_REVERSE is set.
         */
        INDI::PropertySwitch ALTReverseSP {2};

        /**
         * Home management switch. Only defined when PAC_CAN_HOME is set.
         * Elements: [HOME_SET] Set home, [HOME_GO] Return to home, [HOME_RESET] Reset home.
         */
        INDI::PropertySwitch HomeSP {3};
        enum { HOME_SET, HOME_GO, HOME_RESET };

        /**
         * Sync position numbers. Only defined when PAC_CAN_SYNC is set.
         * Writing a value syncs that axis position reference without moving.
         * Element [SYNC_AZ] : new azimuth reference in degrees.
         * Element [SYNC_ALT]: new altitude reference in degrees.
         */
        INDI::PropertyNumber SyncNP {2};
        enum { SYNC_AZ, SYNC_ALT };

        /**
         * Backlash enable/disable switch. Only defined when PAC_HAS_BACKLASH is set.
         */
        INDI::PropertySwitch BacklashSP {2};

        /**
         * Backlash compensation steps per axis. Only defined when PAC_HAS_BACKLASH is set.
         * Element [BACKLASH_AZ] : azimuth backlash in motor steps.
         * Element [BACKLASH_ALT]: altitude backlash in motor steps.
         */
        INDI::PropertyNumber BacklashNP {2};
        enum { BACKLASH_AZ, BACKLASH_ALT };

    private:
        DefaultDevice *m_DefaultDevice {nullptr};
        uint32_t m_Capability {0};
};

}

using PACI = INDI::PACInterface;
