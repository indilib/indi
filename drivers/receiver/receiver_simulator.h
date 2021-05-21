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

#pragma once

#include "indireceiver.h"
#include "stream/streammanager.h"
#include "dsp/convolution.h"
#include "dsp/transforms.h"

enum Settings
{
    FREQUENCY_N = 0,
    SAMPLERATE_N,
    BANDWIDTH_N,
    NUM_SETTINGS
};
class RadioSim : public INDI::Receiver
{
    public:
        RadioSim();
        ~RadioSim();

        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        // General device functions
        bool Connect() override;
        bool Disconnect() override;
        const char *getDefaultName() override;
        bool initProperties() override;
        bool updateProperties() override;

        // Detector specific functions
        bool StartIntegration(double duration) override;
        bool AbortIntegration() override;
        void TimerHit() override;

        bool StartStreaming() override;
        bool StopStreaming() override;
        void streamCaptureHelper();
        void * streamCapture();
        void grabData();

    private:

        // Utility functions
        float CalcTimeLeft();
        void setupParams(float sr, float freq, float bw, float gain);
        struct timeval CapStart;
        double IntegrationRequest;

        int streamPredicate;
        pthread_t primary_thread;
        bool terminateThread;
};
