/*
   INDI Developers Manual
   Tutorial #3

   "Simple Spectrograph Driver"

   We develop a simple Spectrograph driver.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file simpledetector.cpp
    \brief Construct a basic INDI Spectrograph device that simulates exposure & temperature settings. It also generates a random pattern and uploads it as a FITS file.
    \author Ilia Platone, clearly taken from SimpleCCD by Jasem Mutlaq

    \example simpledetector.cpp
    A simple Spectrograph device that can capture images and control temperature. It returns a FITS image to the client. To build drivers for complex Spectrographs, please
    refer to the INDI Generic Spectrograph driver template in INDI SVN (under 3rdparty).
*/

#include "simple_spectrograph.h"

#include <memory>

/* Macro shortcut to Spectrograph temperature value */
#define currentSpectrographTemperature TemperatureN[0].value

std::unique_ptr<SimpleSpectrograph> simpleSpectrograph(new SimpleSpectrograph());

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool SimpleSpectrograph::Connect()
{
    IDMessage(getDeviceName(), "Simple Spectrograph connected successfully!");

    // Let's set a timer that checks teleSpectrographs status every POLLMS milliseconds.
    SetTimer(getCurrentPollingPeriod());

    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool SimpleSpectrograph::Disconnect()
{
    IDMessage(getDeviceName(), "Simple Spectrograph disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *SimpleSpectrograph::getDefaultName()
{
    return "Simple Spectrograph";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool SimpleSpectrograph::initProperties()
{
    // Must init parent properties first!
    INDI::Spectrograph::initProperties();

    // We set the Spectrograph capabilities
    uint32_t cap = SENSOR_CAN_ABORT | SENSOR_HAS_COOLER | SENSOR_HAS_SHUTTER;
    SetCapability(cap);

    // Add Debug, Simulator, and Configuration controls
    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool SimpleSpectrograph::updateProperties()
{
    // Call parent update properties first
    INDI::Spectrograph::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from Spectrograph
        setupParams();

        // Start the timer
        SetTimer(getCurrentPollingPeriod());
    }

    return true;
}

/**************************************************************************************
** Client is updating capture settings
***************************************************************************************/
bool SimpleSpectrograph::paramsUpdated(float sr, float freq, float bps, float bw, float gain)
{
    INDI_UNUSED(bps);
    INDI_UNUSED(freq);
    INDI_UNUSED(sr);
    INDI_UNUSED(bw);
    INDI_UNUSED(gain);
	return true;
}

/**************************************************************************************
** Setting up Spectrograph parameters
***************************************************************************************/
void SimpleSpectrograph::setupParams()
{
    // Our Spectrograph is an 8 bit Spectrograph, 100MHz frequency 1MHz samplerate.
    setFrequency(1000000.0);
    setSampleRate(100000000.0);
    setBPS(16);
    setBandwidth(0.0);
    setGain(25.0);
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool SimpleSpectrograph::StartIntegration(double duration)
{
    IntegrationRequest = duration;

    // Since we have only have one Spectrograph with one chip, we set the exposure duration of the primary Spectrograph
    setIntegrationTime(duration);

    gettimeofday(&CapStart, nullptr);

    InIntegration = true;

    // We're done
    return true;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool SimpleSpectrograph::AbortIntegration()
{
    InIntegration = false;
    return true;
}

/**************************************************************************************
** Client is asking us to set a new temperature
***************************************************************************************/
int SimpleSpectrograph::SetTemperature(double temperature)
{
    TemperatureRequest = temperature;

    // 0 means it will take a while to change the temperature
    return 0;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float SimpleSpectrograph::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now { 0, 0 };

    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                (double)(CapStart.tv_sec * 1000.0 + CapStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = IntegrationRequest - timesince;
    return timeleft;
}

/**************************************************************************************
** Main device loop. We check for exposure and temperature progress here
***************************************************************************************/
void SimpleSpectrograph::TimerHit()
{
    long timeleft;

    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    if (InIntegration)
    {
        timeleft = CalcTimeLeft();

        // Less than a 0.1 second away from exposure completion
        // This is an over simplified timing method, check SpectrographSimulator and simpleSpectrograph for better timing checks
        if (timeleft < 0.1)
        {
            /* We're done exposing */
            IDMessage(getDeviceName(), "Integration done, downloading image...");

            // Set exposure left to zero
            setIntegrationLeft(0);

            // We're no longer exposing...
            InIntegration = false;

            /* grab and save image */
            grabFrame();
        }
        else
            // Just update time left in client
            setIntegrationLeft(timeleft);
    }

    // TemperatureNP is defined in INDI::Spectrograph
    switch (TemperatureNP.s)
    {
        case IPS_IDLE:
        case IPS_OK:
            break;

        case IPS_BUSY:
            /* If target temperature is higher, then increase current Spectrograph temperature */
            if (currentSpectrographTemperature < TemperatureRequest)
                currentSpectrographTemperature++;
            /* If target temperature is lower, then decrese current Spectrograph temperature */
            else if (currentSpectrographTemperature > TemperatureRequest)
                currentSpectrographTemperature--;
            /* If they're equal, stop updating */
            else
            {
                TemperatureNP.s = IPS_OK;
                IDSetNumber(&TemperatureNP, "Target temperature reached.");

                break;
            }

            IDSetNumber(&TemperatureNP, nullptr);

            break;

        case IPS_ALERT:
            break;
    }

    SetTimer(getCurrentPollingPeriod());
}

/**************************************************************************************
** Create a random image and return it to client
***************************************************************************************/
void SimpleSpectrograph::grabFrame()
{
    // Set length of continuum
    int len  = getSampleRate() * getIntegrationTime() * getBPS() / 8;
    setBufferSize(len);
 
   // Let's get a pointer to the frame buffer
    uint8_t *continuum = getBuffer();

    // Fill buffer with random pattern
    for (int i = 0; i < len; i++)
        continuum[i] = rand() % 255;

    IDMessage(getDeviceName(), "Download complete.");

    // Let INDI::Spectrograph know we're done filling the image buffer
    IntegrationComplete();
}
