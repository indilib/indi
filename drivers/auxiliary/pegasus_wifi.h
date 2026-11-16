/*******************************************************************************
  Pegasus Astro WiFi settings

  Copyright (C) 2026 Dominik Merkel

  Shared by the drivers of Pegasus Astro devices with the common WiFi command set.

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
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*******************************************************************************/

#pragma once

#include "indipropertynumber.h"
#include "indipropertyswitch.h"
#include "indipropertytext.h"

#include <cstdio>
#include <functional>
#include <string>
#include <sys/types.h>

namespace INDI
{
class DefaultDevice;
}

namespace PegasusAstro
{

/**
 * @brief The WiFi class handles the WiFi settings of Pegasus Astro devices that share the
 *        WiFi command set, so they can be set up without the Unity Platform: hotspot,
 *        hotspot channel, WiFi client and network scan. It also logs in on the TCP command
 *        socket, which accepts the same commands as USB after the hotspot password.
 *
 * Replies seen on Falcon V2 firmware 1.8 and FocusCube 3 firmware 2.0:
 *   A? -> 0|1, AE:x -> 1, AL -> ssid:password, AN:/AP: -> 1, AC -> n, AC:n -> n,
 *   WA -> 0 while not connected, WS -> 0 when no network is found, WN:/WP: -> 0 on error.
 * The FlatMaster Neo (firmware 1.4) echoes WN:/WP: and probably AN:/AP:, so for these
 * commands, AE:x and AC:n only 0 counts as a failure.
 */
class WiFi
{
    public:
        /** Load (by property name) and save a property in the driver configuration; these are protected in DefaultDevice. */
        using LoadConfig = std::function<void(const char *)>;
        using SaveConfig = std::function<void(INDI::Property &)>;

        explicit WiFi(INDI::DefaultDevice *device);

        /** @brief Create the properties: the WiFi settings on @a tab, the TCP password on the connection tab. */
        void initProperties(const char *tab, LoadConfig loadConfig, SaveConfig saveConfig);

        /** @brief Define the TCP password and load it once from the configuration. Call from ISGetProperties. */
        void ISGetProperties();

        /** @brief Define the WiFi properties and read the settings, or delete them on disconnect. */
        void updateProperties(bool connected, int portFD);

        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

        /** @brief Save the TCP password; the device stores its WiFi settings itself. */
        void saveConfigItems(FILE *fp);

        /** @brief Log in on the TCP command socket with the hotspot password. */
        bool login(int portFD);

        /**
         * @brief Discard pending input. tcflush() only works on a serial port, so over TCP a line end
         *        left behind by a previous reply would otherwise start the next one.
         */
        static void drain(int portFD);

        /** @brief Read the current WiFi connection again, e.g. on every poll. Does nothing while not connected. */
        void updateConnection();

        /**
         * @brief Allow joining an open network with an empty password. The device then gets the
         *        placeholder password "PASSWORD", as Pegasus' software sends to the FlatMaster Neo.
         */
        void allowOpenNetworks(bool allow)
        {
            m_AllowOpenNetworks = allow;
        }

        /** @brief Port of the TCP command socket. */
        static constexpr int TCP_PORT {9999};

    private:
        /** @brief Send one command and read the one-line reply. @a secret hides command and reply in the log. */
        bool sendCommand(const char *cmd, char *res, int len, int timeout = 3, bool secret = false);
        bool readHotspotStatus();
        bool readHotspotCredentials();
        bool readChannel();
        bool readConnection();
        bool scan();
        /** @brief Turn a WS reply into "SSID (signal%)" entries, each SSID once, strongest first. */
        static std::string formatScan(const char *reply);
        bool setHotspotCredentials(const char *ssid, const char *password);
        bool connectToNetwork(const char *ssid, const char *password);
        static bool validText(const char *text, size_t maxLength);
        static void readConnectionLater(void *context);

        INDI::DefaultDevice *m_Device {nullptr};
        LoadConfig m_LoadConfig;
        SaveConfig m_SaveConfig;
        int m_PortFD {-1};
        bool m_PasswordLoaded {false};
        bool m_Connected {false};
        bool m_AllowOpenNetworks {false};

        INDI::PropertySwitch HotspotSP {2};
        INDI::PropertyText HotspotTP {2};
        INDI::PropertyNumber ChannelNP {1};
        INDI::PropertyText ConnectionTP {1};
        INDI::PropertySwitch ScanSP {1};
        INDI::PropertyText ScanTP {1};
        INDI::PropertyText NetworkTP {2};
        INDI::PropertySwitch ConnectSP {1};
        INDI::PropertyText PasswordTP {1};

        static constexpr int LEN {128};
        static constexpr int SCAN_LEN {4096};
        /** WS scans for up to 15 seconds. */
        static constexpr int SCAN_TIMEOUT {16};
        static constexpr int SCAN_RETRIES {4};
        /** The device joins a network in about 5 seconds; check the result after this time. */
        static constexpr int CONNECT_CHECK_MS {10000};
        static constexpr useconds_t SCAN_RETRY_DELAY_US {3000000};
        static constexpr const char *DEFAULT_PASSWORD {"12345678"};
        static constexpr const char *OPEN_NETWORK_PASSWORD {"PASSWORD"};
};

}
