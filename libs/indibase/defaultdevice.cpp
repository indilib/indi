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

#include "defaultdevice.h"
#include "defaultdevice_p.h"

#include "indicom.h"
#include "indiapi.h"

#include "indistandardproperty.h"
#include "connectionplugins/connectionserial.h"

#include "indipropertyswitch.h"
#include "indipropertytext.h"
#include "indipropertynumber.h"
#include "indipropertyblob.h"

#include <cstdlib>
#include <cstring>
#include <assert.h>
#include <algorithm>

const char *COMMUNICATION_TAB = "Communication";
const char *MAIN_CONTROL_TAB  = "Main Control";
const char *CONNECTION_TAB    = "Connection";
const char *MOTION_TAB        = "Motion Control";
const char *DATETIME_TAB      = "Date/Time";
const char *SITE_TAB          = "Site Management";
const char *OPTIONS_TAB       = "Options";
const char *FILTER_TAB        = "Filter Wheel";
const char *FOCUS_TAB         = "Focuser";
const char *GUIDE_TAB         = "Guide";
const char *ALIGNMENT_TAB     = "Alignment";
const char *SATELLITE_TAB     = "Satellite";
const char *INFO_TAB          = "General Info";

std::list<INDI::DefaultDevicePrivate*> INDI::DefaultDevicePrivate::devices;
std::recursive_mutex                   INDI::DefaultDevicePrivate::devicesLock;

extern "C"
{

    void ISGetProperties(const char *dev)
    {
        const std::unique_lock<std::recursive_mutex> lock(INDI::DefaultDevicePrivate::devicesLock);
        for(auto &it : INDI::DefaultDevicePrivate::devices)
            it->defaultDevice->ISGetProperties(dev);
    }

    void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
    {
        const std::unique_lock<std::recursive_mutex> lock(INDI::DefaultDevicePrivate::devicesLock);
        for(auto &it : INDI::DefaultDevicePrivate::devices)
            if (dev == nullptr || strcmp(dev, it->defaultDevice->getDeviceName()) == 0)
                it->defaultDevice->ISNewSwitch(dev, name, states, names, n);
    }

    void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
    {
        const std::unique_lock<std::recursive_mutex> lock(INDI::DefaultDevicePrivate::devicesLock);
        for(auto &it : INDI::DefaultDevicePrivate::devices)
            if (dev == nullptr || strcmp(dev, it->defaultDevice->getDeviceName()) == 0)
                it->defaultDevice->ISNewNumber(dev, name, values, names, n);
    }

    void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
    {
        const std::unique_lock<std::recursive_mutex> lock(INDI::DefaultDevicePrivate::devicesLock);
        for(auto &it : INDI::DefaultDevicePrivate::devices)
            if (dev == nullptr || strcmp(dev, it->defaultDevice->getDeviceName()) == 0)
                it->defaultDevice->ISNewText(dev, name, texts, names, n);
    }

    void ISNewBLOB(const char *dev, const char *name,
                   int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n
                  )
    {
        const std::unique_lock<std::recursive_mutex> lock(INDI::DefaultDevicePrivate::devicesLock);
        for(auto &it : INDI::DefaultDevicePrivate::devices)
            if (dev == nullptr || strcmp(dev, it->defaultDevice->getDeviceName()) == 0)
                it->defaultDevice->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
    }

    void ISSnoopDevice(XMLEle *root)
    {
        const std::unique_lock<std::recursive_mutex> lock(INDI::DefaultDevicePrivate::devicesLock);
        for(auto &it : INDI::DefaultDevicePrivate::devices)
            it->defaultDevice->ISSnoopDevice(root);
    }

} // extern "C"


// For drivers, indiproperty need access to ID/IU functions
extern void (*WeakIDSetTextVA)(const ITextVectorProperty *, const char *, va_list);
extern void (*WeakIDDefTextVA)(const ITextVectorProperty *, const char *, va_list);
extern void (*WeakIDSetNumberVA)(const INumberVectorProperty *, const char *, va_list);
extern void (*WeakIDDefNumberVA)(const INumberVectorProperty *, const char *, va_list);
extern void (*WeakIDSetSwitchVA)(const ISwitchVectorProperty *, const char *, va_list);
extern void (*WeakIDDefSwitchVA)(const ISwitchVectorProperty *, const char *, va_list);
extern void (*WeakIDSetLightVA)(const ILightVectorProperty *, const char *, va_list);
extern void (*WeakIDDefLightVA)(const ILightVectorProperty *, const char *, va_list);
extern void (*WeakIDSetBLOBVA)(const IBLOBVectorProperty *, const char *, va_list);
extern void (*WeakIDDefBLOBVA)(const IBLOBVectorProperty *, const char *, va_list);
extern int (*WeakIUUpdateText)(ITextVectorProperty *, char *[], char *[], int);
extern int (*WeakIUUpdateNumber)(INumberVectorProperty *, double[], char *[], int n);
extern int (*WeakIUUpdateSwitch)(ISwitchVectorProperty *, ISState *, char *[], int n);
extern int (*WeakIUUpdateBLOB)(IBLOBVectorProperty *, int [], int [], char *[], char *[], char *[], int n);
extern void (*WeakIUUpdateMinMax)(const INumberVectorProperty *);

