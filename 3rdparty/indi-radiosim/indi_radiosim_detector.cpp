/*
    indi_radiosim_detector - a simulated radio telescope driver for INDI
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

#include "indi_radiosim_detector.h"
#include <cstdio>
#include <cstdlib>
#include <indicom.h>
#include <math.h>
#include <unistd.h>
#include <indilogger.h>
#include <memory>

#define MAX_TRIES 20
#define MAX_DEVICES 4
#define SPECTRUM_SIZE 255
#define RESOLUTION0 (LIGHTSPEED / PrimaryDetector.getFrequency())
#define RESOLUTION (RESOLUTION0 / primaryAperture)
#define DISH_SIZE_M 5.0
#define MAX_DISH_SIZE_M 32.0
#define FOV_DEG_X (360.0)
#define FOV_DEG_Y (180.0)

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
        receiver = new RadioSim();

        atexit(cleanup);
        isInit = true;
    }
}

void ISGetProperties(const char *dev)
{
    ISInit();
    if (dev == nullptr || !strcmp(dev, receiver->getDeviceName()))
    {
        receiver->ISGetProperties(dev);
    }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ISInit();
    if (dev == nullptr || !strcmp(dev, receiver->getDeviceName()))
    {
        receiver->ISNewSwitch(dev, name, states, names, num);
    }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ISInit();
    if (dev == nullptr || !strcmp(dev, receiver->getDeviceName()))
    {
        receiver->ISNewText(dev, name, texts, names, num);
    }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ISInit();
    if (dev == nullptr || !strcmp(dev, receiver->getDeviceName()))
    {
        receiver->ISNewNumber(dev, name, values, names, num);
    }
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
    DishSize = 5;
    RA = 0;
    Dec = 0;
    setDeviceName(getDefaultName());
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool RadioSim::Connect()
{
    LOG_INFO("RadioSim connected successfully!");
    continuum = (uint8_t*)malloc(sizeof(uint8_t));
    spectrum = (uint8_t*)malloc(sizeof(uint8_t));
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
    free(continuum);
    free(spectrum);
    LOG_INFO("RadioSim Detector disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *RadioSim::getDefaultName()
{
    return "RadioSim Receiver";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool RadioSim::initProperties()
{
    // Must init parent properties first!
    INDI::Detector::initProperties();

    // We set the Detector capabilities
    uint32_t cap = DETECTOR_CAN_ABORT | DETECTOR_HAS_CONTINUUM | DETECTOR_HAS_SPECTRUM;
    SetDetectorCapability(cap);

    IUFillNumber(&DetectorPropertiesN[0], "DETECTOR_SIZE", "Dish size (m)", "%4.0f", 5, MAX_DISH_SIZE_M, 1, 5.0);
    IUFillNumberVector(&DetectorPropertiesNP, DetectorPropertiesN, 1, getDeviceName(), "DETECTOR_PROPERTIES", "Control", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&DetectorCoordsN[0], "DETECTOR_RA", "Position (RA)", "%2.3f", 0, FOV_DEG_X, 1, 0);
    IUFillNumber(&DetectorCoordsN[1], "DETECTOR_DEC", "Position (DEC)", "%2.3f", 0, FOV_DEG_Y, 1, 0);
    IUFillNumberVector(&DetectorCoordsNP, DetectorCoordsN, 2, getDeviceName(), "DETECTOR_COORDS", "Coordinates", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    PrimaryDetector.setMinMaxStep("DETECTOR_CAPTURE", "DETECTOR_CAPTURE_VALUE", 1.0e-6, 86164.092, 0.001, false);
    PrimaryDetector.setMinMaxStep("DETECTOR_SETTINGS", "DETECTOR_FREQUENCY", 1.0e+6, 50.0e+9, 1.0, false);
    PrimaryDetector.setMinMaxStep("DETECTOR_SETTINGS", "DETECTOR_SAMPLERATE", 1.0e+3, 100.0e+3, 1.0, false);
    PrimaryDetector.setMinMaxStep("DETECTOR_SETTINGS", "DETECTOR_GAIN", 0.0, 25.0, 1.0, false);
    PrimaryDetector.setMinMaxStep("DETECTOR_SETTINGS", "DETECTOR_BANDWIDTH", 1.0e+3, 100.0e+6, 1.0, false);
    PrimaryDetector.setMinMaxStep("DETECTOR_SETTINGS", "DETECTOR_BITSPERSAMPLE", -64, -64, -64, false);
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
        defineNumber(&DetectorPropertiesNP);
        defineNumber(&DetectorCoordsNP);

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
    SetDetectorParams(1e+6, 1.42e+9, -64, 1.0e+6, 1);
}

/**************************************************************************************
** INDI is asking us to submit list of properties for the device
***************************************************************************************/
void RadioSim::ISGetProperties(const char *dev)
{
    INDI::Detector::ISGetProperties(dev);

    if (isConnected())
    {
        // Define our properties
        defineNumber(&DetectorPropertiesNP);
        defineNumber(&DetectorCoordsNP);
    }
}

