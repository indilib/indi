#include "indidevapi.h"
#include "indicom.h"
#include "locale_compat.h"
#include "base64.h"
#include "userio.h"
#include "indiuserio.h"
#include "indiutility.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <Windows.h>
#endif

#define MAXRBUF 2048

/** \section IUSave */

void IUSaveConfigNumber(FILE *fp, const INumberVectorProperty *nvp)
{
    IUUserIONewNumber(userio_file(), fp, nvp);
}

void IUSaveConfigText(FILE *fp, const ITextVectorProperty *tvp)
{
    IUUserIONewText(userio_file(), fp, tvp);
}

void IUSaveConfigSwitch(FILE *fp, const ISwitchVectorProperty *svp)
{
    IUUserIONewSwitchFull(userio_file(), fp, svp);
}

void IUSaveConfigBLOB(FILE *fp, const IBLOBVectorProperty *bvp)
{
    IUUserIONewBLOB(userio_file(), fp, bvp);
}

/* save malloced copy of newtext in tp->text, reusing if not first time */
void IUSaveText(IText *tp, const char *newtext)
{
    /* copy in fresh string */
    size_t size = strlen(newtext) + 1;
    tp->text = realloc(tp->text, size);
    memcpy(tp->text, newtext, size);
}

int IUSaveBLOB(IBLOB *bp, int size, int blobsize, char *blob, char *format)
{
    bp->bloblen = blobsize;
    bp->size    = size;
    bp->blob    = blob;
    indi_strlcpy(bp->format, format, MAXINDIFORMAT);
    return 0;
}

/** Get configuration root XML pointer
 *  N.B. Must be freed by the caller */
XMLEle *configRootFP(const char *device)
{
    char configFileName[MAXRBUF];
    char configDir[MAXRBUF];
    char errmsg[MAXRBUF];
    struct stat st;
    FILE *fp = NULL;

    snprintf(configDir, MAXRBUF-1, "%s/.indi/", getenv("HOME"));

        if (getenv("INDICONFIG"))
            strncpy(configFileName, getenv("INDICONFIG"), MAXRBUF-1);
        else
            snprintf(configFileName, MAXRBUF-1, "%s%s_config.xml", configDir, device);


    if (stat(configDir, &st) != 0)
    {
#ifdef _WIN32
        if (mkdir(configDir) != 0)
#else
        if (mkdir(configDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0)
#endif
            return NULL;
    }

    stat(configFileName, &st);
    
#ifdef _WIN32
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsGroup))
    {
        if (CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin) == FALSE)
            isAdmin = FALSE;
        FreeSid(AdministratorsGroup);
    }
    if ((st.st_uid == 0 && st.st_mode & S_IFDIR) || (st.st_gid == 0 && isAdmin == TRUE))
        return NULL;
#else
    /* If file is owned by root and current user is NOT root then abort */
    if ( (st.st_uid == 0 && getuid() != 0) || (st.st_gid == 0 && getgid() != 0) )
        return NULL;
#endif

    fp = fopen(configFileName, "r");
    if (fp == NULL)
        return NULL;

    LilXML *lp = newLilXML();

    XMLEle *root = readXMLFile(fp, lp, errmsg);

    delLilXML(lp);
    fclose(fp);

    return root;
}
/** \section IULoad */

int IULoadConfigNumber(const INumberVectorProperty *nvp)
{
    char errmsg[1024];
    char *rdev, *rname;
    int foundCounter = 0;
    XMLEle *root = configRootFP(nvp->device);
    if (root == NULL)
        return -1;

    XMLEle *ep = NULL;

    for (ep = nextXMLEle(root, 1); ep != NULL; ep = nextXMLEle(root, 0))
    {
        /* pull out device and name */
        if (crackDN(ep, &rdev, &rname, errmsg) < 0)
        {
            delXMLEle(root);
            return -1;
        }

        // It doesn't belong to our device??
        if (strcmp(nvp->device, rdev))
            continue;

        if (!strcmp(nvp->name, rname))
        {
            XMLEle *element = NULL;
            for (element = nextXMLEle(ep, 1); element != NULL; element = nextXMLEle(ep, 0))
            {
                INumber *member = IUFindNumber(nvp, findXMLAttValu(element, "name"));
                if (member)
                {
                    member->value = atof(pcdataXMLEle(element));
                    foundCounter++;
                }
            }
            break;
        }
    }

    delXMLEle(root);
    return foundCounter;
}

