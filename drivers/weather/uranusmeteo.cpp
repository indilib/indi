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
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::initProperties()
{
    INDI::GPS::initProperties();

    setDriverInterface(GPS_INTERFACE | WEATHER_INTERFACE);

    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

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
    /// Weather Parameters
    ////////////////////////////////////////////////////////////////////////////
    addParameter("WEATHER_CLOUD", "Cloud (%)", 0, 100, 15);
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -10, 30, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity (%)", 0, 80, 15);

    setCriticalParameter("WEATHER_CLOUD");
    setCriticalParameter("WEATHER_TEMPERATURE");
    setCriticalParameter("WEATHER_HUMIDITY");

    ////////////////////////////////////////////////////////////////////////////
    /// Serial Connection
    ////////////////////////////////////////////////////////////////////////////
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
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {

        defineProperty(SensorNP);

        WI::updateProperties();
        m_SetupComplete = true;
    }
    else
    {

        deleteProperty(SensorNP);

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
    return IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
IPState UranusMeteo::updateWeather()
{
    auto sensorsOK = readSensors();

    if (sensorsOK)
    {
        setParameterValue("WEATHER_TEMPERATURE", SensorNP[AmbientTemperature].getValue());
        setParameterValue("WEATHER_HUMIDITY", SensorNP[RelativeHumidity].getValue());
    }

    auto cloudsOK = readClouds();

    if (cloudsOK)
    {
        setParameterValue("WEATHER_CLOUD", CloudsNP[CloudIndex].getValue());
    }

    return (sensorsOK && cloudsOK) ? IPS_OK : IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {

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
        if (strstr(name, "WEATHER_"))
            return WI::processNumber(dev, name, values, names, n);
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


    SetTimer(getCurrentPollingPeriod());
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::saveConfigItems(FILE * fp)
{
    INDI::GPS::saveConfigItems(fp);
    WI::saveConfigItems(fp);
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

            SensorNP->setState(IPS_OK);
            SensorNP->apply();
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
    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::readClouds()
{
    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool UranusMeteo::readGPS()
{
    return false;
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

