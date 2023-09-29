/*
    GPS Interface
    Copyright (C) 2023 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include "inditimer.h"

#include <stdint.h>

// Alias
using GI = INDI::GPSInterface;

namespace INDI
{

/**
 * \class GPSInterface
   \brief Provides interface to implement GPS functionality.

\author Jasem Mutlaq
*/
class GPSInterface
{
    public:
        enum GPSLocation
        {
            LOCATION_LATITUDE,
            LOCATION_LONGITUDE,
            LOCATION_ELEVATION
        };

    protected:
        explicit GPSInterface(DefaultDevice * defaultDevice);
        virtual ~GPSInterface() = default;

        /**
         * \brief Initilize focuser properties. It is recommended to call this function within
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
         * @brief saveConfigItems save focuser properties defined in the interface in config file
         * @param fp pointer to config file
         * @return Always return true
         */
        virtual bool saveConfigItems(FILE * fp);

        /**
         * @brief checkGPSState Check and update GPS state
         */
        void checkGPSState();

        /**
         * @brief SetSystemTime Update system-wide time
         * @param raw_time raw time
         * @return true if successful, false other.
         * @note Process user must have permission to set time.
         */
        bool setSystemTime(time_t &raw_time);

        /**
             * @brief updateGPS Retrieve Location & Time from GPS. Update LocationNP & TimeTP properties (value and state) without sending them to the client (i.e. IDSetXXX).
             * @return Return overall state. The state should be IPS_OK if data is valid. IPS_BUSY if GPS fix is in progress. IPS_ALERT is there is an error. The clients will only accept values with IPS_OK state.
             */
        virtual IPState updateGPS();


        //  A number vector that stores latitude, longitude and altitude.
        INDI::PropertyNumber LocationNP {3};

        // UTC and UTC Offset
        INDI::PropertyText TimeTP {2};

        // Refresh data
        INDI::PropertySwitch RefreshSP {1};

        // Refresh Period
        INDI::PropertyNumber PeriodNP {1};

        // System Time
        INDI::PropertySwitch SystemTimeUpdateSP {3};
        enum
        {
            UPDATE_NEVER,
            UPDATE_ON_STARTUP,
            UPDATE_ON_REFRESH
        };

        INDI::Timer m_UpdateTimer;

        // # of seconds since 1970 UTC
        std::time_t m_GPSTime;

        // Track whether system time was updated
        bool m_SystemTimeUpdated {false};

        DefaultDevice * m_DefaultDevice { nullptr };
};
}
