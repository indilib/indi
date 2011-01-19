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
    delLilXML (lp);
}

INumberVectorProperty * INDI::BaseDriver::getNumber(const char *name)
{
    INumberVectorProperty * nvp = NULL;

    nvp = static_cast<INumberVectorProperty *> (getProperty(name, INDI_NUMBER));

    return nvp;
}

ITextVectorProperty * INDI::BaseDriver::getText(const char *name)
{
    ITextVectorProperty * tvp = NULL;

    tvp = static_cast<ITextVectorProperty *> (getProperty(name, INDI_TEXT));

    return tvp;
}

ISwitchVectorProperty * INDI::BaseDriver::getSwitch(const char *name)
{
    ISwitchVectorProperty * svp = NULL;

    svp = static_cast<ISwitchVectorProperty *> (getProperty(name, INDI_SWITCH));

    return svp;
}

ILightVectorProperty * INDI::BaseDriver::getLight(const char *name)
{
    ILightVectorProperty * lvp = NULL;

    lvp = static_cast<ILightVectorProperty *> (getProperty(name, INDI_LIGHT));

    return lvp;
}

IBLOBVectorProperty * INDI::BaseDriver::getBLOB(const char *name)
{       
  IBLOBVectorProperty * bvp = NULL;

  bvp = static_cast<IBLOBVectorProperty *> (getProperty(name, INDI_BLOB));

  return bvp;
}

void * INDI::BaseDriver::getProperty(const char *name, INDI_TYPE type)
{
    std::map< boost::shared_ptr<void>, INDI_TYPE>::iterator orderi;

    INumberVectorProperty *nvp;
    ITextVectorProperty *tvp;
    ISwitchVectorProperty *svp;
    ILightVectorProperty *lvp;
    IBLOBVectorProperty *bvp;

    for (orderi = pAll.begin(); orderi != pAll.end(); orderi++)
    {
        if (type != INDI_UNKNOWN &&  orderi->second != type)
            continue;

        switch (orderi->second)
        {
        case INDI_NUMBER:
            nvp = static_cast<INumberVectorProperty *>((orderi->first).get());
            if (!strcmp(name, nvp->name))
                return (orderi->first).get();
             break;
        case INDI_TEXT:
             tvp = static_cast<ITextVectorProperty *>((orderi->first).get());
             if (!strcmp(name, tvp->name))
                return (orderi->first).get();
             break;
        case INDI_SWITCH:
             svp = static_cast<ISwitchVectorProperty *>((orderi->first).get());
             if (!strcmp(name, svp->name))
                 return (orderi->first).get();
             break;
        case INDI_LIGHT:
             lvp = static_cast<ILightVectorProperty *>((orderi->first).get());
             if (!strcmp(name, lvp->name))
                 return (orderi->first).get();
             break;
        case INDI_BLOB:
             bvp = static_cast<IBLOBVectorProperty *>((orderi->first).get());
             if (!strcmp(name, bvp->name))
                 return (orderi->first).get();
             break;
        }

    }

    return NULL;
}