static struct WeakIDLoader
{
    WeakIDLoader()
    {
        WeakIDSetTextVA = IDSetTextVA;
        WeakIDDefTextVA = IDDefTextVA;
        WeakIDSetNumberVA = IDSetNumberVA;
        WeakIDDefNumberVA = IDDefNumberVA;
        WeakIDSetSwitchVA = IDSetSwitchVA;
        WeakIDDefSwitchVA = IDDefSwitchVA;
        WeakIDSetLightVA = IDSetLightVA;
        WeakIDDefLightVA = IDDefLightVA;
        WeakIDSetBLOBVA = IDSetBLOBVA;
        WeakIDDefBLOBVA = IDDefBLOBVA;
        WeakIUUpdateText = IUUpdateText;
        WeakIUUpdateNumber = IUUpdateNumber;
        WeakIUUpdateSwitch = IUUpdateSwitch;
        WeakIUUpdateBLOB = IUUpdateBLOB;
        WeakIUUpdateMinMax = IUUpdateMinMax;
    }
} weakLoader;

void timerfunc(void *t)
{
    INDI::DefaultDevice *devPtr = static_cast<INDI::DefaultDevice *>(t);
    if (devPtr != nullptr)
    {
        //  this was for my device
        //  but we dont have a way of telling
        //  WHICH timer was hit :(
        devPtr->TimerHit();
    }
    return;
}


namespace INDI
{

DefaultDevicePrivate::DefaultDevicePrivate(DefaultDevice *defaultDevice)
    : defaultDevice(defaultDevice)
{
    const std::unique_lock<std::recursive_mutex> lock(DefaultDevicePrivate::devicesLock);
    devices.push_back(this);
}

DefaultDevicePrivate::~DefaultDevicePrivate()
{
    const std::unique_lock<std::recursive_mutex> lock(DefaultDevicePrivate::devicesLock);
    devices.remove(this);
}

DefaultDevice::DefaultDevice()
    : ParentDevice(std::shared_ptr<ParentDevicePrivate>(new DefaultDevicePrivate(this)))
{
    D_PTR(DefaultDevice);
    d->m_MainLoopTimer.setSingleShot(true);
    d->m_MainLoopTimer.setInterval(getPollingPeriod());
    d->m_MainLoopTimer.callOnTimeout(std::bind(&DefaultDevice::TimerHit, this));
}

bool DefaultDevice::loadConfig(INDI::Property &property)
{
    return loadConfig(true, property.getName());
}

bool DefaultDevice::loadConfig(bool silent, const char *property)
{
    D_PTR(DefaultDevice);
    char errmsg[MAXRBUF] = {0};
    d->isConfigLoading = true;
    bool pResult = IUReadConfig(nullptr, getDeviceName(), property, silent ? 1 : 0, errmsg) == 0 ? true : false;
    d->isConfigLoading = false;

    if (!silent)
    {
        if (pResult)
        {
            LOG_DEBUG("Configuration successfully loaded.");
        }
        else
            LOG_INFO("No previous configuration found. To save driver configuration, click Save Configuration in Options tab.");
    }

    // Determine default config file name
    // Need to be done only once per device.
    if (d->isDefaultConfigLoaded == false)
    {
        d->isDefaultConfigLoaded = IUSaveDefaultConfig(nullptr, nullptr, getDeviceName()) == 0;
    }

    return pResult;
}

bool DefaultDevice::saveConfigItems(FILE *fp)
{
    D_PTR(DefaultDevice);
    d->DebugSP.save(fp);
    d->PollPeriodNP.save(fp);
    if (!d->ConnectionModeSP.isEmpty())
        d->ConnectionModeSP.save(fp);

    if (d->activeConnection != nullptr)
        d->activeConnection->saveConfigItems(fp);

    return INDI::Logger::saveConfigItems(fp);
}

bool DefaultDevice::saveAllConfigItems(FILE *fp)
{
    for (const auto &oneProperty : getProperties())
    {
        if (oneProperty.getType() == INDI_SWITCH)
        {
            const auto &svp = oneProperty.getSwitch();
            /* Never save CONNECTION property. Don't save switches with no switches on if the rule is one of many */
            if (
                (svp->isNameMatch(INDI::SP::CONNECTION)) ||
                (svp->getRule() == ISR_1OFMANY && svp->findOnSwitch() == nullptr)
            )
                continue;
        }
        oneProperty.save(fp);
    }
    return true;
}

bool DefaultDevice::purgeConfig()
{
    char errmsg[MAXRBUF];
    if (IUPurgeConfig(nullptr, getDeviceName(), errmsg) == -1)
    {
        LOGF_WARN("%s", errmsg);
        return false;
    }

    LOG_INFO("Configuration file successfully purged.");
    return true;
}

bool DefaultDevice::saveConfig(INDI::Property &property)
{
    return saveConfig(true, property.getName());
}

bool DefaultDevice::saveConfig(bool silent, const char *property)
{
    D_PTR(DefaultDevice);
    if (d->isConfigLoading)
        return false;
    silent = false;
    char errmsg[MAXRBUF] = {0};

    FILE *fp = nullptr;

    if (property == nullptr)
    {
        fp = IUGetConfigFP(nullptr, getDeviceName(), "w", errmsg);

        if (fp == nullptr)
        {
            if (!silent)
                LOGF_WARN("Failed to save configuration. %s", errmsg);
            return false;
        }

        IUSaveConfigTag(fp, 0, getDeviceName(), silent ? 1 : 0);

        saveConfigItems(fp);

        IUSaveConfigTag(fp, 1, getDeviceName(), silent ? 1 : 0);

        fflush(fp);
        fclose(fp);

        if (d->isDefaultConfigLoaded == false)
        {
            d->isDefaultConfigLoaded = IUSaveDefaultConfig(nullptr, nullptr, getDeviceName()) == 0;
        }

        LOG_DEBUG("Configuration successfully saved.");
    }
    else
    {
        fp = IUGetConfigFP(nullptr, getDeviceName(), "r", errmsg);

        if (fp == nullptr)
        {
            //if (!silent)
            //   LOGF_ERROR("Error saving configuration. %s", errmsg);
            //return false;
            // If we don't have an existing file pointer, save all properties.
            return saveConfig(silent);
        }

        LilXML *lp   = newLilXML();
        XMLEle *root = readXMLFile(fp, lp, errmsg);

        fclose(fp);
        delLilXML(lp);

        if (root == nullptr)
            return false;

        XMLEle *ep         = nullptr;
        bool propertySaved = false;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char *elemName = findXMLAttValu(ep, "name");
            const char *tagName  = tagXMLEle(ep);

            if (strcmp(elemName, property))
                continue;

            if (!strcmp(tagName, "newSwitchVector"))
            {
                auto svp = getSwitch(elemName);
                if (!svp)
                {
                    delXMLEle(root);
                    return false;
                }

                XMLEle *sw = nullptr;
                for (sw = nextXMLEle(ep, 1); sw != nullptr; sw = nextXMLEle(ep, 0))
                {
                    auto oneSwitch = svp.findWidgetByName(findXMLAttValu(sw, "name"));
                    if (!oneSwitch)
                    {
                        delXMLEle(root);
                        return false;
                    }
                    char formatString[MAXRBUF];
                    snprintf(formatString, MAXRBUF, "      %s\n", oneSwitch->getStateAsString());
                    editXMLEle(sw, formatString);
                }

                propertySaved = true;
                break;
            }
            else if (!strcmp(tagName, "newNumberVector"))
            {
                auto nvp = getNumber(elemName);
                if (!nvp)
                {
                    delXMLEle(root);
                    return false;
                }

                XMLEle *np = nullptr;
                for (np = nextXMLEle(ep, 1); np != nullptr; np = nextXMLEle(ep, 0))
                {
                    auto oneNumber = nvp.findWidgetByName(findXMLAttValu(np, "name"));
                    if (!oneNumber)
                        return false;

                    char formatString[MAXRBUF];
                    snprintf(formatString, MAXRBUF, "      %.20g\n", oneNumber->getValue());
                    editXMLEle(np, formatString);
                }

                propertySaved = true;
                break;
            }
            else if (!strcmp(tagName, "newTextVector"))
            {
                auto tvp = getText(elemName);
                if (!tvp)
                {
                    delXMLEle(root);
                    return false;
                }

                XMLEle *tp = nullptr;
                for (tp = nextXMLEle(ep, 1); tp != nullptr; tp = nextXMLEle(ep, 0))
                {
                    auto oneText = tvp.findWidgetByName(findXMLAttValu(tp, "name"));
                    if (!oneText)
                        return false;

                    char formatString[MAXRBUF];
                    snprintf(formatString, MAXRBUF, "      %s\n", oneText->getText() ? oneText->getText() : "");
                    editXMLEle(tp, formatString);
                }

                propertySaved = true;
                break;
            }
        }

        if (propertySaved)
        {
            fp = IUGetConfigFP(nullptr, getDeviceName(), "w", errmsg);
            prXMLEle(fp, root, 0);
            fflush(fp);
            fclose(fp);
            delXMLEle(root);
            LOGF_DEBUG("Configuration successfully saved for %s.", property);
            return true;
        }
        else
        {
            delXMLEle(root);
            // If property does not exist, save the whole thing
            return saveConfig(silent);
        }
    }

