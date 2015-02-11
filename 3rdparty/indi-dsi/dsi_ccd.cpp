/**
 * Copyright (C) 2015 Ben Gilsrud
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <sys/time.h>
#include <memory>
#include <stdint.h>
#include <arpa/inet.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

#include <indiapi.h>
#include <iostream>
#include "dsi_ccd.h"
#include "DsiDeviceFactory.h"

const int POLLMS = 250;

std::auto_ptr<DSICCD> dsiCCD(0);


void ISInit()
{
    static int isInit =0;
    if (isInit == 1)
        return;

     isInit = 1;
     if(dsiCCD.get() == 0) dsiCCD.reset(new DSICCD());
}

void ISGetProperties(const char *dev)
{
         ISInit();
         dsiCCD->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
         ISInit();
         dsiCCD->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
         ISInit();
         dsiCCD->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
         ISInit();
         dsiCCD->ISNewNumber(dev, name, values, names, num);
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
     dsiCCD->ISSnoopDevice(root);
}


DSICCD::DSICCD()
{
    InExposure = false;
    capturing = false;
    dsi = NULL;

    setVersion(0, 1);
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool DSICCD::Connect()
{
    string ccd;
    dsi = DSI::DeviceFactory::getInstance(NULL);
    if (!dsi) {
            /* The vendor and product ID for all DSI's (I/II/III) are the same. When the
            * Cypress FX2 firmware hasn't been loaded, the PID will be 0x0100. Once the fw
            * is loaded, the PID becomes 0x0101.
            */
            DEBUG(INDI::Logger::DBG_SESSION,  "Unable to find DSI. Has the firmware been loaded?");
            return false;
    }
    ccd = dsi->getCcdChipName();
    if (ccd == "ICX254AL") {
        DEBUG(INDI::Logger::DBG_SESSION,  "Found a DSI Pro!");
    } else if (ccd == "ICX429ALL") {
        DEBUG(INDI::Logger::DBG_SESSION,  "Found a DSI Pro II!");
    }

    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool DSICCD::Disconnect()
{
    if (dsi) {
        delete dsi;
        dsi = NULL;
    }

    DEBUG(INDI::Logger::DBG_SESSION,  "Successfully disconnected!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char * DSICCD::getDefaultName()
{
    return "Meade DSI Pro (I/II)";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool DSICCD::initProperties()
{
    CCDCapability cap;

    // Must init parent properties first!
    INDI::CCD::initProperties();

    // Add Debug Control.
    addDebugControl();

    /* Add Gain number property */
    IUFillNumber(GainN, "GAIN", "Gain", "%d", 0, 63, 1, 0);
    IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "GAIN", "Gain", IMAGE_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    cap.canAbort    = true;
    cap.canBin      = false;
    cap.canSubFrame = false;
    cap.hasCooler   = false;
    cap.hasGuideHead= false;
    cap.hasShutter  = false;
    cap.hasST4Port  = false;
    cap.hasBayer = false;

    SetCCDCapability(&cap);

    return true;

}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool DSICCD::updateProperties()
{
    // Call parent update properties first
    INDI::CCD::updateProperties();

    if (isConnected())
    {

        // Let's get parameters now from CCD
        setupParams();

        // Start the timer
        SetTimer(POLLMS);
        defineNumber(&GainNP);
    } else {
        deleteProperty(GainNP.name);
    }

    return true;
}

/**************************************************************************************
** Setting up CCD parameters
***************************************************************************************/
void DSICCD::setupParams()
{
    SetCCDParams(dsi->getImageWidth(), dsi->getImageHeight(),
        dsi->getReadBpp() * 8, dsi->getPixelSizeX(), dsi->getPixelSizeY());

    /* calculate how much memory we need for the primary CCD buffer */
    PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes()*PrimaryCCD.getYRes() * PrimaryCCD.getBPP()/8);
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool DSICCD::StartExposure(float duration)
{
    ExposureRequest=duration;

    // Since we have only have one CCD with one chip, we set the exposure duration of the primary CCD
    PrimaryCCD.setBPP(dsi->getReadBpp() * 8);
    PrimaryCCD.setExposureDuration(duration);

    gettimeofday(&ExpStart,NULL);

    InExposure=true;
    DEBUG(INDI::Logger::DBG_SESSION,  "Exposure has begun.");

    // Get width and height
    int width = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
    int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

    dsi->startExposure(duration * 10000);


    return true;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool DSICCD::AbortExposure()
{
    InExposure = false;
    return true;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float DSICCD::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec/1000);
    timesince=timesince/1000;

    timeleft=ExposureRequest-timesince;
    return timeleft;
}

/**************************************************************************************
** Client is asking us to set a new number
***************************************************************************************/
bool DSICCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp(dev, getDeviceName())) {
        if (!strcmp(name, GainNP.name)) {
            dsi->setGain(values[0]);
            if (IUUpdateNumber(&GainNP, values, names, n) < 0) {
                return false;
            }
            return true;
        }

    }

    // If we didn't process anything above, let the parent handle it.
    return INDI::CCD::ISNewNumber(dev,name,values,names,n);
}

/**************************************************************************************
** Main device loop. We check for exposure progress
***************************************************************************************/
void DSICCD::TimerHit()
{
    long timeleft;

    if(isConnected() == false) {
        return;  //  No need to reset timer if we are not connected anymore
    }

    if (InExposure) {
        timeleft=CalcTimeLeft();

        // Less than a 0.1 second away from exposure completion
        // This is an over simplified timing method, check CCDSimulator and dsiCCD for better timing checks
        if (timeleft < 0.1) {
            /* We're done exposing */
            DEBUG(INDI::Logger::DBG_SESSION,  "Exposure done, downloading image...");

            // Set exposure left to zero
            PrimaryCCD.setExposureLeft(0);

            // We're no longer exposing...
            InExposure = false;

            /* grab and save image */
            grabImage();
        } else {
            // Just update time left in client
            PrimaryCCD.setExposureLeft(timeleft);
        }
    }

    SetTimer(POLLMS);
    return;
}

/**
 * Download image from FireFly
 */
void DSICCD::grabImage()
{
   int sub;
   uint16_t val;
   struct timeval start, end;
   int rc;
   uint16_t *buf;
   int x, y;

   // Let's get a pointer to the frame buffer
   char * image = PrimaryCCD.getFrameBuffer();

   // Get width and height
   int width = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
   int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

   try {
       buf = (uint16_t *) dsi->downloadImage();
   } catch (...) {
       DEBUG(INDI::Logger::DBG_SESSION,  "Image download failed!");
       return;
   }

   for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            ((uint16_t *) image)[x + width * y] = ntohs(buf[x + width * y]);
        }
   }
   
   delete buf;

   // Let INDI::CCD know we're done filling the image buffer
   ExposureComplete(&PrimaryCCD);

   DEBUG(INDI::Logger::DBG_SESSION, "Exposure complete.");
}
