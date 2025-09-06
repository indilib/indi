/*******************************************************************************
  Copyright(c) 2011 Jasem Mutlaq. All rights reserved.

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

#include "parentdevice.h"
#include "indidriver.h"
#include "indilogger.h"

#include <stdint.h>
#include <any>
#include <string>
#include <functional>

namespace Connection
{
class Interface;
class Serial;
class TCP;
class I2C;
}
/**
 * @brief COMMUNICATION_TAB Where all the properties required to connect/disconnect from
 * a device are located. Usually such properties may include port number, IP address, or
 * any property necessarily to establish a connection to the device.
 */
extern const char *COMMUNICATION_TAB;

/**
 * @brief MAIN_CONTROL_TAB Where all the primary controls for the device are located.
 */
extern const char *MAIN_CONTROL_TAB;

/**
 * @brief CONNECTION_TAB Where all device connection settings (serial, usb, ethernet) are defined and controlled.
 */
extern const char *CONNECTION_TAB;

/**
 * @brief MOTION_TAB Where all the motion control properties of the device are located.
 */
extern const char *MOTION_TAB;

/**
 * @brief DATETIME_TAB Where all date and time setting properties are located.
 */
extern const char *DATETIME_TAB;

/**
 * @brief SITE_TAB Where all site information setting are located.
 */
extern const char *SITE_TAB;

/**
 * @brief OPTIONS_TAB Where all the driver's options are located. Those may include auxiliary controls, driver
 * metadata, version information..etc.
 */
extern const char *OPTIONS_TAB;

/**
 * @brief FILTER_TAB Where all the properties for filter wheels are located.
 */
extern const char *FILTER_TAB;

/**
 * @brief FOCUS_TAB Where all the properties for focuser are located.
 */
extern const char *FOCUS_TAB;

/**
 * @brief GUIDE_TAB Where all the properties for guiding are located.
 */
extern const char *GUIDE_TAB;

/**
 * @brief ALIGNMENT_TAB Where all the properties for guiding are located.
 */
extern const char *ALIGNMENT_TAB;

/**
 * @brief SATELLITE_TAB
 */
extern const char *SATELLITE_TAB;

/**
 * @brief INFO_TAB Where all the properties for general information are located.
 */
extern const char *INFO_TAB;

/**
 * \class INDI::DefaultDevice
 * \brief Class to provide extended functionality for devices in addition
 * to the functionality provided by INDI::BaseDevice. This class should \e only be subclassed by
 * drivers directly as it is linked with main(). Virtual drivers cannot employ INDI::DefaultDevice.
 *
 * INDI::DefaultDevice provides capability to add Debug, Simulation, and Configuration controls.
 * These controls (switches) are defined to the client. Configuration options permit saving and
 * loading of AS-IS property values.
 *
 * \see <a href='tutorial__four_8h_source.html'>Tutorial Four</a>
 * \author Jasem Mutlaq
 */
namespace INDI
{

class DefaultDevicePrivate;
class DefaultDevice : public ParentDevice
{
        DECLARE_PRIVATE(DefaultDevice)

    public:
        DefaultDevice();
        virtual ~DefaultDevice() override = default;

    public:
        /** \brief Add Debug, Simulation, and Configuration options to the driver */
        void addAuxControls();

        /** \brief Add Debug control to the driver */
        void addDebugControl();

        /** \brief Add Simulation control to the driver */
        void addSimulationControl();

        /** \brief Add Configuration control to the driver */
        void addConfigurationControl();

        /** \brief Add Polling period control to the driver */
        void addPollPeriodControl();

    public:
        /** \brief Set all properties to IDLE state */
        void resetProperties();

        /**
         * \brief Define number vector to client & register it. Alternatively, IDDefNumber can
         * be used but the property will not get registered and the driver will not be able to
         * save configuration files.
         * \param nvp The number vector property to be defined
         */
        INDI_DEPRECATED("Use defineProperty(INDI::Property &).")
        void defineNumber(INumberVectorProperty *nvp);
        void defineProperty(INumberVectorProperty *property);

        /**
         * \brief Define text vector to client & register it. Alternatively, IDDefText can be
         * used but the property will not get registered and the driver will not be able to save
         * configuration files.
         * \param tvp The text vector property to be defined
         */
        INDI_DEPRECATED("Use defineProperty(INDI::Property &).")
        void defineText(ITextVectorProperty *tvp);
        void defineProperty(ITextVectorProperty *property);

