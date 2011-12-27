/*****************************************************************************************
NAME
 States

DESCRIPTION
 Camera states

COPYRIGHT (C)
 QSI (Quantum Scientific Imaging) 2005-2006

REVISION HISTORY
 MWB 04.16.06 Original Version
*****************************************************************************************/

#pragma once
//
//QSI uses MaxIm definitions for the following
//
enum QSICameraState
{							// Highest priority at top of list
	CCD_ERROR,				// Camera is not available
	CCD_FILTERWHEELMOVING,	// Waiting for filter wheel to finish moving
	CCD_FLUSHING,			// Flushing CCD chip or camera otherwise busy
	CCD_WAITTRIGGER,		// Waiting for an external trigger event
	CCD_DOWNLOADING,		// Downloading the image from camera hardware
	CCD_READING,			// Reading the CCD chip into camera hardware
	CCD_EXPOSING,			// Exposing dark or light frame
	CCD_IDLE				// Camera idle
};

enum QSICoolerState
{
	COOL_OFF,				// Cooler is off
	COOL_ON,				// Cooler is on
	COOL_ATAMBIENT,			// Cooler is on and regulating at ambient temperature (optional)
	COOL_GOTOAMBIENT,		// Cooler is on and ramping to ambient
	COOL_NOCONTROL,			// Cooler cannot be controlled on this camera; open loop
	COOL_INITIALIZING,		// Cooler control is initializing (optional -- displays "Please Wait")
	COOL_INCREASING,		// Cooler temperature is going up    NEW 2000/02/04
	COOL_DECREASING,		// Cooler temperature is going down  NEW 2000/02/04
	COOL_BROWNOUT			// Cooler brownout condition  NEW 2001/09/06
};
