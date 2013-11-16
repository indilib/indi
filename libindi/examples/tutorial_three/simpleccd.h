#ifndef SIMPLECCD_H
#define SIMPLECCD_H

/*
   INDI Developers Manual
   Tutorial #3

   "Simple CCD Driver"

   We develop a simple CCD driver.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file simpleccd.h
    \brief Construct a basic INDI CCD device that simulates exposure & temperature settings. It also generates a random pattern and uploads it as a FITS file.
    \author Jasem Mutlaq

    \example simpleccd.h
    A simple CCD device that can capture images and control temperature. It returns a FITS image to the client. To build drivers for complex CCDs, please
    refer to the INDI Generic CCD driver template in INDI SVN (under 3rdparty).
*/


#include "indibase/indiccd.h"

class SimpleCCD : public INDI::CCD
{
public:
    SimpleCCD();

    bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    void ISGetProperties(const char *dev);

protected:
    // General device functions
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();
    bool initProperties();
    bool updateProperties();

    // CCD specific functions
    bool StartExposure(float duration);
    bool AbortExposure();
    void TimerHit();
    void addFITSKeywords(fitsfile *fptr, CCDChip *targetChip);

private:
    // Utility functions
    float CalcTimeLeft();
    void  setupParams();
    void  grabImage();

    // Are we exposing?
    bool InExposure;
    // Struct to keep timing
    struct timeval ExpStart;

    float ExposureRequest;
    float TemperatureRequest;
    int   timerID;

    // We declare the CCD temperature property
    INumber TemperatureN[1];
    INumberVectorProperty TemperatureNP;

};

#endif // SIMPLECCD_H
