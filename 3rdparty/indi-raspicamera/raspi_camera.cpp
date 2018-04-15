/*
 Generic CCD
 CCD Template for INDI Developers
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

#include "config.h"
#include "indidevapi.h"
#include "eventloop.h"

#include "raspi_camera.h"
#include "./raspicam/src/raspicam.h"

#define MAX_CCD_TEMP   45   /* Max CCD temperature */
#define MIN_CCD_TEMP   -55  /* Min CCD temperature */
#define MAX_X_BIN      16   /* Max Horizontal binning */
#define MAX_Y_BIN      16   /* Max Vertical binning */
#define MAX_PIXELS     4096 /* Max number of pixels in one dimension */
#define TEMP_THRESHOLD .25  /* Differential temperature threshold (C)*/
#define MAX_DEVICES    2   /* Max device cameraCount */

static int cameraCount;
static RasPiCamera *cameras[MAX_DEVICES];


/**********************************************************
 *
 *  IMPORRANT: List supported camera models in initializer of deviceTypes structure
 *
 **********************************************************/

static struct
{
    int vid;
    int pid;
    const char *name;
} deviceTypes[] = { { 0x0001, 0x0001, "Model 1" }, { 0x0001, 0x0002, "Model 2" }, { 0, 0, NULL } };

static void cleanup()
{
    for (int i = 0; i < cameraCount; i++)
    {
        delete cameras[i];
    }
}

void ISInit()
{
    //LOG_DEBUG("Raspberry Pi Camera::ISInit()");
    static bool isInit = false;
    if (!isInit)
    {
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
             cameras[i] = new RasPiCamera(dev, deviceTypes[i].name);
             break;
           }
         }
       }
     }
     */

        /* For demo purposes we are creating two test devices */
        cameraCount            = 2;
        struct usb_device *dev = NULL;
        cameras[0]             = new RasPiCamera(dev, deviceTypes[0].name);
        cameras[1]             = new RasPiCamera(dev, deviceTypes[1].name);

        atexit(cleanup);
        isInit = true;
    }
    //LOG_DEBUG("Raspberry Pi Camera::ISInit() done");
}

void ISGetProperties(const char *dev)
{
    ISInit();
    //LOG_DEBUG("Raspberry Pi Camera::ISGetProperties()");
    for (int i = 0; i < cameraCount; i++)
    {
        RasPiCamera *camera = cameras[i];
        if (dev == NULL || !strcmp(dev, camera->name))
        {
            camera->ISGetProperties(dev);
            if (dev != NULL)
                break;
        }
    }
//    LOG_DEBUG("Raspberry Pi Camera::ISGetProperties()");
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        RasPiCamera *camera = cameras[i];
        if (dev == NULL || !strcmp(dev, camera->name))
        {
            camera->ISNewSwitch(dev, name, states, names, num);
            if (dev != NULL)
                break;
        }
    }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        RasPiCamera *camera = cameras[i];
        if (dev == NULL || !strcmp(dev, camera->name))
        {
            camera->ISNewText(dev, name, texts, names, num);
            if (dev != NULL)
                break;
        }
    }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    ISInit();
    for (int i = 0; i < cameraCount; i++)
    {
        RasPiCamera *camera = cameras[i];
        if (dev == NULL || !strcmp(dev, camera->name))
        {
            camera->ISNewNumber(dev, name, values, names, num);
            if (dev != NULL)
                break;
        }
    }
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
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
void ISSnoopDevice(XMLEle *root)
{
    ISInit();

    for (int i = 0; i < cameraCount; i++)
    {
        RasPiCamera *camera = cameras[i];
        camera->ISSnoopDevice(root);
    }
}

RasPiCamera::RasPiCamera(DEVICE device, const char *name)
{
	LOG_DEBUG("Raspberry Pi Camera::RasPiCamera()");
    this->device = device;
    snprintf(this->name, 32, "Raspberry Pi Camera %s", name);
    setDeviceName(this->name);

    setVersion(GENERIC_VERSION_MAJOR, GENERIC_VERSION_MINOR);
	LOG_DEBUG("Raspberry Pi Camera::RasPiCamera() done");
}

