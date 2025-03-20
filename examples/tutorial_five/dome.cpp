/*
   INDI Developers Manual
   Tutorial #5 - Snooping

   Dome

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file dome.cpp
    \brief Construct a dome device that the user may operate to open or close the dome shutter door. This driver is \e snooping on the Rain Detector rain property status.
    If rain property state is alert, we close the dome shutter door if it is open, and we prevent the user from opening it until the rain threat passes.
    \author Jasem Mutlaq

    \example dome.cpp
    The dome driver \e snoops on the rain detector signal and watches whether rain is detected or not. If it is detector and the dome is closed, it performs
    no action, but it also prevents you from opening the dome due to rain. If the dome is open, it will automatically starts closing the shutter. In order
    snooping to work, both drivers must be started by the same indiserver (or chained INDI servers):
    \code indiserver tutorial_dome tutorial_rain \endcode
    The dome driver keeps a copy of RainL light property from the rain driver. This makes it easy to parse the property status once an update from the rain
    driver arrives in the dome driver. Alternatively, you can directly parse the XML root element in ISSnoopDevice(XMLEle *root) to extract the required data.
*/

#include "dome.h"
#include <inditimer.h>

#include <memory>
#include <cstring>
#include <unistd.h>

std::unique_ptr<Dome> dome(new Dome());

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool Dome::Connect()
{
    LOG_INFO("Dome connected successfully!");
    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool Dome::Disconnect()
{
    LOG_INFO("Dome disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *Dome::getDefaultName()
{
    return "Dome";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool Dome::initProperties()
{
    // Must init parent properties first!
    INDI::DefaultDevice::initProperties();

    mShutterSwitch[0].fill("Open", "", ISS_ON);
    mShutterSwitch[1].fill("Close", "", ISS_OFF);
    mShutterSwitch.fill(getDeviceName(), "Shutter Door", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    mShutterSwitch.onUpdate([this]()
    {
        if (mShutterSwitch[0].getState() == ISS_ON)
            openShutter();
        else
            closeShutter();

    });
    // We init here the property we wish to "snoop" from the target device
    mRainLight[0].fill("Status", "", IPS_IDLE);

    // wait for "Rain Detector" driver
    watchDevice("Rain Detector", [this](INDI::BaseDevice device)
    {
        // wait for "Rain Alert" property available
        device.watchProperty("Rain Alert", [this](INDI::PropertyLight rain)
        {
            mRainLight = rain; // we have real rainLight property, override mRainLight
            static IPState oldRainState = rain[0].getState();

            IPState newRainState = rain[0].getState();
            if (newRainState == IPS_ALERT)
            {
                // If dome is open, then close it */
                if (mShutterSwitch[0].getState() == ISS_ON)
                    closeShutter();
                else
                    LOG_INFO("Rain Alert Detected! Dome is already closed.");
            }

            if (newRainState != IPS_ALERT && oldRainState == IPS_ALERT)
            {
                LOG_INFO("Rain threat passed. Opening the dome is now safe.");
            }

            oldRainState = newRainState;
        }, BaseDevice::WATCH_NEW_OR_UPDATE);
    });

    return true;
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This function is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool Dome::updateProperties()
{
    // Call parent update properties first
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
        defineProperty(mShutterSwitch);
    else
        // We're disconnected
        deleteProperty(mShutterSwitch);

    return true;
}

/********************************************************************************************
** Close shutter
*********************************************************************************************/
void Dome::closeShutter()
{
    mShutterSwitch.setState(IPS_BUSY);
    mShutterSwitch.apply("Shutter is closing...");

    INDI::Timer::singleShot(5000 /* ms */, [this]()
    {
        mShutterSwitch[0].setState(ISS_OFF);
        mShutterSwitch[1].setState(ISS_ON);

        mShutterSwitch.setState(IPS_OK);
        mShutterSwitch.apply("Shutter is closed.");
    });
}

/********************************************************************************************
** Open shutter
*********************************************************************************************/
void Dome::openShutter()
{
    if (mRainLight[0].getState() == IPS_ALERT)
    {
        mShutterSwitch.setState(IPS_ALERT);
        mShutterSwitch[0].setState(ISS_OFF);
        mShutterSwitch[1].setState(ISS_ON);
        mShutterSwitch.apply("It is raining, cannot open Shutter.");
        return;
    }

    mShutterSwitch.setState(IPS_BUSY);
    mShutterSwitch.apply("Shutter is opening...");

    INDI::Timer::singleShot(5000 /* ms */, [this]()
    {
        mShutterSwitch[0].setState(ISS_ON);
        mShutterSwitch[1].setState(ISS_OFF);

        mShutterSwitch.setState(IPS_OK);
        mShutterSwitch.apply("Shutter is open.");
    });
}