int IULoadConfigText(const ITextVectorProperty *tvp)
{
    char errmsg[1024];
    char *rdev, *rname;
    int foundCounter = 0;
    XMLEle *root = configRootFP(tvp->device);
    if (root == NULL)
        return -1;

    XMLEle *ep = NULL;

    for (ep = nextXMLEle(root, 1); ep != NULL; ep = nextXMLEle(root, 0))
    {
        /* pull out device and name */
        if (crackDN(ep, &rdev, &rname, errmsg) < 0)
        {
            delXMLEle(root);
            return -1;
        }

        // It doesn't belong to our device??
        if (strcmp(tvp->device, rdev))
            continue;

        if (!strcmp(tvp->name, rname))
        {
            XMLEle *element = NULL;
            for (element = nextXMLEle(ep, 1); element != NULL; element = nextXMLEle(ep, 0))
            {
                IText *member = IUFindText(tvp, findXMLAttValu(element, "name"));
                if (member)
                {
                    IUSaveText(member, pcdataXMLEle(element));
                    foundCounter++;
                }
            }
            break;
        }
    }

    delXMLEle(root);
    return foundCounter;
}

int IULoadConfigSwitch(const ISwitchVectorProperty *svp)
{
    char errmsg[1024];
    char *rdev, *rname;
    int foundCounter = 0;
    XMLEle *root = configRootFP(svp->device);
    if (root == NULL)
        return -1;

    XMLEle *ep = NULL;

    for (ep = nextXMLEle(root, 1); ep != NULL; ep = nextXMLEle(root, 0))
    {
        /* pull out device and name */
        if (crackDN(ep, &rdev, &rname, errmsg) < 0)
        {
            delXMLEle(root);
            return -1;
        }

        // It doesn't belong to our device??
        if (strcmp(svp->device, rdev))
            continue;

        if (!strcmp(svp->name, rname))
        {
            XMLEle *element = NULL;
            for (element = nextXMLEle(ep, 1); element != NULL; element = nextXMLEle(ep, 0))
            {
                ISwitch *member = IUFindSwitch(svp, findXMLAttValu(element, "name"));
                if (member)
                {
                    ISState state;
                    if (crackISState(pcdataXMLEle(element), &state) == 0)
                    {
                        member->s = state;
                        foundCounter++;
                    }
                }
            }
            break;
        }
    }

    delXMLEle(root);
    return foundCounter;
}

/** \section IUFind */

/* find a member of an IText vector, else NULL */
IText *IUFindText(const ITextVectorProperty *tvp, const char *name)
{
    for (int i = 0; i < tvp->ntp; i++)
        if (strcmp(tvp->tp[i].name, name) == 0)
            return (&tvp->tp[i]);
    fprintf(stderr, "No IText '%s' in %s.%s\n", name, tvp->device, tvp->name);
    return (NULL);
}

/* find a member of an INumber vector, else NULL */
INumber *IUFindNumber(const INumberVectorProperty *nvp, const char *name)
{
    for (int i = 0; i < nvp->nnp; i++)
        if (strcmp(nvp->np[i].name, name) == 0)
            return (&nvp->np[i]);
    fprintf(stderr, "No INumber '%s' in %s.%s\n", name, nvp->device, nvp->name);
    return (NULL);
}

/* find a member of an ISwitch vector, else NULL */
ISwitch *IUFindSwitch(const ISwitchVectorProperty *svp, const char *name)
{
    for (int i = 0; i < svp->nsp; i++)
        if (strcmp(svp->sp[i].name, name) == 0)
            return (&svp->sp[i]);
    fprintf(stderr, "No ISwitch '%s' in %s.%s\n", name, svp->device, svp->name);
    return (NULL);
}