RasPiCamera::~RasPiCamera()
{
}

const char *RasPiCamera::getDefaultName()
{
    return "Raspberry Pi Camera";
}

bool RasPiCamera::initProperties()
{
    LOG_DEBUG("Raspberry Pi Camera::initProperties()");
	// Init parent properties first
    INDI::CCD::initProperties();

    //uint32_t cap = CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_COOLER | CCD_HAS_SHUTTER | CCD_HAS_ST4_PORT | CCD_HAS_BAYER ;
    // Possible: CCD_CAN_BIN | CCD_CAN_SUBFRAME
    uint32_t cap = CCD_CAN_ABORT  | CCD_CAN_SUBFRAME ;
    SetCCDCapability(cap);

    addConfigurationControl();
    addDebugControl();
    IUFillSwitchVector(&mIsoSP, NULL, 0, getDeviceName(), "CCD_ISO", "ISO", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60,
		       IPS_IDLE);
    //   if (sim)
    //    {
    setidx             = 0;
    max_opts           = 4;
    const char *isos[] = { "100", "200", "400", "800" };
    options            = (char **)isos;
    //   }
    
    mIsoS      = create_switch("ISO", options, max_opts, setidx);
    mIsoSP.sp  = mIsoS;
    mIsoSP.nsp = max_opts;
	LOG_DEBUG("Raspberry Pi Camera::initProperties() done");
    return true;
}

void RasPiCamera::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);
}

bool RasPiCamera::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Let's get parameters now from CCD
        setupParams();

        timerID = SetTimer(POLLMS);
    }
    else
    {
        rmTimer(timerID);
    }

    return true;
}

bool RasPiCamera::Connect()
{
	int setidx;
	char **options;
	int max_opts;
    LOG_INFO("Attempting to find the Generic CCD...");
    sim = isSimulation();

    /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD Connect function
   *  If you encameraCounter an error, send the client a message
   *  e.g.
   *  LOG_INFO( "Error, unable to connect due to ...");
   *  return false;
   *
   *
   **********************************************************/
    LOG_INFO( "Initializing");
    
    LOG_INFO( "Error, unable to connect to Raspberry Pi Camera");
    
    /* Success! */
    LOG_INFO("CCD is online. Retrieving basic data.");

    
    
    return true;
}

bool RasPiCamera::Disconnect()
{
    /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD disonnect function
   *  If you encameraCounter an error, send the client a message
   *  e.g.
   *  LOG_INFO( "Error, unable to disconnect due to ...");
   *  return false;
   *
   *
   **********************************************************/

    LOG_INFO("CCD is offline.");
    return true;
}

bool RasPiCamera::setupParams()
{
	LOG_DEBUG("Raspberry Pi Camera::setupParams()");
    float x_pixel_size, y_pixel_size;
    int bit_depth = 24;
    int x_1, y_1, x_2, y_2;

    //x_2= 3280;
    //y_2=2464;
    x_2=1280;
    y_2=960;
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
    // Actucal CALL to CCD to get pixel size here
    // TODO: Check camera version Currently V2 only (1.12) V1 is 1.4
    x_pixel_size = 1.12;
    y_pixel_size = 1.12;

    
    ///////////////////////////
    // 2. Get Frame
    ///////////////////////////

    // Actucal CALL to CCD to get frame information here
    x_1 = y_1 = 0;
    Camera.setWidth(x_2-x_1);
    Camera.setHeight(y_2-y_1);
    //TODO: Control
	if (isoSpeed < 100 || isoSpeed >800) isoSpeed = 100;
    Camera.setISO(isoSpeed);
    LOGF_INFO("Camera Speed set to %d ISO", %d);
    //TODO: Check encoding
    //Camera.setEncoding ( raspicam::RASPICAM_ENCODING_PNG );
    Camera.setFormat(raspicam::RASPICAM_FORMAT_RGB); //24 Bit RGB
    Camera.setBrightness(50);
    Camera.setSharpness (0);
    Camera.setContrast (0);
    Camera.setSaturation (0);
    //Camera.setShutterSpeed( getParamVal ( "-ss",argc,argv,0 ) );
    ///////////////////////////
    // 3. Get temperature
    ///////////////////////////
    // Setting sample temperature -- MAKE CALL TO API FUNCTION TO GET TEMPERATURE IN REAL DRIVER
    TemperatureN[0].value = 25.0;
    LOGF_INFO("The CCD Temperature is %f", TemperatureN[0].value);
    IDSetNumber(&TemperatureNP, NULL);

    ///////////////////////////
    // 4. Get temperature
    ///////////////////////////
    bit_depth = 8;
    PrimaryCCD.setNAxis(3);
    SetCCDParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);

    // Now we usually do the following in the hardware
    // Set Frame to LIGHT or NORMAL
    // Set Binning to 1x1
    /* Default frame type is NORMAL */

    // Let's calculate required buffer
    int nbuf;
    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8 * PrimaryCCD.getNAxis(); //  this is pixel cameraCount
    nbuf += 512;                                                                  //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);
