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

#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <eventloop.h>

#include "indi_webcam.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "libavutil/dict.h"
#ifdef __cplusplus 
}
#endif

#include "config.h"

std::unique_ptr<indi_webcam> webcam(new indi_webcam());

void ISInit()
{
    static int isInit =0;
    if (isInit == 1)
        return;
     isInit = 1;
     if(webcam.get() == 0) webcam.reset(new indi_webcam());
}
void ISGetProperties(const char *dev)
{
         ISInit();
         webcam->ISGetProperties(dev);
}
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
         ISInit();
         webcam->ISNewSwitch(dev, name, states, names, num);
}
void ISNewText( const char *dev, const char *name, char *texts[], char *names[], int num)
{
         ISInit();
         webcam->ISNewText(dev, name, texts, names, num);
}
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
         ISInit();
         webcam->ISNewNumber(dev, name, values, names, num);
}
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
   INDI_UNUSED(dev);
   INDI_UNUSED(name);
   INDI_UNUSED(sizes);
   INDI_UNUSED(blobsizes);
   INDI_UNUSED(blobs);
   INDI_UNUSED(formats);
   INDI_UNUSED(names);
   INDI_UNUSED(n);
}
void ISSnoopDevice (XMLEle *root)
{
     ISInit();
     webcam->ISSnoopDevice(root);
}


//Note this is how we get information about AVFoundation Devices
//FFMpeg does not provide a way to programmatically get them, but there is a way to log them.
//So we capture the logging and parse it to get the list of devices.
//This is the function FFMpeg calls.
void logDevices(void *ptr, int level, const char *fmt, va_list vargs)
{
    int printPrefix = 1;
    int lineSize = 1024;
    char *lineBuffer = (char *)malloc(1024);
    av_log_format_line(ptr, level, fmt, vargs, lineBuffer, lineSize, &printPrefix);
    if(checkingDevices)
    {
        if(allDevicesFound)
        {
            free(lineBuffer);
            return;
        }
        if(strstr(lineBuffer, "AVFoundation video devices:") != NULL)
        {
            free(lineBuffer);
            return;
        }
        if(strstr(lineBuffer, "AVFoundation audio devices:") != NULL)
        {
            allDevicesFound = true;
            free(lineBuffer);
            return;
        }
        std::string device = lineBuffer+45;
        listOfSources.push_back(device);
    } else
    {
        fprintf(stderr, "%s", lineBuffer);
    }
    free(lineBuffer);
}

//This method finds AVFoundation Devices.  Please see description above.
void indi_webcam::findAVFoundationVideoSources()
{
    //Need to disconnect streaming if it is running
    bool was_streaming = false;
    if(is_streaming)
    {
        was_streaming = true;
        StopStreaming();
    }

    //Need to disconnect the source to probe the streams
    if(isConnected())
    {
        avcodec_close(pCodecCtx);
        avformat_close_input(&pFormatCtx);
    }
    else
    {
        //This appears to be needed for the list to get updated if a device was not connected
        //But it is fine to do without this the first time, so if the size of the list of sources is 0
        //Then it should be the first time and good to go.
        if(listOfSources.size()!=0)
        {
            ConnectToSource("avfoundation", "default", frameRate, videoSize);
            avcodec_close(pCodecCtx);
            avformat_close_input(&pFormatCtx);
        }
    }

    listOfSources.clear();
    allDevicesFound = false;
    checkingDevices = true;

    //This is how we tell FFMpeg to call our method to log the sources.
    av_log_set_callback(logDevices);

    //This set of commands should open the avfoundation device to list its sources
    AVDictionary* options = NULL;
    av_dict_set(&options,"list_devices","true",0);
    AVInputFormat *iformat = av_find_input_format("avfoundation");
    avformat_open_input(&pFormatCtx,"",iformat,&options);
    avformat_close_input(&pFormatCtx);
    checkingDevices = false;

    //Need to hook back up the source if it should be connected
    if(isConnected())
        ConnectToSource(videoDevice, videoSource, frameRate, videoSize);

    //Hook back up streaming if it should be running
    if(was_streaming)
        StartStreaming();
}

indi_webcam::indi_webcam()
{
  setVersion(WEBCAM_VERSION_MAJOR, WEBCAM_VERSION_MINOR);

  pFormatCtx = NULL;
  pCodecCtx = NULL;
  pCodec = NULL;
  optionsDict=NULL;
  pFrame = NULL; 
  pFrameOUT = NULL;
  sws_ctx = NULL;
  buffer = NULL;

  //This registers all devices
  avdevice_register_all();
  //This registers all codecs
  avcodec_register_all();

  //setting default values
#ifdef __linux__
  videoDevice = "video4linux2,v4l2";
#elif __APPLE__
  videoDevice = "avfoundation";
#else
  videoDevice = ""
#endif
  videoSource = "0";
  frameRate = 30;
  videoSize = "640x480";
  webcamStacking = false;
  averaging = false;
  outputFormat = "8 bit RGB";

  //Creating the format context.
  pFormatCtx = NULL;
  pFormatCtx = avformat_alloc_context();
}

indi_webcam::~indi_webcam()
{
    free(pFormatCtx);
}


