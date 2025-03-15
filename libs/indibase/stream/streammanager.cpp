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

#include <config.h>
#include "streammanager.h"
#include "streammanager_p.h"
#include "indiccd.h"
#include "indisensorinterface.h"
#include "indilogger.h"
#include "indiutility.h"
#include "indisinglethreadpool.h"
#include "indielapsedtimer.h"

#include <cerrno>
#include <sys/stat.h>

#include <algorithm>

static const char * STREAM_TAB = "Streaming";

namespace INDI
{

StreamManagerPrivate::StreamManagerPrivate(DefaultDevice *defaultDevice)
    : currentDevice(defaultDevice)
{
    FPSAverage.setTimeWindow(1000);
#ifdef __arm__
    FPSFast.setTimeWindow(500);
#else
    FPSFast.setTimeWindow(100);
#endif

    recorder = recorderManager.getDefaultRecorder();

    LOGF_DEBUG("Using default recorder (%s)", recorder->getName());

    encoder = encoderManager.getDefaultEncoder();

    encoder->init(currentDevice);

    LOGF_DEBUG("Using default encoder (%s)", encoder->getName());

    framesThread = std::thread(&StreamManagerPrivate::asyncStreamThread, this);
}

StreamManagerPrivate::~StreamManagerPrivate()
{
    if (framesThread.joinable())
    {
        framesThreadTerminate = true;
        framesIncoming.abort();
        framesThread.join();
    }
}

StreamManager::StreamManager(DefaultDevice *mainDevice)
    : d_ptr(new StreamManagerPrivate(mainDevice))
{ }

StreamManager::~StreamManager()
{ }

const char * StreamManagerPrivate::getDeviceName() const
{
    return currentDevice->getDeviceName();
}

const char * StreamManager::getDeviceName() const
{
    D_PTR(const StreamManager);
    return d->getDeviceName();
}

bool StreamManagerPrivate::initProperties()
{
    /* Video Stream */
    // @INDI_STANDARD_PROPERTY@
    StreamSP[0].fill("STREAM_ON",  "Stream On",  ISS_OFF);
    StreamSP[1].fill("STREAM_OFF", "Stream Off", ISS_ON);
    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        StreamSP.fill(getDeviceName(), "SENSOR_DATA_STREAM", "Video Stream",
                      STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    else
        StreamSP.fill(getDeviceName(), "CCD_VIDEO_STREAM", "Video Stream",
                      STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // @INDI_STANDARD_PROPERTY@
    StreamTimeNP[0].fill("STREAM_DELAY_TIME", "Delay (s)", "%.3f", 0, 60, 0.001, 0);
    StreamTimeNP.fill(getDeviceName(), "STREAM_DELAY", "Video Stream Delay", STREAM_TAB, IP_RO, 0, IPS_IDLE);

    // @INDI_STANDARD_PROPERTY@
    StreamExposureNP[STREAM_EXPOSURE].fill("STREAMING_EXPOSURE_VALUE", "Duration (s)", "%.6f", 0.000001, 60, 0.1, 0.1);
    StreamExposureNP[STREAM_DIVISOR ].fill("STREAMING_DIVISOR_VALUE",  "Divisor",      "%.f",  1,        15, 1.0, 1.0);
    StreamExposureNP.fill(getDeviceName(), "STREAMING_EXPOSURE", "Expose", STREAM_TAB, IP_RW, 60, IPS_IDLE);

    /* Measured FPS */
    // @INDI_STANDARD_PROPERTY@
    FpsNP[FPS_INSTANT].fill("EST_FPS", "Instant.",         "%.2f", 0.0, 999.0, 0.0, 30);
    FpsNP[FPS_AVERAGE].fill("AVG_FPS", "Average (1 sec.)", "%.2f", 0.0, 999.0, 0.0, 30);
    FpsNP.fill(getDeviceName(), "FPS", "FPS", STREAM_TAB, IP_RO, 60, IPS_IDLE);

    /* Record Frames */
    /* File */
    // @INDI_STANDARD_PROPERTY@
    std::string defaultDirectory = std::string(getenv("HOME")) + std::string("/Videos/indi__D_");
    RecordFileTP[0].fill("RECORD_FILE_DIR", "Dir.", defaultDirectory.data());
    RecordFileTP[1].fill("RECORD_FILE_NAME", "Name", "indi_record__T_");
    RecordFileTP.fill(getDeviceName(), "RECORD_FILE", "Record File",
                      STREAM_TAB, IP_RW, 0, IPS_IDLE);

    /* Record Options */
    // @INDI_STANDARD_PROPERTY@
    RecordOptionsNP[0].fill("RECORD_DURATION",    "Duration (sec)", "%.3f", 0.001,    999999.0, 0.0,  1.0);
    RecordOptionsNP[1].fill("RECORD_FRAME_TOTAL", "Frames",          "%.f", 1.0,   999999999.0, 1.0, 30.0);
    RecordOptionsNP.fill(getDeviceName(), "RECORD_OPTIONS",
                         "Record Options", STREAM_TAB, IP_RW, 60, IPS_IDLE);

    /* Record Switch */
    // @INDI_STANDARD_PROPERTY@
    RecordStreamSP[RECORD_ON   ].fill("RECORD_ON",          "Record On",         ISS_OFF);
    RecordStreamSP[RECORD_TIME ].fill("RECORD_DURATION_ON", "Record (Duration)", ISS_OFF);
    RecordStreamSP[RECORD_FRAME].fill("RECORD_FRAME_ON",    "Record (Frames)",   ISS_OFF);
    RecordStreamSP[RECORD_OFF  ].fill("RECORD_OFF",         "Record Off",        ISS_ON);
    RecordStreamSP.fill(getDeviceName(), "RECORD_STREAM", "Video Record", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        // CCD Streaming Frame
        // @INDI_STANDARD_PROPERTY@
        StreamFrameNP[0].fill("X",      "Left",   "%.f", 0, 0, 0, 0);
        StreamFrameNP[1].fill("Y",      "Top",    "%.f", 0, 0, 0, 0);
        StreamFrameNP[2].fill("WIDTH",  "Width",  "%.f", 0, 0, 0, 0);
        StreamFrameNP[3].fill("HEIGHT", "Height", "%.f", 0, 0, 0, 0);
        StreamFrameNP.fill(getDeviceName(), "CCD_STREAM_FRAME", "Frame", STREAM_TAB, IP_RW,
                           60, IPS_IDLE);
    }

    // Encoder Selection
    // @INDI_STANDARD_PROPERTY@
    EncoderSP[ENCODER_RAW  ].fill("RAW",   "RAW",   ISS_ON);
    EncoderSP[ENCODER_MJPEG].fill("MJPEG", "MJPEG", ISS_OFF);
    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        EncoderSP.fill(getDeviceName(), "SENSOR_STREAM_ENCODER", "Encoder", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    else
        EncoderSP.fill(getDeviceName(), "CCD_STREAM_ENCODER",    "Encoder", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Recorder Selector
    // @INDI_STANDARD_PROPERTY@
    RecorderSP[RECORDER_RAW].fill("SER", "SER", ISS_ON);
    RecorderSP[RECORDER_OGV].fill("OGV", "OGV", ISS_OFF);
    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        RecorderSP.fill(getDeviceName(), "SENSOR_STREAM_RECORDER", "Recorder", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    else
        RecorderSP.fill(getDeviceName(), "CCD_STREAM_RECORDER",    "Recorder", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // If we do not have theora installed, let's just define SER default recorder
#ifndef HAVE_THEORA
    RecorderSP.resize(1);
#endif

    // Limits
    // @INDI_STANDARD_PROPERTY@
    LimitsNP[LIMITS_BUFFER_MAX ].fill("LIMITS_BUFFER_MAX",  "Maximum Buffer Size (MB)", "%.0f", 1, 1024 * 64, 1, 512);
    LimitsNP[LIMITS_PREVIEW_FPS].fill("LIMITS_PREVIEW_FPS", "Maximum Preview FPS",      "%.0f", 1, 120,     1,  10);
    LimitsNP.fill(getDeviceName(), "LIMITS", "Limits", STREAM_TAB, IP_RW, 0, IPS_IDLE);
    return true;
}

bool StreamManager::initProperties()
{
    D_PTR(StreamManager);
    return d->initProperties();
}

void StreamManagerPrivate::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(getDeviceName(), dev))
        return;

    if (currentDevice->isConnected())
    {
        currentDevice->defineProperty(StreamSP);
        if (hasStreamingExposure)
            currentDevice->defineProperty(StreamExposureNP);
        currentDevice->defineProperty(FpsNP);
        currentDevice->defineProperty(RecordStreamSP);
        currentDevice->defineProperty(RecordFileTP);
        currentDevice->defineProperty(RecordOptionsNP);
        currentDevice->defineProperty(StreamFrameNP);
        currentDevice->defineProperty(EncoderSP);
        currentDevice->defineProperty(RecorderSP);
        currentDevice->defineProperty(LimitsNP);
    }
}

void StreamManager::ISGetProperties(const char * dev)
{
    D_PTR(StreamManager);
    d->ISGetProperties(dev);
}

bool StreamManagerPrivate::updateProperties()
{
    if (currentDevice->isConnected())
    {
        if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
        {
            imageBP = currentDevice->getBLOB("CCD1");
        }
        if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        {
            imageBP = currentDevice->getBLOB("SENSOR");
        }

        currentDevice->defineProperty(StreamSP);
        currentDevice->defineProperty(StreamTimeNP);
        if (hasStreamingExposure)
            currentDevice->defineProperty(StreamExposureNP);
        currentDevice->defineProperty(FpsNP);
        currentDevice->defineProperty(RecordStreamSP);
        currentDevice->defineProperty(RecordFileTP);
        currentDevice->defineProperty(RecordOptionsNP);
        currentDevice->defineProperty(StreamFrameNP);
        currentDevice->defineProperty(EncoderSP);
        currentDevice->defineProperty(RecorderSP);
        currentDevice->defineProperty(LimitsNP);
    }
    else
    {
        currentDevice->deleteProperty(StreamSP.getName());
        currentDevice->deleteProperty(StreamTimeNP.getName());
        if (hasStreamingExposure)
            currentDevice->deleteProperty(StreamExposureNP.getName());
        currentDevice->deleteProperty(FpsNP.getName());
        currentDevice->deleteProperty(RecordFileTP.getName());
        currentDevice->deleteProperty(RecordStreamSP.getName());
        currentDevice->deleteProperty(RecordOptionsNP.getName());
        currentDevice->deleteProperty(StreamFrameNP.getName());
        currentDevice->deleteProperty(EncoderSP.getName());
        currentDevice->deleteProperty(RecorderSP.getName());
        currentDevice->deleteProperty(LimitsNP.getName());
    }

    return true;
}

bool StreamManager::updateProperties()
{
    D_PTR(StreamManager);
    return d->updateProperties();
}

/*
 * The camera driver is expected to send the FULL FRAME of the Camera after BINNING without any subframing at all
 * Subframing for streaming/recording is done in the stream manager.
 * Therefore nbytes is expected to be SubW/BinX * SubH/BinY * Bytes_Per_Pixels * Number_Color_Components
 * Binned frame must be sent from the camera driver for this to work consistentaly for all drivers.*/
void StreamManagerPrivate::newFrame(const uint8_t * buffer, uint32_t nbytes, uint64_t timestamp)
{
    // close the data stream on the same thread as the data stream
    // manually triggered to stop recording.
    if (isRecordingAboutToClose)
    {
        stopRecording();
        return;
    }

    // Discard every N frame.
    // do not count it to fps statistics
    // N is StreamExposureNP[STREAM_DIVISOR].getValue()
    ++frameCountDivider;
    if (
        (StreamExposureNP[STREAM_DIVISOR].getValue() > 1) &&
        (frameCountDivider % static_cast<int>(StreamExposureNP[STREAM_DIVISOR].getValue())) == 0
    )
    {
        return;
    }

    if (FPSAverage.newFrame())
    {
        FpsNP[1].setValue(FPSAverage.framesPerSecond());
    }

    if (FPSFast.newFrame())
    {
        FpsNP[0].setValue(FPSFast.framesPerSecond());
        if (fastFPSUpdate.try_lock()) // don't block stream thread / record thread
            std::thread([&]()
        {
            FpsNP.apply();
            fastFPSUpdate.unlock();
        }).detach();
    }

    if (isStreaming || (isRecording && !isRecordingAboutToClose))
    {
        size_t allocatedSize = nbytes * framesIncoming.size() / 1024 / 1024; // allocated size in MB
        if (allocatedSize > LimitsNP[LIMITS_BUFFER_MAX].getValue())
        {
            LOG_WARN("Frame buffer is full, skipping frame...");
            return;
        }

        std::vector<uint8_t> copyBuffer(buffer, buffer + nbytes); // copy the frame

        framesIncoming.push(TimeFrame{FPSFast.deltaTime(), timestamp, std::move(copyBuffer)}); // push it into the queue
    }

    if (isRecording && !isRecordingAboutToClose)
    {
        FPSRecorder.newFrame(); // count frames and total time

        // captured all frames, stream should be close
        if (
            (RecordStreamSP[RECORD_FRAME].getState() == ISS_ON && FPSRecorder.totalFrames() >= (RecordOptionsNP[1].getValue())) ||
            (RecordStreamSP[RECORD_TIME ].getState() == ISS_ON && FPSRecorder.totalTime()   >= (RecordOptionsNP[0].getValue() * 1000.0))
        )
        {
            LOG_INFO("Waiting for all buffered frames to be recorded");
            framesIncoming.waitForEmpty();
            // duplicated message
#if 0
            LOGF_INFO(
                "Ending record after %g millisecs and %d frames",
                FPSRecorder.totalTime(),
                FPSRecorder.totalFrames()
            );
#endif
            RecordStreamSP.reset();
            RecordStreamSP[RECORD_OFF].setState(ISS_ON);
            RecordStreamSP.setState(IPS_IDLE);
            RecordStreamSP.apply();

            stopRecording();
        }
    }
}

void StreamManager::newFrame(const uint8_t * buffer, uint32_t nbytes, uint64_t timestamp)
{
    D_PTR(StreamManager);
    d->newFrame(buffer, nbytes, timestamp);
}


StreamManagerPrivate::FrameInfo StreamManagerPrivate::updateSourceFrameInfo()
{
    FrameInfo srcFrameInfo;

    uint8_t components = (PixelFormat == INDI_RGB) ? 3 : 1;
    uint8_t bytesPerComponent = (PixelDepth + 7) / 8;

    dstFrameInfo.bytesPerColor = components * bytesPerComponent;

    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        srcFrameInfo = FrameInfo(
                           dynamic_cast<const INDI::CCD*>(currentDevice)->PrimaryCCD,
                           components * bytesPerComponent
                       );
    }
    else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
    {
        srcFrameInfo = FrameInfo(
                           *dynamic_cast<const INDI::SensorInterface*>(currentDevice),
                           components * bytesPerComponent
                       );
    }

    // If stream frame was not yet initialized, let's do that now
    if (dstFrameInfo.pixels() == 0)
    {
        //if (dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getNAxis() == 2)
        //    binFactor = dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinX();
        dstFrameInfo = srcFrameInfo;
        setStreamFrame(dstFrameInfo);
        StreamFrameNP.setState(IPS_IDLE);
        StreamFrameNP.apply();
    }

    return srcFrameInfo;
}

void StreamManagerPrivate::subframe(
    const uint8_t *srcBuffer,
    const FrameInfo &srcFrameInfo,
    uint8_t *dstBuffer,
    const FrameInfo &dstFrameInfo
)
{
    size_t   srcOffset = srcFrameInfo.bytesPerColor * (dstFrameInfo.y * srcFrameInfo.w + dstFrameInfo.x);
    uint32_t srcStride = srcFrameInfo.lineSize();
    uint32_t dstStride = dstFrameInfo.lineSize();

    srcBuffer += srcOffset;

    // Copy line-by-line
    for (size_t i = 0; i < dstFrameInfo.h; ++i)
    {
        memcpy(dstBuffer, srcBuffer, dstStride);
        dstBuffer += dstStride;
        srcBuffer += srcStride;
    }
}

void StreamManagerPrivate::asyncStreamThread()
{
    TimeFrame sourceTimeFrame;
    sourceTimeFrame.time = 0;

    std::vector<uint8_t> subframeBuffer;  // Subframe buffer for recording/streaming
    std::vector<uint8_t> downscaleBuffer; // Downscale buffer for streaming

    INDI::SingleThreadPool previewThreadPool;
    INDI::ElapsedTimer previewElapsed;

    while(!framesThreadTerminate)
    {
        if (framesIncoming.pop(sourceTimeFrame) == false)
            continue;

        FrameInfo srcFrameInfo = updateSourceFrameInfo();

        std::vector<uint8_t> *sourceBuffer = &sourceTimeFrame.frame;

        // Source buffer size may be equal or larger than frame info size
        // as some driver still retain full unbinned window size even when binning the output
        // frame
        if (PixelFormat != INDI_JPG && sourceBuffer->size() < srcFrameInfo.totalSize())
        {
            LOGF_ERROR("Source buffer size %d is less than frame size %d, skipping frame...", sourceBuffer->size(),
                       srcFrameInfo.totalSize());
            continue;
        }

        // Check if we need to subframe
        if (
            PixelFormat != INDI_JPG &&
            dstFrameInfo.pixels() != 0 &&
            dstFrameInfo != srcFrameInfo
        )
        {
            subframeBuffer.resize(dstFrameInfo.totalSize());
            subframe(sourceBuffer->data(), srcFrameInfo, subframeBuffer.data(), dstFrameInfo);

            sourceBuffer = &subframeBuffer;
        }

        // For recording, save immediately.
        {
            std::lock_guard<std::mutex> lock(recordMutex);
            if (
                isRecording && !isRecordingAboutToClose &&
                recordStream(sourceBuffer->data(), sourceBuffer->size(), sourceTimeFrame.time, sourceTimeFrame.timestamp) == false
            )
            {
                LOG_ERROR("Recording failed.");
                isRecordingAboutToClose = true;
            }
        }

        // For streaming, downscale to 8bit if higher than 8bit to reduce bandwidth
        // You can reduce the number of frames by setting a frame limit.
        if (isStreaming && FPSPreview.newFrame())
        {
            // Downscale to 8bit always for streaming to reduce bandwidth
            if (PixelFormat != INDI_JPG && PixelDepth > 8)
            {
                // Allocale new buffer if size changes
                downscaleBuffer.resize(dstFrameInfo.pixels());

                // Apply gamma
                gammaLut16.apply(
                    reinterpret_cast<const uint16_t*>(sourceBuffer->data()),
                    downscaleBuffer.size(),
                    downscaleBuffer.data()
                );

                sourceBuffer = &downscaleBuffer;
            }

            //uploadStream(sourceBuffer->data(), sourceBuffer->size());
            previewThreadPool.start(std::bind([this, &previewElapsed](const std::atomic_bool & isAboutToQuit,
                                              std::vector<uint8_t> frame)
            {
                INDI_UNUSED(isAboutToQuit);
                previewElapsed.start();
                uploadStream(frame.data(), frame.size());
                StreamTimeNP[0].setValue(previewElapsed.nsecsElapsed() / 1000000000.0);
                StreamTimeNP.apply();

            }, std::placeholders::_1, std::move(*sourceBuffer)));
        }
    }
}

void StreamManagerPrivate::setSize(uint16_t width, uint16_t height)
{
    if (width != StreamFrameNP[CCDChip::FRAME_W].getValue() || height != StreamFrameNP[CCDChip::FRAME_H].getValue())
    {
        if (PixelFormat == INDI_JPG)
            LOG_WARN("Cannot subframe JPEG streams.");

        StreamFrameNP[CCDChip::FRAME_X].setValue(0);
        StreamFrameNP[CCDChip::FRAME_X].setMax(width - 1);
        StreamFrameNP[CCDChip::FRAME_Y].setValue(0);
        StreamFrameNP[CCDChip::FRAME_Y].setMax(height - 1);
        StreamFrameNP[CCDChip::FRAME_W].setValue(width);
        StreamFrameNP[CCDChip::FRAME_W].setMin(10);
        StreamFrameNP[CCDChip::FRAME_W].setMax(width);
        StreamFrameNP[CCDChip::FRAME_H].setValue(height);
        StreamFrameNP[CCDChip::FRAME_H].setMin(10);
        StreamFrameNP[CCDChip::FRAME_H].setMax(height);

        StreamFrameNP.setState(IPS_OK);
        StreamFrameNP.updateMinMax();
    }

    dstFrameInfo.x = StreamFrameNP[CCDChip::FRAME_X].getValue();
    dstFrameInfo.y = StreamFrameNP[CCDChip::FRAME_Y].getValue();
    dstFrameInfo.w = StreamFrameNP[CCDChip::FRAME_W].getValue();
    dstFrameInfo.h = StreamFrameNP[CCDChip::FRAME_H].getValue();

    // Width & Height are BINNED dimensions.
    // Since they're the final size to make it to encoders and recorders.
    rawWidth = width;
    rawHeight = height;

    for (EncoderInterface * oneEncoder : encoderManager.getEncoderList())
        oneEncoder->setSize(rawWidth, rawHeight);
    for (RecorderInterface * oneRecorder : recorderManager.getRecorderList())
        oneRecorder->setSize(rawWidth, rawHeight);
}

bool StreamManager::close()
{
    D_PTR(StreamManager);
    std::lock_guard<std::mutex> lock(d->recordMutex);
    return d->recorder->close();
}

bool StreamManagerPrivate::setPixelFormat(INDI_PIXEL_FORMAT pixelFormat, uint8_t pixelDepth)
{
    if (pixelFormat == PixelFormat && pixelDepth == PixelDepth)
        return true;

    bool recorderOK = recorder->setPixelFormat(pixelFormat, pixelDepth);
    if (recorderOK == false)
    {
        LOGF_ERROR("Pixel format %d is not supported by %s recorder.", pixelFormat, recorder->getName());
    }
    else
    {
        LOGF_DEBUG("Pixel format %d is supported by %s recorder.", pixelFormat, recorder->getName());
    }
    bool encoderOK = encoder->setPixelFormat(pixelFormat, pixelDepth);
    if (encoderOK == false)
    {
        LOGF_ERROR("Pixel format %d is not supported by %s encoder.", pixelFormat, encoder->getName());
    }
    else
    {
        LOGF_DEBUG("Pixel format %d is supported by %s encoder.", pixelFormat, encoder->getName());
    }

    PixelFormat = pixelFormat;
    PixelDepth  = pixelDepth;
    return true;
}

bool StreamManager::setPixelFormat(INDI_PIXEL_FORMAT pixelFormat, uint8_t pixelDepth)
{
    D_PTR(StreamManager);
    return d->setPixelFormat(pixelFormat, pixelDepth);
}


void StreamManager::setSize(uint16_t width, uint16_t height)
{
    D_PTR(StreamManager);
    d->setSize(width, height);
}

bool StreamManagerPrivate::recordStream(const uint8_t * buffer, uint32_t nbytes, double deltams, uint64_t timestamp)
{
    INDI_UNUSED(deltams);
    if (!isRecording)
        return false;

    return recorder->writeFrame(buffer, nbytes, timestamp);
}

std::string StreamManagerPrivate::expand(const std::string &fname, const std::map<std::string, std::string> &patterns)
{
    std::string result = fname;

    std::time_t t  =  std::time(nullptr);
    std::tm     tm = *std::gmtime(&t);

    auto extendedPatterns = patterns;
    extendedPatterns["_D_"] = format_time(tm, "%Y-%m-%d");
    extendedPatterns["_H_"] = format_time(tm, "%H-%M-%S");
    extendedPatterns["_T_"] = format_time(tm, "%Y-%m-%d" "@" "%H-%M-%S");

    for(const auto &pattern : extendedPatterns)
    {
        replace_all(result, pattern.first, pattern.second);
    }

    // Replace all : to - to be valid filename on Windows
    std::replace(result.begin(), result.end(), ':', '-'); // it's really needed now?

    return result;
}

bool StreamManagerPrivate::startRecording()
{
    char errmsg[MAXRBUF];
    std::string filename, expfilename, expfiledir;
    std::string filtername;
    std::map<std::string, std::string> patterns;
    if (isRecording)
        return true;

    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        /* get filter name for pattern substitution */
        if (dynamic_cast<INDI::CCD*>(currentDevice)->CurrentFilterSlot != -1
                && dynamic_cast<INDI::CCD*>(currentDevice)->CurrentFilterSlot <= static_cast<int>(dynamic_cast<INDI::CCD*>
                        (currentDevice)->FilterNames.size()))
        {
            filtername      = dynamic_cast<INDI::CCD*>(currentDevice)->FilterNames.at(dynamic_cast<INDI::CCD*>
                              (currentDevice)->CurrentFilterSlot - 1);
            patterns["_F_"] = filtername;
            LOGF_DEBUG("Adding filter pattern %s", filtername.c_str());
        }
    }

    recorder->setFPS(FpsNP[FPS_AVERAGE].getValue());

    /* pattern substitution */
    recordfiledir.assign(RecordFileTP[0].getText());
    expfiledir = expand(recordfiledir, patterns);
    if (expfiledir.at(expfiledir.size() - 1) != '/')
        expfiledir += '/';
    recordfilename.assign(RecordFileTP[1].getText());
    expfilename = expand(recordfilename, patterns);
    if (expfilename.size() < 4 || expfilename.substr(expfilename.size() - 4, 4) != recorder->getExtension())
        expfilename += recorder->getExtension();

    filename = expfiledir + expfilename;
    //LOGF_INFO("Expanded file is %s", filename.c_str());
    //filename=recordfiledir+recordfilename;
    LOGF_INFO("Record file is %s", filename.c_str());
    /* Create/open file/dir */
    if (mkpath(expfiledir, 0755))
    {
        LOGF_WARN("Can not create record directory %s: %s", expfiledir.c_str(),
                  strerror(errno));
        return false;
    }
    if (!recorder->open(filename.c_str(), errmsg))
    {
        RecordStreamSP.setState(IPS_ALERT);
        RecordStreamSP.apply();
        LOGF_WARN("Can not open record file: %s", errmsg);
        return false;
    }

#if 0
    /* start capture */
    // TODO direct recording should this be part of StreamManager?
    if (direct_record)
    {
        LOG_INFO("Using direct recording (no software cropping).");
        //v4l_base->doDecode(false);
        //v4l_base->doRecord(true);
    }
    else
    {
        //if (ImageColorS[IMAGE_GRAYSCALE].s == ISS_ON)
        if (dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getNAxis() == 2)
            recorder->setDefaultMono();
        else
            recorder->setDefaultColor();
    }
#endif
    FPSRecorder.reset();
    frameCountDivider = 0;

    if (isStreaming == false)
    {
        FPSAverage.reset();
        FPSFast.reset();
    }

    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        if (isStreaming == false && dynamic_cast<INDI::CCD*>(currentDevice)->StartStreaming() == false)
        {
            LOG_ERROR("Failed to start recording.");
            RecordStreamSP.setState(IPS_ALERT);
            RecordStreamSP.reset();
            RecordStreamSP[RECORD_OFF].setState(ISS_ON);
            RecordStreamSP.apply();
        }
    }
    else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
    {
        if (isStreaming == false && dynamic_cast<INDI::SensorInterface*>(currentDevice)->StartStreaming() == false)
        {
            LOG_ERROR("Failed to start recording.");
            RecordStreamSP.setState(IPS_ALERT);
            RecordStreamSP.reset();
            RecordStreamSP[RECORD_OFF].setState(ISS_ON);
            RecordStreamSP.apply();
        }
    }
    isRecording = true;
    return true;
}

bool StreamManagerPrivate::stopRecording(bool force)
{
    if (!isRecording && force == false)
        return true;

    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        if (!isStreaming)
            dynamic_cast<INDI::CCD*>(currentDevice)->StopStreaming();
    }
    else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
    {
        if (!isStreaming)
            dynamic_cast<INDI::SensorInterface*>(currentDevice)->StopStreaming();

    }

    isRecording = false;
    isRecordingAboutToClose = false;

    {
        std::lock_guard<std::mutex> lock(recordMutex);
        recorder->close();
    }

    if (force)
        return false;

    LOGF_INFO(
        "Record Duration: %g millisec / %d frames",
        FPSRecorder.totalTime(),
        FPSRecorder.totalFrames()
    );

    return true;
}

bool StreamManagerPrivate::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    /* ignore if not ours */
    if (dev != nullptr && strcmp(getDeviceName(), dev))
        return false;