/* find a member of an ILight vector, else NULL */
ILight *IUFindLight(const ILightVectorProperty *lvp, const char *name)
{
    for (int i = 0; i < lvp->nlp; i++)
        if (strcmp(lvp->lp[i].name, name) == 0)
            return (&lvp->lp[i]);
    fprintf(stderr, "No ILight '%s' in %s.%s\n", name, lvp->device, lvp->name);
    return (NULL);
}

/* find a member of an IBLOB vector, else NULL */
IBLOB *IUFindBLOB(const IBLOBVectorProperty *bvp, const char *name)
{
    for (int i = 0; i < bvp->nbp; i++)
        if (strcmp(bvp->bp[i].name, name) == 0)
            return (&bvp->bp[i]);
    fprintf(stderr, "No IBLOB '%s' in %s.%s\n", name, bvp->device, bvp->name);
    return (NULL);
}

/* find an ON member of an ISwitch vector, else NULL.
 * N.B. user must make sense of result with ISRule in mind.
 */
ISwitch *IUFindOnSwitch(const ISwitchVectorProperty *svp)
{
    for (int i = 0; i < svp->nsp; i++)
        if (svp->sp[i].s == ISS_ON)
            return (&svp->sp[i]);
    /*fprintf(stderr, "No ISwitch On in %s.%s\n", svp->device, svp->name);*/
    return (NULL);
}

int IUFindIndex(const char *needle, char **hay, unsigned int n)
{
    for (int i = 0; i < (int)n; i++)
    {
        if (!strcmp(hay[i], needle))
            return i;
    }
    return -1;
}

/* Find index of the ON member of an ISwitchVectorProperty */
int IUFindOnSwitchIndex(const ISwitchVectorProperty *svp)
{
    for (int i = 0; i < svp->nsp; i++)
        if (svp->sp[i].s == ISS_ON)
            return i;
    return -1;
}

/* Find index of the ON member of ISState array */
int IUFindOnStateIndex(ISState *states, int n)
{
    for (int i = 0; i < n; i++)
        if (states[i] == ISS_ON)
            return i;
    return -1;
}

/* Find name the ON member in the given states and names */
const char *IUFindOnSwitchName(ISState *states, char *names[], int n)
{
    for (int i = 0; i < n; i++)
        if (states[i] == ISS_ON)
            return names[i];
    return NULL;
}

/** \section IUReset */

/* Set all switches to off */
void IUResetSwitch(ISwitchVectorProperty *svp)
{
    for (int i = 0; i < svp->nsp; i++)
        svp->sp[i].s = ISS_OFF;
}

/** \section IUUpdate */

/** \section IUFill */

void IUFillSwitch(ISwitch *sp, const char *name, const char *label, ISState s)
{
    indi_strlcpy(sp->name, name, sizeof(sp->name));

    indi_strlcpy(sp->label, label[0] ? label : name, sizeof(sp->label));

    sp->s   = s;
    sp->svp = NULL;
    sp->aux = NULL;
}

void IUFillLight(ILight *lp, const char *name, const char *label, IPState s)
{
    indi_strlcpy(lp->name, name, sizeof(lp->name));

    indi_strlcpy(lp->label, label[0] ? label : name, sizeof(lp->label));

    lp->s   = s;
    lp->lvp = NULL;
    lp->aux = NULL;
}

void IUFillNumber(INumber *np, const char *name, const char *label, const char *format, double min, double max,
                  double step, double value)
{
    indi_strlcpy(np->name, name, sizeof(np->name));

    indi_strlcpy(np->label, label[0] ? label : name, sizeof(np->label));

    indi_strlcpy(np->format, format, sizeof(np->format));

    np->min   = min;
    np->max   = max;
    np->step  = step;
    np->value = value;
    np->nvp   = NULL;
    np->aux0  = NULL;
    np->aux1  = NULL;
}