LOG_DEBUG("Raspberry Pi Camera::setupParams() done");
    return true;
}

int RasPiCamera::SetTemperature(double temperature)
{
    LOG_DEBUG("Raspberry Pi Camera::SetTemperature()");
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature - TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    /**********************************************************
     *
     *  IMPORRANT: Put here your CCD Set Temperature Function
     *  We return 0 if setting the temperature will take some time
     *  If the requested is the same as current temperature, or very
     *  close, we return 1 and INDI::CCD will mark the temperature status as OK
     *  If we return 0, INDI::CCD will mark the temperature status as BUSY
     **********************************************************/

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    TemperatureRequest = temperature;
    LOGF_INFO("Setting CCD temperature to %+06.2f C", temperature);
    LOG_DEBUG("Raspberry Pi Camera::SetTemperature() done");
    return 0;
}

bool RasPiCamera::StartExposure(float duration)
{
    LOG_DEBUG("Raspberry Pi Camera::StartExposure()");
    if (duration < minDuration)
    {
        DEBUGF(INDI::Logger::DBG_WARNING,
               "Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.", duration,
               minDuration);
        duration = minDuration;
    }

    if (imageFrameType == INDI::CCDChip::BIAS_FRAME)
    {
        duration = minDuration;
        LOGF_INFO("Bias Frame (s) : %g\n", minDuration);
    }

    /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD start exposure here
   *  Please note that duration passed is in seconds.
   *  If there is an error, report it back to client
   *  e.g.
   *  LOG_INFO( "Error, unable to start exposure due to ...");
   *  return -1;
   *
   *
   **********************************************************/

    PrimaryCCD.setExposureDuration(duration);
    ExposureRequest = duration;

    gettimeofday(&ExpStart, NULL);
    LOGF_INFO("Taking a %g seconds frame...", ExposureRequest);

    InExposure = true;
    long int RPI_Duration = duration * 1000;
    //if (RPI_Duration > 6000000) RPI_Duration = 6000000;
    
    if (RPI_Duration > 330000) RPI_Duration = 330000;
    Camera.setShutterSpeed(RPI_Duration);
 //   Camera.setExposure(RPI_Duration);
    Camera.setExposure(raspicam::RASPICAM_EXPOSURE_AUTO);
    Camera.startCapture();
    LOG_DEBUG("Raspberry Pi Camera::StartExposure() done");
    return true;
}

bool RasPiCamera::AbortExposure()
{
    /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD abort exposure here
   *  If there is an error, report it back to client
   *  e.g.
   *  LOG_INFO( "Error, unable to abort exposure due to ...");
   *  return false;
   *
   *
   **********************************************************/

    InExposure = false;
    return true;
}