    /* Video Stream */
    if (StreamSP.isNameMatch(name))
    {
        for (int i = 0; i < n; i++)
        {
            if (!strcmp(names[i], "STREAM_ON") && states[i] == ISS_ON)
            {
                setStream(true);
                break;
            }
            else if (!strcmp(names[i], "STREAM_OFF") && states[i] == ISS_ON)
            {
                setStream(false);
                break;
            }
        }
        return true;
    }

    // Record Stream
    if (RecordStreamSP.isNameMatch(name))
    {
        int prevSwitch = RecordStreamSP.findOnSwitchIndex();
        RecordStreamSP.update(states, names, n);

        if (isRecording && RecordStreamSP[RECORD_OFF].getState() != ISS_ON)
        {
            RecordStreamSP.reset();
            RecordStreamSP[prevSwitch].setState(ISS_ON);
            RecordStreamSP.apply();
            LOG_WARN("Recording device is busy.");
            return true;
        }

        if (
            RecordStreamSP[RECORD_ON   ].getState() == ISS_ON ||
            RecordStreamSP[RECORD_TIME ].getState() == ISS_ON ||
            RecordStreamSP[RECORD_FRAME].getState() == ISS_ON
        )
        {
            if (!isRecording)
            {
                RecordStreamSP.setState(IPS_BUSY);
                if (RecordStreamSP[RECORD_TIME].getState() == ISS_ON)
                    LOGF_INFO("Starting video record (Duration): %g secs.", RecordOptionsNP[0].getValue());
                else if (RecordStreamSP[RECORD_FRAME].getState() == ISS_ON)
                    LOGF_INFO("Starting video record (Frame count): %d.", static_cast<int>(RecordOptionsNP[1].getValue()));
                else
                    LOG_INFO("Starting video record.");

                if (!startRecording())
                {
                    RecordStreamSP.reset();
                    RecordStreamSP[RECORD_OFF].setState(ISS_ON);
                    RecordStreamSP.setState(IPS_ALERT);
                }
            }
        }
        else
        {
            RecordStreamSP.setState(IPS_IDLE);
            Format.clear();
            FpsNP[FPS_INSTANT].setValue(0);
            FpsNP[FPS_AVERAGE].setValue(0);
            if (isRecording)
            {
                LOG_INFO("Recording stream has been disabled. Closing the stream...");
                isRecordingAboutToClose = true;
            }
        }

        RecordStreamSP.apply();
        return true;
    }

