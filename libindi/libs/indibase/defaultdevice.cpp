/*******************************************************************************
  Copyright(c) 2011 Jasem Mutlaq. All rights reserved.

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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <zlib.h>

#include "defaultdevice.h"
#include "indicom.h"
#include "base64.h"
#include "indiproperty.h"

const char *COMMUNICATION_TAB = "Communication";
const char *MAIN_CONTROL_TAB = "Main Control";
const char *MOTION_TAB = "Motion Control";
const char *DATETIME_TAB = "Date/Time";
const char *SITE_TAB = 	"Site Management";
const char *OPTIONS_TAB = "Options";
const char *FILTER_TAB = "Filter Wheel";
const char *FOCUS_TAB = "Focuser";
const char *GUIDE_TAB = "Guide";

void timerfunc(void *t)
{
    //fprintf(stderr,"Got a timer hit with %x\n",t);
    INDI::DefaultDevice *devPtr = static_cast<INDI::DefaultDevice *> (t);
    if (devPtr != NULL)
    {
        //  this was for my device
        //  but we dont have a way of telling
        //  WHICH timer was hit :(
        devPtr->TimerHit();
    }
    return;
}


INDI::DefaultDevice::DefaultDevice()
{
    pDebug = false;
    pSimulation = false;
    isInit = false;

    majorVersion = 1;
    minorVersion = 0;
}

INDI::DefaultDevice::~DefaultDevice()
{
}

bool INDI::DefaultDevice::loadConfig()
{
    char errmsg[MAXRBUF];
    bool pResult = false;

    pResult = IUReadConfig(NULL, deviceID, errmsg) == 0 ? true : false;

   if (pResult)
       IDMessage(deviceID, "Configuration successfully loaded.\n");
	else
		IDMessage(deviceID,"Error loading configuration\n");

   IUSaveDefaultConfig(NULL, NULL, deviceID);

   return pResult;
}

bool INDI::DefaultDevice::saveConfigItems(FILE *fp)
{
    std::vector<INDI::Property *>::iterator orderi;

    INDI_TYPE pType;
    void *pPtr;

    ISwitchVectorProperty *svp=NULL;
    INumberVectorProperty *nvp=NULL;
    ITextVectorProperty   *tvp=NULL;
    IBLOBVectorProperty   *bvp=NULL;

    for (orderi = pAll.begin(); orderi != pAll.end(); orderi++)
    {

        pType       = (*orderi)->getType();
        pPtr        = (*orderi)->getProperty();

        switch (pType)
        {
        case INDI_NUMBER:
             nvp = static_cast<INumberVectorProperty *>(pPtr);
             //IDLog("Trying to save config for number %s\n", nvp->name);
             IUSaveConfigNumber(fp, nvp);
             break;
        case INDI_TEXT:
             tvp = static_cast<ITextVectorProperty *>(pPtr);
             IUSaveConfigText(fp, tvp);
             break;
        case INDI_SWITCH:
             svp = static_cast<ISwitchVectorProperty *>(pPtr);
             /* Never save CONNECTION property. Don't save switches with no switches on if the rule is one of many */
             if (!strcmp(svp->name, "CONNECTION") || (svp->r == ISR_1OFMANY && !IUFindOnSwitch(svp)))
                 continue;
             IUSaveConfigSwitch(fp, svp);
             break;
        case INDI_BLOB:
             bvp = static_cast<IBLOBVectorProperty *>(pPtr);
             IUSaveConfigBLOB(fp, bvp);
             break;
        }
    }
	return true;
}

bool INDI::DefaultDevice::saveConfig()
{
    //std::vector<orderPtr>::iterator orderi;
    char errmsg[MAXRBUF];

    FILE *fp = NULL;

    fp = IUGetConfigFP(NULL, deviceID, errmsg);

    if (fp == NULL)
    {
        IDMessage(deviceID, "Error saving configuration. %s\n", errmsg);
        return false;
    }

    IUSaveConfigTag(fp, 0);

	saveConfigItems(fp);

    IUSaveConfigTag(fp, 1);

    fclose(fp);

    IUSaveDefaultConfig(NULL, NULL, deviceID);

    IDMessage(deviceID, "Configuration successfully saved.");

    return true;
}

bool INDI::DefaultDevice::loadDefaultConfig()
{
    char configDefaultFileName[MAXRBUF];
    char errmsg[MAXRBUF];
    bool pResult = false;

    if (getenv("INDICONFIG"))
        snprintf(configDefaultFileName, MAXRBUF, "%s.default", getenv("INDICONFIG"));
    else
        snprintf(configDefaultFileName, MAXRBUF, "%s/.indi/%s_config.xml.default", getenv("HOME"), deviceID);

    if (pDebug)
        IDLog("Requesting to load default config with: %s\n", configDefaultFileName);

    pResult = IUReadConfig(configDefaultFileName, deviceID, errmsg) == 0 ? true : false;

    if (pResult)
        IDMessage(deviceID, "Default configuration loaded.");
    else
        IDMessage(deviceID, "Error loading default configuraiton. %s", errmsg);

    return pResult;
}

