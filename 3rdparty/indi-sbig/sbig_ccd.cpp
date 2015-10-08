/*
    Driver type: SBIG CCD Camera INDI Driver

    Copyright (C) 2013-2014 Jasem Mutlaq (mutlaqja AT ikarustech DOT com)
    Copyright (C) 2005-2006 Jan Soldan (jsoldan AT asu DOT cas DOT cz)

    Acknowledgement:
    Matt Longmire 	(matto AT sbig DOT com)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
    License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

 */

#include <memory>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include "indidevapi.h"
#include "eventloop.h"
#include <indilogger.h>
#include <ccvt.h>

#include "sbig_ccd.h"

#define TEMPERATURE_POLL_MS 5000    /* Temperature Polling time (ms) */
#define MAX_RESOLUTION      4096    /* Maximum resolutoin for secondary chip */
#define POLLMS          	1000	/* Polling time (ms) */
#define MAX_DEVICES         20      /* Max device cameraCount */
#define MAX_THREAD_RETRIES  3
#define MAX_THREAD_WAIT     300000

static int cameraCount;
static SBIGCCD *cameras[MAX_DEVICES];
pthread_cond_t      cv  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     condMutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t     sbigMutex = PTHREAD_MUTEX_INITIALIZER;

/**********************************************************
 *
 *  IMPORRANT: List supported camera models in initializer of deviceTypes structure
 *
 **********************************************************/

static void cleanup() {
  for (int i = 0; i < cameraCount; i++) {
    delete cameras[i];
  }
}

void ISInit()
{
  static bool isInit = false;
  if (!isInit)
  {

      // Let's just create one camera for now
     cameraCount = 1;
     cameras[0] = new SBIGCCD();

    atexit(cleanup);
    isInit = true;
  }
}