    // Encoder Selection
    if (EncoderSP.isNameMatch(name))
    {
        EncoderSP.update(states, names, n);
        EncoderSP.setState(IPS_ALERT);

        const char * selectedEncoder = EncoderSP.findOnSwitch()->getName();

        for (EncoderInterface * oneEncoder : encoderManager.getEncoderList())
        {
            if (!strcmp(selectedEncoder, oneEncoder->getName()))
            {
                encoderManager.setEncoder(oneEncoder);

                oneEncoder->setPixelFormat(PixelFormat, PixelDepth);

                encoder = oneEncoder;

                EncoderSP.setState(IPS_OK);
            }
        }
        EncoderSP.apply();
        return true;
    }

    // Recorder Selection
    if (RecorderSP.isNameMatch(name))
    {
        RecorderSP.update(states, names, n);
        RecorderSP.setState(IPS_ALERT);

        const char * selectedRecorder = RecorderSP.findOnSwitch()->getName();

        for (RecorderInterface * oneRecorder : recorderManager.getRecorderList())
        {
            if (!strcmp(selectedRecorder, oneRecorder->getName()))
            {
                recorderManager.setRecorder(oneRecorder);

                oneRecorder->setPixelFormat(PixelFormat, PixelDepth);

                recorder = oneRecorder;

                RecorderSP.setState(IPS_OK);
            }
        }
        RecorderSP.apply();
        return true;
    }