/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool indi_webcam::Connect()
{
    bool rc=false;

    if(isConnected()) return true;

    ISwitchVectorProperty *connect=getSwitch("CONNECTION");
    if (connect) {
      connect->s=IPS_BUSY;
      IDSetSwitch(connect,"Connecting to source: %s, on device: %s", videoSource.c_str(), videoDevice.c_str());
    }
    DEBUGF(INDI::Logger::DBG_SESSION, "Trying to connect to: %s, on device: %s with %s at %u frames per second", videoSource.c_str(), videoDevice.c_str(), videoSize.c_str(), frameRate);
    rc=ConnectToSource(videoDevice, videoSource, frameRate, videoSize);

    return rc;
}

//This is the code that we use for FFMpeg to set up an input, connect to it, and set up the correct codecs.
bool indi_webcam::ConnectToSource(std::string device,std::string source, int framerate, std::string videosize)
{
    char stringFrameRate[16];
    snprintf(stringFrameRate,16,"%u",framerate);
    int i;
    if(isConnected())
    {
        avcodec_close(pCodecCtx);
        avformat_close_input(&pFormatCtx);
    }

    AVDictionary* options = NULL;
    av_dict_set(&options,"framerate",stringFrameRate,0);
    av_dict_set(&options, "video_size", videosize.c_str(), 0);
    av_dict_set(&options, "timeout", "2", 0); //Note this timeout doesn't seem to always work
    AVInputFormat *iformat = av_find_input_format(device.c_str());
    DEBUG(INDI::Logger::DBG_SESSION,"Attempting to connect");

    //This opens the input to get it ready for streaming.
    //Warning:  It is possible for this command to hang if the camera is there and does not respond.
    //I have not yet solved this problem.  It does not happen often.
    if (avformat_open_input(&pFormatCtx, source.c_str() , iformat , &options )!=0)
    {
      DEBUG(INDI::Logger::DBG_SESSION,"Failed to open source. Check your settings.");
      return false;
    }
    
    //This will attempt to find a video stream in the input.
    videoStream=-1;
    for(i=0; i<pFormatCtx->nb_streams; i++)
      if(pFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            videoStream=i;
    if(videoStream==-1) {
        DEBUG(INDI::Logger::DBG_SESSION,"Failed to get a video stream.");
        return false;
    }

    //Find an appropriate decoder and then
    // Allocate a pointer to the codec context for the video stream
    pCodec=avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
    pCodecCtx=avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar);

    //If an appropriate codec was not found, abort the connection
    if(pCodec==NULL)
    {
      DEBUG(INDI::Logger::DBG_SESSION,"Unsupported codec.");
      return false;      
    }

    //Attempt to open the codec.  If that fails, abort the connection.
    if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
    {
      DEBUG(INDI::Logger::DBG_SESSION,"Failed to open codec.");
      return false;
    }

    //Set the initial parameters for the CCD.
    SetCCDParams(pCodecCtx->width, pCodecCtx->height, 8, 5, 5); //Note 5 microns is a guess!

    return true;
    
}

//This is the method that should be called to change the streaming device, source, framerate, or video size
//If it was already connected, it will attempt a connection with the new settings and if it is not successful, it will revert to the old ones.
//It should be safe to use while streaming or between image captures because it will pause them and return them to normal afterwards.
bool indi_webcam::ChangeSource(std::string newDevice, std::string newSource, int newFramerate, std::string newVideosize)
{
    //This will pause the streaming while it attempts the new connection settings.
    bool was_streaming = false;
    if(is_streaming)
    {
        was_streaming = true;
        StopStreaming();
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "New Connection Settings: %s, on device: %s with %s at %u frames per second", newSource.c_str(), newDevice.c_str(), newVideosize.c_str(), newFramerate);

    //This is the case if the source is not currently connected yet.
    if(isConnected() == false)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Not connected now, accepting settings.  It will be tested on conneciton");
        videoDevice = newDevice;
        videoSource = newSource;
        frameRate = newFramerate;
        videoSize = newVideosize;
        return true;
    }

    //This is an attempt to connect, if it is already connected.  If it is not successful, it goes back to the old settings.
    if(ConnectToSource(newDevice, newSource, newFramerate, newVideosize) == false)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Connection was NOT successful");
        DEBUGF(INDI::Logger::DBG_SESSION, "Changing back to: %s, on device: %s with %s at %u frames per second", videoSource.c_str(), videoDevice.c_str(), videoSize.c_str(), frameRate);
        ConnectToSource(videoDevice, videoSource, frameRate, videoSize);
        DEBUG(INDI::Logger::DBG_SESSION, "Connection Successful");
        if(was_streaming)
            StartStreaming();
        return false;
    }

    //This is what happens if the connection was successful, it saves the settings and continues.
    DEBUG(INDI::Logger::DBG_SESSION, "Connection Successful, saving settings.");
    videoDevice = newDevice;
    videoSource = newSource;
    frameRate = newFramerate;
    videoSize = newVideosize;

    //If it was streaming, we need to reinitialize that.
    if(was_streaming)
        StartStreaming();
    return true;
}


