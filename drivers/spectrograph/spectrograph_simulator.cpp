/*
    indi_RadioSim_detector - a software defined radio driver for INDI
    Copyright (C) 2017  Ilia Platone

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "spectrograph_simulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <indilogger.h>
#include <memory>

#define SPECTRUM_SIZE (256)
#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

static pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;
std::unique_ptr<RadioSim> receiver(new RadioSim());

RadioSim::RadioSim()
{
    streamPredicate = 0;
    terminateThread = false;
}

RadioSim::~RadioSim()
{

}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool RadioSim::Connect()
{
    LOG_INFO("Simulator Spectrograph connected successfully!");
    // Let's set a timer that checks teleSpectrographs status every POLLMS milliseconds.
    // JM 2017-07-31 SetTimer already called in updateProperties(). Just call it once
    //SetTimer(getCurrentPollingPeriod());

    streamPredicate = 0;
    terminateThread = false;
    // Run threads
    std::thread(&RadioSim::streamCaptureHelper, this).detach();
    SetTimer(getCurrentPollingPeriod());

    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool RadioSim::Disconnect()
{
    InIntegration = false;
    setBufferSize(1);
    pthread_mutex_lock(&condMutex);
    streamPredicate = 1;
    terminateThread = true;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);
    LOG_INFO("Simulator Spectrograph disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *RadioSim::getDefaultName()
{
    return "Spectrograph Simulator";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool RadioSim::initProperties()
{
    // We set the Spectrograph capabilities
    uint32_t cap = SENSOR_CAN_ABORT | SENSOR_HAS_STREAMING | SENSOR_HAS_DSP;
    SetCapability(cap);

    // Must init parent properties first!
    INDI::Spectrograph::initProperties();

    setMinMaxStep("SENSOR_INTEGRATION", "SENSOR_INTEGRATION_VALUE", 0.001, 86164.092, 0.001, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_FREQUENCY", 2.4e+7, 2.0e+9, 1, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_SAMPLERATE", 1.0e+6, 2.0e+6, 1, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_GAIN", 0.0, 25.0, 0.1, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_BANDWIDTH", 0, 0, 0, false);
    setMinMaxStep("SPECTROGRAPH_SETTINGS", "SPECTROGRAPH_BITSPERSAMPLE", 16, 16, 0, false);
    setIntegrationFileExtension("fits");

    // Add Debug, Simulator, and Configuration controls
    addAuxControls();

    setDefaultPollingPeriod(500);
    return true;
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool RadioSim::updateProperties()
{
    if (isConnected())
    {
        // Inital values
        setupParams(1000000, 1420000000, 10000, 10);

        // Start the timer
        SetTimer(getCurrentPollingPeriod());
    }

    return INDI::Spectrograph::updateProperties();
}

/**************************************************************************************
** Setting up Spectrograph parameters
***************************************************************************************/
void RadioSim::setupParams(float sr, float freq, float bw, float gain)
{
    // Our Spectrograph is an 8 bit Spectrograph, 100MHz frequency 1MHz bandwidth.
    setFrequency(freq);
    setSampleRate(sr);
    setBPS(16);
    setBandwidth(bw);
    setGain(gain);
}

bool RadioSim::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    bool r = false;
    if (dev && !strcmp(dev, getDeviceName()) && !strcmp(name, SpectrographSettingsNP.name)) {
        for(int i = 0; i < n; i++) {
            if (!strcmp(names[i], "SPECTROGRAPH_GAIN")) {
                setupParams(getSampleRate(), getFrequency(), getBandwidth(), values[i]);
            } else if (!strcmp(names[i], "SPECTROGRAPH_BANDWIDTH")) {
                setupParams(getSampleRate(), getFrequency(), values[i], getGain());
            } else if (!strcmp(names[i], "SPECTROGRAPH_FREQUENCY")) {
                setupParams(getSampleRate(), values[i], getBandwidth(), getGain());
            } else if (!strcmp(names[i], "SPECTROGRAPH_SAMPLERATE")) {
                setupParams(values[i], getFrequency(), getBandwidth(), getGain());
            }
        }
        IDSetNumber(&SpectrographSettingsNP, nullptr);
    }
    return processNumber(dev, name, values, names, n) & !r;
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool RadioSim::StartIntegration(double duration)
{
    IntegrationRequest = duration;
    AbortIntegration();

    // Since we have only have one Spectrograph with one chip, we set the exposure duration of the primary Spectrograph
    setIntegrationTime(duration);
    int to_read = getSampleRate() * getIntegrationTime() * abs(getBPS()) / 8;

    setBufferSize(to_read);
    InIntegration = true;

    gettimeofday(&CapStart, nullptr);
    if(HasStreaming()) {
        Streamer->setPixelFormat(INDI_MONO, getBPS());
        Streamer->setSize(getBufferSize() * 8 / abs(getBPS()), 1);
    }

    // We're done
    return true;
}

