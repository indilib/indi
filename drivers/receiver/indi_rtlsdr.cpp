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

#include "indi_rtlsdr.h"
#include <rtl-sdr.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <indilogger.h>
#include <memory>
#include <deque>
#include <indicom.h>

#define min(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a < _b ? _a : _b;      \
    })
#define MAX_TRIES      20
#define SUBFRAME_SIZE  (16384)
#define MIN_FRAME_SIZE (512)
#define MAX_FRAME_SIZE (SUBFRAME_SIZE * 16)
#define SPECTRUM_SIZE  (256)

static pthread_cond_t cv         = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;

void RTLSDR::Callback()
{
    b_read  = 0;
    to_read = getSampleRate() * IntegrationRequest * getBPS() / 8;
    setBufferSize(to_read);

    int len            = min(MAX_FRAME_SIZE, to_read);
    int olen           = 0;
    unsigned char *buf = (unsigned char *)malloc(len);
    if((getSensorConnection() & CONNECTION_TCP) == 0)
        rtlsdr_reset_buffer(rtl_dev);
    else
        tcflush(PortFD, TCOFLUSH);
    setIntegrationTime(IntegrationRequest);
    while (InIntegration)
    {
        if((getSensorConnection() & CONNECTION_TCP) == 0)
            rtlsdr_read_sync(rtl_dev, buf, len, &olen);
        else
            olen = read(PortFD, buf, len);
        if(olen < 0 )
            AbortIntegration();
        else {
            buffer = buf;
            n_read = olen;
            grabData();
        }
    }
}

static class Loader
{
    std::deque<std::unique_ptr<RTLSDR>> receivers;
public:
    Loader()
    {
        size_t numofConnectedReceivers = rtlsdr_get_device_count();
        if (numofConnectedReceivers == 0)
        {
            //Try sending IDMessage as well?
            IDLog("No USB RTLSDR receivers detected. Power on?");// Trying with TCP..");
            IDMessage(nullptr, "No USB RTLSDR receivers detected. Power on?");// Trying with TCP..");
            // receivers.push_back(std::unique_ptr<RTLSDR>(new RTLSDR(-1)));
            return;
        }

        for (size_t i = 0; i < numofConnectedReceivers; i++)
        {
            receivers.push_back(std::unique_ptr<RTLSDR>(new RTLSDR(i)));
        }

    }
} loader;

RTLSDR::RTLSDR(int32_t index)
{
    InIntegration = false;
    if(index<0) {
        setSensorConnection(CONNECTION_TCP);
    }

    receiverIndex = index;

    char name[MAXINDIDEVICE];
    snprintf(name, MAXINDIDEVICE, "%s %s%c", getDefaultName(), index < 0 ? "TCP" : "USB", index < 0 ? '\0' : index + '1');
    setDeviceName(name);
    // We set the Receiver capabilities
    uint32_t cap = SENSOR_CAN_ABORT | SENSOR_HAS_STREAMING | SENSOR_HAS_DSP;
    SetReceiverCapability(cap);

}

bool RTLSDR::Connect()
{
    if((getSensorConnection() & CONNECTION_TCP) == 0) {
        int r = rtlsdr_open(&rtl_dev, static_cast<uint32_t>(receiverIndex));
        if (r < 0)
        {
            LOGF_ERROR("Failed to open rtlsdr device index %d.", receiverIndex);
            return false;
        }
    }
    return true;
}
/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool RTLSDR::Disconnect()
{
    InIntegration = false;
    if((getSensorConnection() & CONNECTION_TCP) == 0) {
        rtlsdr_close(rtl_dev);
    }
    PortFD = -1;

    setBufferSize(1);
    pthread_mutex_lock(&condMutex);
    streamPredicate = 1;
    terminateThread = true;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);
    LOG_INFO("RTL-SDR Receiver disconnected successfully!");
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
    INDI::Receiver::initProperties();

    setMinMaxStep("SENSOR_INTEGRATION", "SENSOR_INTEGRATION_VALUE", 0.001, 600, 0.001, false);
    setMinMaxStep("RECEIVER_SETTINGS", "RECEIVER_FREQUENCY", 2.4e+7, 2.0e+9, 1, false);
    setMinMaxStep("RECEIVER_SETTINGS", "RECEIVER_SAMPLERATE", 2.5e+5, 2.0e+6, 2.5e+5, false);
    setMinMaxStep("RECEIVER_SETTINGS", "RECEIVER_GAIN", 0.0, 25.0, 0.1, false);
    setMinMaxStep("RECEIVER_SETTINGS", "RECEIVER_BANDWIDTH", 2.5e+5, 2.0e+6, 2.5e+5, false);
    setMinMaxStep("RECEIVER_SETTINGS", "RECEIVER_BITSPERSAMPLE", 16, 16, 0, false);
    setIntegrationFileExtension("fits");

    // Add Debug, Simulator, and Configuration controls
    addAuxControls();

    setDefaultPollingPeriod(500);
    return true;
}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool RTLSDR::updateProperties()
{
    // Call parent update properties first
    INDI::Receiver::updateProperties();

    if (isConnected())
    {
        // Inital values
        setupParams(1000000, 1420000000, 10);

        // Start the timer
        SetTimer(getCurrentPollingPeriod());
    }

    return true;
}