    return true;
}

bool DefaultDevice::loadDefaultConfig()
{
    char configDefaultFileName[MAXRBUF];
    char errmsg[MAXRBUF];
    bool pResult = false;

    if (getenv("INDICONFIG"))
        snprintf(configDefaultFileName, MAXRBUF, "%s.default", getenv("INDICONFIG"));
    else
        snprintf(configDefaultFileName, MAXRBUF, "%s/.indi/%s_config.xml.default", getenv("HOME"), getDeviceName());

    LOGF_DEBUG("Requesting to load default config with: %s", configDefaultFileName);

    pResult = IUReadConfig(configDefaultFileName, getDeviceName(), nullptr, 0, errmsg) == 0 ? true : false;

    if (pResult)
        LOG_INFO("Default configuration loaded.");
    else
        LOGF_INFO("Error loading default configuration. %s", errmsg);

    return pResult;
}

bool DefaultDevice::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    D_PTR(DefaultDevice);
    // ignore if not ours //
    if (strcmp(dev, getDeviceName()))
        return false;

    INDI::PropertySwitch property = getProperty(name, INDI_SWITCH);

    if (!property.isValid())
        return false;

    // #PS: TODO Remove
    ////////////////////////////////////////////////////
    // Debugging and Logging Levels
    ////////////////////////////////////////////////////
    if (
        property.isNameMatch("DEBUG_LEVEL") ||
        property.isNameMatch("LOGGING_LEVEL") ||
        property.isNameMatch("LOG_OUTPUT"))
    {
        bool rc = Logger::ISNewSwitch(dev, name, states, names, n);

        if (property.isNameMatch("LOG_OUTPUT"))
        {
            auto sw = property.findWidgetByName("FILE_DEBUG");
            if (sw != nullptr && sw->getState() == ISS_ON)
                DEBUGF(Logger::DBG_SESSION, "Session log file %s", Logger::getLogFile().c_str());
        }

        return rc;
    }

    property.update(states, names, n); // update and callback
    if (property.hasUpdateCallback())
        return true;

    bool rc = false;
    for (Connection::Interface *oneConnection : d->connections)
        rc |= oneConnection->ISNewSwitch(dev, name, states, names, n);

    return rc;
}

