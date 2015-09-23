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

#include <indilogger.h>

#include <signal.h>
#include <zlib.h>
#include <sys/stat.h>

#include "stream_recorder.h"

const char *STREAM_TAB          = "Streaming";

StreamRecorder::StreamRecorder(INDI::CCD *mainCCD)
{
   ccd = mainCCD;

   is_streaming = false;
   is_recording = false;

   compressedFrame = (uint8_t* ) malloc(1);

   // Timer
   // now use BSD setimer to avoi librt dependency
   //sevp.sigev_notify=SIGEV_NONE;
   //timer_create(CLOCK_MONOTONIC, &sevp, &fpstimer);
   //fpssettings.it_interval.tv_sec=24*3600;
   //fpssettings.it_interval.tv_nsec=0;
   //fpssettings.it_value=fpssettings.it_interval;
   //timer_settime(fpstimer, 0, &fpssettings, NULL);

   struct itimerval fpssettings;
   fpssettings.it_interval.tv_sec=24*3600;
   fpssettings.it_interval.tv_usec=0;
   fpssettings.it_value=fpssettings.it_interval;
   signal(SIGALRM, SIG_IGN); //portable
   setitimer(ITIMER_REAL, &fpssettings, NULL);

   v4l2_record=new V4L2_Record();
   recorder=v4l2_record->getDefaultRecorder();
   recorder->init();
   direct_record=false;

   DEBUGF( INDI::Logger::DBG_SESSION, "Using default recorder (%s)", recorder->getName());

}

StreamRecorder::~StreamRecorder()
{
    delete (v4l2_record);
    free(compressedFrame);
}