/**************************************************************************************
** Setting up Receiver parameters
***************************************************************************************/
void RTLSDR::setupParams(float sr, float freq, float gain)
{
    int r = 0;

    if((getSensorConnection() & CONNECTION_TCP) == 0) {
        r |= rtlsdr_set_tuner_gain_mode(rtl_dev, 1);
        r |= rtlsdr_set_tuner_gain(rtl_dev, static_cast<int>(gain * 10));
        r |= rtlsdr_set_center_freq(rtl_dev, static_cast<uint32_t>(freq));
        r |= rtlsdr_set_sample_rate(rtl_dev, static_cast<uint32_t>(sr));
        r |= rtlsdr_set_tuner_bandwidth(rtl_dev, static_cast<uint32_t>(sr));
        if (r != 0)
        {
            LOG_INFO("Issue(s) setting parameters.");
        }

        setBPS(16);
        setGain(static_cast<double>(rtlsdr_get_tuner_gain(rtl_dev))/10.0);
        setFrequency(static_cast<double>(rtlsdr_get_center_freq(rtl_dev)));
        setSampleRate(static_cast<double>(rtlsdr_get_sample_rate(rtl_dev)));
        setBandwidth(static_cast<double>(rtlsdr_get_sample_rate(rtl_dev)));
    } else {
        sendTcpCommand(CMD_SET_FREQ, static_cast<int>(freq));
        sendTcpCommand(CMD_SET_SAMPLE_RATE, static_cast<int>(sr));
        sendTcpCommand(CMD_SET_TUNER_GAIN_MODE, 0);
        sendTcpCommand(CMD_SET_GAIN, static_cast<int>(gain * 10));
        sendTcpCommand(CMD_SET_FREQ_COR, 0);
        sendTcpCommand(CMD_SET_AGC_MODE, 0);
        sendTcpCommand(CMD_SET_TUNER_GAIN_INDEX, 0);

        setBPS(16);
        setGain(gain);
        setFrequency(freq);
        setSampleRate(sr);
        setBandwidth(sr);
    }
}

bool RTLSDR::sendTcpCommand(int cmd, int value)
{
    unsigned char tosend[5];
    tosend[0] = static_cast<unsigned char>(cmd);
    tosend[1] = value&0xff;
    value >>= 8;
    tosend[2] = value&0xff;
    value >>= 8;
    tosend[3] = value&0xff;
    value >>= 8;
    tosend[4] = value&0xff;
    value >>= 8;
    tcflush(PortFD, TCOFLUSH);
    int count = 0;
    while(count < 5) {
        count = write(PortFD, tosend, 5);
        if (count < 0) return false;
    }
    return true;
}