void ISGetProperties(const char *dev) {
  ISInit();
  for (int i = 0; i < cameraCount; i++) {
    SBIGCCD *camera = cameras[i];
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
    SBIGCCD *camera = cameras[i];
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
    SBIGCCD *camera = cameras[i];
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
    SBIGCCD *camera = cameras[i];
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
void ISSnoopDevice(XMLEle *root)
{
    ISInit();

    for (int i = 0; i < cameraCount; i++)
    {
      SBIGCCD *camera = cameras[i];
      camera->ISSnoopDevice(root);
    }
}

//==========================================================================
SBIGCCD::SBIGCCD()
{
    InitVars();
    int res = OpenDriver();
    if (res != CE_NO_ERROR)
        DEBUGF(INDI::Logger::DBG_DEBUG, "%s: Error (%s)", __FUNCTION__, GetErrorString(res).c_str());

    // For now let's set name to default name. In the future, we need to to support multiple devices per one driver
    if (*getDeviceName() == '\0')
        strncpy(name, getDefaultName(), MAXINDINAME);
    else
        strncpy(name, getDeviceName(), MAXINDINAME);

    isColor = false;
    useExternalTrackingCCD=false;
    grabPredicate = GRAB_NO_CCD;
    terminateThread= false;
    hasGuideHead=false;
    hasFilterWheel=false;

    setVersion(1, 6);

}
//==========================================================================
SBIGCCD::SBIGCCD(const char* devName)
{
    int res = CE_NO_ERROR;
    InitVars();
    if( (res = OpenDriver()) == CE_NO_ERROR)
        OpenDevice(devName);

    if (res != CE_NO_ERROR)
        DEBUGF(INDI::Logger::DBG_DEBUG, "%s: Error (%s)", __FUNCTION__, GetErrorString(res).c_str());
}
//==========================================================================
SBIGCCD::~SBIGCCD()
{
    CloseDevice();
    CloseDriver();
}
//==========================================================================
int SBIGCCD::OpenDriver()
{
    int res;
    GetDriverHandleResults gdhr;
    SetDriverHandleParams  sdhp;

    // Call the driver directly.
    if((res = ::SBIGUnivDrvCommand(CC_OPEN_DRIVER, 0, 0)) == CE_NO_ERROR)
    {
        // The driver was not open, so record the driver handle.
        res = ::SBIGUnivDrvCommand(CC_GET_DRIVER_HANDLE, 0, &gdhr);
    }else if(res == CE_DRIVER_NOT_CLOSED)
    {
        // The driver is already open which we interpret as having been
        // opened by another instance of the class so get the driver to
        // allocate a new handle and then record it.
        sdhp.handle = INVALID_HANDLE_VALUE;
        res = ::SBIGUnivDrvCommand(CC_SET_DRIVER_HANDLE, &sdhp, 0);
        if(res == CE_NO_ERROR)
        {
                if((res = ::SBIGUnivDrvCommand(CC_OPEN_DRIVER, 0, 0)) == CE_NO_ERROR)
                {
                        res = ::SBIGUnivDrvCommand(CC_GET_DRIVER_HANDLE, 0, &gdhr);
                }
        }
    }
    if(res == CE_NO_ERROR) SetDriverHandle(gdhr.handle);
    return(res);
}
//==========================================================================
int SBIGCCD::CloseDriver()
{
    int res;

    if((res = ::SBIGUnivDrvCommand(CC_CLOSE_DRIVER, 0, 0)) == CE_NO_ERROR){
            SetDriverHandle();
    }

    if (res != CE_NO_ERROR)
        DEBUGF(INDI::Logger::DBG_DEBUG, "%s: Error (%s)", __FUNCTION__, GetErrorString(res).c_str());

    return(res);
}
//==========================================================================
int SBIGCCD::OpenDevice(const char *devName)
{
    int res;
    OpenDeviceParams odp;

    // Check if device already opened:
    if(IsDeviceOpen()) return(CE_NO_ERROR);

    // Try to open new device:
    if(strcmp(devName, SBIG_USB0) == 0){
            odp.deviceType = DEV_USB1;
    }else if(strcmp(devName, SBIG_USB1) == 0){
            odp.deviceType = DEV_USB2;
    }else if(strcmp(devName, SBIG_USB2) == 0){
            odp.deviceType = DEV_USB3;
    }else if(strcmp(devName, SBIG_USB3) == 0){
            odp.deviceType = DEV_USB4;
    }else if(strcmp(devName, SBIG_LPT0) == 0){
            odp.deviceType = DEV_LPT1;
    }else if(strcmp(devName, SBIG_LPT1) == 0){
            odp.deviceType = DEV_LPT2;
    }else if(strcmp(devName, SBIG_LPT2) == 0){
            odp.deviceType = DEV_LPT3;
    }else{
            return(CE_BAD_PARAMETER);
    }

    if((res = SBIGUnivDrvCommand(CC_OPEN_DEVICE, &odp, 0)) == CE_NO_ERROR){
            SetDeviceName(devName);
            SetFileDescriptor(true);
    }

    if (res != CE_NO_ERROR)
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Error opening device %s (%s)", __FUNCTION__, devName, GetErrorString(res).c_str());

    return(res);
}
//==========================================================================
int SBIGCCD::CloseDevice()
{
    int res = CE_NO_ERROR;

    if (sim)
        return res;

    if(IsDeviceOpen())
    {
        if((res = SBIGUnivDrvCommand(CC_CLOSE_DEVICE, 0, 0)) == CE_NO_ERROR)
        {
                SetFileDescriptor();	// set value to -1
                SetCameraType(); 		// set value to NO_CAMERA
        }
    }

    if (res != CE_NO_ERROR)
        DEBUGF(INDI::Logger::DBG_DEBUG, "%s: Error (%s)", __FUNCTION__, GetErrorString(res).c_str());

    return(res);
}

const char * SBIGCCD::getDefaultName()
{
    return (const char *)"SBIG CCD";
}

bool SBIGCCD::initProperties()
{
  // Init parent properties first
  INDI::CCD::initProperties();

  // CCD PRODUCT:
  IUFillText(&ProductInfoT[0], "NAME", "Name", "");
  IUFillText(&ProductInfoT[1], "ID", "ID", "");
  IUFillTextVector(	&ProductInfoTP, ProductInfoT, 2, getDeviceName(), "CCD_PRODUCT", "Product", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

  // CCD DEVICE PORT:
  IUFillText(&PortT[0], "PORT", "Port", SBIG_USB0);
  IUFillTextVector(	&PortTP, PortT, 1, getDeviceName(), "DEVICE_PORT", "Port", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

  // CCD FAN STATE:
  IUFillSwitch(&FanStateS[0], "ON", "On", ISS_ON);
  IUFillSwitch(&FanStateS[1], "OFF", "Off", ISS_OFF);
  IUFillSwitchVector(&FanStateSP, FanStateS, 2, getDeviceName(), "CCD_FAN", "Fan", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

  // CCD Cooler Switch
  IUFillSwitch(&CoolerS[0], "ON", "On", ISS_OFF);
  IUFillSwitch(&CoolerS[1], "OFF", "Off", ISS_ON);
  IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_REGULATION", "Cooler", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);


  // CCD COOLER:
  IUFillNumber(	&CoolerN[0], "COOLER","[%]", "%.1f", 0, 0, 0, 0);
  IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER","Cooler %", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

  // CFW PRODUCT
  IUFillText(&FilterProdcutT[0], "NAME", "Name", "");
  IUFillText(&FilterProdcutT[1], "ID", "ID", "");
  IUFillTextVector(	&FilterProdcutTP, FilterProdcutT, 2, getDeviceName(), "CFW_PRODUCT" , "Product", FILTER_TAB, IP_RO, 0, IPS_IDLE);


  // CFW_MODEL:
  IUFillSwitch(&FilterTypeS[0], "CFW1", "CFW-2", ISS_OFF);
  IUFillSwitch(&FilterTypeS[1],"CFW2" , "CFW-5", ISS_OFF);
  IUFillSwitch(&FilterTypeS[2], "CFW3","CFW-6A" , ISS_OFF);
  IUFillSwitch(&FilterTypeS[3], "CFW4", "CFW-8", ISS_OFF);
  IUFillSwitch(&FilterTypeS[4], "CFW5", "CFW-402", ISS_OFF);
  IUFillSwitch(&FilterTypeS[5], "CFW6", "CFW-10", ISS_OFF);
  IUFillSwitch(&FilterTypeS[6], "CFW7", "CFW-10 SA", ISS_OFF);
  IUFillSwitch(&FilterTypeS[7], "CFW8", "CFW-L", ISS_OFF);
  IUFillSwitch(&FilterTypeS[8], "CFW9", "CFW-9", ISS_OFF);  
  IUFillSwitch(&FilterTypeS[9], "CFW10", "CFW-8LG", ISS_OFF);
  IUFillSwitch(&FilterTypeS[10], "CFW11", "CFW-1603", ISS_OFF);
  IUFillSwitch(&FilterTypeS[11], "CFW12", "CFW-FW5-STX", ISS_OFF);
  IUFillSwitch(&FilterTypeS[12], "CFW13", "CFW-FW5-8300", ISS_OFF);
  IUFillSwitch(&FilterTypeS[13], "CFW14", "CFW-FW8-8300", ISS_OFF);
  IUFillSwitch(&FilterTypeS[14], "CFW15", "CFW-FW7-STX", ISS_OFF);
  IUFillSwitch(&FilterTypeS[15], "CFW16", "CFW-FW8-STT", ISS_OFF);
  #ifdef	 USE_CFW_AUTO
  IUFillSwitch(&FilterTypeS[16], "CFW17", "CFW-Auto", ISS_OFF);
  #endif
  IUFillSwitchVector(&FilterTypeSP, FilterTypeS, MAX_CFW_TYPES, getDeviceName(),"CFW_TYPE", "Type", FILTER_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

  // CFW CONNECTION:
  IUFillSwitch(&FilterConnectionS[0], "CONNECT", "Connect", ISS_OFF);
  IUFillSwitch(&FilterConnectionS[1], "DISCONNECT", "Disconnect", ISS_ON);
  IUFillSwitchVector(&FilterConnectionSP, FilterConnectionS, 2, getDeviceName(), "CFW_CONNECTION", "Connect", FILTER_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  IUSaveText(&BayerT[2], "BGGR");

  initFilterProperties(getDeviceName(), FILTER_TAB);

  FilterSlotN[0].min = 1;
  FilterSlotN[0].max = MAX_CFW_TYPES;

  setDriverInterface(getDriverInterface() | FILTER_INTERFACE);

  return true;
}

void SBIGCCD::ISGetProperties(const char *dev)
{
  INDI::CCD::ISGetProperties(dev);

  defineText(&PortTP);
  loadConfig(true, "DEVICE_PORT");

  // Add Debug, Simulator, and Configuration controls
  addAuxControls();
}

bool SBIGCCD::updateProperties()
{

  INDI::CCD::updateProperties();

  if (isConnected())
  {
      if (IsFanControlAvailable())
          defineSwitch(&FanStateSP);

    if (HasCooler())
    {
        defineSwitch(&CoolerSP);
        defineNumber(&CoolerNP);
    }

    if (hasFilterWheel)
    {
        defineSwitch(&FilterConnectionSP);
        defineSwitch(&FilterTypeSP);
        defineText(&ProductInfoTP);
        defineText(&FilterProdcutTP);
    }

    // Let's get parameters now from CCD
    setupParams();

    // If filter type already selected (from config file), then try to connect to CFW
    if (hasFilterWheel)
    {
        loadConfig(true, "CFW_TYPE");

        ISwitch *p = IUFindOnSwitch(&FilterTypeSP);
        if (p != NULL && FilterConnectionS[0].s == ISS_OFF)
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Filter type is already selected and filter is not connected. Will attempt to connect to filter now...");
            CFWConnect();
        }
    }

    timerID = SetTimer(POLLMS);
  } else
  {
    deleteProperty(CoolerSP.name);
    deleteProperty(CoolerNP.name);
    deleteProperty(ProductInfoTP.name);

    deleteProperty(FanStateSP.name);

    if (hasFilterWheel)
    {
        deleteProperty(FilterConnectionSP.name);
        deleteProperty(FilterTypeSP.name);
        deleteProperty(FilterProdcutTP.name);
        if (FilterNameT != NULL)
            deleteProperty(FilterNameTP->name);
    }

    rmTimer(timerID);
  }

  return true;
}

bool SBIGCCD::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if(strcmp(name,PortTP.name)==0)
        {
            int count = sizeof(SBIG_DEVICE_PORTS)/sizeof(SBIG_USB0);
            int i=0;
            for (i=0; i < count; i++)
                if (!strcmp(texts[0], SBIG_DEVICE_PORTS[i]))
                    break;

            if (i == count)
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "Invalid port %s. Valid ports are sbigusb0, sbigusb1..etc, sbiglpt0, sbiglpt1..etc", texts[0]);
                PortTP.s=IPS_ALERT;
                IDSetText(&PortTP, NULL);
                return false;
            }

            PortTP.s=IPS_OK;
            IUUpdateText(&PortTP,texts,names,n);
            IDSetText(&PortTP, NULL);
            return true;
        }

        if (strcmp(name, FilterNameTP->name) == 0)
        {
            processFilterName(dev, texts, names, n);
            return true;
        }

    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

bool SBIGCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
  int res=CE_NO_ERROR;
  string str;

  if (strcmp(dev, getDeviceName()) == 0)
  {
    if(!strcmp(name, FanStateSP.name))
    {
       IUResetSwitch(&FanStateSP);
       IUUpdateSwitch(&FanStateSP, states, names, n);
       // Switch FAN ON/OFF:
       MiscellaneousControlParams mcp;

      if(FanStateS[0].s == ISS_ON)
      {
              mcp.fanEnable = 1;
      }else{
              mcp.fanEnable = 0;
      }

      mcp.shutterCommand = SC_LEAVE_SHUTTER;
      mcp.ledState  = LED_OFF;

      if((res = MiscellaneousControl(&mcp)) == CE_NO_ERROR)
      {
              FanStateSP.s = IPS_OK;
              if(mcp.fanEnable == 1)
              {
                      str = "Fan turned ON.";
              }else
              {
                      str = "Fan turned OFF.";
              }
      }
      else
      {
              FanStateSP.s = IPS_ALERT;
              if(mcp.fanEnable == 1){
                      str = "Error: Cannot turn Fan ON. ";
              }else{
                      str = "Error: Cannot turn Fan OFF.";
              }
              str += GetErrorString(res);

              DEBUGF(INDI::Logger::DBG_ERROR, "%s", str.c_str());
              IDSetSwitch(&FanStateSP, NULL);
              return false;
      }

            DEBUGF(INDI::Logger::DBG_SESSION, "%s", str.c_str());
            IDSetSwitch(&FanStateSP, NULL);
            return true;
      }

        // CFW TYPE:
        if(!strcmp(name, FilterTypeSP.name))
        {
          IUResetSwitch(&FilterTypeSP);
          IUUpdateSwitch(&FilterTypeSP, states, names, n);
          FilterTypeSP.s = IPS_OK;
          IDSetSwitch(&FilterTypeSP, NULL);

          return true;
        }

      if (!strcmp(name, CoolerSP.name))
      {
          IUUpdateSwitch(&CoolerSP, states, names, n);

          if ( (res = SetTemperatureRegulation(TemperatureN[0].value, CoolerS[0].s == ISS_ON)) == CE_NO_ERROR)
            CoolerSP.s = (CoolerS[0].s == ISS_ON) ? IPS_BUSY : IPS_IDLE;
          else
          {
              DEBUGF(INDI::Logger::DBG_ERROR, "Setting temperature regulation failed (%s).", GetErrorString(res).c_str());
              CoolerSP.s = IPS_ALERT;
          }

          IDSetSwitch(&CoolerSP, NULL);
          return true;
      }

        // CFW CONNECTION:
      if(!strcmp(name, FilterConnectionSP.name))
      {
            IUUpdateSwitch(&FilterConnectionSP, states, names, n);
            FilterConnectionSP.s = IPS_BUSY;

            if(FilterConnectionS[0].s == ISS_ON)
            {

                ISwitch *p = IUFindOnSwitch(&FilterTypeSP);
                if (p == NULL)
                {

                    FilterConnectionSP.s = IPS_IDLE;
                    IUResetSwitch(&FilterConnectionSP);
                    FilterConnectionSP.sp[1].s = ISS_ON;
                    DEBUG(INDI::Logger::DBG_WARNING, "Please select filter type before connecting.");
                    IDSetSwitch(&FilterConnectionSP, NULL);
                    return false;
                }

                // Connect CFW
                CFWConnect();
            }
            else
            {
                // Disconnect CFW
                CFWDisconnect();
            }
            return true;
        }

  }

  //  Nobody has claimed this, so, ignore it
  return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool SBIGCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{

  if (strcmp(dev, getDeviceName()) == 0)
  {

     if (strcmp(name, FilterSlotNP.name)==0)
      {
          processFilterSlot(dev, values, names);
          return true;
      }

  }

  //  if we didn't process it, continue up the chain, let somebody else
  //  give it a shot
  return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool SBIGCCD::Connect()
{

  sim = isSimulation();
  int res= CE_NO_ERROR;
  string str;
  hasGuideHead = false;
  hasFilterWheel=false;

  if (sim)
  {              
    GetExtendedCCDInfo();

    SetCCDCapability(CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_COOLER | CCD_HAS_GUIDE_HEAD | CCD_HAS_SHUTTER | CCD_HAS_ST4_PORT);

    pthread_create( &primary_thread, NULL, &grabCCDHelper, this);

    IEAddTimer(TEMPERATURE_POLL_MS, SBIGCCD::updateTemperatureHelper, this);

    return true;
  }

  // Open device:
  if((res = OpenDevice(PortTP.tp->text)) == CE_NO_ERROR)
  {
          // Establish link:
          if((res = EstablishLink()) == CE_NO_ERROR)
          {
                  // Link established.
              DEBUG(INDI::Logger::DBG_SESSION, "SBIG CCD is online. Retrieving basic data.");

              bool hasCooler = false;
              hasCooler = (GetCameraType() != STI_CAMERA);

              if (hasCooler)
                  IEAddTimer(TEMPERATURE_POLL_MS, SBIGCCD::updateTemperatureHelper, this);

              GetExtendedCCDInfo();

              uint32_t cap= CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_SHUTTER | CCD_HAS_ST4_PORT;

              if (hasCooler)
                  cap |= CCD_HAS_COOLER;

              if (hasGuideHead)
                  cap |= CCD_HAS_GUIDE_HEAD;

              if (isColor)
                  cap |= CCD_HAS_BAYER;


              SetCCDCapability(cap);

              pthread_create( &primary_thread, NULL, &grabCCDHelper, this);
              return true;
          }
          else
          {
                  // Establish link error.
                  str = "Error: Cannot establish link to SBIG CCD camera at port ";
                  str += string(PortTP.tp->text) + " ";
                  str += GetErrorString(res);
                  DEBUGF(INDI::Logger::DBG_ERROR, "%s", str.c_str());
                  return false;

          }
  }
  else
  {
          // Open device error.
          str = "Error: Cannot open SBIG CCD camera device at port ";
          str += string(PortTP.tp->text) + " ";
          str += GetErrorString(res);

          DEBUGF(INDI::Logger::DBG_ERROR, "%s", str.c_str());

          return false;
  }

  return false;
}

bool SBIGCCD::Disconnect()
{

  pthread_mutex_lock(&condMutex);
  grabPredicate=GRAB_PRIMARY_CCD;
  terminateThread=true;
  useExternalTrackingCCD=false;
  hasGuideHead=false;
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&condMutex);

  int res=CE_NO_ERROR;
  string str;

  if (FilterConnectionS[0].s == ISS_ON)
      CFWDisconnect();

  // Close device.
  if((res = CloseDevice()) == CE_NO_ERROR)
  {
        DEBUG(INDI::Logger::DBG_SESSION, "SBIG CCD is offline.");
        return true;
  }
  else
  {
          // Close device error:
          str = "Error: Cannot close SBIG CCD camera device. ";
          str += GetErrorString(res);
          DEBUGF(INDI::Logger::DBG_ERROR, "%s", str.c_str());
  }

  return false;

}

bool SBIGCCD::setupParams()
{

  DEBUG(INDI::Logger::DBG_DEBUG, "Retrieving CCD Parameters...");

  float x_pixel_size, y_pixel_size;
  int bit_depth = 16;
  int x_1, y_1, x_2, y_2;

  int 		res = CE_NO_ERROR, wCcd=0, hCcd=0, binning=0;
  double	wPixel=0, hPixel=0;


  if ( (res = getBinningMode(&PrimaryCCD, binning)) != CE_NO_ERROR)
  {
      return false;
  }

  if ( (res = getCCDSizeInfo(CCD_IMAGING, binning, wCcd, hCcd, wPixel, hPixel)) != CE_NO_ERROR)
  {
      DEBUGF(INDI::Logger::DBG_ERROR, "Error getting CCD Size info. %s", GetErrorString(res).c_str());
      return false;
  }

  if(res == CE_NO_ERROR)
  {
      // CCD INFO:
      x_pixel_size  = wPixel;
      y_pixel_size  = hPixel;

      x_1 = y_1 = 0;
      x_2 = wCcd;
      y_2 = hCcd;
   }

    SetCCDParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);

    if (HasGuideHead())
    {
        if ( (res = getBinningMode(&GuideCCD, binning)) != CE_NO_ERROR)
        {
            return false;
        }

        if ( (res = getCCDSizeInfo(useExternalTrackingCCD ? CCD_EXT_TRACKING : CCD_TRACKING, binning, wCcd, hCcd, wPixel, hPixel)) != CE_NO_ERROR)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error getting CCD Size info. %s", GetErrorString(res).c_str());
            return false;
        }

        if(res == CE_NO_ERROR)
        {
            if (useExternalTrackingCCD && (wCcd <= 0 || hCcd <=0 || wCcd > MAX_RESOLUTION || hCcd > MAX_RESOLUTION))
            {
                DEBUG(INDI::Logger::DBG_DEBUG, "Invalid external tracking CCD dimensions, trying regular CCD_TRACKING");

                if ( (res = getCCDSizeInfo(CCD_TRACKING, binning, wCcd, hCcd, wPixel, hPixel)) != CE_NO_ERROR)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR, "Error getting Tracking CCD Size info. %s", GetErrorString(res).c_str());
                    return false;
                }

                useExternalTrackingCCD = false;
            }

            // CCD INFO:
            x_pixel_size  = wPixel;
            y_pixel_size  = hPixel;

            x_1 = y_1 = 0;
            x_2 = wCcd;
            y_2 = hCcd;
         }

          SetGuiderParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);
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

  if (PrimaryCCD.getFrameBuffer() == NULL)
        DEBUG(INDI::Logger::DBG_WARNING, "Unable to allocate memory for CCD Chip buffer!");

  DEBUGF(INDI::Logger::DBG_DEBUG, "Created Primary CCD buffer %d bytes.", nbuf);

  if (HasGuideHead())
  {
      nbuf = GuideCCD.getXRes() * GuideCCD.getYRes() * GuideCCD.getBPP() / 8;    //  this is pixel cameraCount
      nbuf += 512;    //  leave a little extra at the end
      GuideCCD.setFrameBufferSize(nbuf);

      DEBUGF(INDI::Logger::DBG_DEBUG, "Created Guide Head CCD buffer %d bytes.", nbuf);
  }

  // CCD Cooler
  if (HasCooler())
  {
    bool regulationEnabled = false;
    double temp, setPoint,power;
    QueryTemperatureStatus(regulationEnabled, temp, setPoint, power);
    CoolerS[0].s = regulationEnabled ? ISS_ON : ISS_OFF;
    CoolerS[1].s = regulationEnabled ? ISS_OFF: ISS_ON;
    IDSetSwitch(&CoolerSP, NULL);

    CoolerN[0].value = power * 100;
    IDSetNumber(&CoolerNP, NULL);

    // Update CCD Temperature Min & Max limits
    TemperatureN[0].min = MIN_CCD_TEMP;
    TemperatureN[0].max = MAX_CCD_TEMP;
    IUUpdateMinMax(&TemperatureNP);
  }

  // CCD PRODUCT:
  IText *pIText;
  string msg;
  msg = GetCameraName();
  pIText = IUFindText(&ProductInfoTP, "NAME");
  if(pIText) IUSaveText(pIText, msg.c_str());
  msg = GetCameraID();
  pIText = IUFindText(&ProductInfoTP, "ID");
  if(pIText) IUSaveText(pIText, msg.c_str());
  ProductInfoTP.s = IPS_OK;
  IDSetText(&ProductInfoTP, NULL);

  return true;

}