void IUFillText(IText *tp, const char *name, const char *label, const char *initialText)
{
    indi_strlcpy(tp->name, name, sizeof(tp->name));

    indi_strlcpy(tp->label, label[0] ? label : name, sizeof(tp->label));

    if (tp->text && tp->text[0])
        free(tp->text);
    tp->text = NULL;

    tp->tvp  = NULL;
    tp->aux0 = NULL;
    tp->aux1 = NULL;

    if (initialText && initialText[0])
        IUSaveText(tp, initialText);
}

void IUFillBLOB(IBLOB *bp, const char *name, const char *label, const char *format)
{
    memset(bp, 0, sizeof(IBLOB));

    indi_strlcpy(bp->name, name, sizeof(bp->name));

    indi_strlcpy(bp->label, label[0] ? label : name, sizeof(bp->label));

    indi_strlcpy(bp->format, format, sizeof(bp->format));

    bp->blob    = 0;
    bp->bloblen = 0;
    bp->size    = 0;
    bp->bvp     = 0;
    bp->aux0    = 0;
    bp->aux1    = 0;
    bp->aux2    = 0;
}

void IUFillSwitchVector(ISwitchVectorProperty *svp, ISwitch *sp, int nsp, const char *dev, const char *name,
                        const char *label, const char *group, IPerm p, ISRule r, double timeout, IPState s)
{
    indi_strlcpy(svp->device, dev, sizeof(svp->device));

    indi_strlcpy(svp->name, name, sizeof(svp->name));

    indi_strlcpy(svp->label, label[0] ? label : name, sizeof(svp->label));

    indi_strlcpy(svp->group, group, sizeof(svp->group));
    svp->timestamp[0] = '\0';

    svp->p       = p;
    svp->r       = r;
    svp->timeout = timeout;
    svp->s       = s;
    svp->sp      = sp;
    svp->nsp     = nsp;
}

void IUFillLightVector(ILightVectorProperty *lvp, ILight *lp, int nlp, const char *dev, const char *name,
                       const char *label, const char *group, IPState s)
{
    indi_strlcpy(lvp->device, dev, sizeof(lvp->device));

    indi_strlcpy(lvp->name, name, sizeof(lvp->name));

    indi_strlcpy(lvp->label, label[0] ? label : name, sizeof(lvp->label));

    indi_strlcpy(lvp->group, group, sizeof(lvp->group));
    lvp->timestamp[0] = '\0';

    lvp->s   = s;
    lvp->lp  = lp;
    lvp->nlp = nlp;
}

void IUFillNumberVector(INumberVectorProperty *nvp, INumber *np, int nnp, const char *dev, const char *name,
                        const char *label, const char *group, IPerm p, double timeout, IPState s)
{
    indi_strlcpy(nvp->device, dev, sizeof(nvp->device));

    indi_strlcpy(nvp->name, name, sizeof(nvp->name));

    indi_strlcpy(nvp->label, label[0] ? label : name, sizeof(nvp->label));

    indi_strlcpy(nvp->group, group, sizeof(nvp->group));
    nvp->timestamp[0] = '\0';

    nvp->p       = p;
    nvp->timeout = timeout;
    nvp->s       = s;
    nvp->np      = np;
    nvp->nnp     = nnp;
}

void IUFillTextVector(ITextVectorProperty *tvp, IText *tp, int ntp, const char *dev, const char *name,
                      const char *label, const char *group, IPerm p, double timeout, IPState s)
{
    indi_strlcpy(tvp->device, dev, sizeof(tvp->device));

    indi_strlcpy(tvp->name, name, sizeof(tvp->name));

    indi_strlcpy(tvp->label, label[0] ? label : name, sizeof(tvp->label));

    indi_strlcpy(tvp->group, group, sizeof(tvp->group));
    tvp->timestamp[0] = '\0';

    tvp->p       = p;
    tvp->timeout = timeout;
    tvp->s       = s;
    tvp->tp      = tp;
    tvp->ntp     = ntp;
}