bool RTLSDR::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    bool r = false;
    if (dev && !strcmp(dev, getDeviceName()) && !strcmp(name, ReceiverSettingsNP.name)) {
        for(int i = 0; i < n; i++) {
            if (!strcmp(names[i], "RECEIVER_GAIN")) {
                setupParams(getSampleRate(), getFrequency(), values[i]);
            } else if (!strcmp(names[i], "RECEIVER_FREQUENCY")) {
                setupParams(getSampleRate(), values[i], getGain());
            } else if (!strcmp(names[i], "RECEIVER_SAMPLERATE")) {
                setupParams(values[i], getFrequency(), getGain());
                setMinMaxStep("RECEIVER_SETTINGS", "RECEIVER_BANDWIDTH", getSampleRate(), getSampleRate(), getSampleRate(), false);
            }
        }
        values[RECEIVER_GAIN] = getGain();
        values[RECEIVER_BANDWIDTH] = getBandwidth();
        values[RECEIVER_FREQUENCY] = getFrequency();
        values[RECEIVER_SAMPLERATE] = getSampleRate();
        values[RECEIVER_BITSPERSAMPLE] = 16;
        IUUpdateNumber(&ReceiverSettingsNP, values, names, n);
        IDSetNumber(&ReceiverSettingsNP, nullptr);
    }
    return processNumber(dev, name, values, names, n) & !r;
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool RTLSDR::StartIntegration(double duration)
{
    IntegrationRequest = static_cast<float>(duration);
    AbortIntegration();

    LOG_INFO("Integration started...");
    // Run threads
    std::thread(&RTLSDR::Callback, this).detach();
    gettimeofday(&IntStart, nullptr);
    InIntegration = true;
    return true;

    // We're done
    return false;
}

/**************************************************************************************
** Client is asking us to abort a capture
***************************************************************************************/
bool RTLSDR::AbortIntegration()
{
    if (InIntegration)
    {
        InIntegration = false;
    }
    return true;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float RTLSDR::CalcTimeLeft()
{
    float timesince;
    float timeleft;
    struct timeval now;
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                (double)(IntStart.tv_sec * 1000.0 + IntStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = IntegrationRequest - timesince;
    return timeleft;
}

/**************************************************************************************
** Main device loop. We check for capture progress here
***************************************************************************************/
void RTLSDR::TimerHit()
{
    long timeleft;

    if (isConnected() == false)
        return; //  No need to reset timer if we are not connected anymore

    if (InIntegration)
    {
        timeleft = static_cast<long>(CalcTimeLeft());
        if (timeleft < 0.1)
        {
            /* We're done capturing */
            LOG_INFO("Integration done, expecting data...");
            timeleft = 0.0;
        }

        // This is an over simplified timing method, check ReceiverSimulator and rtlsdrReceiver for better timing checks
        setIntegrationLeft(timeleft);
    }

    SetTimer(getCurrentPollingPeriod());
    return;
}

/**************************************************************************************
** Create the spectrum
***************************************************************************************/
void RTLSDR::grabData()
{
    if (InIntegration)
    {
        n_read    = min(to_read, n_read);
        continuum = getBuffer();
        if (n_read > 0)
        {
            memcpy(continuum + b_read, buffer, n_read);
            b_read += n_read;
            to_read -= n_read;
        }

        if (to_read <= 0)
        {
            InIntegration = false;
            if(!streamPredicate) {
                LOG_INFO("Download complete.");
                IntegrationComplete();
            } else {
                StartIntegration(1.0 / Streamer->getTargetFPS());
                int32_t size = getBufferSize();
                Streamer->newFrame(getBuffer(), size);
            }
        }
    }
}

//Streamer API functions

bool RTLSDR::StartStreaming()
{
    pthread_mutex_lock(&condMutex);
    streamPredicate = 1;
    StartIntegration(1.0 / Streamer->getTargetFPS());
    pthread_mutex_unlock(&condMutex);
    pthread_cond_signal(&cv);

    return true;
}

bool RTLSDR::StopStreaming()
{
    pthread_mutex_lock(&condMutex);
    streamPredicate = 0;
    pthread_mutex_unlock(&condMutex);
    pthread_cond_signal(&cv);

    return true;
}

bool RTLSDR::Handshake()
{
    if(getSensorConnection() & CONNECTION_TCP) {
        if(PortFD == -1) {
            LOG_ERROR("Failed to connect to rtl_tcp server.");
            return false;
        }
    }

    streamPredicate = 0;
    terminateThread = false;
    LOG_INFO("RTL-SDR Receiver connected successfully!");
    // Let's set a timer that checks teleReceivers status every POLLMS milliseconds.
    // JM 2017-07-31 SetTimer already called in updateProperties(). Just call it once
    //SetTimer(getCurrentPollingPeriod());

    return true;
}