int SBIGCCD::SetTemperature(double temperature)
{
    int res=CE_NO_ERROR;

    if (fabs(temperature - TemperatureN[0].value) < 0.1)
        return 1;

    if((res = SetTemperatureRegulation(temperature)) == CE_NO_ERROR)
    {
        // Set property to busy and poll in ISPoll for CCD temp
        //TemperatureN[0].value = values[0];
        TemperatureRequest = temperature;
        DEBUGF(INDI::Logger::DBG_SESSION, "Setting CCD temperature to %+.1f [C].",temperature);

        if (CoolerS[0].s != ISS_ON)
        {
            CoolerS[0].s = ISS_ON;
            CoolerS[1].s = ISS_OFF;
            CoolerSP.s = IPS_BUSY;
            IDSetSwitch(&CoolerSP, NULL);
        }
        return 0;
    }else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error: Cannot set CCD temperature to %+.1f [C]. %s",
               temperature, GetErrorString(res).c_str());
        return -1;
    }

    return -1;

}

int SBIGCCD::StartExposure(CCDChip *targetChip, double duration)
{
    int  res = CE_NO_ERROR;

    // Sanity check:
    int  binning, shutter;
    if((res = getShutterMode(targetChip, shutter)) != CE_NO_ERROR) return(res);
    if((res = getBinningMode(targetChip, binning)) != CE_NO_ERROR) return(res);

    // Is the expose time zero ?
    if(duration == 0)
    {
            DEBUG(INDI::Logger::DBG_ERROR, "Please set non-zero exposure time and try again.");
            return(CE_BAD_PARAMETER);
    }

      // Calculate an expose time:;
      ulong expTime = (ulong)floor(duration * 100.0 + 0.5);

    // Get image size:
    unsigned short left	= (unsigned short)targetChip->getSubX();
    unsigned short top	= (unsigned short)targetChip->getSubY();
    unsigned short width	= (unsigned short)targetChip->getSubW()/targetChip->getBinX();
    unsigned short height	= (unsigned short)targetChip->getSubH()/targetChip->getBinY();

    int ccd;
    if (targetChip == &PrimaryCCD)
        ccd = CCD_IMAGING;
    else
        ccd = useExternalTrackingCCD ? CCD_EXT_TRACKING : CCD_TRACKING;

      // Start exposure:
      StartExposureParams2	sep;
      sep.ccd = (unsigned short)ccd;
      sep.abgState = (unsigned short)ABG_LOW7;
      sep.openShutter = (unsigned short)shutter;
      sep.exposureTime = expTime;
      sep.readoutMode = binning;
      sep.left        = left;
      sep.top         = top;
      sep.width       = width;
      sep.height      = height;

      DEBUGF(INDI::Logger::DBG_DEBUG, "Exposure Params. CCD (%d) openShutter(%d), exposureTime(%ld), binnig (%d), left (%d), top (%d), w(%d), h(%d)", sep.ccd, sep.openShutter,
             sep.exposureTime,  sep.readoutMode, sep.left , sep.top , sep.width , sep.height);

    for (int i=0; i < MAX_THREAD_RETRIES; i++)
    {
        pthread_mutex_lock(&sbigMutex);
        res = StartExposure(&sep);
        pthread_mutex_unlock(&sbigMutex);
        if(res == CE_NO_ERROR)
        {
                targetChip->setExposureDuration(duration);
                break;
        }

        usleep(MAX_THREAD_WAIT);
    }

    if (res != CE_NO_ERROR)
        return(res);

    string msg, frame_type;
    if((res = getFrameType(targetChip, frame_type)) != CE_NO_ERROR) return(res);
    if(!strcmp(frame_type.c_str(), "FRAME_LIGHT")){
            msg = "Light Frame exposure in progress...";
    }else if(!strcmp(frame_type.c_str(), "FRAME_DARK")){
            msg = "Dark Frame exposure in progress...";
    }else if(!strcmp(frame_type.c_str(), "FRAME_FLAT")){
            msg = "Flat Frame exposure in progress...";
    }else if(!strcmp(frame_type.c_str(), "FRAME_BIAS")){
            msg = "Bias Frame exposure in progress...";
    }
    DEBUGF(INDI::Logger::DBG_DEBUG, "%s", msg.c_str());

    return (res);

}

bool SBIGCCD::StartExposure(float duration)
{

    if (!sim)
    {
        int res = StartExposure(&PrimaryCCD, duration);
        if (res != CE_NO_ERROR)
            return false;
    }

  ExposureRequest = duration;

  DEBUGF(INDI::Logger::DBG_DEBUG, "Primary CCD Exposure Time (s) is: %g", duration);
  if (ExposureRequest >= 5)
    DEBUGF(INDI::Logger::DBG_SESSION, "Taking a %g seconds frame...", ExposureRequest);
  gettimeofday(&ExpStart, NULL);

  InExposure = true;

  return true;

}

bool SBIGCCD::StartGuideExposure(float duration)
{
    if (!sim)
    {
        int res = StartExposure(&GuideCCD, duration);
        if (res != CE_NO_ERROR)
            return false;
    }

    GuideExposureRequest = duration;
    DEBUGF(INDI::Logger::DBG_DEBUG, "Guide Exposure Time (s) is: %g", duration);

    gettimeofday(&GuideExpStart, NULL);

    InGuideExposure = true;

    return true;

}

int SBIGCCD::StopExposure(CCDChip *targetChip)
{
    int 		res = CE_NO_ERROR, ccd;

    if (sim)
        return res;

    if (targetChip == &PrimaryCCD)
        ccd = CCD_IMAGING;
    else
        ccd = useExternalTrackingCCD ? CCD_EXT_TRACKING : CCD_TRACKING;

    // END_EXPOSURE:
    EndExposureParams eep;
    eep.ccd = (unsigned short)ccd;

    pthread_mutex_lock(&sbigMutex);
    res = EndExposure(&eep);    
    pthread_mutex_unlock(&sbigMutex);

    return(res);
}

bool SBIGCCD::AbortExposure()
{
   int res;

   DEBUG(INDI::Logger::DBG_DEBUG, "Aborting Primary CCD Exposure...");

   for (int i=0; i < MAX_THREAD_RETRIES; i++)
   {
        if ( (res = StopExposure(&PrimaryCCD)) == CE_NO_ERROR)
            break;

        usleep(MAX_THREAD_WAIT);
   }

  if(res == CE_NO_ERROR)
  {
      InExposure = false;
      DEBUG(INDI::Logger::DBG_DEBUG, "Exposure cancelled.");
      return true;
  }
  else
  {
      DEBUGF(INDI::Logger::DBG_ERROR, "Stop exposure error. %s", GetErrorString(res).c_str());
      return false;
  }
}

bool SBIGCCD::AbortGuideExposure()
{
   int res;

   DEBUG(INDI::Logger::DBG_DEBUG, "Aborting Guide Head Exposure...");

   for (int i=0; i < MAX_THREAD_RETRIES; i++)
   {
        if ( (res = StopExposure(&GuideCCD)) == CE_NO_ERROR)
            break;

        usleep(MAX_THREAD_WAIT);
   }

  if(res == CE_NO_ERROR)
  {
      InGuideExposure = false;
      DEBUG(INDI::Logger::DBG_DEBUG, "Exposure cancelled.");
      return true;
  }
  else
  {
      DEBUGF(INDI::Logger::DBG_ERROR, "Stop exposure error. %s", GetErrorString(res).c_str());
      return false;
  }
}

bool SBIGCCD::UpdateCCDFrameType(CCDChip::CCD_FRAME fType)
{
  CCDChip::CCD_FRAME imageFrameType = PrimaryCCD.getFrameType();

  if (fType == imageFrameType || sim)
    return true;

  PrimaryCCD.setFrameType(fType);

  return true;

}