void IUFillBLOBVector(IBLOBVectorProperty *bvp, IBLOB *bp, int nbp, const char *dev, const char *name,
                      const char *label, const char *group, IPerm p, double timeout, IPState s)
{
    memset(bvp, 0, sizeof(IBLOBVectorProperty));
    indi_strlcpy(bvp->device, dev, sizeof(bvp->device));

    indi_strlcpy(bvp->name, name, sizeof(bvp->name));

    indi_strlcpy(bvp->label, label[0] ? label : name, sizeof(bvp->label));

    indi_strlcpy(bvp->group, group, sizeof(bvp->group));
    bvp->timestamp[0] = '\0';

    bvp->p       = p;
    bvp->timeout = timeout;
    bvp->s       = s;
    bvp->bp      = bp;
    bvp->nbp     = nbp;
}

/** \section IUSnoop **/

/* crack the snooped driver setNumberVector or defNumberVector message into
 * the given INumberVectorProperty.
 * return 0 if type, device and name match and all members are present, else
 * return -1
 */
int IUSnoopNumber(XMLEle *root, INumberVectorProperty *nvp)
{
    char *dev, *name;
    XMLEle *ep;

    /* check and crack type, device, name and state */
    if (strcmp(tagXMLEle(root) + 3, "NumberVector") || crackDN(root, &dev, &name, NULL) < 0)
        return (-1);
    if (strcmp(dev, nvp->device) || strcmp(name, nvp->name))
        return (-1); /* not this property */
    (void)crackIPState(findXMLAttValu(root, "state"), &nvp->s);

    /* match each INumber with a oneNumber */
    locale_char_t *orig = indi_locale_C_numeric_push();
    for (int i = 0; i < nvp->nnp; i++)
    {
        for (ep = nextXMLEle(root, 1); ep; ep = nextXMLEle(root, 0))
        {
            if (!strcmp(tagXMLEle(ep) + 3, "Number") && !strcmp(nvp->np[i].name, findXMLAttValu(ep, "name")))
            {
                if (f_scansexa(pcdataXMLEle(ep), &nvp->np[i].value) < 0)
                {
                    indi_locale_C_numeric_pop(orig);
                    return (-1); /* bad number format */
                }
                break;
            }
        }
        if (!ep)
        {
            indi_locale_C_numeric_pop(orig);
            return (-1); /* element not found */
        }
    }
    indi_locale_C_numeric_pop(orig);

    /* ok */
    return (0);
}

/* crack the snooped driver setTextVector or defTextVector message into
 * the given ITextVectorProperty.
 * return 0 if type, device and name match and all members are present, else
 * return -1
 */
int IUSnoopText(XMLEle *root, ITextVectorProperty *tvp)
{
    char *dev, *name;
    XMLEle *ep;

    /* check and crack type, device, name and state */
    if (strcmp(tagXMLEle(root) + 3, "TextVector") || crackDN(root, &dev, &name, NULL) < 0)
        return (-1);
    if (strcmp(dev, tvp->device) || strcmp(name, tvp->name))
        return (-1); /* not this property */
    (void)crackIPState(findXMLAttValu(root, "state"), &tvp->s);

    /* match each IText with a oneText */
    for (int i = 0; i < tvp->ntp; i++)
    {
        for (ep = nextXMLEle(root, 1); ep; ep = nextXMLEle(root, 0))
        {
            if (!strcmp(tagXMLEle(ep) + 3, "Text") && !strcmp(tvp->tp[i].name, findXMLAttValu(ep, "name")))
            {
                IUSaveText(&tvp->tp[i], pcdataXMLEle(ep));
                break;
            }
        }
        if (!ep)
            return (-1); /* element not found */
    }

    /* ok */
    return (0);
}

/* crack the snooped driver setLightVector or defLightVector message into
 * the given ILightVectorProperty. it is not necessary that all ILight names
 * be found.
 * return 0 if type, device and name match, else return -1.
 */
