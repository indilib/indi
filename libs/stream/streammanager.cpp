/*
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
#include "indidetector.h"
#include "indilogger.h"

#include <cerrno>
#include <signal.h>
#include <sys/stat.h>
#include <cmath>

static const char * STREAM_TAB = "Streaming";

namespace INDI
{

StreamManager::StreamManager(DefaultDevice *mainDevice)
{
    currentDevice = mainDevice;

    m_isStreaming = false;
    m_isRecording = false;

    prepareGammaLUT();

    // Timer
    // now use BSD setimer to avoi librt dependency
    //sevp.sigev_notify=SIGEV_NONE;
    //timer_create(CLOCK_MONOTONIC, &sevp, &fpstimer);
    //fpssettings.it_interval.tv_sec=24*3600;
    //fpssettings.it_interval.tv_nsec=0;
    //fpssettings.it_value=fpssettings.it_interval;
    //timer_settime(fpstimer, 0, &fpssettings, nullptr);

    struct itimerval fpssettings;
    fpssettings.it_interval.tv_sec  = 24 * 3600;
    fpssettings.it_interval.tv_usec = 0;
    fpssettings.it_value            = fpssettings.it_interval;
    signal(SIGALRM, SIG_IGN); //portable
    setitimer(ITIMER_REAL, &fpssettings, nullptr);

    recorderManager = new RecorderManager();
    recorder    = recorderManager->getDefaultRecorder();
    direct_record = false;

    LOGF_DEBUG("Using default recorder (%s)", recorder->getName());

    encoderManager = new EncoderManager();
    encoder = encoderManager->getDefaultEncoder();

    encoder->init(currentDevice);

    LOGF_DEBUG("Using default encoder (%s)", encoder->getName());
}

StreamManager::~StreamManager()
{
    delete (recorderManager);
    delete (encoderManager);
    delete [] downscaleBuffer;
    delete [] gammaLUT_16_8;
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
    if(currentDevice->getDriverInterface() == INDI::DefaultDevice::DETECTOR_INTERFACE)
        IUFillSwitchVector(&StreamSP, StreamS, NARRAY(StreamS), getDeviceName(), "DETECTOR_DATA_STREAM", "Video Stream",
                           STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    else
        IUFillSwitchVector(&StreamSP, StreamS, NARRAY(StreamS), getDeviceName(), "CCD_VIDEO_STREAM", "Video Stream",
                           STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    IUFillNumber(&StreamExposureN[STREAM_EXPOSURE], "STREAMING_EXPOSURE_VALUE", "Duration (s)", "%.6f", 0.000001, 10, 0.1, 0.1);
    IUFillNumber(&StreamExposureN[STREAM_DIVISOR], "STREAMING_DIVISOR_VALUE", "Divisor", "%.f", 1, 15, 1, 1);
    IUFillNumberVector(&StreamExposureNP, StreamExposureN, NARRAY(StreamExposureN), getDeviceName(), "STREAMING_EXPOSURE", "Expose", STREAM_TAB, IP_RW, 60, IPS_IDLE);

    /* Measured FPS */
    IUFillNumber(&FpsN[FPS_INSTANT], "EST_FPS", "Instant.", "%3.2f", 0.0, 999.0, 0.0, 30);
    IUFillNumber(&FpsN[FPS_AVERAGE], "AVG_FPS", "Average (1 sec.)", "%3.2f", 0.0, 999.0, 0.0, 30);
    IUFillNumberVector(&FpsNP, FpsN, NARRAY(FpsN), getDeviceName(), "FPS", "FPS", STREAM_TAB, IP_RO, 60, IPS_IDLE);

    /* Record Frames */
    /* File */
    std::string defaultDirectory = std::string(getenv("HOME")) + std::string("/indi__D_");
    IUFillText(&RecordFileT[0], "RECORD_FILE_DIR", "Dir.", defaultDirectory.data());
    IUFillText(&RecordFileT[1], "RECORD_FILE_NAME", "Name", "indi_record__T_");
    IUFillTextVector(&RecordFileTP, RecordFileT, NARRAY(RecordFileT), getDeviceName(), "RECORD_FILE", "Record File",
                     STREAM_TAB, IP_RW, 0, IPS_IDLE);

    /* Record Options */
    IUFillNumber(&RecordOptionsN[0], "RECORD_DURATION", "Duration (sec)", "%6.3f", 0.001, 999999.0, 0.0, 1);
    IUFillNumber(&RecordOptionsN[1], "RECORD_FRAME_TOTAL", "Frames", "%9.0f", 1.0, 999999999.0, 1.0, 30.0);
    IUFillNumberVector(&RecordOptionsNP, RecordOptionsN, NARRAY(RecordOptionsN), getDeviceName(), "RECORD_OPTIONS",
                       "Record Options", STREAM_TAB, IP_RW, 60, IPS_IDLE);

    /* Record Switch */
    IUFillSwitch(&RecordStreamS[0], "RECORD_ON", "Record On", ISS_OFF);
    IUFillSwitch(&RecordStreamS[1], "RECORD_DURATION_ON", "Record (Duration)", ISS_OFF);
    IUFillSwitch(&RecordStreamS[2], "RECORD_FRAME_ON", "Record (Frames)", ISS_OFF);
    IUFillSwitch(&RecordStreamS[3], "RECORD_OFF", "Record Off", ISS_ON);
    IUFillSwitchVector(&RecordStreamSP, RecordStreamS, NARRAY(RecordStreamS), getDeviceName(), "RECORD_STREAM",
                       "Video Record", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        // CCD Streaming Frame
        IUFillNumber(&StreamFrameN[0], "X", "Left ", "%4.0f", 0, 0.0, 0, 0);
        IUFillNumber(&StreamFrameN[1], "Y", "Top", "%4.0f", 0, 0, 0, 0);
        IUFillNumber(&StreamFrameN[2], "WIDTH", "Width", "%4.0f", 0, 0.0, 0, 0.0);
        IUFillNumber(&StreamFrameN[3], "HEIGHT", "Height", "%4.0f", 0, 0, 0, 0.0);
        IUFillNumberVector(&StreamFrameNP, StreamFrameN, 4, getDeviceName(), "CCD_STREAM_FRAME", "Frame", STREAM_TAB, IP_RW,
                           60, IPS_IDLE);
    }

    // Encoder Selection
    IUFillSwitch(&EncoderS[ENCODER_RAW], "RAW", "RAW", ISS_ON);
    IUFillSwitch(&EncoderS[ENCODER_MJPEG], "MJPEG", "MJPEG", ISS_OFF);
    if(currentDevice->getDriverInterface() == INDI::DefaultDevice::DETECTOR_INTERFACE)
        IUFillSwitchVector(&EncoderSP, EncoderS, NARRAY(EncoderS), getDeviceName(), "DETECTOR_STREAM_ENCODER", "Encoder", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    else
        IUFillSwitchVector(&EncoderSP, EncoderS, NARRAY(EncoderS), getDeviceName(), "CCD_STREAM_ENCODER", "Encoder", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Recorder Selector
    IUFillSwitch(&RecorderS[RECORDER_RAW], "SER", "SER", ISS_ON);
    IUFillSwitch(&RecorderS[RECORDER_OGV], "OGV", "OGV", ISS_OFF);
    if(currentDevice->getDriverInterface() == INDI::DefaultDevice::DETECTOR_INTERFACE)
        IUFillSwitchVector(&RecorderSP, RecorderS, NARRAY(RecorderS), getDeviceName(), "DETECTOR_STREAM_RECORDER", "Recorder", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    else
        IUFillSwitchVector(&RecorderSP, RecorderS, NARRAY(RecorderS), getDeviceName(), "CCD_STREAM_RECORDER", "Recorder", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    // If we do not have theora installed, let's just define SER default recorder
#ifndef HAVE_THEORA
    RecorderSP.nsp = 1;
#endif
    return true;
}


void StreamManager::ISGetProperties(const char * dev)
{
    if (dev != nullptr && strcmp(getDeviceName(), dev))
        return;

    if (currentDevice->isConnected())
    {
        currentDevice->defineSwitch(&StreamSP);
        if (m_hasStreamingExposure)
            currentDevice->defineNumber(&StreamExposureNP);
        currentDevice->defineNumber(&FpsNP);
        currentDevice->defineSwitch(&RecordStreamSP);
        currentDevice->defineText(&RecordFileTP);
        currentDevice->defineNumber(&RecordOptionsNP);
        currentDevice->defineNumber(&StreamFrameNP);
        currentDevice->defineSwitch(&EncoderSP);
        currentDevice->defineSwitch(&RecorderSP);
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
        if(currentDevice->getDriverInterface() & INDI::DefaultDevice::DETECTOR_INTERFACE)
        {
            imageBP = currentDevice->getBLOB("DETECTOR");
        }
        imageB  = imageBP->bp;

        currentDevice->defineSwitch(&StreamSP);
        if (m_hasStreamingExposure)
            currentDevice->defineNumber(&StreamExposureNP);
        currentDevice->defineNumber(&FpsNP);
        currentDevice->defineSwitch(&RecordStreamSP);
        currentDevice->defineText(&RecordFileTP);
        currentDevice->defineNumber(&RecordOptionsNP);
        currentDevice->defineNumber(&StreamFrameNP);
        currentDevice->defineSwitch(&EncoderSP);
        currentDevice->defineSwitch(&RecorderSP);
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
    m_FrameCounterPerSecond += 1;
    if (StreamExposureN[STREAM_DIVISOR].value > 1 && (m_FrameCounterPerSecond % static_cast<int>(StreamExposureN[STREAM_DIVISOR].value)) == 0)
        return;

    double ms1, ms2, deltams;
    // Measure FPS
    getitimer(ITIMER_REAL, &tframe2);
    ms1 = (1000.0 * tframe1.it_value.tv_sec) + (tframe1.it_value.tv_usec / 1000.0);
    ms2 = (1000.0 * tframe2.it_value.tv_sec) + (tframe2.it_value.tv_usec / 1000.0);
    if (ms2 > ms1)
        deltams = ms2 - ms1;
    else
        deltams = ms1 - ms2;

    tframe1 = tframe2;
    mssum += deltams;

    double newFPS = 1000.0 / deltams;
    if (mssum >= 1000.0)
    {
        FpsN[1].value = (m_FrameCounterPerSecond * 1000.0) / mssum;
        mssum         = 0;
        m_FrameCounterPerSecond = 0;
    }

    // Only send FPS when there is a substancial update
    if (fabs(newFPS - FpsN[0].value) > 1 || m_FrameCounterPerSecond == 0)
    {
        FpsN[0].value = newFPS;
        IDSetNumber(&FpsNP, nullptr);
    }
    std::thread(&StreamManager::asyncStream, this, buffer, nbytes, deltams).detach();
}

void StreamManager::asyncStream(const uint8_t *buffer, uint32_t nbytes, double deltams)
{
    std::unique_lock<std::mutex> guard((currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE) ?
                                       dynamic_cast<INDI::CCD*>(currentDevice)->ccdBufferLock :
                                       dynamic_cast<INDI::Detector*>(currentDevice)->detectorBufferLock);

    // For streaming, downscale 16 to 8
    if (m_PixelDepth == 16 && (StreamSP.s == IPS_BUSY || RecordStreamSP.s == IPS_BUSY))
    {
        if (isStreaming() || strcmp(recorder->getName(), "SER"))
        {
            uint32_t npixels = 0;
            if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
            {
                npixels = (dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getSubW() / dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinX()) * (dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getSubH() / dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinY()) * ((m_PixelFormat == INDI_RGB) ? 3 : 1);
                //uint32_t npixels = StreamFrameN[CCDChip::FRAME_W].value * StreamFrameN[CCDChip::FRAME_H].value * ((m_PixelFormat == INDI_RGB) ? 3 : 1);
            }
            else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::DETECTOR_INTERFACE)
            {
                npixels = nbytes * 8 / dynamic_cast<INDI::Detector*>(currentDevice)->PrimaryDetector.getBPS();
            }
            // Allocale new buffer if size changes
            if (downscaleBufferSize != npixels)
            {
                downscaleBufferSize = npixels;
                delete [] downscaleBuffer;
                downscaleBuffer = new uint8_t[npixels];
            }

            const uint16_t * srcBuffer = reinterpret_cast<const uint16_t *>(buffer);
            //buffer = downscaleBuffer;

            // Slow method: proper downscale
            /*

            uint16_t max = 32768;
            uint16_t min = 0;

            for (uint32_t i=0; i < npixels; i++)
            {
                if (srcBuffer[i] > max)
                    max = srcBuffer[i];
                else if (srcBuffer[i] < min)
                    min = srcBuffer[i];
            }

            double bscale = 255. / (max - min);
            double bzero  = (-min) * (255. / (max - min));

            for (uint32_t i=0; i < npixels; i++)
                streamBuffer[i] = srcBuffer[i] * bscale + bzero;
            */

            // Apply gamma
            for (uint32_t i = 0; i < npixels; i++)
                downscaleBuffer[i] = gammaLUT_16_8[srcBuffer[i]];

            nbytes /= 2;

#if 0
            if (StreamSP.s == IPS_BUSY)
            {
                streamframeCount++;
                if (streamframeCount >= StreamOptionsN[OPTION_RATE_DIVISOR].value)
                {
                    uploadStream(downscaleBuffer, nbytes);
                    streamframeCount = 0;
                }
            }
#endif

            if (StreamSP.s == IPS_BUSY)
                uploadStream(downscaleBuffer, nbytes);

            // If anything but SER, let's call recorder. Otherwise, it's been called up before.
            if (isRecording() && strcmp(recorder->getName(), "SER"))
            {
                recordStream(downscaleBuffer, nbytes, deltams);
            }
        }

        // Do not downscale for SER recorder.
        if (isRecording() && !strcmp(recorder->getName(), "SER"))
        {
            recordStream(buffer, nbytes, deltams);
        }
    }
    else
    {
#if 0
        if (StreamSP.s == IPS_BUSY)
        {
            streamframeCount++;
            if (streamframeCount >= StreamOptionsN[OPTION_RATE_DIVISOR].value)
            {
                if (uploadStream(buffer, nbytes) == false)
                {
                    LOG_ERROR("Streaming failed.");
                    setStream(false);
                    return;
                }
                streamframeCount = 0;
            }
        }
#endif

        if (StreamSP.s == IPS_BUSY)
        {
            if (uploadStream(buffer, nbytes) == false)
            {
                LOG_ERROR("Streaming failed.");
                setStream(false);
                return;
            }
        }

        if (RecordStreamSP.s == IPS_BUSY)
        {
            if (recordStream(buffer, nbytes, deltams) == false)
            {
                LOG_ERROR("Recording failed.");
                stopRecording(true);
                return;
            }
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
    if (!m_isRecording)
        return false;

    bool rc = recorder->writeFrame(buffer, nbytes);
    if (rc == false)
        return rc;

    m_RecordingFrameDuration += deltams;
    m_RecordingFrameTotal += 1;

    if ((RecordStreamSP.sp[1].s == ISS_ON) && (m_RecordingFrameDuration >= (RecordOptionsNP.np[0].value * 1000.0)))
    {
        LOGF_INFO("Ending record after %g millisecs", m_RecordingFrameDuration);
        stopRecording();
        RecordStreamSP.sp[1].s = ISS_OFF;
        RecordStreamSP.sp[3].s = ISS_ON;
        RecordStreamSP.s       = IPS_IDLE;
        IDSetSwitch(&RecordStreamSP, nullptr);
    }

    if ((RecordStreamSP.sp[2].s == ISS_ON) && (m_RecordingFrameTotal >= (RecordOptionsNP.np[1].value)))
    {
        LOGF_INFO("Ending record after %d frames", m_RecordingFrameTotal);
        stopRecording();
        RecordStreamSP.sp[2].s = ISS_OFF;
        RecordStreamSP.sp[3].s = ISS_ON;
        RecordStreamSP.s       = IPS_IDLE;
        IDSetSwitch(&RecordStreamSP, nullptr);
    }

    return true;
}

int StreamManager::mkpath(std::string s, mode_t mode)
{
    size_t pre = 0, pos;
    std::string dir;
    int mdret = 0;
    struct stat st;

    if (s[s.size() - 1] != '/')
        s += '/';

    while ((pos = s.find_first_of('/', pre)) != std::string::npos)
    {
        dir = s.substr(0, pos++);
        pre = pos;
        if (dir.size() == 0)
            continue;
        if (stat(dir.c_str(), &st))
        {
            if (errno != ENOENT || ((mdret = mkdir(dir.c_str(), mode)) && errno != EEXIST))
            {
                LOGF_WARN("mkpath: can not create %s", dir.c_str());
                return mdret;
            }
        }
        else
        {
            if (!S_ISDIR(st.st_mode))
            {
                LOGF_WARN("mkpath: %s is not a directory", dir.c_str());
                return -1;
            }
        }
    }
    return mdret;
}

std::string StreamManager::expand(const std::string &fname, const std::map<std::string, std::string> &patterns)
{
    std::string res = fname;
    std::size_t pos;
    time_t now;
    struct tm * tm_now;
    char val[20];
    *(val + 19) = '\0';

    time(&now);
    tm_now = gmtime(&now);

    pos = res.find("_D_");
    if (pos != std::string::npos)
    {
        strftime(val, 11, "%F", tm_now);
        res.replace(pos, 3, val);
    }

    pos = res.find("_T_");
    if (pos != std::string::npos)
    {
        strftime(val, 20, "%F@%T", tm_now);
        res.replace(pos, 3, val);
    }

    pos = res.find("_H_");
    if (pos != std::string::npos)
    {
        strftime(val, 9, "%T", tm_now);
        res.replace(pos, 3, val);
    }

    for (std::map<std::string, std::string>::const_iterator it = patterns.begin(); it != patterns.end(); ++it)
    {
        pos = res.find(it->first);
        if (pos != std::string::npos)
        {
            res.replace(pos, it->first.size(), it->second);
        }
    }

    // Replace all : to - to be valid filename on Windows
    size_t start_pos = 0;
    while ((start_pos = res.find(":", start_pos)) != std::string::npos)
    {
        res.replace(start_pos, 1, "-");
        start_pos++;
    }

    return res;
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
        if (dynamic_cast<INDI::CCD*>(currentDevice)->CurrentFilterSlot != -1 && dynamic_cast<INDI::CCD*>(currentDevice)->CurrentFilterSlot <= static_cast<int>(dynamic_cast<INDI::CCD*>(currentDevice)->FilterNames.size()))
        {
            filtername      = dynamic_cast<INDI::CCD*>(currentDevice)->FilterNames.at(dynamic_cast<INDI::CCD*>(currentDevice)->CurrentFilterSlot - 1);
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
    m_RecordingFrameDuration   = 0.0;
    m_RecordingFrameTotal = 0;

    getitimer(ITIMER_REAL, &tframe1);
    mssum         = 0;
    m_FrameCounterPerSecond = 0;
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
    else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::DETECTOR_INTERFACE)
    {
        if (m_isStreaming == false && dynamic_cast<INDI::Detector*>(currentDevice)->StartStreaming() == false)
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
    else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::DETECTOR_INTERFACE)
    {
        if (!m_isStreaming)
            dynamic_cast<INDI::Detector*>(currentDevice)->StopStreaming();

    }

    m_isRecording = false;
    recorder->close();

    if (force)
        return false;

    LOGF_INFO("Record Duration(millisec): %g -- Frame count: %d", m_RecordingFrameDuration,
              m_RecordingFrameTotal);
    return true;
}

bool StreamManager::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(getDeviceName(), dev))
        return true;

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

        if (m_isRecording && RecordStreamSP.sp[3].s != ISS_ON)
        {
            IUResetSwitch(&RecordStreamSP);
            RecordStreamS[prevSwitch].s = ISS_ON;
            IDSetSwitch(&RecordStreamSP, nullptr);
            LOG_WARN("Recording device is busy.");
            return false;
        }

        if ((RecordStreamSP.sp[0].s == ISS_ON) || (RecordStreamSP.sp[1].s == ISS_ON) ||
                (RecordStreamSP.sp[2].s == ISS_ON))
        {
            if (!m_isRecording)
            {
                RecordStreamSP.s = IPS_BUSY;
                if (RecordStreamSP.sp[1].s == ISS_ON)
                    LOGF_INFO("Starting video record (Duration): %g secs.", RecordOptionsNP.np[0].value);
                else if (RecordStreamSP.sp[2].s == ISS_ON)
                    LOGF_INFO("Starting video record (Frame count): %d.", static_cast<int>(RecordOptionsNP.np[1].value));
                else
                    LOG_INFO("Starting video record.");

                if (!startRecording())
                {
                    RecordStreamSP.sp[0].s = ISS_OFF;
                    RecordStreamSP.sp[1].s = ISS_OFF;
                    RecordStreamSP.sp[2].s = ISS_OFF;
                    RecordStreamSP.sp[3].s = ISS_ON;
                    RecordStreamSP.s       = IPS_ALERT;
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
                LOGF_INFO("Recording stream has been disabled. Frame count %d", m_RecordingFrameTotal);
                stopRecording();
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
    }

    return true;
}

bool StreamManager::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    /* ignore if not ours */
    if (dev != nullptr && strcmp(getDeviceName(), dev))
        return true;

    if (!strcmp(name, RecordFileTP.name))
    {
        IText * tp = IUFindText(&RecordFileTP, "RECORD_FILE_NAME");
        if (strchr(tp->text, '/'))
        {
            LOG_WARN("Dir. separator (/) not allowed in filename.");
            return false;
        }

        IUUpdateText(&RecordFileTP, texts, names, n);
        IDSetText(&RecordFileTP, nullptr);
        return true;
    }
    return true;
}

bool StreamManager::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    /* ignore if not ours */
    if (dev != nullptr && strcmp(getDeviceName(), dev))
        return true;

    if (!strcmp(StreamExposureNP.name, name))
    {
        IUUpdateNumber(&StreamExposureNP, values, names, n);
        StreamExposureNP.s = IPS_OK;
        IDSetNumber(&StreamExposureNP, nullptr);
        return true;
    }

    /* Record Options */
    if (!strcmp(RecordOptionsNP.name, name))
    {
        if (m_isRecording)
        {
            LOG_WARN("Recording device is busy");
            return false;
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
            return false;
        }

        int subW = 0;
        int subH = 0;

        if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
        {
            subW = dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getSubW() / dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinX();
            subH = dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getSubH() / dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinY();
        }
        else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::DETECTOR_INTERFACE)
        {
            subW = dynamic_cast<INDI::Detector*>(currentDevice)->PrimaryDetector.getContinuumBufferSize() * 8 / dynamic_cast<INDI::Detector*>(currentDevice)->PrimaryDetector.getBPS();
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

    /* Frames to drop */
    /*if (!strcmp (FramestoDropNP.name, name))
      {
        IUUpdateNumber(&FramestoDropNP, values, names, n);
        //v4l_base->setDropFrameCount(values[0]);
        FramestoDropNP.s = IPS_OK;
        IDSetNumber(&FramestoDropNP, nullptr);
        return true;
      }*/
    return true;
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
            LOGF_INFO("Starting the video stream with target exposure %.6f s (Max theoritical FPS %.f)", StreamExposureN[0].value, 1 / StreamExposureN[0].value);

            getitimer(ITIMER_REAL, &tframe1);
            mssum         = 0;
            m_FrameCounterPerSecond = 0;
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
            else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::DETECTOR_INTERFACE)
            {
                if (dynamic_cast<INDI::Detector*>(currentDevice)->StartStreaming() == false)
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
            //LOGF_DEBUG("The video stream has been disabled. Frame count %d", streamframeCount);
            //if (!is_exposing && !is_recording) stop_capturing();
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
                else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::DETECTOR_INTERFACE)
                {
                    if (dynamic_cast<INDI::Detector*>(currentDevice)->StopStreaming() == false)
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
        if (dynamic_cast<INDI::CCD*>(currentDevice)->HasWebSocket() && dynamic_cast<INDI::CCD*>(currentDevice)->WebSocketS[CCD::WEBSOCKET_ENABLED].s == ISS_ON)
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

    //memcpy(dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getFrameBuffer(), buffer, dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getFrameBufferSize());

    // Binning for grayscale frames only for now
#if 0
    if (dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getNAxis() == 2)
    {
        dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.binFrame();
        nbytes /= dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinX() * dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinY();
    }
#endif

    int subX, subY, subW, subH;
    subX = subY = 0;
    subW = subH = 0;
    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        subX = dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getSubX() / dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinX();
        subY = dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getSubY() / dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinY();
        subW = dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getSubW() / dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinX();
        subH = dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getSubH() / dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinY();
    }
    else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::DETECTOR_INTERFACE)
    {
        subX = 0;
        subY = 0;
        subW = dynamic_cast<INDI::Detector*>(currentDevice)->PrimaryDetector.getContinuumBufferSize() * 8 / dynamic_cast<INDI::Detector*>(currentDevice)->PrimaryDetector.getBPS();
        subH = 1;
    }

    //uint8_t *streamBuffer = buffer;

    // If stream frame was not yet initilized, let's do that now
    if (StreamFrameN[CCDChip::FRAME_W].value == 0 || StreamFrameN[CCDChip::FRAME_H].value == 0)
    {
        //if (dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getNAxis() == 2)
        //    binFactor = dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinX();

        StreamFrameN[CCDChip::FRAME_X].value = subX;
        StreamFrameN[CCDChip::FRAME_Y].value = subY;
        StreamFrameN[CCDChip::FRAME_W].value = subW;
        StreamFrameN[CCDChip::FRAME_W].value = subH;
        StreamFrameNP.s                      = IPS_IDLE;
        IDSetNumber(&StreamFrameNP, nullptr);
    }
    // Check if we need to subframe
    else if ((StreamFrameN[CCDChip::FRAME_W].value > 0 && StreamFrameN[CCDChip::FRAME_H].value > 0) &&
             (StreamFrameN[CCDChip::FRAME_X].value != subX || StreamFrameN[CCDChip::FRAME_Y].value != subY ||
              StreamFrameN[CCDChip::FRAME_W].value != subW || StreamFrameN[CCDChip::FRAME_H].value != subH))
    {
        uint32_t npixels = StreamFrameN[CCDChip::FRAME_W].value * StreamFrameN[CCDChip::FRAME_H].value * ((m_PixelFormat == INDI_RGB) ? 3 : 1);
        if (downscaleBufferSize < npixels)
        {
            downscaleBufferSize = npixels;
            delete [] downscaleBuffer;
            downscaleBuffer = new uint8_t[npixels];
        }

        uint32_t sourceOffset = (subW * StreamFrameN[CCDChip::FRAME_Y].value) + StreamFrameN[CCDChip::FRAME_X].value;
        uint8_t components = (m_PixelFormat == INDI_RGB) ? 3 : 1;

        const uint8_t * srcBuffer  = buffer + sourceOffset * components;
        uint32_t sourceStride = subW * components;

        uint8_t * destBuffer = downscaleBuffer;
        uint32_t desStride = StreamFrameN[CCDChip::FRAME_W].value * components;

        // Copy line-by-line
        for (int i = 0; i < StreamFrameN[CCDChip::FRAME_H].value; i++)
            memcpy(destBuffer + i * desStride, srcBuffer + sourceStride * i, desStride);

        nbytes = StreamFrameN[CCDChip::FRAME_W].value * StreamFrameN[CCDChip::FRAME_H].value * components;

        if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
        {
            if (encoder->upload(imageB, downscaleBuffer, nbytes, dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.isCompressed()))
            {
#ifdef HAVE_WEBSOCKET
                if (dynamic_cast<INDI::CCD*>(currentDevice)->HasWebSocket() && dynamic_cast<INDI::CCD*>(currentDevice)->WebSocketS[CCD::WEBSOCKET_ENABLED].s == ISS_ON)
                {
                    if (m_Format != ".stream")
                    {
                        m_Format = ".stream";
                        dynamic_cast<INDI::CCD*>(currentDevice)->wsServer.send_text(m_Format);
                    }

                    dynamic_cast<INDI::CCD*>(currentDevice)->wsServer.send_binary(downscaleBuffer, nbytes);
                    return true;
                }
#endif
                // Upload to client now
                imageBP->s = IPS_OK;
                IDSetBLOB(imageBP, nullptr);
                return true;
            }
        }
        else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::DETECTOR_INTERFACE)
        {
            if (encoder->upload(imageB, downscaleBuffer, nbytes, false))//dynamic_cast<INDI::Detector*>(currentDevice)->PrimaryDetector.isCompressed())
            {
                /*
                            if (dynamic_cast<INDI::Detector*>(currentDevice)->HasWebSocket() && dynamic_cast<INDI::Detector*>(currentDevice)->WebSocketS[Detector::WEBSOCKET_ENABLED].s == ISS_ON)
                            {
                                if (m_Format != ".stream")
                                {
                                    m_Format = ".stream";
                                    dynamic_cast<INDI::Detector*>(currentDevice)->wsServer.send_text(m_Format);
                                }

                                dynamic_cast<INDI::Detector*>(currentDevice)->wsServer.send_binary(downscaleBuffer, nbytes);
                                return true;
                            }
                */
                // Upload to client now
                imageBP->s = IPS_OK;
                IDSetBLOB(imageBP, nullptr);
                return true;
            }
        }

        return false;
    }
#if 0
    // For MONO
    if (dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getNAxis() == 2)
    {
        int binFactor = dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinX() * dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.getBinY();
        int offset =
            ((subW * StreamFrameN[CCDChip::FRAME_Y].value) + StreamFrameN[CCDChip::FRAME_X].value) / binFactor;

        uint8_t * srcBuffer  = buffer + offset;
        uint8_t * destBuffer = buffer;

        for (int i = 0; i < StreamFrameN[CCDChip::FRAME_H].value; i++)
            memcpy(destBuffer + i * static_cast<int>(StreamFrameN[CCDChip::FRAME_W].value), srcBuffer + subW * i,
                   StreamFrameN[CCDChip::FRAME_W].value);

        streamW = StreamFrameN[CCDChip::FRAME_W].value;
        streamH = StreamFrameN[CCDChip::FRAME_H].value;
    }
    // For Color
    else
    {
        // Subframe offset in source frame. i.e. where we start copying data from in the original data frame
        int sourceOffset = (subW * StreamFrameN[CCDChip::FRAME_Y].value) + StreamFrameN[CCDChip::FRAME_X].value;
        // Total bytes
        //totalBytes = (StreamFrameN[CCDChip::FRAME_W].value * StreamFrameN[CCDChip::FRAME_H].value) * 3;

        // Copy each color component back into buffer. Since each subframed page is equal or small than source component
        // no need to a new buffer

        uint8_t * srcBuffer  = buffer + sourceOffset * 3;
        uint8_t * destBuffer = buffer;

        // RGB
        for (int i = 0; i < StreamFrameN[CCDChip::FRAME_H].value; i++)
            memcpy(destBuffer + i * static_cast<int>(StreamFrameN[CCDChip::FRAME_W].value * 3),
                   srcBuffer + subW * 3 * i, StreamFrameN[CCDChip::FRAME_W].value * 3);
    }
#endif

    if(currentDevice->getDriverInterface() & INDI::DefaultDevice::CCD_INTERFACE)
    {
        if (encoder->upload(imageB, buffer, nbytes, dynamic_cast<INDI::CCD*>(currentDevice)->PrimaryCCD.isCompressed()))
        {
#ifdef HAVE_WEBSOCKET
            if (dynamic_cast<INDI::CCD*>(currentDevice)->HasWebSocket() && dynamic_cast<INDI::CCD*>(currentDevice)->WebSocketS[CCD::WEBSOCKET_ENABLED].s == ISS_ON)
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
    else if(currentDevice->getDriverInterface() & INDI::DefaultDevice::DETECTOR_INTERFACE)
    {
        if (encoder->upload(imageB, buffer, nbytes, false))//dynamic_cast<INDI::Detector*>(currentDevice)->PrimaryDetector.isCompressed()))
        {
            /*
                        if (dynamic_cast<INDI::Detector*>(currentDevice)->HasWebSocket() && dynamic_cast<INDI::Detector*>(currentDevice)->WebSocketS[Detector::WEBSOCKET_ENABLED].s == ISS_ON)
                        {
                            if (m_Format != ".stream")
                            {
                                m_Format = ".stream";
                                dynamic_cast<INDI::Detector*>(currentDevice)->wsServer.send_text(m_Format);
                            }

                            dynamic_cast<INDI::Detector*>(currentDevice)->wsServer.send_binary(buffer, nbytes);
                            return true;
                        }
            */
            // Upload to client now
            imageBP->s = IPS_OK;
            IDSetBLOB(imageBP, nullptr);
            return true;
        }
    }

    return false;
}

void StreamManager::prepareGammaLUT(double gamma, double a, double b, double Ii)
{
    if (!gammaLUT_16_8) gammaLUT_16_8 = new uint8_t[65536];

    for (int i = 0; i < 65536; i++)
    {
        double I = static_cast<double>(i) / 65535.0;
        double p;
        if (I <= Ii)
            p = a * I;
        else
            p = (1 + b) * powf(I, 1.0 / gamma) - b;
        gammaLUT_16_8[i] = round(255.0 * p);
    }
}

}
