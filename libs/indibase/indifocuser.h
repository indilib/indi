/*******************************************************************************
  Copyright(c) 2013 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "defaultdevice.h"
#include "indifocuserinterface.h"

namespace Connection
{
class Serial;
class TCP;
}
/**
 * \class Focuser
   \brief Class to provide general functionality of a focuser device.

   Both relative and absolute focuser supported. Furthermore, if no position feedback is available from the focuser,
   an open-loop control is possible using timers, speed presets, and direction of motion.
   Developers need to subclass Focuser to implement any driver for focusers within INDI.

\author Jasem Mutlaq
\author Gerry Rozema
*/
namespace INDI
{

class Focuser : public DefaultDevice, public FocuserInterface
{
    public:
        /**
         * @brief Construct a new Focuser object.
         * Initializes the controller and sets up the FocuserInterface.
         */
        Focuser();
        /**
         * @brief Destroy the Focuser object.
         * Cleans up the controller resource.
         */
        virtual ~Focuser();

        /** \struct FocuserConnection
                \brief Holds the connection mode of the Focuser.
            */
        enum
        {
            CONNECTION_NONE   = 1 << 0, /** Do not use any connection plugin */
            CONNECTION_SERIAL = 1 << 1, /** For regular serial and bluetooth connections */
            CONNECTION_TCP    = 1 << 2  /** For Wired and WiFI connections */
        } FocuserConnection;

        /**
         * @brief Initializes the default properties for the focuser device.
         * Sets up standard INDI properties, presets, controller mappings, and connection types.
         * Overrides DefaultDevice::initProperties.
         * @return true if initialization is successful, false otherwise.
         */
        virtual bool initProperties() override;
        /**
         * @brief Handles client requests for property definitions for this device or all devices.
         * Delegates to the base class and the internal controller.
         * Overrides DefaultDevice::ISGetProperties.
         * @param dev The device name, or nullptr to request properties for all devices.
         */
        virtual void ISGetProperties(const char *dev) override;
        /**
         * @brief Updates the state and definition of properties based on the current connection status.
         * For example, preset properties are only defined if the device is connected and supports absolute moves.
         * Overrides DefaultDevice::updateProperties.
         * @return true always.
         */
        virtual bool updateProperties() override;
        /**
         * @brief Processes incoming new number values from the client for this device.
         * Handles updates to FocuserInterface properties and preset values.
         * Overrides DefaultDevice::ISNewNumber.
         * @param dev The target device name.
         * @param name The property name.
         * @param values Array of new numerical values.
         * @param names Array of element names corresponding to the values.
         * @param n The number of elements being updated.
         * @return true if the number property was successfully processed, false otherwise.
         */
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        /**
         * @brief Processes incoming new switch states from the client for this device.
         * Handles updates to FocuserInterface properties and preset goto commands.
         * Overrides DefaultDevice::ISNewSwitch.
         * @param dev The target device name.
         * @param name The property name.
         * @param states Array of new switch states (ISS_ON or ISS_OFF).
         * @param names Array of element names corresponding to the states.
         * @param n The number of elements being updated.
         * @return true if the switch property was successfully processed, false otherwise.
         */
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        /**
         * @brief Processes incoming new text values from the client for this device.
         * Delegates processing to the controller and the base class.
         * Overrides DefaultDevice::ISNewText.
         * @param dev The target device name.
         * @param name The property name.
         * @param texts Array of new text values.
         * @param names Array of element names corresponding to the texts.
         * @param n The number of elements being updated.
         * @return true if the text property was handled by the base class, false otherwise.
         */
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        /**
         * @brief Processes properties snooped from other devices.
         * Delegates processing to the controller and the base class.
         * Overrides DefaultDevice::ISSnoopDevice.
         * @param root The XML element representing the snooped property definition or update.
         * @return true if the snooped property was handled by the base class, false otherwise.
         */
        virtual bool ISSnoopDevice(XMLEle *root) override;

        /**
             * @brief setConnection Set Focuser connection mode. Child class should call this in the constructor before Focuser registers
             * any connection interfaces
         * @param value A bitmask (ORed combination) of FocuserConnection enum values (e.g., CONNECTION_SERIAL | CONNECTION_TCP).
         */
        void setSupportedConnections(const uint8_t &value);

        /**
             * @brief Get the currently configured supported connection modes.
             * @return A bitmask (ORed combination) of FocuserConnection enum values.
             */
        uint8_t getSupportedConnections() const
        {
            return focuserConnection;
        }

        /**
         * @brief Static callback function used by the Controller class.
         * Forwards button press events to the appropriate Focuser instance's processButton method.
         * @param button_n The name of the button element that changed state.
         * @param state The new state of the button (ISS_ON or ISS_OFF).
         * @param context A pointer to the Focuser instance associated with the controller.
         */
        static void buttonHelper(const char *button_n, ISState state, void *context);

    protected:
        /**
             * @brief saveConfigItems Saves the Device Port and Focuser Presets in the configuration file
             * @param fp pointer to configuration file
         * @return true always.
         */
        virtual bool saveConfigItems(FILE *fp) override;

        /**
         * @brief Performs a device-specific handshake after a connection is established.
         * This method should be overridden by concrete focuser drivers to implement
         * any necessary communication checks or initialization sequences with the hardware.
         * The base implementation does nothing and returns false.
         * @return true if the handshake is successful, false otherwise.
         */
        virtual bool Handshake();

        /**
         * @brief SetFocuserMaxPosition Update focuser maximum position. It only updates the PresetNP property limits.
         * @param ticks The new maximum position in focuser ticks.
         * @return true always.
         */
        virtual bool SetFocuserMaxPosition(uint32_t ticks) override;

        /**
         * @brief Synchronizes the range (min, max, step) of the preset number properties (PresetNP)
         * based on the focuser's maximum absolute position.
         * @param ticks The maximum absolute position in focuser ticks.
         */
        virtual void SyncPresets(uint32_t ticks);

        INDI::PropertyNumber PresetNP {3}; /**< INDI Property for storing focus position presets. */
        INDI::PropertySwitch PresetGotoSP {3}; /**< INDI Property for commanding a move to a stored preset position. */

        /**
         * @brief Processes button press events received from the controller via buttonHelper.
         * Handles actions like "Focus In", "Focus Out", and "Abort Focus".
         * @param button_n The name of the button element that was pressed.
         * @param state The state of the button (typically ISS_ON for presses).
         */
        void processButton(const char *button_n, ISState state);

        Controller *controller;

        Connection::Serial *serialConnection = nullptr;
        Connection::TCP *tcpConnection       = nullptr;

        int PortFD = -1;

    private:
        bool callHandshake();
        uint8_t focuserConnection = CONNECTION_SERIAL | CONNECTION_TCP;
};
}
