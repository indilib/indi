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
    if (ShutdownTriggerS[TRIGGER_CLIENT].s == ISS_ON && HeartBeatN[0].value > 0)
    {
        LOGF_INFO("Client Watchdog is enabled. Shutdown is triggered after %.f seconds of communication loss with the client.",
                  HeartBeatN[0].value);
        if (m_WatchDogTimer > 0)
            RemoveTimer(m_WatchDogTimer);
        m_WatchDogTimer = SetTimer(HeartBeatN[0].value * 1000);
    }

    if (ShutdownTriggerS[TRIGGER_WEATHER].s == ISS_ON)
    {
        if (WeatherThresholdN[0].value > 0)
            LOGF_INFO("Weather Watchdog is enabled. Shutdown is triggered %.f seconds after Weather status enters DANGER zone.",
                      WeatherThresholdN[0].value);
        else
            LOG_INFO("Weather Watchdog is enabled. Shutdown is triggered when Weather status in DANGER zone.");
        // Trigger Snoop
        IDSnoopDevice(ActiveDeviceT[ACTIVE_WEATHER].text, "WEATHER_STATUS");
    }

    IDSnoopDevice(ActiveDeviceT[ACTIVE_TELESCOPE].text, "TELESCOPE_PARK");
    IDSnoopDevice(ActiveDeviceT[ACTIVE_DOME].text, "DOME_PARK");

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
    IUFillNumber(&HeartBeatN[0], "WATCHDOG_HEARTBEAT_VALUE", "Threshold (s)", "%.f", 0, 3600, 60, 0);
    IUFillNumberVector(&HeartBeatNP, HeartBeatN, 1, getDeviceName(), "WATCHDOG_HEARTBEAT", "Heart beat",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Weather Threshold
    IUFillNumber(&WeatherThresholdN[0], "WATCHDOG_WEATHER_VALUE", "Threshold (s)", "%.f", 0, 3600, 60, 0);
    IUFillNumberVector(&WeatherThresholdNP, WeatherThresholdN, 1, getDeviceName(), "WATCHDOG_WEATHER", "Weather",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // INDI Server Settings
    IUFillText(&SettingsT[INDISERVER_HOST], "INDISERVER_HOST", "indiserver host", "localhost");
    IUFillText(&SettingsT[INDISERVER_PORT], "INDISERVER_PORT", "indiserver port", "7624");
    IUFillText(&SettingsT[SHUTDOWN_SCRIPT], "SHUTDOWN_SCRIPT", "shutdown script", nullptr);
    IUFillTextVector(&SettingsTP, SettingsT, 3, getDeviceName(), "WATCHDOG_SETTINGS", "Settings", MAIN_CONTROL_TAB,
                     IP_RW, 60, IPS_IDLE);

    // Shutdown procedure
    IUFillSwitch(&ShutdownProcedureS[PARK_MOUNT], "PARK_MOUNT", "Park Mount", ISS_OFF);
    IUFillSwitch(&ShutdownProcedureS[PARK_DOME], "PARK_DOME", "Park Dome", ISS_OFF);
    IUFillSwitch(&ShutdownProcedureS[EXECUTE_SCRIPT], "EXECUTE_SCRIPT", "Execute Script", ISS_OFF);
    IUFillSwitchVector(&ShutdownProcedureSP, ShutdownProcedureS, 3, getDeviceName(), "WATCHDOG_SHUTDOWN", "Shutdown",
                       MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // Shutdown Trigger
    IUFillSwitch(&ShutdownTriggerS[TRIGGER_CLIENT], "TRIGGER_CLIENT", "Client", ISS_OFF);
    IUFillSwitch(&ShutdownTriggerS[TRIGGER_WEATHER], "TRIGGER_WEATHER", "Weather", ISS_OFF);
    IUFillSwitchVector(&ShutdownTriggerSP, ShutdownTriggerS, 2, getDeviceName(), "WATCHDOG_Trigger", "Trigger",
                       MAIN_CONTROL_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // Mount Policy
    IUFillSwitch(&MountPolicyS[MOUNT_IGNORED], "MOUNT_IGNORED", "Mount ignored", ISS_ON);
    IUFillSwitch(&MountPolicyS[MOUNT_LOCKS], "MOUNT_LOCKS", "Mount locks", ISS_OFF);
    IUFillSwitchVector(&MountPolicySP, MountPolicyS, 2, getDeviceName(), "WATCHDOG_MOUNT_POLICY", "Mount Policy",
                       OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Active Devices
    IUFillText(&ActiveDeviceT[ACTIVE_TELESCOPE], "ACTIVE_TELESCOPE", "Telescope", "Telescope Simulator");
    IUFillText(&ActiveDeviceT[ACTIVE_DOME], "ACTIVE_DOME", "Dome", "Dome Simulator");
    IUFillText(&ActiveDeviceT[ACTIVE_WEATHER], "ACTIVE_WEATHER", "Weather", "Weather Simulator");
    IUFillTextVector(&ActiveDeviceTP, ActiveDeviceT, 3, getDeviceName(), "ACTIVE_DEVICES", "Active devices",
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

    defineProperty(&HeartBeatNP);
    defineProperty(&WeatherThresholdNP);
    defineProperty(&SettingsTP);
    defineProperty(&ShutdownTriggerSP);
    defineProperty(&ShutdownProcedureSP);
    defineProperty(&MountPolicySP);
    defineProperty(&ActiveDeviceTP);
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
bool WatchDog::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Update Settings
        if (!strcmp(SettingsTP.name, name))
        {
            IUUpdateText(&SettingsTP, texts, names, n);
            SettingsTP.s = IPS_OK;
            IDSetText(&SettingsTP, nullptr);

            try
            {
                m_INDIServerPort = std::stoi(SettingsT[INDISERVER_PORT].text);
            }
            catch(...)
            {
                SettingsTP.s = IPS_ALERT;
                LOG_ERROR("Failed to parse numbers.");
            }

            IDSetText(&SettingsTP, nullptr);
            return true;
        }

        // Snoop Active Devices.
        if (!strcmp(ActiveDeviceTP.name, name))
        {
            if (watchdogClient->isBusy())
            {
                ActiveDeviceTP.s = IPS_ALERT;
                IDSetText(&ActiveDeviceTP, nullptr);
                LOG_ERROR("Cannot change devices names while shutdown is in progress...");
                return true;
            }

            IDSnoopDevice(ActiveDeviceT[ACTIVE_WEATHER].text, "WEATHER_STATUS");
            IDSnoopDevice(ActiveDeviceT[ACTIVE_TELESCOPE].text, "TELESCOPE_PARK");
            IDSnoopDevice(ActiveDeviceT[ACTIVE_DOME].text, "DOME_PARK");

            IUUpdateText(&ActiveDeviceTP, texts, names, n);
            ActiveDeviceTP.s = IPS_OK;
            IDSetText(&ActiveDeviceTP, nullptr);
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
        if (!strcmp(WeatherThresholdNP.name, name))
        {
            IUUpdateNumber(&WeatherThresholdNP, values, names, n);
            WeatherThresholdNP.s = IPS_OK;
            IDSetNumber(&WeatherThresholdNP, nullptr);
            return true;
        }
        // Heart Beat
        // Client must set this property to indicate it is alive.
        // If heat beat not received from client then shutdown procedure begins if
        // the client trigger is selected.
        else if (!strcmp(HeartBeatNP.name, name))
        {
            double prevHeartBeat = HeartBeatN[0].value;

            if (watchdogClient->isBusy())
            {
                HeartBeatNP.s = IPS_ALERT;
                IDSetNumber(&HeartBeatNP, nullptr);
                LOG_ERROR("Cannot change heart beat while shutdown is in progress...");
                return true;
            }

            IUUpdateNumber(&HeartBeatNP, values, names, n);
            HeartBeatNP.s = IPS_OK;

            // If trigger is not set, don't do anything else
            if (ShutdownTriggerS[TRIGGER_CLIENT].s == ISS_OFF)
            {
                if (m_WatchDogTimer > 0)
                {
                    RemoveTimer(m_WatchDogTimer);
                    m_WatchDogTimer = -1;
                }
                IDSetNumber(&HeartBeatNP, nullptr);
                return true;
            }

            if (HeartBeatN[0].value == 0)
                LOG_INFO("Client Watchdog is disabled.");
            else
            {
                if (prevHeartBeat != HeartBeatN[0].value)
                    LOGF_INFO("Client Watchdog is enabled. Shutdown is triggered after %.f seconds of communication loss with the client.",
                              HeartBeatN[0].value);

                LOG_DEBUG("Received heart beat from client.");

                if (m_WatchDogTimer > 0)
                {
                    RemoveTimer(m_WatchDogTimer);
                    m_WatchDogTimer = -1;
                }

                m_WatchDogTimer = SetTimer(HeartBeatN[0].value * 1000);
            }
            IDSetNumber(&HeartBeatNP, nullptr);

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
        if (!strcmp(ShutdownProcedureSP.name, name))
        {
            IUUpdateSwitch(&ShutdownProcedureSP, states, names, n);

            if (ShutdownProcedureS[EXECUTE_SCRIPT].s == ISS_ON &&
                    (SettingsT[EXECUTE_SCRIPT].text == nullptr || SettingsT[EXECUTE_SCRIPT].text[0] == '\0'))
            {
                LOG_ERROR("Error: shutdown script file is not set.");
                ShutdownProcedureSP.s                = IPS_ALERT;
                ShutdownProcedureS[EXECUTE_SCRIPT].s = ISS_OFF;
            }
            else
                ShutdownProcedureSP.s = IPS_OK;
            IDSetSwitch(&ShutdownProcedureSP, nullptr);
            return true;
        }
        // Mount Lock Policy
        else if (!strcmp(MountPolicySP.name, name))
        {
            IUUpdateSwitch(&MountPolicySP, states, names, n);
            MountPolicySP.s = IPS_OK;

            if (MountPolicyS[MOUNT_IGNORED].s == ISS_ON)
                LOG_INFO("Mount is ignored. Dome can start parking without waiting for mount to complete parking.");
            else
                LOG_INFO("Mount locks. Dome must wait for mount to park before it can start the parking procedure.");
            IDSetSwitch(&MountPolicySP, nullptr);
            return true;
        }
        // Shutdown Trigger handling
        else if (!strcmp(ShutdownTriggerSP.name, name))
        {
            std::vector<ISState> oldStates(ShutdownTriggerSP.nsp, ISS_OFF);
            std::vector<ISState> newStates(ShutdownTriggerSP.nsp, ISS_OFF);
            for (int i = 0; i < ShutdownTriggerSP.nsp; i++)
                oldStates[i] = ShutdownTriggerS[i].s;
            IUUpdateSwitch(&ShutdownTriggerSP, states, names, n);
            for (int i = 0; i < ShutdownTriggerSP.nsp; i++)
                newStates[i] = ShutdownTriggerS[i].s;


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
                    if (HeartBeatN[0].value == 0)
                    {
                        LOG_ERROR("Heart beat timeout should be set first.");
                        ShutdownTriggerSP.s = IPS_ALERT;
                        for (int i = 0; i < ShutdownTriggerSP.nsp; i++)
                            ShutdownTriggerS[i].s = oldStates[i];
                        IDSetSwitch(&ShutdownTriggerSP, nullptr);
                        return true;
                    }

                    LOGF_INFO("Client Watchdog is enabled. Shutdown is triggered after %.f seconds of communication loss with the client.",
                              HeartBeatN[0].value);
                    if (m_WatchDogTimer > 0)
                        RemoveTimer(m_WatchDogTimer);
                    m_WatchDogTimer = SetTimer(HeartBeatN[0].value * 1000);
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

            ShutdownTriggerSP.s = IPS_OK;
            IDSetSwitch(&ShutdownTriggerSP, nullptr);
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
            if (ShutdownTriggerS[TRIGGER_WEATHER].s == ISS_ON && m_WeatherAlertTimer == -1)
            {
                if (WeatherThresholdN[0].value > 0)
                    LOGF_INFO("Shutdown procedure shall commence in %.f seconds unless weather status improves.", WeatherThresholdN[0].value);
                m_WeatherAlertTimer = SetTimer(WeatherThresholdN[0].value * 1000);
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
                        ShutdownTriggerS[TRIGGER_WEATHER].s == ISS_ON &&
                        m_WeatherState == IPS_ALERT &&
                        ShutdownProcedureS[PARK_MOUNT].s == ISS_ON)
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
                        ShutdownTriggerS[TRIGGER_WEATHER].s == ISS_ON &&
                        m_WeatherState == IPS_ALERT &&
                        ShutdownProcedureS[PARK_DOME].s == ISS_ON)
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

    IUSaveConfigNumber(fp, &HeartBeatNP);
    IUSaveConfigNumber(fp, &WeatherThresholdNP);
    IUSaveConfigText(fp, &SettingsTP);
    IUSaveConfigText(fp, &ActiveDeviceTP);
    IUSaveConfigSwitch(fp, &MountPolicySP);
    IUSaveConfigSwitch(fp, &ShutdownTriggerSP);
    IUSaveConfigSwitch(fp, &ShutdownProcedureSP);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////
void WatchDog::TimerHit()
{
    // Timer is up, we need to start shutdown procedure

    // If nothing to do, then return
    if (ShutdownProcedureS[PARK_DOME].s == ISS_OFF && ShutdownProcedureS[PARK_MOUNT].s == ISS_OFF &&
            ShutdownProcedureS[EXECUTE_SCRIPT].s == ISS_OFF)
        return;

    switch (m_ShutdownStage)
    {
        // Connect to server
        case WATCHDOG_IDLE:

            ShutdownProcedureSP.s = IPS_BUSY;
            IDSetSwitch(&ShutdownProcedureSP, nullptr);

            if (m_WeatherState == IPS_ALERT)
                LOG_WARN("Warning! Weather status in DANGER zone, executing shutdown procedure...");
            else
                LOG_WARN("Warning! Heartbeat threshold timed out, executing shutdown procedure...");

            // No need to start client if only we need to execute the script
            if (ShutdownProcedureS[PARK_MOUNT].s == ISS_OFF && ShutdownProcedureS[PARK_DOME].s == ISS_OFF &&
                    ShutdownProcedureS[EXECUTE_SCRIPT].s == ISS_ON)
            {
                executeScript();
                break;
            }

            // Watch mount if requied
            if (ShutdownProcedureS[PARK_MOUNT].s == ISS_ON)
                watchdogClient->setMount(ActiveDeviceT[ACTIVE_TELESCOPE].text);
            // Watch dome
            if (ShutdownProcedureS[PARK_DOME].s == ISS_ON)
                watchdogClient->setDome(ActiveDeviceT[ACTIVE_DOME].text);

            // Set indiserver host and port
            watchdogClient->setServer(SettingsT[INDISERVER_HOST].text, m_INDIServerPort);

            LOG_DEBUG("Connecting to INDI server...");

            watchdogClient->connectServer();

            m_ShutdownStage = WATCHDOG_CLIENT_STARTED;

            break;

        case WATCHDOG_CLIENT_STARTED:
            // Check if client is ready
            if (watchdogClient->isConnected())
            {
                LOGF_DEBUG("Connected to INDI server %s @ %s", SettingsT[0].text,
                           SettingsT[1].text);

                if (ShutdownProcedureS[PARK_MOUNT].s == ISS_ON)
                    parkMount();
                else if (ShutdownProcedureS[PARK_DOME].s == ISS_ON)
                    parkDome();
                else if (ShutdownProcedureS[EXECUTE_SCRIPT].s == ISS_ON)
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

                if (ShutdownProcedureS[PARK_DOME].s == ISS_ON)
                    parkDome();
                else if (ShutdownProcedureS[EXECUTE_SCRIPT].s == ISS_ON)
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

                if (ShutdownProcedureS[EXECUTE_SCRIPT].s == ISS_ON)
                    executeScript();
                else
                    m_ShutdownStage = WATCHDOG_COMPLETE;
            }
        }
        break;

        case WATCHDOG_COMPLETE:
            LOG_INFO("Shutdown procedure complete.");
            ShutdownProcedureSP.s = IPS_OK;
            IDSetSwitch(&ShutdownProcedureSP, nullptr);
            // If watch dog client still connected, keep it as such
            if (watchdogClient->isConnected())
                m_ShutdownStage = WATCHDOG_CLIENT_STARTED;
            // If server is shutdown, then we reset to IDLE
            else
                m_ShutdownStage = WATCHDOG_IDLE;
            return;

        case WATCHDOG_ERROR:
            ShutdownProcedureSP.s = IPS_ALERT;
            IDSetSwitch(&ShutdownProcedureSP, nullptr);
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
    if (MountPolicyS[MOUNT_IGNORED].s == ISS_ON && ShutdownProcedureS[PARK_DOME].s == ISS_ON)
        parkDome();
    else
        m_ShutdownStage = WATCHDOG_MOUNT_PARKED;
}

void WatchDog::executeScript()
{
    // child
    if (fork() == 0)
    {
        int rc = execlp(SettingsT[EXECUTE_SCRIPT].text, SettingsT[EXECUTE_SCRIPT].text, nullptr);

        if (rc)
            exit(rc);
    }
    // parent
    else
    {
        int statval;
        LOGF_INFO("Executing script %s...", SettingsT[EXECUTE_SCRIPT].text);
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
                           SettingsT[EXECUTE_SCRIPT].text);
                m_ShutdownStage = WATCHDOG_ERROR;
                return;
            }
        }
        else
        {
            LOGF_ERROR(
                "Error: script %s did not terminate with exit. Shutdown procedure terminated.",
                SettingsT[EXECUTE_SCRIPT].text);
            m_ShutdownStage = WATCHDOG_ERROR;
            return;
        }
    }
}
