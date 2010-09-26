#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <zlib.h>

#include "indidrivermain.h"
#include "basedevice.h"
#include "indicom.h"
#include "base64.h"

INDI::BaseDevice::BaseDevice()
{
    lp = newLilXML();
    pDebug = false;
    pSimulation = false;
}


INDI::BaseDevice::~BaseDevice()
{
    std::vector<INumberVectorProperty *>::const_iterator numi;
    std::vector<ISwitchVectorProperty *>::const_iterator switchi;
    std::vector<ITextVectorProperty *>::const_iterator texti;
    std::vector<ILightVectorProperty *>::const_iterator lighti;
    std::vector<IBLOBVectorProperty *>::const_iterator blobi;

    delLilXML (lp);
    pAll.clear();

    for ( numi = pNumbers.begin(); numi != pNumbers.end(); numi++)
        delete (*numi);

   for ( switchi = pSwitches.begin(); switchi != pSwitches.end(); switchi++)
        delete (*switchi);

   for ( texti = pTexts.begin(); texti != pTexts.end(); texti++)
        delete (*texti);

   for ( lighti = pLights.begin(); lighti != pLights.end(); lighti++)
        delete (*lighti);

   for ( blobi = pBlobs.begin(); blobi != pBlobs.end(); blobi++)
        delete (*blobi);
}

INumberVectorProperty * INDI::BaseDevice::getNumber(const char *name)
{
    std::vector<INumberVectorProperty *>::const_iterator numi;

    for ( numi = pNumbers.begin(); numi != pNumbers.end(); numi++)
        if (!strcmp(name, (*numi)->name))
            return *numi;

    return NULL;

}

ITextVectorProperty * INDI::BaseDevice::getText(const char *name)
{
    std::vector<ITextVectorProperty *>::const_iterator texti;

    for ( texti = pTexts.begin(); texti != pTexts.end(); texti++)
        if (!strcmp(name, (*texti)->name))
            return *texti;

    return NULL;
}

ISwitchVectorProperty * INDI::BaseDevice::getSwitch(const char *name)
{
    std::vector<ISwitchVectorProperty *>::const_iterator switchi;

    for ( switchi = pSwitches.begin(); switchi != pSwitches.end(); switchi++)
        if (!strcmp(name, (*switchi)->name))
            return *switchi;

    return NULL;

}

ILightVectorProperty * INDI::BaseDevice::getLight(const char *name)
{
    std::vector<ILightVectorProperty *>::const_iterator lighti;

    for ( lighti = pLights.begin(); lighti != pLights.end(); lighti++)
        if (!strcmp(name, (*lighti)->name))
            return *lighti;

    return NULL;

}

IBLOBVectorProperty * INDI::BaseDevice::getBLOB(const char *name)
{
    std::vector<IBLOBVectorProperty *>::const_iterator blobi;

    for ( blobi = pBlobs.begin(); blobi != pBlobs.end(); blobi++)
        if (!strcmp(name, (*blobi)->name))
            return *blobi;

    return NULL;
}

void INDI::BaseDevice::ISGetProperties (const char *dev)
{
    std::vector<pOrder>::const_iterator orderi;

    for (orderi = pAll.begin(); orderi != pAll.end(); orderi++)
    {
        switch ( (*orderi).type)
        {
        case INDI_NUMBER:
             IDDefNumber(static_cast<INumberVectorProperty *>((*orderi).p) , NULL);
             break;
        case INDI_TEXT:
             IDDefText(static_cast<ITextVectorProperty *>((*orderi).p) , NULL);
             break;
        case INDI_SWITCH:
             IDDefSwitch(static_cast<ISwitchVectorProperty *>((*orderi).p) , NULL);
             break;
        case INDI_LIGHT:
             IDDefLight(static_cast<ILightVectorProperty *>((*orderi).p) , NULL);
             break;
        case INDI_BLOB:
             IDDefBLOB(static_cast<IBLOBVectorProperty *>((*orderi).p) , NULL);
             break;
        }
    }

}