/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool indi_webcam::Disconnect()
{
    if (isConnected()) {
      // Close the codecs
      avcodec_close(pCodecCtx);

      // Close the video file
      avformat_close_input(&pFormatCtx);
      
      DEBUG(INDI::Logger::DBG_SESSION,"INDI Webcam disconnected successfully!");
    }
    return true;
}
/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char * indi_webcam::getDefaultName()
{
    return "INDI Webcam";
}
/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool indi_webcam::initProperties()
{
    setDefaultPollingPeriod(10);

    DEBUG(INDI::Logger::DBG_SESSION, "Webcam Driver initialized");

    // Must init parent properties first!
    INDI::CCD::initProperties();



    //PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.033, 1, .1, true);

    IUFillSwitch(&RefreshS[0], "Scan Ports", "Scan Sources", ISS_OFF);
    IUFillSwitchVector(&RefreshSP, RefreshS, 1, "INDI Webcam", "INPUT_SCAN", "Refresh", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    defineSwitch(&RefreshSP);

#ifdef __linux__
    IUFillText(&InputDeviceT[0], "CAPTURE_DEVICE_TEXT", "Capture Device", "video4linux2,v4l2");
    IUFillText(&InputDeviceT[1], "CAPTURE_SOURCE_TEXT", "Capture Source", "/dev/video0");
#elif __APPLE__
    IUFillText(&InputDeviceT[0], "CAPTURE_DEVICE_TEXT", "Capture Device", "avfoundation");
    IUFillText(&InputDeviceT[1], "CAPTURE_SOURCE_TEXT", "Capture Source", "0");
#else
    IUFillText(&InputDeviceT[0], "CAPTURE_DEVICE_TEXT", "Capture Device", "");
    IUFillText(&InputDeviceT[1], "CAPTURE_SOURCE_TEXT", "Capture Source", "0");
#endif

    IUFillText(&InputDeviceT[2], "CAPTURE_FRAME_RATE", "Frame Rate", "30");
    IUFillText(&InputDeviceT[3], "CAPTURE_VIDEO_SIZE", "Video Size", "640x480");
    IUFillTextVector(&InputDeviceTP, InputDeviceT, NARRAY(InputDeviceT), getDeviceName(), "INPUT_OPTIONS", "Input Options", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    refreshInputDevices();
    refreshInputSources();

    FrameRates = new ISwitch[7];
    IUFillSwitch(&FrameRates[0], "30", "30 fps", ISS_ON);
    IUFillSwitch(&FrameRates[1], "25", "25 fps", ISS_OFF);
    IUFillSwitch(&FrameRates[2], "20", "20 fps", ISS_OFF);
    IUFillSwitch(&FrameRates[3], "15", "15 fps", ISS_OFF);
    IUFillSwitch(&FrameRates[4], "10", "10 fps", ISS_OFF);
    IUFillSwitch(&FrameRates[5], "5", "5 fps", ISS_OFF);
     IUFillSwitch(&FrameRates[6], "1", "1 fps", ISS_OFF);

    IUFillSwitchVector(&FrameRateSelection, FrameRates, 7, "INDI Webcam", "CAPTURE_FRAME_RATE", "Frame Rate",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    defineSwitch(&FrameRateSelection);

    VideoSizes = new ISwitch[7];
    IUFillSwitch(&VideoSizes[0], "320x240", "320x240", ISS_OFF);
    IUFillSwitch(&VideoSizes[1], "640x480", "640x480", ISS_ON);
    IUFillSwitch(&VideoSizes[2], "800x600", "800x600", ISS_OFF);
    IUFillSwitch(&VideoSizes[3], "1024x768", "1024x768", ISS_OFF);
    IUFillSwitch(&VideoSizes[4], "1280x720", "1280x720", ISS_OFF);
    IUFillSwitch(&VideoSizes[5], "1280x1024", "1280x1024", ISS_OFF);
    IUFillSwitch(&VideoSizes[6], "1600x1200", "1600x1200", ISS_OFF);

    IUFillSwitchVector(&VideoSizeSelection, VideoSizes, 7, "INDI Webcam", "CAPTURE_VIDEO_SIZE", "Video Size",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    defineSwitch(&VideoSizeSelection);

    RapidStacking = new ISwitch[3];
    IUFillSwitch(&RapidStacking[0], "Integration", "Integration", ISS_OFF);
    IUFillSwitch(&RapidStacking[1], "Average", "Average", ISS_OFF);
    IUFillSwitch(&RapidStacking[2], "Off", "Off", ISS_ON);

    IUFillSwitchVector(&RapidStackingSelection, RapidStacking, 3, "INDI Webcam", "RAPID_STACKING_OPTION", "Rapid Stacking",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    defineSwitch(&RapidStackingSelection);

    OutputFormats = new ISwitch[3];
    IUFillSwitch(&OutputFormats[0], "16 bit Grayscale", "16 bit Grayscale", ISS_OFF);
    IUFillSwitch(&OutputFormats[1], "16 bit RGB", "16 bit RGB", ISS_OFF);
    IUFillSwitch(&OutputFormats[2], "8 bit RGB", "8 bit RGB", ISS_ON);

    IUFillSwitchVector(&OutputFormatSelection, OutputFormats, 3, "INDI Webcam", "OUTPUT_FORMAT_OPTION", "Output Format",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    defineSwitch(&OutputFormatSelection);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();

    uint32_t cap = 0;
    cap |= CCD_HAS_STREAMING;
    cap |= CCD_CAN_SUBFRAME;
    SetCCDCapability(cap);
    return true;
}

//This refreshes the input device list
bool indi_webcam::refreshInputDevices()
{
    AVInputFormat * d=NULL;
    int i =0;
    int numDevices = getNumOfInputDevices();
    CaptureDevices = new ISwitch[numDevices];
    while ((d = av_input_video_device_next(d))) {
        if(strcmp(d->name, videoDevice.c_str()))
            IUFillSwitch(&CaptureDevices[i], d->name, d->name, ISS_ON);
        else
            IUFillSwitch(&CaptureDevices[i], d->name, d->name, ISS_OFF);
        i++;
    }
    IUFillSwitchVector(&CaptureDeviceSelection, CaptureDevices, numDevices, "INDI Webcam", "CAPTURE_DEVICE", "Capture Devices",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    defineSwitch(&CaptureDeviceSelection);

    return true;
}

//This finds the number of devices available from FFMpeg
//I had to write it because there doesn't seem to be a method available in the FFMpeg Libraries
int indi_webcam::getNumOfInputDevices()
{
    int i = 0;
    AVInputFormat * d=NULL;
    while ((d = av_input_video_device_next(d)))
    {
        i++;
    }
    return i;
}

//This finds all the sources available from the selected device.
//FFMpeg provides a way to get the sources using the list input sources method
//But on OS X, AV Foundation is not supported in this list, so I wrote a method to get them.
//There are also other cases of devices that are not supported, for them, I just load numbers for devices.
bool indi_webcam::refreshInputSources()
{
    if (CaptureSources)
        deleteProperty(CaptureSourceSelection.name);

    delete[] CaptureSources;
    CaptureSources = nullptr;

    int sourceNum = 0;
    if(videoDevice == "avfoundation")
    {
        findAVFoundationVideoSources();
        sourceNum = listOfSources.size();
        CaptureSources = new ISwitch[sourceNum];
        for(int x = 0; x<sourceNum; x++)
        {
            char num[2];
            snprintf(num,2,"%u",x);
            if(x == 0)
                IUFillSwitch(&CaptureSources[x], num, listOfSources.at(x).c_str(), ISS_ON);
            else
                IUFillSwitch(&CaptureSources[x], num, listOfSources.at(x).c_str(), ISS_OFF);
        }
    }
    else
    {
        int nbdev=0;
        struct AVDeviceInfoList *devlist = NULL;
        AVInputFormat *iformat = av_find_input_format(videoDevice.c_str());
        nbdev=avdevice_list_input_sources(iformat, NULL, NULL, &devlist);

        //For this case the source list function is not implemented, we have to just list them by number
        if ((nbdev < 0 ) || (devlist->nb_devices==0)) {
            avdevice_free_list_devices(&devlist);
            sourceNum = 5;
            CaptureSources = new ISwitch[sourceNum];
            IUFillSwitch(&CaptureSources[0], "0", "0", ISS_ON);
            for(int x = 1; x<sourceNum; x++)
            {
                char num[2];
                snprintf(num,2,"%u",x);
                IUFillSwitch(&CaptureSources[x], num, num, ISS_OFF);
            }
        }
        else
        {
            //For this case, we can use the names of the autodetected devices from FFMPEG
            sourceNum = devlist->nb_devices;
            CaptureSources = new ISwitch[sourceNum];
            for(int x = 0; x < sourceNum; x++)
            {
                if(!strcmp(devlist->devices[x]->device_name, videoSource.c_str()))
                    IUFillSwitch(&CaptureSources[x], devlist->devices[x]->device_name, devlist->devices[x]->device_name, ISS_ON);
                else
                    IUFillSwitch(&CaptureSources[x], devlist->devices[x]->device_name, devlist->devices[x]->device_name, ISS_OFF);
            }
            avdevice_free_list_devices(&devlist);
        }    
    }

    IUFillSwitchVector(&CaptureSourceSelection, CaptureSources, sourceNum, "INDI Webcam", "CAPTURE_SOURCE", "Capture Sources",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    defineSwitch(&CaptureSourceSelection);
    return true;
}


void indi_webcam::ISGetProperties(const char *dev)
{
    //DEBUGF(INDI::Logger::DBG_SESSION, "isGetProperties connected=%s",(isConnected()?"True":"False"));
    INDI::CCD::ISGetProperties(dev);

    defineText(&InputDeviceTP);

}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool indi_webcam::updateProperties()
{

    // Call parent update properties first
    INDI::CCD::updateProperties();

    return true;
}

bool indi_webcam::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
      /* ignore if not ours */
    if (dev && strcmp (getDeviceName(), dev))
      return true;
    DEBUGF(INDI::Logger::DBG_SESSION, "Setting number %s", name);
    
    return INDI::CCD::ISNewNumber(dev,name,values,names,n);
}

bool indi_webcam::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    /* ignore if not ours */
    if (dev && strcmp (getDeviceName(), dev))
      return true;

    ISwitchVectorProperty *svp = getSwitch(name);
    if (!strcmp(svp->name, CaptureDeviceSelection.name))
    {
        IUUpdateSwitch(&CaptureDeviceSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&CaptureDeviceSelection);
        if (sp)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Setting device to: %s, Refreshing Sources", sp->name);

            videoDevice = sp->name;
            IUSaveText(&InputDeviceT[0], sp->name);
            IDSetText(&InputDeviceTP, nullptr);
            CaptureDeviceSelection.s = IPS_OK;
            IDSetSwitch(&CaptureDeviceSelection, nullptr);
            refreshInputSources();
            if(isConnected())
            {
                DEBUGF(INDI::Logger::DBG_SESSION, "Disconnecting now.", sp->name);
                DEBUG(INDI::Logger::DBG_SESSION, "Please select a new source to connect to.");
                Disconnect();
            }
            return true;
        }
        return false;
    }
    if (!strcmp(svp->name, CaptureSourceSelection.name))
    {
        IUUpdateSwitch(&CaptureSourceSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&CaptureSourceSelection);
        if (sp)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Setting source to: %s", sp->name);
            if(ChangeSource(videoDevice, sp->name, frameRate, videoSize))
            {
                IUSaveText(&InputDeviceT[1], sp->name);
                IDSetText(&InputDeviceTP, nullptr);
                CaptureSourceSelection.s = IPS_OK;
                IDSetSwitch(&CaptureSourceSelection, nullptr);
                return true;
            }
        }
        return false;
    }
    if (!strcmp(svp->name, FrameRateSelection.name))
    {
        IUUpdateSwitch(&FrameRateSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&FrameRateSelection);
        if (sp)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Setting frame rate to: %u frames per second", atoi(sp->name));
            if(ChangeSource(videoDevice, videoSource, atoi(sp->name), videoSize))
            {
                IUSaveText(&InputDeviceT[2], sp->name);
                IDSetText(&InputDeviceTP, nullptr);
                FrameRateSelection.s = IPS_OK;
                IDSetSwitch(&FrameRateSelection, nullptr);
                return true;
            }
        }
        return false;
    }
    if (!strcmp(svp->name, VideoSizeSelection.name))
    {
        IUUpdateSwitch(&VideoSizeSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&VideoSizeSelection);
        if (sp)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Setting video size to: %s", sp->name);
            if(ChangeSource(videoDevice, videoSource, frameRate, sp->name))
            {
                IUSaveText(&InputDeviceT[3], sp->name);
                IDSetText(&InputDeviceTP, nullptr);
                VideoSizeSelection.s = IPS_OK;
                IDSetSwitch(&VideoSizeSelection, nullptr);
                return true;
            }
        }
        return false;
    }

    if (!strcmp(svp->name, RapidStackingSelection.name))
    {
        IUUpdateSwitch(&RapidStackingSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&RapidStackingSelection);
        if (sp)
        {
           if(!strcmp(sp->name, "Integration"))
           {
               webcamStacking = true;
               averaging = false;
           }
           if(!strcmp(sp->name, "Average"))
           {
               webcamStacking = true;
               averaging = true;
           }
           if(!strcmp(sp->name, "Off"))
           {
               webcamStacking = false;
               averaging = false;
           }
                RapidStackingSelection.s = IPS_OK;
                IDSetSwitch(&RapidStackingSelection, nullptr);
                return true;
        }
        return false;
    }

    if (!strcmp(svp->name, OutputFormatSelection.name))
    {
        IUUpdateSwitch(&OutputFormatSelection, states, names, n);
        ISwitch *sp = IUFindOnSwitch(&OutputFormatSelection);
        if (sp)
        {
            outputFormat = sp->name;
            OutputFormatSelection.s = IPS_OK;
            IDSetSwitch(&OutputFormatSelection, nullptr);
            return true;
        }
        return false;
    }

    if (!strcmp(name, RefreshSP.name))
    {
        bool a = refreshInputDevices();
        bool b = refreshInputSources();
        RefreshSP.s = (a && b) ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&RefreshSP, nullptr);
        return true;
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "Setting switch %s", name);

    return INDI::CCD::ISNewSwitch(dev,name,states,names,n);
}

bool indi_webcam::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    IText *tp;
  
    /* ignore if not ours */
    if (dev && strcmp (getDeviceName(), dev))
      return true;
    
    if (!strcmp(name, InputDeviceTP.name) )
      {
            InputDeviceTP.s = IPS_OK;

            IText *videoDeviceText = IUFindText( &InputDeviceTP, names[0] );
            IText *videoSourceText = IUFindText( &InputDeviceTP, names[1] );
            IText *frameRateText = IUFindText( &InputDeviceTP, names[2] );
            IText *videoSizeText = IUFindText( &InputDeviceTP, names[3] );

            if (!videoDeviceText || !videoSourceText || !frameRateText || !videoSizeText)
                return false;

            if(ChangeSource(texts[0], texts[1], atoi(texts[2]), texts[3]))
            {
                IUSaveText(videoDeviceText, texts[0]);
                IUSaveText(videoSourceText, texts[1]);
                IUSaveText(frameRateText, texts[2]);
                IUSaveText(videoSizeText, texts[3]);
                IDSetText (&InputDeviceTP, NULL);
                return true;
            }
      }

    return INDI::CCD::ISNewText(dev,name,texts,names,n);
}

