/*
 Asicam CCD INDI Driver
 
 Copyright (C) 2014 Chrstian Pellegrin <chripell@gmail.com>
 
 Based on CCD Template for INDI Developers
 Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)
 Multiple device support Copyright (C) 2013 Peter Polakovic (peter.polakovic@cloudmakers.eu)

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

#include <memory>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "indidevapi.h"
#include "eventloop.h"

#include "indi_asicam.h"
#include "ASICamera.h"

#define POLLMS		10		/* Polling time (ms) */

#define MAX_DEVICES 20  /* Max device cameraCount */

static int cameraCount;
static int do_debug;
static AsicamCCD *cameras[MAX_DEVICES];

/**********************************************************
 *
 *  IMPORRANT: List supported camera models in initializer of deviceTypes structure
 *
 **********************************************************/

static void cleanup() {
  for (int i = 0; i < cameraCount; i++) {
    if (cameras[i])
      delete cameras[i];
  }
}

void ISInit() {
  static bool isInit = false;
  if (getenv("INDI_ASICAM_VERBOSE"))
      do_debug = 1;
  if (!isInit) {
    /**********************************************************
     *
     *  IMPORRANT: If available use CCD API function for enumeration available CCD's otherwise use code like this:
     *
     **********************************************************

     cameraCount = 0;
     for (struct usb_bus *bus = usb_get_busses(); bus && cameraCount < MAX_DEVICES; bus = bus->next) {
       for (struct usb_device *dev = bus->devices; dev && cameraCount < MAX_DEVICES; dev = dev->next) {
         int vid = dev->descriptor.idVendor;
         int pid = dev->descriptor.idProduct;
         for (int i = 0; deviceTypes[i].pid; i++) {
           if (vid == deviceTypes[i].vid && pid == deviceTypes[i].pid) {
             cameras[i] = new AsicamCCD(dev, deviceTypes[i].name);
             break;
           }
         }
       }
     }

     */    
    int asin = getNumberOfConnectedCameras();
    pid_t pid = getpid();
    char fname[255];
    snprintf(fname, 255, "/proc/%d/cmdline", pid);
    int fd = open(fname, O_RDONLY);
    if (fd == -1) {
      IDLog("Cannot open cmdline <%d>\n", fname);
    }
    else {
      char cmdline[100] = {0};
      char *p;
      const char *name = "indi_asicam";
      read(fd, cmdline, 100);
      p = strstr(cmdline, name);
      if (!p) {
	IDLog("Cannot find my number: <%s>.\n", cmdline);
      }
      else {
	unsigned int myn = atoi(&p[strlen(name)]);
	IDLog("Controlling asicamera %d out of %d.\n", myn, asin);
	if (myn < asin) {
	  if (!openCamera(myn)) {
	    IDLog("Open asicamera %d failed.\n", myn);
	  }
	  else {
	    if (!initCamera()) {
	      IDLog("Init asicamera %d failed.\n", myn);
	    }
	    else {
	      bool is_auto;
	      int val;
	      val = getValue(CONTROL_GAIN, &is_auto);
	      setValue(CONTROL_GAIN, val, false); 
	      val = getValue(CONTROL_EXPOSURE, &is_auto);
	      setValue(CONTROL_EXPOSURE, val, false);
	      setImageFormat(getMaxWidth(), getMaxHeight(),  1, isColorCam() ? IMG_Y8: IMG_RAW8);
	      SetMisc(false, false);
	      setStartPos(0, 0);
	      cameraCount = 1;
	      char n[200];
	      snprintf(n, 200, "asicamera%d: %s", myn, getCameraModel(myn));
	      IDLog("<%s> up and running!\n", n);
	      cameras[0] = new AsicamCCD(0, n);
	      atexit(cleanup);
	    }
	  }
	}
      }
    }
    isInit = true;
  }
}