bool SBIGCCD::updateFrameProperties(CCDChip *targetChip)
{
    int 	res = CE_NO_ERROR, wCcd, hCcd, binning;
    double	wPixel, hPixel;

    if((res = getBinningMode(targetChip, binning)) != CE_NO_ERROR)
        return false;

    if (targetChip == &PrimaryCCD)
        res = getCCDSizeInfo(CCD_IMAGING, binning, wCcd, hCcd, wPixel, hPixel);
    else
        res = getCCDSizeInfo(useExternalTrackingCCD ? CCD_EXT_TRACKING : CCD_TRACKING, binning, wCcd, hCcd, wPixel, hPixel);


    if(res == CE_NO_ERROR)
    {
        // SBIG returns binned width and height, which is OK, but we used unbinned width and height across
        // all drivers to be consistent.
        wCcd *= targetChip->getBinX();
        hCcd *= targetChip->getBinY();

        targetChip->setResolution(wCcd, hCcd);

        if (targetChip == &PrimaryCCD)
            UpdateCCDFrame(0, 0, wCcd, hCcd);
        else
            UpdateGuiderFrame(0,0, wCcd, hCcd);

        return true;
    }
    else
       DEBUGF(INDI::Logger::DBG_DEBUG, "%s: Error (%s)", __FUNCTION__, GetErrorString(res).c_str());

    return false;
}

bool SBIGCCD::UpdateCCDFrame(int x, int y, int w, int h)
{
  DEBUGF(INDI::Logger::DBG_DEBUG, "The Final CCD image area is (%ld, %ld), (%ld, %ld)", x, y, w, h);

  // Set UNBINNED coords
  PrimaryCCD.setFrame(x, y, w, h);

  int nbuf;
  nbuf = (w * h * PrimaryCCD.getBPP() / 8);    //  this is pixel count
  nbuf += 512;    //  leave a little extra at the end
  PrimaryCCD.setFrameBufferSize(nbuf);

  DEBUGF(INDI::Logger::DBG_DEBUG, "Setting CCD frame buffer size to %d bytes.", nbuf);

  return true;
}

bool SBIGCCD::UpdateGuiderFrame(int x, int y, int w, int h)
{
  DEBUGF(INDI::Logger::DBG_DEBUG, "The Final Guide image area is (%ld, %ld), (%ld, %ld)", x, y, w, h);

  // Set UNBINNED coords
  GuideCCD.setFrame(x, y, w, h);

  int nbuf;
  nbuf = (w * h * GuideCCD.getBPP() / 8);    //  this is pixel count
  nbuf += 512;    //  leave a little extra at the end
  GuideCCD.setFrameBufferSize(nbuf);

  DEBUGF(INDI::Logger::DBG_DEBUG, "Setting Guide head frame buffer size to %d bytes.", nbuf);

  return true;
}

bool SBIGCCD::UpdateCCDBin(int binx, int biny)
{
    if (binx != biny)
        biny = binx;

    if (binx <1 || binx > 3)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error: Bad CCD binning mode! Use: 1x1, 2x2 or 3x3");
        return false;
    }

  PrimaryCCD.setBin(binx, biny);

  return updateFrameProperties(&PrimaryCCD);
}

bool SBIGCCD::UpdateGuiderBin(int binx, int biny)
{
    if (binx != biny)
        biny = binx;

    if (binx <1 || binx > 3)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error: Bad CCD binning mode! Use: 1x1, 2x2 or 3x3");
        return false;
    }

  GuideCCD.setBin(binx, biny);

  return updateFrameProperties(&GuideCCD);
}

IPState SBIGCCD::GuideNorth(float duration)
{
   ActivateRelayParams rp;
   rp.tXMinus = rp.tXPlus = rp.tYMinus = rp.tYPlus = 0;
   unsigned short dur = duration / 10.0;

   rp.tYMinus = dur;

   ActivateRelay(&rp);

   return IPS_OK;

}

IPState SBIGCCD::GuideSouth(float duration)
{
    ActivateRelayParams rp;
    rp.tXMinus = rp.tXPlus = rp.tYMinus = rp.tYPlus = 0;
    unsigned short dur = duration / 10.0;

    rp.tYPlus = dur;

    ActivateRelay(&rp);

    return IPS_OK;

}

IPState SBIGCCD::GuideEast(float duration)
{
    ActivateRelayParams rp;
    rp.tXMinus = rp.tXPlus = rp.tYMinus = rp.tYPlus = 0;
    unsigned short dur = duration / 10.0;

    rp.tXPlus = dur;

    ActivateRelay(&rp);

    return IPS_OK;

}

IPState SBIGCCD::GuideWest(float duration)
{
    ActivateRelayParams rp;
    rp.tXMinus = rp.tXPlus = rp.tYMinus = rp.tYPlus = 0;
    unsigned short dur = duration / 10.0;

    rp.tXMinus = dur;

    ActivateRelay(&rp);

    return IPS_OK;
}

float SBIGCCD::CalcTimeLeft(timeval start,float req)
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec/1000);
    timesince=timesince/1000;
    timeleft=req-timesince;
    return timeleft;
}

void * SBIGCCD::grabCCDHelper(void* context)
{
    return ((SBIGCCD*)context)->grabCCD();
}

void* SBIGCCD::grabCCD()
{
  CCDChip *targetChip = NULL;

  pthread_mutex_lock(&condMutex);

  while (true)
  {
      while (grabPredicate == GRAB_NO_CCD)
                  pthread_cond_wait(&cv, &condMutex);

      targetChip = (grabPredicate == GRAB_PRIMARY_CCD) ? &PrimaryCCD : &GuideCCD;
      grabPredicate=GRAB_NO_CCD;
      if (terminateThread)
          break;

      // release condMutex
      pthread_mutex_unlock(&condMutex);

      if (grabImage(targetChip) == false)
          targetChip->setExposureFailed();

     // lock cond before wait again
     pthread_mutex_lock(&condMutex);

  }

  pthread_mutex_unlock(&condMutex);
  return 0;
}

bool SBIGCCD::grabImage(CCDChip *targetChip)
{
    int res=0;
    unsigned short left	= (unsigned short)targetChip->getSubX()/targetChip->getBinX();
    unsigned short top	= (unsigned short)targetChip->getSubY()/targetChip->getBinX();
    unsigned short width	= (unsigned short)targetChip->getSubW()/targetChip->getBinX();
    unsigned short height	= (unsigned short)targetChip->getSubH()/targetChip->getBinY();

    if (sim)
    {
          DEBUGF(INDI::Logger::DBG_DEBUG, "GrabImage X: %d Y: %d Width: %d - Height: %d", left, top, width, height);
          DEBUGF(INDI::Logger::DBG_DEBUG, "Buf size: %d bytes.", width * height * 2);

          uint8_t * image = targetChip->getFrameBuffer();

        for (int i = 0; i < height*2; i++)
        for (int j = 0; j < width; j++)
            image[i * width + j] = rand() % 255;
    } else
    {
          unsigned short *buffer = (unsigned short *) targetChip->getFrameBuffer();

      // Readout CCD:
      DEBUGF(INDI::Logger::DBG_DEBUG, "%s CCD readout in progress...", targetChip == &PrimaryCCD ? "Primary":"Guide");

      for (int i=0; i < MAX_THREAD_RETRIES; i++)
      {
           if ( (res=readoutCCD(left, top, width, height, buffer, targetChip)) == CE_NO_ERROR)
               break;

          usleep(MAX_THREAD_WAIT);
      }

      if (res != CE_NO_ERROR)
      {
              DEBUGF(INDI::Logger::DBG_ERROR, "%s CCD readout error %s!", (targetChip == &PrimaryCCD ? "Primary":"Guide"), GetErrorString(res).c_str());
              return false;
      }
  }

  DEBUGF(INDI::Logger::DBG_DEBUG, "%s CCD Download complete.", (targetChip == &PrimaryCCD) ? "Primary" : "Guide");

  if (targetChip == &PrimaryCCD && targetChip->getExposureDuration() >= 5)
    DEBUG(INDI::Logger::DBG_SESSION, "Download complete.");

  ExposureComplete(targetChip);

  return true;

}


void SBIGCCD::addFITSKeywords(fitsfile *fptr, CCDChip *targetChip)
{
  INDI::CCD::addFITSKeywords(fptr, targetChip);
  int status=0;

  fits_update_key_s(fptr, TSTRING, "INSTRUME", ProductInfoT[0].text, "CCD Name" , &status);

}

bool SBIGCCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &FilterSlotNP);
    IUSaveConfigText(fp, FilterNameTP);
    IUSaveConfigSwitch(fp, &FilterTypeSP);

    return true;
}

void SBIGCCD::TimerHit()
{
  long timeleft=1e6;
  CCDChip *targetChip=NULL;

  if (isConnected() == false)
    return;  //  No need to reset timer if we are not connected anymore

  if (InExposure)
  {
   targetChip = &PrimaryCCD;
   timeleft = CalcTimeLeft(ExpStart,ExposureRequest);

   if (isExposureDone(targetChip))
   {
      /* We're done exposing */
      DEBUG(INDI::Logger::DBG_DEBUG, "Primay CCD exposure done, downloading image...");

      timeleft=0;
      targetChip->setExposureLeft(0);
      InExposure = false;
      //InGuideExposure = false;

      /* grab and save image */
      //if (grabImage(targetChip) == false)
      //      targetChip->setExposureFailed();
      pthread_mutex_lock(&condMutex);
      grabPredicate=GRAB_PRIMARY_CCD;
      pthread_mutex_unlock(&condMutex);
      pthread_cond_signal(&cv);

    }
     else
   {
       targetChip->setExposureLeft(timeleft);
       DEBUGF(INDI::Logger::DBG_DEBUG, "Primary CCD exposure in progress with %ld seconds left.", timeleft);
   }


  }

  if (InGuideExposure)
  {
    targetChip = &GuideCCD;
    timeleft = CalcTimeLeft(GuideExpStart,GuideExposureRequest);

    if (isExposureDone(targetChip))
    {
       /* We're done exposing */
       DEBUG(INDI::Logger::DBG_DEBUG, "Guide chip exposure done, downloading image...");

       timeleft=0;
       targetChip->setExposureLeft(0);
       //InExposure = false;
       InGuideExposure = false;

       /* grab and save image */
       //if (grabImage(targetChip) == false)
       //      targetChip->setExposureFailed();
       pthread_mutex_lock(&condMutex);
       grabPredicate=GRAB_GUIDE_CCD;
       pthread_mutex_unlock(&condMutex);
       pthread_cond_signal(&cv);

     }
    else
    {
        targetChip->setExposureLeft(timeleft);
        DEBUGF(INDI::Logger::DBG_DEBUG, "Guide chip exposure in progress with %ld seconds left.", timeleft);
    }


  }


  SetTimer(POLLMS);
  return;
}