    // No properties were processed
    return false;
}

bool StreamManager::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    D_PTR(StreamManager);
    return d->ISNewSwitch(dev, name, states, names, n);
}

bool StreamManagerPrivate::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    /* ignore if not ours */
    if (dev != nullptr && strcmp(getDeviceName(), dev))
        return false;

    if (RecordFileTP.isNameMatch(name))
    {
        auto tp = RecordFileTP.findWidgetByName("RECORD_FILE_NAME");
        if (strchr(tp->getText(), '/'))
        {
            LOG_WARN("Dir. separator (/) not allowed in filename.");
            return true;
        }

        RecordFileTP.update(texts, names, n);
        RecordFileTP.apply();
        return true;
    }

    // No Properties were processed.
    return false;
}

bool StreamManager::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    D_PTR(StreamManager);
    return d->ISNewText(dev, name, texts, names, n);
}

bool StreamManagerPrivate::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    /* ignore if not ours */
    if (dev != nullptr && strcmp(getDeviceName(), dev))
        return false;

    if (StreamExposureNP.isNameMatch(name))
    {
        StreamExposureNP.update(values, names, n);
        StreamExposureNP.setState(IPS_OK);
        StreamExposureNP.apply();
        return true;
    }

    /* Limits */
    if (LimitsNP.isNameMatch(name))
    {
        LimitsNP.update(values, names, n);

        FPSPreview.setTimeWindow(1000.0 / LimitsNP[LIMITS_PREVIEW_FPS].getValue());
        FPSPreview.reset();

        LimitsNP.setState(IPS_OK);
        LimitsNP.apply();
        return true;
    }

    /* Record Options */
    if (RecordOptionsNP.isNameMatch(name))
    {
        if (isRecording)
        {
            LOG_WARN("Recording device is busy");
            return true;
        }

        RecordOptionsNP.update(values, names, n);
        RecordOptionsNP.setState(IPS_OK);
        RecordOptionsNP.apply();
        return true;
    }

    /* Stream Frame */
    if (StreamFrameNP.isNameMatch(name))
    {
        if (isRecording)
        {
            LOG_WARN("Recording device is busy");
            return true;
        }

        FrameInfo srcFrameInfo;

        if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
        {
            srcFrameInfo = FrameInfo(dynamic_cast<const INDI::CCD*>(currentDevice)->PrimaryCCD);
        }
        else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        {
            srcFrameInfo = FrameInfo(*dynamic_cast<const INDI::SensorInterface*>(currentDevice));
        }

        StreamFrameNP.update(values, names, n);
        StreamFrameNP.setState(IPS_OK);

        double subW = srcFrameInfo.w - StreamFrameNP[CCDChip::FRAME_X].getValue();
        double subH = srcFrameInfo.h - StreamFrameNP[CCDChip::FRAME_Y].getValue();

        StreamFrameNP[CCDChip::FRAME_W].setValue(std::min(StreamFrameNP[CCDChip::FRAME_W].getValue(), subW));
        StreamFrameNP[CCDChip::FRAME_H].setValue(std::min(StreamFrameNP[CCDChip::FRAME_H].getValue(), subH));

        setSize(StreamFrameNP[CCDChip::FRAME_W].getValue(), StreamFrameNP[CCDChip::FRAME_H].getValue());

        StreamFrameNP.apply();
        return true;
    }

    // No properties were processed
    return false;
}

