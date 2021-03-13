/*
   INDI Developers Manual
   Tutorial #3

   "Simple Spectrograph Driver"

   We develop a simple Spectrograph driver.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file simpleSpectrograph.h
    \brief Construct a basic INDI Spectrograph device that simulates capture & temperature settings. It also generates a random pattern and uploads it as a FITS file.
    \author Ilia Platone

    \example simpleSpectrograph.h
    A simple detector device that can capture stream frames and controls temperature. It returns a FITS image to the client. To build drivers for complex Spectrographs, please
    refer to the INDI Generic Spectrograph driver template in INDI github (under 3rdparty).
*/

#pragma once

#include "indispectrograph.h"

class SimpleSpectrograph : public INDI::Spectrograph
{
public:
    SimpleSpectrograph() = default;

protected:
    // General device functions
    bool Connect() override;
    bool Disconnect() override;
    const char *getDefaultName() override;
    bool initProperties() override;
    bool updateProperties() override;

    // Spectrograph specific functions
    bool StartIntegration(double duration) override;
    bool AbortIntegration() override;
    int SetTemperature(double temperature) override;
    void TimerHit() override;

    bool paramsUpdated(float sr, float freq, float bps, float bw, float gain);

private:
    // Utility functions
    float CalcTimeLeft();
    void setupParams();
    void grabFrame();

    // Are we exposing?
    bool InIntegration { false };
    // Struct to keep timing
    struct timeval CapStart { 0, 0 };

    double IntegrationRequest { 0 };
    double TemperatureRequest { 0 };
};
