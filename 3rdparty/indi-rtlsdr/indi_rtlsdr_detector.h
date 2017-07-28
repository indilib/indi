/*
   INDI Developers Manual
   Tutorial #3

   "Simple Detector Driver"

   We develop a simple Detector driver.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file simpleDetector.h
	\brief Construct a basic INDI Detector device that simulates capture & temperature settings. It also generates a random pattern and uploads it as a FITS file.
	\author Ilia Platone

	\example simpleDetector.h
	A simple detector device that can capture stream frames and controls temperature. It returns a FITS image to the client. To build drivers for complex Detectors, please
	refer to the INDI Generic Detector driver template in INDI github (under 3rdparty).
*/

#pragma once

#include <rtl-sdr.h>
#include "indidetector.h"

enum Settings
{
	FREQUENCY_N=0,
	SAMPLERATE_N,
	BANDWIDTH_N,
	NUM_SETTINGS
};
class RTLSDR : public INDI::Detector
{
  public:
	RTLSDR();

  protected:
	// General device functions
	bool Connect();
	bool Disconnect();
	const char *getDefaultName();
	bool initProperties();
	bool updateProperties();

	// Detector specific functions
	bool StartCapture(float duration);
	bool CaptureParamsUpdated(float bw, float capfreq, float samfreq, float bps);
	bool AbortCapture();
	int SetTemperature(double temperature);
	void TimerHit();


  private:
	rtlsdr_dev_t *rtl_dev = NULL;
	// Utility functions
	float CalcTimeLeft();
	void setupParams();
	void grabFrame();

	// Are we exposing?
	bool InCapture;
	// Struct to keep timing
	struct timeval CapStart;

	float CaptureRequest;
	float TemperatureRequest;
	INumber *SettingsN;
	INumberVectorProperty SettingsNP;
};