bool INDI::DefaultDevice::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // ignore if not ours //
    if (strcmp (dev, deviceID))
        return false;

    ISwitchVectorProperty *svp = getSwitch(name);

    if (!svp)
        return false;

     if(!strcmp(svp->name,ConnectionSP.name))
     {
        bool rc;
        
      for (int i=0; i < n; i++)
      {
        if ( !strcmp(names[i], "CONNECT") && (states[i] == ISS_ON))
        {

            // If not connected, attempt to connect
            if (isConnected() == false)
            {
                rc = Connect();

                // If connection is successful, set it thus
                if (rc)
                {
                        setConnected(true, IPS_OK);
                        updateProperties();
                }
                else
                        setConnected(false, IPS_ALERT);



            }
            else
                // Just tell client we're connected yes
                setConnected(true);
        }
        else if ( !strcmp(names[i], "DISCONNECT") && (states[i] == ISS_ON))
        {
            // If connected, then true to disconnect.
            if (isConnected() == true)
                rc = Disconnect();
            else
                rc = true;

            if (rc)
            {
                setConnected(false, IPS_IDLE);
                updateProperties();
            }
            else
                setConnected(true, IPS_ALERT);


        }
    }

        return true;
    }


    if (!strcmp(svp->name, "DEBUG"))
    {
        IUUpdateSwitch(svp, states, names, n);
        ISwitch *sp = IUFindOnSwitch(svp);
        if (!sp)
            return false;

        if (!strcmp(sp->name, "ENABLE"))
            setDebug(true);
        else
            setDebug(false);
        return true;
    }

    if (!strcmp(svp->name, "SIMULATION"))
    {
        IUUpdateSwitch(svp, states, names, n);
        ISwitch *sp = IUFindOnSwitch(svp);
        if (!sp)
            return false;

        if (!strcmp(sp->name, "ENABLE"))
            setSimulation(true);
        else
            setSimulation(false);
        return true;
    }

    if (!strcmp(svp->name, "CONFIG_PROCESS"))
    {
        IUUpdateSwitch(svp, states, names, n);
        ISwitch *sp = IUFindOnSwitch(svp);
        IUResetSwitch(svp);
        bool pResult = false;
        if (!sp)
            return false;

        if (!strcmp(sp->name, "CONFIG_LOAD"))
            pResult = loadConfig();
        else if (!strcmp(sp->name, "CONFIG_SAVE"))
            pResult = saveConfig();
        else if (!strcmp(sp->name, "CONFIG_DEFAULT"))
            pResult = loadDefaultConfig();

        if (pResult)
            svp->s = IPS_OK;
        else
            svp->s = IPS_ALERT;

        IDSetSwitch(svp, NULL);
        return true;
    }

    if (!strcmp(svp->name, "DEBUG_LEVEL") || !strcmp(svp->name, "LOGGING_LEVEL") || !strcmp(svp->name, "LOG_OUTPUT"))
    {
        return Logger::ISNewSwitch(dev, name, states, names, n);
    }

    return false;

}

void INDI::DefaultDevice::addDebugControl()
{
    registerProperty(&DebugSP, INDI_SWITCH);
    pDebug = false;

}

void INDI::DefaultDevice::addSimulationControl()
{
        registerProperty(&SimulationSP, INDI_SWITCH);
        pSimulation = false;
}

void INDI::DefaultDevice::addConfigurationControl()
{
        registerProperty(&ConfigProcessSP, INDI_SWITCH);
}

void INDI::DefaultDevice::addAuxControls()
{
   addDebugControl();
   addSimulationControl();
   addConfigurationControl();
}

void INDI::DefaultDevice::setDebug(bool enable)
{
    if (pDebug == enable)
    {
        DebugSP.s = IPS_OK;
        IDSetSwitch(&DebugSP, NULL);
        return;
    }

    IUResetSwitch(&DebugSP);

    if (enable)
    {
        ISwitch *sp = IUFindSwitch(&DebugSP, "ENABLE");
        if (sp)
        {
            sp->s = ISS_ON;
            IDMessage(deviceID, "Debug is enabled.");
        }
    }
    else
    {
        ISwitch *sp = IUFindSwitch(&DebugSP, "DISABLE");
        if (sp)
        {
            sp->s = ISS_ON;
            IDMessage(deviceID, "Debug is disabled.");
        }
    }

    pDebug = enable;

    // Inform logger
    if (Logger::updateProperties(enable, this) == false)
      DEBUG(Logger::DBG_WARNING,"setLogDebug: Logger error");

    debugTriggered(enable);
    DebugSP.s = IPS_OK;
    IDSetSwitch(&DebugSP, NULL);

}