bool StreamRecorder::initProperties()
{
    /* Video Stream */
     IUFillSwitch(&StreamS[0], "STREAM_ON", "Stream On", ISS_OFF);
     IUFillSwitch(&StreamS[1], "STREAM_OFF", "Stream Off", ISS_ON);
     IUFillSwitchVector(&StreamSP, StreamS, NARRAY(StreamS), getDeviceName(), "CCD_VIDEO_STREAM", "Video Stream", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

     /* Stream Rate divisor */
     IUFillNumber(&StreamOptionsN[0], "STREAM_RATE", "Rate Divisor", "%3.0f", 1.0, 999.0, 1, 10);
     IUFillNumberVector(&StreamOptionsNP, StreamOptionsN, NARRAY(StreamOptionsN), getDeviceName(), "STREAM_OPTIONS", "Streaming", STREAM_TAB, IP_RW, 60, IPS_IDLE);

     /* Measured FPS */
     IUFillNumber(&FpsN[0], "EST_FPS", "Instant.", "%3.2f", 0.0, 999.0, 0.0, 30);
     IUFillNumber(&FpsN[1], "AVG_FPS", "Average (1 sec.)", "%3.2f", 0.0, 999.0, 0.0, 30);
     IUFillNumberVector(&FpsNP, FpsN, NARRAY(FpsN), getDeviceName(), "FPS", "FPS", STREAM_TAB, IP_RO, 60, IPS_IDLE);

     /* Frames to Drop */
     //IUFillNumber(&FramestoDropN[0], "To drop", "", "%2.0f", 0, 99, 1, 0);
     //IUFillNumberVector(&FramestoDropNP, FramestoDropN, NARRAY(FramestoDropN), getDeviceName(), "Frames", "", STREAM_TAB, IP_RW, 60, IPS_IDLE);

     /* Record Frames */
     /* File */
     IUFillText(&RecordFileT[0], "RECORD_FILE_DIR", "Dir.", "/tmp/indi__D_");
     IUFillText(&RecordFileT[1], "RECORD_FILE_NAME", "Name", "indi_record__T_.ser");
     IUFillTextVector(&RecordFileTP, RecordFileT, NARRAY(RecordFileT), getDeviceName(), "RECORD_FILE", "Record File", STREAM_TAB, IP_RW, 0, IPS_IDLE);

     /* Record Options */
     IUFillNumber(&RecordOptionsN[0], "RECORD_DURATION", "Duration (sec)", "%6.3f", 0.001, 999999.0, 0.0, 1);
     IUFillNumber(&RecordOptionsN[1], "RECORD_FRAME_TOTAL", "Frames", "%9.0f", 1.0, 999999999.0, 1.0, 30.0);
     IUFillNumberVector(&RecordOptionsNP, RecordOptionsN, NARRAY(RecordOptionsN), getDeviceName(), "RECORD_OPTIONS", "Record Options", STREAM_TAB, IP_RW, 60, IPS_IDLE);

     /* Record Switch */
     IUFillSwitch(&RecordStreamS[0], "RECORD_ON", "Record On", ISS_OFF);
     IUFillSwitch(&RecordStreamS[1], "RECORD_DURATION_ON", "Record (Duration)", ISS_OFF);
     IUFillSwitch(&RecordStreamS[2], "RECORD_FRAME_ON", "Record (Frames)", ISS_OFF);
     IUFillSwitch(&RecordStreamS[3], "RECORD_OFF", "Record Off", ISS_ON);
     IUFillSwitchVector(&RecordStreamSP, RecordStreamS, NARRAY(RecordStreamS), getDeviceName(), "RECORD_STREAM", "Video Record", STREAM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

}

void StreamRecorder::ISGetProperties(const char *dev)
{
    if (dev && strcmp (getDeviceName(), dev))
      return;

    if (ccd->isConnected())
    {
      ccd->defineSwitch(&StreamSP);
      ccd->defineNumber(&StreamOptionsNP);
      ccd->defineNumber(&FpsNP);
      //ccd->defineNumber(&FramestoDropNP);
      ccd->defineSwitch(&RecordStreamSP);
      ccd->defineText(&RecordFileTP);
      ccd->defineNumber(&RecordOptionsNP);
    }
}

bool StreamRecorder::updateProperties()
{
    if (ccd->isConnected())
    {
      imageBP=ccd->getBLOB("CCD1");
      imageB=imageBP->bp;

      ccd->defineSwitch(&StreamSP);
      ccd->defineNumber(&StreamOptionsNP);
      ccd->defineNumber(&FpsNP);
      //ccd->defineNumber(&FramestoDropNP);
      ccd->defineSwitch(&RecordStreamSP);
      ccd->defineText(&RecordFileTP);
      ccd->defineNumber(&RecordOptionsNP);

    }
    else
    {
      ccd->deleteProperty(StreamSP.name);
      ccd->deleteProperty(StreamOptionsNP.name);
      ccd->deleteProperty(FpsNP.name);
      //ccd->deleteProperty(FramestoDropNP.name);
      ccd->deleteProperty(RecordFileTP.name);
      ccd->deleteProperty(RecordStreamSP.name);
      ccd->deleteProperty(RecordOptionsNP.name);

      return true;
    }
}

void StreamRecorder::newFrame(unsigned char *buffer, ImageColorSpace imageType)
{
    double ms1, ms2, deltams;

    // Measure FPS
    getitimer(ITIMER_REAL, &tframe2);
    //ms2=capture->get(CV_CAP_PROP_POS_MSEC);

    ms1=(1000.0 * (double)tframe1.it_value.tv_sec) + ((double)tframe1.it_value.tv_usec / 1000.0);
    ms2=(1000.0 * (double)tframe2.it_value.tv_sec) + ((double)tframe2.it_value.tv_usec / 1000.0);
    if (ms2 > ms1)
        deltams = ms2-ms1;//ms1 +( (24*3600*1000.0) - ms2);
    else
        deltams=ms1-ms2;

    //EstFps->value=1000.0 / deltams;
    tframe1=tframe2;
    mssum+=deltams;
    framecountsec+=1;

    FpsN[0].value= 1000.0 / deltams;

    if (mssum >= 1000.0)
    {
      FpsN[1].value=(framecountsec * 1000.0) / mssum;
      mssum=0; framecountsec=0;
    }

    IDSetNumber(&FpsNP, NULL);

    if (StreamSP.s == IPS_BUSY)
    {
      streamframeCount++;
      if (streamframeCount >= StreamOptionsN[0].value)
      {
        uploadStream(buffer);
        streamframeCount = 0;
      }
    }

    if (RecordStreamSP.s == IPS_BUSY)
    {
      recordStream(deltams, buffer, imageType);
    }
}

void StreamRecorder::setRecorderSize(uint16_t width, uint16_t height)
{
    recorder->setsize(width, height);
}

bool StreamRecorder::close()
{
    return recorder->close();
}

bool StreamRecorder::setPixelFormat(uint32_t format)
{
    direct_record = recorder->setpixelformat(format);
}

bool StreamRecorder::uploadStream(uint8_t *buffer)
{
    int ret=0;

    uLongf compressedBytes = 0;
    uLong totalBytes = ccd->PrimaryCCD.getFrameBufferSize();

    /* Do we want to compress ? */
     if (ccd->PrimaryCCD.isCompressed())
     {
        /* Compress frame */
        compressedFrame = (uint8_t *) realloc (compressedFrame, sizeof(uint8_t) * totalBytes + totalBytes / 64 + 16 + 3);
        compressedBytes = sizeof(uint8_t) * totalBytes + totalBytes / 64 + 16 + 3;

        ret = compress2(compressedFrame, &compressedBytes, buffer, totalBytes, 4);
        if (ret != Z_OK)
        {
             /* this should NEVER happen */
             DEBUGF( INDI::Logger::DBG_ERROR, "internal error - compression failed: %d", ret);
             return false;
         }

        /* #3.A Send it compressed */
        imageB->blob = compressedFrame;
        imageB->bloblen = compressedBytes;
        imageB->size = totalBytes;
        strcpy(imageB->format, ".stream.z");
      }
      else
      {
        /* #3.B Send it uncompressed */
         imageB->blob = buffer;
         imageB->bloblen = totalBytes;
         imageB->size = totalBytes;
         strcpy(imageB->format, ".stream");
      }

    imageBP->s = IPS_OK;
    IDSetBLOB (imageBP, NULL);
    return true;
}

void StreamRecorder::recordStream(double deltams, unsigned char *buffer, ImageColorSpace imageType)
{
  if (!is_recording)
      return;

  if (imageType == IMAGE_MONO_8)
    recorder->writeFrameMono(buffer);
  else
    recorder->writeFrameColor(buffer);

  recordDuration+=deltams;
  recordframeCount+=1;

  if ((RecordStreamSP.sp[1].s == ISS_ON) && (recordDuration >= (RecordOptionsNP.np[0].value * 1000.0)))
  {
        DEBUGF(INDI::Logger::DBG_SESSION,"Ending record after %g millisecs", recordDuration);
        stopRecording();
        RecordStreamSP.sp[1].s = ISS_OFF; RecordStreamSP.sp[3].s = ISS_ON; RecordStreamSP.s = IPS_IDLE;
        IDSetSwitch(&RecordStreamSP, NULL);
  }

  if ((RecordStreamSP.sp[2].s == ISS_ON) && (recordframeCount >= (RecordOptionsNP.np[1].value)))
  {
        DEBUGF(INDI::Logger::DBG_SESSION,"Ending record after %d frames", recordframeCount);
        stopRecording();
        RecordStreamSP.sp[2].s = ISS_OFF; RecordStreamSP.sp[3].s = ISS_ON; RecordStreamSP.s = IPS_IDLE;
        IDSetSwitch(&RecordStreamSP, NULL);
  }
}

int StreamRecorder::mkpath(std::string s, mode_t mode)
{
  size_t pre=0,pos;
  std::string dir;
  int mdret=0;
  struct stat st;

  if(s[s.size()-1]!='/') s+='/';

  while((pos=s.find_first_of('/',pre)) != std::string::npos){
    dir=s.substr(0,pos++);
    pre=pos;
    if(dir.size()==0) continue;
    if (stat(dir.c_str(), &st)) {
      if (errno != ENOENT || ((mdret=mkdir(dir.c_str(),mode)) && errno!=EEXIST)) {
    DEBUGF(INDI::Logger::DBG_WARNING,"mkpath: can not create %s", dir.c_str());
    return mdret;
      }
    } else {
      if (!S_ISDIR(st.st_mode)) {
    DEBUGF(INDI::Logger::DBG_WARNING,"mkpath: %s is not a directory", dir.c_str());
    return -1;
      }
    }
  }
  return mdret;
}

std::string StreamRecorder::expand(std::string fname, const std::map<std::string, std::string>& patterns)
{
  std::string res = fname;
  std::size_t pos;
  time_t now;
  struct tm *tm_now;
  char val[20];
  *(val+19) = '\0';

  time(&now);
  tm_now = gmtime(&now);

  pos=res.find("_D_");
  if (pos != std::string::npos) {
    strftime(val, 11, "%F", tm_now);
    res.replace(pos, 3, val);
  }

  pos=res.find("_T_");
  if (pos != std::string::npos) {
    strftime(val, 20, "%F@%T", tm_now);
    res.replace(pos, 3, val);
  }

  pos=res.find("_H_");
  if (pos != std::string::npos) {
    strftime(val, 9, "%T", tm_now);
    res.replace(pos, 3, val);
  }

  for (std::map<std::string, std::string>::const_iterator it=patterns.begin(); it!=patterns.end(); ++it) {
    pos=res.find(it->first);
    if (pos != std::string::npos) {
      res.replace(pos, it->first.size(), it->second);
    }
  }

  return res;
}

bool StreamRecorder::startRecording()
{
  char errmsg[MAXRBUF];
  std::string filename, expfilename, expfiledir;
  std::string filtername;
  std::map<std::string, std::string> patterns;
  if (is_recording)
      return true;

  /* get filter name for pattern substitution */
  if (ccd->CurrentFilterSlot != -1 && ccd->CurrentFilterSlot <= ccd->FilterNames.size())
  {
    filtername=ccd->FilterNames.at(ccd->CurrentFilterSlot-1);
    patterns["_F_"]=filtername;
    DEBUGF(INDI::Logger::DBG_SESSION, "Adding filter pattern %s", filtername.c_str());
  }
  /* pattern substitution */
  recordfiledir.assign(RecordFileTP.tp[0].text);
  expfiledir=expand(recordfiledir, patterns);
  if (expfiledir.at(expfiledir.size() - 1) != '/')
    expfiledir+='/';
  recordfilename.assign(RecordFileTP.tp[1].text);
  expfilename=expand(recordfilename, patterns);
  if (expfilename.substr(expfilename.size() - 4, 4) != ".ser")
    expfilename+=".ser";
  filename=expfiledir+expfilename;
  //DEBUGF(INDI::Logger::DBG_SESSION, "Expanded file is %s", filename.c_str());
  //filename=recordfiledir+recordfilename;
  DEBUGF(INDI::Logger::DBG_SESSION, "Record file is %s", filename.c_str());
  /* Create/open file/dir */
  if (mkpath(expfiledir, 0755))
  {
    DEBUGF(INDI::Logger::DBG_WARNING, "Can not create record directory %s: %s", expfiledir.c_str(), strerror(errno));
    return false;
  }
  if (!recorder->open(filename.c_str(), errmsg))
  {
    RecordStreamSP.s = IPS_ALERT;
    IDSetSwitch(&RecordStreamSP, NULL);
    DEBUGF(INDI::Logger::DBG_WARNING, "Can not open record file: %s", errmsg);
    return false;
  }
  /* start capture */
  if (direct_record)
  {
    DEBUG(INDI::Logger::DBG_SESSION, "Using direct recording (no software cropping).");
    //v4l_base->doDecode(false);
    //v4l_base->doRecord(true);
  }
  else
  {
    //if (ImageColorS[0].s == ISS_ON)
      if (ccd->PrimaryCCD.getNAxis() == 2)
      recorder->setDefaultMono();
    else
      recorder->setDefaultColor();
  }
  recordDuration=0.0;
  recordframeCount=0;

  getitimer(ITIMER_REAL, &tframe1);
  mssum=0; framecountsec=0;
  if (is_streaming == false && ccd->StartStreaming() == false)
  {
      DEBUG(INDI::Logger::DBG_ERROR, "Failed to start recording.");
      RecordStreamSP.s = IPS_ALERT;
      IUResetSwitch(&RecordStreamSP);
      RecordStreamS[RECORD_OFF].s = ISS_ON;
      IDSetSwitch(&RecordStreamSP, NULL);
  }
  is_recording=true;
  return true;
}

bool StreamRecorder::stopRecording()
{
  if (!is_recording) return true;
  if (!is_streaming)
      ccd->StopStreaming();

  is_recording=false;
  recorder->close();
  DEBUGF(INDI::Logger::DBG_SESSION, "Record Duration(millisec): %g -- Frame count: %d", recordDuration, recordframeCount);
  return true;
}

bool StreamRecorder::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && strcmp (getDeviceName(), dev))
      return true;

    /* Video Stream */
    if (!strcmp(name, StreamSP.name))
    {      
      for (int i=0; i < n; i++)
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

    /* Record Stream */
    if (!strcmp(name, RecordStreamSP.name))
    {
      int prevSwitch = IUFindOnSwitchIndex(&RecordStreamSP);
      IUUpdateSwitch(&RecordStreamSP, states, names, n);

      if (is_recording && RecordStreamSP.sp[3].s != ISS_ON)
      {
        IUResetSwitch(&RecordStreamSP);
        RecordStreamS[prevSwitch].s = ISS_ON;
        IDSetSwitch(&RecordStreamSP, NULL);
        DEBUG(INDI::Logger::DBG_WARNING, "Recording device is busy.");
        return false;
      }

      if ((RecordStreamSP.sp[0].s == ISS_ON) || (RecordStreamSP.sp[1].s == ISS_ON) || (RecordStreamSP.sp[2].s == ISS_ON))
      {
        if (!is_recording)
        {
            RecordStreamSP.s  = IPS_BUSY;
            if (RecordStreamSP.sp[1].s == ISS_ON)
                DEBUGF(INDI::Logger::DBG_SESSION, "Starting video record (Duration): %g secs.", RecordOptionsNP.np[0].value);
            else if (RecordStreamSP.sp[2].s == ISS_ON)
                DEBUGF(INDI::Logger::DBG_SESSION, "Starting video record (Frame count): %d.", (int)(RecordOptionsNP.np[1].value));
            else
                DEBUG(INDI::Logger::DBG_SESSION, "Starting video record.");

            if (!startRecording())
            {
                RecordStreamSP.sp[0].s = ISS_OFF; RecordStreamSP.sp[1].s = ISS_OFF;
                RecordStreamSP.sp[2].s = ISS_OFF; RecordStreamSP.sp[3].s = ISS_ON;
                RecordStreamSP.s  = IPS_ALERT;
            }
        }
      }
      else
      {
            RecordStreamSP.s = IPS_IDLE;
            if (is_recording)
            {
                DEBUGF(INDI::Logger::DBG_SESSION, "Recording stream has been disabled. Frame count %d", recordframeCount);
                stopRecording();
            }
      }

      IDSetSwitch(&RecordStreamSP, NULL);
      return true;
    }


}

bool StreamRecorder::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    /* ignore if not ours */
    if (dev && strcmp (getDeviceName(), dev))
      return true;

    if (!strcmp(name, RecordFileTP.name) )
    {
        IText *tp = IUFindText(&RecordFileTP, "RECORD_FILE_NAME");
        if (strchr(tp->text, '/'))
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Dir. separator (/) not allowed in filename.");
            return false;
        }

        IUUpdateText(&RecordFileTP, texts, names, n);
        IDSetText (&RecordFileTP, NULL);
        return true;
    }
}

