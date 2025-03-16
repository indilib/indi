/*
   INDI Developers Manual
   Tutorial #5 - Snooping

   Rain Detector

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file raindetector.cpp
    \brief Construct a rain detector device that the user may operate to raise a rain alert. This rain light property defined by this driver is \e snooped by the Dome driver
           then takes whatever appropriate action to protect the dome.
    \author Jasem Mutlaq
*/

#include "raindetector.h"

#include <memory>
#include <cstring>

std::unique_ptr<RainDetector> rainDetector(new RainDetector());

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool RainDetector::Connect()
{
    LOG_INFO("Rain Detector connected successfully!");
    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool RainDetector::Disconnect()
{
    LOG_INFO("Rain Detector disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *RainDetector::getDefaultName()
{
    return "Rain Detector";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool RainDetector::initProperties()
{
    // Must init parent properties first!
    INDI::DefaultDevice::initProperties();

    mRainLight[0].fill("Status", "", IPS_IDLE);
    mRainLight.fill(getDeviceName(), "Rain Alert", "", MAIN_CONTROL_TAB, IPS_IDLE);

    mRainSwitch[0].fill("On", "");
    mRainSwitch[1].fill("Off", "");
    mRainSwitch.fill(getDeviceName(), "Control Rain", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    mRainSwitch.onUpdate([this]()
    {
        if (mRainSwitch[0].getState() == ISS_ON)
        {
            mRainLight[0].setState(IPS_ALERT);
            mRainLight.setState(IPS_ALERT);
            mRainLight.apply("Alert! Alert! Rain detected!");
        }
        else
        {
            mRainLight[0].setState(IPS_IDLE);
            mRainLight.setState(IPS_OK);
            mRainLight.apply("Rain threat passed. The skies are clear.");
        }
        mRainSwitch.setState(IPS_OK);
        mRainSwitch.apply();
    });
    return true;
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This function is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool RainDetector::updateProperties()
{
    // Call parent update properties first
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(mRainLight);
        defineProperty(mRainSwitch);
    }
    else
        // We're disconnected
    {
        deleteProperty(mRainLight);
        deleteProperty(mRainSwitch);
    }

    return true;
}