void INDI::DefaultDevice::setSimulation(bool enable)
{

   if (pSimulation == enable)
   {
       SimulationSP.s = IPS_OK;
       IDSetSwitch(&SimulationSP, NULL);
       return;
   }

   IUResetSwitch(&SimulationSP);

   if (enable)
   {
       ISwitch *sp = IUFindSwitch(&SimulationSP, "ENABLE");
       if (sp)
       {
           IDMessage(deviceID, "Simulation is enabled.");
           sp->s = ISS_ON;
       }
   }
   else
   {
       ISwitch *sp = IUFindSwitch(&SimulationSP, "DISABLE");
       if (sp)
       {
           sp->s = ISS_ON;
           IDMessage(deviceID, "Simulation is disabled.");
       }
   }

   pSimulation = enable;
   simulationTriggered(enable);
   SimulationSP.s = IPS_OK;
   IDSetSwitch(&SimulationSP, NULL);

}

bool INDI::DefaultDevice::isDebug()
{
  return pDebug;
}

bool INDI::DefaultDevice::isSimulation()
{
 return pSimulation;
}

void INDI::DefaultDevice::debugTriggered(bool enable)
{
    INDI_UNUSED(enable);
}

void INDI::DefaultDevice::simulationTriggered(bool enable)
{
    INDI_UNUSED(enable);
}

void INDI::DefaultDevice::ISGetProperties (const char *dev)
{
    std::vector<INDI::Property *>::iterator orderi;
    INDI_TYPE pType;
    void *pPtr;

    if(isInit == 0)
    {
        if(dev != NULL)
             setDeviceName(dev);
        else if (*getDeviceName() == '\0')
        {
            char *envDev = getenv("INDIDEV");
            if (envDev != NULL)
                setDeviceName(envDev);
            else
               setDeviceName(getDefaultName());
        }

        strncpy(ConnectionSP.device, getDeviceName(), MAXINDIDEVICE);
        initProperties();
        addConfigurationControl();

        isInit = 1;
    }

    for (orderi = pAll.begin(); orderi != pAll.end(); orderi++)
    {
        pType       = (*orderi)->getType();
        pPtr        = (*orderi)->getProperty();

        switch (pType)
        {
        case INDI_NUMBER:
             IDDefNumber(static_cast<INumberVectorProperty *>(pPtr) , NULL);
             break;
        case INDI_TEXT:
             IDDefText(static_cast<ITextVectorProperty *>(pPtr) , NULL);
             break;
        case INDI_SWITCH:
             IDDefSwitch(static_cast<ISwitchVectorProperty *>(pPtr) , NULL);
             break;
        case INDI_LIGHT:
             IDDefLight(static_cast<ILightVectorProperty *>(pPtr) , NULL);
             break;
        case INDI_BLOB:
             IDDefBLOB(static_cast<IBLOBVectorProperty *>(pPtr) , NULL);
             break;
        }
    }

}

void INDI::DefaultDevice::resetProperties()
{
    std::vector<INDI::Property *>::iterator orderi;
    INDI_TYPE pType;
    void *pPtr;

    for (orderi = pAll.begin(); orderi != pAll.end(); orderi++)
    {
        pType       = (*orderi)->getType();
        pPtr        = (*orderi)->getProperty();

        switch (pType)
        {
        case INDI_NUMBER:
            static_cast<INumberVectorProperty *>(pPtr)->s = IPS_IDLE;
            IDSetNumber(static_cast<INumberVectorProperty *>(pPtr) , NULL);
             break;
        case INDI_TEXT:
            static_cast<ITextVectorProperty *>(pPtr)->s = IPS_IDLE;
            IDSetText(static_cast<ITextVectorProperty *>(pPtr) , NULL);
             break;
        case INDI_SWITCH:
            static_cast<ISwitchVectorProperty *>(pPtr)->s = IPS_IDLE;
            IDSetSwitch(static_cast<ISwitchVectorProperty *>(pPtr) , NULL);
             break;
        case INDI_LIGHT:
            static_cast<ILightVectorProperty *>(pPtr)->s = IPS_IDLE;
            IDSetLight(static_cast<ILightVectorProperty *>(pPtr) , NULL);
             break;
        case INDI_BLOB:
            static_cast<IBLOBVectorProperty *>(pPtr)->s = IPS_IDLE;
            IDSetBLOB(static_cast<IBLOBVectorProperty *>(pPtr) , NULL);
             break;
        }
    }
}

