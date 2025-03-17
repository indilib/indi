/*
    Filter Interface
    Copyright (C) 2011 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include "indipropertynumber.h"
#include "indipropertyswitch.h"
#include "indipropertytext.h"
#include <stdint.h>

// Alias
using FI = INDI::FocuserInterface;

namespace INDI
{

/**
 * \class FocuserInterface
   \brief Provides interface to implement focuser functionality.

   A focuser can be an independent device, or an embedded focuser within another device (e.g. Camera or mount).

   When developing a driver for a fully indepdent focuser device, use INDI::Focuser directly. To add focus functionality to
   an existing mount or camera driver, subclass INDI::FocuserInterface. In your driver, then call the necessary focuser interface functions.

   <table>
   <tr><th>Function</th><th>Where to call it from your driver</th></tr>
   <tr><td>FI::SetCapability</td><td>Constructor</td></tr>
   <tr><td>FI::initProperties</td><td>initProperties()</td></tr>
   <tr><td>FI::updateProperties</td><td>updateProperties()</td></tr>
   <tr><td>FI::processNumber</td><td>ISNewNumber(...) Check if the property name contains FOCUS_* and then call FI::processNumber(..) for such properties</td></tr>
   <tr><td>FI::processSwitch</td><td>ISNewSwitch(...)</td></tr>
   </table>

   The interface supports three types of focusers:
   + **Absolute Position**: Focusers that know their absolute position in steps on power up. You can issue GOTO to an absolute position. By definition,
   position 0 (zero) is where the focuser is completely @a retracted (i.e. closest to the OTA) and it increases positively as it move @a outwards. When moving
   @a inward, the position value decreases. @warning Negative values are not accepted for absolute focusers.
   + **Relateive Position**: Focusers that can move in specific steps inwward or outward, but have no information on absolute position on power up.
   + **DC Motor**: Focusers without any position feedback. The only way to reliably control them is by using timers and moving them for specific pulses in or
   out.

   Implement and overwrite the rest of the virtual functions as needed. INDI GPhoto driver is a good example to check for an actual implementation
   of a focuser interface within a CCD driver.
\author Jasem Mutlaq
*/
class FocuserInterface
{
    public:
        enum FocusDirection
        {
            FOCUS_INWARD,
            FOCUS_OUTWARD
        };

        enum
        {
            FOCUSER_CAN_ABS_MOVE       = 1 << 0, /*!< Can the focuser move by absolute position? */
            FOCUSER_CAN_REL_MOVE       = 1 << 1, /*!< Can the focuser move by relative position? */
            FOCUSER_CAN_ABORT          = 1 << 2, /*!< Is it possible to abort focuser motion? */
            FOCUSER_CAN_REVERSE        = 1 << 3, /*!< Is it possible to reverse focuser motion? */
            FOCUSER_CAN_SYNC           = 1 << 4, /*!< Can the focuser sync to a custom position */
            FOCUSER_HAS_VARIABLE_SPEED = 1 << 5, /*!< Can the focuser move in different configurable speeds? */
            FOCUSER_HAS_BACKLASH       = 1 << 6  /*!< Can the focuser compensate for backlash? */
        } FocuserCapability;

        /**
         * @brief GetFocuserCapability returns the capability of the focuser
         */
        uint32_t GetCapability() const
        {
            return capability;
        }

        /**
         * @brief FI::SetCapability sets the focuser capabilities. All capabilities must be initialized.
         * @param cap pointer to focuser capability struct.
         */
        void SetCapability(uint32_t cap)
        {
            capability = cap;
        }

        /**
         * @return True if the focuser has absolute position encoders.
         */
        bool CanAbsMove()
        {
            return capability & FOCUSER_CAN_ABS_MOVE;
        }

        /**
         * @return True if the focuser has relative position encoders.
         */
        bool CanRelMove()
        {
            return capability & FOCUSER_CAN_REL_MOVE;
        }

        /**
         * @return True if the focuser motion can be aborted.
         */
        bool CanAbort()
        {
            return capability & FOCUSER_CAN_ABORT;
        }

        /**
         * @return True if the focuser motion can be reversed.
         */
        bool CanReverse()
        {
            return capability & FOCUSER_CAN_REVERSE;
        }

        /**
         * @return True if the focuser motion can be reversed.
         */
        bool CanSync()
        {
            return capability & FOCUSER_CAN_SYNC;
        }

        /**
         * @return True if the focuser has multiple speeds.
         */
        bool HasVariableSpeed()
        {
            return capability & FOCUSER_HAS_VARIABLE_SPEED;
        }

        /**
         * @return True if the focuser supports backlash.
         */
        bool HasBacklash()
        {
            return capability & FOCUSER_HAS_BACKLASH;
        }

    protected:
        explicit FocuserInterface(DefaultDevice * defaultDevice);
        virtual ~FocuserInterface() = default;

        /**
         * \brief Initialize focuser properties. It is recommended to call this function within
         * initProperties() of your primary device
         * \param groupName Group or tab name to be used to define focuser properties.
         */
        void initProperties(const char * groupName);