bool DefaultDevice::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    D_PTR(DefaultDevice);

    INDI::PropertyNumber property = getProperty(name, INDI_NUMBER);

    if (!property.isValid())
        return false;

    property.update(values, names, n); // update and callback
    if (property.hasUpdateCallback())
        return true;

    for (Connection::Interface *oneConnection : d->connections)
        oneConnection->ISNewNumber(dev, name, values, names, n);

    return false;
}

bool DefaultDevice::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    D_PTR(DefaultDevice);

    INDI::PropertyText property = getProperty(name, INDI_TEXT);

    if (!property.isValid())
        return false;

    property.update(texts, names, n); // update and callback
    if (property.hasUpdateCallback())
        return true;

    for (Connection::Interface *oneConnection : d->connections)
        oneConnection->ISNewText(dev, name, texts, names, n);

    return false;
}

bool DefaultDevice::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                              char *formats[], char *names[], int n)
{
    D_PTR(DefaultDevice);

    INDI::PropertyBlob property = getProperty(name, INDI_BLOB);

    if (!property.isValid())
        return false;

    property.update(sizes, blobsizes, blobs, formats, names, n); // update and callback
    if (property.hasUpdateCallback())
        return true;

    for (Connection::Interface *oneConnection : d->connections)
        oneConnection->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);

    return false;
}

bool DefaultDevice::ISSnoopDevice(XMLEle *root)
{
    D_PTR(DefaultDevice);
    char errmsg[MAXRBUF];
    return d->watchDevice.processXml(INDI::LilXmlElement(root), errmsg) < 0;
}

void DefaultDevice::watchDevice(const char *name, const std::function<void (BaseDevice)> &callback)
{
    D_PTR(DefaultDevice);
    d->watchDevice.watchDevice(name, callback);
    IDSnoopDevice(name, nullptr);
}

void DefaultDevice::addDebugControl()
{
    D_PTR(DefaultDevice);
    registerProperty(d->DebugSP);
    d->isDebug = false;
}

void DefaultDevice::addSimulationControl()
{
    D_PTR(DefaultDevice);
    registerProperty(d->SimulationSP);
    d->isSimulation = false;
}

void DefaultDevice::addConfigurationControl()
{
    D_PTR(DefaultDevice);
    registerProperty(d->ConfigProcessSP);
}

void DefaultDevice::addPollPeriodControl()
{
    D_PTR(DefaultDevice);
    registerProperty(d->PollPeriodNP);
}

void DefaultDevice::addAuxControls()
{
    addDebugControl();
    addSimulationControl();
    addConfigurationControl();
    addPollPeriodControl();
}

void DefaultDevice::setDebug(bool enable)
{
    D_PTR(DefaultDevice);
    if (d->isDebug == enable)
    {
        d->DebugSP.setState(IPS_OK);
        d->DebugSP.apply();
        return;
    }

    d->DebugSP.reset();

    auto sp = d->DebugSP.findWidgetByName(enable ? "ENABLE" : "DISABLE");
    if (sp)
    {
        sp->setState(ISS_ON);
        LOGF_INFO("Debug is %s.", enable ? "enabled" : "disabled");
    }

    d->isDebug = enable;

    // Inform logger
    if (Logger::updateProperties(enable) == false)
        DEBUG(Logger::DBG_WARNING, "setLogDebug: Logger error");

    debugTriggered(enable);
    d->DebugSP.setState(IPS_OK);
    d->DebugSP.apply();
}

void DefaultDevice::setSimulation(bool enable)
{
    D_PTR(DefaultDevice);
    if (d->isSimulation == enable)
    {
        d->SimulationSP.setState(IPS_OK);
        d->SimulationSP.apply();
        return;
    }

    d->SimulationSP.reset();

    auto sp = d->SimulationSP.findWidgetByName(enable ? "ENABLE" : "DISABLE");
    if (sp)
    {
        LOGF_INFO("Simulation is %s.", enable ? "enabled" : "disabled");
        sp->setState(ISS_ON);
    }

    d->isSimulation = enable;
    simulationTriggered(enable);
    d->SimulationSP.setState(IPS_OK);
    d->SimulationSP.apply();
}

bool DefaultDevice::isDebug() const
{
    D_PTR(const DefaultDevice);
    return d->isDebug;
}

bool DefaultDevice::isSimulation() const
{
    D_PTR(const DefaultDevice);
    return d->isSimulation;
}

void DefaultDevice::debugTriggered(bool enable)
{
    INDI_UNUSED(enable);
}

void DefaultDevice::simulationTriggered(bool enable)
{
    INDI_UNUSED(enable);
}