//This sets up a single exposure, or if rapid stacking is requested,
//It can also set up a series of exposures till the time runs out.
bool indi_webcam::StartExposure(float duration)
{
    //If the webcam is currently streaming, it cannot capture single exposures
    if (is_streaming || is_capturing)
    {
         DEBUG(INDI::Logger::DBG_SESSION, "Device is currently streaming.");
        return 0;
    }

    //This resets the stack buffer
    if(webcamStacking){
        if(stackBuffer)
            free(stackBuffer);
        stackBuffer = nullptr;
    }

    //This sets up the output format for the exposure
    if(outputFormat == "16 bit RGB")
    {
        out_pix_fmt=AV_PIX_FMT_RGB48LE;
        PrimaryCCD.setBPP(16);
        PrimaryCCD.setNAxis(3);
    }
    else if(outputFormat == "8 bit RGB")
    {
        out_pix_fmt=AV_PIX_FMT_RGB24;
        PrimaryCCD.setBPP(8);
        PrimaryCCD.setNAxis(3);
    }
    else if(outputFormat == "16 bit Grayscale")
    {
        out_pix_fmt=AV_PIX_FMT_GRAY16LE;
        PrimaryCCD.setBPP(16);
        PrimaryCCD.setNAxis(2);
    }
    else
        return -1;

    //This sets up the exposure time settings
    ExposureRequest = duration;
    PrimaryCCD.setExposureDuration(duration);
    gettimeofday(&ExpStart, nullptr);
    timerID = SetTimer(POLLMS);
    InExposure = true;
    return 0;
}

