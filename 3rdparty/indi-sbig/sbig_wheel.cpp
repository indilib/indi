/*******************************************************************************
  Copyright(c) 2014. Jasem Mutlaq.

  SBIG CFW

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/
#include <memory>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include <indidevapi.h>
#include "eventloop.h"
#include <indilogger.h>
#include "sbig_wheel.h"

#define MAX_DEVICES         5      /* Max device filterCount */

static int filterCount;
static SBIGCFW *filterwheels[MAX_DEVICES];
pthread_cond_t      cv  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     condMutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t     sbigMutex = PTHREAD_MUTEX_INITIALIZER;

/**********************************************************
 *
 *  IMPORRANT: List supported camera models in initializer of deviceTypes structure
 *
 **********************************************************/

static void cleanup() {
  for (int i = 0; i < filterCount; i++) {
    delete filterwheels[i];
  }
}

void ISInit()
{
  static bool isInit = false;
  if (!isInit)
  {

      // Let's just create one camera for now
     filterCount = 1;
     filterwheels[0] = new SBIGCFW();

    atexit(cleanup);
    isInit = true;
  }
}

void ISGetProperties(const char *dev) {
  ISInit();
  for (int i = 0; i < filterCount; i++) {
    SBIGCFW *camera = filterwheels[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISGetProperties(dev);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
  ISInit();
  for (int i = 0; i < filterCount; i++) {
    SBIGCFW *camera = filterwheels[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewSwitch(dev, name, states, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num) {
  ISInit();
  for (int i = 0; i < filterCount; i++) {
    SBIGCFW *camera = filterwheels[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewText(dev, name, texts, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
  ISInit();
  for (int i = 0; i < filterCount; i++) {
    SBIGCFW *camera = filterwheels[i];
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

//==========================================================================
SBIGCFW::SBIGCFW()
{
    InitVars();
    OpenDriver();

    // For now let's set name to default name. In the future, we need to to support multiple devices per one driver
    if (*getDeviceName() == '\0')
        strncpy(name, getDefaultName(), MAXINDINAME);
    else
        strncpy(name, getDeviceName(), MAXINDINAME);

    setVersion(1, 0);

}
//==========================================================================
SBIGCFW::SBIGCFW(const char* devName)
{
    InitVars();
    if(OpenDriver() == CE_NO_ERROR) OpenDevice(devName);
}
//==========================================================================
SBIGCFW::~SBIGCFW()
{
    CloseDevice();
    CloseDriver();
}
//==========================================================================
int SBIGCFW::OpenDriver()
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
int SBIGCFW::CloseDriver()
{
    int res;

    if((res = ::SBIGUnivDrvCommand(CC_CLOSE_DRIVER, 0, 0)) == CE_NO_ERROR){
            SetDriverHandle();
    }
    return(res);
}
//==========================================================================
int SBIGCFW::OpenDevice(const char *devName)
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
    return(res);
}
//==========================================================================
int SBIGCFW::CloseDevice()
{
    int res = CE_NO_ERROR;

    if(IsDeviceOpen())
    {
        if((res = SBIGUnivDrvCommand(CC_CLOSE_DEVICE, 0, 0)) == CE_NO_ERROR)
        {
                SetFileDescriptor();	// set value to -1
                //SetCameraType(); 		// set value to NO_CAMERA
        }
    }
    return(res);
}

const char * SBIGCFW::getDefaultName()
{
    return (const char *)"SBIG CFW";
}

bool SBIGCFW::initProperties()
{
  // Init parent properties first
  INDI::FilterWheel::initProperties();

  //CFW Port
  IUFillText(&PortT[0], "PORT", "Port", SBIG_USB0);
  IUFillTextVector(	&PortTP, PortT, 1, getDeviceName(), "DEVICE_PORT", "Port", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

  // CFW PRODUCT
  IUFillText(&FilterProdcutT[0], "NAME", "Name", "");
  IUFillText(&FilterProdcutT[1], "LABEL", "Label", "");
  IUFillTextVector(	&FilterProdcutTP, FilterProdcutT, 2, getDeviceName(), "CFW_PRODUCT" , "Product", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

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
  IUFillSwitchVector(&FilterTypeSP, FilterTypeS, MAX_CFW_TYPES, getDeviceName(),"CFW_TYPE", "Type", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  FilterSlotN[0].min = 1;
  FilterSlotN[0].max = MAX_CFW_TYPES;

  return true;
}

void SBIGCFW::ISGetProperties(const char *dev)
{
  INDI::FilterWheel::ISGetProperties(dev);

  defineSwitch(&FilterTypeSP);
  defineText(&PortTP);
  defineText(&FilterProdcutTP);


  // Add Debug, Simulator, and Configuration controls
  addAuxControls();
}

bool SBIGCFW::updateProperties()
{
  INDI::FilterWheel::updateProperties();

  if (isConnected())
  {
      defineNumber(&FilterSlotNP);
      defineText(FilterNameTP);
  }
  else
  {
    deleteProperty(FilterSlotNP.name);
    if (FilterNameT != NULL)
        deleteProperty(FilterNameTP->name);
  }

  return true;
}

bool SBIGCFW::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if(strcmp(name,PortTP.name)==0)
        {
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

    return INDI::FilterWheel::ISNewText(dev, name, texts, names, n);
}

bool SBIGCFW::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
  if (strcmp(dev, getDeviceName()) == 0)
  {
        // CFW TYPE:
        if(!strcmp(name, FilterTypeSP.name))
        {

            IUUpdateSwitch(&FilterTypeSP, states, names, n);
            FilterTypeSP.s = IPS_OK;
            IDSetSwitch(&FilterTypeSP, NULL);
            return true;
       }

  }

  //  Nobody has claimed this, so, ignore it
  return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

bool SBIGCFW::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
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
  return INDI::FilterWheel::ISNewNumber(dev, name, values, names, n);
}

bool SBIGCFW::Connect()
{
  int res = CE_NO_ERROR;
  string str;

  ISwitch *p = IUFindOnSwitch(&FilterTypeSP);
  if (p == NULL)
  {

      DEBUG(INDI::Logger::DBG_WARNING, "Please select filter type before connecting.");
      return false;
  }

  // Open device:
  if((res = OpenDevice(PortTP.tp->text)) == CE_NO_ERROR)
  {
          // Establish link:
          if((res = EstablishLink()) == CE_NO_ERROR)
          {
              // Open device.
              if(CFWConnect() == CE_NO_ERROR)
              {
                  DEBUG(INDI::Logger::DBG_SESSION, "SBIG CFW is online. Retrieving basic data.");
                  return true;
              }else
              {
                  DEBUG(INDI::Logger::DBG_ERROR, "CFW connection error!");
                  return false;
              }
          }
          else
          {
                  // Establish link error.
                  str = "Error: Cannot establish link to SBIG CFW. ";
                  str += GetErrorString(res);
                  DEBUGF(INDI::Logger::DBG_ERROR, "%s", str.c_str());
                  return false;

          }
  }
  else
  {
          // Open device error.
          str = "Error: Cannot open SBIG CFW device.";
          str += GetErrorString(res);

          DEBUGF(INDI::Logger::DBG_ERROR, "%s", str.c_str());

          return false;
  }

  return false;
}

bool SBIGCFW::Disconnect()
{
    CFWResults	CFWr;

    // Close device.
    if(CFWDisconnect() != CE_NO_ERROR)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "CFW disconnection error!");
        return false;
    }else
    {
        // Update CFW's Product/ID texts.
        CFWr.cfwModel 		= CFWSEL_UNKNOWN;
        CFWr.cfwPosition    = CFWP_UNKNOWN;
        CFWr.cfwStatus		= CFWS_UNKNOWN;
        CFWr.cfwError		= CFWE_DEVICE_NOT_OPEN;
        CFWr.cfwResult1		= 0;
        CFWr.cfwResult2		= 0;

        DEBUG(INDI::Logger::DBG_SESSION, "CFW disconnected.");
        return true;
    }
}

bool SBIGCFW::saveConfigItems(FILE *fp)
{
    INDI::FilterWheel::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &FilterSlotNP);
    IUSaveConfigText(fp, FilterNameTP);

    return true;
}

//=========================================================================
int SBIGCFW::GetDriverInfo(GetDriverInfoParams *gdip, void *res)
{
    return(SBIGUnivDrvCommand(CC_GET_DRIVER_INFO, gdip, res));
}
//==========================================================================
int SBIGCFW::SetDriverHandle(SetDriverHandleParams *sdhp)
{
    return(SBIGUnivDrvCommand(CC_SET_DRIVER_HANDLE, sdhp, 0));
}
//==========================================================================
int SBIGCFW::GetDriverHandle(GetDriverHandleResults *gdhr)
{
    return(SBIGUnivDrvCommand(CC_GET_DRIVER_HANDLE, 0, gdhr));
}

//==========================================================================
int SBIGCFW::CFW(CFWParams *CFWp, CFWResults *CFWr)
{
    return(SBIGUnivDrvCommand(CC_CFW, CFWp, CFWr));
}
//==========================================================================
int SBIGCFW::EstablishLink()
{
    int 									res;
    EstablishLinkParams  elp;
    EstablishLinkResults elr;

    elp.sbigUseOnly = 0;

    if((res = SBIGUnivDrvCommand(CC_ESTABLISH_LINK, &elp, &elr)) == CE_NO_ERROR){
        //SetCameraType((CAMERA_TYPE)elr.cameraType);
        SetLinkStatus(true);
    }
    return(res);
}
//==========================================================================
int SBIGCFW::QueryCommandStatus(	QueryCommandStatusParams  *qcsp,
                                QueryCommandStatusResults *qcsr)
{
    return(SBIGUnivDrvCommand(CC_QUERY_COMMAND_STATUS, qcsp, qcsr));
}
//==========================================================================
int SBIGCFW::MiscellaneousControl(MiscellaneousControlParams *mcp)
{
    return(SBIGUnivDrvCommand(CC_MISCELLANEOUS_CONTROL, mcp, 0));
}
//==========================================================================
int SBIGCFW::GetLinkStatus(GetLinkStatusResults *glsr)
{
    return(SBIGUnivDrvCommand(CC_GET_LINK_STATUS, glsr, 0));
}
//==========================================================================
string SBIGCFW::GetErrorString(int err)
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
int SBIGCFW::SetDriverControl(SetDriverControlParams *sdcp)
{
    return(SBIGUnivDrvCommand(CC_SET_DRIVER_CONTROL, sdcp, 0));
}
//==========================================================================
int SBIGCFW::GetDriverControl(	GetDriverControlParams  *gdcp,
                                                        GetDriverControlResults *gdcr)
{
    return(SBIGUnivDrvCommand(CC_GET_DRIVER_CONTROL, gdcp, gdcr));
}
//==========================================================================
int SBIGCFW::UsbAdControl(USBADControlParams *usbadcp)
{
    return(SBIGUnivDrvCommand(CC_USB_AD_CONTROL, usbadcp, 0));
}
//==========================================================================
int SBIGCFW::QueryUsb(QueryUSBResults *qusbr)
{
    return(SBIGUnivDrvCommand(CC_QUERY_USB, 0, qusbr));
}
//==========================================================================
int	SBIGCFW::RwUsbI2c(RWUSBI2CParams *rwusbi2cp)
{
    return(SBIGUnivDrvCommand(CC_RW_USB_I2C, rwusbi2cp, 0));
}
//==========================================================================
int	SBIGCFW::BitIo(BitIOParams *biop, BitIOResults *bior)
{
    return(SBIGUnivDrvCommand(CC_BIT_IO, biop, bior));
}

//==========================================================================
int SBIGCFW::SetDeviceName(const char *name)
{
    int res = CE_NO_ERROR;

    if(strlen(name) < MAXRBUF){
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
// class dealing with multiple filterwheels on different communications port.
// Also allows direct access to the SBIG Universal Driver after the driver
// has been opened.
int SBIGCFW::SBIGUnivDrvCommand(PAR_COMMAND command, void *params, void *results)
{
    int res;
    SetDriverHandleParams sdhp;

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
bool SBIGCFW::CheckLink()
{
    if(/*GetCameraType() != NO_CAMERA && */GetLinkStatus()) return(true);
    return(false);
}

//==========================================================================
void SBIGCFW::InitVars()
{
    SetFileDescriptor();
    //SetCameraType();
    SetLinkStatus();
    SetDeviceName("");

}

bool SBIGCFW::SelectFilter(int position)
{
    CFWResults 	CFWr;
    int 					type;
    char					str[64];

    if(CFWGoto(&CFWr, position) == CE_NO_ERROR)
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
            sprintf(str, "Please Connect/Disconnect CFW, then try again...");
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", str);
            return false;
   }

    return false;
}

bool SBIGCFW::SetFilterNames()
{
    // Cannot save it in hardware, so let's just save it in the config file to be loaded later
    saveConfig();
    return true;
}

bool SBIGCFW::GetFilterNames(const char* groupName)
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

int SBIGCFW::QueryFilter()
{
    return CurrentFilter;
}

//==========================================================================

int SBIGCFW::CFWConnect()
{
    int				res;
    CFWResults	CFWr;

    ISwitch *p = IUFindOnSwitch(&FilterTypeSP);
    if(!p) return(CE_OS_ERROR);

    do{
        // 1. CFWC_OPEN_DEVICE:
        if((res = CFWOpenDevice(&CFWr)) != CE_NO_ERROR){
            DEBUGF(INDI::Logger::DBG_ERROR, "CFWC_OPEN_DEVICE error: %s !",  GetErrorString(res).c_str());
            continue;
        }

        // 2. CFWC_INIT:
        if((res = CFWInit(&CFWr)) != CE_NO_ERROR){
            DEBUGF(INDI::Logger::DBG_ERROR,  "CFWC_INIT error: %s !",  GetErrorString(res).c_str());
            CFWCloseDevice(&CFWr);
            DEBUG(INDI::Logger::DBG_DEBUG, "CFWC_CLOSE_DEVICE called.");
            continue;
        }

        // 3. CFWC_GET_INFO:
        if((res = CFWGetInfo(&CFWr)) != CE_NO_ERROR){
            DEBUG(INDI::Logger::DBG_ERROR, "CFWC_GET_INFO error!");
            continue;
        }

        // 4. CFWUpdateProperties:
        CFWUpdateProperties(CFWr);


    }while(0);
    return(res);
}

//==========================================================================

int SBIGCFW::CFWDisconnect()
{
    CFWResults	CFWr;

    ISwitch *p = IUFindOnSwitch(&FilterTypeSP);
    if(!p) return(CE_OS_ERROR);

    deleteProperty(FilterNameTP->name);

    // Close CFW device:
    return(CFWCloseDevice(&CFWr));
}

//==========================================================================

int SBIGCFW::CFWOpenDevice(CFWResults *CFWr)
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

int SBIGCFW::CFWCloseDevice(CFWResults *CFWr)
{
    CFWParams CFWp;

    CFWp.cfwModel 		= GetCFWSelType();
    CFWp.cfwCommand 	= CFWC_CLOSE_DEVICE;
    return(SBIGUnivDrvCommand(CC_CFW, &CFWp, CFWr));
}

//==========================================================================

int SBIGCFW::CFWInit(CFWResults *CFWr)
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

int SBIGCFW::CFWGetInfo(CFWResults *CFWr)
{
    CFWParams CFWp;

    CFWp.cfwModel 		= GetCFWSelType();
    CFWp.cfwCommand 	= CFWC_GET_INFO;
    CFWp.cfwParam1		= CFWG_FIRMWARE_VERSION;
    return(SBIGUnivDrvCommand(CC_CFW, &CFWp, CFWr));
}

//==========================================================================

int SBIGCFW::CFWQuery(CFWResults *CFWr)
{
    CFWParams CFWp;

    CFWp.cfwModel 		= GetCFWSelType();
    CFWp.cfwCommand 	= CFWC_QUERY;
    return(SBIGUnivDrvCommand(CC_CFW, &CFWp, CFWr));
}

//==========================================================================

int SBIGCFW::CFWGoto(CFWResults *CFWr, int position)
{
    int res;
    CFWParams CFWp;

    CFWp.cfwModel 		= GetCFWSelType();
    CFWp.cfwCommand 	= CFWC_GOTO;
    CFWp.cfwParam1	= (unsigned long)position;

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

int SBIGCFW::CFWGotoMonitor(CFWResults *CFWr)
{
    int res;

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

void SBIGCFW::CFWUpdateProperties(CFWResults CFWr)
{
    char 	str[64];
    bool	bClear = false;

    switch(CFWr.cfwModel){
        case	CFWSEL_CFW2:
                    sprintf(str, "%s", "CFW - 2");
                    break;
        case 	CFWSEL_CFW5:
                    sprintf(str, "%s", "CFW - 5");
                    break;
        case 	CFWSEL_CFW6A:
                    sprintf(str, "%s", "CFW - 6A");
                    break;
        case 	CFWSEL_CFW8:
                    sprintf(str, "%s", "CFW - 8");
                    break;
        case 	CFWSEL_CFW402:
                    sprintf(str, "%s", "CFW - 402");
                    break;
        case 	CFWSEL_CFW10:
                    sprintf(str, "%s", "CFW - 10");
                    break;
        case 	CFWSEL_CFW10_SERIAL:
                    sprintf(str, "%s", "CFW - 10SA");
                    break;
        case 	CFWSEL_CFWL:
                    sprintf(str, "%s", "CFW - L");
                    break;
        case	CFWSEL_CFW9:
                    sprintf(str, "%s", "CFW - 9");
                    break ;
        default:
                    sprintf(str, "%s", "Unknown");
                    bClear = true;
                    break;
    }
    // Set CFW's product ID:
    IText *pIText = IUFindText(&FilterProdcutTP, "NAME");
    if(pIText) IUSaveText(pIText, str);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CFW Product ID: %s", str);

    // Set CFW's firmware version:
    if(bClear){
            sprintf(str, "%s", "Unknown");
    }else{
            sprintf(str, "%d", (int)CFWr.cfwResult1);
    }
    pIText = IUFindText(&FilterProdcutTP, "ID");
    if(pIText) IUSaveText(pIText, str);
    FilterProdcutTP.s = IPS_OK;
    IDSetText(&FilterProdcutTP, 0);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CFW Firmware: %s", str);

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

    //defineText(FilterNameTP);

}

//==========================================================================

int SBIGCFW::GetCFWSelType()
{
    int 	type = CFWSEL_UNKNOWN;;
    ISwitch *p = IUFindOnSwitch(&FilterTypeSP);

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
        #ifdef	 USE_CFW_AUTO
        }else if(!strcmp(p->name, "CFW10")){
                type = CFWSEL_AUTO;
        #endif
        }
    }
    return(type);
}

//==========================================================================

void SBIGCFW::CFWShowResults(string name, CFWResults CFWr)
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