void DefaultDevice::ISGetProperties(const char *dev)
{
    D_PTR(DefaultDevice);
    if (d->isInit == false)
    {
        if (dev != nullptr)
            setDeviceName(dev);
        else if (*getDeviceName() == '\0')
        {
            char *envDev = getenv("INDIDEV");
            if (envDev != nullptr)
                setDeviceName(envDev);
            else
                setDeviceName(getDefaultName());
        }

        d->ConnectionSP.setDeviceName(getDeviceName());
        initProperties();
        addConfigurationControl();

        // If we have no connections, move Driver Info to General Info tab
        if (d->connections.size() == 0)
            d->DriverInfoTP.setGroupName(INFO_TAB);
    }

    for (const auto &oneProperty : *getProperties())
    {
        if (d->defineDynamicProperties == false && oneProperty.isDynamic())
            continue;

        oneProperty.define();
    }

    // Remember debug & logging settings
    if (d->isInit == false)
    {
        loadConfig(true, "DEBUG");
        loadConfig(true, "DEBUG_LEVEL");
        loadConfig(true, "LOGGING_LEVEL");
        loadConfig(true, "POLLING_PERIOD");
        loadConfig(true, "LOG_OUTPUT");
    }

    if (d->ConnectionModeSP.isEmpty())
    {
        if (d->connections.size() > 0)
        {
            d->ConnectionModeSP.resize(d->connections.size());
            auto sp     = &d->ConnectionModeSP[0];
            for (Connection::Interface *oneConnection : d->connections)
            {
                (sp++)->fill(oneConnection->name(), oneConnection->label(), ISS_OFF);
            }
            d->ConnectionModeSP.fill(getDeviceName(), "CONNECTION_MODE", "Connection Mode", CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60,
                                     IPS_IDLE);

            // Try to read config first
            if (IUGetConfigOnSwitchIndex(getDeviceName(), d->ConnectionModeSP.getName(), &d->m_ConfigConnectionMode) == 0)
            {
                d->ConnectionModeSP[d->m_ConfigConnectionMode].setState(ISS_ON);
                d->activeConnection = d->connections[d->m_ConfigConnectionMode];
            }
            // Check if we already have an active connection set.
            else if (d->activeConnection != nullptr)
            {
                auto it = std::find(d->connections.begin(), d->connections.end(), d->activeConnection);
                if (it != d->connections.end())
                {
                    int index = std::distance(d->connections.begin(), it);
                    if (index >= 0)
                        d->ConnectionModeSP[index].setState(ISS_ON);
                }
            }
            // Otherwise use connection 0
            else
            {
                d->ConnectionModeSP[0].setState(ISS_ON);
                d->activeConnection = d->connections[0];
            }

            defineProperty(d->ConnectionModeSP);
            d->activeConnection->Activated();
        }
    }

    d->isInit = true;
}

void DefaultDevice::resetProperties()
{
    for (auto &oneProperty : getProperties())
    {
        oneProperty.setState(IPS_IDLE);
        oneProperty.apply();
    }
}

void DefaultDevice::setConnected(bool status, IPState state, const char *msg)
{
    auto svp = getSwitch(INDI::SP::CONNECTION);
    if (!svp)
        return;

    svp[INDI_ENABLED].setState(status ? ISS_ON : ISS_OFF);
    svp[INDI_DISABLED].setState(status ? ISS_OFF : ISS_ON);
    svp.setState(state);

    if (msg == nullptr)
        svp.apply();
    else
        svp.apply("%s", msg);
}

// Set the timeout for the TimerHit function.
// This is a single shot timer.
int DefaultDevice::SetTimer(uint32_t ms)
{
    D_PTR(DefaultDevice);
    d->m_MainLoopTimer.start(ms);
    return 1;
}

// Remove main timer. ID is not used.
// Kept for backward compatibility
void DefaultDevice::RemoveTimer(int id)
{
    INDI_UNUSED(id);
    D_PTR(DefaultDevice);
    d->m_MainLoopTimer.stop();
    return;
}

//  This is just a placeholder
//  This function should be overridden by child classes if they use timers
//  So we should never get here
void DefaultDevice::TimerHit()
{
    return;
}

bool DefaultDevice::updateProperties()
{
    //  The base device has no properties to update
    return true;
}

// Early access to properties.
// The `BaseDevice::getDriverInterface()` function will return a false value of 0
// when the `DriverInfoTP` property has not yet been registered.
//
// The situation occurs in the case of drivers where the class that inherits from `DefaultDevice`
// will call the `setDriverInterface` function in the constructor

uint32_t DefaultDevice::getDriverInterface() const
{
    D_PTR(const DefaultDevice);
    return atoi(d->DriverInfoTP[3 /* DRIVER_INTERFACE */].getText());
}

void DefaultDevice::setDriverInterface(uint32_t value)
{
    D_PTR(DefaultDevice);
    d->DriverInfoTP[3 /* DRIVER_INTERFACE */].setText(std::to_string(value));
}

void DefaultDevice::syncDriverInfo()
{
    D_PTR(DefaultDevice);
    d->DriverInfoTP.apply();
}

