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

#include "indiapi.h"
#include "indibase.h"

#include <string>
#include <vector>

#include <indimacros.h>
#include <memory>

// #define MAXRBUF 2048 // #PS: defined in indibase.h

/** @class INDI::BaseClient
 *  @brief Class to provide basic client functionality.
 *
 *  BaseClient enables accelerated development of INDI Clients by providing a framework that facilitates communication, device
 *  handling, and event notification. By subclassing BaseClient, clients can quickly connect to an INDI server, and query for
 *  a set of INDI::BaseDevice devices, and read and write properties seamlessly. Event driven programming is possible due to
 *  notifications upon reception of new devices or properties.
 *
 *  Upon connecting to an INDI server, it creates a dedicated thread to handle all incoming traffic. The thread is terminated
 *  when disconnectServer() is called or when a communication error occurs.
 *
 *  @attention All notifications functions defined in INDI::BaseMediator <b>must</b> be implemented in the client class even if
 *  they are not used because these are pure virtual functions.
 *
 *  @see <a href="http://indilib.org/develop/tutorials/107-client-development-tutorial.html">INDI Client Tutorial</a> for more details.
 *  @author Jasem Mutlaq
 */

namespace INDI
{
    class BaseClientPrivate;
}
class INDI::BaseClient : public INDI::BaseMediator
{
        DECLARE_PRIVATE(BaseClient)

    public:
        BaseClient();
        virtual ~BaseClient();

    public:
        /** @brief Set the server host name and port.
         *  @param hostname INDI server host name or IP address.
         *  @param port INDI server port.
         */
        void setServer(const char *hostname, unsigned int port);

        /** @brief Get the server host name. */
        const char *getHost() const;

        /** @brief Get the port number */
        int getPort() const;

        /** @brief Connect to INDI server.
         *  @returns True if the connection is successful, false otherwise.
         *  @note This function blocks until connection is either successull or unsuccessful.
         */
        bool connectServer();

        /** @brief Disconnect from INDI server.
         *
         *  Disconnects from INDI servers. Any devices previously created will be deleted and memory cleared.
         *
         *  @return True if disconnection is successful, false otherwise.
         */
        bool disconnectServer(int exit_code = 0);

        /** @brief Get status of the connection. */
        bool isServerConnected() const;

        /** @brief setConnectionTimeout Set connection timeout. By default it is 3 seconds.
         *  @param seconds seconds
         *  @param microseconds microseconds
         */
        void setConnectionTimeout(uint32_t seconds, uint32_t microseconds);

        void serverDisconnected(int exit_code) override;

    public:
        /** @brief setVerbose Set verbose mode
         *  @param enable If true, enable <b>FULL</b> verbose output. Any XML message received, including BLOBs, are printed on
         *                standard output. Only use this for debugging purposes.
         */
        void setVerbose(bool enable);

        /** @brief isVerbose Is client in verbose mode?
         *  @return Is client in verbose mode?
         */
        bool isVerbose() const;

    public:
        /** @brief watchProperties Add a property to the watch list. When communicating with INDI server.
         *
         *  The client calls <getProperties device=deviceName property=propertyName/> so that only a particular
         *  property (or list of properties if more than one) are defined back to the client. This function
         *  will call watchDevice(deviceName) as well to limit the traffic to this device.
         *
         *  @param propertyName Property to watch for.
         */
        void watchProperty(const char *deviceName, const char *propertyName);

    public:
        /** @brief Add a device to the watch list.
         *
         *  A client may select to receive notifications of only a specific device or a set of devices.
         *  If the client encounters any of the devices set via this function, it will create a corresponding
         *  INDI::BaseDevice object to handle them. If no devices are watched, then all devices owned by INDI server
         *  will be created and handled.
         */
        void watchDevice(const char *deviceName);

        /** @brief Connect to INDI driver
         *  @param deviceName Name of the device to connect to.
        */
        void connectDevice(const char *deviceName);

        /** @brief Disconnect INDI driver
         *  @param deviceName Name of the device to disconnect.
         */
        void disconnectDevice(const char *deviceName);

