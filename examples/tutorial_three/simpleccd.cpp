/*
   INDI Developers Manual
   Tutorial #3

   "Simple CCD Driver"

   We develop a simple CCD driver.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file simpleccd.cpp
    \brief Construct a basic INDI CCD device that simulates exposure & temperature settings. It also generates a random pattern and uploads it as a FITS file.
    \author Jasem Mutlaq

    \example simpleccd.cpp
    A simple CCD device that can capture images and control temperature. It returns a FITS image to the client. To build drivers for complex CCDs, please
    refer to the INDI Generic CCD driver template in INDI SVN (under 3rdparty).
*/

#include "simpleccd.h"

#include <memory>

/* Macro shortcut to CCD temperature value */
#define currentCCDTemperature TemperatureNP[0].value

std::unique_ptr<SimpleCCD> simpleCCD(new SimpleCCD());

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool SimpleCCD::Connect()
{
    LOG_INFO("Simple CCD connected successfully!");

    // Let's set a timer that checks teleCCDs status every POLLMS milliseconds.
    SetTimer(getCurrentPollingPeriod());
    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool SimpleCCD::Disconnect()
{
    LOG_INFO("Simple CCD disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *SimpleCCD::getDefaultName()
{
    return "Simple CCD";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool SimpleCCD::initProperties()
{
    // Must init parent properties first!
    INDI::CCD::initProperties();

    // We set the CCD capabilities
    uint32_t cap = CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_COOLER | CCD_HAS_SHUTTER;
    SetCCDCapability(cap);

    // Add Debug, Simulator, and Configuration controls
    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This function is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool SimpleCCD::updateProperties()
{
    // Call parent update properties first
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from CCD
        setupParams();

        // Start the timer
        SetTimer(getCurrentPollingPeriod());
    }

    return true;
}

/**************************************************************************************
** Setting up CCD parameters
***************************************************************************************/
void SimpleCCD::setupParams()
{
    // Our CCD is an 8 bit CCD, 1280x1024 resolution, with 5.4um square pixels.
    SetCCDParams(1280, 1024, 8, 5.4, 5.4);

    // Let's calculate how much memory we need for the primary CCD buffer
    uint32_t nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool SimpleCCD::StartExposure(float duration)
{
    ExposureRequest = duration;

    // Since we have only have one CCD with one chip, we set the exposure duration of the primary CCD
    PrimaryCCD.setExposureDuration(duration);

    ExposureTimer.start();
    InExposure = true;

    // We're done
    return true;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool SimpleCCD::AbortExposure()
{
    InExposure = false;
    return true;
}

/**************************************************************************************
** Client is asking us to set a new temperature
***************************************************************************************/
int SimpleCCD::SetTemperature(double temperature)
{
    TemperatureRequest = temperature;

    // 0 means it will take a while to change the temperature
    return 0;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float SimpleCCD::CalcTimeLeft()
{
    return ExposureRequest - ExposureTimer.elapsed() / 1000.0;
}

/**************************************************************************************
** Main device loop. We check for exposure and temperature progress here
***************************************************************************************/
void SimpleCCD::TimerHit()
{
    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        double timeleft = CalcTimeLeft();

        // Less than a 0.1 second away from exposure completion
        // This is an over simplified timing method, check CCDSimulator and simpleCCD for better timing checks
        if (timeleft < 0.1)
        {
            /* We're done exposing */
            LOG_INFO("Exposure done, downloading image...");

            // Set exposure left to zero
            PrimaryCCD.setExposureLeft(0);

            // We're no longer exposing...
            InExposure = false;

            /* grab and save image */
            grabImage();
        }
        else
            // Just update time left in client
            PrimaryCCD.setExposureLeft(timeleft);
    }

    // TemperatureNP is defined in INDI::CCD
    switch (TemperatureNP.getState())
    {
        case IPS_IDLE:
        case IPS_OK:
            break;

        case IPS_BUSY:
            /* If target temperature is higher, then increase current CCD temperature */
            if (currentCCDTemperature < TemperatureRequest)
                currentCCDTemperature++;
            /* If target temperature is lower, then decrease current CCD temperature */
            else if (currentCCDTemperature > TemperatureRequest)
                currentCCDTemperature--;
            /* If they're equal, stop updating */
            else
            {
                TemperatureNP.setState(IPS_OK);
                LOG_WARN("Target temperature reached.");
                TemperatureNP.apply();
                break;
            }

            TemperatureNP.apply();

            break;

        case IPS_ALERT:
            break;
    }

    SetTimer(getCurrentPollingPeriod());
}

/**************************************************************************************
** Create a random image and return it to client
***************************************************************************************/
void SimpleCCD::grabImage()
{
    // Let's get a pointer to the frame buffer
    uint8_t *image = PrimaryCCD.getFrameBuffer();

    // Get width and height
    int width  = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP() / 8;
    int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

    // Fill buffer with random pattern
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            image[i * width + j] = rand() % 255;

    LOG_INFO("Download complete.");

    // Let INDI::CCD know we're done filling the image buffer
    ExposureComplete(&PrimaryCCD);
}
