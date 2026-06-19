/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  INDI Weather Device Class

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

#include "defaultdevice.h"
#include "indiweatherinterface.h"

#include <list>

namespace Connection
{
class Serial;
class TCP;
}

/**
 * \class Weather
 * \brief Class to provide general functionality of a weather device.
 *
 * The Weather provides a simple interface for weather devices. Parameters such as temperature,
 * wind, humidity etc can be added by the child class as supported by the physical device. With each
 * parameter, the caller specifies the minimum and maximum ranges of OK and WARNING zones. Any value
 * outside of the warning zone is automatically treated as ALERT.
 *
 * The class also specifies the list of critical parameters for observatory operations. When any of
 * the parameters changes state to WARNING or ALERT, then the overall state of the WEATHER_STATUS
 * property reflects the worst state of any individual parameter. The WEATHER_STATUS property may be
 * used by clients to determine whether to proceed with observation tasks or not, and
 * whether to take any safety measures to protect the observatory from severe weather conditions.
 *
 * The child class should start by first adding all the weather parameters via the addParameter()
 * function, then set all the critical parameters via the setCriticalParameter() function, and finally call
 * generateParameterRanges() function to generate all the parameter ranges properties.
 *
 * Weather update period is controlled by the WEATHER_UPDATE property which stores the update period
 * in seconds and calls updateWeather() every X seconds as given in the property.
 *
 * \e IMPORTANT: GEOGRAPHIC_COORD stores latitude and longitude in INDI specific format, refer to
 * <a href="http://indilib.org/develop/developer-manual/101-standard-properties.html">INDI Standard
 * Properties</a> for details.
 *
 * \author Jasem Mutlaq
 */
namespace INDI
{
class Weather : public DefaultDevice, public WeatherInterface
{
    public:
        enum WeatherLocation
        {
            LOCATION_LATITUDE,
            LOCATION_LONGITUDE,
            LOCATION_ELEVATION
        };

        /** \struct WeatherConnection
         * \brief Holds the connection mode of the Weather.
         */
        enum
        {
            CONNECTION_NONE   = 1 << 0, /** Do not use any connection plugin */
            CONNECTION_SERIAL = 1 << 1, /** For regular serial and bluetooth connections */
            CONNECTION_TCP    = 1 << 2  /** For Wired and WiFI connections */
        } WeatherConnection;

        Weather();

        virtual bool initProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

        virtual bool ISSnoopDevice(XMLEle *root) override;

    protected:
        /** \brief Update weather station location
         *  \param latitude Site latitude in degrees.
         *  \param longitude Site latitude in degrees increasing eastward from Greenwich (0 to 360).
         *  \param elevation Site elevation in meters.
         *  \return True if successful, false otherwise
         *  \note This function performs no action unless subclassed by the child class if required.
         */
        virtual bool updateLocation(double latitude, double longitude, double elevation);

        /**
         * @brief setWeatherConnection Set Weather connection mode. Child class should call this
         * in the constructor before Weather registers any connection interfaces
         * @param value ORed combination of WeatherConnection values.
         */
        void setWeatherConnection(const uint8_t &value);

        /**
         * @return Get current Weather connection mode
         */
        uint8_t getWeatherConnection() const;

        virtual bool saveConfigItems(FILE *fp) override;

        /** \brief perform handshake with device to check communication */
        virtual bool Handshake();

        // A number vector that stores latitude and longitude
        INDI::PropertyNumber LocationNP {3};

        // Active devices to snoop
        INDI::PropertyText ActiveDeviceTP {1};

        Connection::Serial *serialConnection {nullptr};
        Connection::TCP *tcpConnection       {nullptr};

        int PortFD = -1;

    private:
        void processLocationInfo(double latitude, double longitude, double elevation);

        bool callHandshake();
        uint8_t weatherConnection = CONNECTION_SERIAL | CONNECTION_TCP;
};
}
