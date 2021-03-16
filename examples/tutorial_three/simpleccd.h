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

#pragma once

#include "indiccd.h"
#include "indielapsedtimer.h"

class SimpleCCD : public INDI::CCD
{
public:
    SimpleCCD() = default;

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

private:
    // Utility functions
    float CalcTimeLeft();
    void setupParams();
    void grabImage();

    // Are we exposing?
    bool InExposure { false };

    INDI::ElapsedTimer ExposureTimer;

    float ExposureRequest { 0 };
    float TemperatureRequest { 0 };
};