bool StreamRecorder::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    /* ignore if not ours */
    if (dev && strcmp (getDeviceName(), dev))
      return true;

    /* Stream rate */
    if (!strcmp (StreamOptionsNP.name, name))
    {
        IUUpdateNumber(&StreamOptionsNP, values, names, n);
        StreamOptionsNP.s = IPS_OK;
        IDSetNumber(&StreamOptionsNP, NULL);
        return true;
    }

    /* Record Options */
    if (!strcmp (RecordOptionsNP.name, name))
    {
        if (is_recording)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Recording device is busy");
            return false;
        }

        IUUpdateNumber(&RecordOptionsNP, values, names, n);
        RecordOptionsNP.s = IPS_OK;
        IDSetNumber(&RecordOptionsNP, NULL);
        return true;
    }

    /* Frames to drop */
   /*if (!strcmp (FramestoDropNP.name, name))
     {
       IUUpdateNumber(&FramestoDropNP, values, names, n);
       //v4l_base->setDropFrameCount(values[0]);
       FramestoDropNP.s = IPS_OK;
       IDSetNumber(&FramestoDropNP, NULL);
       return true;
     }*/
}

bool StreamRecorder::setStream(bool enable)
{
    if (enable)
    {
        if (!is_streaming)
        {
            StreamSP.s  = IPS_BUSY;
            streamframeCount = 0;
            DEBUG(INDI::Logger::DBG_DEBUG, "Starting the video stream.");
            streamframeCount = 0;

            getitimer(ITIMER_REAL, &tframe1);
            mssum=0; framecountsec=0;
            if (ccd->StartStreaming() == false)
            {
                IUResetSwitch(&StreamSP);
                StreamS[1].s = ISS_ON;
                StreamSP.s = IPS_ALERT;
                DEBUG(INDI::Logger::DBG_ERROR, "Failed to start streaming.");
                IDSetSwitch(&StreamSP, NULL);
                return false;
            }

            is_streaming=true;
            IUResetSwitch(&StreamSP);
            StreamS[0].s = ISS_ON;
        }
    }
    else
    {

        StreamSP.s = IPS_IDLE;
        if (is_streaming)
        {
            DEBUGF(INDI::Logger::DBG_DEBUG, "The video stream has been disabled. Frame count %d", streamframeCount);
            //if (!is_exposing && !is_recording) stop_capturing();
            if (!is_recording)
            {
                if (ccd->StopStreaming() == false)
                {
                    StreamSP.s = IPS_ALERT;
                    DEBUG(INDI::Logger::DBG_ERROR, "Failed to stop streaming.");
                    IDSetSwitch(&StreamSP, NULL);
                    return false;
                }
            }

            IUResetSwitch(&StreamSP);
            StreamS[1].s = ISS_ON;
            is_streaming=false;
        }
    }

    IDSetSwitch(&StreamSP, NULL);
    return true;
}