int IUSnoopLight(XMLEle *root, ILightVectorProperty *lvp)
{
    char *dev, *name;
    XMLEle *ep;

    /* check and crack type, device, name and state */
    if (strcmp(tagXMLEle(root) + 3, "LightVector") || crackDN(root, &dev, &name, NULL) < 0)
        return (-1);
    if (strcmp(dev, lvp->device) || strcmp(name, lvp->name))
        return (-1); /* not this property */

    (void)crackIPState(findXMLAttValu(root, "state"), &lvp->s);

    /* match each oneLight with one ILight */
    for (ep = nextXMLEle(root, 1); ep; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep) + 3, "Light"))
        {
            const char *name = findXMLAttValu(ep, "name");
            for (int i = 0; i < lvp->nlp; i++)
            {
                if (!strcmp(lvp->lp[i].name, name))
                {
                    if (crackIPState(pcdataXMLEle(ep), &lvp->lp[i].s) < 0)
                    {
                        return (-1); /* unrecognized state */
                    }
                    break;
                }
            }
        }
    }

    /* ok */
    return (0);
}

/* crack the snooped driver setSwitchVector or defSwitchVector message into the
 * given ISwitchVectorProperty. it is not necessary that all ISwitch names be
 * found.
 * return 0 if type, device and name match, else return -1.
 */
int IUSnoopSwitch(XMLEle *root, ISwitchVectorProperty *svp)
{
    char *dev, *name;
    XMLEle *ep;

    /* check and crack type, device, name and state */
    if (strcmp(tagXMLEle(root) + 3, "SwitchVector") || crackDN(root, &dev, &name, NULL) < 0)
        return (-1);
    if (strcmp(dev, svp->device) || strcmp(name, svp->name))
        return (-1); /* not this property */
    (void)crackIPState(findXMLAttValu(root, "state"), &svp->s);

    /* match each oneSwitch with one ISwitch */
    for (ep = nextXMLEle(root, 1); ep; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep) + 3, "Switch"))
        {
            const char *name = findXMLAttValu(ep, "name");
            for (int i = 0; i < svp->nsp; i++)
            {
                if (!strcmp(svp->sp[i].name, name))
                {
                    if (crackISState(pcdataXMLEle(ep), &svp->sp[i].s) < 0)
                    {
                        return (-1); /* unrecognized state */
                    }
                    break;
                }
            }
        }
    }

    /* ok */
    return (0);
}

/* crack the snooped driver setBLOBVector message into the given
 * IBLOBVectorProperty. it is not necessary that all IBLOB names be found.
 * return 0 if type, device and name match, else return -1.
 * N.B. we assume any existing blob in bvp has been malloced, which we free
 * and replace with a newly malloced blob if found.
 */
int IUSnoopBLOB(XMLEle *root, IBLOBVectorProperty *bvp)
{
    char *dev, *name;
    XMLEle *ep;

    /* check and crack type, device, name and state */
    if (strcmp(tagXMLEle(root), "setBLOBVector") || crackDN(root, &dev, &name, NULL) < 0)
        return (-1);

    if (strcmp(dev, bvp->device) || strcmp(name, bvp->name))
        return (-1); /* not this property */

    crackIPState(findXMLAttValu(root, "state"), &bvp->s);

    for (ep = nextXMLEle(root, 1); ep; ep = nextXMLEle(root, 0))
    {
        if (strcmp(tagXMLEle(ep), "oneBLOB") == 0)
        {
            XMLAtt *na = findXMLAtt(ep, "name");
            if (na == NULL)
                return (-1);

            IBLOB *bp = IUFindBLOB(bvp, valuXMLAtt(na));

            if (bp == NULL)
                return (-1);

            XMLAtt *fa = findXMLAtt(ep, "format");
            XMLAtt *sa = findXMLAtt(ep, "size");
            if (fa && sa)
            {
                int base64datalen = pcdatalenXMLEle(ep);
                assert_mem(bp->blob = realloc(bp->blob, 3 * base64datalen / 4));
                bp->bloblen = from64tobits_fast(bp->blob, pcdataXMLEle(ep), base64datalen);
                indi_strlcpy(bp->format, valuXMLAtt(fa), MAXINDIFORMAT);
                bp->size = atoi(valuXMLAtt(sa));
            }
        }
    }

    /* ok */
    return (0);
}