/**************************************************************************************
** Client is asking us to abort a capture
***************************************************************************************/
bool RadioSim::AbortIntegration()
{
    if(InIntegration) {
        InIntegration = false;
    }
    return true;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float RadioSim::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                (double)(CapStart.tv_sec * 1000.0 + CapStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = IntegrationRequest - timesince;
    return timeleft;
}

/**************************************************************************************
** Main device loop. We check for capture progress here
***************************************************************************************/
void RadioSim::TimerHit()
{
    long timeleft;

    if (isConnected() == false)
        return; //  No need to reset timer if we are not connected anymore

    if (InIntegration)
    {
        timeleft = CalcTimeLeft();
        if(timeleft <= 0.0)
        {
            /* We're done capturing */
            LOG_INFO("Integration done, expecting data...");
            timeleft = 0.0;
            grabData();
        }

        // This is an over simplified timing method, check SpectrographSimulator and RadioSimSpectrograph for better timing checks
        setIntegrationLeft(timeleft);
    }

    SetTimer(getCurrentPollingPeriod());
    return;
}

/**************************************************************************************
** Create the spectrum
***************************************************************************************/
void RadioSim::grabData()
{
    if(InIntegration) {

        LOG_INFO("Downloading...");
        InIntegration = false;

        uint8_t* continuum;
        int size = getBufferSize();

        //Fill the continuum
        continuum = getBuffer();
        for(int i = 0; i < size; i++)
            continuum[i] = rand() % 255;

        LOG_INFO("Download complete.");
        IntegrationComplete();
    }
}

//Streamer API functions

bool RadioSim::StartStreaming()
{
    pthread_mutex_lock(&condMutex);
    streamPredicate = 1;
    pthread_mutex_unlock(&condMutex);
    pthread_cond_signal(&cv);

    return true;
}

bool RadioSim::StopStreaming()
{
    pthread_mutex_lock(&condMutex);
    streamPredicate = 0;
    pthread_mutex_unlock(&condMutex);
    pthread_cond_signal(&cv);

    return true;
}

void RadioSim::streamCaptureHelper()
{
    struct itimerval tframe1, tframe2;
    double deltas;
    getitimer(ITIMER_REAL, &tframe1);
    auto s1 = ((double)tframe2.it_value.tv_sec) + ((double)tframe2.it_value.tv_usec / 1e6);
    auto s2 = ((double)tframe2.it_value.tv_sec) + ((double)tframe2.it_value.tv_usec / 1e6);

    while (true)
    {
        pthread_mutex_lock(&condMutex);

        while (streamPredicate == 0)
        {
            pthread_cond_wait(&cv, &condMutex);
        }
        StartIntegration(1.0 / Streamer->getTargetFPS());

        if (terminateThread)
            break;

        // release condMutex
        pthread_mutex_unlock(&condMutex);

        // Simulate exposure time
        //usleep(ExposureRequest*1e5);
        grabData();
        getitimer(ITIMER_REAL, &tframe1);

        s2 = ((double)tframe2.it_value.tv_sec) + ((double)tframe2.it_value.tv_usec / 1e6);
        deltas = fabs(s2 - s1);

        if (deltas < IntegrationTime)
            usleep(fabs(IntegrationTime - deltas) * 1e6);

        int32_t size = getBufferSize();
        Streamer->newFrame(getBuffer(), size);

        s1 = ((double)tframe1.it_value.tv_sec) + ((double)tframe1.it_value.tv_usec / 1e6);

        getitimer(ITIMER_REAL, &tframe2);
    }

    pthread_mutex_unlock(&condMutex);
}