//=========================================================================
int SBIGCCD::GetDriverInfo(GetDriverInfoParams *gdip, void *res)
{
    return(SBIGUnivDrvCommand(CC_GET_DRIVER_INFO, gdip, res));
}
//==========================================================================
int SBIGCCD::SetDriverHandle(SetDriverHandleParams *sdhp)
{
    return(SBIGUnivDrvCommand(CC_SET_DRIVER_HANDLE, sdhp, 0));
}
//==========================================================================
int SBIGCCD::GetDriverHandle(GetDriverHandleResults *gdhr)
{
    return(SBIGUnivDrvCommand(CC_GET_DRIVER_HANDLE, 0, gdhr));
}
//==========================================================================
int SBIGCCD::StartExposure(StartExposureParams2 *sep)
{
    return(SBIGUnivDrvCommand(CC_START_EXPOSURE2, sep, 0));
}
//==========================================================================
int SBIGCCD::EndExposure(EndExposureParams *eep)
{
    return(SBIGUnivDrvCommand(CC_END_EXPOSURE, eep, 0));
}
//==========================================================================
int SBIGCCD::StartReadout(StartReadoutParams *srp)
{
    return(SBIGUnivDrvCommand(CC_START_READOUT, srp, 0));
}
//==========================================================================
int SBIGCCD::ReadoutLine(ReadoutLineParams   *rlp,
                         unsigned short      *results,
                         bool				bSubtract)
{
    int	res;

    if(bSubtract){
        res = SBIGUnivDrvCommand(CC_READ_SUBTRACT_LINE, rlp, results);
    }else{
        res = SBIGUnivDrvCommand(CC_READOUT_LINE, rlp, results);
    }
    return(res);
}
//==========================================================================
int SBIGCCD::DumpLines(DumpLinesParams *dlp)
{
    return(SBIGUnivDrvCommand(CC_DUMP_LINES, dlp, 0));
}
//==========================================================================
int SBIGCCD::EndReadout(EndReadoutParams *erp)
{
    return(SBIGUnivDrvCommand(CC_END_READOUT, erp, 0));
}
//==========================================================================
int SBIGCCD::SetTemperatureRegulation(SetTemperatureRegulationParams *strp)
{
    return(SBIGUnivDrvCommand(CC_SET_TEMPERATURE_REGULATION, strp, 0));
}
//==========================================================================
int SBIGCCD::SetTemperatureRegulation(double temperature, bool enable)
{
    int res;
    SetTemperatureRegulationParams strp;

    if (sim)
    {
        TemperatureN[0].value = temperature;
        return CE_NO_ERROR;
    }

    if(CheckLink()){
        strp.regulation  = enable ? REGULATION_ON : REGULATION_OFF;
        strp.ccdSetpoint = CalcSetpoint(temperature);
        res = SBIGUnivDrvCommand(CC_SET_TEMPERATURE_REGULATION, &strp, 0);
    }else{
            res = CE_DEVICE_NOT_OPEN;
    }
    return(res);
}
//==========================================================================
int SBIGCCD::QueryTemperatureStatus(bool &enabled, double &ccdTemp, double &setpointTemp, double &power)
{
    int res;
    QueryTemperatureStatusResults qtsr;

    if (sim)
    {
        enabled = (CoolerS[0].s == ISS_ON);
        ccdTemp = TemperatureN[0].value;
        setpointTemp = ccdTemp;
        power = enabled ? 0.5 : 0;

        return CE_NO_ERROR;
    }

    if(CheckLink()){
        res = SBIGUnivDrvCommand(CC_QUERY_TEMPERATURE_STATUS, 0, &qtsr);
        if(res == CE_NO_ERROR)
        {
                enabled      = (qtsr.enabled != 0);
                ccdTemp      = CalcTemperature(CCD_THERMISTOR, qtsr.ccdThermistor);
                setpointTemp = CalcTemperature(CCD_THERMISTOR, qtsr.ccdSetpoint);
                power        = qtsr.power/255.0;


                DEBUGF(INDI::Logger::DBG_DEBUG, "%s: Regulation Enabled (%s) ccdTemp (%g) setpointTemp (%g) power (%g)", __FUNCTION__, enabled ? "True": "False", ccdTemp, setpointTemp, power);
        }
    }else{
        res = CE_DEVICE_NOT_OPEN;
    }
    return(res);
}
//==========================================================================
unsigned short SBIGCCD::CalcSetpoint(double temperature)
{
    // Calculate 'setpoint' from the temperature T in degr. of Celsius.
    double expo = (log(R_RATIO_CCD) * (T0 - temperature)) / DT_CCD;
    double r    = R0 * exp(expo);
    return((unsigned short)(((MAX_AD / (R_BRIDGE_CCD / r + 1.0)) + 0.5)));
}
//==========================================================================
double SBIGCCD::CalcTemperature(short thermistorType, short setpoint)
{
    double r, expo, rBridge, rRatio, dt;

    switch(thermistorType){
        case 	AMBIENT_THERMISTOR:
                    rBridge = R_BRIDGE_AMBIENT;
                    rRatio  = R_RATIO_AMBIENT;
                    dt      = DT_AMBIENT;
                    break;
    case CCD_THERMISTOR:
    default:
                    rBridge = R_BRIDGE_CCD;
                    rRatio  = R_RATIO_CCD;
                    dt      = DT_CCD;
                    break;
    }

    // Calculate temperature T in degr. Celsius from the 'setpoint'
    r = rBridge / ((MAX_AD / setpoint) - 1.0);
    expo = log(r / R0) / log(rRatio);
    return(T0 - dt * expo);
}
//==========================================================================
int SBIGCCD::ActivateRelay(ActivateRelayParams *arp)
{
    return(SBIGUnivDrvCommand(CC_ACTIVATE_RELAY, arp, 0));
}
//==========================================================================
int SBIGCCD::PulseOut(PulseOutParams *pop)
{
    return(SBIGUnivDrvCommand(CC_PULSE_OUT, pop, 0));
}
//==========================================================================
int SBIGCCD::TxSerialBytes(	TXSerialBytesParams  *txsbp,
                            TXSerialBytesResults *txsbr)
{
    return(SBIGUnivDrvCommand(CC_TX_SERIAL_BYTES, txsbp, txsbr));
}
//==========================================================================
int SBIGCCD::GetSerialStatus(GetSerialStatusResults *gssr)
{
    return(SBIGUnivDrvCommand(CC_GET_SERIAL_STATUS, 0, gssr));
}
//==========================================================================
int SBIGCCD::AoTipTilt(AOTipTiltParams *aottp)
{
    return(SBIGUnivDrvCommand(CC_AO_TIP_TILT, aottp, 0));
}
//==========================================================================
int SBIGCCD::AoDelay(AODelayParams *aodp)
{
    return(SBIGUnivDrvCommand(CC_AO_DELAY, aodp, 0));
}
//==========================================================================
int SBIGCCD::CFW(CFWParams *CFWp, CFWResults *CFWr)
{
    return(SBIGUnivDrvCommand(CC_CFW, CFWp, CFWr));
}
//==========================================================================
int SBIGCCD::EstablishLink()
{
    int 									res;
    EstablishLinkParams  elp;
    EstablishLinkResults elr;

    elp.sbigUseOnly = 0;

    if((res = SBIGUnivDrvCommand(CC_ESTABLISH_LINK, &elp, &elr)) == CE_NO_ERROR){
        SetCameraType((CAMERA_TYPE)elr.cameraType);
        SetLinkStatus(true);
    }
    return(res);
}
//==========================================================================
int SBIGCCD::GetCcdInfo(GetCCDInfoParams *gcp, void *gcr)
{
    return(SBIGUnivDrvCommand(CC_GET_CCD_INFO, gcp, gcr));
}
//==========================================================================
int	SBIGCCD::getCCDSizeInfo(	int ccd, int binning, int &frmW, int &frmH,
                                                    double &pixW, double &pixH)
{
    int res;
    GetCCDInfoParams		gcp;
  GetCCDInfoResults0	gcr;
  if (sim)
  {
      if (ccd == CCD_IMAGING)
      {
          frmW = 1024;
          frmH = 1024;
      }
      else
      {
          frmW = 512;
          frmH = 512;
      }

      pixW = 5.2;
      pixH = 5.2;

      return CE_NO_ERROR;
  }

  gcp.request = ccd;
    res = SBIGUnivDrvCommand(CC_GET_CCD_INFO, &gcp, &gcr);
    if(res == CE_NO_ERROR)
    {
            frmW = gcr.readoutInfo[binning].width;
            frmH = gcr.readoutInfo[binning].height;
            pixW = BcdPixel2double(gcr.readoutInfo[binning].pixelWidth);
            pixH = BcdPixel2double(gcr.readoutInfo[binning].pixelHeight);

            DEBUGF(INDI::Logger::DBG_DEBUG, "%s: ccd (%d) binning (%d) width (%d) height (%d) pixW (%g) pixH (%g)", __FUNCTION__, ccd, binning, frmW, frmH, pixW, pixH);
    }
    return(res);
}
//==========================================================================
int SBIGCCD::QueryCommandStatus(	QueryCommandStatusParams  *qcsp,
                                QueryCommandStatusResults *qcsr)
{
    return(SBIGUnivDrvCommand(CC_QUERY_COMMAND_STATUS, qcsp, qcsr));
}
//==========================================================================
int SBIGCCD::MiscellaneousControl(MiscellaneousControlParams *mcp)
{
    return(SBIGUnivDrvCommand(CC_MISCELLANEOUS_CONTROL, mcp, 0));
}
//==========================================================================
int SBIGCCD::ReadOffset(ReadOffsetParams *rop, ReadOffsetResults *ror)
{
    return(SBIGUnivDrvCommand(CC_READ_OFFSET, rop, ror));
}
//==========================================================================
int SBIGCCD::GetLinkStatus(GetLinkStatusResults *glsr)
{
    return(SBIGUnivDrvCommand(CC_GET_LINK_STATUS, glsr, 0));
}
//==========================================================================
string SBIGCCD::GetErrorString(int err)
{
    int res;
    GetErrorStringParams		gesp;
    GetErrorStringResults		gesr;

    gesp.errorNo = err;
    res = SBIGUnivDrvCommand(CC_GET_ERROR_STRING, &gesp, &gesr);
    if(res == CE_NO_ERROR) return(string(gesr.errorString));

    char str [128];
    sprintf(str, "No error string found! Error code: %d", err);
    return(string(str));
}
//==========================================================================
int SBIGCCD::SetDriverControl(SetDriverControlParams *sdcp)
{
    return(SBIGUnivDrvCommand(CC_SET_DRIVER_CONTROL, sdcp, 0));
}
//==========================================================================
int SBIGCCD::GetDriverControl(	GetDriverControlParams  *gdcp,
                                                        GetDriverControlResults *gdcr)
{
    return(SBIGUnivDrvCommand(CC_GET_DRIVER_CONTROL, gdcp, gdcr));
}
//==========================================================================
int SBIGCCD::UsbAdControl(USBADControlParams *usbadcp)
{
    return(SBIGUnivDrvCommand(CC_USB_AD_CONTROL, usbadcp, 0));
}
//==========================================================================
int SBIGCCD::QueryUsb(QueryUSBResults *qusbr)
{
    return(SBIGUnivDrvCommand(CC_QUERY_USB, 0, qusbr));
}
//==========================================================================
int	SBIGCCD::RwUsbI2c(RWUSBI2CParams *rwusbi2cp)
{
    return(SBIGUnivDrvCommand(CC_RW_USB_I2C, rwusbi2cp, 0));
}
//==========================================================================
int	SBIGCCD::BitIo(BitIOParams *biop, BitIOResults *bior)
{
    return(SBIGUnivDrvCommand(CC_BIT_IO, biop, bior));
}
//==========================================================================
string SBIGCCD::GetCameraName()
{
    int									res;
    string             	name = "Unknown camera";
    GetCCDInfoParams   	gccdip;
    GetCCDInfoResults0	gccdir;
    char               	*p1, *p2;

    if (sim)
    {
        name = "Simulated SBIG";
        return name;
    }

    gccdip.request = CCD_INFO_IMAGING;  // request 0
    res = SBIGUnivDrvCommand(CC_GET_CCD_INFO, &gccdip, &gccdir);
    if(res == CE_NO_ERROR)
    {
        name = gccdir.name;
        switch(gccdir.cameraType)
        {
            case	ST237_CAMERA:
                    if(gccdir.readoutInfo[0].gain >= 0x100) name += 'A';
                            break;
            case 	STL_CAMERA:
                            // driver reports name as "SBIG ST-L-XXX..."
                        p1 = gccdir.name + 5;
                        if((p2  = strchr(p1,' ')) != NULL){
                            *p2  = '\0';
                            name = p1;
                        }
                            break;
            case 	NO_CAMERA:
                    name = "No camera";
                            break;
            default:
                            break;
        }

    }
    return(name);
}
//==========================================================================
void SBIGCCD::GetExtendedCCDInfo()
{
    int res= CE_NO_ERROR;
    GetCCDInfoParams		gccdip;
    GetCCDInfoResults4	imaging_ccd_results4;
    GetCCDInfoResults4	tracking_ccd_results4;
    GetCCDInfoResults6	results6;

    if (sim)
    {
        hasGuideHead   = true;
        hasFilterWheel = true;
        return;
    }

    gccdip.request = 4;
    if( (res = GetCcdInfo(&gccdip, (void *)&imaging_ccd_results4)) == CE_NO_ERROR)
        DEBUGF(INDI::Logger::DBG_DEBUG, "CCD_IMAGING Extended CCD Info 4. CapabilitiesBit: (%u) Dump Extra (%u)", imaging_ccd_results4.capabilitiesBits, imaging_ccd_results4.dumpExtra);
    else
        DEBUGF(INDI::Logger::DBG_DEBUG, "Error getting extended CCD_IMAGING CCD Info 4 (%s)", GetErrorString(res).c_str());


    gccdip.request = 5;
    if( (res = GetCcdInfo(&gccdip, (void *)&tracking_ccd_results4)) == CE_NO_ERROR)
    {
        hasGuideHead = true;

        DEBUGF(INDI::Logger::DBG_DEBUG, "TRACKING_CCD Extended CCD Info 4. CapabilitiesBit: (%u) Dump Extra (%u)", tracking_ccd_results4.capabilitiesBits, tracking_ccd_results4.dumpExtra);

        if (tracking_ccd_results4.capabilitiesBits & CB_CCD_EXT_TRACKER_YES)
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "External tracking CCD detected.");
            useExternalTrackingCCD = true;
        }
        else
            useExternalTrackingCCD = false;
    }
    else
    {
        hasGuideHead = false;
        DEBUGF(INDI::Logger::DBG_DEBUG, "TRACKING_CCD Error getting extended CCD Info 4 (%s). No guide head detected.", GetErrorString(res).c_str());
    }

    gccdip.request = 6;
    if( (res = GetCcdInfo(&gccdip, (void *)&results6)) == CE_NO_ERROR)
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "Extended CCD Info 6. Camerabit: (%ld) CCD bits (%ld) Extra bit (%ld)", results6.cameraBits, results6.ccdBits, results6.extraBits);

        if (results6.ccdBits & 0x0001)
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Color CCD detected.");
            isColor = true;
            DEBUGF(INDI::Logger::DBG_DEBUG, "Detected color matrix is %s.", (results6.ccdBits & 0x0002) ? "Truesense" : "Bayer");
        }
        else
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Mono CCD detected.");
            isColor = false;
        }
    }
    else
        DEBUGF(INDI::Logger::DBG_DEBUG, "Error getting extended CCD Info 6 (%s)", GetErrorString(res).c_str());

    // Try to detect if there is a filter wheel
    CFWParams 	CFWp;
    CFWResults  CFWr;
    CFWp.cfwModel 		= CFWSEL_AUTO;
    CFWp.cfwCommand 	= CFWC_GET_INFO;
    CFWp.cfwParam1      = CFWG_FIRMWARE_VERSION;
    if ( (res = SBIGUnivDrvCommand(CC_CFW, &CFWp, &CFWr)) == CE_NO_ERROR)
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "Fitler wheel detected (firmware %ld).", CFWr.cfwResult1);
        hasFilterWheel = true;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "No fitler wheel detected (%s)", GetErrorString(res).c_str());
        hasFilterWheel = false;
    }
}
//==========================================================================
string SBIGCCD::GetCameraID()
{
    string str;
    int res = CE_NO_ERROR;
    GetCCDInfoParams	gccdip;
    GetCCDInfoResults2	gccdir2;
    if (sim)
    {
        str = "SBIG 1.6";
        return str;
    }

    gccdip.request = 2;
    if( ( res = GetCcdInfo(&gccdip, (void *)&gccdir2)) == CE_NO_ERROR)
    {
        str = gccdir2.serialNumber;
    }
    else
        DEBUGF(INDI::Logger::DBG_DEBUG, "%s: Error (%s)", __FUNCTION__, GetErrorString(res).c_str());


    return(str);
}
//==========================================================================

