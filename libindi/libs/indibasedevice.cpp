#include <stdlib.h>
#include <string.h>

#include "indidrivermain.h"
#include "indibasedevice.h"
#include "indicom.h"

#include <errno.h>

INDIBaseDevice::INDIBaseDevice()
{
    lp = newLilXML();
    pDebug = false;
    pSimulation = false;
}


INDIBaseDevice::~INDIBaseDevice()
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

INumberVectorProperty * INDIBaseDevice::getNumber(const char *name)
{
    std::vector<INumberVectorProperty *>::const_iterator numi;

    for ( numi = pNumbers.begin(); numi != pNumbers.end(); numi++)
        if (!strcmp(name, (*numi)->name))
            return *numi;

    return NULL;

}

ITextVectorProperty * INDIBaseDevice::getText(const char *name)
{
    std::vector<ITextVectorProperty *>::const_iterator texti;

    for ( texti = pTexts.begin(); texti != pTexts.end(); texti++)
        if (!strcmp(name, (*texti)->name))
            return *texti;

    return NULL;
}

ISwitchVectorProperty * INDIBaseDevice::getSwitch(const char *name)
{
    std::vector<ISwitchVectorProperty *>::const_iterator switchi;

    for ( switchi = pSwitches.begin(); switchi != pSwitches.end(); switchi++)
        if (!strcmp(name, (*switchi)->name))
            return *switchi;

    return NULL;

}

ILightVectorProperty * INDIBaseDevice::getLight(const char *name)
{
    std::vector<ILightVectorProperty *>::const_iterator lighti;

    for ( lighti = pLights.begin(); lighti != pLights.end(); lighti++)
        if (!strcmp(name, (*lighti)->name))
            return *lighti;

    return NULL;

}

IBLOBVectorProperty * INDIBaseDevice::getBLOB(const char *name)
{
    std::vector<IBLOBVectorProperty *>::const_iterator blobi;

    for ( blobi = pBlobs.begin(); blobi != pBlobs.end(); blobi++)
        if (!strcmp(name, (*blobi)->name))
            return *blobi;

    return NULL;
}

void INDIBaseDevice::ISGetProperties (const char *dev)
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