int INDI::BaseDevice::removeProperty(const char *name)
{
    std::vector<pOrder>::const_iterator orderi;

    INumberVectorProperty *nvp;
    ITextVectorProperty *tvp;
    ISwitchVectorProperty *svp;
    ILightVectorProperty *lvp;
    IBLOBVectorProperty *bvp;
    int i=0;

    for (i=0, orderi = pAll.begin(); orderi != pAll.end(); orderi++, i++)
    {
        switch ( (*orderi).type)
        {
        case INDI_NUMBER:
            nvp = static_cast<INumberVectorProperty *>((*orderi).p);
            if (!strcmp(name, nvp->name))
            {
                 pAll.erase(pAll.begin()+i);
                 delete (nvp);
                 return 0;
             }
             break;
        case INDI_TEXT:
             tvp = static_cast<ITextVectorProperty *>((*orderi).p);
             if (!strcmp(name, tvp->name))
             {
                  pAll.erase(pAll.begin()+i);
                  delete (tvp);
                  return 0;
              }
             break;
        case INDI_SWITCH:
             svp = static_cast<ISwitchVectorProperty *>((*orderi).p);
             if (!strcmp(name, svp->name))
             {
                  pAll.erase(pAll.begin()+i);
                  delete (svp);
                  return 0;
              }
             break;
        case INDI_LIGHT:
             lvp = static_cast<ILightVectorProperty *>((*orderi).p);
             if (!strcmp(name, lvp->name))
             {
                  pAll.erase(pAll.begin()+i);
                  delete (lvp);
                  return 0;
              }
             break;
        case INDI_BLOB:
             bvp = static_cast<IBLOBVectorProperty *>((*orderi).p);
             if (!strcmp(name, bvp->name))
             {
                  pAll.erase(pAll.begin()+i);
                  delete (bvp);
                  return 0;
              }
             break;
        }
    }

    return INDI_PROPERTY_INVALID;
}