//==========================================================================
int SBIGCCD::SetDeviceName(const char *name)
{
    int res = CE_NO_ERROR;

    if(strlen(name) < PATH_MAX){
            strcpy(m_dev_name, name);
    }else{
            res = CE_BAD_PARAMETER;
    }
    return(res);
}
//==========================================================================
// SBIGUnivDrvCommand:
// Bottleneck function for all calls to the driver that logs the command
// and error. First it activates our handle and then it calls the driver.
// Activating the handle first allows having multiple instances of this
// class dealing with multiple cameras on different communications port.
// Also allows direct access to the SBIG Universal Driver after the driver
// has been opened.
int SBIGCCD::SBIGUnivDrvCommand(PAR_COMMAND command, void *params, void *results)
{
    int res;
    SetDriverHandleParams sdhp;

    if (sim)
        return CE_NO_ERROR;

    // Make sure we have a valid handle to the driver.
    if(GetDriverHandle() == INVALID_HANDLE_VALUE){
            res = CE_DRIVER_NOT_OPEN;
    }else{
            // Handle is valid so install it in the driver.
            sdhp.handle = GetDriverHandle();
            res = ::SBIGUnivDrvCommand(CC_SET_DRIVER_HANDLE, &sdhp, 0);

            if(res == CE_FAKE_DRIVER)
            {
            // The user is using the dummy driver. Tell him to download the real driver
            IDMessage(getDeviceName(), "Error: SBIG Dummy Driver is being used now. You can only control your camera by downloading SBIG driver from INDI website @ indi.sf.net");
            }
            else if(res == CE_NO_ERROR){
                    res = ::SBIGUnivDrvCommand(command, params, results);
            }
    }
    return(res);
}
//==========================================================================
bool SBIGCCD::CheckLink()
{
    if(GetCameraType() != NO_CAMERA && GetLinkStatus()) return(true);
    return(false);
}
//==========================================================================
/*int SBIGCCD::getNumberOfCCDChips()
{
    int res;

    switch(GetCameraType())
    {
        case 	ST237_CAMERA:
        case 	ST5C_CAMERA:
        case 	ST402_CAMERA:
        case    STI_CAMERA:
        case    STT_CAMERA:
        case    STF_CAMERA:
                    res = 1;
                    break;
        case 	ST7_CAMERA:
        case 	ST8_CAMERA:
        case 	ST9_CAMERA:
        case    ST10_CAMERA:
        case 	ST2K_CAMERA:
                    res = 2;
                    break;
        case 	STL_CAMERA:
                    res = 3;
                    break;
        case 	NO_CAMERA:
        default:
                    res = 0;
                    break;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "%s Camera Type (%d) Number of chips (%d)", __FUNCTION__, GetCameraType(), res);
    return(res);
}*/

//==========================================================================
bool SBIGCCD::IsFanControlAvailable()
{
    CAMERA_TYPE camera = GetCameraType();
    if(camera == ST5C_CAMERA || camera == ST402_CAMERA || camera == STI_CAMERA) return(false);
    return(true);
}
//==========================================================================
double SBIGCCD::BcdPixel2double(ulong bcd)
{
    double value = 0.0;
    double digit = 0.01;

    for(int i = 0; i < 8; i++){
            value += (bcd & 0x0F) * digit;
            digit *= 10.0;
            bcd  >>= 4;
    }
  return(value);
 }
//==========================================================================
void SBIGCCD::InitVars()
{
    SetFileDescriptor();
    SetCameraType();
    SetLinkStatus();
    SetDeviceName("");

}

//==========================================================================

int SBIGCCD::getBinningMode(CCDChip *targetChip, int &binning)
{
     int res=CE_NO_ERROR;

    if (targetChip->getBinX() == 1 && targetChip->getBinY() == 1)
    {
         binning = CCD_BIN_1x1_I;
    }
    else if (targetChip->getBinX() == 2 && targetChip->getBinY() == 2)
    {
         binning = CCD_BIN_2x2_I;
    }
    else if (targetChip->getBinX() == 3 && targetChip->getBinY() == 3)
    {
         binning = CCD_BIN_3x3_I;
    }
    else if (targetChip->getBinX() == 9 && targetChip->getBinY() == 9)
    {
                     binning = CCD_BIN_9x9_I;
    }
    else
    {
            res = CE_BAD_PARAMETER;
            DEBUG(INDI::Logger::DBG_ERROR, "Error: Bad CCD binning mode! Use: 1x1, 2x2 or 3x3");
    }

    return(res);
}

//==========================================================================

int SBIGCCD::getFrameType(CCDChip *targetChip, string &frame_type)
{
    int res = CE_NO_ERROR;

    CCDChip::CCD_FRAME fType;

   fType = targetChip->getFrameType();
   frame_type = targetChip->getFrameTypeName(fType);

    return(res);
}

//==========================================================================

int SBIGCCD::getShutterMode(CCDChip *targetChip, int &shutter)
{
    string 	frame_type;
    int res = getFrameType(targetChip, frame_type);
    if(res != CE_NO_ERROR) return(res);
    int ccd = CCD_IMAGING;

    if (targetChip == &PrimaryCCD)
        ccd = CCD_IMAGING;
    else if (targetChip == &GuideCCD)
        ccd = useExternalTrackingCCD ? CCD_EXT_TRACKING : CCD_TRACKING;

    if(	!strcmp(frame_type.c_str(), "FRAME_LIGHT")	||  !strcmp(frame_type.c_str(), "FRAME_FLAT")
            ||  !strcmp(frame_type.c_str(), "FRAME_BIAS")		)
    {
            if(ccd == CCD_EXT_TRACKING)
            {
                    shutter = SC_OPEN_EXT_SHUTTER;
            }else
            {
                    shutter = SC_OPEN_SHUTTER;
            }
    }else if(!strcmp(frame_type.c_str(), "FRAME_DARK"))
    {
            if(ccd == CCD_EXT_TRACKING)
            {
                    shutter = SC_CLOSE_EXT_SHUTTER;
            }else
            {
                    shutter = SC_CLOSE_SHUTTER;
            }
    }else
    {
            res = CE_OS_ERROR;
            DEBUGF(INDI::Logger::DBG_ERROR, "Error: Unknown selected CCD frame type! %s", frame_type.c_str());
    }
    return(res);
}

bool SBIGCCD::SelectFilter(int position)
{
    CFWResults 	CFWr;
    int 		type;
    char		str[64];
    int         res = CE_NO_ERROR;

    if( (res = CFWGoto(&CFWr, position)) == CE_NO_ERROR)
    {
            type = GetCFWSelType();
            if(type == CFWSEL_CFW6A || type == CFWSEL_CFW8)
            {
                sprintf(str, "CFW position reached.");
                CFWr.cfwPosition = position;
            }else
            {
                sprintf(str, "CFW position %d reached.", CFWr.cfwPosition);
            }
            DEBUGF(INDI::Logger::DBG_SESSION, "%s", str);
            SelectFilterDone(CFWr.cfwPosition);
            CurrentFilter =CFWr.cfwPosition;
            return true;
   }
   else
   {
            // CFW error occurred, so report all the available infos to the client:
            CFWShowResults("CFWGoto:", CFWr);
            FilterSlotNP.s = IPS_ALERT;
            IDSetNumber(&FilterSlotNP, NULL);
            DEBUG(INDI::Logger::DBG_SESSION, "Please Connect/Disconnect CFW, then try again...");
            DEBUGF(INDI::Logger::DBG_DEBUG, "%s: Error (%s)", __FUNCTION__, GetErrorString(res).c_str());
            return false;
   }

    return false;
}