void INDIBaseDevice::buildSkeleton(const char *filename)
{
    char errmsg[MAXRBUF];
    char *rtag, *rname, *rdev;
    double timeout=0;
    FILE *fp = NULL;
    IPerm perm;
    IPState state;
    ISRule rule;
    XMLEle *ep = NULL, *root = NULL, *fproot = NULL;

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
    {

        rtag = tagXMLEle(root);

        /* pull out device and name */
        if (crackDN (root, &rdev, &rname, errmsg) < 0)
            return;

        if (!deviceName[0])
        {
            if (getenv("INDIDEV"))
                strncpy(deviceName, getenv("INDIDEV"), MAXINDINAME);
            else
                strncpy(deviceName, rdev, MAXINDINAME);
        }

        if (crackIPerm(findXMLAttValu(root, "perm"), &perm) < 0)
        {
            IDLog("Error extracting %s permission (%s)", rname, findXMLAttValu(root, "perm"));
            continue;
        }

        timeout = atoi(findXMLAttValu(root, "timeout"));

        if (crackIPState (findXMLAttValu(root, "state"), &state) < 0)
        {
            IDLog("Error extracting %s state (%s)", rname, findXMLAttValu(root, "state"));
            continue;
        }


        if (!strcmp (rtag, "defNumberVector"))
        {

            INumberVectorProperty *nvp = new INumberVectorProperty;
            INumber *np = NULL;
            int n=0;

            strncpy(nvp->device, deviceName, MAXINDIDEVICE);
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

                strncpy(svp->device, deviceName, MAXINDIDEVICE);
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

            strncpy(tvp->device, deviceName, MAXINDIDEVICE);
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

            strncpy(lvp->device, deviceName, MAXINDIDEVICE);
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

            strncpy(bvp->device, deviceName, MAXINDIDEVICE);
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
  }

    /**************************************************************************/
    DebugSP = getSwitch("DEBUG");
    if (!DebugSP)
    {
        DebugSP = new ISwitchVectorProperty;
        IUFillSwitch(&DebugS[0], "ENABLE", "Enable", ISS_OFF);
        IUFillSwitch(&DebugS[1], "DISABLE", "Disable", ISS_ON);
        IUFillSwitchVector(DebugSP, DebugS, NARRAY(DebugS), deviceName, "DEBUG", "Debug", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
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
        IUFillSwitchVector(SimulationSP, SimulationS, NARRAY(SimulationS), deviceName, "SIMULATION", "Simulation", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
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
        IUFillSwitchVector(ConfigProcessSP, ConfigProcessS, NARRAY(ConfigProcessS), deviceName, "CONFIG_PROCESS", "Configuration", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        pSwitches.push_back(ConfigProcessSP);
        pOrder cpono = {INDI_SWITCH, ConfigProcessSP};
        pAll.push_back(cpono);
    }
    /**************************************************************************/


}

bool INDIBaseDevice::isConnected()
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

void INDIBaseDevice::setConnected(bool status)
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

bool INDIBaseDevice::loadConfig(bool ignoreConnection)
{
    char errmsg[MAXRBUF];
    bool pResult = false;

    if (ignoreConnection || isConnected() )
        pResult = readConfig(NULL, deviceName, errmsg) == 0 ? true : false;

   if (pResult && ignoreConnection)
       IDMessage(deviceName, "Configuration successfully loaded.");

   IUSaveDefaultConfig(NULL, NULL, deviceName);

   return pResult;
}

void INDIBaseDevice::setDebug(bool enable)
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
            IDMessage(deviceName, "Debug is enabled.");
        }
    }
    else
    {
        ISwitch *sp = IUFindSwitch(DebugSP, "DISABLE");
        if (sp)
        {
            sp->s = ISS_ON;
            IDMessage(deviceName, "Debug is disabled.");
        }
    }

    pDebug = enable;
    DebugSP->s = IPS_OK;
    IDSetSwitch(DebugSP, NULL);

}

void INDIBaseDevice::setSimulation(bool enable)
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
           IDMessage(deviceName, "Simulation is enabled.");
           sp->s = ISS_ON;
       }
   }
   else
   {
       ISwitch *sp = IUFindSwitch(SimulationSP, "DISABLE");
       if (sp)
       {
           sp->s = ISS_ON;
           IDMessage(deviceName, "Simulation is disabled.");
       }
   }

   pSimulation = enable;
   SimulationSP->s = IPS_OK;
   IDSetSwitch(SimulationSP, NULL);

}

bool INDIBaseDevice::isDebug()
{
  return pDebug;
}

bool INDIBaseDevice::isSimulation()
{
 return pSimulation;
}

void INDIBaseDevice::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    // ignore if not ours //
    if (strcmp (dev, deviceName))
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


bool INDIBaseDevice::saveConfig()
{
    std::vector<pOrder>::const_iterator orderi;
    ISwitchVectorProperty *svp=NULL;
    char errmsg[MAXRBUF];
    FILE *fp = NULL;

    fp = IUGetConfigFP(NULL, deviceName, errmsg);

    if (fp == NULL)
    {
        IDMessage(deviceName, "Error saving configuration. %s", errmsg);
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

    IUSaveDefaultConfig(NULL, NULL, deviceName);

    IDMessage(deviceName, "Configuration successfully saved.");

    return true;
}

bool INDIBaseDevice::loadDefaultConfig()
{
    char configDefaultFileName[MAXRBUF];
    char errmsg[MAXRBUF];
    bool pResult = false;

    if (getenv("INDICONFIG"))
        snprintf(configDefaultFileName, MAXRBUF, "%s.default", getenv("INDICONFIG"));
    else
        snprintf(configDefaultFileName, MAXRBUF, "%s/.indi/%s_config.xml.default", getenv("HOME"), deviceName);

    IDLog("Requesting to load default config with: %s\n", configDefaultFileName);

    pResult = readConfig(configDefaultFileName, deviceName, errmsg) == 0 ? true : false;

    if (pResult)
        IDMessage(deviceName, "Default configuration loaded.");
    else
        IDMessage(deviceName, "Error loading default configuraiton. %s", errmsg);

    return pResult;
}
