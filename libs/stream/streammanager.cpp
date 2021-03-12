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
    IUFillSwitch(&StreamS[0], "STREAM_ON", "Stream On", ISS_OFF);
    IUFillSwitch(&StreamS[1], "STREAM_OFF", "Stream Off", ISS_ON);
    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        IUFillSwitchVector(&StreamSP, StreamS, NARRAY(StreamS), getDeviceName(), "SENSOR_DATA_STREAM", "Video Stream",
                           STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    else
        IUFillSwitchVector(&StreamSP, StreamS, NARRAY(StreamS), getDeviceName(), "CCD_VIDEO_STREAM", "Video Stream",
                           STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    IUFillNumber(&StreamExposureN[STREAM_EXPOSURE], "STREAMING_EXPOSURE_VALUE", "Duration (s)", "%.6f", 0.000001, 60, 0.1, 0.1);
    IUFillNumber(&StreamExposureN[STREAM_DIVISOR], "STREAMING_DIVISOR_VALUE", "Divisor", "%.f", 1, 15, 1, 1);
    IUFillNumberVector(&StreamExposureNP, StreamExposureN, NARRAY(StreamExposureN), getDeviceName(), "STREAMING_EXPOSURE",
                       "Expose", STREAM_TAB, IP_RW, 60, IPS_IDLE);

    /* Measured FPS */
    IUFillNumber(&FpsN[FPS_INSTANT], "EST_FPS", "Instant.", "%.2f", 0.0, 999.0, 0.0, 30);
    IUFillNumber(&FpsN[FPS_AVERAGE], "AVG_FPS", "Average (1 sec.)", "%.2f", 0.0, 999.0, 0.0, 30);
    IUFillNumberVector(&FpsNP, FpsN, NARRAY(FpsN), getDeviceName(), "FPS", "FPS", STREAM_TAB, IP_RO, 60, IPS_IDLE);

    /* Record Frames */
    /* File */
    std::string defaultDirectory = std::string(getenv("HOME")) + std::string("/indi__D_");
    IUFillText(&RecordFileT[0], "RECORD_FILE_DIR", "Dir.", defaultDirectory.data());
    IUFillText(&RecordFileT[1], "RECORD_FILE_NAME", "Name", "indi_record__T_");
    IUFillTextVector(&RecordFileTP, RecordFileT, NARRAY(RecordFileT), getDeviceName(), "RECORD_FILE", "Record File",
                     STREAM_TAB, IP_RW, 0, IPS_IDLE);

    /* Record Options */
    IUFillNumber(&RecordOptionsN[0], "RECORD_DURATION", "Duration (sec)", "%.3f", 0.001, 999999.0, 0.0, 1);
    IUFillNumber(&RecordOptionsN[1], "RECORD_FRAME_TOTAL", "Frames", "%.f", 1.0, 999999999.0, 1.0, 30.0);
    IUFillNumberVector(&RecordOptionsNP, RecordOptionsN, NARRAY(RecordOptionsN), getDeviceName(), "RECORD_OPTIONS",
                       "Record Options", STREAM_TAB, IP_RW, 60, IPS_IDLE);

    /* Record Switch */
    IUFillSwitch(&RecordStreamS[RECORD_ON], "RECORD_ON", "Record On", ISS_OFF);
    IUFillSwitch(&RecordStreamS[RECORD_TIME], "RECORD_DURATION_ON", "Record (Duration)", ISS_OFF);
    IUFillSwitch(&RecordStreamS[RECORD_FRAME], "RECORD_FRAME_ON", "Record (Frames)", ISS_OFF);
    IUFillSwitch(&RecordStreamS[RECORD_OFF], "RECORD_OFF", "Record Off", ISS_ON);
    IUFillSwitchVector(&RecordStreamSP, RecordStreamS, NARRAY(RecordStreamS), getDeviceName(), "RECORD_STREAM",
                       "Video Record", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        // CCD Streaming Frame
        IUFillNumber(&StreamFrameN[0], "X", "Left ", "%.f", 0, 0.0, 0, 0);
        IUFillNumber(&StreamFrameN[1], "Y", "Top", "%.f", 0, 0, 0, 0);
        IUFillNumber(&StreamFrameN[2], "WIDTH", "Width", "%.f", 0, 0.0, 0, 0.0);
        IUFillNumber(&StreamFrameN[3], "HEIGHT", "Height", "%.f", 0, 0, 0, 0.0);
        IUFillNumberVector(&StreamFrameNP, StreamFrameN, 4, getDeviceName(), "CCD_STREAM_FRAME", "Frame", STREAM_TAB, IP_RW,
                           60, IPS_IDLE);
    }

    // Encoder Selection
    IUFillSwitch(&EncoderS[ENCODER_RAW], "RAW", "RAW", ISS_ON);
    IUFillSwitch(&EncoderS[ENCODER_MJPEG], "MJPEG", "MJPEG", ISS_OFF);
    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        IUFillSwitchVector(&EncoderSP, EncoderS, NARRAY(EncoderS), getDeviceName(), "SENSOR_STREAM_ENCODER", "Encoder", STREAM_TAB,
                           IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    else
        IUFillSwitchVector(&EncoderSP, EncoderS, NARRAY(EncoderS), getDeviceName(), "CCD_STREAM_ENCODER", "Encoder", STREAM_TAB,
                           IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Recorder Selector
    IUFillSwitch(&RecorderS[RECORDER_RAW], "SER", "SER", ISS_ON);
    IUFillSwitch(&RecorderS[RECORDER_OGV], "OGV", "OGV", ISS_OFF);
    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
        IUFillSwitchVector(&RecorderSP, RecorderS, NARRAY(RecorderS), getDeviceName(), "SENSOR_STREAM_RECORDER", "Recorder",
                           STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    else
        IUFillSwitchVector(&RecorderSP, RecorderS, NARRAY(RecorderS), getDeviceName(), "CCD_STREAM_RECORDER", "Recorder",
                           STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    // If we do not have theora installed, let's just define SER default recorder
#ifndef HAVE_THEORA
    RecorderSP.nsp = 1;
#endif

    // Limits
    IUFillNumber(&LimitsN[LIMITS_BUFFER_MAX], "LIMITS_BUFFER_MAX", "Maximum Buffer Size (MB)", "%.0f", 1, 1024*64, 1, 512);
    IUFillNumber(&LimitsN[LIMITS_PREVIEW_FPS], "LIMITS_PREVIEW_FPS", "Maximum Preview FPS", "%.0f", 1, 120, 1, 10);
    IUFillNumberVector(&LimitsNP, LimitsN, NARRAY(LimitsN), getDeviceName(), "LIMITS",
                       "Limits", STREAM_TAB, IP_RW, 0, IPS_IDLE);
    return true;
}


void StreamManager::ISGetProperties(const char * dev)
{
    if (dev != nullptr && strcmp(getDeviceName(), dev))
        return;

    if (currentDevice->isConnected())
    {
        currentDevice->defineProperty(&StreamSP);
        if (m_hasStreamingExposure)
            currentDevice->defineProperty(&StreamExposureNP);
        currentDevice->defineProperty(&FpsNP);
        currentDevice->defineProperty(&RecordStreamSP);
        currentDevice->defineProperty(&RecordFileTP);
        currentDevice->defineProperty(&RecordOptionsNP);
        currentDevice->defineProperty(&StreamFrameNP);
        currentDevice->defineProperty(&EncoderSP);
        currentDevice->defineProperty(&RecorderSP);
        currentDevice->defineProperty(&LimitsNP);
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

        currentDevice->defineProperty(&StreamSP);
        if (m_hasStreamingExposure)
            currentDevice->defineProperty(&StreamExposureNP);
        currentDevice->defineProperty(&FpsNP);
        currentDevice->defineProperty(&RecordStreamSP);
        currentDevice->defineProperty(&RecordFileTP);
        currentDevice->defineProperty(&RecordOptionsNP);
        currentDevice->defineProperty(&StreamFrameNP);
        currentDevice->defineProperty(&EncoderSP);
        currentDevice->defineProperty(&RecorderSP);
        currentDevice->defineProperty(&LimitsNP);
    }
    else
    {
        currentDevice->deleteProperty(StreamSP.name);
        if (m_hasStreamingExposure)
            currentDevice->deleteProperty(StreamExposureNP.name);
        currentDevice->deleteProperty(FpsNP.name);
        currentDevice->deleteProperty(RecordFileTP.name);
        currentDevice->deleteProperty(RecordStreamSP.name);
        currentDevice->deleteProperty(RecordOptionsNP.name);
        currentDevice->deleteProperty(StreamFrameNP.name);
        currentDevice->deleteProperty(EncoderSP.name);
        currentDevice->deleteProperty(RecorderSP.name);
        currentDevice->deleteProperty(LimitsNP.name);
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
    // N is StreamExposureN[STREAM_DIVISOR].value
    ++m_frameCountDivider;
    if (
        (StreamExposureN[STREAM_DIVISOR].value > 1) &&
        (m_frameCountDivider % static_cast<int>(StreamExposureN[STREAM_DIVISOR].value)) == 0
    )
    {
        return;
    }

    if (m_FPSAverage.newFrame())
    {
        FpsN[1].value = m_FPSAverage.framesPerSecond();
    }

    if (m_FPSFast.newFrame())
    {
        FpsN[0].value = m_FPSFast.framesPerSecond();
        if (m_fastFPSUpdate.try_lock()) // don't block stream thread / record thread
            std::thread([&](){ IDSetNumber(&FpsNP, nullptr); m_fastFPSUpdate.unlock(); }).detach();
    }

    if (isStreaming() || isRecording())
    {
        size_t allocatedSize = nbytes * m_framesIncoming.size() / 1024 / 1024; // allocated size in MB
        if (allocatedSize > LimitsN[LIMITS_BUFFER_MAX].value)
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
            (RecordStreamSP.sp[RECORD_FRAME].s == ISS_ON && m_FPSRecorder.totalFrames() >= (RecordOptionsNP.np[1].value)) ||
            (RecordStreamSP.sp[RECORD_TIME].s  == ISS_ON && m_FPSRecorder.totalTime()   >= (RecordOptionsNP.np[0].value * 1000.0))
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
            RecordStreamSP.sp[RECORD_TIME].s  = ISS_OFF;
            RecordStreamSP.sp[RECORD_FRAME].s = ISS_OFF;
            RecordStreamSP.sp[RECORD_OFF].s   = ISS_ON;
            RecordStreamSP.s = IPS_IDLE;
            IDSetSwitch(&RecordStreamSP, nullptr);

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

    double frameW = StreamFrameN[CCDChip::FRAME_W].value;
    double frameH = StreamFrameN[CCDChip::FRAME_H].value;
    double frameX = StreamFrameN[CCDChip::FRAME_X].value;
    double frameY = StreamFrameN[CCDChip::FRAME_Y].value;

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
            StreamFrameN[CCDChip::FRAME_W].value = frameW;
            StreamFrameN[CCDChip::FRAME_H].value = frameH;
            StreamFrameN[CCDChip::FRAME_X].value = frameX;
            StreamFrameN[CCDChip::FRAME_Y].value = frameY;
            StreamFrameNP.s = IPS_IDLE;
            IDSetNumber(&StreamFrameNP, nullptr);
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
    if (width != StreamFrameN[CCDChip::FRAME_W].value || height != StreamFrameN[CCDChip::FRAME_H].value)
    {
        if (m_PixelFormat == INDI_JPG)
            LOG_WARN("Cannot subframe JPEG streams.");

        StreamFrameN[CCDChip::FRAME_X].value = 0;
        StreamFrameN[CCDChip::FRAME_X].max   = width - 1;
        StreamFrameN[CCDChip::FRAME_Y].value = 0;
        StreamFrameN[CCDChip::FRAME_Y].max   = height - 1;
        StreamFrameN[CCDChip::FRAME_W].value = width;
        StreamFrameN[CCDChip::FRAME_W].min   = 10;
        StreamFrameN[CCDChip::FRAME_W].max   = width;
        StreamFrameN[CCDChip::FRAME_H].value = height;
        StreamFrameN[CCDChip::FRAME_H].min   = 10;
        StreamFrameN[CCDChip::FRAME_H].max   = height;

        StreamFrameNP.s = IPS_OK;
        IUUpdateMinMax(&StreamFrameNP);
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

    recorder->setFPS(FpsN[FPS_AVERAGE].value);

    /* pattern substitution */
    recordfiledir.assign(RecordFileTP.tp[0].text);
    expfiledir = expand(recordfiledir, patterns);
    if (expfiledir.at(expfiledir.size() - 1) != '/')
        expfiledir += '/';
    recordfilename.assign(RecordFileTP.tp[1].text);
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
        RecordStreamSP.s = IPS_ALERT;
        IDSetSwitch(&RecordStreamSP, nullptr);
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
            RecordStreamSP.s = IPS_ALERT;
            IUResetSwitch(&RecordStreamSP);
            RecordStreamS[RECORD_OFF].s = ISS_ON;
            IDSetSwitch(&RecordStreamSP, nullptr);
        }
    }
    else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
    {
        if (m_isStreaming == false && dynamic_cast<INDI::SensorInterface*>(currentDevice)->StartStreaming() == false)
        {
            LOG_ERROR("Failed to start recording.");
            RecordStreamSP.s = IPS_ALERT;
            IUResetSwitch(&RecordStreamSP);
            RecordStreamS[RECORD_OFF].s = ISS_ON;
            IDSetSwitch(&RecordStreamSP, nullptr);
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
    if (!strcmp(name, StreamSP.name))
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
    if (!strcmp(name, RecordStreamSP.name))
    {
        int prevSwitch = IUFindOnSwitchIndex(&RecordStreamSP);
        IUUpdateSwitch(&RecordStreamSP, states, names, n);

        if (m_isRecording && RecordStreamSP.sp[RECORD_OFF].s != ISS_ON)
        {
            IUResetSwitch(&RecordStreamSP);
            RecordStreamS[prevSwitch].s = ISS_ON;
            IDSetSwitch(&RecordStreamSP, nullptr);
            LOG_WARN("Recording device is busy.");
            return true;
        }

        if ((RecordStreamSP.sp[RECORD_ON].s == ISS_ON) ||
                (RecordStreamSP.sp[RECORD_TIME].s == ISS_ON) ||
                (RecordStreamSP.sp[RECORD_FRAME].s == ISS_ON))
        {
            if (!m_isRecording)
            {
                RecordStreamSP.s = IPS_BUSY;
                if (RecordStreamSP.sp[RECORD_TIME].s == ISS_ON)
                    LOGF_INFO("Starting video record (Duration): %g secs.", RecordOptionsNP.np[0].value);
                else if (RecordStreamSP.sp[RECORD_FRAME].s == ISS_ON)
                    LOGF_INFO("Starting video record (Frame count): %d.", static_cast<int>(RecordOptionsNP.np[1].value));
                else
                    LOG_INFO("Starting video record.");

                if (!startRecording())
                {
                    IUResetSwitch(&RecordStreamSP);
                    RecordStreamSP.sp[RECORD_OFF].s = ISS_ON;
                    RecordStreamSP.s = IPS_ALERT;
                }
            }
        }
        else
        {
            RecordStreamSP.s = IPS_IDLE;
            m_Format.clear();
            FpsN[FPS_INSTANT].value = FpsN[FPS_AVERAGE].value = 0;
            if (m_isRecording)
            {
                LOG_INFO("Recording stream has been disabled. Closing the stream...");
                m_isRecordingAboutToClose = true;
            }
        }

        IDSetSwitch(&RecordStreamSP, nullptr);
        return true;
    }

    // Encoder Selection
    if (!strcmp(name, EncoderSP.name))
    {
        IUUpdateSwitch(&EncoderSP, states, names, n);
        EncoderSP.s = IPS_ALERT;

        const char * selectedEncoder = IUFindOnSwitch(&EncoderSP)->name;

        for (EncoderInterface * oneEncoder : encoderManager->getEncoderList())
        {
            if (!strcmp(selectedEncoder, oneEncoder->getName()))
            {
                encoderManager->setEncoder(oneEncoder);

                oneEncoder->setPixelFormat(m_PixelFormat, m_PixelDepth);

                encoder = oneEncoder;

                EncoderSP.s = IPS_OK;
            }
        }
        IDSetSwitch(&EncoderSP, nullptr);
        return true;
    }

    // Recorder Selection
    if (!strcmp(name, RecorderSP.name))
    {
        IUUpdateSwitch(&RecorderSP, states, names, n);
        RecorderSP.s = IPS_ALERT;

        const char * selectedRecorder = IUFindOnSwitch(&RecorderSP)->name;

        for (RecorderInterface * oneRecorder : recorderManager->getRecorderList())
        {
            if (!strcmp(selectedRecorder, oneRecorder->getName()))
            {
                recorderManager->setRecorder(oneRecorder);

                oneRecorder->setPixelFormat(m_PixelFormat, m_PixelDepth);

                recorder = oneRecorder;

                RecorderSP.s = IPS_OK;
            }
        }
        IDSetSwitch(&RecorderSP, nullptr);
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

    if (!strcmp(name, RecordFileTP.name))
    {
        IText * tp = IUFindText(&RecordFileTP, "RECORD_FILE_NAME");
        if (strchr(tp->text, '/'))
        {
            LOG_WARN("Dir. separator (/) not allowed in filename.");
            return true;
        }

        IUUpdateText(&RecordFileTP, texts, names, n);
        IDSetText(&RecordFileTP, nullptr);
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

    if (!strcmp(StreamExposureNP.name, name))
    {
        IUUpdateNumber(&StreamExposureNP, values, names, n);
        StreamExposureNP.s = IPS_OK;
        IDSetNumber(&StreamExposureNP, nullptr);
        return true;
    }

    /* Limits */
    if (!strcmp(LimitsNP.name, name))
    {
        IUUpdateNumber(&LimitsNP, values, names, n);

        m_FPSPreview.setTimeWindow(1000.0 / LimitsN[LIMITS_PREVIEW_FPS].value);
        m_FPSPreview.reset();

        LimitsNP.s = IPS_OK;
        IDSetNumber(&LimitsNP, nullptr);
        return true;
    }

    /* Record Options */
    if (!strcmp(RecordOptionsNP.name, name))
    {
        if (m_isRecording)
        {
            LOG_WARN("Recording device is busy");
            return true;
        }

        IUUpdateNumber(&RecordOptionsNP, values, names, n);
        RecordOptionsNP.s = IPS_OK;
        IDSetNumber(&RecordOptionsNP, nullptr);
        return true;
    }

    /* Stream Frame */
    if (!strcmp(StreamFrameNP.name, name))
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

        IUUpdateNumber(&StreamFrameNP, values, names, n);
        StreamFrameNP.s = IPS_OK;
        if (StreamFrameN[CCDChip::FRAME_X].value + StreamFrameN[CCDChip::FRAME_W].value > subW)
            StreamFrameN[CCDChip::FRAME_W].value = subW - StreamFrameN[CCDChip::FRAME_X].value;

        if (StreamFrameN[CCDChip::FRAME_Y].value + StreamFrameN[CCDChip::FRAME_H].value > subH)
            StreamFrameN[CCDChip::FRAME_H].value = subH - StreamFrameN[CCDChip::FRAME_Y].value;

        setSize(StreamFrameN[CCDChip::FRAME_W].value, StreamFrameN[CCDChip::FRAME_H].value);

        IDSetNumber(&StreamFrameNP, nullptr);
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
            StreamSP.s       = IPS_BUSY;
#if 0
            if (StreamOptionsN[OPTION_RATE_DIVISOR].value > 0)
                DEBUGF(INDI::Logger::DBG_SESSION,
                       "Starting the video stream with target FPS %.f and rate divisor of %.f",
                       StreamOptionsN[OPTION_TARGET_FPS].value, StreamOptionsN[OPTION_RATE_DIVISOR].value);
            else
                LOGF_INFO("Starting the video stream with target FPS %.f", StreamOptionsN[OPTION_TARGET_FPS].value);
#endif
            LOGF_INFO("Starting the video stream with target exposure %.6f s (Max theoritical FPS %.f)", StreamExposureN[0].value,
                      1 / StreamExposureN[0].value);

            m_FPSAverage.reset();
            m_FPSFast.reset();
            m_FPSPreview.reset();
            m_FPSPreview.setTimeWindow(1000.0 / LimitsN[LIMITS_PREVIEW_FPS].value);
            m_frameCountDivider = 0;
            
            if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
            {
                if (dynamic_cast<INDI::CCD*>(currentDevice)->StartStreaming() == false)
                {
                    IUResetSwitch(&StreamSP);
                    StreamS[1].s = ISS_ON;
                    StreamSP.s   = IPS_ALERT;
                    LOG_ERROR("Failed to start streaming.");
                    IDSetSwitch(&StreamSP, nullptr);
                    return false;
                }
            }
            else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
            {
                if (dynamic_cast<INDI::SensorInterface*>(currentDevice)->StartStreaming() == false)
                {
                    IUResetSwitch(&StreamSP);
                    StreamS[1].s = ISS_ON;
                    StreamSP.s   = IPS_ALERT;
                    LOG_ERROR("Failed to start streaming.");
                    IDSetSwitch(&StreamSP, nullptr);
                    return false;
                }
            }
            m_isStreaming = true;
            m_Format.clear();
            FpsN[FPS_INSTANT].value = FpsN[FPS_AVERAGE].value = 0;
            IUResetSwitch(&StreamSP);
            StreamS[0].s = ISS_ON;
            recorder->setStreamEnabled(true);
        }
    }
    else
    {
        StreamSP.s = IPS_IDLE;
        m_Format.clear();
        FpsN[FPS_INSTANT].value = FpsN[FPS_AVERAGE].value = 0;
        if (m_isStreaming)
        {
            if (!m_isRecording)
            {
                if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
                {
                    if (dynamic_cast<INDI::CCD*>(currentDevice)->StopStreaming() == false)
                    {
                        StreamSP.s = IPS_ALERT;
                        LOG_ERROR("Failed to stop streaming.");
                        IDSetSwitch(&StreamSP, nullptr);
                        return false;
                    }
                }
                else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::SENSOR_INTERFACE)
                {
                    if (dynamic_cast<INDI::SensorInterface*>(currentDevice)->StopStreaming() == false)
                    {
                        StreamSP.s = IPS_ALERT;
                        LOG_ERROR("Failed to stop streaming.");
                        IDSetSwitch(&StreamSP, nullptr);
                        return false;
                    }
                }
            }

            IUResetSwitch(&StreamSP);
            StreamS[1].s = ISS_ON;
            m_isStreaming = false;
            m_Format.clear();
            FpsN[FPS_INSTANT].value = FpsN[FPS_AVERAGE].value = 0;

            recorder->setStreamEnabled(false);
        }
    }

    IDSetSwitch(&StreamSP, nullptr);
    return true;
}

bool StreamManager::saveConfigItems(FILE * fp)
{
    IUSaveConfigSwitch(fp, &EncoderSP);
    IUSaveConfigText(fp, &RecordFileTP);
    IUSaveConfigNumber(fp, &RecordOptionsNP);
    IUSaveConfigSwitch(fp, &RecorderSP);
    return true;
}

void StreamManager::getStreamFrame(uint16_t * x, uint16_t * y, uint16_t * w, uint16_t * h)
{
    *x = StreamFrameN[CCDChip::FRAME_X].value;
    *y = StreamFrameN[CCDChip::FRAME_Y].value;
    *w = StreamFrameN[CCDChip::FRAME_W].value;
    *h = StreamFrameN[CCDChip::FRAME_H].value;
}

bool StreamManager::uploadStream(const uint8_t * buffer, uint32_t nbytes)
{
    // Send as is, already encoded.
    if (m_PixelFormat == INDI_JPG)
    {
        // Upload to client now
#ifdef HAVE_WEBSOCKET
        if (dynamic_cast<INDI::CCD*>(currentDevice)->HasWebSocket()
                && dynamic_cast<INDI::CCD*>(currentDevice)->WebSocketS[CCD::WEBSOCKET_ENABLED].s == ISS_ON)
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
                    && dynamic_cast<INDI::CCD*>(currentDevice)->WebSocketS[CCD::WEBSOCKET_ENABLED].s == ISS_ON)
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