        /**
         * @brief updateProperties Define or Delete Rotator properties based on the connection status of the base device
         * @return True if successful, false otherwise.
         */
        bool updateProperties();

        /** \brief Process focus number properties */
        bool processNumber(const char * dev, const char * name, double values[], char * names[], int n);

        /** \brief Process focus switch properties */
        bool processSwitch(const char * dev, const char * name, ISState * states, char * names[], int n);

        /**
         * @brief SetFocuserSpeed Set Focuser speed
         * @param speed focuser speed
         * @return true if successful, false otherwise
         */
        virtual bool SetFocuserSpeed(int speed);

        /**
         * \brief MoveFocuser the focuser in a particular direction with a specific speed for a
         * finite duration.
         * \param dir Direction of focuser, either FOCUS_INWARD or FOCUS_OUTWARD.
         * \param speed Speed of focuser if supported by the focuser.
         * \param duration The timeout in milliseconds before the focus motion halts. Pass 0 to move indefinitely.
         * \return Return IPS_OK if motion is completed and focuser reached requested position.
         * Return IPS_BUSY if focuser started motion to requested position and is in progress.
         * Return IPS_ALERT if there is an error.
         */
        virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);

        /**
         * \brief MoveFocuser the focuser to an absolute position.
         * \param ticks The new position of the focuser.
         * \return Return IPS_OK if motion is completed and focuser reached requested position. Return
         * IPS_BUSY if focuser started motion to requested position and is in progress.
         * Return IPS_ALERT if there is an error.
         */
        virtual IPState MoveAbsFocuser(uint32_t targetTicks);

        /**
         * \brief MoveFocuser the focuser to an relative position.
         * \param dir Direction of focuser, either FOCUS_INWARD or FOCUS_OUTWARD.
         * \param ticks The relative ticks to move.
         * \return Return IPS_OK if motion is completed and focuser reached requested position. Return
         * IPS_BUSY if focuser started motion to requested position and is in progress.
         * Return IPS_ALERT if there is an error.
         */
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);

        /**
         * @brief ReverseFocuser Reverse focuser motion direction
         * @param enabled If true, normal default focuser motion is reversed. If false, the direction is set to the default focuser motion.
         * @return True if successful, false otherwise.
         */
        virtual bool ReverseFocuser(bool enabled);

        /**
         * @brief SyncFocuser Set current position to ticks without moving the focuser.
         * @param ticks Desired new sync position.
         * @return True if successful, false otherwise.
         */
        virtual bool SyncFocuser(uint32_t ticks);

        /**
         * @brief SetFocuserMaxPosition Set Focuser Maximum position limit in the hardware.
         * @param ticks maximum steps permitted
         * @return True if successful, false otherwise.
         * @note If setting maximum position limit in the hardware is not available or not supported, do not override this function as the default
         * implementation will always return true.
         */
        virtual bool SetFocuserMaxPosition(uint32_t ticks);

        /**
         * @brief SetFocuserBacklash Set the focuser backlash compensation value
         * @param steps value in absolute steps to compensate
         * @return True if successful, false otherwise.
         */
        virtual bool SetFocuserBacklash(int32_t steps);

        /**
         * @brief SetFocuserBacklashEnabled Enables or disables the focuser backlash compensation
         * @param enable flag to enable or disable backlash compensation
         * @return True if successful, false otherwise.
         */
        virtual bool SetFocuserBacklashEnabled(bool enabled);

        /**
         * @brief AbortFocuser all focus motion
         * @return True if abort is successful, false otherwise.
         */
        virtual bool AbortFocuser();

        /**
         * @brief saveConfigItems save focuser properties defined in the interface in config file
         * @param fp pointer to config file
         * @return Always return true
         */
        bool saveConfigItems(FILE * fp);

        // Focuser Speed (if variable speeds are supported)
        INDI::PropertyNumber FocusSpeedNP {1};

        // Focuser Motion switch.
        // For absolute focusers, this controls the directoin of FocusRelPos when updated.
        // For DC speed based focusers, this moves the focuser continues in the CW/CCW directions until stopped.
        INDI::PropertySwitch FocusMotionSP {2};

        // Timer for user with DC focusers to run focuser in specific direction for this duration
        INDI::PropertyNumber FocusTimerNP {1};

        // Absolute Focuser Position in steps
        INDI::PropertyNumber FocusAbsPosNP {1};

        // Relative Focuser position to be commanded
        INDI::PropertyNumber FocusRelPosNP {1};

        // Absolute Focuser position is 0 to this maximum limit. By Default, it is set to 200,000.
        INDI::PropertyNumber FocusMaxPosNP {1};

        // Sync
        INDI::PropertyNumber FocusSyncNP {1};

        // Abort Focuser
        INDI::PropertySwitch FocusAbortSP {1};

        // Reverse Focuser
        INDI::PropertySwitch FocusReverseSP {2};

        // Backlash toggle
        INDI::PropertySwitch FocusBacklashSP {2};

        // Backlash steps
        INDI::PropertyNumber FocusBacklashNP {1};

        uint32_t capability;

        double lastTimerValue = { 0 };

        DefaultDevice * m_defaultDevice { nullptr };
};
}