void INDI::DefaultDevice::setConnected(bool status, IPState state, const char *msg)
{
    ISwitch *sp = NULL;
    ISwitchVectorProperty *svp = getSwitch("CONNECTION");
    if (!svp)
        return;

    IUResetSwitch(svp);

    // Connect
    if (status)
    {
        sp = IUFindSwitch(svp, "CONNECT");
        if (!sp)
            return;
        sp->s = ISS_ON;
    }
    // Disconnect
    else
    {
        sp = IUFindSwitch(svp, "DISCONNECT");
        if (!sp)
            return;
        sp->s = ISS_ON;
    }

    svp->s = state;

    IDSetSwitch(svp, msg, NULL);
}

//  This is a helper function
//  that just encapsulates the Indi way into our clean c++ way of doing things
int INDI::DefaultDevice::SetTimer(int ms)
{
    return IEAddTimer(ms,timerfunc,this);
}

//  Just another helper to help encapsulate indi into a clean class
void INDI::DefaultDevice::RemoveTimer(int id)
{
    IERmTimer(id);
    return;
}

//  This is just a placeholder
//  This function should be overriden by child classes if they use timers
//  So we should never get here
void INDI::DefaultDevice::TimerHit()
{
    return;
}


bool INDI::DefaultDevice::updateProperties()
{
    //  The base device has no properties to update
    return true;
}


bool INDI::DefaultDevice::initProperties()
{
    char versionStr[16];

    snprintf(versionStr, 16, "%d.%d", majorVersion, minorVersion);

    IUFillSwitch(&ConnectionS[0],"CONNECT","Connect",ISS_OFF);
    IUFillSwitch(&ConnectionS[1],"DISCONNECT","Disconnect",ISS_ON);
    IUFillSwitchVector(&ConnectionSP,ConnectionS,2,getDeviceName(),"CONNECTION","Connection","Main Control",IP_RW,ISR_1OFMANY,60,IPS_IDLE);
    registerProperty(&ConnectionSP, INDI_SWITCH);

    IUFillText(&DriverInfoT[0],"NAME","Name",getDefaultName());
    IUFillText(&DriverInfoT[1],"EXEC","Exec",getDriverName());
    IUFillText(&DriverInfoT[2],"VERSION","Version",versionStr);
    IUFillTextVector(&DriverInfoTP,DriverInfoT,3,getDeviceName(),"DRIVER_INFO","Driver Info",OPTIONS_TAB,IP_RO,60,IPS_IDLE);
    registerProperty(&DriverInfoTP, INDI_TEXT);

    IUFillSwitch(&DebugS[0], "ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&DebugS[1], "DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&DebugSP, DebugS, NARRAY(DebugS), getDeviceName(), "DEBUG", "Debug", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&SimulationS[0], "ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&SimulationS[1], "DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&SimulationSP, SimulationS, NARRAY(SimulationS), getDeviceName(), "SIMULATION", "Simulation", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&ConfigProcessS[0], "CONFIG_LOAD", "Load", ISS_OFF);
    IUFillSwitch(&ConfigProcessS[1], "CONFIG_SAVE", "Save", ISS_OFF);
    IUFillSwitch(&ConfigProcessS[2], "CONFIG_DEFAULT", "Default", ISS_OFF);
    IUFillSwitchVector(&ConfigProcessSP, ConfigProcessS, NARRAY(ConfigProcessS), getDeviceName(), "CONFIG_PROCESS", "Configuration", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

   return true;
}

bool INDI::DefaultDevice::deleteProperty(const char *propertyName)
{
    char errmsg[MAXRBUF];

    removeProperty(propertyName, errmsg);
    IDDelete(getDeviceName(), propertyName ,NULL);
    return true;
}

void INDI::DefaultDevice::defineNumber(INumberVectorProperty *nvp)
{
    registerProperty(nvp, INDI_NUMBER);
    IDDefNumber(nvp, NULL);
}

void INDI::DefaultDevice::defineText(ITextVectorProperty *tvp)
{
    registerProperty(tvp, INDI_TEXT);
    IDDefText(tvp, NULL);
}

void INDI::DefaultDevice::defineSwitch(ISwitchVectorProperty *svp)
{
    registerProperty(svp, INDI_SWITCH);
    IDDefSwitch(svp, NULL);
}

void INDI::DefaultDevice::defineLight(ILightVectorProperty *lvp)
{
    registerProperty(lvp, INDI_LIGHT);
    IDDefLight(lvp, NULL);
}

void INDI::DefaultDevice::defineBLOB(IBLOBVectorProperty *bvp)
{
    registerProperty(bvp, INDI_BLOB);
    IDDefBLOB(bvp, NULL);
}