bool RasPiCamera::UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType)
{
    INDI::CCDChip::CCD_FRAME imageFrameType = PrimaryCCD.getFrameType();

    if (fType == imageFrameType)
        return true;

    switch (imageFrameType)
    {
        case INDI::CCDChip::BIAS_FRAME:
        case INDI::CCDChip::DARK_FRAME:
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
     *  LOG_INFO( "Error, unable to set frame type to ...");
     *  return false;
     *
     *
     **********************************************************/
            break;

        case INDI::CCDChip::LIGHT_FRAME:
        case INDI::CCDChip::FLAT_FRAME:
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
     *  LOG_INFO( "Error, unable to set frame type to ...");
     *  return false;
     *
     *
     **********************************************************/
            break;
    }

    PrimaryCCD.setFrameType(fType);

    return true;
}

bool RasPiCamera::UpdateCCDFrame(int x, int y, int w, int h)
{
    /* Add the X and Y offsets */
    long x_1 = x;
    long y_1 = y;

    long bin_width  = x_1 + (w / PrimaryCCD.getBinX());
    long bin_height = y_1 + (h / PrimaryCCD.getBinY());

    if (bin_width > PrimaryCCD.getXRes() / PrimaryCCD.getBinX())
    {
        LOGF_INFO("Error: invalid width requested %d", w);
        return false;
    }
    else if (bin_height > PrimaryCCD.getYRes() / PrimaryCCD.getBinY())
    {
        LOGF_INFO("Error: invalid height request %d", h);
        return false;
    }

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
   *  LOG_INFO( "Error, unable to set frame to ...");
   *  return false;
   *
   *
   **********************************************************/

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x_1, y_1, w, h);

    int nbuf;
    nbuf = (bin_width * bin_height * PrimaryCCD.getBPP() / 8); //  this is pixel count
    nbuf += 512;                                               //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    LOGF_DEBUG("Setting frame buffer size to %d bytes.", nbuf);

    return true;
}

bool RasPiCamera::UpdateCCDBin(int binx, int biny)
{
    /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD Binning call
   *  If there is an error, report it back to client
   *  e.g.
   *  LOG_INFO( "Error, unable to set binning to ...");
   *  return false;
   *
   *
   **********************************************************/

    PrimaryCCD.setBin(binx, biny);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

float RasPiCamera::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now, NULL);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = ExposureRequest - timesince;
    return timeleft;
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int RasPiCamera::grabImage()
{
//    uint8_t *image = PrimaryCCD.getFrameBuffer();
//    int width      = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP() / 8;
//    int height     = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();
//    width= 3280*3;
//    height = 2464*3;

    /**********************************************************
     *
     *
     *  IMPORRANT: Put here your CCD Get Image routine here
     *  use the image, width, and height variables above
     *  If there is an error, report it back to client
     *
     *
     **********************************************************/


    uint8_t *image = PrimaryCCD.getFrameBuffer();
    
    uint8_t *buffer = image;
    LOGF_DEBUG("Camera.getImageBufferSize() %d", Camera.getImageBufferSize() );
    LOGF_DEBUG("PrimaryCCD.getFrameBuffer() %d", PrimaryCCD.getFrameBuffer());
    
    int x_width     = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * (PrimaryCCD.getBPP() / 8);
    int x_height    = PrimaryCCD.getSubH() / PrimaryCCD.getBinY() * (PrimaryCCD.getBPP() / 8);
//     int nChannels = (type == ASI_IMG_RGB24) ? 3 : 1;
    int nChannels = 3;
    size_t size = x_width * x_height * nChannels;
    
//     if (type == ASI_IMG_RGB24)
//     {
	    buffer = (unsigned char *)malloc(size);
	    if (buffer == nullptr)
	    {
		    LOGF_ERROR("RasPiCamera ID: %d sized malloc failed (RGB 24)", size);
// 		    LOGF_ERROR("CCD ID: %d malloc failed (RGB 24)",
// 			       m_camInfo->CameraID);
		    return -1;
	    }
//     }

    Camera.grab();
    //Camera.retrieve(image);
    Camera.retrieve ( buffer);//,raspicam::RASPICAM_FORMAT_RGB );

    LOG_INFO("Download complete. Starting Conversion from RGBRGB to  RRRR....GGGG...BBBB....");
    
    //Below is copied from INDI ASI
//    if ((errCode = ASIGetDataAfterExp(m_camInfo->CameraID, buffer, size)) != ASI_SUCCESS)
//     {
// // 	    LOGF_ERROR("ASIGetDataAfterExp (%dx%d #%d channels) error (%d)", width, height, nChannels,
// 		       errCode);
// 	    if (type == ASI_IMG_RGB24)
// 		    free(buffer);
// 	    return -1;
//     }
    
//     if (type == ASI_IMG_RGB24)
//     {
//    if (1) {
	    uint8_t *subR = image;
	    uint8_t *subG = image + x_width * x_height;
	    uint8_t *subB = image + x_width * x_height * 2;
	    int x_size      = x_width * x_height * 3 - 3;
	    
	    for (int i = 0; i <= x_size; i += 3)
	    {
		    *subB++ = buffer[i];
		    *subG++ = buffer[i + 1];
		    *subR++ = buffer[i + 2];
	    }
	    
	    free(buffer);
//     }
//    }
	LOG_INFO("Conversion complete.");
    
    
    


    ExposureComplete(&PrimaryCCD);
	
    return 0;
}

