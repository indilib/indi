/*
    Output Interface
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
#include "indipropertynumber.h"
#include "inditimer.h"

/**
 * \class OutputInterface
   \brief Provides interface to implement Digital Boolean Output (On/Off) functionality.

   Example implemenations are web-enabled Outputs and GPIOs.

   A web controlled Output is a simple device that can open, close, or flip a Output switch.

   \e IMPORTANT: initFilterProperties() must be called before any other function to initialize the filter properties.

\author Jasem Mutlaq
*/
namespace INDI
{

class OutputInterface
{
    public:
        /*! Digital Output status.
         */
        typedef enum
        {
            Off,     /*!< Output is off. For Relays, it is open circuit. */
            On,      /*!< Output is on. For Relays, it is closed circuit. */
        } OutputState;

        /**
         * \brief Update all digital outputs
         * \return True if operation is successful, false otherwise
         * \note UpdateDigitalOutputs should either be called periodically in the child's TimerHit or custom timer function or when an
         * interrupt or trigger warrants updating the digital outputs. Only updated properties that had a change in status since the last
         * time this function was called should be sent to the clients to reduce unnecessary updates.
         * Polling or Event driven implemetation depends on the concrete class hardware capabilities.
         */
        virtual bool UpdateDigitalOutputs() = 0;

        /**
         * \brief Send command to output
         * \return True if operation is successful, false otherwise
         */
        virtual bool CommandOutput(uint32_t index, OutputState command) = 0;

    protected:
        /**
         * @brief OutputInterface Initiailize Output Interface
         * @param defaultDevice default device that owns the interface
         */
        explicit OutputInterface(DefaultDevice *defaultDevice);
        ~OutputInterface();

        /**
         * \brief Initialize filter wheel properties. It is recommended to call this function within
         * initProperties() of your primary device
         * \param groupName Group or tab name to be used to define filter wheel properties.
         * \param outputs Number of Outputs
         * \param prefix Prefix used to label outputs. By default, Output #1, Output #2..etc
         */
        void initProperties(const char *groupName, uint8_t outputs, const std::string &prefix = "Output");

        /**
         * @brief updateProperties Defines or Delete properties based on default device connection status
         * @return True if all is OK, false otherwise.
         */
        bool updateProperties();

        /** \brief Process switch properties */
        bool processSwitch(const char *dev, const char *name, ISState states[], char *names[], int n);

        /** \brief Process text properties */
        bool processText(const char *dev, const char *name, char *texts[], char *names[], int n);

        /** \brief Process number properties */
        bool processNumber(const char *dev, const char *name, double values[], char *names[], int n);

        /**
         * @brief saveConfigItems save Filter Names in config file
         * @param fp pointer to config file
         * @return Always return true
         */
        bool saveConfigItems(FILE *fp);

        // Output Toggle
        std::vector<INDI::PropertySwitch> DigitalOutputsSP;
        // Output Labels
        INDI::PropertyText DigitalOutputLabelsTP {0};
        // Pulse Duration
        std::vector<INDI::PropertyNumber> PulseDurationNP;

        // Indicates whether we loaded the labels from configuration file successfully.
        // If loaded from config file, then we do not need to overwrite.
        bool m_DigitalOutputLabelsConfig {false};

        DefaultDevice *m_defaultDevice { nullptr };
};
}