int INDI::BaseDriver::removeProperty(const char *name)
{
    std::map< boost::shared_ptr<void>, INDI_TYPE>::iterator orderi;

    INumberVectorProperty *nvp;
    ITextVectorProperty *tvp;
    ISwitchVectorProperty *svp;
    ILightVectorProperty *lvp;
    IBLOBVectorProperty *bvp;

    for (orderi = pAll.begin(); orderi != pAll.end(); orderi++)
    {
        switch (orderi->second)
        {
        case INDI_NUMBER:
            nvp = static_cast<INumberVectorProperty *>((orderi->first).get());
            if (!strcmp(name, nvp->name))
            {
                 pAll.erase(orderi);
                 return 0;
             }
             break;
        case INDI_TEXT:
             tvp = static_cast<ITextVectorProperty *>((orderi->first).get());
             if (!strcmp(name, tvp->name))
             {
                  pAll.erase(orderi);
                  return 0;
              }
             break;
        case INDI_SWITCH:
             svp = static_cast<ISwitchVectorProperty *>((orderi->first).get());
             if (!strcmp(name, svp->name))
             {
                  pAll.erase(orderi);
                  return 0;
              }
             break;
        case INDI_LIGHT:
             lvp = static_cast<ILightVectorProperty *>((orderi->first).get());
             if (!strcmp(name, lvp->name))
             {
                  pAll.erase(orderi);
                  return 0;
              }
             break;
        case INDI_BLOB:
             bvp = static_cast<IBLOBVectorProperty *>((orderi->first).get());
             if (!strcmp(name, bvp->name))
             {
                  pAll.erase(orderi);
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
    //INDI_TYPE type;
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

    //if (getProperty(rname, type) != NULL)
    if (getProperty(rname) != NULL)
        return INDI::BaseClient::INDI_PROPERTY_DUPLICATED;

    if (strcmp (rtag, "defLightVector") && crackIPerm(findXMLAttValu(root, "perm"), &perm) < 0)
    {
        IDLog("Error extracting %s permission (%s)\n", rname, findXMLAttValu(root, "perm"));
        return -1;
    }

    timeout = atoi(findXMLAttValu(root, "timeout"));

    if (crackIPState (findXMLAttValu(root, "state"), &state) < 0)
    {
        IDLog("Error extracting %s state (%s)\n", rname, findXMLAttValu(root, "state"));
        return -1;
    }

    if (!strcmp (rtag, "defNumberVector"))
    {

        numberPtr nvp(new INumberVectorProperty);
        //INumberVectorProperty *nvp = new INumberVectorProperty;
        //INumberVectorProperty *nvp = (INumberVectorProperty *) malloc(sizeof(INumberVectorProperty));
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

            np[n].nvp = nvp.get();

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
        //orderPtr o(new pOrder);
        //o->p = &nvp;
        //o->type = INDI_NUMBER;
        //pAll.push_back(o);
        pAll[nvp] = INDI_NUMBER;
        //IDLog("Adding number property %s to list.\n", nvp->name);
        if (mediator)
            mediator->newProperty(nvp->device, nvp->name);
    }
    else
        IDLog("%s: newNumberVector with no valid members\n",rname);
}
  else if (!strcmp (rtag, "defSwitchVector"))
        {
            switchPtr svp(new ISwitchVectorProperty);
            //ISwitchVectorProperty *svp = new ISwitchVectorProperty;
            //ISwitchVectorProperty *svp = (ISwitchVectorProperty *) malloc(sizeof(ISwitchVectorProperty));
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

                sp[n].svp = svp.get();

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
            //orderPtr o(new pOrder);
            //o->p = &svp;
            //o->type = INDI_SWITCH;
            //pAll.push_back(o);
            pAll[svp] = INDI_SWITCH;
            //IDLog("Adding Switch property %s to list.\n", svp->name);
            if (mediator)
                mediator->newProperty(svp->device, svp->name);
        }
        else
            IDLog("%s: newSwitchVector with no valid members\n",rname);
    }

else if (!strcmp (rtag, "defTextVector"))
    {

        //ITextVectorProperty *tvp = new ITextVectorProperty;
        //ITextVectorProperty *tvp = (ITextVectorProperty *) malloc(sizeof(ITextVectorProperty));
        textPtr tvp(new ITextVectorProperty);
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

            tp[n].tvp = tvp.get();

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
        //orderPtr o(new pOrder);
        //o->p = &tvp;
        //o->type = INDI_TEXT;
        //pAll.push_back(o);
        pAll[tvp] = INDI_TEXT;
        //IDLog("Adding Text property %s to list.\n", tvp->name);
        if (mediator)
            mediator->newProperty(tvp->device, tvp->name);
    }
    else
        IDLog("%s: newTextVector with no valid members\n",rname);
}
else if (!strcmp (rtag, "defLightVector"))
    {

        //ILightVectorProperty *lvp = new ILightVectorProperty;
        //ILightVectorProperty *lvp = (ILightVectorProperty *) malloc(sizeof(ILightVectorProperty));
        lightPtr lvp(new ILightVectorProperty);
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

            lp[n].lvp = lvp.get();

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
        //orderPtr o(new pOrder);
        //o->p = &lvp;
        //o->type = INDI_LIGHT;
        //pAll.push_back(o);
        pAll[lvp] = INDI_LIGHT;
        //IDLog("Adding Light property %s to list.\n", lvp->name);
        if (mediator)
            mediator->newProperty(lvp->device, lvp->name);
    }
    else
        IDLog("%s: newLightVector with no valid members\n",rname);
}
else if (!strcmp (rtag, "defBLOBVector"))
    {

        //IBLOBVectorProperty *bvp = new IBLOBVectorProperty;
        //IBLOBVectorProperty *bvp = (IBLOBVectorProperty *) malloc(sizeof(IBLOBVectorProperty));
        blobPtr bvp(new IBLOBVectorProperty);
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

            bp[n].bvp = bvp.get();

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
        //orderPtr o(new pOrder);
        //o->p = &bvp;
        //o->type = INDI_BLOB;
        //pAll.push_back(o);
        pAll[bvp] = INDI_BLOB;
        //IDLog("Adding BLOB property %s to list.\n", bvp->name);
        if (mediator)
            mediator->newProperty(bvp->device, bvp->name);
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

void INDI::BaseDriver::registerProperty(void *p, INDI_TYPE type)
{


    if (type == INDI_NUMBER)
    {
        INumberVectorProperty *nvp = static_cast<INumberVectorProperty *> (p);
        if ( (getProperty(nvp->name), INDI_NUMBER) != NULL)
            return;

        numberPtr ovp(nvp);

        pAll[ovp] = INDI_NUMBER;
    }
    else if (type == INDI_TEXT)
    {
       ITextVectorProperty *tvp = static_cast<ITextVectorProperty *> (p);
       if ( (getProperty(tvp->name), INDI_TEXT) != NULL)
           return;

       textPtr ovp(tvp);
       //o->p = &ovp;

       pAll[ovp] = INDI_TEXT;
   }
    else if (type == INDI_SWITCH)
    {
       ISwitchVectorProperty *svp = static_cast<ISwitchVectorProperty *> (p);
       if ( (getProperty(svp->name), INDI_SWITCH) != NULL)
           return;

       switchPtr ovp(svp);
       //o->p = &ovp;

       pAll[ovp] = INDI_SWITCH;
    }
    else if (type == INDI_LIGHT)
    {
       ILightVectorProperty *lvp = static_cast<ILightVectorProperty *> (p);
       if ( (getProperty(lvp->name), INDI_LIGHT) != NULL)
           return;

       lightPtr ovp(lvp);
       //o->p = &ovp;
       pAll[ovp] = INDI_LIGHT;
   }
    else if (type == INDI_BLOB)
    {
       IBLOBVectorProperty *bvp = static_cast<IBLOBVectorProperty *> (p);
       if ( (getProperty(bvp->name), INDI_BLOB) != NULL)
           return;

       blobPtr ovp(bvp);
       //o->p = &ovp;

       pAll[ovp] = INDI_BLOB;
    }

    //o->type = type;

    //pAll.push_back(o);
}