void RasPiCamera::TimerHit()
{
    int timerID = -1;
    long timeleft;

    if (isConnected() == false)
        return; //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        timeleft = CalcTimeLeft();

        if (timeleft < 1.0)
        {
            if (timeleft > 0.25)
            {
                //  a quarter of a second or more
                //  just set a tighter timer
                timerID = SetTimer(250);
            }
            else
            {
                if (timeleft > 0.07)
                {
                    //  use an even tighter timer
                    timerID = SetTimer(50);
                }
                else
                {
                    //  it's real close now, so spin on it
                    while (timeleft > 0)
                    {
                        /**********************************************************
             *
             *  IMPORRANT: If supported by your CCD API
             *  Add a call here to check if the image is ready for download
             *  If image is ready, set timeleft to 0. Some CCDs (check FLI)
             *  also return timeleft in msec.
             *
             **********************************************************/

                        // Breaking in simulation, in real driver either loop until time left = 0 or use an API call to know if the image is ready for download
                        break;

                        //int slv;
                        //slv = 100000 * timeleft;
                        //usleep(slv);
                    }

                    /* We're done exposing */
                    LOG_INFO("Exposure done, downloading image...");

                    PrimaryCCD.setExposureLeft(0);
                    InExposure = false;
                    /* grab and save image */
                    grabImage();
                }
            }
        }
        else
        {
            if (isDebug())
            {
                IDLog("With time left %ld\n", timeleft);
                IDLog("image not yet ready....\n");
            }

            PrimaryCCD.setExposureLeft(timeleft);
        }
    }

    switch (TemperatureNP.s)
    {
        case IPS_IDLE:
        case IPS_OK:
            /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Get temperature call here
     *  If there is an error, report it back to client
     *  e.g.
     *  LOG_INFO( "Error, unable to get temp due to ...");
     *  return false;
     *
     *
     **********************************************************/
            break;

        case IPS_BUSY:
            /**********************************************************
       *
       *
       *
       *  IMPORRANT: Put here your CCD Get temperature call here
       *  If there is an error, report it back to client
       *  e.g.
       *  LOG_INFO( "Error, unable to get temp due to ...");
       *  return false;
       *
       *
       **********************************************************/
            TemperatureN[0].value = TemperatureRequest;

            // If we're within threshold, let's make it BUSY ---> OK
            if (fabs(TemperatureRequest - TemperatureN[0].value) <= TEMP_THRESHOLD)
                TemperatureNP.s = IPS_OK;

            IDSetNumber(&TemperatureNP, NULL);
            break;

        case IPS_ALERT:
            break;
    }

    if (timerID == -1)
        SetTimer(POLLMS);
    return;
}

IPState RasPiCamera::GuideNorth(float ms)
{
    INDI_UNUSED(ms);
    /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD Guide call
   *  Some CCD API support pulse guiding directly (i.e. without timers)
   *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
   *  will have to start a timer and then stop it after the 'ms' milliseconds
   *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
   *  available in INDI 3rd party repository
   *  If there is an error, report it back to client
   *  e.g.
   *  LOG_INFO( "Error, unable to guide due ...");
   *  return IPS_ALERT;
   *
   *
   **********************************************************/

    return IPS_OK;
}

