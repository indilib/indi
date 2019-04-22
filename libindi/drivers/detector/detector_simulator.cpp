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

#include "detector_simulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <indilogger.h>
#include <memory>

#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

static RadioSim *receiver;

static void cleanup()
{
    delete receiver;
}

void ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        if (receiver == nullptr)
        {
            receiver = new RadioSim();
        }

        atexit(cleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    ISInit();

    receiver->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ISInit();

    receiver->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ISInit();

    receiver->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ISInit();

    receiver->ISNewNumber(dev, name, values, names, num);
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
    ISInit();

    receiver->ISSnoopDevice(root);
}

RadioSim::RadioSim()
{
    InCapture = false;
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool RadioSim::Connect()
{
    LOG_INFO("Simulator Detector connected successfully!");
    // Let's set a timer that checks teleDetectors status every POLLMS milliseconds.
    // JM 2017-07-31 SetTimer already called in updateProperties(). Just call it once
    //SetTimer(POLLMS);


    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool RadioSim::Disconnect()
{
    InCapture = false;
    PrimaryDetector.setContinuumBufferSize(1);
    PrimaryDetector.setSpectrumBufferSize(1);
    LOG_INFO("Simulator Detector disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *RadioSim::getDefaultName()
{
    return "Detector Simulator";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool RadioSim::initProperties()
{
    // We set the Detector capabilities
    uint32_t cap = DETECTOR_CAN_ABORT | DETECTOR_HAS_CONTINUUM | DETECTOR_HAS_SPECTRUM | DETECTOR_HAS_STREAMING;
    SetDetectorCapability(cap);

    // Must init parent properties first!
    INDI::Detector::initProperties();

    PrimaryDetector.setMinMaxStep("DETECTOR_CAPTURE", "DETECTOR_CAPTURE_VALUE", 0.001, 86164.092, 0.001, false);
    PrimaryDetector.setMinMaxStep("DETECTOR_SETTINGS", "DETECTOR_FREQUENCY", 2.4e+7, 2.0e+9, 1, false);
    PrimaryDetector.setMinMaxStep("DETECTOR_SETTINGS", "DETECTOR_SAMPLERATE", 1.0e+6, 2.0e+6, 1, false);
    PrimaryDetector.setMinMaxStep("DETECTOR_SETTINGS", "DETECTOR_GAIN", 0.0, 25.0, 0.1, false);
    PrimaryDetector.setMinMaxStep("DETECTOR_SETTINGS", "DETECTOR_BANDWIDTH", 0, 0, 0, false);
    PrimaryDetector.setMinMaxStep("DETECTOR_SETTINGS", "DETECTOR_BITSPERSAMPLE", 16, 16, 0, false);
    PrimaryDetector.setCaptureExtension("fits");

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
    // Call parent update properties first
    INDI::Detector::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from Detector
        setupParams();

        // Start the timer
        SetTimer(POLLMS);
    }

    return true;
}

/**************************************************************************************
** Setting up Detector parameters
***************************************************************************************/
void RadioSim::setupParams()
{
    // Our Detector is an 8 bit Detector, 100MHz frequency 1MHz bandwidth.
    SetDetectorParams(1000000.0, 100000000.0, 16, 0.0, 25.0);
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool RadioSim::StartCapture(float duration)
{
    CaptureRequest = duration;
    AbortCapture();

    // Since we have only have one Detector with one chip, we set the exposure duration of the primary Detector
    PrimaryDetector.setCaptureDuration(duration);
    int to_read = PrimaryDetector.getSampleRate() * PrimaryDetector.getCaptureDuration() * sizeof(unsigned short);

    PrimaryDetector.setContinuumBufferSize(to_read);
    PrimaryDetector.setSpectrumBufferSize((1 << abs(PrimaryDetector.getBPS())) * sizeof(unsigned short));

    // We're done
    return false;
}

/**************************************************************************************
** Client is updating capture settings
***************************************************************************************/
bool RadioSim::CaptureParamsUpdated(float sr, float freq, float bps, float bw, float gain)
{
    INDI_UNUSED(gain);
    INDI_UNUSED(freq);
    INDI_UNUSED(bps);
    PrimaryDetector.setBandwidth(bw);
    PrimaryDetector.setSampleRate(sr);

    return true;
}

/**************************************************************************************
** Client is asking us to abort a capture
***************************************************************************************/
bool RadioSim::AbortCapture()
{
    if(InCapture) {
        InCapture = false;
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

    timeleft = CaptureRequest - timesince;
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

    if (InCapture)
    {
        timeleft = CalcTimeLeft();
        if(timeleft < 0.1)
        {
            /* We're done capturing */
            LOG_INFO("Capture done, expecting data...");
            timeleft = 0.0;
        }

        // This is an over simplified timing method, check DetectorSimulator and RadioSimDetector for better timing checks
        PrimaryDetector.setCaptureLeft(timeleft);
    }

    SetTimer(POLLMS);
    return;
}

/**************************************************************************************
** Create the spectrum
***************************************************************************************/
void RadioSim::grabData()
{
    if(InCapture) {

        LOG_INFO("Downloading...");
        InCapture = false;

        uint8_t* continuum;
        uint8_t* spectrum;
        int size = PrimaryDetector.getContinuumBufferSize() * 8 / PrimaryDetector.getBPS();
        //Fill the continuum
        continuum = PrimaryDetector.getContinuumBuffer();
        WhiteNoise(continuum, size, PrimaryDetector.getBPS());

        //Create the spectrum
        spectrum = PrimaryDetector.getSpectrumBuffer();
        Spectrum(continuum, spectrum, size, (1 << abs(PrimaryDetector.getBPS())), PrimaryDetector.getBPS());

        LOG_INFO("Download complete.");
        CaptureComplete(&PrimaryDetector);
    }
}
