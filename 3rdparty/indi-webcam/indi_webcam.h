/*
INDI Webcam CCD Driver

Copyright (C) 2018 Robert Lancaster (rlancaste AT gmail DOT com)

This driver was inspired by the INDI FFMpeg Driver written by
Geehalel (geehalel AT gmail DOT com), see: https://github.com/geehalel/indi-ffmpeg

This driver is free software; you can redistribute it and/or
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

#ifndef indi_webcam_H
#define indi_webcam_H

#include <indiccd.h>
#include <stream/streammanager.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>  
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
  
#ifdef __cplusplus 
}
#endif
//#include <ctime>
#include <thread>

//These are required to check for AVFoundation Devices
//The reason is that we have to print and parse the output
//These can't be in indi_webcam class declaration because the callback method has to be passed to FFMpeg
//We need access to these variables in the callback method
std::vector<std::string> listOfSources;
bool allDevicesFound = false;
bool checkingDevices = false;

class indi_webcam : public INDI::CCD
{
public:
    indi_webcam();
    ~indi_webcam();
    void ISGetProperties(const char *dev) override;
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) override;
protected:
    // General device functions
    bool Connect() override;
    bool Disconnect() override;
    const char *getDefaultName() override;
    bool initProperties() override;
    bool updateProperties() override;

    void debugTriggered(bool enabled) override;



    //Related to exposures
    bool StartExposure(float duration) override;
    bool AbortExposure() override;
    void finishExposure();
    void TimerHit() override;
    float CalcTimeLeft();
    int timerID;
    bool grabImage();

    bool UpdateCCDFrame(int x, int y, int w, int h) override;

    //Related to streaming
    virtual bool StartStreaming() override;
    virtual bool StopStreaming() override;

    bool saveConfigItems(FILE *fp) override;
    
private:

    //Related to exposures
    struct timeval ExpStart { 0, 0 };
    float ExposureRequest { 0 };
    bool convertINDI_RGBtoFITS_RGB(uint8_t *originalImage, uint8_t *convertedImage);

    //These are related to how we change sources
    bool ConnectToSource(std::string device, std::string source, int framerate, std::string videosize, std::string htmlSource);
    bool ChangeSource(std::string newDevice, std::string newSource, int newFramerate, std::string newVideosize);
    bool ChangeHTMLSource(std::string newIPAddress, std::string newPort, std::string newUserName, std::string newPassword);
    bool reconnectSource();

    //These are related to updating the device list
    void findAVFoundationVideoSources();
    int getNumOfInputDevices();
    bool refreshInputDevices();
    bool refreshInputSources();
    ISwitch RefreshS[1];
    ISwitchVectorProperty RefreshSP;

    //webcam stacking.
    bool webcamStacking;
    bool averaging;
    float *stackBuffer;
    int numberOfFramesInStack;
    bool addToStack();
    void copyFinalStackToPrimaryFrameBuffer();
    void setImageDataValueFromFloat(int x, int y, float value, bool roundAnswer);
    void setRGBImageDataValueFromFloat(int x, int y, int channel, float value, bool roundAnswer);
    float getImageDataFloatValue(int x, int y);
    float getRGBImageDataFloatValue(int x, int y, int channel);

    //These are our device capture settings
    bool use16Bit = true;
    std::string videoDevice;
    std::string videoSource;
    int frameRate;
    std::string videoSize;
    std::string outputFormat;
    //These are our online device capture settings
    std::string IPAddress;
    std::string port;
    std::string username;
    std::string password;

    //The timeout for avformat commands like av_open_input and av_read_frame
    std::string ffmpegTimeout;
    //The timeout for how long of a wait time constitutes a buffered frame vs a new frame
    std::string bufferTimeout;

    //Related to Options in the Control Panel
    IText InputOptionsT[4];
    ITextVectorProperty InputOptionsTP;
    IText HTTPInputOptions[4];
    ITextVectorProperty HTTPInputOptionsP;
    ISwitch *CaptureDevices = nullptr;
    ISwitchVectorProperty CaptureDeviceSelection;
    ISwitch *CaptureSources = nullptr;
    ISwitchVectorProperty CaptureSourceSelection;
    ISwitch *FrameRates = nullptr;
    ISwitchVectorProperty FrameRateSelection;
    ISwitch *VideoSizes = nullptr;
    ISwitchVectorProperty VideoSizeSelection;
    ISwitch *RapidStacking = nullptr;
    ISwitchVectorProperty RapidStackingSelection;
    ISwitch *OutputFormats = nullptr;
    ISwitchVectorProperty OutputFormatSelection;
    IText TimeoutOptionsT[2];
    ITextVectorProperty TimeoutOptionsTP;


    //Webcam setup, release, and frame capture
    bool setupStreaming();
    void freeMemory();
    bool getStreamFrame();
    bool flush_frame_buffer();

    //Related to streaming
    std::thread capture_thread;
    static void RunCaptureThread(indi_webcam *webcam);
    void run_capture();
    bool is_capturing;
    bool is_streaming;
    void start_capturing();
    void stop_capturing();    

    //FFMpeg Variables to make captures work.
    struct SwsContext *sws_ctx;
    uint8_t *buffer;
    int numBytes;
    AVPixelFormat out_pix_fmt;
    AVFormatContext *pFormatCtx;
    int              videoStream;
    AVCodecContext  *pCodecCtx;
    AVCodec         *pCodec;
    AVFrame         *pFrame; 
    AVFrame         *pFrameOUT;
    AVDictionary *optionsDict;
    
};
#endif // indi_webcam_H