IPState RasPiCamera::GuideSouth(float ms)
{
    INDI_UNUSED(ms);
    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Guide call
     *  Some CCD API support pulse guiding directly (i.e. without timers)
     *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
     *  will have to start a timer and then stop it after the 'ms' milliseconds
     *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
     *  available in INDI 3rd party repository
     *  If there is an error, report it back to client
     *  e.g.
     *  LOG_INFO( "Error, unable to guide due ...");
     *  return IPS_ALERT;
     *
     *
     **********************************************************/

    return IPS_OK;
}

IPState RasPiCamera::GuideEast(float ms)
{
    INDI_UNUSED(ms);
    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Guide call
     *  Some CCD API support pulse guiding directly (i.e. without timers)
     *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
     *  will have to start a timer and then stop it after the 'ms' milliseconds
     *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
     *  available in INDI 3rd party repository
     *  If there is an error, report it back to client
     *  e.g.
     *  LOG_INFO( "Error, unable to guide due ...");
     *  return IPS_ALERT;
     *
     *
     **********************************************************/

    return IPS_OK;
}

IPState RasPiCamera::GuideWest(float ms)
{
    INDI_UNUSED(ms);
    /**********************************************************
     *
     *
     *
     *  IMPORRANT: Put here your CCD Guide call
     *  Some CCD API support pulse guiding directly (i.e. without timers)
     *  Others implement GUIDE_ON and GUIDE_OFF for each direction, and you
     *  will have to start a timer and then stop it after the 'ms' milliseconds
     *  For an example on timer usage, please refer to indi-sx and indi-gpusb drivers
     *  available in INDI 3rd party repository
     *  If there is an error, report it back to client
     *  e.g.
     *  LOG_INFO( "Error, unable to guide due ...");
     *  return IPS_ALERT;
     *
     *
     **********************************************************/

    return IPS_OK;
}

//Copied from gphoto_ccd.cpp
ISwitch *RasPiCamera::create_switch(const char *basestr, char **options, int max_opts, int setidx)
{
	int i;
	ISwitch *sw     = (ISwitch *)calloc(sizeof(ISwitch), max_opts);
	ISwitch *one_sw = sw;
	
	char sw_name[MAXINDINAME];
	char sw_label[MAXINDILABEL];
	ISState sw_state;
	
	for (i = 0; i < max_opts; i++)
	{
		snprintf(sw_name, MAXINDINAME, "%s%d", basestr, i);
		strncpy(sw_label, options[i], MAXINDILABEL);
		sw_state = (i == setidx) ? ISS_ON : ISS_OFF;
		
		IUFillSwitch(one_sw++, sw_name, sw_label, sw_state);
	}
	return sw;
}

bool RasPiCamera::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
	LOG_DEBUG("RasPiCamera::ISNewSwitch");
	if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
	{
		if (!strcmp(name, mIsoSP.name))
		{
			LOG_DEBUG("RasPiCamera::ISNewSwitch mIsoSP.name");
			if (IUUpdateSwitch(&mIsoSP, states, names, n) < 0)
				return false;
			
			for (int i = 0; i < mIsoSP.nsp; i++)
			{

				ISwitch *onISO = IUFindOnSwitch(&mIsoSP);
				if (onISO)
				{
					isoSpeed = -1;
					isoSpeed     = atoi(onISO->label);
					//if (sim == false) {
					if (isoSpeed > 0)
						LOGF_INFO("Setting ISO Speed to: %d", isoSpeed);
						//fits_update_key_s(fptr, TUINT, "ISOSPEED", &isoSpeed, "ISO Speed", &status);
						Camera.setISO(isoSpeed);
					//}
				}
				mIsoSP.s = IPS_OK;
				IDSetSwitch(&mIsoSP, NULL);
				break;
				
			}
		}
	}
	return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}
