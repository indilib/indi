/*
    indi_rtlsdr - a software defined radio driver for INDI
    Copyright (C) 2017  Ilia Platone - Jasem Mutlaq
    Collaborators:
        - Ilia Platone <info@iliaplatone.com>
        - Jasem Mutlaq - INDI library - <http://indilib.org>
        - Monroe Pattillo - Fox Observatory - South Florida Amateur Astronomers Association <http://www.sfaaa.com/>

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

#include <rtl-sdr.h>
#include "indireceiver.h"
#include "stream/streammanager.h"

enum Settings
{
    FREQUENCY_N = 0,
    SAMPLERATE_N,
    BANDWIDTH_N,
    NUM_SETTINGS
};
class RTLSDR : public INDI::Receiver
{
  public:
    RTLSDR(int32_t index);

    void grabData();
    rtlsdr_dev *rtl_dev = { nullptr };
    int to_read;
    // Are we integrating?
    bool InIntegration;
    uint8_t *buffer;
    int b_read, n_read;
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

  protected:
    // General device functions
    bool Connect() override;
    bool Disconnect() override;
    const char *getDefaultName() override;
    bool initProperties() override;
    bool updateProperties() override;

    // Receiver specific functions
    bool StartIntegration(double duration) override;
    void setupParams(float sr, float freq, float gain);
    bool AbortIntegration() override;
    void TimerHit() override;

    bool StartStreaming() override;
    bool StopStreaming() override;
    void * streamCapture();

    bool Handshake() override;

  private:
    void Callback();

    // Utility functions
    float CalcTimeLeft();

    // Struct to keep timing
    struct timeval IntStart;
    float IntegrationRequest;
    uint8_t *continuum;

    int32_t receiverIndex = { 0 };

    int streamPredicate;
    pthread_t primary_thread;
    bool terminateThread;

    bool sendTcpCommand(int cmd, int value);
    enum TcpCommands {
        CMD_SET_FREQ = 0x1,
        CMD_SET_SAMPLE_RATE = 0x2,
        CMD_SET_TUNER_GAIN_MODE = 0x3,
        CMD_SET_GAIN = 0x4,
        CMD_SET_FREQ_COR = 0x5,
        CMD_SET_AGC_MODE = 0x8,
        CMD_SET_TUNER_GAIN_INDEX = 0xd
    };
};