bool StreamManager::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    D_PTR(StreamManager);
    return d->ISNewNumber(dev, name, values, names, n);
}

bool StreamManagerPrivate::setStream(bool enable)
{
    if (enable)
    {
        if (!isStreaming)
        {
            StreamSP.setState(IPS_BUSY);
#if 0
            if (StreamOptionsN[OPTION_RATE_DIVISOR].value > 0)
                DEBUGF(INDI::Logger::DBG_SESSION,
                       "Starting the video stream with target FPS %.f and rate divisor of %.f",
                       StreamOptionsN[OPTION_TARGET_FPS].value, StreamOptionsN[OPTION_RATE_DIVISOR].value);
            else
                LOGF_INFO("Starting the video stream with target FPS %.f", StreamOptionsN[OPTION_TARGET_FPS].value);
#endif
            LOGF_INFO("Starting the video stream with target exposure %.6f s (Max theoretical FPS %.f)",
                      StreamExposureNP[0].getValue(), 1 / StreamExposureNP[0].getValue());

            FPSAverage.reset();
            FPSFast.reset();
            FPSPreview.reset();
            FPSPreview.setTimeWindow(1000.0 / LimitsNP[LIMITS_PREVIEW_FPS].getValue());
            frameCountDivider = 0;

            if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
            {
                if (dynamic_cast<INDI::CCD*>(currentDevice)->StartStreaming() == false)
                {
                    StreamSP.reset();
                    StreamSP[1].setState(ISS_ON);
                    StreamSP.setState(IPS_ALERT);
                    LOG_ERROR("Failed to start streaming.");
                    StreamSP.apply();
                    return false;
                }
            }
            else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
            {
                if (dynamic_cast<INDI::SensorInterface*>(currentDevice)->StartStreaming() == false)
                {
                    StreamSP.reset();
                    StreamSP[1].setState(ISS_ON);
                    StreamSP.setState(IPS_ALERT);
                    LOG_ERROR("Failed to start streaming.");
                    StreamSP.apply();
                    return false;
                }
            }
            isStreaming = true;
            Format.clear();
            FpsNP[FPS_INSTANT].setValue(0);
            FpsNP[FPS_AVERAGE].setValue(0);
            StreamSP.reset();
            StreamSP[0].setState(ISS_ON);
            recorder->setStreamEnabled(true);
        }
    }
    else
    {
        StreamSP.setState(IPS_IDLE);
        Format.clear();
        FpsNP[FPS_INSTANT].setValue(0);
        FpsNP[FPS_AVERAGE].setValue(0);
        if (isStreaming)
        {
            if (!isRecording)
            {
                if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
                {
                    if (dynamic_cast<INDI::CCD*>(currentDevice)->StopStreaming() == false)
                    {
                        StreamSP.setState(IPS_ALERT);
                        LOG_ERROR("Failed to stop streaming.");
                        StreamSP.apply();
                        return false;
                    }
                }
                else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
                {
                    if (dynamic_cast<INDI::SensorInterface*>(currentDevice)->StopStreaming() == false)
                    {
                        StreamSP.setState(IPS_ALERT);
                        LOG_ERROR("Failed to stop streaming.");
                        StreamSP.apply();
                        return false;
                    }
                }
            }

            StreamSP.reset();
            StreamSP[1].setState(ISS_ON);
            isStreaming = false;
            Format.clear();
            FpsNP[FPS_INSTANT].setValue(0);
            FpsNP[FPS_AVERAGE].setValue(0);

            recorder->setStreamEnabled(false);
        }
    }

    StreamSP.apply();
    return true;
}

