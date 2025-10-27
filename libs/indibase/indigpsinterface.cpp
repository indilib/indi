/*
    GPS Interface
    Copyright (C) 2023 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indigpsinterface.h"

#include "indilogger.h"

#include <cstring>
#include <cmath>

namespace INDI
{

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
GPSInterface::GPSInterface(DefaultDevice * defaultDevice) : m_DefaultDevice(defaultDevice)
{
    m_UpdateTimer.callOnTimeout(std::bind(&GPSInterface::checkGPSState, this));
    m_UpdateTimer.setSingleShot(true);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void GPSInterface::initProperties(const char * groupName)
{
    time(&m_GPSTime);

    // @INDI_STANDARD_PROPERTY@
    PeriodNP[0].fill("PERIOD", "Period (s)", "%.f", 0, 3600, 60.0, 0);
    PeriodNP.fill(m_DefaultDevice->getDeviceName(), "GPS_REFRESH_PERIOD", "Refresh", groupName, IP_RW, 0, IPS_IDLE);

    // @INDI_STANDARD_PROPERTY@
    RefreshSP[0].fill("REFRESH", "GPS", ISS_OFF);
    RefreshSP.fill(m_DefaultDevice->getDeviceName(), "GPS_REFRESH", "Refresh", groupName, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // @INDI_STANDARD_PROPERTY@
    LocationNP[LOCATION_LATITUDE].fill("LAT", "Lat (dd:mm:ss)", "%010.6m", -90, 90, 0, 0.0);
    LocationNP[LOCATION_LONGITUDE].fill("LONG", "Lon (dd:mm:ss)", "%010.6m", 0, 360, 0, 0.0);
    LocationNP[LOCATION_ELEVATION].fill("ELEV", "Elevation (m)", "%g", -200, 10000, 0, 0);
    LocationNP.fill(m_DefaultDevice->getDeviceName(), "GEOGRAPHIC_COORD", "Location", groupName, IP_RO, 60, IPS_IDLE);

    // System Time Settings
    // @INDI_STANDARD_PROPERTY@
    SystemTimeUpdateSP[UPDATE_NEVER].fill("UPDATE_NEVER", "Never", ISS_OFF);
    SystemTimeUpdateSP[UPDATE_ON_STARTUP].fill("UPDATE_ON_STARTUP", "On Startup", ISS_ON);
    SystemTimeUpdateSP[UPDATE_ON_REFRESH].fill("UPDATE_ON_REFRESH", "On Refresh", ISS_OFF);
    SystemTimeUpdateSP.fill(m_DefaultDevice->getDeviceName(), "SYSTEM_TIME_UPDATE", "System Time", groupName, IP_RW,
                            ISR_1OFMANY, 60,
                            IPS_IDLE);
    SystemTimeUpdateSP.load();

    // @INDI_STANDARD_PROPERTY@
    TimeTP[0].fill("UTC", "UTC Time", nullptr);
    TimeTP[1].fill("OFFSET", "UTC Offset", nullptr);
    TimeTP.fill(m_DefaultDevice->getDeviceName(), "TIME_UTC", "UTC", groupName, IP_RO, 60, IPS_IDLE);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void GPSInterface::checkGPSState()
{
    IPState state = updateGPS();

    LocationNP.setState(state);
    TimeTP.setState(state);
    RefreshSP.setState(state);

    switch (state)
    {
        // Ok
        case IPS_OK:
            LocationNP.apply();
            TimeTP.apply();

            // Update system time
            // This ideally should be done only ONCE
            switch (SystemTimeUpdateSP.findOnSwitchIndex())
            {
                case UPDATE_ON_STARTUP:
                    if (m_SystemTimeUpdated == false)
                    {
                        setSystemTime(m_GPSTime);
                        m_SystemTimeUpdated = true;
                    }
                    break;

                case UPDATE_ON_REFRESH:
                    setSystemTime(m_GPSTime);
                    break;

                default:
                    break;
            }

            if (PeriodNP[0].getValue() > 0)
            {
                // We got data OK, but if we are required to update once in a while, we'll call it.
                m_UpdateTimer.setInterval(PeriodNP[0].getValue() * 1000);
                m_UpdateTimer.start();
            }
            else
                m_UpdateTimer.stop();
            return;
            break;

        // GPS fix is in progress or alert
        case IPS_ALERT:
            LocationNP.apply();
            TimeTP.apply();
            break;

        default:
            break;
    }

    m_UpdateTimer.setInterval(5000);
    m_UpdateTimer.start();
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool GPSInterface::updateProperties()
{
    if (m_DefaultDevice->isConnected())
    {
        // Update GPS and send values to client
        IPState state = updateGPS();

        LocationNP.setState(state);
        m_DefaultDevice->defineProperty(LocationNP);
        TimeTP.setState(state);
        m_DefaultDevice->defineProperty(TimeTP);
        RefreshSP.setState(state);
        m_DefaultDevice->defineProperty(RefreshSP);
        m_DefaultDevice->defineProperty(PeriodNP);
        m_DefaultDevice->defineProperty(SystemTimeUpdateSP);

        if (state != IPS_OK)
        {
            if (state == IPS_BUSY)
                DEBUGDEVICE(m_DefaultDevice->getDeviceName(), Logger::DBG_SESSION, "GPS fix is in progress...");

            m_UpdateTimer.setInterval(5000);
            m_UpdateTimer.start();
        }
        else if (PeriodNP[0].getValue() > 0)
        {
            m_UpdateTimer.setInterval(PeriodNP[0].getValue());
            m_UpdateTimer.start();
        }
    }
    else
    {
        m_DefaultDevice->deleteProperty(LocationNP);
        m_DefaultDevice->deleteProperty(TimeTP);
        m_DefaultDevice->deleteProperty(RefreshSP);
        m_DefaultDevice->deleteProperty(PeriodNP);
        m_DefaultDevice->deleteProperty(SystemTimeUpdateSP);
        m_UpdateTimer.stop();
        m_SystemTimeUpdated = false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool GPSInterface::processNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    INDI_UNUSED(dev);

    if (PeriodNP.isNameMatch(name))
    {
        double prevPeriod = PeriodNP[0].getValue();
        PeriodNP.update(values, names, n);
        // Do not remove timer if GPS update is still in progress
        if (m_UpdateTimer.isActive() && RefreshSP.getState() != IPS_BUSY)
        {
            m_UpdateTimer.stop();
        }

        if (PeriodNP[0].getValue() == 0)
        {
            DEBUGDEVICE(m_DefaultDevice->getDeviceName(), Logger::DBG_SESSION, "GPS Update Timer disabled.");
        }
        else
        {
            m_UpdateTimer.setInterval(PeriodNP[0].value * 1000);
            m_UpdateTimer.start();
            // Need to warn user this is not recommended. Startup values should be enough
            if (prevPeriod == 0)
            {
                DEBUGDEVICE(m_DefaultDevice->getDeviceName(), Logger::DBG_SESSION,  "GPS Update Timer enabled.");
                if (SystemTimeUpdateSP[UPDATE_ON_REFRESH].getState() == ISS_ON)
                    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), Logger::DBG_WARNING,
                                "Updating system-wide time repeatedly may lead to undesirable side-effects.");
            }
        }

        PeriodNP.setState(IPS_OK);
        PeriodNP.apply();

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool GPSInterface::processSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    INDI_UNUSED(dev);

    // Refresh
    if (RefreshSP.isNameMatch(name))
    {
        RefreshSP[0].s = ISS_OFF;
        RefreshSP.setState(IPS_OK);
        RefreshSP.apply();

        // Manual trigger
        checkGPSState();
        return true;
    }
    // System Time Update
    else if (SystemTimeUpdateSP.isNameMatch(name))
    {
        SystemTimeUpdateSP.update(states, names, n);
        SystemTimeUpdateSP.setState(IPS_OK);
        SystemTimeUpdateSP.apply();
        if (SystemTimeUpdateSP.findOnSwitchIndex() == UPDATE_ON_REFRESH)
            DEBUGDEVICE(m_DefaultDevice->getDeviceName(), Logger::DBG_WARNING,
                        "Updating system time on refresh may lead to undesirable effects on system time accuracy.");
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool GPSInterface::setSystemTime(time_t &raw_time)
{
#ifdef __linux__
#if defined(__GNU_LIBRARY__)
#if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ > 30)
    timespec sTime = {};
    sTime.tv_sec = raw_time;
    if (clock_settime(CLOCK_REALTIME, &sTime) == -1)
        DEBUGFDEVICE(m_DefaultDevice->getDeviceName(), Logger::DBG_WARNING, "Failed to update system time: %s", strerror(errno));
#else
    stime(&raw_time);
#endif
#else
    stime(&raw_time);
#endif
#else
    INDI_UNUSED(raw_time);
#endif
    return true;
}


//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
IPState GPSInterface::updateGPS()
{
    DEBUGDEVICE(m_DefaultDevice->getDeviceName(), Logger::DBG_ERROR,
                "updateGPS() must be implemented in GPS device child class to update TIME_UTC and GEOGRAPHIC_COORD properties.");
    return IPS_ALERT;
}


bool GPSInterface::saveConfigItems(FILE * fp)
{
    PeriodNP.save(fp);
    SystemTimeUpdateSP.save(fp);
    return true;
}

}