bool SBIGCCD::SetFilterNames()
{
    // Cannot save it in hardware, so let's just save it in the config file to be loaded later
    saveConfig();
    return true;
}

bool SBIGCCD::GetFilterNames(const char* groupName)
{
    char filterName[MAXINDINAME];
    char filterLabel[MAXINDILABEL];
    char filterBand[MAXINDILABEL];
    int MaxFilter = FilterSlotN[0].max;

    if (FilterNameT != NULL)
        delete FilterNameT;

    FilterNameT = new IText[MaxFilter];

    for (int i=0; i < MaxFilter; i++)
    {
        snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i+1);
        snprintf(filterLabel, MAXINDILABEL, "Filter#%d", i+1);
        snprintf(filterBand, MAXINDILABEL, "Filter #%d", i+1);
        IUFillText(&FilterNameT[i], filterName, filterLabel, filterBand);
    }

    IUFillTextVector(FilterNameTP, FilterNameT, MaxFilter, getDeviceName(), "FILTER_NAME", "Filter", groupName, IP_RW, 0, IPS_IDLE);

    return true;
}

int SBIGCCD::QueryFilter()
{
    return CurrentFilter;
}

void SBIGCCD::updateTemperatureHelper(void *p)
{
    if (static_cast<SBIGCCD*>(p)->isConnected())
        static_cast<SBIGCCD*>(p)->updateTemperature();

}

//==========================================================================
void SBIGCCD::updateTemperature()
{
    int res=CE_NO_ERROR;
    bool   	enabled;
    double 	ccdTemp, setpointTemp, percentTE, power;

    pthread_mutex_lock(&sbigMutex);
    res = QueryTemperatureStatus(enabled, ccdTemp, setpointTemp, percentTE);
    pthread_mutex_unlock(&sbigMutex);

    if( res == CE_NO_ERROR)
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "ccdTemp: %g setpointTemp: %g TEMP_DIFF %g", ccdTemp, setpointTemp, TEMP_DIFF);

        power = 100.0 * percentTE;

        // Compare the current temperature against the setpoint value:
        if(fabs(setpointTemp - ccdTemp) <= TEMP_DIFF)
        {
                TemperatureNP.s = IPS_OK;
        }
        else if (power == 0)
        {
            TemperatureNP.s = IPS_IDLE;
        }
        else
        {
                TemperatureNP.s = IPS_BUSY;
                DEBUGF(INDI::Logger::DBG_DEBUG, "CCD temperature %+.1f [C], TE cooler: %.1f [%%].", ccdTemp, power);
        }

        TemperatureN[0].value = ccdTemp;
        // Check the TE cooler if inside the range:

        if(power <= CCD_COOLER_THRESHOLD)
        {
                CoolerNP.s = IPS_OK;
        }else
        {
                CoolerNP.s = IPS_BUSY;
        }
        CoolerN[0].value = power;

        IDSetNumber(&TemperatureNP, NULL);
        IDSetNumber(&CoolerNP, 0);
    }
    else
    {
        // ignore share errors
        if (res == CE_SHARE_ERROR)
        {
            DEBUGF(INDI::Logger::DBG_DEBUG, "Erro reading temperature. %s", GetErrorString(res).c_str());
            TemperatureNP.s = IPS_IDLE;
        }
        else
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Erro reading temperature. %s", GetErrorString(res).c_str());
            TemperatureNP.s = IPS_ALERT;
        }

        IDSetNumber(&TemperatureNP, NULL);
    }

    IEAddTimer(TEMPERATURE_POLL_MS, SBIGCCD::updateTemperatureHelper, this);
}


//==========================================================================

bool SBIGCCD::isExposureDone(CCDChip *targetChip)
{
    int ccd, res;

    if (isSimulation())
    {
        long timeleft=1e6;

         if (targetChip == &PrimaryCCD)
            timeleft = CalcTimeLeft(ExpStart,ExposureRequest);
         else
            timeleft = CalcTimeLeft(GuideExpStart,GuideExposureRequest);

         if (timeleft <= 0)
             return true;
         else
             return false;
    }

    if (targetChip == &PrimaryCCD)
        ccd = CCD_IMAGING;
    else
        ccd = useExternalTrackingCCD ? CCD_EXT_TRACKING : CCD_TRACKING;

  EndExposureParams			eep;
  QueryCommandStatusParams	qcsp;
  QueryCommandStatusResults	qcsr;

    // Query command status:
  qcsp.command = CC_START_EXPOSURE2;

  pthread_mutex_lock(&sbigMutex);
  res = QueryCommandStatus(&qcsp, &qcsr);

  if(res != CE_NO_ERROR)
  {
      pthread_mutex_unlock(&sbigMutex);
      return false;
  }

  int mask = 12; // Tracking & external tracking CCD chip mask.
  if(ccd == CCD_IMAGING)
      mask = 3; // Imaging chip mask.

    // Check exposure progress:
    if((qcsr.status & mask) != mask)
    {
            // The exposure is still in progress, decrement an
            // exposure time:
             pthread_mutex_unlock(&sbigMutex);
            return false;
    }

    // Exposure done - update client's property:
    eep.ccd = ccd;
    EndExposure(&eep);

    pthread_mutex_unlock(&sbigMutex);

    return true;
}

//==========================================================================

int SBIGCCD::readoutCCD(unsigned short left,	unsigned short top,
                                                unsigned short width,	unsigned short height,
                                                unsigned short *buffer, CCDChip *targetChip)
{
    int h, ccd, binning, res;

    if (targetChip == &PrimaryCCD)
        ccd = CCD_IMAGING;
    else
        ccd = useExternalTrackingCCD ? CCD_EXT_TRACKING : CCD_TRACKING;

    if((res = getBinningMode(targetChip, binning)) != CE_NO_ERROR) return(res);

  StartReadoutParams srp;
  srp.ccd 			= ccd;
  srp.readoutMode	= binning;
  srp.left			= left;
  srp.top			= top;
  srp.width			= width;
  srp.height		= height;

  pthread_mutex_lock(&sbigMutex);
  res = StartReadout(&srp);

  if(res != CE_NO_ERROR)
  {
            DEBUGF(INDI::Logger::DBG_ERROR, "%s readoutCCD - StartReadout error! (%s)", (targetChip == &PrimaryCCD) ? "Primary" : "Guide", GetErrorString(res).c_str());
            pthread_mutex_unlock(&sbigMutex);
            return(res);
  }

    // Readout lines.
  ReadoutLineParams rlp;
  rlp.ccd			= ccd;
  rlp.readoutMode	= binning;
  rlp.pixelStart	= left;
  rlp.pixelLength	= width;

    // Readout CCD row by row:
    for(h = 0; h < height; h++)
    {
            ReadoutLine(&rlp, buffer + (h * width), false);
    }

    // End readout:
    EndReadoutParams	erp;
    erp.ccd = ccd;
    if((res = EndReadout(&erp)) != CE_NO_ERROR)
    {
            DEBUGF(INDI::Logger::DBG_ERROR, "%s readoutCCD - EndReadout error! (%s)", (targetChip == &PrimaryCCD) ? "Primary" : "Guide", GetErrorString(res).c_str());
            pthread_mutex_unlock(&sbigMutex);
            return(res);
    }

    pthread_mutex_unlock(&sbigMutex);
    return(res);
}

//==========================================================================

int SBIGCCD::CFWConnect()
{
    int				res;
    CFWResults	CFWr;

    IUResetSwitch(&FilterConnectionSP);

    if (isConnected() == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "You must establish connection to CCD before connecting to filter wheel.");
        FilterConnectionSP.s = IPS_IDLE;
        FilterConnectionS[1].s = ISS_ON;
        IDSetSwitch(&FilterConnectionSP, NULL);
        return CE_OS_ERROR;
    }

    do{
        // 1. CFWC_OPEN_DEVICE:
        if((res = CFWOpenDevice(&CFWr)) != CE_NO_ERROR)
        {
            FilterConnectionSP.s = IPS_IDLE;
            DEBUGF(INDI::Logger::DBG_ERROR, "CFWC_OPEN_DEVICE error: %s !",  GetErrorString(res).c_str());
            continue;
        }

        // 2. CFWC_INIT:
        if((res = CFWInit(&CFWr)) != CE_NO_ERROR)
        {
            DEBUGF(INDI::Logger::DBG_ERROR,  "CFWC_INIT error: %s !",  GetErrorString(res).c_str());
            CFWCloseDevice(&CFWr);
            DEBUG(INDI::Logger::DBG_DEBUG, "CFWC_CLOSE_DEVICE called.");
            continue;
        }

        // 3. CFWC_GET_INFO:
        if((res = CFWGetInfo(&CFWr)) != CE_NO_ERROR)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "CFWC_GET_INFO error: %s", GetErrorString(res).c_str());
            continue;
        }

        if (sim)
        {
            int cfwsim[16] = {  2, 5 , 6 , 8, 4, 10, 10, 8, 9, 8, 10, 5, 5,8 ,7 ,8};
            int cfwmodel[16] = { CFWSEL_CFW2, CFWSEL_CFW5, CFWSEL_CFW6A, CFWSEL_CFW8, CFWSEL_CFW402, CFWSEL_CFW10, CFWSEL_CFW10_SERIAL, CFWSEL_CFWL, CFWSEL_CFW9, CFWSEL_CFWL8G, CFWSEL_CFW1603, CFWSEL_FW5_STX, CFWSEL_FW5_8300, CFWSEL_FW8_8300, CFWSEL_FW7_STX, CFWSEL_FW8_STT};
            int filnum = IUFindOnSwitchIndex(&FilterTypeSP);
            if (filnum < 0)
                CFWr.cfwResult2 = 5;
            else
                CFWr.cfwResult2 = cfwsim[filnum];

            CFWr.cfwModel = cfwmodel[filnum];
        }

        // 4. CFWUpdateProperties:
        CFWUpdateProperties(CFWr);


    }while(0);

    if (res == CE_NO_ERROR)
    {
        FilterConnectionSP.s = IPS_OK;
        DEBUG(INDI::Logger::DBG_SESSION, "CFW connected.");
        FilterConnectionS[0].s = ISS_ON;
        IDSetSwitch(&FilterConnectionSP, NULL);
        defineNumber(&FilterSlotNP);
        DEBUG(INDI::Logger::DBG_DEBUG, "Loading FILTER_SLOT from config file...");
        loadConfig(true, "FILTER_SLOT");
        IUUpdateMinMax(&FilterSlotNP);
        DEBUG(INDI::Logger::DBG_DEBUG, "Loading FILTER_NAME from config file...");
        // Load filter names from file
        loadConfig(true, "FILTER_NAME");

    }else
    {
        FilterConnectionSP.s = IPS_ALERT;
        FilterConnectionS[1].s = ISS_ON;
        IUResetSwitch(&FilterConnectionSP);
        FilterConnectionSP.sp[1].s = ISS_ON;
        DEBUGF(INDI::Logger::DBG_ERROR, "CFW connection error! (%s)", GetErrorString(res).c_str());
        IDSetSwitch(&FilterConnectionSP, NULL);
    }

    return(res);
}

//==========================================================================