/** \section `IS` */

/* Functions are implemented in defaultdevice.cpp */

/** \section other */

int crackDN(XMLEle *root, char **dev, char **name, char msg[])
{
    XMLAtt *ap;

    ap = findXMLAtt(root, "device");
    if (!ap)
    {
        sprintf(msg, "%s requires 'device' attribute", tagXMLEle(root));
        return (-1);
    }
    *dev = valuXMLAtt(ap);

    ap = findXMLAtt(root, "name");
    if (!ap)
    {
        sprintf(msg, "%s requires 'name' attribute", tagXMLEle(root));
        return (-1);
    }
    *name = valuXMLAtt(ap);

    return (0);
}

int crackIPState(const char *str, IPState *ip)
{
    if (!strcmp(str, "Idle"))
        *ip = IPS_IDLE;
    else if (!strncmp(str, "Ok", 2))
        *ip = IPS_OK;
    else if (!strcmp(str, "Busy"))
        *ip = IPS_BUSY;
    else if (!strcmp(str, "Alert"))
        *ip = IPS_ALERT;
    else
        return (-1);
    return (0);
}

int crackISState(const char *str, ISState *ip)
{
    if (!strncmp(str, "On", 2))
        *ip = ISS_ON;
    else if (!strcmp(str, "Off"))
        *ip = ISS_OFF;
    else
        return (-1);
    return (0);
}

int crackIPerm(const char *str, IPerm *ip)
{
    if (!strncmp(str, "rw", 2))
        *ip = IP_RW;
    else if (!strncmp(str, "ro", 2))
        *ip = IP_RO;
    else if (!strncmp(str, "wo", 2))
        *ip = IP_WO;
    else
        return (-1);
    return (0);
}

int crackISRule(const char *str, ISRule *ip)
{
    if (!strcmp(str, "OneOfMany"))
        *ip = ISR_1OFMANY;
    else if (!strcmp(str, "AtMostOne"))
        *ip = ISR_ATMOST1;
    else if (!strcmp(str, "AnyOfMany"))
        *ip = ISR_NOFMANY;
    else
        return (-1);
    return (0);
}

const char *pstateStr(IPState s)
{
    switch (s)
    {
        case IPS_IDLE:
            return "Idle";
        case IPS_OK:
            return "Ok";
        case IPS_BUSY:
            return "Busy";
        case IPS_ALERT:
            return "Alert";
        default:
            fprintf(stderr, "Impossible IPState %d\n", s);
            return NULL;
    }
}

const char *sstateStr(ISState s)
{
    switch (s)
    {
        case ISS_ON:
            return "On";
        case ISS_OFF:
            return "Off";
        default:
            fprintf(stderr, "Impossible ISState %d\n", s);
            return NULL;
    }
}

const char *ruleStr(ISRule r)
{
    switch (r)
    {
        case ISR_1OFMANY:
            return "OneOfMany";
        case ISR_ATMOST1:
            return "AtMostOne";
        case ISR_NOFMANY:
            return "AnyOfMany";
        default:
            fprintf(stderr, "Impossible ISRule %d\n", r);
            return NULL;
    }
}

const char *permStr(IPerm p)
{
    switch (p)
    {
        case IP_RO:
            return "ro";
        case IP_WO:
            return "wo";
        case IP_RW:
            return "rw";
        default:
            fprintf(stderr, "Impossible IPerm %d\n", p);
            return NULL;
    }
}

void xmlv1()
{
    userio_xmlv1(userio_file(), stdout);
}
