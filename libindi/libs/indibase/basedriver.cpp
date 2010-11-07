#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <zlib.h>

#include "basedriver.h"
#include "baseclient.h"
#include "indicom.h"
#include "base64.h"

INDI::BaseDriver::BaseDriver()
{
    mediator = NULL;
    lp = newLilXML();
}


INDI::BaseDriver::~BaseDriver()
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

INumberVectorProperty * INDI::BaseDriver::getNumber(const char *name)
{
    std::vector<INumberVectorProperty *>::const_iterator numi;

    for ( numi = pNumbers.begin(); numi != pNumbers.end(); numi++)
        if (!strcmp(name, (*numi)->name))
            return *numi;

    return NULL;

}

ITextVectorProperty * INDI::BaseDriver::getText(const char *name)
{
    std::vector<ITextVectorProperty *>::const_iterator texti;

    for ( texti = pTexts.begin(); texti != pTexts.end(); texti++)
        if (!strcmp(name, (*texti)->name))
            return *texti;

    return NULL;
}

ISwitchVectorProperty * INDI::BaseDriver::getSwitch(const char *name)
{
    std::vector<ISwitchVectorProperty *>::const_iterator switchi;

    for ( switchi = pSwitches.begin(); switchi != pSwitches.end(); switchi++)
        if (!strcmp(name, (*switchi)->name))
            return *switchi;

    return NULL;

}

ILightVectorProperty * INDI::BaseDriver::getLight(const char *name)
{
    std::vector<ILightVectorProperty *>::const_iterator lighti;

    for ( lighti = pLights.begin(); lighti != pLights.end(); lighti++)
        if (!strcmp(name, (*lighti)->name))
            return *lighti;

    return NULL;

}

IBLOBVectorProperty * INDI::BaseDriver::getBLOB(const char *name)
{
    std::vector<IBLOBVectorProperty *>::const_iterator blobi;

    for ( blobi = pBlobs.begin(); blobi != pBlobs.end(); blobi++)
        if (!strcmp(name, (*blobi)->name))
            return *blobi;

    return NULL;
}

void * INDI::BaseDriver::getProperty(const char *name, INDI_TYPE & type)
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
                type = INDI_NUMBER;
                return (*orderi).p;
            }

             break;
        case INDI_TEXT:
             tvp = static_cast<ITextVectorProperty *>((*orderi).p);
             if (!strcmp(name, tvp->name))
             {
                 type = INDI_TEXT;
                 return (*orderi).p;
             }
             break;
        case INDI_SWITCH:
             svp = static_cast<ISwitchVectorProperty *>((*orderi).p);
             if (!strcmp(name, svp->name))
             {
                 type = INDI_SWITCH;
                 return (*orderi).p;
             }
             break;
        case INDI_LIGHT:
             lvp = static_cast<ILightVectorProperty *>((*orderi).p);
             if (!strcmp(name, lvp->name))
             {
                 type = INDI_LIGHT;
                 return (*orderi).p;
             }
             break;
        case INDI_BLOB:
             bvp = static_cast<IBLOBVectorProperty *>((*orderi).p);
             if (!strcmp(name, bvp->name))
             {
                 type = INDI_BLOB;
                 return (*orderi).p;
             }
             break;
        }
    }

    return NULL;
}

int INDI::BaseDriver::removeProperty(const char *name)
{
    std::vector<pOrder>::iterator orderi;

    INumberVectorProperty *nvp;
    ITextVectorProperty *tvp;
    ISwitchVectorProperty *svp;
    ILightVectorProperty *lvp;
    IBLOBVectorProperty *bvp;

    for (orderi = pAll.begin(); orderi != pAll.end(); orderi++)
    {
        switch ( (*orderi).type)
        {
        case INDI_NUMBER:
            nvp = static_cast<INumberVectorProperty *>((*orderi).p);
            if (!strcmp(name, nvp->name))
            {
                 pAll.erase(orderi);
                 delete (nvp);
                 return 0;
             }
             break;
        case INDI_TEXT:
             tvp = static_cast<ITextVectorProperty *>((*orderi).p);
             if (!strcmp(name, tvp->name))
             {
                  pAll.erase(orderi);
                  delete (tvp);
                  return 0;
              }
             break;
        case INDI_SWITCH:
             svp = static_cast<ISwitchVectorProperty *>((*orderi).p);
             if (!strcmp(name, svp->name))
             {
                  pAll.erase(orderi);
                  delete (svp);
                  return 0;
              }
             break;
        case INDI_LIGHT:
             lvp = static_cast<ILightVectorProperty *>((*orderi).p);
             if (!strcmp(name, lvp->name))
             {
                  pAll.erase(orderi);
                  delete (lvp);
                  return 0;
              }
             break;
        case INDI_BLOB:
             bvp = static_cast<IBLOBVectorProperty *>((*orderi).p);
             if (!strcmp(name, bvp->name))
             {
                  pAll.erase(orderi);
                  delete (bvp);
                  return 0;
              }
             break;
        }
    }

    return INDI_PROPERTY_INVALID;
}