bool StreamManager::setStream(bool enable)
{
    D_PTR(StreamManager);
    return d->setStream(enable);
}

bool StreamManager::saveConfigItems(FILE * fp)
{
    D_PTR(StreamManager);
    d->EncoderSP.save(fp);
    d->RecordFileTP.save(fp);
    d->RecordOptionsNP.save(fp);
    d->RecorderSP.save(fp);
    d->LimitsNP.save(fp);
    return true;
}

void StreamManagerPrivate::getStreamFrame(uint16_t * x, uint16_t * y, uint16_t * w, uint16_t * h) const
{
    *x = StreamFrameNP[CCDChip::FRAME_X].getValue();
    *y = StreamFrameNP[CCDChip::FRAME_Y].getValue();
    *w = StreamFrameNP[CCDChip::FRAME_W].getValue();
    *h = StreamFrameNP[CCDChip::FRAME_H].getValue();
}

void StreamManagerPrivate::setStreamFrame(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    StreamFrameNP[CCDChip::FRAME_X].setValue(x);
    StreamFrameNP[CCDChip::FRAME_Y].setValue(y);
    StreamFrameNP[CCDChip::FRAME_W].setValue(w);
    StreamFrameNP[CCDChip::FRAME_H].setValue(h);
}

void StreamManagerPrivate::setStreamFrame(const FrameInfo &frameInfo)
{
    setStreamFrame(frameInfo.x, frameInfo.y, frameInfo.w, frameInfo.h);
}

