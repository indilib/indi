/*
   INDI Developers Manual
   Tutorial #3

   "Simple Receiver Driver"

   We develop a simple Receiver driver.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file simpleReceiver.h
    \brief Construct a basic INDI Receiver device that simulates capture & temperature settings. It also generates a random pattern and uploads it as a FITS file.
    \author Ilia Platone

    \example simpleReceiver.h
    A simple detector device that can capture stream frames and controls temperature. It returns a FITS image to the client. To build drivers for complex Receivers, please
    refer to the INDI Generic Receiver driver template in INDI github (under 3rdparty).
*/

#pragma once

#include "indireceiver.h"

class SimpleReceiver : public INDI::Receiver
{
public:
    SimpleReceiver() = default;

protected:
    // General device functions
    bool Connect() override;
    bool Disconnect() override;
    const char *getDefaultName() override;
    bool initProperties() override;
    bool updateProperties() override;

    // Receiver specific functions
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
