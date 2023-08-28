/*******************************************************************************
  Copyright(c) 2022 Jasem Mutlaq. All rights reserved.

  Pegasus Uranus Meteo Sensor Driver.

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

#include "uranusmeteo.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <regex>
#include <termios.h>
#include <chrono>
#include <iomanip>

// We declare an auto pointer to UranusMeteo.
static std::unique_ptr<UranusMeteo> ppba(new UranusMeteo());

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
UranusMeteo::UranusMeteo() : WI(this)
{
    setVersion(1, 0);
    m_SkyQualityUpdateTimer.setInterval(60000);
    m_SkyQualityUpdateTimer.callOnTimeout(std::bind(&UranusMeteo::measureSkyQuality, this));
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::initProperties()
{
    INDI::GPS::initProperties();

    setDriverInterface(GPS_INTERFACE | WEATHER_INTERFACE);

    WI::initProperties(MAIN_CONTROL_TAB, ENVIRONMENT_TAB);

    // To distinguish them from GPS properties.
    WI::UpdatePeriodNP.setLabel("Weather Update");
    WI::RefreshSP.setLabel("Weather Refresh");

    addAuxControls();

    ////////////////////////////////////////////////////////////////////////////
    /// Sensors
    ////////////////////////////////////////////////////////////////////////////
    SensorNP[AmbientTemperature].fill("AmbientTemperature",  "AmbientTemperature", "%.2f", -100, 100, 10, 0);
    SensorNP[RelativeHumidity].fill("RelativeHumidity",  "RelativeHumidity", "%.2f%", 0, 100, 10, 0);
    SensorNP[DewPoint].fill("DewPoint",  "DewPoint (C)", "%.2f", 0, 100, 10, 0);
    SensorNP[AbsolutePressure].fill("AbsolutePressure",  "AbsolutePressure (hPA)", "%.2f", 0, 100, 10, 0);
    SensorNP[RelativePressure].fill("RelativePressure",  "RelativePressure (hPA)", "%.2f", 0, 100, 10, 0);
    SensorNP[BarometricAltitude].fill("BarometricAltitude",  "BarometricAltitude (m)", "%.2f", 0, 100, 10, 0);
    SensorNP[SkyTemperature].fill("SkyTemperature",  "SkyTemperature (C)", "%.2f", 0, 100, 10, 0);
    SensorNP[InfraredTemperature].fill("InfraredTemperature",  "InfraredTemperature (C)", "%.2f", 0, 100, 10, 0);
    SensorNP[BatteryUsage].fill("BatteryUsage",  "BatteryUsage", "%.2f%", 0, 100, 10, 0);
    SensorNP[BatteryVoltage].fill("BatteryVoltage",  "BatteryVoltage", "%.2f", 0, 100, 10, 0);
    SensorNP.fill(getDeviceName(), "SENSORS", "Sensors", SENSORS_TAB, IP_RO, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Cloud
    ////////////////////////////////////////////////////////////////////////////
    CloudsNP[TemperatureDifference].fill("TemperatureDifference",  "Temperature Difference (C)", "%.2f", -1000, 1000, 10, 0);
    CloudsNP[CloudIndex].fill("CloudIndex",  "Cloud Coverage (%)", "%.2f", 0, 100, 10, 0);
    CloudsNP[CloudSkyTemperature].fill("CloudSkyTemperature",  "Sky Temperature (C)", "%.2f", -1000, 1000, 10, 0);
    CloudsNP[CloudAmbientTemperature].fill("CloudAmbientTemperature",  "Ambient Temperature (C)", "%.2f", -1000, 1000, 10, 0);
    CloudsNP[InfraredEmissivity].fill("InfraredEmissivity",  "Infrared Emissivity", "%.2f", 0, 1, 0.1, 0);
    CloudsNP.fill(getDeviceName(), "CLOUDS", "Clouds", CLOUDS_TAB, IP_RO, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Sky Quality
    ////////////////////////////////////////////////////////////////////////////
    SkyQualityNP[MPAS].fill("MPAS",  "MPAS (mag/arcsec^2)", "%.2f", 0, 30, 10, 0);
    SkyQualityNP[NELM].fill("NELM",  "Naked Eye Llimit (mag)", "%.2f", 0, 100, 10, 0);
    SkyQualityNP[FullSpectrum].fill("FullSpectrum",  "Full Spectrum", "%.2f", -1000, 1000, 10, 0);
    SkyQualityNP[VisualSpectrum].fill("VisualSpectrum",  "Visual Spectrum", "%.2f", -1000, 1000, 10, 0);
    SkyQualityNP[InfraredSpectrum].fill("InfraredSpectrum",  "Infrared Spectrum", "%.2f", 0, 1, 0.1, 0);
    SkyQualityNP.fill(getDeviceName(), "SKYQUALITY", "Sky Quality", SKYQUALITY_TAB, IP_RO, 60, IPS_IDLE);

    SkyQualityUpdateNP[0].fill("VALUE", "Period (s)", "%.f", 0, 3600, 60, 60);
    SkyQualityUpdateNP.fill(getDeviceName(), "SKYQUALITY_TIMER", "Update", SKYQUALITY_TAB, IP_RW, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// GPS
    ////////////////////////////////////////////////////////////////////////////
    GPSNP[GPSFix].fill("GPSFix",  "GPS Fix", "%.f", 0, 3, 1, 0);
    GPSNP[GPSTime].fill("GPSTime",  "Unix Time", "%.f", 0, 1e9, 10, 0);
    GPSNP[UTCOffset].fill("UTCOffset",  "UTC Offset", "%.2f", -12, 12, 1, 0);
    GPSNP[Latitude].fill("Latitude",  "Latitude", "%.2f", -90, 90, 10, 0);
    GPSNP[Longitude].fill("Longitude",  "Longitude", "%.2f", -180, 180, 10, 0);
    GPSNP[SatelliteNumber].fill("SatelliteNumber",  "Sat. #", "%.f", 0, 30, 10, 0);
    GPSNP[GPSSpeed].fill("GPSSpeed",  "Speed (kph)", "%.2f", 0, 30, 10, 0);
    GPSNP[GPSBearing].fill("GPSBearing",  "Bearing (deg)", "%.2f", 0, 360, 10, 0);
    GPSNP.fill(getDeviceName(), "GPS", "GPS", GPS_TAB, IP_RO, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Weather Parameters
    ////////////////////////////////////////////////////////////////////////////
    addParameter("WEATHER_CLOUD", "Cloud (%)", 0, 85, 15);
    addParameter("WEATHER_MPAS", "MPAS (mag/arcsec^2)", 1, 30, 15);
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -20, 50, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity (%)", 0, 75, 15);

    setCriticalParameter("WEATHER_CLOUD");
    setCriticalParameter("WEATHER_TEMPERATURE");
    setCriticalParameter("WEATHER_HUMIDITY");

    ////////////////////////////////////////////////////////////////////////////
    /// Serial Connection
    ////////////////////////////////////////////////////////////////////////////
    setDefaultPollingPeriod(5000);
    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::updateProperties()
{
    INDI::GPS::updateProperties();

    if (isConnected())
    {
        defineProperty(SensorNP);
        defineProperty(CloudsNP);

        defineProperty(SkyQualityNP);
        defineProperty(SkyQualityUpdateNP);

        defineProperty(GPSNP);

        WI::updateProperties();
        m_SetupComplete = true;

        readSensors();
        readClouds();

        measureSkyQuality();
    }
    else
    {

        deleteProperty(SensorNP);
        deleteProperty(CloudsNP);

        deleteProperty(SkyQualityNP);
        deleteProperty(SkyQualityUpdateNP);

        deleteProperty(GPSNP);

        WI::updateProperties();
        m_SetupComplete = false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
const char * UranusMeteo::getDefaultName()
{
    return "Uranus Meteo Sensor";
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::Handshake()
{
    PortFD = serialConnection->getPortFD();
    m_SetupComplete = false;
    char response[PEGASUS_LEN] = {0};

    if (sendCommand("M#", response))
        return (!strcmp(response, "MS_OK"));

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
IPState UranusMeteo::updateGPS()
{
    char response[PEGASUS_LEN] = {0};

    if (sendCommand("GP", response))
    {
        std::vector<std::string> result = split(response + 3, ":");

        if (result == m_GPS)
            return IPS_OK;

        m_Sensors = result;

        try
        {
            GPSNP[GPSFix].setValue(std::stod(result[GPSFix]));
            GPSNP[GPSTime].setValue(std::stod(result[GPSTime]));
            GPSNP[UTCOffset].setValue(std::stod(result[UTCOffset]));
            GPSNP[Latitude].setValue(std::stod(result[Latitude]));
            GPSNP[Longitude].setValue(std::stod(result[Longitude]));
            GPSNP[SatelliteNumber].setValue(std::stod(result[SatelliteNumber]));
            GPSNP[GPSSpeed].setValue(std::stod(result[GPSSpeed]));
            GPSNP[GPSBearing].setValue(std::stod(result[GPSBearing]));

            GPSNP.setState(IPS_OK);
            GPSNP.apply();

            if (GPSNP[GPSFix].getValue() < 3)
                return IPS_BUSY;

            LocationNP[LOCATION_LATITUDE].value  = GPSNP[Latitude].getValue();
            LocationNP[LOCATION_LONGITUDE].value = GPSNP[Longitude].getValue();
            // 2017-11-15 Jasem: INDI Longitude is 0 to 360 East+
            if (LocationNP[LOCATION_LONGITUDE].value < 0)
                LocationNP[LOCATION_LONGITUDE].value += 360;

            LocationNP[LOCATION_ELEVATION].value = SensorNP[BarometricAltitude].getValue();

            // Get GPS Time
            char ts[32] = {0};
            time_t raw_time = GPSNP[GPSTime].getValue();

            // Convert to UTC
            // JM 2022.12.28: Uranus returns LOCAL TIME, not UTC.
            struct tm *local = localtime(&raw_time);
            // Get UTC Offset
            auto utcOffset = local->tm_gmtoff / 3600.0;
            // Convert to UTC time
            time_t utcTime = raw_time - utcOffset * 3600.0;
            // Store in GPS
            m_GPSTime = utcTime;
            // Get tm struct in UTC
            struct tm *utc = gmtime(&utcTime);
            // Format it
            strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", utc);
            IUSaveText(&TimeTP[0], ts);

            snprintf(ts, sizeof(ts), "%.2f", utcOffset);
            IUSaveText(&TimeTP[1], ts);

            // Set UTC offset in device
            char command[PEGASUS_LEN] = {0};
            snprintf(command, PEGASUS_LEN, "C3:%d", static_cast<int>(utcOffset));
            sendCommand(command, response);

            return IPS_OK;
        }
        catch(...)
        {
            LOGF_WARN("Failed to process sensor response: %s (%d bytes)", response, strlen(response));
            return IPS_ALERT;
        }
    }

    return IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
IPState UranusMeteo::updateWeather()
{
    setParameterValue("WEATHER_TEMPERATURE", SensorNP[AmbientTemperature].getValue());
    setParameterValue("WEATHER_HUMIDITY", SensorNP[RelativeHumidity].getValue());
    setParameterValue("WEATHER_CLOUD", CloudsNP[CloudIndex].getValue());
    setParameterValue("WEATHER_MPAS", SkyQualityNP[MPAS].getValue());
    return IPS_OK;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (processSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::GPS::ISNewSwitch(dev, name, states, names, n);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Sky Quality Update
        if (SkyQualityUpdateNP.isNameMatch(name))
        {
            SkyQualityUpdateNP.update(values, names, n);
            auto value = SkyQualityUpdateNP[0].getValue();
            if (value > 0)
            {
                m_SkyQualityUpdateTimer.start(value * 1000);
                SkyQualityUpdateNP.setState(IPS_OK);
            }
            else
            {
                LOG_INFO("Sky Quality Update is disabled.");
                SkyQualityUpdateNP.setState(IPS_IDLE);
            }
            SkyQualityUpdateNP.apply();
            return true;
        }

        if (processNumber(dev, name, values, names, n))
            return true;
    }

    return INDI::GPS::ISNewNumber(dev, name, values, names, n);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void UranusMeteo::TimerHit()
{
    if (!isConnected() || m_SetupComplete == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    readSensors();
    readClouds();

    SetTimer(getCurrentPollingPeriod());
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::saveConfigItems(FILE * fp)
{
    INDI::GPS::saveConfigItems(fp);
    WI::saveConfigItems(fp);
    SkyQualityUpdateNP.save(fp);
    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::readSensors()
{
    char response[PEGASUS_LEN] = {0};

    if (sendCommand("MA", response))
    {
        std::vector<std::string> result = split(response + 6, ":");

        if (result == m_Sensors)
            return true;

        m_Sensors = result;

        try
        {
            SensorNP[AmbientTemperature].setValue(std::stod(result[AmbientTemperature]));
            SensorNP[RelativeHumidity].setValue(std::stod(result[RelativeHumidity]));
            SensorNP[DewPoint].setValue(std::stod(result[DewPoint]));
            SensorNP[AbsolutePressure].setValue(std::stod(result[AbsolutePressure]));
            SensorNP[BarometricAltitude].setValue(std::stod(result[BarometricAltitude]));
            SensorNP[SkyTemperature].setValue(std::stod(result[SkyTemperature]));
            SensorNP[InfraredTemperature].setValue(std::stod(result[InfraredTemperature]));
            SensorNP[BatteryUsage].setValue(std::stod(result[BatteryUsage]));
            SensorNP[BatteryVoltage].setValue(std::stod(result[BatteryVoltage]));

            SensorNP.setState(IPS_OK);
            SensorNP.apply();
            return true;
        }
        catch(...)
        {
            LOGF_WARN("Failed to process sensor response: %s (%d bytes)", response, strlen(response));
            return false;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::readSkyQuality()
{
    char response[PEGASUS_LEN] = {0};

    if (sendCommand("SQ", response))
    {
        std::vector<std::string> result = split(response + 3, ":");

        if (result == m_SkyQuality)
            return true;

        m_SkyQuality = result;

        try
        {
            SkyQualityNP[MPAS].setValue(std::stod(result[MPAS]));
            SkyQualityNP[NELM].setValue(std::stod(result[NELM]));
            SkyQualityNP[FullSpectrum].setValue(std::stod(result[FullSpectrum]));
            SkyQualityNP[VisualSpectrum].setValue(std::stod(result[VisualSpectrum]));
            SkyQualityNP[InfraredSpectrum].setValue(std::stod(result[InfraredSpectrum]));

            SkyQualityNP.setState(IPS_OK);
            SkyQualityNP.apply();
            return true;
        }
        catch(...)
        {
            LOGF_WARN("Failed to process sky quality response: %s (%d bytes)", response, strlen(response));
            return false;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::readClouds()
{
    char response[PEGASUS_LEN] = {0};

    if (sendCommand("CI", response))
    {
        std::vector<std::string> result = split(response + 3, ":");

        if (result == m_Clouds)
            return true;

        m_Clouds = result;

        try
        {
            CloudsNP[TemperatureDifference].setValue(std::stod(result[TemperatureDifference]));
            CloudsNP[CloudIndex].setValue(std::stod(result[CloudIndex]));
            CloudsNP[CloudSkyTemperature].setValue(std::stod(result[CloudSkyTemperature]));
            CloudsNP[CloudAmbientTemperature].setValue(std::stod(result[CloudAmbientTemperature]));
            CloudsNP[InfraredEmissivity].setValue(std::stod(result[InfraredEmissivity]));

            CloudsNP.setState(IPS_OK);
            CloudsNP.apply();
            return true;
        }
        catch(...)
        {
            LOGF_WARN("Failed to process cloud response: %s (%d bytes)", response, strlen(response));
            return false;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void UranusMeteo::measureSkyQuality()
{
    char response[PEGASUS_LEN] = {0};
    LOG_DEBUG("Measuring sky quality...");
    if (sendCommand("SQ:1", response))
    {
        readSkyQuality();
        if (SkyQualityUpdateNP[0].getValue() > 0)
            m_SkyQualityUpdateTimer.start(SkyQualityUpdateNP[0].getValue() * 1000);
    }
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::readMoon()
{
    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::readTwilight()
{
    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::readConfig()
{
    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::sendCommand(const char * cmd, char * res)
{
    int nbytes_read = 0, nbytes_written = 0, tty_rc = 0;
    char command[PEGASUS_LEN] = {0};
    LOGF_DEBUG("CMD <%s>", cmd);

    for (int i = 0; i < 2; i++)
    {
        tcflush(PortFD, TCIOFLUSH);
        snprintf(command, PEGASUS_LEN, "%s\r\n", cmd);
        if ( (tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
            continue;

        if (!res)
        {
            tcflush(PortFD, TCIOFLUSH);
            return true;
        }

        if ( (tty_rc = tty_nread_section(PortFD, res, PEGASUS_LEN, PEGASUS_STOP_CHAR, PEGASUS_TIMEOUT, &nbytes_read)) != TTY_OK
                || nbytes_read == 1)
            continue;

        tcflush(PortFD, TCIOFLUSH);
        res[nbytes_read - 2] = '\0';
        LOGF_DEBUG("RES <%s>", res);
        return true;
    }

    if (tty_rc != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(tty_rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial error: %s", errorMessage);
    }

    return false;
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
std::vector<std::string> UranusMeteo::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}