bool DefaultDevice::initProperties()
{
    D_PTR(DefaultDevice);
    char versionStr[16];
    char interfaceStr[16];

    snprintf(versionStr, 16, "%d.%d", d->majorVersion, d->minorVersion);
    snprintf(interfaceStr, 16, "%d", getDriverInterface());

    // Connection Mode
    d->ConnectionModeSP.onUpdate([d]()
    {
        int activeConnectionMode = d->ConnectionModeSP.findOnSwitchIndex();

        if (activeConnectionMode >= 0 && activeConnectionMode < static_cast<int>(d->connections.size()))
        {
            d->activeConnection = d->connections[activeConnectionMode];
            d->activeConnection->Activated();

            for (Connection::Interface *oneConnection : d->connections)
            {
                if (oneConnection == d->activeConnection)
                    continue;

                oneConnection->Deactivated();
            }

            d->ConnectionModeSP.setState(IPS_OK);
        }
        else
            d->ConnectionModeSP.setState(IPS_ALERT);

        d->ConnectionModeSP.apply();

    });

    // Connection
    d->ConnectionSP[INDI_ENABLED ].fill("CONNECT",    "Connect",    ISS_OFF);
    d->ConnectionSP[INDI_DISABLED].fill("DISCONNECT", "Disconnect", ISS_ON);
    d->ConnectionSP.fill(getDeviceName(), INDI::SP::CONNECTION, "Connection", "Main Control", IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    d->ConnectionSP.onNewValues([this](const INDI::PropertySwitch::NewValues & values)
    {
        if (values.contains("CONNECT", ISS_ON))
        {
            // If disconnected, try to connect.
            if (isConnected() == false)
            {
                if (Connect())
                {
                    // Connection is successful, set it to OK and updateProperties.
                    setConnected(true);
                    updateProperties();
                }
                else
                    setConnected(false, IPS_ALERT);
            }
            else
                // Already connected, tell client we're connected already.
                setConnected(true);
        }

        if (values.contains("DISCONNECT", ISS_ON))
        {
            // If connected, try to disconnect.
            if (isConnected() == true)
            {
                // Disconnection is successful, set it IDLE and updateProperties.
                if (Disconnect())
                {
                    setConnected(false, IPS_IDLE);
                    updateProperties();
                }
                else
                    setConnected(true, IPS_ALERT);
            }
            // Already disconnected, tell client we're disconnected already.
            else
                setConnected(false, IPS_IDLE);
        }

    });
    registerProperty(d->ConnectionSP);

    d->DriverInfoTP[0].fill("DRIVER_NAME", "Name", getDriverName());
    d->DriverInfoTP[1].fill("DRIVER_EXEC", "Exec", getDriverExec());
    d->DriverInfoTP[2].fill("DRIVER_VERSION", "Version", versionStr);
    d->DriverInfoTP[3].fill("DRIVER_INTERFACE", "Interface", interfaceStr);
    d->DriverInfoTP.fill(getDeviceName(), "DRIVER_INFO", "Driver Info", CONNECTION_TAB, IP_RO, 60, IPS_IDLE);
    registerProperty(d->DriverInfoTP);

    // Debug
    d->DebugSP[INDI_ENABLED ].fill("ENABLE",  "Enable",  ISS_OFF);
    d->DebugSP[INDI_DISABLED].fill("DISABLE", "Disable", ISS_ON);
    d->DebugSP.fill(getDeviceName(), "DEBUG", "Debug", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    d->DebugSP.onUpdate([this, d]()
    {
        auto sp = d->DebugSP.findOnSwitch();
        assert(sp != nullptr);
        setDebug(sp->isNameMatch("ENABLE") ? true : false);
    });

    // Simulation
    d->SimulationSP[INDI_ENABLED ].fill("ENABLE",  "Enable",  ISS_OFF);
    d->SimulationSP[INDI_DISABLED].fill("DISABLE", "Disable", ISS_ON);
    d->SimulationSP.fill(getDeviceName(), "SIMULATION", "Simulation", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    d->SimulationSP.onUpdate([this, d]()
    {
        auto sp = d->SimulationSP.findOnSwitch();
        assert(sp != nullptr);
        setSimulation(sp->isNameMatch("ENABLE") ? true : false);
    });

    // Configuration
    d->ConfigProcessSP[0].fill("CONFIG_LOAD",    "Load",    ISS_OFF);
    d->ConfigProcessSP[1].fill("CONFIG_SAVE",    "Save",    ISS_OFF);
    d->ConfigProcessSP[2].fill("CONFIG_DEFAULT", "Default", ISS_OFF);
    d->ConfigProcessSP[3].fill("CONFIG_PURGE",   "Purge",   ISS_OFF);
    d->ConfigProcessSP.fill(getDeviceName(), "CONFIG_PROCESS", "Configuration", "Options", IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
    d->ConfigProcessSP.onUpdate([this, d]()
    {
        auto svp = d->ConfigProcessSP;
        auto sp = svp.findOnSwitch();
        svp.reset();
        bool pResult = false;

        // Not suppose to happen (all switches off) but let's handle it anyway
        if (sp == nullptr)
        {
            svp.setState(IPS_IDLE);
            svp.apply();
            return;
        }

        if (sp->isNameMatch("CONFIG_LOAD"))
            pResult = loadConfig();
        else if (sp->isNameMatch("CONFIG_SAVE"))
            pResult = saveConfig();
        else if (sp->isNameMatch("CONFIG_DEFAULT"))
            pResult = loadDefaultConfig();
        else if (sp->isNameMatch("CONFIG_PURGE"))
            pResult = purgeConfig();

        svp.setState(pResult ? IPS_OK : IPS_ALERT);
        svp.apply();
    });

    // Polling Period
    d->PollPeriodNP[0].fill("PERIOD_MS", "Period (ms)", "%.f", 10, 600000, 1000, d->pollingPeriod);
    d->PollPeriodNP.fill(getDeviceName(), "POLLING_PERIOD", "Polling", "Options", IP_RW, 0, IPS_IDLE);
    d->PollPeriodNP.onUpdate([d]()
    {
        d->PollPeriodNP.setState(IPS_OK);
        d->pollingPeriod = static_cast<uint32_t>(d->PollPeriodNP[0].getValue());
        d->PollPeriodNP.apply();
    });

    INDI::Logger::initProperties(this);

    // Ready the logger
    std::string logFile = getDriverExec();

    DEBUG_CONF(logFile, Logger::file_off | Logger::screen_on, Logger::defaultlevel, Logger::defaultlevel)

    return true;
}

bool DefaultDevice::deleteProperty(INDI::Property &property)
{
    return deleteProperty(property.getName());
}

bool DefaultDevice::deleteProperty(const char *propertyName)
{
    D_PTR(DefaultDevice);
    char errmsg[MAXRBUF];

    if (propertyName == nullptr)
    {
        //while(!pAll.empty()) delete bar.back(), bar.pop_back();
        IDDelete(getDeviceName(), nullptr, nullptr);
        return true;
    }

    // Keep dynamic properties in existing property list so they can be reused
    if (d->deleteDynamicProperties == false)
    {
        INDI::Property prop = getProperty(propertyName);
        if (prop && prop.isDynamic())
        {
            IDDelete(getDeviceName(), propertyName, nullptr);
            return true;
        }
    }

    if (removeProperty(propertyName, errmsg) == 0)
    {
        IDDelete(getDeviceName(), propertyName, nullptr);
        return true;
    }
    else
        return false;
}

void DefaultDevice::defineProperty(INumberVectorProperty *property)
{
    registerProperty(INDI::Property(property));
    static_cast<PropertyViewNumber*>(property)->define();
}

void DefaultDevice::defineProperty(ITextVectorProperty *property)
{
    registerProperty(INDI::Property(property));
    static_cast<PropertyViewText*>(property)->define();
}

void DefaultDevice::defineProperty(ISwitchVectorProperty *property)
{
    registerProperty(INDI::Property(property));
    static_cast<PropertyViewSwitch*>(property)->define();
}

void DefaultDevice::defineProperty(ILightVectorProperty *property)
{
    registerProperty(INDI::Property(property));
    static_cast<PropertyViewLight*>(property)->define();
}

void DefaultDevice::defineProperty(IBLOBVectorProperty *property)
{
    registerProperty(INDI::Property(property));
    static_cast<PropertyViewBlob*>(property)->define();
}

void DefaultDevice::defineProperty(INDI::Property &property)
{
    registerProperty(property);
    property.define();
}

void DefaultDevice::defineNumber(INumberVectorProperty *nvp)
{
    defineProperty(nvp);
}

void DefaultDevice::defineText(ITextVectorProperty *tvp)
{
    defineProperty(tvp);
}

void DefaultDevice::defineSwitch(ISwitchVectorProperty *svp)
{
    defineProperty(svp);
}

void DefaultDevice::defineLight(ILightVectorProperty *lvp)
{
    defineProperty(lvp);
}

void DefaultDevice::defineBLOB(IBLOBVectorProperty *bvp)
{
    defineProperty(bvp);
}

bool DefaultDevice::Connect()
{
    D_PTR(DefaultDevice);
    if (isConnected())
        return true;

    if (d->activeConnection == nullptr)
    {
        LOG_ERROR("No active connection defined.");
        return false;
    }

    bool rc = d->activeConnection->Connect();

    if (rc)
    {
        if (d->ConnectionModeSP.findOnSwitchIndex() != d->m_ConfigConnectionMode)
            saveConfig(true, d->ConnectionModeSP.getName());
        if (d->pollingPeriod > 0)
            SetTimer(d->pollingPeriod);
    }

    return rc;
}

bool DefaultDevice::Disconnect()
{
    D_PTR(DefaultDevice);
    if (isSimulation())
    {
        DEBUGF(Logger::DBG_SESSION, "%s is offline.", getDeviceName());
        return true;
    }

    if (d->activeConnection)
    {
        bool rc = d->activeConnection->Disconnect();
        if (rc)
        {
            DEBUGF(Logger::DBG_SESSION, "%s is offline.", getDeviceName());
            return true;
        }
        else
            return false;
    }

    return false;
}

void DefaultDevice::registerConnection(Connection::Interface *newConnection)
{
    D_PTR(DefaultDevice);
    d->connections.push_back(newConnection);
}

bool DefaultDevice::unRegisterConnection(Connection::Interface *existingConnection)
{
    D_PTR(DefaultDevice);

    auto i = std::begin(d->connections);

    while (i != std::end(d->connections))
    {
        if (*i == existingConnection)
        {
            i = d->connections.erase(i);
            return true;
        }
        else
            ++i;
    }

    return false;
}

void DefaultDevice::setCurrentPollingPeriod(uint32_t msec)
{
    D_PTR(DefaultDevice);
    d->pollingPeriod = msec;
}

uint32_t DefaultDevice::getCurrentPollingPeriod() const
{
    D_PTR(const DefaultDevice);
    return d->pollingPeriod;
}

uint32_t &DefaultDevice::refCurrentPollingPeriod()
{
    D_PTR(DefaultDevice);
    return d->pollingPeriod;
}

uint32_t DefaultDevice::refCurrentPollingPeriod() const
{
    D_PTR(const DefaultDevice);
    return d->pollingPeriod;
}

void DefaultDevice::setDefaultPollingPeriod(uint32_t msec)
{
    D_PTR(DefaultDevice);
    d->PollPeriodNP[0].setValue(msec);
    d->pollingPeriod = msec;
}

void DefaultDevice::setPollingPeriodRange(uint32_t minimum, uint32_t maximum)
{
    D_PTR(DefaultDevice);

    d->PollPeriodNP[0].setMinMax(minimum, maximum);
    d->PollPeriodNP.updateMinMax();
}

void DefaultDevice::setActiveConnection(Connection::Interface *existingConnection)
{
    D_PTR(DefaultDevice);

    if (existingConnection == d->activeConnection)
        return;

    for (Connection::Interface *oneConnection : d->connections)
    {
        if (oneConnection == d->activeConnection)
        {
            oneConnection->Deactivated();
            break;
        }
    }

    d->activeConnection = existingConnection;
    if (!d->ConnectionModeSP.isEmpty())
    {
        auto it = std::find(d->connections.begin(), d->connections.end(), d->activeConnection);
        if (it != d->connections.end())
        {
            int index = std::distance(d->connections.begin(), it);
            if (index >= 0)
            {
                d->ConnectionModeSP.reset();
                d->ConnectionModeSP[index].setState(ISS_ON);
                d->ConnectionModeSP.setState(IPS_OK);
                // If property is registerned then send back response to client
                INDI::Property connectionProperty = getProperty(d->ConnectionModeSP.getName(), INDI_SWITCH);
                if (connectionProperty && connectionProperty.getRegistered())
                    d->ConnectionModeSP.apply();
            }
        }
    }
}

const char *DefaultDevice::getDriverExec()
{
    return me;
}

const char *DefaultDevice::getDriverName()
{
    return getDefaultName();
}

void DefaultDevice::setVersion(uint16_t vMajor, uint16_t vMinor)
{
    D_PTR(DefaultDevice);
    d->majorVersion = vMajor;
    d->minorVersion = vMinor;
}

uint16_t DefaultDevice::getMajorVersion() const
{
    D_PTR(const DefaultDevice);
    return d->majorVersion;
}

uint16_t DefaultDevice::getMinorVersion() const
{
    D_PTR(const DefaultDevice);
    return d->minorVersion;
}

void DefaultDevice::setDynamicPropertiesBehavior(bool defineEnabled, bool deleteEnabled)
{
    D_PTR(DefaultDevice);
    d->defineDynamicProperties = defineEnabled;
    d->deleteDynamicProperties = deleteEnabled;
}

Connection::Interface *DefaultDevice::getActiveConnection()
{
    D_PTR(DefaultDevice);
    return d->activeConnection;
}

uint32_t DefaultDevice::getPollingPeriod() const
{
    D_PTR(const DefaultDevice);
    return static_cast<uint32_t>(d->PollPeriodNP[0].getValue());
}

bool DefaultDevice::isConfigLoading() const
{
    D_PTR(const DefaultDevice);
    return d->isConfigLoading;
}

bool DefaultDevice::isInitializationComplete() const
{
    D_PTR(const DefaultDevice);
    return d->isInit;
}

bool DefaultDevice::ISNewProperty(INDI::Property &property, const std::string &elementName, const std::any &value)
{
    if (!property.isValid() || elementName.empty())
    {
        LOGF_WARN("ISNewProperty: Invalid property or empty element name for device '%s'.", getDeviceName());
        return false;
    }

    char mutableElementName[MAXINDINAME]; // Use MAXINDINAME from indicom.h
    strncpy(mutableElementName, elementName.c_str(), sizeof(mutableElementName) - 1);
    mutableElementName[sizeof(mutableElementName) - 1] = '\0'; // Ensure null termination
    char *names[] = { mutableElementName };

    switch (property.getType())
    {
        case INDI_SWITCH:
        {
            try
            {
                ISState newState = std::any_cast<ISState>(value);
                ISState states[] = { newState };
                return ISNewSwitch(getDeviceName(), property.getName(), states, names, 1);
            }
            catch (const std::bad_any_cast &e)
            {
                LOGF_ERROR("ISNewProperty: Type mismatch for Switch property '%s'. Expected ISState. Got: %s",
                           property.getName(), e.what());
                return false;
            }
        }
        case INDI_NUMBER:
        {
            try
            {
                double newValue = 0;
                if (value.type() == typeid(int))
                {
                    newValue = static_cast<double>(std::any_cast<int>(value));
                }
                else if (value.type() == typeid(long))
                {
                    newValue = static_cast<double>(std::any_cast<long>(value));
                }
                else if (value.type() == typeid(double))
                {
                    newValue = std::any_cast<double>(value);
                }
                else
                {
                    throw std::bad_any_cast();
                }

                double values[] = { newValue };
                return ISNewNumber(getDeviceName(), property.getName(), values, names, 1);
            }
            catch (const std::bad_any_cast &e)
            {
                LOGF_ERROR("ISNewProperty: Type mismatch for Number property '%s'. Expected double, int, or long. Got: %s",
                           property.getName(), e.what());
                return false;
            }
        }
        case INDI_TEXT:
        {
            try
            {
                char mutableTextValue[1024]; // Use 1024 for text buffer
                memset(mutableTextValue, 0, sizeof(mutableTextValue));

                if (value.type() == typeid(const char *))
                {
                    const char *text = std::any_cast<const char *>(value);
                    if (text) strncpy(mutableTextValue, text, sizeof(mutableTextValue) - 1);
                }
                else if (value.type() == typeid(std::string))
                {
                    const std::string &text = std::any_cast<std::string>(value);
                    strncpy(mutableTextValue, text.c_str(), sizeof(mutableTextValue) - 1);
                }
                else
                {
                    throw std::bad_any_cast();
                }
                mutableTextValue[sizeof(mutableTextValue) - 1] = '\0'; // Ensure null termination

                char *texts[] = { mutableTextValue };
                return ISNewText(getDeviceName(), property.getName(), texts, names, 1);
            }
            catch (const std::bad_any_cast &e)
            {
                LOGF_ERROR("ISNewProperty: Type mismatch for Text property '%s'. Expected const char* or std::string. Got: %s",
                           property.getName(), e.what());
                return false;
            }
        }
        default:
            LOGF_WARN("ISNewProperty: Unsupported property type for property '%s'.", property.getName());
            return false;
    }
    return false;
}

}