void StreamManager::getStreamFrame(uint16_t * x, uint16_t * y, uint16_t * w, uint16_t * h) const
{
    D_PTR(const StreamManager);
    d->getStreamFrame(x, y, w, h);
}

bool StreamManagerPrivate::uploadStream(const uint8_t * buffer, uint32_t nbytes)
{
    // Send as is, already encoded.
    if (PixelFormat == INDI_JPG)
    {
        imageBP[0].setBlob(const_cast<uint8_t *>(buffer));
        imageBP[0].setBlobLen(nbytes);
        imageBP[0].setSize(nbytes);
        imageBP[0].setFormat(".stream_jpg");
        imageBP.setState(IPS_OK);
        imageBP.apply();
        return true;
    }

    // Binning for grayscale frames only for now - REMOVE ME
#if 0
    if (dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getNAxis() == 2)
    {
        dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.binFrame();
        nbytes /= dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinX() * dynamic_cast<INDI::CCD*>
                  (currentDevice)->PrimaryCCD.getBinY();
    }
#endif

    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        if (encoder->upload(&imageBP[0], buffer, nbytes, dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.isCompressed()))
        {
            // Upload to client now
            imageBP.setState(IPS_OK);
            imageBP.apply();
            return true;
        }
    }
    else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
    {
        if (encoder->upload(&imageBP[0], buffer, nbytes,
                            false))//dynamic_cast<INDI::SensorInterface*>(currentDevice)->isCompressed()))
        {
            // Upload to client now
            imageBP.setState(IPS_OK);
            imageBP.apply();
            return true;
        }
    }

    return false;
}

RecorderInterface *StreamManager::getRecorder() const
{
    D_PTR(const StreamManager);
    return d->recorder;
}

bool StreamManager::isDirectRecording() const
{
    D_PTR(const StreamManager);
    return d->direct_record;
}

bool StreamManager::isStreaming() const
{
    D_PTR(const StreamManager);
    return d->isStreaming;
}

bool StreamManager::isRecording() const
{
    D_PTR(const StreamManager);
    return d->isRecording && !d->isRecordingAboutToClose;
}

bool StreamManager::isBusy() const
{
    D_PTR(const StreamManager);
    return (d->isStreaming || d->isRecording);
}

double StreamManager::getTargetFPS() const
{
    D_PTR(const StreamManager);
    return 1.0 / d->StreamExposureNP[0].getValue();
}
double StreamManager::getTargetExposure() const
{
    D_PTR(const StreamManager);
    return d->StreamExposureNP[0].getValue();
}

void StreamManager::setStreamingExposureEnabled(bool enable)
{
    D_PTR(StreamManager);
    d->hasStreamingExposure = enable;
}

}
