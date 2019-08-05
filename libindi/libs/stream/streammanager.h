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

#pragma once

#include "indidevapi.h"
#include "recorder/recordermanager.h"
#include "encoder/encodermanager.h"

#include <string>
#include <map>
#include <sys/time.h>

#include <stdint.h>

/**
 * \class StreamManager
   \brief Class to provide video streaming and recording functionality.

   INDI::CCD can utilize this class to add streaming and recording functionality to the driver.

   Transfer of the video stream is done via the same BLOB property \e CCD1 used for transfer of image data to the client. Therefore,
   it is not possible to transmit image data and video stream at the same time. Two formats are accepted for video streaming:

   + Grayscale 8bit frame that represents intensity/lumienance.
   + Color 24bit RGB frame.

   Use setPixelFormat() and setSize() before uploading the stream data. 16bit frames are only supported in some recorders. You can send
   16bit frames, but they will be downscaled to 8bit when necessary for streaming and recording purposes. Base classes must implement
   startStreaming() and stopStreaming() functions. When a frame is ready, use uploadStream() to send the data to active encoders and recorders.

   It is highly recommended to implement the streaming functionality in a dedicated thread.

   \section Encoders

   Encoders are responsible for encoding the frame and transmitting it to the client. The CCD1 BLOB format is set to the desired format.
   Default encoding format is RAW (format = ".stream").

   Currently, two encoders are supported:

   1. RAW Encoder: Frame is sent as is (lossless). If compression is enabled, the frame is compressed with zlib. Uncompressed format is ".stream"
   and compressed format is ".stream.z"
   2. MJPEG Encoder: Frame is encoded to a JPEG image before being transmitted. Format is ".stream_jpg"

   \section Recorders

   Recorders are responsible for recording the video stream to a file. The recording file directory and name can be set via the RECORD_FILE
   property which is composed of RECORD_FILE_DIR and RECORD_FILE_NAME elements. You can specify a record directory name together with a file name.
   You may use special character sequences to generate dynamic names:
   * _D_ is replaced with the date ('YYYY-MM-DD')
   * _H_ is replaced with the time ('hh-mm-ss')
   * _T_ is replaced with a timestamp
   * _F_ is replaced with the filter name currently in use (see Snoop Devices in Options tab)

   Currently, two recorders are supported:

   1. SER recorder: Saves video streams along with timestamps in <a href=http://www.grischa-hahn.homepage.t-online.de/astro/ser/">SER format</a>.
   2. OGV recorder: Saves video streams in libtheora OGV files. INDI must be compiled with the optional OGG Theora support for this functionality to be
   available. Frame rate is estimated from the average FPS.

   \section Subframing

   By default, the full image width and height are used for transmitting the data. Subframing is possible by updating the CCD_STREAM_FRAME
   property. All values set in this property must be set in BINNED coordinates, unlike the CCD_FRAME which is set in UNBINNED coordinates.

   \example Check CCD Simulator, V4L2 CCD, and ZWO ASI drivers for example implementations.

\author Jasem Mutlaq
\author Jean-Luc Geehalel
*/
namespace INDI
{

class CCD;
class Detector;

class StreamManager
{
    public:
        enum
        {
            RECORD_ON,
            RECORD_TIME,
            RECORD_FRAME,
            RECORD_OFF
        };

        StreamManager(DefaultDevice *currentDevice);
        virtual ~StreamManager();

        virtual void ISGetProperties(const char *dev);
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

        virtual bool initProperties();
        virtual bool updateProperties();

        virtual bool saveConfigItems(FILE *fp);

        /**
             * @brief newFrame CCD drivers call this function when a new frame is received. It is then streamed, or recorded, or both according to the settings in the streamer.
             */
        void newFrame(const uint8_t *buffer, uint32_t nbytes);

        /**
         * @brief asyncStream Upload the stream asynchronously in a separate thread. ccdBufferLock mutex shall be locked until the record/upload
         * operation is complete.
         * @param buffer Buffer to stream/record
         * @param nbytes size of buffer.
         */
        void asyncStream(const uint8_t *buffer, uint32_t nbytes, double deltams);

        /**
             * @brief setStream Enables (starts) or disables (stops) streaming.
             * @param enable True to enable, false to disable
             * @return True if operation is successful, false otherwise.
             */
        bool setStream(bool enable);

        RecorderInterface *getRecorder()
        {
            return recorder;
        }
        bool isDirectRecording()
        {
            return direct_record;
        }
        bool isStreaming()
        {
            return m_isStreaming;
        }
        bool isRecording()
        {
            return m_isRecording;
        }
        bool isBusy()
        {
            return (isStreaming() || isRecording());
        }
        double getTargetFPS()
        {
            return 1.0 / StreamExposureN[0].value;
        }
        double getTargetExposure()
        {
            return StreamExposureN[0].value;
        }

        uint8_t *getDownscaleBuffer()
        {
            return downscaleBuffer;
        }
        uint32_t getDownscaleBufferSize()
        {
            return downscaleBufferSize;
        }

        const char *getDeviceName();

        void setSize(uint16_t width, uint16_t height);
        bool setPixelFormat(INDI_PIXEL_FORMAT pixelFormat, uint8_t pixelDepth = 8);
        void getStreamFrame(uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h);

        /**
         * @brief setStreamingExposureEnabled Can stream exposure time be changed?
         * @param enable True if we can control the exact exposure time for each frame in the stream, false otherwise.
         */
        void setStreamingExposureEnabled(bool enable)
        {
            m_hasStreamingExposure = enable;
        }
        bool close();

    protected:
        DefaultDevice *currentDevice = nullptr;

    private:
        /* Utility for record file */
        int mkpath(std::string s, mode_t mode);
        std::string expand(const std::string &fname, const std::map<std::string, std::string> &patterns);

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

        void prepareGammaLUT(double gamma = 2.4, double a = 12.92, double b = 0.055, double Ii = 0.00304);

        /* Stream switch */
        ISwitch StreamS[2];
        ISwitchVectorProperty StreamSP;

        /* Record switch */
        ISwitch RecordStreamS[4];
        ISwitchVectorProperty RecordStreamSP;

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

        bool m_isStreaming { false };
        bool m_isRecording { false };
        bool m_hasStreamingExposure { true };

        uint32_t m_RecordingFrameTotal {0};
        double m_RecordingFrameDuration {0};

        // Recorder
        RecorderManager *recorderManager = nullptr;
        RecorderInterface *recorder = nullptr;
        bool direct_record = false;
        std::string recordfiledir, recordfilename; /* in case we should move it */

        // Encoders
        EncoderManager *encoderManager = nullptr;
        EncoderInterface *encoder = nullptr;

        // Measure FPS
        struct itimerval tframe1, tframe2;
        uint32_t mssum = 0, m_FrameCounterPerSecond = 0;

        INDI_PIXEL_FORMAT m_PixelFormat = INDI_MONO;
        uint8_t m_PixelDepth = 8;
        uint16_t rawWidth = 0, rawHeight = 0;
        std::string m_Format;

        // Downscale buffer for streaming
        uint8_t *downscaleBuffer = nullptr;
        uint32_t downscaleBufferSize = 0;

        uint8_t *gammaLUT_16_8 = nullptr;
};
}
