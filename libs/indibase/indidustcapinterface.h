/*
    Dust Cap Interface
    Copyright (C) 2015-2025 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include <cstdint>

/**
 * \class DustCapInterface
   \brief Provides interface to implement remotely controlled dust cover

   \e IMPORTANT: initProperties() must be called before any other function to initialize the Dust Cap properties.
   \e IMPORTANT: updateProperties() must be called in your driver updateProperties function.
   \e IMPORTANT: processSwitch() must be called in your driver ISNewSwitch function.
\author Jasem Mutlaq
*/

// Alias
using DI = INDI::DustCapInterface;
namespace INDI
{

class DustCapInterface
{
    public:
        enum
        {
            CAP_PARK,
            CAP_UNPARK
        };

        enum
        {
            CAN_ABORT           = 1 << 0,   /** Can the dust cap abort motion? */
            CAN_SET_POSITION    = 1 << 1,   /** Can the dust go to a specific angular position? UNUSED */
            CAN_SET_LIMITS      = 1 << 2,   /** Can the dust set the minimum and maximum ranges for Park and Unpark? UNUSED */
        } DustCapCapability;

    protected:
        DustCapInterface(DefaultDevice *device);
        virtual ~DustCapInterface() = default;

        /**
             * @brief Park dust cap (close cover). Must be implemented by child.
             * @return If command completed immediately, return IPS_OK. If command is in progress, return IPS_BUSY. If there is an error, return IPS_ALERT
             */
        virtual IPState ParkCap();

        /**
             * @brief unPark dust cap (open cover). Must be implemented by child.
             * @return If command completed immediately, return IPS_OK. If command is in progress, return IPS_BUSY. If there is an error, return IPS_ALERT
             */
        virtual IPState UnParkCap();

        /**
             * @brief Abort motion. Must be implemented by child.
             * @return If command completed immediately, return IPS_OK. If command is in progress, return IPS_BUSY. If there is an error, return IPS_ALERT
             */
        virtual IPState AbortCap();

        /** \brief Initialize dust cap properties. It is recommended to call this function within initProperties() of your primary device
                \param group Group or tab name to be used to define properties.
                \param capabilities Any additional capabilities the dust cap supports besides Parking and Unparking.
            */
        void initProperties(const char *group, uint32_t capabilities = 0);

        /**
         * @brief updateProperties Defines or Delete properties based on default device connection status
         * @return True if all is OK, false otherwise.
         */
        bool updateProperties();

        /** \brief Process dust cap switch properties */
        bool processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        // Open/Close cover
        INDI::PropertySwitch ParkCapSP {2};
        // Abort Motion
        INDI::PropertySwitch AbortCapSP {1};

    private:
        DefaultDevice *m_DefaultDevice { nullptr };
        uint32_t m_Capabilities {0};
};
}
