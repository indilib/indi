/*******************************************************************************
  Copyright(c) 2025,2026 Jérémie Klein, Philippe Bazart. All rights reserved.

  ASCOM Alpaca Dome INDI Driver

  Based on Protocol extracted from https://github.com/rpineau/RigelDome

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#pragma once

#include "indidome.h"

#include <httplib.h>
#include <string>
#include <functional>

// Add these includes
#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

class AlpacaDome : public INDI::Dome
{
    public:
        AlpacaDome();
        virtual ~AlpacaDome() = default;

        /** @brief Callback function to be called once SetTimer duration elapses. */
        virtual void TimerHit() override;

        /**
         * @brief Define the driver's properties to the client.
         * Usually, only a minimum set of properties are defined to the client in this function
         * if the device is in disconnected state. Those properties should be enough to enable the
         * client to establish a connection to the device. In addition to CONNECT/DISCONNECT, such
         * properties may include port name, IP address, etc. You should check if the device is
         * already connected, and if this is true, then you must define the remainder of the
         * the properties to the client in this function. Otherwise, the remainder of the driver's
         * properties are defined to the client in updateProperties() function which is called when
         * a client connects/disconnects from a device.
         * @param dev name of the device
         * @note This function is called by the INDI framework, do not call it directly.
         */
        virtual void ISGetProperties(const char *dev) override;

        /**
         * @brief Process the client newSwitch command.
         * @note This function is called by the INDI framework, do not call it directly.
         * @returns True if any property was successfully processed, false otherwise.
         */
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        /**
         * @brief Process the client newNumber command.
         * @note This function is called by the INDI framework, do not call it directly.
         * @returns True if any property was successfully processed, false otherwise.
         */
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

        /**
         * @brief Process the client newSwitch command.
         * @note This function is called by the INDI framework, do not call it directly.
         * @returns True if any property was successfully processed, false otherwise.
         */
        virtual bool ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n) override;

        /**
         * @return True if the dome can find home position automatically.
         */
        bool CanFindHome()
        {
            return m_CanFindHome;
        }

        /**
         * @return True if the dome can proceed calibration.
         */
        bool CanCalibrate()
        {
            return m_CanCalibrate;
        }
 
        /**
         * @return True if the dome can be parked.
         */
        bool CanPark()
        {
            return m_CanPark;
        }
 
        /**
         * @return True if the dome altitude can be set.
         */
        bool CanSetAltitude()
        {
            return m_CanSetAltitude;
        }
 
        /**
         * @return True if the dome azimuth can be set.
         */
        bool CanSetAzimuth()
        {
            return m_CanSetAzimuth;
        }
 
        /**
         * @return True if the dome can set park position to the current azimuth angle.
         */
        bool CanSetPark()
        {
            return m_CanSetPark;
        }
 
        /**
         * @return True if the dome can open or close the shutter.
         */
        bool CanSetShutter()
        {
            return m_CanSetShutter;
        }

        /**
         * @return True if the dome is electronically slaved to the mount (ie. if this driver does not need to worry about computing geometry adjustments).
         */
        bool CanSlave()
        {
            return m_CanSlave;
        }

        /**
         * @return True if the dome azimuth position can be synchronized with this driver.
         */
        bool CanSyncAzimuth()
        {
            return m_CanSyncAzimuth;
        }

    protected:
        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool saveConfigItems(FILE *fp) override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

        /**
         * @brief SetSpeed Set Dome speed. This does not initiate motion, it sets the speed for the next motion command. If motion is in progress, then change speed accordingly.
         * @param rpm Dome speed (RPM)
         * @return true if successful, false otherwise
         */
        virtual bool SetSpeed(double rpm);

        /**
         * @brief Move the Dome to an relative position.
         * @param azDiff The relative azimuth angle to move. Positive degree is clock-wise direction. Negative degrees is counter clock-wise direction.
         * @return Return IPS_OK if motion is completed and Dome reached requested position. Return IPS_BUSY if Dome started motion to requested position and is in progress. IPS_ALERT on error.
         */
        virtual IPState MoveRel(double azDiff) override;

        /**
         * @brief Move the Dome to an absolute azimuth.
         * @param az The new position of the Dome.
         * @return Return IPS_OK if motion is completed and Dome reached requested position. Return IPS_BUSY if Dome started motion to requested position and is in progress. IPS_ALERT on error.
         */
        virtual IPState MoveAbs(double az) override;

        /**
         * @brief Move the Dome in a particular direction.
         * @param dir Direction of Dome, either DOME_CW or DOME_CCW.
         * @return Return IPS_OK if dome operation is complete. IPS_BUSY if operation is in progress. IPS_ALERT on error.
         */
        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;

        virtual bool Sync(double az) override;
        virtual bool Abort() override;
        virtual IPState Park() override;
        virtual IPState UnPark() override;
        virtual IPState ControlShutter(ShutterOperation operation) override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

        virtual IPState FindHome();
        virtual IPState Calibrate();

        // Alpaca customization hooks for derived drivers.
        virtual std::string getAlpacaURL(const std::string &endpoint);
        virtual std::string buildAlpacaFormData(const nlohmann::json &request);
        virtual void setDefaultServerAddress(const char *host, const char *port, bool force = false);
        virtual uint32_t getTransactionId();

        ///////////////////////////////////////////////////////////////////////////////////
        /// Additional properties
        ///////////////////////////////////////////////////////////////////////////////////
        INDI::PropertyText ServerAddressTP {2};  // Host and port
        enum
        {
            HOST,
            PORT
        };

        INDI::PropertyNumber DeviceNumberNP {1};  // Alpaca device number (remplace l'ancien int)

        INDI::PropertyNumber ConnectionSettingsNP {2};
        enum
        {
            TIMEOUT,
            RW_TIMEOUT
        };

        INDI::PropertySwitch OperationSP {2};
        enum
        {
            OPERATION_FIND_HOME,
            OPERATION_CALIBRATE
        };

        // Info
        INDI::PropertyText InfoTP {5};
        enum
        {
            INFO_DEVICE_NAME,
            INFO_DESCRIPTION,
            INFO_DRIVER_INFO,
            INFO_DRIVER_VERSION,
            INFO_INTERFACE_VERSION
        };

    private:
        struct AlapacaDeviceState
        {
            double altitude;
            double azimuth;
            int shutterStatus;
            bool slewing;
            bool atHome;
            bool atPark;
        };

        void updateStatus();
        void updateDomeStatus(AlapacaDeviceState &state);
        void updateShutterStatus(AlapacaDeviceState &state);

        ///////////////////////////////////////////////////////////////////////////////
        /// Alpaca Communication
        ///////////////////////////////////////////////////////////////////////////////
        bool sendAlpacaGET(const std::string &endpoint, nlohmann::json &response);
        bool sendAlpacaPUT(const std::string &endpoint, const nlohmann::json &request, nlohmann::json &response);

        ///////////////////////////////////////////////////////////////////////////////
        /// General Alpaca requests for Connecting and Disconnecting
        ///////////////////////////////////////////////////////////////////////////////
        bool alpacaConnect();
        bool alpacaDisconnect();

        ///////////////////////////////////////////////////////////////////////////////
        /// General Alpaca requests for getting information
        ///////////////////////////////////////////////////////////////////////////////
        bool alpacaGetBool(const std::string &endpoint, bool& value, bool defaultValue = false);
        bool alpacaGetInt(const std::string &endpoint, int& value, int defaultValue = 0);
        bool alpacaGetDouble(const std::string &endpoint, double& value, double defaultValue = 0.0);
        bool alpacaGetString(const std::string &endpoint, std::string& value, const std::string& defaultValue = "");
        bool alpacaGetDeviceState(AlapacaDeviceState &value, double defaultAz = 0.0, double defaultAlt = 0.0);

        ///////////////////////////////////////////////////////////////////////////////
        /// Dome requests for Parking, Homing, Moving, Settings, Shutter operations
        ///////////////////////////////////////////////////////////////////////////////
        // bool alpacaDomeSetSlaved(bool slaved);
        bool alpacaDomeAbortSlew();
        bool alpacaDomeCloseShutter();
        bool alpacaDomeFindHome();
        bool alpacaDomeOpenShutter();
        bool alpacaDomePark();
        bool alpacaDomeSetPark();
        bool alpacaDomeSlewToAltitude(double alt);
        bool alpacaDomeSlewToAzimuth(double az);
        bool alpacaDomeSyncToAzimuth(double az);

        ///////////////////////////////////////////////////////////////////////////////
        /// Flags to track device capabilities
        ///////////////////////////////////////////////////////////////////////////////
        bool m_CanFindHome {false};
        bool m_CanCalibrate {false};
        bool m_CanPark {false};
        bool m_CanSetAltitude {false};
        bool m_CanSetAzimuth {false};
        bool m_CanSetPark {false};
        bool m_CanSetShutter {true};
        bool m_CanSlave {false};
        bool m_CanSyncAzimuth {false};

        // Internal state variables
        std::string m_DeviceName, m_Description, m_DriverInfo, m_DriverVersion;
        int m_InterfaceVersion {1};

        ///////////////////////////////////////////////////////////////////////////////
        /// Alpaca Communication
        ///////////////////////////////////////////////////////////////////////////////
        std::unique_ptr<httplib::Client> httpClient;
        uint32_t m_ClientTransactionID {1};

        // Config-loaded values (empty = no saved config, mirrors TCP plugin pattern)
        std::string m_ConfigHost;
        std::string m_ConfigPort;
};