bool indi_webcam::AbortExposure()
{
    if(stackBuffer)
        free(stackBuffer);
    InExposure = false;
    return true;
}

//This calculates the time left in the exposure.
float indi_webcam::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now { 0, 0 };
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = ExposureRequest - timesince;
    return timeleft;
}

//This method is called repeatedly during the exposure
//If webcam stacking is happening, it repeatedly calls for images
//It also updates the reported exposure time left
//Finally when time is up, it copies any stack to the primary buffer
//And calls finish exposure to do any subframing necessary.

void indi_webcam::TimerHit()
{
    float timeleft;

    if (InExposure)
    {
        if (!isConnected())
            return; //  No need to reset timer if we are not connected anymore

        timeleft = CalcTimeLeft();

        if (timeleft < (1/frameRate))
        {
            grabImage(); //Note that this both starts and ends the exposure
            if(webcamStacking)
                copyFinalStackToPrimaryFrameBuffer();
            PrimaryCCD.setExposureLeft(0);
            InExposure = false;
            LOG_INFO("Download complete.");
            finishExposure();
        }
        else
        {
            PrimaryCCD.setExposureLeft(timeleft);
            if(webcamStacking)
                grabImage();  //This will take another frame which will get added to the average.
        }
        if(webcamStacking)
            SetTimer(10);
    }

    SetTimer(POLLMS);
}