int SBIGCCD::CFWDisconnect()
{
    CFWResults	CFWr;

    IUResetSwitch(&FilterConnectionSP);

    // Close CFW device:
    int res = CFWCloseDevice(&CFWr);

    if(res != CE_NO_ERROR)
    {
        FilterConnectionS[0].s = ISS_ON;
        FilterConnectionSP.s = IPS_ALERT;
        DEBUGF(INDI::Logger::DBG_ERROR, "CFW disconnection error! (%s)", GetErrorString(res).c_str());
        IDSetSwitch(&FilterConnectionSP, NULL);
    }
    else
    {
        // Update CFW's Product/ID texts.
        CFWr.cfwModel 		= CFWSEL_UNKNOWN;
        CFWr.cfwPosition    = CFWP_UNKNOWN;
        CFWr.cfwStatus		= CFWS_UNKNOWN;
        CFWr.cfwError		= CFWE_DEVICE_NOT_OPEN;
        CFWr.cfwResult1		= 0;
        CFWr.cfwResult2		= 0;
        //CFWUpdateProperties(CFWr);
        // Remove connection text.
        FilterConnectionS[1].s = ISS_ON;
        FilterConnectionSP.s = IPS_IDLE;
        DEBUG(INDI::Logger::DBG_SESSION, "CFW disconnected.");
        IDSetSwitch(&FilterConnectionSP, NULL);
        deleteProperty(FilterSlotNP.name);
        deleteProperty(FilterNameTP->name);

    }

    return res;
}

//==========================================================================

int SBIGCCD::CFWOpenDevice(CFWResults *CFWr)
{
    // Under Linux we always try to open the "sbigCFW" device. There has to be a
  // symbolic link (ln -s) between the actual device and this name.

    CFWParams 	CFWp;
    int 				res = CE_NO_ERROR;
    int				CFWModel = GetCFWSelType();

    switch(CFWModel)
    {
        case 	CFWSEL_CFW10_SERIAL:
                    CFWp.cfwModel 		= CFWModel;
                    CFWp.cfwCommand 	= CFWC_OPEN_DEVICE;
                    res = SBIGUnivDrvCommand(CC_CFW, &CFWp, CFWr);
                    break;
        default:
                    break;
    }

    return(res);
}

//==========================================================================

int SBIGCCD::CFWCloseDevice(CFWResults *CFWr)
{
    CFWParams CFWp;

    if (sim)
        return CE_NO_ERROR;

    CFWp.cfwModel 		= GetCFWSelType();
    CFWp.cfwCommand 	= CFWC_CLOSE_DEVICE;
    return(SBIGUnivDrvCommand(CC_CFW, &CFWp, CFWr));
}

//==========================================================================

int SBIGCCD::CFWInit(CFWResults *CFWr)
{
    // Try to init CFW maximum three times:
    int res;
    CFWParams CFWp;

    CFWp.cfwModel 		= GetCFWSelType();
    CFWp.cfwCommand 	= CFWC_INIT;
    for(int i=0; i < 3; i++){
            if((res = SBIGUnivDrvCommand(CC_CFW, &CFWp, CFWr)) == CE_NO_ERROR) break;
            sleep(1);
    }

    if(res != CE_NO_ERROR) return(res);
    return(CFWGotoMonitor(CFWr));
}

//==========================================================================

int SBIGCCD::CFWGetInfo(CFWResults *CFWr)
{
    CFWParams CFWp;

    CFWp.cfwModel 		= GetCFWSelType();
    CFWp.cfwCommand 	= CFWC_GET_INFO;
    CFWp.cfwParam1		= CFWG_FIRMWARE_VERSION;
    return(SBIGUnivDrvCommand(CC_CFW, &CFWp, CFWr));
}

//==========================================================================

int SBIGCCD::CFWQuery(CFWResults *CFWr)
{
    CFWParams CFWp;

    CFWp.cfwModel 		= GetCFWSelType();
    CFWp.cfwCommand 	= CFWC_QUERY;
    return(SBIGUnivDrvCommand(CC_CFW, &CFWp, CFWr));
}

//==========================================================================

int SBIGCCD::CFWGoto(CFWResults *CFWr, int position)
{
    int res;
    CFWParams CFWp;

    CFWp.cfwModel 		= GetCFWSelType();
    CFWp.cfwCommand 	= CFWC_GOTO;
    CFWp.cfwParam1	= (unsigned long)position;

    if (sim)
    {
        CFWr->cfwPosition = position;
        return CE_NO_ERROR;
    }

     DEBUGF(INDI::Logger::DBG_DEBUG, "CFW GOTO: %d", position);
    // 2014-06-16: Do we need to also checking if the position is reached here? A test will determine.
    if((res = SBIGUnivDrvCommand(CC_CFW, &CFWp, CFWr)) != CE_NO_ERROR && CFWp.cfwParam1 == CFWr->cfwPosition)
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "CFW Reached position %d", CFWr->cfwPosition);
        return(res);
    }

    DEBUG(INDI::Logger::DBG_DEBUG, "CFW did not reach position yet, invoking CFWGotoMonitor");
    return(CFWGotoMonitor(CFWr));
}

//==========================================================================

int SBIGCCD::CFWGotoMonitor(CFWResults *CFWr)
{
    int res;

    if (sim)
        return CE_NO_ERROR;

    do{
            if((res = CFWQuery(CFWr)) != CE_NO_ERROR)
                return(res);

            switch (CFWr->cfwStatus)
            {
                case CFWS_IDLE:
                DEBUG(INDI::Logger::DBG_DEBUG, "CFW Status Idle.");
                break;
                case CFWS_BUSY:
                DEBUG(INDI::Logger::DBG_DEBUG, "CFW Status Busy.");
                break;
                default:
                DEBUG(INDI::Logger::DBG_DEBUG, "CFW Status unknown.");
                break;
            }


    }while(CFWr->cfwStatus != CFWS_IDLE);
    return(res);
}

//==========================================================================

void SBIGCCD::CFWUpdateProperties(CFWResults CFWr)
{
    string str;
    bool	bClear = false;

    switch(CFWr.cfwModel)
    {
        case	CFWSEL_CFW2:
                    str = "CFW - 2";
                    break;
        case 	CFWSEL_CFW5:
                    str = "CFW - 5";
                    break;
        case 	CFWSEL_CFW6A:
                   str = "CFW - 6A";
                    break;
        case 	CFWSEL_CFW8:
                  str = "CFW - 8";
                    break;
        case 	CFWSEL_CFW402:
                  str = "CFW - 402";
                    break;
        case 	CFWSEL_CFW10:
                  str = "CFW - 10";
                    break;
        case 	CFWSEL_CFW10_SERIAL:
                  str = "CFW - 10SA";
                    break;
        case 	CFWSEL_CFWL:
                  str = "CFW - L";
                    break;
        case	CFWSEL_CFW9:
                  str = "CFW - 9";
                    break ;
        case    CFWSEL_CFWL8G:
                 str = "CFW - L8G";
            break ;
        case    CFWSEL_CFW1603:
                str = "CFW - 1603";
            break ;
        case    CFWSEL_FW5_STX:
                str = "CFW - FW5 STX";
            break ;
        case    CFWSEL_FW5_8300:
                str = "CFW - FW5 8300";
            break ;
        case    CFWSEL_FW8_8300:
                str = "CFW - FW8 8300";
            break ;
        case    CFWSEL_FW7_STX:
               str = "CFW - FW7 STX";
            break ;
        case    CFWSEL_FW8_STT:
              str = "CFW - FW8 STT";
            break ;
        default:
             str = "Unknown";
             bClear = true;
             break;
    }
    // Set CFW's product ID:
    IText *pIText = IUFindText(&FilterProdcutTP, "NAME");
    if(pIText) IUSaveText(pIText, str.c_str());

    DEBUGF(INDI::Logger::DBG_DEBUG, "CFW Product ID: %s", str.c_str());

    // Set CFW's firmware version:
    if(bClear){
            str = "Unknown";
    }else{
            ostringstream ss;
            ss << (int)CFWr.cfwResult1;
            str = ss.str();
    }
    pIText = IUFindText(&FilterProdcutTP, "ID");
    if(pIText) IUSaveText(pIText, str.c_str());
    FilterProdcutTP.s = IPS_OK;
    IDSetText(&FilterProdcutTP, 0);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CFW Firmware: %s", str.c_str());

    if (sim)
        CFWr.cfwPosition = 1;

    // Set CFW's filter min/max values:
    FilterSlotN[0].min = 1;
    FilterSlotN[0].max = CFWr.cfwResult2;
    FilterSlotN[0].value = CFWr.cfwPosition;
    if (FilterSlotN[0].value < FilterSlotN[0].min)
        FilterSlotN[0].value = FilterSlotN[0].min;
    else if (FilterSlotN[0].value > FilterSlotN[0].max)
        FilterSlotN[0].value = FilterSlotN[0].max;
    //IUUpdateMinMax(&FilterSlotNP);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CFW min: 1 Max: %g Current Slot: %g", FilterSlotN[0].max, FilterSlotN[0].value);

    GetFilterNames(FILTER_TAB);

    defineText(FilterNameTP);

}

//==========================================================================

int SBIGCCD::GetCFWSelType()
{
    int 	type = CFWSEL_UNKNOWN;;
    ISwitch *p = IUFindOnSwitch(&FilterTypeSP);
    useExternalTrackingCCD = false;

    if(p){
        if(!strcmp(p->name, "CFW1")){
                type = CFWSEL_CFW2;
        }else if(!strcmp(p->name, "CFW2")){
                type = CFWSEL_CFW5;
        }else if(!strcmp(p->name, "CFW3")){
                type = CFWSEL_CFW6A;
        }else if(!strcmp(p->name, "CFW4")){
                type = CFWSEL_CFW8;
        }else if(!strcmp(p->name, "CFW5")){
                type = CFWSEL_CFW402;
        }else if(!strcmp(p->name, "CFW6")){
                type = CFWSEL_CFW10;
        }else if(!strcmp(p->name, "CFW7")){
                type = CFWSEL_CFW10_SERIAL;
        }else if(!strcmp(p->name, "CFW8")){
                type = CFWSEL_CFWL;
        }else if(!strcmp(p->name, "CFW9")){
                type = CFWSEL_CFW9;
        }else if(!strcmp(p->name, "CFW10")){
                type = CFWSEL_CFWL8G;
        }else if(!strcmp(p->name, "CFW11")){
                type = CFWSEL_CFW1603;
        }else if(!strcmp(p->name, "CFW12")){
                type = CFWSEL_FW5_STX;
        }else if(!strcmp(p->name, "CFW13")){
                type = CFWSEL_FW5_8300;
        }else if(!strcmp(p->name, "CFW14")){
                type = CFWSEL_FW8_8300;
        }else if(!strcmp(p->name, "CFW15")){
                type = CFWSEL_FW7_STX;
        }else if(!strcmp(p->name, "CFW16")){
                type = CFWSEL_FW8_STT;
        #ifdef	 USE_CFW_AUTO
        }else if(!strcmp(p->name, "CFW17")){
                type = CFWSEL_AUTO;
        #endif
        }
    }
    return(type);
}

//==========================================================================

void SBIGCCD::CFWShowResults(string name, CFWResults CFWr)
{
    DEBUGF(INDI::Logger::DBG_SESSION, "%s", name.c_str());
    DEBUGF(INDI::Logger::DBG_SESSION, "CFW Model:	%d", CFWr.cfwModel);
    DEBUGF(INDI::Logger::DBG_SESSION, "CFW Position:	%d", CFWr.cfwPosition);
    DEBUGF(INDI::Logger::DBG_SESSION, "CFW Status:	%d", CFWr.cfwStatus);
    DEBUGF(INDI::Logger::DBG_SESSION, "CFW Error:	%d", CFWr.cfwError);
    DEBUGF(INDI::Logger::DBG_SESSION, "CFW Result1:	%ld", CFWr.cfwResult1);
    DEBUGF(INDI::Logger::DBG_SESSION, "CFW Result2:	%ld", CFWr.cfwResult2);
}

//==========================================================================
