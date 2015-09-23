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

#ifndef STREAM_RECORDER_H
#define STREAM_RECORDER_H

#include <stdint.h>
#include <string>
#include <map>

#include <indiccd.h>
#include <indidevapi.h>
#include "v4l2_record.h"

class StreamRecorder
{
public:
    typedef enum
    {
        IMAGE_MONO_8,         // Only 8 bit mono
        IMAGE_RGB_32         // Only 32 bit RGB color
    } ImageColorSpace;

    enum
    {
        RECORD_ON,
        RECORD_TIME,
        RECORD_FRAME,
        RECORD_OFF
    };

    StreamRecorder(INDI::CCD *mainCCD);
    ~StreamRecorder();

    virtual void ISGetProperties (const char *dev);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);

    virtual bool initProperties();
    virtual bool updateProperties();

    /**
     * @brief newFrame CCD drivers calls this function when a new frame is received.
     */
    void newFrame(unsigned char *buffer, ImageColorSpace imageType = IMAGE_MONO_8);

    void recordStream(double deltams, unsigned char *buffer, ImageColorSpace imageType);

   bool setStream(bool enable);
   // uint8_t getFramesToDrop() { return (uint8_t) FramestoDropN[0].value; }

    V4L2_Recorder *getRecorder() { return recorder; }
    bool isDirectRecording() { return direct_record; }
    bool isStreaming() { return is_streaming; }
    bool isRecording() { return is_recording; }
    bool isBusy()      { return (isStreaming() || isRecording()); }
    const char *getDeviceName() { return ccd->getDeviceName(); }

    void setRecorderSize(uint16_t width, uint16_t height);
    bool setPixelFormat(uint32_t format);
    bool close();

protected:

    INDI::CCD *ccd;

private:

    /* Utility for record file */
    int mkpath(std::string s, mode_t mode);
    std::string expand(std::string fname, const std::map<std::string, std::string>& patterns);

    bool startRecording();
    bool stopRecording();

    bool uploadStream(uint8_t *buffer);

    /* Stream switch */
    ISwitch StreamS[2];
    ISwitchVectorProperty StreamSP;

    /* Record switch */
    ISwitch RecordStreamS[4];
    ISwitchVectorProperty RecordStreamSP;

    /* How many frames to drop */
    //INumber FramestoDropN[1];
    //INumberVectorProperty FramestoDropNP;

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


#endif // STREAM_RECORDER_H