void INDI::BaseDriver::buildSkeleton(const char *filename)
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

    //prXMLEle(stderr, fproot, 0);

    for (root = nextXMLEle (fproot, 1); root != NULL; root = nextXMLEle (fproot, 0))
            buildProp(root, errmsg);



    /**************************************************************************/

}

int INDI::BaseDriver::buildProp(XMLEle *root, char *errmsg)
{
    IPerm perm;
    IPState state;
    ISRule rule;
    XMLEle *ep = NULL;
    char *rtag, *rname, *rdev;
    INDI_TYPE type;
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

    if (getProperty(rname, type) != NULL)
        return INDI::BaseClient::INDI_PROPERTY_DUPLICATED;

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
        //IDLog("Adding number property %s to list.\n", nvp->name);
        if (mediator)
            mediator->newProperty(nvp->name);
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
            //IDLog("Adding Switch property %s to list.\n", svp->name);
            if (mediator)
                mediator->newProperty(svp->name);
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
        //IDLog("Adding Text property %s to list.\n", tvp->name);
        if (mediator)
            mediator->newProperty(tvp->name);
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
        //IDLog("Adding Light property %s to list.\n", lvp->name);
        if (mediator)
            mediator->newProperty(lvp->name);
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
                strncpy(bp[n].name, valuXMLAtt(na), MAXINDINAME);

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
        if (mediator)
            mediator->newProperty(bvp->name);
    }
    else
        IDLog("%s: newBLOBVector with no valid members\n",rname);
 }

return (0);

}

bool INDI::BaseDriver::isConnected()
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

void INDI::BaseDriver::setConnected(bool status)
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
}

/*
 * return 0 if ok else -1 with reason in errmsg
 */
int INDI::BaseDriver::setValue (XMLEle *root, char * errmsg)
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
    {
        snprintf(errmsg, MAXRBUF, "INDI: <%s> unable to find name attribute", tagXMLEle(root));
        return (-1);
    }

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
        {
            snprintf(errmsg, MAXRBUF, "INDI: Could not find property %s in %s", name, deviceID);
            return -1;
        }

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

       if (mediator)
           mediator->newNumber(nvp);

       return 0;
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

       if (mediator)
           mediator->newText(tvp);

       return 0;
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

       if (mediator)
           mediator->newSwitch(svp);

       return 0;
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

       if (mediator)
           mediator->newLight(lvp);

       return 0;

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

    snprintf(errmsg, MAXRBUF, "INDI: <%s> Unable to process tag", tagXMLEle(root));
    return -1;
}

/* Set BLOB vector. Process incoming data stream
 * Return 0 if okay, -1 if error
*/
int INDI::BaseDriver::setBLOB(IBLOBVectorProperty *bvp, XMLEle * root, char * errmsg)
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

    return -1;
}

/* Process incoming data stream
 * Return 0 if okay, -1 if error
*/
int INDI::BaseDriver::processBLOB(IBLOB *blobEL, XMLEle *ep, char * errmsg)
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

    strncpy(blobEL->format, dataFormat, MAXINDIFORMAT);

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
        blobEL->size = dataSize;
    }
    else
    {
        dataBuffer = (unsigned char *) realloc (dataBuffer,  (dataSize * sizeof(unsigned char)));
        memcpy(dataBuffer, blobBuffer, dataSize);
        blobEL->size = dataSize;
    }

    blobEL->blob = dataBuffer;

    if (mediator)
        mediator->newBLOB(blobEL);

   // newBLOB(dataBuffer, dataSize, dataFormat);

    free (blobBuffer);
    return (0);
}

void INDI::BaseDriver::setDeviceName(const char *dev)
{
    strncpy(deviceID, dev, MAXINDINAME);
}

const char * INDI::BaseDriver::deviceName()
{
    return deviceID;
}

void INDI::BaseDriver::addMessage(const char *msg)
{
    messageQueue.append(msg);
}

