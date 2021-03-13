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
#include "indiccd.h"
#include "indisensorinterface.h"
#include "indilogger.h"
#include "indiutility.h"

#include <cerrno>
#include <sys/stat.h>

#include <algorithm>

static const char * STREAM_TAB = "Streaming";

namespace INDI
{

StreamManager::StreamManager(DefaultDevice *mainDevice)
{
    currentDevice = mainDevice;

    m_isStreaming = false;
    m_isRecording = false;

    m_FPSAverage.setTimeWindow(1000);
    m_FPSFast.setTimeWindow(50);

    recorderManager = new RecorderManager();
    recorder    = recorderManager->getDefaultRecorder();
    direct_record = false;

    LOGF_DEBUG("Using default recorder (%s)", recorder->getName());

    encoderManager = new EncoderManager();
    encoder = encoderManager->getDefaultEncoder();

    encoder->init(currentDevice);

    LOGF_DEBUG("Using default encoder (%s)", encoder->getName());

    m_framesThreadTerminate = false;
    m_framesThread = std::thread(&StreamManager::asyncStreamThread, this);

}

StreamManager::~StreamManager()
{
    if (m_framesThread.joinable())
    {
        m_framesThreadTerminate = true;
        m_framesIncoming.abort();
        m_framesThread.join();
    }

    delete (recorderManager);
    delete (encoderManager);
}

const char * StreamManager::getDeviceName()
{
    return currentDevice->getDeviceName();
}

bool StreamManager::initProperties()
{
    /* Video Stream */
    StreamSP[0].fill("STREAM_ON", "Stream On", ISS_OFF);
    StreamSP[1].fill("STREAM_OFF", "Stream Off", ISS_ON);
    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        StreamSP.fill(getDeviceName(), "SENSOR_DATA_STREAM", "Video Stream",
                           STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    else
        StreamSP.fill(getDeviceName(), "CCD_VIDEO_STREAM", "Video Stream",
                           STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    StreamExposureNP[STREAM_EXPOSURE].fill("STREAMING_EXPOSURE_VALUE", "Duration (s)", "%.6f", 0.000001, 60, 0.1, 0.1);
    StreamExposureNP[STREAM_DIVISOR].fill("STREAMING_DIVISOR_VALUE", "Divisor", "%.f", 1, 15, 1, 1);
    StreamExposureNP.fill(getDeviceName(), "STREAMING_EXPOSURE",
                       "Expose", STREAM_TAB, IP_RW, 60, IPS_IDLE);

    /* Measured FPS */
    FpsNP[FPS_INSTANT].fill("EST_FPS", "Instant.", "%.2f", 0.0, 999.0, 0.0, 30);
    FpsNP[FPS_AVERAGE].fill("AVG_FPS", "Average (1 sec.)", "%.2f", 0.0, 999.0, 0.0, 30);
    FpsNP.fill(getDeviceName(), "FPS", "FPS", STREAM_TAB, IP_RO, 60, IPS_IDLE);

    /* Record Frames */
    /* File */
    std::string defaultDirectory = std::string(getenv("HOME")) + std::string("/indi__D_");
    RecordFileTP[0].fill("RECORD_FILE_DIR", "Dir.", defaultDirectory.data());
    RecordFileTP[1].fill("RECORD_FILE_NAME", "Name", "indi_record__T_");
    RecordFileTP.fill(getDeviceName(), "RECORD_FILE", "Record File",
                     STREAM_TAB, IP_RW, 0, IPS_IDLE);

    /* Record Options */
    RecordOptionsNP[0].fill("RECORD_DURATION", "Duration (sec)", "%.3f", 0.001, 999999.0, 0.0, 1);
    RecordOptionsNP[1].fill("RECORD_FRAME_TOTAL", "Frames", "%.f", 1.0, 999999999.0, 1.0, 30.0);
    RecordOptionsNP.fill(getDeviceName(), "RECORD_OPTIONS",
                       "Record Options", STREAM_TAB, IP_RW, 60, IPS_IDLE);

    /* Record Switch */
    RecordStreamSP[RECORD_ON].fill("RECORD_ON", "Record On", ISS_OFF);
    RecordStreamSP[RECORD_TIME].fill("RECORD_DURATION_ON", "Record (Duration)", ISS_OFF);
    RecordStreamSP[RECORD_FRAME].fill("RECORD_FRAME_ON", "Record (Frames)", ISS_OFF);
    RecordStreamSP[RECORD_OFF].fill("RECORD_OFF", "Record Off", ISS_ON);
    RecordStreamSP.fill(getDeviceName(), "RECORD_STREAM",
                       "Video Record", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        // CCD Streaming Frame
        StreamFrameNP[0].fill("X", "Left ", "%.f", 0, 0.0, 0, 0);
        StreamFrameNP[1].fill("Y", "Top", "%.f", 0, 0, 0, 0);
        StreamFrameNP[2].fill("WIDTH", "Width", "%.f", 0, 0.0, 0, 0.0);
        StreamFrameNP[3].fill("HEIGHT", "Height", "%.f", 0, 0, 0, 0.0);
        StreamFrameNP.fill(getDeviceName(), "CCD_STREAM_FRAME", "Frame", STREAM_TAB, IP_RW,
                           60, IPS_IDLE);
    }

    // Encoder Selection
    EncoderSP[ENCODER_RAW].fill("RAW", "RAW", ISS_ON);
    EncoderSP[ENCODER_MJPEG].fill("MJPEG", "MJPEG", ISS_OFF);
    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        EncoderSP.fill(getDeviceName(), "SENSOR_STREAM_ENCODER", "Encoder", STREAM_TAB,
                           IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    else
        EncoderSP.fill(getDeviceName(), "CCD_STREAM_ENCODER", "Encoder", STREAM_TAB,
                           IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Recorder Selector
    RecorderSP[RECORDER_RAW].fill("SER", "SER", ISS_ON);
    RecorderSP[RECORDER_OGV].fill("OGV", "OGV", ISS_OFF);
    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        RecorderSP.fill(getDeviceName(), "SENSOR_STREAM_RECORDER", "Recorder",
                           STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    else
        RecorderSP.fill(getDeviceName(), "CCD_STREAM_RECORDER", "Recorder",
                           STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    // If we do not have theora installed, let's just define SER default recorder
#ifndef HAVE_THEORA
    RecorderSP.resize(1);
#endif

    // Limits
    LimitsNP[LIMITS_BUFFER_MAX].fill("LIMITS_BUFFER_MAX", "Maximum Buffer Size (MB)", "%.0f", 1, 1024*64, 1, 512);
    LimitsNP[LIMITS_PREVIEW_FPS].fill("LIMITS_PREVIEW_FPS", "Maximum Preview FPS", "%.0f", 1, 120, 1, 10);
    LimitsNP.fill(getDeviceName(), "LIMITS",
                       "Limits", STREAM_TAB, IP_RW, 0, IPS_IDLE);
    return true;
}


void StreamManager::ISGetProperties(const char * dev)
{
    if (dev != nullptr && strcmp(getDeviceName(), dev))
        return;

    if (currentDevice->isConnected())
    {
        currentDevice->defineProperty(StreamSP);
        if (m_hasStreamingExposure)
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

bool StreamManager::updateProperties()
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
        imageB  = imageBP->bp;

        currentDevice->defineProperty(StreamSP);
        if (m_hasStreamingExposure)
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
        if (m_hasStreamingExposure)
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

/*
 * The camera driver is expected to send the FULL FRAME of the Camera after BINNING without any subframing at all
 * Subframing for streaming/recording is done in the stream manager.
 * Therefore nbytes is expected to be SubW/BinX * SubH/BinY * Bytes_Per_Pixels * Number_Color_Components
 * Binned frame must be sent from the camera driver for this to work consistentaly for all drivers.*/
void StreamManager::newFrame(const uint8_t * buffer, uint32_t nbytes)
{
    // close the data stream on the same thread as the data stream
    // manually triggered to stop recording.
    if (m_isRecordingAboutToClose)
    {
        stopRecording();
        return;
    }

    // Discard every N frame.
    // do not count it to fps statistics
    // N is StreamExposureNP[STREAM_DIVISOR].getValue()
    ++m_frameCountDivider;
    if (
        (StreamExposureNP[STREAM_DIVISOR].value > 1) &&
        (m_frameCountDivider % static_cast<int>(StreamExposureNP[STREAM_DIVISOR].value)) == 0
    )
    {
        return;
    }

    if (m_FPSAverage.newFrame())
    {
        FpsNP[1].setValue(m_FPSAverage.framesPerSecond());
    }

    if (m_FPSFast.newFrame())
    {
        FpsNP[0].setValue(m_FPSFast.framesPerSecond());
        if (m_fastFPSUpdate.try_lock()) // don't block stream thread / record thread
            std::thread([&](){ FpsNP.apply(); m_fastFPSUpdate.unlock(); }).detach();
    }

    if (isStreaming() || isRecording())
    {
        size_t allocatedSize = nbytes * m_framesIncoming.size() / 1024 / 1024; // allocated size in MB
        if (allocatedSize > LimitsNP[LIMITS_BUFFER_MAX].getValue())
        {
            LOG_WARN("Frame buffer is full, skipping frame...");
            return;
        }

        std::vector<uint8_t> copyBuffer(buffer, buffer + nbytes); // copy the frame

        m_framesIncoming.push(TimeFrame{m_FPSFast.deltaTime(), std::move(copyBuffer)}); // push it into the queue
    }

    if (isRecording())
    {
        m_FPSRecorder.newFrame(); // count frames and total time

        // captured all frames, stream should be close
        if (
            (RecordStreamSP[RECORD_FRAME].s == ISS_ON && m_FPSRecorder.totalFrames() >= (RecordOptionsNP[1].value)) ||
            (RecordStreamSP[RECORD_TIME].s  == ISS_ON && m_FPSRecorder.totalTime()   >= (RecordOptionsNP[0].value * 1000.0))
        )
        {
            LOG_INFO("Waiting for all buffered frames to be recorded");
            m_framesIncoming.waitForEmpty();
            // duplicated message
#if 0
            LOGF_INFO(
                "Ending record after %g millisecs and %d frames",
                m_FPSRecorder.totalTime(),
                m_FPSRecorder.totalFrames()
            );
#endif
            RecordStreamSP[RECORD_TIME].setState(ISS_OFF);
            RecordStreamSP[RECORD_FRAME].setState(ISS_OFF);
            RecordStreamSP[RECORD_OFF].setState(ISS_ON);
            RecordStreamSP.setState(IPS_IDLE);
            RecordStreamSP.apply();

            stopRecording();
        }
    }
}

void StreamManager::asyncStreamThread()
{
    TimeFrame sourceTimeFrame;
    sourceTimeFrame.time = 0;

    std::vector<uint8_t> subframeBuffer;  // Subframe buffer for recording/streaming
    std::vector<uint8_t> downscaleBuffer; // Downscale buffer for streaming

    double frameW = StreamFrameNP[CCDChip::FRAME_W].getValue();
    double frameH = StreamFrameNP[CCDChip::FRAME_H].getValue();
    double frameX = StreamFrameNP[CCDChip::FRAME_X].getValue();
    double frameY = StreamFrameNP[CCDChip::FRAME_Y].getValue();

    while(!m_framesThreadTerminate)
    {
        if (m_framesIncoming.pop(sourceTimeFrame) == false)
            continue;


        const uint8_t *sourceBufferData = sourceTimeFrame.frame.data();
        uint32_t nbytes                 = sourceTimeFrame.frame.size();

#if 1 // TODO move above the loop
        int subX = 0, subY = 0, subW = 0, subH = 0;
        uint32_t npixels = 0;
        uint8_t components = (m_PixelFormat == INDI_RGB) ? 3 : 1;
        uint8_t bytesPerPixel = (m_PixelDepth + 7) / 8;

        if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
        {
            INDI::CCD * ccd = dynamic_cast<INDI::CCD*>(currentDevice);
            subX = ccd->PrimaryCCD.getSubX() / ccd->PrimaryCCD.getBinX();
            subY = ccd->PrimaryCCD.getSubY() / ccd->PrimaryCCD.getBinY();
            subW = ccd->PrimaryCCD.getSubW() / ccd->PrimaryCCD.getBinX();
            subH = ccd->PrimaryCCD.getSubH() / ccd->PrimaryCCD.getBinY();
            npixels = subW * subH * components;
        }
        else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        {
            INDI::SensorInterface* si = dynamic_cast<INDI::SensorInterface*>(currentDevice);
            subX = 0;
            subY = 0;
            subW = si->getBufferSize() * 8 / si->getBPS();
            subH = 1;
            npixels = nbytes * 8 / si->getBPS();
        }

        // If stream frame was not yet initilized, let's do that now
        if (frameW == 0 || frameH == 0)
        {
            //if (dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getNAxis() == 2)
            //    binFactor = dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinX();
            frameX = subX;
            frameY = subY;
            frameW = subW;
            frameH = subH;
            StreamFrameNP[CCDChip::FRAME_W].setValue(frameW);
            StreamFrameNP[CCDChip::FRAME_H].setValue(frameH);
            StreamFrameNP[CCDChip::FRAME_X].setValue(frameX);
            StreamFrameNP[CCDChip::FRAME_Y].setValue(frameY);
            StreamFrameNP.setState(IPS_IDLE);
            StreamFrameNP.apply();
        }
#endif

        // Check if we need to subframe
        if (
            (m_PixelFormat != INDI_JPG) &&
            (frameW > 0 && frameH > 0) &&
            (frameX != subX || frameY != subY || frameW != subW || frameH != subH))
        {
            npixels = frameW * frameH * components;

            subframeBuffer.resize(npixels * bytesPerPixel);

            uint32_t srcOffset = components * bytesPerPixel * (frameY * subW + frameX);

            const uint8_t * srcBuffer = sourceBufferData + srcOffset;
            uint32_t        srcStride = components * bytesPerPixel * subW;

            uint8_t *       dstBuffer = subframeBuffer.data();
            uint32_t        dstStride = components * bytesPerPixel * frameW;

            // Copy line-by-line
            for (int i = 0; i < frameH; ++i)
                memcpy(dstBuffer + i * dstStride, srcBuffer + srcStride * i, dstStride);

            sourceBufferData = dstBuffer;
            nbytes = frameW * frameH * components * bytesPerPixel;
        }

        {
            std::lock_guard<std::mutex> lock(m_recordMutex);
            // For recording, save immediately.
            if (isRecording() && recordStream(sourceBufferData, nbytes, sourceTimeFrame.time) == false)
            {
                LOG_ERROR("Recording failed.");
                m_isRecordingAboutToClose = true;
            }
        }

        // For streaming, downscale to 8bit if higher than 8bit to reduce bandwidth
        // You can reduce the number of frames by setting a frame limit.
        if (isStreaming() && m_FPSPreview.newFrame())
        {
            // Downscale to 8bit always for streaming to reduce bandwidth
            if (m_PixelFormat != INDI_JPG && m_PixelDepth > 8)
            {
                // Allocale new buffer if size changes
                downscaleBuffer.resize(npixels);

                const uint16_t * srcBuffer = reinterpret_cast<const uint16_t *>(sourceBufferData);
                uint8_t *        dstBuffer = downscaleBuffer.data();

                // Apply gamma
                m_gammaLut16.apply(srcBuffer, npixels, dstBuffer);

                sourceBufferData = dstBuffer;
                nbytes /= 2;
            }
            uploadStream(sourceBufferData, nbytes);
        }
    }
}

void StreamManager::setSize(uint16_t width, uint16_t height)
{
    if (width != StreamFrameNP[CCDChip::FRAME_W].value || height != StreamFrameNP[CCDChip::FRAME_H].getValue())
    {
        if (m_PixelFormat == INDI_JPG)
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

    // Width & Height are BINNED dimensions.
    // Since they're the final size to make it to encoders and recorders.
    rawWidth = width;
    rawHeight = height;

    for (EncoderInterface * oneEncoder : encoderManager->getEncoderList())
        oneEncoder->setSize(rawWidth, rawHeight);
    for (RecorderInterface * oneRecorder : recorderManager->getRecorderList())
        oneRecorder->setSize(rawWidth, rawHeight);
}

bool StreamManager::close()
{
    std::lock_guard<std::mutex> lock(m_recordMutex);
    return recorder->close();
}

bool StreamManager::setPixelFormat(INDI_PIXEL_FORMAT pixelFormat, uint8_t pixelDepth)
{
    if (pixelFormat == m_PixelFormat && pixelDepth == m_PixelDepth)
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

    m_PixelFormat = pixelFormat;
    m_PixelDepth  = pixelDepth;
    return true;
}

bool StreamManager::recordStream(const uint8_t * buffer, uint32_t nbytes, double deltams)
{
    INDI_UNUSED(deltams);
    if (!m_isRecording)
        return false;

    return recorder->writeFrame(buffer, nbytes);
}

std::string StreamManager::expand(const std::string &fname, const std::map<std::string, std::string> &patterns)
{
    std::string result = fname;

    std::time_t t  =  std::time(nullptr);
    std::tm     tm = *std::gmtime(&t);

    auto extendedPatterns = patterns;
    extendedPatterns["_D_"] = format_time(tm, "%Y-%m-%d");
    extendedPatterns["_H_"] = format_time(tm, "%H-%M-%S");
    extendedPatterns["_T_"] = format_time(tm, "%Y-%m-%d" "@" "%H-%M-%S");

    for(const auto &pattern: extendedPatterns)
    {
        replace_all(result, pattern.first, pattern.second);
    }

    // Replace all : to - to be valid filename on Windows
    std::replace(result.begin(), result.end(), ':', '-'); // it's really needed now?

    return result;
}

bool StreamManager::startRecording()
{
    char errmsg[MAXRBUF];
    std::string filename, expfilename, expfiledir;
    std::string filtername;
    std::map<std::string, std::string> patterns;
    if (m_isRecording)
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

    recorder->setFPS(FpsNP[FPS_AVERAGE].value);

    /* pattern substitution */
    recordfiledir.assign(RecordFileTP[0].text);
    expfiledir = expand(recordfiledir, patterns);
    if (expfiledir.at(expfiledir.size() - 1) != '/')
        expfiledir += '/';
    recordfilename.assign(RecordFileTP[1].text);
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
        //if (ImageColorSP[IMAGE_GRAYSCALE].getState() == ISS_ON)
        if (dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getNAxis() == 2)
            recorder->setDefaultMono();
        else
            recorder->setDefaultColor();
    }
#endif
    m_FPSRecorder.reset();
    m_frameCountDivider = 0;

    if (m_isStreaming == false)
    {
        m_FPSAverage.reset();
        m_FPSFast.reset();
    }

    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        if (m_isStreaming == false && dynamic_cast<INDI::CCD*>(currentDevice)->StartStreaming() == false)
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
        if (m_isStreaming == false && dynamic_cast<INDI::SensorInterface*>(currentDevice)->StartStreaming() == false)
        {
            LOG_ERROR("Failed to start recording.");
            RecordStreamSP.setState(IPS_ALERT);
            RecordStreamSP.reset();
            RecordStreamSP[RECORD_OFF].setState(ISS_ON);
            RecordStreamSP.apply();
        }
    }
    m_isRecording = true;
    return true;
}

bool StreamManager::stopRecording(bool force)
{
    if (!m_isRecording && force == false)
        return true;

    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        if (!m_isStreaming)
            dynamic_cast<INDI::CCD*>(currentDevice)->StopStreaming();
    }
    else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
    {
        if (!m_isStreaming)
            dynamic_cast<INDI::SensorInterface*>(currentDevice)->StopStreaming();

    }

    m_isRecording = false;
    m_isRecordingAboutToClose = false;

    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        recorder->close();
    }

    if (force)
        return false;

    LOGF_INFO(
        "Record Duration: %g millisec / %d frames",
        m_FPSRecorder.totalTime(),
        m_FPSRecorder.totalFrames()
    );

    return true;
}

bool StreamManager::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
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

        if (m_isRecording && RecordStreamSP[RECORD_OFF].s != ISS_ON)
        {
            RecordStreamSP.reset();
            RecordStreamSP[prevSwitch].setState(ISS_ON);
            RecordStreamSP.apply();
            LOG_WARN("Recording device is busy.");
            return true;
        }

        if ((RecordStreamSP[RECORD_ON].s == ISS_ON) ||
                (RecordStreamSP[RECORD_TIME].s == ISS_ON) ||
                (RecordStreamSP[RECORD_FRAME].s == ISS_ON))
        {
            if (!m_isRecording)
            {
                RecordStreamSP.setState(IPS_BUSY);
                if (RecordStreamSP[RECORD_TIME].s == ISS_ON)
                    LOGF_INFO("Starting video record (Duration): %g secs.", RecordOptionsNP[0].getValue());
                else if (RecordStreamSP[RECORD_FRAME].s == ISS_ON)
                    LOGF_INFO("Starting video record (Frame count): %d.", static_cast<int>(RecordOptionsNP[1].value));
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
            m_Format.clear();
            FpsNP[FPS_INSTANT].setValue(FpsNP[FPS_AVERAGE].value = 0);
            if (m_isRecording)
            {
                LOG_INFO("Recording stream has been disabled. Closing the stream...");
                m_isRecordingAboutToClose = true;
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

        const char * selectedEncoder = EncoderSP.findOnSwitch()->name;

        for (EncoderInterface * oneEncoder : encoderManager->getEncoderList())
        {
            if (!strcmp(selectedEncoder, oneEncoder->getName()))
            {
                encoderManager->setEncoder(oneEncoder);

                oneEncoder->setPixelFormat(m_PixelFormat, m_PixelDepth);

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

        const char * selectedRecorder = RecorderSP.findOnSwitch()->name;

        for (RecorderInterface * oneRecorder : recorderManager->getRecorderList())
        {
            if (!strcmp(selectedRecorder, oneRecorder->getName()))
            {
                recorderManager->setRecorder(oneRecorder);

                oneRecorder->setPixelFormat(m_PixelFormat, m_PixelDepth);

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

bool StreamManager::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    /* ignore if not ours */
    if (dev != nullptr && strcmp(getDeviceName(), dev))
        return false;

    if (RecordFileTP.isNameMatch(name))
    {
        IText * tp = RecordFileTP.findWidgetByName("RECORD_FILE_NAME");
        if (strchr(tp->text, '/'))
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

bool StreamManager::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
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

        m_FPSPreview.setTimeWindow(1000.0 / LimitsNP[LIMITS_PREVIEW_FPS].getValue());
        m_FPSPreview.reset();

        LimitsNP.setState(IPS_OK);
        LimitsNP.apply();
        return true;
    }

    /* Record Options */
    if (RecordOptionsNP.isNameMatch(name))
    {
        if (m_isRecording)
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
        if (m_isRecording)
        {
            LOG_WARN("Recording device is busy");
            return true;
        }

        int subW = 0;
        int subH = 0;

        if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
        {
            subW = dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getSubW() / dynamic_cast<INDI::CCD*>
                   (currentDevice)->PrimaryCCD.getBinX();
            subH = dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getSubH() / dynamic_cast<INDI::CCD*>
                   (currentDevice)->PrimaryCCD.getBinY();
        }
        else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        {
            subW = dynamic_cast<INDI::SensorInterface*>(currentDevice)->getBufferSize() * 8 / dynamic_cast<INDI::SensorInterface*>
                   (currentDevice)->getBPS();
            subH = 1;
        }

        StreamFrameNP.update(values, names, n);
        StreamFrameNP.setState(IPS_OK);
        if (StreamFrameNP[CCDChip::FRAME_X].value + StreamFrameNP[CCDChip::FRAME_W].getValue() > subW)
            StreamFrameNP[CCDChip::FRAME_W].setValue(subW - StreamFrameNP[CCDChip::FRAME_X].getValue());

        if (StreamFrameNP[CCDChip::FRAME_Y].value + StreamFrameNP[CCDChip::FRAME_H].getValue() > subH)
            StreamFrameNP[CCDChip::FRAME_H].setValue(subH - StreamFrameNP[CCDChip::FRAME_Y].getValue());

        setSize(StreamFrameNP[CCDChip::FRAME_W].value, StreamFrameNP[CCDChip::FRAME_H].getValue());

        StreamFrameNP.apply();
        return true;
    }

    // No properties were processed
    return false;
}

bool StreamManager::setStream(bool enable)
{
    if (enable)
    {
        if (!m_isStreaming)
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
            LOGF_INFO("Starting the video stream with target exposure %.6f s (Max theoritical FPS %.f)", StreamExposureNP[0].getValue(),
                      1 / StreamExposureNP[0].getValue());

            m_FPSAverage.reset();
            m_FPSFast.reset();
            m_FPSPreview.reset();
            m_FPSPreview.setTimeWindow(1000.0 / LimitsNP[LIMITS_PREVIEW_FPS].getValue());
            m_frameCountDivider = 0;
            
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
            m_isStreaming = true;
            m_Format.clear();
            FpsNP[FPS_INSTANT].setValue(FpsNP[FPS_AVERAGE].value = 0);
            StreamSP.reset();
            StreamSP[0].setState(ISS_ON);
            recorder->setStreamEnabled(true);
        }
    }
    else
    {
        StreamSP.setState(IPS_IDLE);
        m_Format.clear();
        FpsNP[FPS_INSTANT].setValue(FpsNP[FPS_AVERAGE].value = 0);
        if (m_isStreaming)
        {
            if (!m_isRecording)
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
            m_isStreaming = false;
            m_Format.clear();
            FpsNP[FPS_INSTANT].setValue(FpsNP[FPS_AVERAGE].value = 0);

            recorder->setStreamEnabled(false);
        }
    }

    StreamSP.apply();
    return true;
}

bool StreamManager::saveConfigItems(FILE * fp)
{
    EncoderSP.save(fp);
    RecordFileTP.save(fp);
    RecordOptionsNP.save(fp);
    RecorderSP.save(fp);
    return true;
}

void StreamManager::getStreamFrame(uint16_t * x, uint16_t * y, uint16_t * w, uint16_t * h)
{
    *x = StreamFrameNP[CCDChip::FRAME_X].getValue();
    *y = StreamFrameNP[CCDChip::FRAME_Y].getValue();
    *w = StreamFrameNP[CCDChip::FRAME_W].getValue();
    *h = StreamFrameNP[CCDChip::FRAME_H].getValue();
}

bool StreamManager::uploadStream(const uint8_t * buffer, uint32_t nbytes)
{
    // Send as is, already encoded.
    if (m_PixelFormat == INDI_JPG)
    {
        // Upload to client now
#ifdef HAVE_WEBSOCKET
        if (dynamic_cast<INDI::CCD*>(currentDevice)->HasWebSocket()
                && dynamic_cast<INDI::CCD*>(currentDevice)->WebSocketSP[CCD::WEBSOCKET_ENABLED].getState() == ISS_ON)
        {
            if (m_Format != ".stream_jpg")
            {
                m_Format = ".stream_jpg";
                dynamic_cast<INDI::CCD*>(currentDevice)->wsServer.send_text(m_Format);
            }

            dynamic_cast<INDI::CCD*>(currentDevice)->wsServer.send_binary(buffer, nbytes);
            return true;
        }
#endif
        imageB->blob    = (const_cast<uint8_t *>(buffer));
        imageB->bloblen = nbytes;
        imageB->size    = nbytes;
        strcpy(imageB->format, ".stream_jpg");
        imageBP->s = IPS_OK;
        IDSetBLOB(imageBP, nullptr);
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
        if (encoder->upload(imageB, buffer, nbytes, dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.isCompressed()))
        {
#ifdef HAVE_WEBSOCKET
            if (dynamic_cast<INDI::CCD*>(currentDevice)->HasWebSocket()
                    && dynamic_cast<INDI::CCD*>(currentDevice)->WebSocketSP[CCD::WEBSOCKET_ENABLED].getState() == ISS_ON)
            {
                if (m_Format != ".stream")
                {
                    m_Format = ".stream";
                    dynamic_cast<INDI::CCD*>(currentDevice)->wsServer.send_text(m_Format);
                }

                dynamic_cast<INDI::CCD*>(currentDevice)->wsServer.send_binary(buffer, nbytes);
                return true;
            }
#endif
            // Upload to client now
            imageBP->s = IPS_OK;
            IDSetBLOB(imageBP, nullptr);
            return true;
        }
    }
    else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
    {
        if (encoder->upload(imageB, buffer, nbytes, false))//dynamic_cast<INDI::SensorInterface*>(currentDevice)->isCompressed()))
        {
            // Upload to client now
            imageBP->s = IPS_OK;
            IDSetBLOB(imageBP, nullptr);
            return true;
        }
    }

    return false;
}
}
