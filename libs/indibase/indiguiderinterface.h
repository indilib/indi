/*
    Guider Interface
    Copyright (C) 2011-2024 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

/**
 * @class GuiderInterface
 * @brief Provides interface to implement guider (ST4) port functionality.
 *
 * The child class implements GuideXXXX() functions and returns:
 * IPS_OK if the guide operation is completed in the function, which is usually appropriate for
 * very short guiding pulses.
 * IPS_BUSY if the guide operation is in progress and will take time to complete. In this
 * case, the child class must call GuideComplete() once the guiding pulse is complete.
 * IPS_ALERT if the guide operation failed.
 *
 * \e IMPORTANT: initProperties() must be called before any other function to initialize
 * the guider properties.
 * \e IMPORATNT: processNumber() must be called in your driver's ISNewNumber(..)
 * function. processNumber() will call the guide functions
 * GuideXXXX functions according to the driver. If there are no guide functions to process, the function will return
 * false so you can continue processing the properties down the chain.
 *
 * @author Jasem Mutlaq
 */

#include <stdint.h>
#include "defaultdevice.h"

// Alias
using GI = INDI::GuiderInterface;

namespace INDI
{

class GuiderInterface
{
    public:
        /**
         * @brief Guide north for ms milliseconds. North is defined as DEC+
         * @return IPS_OK if operation is completed successfully, IPS_BUSY if operation will take take to
         * complete, or IPS_ALERT if operation failed.
         */
        virtual IPState GuideNorth(uint32_t ms) = 0;

        /**
         * @brief Guide south for ms milliseconds. South is defined as DEC-
         * @return IPS_OK if operation is completed successfully, IPS_BUSY if operation will take take to
         * complete, or IPS_ALERT if operation failed.
         */
        virtual IPState GuideSouth(uint32_t ms) = 0;

        /**
         * @brief Guide east for ms milliseconds. East is defined as RA+
         * @return IPS_OK if operation is completed successfully, IPS_BUSY if operation will take take to
         * complete, or IPS_ALERT if operation failed.
         */
        virtual IPState GuideEast(uint32_t ms) = 0;

        /**
         * @brief Guide west for ms milliseconds. West is defined as RA-
         * @return IPS_OK if operation is completed successfully, IPS_BUSY if operation will take take to
         * complete, or IPS_ALERT if operation failed.
         */
        virtual IPState GuideWest(uint32_t ms) = 0;

        /**
         * @brief Call GuideComplete once the guiding pulse is complete.
         * @param axis Axis of completed guiding operation.
         */
        virtual void GuideComplete(INDI_EQ_AXIS axis);

    protected:
        GuiderInterface(DefaultDevice *defaultDevice);
        ~GuiderInterface();

        /**
         * @brief Initialize guider properties. It is recommended to call this function within
         * initProperties() of your primary device
         * @param groupName Group or tab name to be used to define guider properties.
         */
        void initProperties(const char *groupName);

        /**
         * @brief updateProperties Define or Delete Rotator properties based on the connection status of the base device
         * @return True if successful, false otherwise.
         */
        bool updateProperties();

        /**
         * @brief Call this function whenever client updates GuideNSNP or GuideWSP properties in the
         * primary device. This function then takes care of issuing the corresponding GuideXXXX
         * function accordingly.
         * @param dev device name
         * @param name property name
         * @param values value as passed by the client
         * @param names names as passed by the client
         * @param n number of values and names pair to process.
         * @return True if the Guide properties are processed, false otherwise.
         */
        bool processNumber(const char *dev, const char *name, double values[], char *names[], int n);

        INDI::PropertyNumber GuideNSNP {2};
        INDI::PropertyNumber GuideWENP {2};

    private:
        DefaultDevice *m_defaultDevice { nullptr };
};
}
