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

    IUFillSwitch(&ConnectionS[0],"CONNECT","Connect",ISS_OFF);
    IUFillSwitch(&ConnectionS[1],"DISCONNECT","Disconnect",ISS_ON);
    IUFillSwitchVector(ConnectionSP,ConnectionS,2,deviceName(),"CONNECTION","Connection","Main Control",IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    registerProperty(ConnectionSP, INDI_SWITCH);

}

bool INDI::DefaultDriver::loadConfig()
{
    char errmsg[MAXRBUF];
    bool pResult = false;

    pResult = IUReadConfig(NULL, deviceID, errmsg) == 0 ? true : false;

   if (pResult)
       IDMessage(deviceID, "Configuration successfully loaded.");

   IUSaveDefaultConfig(NULL, NULL, deviceID);

   return pResult;
}

bool INDI::DefaultDriver::saveConfig()
{
    //std::vector<orderPtr>::iterator orderi;

    std::map< boost::shared_ptr<void>, INDI_TYPE>::iterator orderi;

    ISwitchVectorProperty *svp=NULL;
    INumberVectorProperty *nvp=NULL;
    ITextVectorProperty   *tvp=NULL;
    IBLOBVectorProperty   *bvp=NULL;
    char errmsg[MAXRBUF];
    FILE *fp = NULL;

    fp = IUGetConfigFP(NULL, deviceID, errmsg);

    if (fp == NULL)
    {
        IDMessage(deviceID, "Error saving configuration. %s", errmsg);
        return false;
    }

    IUSaveConfigTag(fp, 0);

    for (orderi = pAll.begin(); orderi != pAll.end(); orderi++)
    {

        switch (orderi->second)
        {
        case INDI_NUMBER:
             nvp = static_cast<INumberVectorProperty *>((orderi->first).get());
             IDLog("Trying to save config for number %s\n", nvp->name);
             IUSaveConfigNumber(fp, nvp);
             break;
        case INDI_TEXT:
             tvp = static_cast<ITextVectorProperty *>((orderi->first).get());
             IUSaveConfigText(fp, tvp);
             break;
        case INDI_SWITCH:
             svp = static_cast<ISwitchVectorProperty *>((orderi->first).get());
             /* Never save CONNECTION property. Don't save switches with no switches on if the rule is one of many */
             if (!strcmp(svp->name, "CONNECTION") || (svp->r == ISR_1OFMANY && !IUFindOnSwitch(svp)))
                 continue;
             IUSaveConfigSwitch(fp, svp);
             break;
        case INDI_BLOB:
             bvp = static_cast<IBLOBVectorProperty *>((orderi->first).get());
             IUSaveConfigBLOB(fp, bvp);
             break;
        }
    }

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
                  setConnected(true);
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
        switchPtr debSw(new ISwitchVectorProperty);
        DebugSP = debSw.get();
        IUFillSwitch(&DebugS[0], "ENABLE", "Enable", ISS_OFF);
        IUFillSwitch(&DebugS[1], "DISABLE", "Disable", ISS_ON);
        IUFillSwitchVector(DebugSP, DebugS, NARRAY(DebugS), deviceID, "DEBUG", "Debug", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        pAll[debSw] = INDI_SWITCH;
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
        switchPtr simSw(new ISwitchVectorProperty);
        SimulationSP = simSw.get();
        IUFillSwitch(&SimulationS[0], "ENABLE", "Enable", ISS_OFF);
        IUFillSwitch(&SimulationS[1], "DISABLE", "Disable", ISS_ON);
        IUFillSwitchVector(SimulationSP, SimulationS, NARRAY(SimulationS), deviceID, "SIMULATION", "Simulation", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        pAll[simSw] = INDI_SWITCH;
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
        switchPtr configSw(new ISwitchVectorProperty);
        ConfigProcessSP = configSw.get();
        IUFillSwitch(&ConfigProcessS[0], "CONFIG_LOAD", "Load", ISS_OFF);
        IUFillSwitch(&ConfigProcessS[1], "CONFIG_SAVE", "Save", ISS_OFF);
        IUFillSwitch(&ConfigProcessS[2], "CONFIG_DEFAULT", "Default", ISS_OFF);
        IUFillSwitchVector(ConfigProcessSP, ConfigProcessS, NARRAY(ConfigProcessS), deviceID, "CONFIG_PROCESS", "Configuration", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        pAll[configSw] = INDI_SWITCH;
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
    std::map< boost::shared_ptr<void>, INDI_TYPE>::iterator orderi;
    static int isInit = 0;

    //fprintf(stderr,"Enter ISGetProperties '%s'\n",dev);
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
        switch (orderi->second)
        {
        case INDI_NUMBER:
             IDDefNumber(static_cast<INumberVectorProperty *>((orderi->first).get()) , NULL);
             break;
        case INDI_TEXT:
             IDDefText(static_cast<ITextVectorProperty *>((orderi->first).get()) , NULL);
             break;
        case INDI_SWITCH:
             IDDefSwitch(static_cast<ISwitchVectorProperty *>((orderi->first).get()) , NULL);
             break;
        case INDI_LIGHT:
             IDDefLight(static_cast<ILightVectorProperty *>((orderi->first).get()) , NULL);
             break;
        case INDI_BLOB:
             IDDefBLOB(static_cast<IBLOBVectorProperty *>((orderi->first).get()) , NULL);
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
    registerProperty(nvp, INDI_NUMBER);
    IDDefNumber(nvp, NULL);
}

void INDI::DefaultDriver::defineText(ITextVectorProperty *tvp)
{
    registerProperty(tvp, INDI_TEXT);
    IDDefText(tvp, NULL);
}

void INDI::DefaultDriver::defineSwitch(ISwitchVectorProperty *svp)
{
    registerProperty(svp, INDI_SWITCH);
    IDDefSwitch(svp, NULL);
}

void INDI::DefaultDriver::defineLight(ILightVectorProperty *lvp)
{
    registerProperty(lvp, INDI_LIGHT);
    IDDefLight(lvp, NULL);
}

void INDI::DefaultDriver::defineBLOB(IBLOBVectorProperty *bvp)
{
    registerProperty(bvp, INDI_BLOB);
    IDDefBLOB(bvp, NULL);
}
