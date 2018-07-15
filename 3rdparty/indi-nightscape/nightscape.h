/*
   Based on INDI Developers Manual Tutorial #3

   "Simple CCD Driver"

   We develop a simple CCD driver.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file nightscape.h
    \brief Construct a basic INDI CCD device that supports exposure & temperature settings. 
    It also gcollects an image and uploads it as a FITS file.
    \author Dirk Niggemann

    \example nightscape.h
    A simple CCD device that can capture images and control temperature. It returns a FITS image to the client. To build drivers for complex CCDs, please
    refer to the INDI Generic CCD driver template in INDI SVN (under 3rdparty).
*/

#pragma once

#include "indiccd.h"
#include "nsmsg.h"
#include "nschannel.h"
#include "nsdownload.h"
#include "nsstatus.h"
#include "nschannel-u.h"
#ifdef HAVE_D2XX
#include "nschannel-ftd.h"
#endif
#ifdef HAVE_SERIAL
#include "nschannel-ser.h"
#endif

class NightscapeCCD : public INDI::CCD
{
  public:
    NightscapeCCD() = default;
		virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

  protected:
    // General device functions
    bool Connect() override;
    bool Disconnect() override;
    const char *getDefaultName() override;
    bool initProperties() override;
    bool updateProperties() override;

    // CCD specific functions
    bool StartExposure(float duration) override;
    bool AbortExposure() override;
    int SetTemperature(double temperature) override;
    void TimerHit() override;
    bool saveConfigItems(FILE *fp) override;

	  
	  ISwitch  CoolerS [2];
	  ISwitchVectorProperty CoolerSP;
	   
		ISwitch	FanS[3];
		ISwitchVectorProperty FanSP;
		
		INumber CamNumN[1];
    INumberVectorProperty CamNumNP;

#ifdef HAVE_D2XX
#ifdef HAVE_SERIAL
 		ISwitch  D2xxS [3];
#else
		ISwitch  D2xxS [2];
#endif
#else 
#ifdef HAVE_SERIAL
		ISwitch  D2xxS [2];
#else
		ISwitch  D2xxS [1];
#endif
#endif
	  ISwitchVectorProperty D2xxSP;
	   
  private:
    // Utility functions
    float CalcTimeLeft();
    void setupParams();
    void grabImage();

    // Are we exposing?
    bool InExposure { false };
    bool InReadout { false };
    bool InDownload { false };
    int stat {0};
    int oldstat { 0};
    // Struct to keep timing
    struct timeval ExpStart { 0, 0 };

    float ExposureRequest { 0 };
    float TemperatureRequest { 0 };
    Nsmsg * m;
    NsChannel * cn;
    NsDownload * dn;
    NsStatus * st;
    int fanspeed {3 };
    int camnum { 1};

    bool cooler { true };
    float setTemp;
#ifdef HAVE_D2XX
    int useD2xx { 1 };
#else
    int useD2xx { 0 };
#endif
    bool bayer { true };
    bool dark { false };
    int ntemps {0};
    int backoffs {1};
};
