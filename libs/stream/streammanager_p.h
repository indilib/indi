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

#include "indiccdchip.h"
#include "indisensorinterface.h"

/* Smart Widget-Property */
#include "indipropertytext.h"
#include "indipropertynumber.h"
#include "indipropertyswitch.h"
namespace INDI
{

class StreamManagerPrivate
{
public:
    struct FrameInfo
    {
        size_t x, y, w, h;
        size_t bytesPerColor;

        FrameInfo()
            : x(0)
            , y(0)
            , w(0)
            , h(0)
            , bytesPerColor(0)
        { }

        explicit FrameInfo(const CCDChip &ccdChip, size_t bytesPerColor = 1)
            : x(ccdChip.getSubX() / ccdChip.getBinX())
            , y(ccdChip.getSubY() / ccdChip.getBinY())
            , w(ccdChip.getSubW() / ccdChip.getBinX())
            , h(ccdChip.getSubH() / ccdChip.getBinY())
            , bytesPerColor(bytesPerColor)
        { }

        explicit FrameInfo(const SensorInterface &sensorInterface, size_t bytesPerColor = 1)
            : x(0)
            , y(0)
            , w(sensorInterface.getBufferSize() * 8 / sensorInterface.getBPS())
            , h(1)
            , bytesPerColor(bytesPerColor)
        { }

        size_t pixels() const
        { return w * h; }

        size_t totalSize() const
        { return w * h * bytesPerColor; }

        size_t lineSize() const
        { return w * bytesPerColor; }

        bool operator!=(const FrameInfo &other) const
        { return other.x != x || other.y != y || other.w != w || other.h != h; }
    };
public:
    StreamManagerPrivate(DefaultDevice *defaultDevice);
    virtual ~StreamManagerPrivate();

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

    void getStreamFrame(uint16_t * x, uint16_t * y, uint16_t * w, uint16_t * h) const;
    void setStreamFrame(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void setStreamFrame(const FrameInfo &frameInfo);

    FrameInfo updateSourceFrameInfo();

    static void subframe(
            const uint8_t *srcBuffer,
            const FrameInfo &srcFrameInfo,
            uint8_t *dstBuffer,
            const FrameInfo &dstFrameInfo
    );

public:
    DefaultDevice *currentDevice = nullptr;

    FrameInfo dstFrameInfo;

    /* Stream switch */
    INDI::PropertySwitch StreamSP {2};

    INDI::PropertyNumber StreamTimeNP {1};

    /* Record switch */
    INDI::PropertySwitch RecordStreamSP {4};
    enum
    {
        RECORD_ON,
        RECORD_TIME,
        RECORD_FRAME,
        RECORD_OFF
    };

    /* Record File Info */
    INDI::PropertyText RecordFileTP {2};

    INDI::PropertyNumber StreamExposureNP {2};
    enum
    {
        STREAM_EXPOSURE,
        STREAM_DIVISOR,
    };

    /* Measured FPS */
    INDI::PropertyNumber FpsNP {2};
    enum { FPS_INSTANT, FPS_AVERAGE };

    /* Record Options */
    INDI::PropertyNumber RecordOptionsNP {2};

    // Stream Frame
    INDI::PropertyNumber StreamFrameNP {4};

    /* BLOBs */
    INDI::PropertyView<IBLOB> *imageBP {nullptr};

    // Encoder Selector. It's static now but should this implemented as plugin interface?
    INDI::PropertySwitch EncoderSP {2};
    enum { ENCODER_RAW, ENCODER_MJPEG };

    // Recorder Selector. Static but should be implmeneted as a dynamic plugin interface
    INDI::PropertySwitch RecorderSP {2};
    enum { RECORDER_RAW, RECORDER_OGV };

    // Limits. Maximum queue size for incoming frames. FPS Limit for preview
    INDI::PropertyNumber LimitsNP {2};
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
    std::atomic<bool>        framesThreadTerminate {false};
    UniqueQueue<TimeFrame>   framesIncoming;

    std::mutex               fastFPSUpdate;
    std::mutex               recordMutex;

    GammaLut16               gammaLut16;
};

}