// Downloads the image from the Webcam.
//If the image is an RGB, it converts it to Fits RGB
//If rapid stacking is happening, it adds the image to the stack.

int indi_webcam::grabImage()
{
    //Set up the stream, if there is an error, return
    if(setupStreaming()!=0)
        return -1;

    if(getStreamFrame()==0)
    {
        if(PrimaryCCD.getNAxis()==3)
            convertINDI_RGBtoFITS_RGB(pFrameOUT->data[0], PrimaryCCD.getFrameBuffer());
        else
            memcpy(PrimaryCCD.getFrameBuffer(), pFrameOUT->data[0], numBytes);
        if(webcamStacking)
            addToStack();
    }

    freeMemory();
    return 0;
}

//This adds each image to the running stack
bool indi_webcam::addToStack()
{
    if(!stackBuffer)
    {
        stackBuffer = (float *)malloc(numBytes*sizeof(float_t));
        numberOfFramesInStack = 0;
    }

    int w = pCodecCtx->width  * ((PrimaryCCD.getNAxis() == 3) ? 3 : 1);
    int h = pCodecCtx->height;

    for (int i = 0; i < w * h; i++)
       {
           int x = i % w;
           int y = i / w;
           if(x >= 0 && y >= 0 && x < w && y < h)
           {
                   if(numberOfFramesInStack == 0)
                       stackBuffer[i]=getImageDataFloatValue(x, y);
                   else
                       stackBuffer[i]+=getImageDataFloatValue(x, y);
           }
       }
    numberOfFramesInStack++;
    return true;
}

//This gets the pixel value at an x, y position in the image
float indi_webcam::getImageDataFloatValue(int x, int y){
    int w = pCodecCtx->width  * ((PrimaryCCD.getNAxis() == 3) ? 3 : 1);
    uint8_t *primaryBuffer = PrimaryCCD.getFrameBuffer();
    if(PrimaryCCD.getBPP()==8)
        return (float) primaryBuffer[y * w + x];
    else if(PrimaryCCD.getBPP()==16)
    {
        uint16_t *newBuffer = reinterpret_cast<uint16_t *>(primaryBuffer);
        return (float) newBuffer[y * w + x];
    }
    else
        return 0;
}
//This sets the pixel value at an x, y position in the image
void indi_webcam::setImageDataValueFromFloat(int x, int y, float value, bool roundAnswer)
{
    uint8_t *primaryBuffer = PrimaryCCD.getFrameBuffer();
    int w = pCodecCtx->width * ((PrimaryCCD.getNAxis() == 3) ? 3 : 1);
    if(!stackBuffer)
        return;
    if(PrimaryCCD.getBPP()==8)
    {
        int max = std::numeric_limits<uint8_t>::max();
        if (value > max)
            value = max;
        if(roundAnswer)
            primaryBuffer[y * w + x] = round(value);
        else
            primaryBuffer[y * w + x] = value;
    }
    else if(PrimaryCCD.getBPP()==16)
    {
        int max = std::numeric_limits<uint16_t>::max();
        if (value > max)
            value = max;
        uint16_t *newBuffer = reinterpret_cast<uint16_t *>(primaryBuffer);
        if(roundAnswer)
            newBuffer[y * w + x] = round(value);
        else
            newBuffer[y * w + x] = value;
    }
}

