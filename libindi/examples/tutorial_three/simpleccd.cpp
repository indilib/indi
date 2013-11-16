/*
   INDI Developers Manual
   Tutorial #3

   "Simple CCD Driver"

   We develop a simple CCD driver.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file simpleccd.cpp
    \brief Construct a basic INDI CCD device that simulates exposure & temperature settings. It also generates a random pattern and uploads it as a FITS file.
    \author Jasem Mutlaq

    \example simpleccd.cpp
    A simple CCD device that can capture images and control temperature. It returns a FITS image to the client. To build drivers for complex CCDs, please
    refer to the INDI Generic CCD driver template in INDI SVN (under 3rdparty).
*/

#include <sys/time.h>
#include <memory>

#include "simpleccd.h"

const int POLLMS           = 500;       /* Polling interval 500 ms */
const int MAX_CCD_TEMP     = 45;		/* Max CCD temperature */
const int MIN_CCD_TEMP	   = -55;		/* Min CCD temperature */
const float TEMP_THRESHOLD = .25;		/* Differential temperature threshold (C)*/

/* Macro shortcut to CCD temperature value */
#define currentCCDTemperature   TemperatureN[0].value

std::auto_ptr<SimpleCCD> simpleCCD(0);

void ISInit()
{
    static int isInit =0;
    if (isInit == 1)
        return;

     isInit = 1;
     if(simpleCCD.get() == 0) simpleCCD.reset(new SimpleCCD());
}

void ISGetProperties(const char *dev)
{
         ISInit();
         simpleCCD->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
         ISInit();
         simpleCCD->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
         ISInit();
         simpleCCD->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
         ISInit();
         simpleCCD->ISNewNumber(dev, name, values, names, num);
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
     simpleCCD->ISSnoopDevice(root);
}