void ISGetProperties(const char *dev) {
  ISInit();
  for (int i = 0; i < cameraCount; i++) {
    AsicamCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISGetProperties(dev);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
  ISInit();
  for (int i = 0; i < cameraCount; i++) {
    AsicamCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewSwitch(dev, name, states, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num) {
  ISInit();
  for (int i = 0; i < cameraCount; i++) {
    AsicamCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewText(dev, name, texts, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
  ISInit();
  for (int i = 0; i < cameraCount; i++) {
    AsicamCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewNumber(dev, name, values, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}
void ISSnoopDevice(XMLEle *root) {
  INDI_UNUSED(root);
}

AsicamCCD::AsicamCCD(DEVICE device, const char *name) {
  this->device = device;
  snprintf(this->name, 32, "SX CCD %s", name);
  setDeviceName(this->name);

  sim = false;
  need_flush = false;
}

AsicamCCD::~AsicamCCD() {

}

const char * AsicamCCD::getDefaultName() {
  return name;
}

bool AsicamCCD::initProperties() {
  // Init parent properties first
  INDI::CCD::initProperties();
  bool is_auto;

  // reset settings
  IUFillSwitch(&ResetS[0], "RESET", "Reset", ISS_OFF);
  IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "FRAME_RESET", "Frame Values", IMAGE_SETTINGS_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

  // gain
  if (isAvailable(CONTROL_GAIN)) {
    IUFillNumber(&GainN[0], "GAIN", "Gain", "%0.f", getMin(CONTROL_GAIN), getMax(CONTROL_GAIN), 1., getValue(CONTROL_GAIN, &is_auto));
    IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "CCD_GAIN", "Gain", IMAGE_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);
  }

  // image mode
  IUFillSwitch(&ModeS[0], "Y8", "Y8", ISS_ON);
  IUFillSwitch(&ModeS[1], "RAW16", "RAW16", ISS_OFF);
  IUFillSwitch(&ModeS[2], "RGB24", "RGB24", ISS_OFF);
  IUFillSwitch(&ModeS[3], "RAW8", "RAW8", ISS_OFF);
  IUFillSwitchVector(&ModeSP, ModeS, 4, getDeviceName(), "IMAGE_MODE", "Image Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

  Capability cap;
  cap.canAbort = true;
  cap.canBin = true;
  cap.canSubFrame = true;
  cap.hasCooler = false;
  cap.hasGuideHead = false;
  cap.hasShutter = false;
  cap.hasST4Port = true;
  SetCapability(&cap);

  return true;
}

void AsicamCCD::ISGetProperties(const char *dev) {
  INDI::CCD::ISGetProperties(dev);

  // Add Debug, Simulator, and Configuration controls
  addAuxControls();
}

bool AsicamCCD::updateProperties() {
  INDI::CCD::updateProperties();

  if (isConnected()) {
    bool is_auto;
    defineSwitch(&ResetSP);
    defineSwitch(&ModeSP);
    GainN[0].value = getValue(CONTROL_GAIN, &is_auto);
    defineNumber(&GainNP);

    // Let's get parameters now from CCD
    setupParams();

    timerID = SetTimer(POLLMS);
  } else {
    deleteProperty(ResetSP.name);
    deleteProperty(GainNP.name);
    deleteProperty(ModeSP.name);
    rmTimer(timerID);
  }

  return true;
}

bool AsicamCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) {

  if (strcmp(dev, getDeviceName()) == 0) {

    /* Reset */
    if (!strcmp(name, ResetSP.name)) {
      if (IUUpdateSwitch(&ResetSP, states, names, n) < 0)
        return false;
      resetFrame();
      return true;
    }

    // change mode
    if (!strcmp(name, ModeSP.name)) {
      if (IUUpdateSwitch(&ModeSP, states, names, n) < 0)
        return false;
      ISwitch *p = IUFindOnSwitch(&ModeSP);
      enum IMG_TYPE fmt = IMG_RAW8;
      if (!strcmp(p->name, "RGB24"))
	fmt = IMG_RGB24;
      else if (!strcmp(p->name, "RAW16"))
	fmt = IMG_RAW16;
      else if (!strcmp(p->name, "Y8"))
	fmt = IMG_Y8;
      if (do_debug) {
	IDLog("Setting format to %dx%d bin %d fmt %d\n",
	      getWidth(), getHeight(),  getBin(), fmt);
      }
      setImageFormat(getWidth(), getHeight(),  getBin(), fmt);
      setupParams();
      IDSetSwitch(&ModeSP, NULL);      
      return true;
    }
  }

  //  Nobody has claimed this, so, ignore it
  return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool AsicamCCD::Connect() {

  sim = isSimulation();

  if (sim)
    return true;
  else {
    startCapture();
  }

  IDMessage(getDeviceName(), "Attempting to find the Asicam CCD...");

  if (do_debug) {
    IDLog("Connecting CCD\n");
    IDLog("Attempting to find the camera\n");
  }

  /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD Connect function
   *  If you encameraCounter an error, send the client a message
   *  e.g.
   *  IDMessage(getDeviceName(), "Error, unable to connect due to ...");
   *  return false;
   *
   *
   **********************************************************/

  /* Success! */
  IDMessage(getDeviceName(), "CCD is online. Retrieving basic data.");
  if (do_debug)
    IDLog("CCD is online. Retrieving basic data.\n");

  return true;
}

bool AsicamCCD::Disconnect() {
  if (sim)
    return true;

  /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD disonnect function
   *  If you encameraCounter an error, send the client a message
   *  e.g.
   *  IDMessage(getDeviceName(), "Error, unable to disconnect due to ...");
   *  return false;
   *
   *
   **********************************************************/

  stopCapture();
  IDMessage(getDeviceName(), "CCD is offline.");
  return true;
}

bool AsicamCCD::setupParams() {

  if (do_debug)
    IDLog("In setupParams\n");

  float x_pixel_size, y_pixel_size;
  int bit_depth, mode;
  int x_1, y_1, x_2, y_2;

  /**********************************************************
   *
   *
   *
   *  IMPORRANT: Get basic CCD parameters here such as
   *  + Pixel Size X
   *  + Pixel Size Y
   *  + Bit Depth?
   *  + X, Y, W, H of frame
   *  + Temperature
   *  + ...etc
   *
   *
   *
   **********************************************************/

  ///////////////////////////
  // 1. Get Pixel size
  ///////////////////////////
  if (sim) {
    x_pixel_size = 5.4;
    y_pixel_size = 5.4;
  } else {
    // Actucal CALL to CCD to get pixel size here
    x_pixel_size = getPixelSize();
    y_pixel_size = getPixelSize();
  }

  ///////////////////////////
  // 2. Get Frame
  ///////////////////////////
  if (sim) {
    x_1 = y_1 = 0;
    x_2 = 1280;
    y_2 = 1024;
  } else {
    // Actucal CALL to CCD to get frame information here
    x_1 = getStartX();
    y_1 = getStartY();
    x_2 = x_1 + getWidth();
    y_2 = y_1 + getHeight();
  }

  ///////////////////////////
  // 3. Get temperature
  ///////////////////////////
  if (sim)
    TemperatureN[0].value = 25.0;
  else {
    // Actucal CALL to CCD to get temperature here
    TemperatureN[0].value = getSensorTemp();
  }

  IDMessage(getDeviceName(), "The CCD Temperature is %f.\n", TemperatureN[0].value);
  IDSetNumber(&TemperatureNP, NULL);

  if (do_debug)
    IDLog("The CCD Temperature is %f.\n", TemperatureN[0].value);

  ///////////////////////////
  // 4. Get temperature
  ///////////////////////////

  mode = getImgType();
  if (sim)
    bit_depth = 16;
  else {
    // Set your actual CCD bit depth
    if (mode == IMG_RGB24) bit_depth = 24;
    else if (mode == IMG_RAW16) bit_depth = 16;
    else bit_depth = 8;
  }

  SetCCDParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);
  if (do_debug) {
    IDLog("SetCCDParams %d %d - %d - %f %f\n", x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);
  }

  if (sim)
    minDuration = 0.05;
  else {
    // Set your actual CCD minimum exposure duration
    minDuration = getMin(CONTROL_EXPOSURE) / 1000000.0;
  }

  // Now we usually do the following in the hardware
  // Set Frame to LIGHT or NORMAL
  // Set Binning to 1x1
  /* Default frame type is NORMAL */

  // Let's calculate required buffer
  int nbuf;
  nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;    //  this is pixel cameraCount
  nbuf += 512;    //  leave a little extra at the end
  PrimaryCCD.setFrameBufferSize(nbuf);

  return true;

}

bool AsicamCCD::StartExposure(float duration)
{

  if (duration < minDuration)
  {
    IDLog("Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.", duration, minDuration);
    duration = minDuration;
  }

  if (imageFrameType == CCDChip::BIAS_FRAME)
  {
    duration = minDuration;
    IDLog("Bias Frame (s) : %g\n", minDuration);
  }

  /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD start exposure here
   *  Please note that duration passed is in seconds.
   *  If there is an error, report it back to client
   *  e.g.
   *  IDMessage(getDeviceName(), "Error, unable to start exposure due to ...");
   *  return -1;
   *
   *
   **********************************************************/

  PrimaryCCD.setExposureDuration(duration);
  ExposureRequest = duration;

  bool is_auto;
  int cur_dur = getValue(CONTROL_EXPOSURE, &is_auto);
  int ask_dur = duration * 1000000;
  if (cur_dur != ask_dur) {
    ::setValue(CONTROL_EXPOSURE, ask_dur, false);
    need_flush = true;
  }

  if (need_flush) {
    char * image = PrimaryCCD.getFrameBuffer();
    int i = 0;
    int BPP;

    switch (getImgType()) {
    case IMG_RAW8: BPP = 1; break;
    case IMG_RGB24: BPP = 3; break;
    case IMG_RAW16: BPP = 2; break;
    case IMG_Y8: BPP = 1; break;
    }

    while (getImageData((unsigned char *) image, getWidth() *  getHeight() * BPP, 0) && i < 10) {
      if (do_debug) {
	IDLog("flushing\n");
      }
      i++;
    }
    need_flush = false;
  }

  gettimeofday(&ExpStart, NULL);
  IDLog("Taking a %g seconds frame\n", ExposureRequest);
  if (do_debug) {
    bool gain_auto, exp_auto;
    int gain, exp;

    gain = getValue(CONTROL_GAIN, &gain_auto);
    exp = getValue(CONTROL_EXPOSURE, &exp_auto);
    IDLog("Cur: %d+%d  %dx%d bin %d type %d exp %d/%d gain %d/%d\n",
	  getStartX(), getStartY(),
	  getWidth(), getHeight(),
	  getBin(), getImgType(),
	  exp, exp_auto,
	  gain, gain_auto);
  }

  InExposure = true;

  return true;
}

bool AsicamCCD::AbortExposure() {

  /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD abort exposure here
   *  If there is an error, report it back to client
   *  e.g.
   *  IDMessage(getDeviceName(), "Error, unable to abort exposure due to ...");
   *  return false;
   *
   *
   **********************************************************/

  InExposure = false;
  return true;
}

bool AsicamCCD::UpdateCCDFrameType(CCDChip::CCD_FRAME fType) {
  int err = 0;
  CCDChip::CCD_FRAME imageFrameType = PrimaryCCD.getFrameType();

  need_flush = true;

  if (fType == imageFrameType || sim)
    return true;

  switch (imageFrameType) {
  case CCDChip::BIAS_FRAME:
  case CCDChip::DARK_FRAME:
  case CCDChip::FLAT_FRAME:
    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Frame type here
     *  BIAS and DARK are taken with shutter closed, so _usually_
     *  most CCD this is a call to let the CCD know next exposure shutter
     *  must be closed. Customize as appropiate for the hardware
     *  If there is an error, report it back to client
     *  e.g.
     *  IDMessage(getDeviceName(), "Error, unable to set frame type to ...");
     *  return false;
     *
     *
     **********************************************************/
    IDMessage(getDeviceName(), "Error, unable to set frame type");
    return false;
    break;

  case CCDChip::LIGHT_FRAME:
    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Frame type here
     *  LIGHT and FLAT are taken with shutter open, so _usually_
     *  most CCD this is a call to let the CCD know next exposure shutter
     *  must be open. Customize as appropiate for the hardware
     *  If there is an error, report it back to client
     *  e.g.
     *  IDMessage(getDeviceName(), "Error, unable to set frame type to ...");
     *  return false;
     *
     *
     **********************************************************/
    break;
  }

  PrimaryCCD.setFrameType(fType);

  return true;

}

bool AsicamCCD::UpdateCCDFrame(int x, int y, int w, int h) {
  /* Add the X and Y offsets */
  long x_1 = x;
  long y_1 = y;

  long bin_width = (w / PrimaryCCD.getBinX());
  long bin_height = (h / PrimaryCCD.getBinY());

  if (do_debug)
    IDLog("Asked image area is (%ld, %ld), (%ld, %ld)\n", x_1, y_1, bin_width, bin_height);

  if (bin_width > PrimaryCCD.getXRes() / PrimaryCCD.getBinX()) {
    IDMessage(getDeviceName(), "Error: invalid width requested %d", w);
    return false;
  } else if (bin_height > PrimaryCCD.getYRes() / PrimaryCCD.getBinY()) {
    IDMessage(getDeviceName(), "Error: invalid height request %d", h);
    return false;
  }

  if (do_debug)
    IDLog("The Final image area is (%ld, %ld), (%ld, %ld) bin %d\n", x_1, y_1, bin_width, bin_height, getBin());

  /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD Frame dimension call
   *  The values calculated above are BINNED width and height
   *  which is what most CCD APIs require, but in case your
   *  CCD API implementation is different, don't forget to change
   *  the above calculations.
   *  If there is an error, report it back to client
   *  e.g.
   *  IDMessage(getDeviceName(), "Error, unable to set frame to ...");
   *  return false;
   *
   *
   **********************************************************/

  if (((x_1 * y_1) % 1024 !=0) || ((bin_width * bin_height) % 1024 !=0)) {
    IDMessage(getDeviceName(), "Error, unable to set frame to width*height must be multiple of 1024\n");
    return false;
  }
  setImageFormat(bin_width, bin_height, getBin(), getImgType());
  setStartPos(x_1, y_1);
  if (do_debug)
    IDLog("Reall image area is (%ld, %ld), (%ld, %ld) bin %d\n", getStartX(), getStartY(), getWidth(), getHeight(), getBin());

  // Set UNBINNED coords
  PrimaryCCD.setFrame(x_1, y_1, w, h);

  int nbuf;
  nbuf = (bin_width * bin_height * PrimaryCCD.getBPP() / 8);    //  this is pixel count
  nbuf += 512;    //  leave a little extra at the end
  PrimaryCCD.setFrameBufferSize(nbuf);

  need_flush = true;

  if (do_debug)
    IDLog("Setting frame buffer size to %d bytes.\n", nbuf);

  return true;
}

bool AsicamCCD::UpdateCCDBin(int binx, int biny) {

  if (do_debug)
    IDLog("Asked for bin %dx%d.\n", binx, biny);

  /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD Binning call
   *  If there is an error, report it back to client
   *  e.g.
   *  IDMessage(getDeviceName(), "Error, unable to set binning to ...");
   *  return false;
   *
   *
   **********************************************************/
  if (binx != biny) {
    IDMessage(getDeviceName(), "Error, unable to set binning, must be equal");
    return false;
  }
  if (!isBinSupported(binx)) {
    IDMessage(getDeviceName(), "Error, unable to set binning, unsuppported");
    return false;
  }
  need_flush = true;
  if (do_debug)
    IDLog("Set bin %dx%d.\n", binx, biny);
  setImageFormat(getWidth() / binx, getHeight() / biny, binx, getImgType());
  if (do_debug)
    IDLog("Got bin %d.\n", getBin());

  PrimaryCCD.setBin(binx, biny);

  return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int AsicamCCD::grabImage() {
  char * image = PrimaryCCD.getFrameBuffer();
  int width = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP() / 8;
  int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

  if (do_debug) {
    int BPP;

    switch (getImgType()) {
    case IMG_RAW8: BPP = 1; break;
    case IMG_RGB24: BPP = 3; break;
    case IMG_RAW16: BPP = 2; break;
    case IMG_Y8: BPP = 1; break;
    }

    IDLog("GrabImage Width: %d - Height: %d\n", width, height);
    IDLog("Buf size: %d bytes vs %d bytes.\n", width * height, getWidth() *  getHeight() * BPP);
  }

  if (sim) {
    for (int i = 0; i < height; i++)
      for (int j = 0; j < width; j++)
        image[i * width + j] = rand() % 255;
  } else {
    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Get Image routine here
     *  use the image, width, and height variables above
     *  If there is an error, report it back to client
     *  e.g.
     *  IDMessage(getDeviceName(), "Error, unable to set binning to ...");
     *  return false;
     *
     *
     **********************************************************/
    if (!getImageData((unsigned char *) image, width * height, 0)) {
      if (do_debug)
	IDLog("getImageData returned 0.\n");
      return -1;
    }
  }

  if (do_debug)
    IDMessage(getDeviceName(), "Download complete.\n");

  if (do_debug)
    IDLog("Download complete.\n");

  ExposureComplete(&PrimaryCCD);

  PrimaryCCD.setExposureLeft(0);
  InExposure = false;

  return 0;
}

void AsicamCCD::addFITSKeywords(fitsfile *fptr, CCDChip *targetChip) {
  INDI::CCD::addFITSKeywords(fptr, targetChip);

  int status = 0;
  fits_update_key_s(fptr, TDOUBLE, "CCD-TEMP", &(TemperatureN[0].value), "CCD Temperature (Celcius)", &status);
  fits_write_date(fptr, &status);

}

void AsicamCCD::resetFrame() {
  UpdateCCDBin(1, 1);
  UpdateCCDFrame(0, 0, PrimaryCCD.getXRes(), PrimaryCCD.getYRes());
  IUResetSwitch(&ResetSP);
  ResetSP.s = IPS_IDLE;
  IDSetSwitch(&ResetSP, "Resetting frame and binning.");

  return;
}

void AsicamCCD::TimerHit() {
  int err = 0;

  if (isConnected() == false)
    return;  //  No need to reset timer if we are not connected anymore

  if (InExposure) {
    grabImage();
  }

  SetTimer(POLLMS);
  return;
}

bool AsicamCCD::GuideNorth(float duration) {
  /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD Guide call
   *  Some CCD API support pulse guiding directly (i.e. without timers)
   *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
   *  will have to start a timer and then stop it after the 'duration' seconds
   *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
   *  available in INDI 3rd party repository
   *  If there is an error, report it back to client
   *  e.g.
   *  IDMessage(getDeviceName(), "Error, unable to guide due ...");
   *  return false;
   *
   *
   **********************************************************/

  if (do_debug) {
    IDLog("N %f\n", duration);
  }
  pulseGuide(guideNorth, duration);
  return true;
}

bool AsicamCCD::GuideSouth(float duration) {
  /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD Guide call
   *  Some CCD API support pulse guiding directly (i.e. without timers)
   *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
   *  will have to start a timer and then stop it after the 'duration' seconds
   *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
   *  available in INDI 3rd party repository
   *  If there is an error, report it back to client
   *  e.g.
   *  IDMessage(getDeviceName(), "Error, unable to guide due ...");
   *  return false;
   *
   *
   **********************************************************/

  if (do_debug) {
    IDLog("S %f\n", duration);
  }
  pulseGuide(guideSouth, duration);
  return true;
}

bool AsicamCCD::GuideEast(float duration) {
  /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD Guide call
   *  Some CCD API support pulse guiding directly (i.e. without timers)
   *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
   *  will have to start a timer and then stop it after the 'duration' seconds
   *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
   *  available in INDI 3rd party repository
   *  If there is an error, report it back to client
   *  e.g.
   *  IDMessage(getDeviceName(), "Error, unable to guide due ...");
   *  return false;
   *
   *
   **********************************************************/

  if (do_debug) {
    IDLog("E %f\n", duration);
  }
  pulseGuide(guideEast, duration);
  return true;
}

bool AsicamCCD::GuideWest(float duration) {
  /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD Guide call
   *  Some CCD API support pulse guiding directly (i.e. without timers)
   *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
   *  will have to start a timer and then stop it after the 'duration' seconds
   *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
   *  available in INDI 3rd party repository
   *  If there is an error, report it back to client
   *  e.g.
   *  IDMessage(getDeviceName(), "Error, unable to guide due ...");
   *  return false;
   *
   *
   **********************************************************/
 
  if (do_debug) {
    IDLog("W %f\n", duration);
  }
  pulseGuide(guideWest, duration);
  return true;
}

bool AsicamCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {
  if (!strcmp(dev, getDeviceName())) {
    if (!strcmp(name, "CCD_GAIN")) {
      GainNP.s = IPS_BUSY;
      IDSetNumber(&GainNP, NULL);
      IUUpdateNumber(&GainNP, values, names, n);
      need_flush = true;
      ::setValue(CONTROL_GAIN, GainN[0].value, false);
      if (do_debug) {
	IDLog("Gain %d\n", GainN[0].value);
      }
      GainNP.s = IPS_OK;
      IDSetNumber(&GainNP, NULL);
      return true;
    }

  }
  return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}
