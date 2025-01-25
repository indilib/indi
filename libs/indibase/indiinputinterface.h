/*
    Input Interface
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
 * \class InputInterface
   \brief Provides interface to implement Digital/Analog input functionality.

   Example implemenations are web-enabled observatory controllers and GPIOs.

   A typical observatory controll usually support both InputInterface and OutputInterface

   \e IMPORTANT: initFilterProperties() must be called before any other function to initialize the filter properties.

\author Jasem Mutlaq
*/
namespace INDI
{

class InputInterface
{
    public:
        /*! Input boolean status. This is regardless on whether Input is Active low or high.
        For relay, Off = Open Circuit. On = Closed Circuit
        */
        typedef enum
        {
            Off,     /*!< Input is off. */
            On,      /*!< Input is on. */
        } InputState;

        /**
         * \brief Update all digital inputs
         * \return True if operation is successful, false otherwise
         */
        virtual bool UpdateDigitalInputs() = 0;

        /**
         * \brief Update all analog inputs
         * \return True if operation is successful, false otherwise
         */
        virtual bool UpdateAnalogInputs() = 0;

    protected:
        /**
         * @brief InputInterface Initiailize Input Interface
         * @param defaultDevice default device that owns the interface
         */
        explicit InputInterface(DefaultDevice *defaultDevice);
        ~InputInterface();

        /**
         * \brief Initialize filter wheel properties. It is recommended to call this function within
         * initProperties() of your primary device
         * \param groupName Group or tab name to be used to define filter wheel properties.
         * \param digital Number of digital Inputs
         * \param analog Number of analog Inputs
         * \param digitalPrefix Prefix used to label digital Inputs. By default, Digital #1, Digital #2..etc
         * \param analogPrefix Prefix used to label analog Inputs. By default, Analog #1, Analog #2..etc
         */
        void initProperties(const char *groupName, uint8_t digital, uint8_t analog, const std::string &digitalPrefix = "Digital",
                            const std::string &analogPrefix = "Analog");

        /**
         * @brief updateProperties Defines or Delete properties based on default device connection status
         * @return True if all is OK, false otherwise.
         */
        bool updateProperties();

        /** \brief Process text properties */
        bool processText(const char *dev, const char *name, char *texts[], char *names[], int n);

        /**
         * @brief saveConfigItems save Filter Names in config file
         * @param fp pointer to config file
         * @return Always return true
         */
        bool saveConfigItems(FILE *fp);

        // Digital Input
        std::vector<INDI::PropertySwitch> DigitalInputsSP;
        // Analog Input
        std::vector<INDI::PropertyNumber> AnalogInputsNP;
        // Digital Input Labels
        INDI::PropertyText DigitalInputLabelsTP {0};
        // Analog Input Labels
        INDI::PropertyText AnalogInputLabelsTP {0};

        // Indicates whether we loaded the labels from configuration file successfully.
        // If loaded from config file, then we do not need to overwrite.
        bool m_DigitalInputLabelsConfig {false}, m_AnalogInputLabelsConfig {false};

        DefaultDevice *m_defaultDevice { nullptr };
};
}