SimpleCCD::SimpleCCD()
{
    InExposure = false;
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool SimpleCCD::Connect()
{
    IDMessage(getDeviceName(), "Simple CCD connected successfully!");

    // Let's set a timer that checks teleCCDs status every POLLMS milliseconds.
    SetTimer(POLLMS);

    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool SimpleCCD::Disconnect()
{
    IDMessage(getDeviceName(), "Simple CCD disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char * SimpleCCD::getDefaultName()
{
    return "Simple CCD";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool SimpleCCD::initProperties()
{
    // Must init parent properties first!
    INDI::CCD::initProperties();

    // We init the property details. This is a stanard property of the INDI Library.
    IUFillNumber(&TemperatureN[0], "CCD_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", MIN_CCD_TEMP, MAX_CCD_TEMP, 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "CCD_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Add Debug, Simulator, and Configuration controls
    addAuxControls();

    return true;

}

/**************************************************************************************
** INDI is asking us to submit list of properties for the device
***************************************************************************************/
void SimpleCCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);

    // If we are _already_ connected, let's define our temperature property to the client now
    if (isConnected())
    {
        // Define our only property temperature
        defineNumber(&TemperatureNP);
    }

}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool SimpleCCD::updateProperties()
{
    // Call parent update properties first
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        // Define our only property temperature
        defineNumber(&TemperatureNP);

        // Let's get parameters now from CCD
        setupParams();

        // Start the timer
        SetTimer(POLLMS);
    }
    else
    // We're disconnected
    {
        deleteProperty(TemperatureNP.name);
    }

    return true;
}

/**************************************************************************************
** Setting up CCD parameters
***************************************************************************************/
void SimpleCCD::setupParams()
{
    // Our CCD is an 8 bit CCD, 1280x1024 resolution, with 5.4um square pixels.
    SetCCDParams(1280, 1024, 8, 5.4, 5.4);

    // Let's calculate how much memory we need for the primary CCD buffer
    int nbuf;
    nbuf=PrimaryCCD.getXRes()*PrimaryCCD.getYRes() * PrimaryCCD.getBPP()/8;
    nbuf+=512;                      //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool SimpleCCD::StartExposure(float duration)
{
    ExposureRequest=duration;

    // Since we have only have one CCD with one chip, we set the exposure duration of the primary CCD
    PrimaryCCD.setExposureDuration(duration);

    gettimeofday(&ExpStart,NULL);

    InExposure=true;

    // We're done
    return true;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool SimpleCCD::AbortExposure()
{
    InExposure = false;
    return true;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float SimpleCCD::CalcTimeLeft()
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
bool SimpleCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INumber *np;

    if(strcmp(dev,getDeviceName())==0)
    {

        /* Temperature*/
        if (!strcmp(TemperatureNP.name, name))
        {
            TemperatureNP.s = IPS_IDLE;

            // Let's find the temperature value
            np = IUFindNumber(&TemperatureNP, names[0]);

            // If it doesn't exist...
            if (np == NULL)
            {
                IDSetNumber(&TemperatureNP, "Unknown error. %s is not a member of %s property.", names[0], name);
                return false;
            }

            // If it's out of range ...
            if (values[0] < MIN_CCD_TEMP || values[0] > MAX_CCD_TEMP)
            {
                IDSetNumber(&TemperatureNP, "Error: valid range of temperature is from %d to %d", MIN_CCD_TEMP, MAX_CCD_TEMP);
                return false;
            }

            // All OK, let's set the requested temperature
            TemperatureRequest = values[0];
            TemperatureNP.s = IPS_BUSY;

            IDSetNumber(&TemperatureNP, "Setting CCD temperature to %+06.2f C", values[0]);
            return true;
        }
    }

    // If we didn't process anything above, let the parent handle it.
    return INDI::CCD::ISNewNumber(dev,name,values,names,n);
}

/**************************************************************************************
** INDI is asking us to add any FITS keywords to the FITS header
***************************************************************************************/
void SimpleCCD::addFITSKeywords(fitsfile *fptr, CCDChip *targetChip)
{
    // Let's first add parent keywords
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    // Add temperature to FITS header
    int status=0;
    fits_update_key_s(fptr, TDOUBLE, "CCD-TEMP", &(TemperatureN[0].value), "CCD Temperature (Celcius)", &status);
    fits_write_date(fptr, &status);

}

/**************************************************************************************
** Main device loop. We check for exposure and temperature progress here
***************************************************************************************/
void SimpleCCD::TimerHit()
{
    long timeleft;

    if(isConnected() == false)
        return;  //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        timeleft=CalcTimeLeft();

        // Less than a 0.1 second away from exposure completion
        // This is an over simplified timing method, check CCDSimulator and simpleCCD for better timing checks
        if(timeleft < 0.1)
        {
          /* We're done exposing */
           IDMessage(getDeviceName(), "Exposure done, downloading image...");

          // Set exposure left to zero
          PrimaryCCD.setExposureLeft(0);

          // We're no longer exposing...
          InExposure = false;

          /* grab and save image */
          grabImage();

        }
        else
         // Just update time left in client
         PrimaryCCD.setExposureLeft(timeleft);

    }

    switch (TemperatureNP.s)
    {
      case IPS_IDLE:
      case IPS_OK:
        break;

      case IPS_BUSY:
        /* If target temperature is higher, then increase current CCD temperature */
        if (currentCCDTemperature < TemperatureRequest)
           currentCCDTemperature++;
        /* If target temperature is lower, then decrese current CCD temperature */
        else if (currentCCDTemperature > TemperatureRequest)
          currentCCDTemperature--;
        /* If they're equal, stop updating */
        else
        {
          TemperatureNP.s = IPS_OK;
          IDSetNumber(&TemperatureNP, "Target temperature reached.");

          break;
        }

        IDSetNumber(&TemperatureNP, NULL);

        break;

      case IPS_ALERT:
        break;
    }

    SetTimer(POLLMS);
    return;
}

/**************************************************************************************
** Create a random image and return it to client
***************************************************************************************/
void SimpleCCD::grabImage()
{
   // Let's get a pointer to the frame buffer
   char * image = PrimaryCCD.getFrameBuffer();

   // Get width and height
   int width = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP()/8;
   int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

   // Fill buffer with random pattern
   for (int i=0; i < height ; i++)
     for (int j=0; j < width; j++)
         image[i*width+j] = rand() % 255;

   IDMessage(getDeviceName(), "Download complete.");

   // Let INDI::CCD know we're done filling the image buffer
   ExposureComplete(&PrimaryCCD);
}