void INDI::BaseDevice::buildSkeleton(const char *filename)
{
    char errmsg[MAXRBUF];
    FILE *fp = NULL;
    XMLEle *root = NULL, *fproot = NULL;

    fp = fopen(filename, "r");

    if (fp == NULL)
    {
        IDLog("Unable to build skeleton. Error loading file %s: %s\n", filename, strerror(errno));
        return;
    }

    fproot = readXMLFile(fp, lp, errmsg);

    if (fproot == NULL)
    {
        IDLog("Unable to parse skeleton XML: %s", errmsg);
        return;
    }

   // prXMLEle(stderr, fproot, 0);

    for (root = nextXMLEle (fproot, 1); root != NULL; root = nextXMLEle (fproot, 0))
            buildProp(root, errmsg);

    /**************************************************************************/
    DebugSP = getSwitch("DEBUG");
    if (!DebugSP)
    {
        DebugSP = new ISwitchVectorProperty;
        IUFillSwitch(&DebugS[0], "ENABLE", "Enable", ISS_OFF);
        IUFillSwitch(&DebugS[1], "DISABLE", "Disable", ISS_ON);
        IUFillSwitchVector(DebugSP, DebugS, NARRAY(DebugS), deviceID, "DEBUG", "Debug", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        pSwitches.push_back(DebugSP);
        pOrder debo = {INDI_SWITCH, DebugSP};
        pAll.push_back(debo);
    }
    else
    {
        ISwitch *sp = IUFindSwitch(DebugSP, "ENABLE");
        if (sp)
            if (sp->s == ISS_ON)
                pDebug = true;
    }
    /**************************************************************************/

    /**************************************************************************/
    SimulationSP = getSwitch("SIMULATION");
    if (!SimulationSP)
    {
        SimulationSP = new ISwitchVectorProperty;
        IUFillSwitch(&SimulationS[0], "ENABLE", "Enable", ISS_OFF);
        IUFillSwitch(&SimulationS[1], "DISABLE", "Disable", ISS_ON);
        IUFillSwitchVector(SimulationSP, SimulationS, NARRAY(SimulationS), deviceID, "SIMULATION", "Simulation", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        pSwitches.push_back(SimulationSP);
        pOrder simo = {INDI_SWITCH, SimulationSP};
        pAll.push_back(simo);
    }
    else
    {
        ISwitch *sp = IUFindSwitch(SimulationSP, "ENABLE");
        if (sp)
            if (sp->s == ISS_ON)
                pSimulation = true;
    }

    /**************************************************************************/

    /**************************************************************************/
    ConfigProcessSP = getSwitch("CONFIG_PROCESS");
    if (!ConfigProcessSP)
    {
        ConfigProcessSP = new ISwitchVectorProperty;
        IUFillSwitch(&ConfigProcessS[0], "CONFIG_LOAD", "Load", ISS_OFF);
        IUFillSwitch(&ConfigProcessS[1], "CONFIG_SAVE", "Save", ISS_OFF);
        IUFillSwitch(&ConfigProcessS[2], "CONFIG_DEFAULT", "Default", ISS_OFF);
        IUFillSwitchVector(ConfigProcessSP, ConfigProcessS, NARRAY(ConfigProcessS), deviceID, "CONFIG_PROCESS", "Configuration", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        pSwitches.push_back(ConfigProcessSP);
        pOrder cpono = {INDI_SWITCH, ConfigProcessSP};
        pAll.push_back(cpono);
    }
    /**************************************************************************/


}

int INDI::BaseDevice::buildProp(XMLEle *root, char *errmsg)
{
    IPerm perm;
    IPState state;
    ISRule rule;
    XMLEle *ep = NULL;
    char *rtag, *rname, *rdev;
    double timeout=0;

    rtag = tagXMLEle(root);

    /* pull out device and name */
    if (crackDN (root, &rdev, &rname, errmsg) < 0)
        return -1;

    if (!deviceID[0])
    {
        if (getenv("INDIDEV"))
            strncpy(deviceID, getenv("INDIDEV"), MAXINDINAME);
        else
            strncpy(deviceID, rdev, MAXINDINAME);
    }

    if (crackIPerm(findXMLAttValu(root, "perm"), &perm) < 0)
    {
        IDLog("Error extracting %s permission (%s)", rname, findXMLAttValu(root, "perm"));
        return -1;
    }

    timeout = atoi(findXMLAttValu(root, "timeout"));

    if (crackIPState (findXMLAttValu(root, "state"), &state) < 0)
    {
        IDLog("Error extracting %s state (%s)", rname, findXMLAttValu(root, "state"));
        return -1;
    }


    if (!strcmp (rtag, "defNumberVector"))
    {

        INumberVectorProperty *nvp = new INumberVectorProperty;
        INumber *np = NULL;
        int n=0;

        strncpy(nvp->device, deviceID, MAXINDIDEVICE);
        strncpy(nvp->name, rname, MAXINDINAME);
        strncpy(nvp->label, findXMLAttValu(root, "label"), MAXINDILABEL);
        strncpy(nvp->group, findXMLAttValu(root, "group"), MAXINDIGROUP);

        nvp->p       = perm;
        nvp->s       = state;
        nvp->timeout = timeout;

    /* pull out each name/value pair */
    for (n = 0, ep = nextXMLEle(root,1); ep != NULL; ep = nextXMLEle(root,0), n++)
    {
        if (!strcmp (tagXMLEle(ep), "defNumber"))
        {
            np = (INumber *) realloc(np, (n+1) * sizeof(INumber));

            np[n].nvp = nvp;

            XMLAtt *na = findXMLAtt (ep, "name");

            if (na)
            {
                if (f_scansexa (pcdataXMLEle(ep), &(np[n].value)) < 0)
                    IDLog("%s: Bad format %s\n", rname, pcdataXMLEle(ep));
                else
                {

                    strncpy(np[n].name, valuXMLAtt(na), MAXINDINAME);

                    na = findXMLAtt (ep, "label");
                    if (na)
                      strncpy(np[n].label, valuXMLAtt(na),MAXINDILABEL);
                    na = findXMLAtt (ep, "format");
                    if (na)
                        strncpy(np[n].format, valuXMLAtt(na), MAXINDIFORMAT);

                   na = findXMLAtt (ep, "min");
                   if (na)
                       np[n].min = atof(valuXMLAtt(na));
                   na = findXMLAtt (ep, "max");
                   if (na)
                       np[n].max = atof(valuXMLAtt(na));
                   na = findXMLAtt (ep, "step");
                   if (na)
                       np[n].step = atof(valuXMLAtt(na));

                }
            }
        }
    }

    if (n > 0)
    {
        nvp->nnp = n;
        nvp->np  = np;
        pNumbers.push_back(nvp);
        pOrder o = { INDI_NUMBER, nvp };
        pAll.push_back(o);
        IDLog("Adding number property %s to list.\n", nvp->name);
    }
    else
        IDLog("%s: newNumberVector with no valid members\n",rname);
}
  else if (!strcmp (rtag, "defSwitchVector"))
        {
            ISwitchVectorProperty *svp = new ISwitchVectorProperty;
            ISwitch *sp = NULL;
            int n=0;

            strncpy(svp->device, deviceID, MAXINDIDEVICE);
            strncpy(svp->name, rname, MAXINDINAME);
            strncpy(svp->label, findXMLAttValu(root, "label"), MAXINDILABEL);
            strncpy(svp->group, findXMLAttValu(root, "group"), MAXINDIGROUP);

            if (crackISRule(findXMLAttValu(root, "rule"), (&svp->r)) < 0)
                svp->r = ISR_1OFMANY;

            svp->p       = perm;
            svp->s       = state;
            svp->timeout = timeout;


        /* pull out each name/value pair */
        for (n = 0, ep = nextXMLEle(root,1); ep != NULL; ep = nextXMLEle(root,0), n++)
        {
            if (!strcmp (tagXMLEle(ep), "defSwitch"))
            {
                sp = (ISwitch *) realloc(sp, (n+1) * sizeof(ISwitch));

                sp[n].svp = svp;

                XMLAtt *na = findXMLAtt (ep, "name");

                if (na)
                {
                    crackISState(pcdataXMLEle(ep), &(sp[n].s));
                    strncpy(sp[n].name, valuXMLAtt(na), MAXINDINAME);

                    na = findXMLAtt (ep, "label");
                    if (na)
                        strncpy(sp[n].label, valuXMLAtt(na),MAXINDILABEL);
                 }
            }
        }

        if (n > 0)
        {
            svp->nsp = n;
            svp->sp  = sp;
            pSwitches.push_back(svp);
            pOrder o = { INDI_SWITCH, svp };
            pAll.push_back(o);
            IDLog("Adding Switch property %s to list.\n", svp->name);
        }
        else
            IDLog("%s: newSwitchVector with no valid members\n",rname);
    }

else if (!strcmp (rtag, "defTextVector"))
    {

        ITextVectorProperty *tvp = new ITextVectorProperty;
        IText *tp = NULL;
        int n=0;

        strncpy(tvp->device, deviceID, MAXINDIDEVICE);
        strncpy(tvp->name, rname, MAXINDINAME);
        strncpy(tvp->label, findXMLAttValu(root, "label"), MAXINDILABEL);
        strncpy(tvp->group, findXMLAttValu(root, "group"), MAXINDIGROUP);

        tvp->p       = perm;
        tvp->s       = state;
        tvp->timeout = timeout;

    /* pull out each name/value pair */
    for (n = 0, ep = nextXMLEle(root,1); ep != NULL; ep = nextXMLEle(root,0), n++)
    {
        if (!strcmp (tagXMLEle(ep), "defText"))
        {
            tp = (IText *) realloc(tp, (n+1) * sizeof(IText));

            tp[n].tvp = tvp;

            XMLAtt *na = findXMLAtt (ep, "name");

            if (na)
            {
                tp[n].text = (char *) malloc(pcdatalenXMLEle(ep)*sizeof(char));
                strncpy(tp[n].text, pcdataXMLEle(ep), pcdatalenXMLEle(ep));
                strncpy(tp[n].name, valuXMLAtt(na), MAXINDINAME);

                na = findXMLAtt (ep, "label");
                if (na)
                    strncpy(tp[n].label, valuXMLAtt(na),MAXINDILABEL);
             }
        }
    }

    if (n > 0)
    {
        tvp->ntp = n;
        tvp->tp  = tp;
        pTexts.push_back(tvp);
        pOrder o = { INDI_TEXT, tvp };
        pAll.push_back(o);
        IDLog("Adding Text property %s to list.\n", tvp->name);
    }
    else
        IDLog("%s: newTextVector with no valid members\n",rname);
}
else if (!strcmp (rtag, "defLightVector"))
    {

        ILightVectorProperty *lvp = new ILightVectorProperty;
        ILight *lp = NULL;
        int n=0;

        strncpy(lvp->device, deviceID, MAXINDIDEVICE);
        strncpy(lvp->name, rname, MAXINDINAME);
        strncpy(lvp->label, findXMLAttValu(root, "label"), MAXINDILABEL);
        strncpy(lvp->group, findXMLAttValu(root, "group"), MAXINDIGROUP);

        lvp->s       = state;

    /* pull out each name/value pair */
    for (n = 0, ep = nextXMLEle(root,1); ep != NULL; ep = nextXMLEle(root,0), n++)
    {
        if (!strcmp (tagXMLEle(ep), "defLight"))
        {
            lp = (ILight *) realloc(lp, (n+1) * sizeof(ILight));

            lp[n].lvp = lvp;

            XMLAtt *na = findXMLAtt (ep, "name");

            if (na)
            {
                crackIPState(pcdataXMLEle(ep), &(lp[n].s));
                strncpy(lp[n].name, valuXMLAtt(na), MAXINDINAME);

                na = findXMLAtt (ep, "label");
                if (na)
                    strncpy(lp[n].label, valuXMLAtt(na),MAXINDILABEL);

             }
        }
    }

    if (n > 0)
    {
        lvp->nlp = n;
        lvp->lp  = lp;
        pLights.push_back(lvp);
        pOrder o = { INDI_LIGHT, lvp };
        pAll.push_back(o);
        IDLog("Adding Light property %s to list.\n", lvp->name);
    }
    else
        IDLog("%s: newLightVector with no valid members\n",rname);
}
else if (!strcmp (rtag, "defBLOBVector"))
    {

        IBLOBVectorProperty *bvp = new IBLOBVectorProperty;
        IBLOB *bp = NULL;
        int n=0;

        strncpy(bvp->device, deviceID, MAXINDIDEVICE);
        strncpy(bvp->name, rname, MAXINDINAME);
        strncpy(bvp->label, findXMLAttValu(root, "label"), MAXINDILABEL);
        strncpy(bvp->group, findXMLAttValu(root, "group"), MAXINDIGROUP);

        bvp->s       = state;

    /* pull out each name/value pair */
    for (n = 0, ep = nextXMLEle(root,1); ep != NULL; ep = nextXMLEle(root,0), n++)
    {
        if (!strcmp (tagXMLEle(ep), "defBLOB"))
        {
            bp = (IBLOB *) realloc(bp, (n+1) * sizeof(IBLOB));

            bp[n].bvp = bvp;

            XMLAtt *na = findXMLAtt (ep, "name");

            if (na)
            {
                na = findXMLAtt (ep, "label");
                if (na)
                    strncpy(bp[n].label, valuXMLAtt(na),MAXINDILABEL);

                na = findXMLAtt (ep, "format");
                if (na)
                    strncpy(bp[n].label, valuXMLAtt(na),MAXINDIBLOBFMT);

                // Initialize everything to zero
                bp[n].blob    = NULL;
                bp[n].size    = 0;
                bp[n].bloblen = 0;
             }

        }
    }

    if (n > 0)
    {
        bvp->nbp = n;
        bvp->bp  = bp;
        pBlobs.push_back(bvp);
        pOrder o = { INDI_BLOB, bvp };
        pAll.push_back(o);
        IDLog("Adding BLOB property %s to list.\n", bvp->name);
    }
    else
        IDLog("%s: newBLOBVector with no valid members\n",rname);
 }

return (0);

}

bool INDI::BaseDevice::isConnected()
{
    ISwitchVectorProperty *svp = getSwitch("CONNECTION");
    if (!svp)
        return false;

    ISwitch * sp = IUFindSwitch(svp, "CONNECT");

    if (!sp)
        return false;

    if (sp->s == ISS_ON)
        return true;
    else
        return false;

}

void INDI::BaseDevice::setConnected(bool status)
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

    svp->s = IPS_OK;

    loadConfig();
}

bool INDI::BaseDevice::loadConfig(bool ignoreConnection)
{
    char errmsg[MAXRBUF];
    bool pResult = false;

    if (ignoreConnection || isConnected() )
        pResult = readConfig(NULL, deviceID, errmsg) == 0 ? true : false;

   if (pResult && ignoreConnection)
       IDMessage(deviceID, "Configuration successfully loaded.");

   IUSaveDefaultConfig(NULL, NULL, deviceID);

   return pResult;
}

void INDI::BaseDevice::setDebug(bool enable)
{
    if (pDebug == enable)
    {
        DebugSP->s = IPS_OK;
        IDSetSwitch(DebugSP, NULL);
        return;
    }

    if (!DebugSP)
        return;

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

void INDI::BaseDevice::setSimulation(bool enable)
{
   if (pSimulation == enable)
   {
       SimulationSP->s = IPS_OK;
       IDSetSwitch(SimulationSP, NULL);
       return;
   }

   if (!SimulationSP)
       return;

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

bool INDI::BaseDevice::isDebug()
{
  return pDebug;
}

bool INDI::BaseDevice::isSimulation()
{
 return pSimulation;
}

void INDI::BaseDevice::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    // ignore if not ours //
    if (strcmp (dev, deviceID))
        return;

    ISwitchVectorProperty *svp = getSwitch(name);

    if (!svp)
        return;

    if (!strcmp(svp->name, "DEBUG"))
    {
        IUUpdateSwitch(svp, states, names, n);
        ISwitch *sp = IUFindOnSwitch(svp);
        if (!sp)
            return;

        if (!strcmp(sp->name, "ENABLE"))
            setDebug(true);
        else
            setDebug(false);
        return;
    }

    if (!strcmp(svp->name, "SIMULATION"))
    {
        IUUpdateSwitch(svp, states, names, n);
        ISwitch *sp = IUFindOnSwitch(svp);
        if (!sp)
            return;

        if (!strcmp(sp->name, "ENABLE"))
            setSimulation(true);
        else
            setSimulation(false);
        return;
    }

    if (!strcmp(svp->name, "CONFIG_PROCESS"))
    {
        IUUpdateSwitch(svp, states, names, n);
        ISwitch *sp = IUFindOnSwitch(svp);
        IUResetSwitch(svp);
        bool pResult = false;
        if (!sp)
            return;

        if (!strcmp(sp->name, "CONFIG_LOAD"))
            pResult = loadConfig(true);
        else if (!strcmp(sp->name, "CONFIG_SAVE"))
            pResult = saveConfig();
        else if (!strcmp(sp->name, "CONFIG_DEFAULT"))
            pResult = loadDefaultConfig();

        if (pResult)
            svp->s = IPS_OK;
        else
            svp->s = IPS_ALERT;

        IDSetSwitch(svp, NULL);
    }

}


bool INDI::BaseDevice::saveConfig()
{
    std::vector<pOrder>::const_iterator orderi;
    ISwitchVectorProperty *svp=NULL;
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
        switch ( (*orderi).type)
        {
        case INDI_NUMBER:
             IUSaveConfigNumber(fp, static_cast<INumberVectorProperty *> ((*orderi).p));
             break;
        case INDI_TEXT:
             IUSaveConfigText(fp, static_cast<ITextVectorProperty *> ((*orderi).p));
             break;
        case INDI_SWITCH:
             svp = static_cast<ISwitchVectorProperty *> ((*orderi).p);
             /* Never save CONNECTION property. Don't save switches with no switches on if the rule is one of many */
             if (!strcmp(svp->name, "CONNECTION") || (svp->r == ISR_1OFMANY && !IUFindOnSwitch(svp)))
                 continue;
             IUSaveConfigSwitch(fp, svp);
             break;
        case INDI_BLOB:
             IUSaveConfigBLOB(fp, static_cast<IBLOBVectorProperty *> ((*orderi).p));
             break;
        }
    }

    IUSaveConfigTag(fp, 1);

    fclose(fp);

    IUSaveDefaultConfig(NULL, NULL, deviceID);

    IDMessage(deviceID, "Configuration successfully saved.");

    return true;
}

bool INDI::BaseDevice::loadDefaultConfig()
{
    char configDefaultFileName[MAXRBUF];
    char errmsg[MAXRBUF];
    bool pResult = false;

    if (getenv("INDICONFIG"))
        snprintf(configDefaultFileName, MAXRBUF, "%s.default", getenv("INDICONFIG"));
    else
        snprintf(configDefaultFileName, MAXRBUF, "%s/.indi/%s_config.xml.default", getenv("HOME"), deviceID);

    IDLog("Requesting to load default config with: %s\n", configDefaultFileName);

    pResult = readConfig(configDefaultFileName, deviceID, errmsg) == 0 ? true : false;

    if (pResult)
        IDMessage(deviceID, "Default configuration loaded.");
    else
        IDMessage(deviceID, "Error loading default configuraiton. %s", errmsg);

    return pResult;
}

/*
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI::BaseDevice::setValue (XMLEle *root, char * errmsg)
{
    XMLAtt *ap;
    XMLEle *ep;
    char *rtag, *name;
    double timeout;
    IPState state;
    bool stateSet=false, timeoutSet=false;

    rtag = tagXMLEle(root);

    ap = findXMLAtt (root, "name");
    if (!ap)
        return (-1);

    name = valuXMLAtt(ap);

    /* set overall property state, if any */
    ap = findXMLAtt (root, "state");
    if (ap)
    {
        if (crackIPState(valuXMLAtt(ap), &state) != 0)
        {
            snprintf(errmsg, MAXRBUF, "INDI: <%s> bogus state %s for %s", tagXMLEle(root),
                     valuXMLAtt(ap), name);
            return (-1);
        }

        stateSet = true;
    }

    /* allow changing the timeout */
    ap = findXMLAtt (root, "timeout");
    if (ap)
    {
        timeout = atof(valuXMLAtt(ap));
        timeoutSet = true;
    }

    if (!strcmp(rtag, "setNumberVector"))
    {
        INumberVectorProperty *nvp = getNumber(name);
        if (nvp == NULL)
            return -1;

        if (stateSet)
            nvp->s = state;

        if (timeoutSet)
            nvp->timeout = timeout;

       for (ep = nextXMLEle (root, 1); ep != NULL; ep = nextXMLEle (root, 0))
        {
           INumber *np =  IUFindNumber(nvp, findXMLAttValu(ep, "name"));
           if (!np)
               continue;

          np->value = atof(pcdataXMLEle(ep));

          // Permit changing of min/max
          if (findXMLAtt(ep, "min"))
              np->min = atof(findXMLAttValu(ep, "min"));
          if (findXMLAtt(ep, "max"))
              np->max = atof(findXMLAttValu(ep, "max"));
       }
    }
    else if (!strcmp(rtag, "setTextVector"))
    {
        ITextVectorProperty *tvp = getText(name);
        if (tvp == NULL)
            return -1;

        if (stateSet)
            tvp->s = state;

        if (timeoutSet)
            tvp->timeout = timeout;

       for (ep = nextXMLEle (root, 1); ep != NULL; ep = nextXMLEle (root, 0))
        {
           IText *tp =  IUFindText(tvp, findXMLAttValu(ep, "name"));
           if (!tp)
               continue;

           IUSaveText(tp, pcdataXMLEle(ep));
       }
    }
    else if (!strcmp(rtag, "setSwitchVector"))
    {
        ISState swState;
        ISwitchVectorProperty *svp = getSwitch(name);
        if (svp == NULL)
            return -1;

        if (stateSet)
            svp->s = state;

        if (timeoutSet)
            svp->timeout = timeout;

       for (ep = nextXMLEle (root, 1); ep != NULL; ep = nextXMLEle (root, 0))
        {
           ISwitch *sp =  IUFindSwitch(svp, findXMLAttValu(ep, "name"));
           if (!sp)
               continue;

           if (crackISState(pcdataXMLEle(ep), &swState) == 0)
               sp->s = swState;
        }
    }
    else if (!strcmp(rtag, "setLightVector"))
    {
        IPState lState;
        ILightVectorProperty *lvp = getLight(name);
        if (lvp == NULL)
            return -1;

        if (stateSet)
            lvp->s = state;

       for (ep = nextXMLEle (root, 1); ep != NULL; ep = nextXMLEle (root, 0))
        {
           ILight *lp =  IUFindLight(lvp, findXMLAttValu(ep, "name"));
           if (!lp)
               continue;

           if (crackIPState(pcdataXMLEle(ep), &lState) == 0)
               lp->s = lState;
        }
    }
    else if (!strcmp(rtag, "setBLOBVector"))
    {
        IBLOBVectorProperty *bvp = getBLOB(name);
        if (bvp == NULL)
            return -1;

        if (stateSet)
            bvp->s = state;

        if (timeoutSet)
            bvp->timeout = timeout;

        return setBLOB(bvp, root, errmsg);
    }

    return (0);
}

/* Set BLOB vector. Process incoming data stream
 * Return 0 if okay, -1 if error
*/
int INDI::BaseDevice::setBLOB(IBLOBVectorProperty *bvp, XMLEle * root, char * errmsg)
{

    XMLEle *ep;
    IBLOB *blobEL;

    for (ep = nextXMLEle(root,1); ep; ep = nextXMLEle(root,0))
    {

        if (strcmp(tagXMLEle(ep), "oneBLOB") == 0)
        {

            blobEL = IUFindBLOB(bvp, findXMLAttValu (ep, "name"));

            if (blobEL)
                return processBLOB(blobEL, ep, errmsg);
            else
            {
                snprintf(errmsg, MAXRBUF, "INDI: set %s.%s.%s not found", bvp->device, bvp->name,
                         findXMLAttValu (ep, "name"));
                return (-1);
            }
        }
    }

    return (0);
}

/* Process incoming data stream
 * Return 0 if okay, -1 if error
*/
int INDI::BaseDevice::processBLOB(IBLOB *blobEL, XMLEle *ep, char * errmsg)
{
    XMLAtt *ap = NULL;
    int blobSize=0, r=0;
    uLongf dataSize=0;
    char *baseBuffer=NULL, *dataFormat;
    unsigned char *blobBuffer(NULL), *dataBuffer(NULL);
    bool iscomp(false);

    ap = findXMLAtt(ep, "size");
    if (!ap)
    {
        snprintf(errmsg, MAXRBUF, "INDI: set %s size not found", blobEL->name);
        return (-1);
    }

    dataSize = atoi(valuXMLAtt(ap));

    ap = findXMLAtt(ep, "format");
    if (!ap)
    {
        snprintf(errmsg, MAXRBUF, "INDI: set %s format not found",blobEL->name);
        return (-1);
    }

    dataFormat = valuXMLAtt(ap);

    baseBuffer = (char *) malloc ( (3*pcdatalenXMLEle(ep)/4) * sizeof (char));

    if (baseBuffer == NULL)
    {
        strncpy(errmsg, "Unable to allocate memory for base buffer", MAXRBUF);
        return (-1);
    }

    blobSize   = from64tobits (baseBuffer, pcdataXMLEle(ep));
    blobBuffer = (unsigned char *) baseBuffer;

    /* Blob size = 0 when only state changes */
    if (dataSize == 0)
    {
        free (blobBuffer);
        return (0);
    }
    else if (blobSize < 0)
    {
        free (blobBuffer);
        snprintf(errmsg, MAXRBUF, "INDI: %s.%s.%s bad base64", blobEL->bvp->device, blobEL->bvp->name, blobEL->name);
        return (-1);
    }

    if (strstr(dataFormat, ".z"))
    {

        dataFormat[strlen(dataFormat)-2] = '\0';
        dataBuffer = (unsigned char *) realloc (dataBuffer,  (dataSize * sizeof(unsigned char)));
        if (baseBuffer == NULL)
        {
                free (blobBuffer);
                strncpy(errmsg, "Unable to allocate memory for data buffer", MAXRBUF);
                return (-1);
        }

        r = uncompress(dataBuffer, &dataSize, blobBuffer, (uLong) blobSize);
        if (r != Z_OK)
        {
            snprintf(errmsg, MAXRBUF, "INDI: %s.%s.%s compression error: %d", blobEL->bvp->device, blobEL->bvp->name, blobEL->name, r);
            free (blobBuffer);
            return -1;
        }
    }
    else
    {
        dataBuffer = (unsigned char *) realloc (dataBuffer,  (dataSize * sizeof(unsigned char)));
        memcpy(dataBuffer, blobBuffer, dataSize);
    }

    BLOBReceived(dataBuffer, dataSize, dataFormat);

    free (blobBuffer);
    return (0);
}

void INDI::BaseDevice::setDeviceName(const char *dev)
{
    strncpy(deviceID, dev, MAXINDINAME);
}

const char * INDI::BaseDevice::deviceName()
{
    return deviceID;
}

void INDI::BaseDevice::addMessage(const char *msg)
{
    messageQueue.append(msg);
}

