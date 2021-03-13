/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  INDI Watchdog driver.

  The driver expects a heartbeat from the client every X minutes. If no heartbeat
  is received, the driver executes the shutdown procedures.

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

#include "watchdog.h"
#include "watchdogclient.h"

#include <memory>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

// Naming the object after my love Juli which I lost in 2018. May she rest in peace.
// http://indilib.org/images/juli_tommy.jpg
std::unique_ptr<WatchDog> juli(new WatchDog());

void ISGetProperties(const char *dev)
{
    juli->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    juli->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    juli->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    juli->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}
void ISSnoopDevice(XMLEle *root)
{
    juli->ISSnoopDevice(root);
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
WatchDog::WatchDog()
{
    setVersion(0, 3);
    setDriverInterface(AUX_INTERFACE);

    watchdogClient = new WatchDogClient();

    m_ShutdownStage = WATCHDOG_IDLE;
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
WatchDog::~WatchDog()
{
    delete (watchdogClient);
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
const char *WatchDog::getDefaultName()
{
    return "WatchDog";
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
bool WatchDog::Connect()
{
    if (ShutdownTriggerSP[TRIGGER_CLIENT].getState() == ISS_ON && HeartBeatNP[0].getValue() > 0)
    {
        LOGF_INFO("Client Watchdog is enabled. Shutdown is triggered after %.f seconds of communication loss with the client.",
                  HeartBeatNP[0].getValue());
        if (m_WatchDogTimer > 0)
            RemoveTimer(m_WatchDogTimer);
        m_WatchDogTimer = SetTimer(HeartBeatNP[0].value * 1000);
    }

    if (ShutdownTriggerSP[TRIGGER_WEATHER].getState() == ISS_ON)
    {
        if (WeatherThresholdNP[0].value > 0)
            LOGF_INFO("Weather Watchdog is enabled. Shutdown is triggered %.f seconds after Weather status enters DANGER zone.",
                      WeatherThresholdNP[0].getValue());
        else
            LOG_INFO("Weather Watchdog is enabled. Shutdown is triggered when Weather status in DANGER zone.");
        // Trigger Snoop
        IDSnoopDevice(ActiveDeviceTP[ACTIVE_WEATHER].getText(), "WEATHER_STATUS");
    }

    IDSnoopDevice(ActiveDeviceTP[ACTIVE_TELESCOPE].getText(), "TELESCOPE_PARK");
    IDSnoopDevice(ActiveDeviceTP[ACTIVE_DOME].getText(), "DOME_PARK");

    return true;
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
bool WatchDog::Disconnect()
{
    if (m_WatchDogTimer > 0)
    {
        RemoveTimer(m_WatchDogTimer);
        m_WatchDogTimer = -1;
    }
    if (m_WeatherAlertTimer > 0)
    {
        RemoveTimer(m_WeatherAlertTimer);
        m_WeatherAlertTimer = -1;
    }

    LOG_INFO("Watchdog is disabled.");
    m_ShutdownStage = WATCHDOG_IDLE;

    return true;
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
bool WatchDog::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Heart Beat to client
    HeartBeatNP[0].fill("WATCHDOG_HEARTBEAT_VALUE", "Threshold (s)", "%.f", 0, 3600, 60, 0);
    HeartBeatNP.fill(getDeviceName(), "WATCHDOG_HEARTBEAT", "Heart beat",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Weather Threshold
    WeatherThresholdNP[0].fill("WATCHDOG_WEATHER_VALUE", "Threshold (s)", "%.f", 0, 3600, 60, 0);
    WeatherThresholdNP.fill(getDeviceName(), "WATCHDOG_WEATHER", "Weather",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // INDI Server Settings
    SettingsTP[INDISERVER_HOST].fill("INDISERVER_HOST", "indiserver host", "localhost");
    SettingsTP[INDISERVER_PORT].fill("INDISERVER_PORT", "indiserver port", "7624");
    SettingsTP[SHUTDOWN_SCRIPT].fill("SHUTDOWN_SCRIPT", "shutdown script", nullptr);
    SettingsTP.fill(getDeviceName(), "WATCHDOG_SETTINGS", "Settings", MAIN_CONTROL_TAB,
                     IP_RW, 60, IPS_IDLE);

    // Shutdown procedure
    ShutdownProcedureSP[PARK_MOUNT].fill("PARK_MOUNT", "Park Mount", ISS_OFF);
    ShutdownProcedureSP[PARK_DOME].fill("PARK_DOME", "Park Dome", ISS_OFF);
    ShutdownProcedureSP[EXECUTE_SCRIPT].fill("EXECUTE_SCRIPT", "Execute Script", ISS_OFF);
    ShutdownProcedureSP.fill(getDeviceName(), "WATCHDOG_SHUTDOWN", "Shutdown",
                       MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // Shutdown Trigger
    ShutdownTriggerSP[TRIGGER_CLIENT].fill("TRIGGER_CLIENT", "Client", ISS_OFF);
    ShutdownTriggerSP[TRIGGER_WEATHER].fill("TRIGGER_WEATHER", "Weather", ISS_OFF);
    ShutdownTriggerSP.fill(getDeviceName(), "WATCHDOG_Trigger", "Trigger",
                       MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // Mount Policy
    MountPolicySP[MOUNT_IGNORED].fill("MOUNT_IGNORED", "Mount ignored", ISS_ON);
    MountPolicySP[MOUNT_LOCKS].fill("MOUNT_LOCKS", "Mount locks", ISS_OFF);
    MountPolicySP.fill(getDeviceName(), "WATCHDOG_MOUNT_POLICY", "Mount Policy",
                       OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Active Devices
    ActiveDeviceTP[ACTIVE_TELESCOPE].fill("ACTIVE_TELESCOPE", "Telescope", "Telescope Simulator");
    ActiveDeviceTP[ACTIVE_DOME].fill("ACTIVE_DOME", "Dome", "Dome Simulator");
    ActiveDeviceTP[ACTIVE_WEATHER].fill("ACTIVE_WEATHER", "Weather", "Weather Simulator");
    ActiveDeviceTP.fill(getDeviceName(), "ACTIVE_DEVICES", "Active devices",
                     OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    addDebugControl();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
void WatchDog::ISGetProperties(const char *dev)
{
    DefaultDevice::ISGetProperties(dev);

    defineProperty(HeartBeatNP);
    defineProperty(WeatherThresholdNP);
    defineProperty(SettingsTP);
    defineProperty(ShutdownTriggerSP);
    defineProperty(ShutdownProcedureSP);
    defineProperty(MountPolicySP);
    defineProperty(ActiveDeviceTP);
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
bool WatchDog::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Update Settings
        if (SettingsTP.isNameMatch(name))
        {
            SettingsTP.update(texts, names, n);
            SettingsTP.setState(IPS_OK);
            SettingsTP.apply();

            try
            {
                m_INDIServerPort = std::stoi(SettingsTP[INDISERVER_PORT].getText());
            }
            catch(...)
            {
                SettingsTP.setState(IPS_ALERT);
                LOG_ERROR("Failed to parse numbers.");
            }

            SettingsTP.apply();
            return true;
        }

        // Snoop Active Devices.
        if (ActiveDeviceTP.isNameMatch(name))
        {
            if (watchdogClient->isBusy())
            {
                ActiveDeviceTP.setState(IPS_ALERT);
                ActiveDeviceTP.apply();
                LOG_ERROR("Cannot change devices names while shutdown is in progress...");
                return true;
            }

            IDSnoopDevice(ActiveDeviceTP[ACTIVE_WEATHER].getText(), "WEATHER_STATUS");
            IDSnoopDevice(ActiveDeviceTP[ACTIVE_TELESCOPE].getText(), "TELESCOPE_PARK");
            IDSnoopDevice(ActiveDeviceTP[ACTIVE_DOME].getText(), "DOME_PARK");

            ActiveDeviceTP.update(texts, names, n);
            ActiveDeviceTP.setState(IPS_OK);
            ActiveDeviceTP.apply();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
bool WatchDog::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Weather threshold
        if (WeatherThresholdNP.isNameMatch(name))
        {
            WeatherThresholdNP.update(values, names, n);
            WeatherThresholdNP.setState(IPS_OK);
            WeatherThresholdNP.apply();
            return true;
        }
        // Heart Beat
        // Client must set this property to indicate it is alive.
        // If heat beat not received from client then shutdown procedure begins if
        // the client trigger is selected.
        else if (HeartBeatNP.isNameMatch(name))
        {
            double prevHeartBeat = HeartBeatNP[0].getValue();

            if (watchdogClient->isBusy())
            {
                HeartBeatNP.setState(IPS_ALERT);
                HeartBeatNP.apply();
                LOG_ERROR("Cannot change heart beat while shutdown is in progress...");
                return true;
            }

            HeartBeatNP.update(values, names, n);
            HeartBeatNP.setState(IPS_OK);

            // If trigger is not set, don't do anything else
            if (ShutdownTriggerSP[TRIGGER_CLIENT].getState() == ISS_OFF)
            {
                if (m_WatchDogTimer > 0)
                {
                    RemoveTimer(m_WatchDogTimer);
                    m_WatchDogTimer = -1;
                }
                HeartBeatNP.apply();
                return true;
            }

            if (HeartBeatNP[0].value == 0)
                LOG_INFO("Client Watchdog is disabled.");
            else
            {
                if (prevHeartBeat != HeartBeatNP[0].getValue())
                    LOGF_INFO("Client Watchdog is enabled. Shutdown is triggered after %.f seconds of communication loss with the client.",
                              HeartBeatNP[0].getValue());

                LOG_DEBUG("Received heart beat from client.");

                if (m_WatchDogTimer > 0)
                {
                    RemoveTimer(m_WatchDogTimer);
                    m_WatchDogTimer = -1;
                }

                m_WatchDogTimer = SetTimer(HeartBeatNP[0].value * 1000);
            }
            HeartBeatNP.apply();

            return true;
        }
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
bool WatchDog::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Shutdown procedure setting
        if (ShutdownProcedureSP.isNameMatch(name))
        {
            ShutdownProcedureSP.update(states, names, n);

            if (ShutdownProcedureSP[EXECUTE_SCRIPT].getState() == ISS_ON &&
                    (SettingsTP[EXECUTE_SCRIPT].text == nullptr || SettingsTP[EXECUTE_SCRIPT].text[0] == '\0'))
            {
                LOG_ERROR("Error: shutdown script file is not set.");
                ShutdownProcedureSP.setState(IPS_ALERT);
                ShutdownProcedureSP[EXECUTE_SCRIPT].setState(ISS_OFF);
            }
            else
                ShutdownProcedureSP.setState(IPS_OK);
            ShutdownProcedureSP.apply();
            return true;
        }
        // Mount Lock Policy
        else if (MountPolicySP.isNameMatch(name))
        {
            MountPolicySP.update(states, names, n);
            MountPolicySP.setState(IPS_OK);

            if (MountPolicySP[MOUNT_IGNORED].getState() == ISS_ON)
                LOG_INFO("Mount is ignored. Dome can start parking without waiting for mount to complete parking.");
            else
                LOG_INFO("Mount locks. Dome must wait for mount to park before it can start the parking procedure.");
            MountPolicySP.apply();
            return true;
        }
        // Shutdown Trigger handling
        else if (ShutdownTriggerSP.isNameMatch(name))
        {
            std::vector<ISState> oldStates(ShutdownTriggerSP.size(), ISS_OFF);
            std::vector<ISState> newStates(ShutdownTriggerSP.size(), ISS_OFF);
            for (size_t i = 0; i < ShutdownTriggerSP.size(); i++)
                oldStates[i] = ShutdownTriggerSP[i].getState();
            ShutdownTriggerSP.update(states, names, n);
            for (size_t i = 0; i < ShutdownTriggerSP.size(); i++)
                newStates[i] = ShutdownTriggerSP[i].getState();


            // Check for client changes
            if (oldStates[TRIGGER_CLIENT] != newStates[TRIGGER_CLIENT])
            {
                // User disabled client trigger
                if (newStates[TRIGGER_CLIENT] == ISS_OFF)
                {
                    LOG_INFO("Disabling client watchdog. Lost communication with client shall not trigger the shutdown procedure.");
                    if (m_WatchDogTimer > 0)
                    {
                        RemoveTimer(m_WatchDogTimer);
                        m_WatchDogTimer = -1;
                    }
                }
                // User enabled client trigger
                else
                {
                    // Check first that we have a valid heart beat
                    if (HeartBeatNP[0].value == 0)
                    {
                        LOG_ERROR("Heart beat timeout should be set first.");
                        ShutdownTriggerSP.setState(IPS_ALERT);
                        for (size_t i = 0; i < ShutdownTriggerSP.size(); i++)
                            ShutdownTriggerSP[i].setState(oldStates[i]);
                        ShutdownTriggerSP.apply();
                        return true;
                    }

                    LOGF_INFO("Client Watchdog is enabled. Shutdown is triggered after %.f seconds of communication loss with the client.",
                              HeartBeatNP[0].getValue());
                    if (m_WatchDogTimer > 0)
                        RemoveTimer(m_WatchDogTimer);
                    m_WatchDogTimer = SetTimer(HeartBeatNP[0].value * 1000);
                }
            }

            // Check for weather changes
            if (oldStates[TRIGGER_WEATHER] != newStates[TRIGGER_WEATHER])
            {
                // User disabled weather trigger
                if (newStates[TRIGGER_WEATHER] == ISS_OFF)
                {
                    // If we have an active timer, remove it.
                    if (m_WeatherAlertTimer > 0)
                    {
                        RemoveTimer(m_WeatherAlertTimer);
                        m_WeatherAlertTimer = -1;
                    }

                    LOG_INFO("Weather Watchdog is disabled.");
                }
                else
                    LOG_INFO("Weather Watchdog is enabled.");

            }

            ShutdownTriggerSP.setState(IPS_OK);
            ShutdownTriggerSP.apply();
            return true;
        }
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
bool WatchDog::ISSnoopDevice(XMLEle * root)
{
    const char * propName = findXMLAttValu(root, "name");

    // Weather Status
    if (!strcmp("WEATHER_STATUS", propName))
    {
        IPState newWeatherState;
        crackIPState(findXMLAttValu(root, "state"), &newWeatherState);

        // In case timer is active, and weather status is now OK
        // Let's disable the timer
        if (m_WeatherState == IPS_ALERT && newWeatherState != IPS_ALERT)
        {
            LOG_INFO("Weather status is no longer in DANGER zone.");
            if (m_WeatherAlertTimer > 0)
            {
                LOG_INFO("Shutdown procedure cancelled.");
                RemoveTimer(m_WeatherAlertTimer);
                m_WeatherAlertTimer = -1;
            }
        }

        // In case weather shutdown is active and;
        // weather timer is off and;
        // previous weather status is not alert and;
        // current weather status is alert, then
        // we start the weather timer which on timeout would case the shutdown procedure to commence.
        if (m_WeatherState != IPS_ALERT && newWeatherState == IPS_ALERT)
        {
            LOG_WARN("Weather is in DANGER zone.");
            if (ShutdownTriggerSP[TRIGGER_WEATHER].getState() == ISS_ON && m_WeatherAlertTimer == -1)
            {
                if (WeatherThresholdNP[0].value > 0)
                    LOGF_INFO("Shutdown procedure shall commence in %.f seconds unless weather status improves.", WeatherThresholdNP[0].getValue());
                m_WeatherAlertTimer = SetTimer(WeatherThresholdNP[0].value * 1000);
            }
        }

        m_WeatherState = newWeatherState;
    }
    // Check Telescope Park status
    else if (!strcmp("TELESCOPE_PARK", propName))
    {
        if (!strcmp(findXMLAttValu(root, "state"), "Ok"))
        {
            XMLEle * ep = nullptr;
            bool parked = false;
            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                const char * elemName = findXMLAttValu(ep, "name");

                if ((!strcmp(elemName, "PARK") && !strcmp(pcdataXMLEle(ep), "On")))
                    parked = true;
                else if ((!strcmp(elemName, "UNPARK") && !strcmp(pcdataXMLEle(ep), "On")))
                    parked = false;
            }
            if (parked != m_IsMountParked)
            {
                LOGF_INFO("Mount is %s", parked ? "Parked" : "Unparked");
                m_IsMountParked = parked;
                // In case mount was UNPARKED while weather status is still ALERT
                // And weather shutdown trigger was active and mount parking was selected
                // then we force the mount to park again.
                if (parked == false &&
                        ShutdownTriggerSP[TRIGGER_WEATHER].getState() == ISS_ON &&
                        m_WeatherState == IPS_ALERT &&
                        ShutdownProcedureSP[PARK_MOUNT].getState() == ISS_ON)
                {
                    LOG_WARN("Mount unparked while weather alert is active! Parking mount...");
                    watchdogClient->parkMount();
                }
            }
            return true;
        }
    }
    // Check Telescope Park status
    else if (!strcmp("DOME_PARK", propName))
    {
        if (!strcmp(findXMLAttValu(root, "state"), "Ok") || !strcmp(findXMLAttValu(root, "state"), "Busy"))
        {
            XMLEle * ep = nullptr;
            bool parked = false;
            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                const char * elemName = findXMLAttValu(ep, "name");

                if ((!strcmp(elemName, "PARK") && !strcmp(pcdataXMLEle(ep), "On")))
                    parked = true;
                else if ((!strcmp(elemName, "UNPARK") && !strcmp(pcdataXMLEle(ep), "On")))
                    parked = false;
            }
            if (parked != m_IsDomeParked)
            {
                LOGF_INFO("Dome is %s", parked ? "Parked" : "Unparked");
                m_IsDomeParked = parked;
                // In case mount was UNPARKED while weather status is still ALERT
                // And weather shutdown trigger was active and mount parking was selected
                // then we force the mount to park again.
                if (parked == false &&
                        ShutdownTriggerSP[TRIGGER_WEATHER].getState() == ISS_ON &&
                        m_WeatherState == IPS_ALERT &&
                        ShutdownProcedureSP[PARK_DOME].getState() == ISS_ON)
                {
                    LOG_WARN("Dome unparked while weather alert is active! Parking dome...");
                    watchdogClient->parkDome();
                }
            }
            return true;
        }
    }

    return DefaultDevice::ISSnoopDevice(root);
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
bool WatchDog::saveConfigItems(FILE * fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    HeartBeatNP.save(fp);
    WeatherThresholdNP.save(fp);
    SettingsTP.save(fp);
    ActiveDeviceTP.save(fp);
    MountPolicySP.save(fp);
    ShutdownTriggerSP.save(fp);
    ShutdownProcedureSP.save(fp);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
void WatchDog::TimerHit()
{
    // Timer is up, we need to start shutdown procedure

    // If nothing to do, then return
    if (ShutdownProcedureSP[PARK_DOME].getState() == ISS_OFF && ShutdownProcedureSP[PARK_MOUNT].s == ISS_OFF &&
            ShutdownProcedureSP[EXECUTE_SCRIPT].getState() == ISS_OFF)
        return;

    switch (m_ShutdownStage)
    {
        // Connect to server
        case WATCHDOG_IDLE:

            ShutdownProcedureSP.setState(IPS_BUSY);
            ShutdownProcedureSP.apply();

            if (m_WeatherState == IPS_ALERT)
                LOG_WARN("Warning! Weather status in DANGER zone, executing shutdown procedure...");
            else
                LOG_WARN("Warning! Heartbeat threshold timed out, executing shutdown procedure...");

            // No need to start client if only we need to execute the script
            if (ShutdownProcedureSP[PARK_MOUNT].getState() == ISS_OFF && ShutdownProcedureSP[PARK_DOME].s == ISS_OFF &&
                    ShutdownProcedureSP[EXECUTE_SCRIPT].getState() == ISS_ON)
            {
                executeScript();
                break;
            }

            // Watch mount if requied
            if (ShutdownProcedureSP[PARK_MOUNT].getState() == ISS_ON)
                watchdogClient->setMount(ActiveDeviceTP[ACTIVE_TELESCOPE].getText());
            // Watch dome
            if (ShutdownProcedureSP[PARK_DOME].getState() == ISS_ON)
                watchdogClient->setDome(ActiveDeviceTP[ACTIVE_DOME].getText());

            // Set indiserver host and port
            watchdogClient->setServer(SettingsTP[INDISERVER_HOST].getText(), m_INDIServerPort);

            LOG_DEBUG("Connecting to INDI server...");

            watchdogClient->connectServer();

            m_ShutdownStage = WATCHDOG_CLIENT_STARTED;

            break;

        case WATCHDOG_CLIENT_STARTED:
            // Check if client is ready
            if (watchdogClient->isConnected())
            {
                LOGF_DEBUG("Connected to INDI server %s @ %s", SettingsTP[0].getText(),
                           SettingsTP[1].getText());

                if (ShutdownProcedureSP[PARK_MOUNT].getState() == ISS_ON)
                    parkMount();
                else if (ShutdownProcedureSP[PARK_DOME].getState() == ISS_ON)
                    parkDome();
                else if (ShutdownProcedureSP[EXECUTE_SCRIPT].getState() == ISS_ON)
                    executeScript();
            }
            else
                LOG_DEBUG("Waiting for INDI server connection...");
            break;

        case WATCHDOG_MOUNT_PARKED:
        {
            // check if mount is parked
            IPState mountState = watchdogClient->getMountParkState();

            if (mountState == IPS_OK || mountState == IPS_IDLE)
            {
                LOG_INFO("Mount parked.");

                if (ShutdownProcedureSP[PARK_DOME].getState() == ISS_ON)
                    parkDome();
                else if (ShutdownProcedureSP[EXECUTE_SCRIPT].getState() == ISS_ON)
                    executeScript();
                else
                    m_ShutdownStage = WATCHDOG_COMPLETE;
            }
        }
        break;

        case WATCHDOG_DOME_PARKED:
        {
            // check if dome is parked
            IPState domeState = watchdogClient->getDomeParkState();

            if (domeState == IPS_OK || domeState == IPS_IDLE)
            {
                LOG_INFO("Dome parked.");

                if (ShutdownProcedureSP[EXECUTE_SCRIPT].getState() == ISS_ON)
                    executeScript();
                else
                    m_ShutdownStage = WATCHDOG_COMPLETE;
            }
        }
        break;

        case WATCHDOG_COMPLETE:
            LOG_INFO("Shutdown procedure complete.");
            ShutdownProcedureSP.setState(IPS_OK);
            ShutdownProcedureSP.apply();
            // If watch dog client still connected, keep it as such
            if (watchdogClient->isConnected())
                m_ShutdownStage = WATCHDOG_CLIENT_STARTED;
            // If server is shutdown, then we reset to IDLE
            else
                m_ShutdownStage = WATCHDOG_IDLE;
            return;

        case WATCHDOG_ERROR:
            ShutdownProcedureSP.setState(IPS_ALERT);
            ShutdownProcedureSP.apply();
            return;
    }

    SetTimer(getCurrentPollingPeriod());
}

void WatchDog::parkDome()
{
    if (watchdogClient->parkDome() == false)
    {
        LOG_ERROR("Error: Unable to park dome! Shutdown procedure terminated.");
        m_ShutdownStage = WATCHDOG_ERROR;
        return;
    }

    LOG_INFO("Parking dome...");
    m_ShutdownStage = WATCHDOG_DOME_PARKED;
}

void WatchDog::parkMount()
{
    if (watchdogClient->parkMount() == false)
    {
        LOG_ERROR("Error: Unable to park mount! Shutdown procedure terminated.");
        m_ShutdownStage = WATCHDOG_ERROR;
        return;
    }

    LOG_INFO("Parking mount...");

    // If mount is set to ignored, and we have active dome shutdown then we start
    // parking the dome immediately.
    if (MountPolicySP[MOUNT_IGNORED].getState() == ISS_ON && ShutdownProcedureSP[PARK_DOME].s == ISS_ON)
        parkDome();
    else
        m_ShutdownStage = WATCHDOG_MOUNT_PARKED;
}

void WatchDog::executeScript()
{
    // child
    if (fork() == 0)
    {
        int rc = execlp(SettingsTP[EXECUTE_SCRIPT].getText(), SettingsTP[EXECUTE_SCRIPT].getText(), nullptr);

        if (rc)
            exit(rc);
    }
    // parent
    else
    {
        int statval;
        LOGF_INFO("Executing script %s...", SettingsTP[EXECUTE_SCRIPT].getText());
        LOGF_INFO("Waiting for script with PID %d to complete...", getpid());
        wait(&statval);
        if (WIFEXITED(statval))
        {
            int exit_code = WEXITSTATUS(statval);
            LOGF_INFO("Script complete with exit code %d", exit_code);

            if (exit_code == 0)
                m_ShutdownStage = WATCHDOG_COMPLETE;
            else
            {
                LOGF_ERROR("Error: script %s failed. Shutdown procedure terminated.",
                           SettingsTP[EXECUTE_SCRIPT].getText());
                m_ShutdownStage = WATCHDOG_ERROR;
                return;
            }
        }
        else
        {
            LOGF_ERROR(
                "Error: script %s did not terminate with exit. Shutdown procedure terminated.",
                SettingsTP[EXECUTE_SCRIPT].getText());
            m_ShutdownStage = WATCHDOG_ERROR;
            return;
        }
    }
}