//This will take the final image stack and copy it back to the primary buffer for final download.
void indi_webcam::copyFinalStackToPrimaryFrameBuffer()
{
    int w = pCodecCtx->width  * ((PrimaryCCD.getNAxis() == 3) ? 3 : 1);
    int h = pCodecCtx->height;
    for (int i = 0; i < w*h; i++)
        {
            int x = i % w;
            int y = i / w;
            if(x >= 0 && y >= 0 && x < w && y < h)
            {
                if(averaging)
                    setImageDataValueFromFloat(x, y, round(stackBuffer[i]/numberOfFramesInStack), true);
                else //Integrating
                    setImageDataValueFromFloat(x, y, round(stackBuffer[i]), true);
            }
        }

    LOGF_INFO("Final Image is a stack of %u exposures.", numberOfFramesInStack);
}

//This will crop the image to a subframe if desired.
//Then it will send the final image.
void indi_webcam::finishExposure()
{
    uint8_t *memptr = PrimaryCCD.getFrameBuffer();
    int w = pCodecCtx->width;
    int h = pCodecCtx->height;
    int bpp = PrimaryCCD.getBPP();
    int naxis = PrimaryCCD.getNAxis();

    if (PrimaryCCD.getSubW() < w || PrimaryCCD.getSubH() < h)
    {
        int subFrameSize     = PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * bpp / 8 * ((naxis == 3) ? 3 : 1);
        int oneFrameSize     = PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * bpp / 8;
        uint8_t *subframeBuf = (uint8_t *)malloc(subFrameSize);

        int startY = PrimaryCCD.getSubY();
        int endY   = startY + PrimaryCCD.getSubH();
        int lineW  = PrimaryCCD.getSubW() * bpp / 8;
        int subX   = PrimaryCCD.getSubX();

        LOGF_DEBUG("Subframing... subFrameSize: %d - oneFrameSize: %d - startY: %d - endY: %d - lineW: %d - subX: %d", subFrameSize, oneFrameSize,
                   startY, endY, lineW, subX);

        if (naxis == 2)
        {
            for (int i = startY; i < endY; i++)
                memcpy(subframeBuf + (i - startY) * lineW, memptr + (i * w + subX) * PrimaryCCD.getBPP() / 8, lineW);
        }
        else
        {
            uint8_t *subR = subframeBuf;
            uint8_t *subG = subframeBuf + oneFrameSize;
            uint8_t *subB = subframeBuf + oneFrameSize * 2;

            uint8_t *startR = memptr;
            uint8_t *startG = memptr + (w * h * bpp / 8);
            uint8_t *startB = memptr + (w * h * bpp / 8 * 2);

            for (int i = startY; i < endY; i++)
            {
                memcpy(subR + (i - startY) * lineW, startR + (i * w + subX) * bpp / 8, lineW);
                memcpy(subG + (i - startY) * lineW, startG + (i * w + subX) * bpp / 8, lineW);
                memcpy(subB + (i - startY) * lineW, startB + (i * w + subX) * bpp / 8, lineW);
            }
        }

        PrimaryCCD.setFrameBuffer(subframeBuf);
        PrimaryCCD.setFrameBufferSize(subFrameSize, false);
        PrimaryCCD.setResolution(w, h);
        PrimaryCCD.setNAxis(naxis);
        PrimaryCCD.setBPP(bpp);

        ExposureComplete(&PrimaryCCD);

        // Restore old pointer and release memory
        PrimaryCCD.setFrameBuffer(memptr);
        PrimaryCCD.setFrameBufferSize(numBytes, false);
        free(subframeBuf);
    }
    else
        ExposureComplete(&PrimaryCCD);
}

bool indi_webcam::UpdateCCDFrame(int x, int y, int w, int h)
{

    PrimaryCCD.setFrame(x, y, w, h);
    return true;
}

//These next several methods handle streaming starting and stopping.

void indi_webcam::start_capturing()
{
    if (is_capturing) return;
    is_capturing=true;
    capture_thread=std::thread(RunCaptureThread, this);
}

void indi_webcam::stop_capturing()
{
    if (!is_capturing) return;
    is_capturing=false;
    if (std::this_thread::get_id() != capture_thread.get_id())
      capture_thread.join();
}

bool indi_webcam::StartStreaming()
{
    if (is_streaming) return true;
    if (!is_capturing) start_capturing();
    is_streaming=true;
    return true;
}

bool indi_webcam::StopStreaming()
{
    if (!is_streaming) return true;
    stop_capturing();
    is_streaming=false;
    return true;
}

