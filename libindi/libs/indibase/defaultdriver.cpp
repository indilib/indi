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

#include "defaultdriver.h"
#include "indicom.h"
#include "base64.h"

const char *COMMUNICATION_TAB = "Communication";
const char *MAIN_CONTROL_TAB = "Main Control";
const char *MOTION_TAB = "Motion Control";
const char *DATETIME_TAB = "Date/Time";
const char *SITE_TAB = 	"Site Management";
const char *OPTIONS_TAB = "Options";
const char *FILTER_TAB = "Filter Wheel";
const char *GUIDER_TAB = "Guide Wheel";

void timerfunc(void *t)
{
    //fprintf(stderr,"Got a timer hit with %x\n",t);
    INDI::DefaultDriver *devPtr = static_cast<INDI::DefaultDriver *> (t);
    if (devPtr != NULL)
    {
        //  this was for my device
        //  but we dont have a way of telling
        //  WHICH timer was hit :(
        devPtr->TimerHit();
    }
    return;
}


INDI::DefaultDriver::DefaultDriver()
{
    pDebug = false;
    pSimulation = false;

    //switchPtr conSw(new ISwitchVectorProperty);
    ConnectionSP = new ISwitchVectorProperty;
    DebugSP = NULL;
    SimulationSP = NULL;
    ConfigProcessSP = NULL;

    IUFillSwitch(&ConnectionS[0],"CONNECT","Connect",ISS_OFF);
    IUFillSwitch(&ConnectionS[1],"DISCONNECT","Disconnect",ISS_ON);
    IUFillSwitchVector(ConnectionSP,ConnectionS,2,deviceName(),"CONNECTION","Connection","Main Control",IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    registerProperty(ConnectionSP, PropertyContainer::INDI_SWITCH);

}

INDI::DefaultDriver::~DefaultDriver()
{
    delete ConnectionSP;
    delete DebugSP;
    delete SimulationSP;
    delete ConfigProcessSP;
}

bool INDI::DefaultDriver::loadConfig()
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

bool INDI::DefaultDriver::saveConfigItems(FILE *fp)
{
    std::vector<PropertyContainer *>::iterator orderi;

    PropertyContainer::INDI_TYPE pType;
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
        case PropertyContainer::INDI_NUMBER:
             nvp = static_cast<INumberVectorProperty *>(pPtr);
             //IDLog("Trying to save config for number %s\n", nvp->name);
             IUSaveConfigNumber(fp, nvp);
             break;
        case PropertyContainer::INDI_TEXT:
             tvp = static_cast<ITextVectorProperty *>(pPtr);
             IUSaveConfigText(fp, tvp);
             break;
        case PropertyContainer::INDI_SWITCH:
             svp = static_cast<ISwitchVectorProperty *>(pPtr);
             /* Never save CONNECTION property. Don't save switches with no switches on if the rule is one of many */
             if (!strcmp(svp->name, "CONNECTION") || (svp->r == ISR_1OFMANY && !IUFindOnSwitch(svp)))
                 continue;
             IUSaveConfigSwitch(fp, svp);
             break;
        case PropertyContainer::INDI_BLOB:
             bvp = static_cast<IBLOBVectorProperty *>(pPtr);
             IUSaveConfigBLOB(fp, bvp);
             break;
        }
    }
	return true;
}

bool INDI::DefaultDriver::saveConfig()
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

bool INDI::DefaultDriver::loadDefaultConfig()
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

bool INDI::DefaultDriver::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // ignore if not ours //
    if (strcmp (dev, deviceID))
        return false;

    ISwitchVectorProperty *svp = getSwitch(name);

    if (!svp)
        return false;

     if(!strcmp(svp->name,ConnectionSP->name))
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
                        setConnected(true, IPS_OK);
                else
                        setConnected(false, IPS_ALERT);


                updateProperties();
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
                setConnected(false, IPS_IDLE);
            else
                setConnected(true, IPS_ALERT);

            updateProperties();
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

    return false;

}

