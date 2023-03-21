/*
    Dust Cap Interface
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "defaultdevice.h"
#include "abstractinterface.h"
#include "indipropertyswitch.h"

/**
 * \class DustCapInterface
   \brief Provides interface to implement remotely controlled dust cover

   \e IMPORTANT: initDustCapProperties() must be called before any other function to initilize the Dust Cap properties.

   \e IMPORTANT: processDustCapSwitch() must be called in your driver ISNewSwitch function.
\author Jasem Mutlaq
*/
namespace INDI
{

class DustCapInterface : public AbstractInterface
{
    public:
        enum
        {
            CAP_PARK,
            CAP_UNPARK
        };

    protected:
        DustCapInterface(DefaultDevice *device);
        virtual ~DustCapInterface() = default;

        /** \brief Initilize dust cap properties. It is recommended to call this function within initProperties() of your primary device
                \param deviceName Name of the primary device
                \param groupName Group or tab name to be used to define focuser properties.
            */
        void initProperties(const char *groupName) override;

        /** \brief Child class should call updateDustCapProperties in the child updateProperties function to
         * define or delete properties when connection status changes.
        */
        bool updateProperties() override;

        /** \brief Process dust cap switch properties */
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        /**
             * @brief Park dust cap (close cover). Must be implemented by child.
             * @return If command completed immediatly, return IPS_OK. If command is in progress, return IPS_BUSY. If there is an error, return IPS_ALERT
             */
        virtual IPState ParkCap();

        /**
             * @brief unPark dust cap (open cover). Must be implemented by child.
             * @return If command completed immediatly, return IPS_OK. If command is in progress, return IPS_BUSY. If there is an error, return IPS_ALERT
             */
        virtual IPState UnParkCap();

        // Open/Close cover
        INDI::PropertySwitch ParkCapSP {2};
};
}