void indi_webcam::RunCaptureThread(indi_webcam *webcam)
{
  webcam->run_capture();
}

//This is the loop that runs during streaming
//Note that it ONLY supports RGB24 aka INDI_RGB format.
void indi_webcam::run_capture()
{
    out_pix_fmt=AV_PIX_FMT_RGB24;

  if(setupStreaming()!=0)
      return;

  Streamer->setPixelFormat(INDI_RGB);
  int w = pCodecCtx->width;
  int h = pCodecCtx->height;
  Streamer->setSize(w, h);
  PrimaryCCD.setFrame(0, 0, w, h);
  PrimaryCCD.setBPP(8);
  PrimaryCCD.setNAxis(3);

  uint8_t *buffer = (uint8_t *)malloc(numBytes);

  while (is_capturing && is_streaming) {

    if(getStreamFrame()==0)
        Streamer->newFrame(pFrameOUT->data[0], numBytes);
    else
    {
        is_capturing = false;
        is_streaming = false;
    }
  }

  freeMemory();

  DEBUG(INDI::Logger::DBG_SESSION,"Capture thread releasing device.");
}

//This converts an image from INDI_RGB to FITS_RGB so the FITSViewer can read it.
bool indi_webcam::convertINDI_RGBtoFITS_RGB(uint8_t *originalImage, uint8_t *convertedImage)
{
    if(PrimaryCCD.getBPP() == 8)
    {
        int size =  numBytes / 3;
        uint8_t *r,*g,*b;
        r = (convertedImage);
        g = (convertedImage + size);
        b = (convertedImage + size * 2);
        for(int i=0; i < numBytes; i+=3)
        {
            *r++ = *originalImage++;
            *g++ = *originalImage++;
            *b++ = *originalImage++;
        }
    }
    else if(PrimaryCCD.getBPP() == 16)
    {
        uint16_t *bigOriginalImage = reinterpret_cast<uint16_t *>(originalImage);
        uint16_t *bigConvertedImage = reinterpret_cast<uint16_t *>(convertedImage);
        int size =  numBytes / 2 / 3;
        uint16_t *r,*g,*b;
        r = (bigConvertedImage);
        g = (bigConvertedImage + size);
        b = (bigConvertedImage + size * 2);
        for(int i=0; i < numBytes / 2; i+=3)
        {
            *r++ = *bigOriginalImage++;
            *g++ = *bigOriginalImage++;
            *b++ = *bigOriginalImage++;
        }
    }
    return true;
}

//This sets up the webcam to get images
//It is used for both the streaming and exposing algorithms
int indi_webcam::setupStreaming()
{
    // Determine required buffer size and allocate buffer for pframeRGB
    numBytes = av_image_get_buffer_size(out_pix_fmt, pCodecCtx->width, pCodecCtx->height, 1);

    // Allocate video frame
    pFrame=av_frame_alloc();
    // Allocate an AVFrame structure
    pFrameOUT=av_frame_alloc();
    if(pFrameOUT==NULL)
      return -1;

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

    av_image_fill_arrays (pFrameOUT->data, pFrameOUT->linesize, buffer, out_pix_fmt,
              pCodecCtx->width, pCodecCtx->height, 1);

    // initialize SWS context for software scaling
    sws_ctx = sws_getContext( pCodecCtx->width, pCodecCtx->height,
                 pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
                 out_pix_fmt, SWS_BILINEAR, NULL, NULL, NULL
                 );

    PrimaryCCD.setFrameBufferSize(numBytes);
    PrimaryCCD.setResolution(pCodecCtx->width, pCodecCtx->height);

    return 0;
}

//This gets one image from the camera.
//It is used for both the streaming and exposing algorithms
int indi_webcam::getStreamFrame()
{
    AVPacket packet;
    if (av_read_frame(pFormatCtx, &packet)<0) { // stream stopped
        DEBUG(INDI::Logger::DBG_SESSION, "Device has been disconnected.");
        av_packet_unref(&packet);
        return -1;
    }
    if(packet.stream_index==videoStream) {
        int ret;
        ret = avcodec_send_packet(pCodecCtx, &packet);
        if (ret < 0) {
            DEBUG(INDI::Logger::DBG_SESSION, "Error sending a packet for decoding");
            av_packet_unref(&packet);
            return -1;
        }
        while (ret >= 0) {
            ret = avcodec_receive_frame(pCodecCtx, pFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                continue;
            else if (ret < 0) {
                DEBUG(INDI::Logger::DBG_SESSION, "Error during decoding");
                av_packet_unref(&packet);
                return -1;
            }
            // We have a frame at that point
            // Convert the image from its native format to our output format
           sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                pFrame->linesize, 0, pCodecCtx->height,
                pFrameOUT->data, pFrameOUT->linesize);
           av_packet_unref(&packet);
           return 0;
        }

    }
    return -1;
}

//This frees up the resources used for streaming/exposing
void indi_webcam::freeMemory()
{
    // Free the sws_context
    sws_freeContext(sws_ctx);

    // Free the RGB image
    av_free(buffer);
    av_free(pFrameOUT);

    // Free the input frame
    av_free(pFrame);
}

bool indi_webcam::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);
    //Need to add some things here
    //Do NOT add the connection option switches.  If you do, it will repeatedly connect and disconnect as it loads the saved options.

    return true;
}