void INDI::DefaultDriver::addDebugControl()
{
    /**************************************************************************/
    DebugSP = getSwitch("DEBUG");
    if (!DebugSP)
    {
        DebugSP = new ISwitchVectorProperty;
        IUFillSwitch(&DebugS[0], "ENABLE", "Enable", ISS_OFF);
        IUFillSwitch(&DebugS[1], "DISABLE", "Disable", ISS_ON);
        IUFillSwitchVector(DebugSP, DebugS, NARRAY(DebugS), deviceName(), "DEBUG", "Debug", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        registerProperty(DebugSP, PropertyContainer::INDI_SWITCH);
    }
    else
    {
        ISwitch *sp = IUFindSwitch(DebugSP, "ENABLE");
        if (sp)
            if (sp->s == ISS_ON)
                pDebug = true;
    }
    /**************************************************************************/

}

void INDI::DefaultDriver::addSimulationControl()
{
    /**************************************************************************/
    SimulationSP = getSwitch("SIMULATION");
    if (!SimulationSP)
    {
        SimulationSP = new ISwitchVectorProperty;
        IUFillSwitch(&SimulationS[0], "ENABLE", "Enable", ISS_OFF);
        IUFillSwitch(&SimulationS[1], "DISABLE", "Disable", ISS_ON);
        IUFillSwitchVector(SimulationSP, SimulationS, NARRAY(SimulationS), deviceName(), "SIMULATION", "Simulation", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        registerProperty(SimulationSP, PropertyContainer::INDI_SWITCH);
    }
    else
    {
        ISwitch *sp = IUFindSwitch(SimulationSP, "ENABLE");
        if (sp)
            if (sp->s == ISS_ON)
                pSimulation = true;
    }
}

void INDI::DefaultDriver::addConfigurationControl()
{
    /**************************************************************************/
    ConfigProcessSP = getSwitch("CONFIG_PROCESS");
    if (!ConfigProcessSP)
    {
        ConfigProcessSP = new ISwitchVectorProperty;
        IUFillSwitch(&ConfigProcessS[0], "CONFIG_LOAD", "Load", ISS_OFF);
        IUFillSwitch(&ConfigProcessS[1], "CONFIG_SAVE", "Save", ISS_OFF);
        IUFillSwitch(&ConfigProcessS[2], "CONFIG_DEFAULT", "Default", ISS_OFF);
        IUFillSwitchVector(ConfigProcessSP, ConfigProcessS, NARRAY(ConfigProcessS), deviceName(), "CONFIG_PROCESS", "Configuration", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        registerProperty(ConfigProcessSP, PropertyContainer::INDI_SWITCH);
    }
    /**************************************************************************/

}

void INDI::DefaultDriver::addAuxControls()
{
   addDebugControl();
   addSimulationControl();
   addConfigurationControl();
}

void INDI::DefaultDriver::setDebug(bool enable)
{
    if (!DebugSP)
        return;

    if (pDebug == enable)
    {
        DebugSP->s = IPS_OK;
        IDSetSwitch(DebugSP, NULL);
        return;
    }

    IUResetSwitch(DebugSP);

    if (enable)
    {
        ISwitch *sp = IUFindSwitch(DebugSP, "ENABLE");
        if (sp)
        {
            sp->s = ISS_ON;
            IDMessage(deviceID, "Debug is enabled.");
        }
    }
    else
    {
        ISwitch *sp = IUFindSwitch(DebugSP, "DISABLE");
        if (sp)
        {
            sp->s = ISS_ON;
            IDMessage(deviceID, "Debug is disabled.");
        }
    }

    pDebug = enable;
    DebugSP->s = IPS_OK;
    IDSetSwitch(DebugSP, NULL);

}

void INDI::DefaultDriver::setSimulation(bool enable)
{
    if (!SimulationSP)
        return;

   if (pSimulation == enable)
   {
       SimulationSP->s = IPS_OK;
       IDSetSwitch(SimulationSP, NULL);
       return;
   }

   IUResetSwitch(SimulationSP);

   if (enable)
   {
       ISwitch *sp = IUFindSwitch(SimulationSP, "ENABLE");
       if (sp)
       {
           IDMessage(deviceID, "Simulation is enabled.");
           sp->s = ISS_ON;
       }
   }
   else
   {
       ISwitch *sp = IUFindSwitch(SimulationSP, "DISABLE");
       if (sp)
       {
           sp->s = ISS_ON;
           IDMessage(deviceID, "Simulation is disabled.");
       }
   }

   pSimulation = enable;
   SimulationSP->s = IPS_OK;
   IDSetSwitch(SimulationSP, NULL);

}

bool INDI::DefaultDriver::isDebug()
{
  return pDebug;
}

bool INDI::DefaultDriver::isSimulation()
{
 return pSimulation;
}

void INDI::DefaultDriver::ISGetProperties (const char *dev)
{
    std::vector<PropertyContainer *>::iterator orderi;
    static int isInit = 0;
    PropertyContainer::INDI_TYPE pType;
    void *pPtr;

    if(isInit == 0)
    {
        if(dev != NULL)
             setDeviceName(dev);
        else
        {
            char *envDev = getenv("INDIDEV");
            if (envDev != NULL)
                setDeviceName(envDev);
            else
               setDeviceName(getDefaultName());
        }

        strncpy(ConnectionSP->device, deviceName(), MAXINDIDEVICE);
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
        case PropertyContainer::INDI_NUMBER:
             IDDefNumber(static_cast<INumberVectorProperty *>(pPtr) , NULL);
             break;
        case PropertyContainer::INDI_TEXT:
             IDDefText(static_cast<ITextVectorProperty *>(pPtr) , NULL);
             break;
        case PropertyContainer::INDI_SWITCH:
             IDDefSwitch(static_cast<ISwitchVectorProperty *>(pPtr) , NULL);
             break;
        case PropertyContainer::INDI_LIGHT:
             IDDefLight(static_cast<ILightVectorProperty *>(pPtr) , NULL);
             break;
        case PropertyContainer::INDI_BLOB:
             IDDefBLOB(static_cast<IBLOBVectorProperty *>(pPtr) , NULL);
             break;
        }
    }
}

void INDI::DefaultDriver::resetProperties()
{
    /*std::vector<numberPtr>::const_iterator numi;
    std::vector<switchPtr>::const_iterator switchi;
    std::vector<textPtr>::const_iterator texti;
    std::vector<lightPtr>::const_iterator lighti;
    std::vector<blobPtr>::const_iterator blobi;

    for ( numi = pNumbers.begin(); numi != pNumbers.end(); numi++)
    {
        (*numi)->s = IPS_IDLE;
        IDSetNumber( (*numi).get(), NULL);
    }

   for ( switchi = pSwitches.begin(); switchi != pSwitches.end(); switchi++)
   {
       (*switchi)->s = IPS_IDLE;
       IDSetSwitch( (*switchi).get(), NULL);
   }

   for ( texti = pTexts.begin(); texti != pTexts.end(); texti++)
   {
      (*texti)->s = IPS_IDLE;
      IDSetText( (*texti).get(), NULL);
   }

   for ( lighti = pLights.begin(); lighti != pLights.end(); lighti++)
   {
       (*lighti)->s = IPS_IDLE;
       IDSetLight( (*lighti).get(), NULL);
   }

   for ( blobi = pBlobs.begin(); blobi != pBlobs.end(); blobi++)
   {
       (*blobi)->s = IPS_IDLE;
       IDSetBLOB( (*blobi).get(), NULL);
   }*/
}

void INDI::DefaultDriver::setConnected(bool status, IPState state, const char *msg)
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
int INDI::DefaultDriver::SetTimer(int t)
{
    return IEAddTimer(t,timerfunc,this);
}

//  Just another helper to help encapsulate indi into a clean class
void INDI::DefaultDriver::RemoveTimer(int t)
{
    IERmTimer(t);
    return;
}

//  This is just a placeholder
//  This function should be overriden by child classes if they use timers
//  So we should never get here
void INDI::DefaultDriver::TimerHit()
{
    return;
}


bool INDI::DefaultDriver::updateProperties()
{
    //  The base device has no properties to update
    return true;
}


bool INDI::DefaultDriver::initProperties()
{
   return true;
}

bool INDI::DefaultDriver::deleteProperty(const char *propertyName)
{
    removeProperty(propertyName);
    IDDelete(deviceName(), propertyName ,NULL);
    return true;
}

void INDI::DefaultDriver::defineNumber(INumberVectorProperty *nvp)
{
    registerProperty(nvp, PropertyContainer::INDI_NUMBER);
    IDDefNumber(nvp, NULL);
}

void INDI::DefaultDriver::defineText(ITextVectorProperty *tvp)
{
    registerProperty(tvp, PropertyContainer::INDI_TEXT);
    IDDefText(tvp, NULL);
}

void INDI::DefaultDriver::defineSwitch(ISwitchVectorProperty *svp)
{
    registerProperty(svp, PropertyContainer::INDI_SWITCH);
    IDDefSwitch(svp, NULL);
}

void INDI::DefaultDriver::defineLight(ILightVectorProperty *lvp)
{
    registerProperty(lvp, PropertyContainer::INDI_LIGHT);
    IDDefLight(lvp, NULL);
}

void INDI::DefaultDriver::defineBLOB(IBLOBVectorProperty *bvp)
{
    registerProperty(bvp, PropertyContainer::INDI_BLOB);
    IDDefBLOB(bvp, NULL);
}
