/*
    Relay Interface
    Copyright (C) 2024 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include <vector>
#include <stdint.h>
#include "indipropertyswitch.h"
#include "indipropertytext.h"

/**
 * \class RelayInterface
   \brief Provides interface to implement Remote Relay functionality.

   A web controlled relay is a simple device that can open, close, or flip a relay switch.

   \e IMPORTANT: initFilterProperties() must be called before any other function to initialize the filter properties.

\author Jasem Mutlaq
*/
namespace INDI
{

class RelayInterface
{
    public:
        /*! Relay switch status. This is regardless on whether switch is normally closed or normally opened. */
        typedef enum
        {
            Opened,     /*!< Switch is open circuit. */
            Closed,     /*!< Switch is close circuit. */
            Unknown     /*!< Could not determined switch status. */
        } Status;

        /*! Relay switch Command. */
        typedef enum
        {
            Open,
            Close,
            Flip
        } Command;

        /**
         * \brief Query single relay status
         * \param index Relay index
         * \param status Store relay status in this variable.
         * \return True if operation is successful, false otherwise
         */
        virtual bool QueryRelay(uint32_t index, Status &status) = 0;

        /**
         * \brief Send command to relay
         * \return True if operation is successful, false otherwise
         */
        virtual bool CommandRelay(uint32_t index, Command command) = 0;

    protected:
        /**
         * @brief RelayInterface Initiailize Relay Interface
         * @param defaultDevice default device that owns the interface
         */
        explicit RelayInterface(DefaultDevice *defaultDevice);
        ~RelayInterface();

        /**
         * \brief Initialize filter wheel properties. It is recommended to call this function within
         * initProperties() of your primary device
         * \param groupName Group or tab name to be used to define filter wheel properties.
         * \param relays Number of relays
         */
        void initProperties(const char *groupName, uint8_t relays);

        /**
         * @brief updateProperties Defines or Delete properties based on default device connection status
         * @return True if all is OK, false otherwise.
         */
        bool updateProperties();

        /** \brief Process switch properties */
        bool processSwitch(const char *dev, const char *name, ISState states[], char *names[], int n);

        /** \brief Process text properties */
        bool processText(const char *dev, const char *name, char *texts[], char *names[], int n);

        /**
         * @brief saveConfigItems save Filter Names in config file
         * @param fp pointer to config file
         * @return Always return true
         */
        bool saveConfigItems(FILE *fp);

        // Relay Toggle
        std::vector<INDI::PropertySwitch> RelaysSP;
        // Relay Labels
        INDI::PropertyText RelayLabelsTP {0};

        DefaultDevice *m_defaultDevice { nullptr };
};
}