        /**
         * \brief Define switch vector to client & register it. Alternatively, IDDefswitch can be
         * used but the property will not get registered and the driver will not be able to save
         * configuration files.
         * \param svp The switch vector property to be defined
         */
        INDI_DEPRECATED("Use defineProperty(INDI::Property &).")
        void defineSwitch(ISwitchVectorProperty *svp);
        void defineProperty(ISwitchVectorProperty *property);

        /**
         * \brief Define light vector to client & register it. Alternatively, IDDeflight can be
         * used but the property will not get registered and the driver will not be able to save
         * configuration files.
         * \param lvp The light vector property to be defined
         */
        INDI_DEPRECATED("Use defineProperty(INDI::Property &).")
        void defineLight(ILightVectorProperty *lvp);
        void defineProperty(ILightVectorProperty *property);

        /**
         * \brief Define BLOB vector to client & register it. Alternatively, IDDefBLOB can be
         * used but the property will not get registered and the driver will not be able to
         * save configuration files.
         * \param bvp The BLOB vector property to be defined
         */
        INDI_DEPRECATED("Use defineProperty(INDI::Property &).")
        void defineBLOB(IBLOBVectorProperty *bvp);
        void defineProperty(IBLOBVectorProperty *property);

        void defineProperty(INDI::Property &property);
        /**
         * \brief Delete a property and unregister it. It will also be deleted from all clients.
         * \param propertyName name of property to be deleted.
         */
        virtual bool deleteProperty(const char *propertyName);

        /**
         * @brief deleteProperty Delete a property and unregister it. It will also be deleted from all clients.
         * @param property Property to be deleted.
         * @return True if successful, false otherwise.
         * @note This is a convenience function that internally calls deleteProperty with the property name.
         */
        bool deleteProperty(INDI::Property &property);

    public:
        /**
         * \brief Set connection switch status in the client.
         * \param status True to toggle CONNECT on, false to toggle DISCONNECT on.
         * \param state State of CONNECTION properti, by default IPS_OK.
         * \param msg A message to be sent along with connect/disconnect command, by default nullptr.
         */
        virtual void setConnected(bool status, IPState state = IPS_OK, const char *msg = nullptr);

        /**
         * \brief Set a timer to call the function TimerHit after ms milliseconds
         * \param ms timer duration in milliseconds.
         * \return id of the timer to be used with RemoveTimer
        */
        int SetTimer(uint32_t ms);

        /**
         * \brief Remove timer added with SetTimer
         * \param id ID of the timer as returned from SetTimer
         */
        void RemoveTimer(int id);

        /** \brief Callback function to be called once SetTimer duration elapses. */
        virtual void TimerHit();

        /** \return driver executable filename */
        virtual const char *getDriverExec();

        /** \return driver name */
        virtual const char *getDriverName();

        /**
         * \brief Set driver version information to be defined in DRIVER_INFO property as vMajor.vMinor
         * \param vMajor major revision number
         * \param vMinor minor revision number
         */
        void setVersion(uint16_t vMajor, uint16_t vMinor);

        /** \return Major driver version number. */
        uint16_t getMajorVersion() const;

        /** \return Minor driver version number. */
        uint16_t getMinorVersion() const;

    public:
        /**
         * \brief define the driver's properties to the client.
         * Usually, only a minimum set of properties are defined to the client in this function
         * if the device is in disconnected state. Those properties should be enough to enable the
         * client to establish a connection to the device. In addition to CONNECT/DISCONNECT, such
         * properties may include port name, IP address, etc. You should check if the device is
         * already connected, and if this is true, then you must define the remainder of the
         * the properties to the client in this function. Otherwise, the remainder of the driver's
         * properties are defined to the client in updateProperties() function which is called when
         * a client connects/disconnects from a device.
         * \param dev name of the device
         * \note This function is called by the INDI framework, do not call it directly. See LX200
         * Generic driver for an example implementation
         */
        virtual void ISGetProperties(const char *dev);

        /**
         * \brief Process the client newSwitch command
         * \note This function is called by the INDI framework, do not call it directly.
         * \returns True if any property was successfully processed, false otherwise.
         */
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        /**
         * \brief Process the client newNumber command
         * \note This function is called by the INDI framework, do not call it directly.
         * \returns True if any property was successfully processed, false otherwise.
         */
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

