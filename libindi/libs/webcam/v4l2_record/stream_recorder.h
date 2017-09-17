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

#include "indiccd.h"
#include "indidevapi.h"
#include "v4l2_record.h"

#include <string>
#include <map>

#include <stdint.h>

/**
 * \class StreamRecorder
   \brief Class to provide video streaming and recording functionality.

   INDI::CCD can utilize this class to add streaming and recording functionality to their driver. Currently, only SER recording backend is supported.

   \example Check V4L2 CCD and ZWO ASI drivers for example implementations.

\author Jean-Luc Geehalel, Jasem Mutlaq
*/
class StreamRecorder
{
  public:
    enum
    {
        RECORD_ON,
        RECORD_TIME,
        RECORD_FRAME,
        RECORD_OFF
    };

    StreamRecorder(INDI::CCD *mainCCD);
    ~StreamRecorder();

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
    void newFrame();
    /**
         * @brief recordStream Calls the backend recorder to record a single frame.
         * @param deltams time in milliseconds since last frame
         */
    void recordStream(double deltams);

    /**
         * @brief setStream Enables (starts) or disables (stops) streaming.
         * @param enable True to enable, false to disable
         * @return True if operation is successful, false otherwise.
         */
    bool setStream(bool enable);

    V4L2_Recorder *getRecorder() { return recorder; }
    bool isDirectRecording() { return direct_record; }
    bool isStreaming() { return is_streaming; }
    bool isRecording() { return is_recording; }
    bool isBusy() { return (isStreaming() || isRecording()); }
    const char *getDeviceName() { return ccd->getDeviceName(); }

    void setRecorderSize(uint16_t width, uint16_t height);
    bool setPixelFormat(uint32_t format);
    void getStreamFrame(uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h);
    bool close();

  protected:
    INDI::CCD *ccd;

  private:
    /* Utility for record file */
    int mkpath(std::string s, mode_t mode);
    std::string expand(std::string fname, const std::map<std::string, std::string> &patterns);

    bool startRecording();
    bool stopRecording();

    bool uploadStream();

    /* Stream switch */
    ISwitch StreamS[2];
    ISwitchVectorProperty StreamSP;

    /* Record switch */
    ISwitch RecordStreamS[4];
    ISwitchVectorProperty RecordStreamSP;

    /* Record File Info */
    IText RecordFileT[2];
    ITextVectorProperty RecordFileTP;

    /* Streaming Options */
    INumber StreamOptionsN[1];
    INumberVectorProperty StreamOptionsNP;

    /* Measured FPS */
    INumber FpsN[2];
    INumberVectorProperty FpsNP;

    /* Record Options */
    INumber RecordOptionsN[2];
    INumberVectorProperty RecordOptionsNP;

    // Stream Frame
    INumberVectorProperty StreamFrameNP;
    INumber StreamFrameN[4];

    /* BLOBs */
    IBLOBVectorProperty *imageBP;
    IBLOB *imageB;

    bool is_streaming;
    bool is_recording;

    int streamframeCount;
    int recordframeCount;
    double recordDuration;

    uint8_t *compressedFrame;

    // Record frames
    V4L2_Record *v4l2_record;
    V4L2_Recorder *recorder;
    bool direct_record;
    std::string recordfiledir, recordfilename; /* in case we should move it */

    // Measure FPS
    // timer_t fpstimer;
    // struct itimerspec tframe1, tframe2;
    // use bsd timers
    struct itimerval tframe1, tframe2;
    double mssum, framecountsec;
};
