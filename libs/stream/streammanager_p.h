/*
    Copyright (C) 2020 by Pawel Soja <kernel32.pl@gmail.com>
    Copyright (C) 2015 by Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2014 by geehalel <geehalel@gmail.com>

    Stream Recorder

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#pragma once

#include "indiapi.h"
#include "recorder/recordermanager.h"
#include "encoder/encodermanager.h"
#include "fpsmeter.h"
#include "uniquequeue.h"
#include "gammalut16.h"

#include <atomic>
#include <string>
#include <map>
#include <thread>

namespace INDI
{

class StreamManagerPrivate
{
public:
    bool initProperties();
    void ISGetProperties(const char *dev);

    bool ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n);
    bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n);
    bool ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n);

    void newFrame(const uint8_t * buffer, uint32_t nbytes);

    bool updateProperties();
    bool setStream(bool enable);

    const char *getDeviceName() const;

    void setSize(uint16_t width, uint16_t height);
    bool setPixelFormat(INDI_PIXEL_FORMAT pixelFormat, uint8_t pixelDepth);

    /**
     * @brief Thread processing frames and forwarding to recording and preview
     */
    void asyncStreamThread();

    // helpers
    static std::string expand(const std::string &fname, const std::map<std::string, std::string> &patterns);

    // Utility for record file
    bool startRecording();

    // Stop recording. Force stop even in abnormal state if needed.
    bool stopRecording(bool force = false);

    /**
     * @brief uploadStream Upload frame to client using the selected encoder
     * @param buffer pointer to frame image buffer
     * @param nbytes size of frame in bytes
     * @return True if frame is encoded and sent to client, false otherwise.
     */
    bool uploadStream(const uint8_t *buffer, uint32_t nbytes);

    /**
     * @brief recordStream Calls the backend recorder to record a single frame.
     * @param deltams time in milliseconds since last frame
     */
    bool recordStream(const uint8_t *buffer, uint32_t nbytes, double deltams);


public:
    DefaultDevice *currentDevice = nullptr;

    /* Stream switch */
    ISwitch StreamS[2];
    ISwitchVectorProperty StreamSP;

    /* Record switch */
    ISwitch RecordStreamS[4];
    ISwitchVectorProperty RecordStreamSP;
    enum
    {
        RECORD_ON,
        RECORD_TIME,
        RECORD_FRAME,
        RECORD_OFF
    };

    /* Record File Info */
    IText RecordFileT[2] {};
    ITextVectorProperty RecordFileTP;

    INumber StreamExposureN[2];
    INumberVectorProperty StreamExposureNP;
    enum
    {
        STREAM_EXPOSURE,
        STREAM_DIVISOR,
    };

    /* Measured FPS */
    INumber FpsN[2];
    INumberVectorProperty FpsNP;
    enum { FPS_INSTANT, FPS_AVERAGE };

    /* Record Options */
    INumber RecordOptionsN[2];
    INumberVectorProperty RecordOptionsNP;

    // Stream Frame
    INumberVectorProperty StreamFrameNP;
    INumber StreamFrameN[4];

    /* BLOBs */
    IBLOBVectorProperty *imageBP = nullptr;
    IBLOB *imageB = nullptr;

    // Encoder Selector. It's static now but should this implemented as plugin interface?
    ISwitch EncoderS[2];
    ISwitchVectorProperty EncoderSP;
    enum { ENCODER_RAW, ENCODER_MJPEG };

    // Recorder Selector. Static but should be implmeneted as a dynamic plugin interface
    ISwitch RecorderS[2];
    ISwitchVectorProperty RecorderSP;
    enum { RECORDER_RAW, RECORDER_OGV };

    // Limits. Maximum queue size for incoming frames. FPS Limit for preview
    INumber LimitsN[2];
    INumberVectorProperty LimitsNP;
    enum { LIMITS_BUFFER_MAX, LIMITS_PREVIEW_FPS };

    std::atomic<bool> isStreaming { false };
    std::atomic<bool> isRecording { false };
    std::atomic<bool> isRecordingAboutToClose { false };
    bool hasStreamingExposure { true };

    // Recorder
    RecorderManager recorderManager;
    RecorderInterface *recorder = nullptr;
    bool direct_record = false;
    std::string recordfiledir, recordfilename; /* in case we should move it */

    // Encoders
    EncoderManager encoderManager;
    EncoderInterface *encoder = nullptr;

    // Measure FPS
    FPSMeter FPSAverage;
    FPSMeter FPSFast;
    FPSMeter FPSPreview;
    FPSMeter FPSRecorder;

    uint32_t frameCountDivider = 0;

    INDI_PIXEL_FORMAT PixelFormat = INDI_MONO;
    uint8_t PixelDepth = 8;
    uint16_t rawWidth = 0, rawHeight = 0;
    std::string Format;

    // Processing for streaming
    typedef struct {
        double time;
        std::vector<uint8_t> frame;
    } TimeFrame;

    std::thread              framesThread;   // async incoming frames processing
    std::atomic<bool>        framesThreadTerminate;
    UniqueQueue<TimeFrame>   framesIncoming;

    std::mutex               fastFPSUpdate;
    std::mutex               recordMutex;

    GammaLut16               gammaLut16;
};

}