        /** @param deviceName Name of device to search for in the list of devices owned by INDI server,
         *  @returns If \e deviceName exists, it returns an instance of the device. Otherwise, it returns NULL.
         */
        INDI::BaseDevice *getDevice(const char *deviceName);

        /** @returns Returns a vector of all devices created in the client. */
        const std::vector<INDI::BaseDevice *> &getDevices() const;

        /** @brief getDevices Returns list of devices that belong to a particular @ref INDI::BaseDevice::DRIVER_INTERFACE "DRIVER_INTERFACE" class.
         *
         *  For example, to get a list of guide cameras:
         *  @code{.cpp}
         *  std::vector<INDI::BaseDevice *> guideCameras;
         *  getDevices(guideCameras, CCD_INTERFACE | GUIDER_INTERFACE);
         *  for (INDI::BaseDevice *device : guideCameras)
         *      cout << "Guide Camera Name: " << device->getDeviceName();
         *  @endcode
         *  @param deviceList Supply device list to be filled by the function.
         *  @param driverInterface ORed DRIVER_INTERFACE values to select the desired class of devices.
         *  @return True if one or more devices are found for the supplied driverInterface, false if no matching devices found.
         */
        bool getDevices(std::vector<INDI::BaseDevice *> &deviceList, uint16_t driverInterface);

    public:
        /** @brief Set Binary Large Object policy mode
         *
         *  Set the BLOB handling mode for the client. The client may either receive:
         *  <ul>
         *    <li>Only BLOBS</li>
         *    <li>BLOBs mixed with normal messages</li>
         *    <li>Normal messages only, no BLOBs</li>
         *  </ul>
         *  If \e dev and \e prop are supplied, then the BLOB handling policy is set for this particular device and property.
         *  if \e prop is NULL, then the BLOB policy applies to the whole device.
         *
         *  @param blobH BLOB handling policy
         *  @param dev name of device, required.
         *  @param prop name of property, optional.
         */
        void setBLOBMode(BLOBHandling blobH, const char *dev, const char *prop = nullptr);

        /** @brief getBLOBMode Get Binary Large Object policy mode IF set previously by setBLOBMode
         *  @param dev name of device.
         *  @param prop property name, can be NULL to return overall device policy if it exists.
         *  @return BLOB Policy, if not found, it always returns B_ALSO
         */
        BLOBHandling getBLOBMode(const char *dev, const char *prop = nullptr);

        /** @brief Send new Text command to server */
        void sendNewText(ITextVectorProperty *pp);
        /** @brief Send new Text command to server */
        void sendNewText(const char *deviceName, const char *propertyName, const char *elementName, const char *text);
        /** @brief Send new Number command to server */
        void sendNewNumber(INumberVectorProperty *pp);
        /** @brief Send new Number command to server */
        void sendNewNumber(const char *deviceName, const char *propertyName, const char *elementName, double value);
        /** @brief Send new Switch command to server */
        void sendNewSwitch(ISwitchVectorProperty *pp);
        /** @brief Send new Switch command to server */
        void sendNewSwitch(const char *deviceName, const char *propertyName, const char *elementName);

        /** @brief Send opening tag for BLOB command to server */
        void startBlob(const char *devName, const char *propName, const char *timestamp);
        /** @brief Send ONE blob content to server. The BLOB data in raw binary format and will be converted to base64 and sent to server */
        void sendOneBlob(IBLOB *bp);
        /** @brief Send ONE blob content to server. The BLOB data in raw binary format and will be converted to base64 and sent to server */
        void sendOneBlob(const char *blobName, unsigned int blobSize, const char *blobFormat, void *blobBuffer);
        /** @brief Send closing tag for BLOB command to server */
        void finishBlob();

    protected:
        /** @brief newUniversalMessage Universal messages are sent from INDI server without a specific device. It is addressed to the client overall.
         *  @param message content of message.
         *  @note The default implementation simply logs the message to stderr. Override to handle the message.
         */
        virtual void newUniversalMessage(std::string message);

    protected:
        std::unique_ptr<INDI::BaseClientPrivate> d_ptr;
};