        /**
         * \brief Process the client newSwitch command
         * \note This function is called by the INDI framework, do not call it directly.
         * \returns True if any property was successfully processed, false otherwise.
         */
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);

        /**
         * \brief Process the client newBLOB command
         * \note This function is called by the INDI framework, do not call it directly.
         * \returns True if any property was successfully processed, false otherwise.
         */
        virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                               char *formats[], char *names[], int n);

        /**
         * \brief Process a snoop event from INDI server. This function is called when a snooped property is
         * updated in a snooped driver.
         * \note This function is called by the INDI framework, do not call it directly.
         * \returns True if any property was successfully processed, false otherwise.
         */
        virtual bool ISSnoopDevice(XMLEle *root);

        /**
         * @brief Generic convenience function to update a property element as if by client request,
         *        simulating an ISNew... call.
         *
         * This function determines the type of the property (Switch, Number, Text) and
         * attempts to cast the std::any value to the appropriate type before calling
         * the corresponding ISNew... function.
         *
         * @param property Reference to the INDI::Property object to be updated.
         * @param elementName The name of the element within the property to update.
         * @param value The new value for the element, wrapped in std::any.
         *              - For Switch properties: std::any should contain an ISState.
         *              - For Number properties: std::any should contain a double or int.
         *              - For Text properties: std::any should contain a const char* or std::string.
         * @return True if the update was successfully dispatched via the appropriate ISNew...
         *         function, false otherwise (e.g., type mismatch, element not found,
         *         property not found).
         */
        bool ISNewProperty(INDI::Property &property, const std::string &elementName, const std::any &value);

        /**
         * @brief Generic helper function to update an INDI property based on an external operation.
         *
         * This function encapsulates the common pattern of:
         * 1. Checking if a property's values have actually changed.
         * 2. Executing an external update function (e.g., communicating with hardware).
         * 3. Updating the INDI property's internal state and applying changes if the external update was successful.
         * 4. Optionally saving the configuration.
         *
         * @tparam PropertyType The type of the INDI property (e.g., INDI::PropertyNumber, INDI::PropertyText, INDI::PropertySwitch).
         * @tparam ValueType The type of the array elements (e.g., double, char*, ISState).
         * @param property The INDI property to update.
         * @param values The array of new values for the property elements.
         * @param names The array of names corresponding to the values.
         * @param n The number of elements in the values and names arrays.
         * @param updater A std::function that performs the actual device update. It should return true on success, false on failure.
         * @param saveConfig If true, save the property's configuration after a successful update. Defaults to false.
         * @return True if the property was updated and the external operation was successful, false otherwise.
         */
        template<typename PropertyType, typename ValueType>
        bool updateProperty(PropertyType& property, ValueType* values, char* names[], int n,
                            std::function<bool()> updater, bool saveConfig = false)
        {
            if (property.isUpdated(values, names, n))
            {
                if (updater())
                {
                    property.update(values, names, n);
                    property.setState(IPS_OK);
                    if (saveConfig)
                        this->saveConfig(property);
                    property.apply();
                    return true;
                }
                else
                {
                    property.setState(IPS_ALERT);
                    property.apply();
                    return false;
                }
            }
            else
            {
                // If nothing is updated, just accept as-is
                property.setState(IPS_OK);
                property.apply();
                return false;
            }
        }

        /**
         * @return getInterface Return the interface declared by the driver.
         */
        uint32_t getDriverInterface() const;

        /**
         * @brief setInterface Set driver interface. By default the driver interface is set to GENERAL_DEVICE.
         * You may send an ORed list of DeviceInterface values.
         * @param value ORed list of DeviceInterface values.
         * @warning This only updates the internal driver interface property and does not send it to the
         * client. To synchronize the client, use syncDriverInfo function.
         */
        void setDriverInterface(uint32_t value);

    public:
        /** @brief Add a device to the watch list.
         *
         *  A driver may select to receive notifications of a specific other device.
         */
        void watchDevice(const char *deviceName, const std::function<void (INDI::BaseDevice)> &callback);

    protected:
        /**
         * @brief setDynamicPropertiesBehavior controls handling of dynamic properties. Dynamic properties
         * are those generated from an external skeleton XML file. By default all properties, including
         * dynamic properties, are defined to the client in ISGetProperties(). Furthermore, when
         * űdeleteProperty(properyName) is called, the dynamic property is deleted by default, and can only
         * be restored by calling buildSkeleton(filename) again. However, it is sometimes desirable to skip
         * the definition of the dynamic properties on startup and delegate this task to the child class.
         * To control this behavior, set enabled to false.
         * @param defineEnabled True to define all dynamic properties in INDI::DefaultDevice own
         * ISGetProperties() on startup. False to skip defining dynamic properties.
         * @param deleteEnabled True to delete dynamic properties from memory in deleteProperty(name).
         * False to keep dynamic property in the properties list, but delete it from the client.
         * @note This function has no effect on regular properties initialized directly by the driver.
         */
        void setDynamicPropertiesBehavior(bool defineEnabled, bool deleteEnabled);

        // Configuration

        /**
         * \brief Load the last saved configuration file
         * \param silent if true, don't report any error or notification messages.
         * \param property Name of property to load configuration for. If nullptr, all properties in the
         * configuration file are loaded which is the default behavior.
         * \return True if successful, false otherwise.
         */
        virtual bool loadConfig(bool silent = false, const char *property = nullptr);

        /**
         * @brief Load property config from the configuration file. If the property configuration is successfully parsed, the corresponding ISNewXXX
         * is called with the values parsed from the config file.
         * @param property Property to load configuration for.
         * @return True if successful, false otherwise.
         * @note This is a convenience function that calls loadConfig(true, property->getName())
         */
        bool loadConfig(INDI::Property &property);

        /**
         * \brief Save the current properties in a configuration file
         * \param silent if true, don't report any error or notification messages.
         * \param property Name of specific property to save while leaving all others properties in the
         * file as is.
         * \return True if successful, false otherwise.
         */
        virtual bool saveConfig(bool silent = false, const char *property = nullptr);

        /**
         * @brief Save a property in the configuration file
         * @param property Property to save in configuration file.
         * @return True if successful, false otherwise.
         * @note This is a convenience function that calls saveConfig(true, property->getName())
         */
        bool saveConfig(INDI::Property &property);

        /**
         * @brief purgeConfig Remove config file from disk.
         * @return True if successful, false otherwise.
         */
        virtual bool purgeConfig();

        /**
         * @brief saveConfigItems Save specific properties in the provide config file handler. Child
         * class usually override this function to save their own properties and the base class
         * saveConfigItems(fp) must be explicitly called by each child class. The Default Device
         * saveConfigItems(fp) only save Debug properties options in the config file.
         * @param fp Pointer to config file handler
         * @return True if successful, false otherwise.
         */
        virtual bool saveConfigItems(FILE *fp);

        /**
         * @brief saveAllConfigItems Save all the drivers' properties in the configuration file
         * @param fp pointer to config file handler
         * @return  True if successful, false otherwise.
         */
        virtual bool saveAllConfigItems(FILE *fp);

        /**
         * \brief Load the default configuration file
         * \return True if successful, false otherwise.
         */
        virtual bool loadDefaultConfig();

        // Simulatin & Debug

        /**
         * \brief Toggle driver debug status
         * A driver can be more verbose if Debug option is enabled by the client.
         * \param enable If true, the Debug option is set to ON.
         */
        void setDebug(bool enable);

        /**
         * \brief Toggle driver simulation status
         * A driver can run in simulation mode if Simulation option is enabled by the client.
         * \param enable If true, the Simulation option is set to ON.
         */
        void setSimulation(bool enable);

        /**
         * \brief Inform driver that the debug option was triggered.
         * This function is called after setDebug is triggered by the client. Reimplement this
         * function if your driver needs to take specific action after debug is enabled/disabled.
         * Otherwise, you can use isDebug() to check if simulation is enabled or disabled.
         * \param enable If true, the debug option is set to ON.
         */
        virtual void debugTriggered(bool enable);

        /**
         * \brief Inform driver that the simulation option was triggered.
         * This function is called after setSimulation is triggered by the client. Reimplement this
         * function if your driver needs to take specific action after simulation is enabled/disabled.
         * Otherwise, you can use isSimulation() to check if simulation is enabled or disabled.
         * \param enable If true, the simulation option is set to ON.
         */
        virtual void simulationTriggered(bool enable);

        /** \return True if Debug is on, False otherwise. */
        bool isDebug() const;

        /** \return True if Simulation is on, False otherwise. */
        bool isSimulation() const;

        /**
         * \brief Initialize properties initial state and value. The child class must implement this function.
         * \return True if initialization is successful, false otherwise.
         */
        virtual bool initProperties();

        /**
         * \brief updateProperties is called whenever there is a change in the CONNECTION status of
         * the driver. This will enable the driver to react to changes of switching ON/OFF a device.
         * For example, a driver may only define a set of properties after a device is connected, but
         * not before.
         * \return True if update is successful, false otherwise.
         */
        virtual bool updateProperties();

        /**
         * \brief Connect to the device. INDI::DefaultDevice implementation connects to appropriate
         * connection interface (Serial or TCP) governed by connectionMode. If connection is successful,
         * it proceed to call Handshake() function to ensure communication with device is successful.
         * For other communication interface, override the method in the child class implementation
         * \return True if connection is successful, false otherwise
         */
        virtual bool Connect();

        /**
         * \brief Disconnect from device
         * \return True if successful, false otherwise
         */
        virtual bool Disconnect();

        /**
         * @brief registerConnection Add new connection plugin to the existing connection pool. The
         * connection type shall be defined to the client in ISGetProperties()
         * @param newConnection Pointer to new connection plugin
         */
        void registerConnection(Connection::Interface *newConnection);

        /**
         * @brief unRegisterConnection Remove connection from existing pool
         * @param existingConnection pointer to connection interface
         * @return True if connection is removed, false otherwise.
         */
        bool unRegisterConnection(Connection::Interface *existingConnection);

        /** @return Return actively selected connection plugin */
        Connection::Interface *getActiveConnection();

        /**
         * @brief setActiveConnection Switch the active connection to the passed connection plugin
         * @param existingConnection pointer to an existing connection to be made active.
         */
        void setActiveConnection(Connection::Interface *existingConnection);

    protected: // polling
        /**
         * @brief setDefaultPollingPeriod Change the default polling period to call TimerHit() function in the driver.
         * @param msec period in milliseconds
         * @note default period is 1000ms
         */
        void setDefaultPollingPeriod(uint32_t msec);

        /**
         * @brief setPollingPeriodRange Set the range permitted by the polling range in milliseconds
         * @param minimum Minimum duration in ms.
         * @param maximum Maximum duration in ms.
         */
        void setPollingPeriodRange(uint32_t minimum, uint32_t maximum);

        /**
         * @brief getPollingPeriod Return the polling period.
         * @return Polling period in milliseconds.
         */
        uint32_t getPollingPeriod() const;

        /**
         * @brief setCurrentPollingPeriod Change the current polling period to call TimerHit() function in the driver.
         * @param msec period in milliseconds
         * @note default period is 1000ms
         */
        void setCurrentPollingPeriod(uint32_t msec);

        /**
         * @brief getCurrentPollingPeriod Return the current polling period.
         * @return Polling period in milliseconds.
         */
        uint32_t getCurrentPollingPeriod() const;

        /* direct access to POLLMS is deprecated, please use setCurrentPollingPeriod/getCurrentPollingPeriod */
        uint32_t &refCurrentPollingPeriod() __attribute__((deprecated));
        uint32_t  refCurrentPollingPeriod() const __attribute__((deprecated));
#define POLLMS refCurrentPollingPeriod()

    protected:
        /**
         * @brief isConfigLoading Check if driver configuration is currently in the process of getting loaded.
         * @return True if property loading in progress, false otherwise.
         */
        bool isConfigLoading() const;

        /**
         * @brief isInitializationComplete Check if driver initialization is complete.
         * @return True if driver is initialized. It is initialized after initProperties() is completed and
         *  after the first ISGetProperties() is executed.
         */
        bool isInitializationComplete() const;

        /** @brief syncDriverInfo sends the current driver information to the client. */
        void syncDriverInfo();


        /** \return Default name of the device. */
        virtual const char *getDefaultName() = 0;

    private:
        // Connection Plugins
        friend class Connection::Serial;
        friend class Connection::TCP;
        friend class Connection::I2C;
        friend class FilterInterface;
        friend class FocuserInterface;
        friend class WeatherInterface;
        friend class LightBoxInterface;
        friend class OutputInterface;
        friend class InputInterface;
        friend class PowerInterface;
        friend class IMUInterface;

    protected:
        DefaultDevice(const std::shared_ptr<DefaultDevicePrivate> &dd);
};

}