/**************************************************************************************
** Client is asking us to set a new number
***************************************************************************************/
bool RadioSim::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    if (!strcmp(name, DetectorPropertiesNP.name))
    {
        IUUpdateNumber(&DetectorPropertiesNP, values, names, n);

        DishSize = (DetectorPropertiesN[0].value);

        DetectorPropertiesNP.s = IPS_OK;
        IDSetNumber(&DetectorPropertiesNP, nullptr);
        return true;
    }

    if (!strcmp(name, DetectorCoordsNP.name))
    {
        IUUpdateNumber(&DetectorCoordsNP, values, names, n);

        RA = (DetectorCoordsN[0].value);
        Dec = (DetectorCoordsN[1].value);

        DetectorCoordsNP.s = IPS_OK;
        IDSetNumber(&DetectorCoordsNP, nullptr);
        return true;
    }

    return INDI::Detector::ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool RadioSim::StartCapture(float duration)
{
    CaptureRequest = duration;

    to_read = duration * PrimaryDetector.getSampleRate() * abs(PrimaryDetector.getBPS()) / 8;
    // Since we have only have one Detector with one chip, we set the exposure duration of the primary Detector
    PrimaryDetector.setCaptureDuration(duration);
    PrimaryDetector.setContinuumBufferSize(to_read);
    PrimaryDetector.setSpectrumBufferSize(SPECTRUM_SIZE);
    gettimeofday(&CapStart, nullptr);

    InCapture = true;

    // We're done
    return true;
}

/**************************************************************************************
** Client is updating capture settings
***************************************************************************************/
bool RadioSim::CaptureParamsUpdated(float sr, float freq, float bps, float bw, float gain)
{
    PrimaryDetector.setBPS(bps);
    PrimaryDetector.setFrequency(freq);
    PrimaryDetector.setBandwidth(bw);
    PrimaryDetector.setSampleRate(sr);
    PrimaryDetector.setGain(gain);

    return true;
}

/**************************************************************************************
** Client is asking us to abort a capture
***************************************************************************************/
bool RadioSim::AbortCapture()
{
    InCapture = false;
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
            LOG_INFO("Capture done, downloading data...");
            timeleft = 0.0;
            grabData();
        }

        // This is an over simplified timing method, check DetectorSimulator and rtlsdrDetector for better timing checks
        PrimaryDetector.setCaptureLeft(timeleft);
    }
    double value = (DetectorCoordsN[0].value + (360.0 / STELLAR_DAY) * POLLMS / 1000.0);
    if (value >= FOV_DEG_X)
    {
        value -= FOV_DEG_X;
    }
    RA = value;
    DetectorCoordsN[0].value = RA;
    IDSetNumber(&DetectorCoordsNP, nullptr);
    SetTimer(POLLMS);
    return;
}

/**************************************************************************************
** Get the Data
***************************************************************************************/
void RadioSim::grabData()
{
    if(InCapture)
    {
        int len = to_read * 8 / PrimaryDetector.getBPS();

        LOG_INFO("Downloading...");
        InCapture = false;
        dsp_stream_p stream = dsp_stream_new();
        dsp_stream_add_dim(stream, len);
        dsp_stream_alloc_buffer(stream, len);
        dsp_signals_sinewave(stream, PrimaryDetector.getSampleRate(), ((rand() % (int)PrimaryDetector.getSampleRate()) + 1));
        dsp_buffer_stretch(stream, 0, RESOLUTION0 * 255 / RESOLUTION);
        for(int x = 0; x < stream->len; x++)
        {
            stream->buf[x] *= PrimaryDetector.getGain();
            stream->buf[x] += (rand() % 255);
        }
        dsp_buffer_normalize(stream, 0, 4096);
        continuum = PrimaryDetector.getContinuumBuffer();
        dsp_buffer_copy(stream->buf, continuum, stream->len);
        dsp_stream_free_buffer(stream);
        dsp_stream_free(stream);

        //Create the spectrum
        spectrum = PrimaryDetector.getSpectrumBuffer();
        Spectrum(continuum, spectrum, to_read, SPECTRUM_SIZE * 8 / PrimaryDetector.getBPS(), PrimaryDetector.getBPS());

        LOG_INFO("Download complete.");
        CaptureComplete(&PrimaryDetector);
    }
}
