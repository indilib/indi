/*
    indi_rtlsdr_detector - a software defined radio driver for INDI
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

#include "indi_rtlsdr_detector.h"

#include <memory>

const int POLLMS		   = 500; /* Polling interval 500 ms */
//const int MAX_DETECTOR_TEMP	 = 45;  /* Max Detector temperature */
//const int MIN_DETECTOR_TEMP	 = -55; /* Min Detector temperature */
//const float TEMP_THRESHOLD = .25; /* Differential temperature threshold (C)*/

/* Macro shortcut to Detector temperature value */
#define currentDetectorTemperature TemperatureN[0].value


std::unique_ptr<RTLSDR> rtlsdrDetector(new RTLSDR());

void ISGetProperties(const char *dev)
{
	rtlsdrDetector->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
	rtlsdrDetector->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
	rtlsdrDetector->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
	rtlsdrDetector->ISNewNumber(dev, name, values, names, num);
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
	rtlsdrDetector->ISSnoopDevice(root);
}

RTLSDR::RTLSDR()
{
	InCapture = false;
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool RTLSDR::Connect()
{
	IDMessage(getDeviceName(), "RTL-SDR Detector connected successfully!");
	int dev_index = rtlsdr_get_device_count();

	if (dev_index < 1) {
		return false;
	}

	int r = rtlsdr_open(&rtl_dev, 0);
	if (r < 0) {
		fprintf(stderr, "Failed to open rtlsdr device #%d.\n", dev_index);
		return false;
	}
	// Let's set a timer that checks teleDetectors status every POLLMS milliseconds.
	SetTimer(POLLMS);

	/* Set the sample rate */
	rtlsdr_set_sample_rate(rtl_dev, 2048000);

	/* Set the frequency */
	rtlsdr_set_center_freq(rtl_dev, 100000000);

	/* Set the bandwidth */
	rtlsdr_set_tuner_bandwidth(rtl_dev, 10000);

	rtlsdr_set_agc_mode(rtl_dev, 1);

	/* Reset endpoint before we start reading from it (mandatory) */
	rtlsdr_reset_buffer(rtl_dev);

	return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool RTLSDR::Disconnect()
{
	rtlsdr_close(rtl_dev);
	IDMessage(getDeviceName(), "RTL-SDR Detector disconnected successfully!");
	return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char *RTLSDR::getDefaultName()
{
	return "RTL-SDR Receiver";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool RTLSDR::initProperties()
{
	// Must init parent properties first!
	INDI::Detector::initProperties();

	// We set the Detector capabilities
	uint32_t cap = DETECTOR_CAN_ABORT | DETECTOR_HAS_CONTINUUM;
	SetDetectorCapability(cap);

	// Add Debug, Simulator, and Configuration controls
	addAuxControls();

	return true;
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool RTLSDR::updateProperties()
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
void RTLSDR::setupParams()
{
	// Our Detector is an 8 bit Detector, 2MSPS sample rate, 100MHz frequency 10Khz bandwidth.
	SetDetectorParams(10000.0, 100000000.0, 2048000.0, 8);
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool RTLSDR::StartCapture(float duration)
{
	CaptureRequest = duration;

	// Since we have only have one Detector with one chip, we set the exposure duration of the primary Detector
	PrimaryDetector.setCaptureDuration(duration);

	gettimeofday(&CapStart, nullptr);

	// Let's calculate how much memory we need for the primary Detector buffer
	int nbuf;
	nbuf = PrimaryDetector.getSamplingFrequency() * PrimaryDetector.getCaptureDuration() * PrimaryDetector.getBPS() / 8;

	PrimaryDetector.setFrameBufferSize(nbuf + 512);

	InCapture = true;

	// We're done
	return true;
}

/**************************************************************************************
** Client is updating capture settings
***************************************************************************************/
bool RTLSDR::CaptureParamsUpdated(float bw, float capfreq, float samfreq, float bps)
{
    	INDI_UNUSED(bps);
	int r = 0;
	r = rtlsdr_set_center_freq(rtl_dev, (uint32_t)capfreq);
	if(r != 0)
		return false;
	r = rtlsdr_set_sample_rate(rtl_dev, (uint32_t)samfreq);
	if(r != 0)
		return false;
	r = rtlsdr_set_tuner_bandwidth(rtl_dev, (uint32_t)bw);
	if(r != 0)
		return false;
	return true;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool RTLSDR::AbortCapture()
{
	InCapture = false;
	return true;
}

/**************************************************************************************
** Client is asking us to set a new temperature
***************************************************************************************/
int RTLSDR::SetTemperature(double temperature)
{
	TemperatureRequest = temperature;

	// 0 means it will take a while to change the temperature
	return 0;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float RTLSDR::CalcTimeLeft()
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
** Main device loop. We check for exposure and temperature progress here
***************************************************************************************/
void RTLSDR::TimerHit()
{
	long timeleft;

	if (isConnected() == false)
		return; //  No need to reset timer if we are not connected anymore

	if (InCapture)
	{
		timeleft = CalcTimeLeft();

		// Less than a 0.1 second away from exposure completion
		// This is an over simplified timing method, check DetectorSimulator and rtlsdrDetector for better timing checks
		if (timeleft < 0.1)
		{
			/* We're done exposing */
			IDMessage(getDeviceName(), "Capture done, downloading image...");

			// Set exposure left to zero
			PrimaryDetector.setCaptureLeft(0);

			// We're no longer exposing...
			InCapture = false;

			/* grab and save image */
			grabFrame();
		}
		else
			// Just update time left in client
			PrimaryDetector.setCaptureLeft(timeleft);
	}

	// TemperatureNP is defined in INDI::Detector
	switch (TemperatureNP.s)
	{
		case IPS_IDLE:
		case IPS_OK:
			break;

		case IPS_BUSY:
			/* If target temperature is higher, then increase current Detector temperature */
			if (currentDetectorTemperature < TemperatureRequest)
				currentDetectorTemperature++;
			/* If target temperature is lower, then decrese current Detector temperature */
			else if (currentDetectorTemperature > TemperatureRequest)
				currentDetectorTemperature--;
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

	SetTimer(POLLMS);
	return;
}

/**************************************************************************************
** Create a random image and return it to client
***************************************************************************************/
void RTLSDR::grabFrame()
{
	// Let's get a pointer to the frame buffer
	uint8_t *image = PrimaryDetector.getFrameBuffer();

	// Get width and height
	int len  = PrimaryDetector.getSamplingFrequency() * PrimaryDetector.getCaptureDuration() * PrimaryDetector.getBPS() / 8;
	int n_read = 0;
	int r = rtlsdr_read_sync(rtl_dev, image, len, &n_read);

	if (r < 0) {
		fprintf(stderr, "WARNING: sync read failed.\n");
		return;
	}

	if (n_read < len) {
		fprintf(stderr, "Short read, samples lost, exiting!\n");
		return;
	}

	IDMessage(getDeviceName(), "Download complete.");

	// Let INDI::Detector know we're done filling the image buffer
	CaptureComplete(&PrimaryDetector);
}
