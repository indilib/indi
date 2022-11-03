#define _GNU_SOURCE 1

#include "indiapi.h"
#include "indiuserio.h"
#include "lilxml.h"
#include "base64.h"
#include "locale_compat.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <math.h>

/* sprint the variable a in sexagesimal format into out[].
 * w is the number of spaces for the whole part.
 * fracbase is the number of pieces a whole is to broken into; valid options:
 *	360000:	<w>:mm:ss.ss
 *	36000:	<w>:mm:ss.s
 *	3600:	<w>:mm:ss
 *	600:	<w>:mm.m
 *	60:	<w>:mm
 * return number of characters written to out, not counting final '\0'.
 */
int fs_sexa(char *out, double a, int w, int fracbase)
{
    char *out0 = out;
    unsigned long n;
    int d;
    int f;
    int m;
    int s;
    int isneg;

    /* save whether it's negative but do all the rest with a positive */
    isneg = (a < 0);
    if (isneg)
        a = -a;

    /* convert to an integral number of whole portions */
    n = (unsigned long)(a * fracbase + 0.5);
    d = n / fracbase;
    f = n % fracbase;

    /* form the whole part; "negative 0" is a special case */
    if (isneg && d == 0)
        out += snprintf(out, MAXINDIFORMAT, "%*s-0", w - 2, "");
    else
        out += snprintf(out, MAXINDIFORMAT, "%*d", w, isneg ? -d : d);

    /* do the rest */
    switch (fracbase)
    {
    case 60: /* dd:mm */
        m = f / (fracbase / 60);
        out += snprintf(out, MAXINDIFORMAT, ":%02d", m);
        break;
    case 600: /* dd:mm.m */
        out += snprintf(out, MAXINDIFORMAT, ":%02d.%1d", f / 10, f % 10);
        break;
    case 3600: /* dd:mm:ss */
        m = f / (fracbase / 60);
        s = f % (fracbase / 60);
        out += snprintf(out, MAXINDIFORMAT, ":%02d:%02d", m, s);
        break;
    case 36000: /* dd:mm:ss.s*/
        m = f / (fracbase / 60);
        s = f % (fracbase / 60);
        out += snprintf(out, MAXINDIFORMAT, ":%02d:%02d.%1d", m, s / 10, s % 10);
        break;
    case 360000: /* dd:mm:ss.ss */
        m = f / (fracbase / 60);
        s = f % (fracbase / 60);
        out += snprintf(out, MAXINDIFORMAT, ":%02d:%02d.%02d", m, s / 100, s % 100);
        break;
    default:
        printf("fs_sexa: unknown fracbase: %d\n", fracbase);
        return -1;
    }

    return (out - out0);
}

/* convert sexagesimal string str AxBxC to double.
 *   x can be anything non-numeric. Any missing A, B or C will be assumed 0.
 *   optional - and + can be anywhere.
 * return 0 if ok, -1 if can't find a thing.
 */
int f_scansexa(const char *str0, /* input string */
               double *dp)       /* cracked value, if return 0 */
{
    locale_char_t *orig = indi_locale_C_numeric_push();

    double a = 0, b = 0, c = 0;
    char str[128];
    //char *neg;
    uint8_t isNegative=0;
    int r= 0;

    /* copy str0 so we can play with it */
    strncpy(str, str0, sizeof(str) - 1);
    str[sizeof(str) - 1] = '\0';

    /* remove any spaces */
    char* i = str;
    char* j = str;
    while(*j != 0)
    {
        *i = *j++;
        if(*i != ' ')
            i++;
    }
    *i = 0;

    // This has problem process numbers in scientific notations e.g. 1e-06
    /*neg = strchr(str, '-');
    if (neg)
        *neg = ' ';
    */
    if (str[0] == '-')
    {
        isNegative = 1;
        str[0] = ' ';
    }

    r = sscanf(str, "%lf%*[^0-9]%lf%*[^0-9]%lf", &a, &b, &c);

    indi_locale_C_numeric_pop(orig);

    if (r < 1)
        return (-1);
    *dp = a + b / 60 + c / 3600;
    if (isNegative)
        *dp *= -1;
    return (0);
}

void getSexComponents(double value, int *d, int *m, int *s)
{
    *d = (int32_t)fabs(value);
    *m = (int32_t)((fabs(value) - *d) * 60.0);
    *s = (int32_t)rint(((fabs(value) - *d) * 60.0 - *m) * 60.0);

    // Special case if seconds are >= 59.5 so it will be rounded by rint above
    // to 60
    if (*s == 60)
    {
        *s  = 0;
        *m += 1;
    }
    if (*m == 60)
    {
        *m  = 0;
        *d += 1;
    }

    if (value < 0)
        *d *= -1;
}

void getSexComponentsIID(double value, int *d, int *m, double *s)
{
    *d = (int32_t)fabs(value);
    *m = (int32_t)((fabs(value) - *d) * 60.0);
    *s = (double)(((fabs(value) - *d) * 60.0 - *m) * 60.0);

    if (value < 0)
        *d *= -1;
}

/* fill buf with properly formatted INumber string. return length */
int numberFormat(char *buf, const char *format, double value)
{
    int w, f, s;
    char m;

    if (sscanf(format, "%%%d.%d%c", &w, &f, &m) == 3 && m == 'm')
    {
        /* INDI sexi format */
        switch (f)
        {
        case 9:  s = 360000;  break;
        case 8:  s = 36000;   break;
        case 6:  s = 3600;    break;
        case 5:  s = 600;     break;
        default: s = 60;      break;
        }
        return (fs_sexa(buf, value, w - f, s));
    }
    else
    {
        /* normal printf format */
        return (snprintf(buf, MAXINDIFORMAT, format, value));
    }
}

/* return current system time in message format */
const char *timestamp()
{
    static char ts[32];
    struct tm *tp;
    time_t t;

    time(&t);
    tp = gmtime(&t);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tp);
    return (ts);
}

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

/* Find name the ON member in the given states and names */
const char *IUFindOnSwitchName(ISState *states, char *names[], int n)
{
    for (int i = 0; i < n; i++)
        if (states[i] == ISS_ON)
            return names[i];
    return NULL;
}

/* Set all switches to off */
void IUResetSwitch(ISwitchVectorProperty *svp)
{
    for (int i = 0; i < svp->nsp; i++)
        svp->sp[i].s = ISS_OFF;
}

/* save malloced copy of newtext in tp->text, reusing if not first time */
void IUSaveText(IText *tp, const char *newtext)
{
    /* copy in fresh string */
    tp->text = strcpy(realloc(tp->text, strlen(newtext) + 1), newtext);
}

int IUSaveBLOB(IBLOB *bp, int size, int blobsize, char *blob, char *format)
{
    bp->bloblen = blobsize;
    bp->size    = size;
    bp->blob    = blob;
    strncpy(bp->format, format, MAXINDIFORMAT);
    return 0;
}

void IUFillSwitch(ISwitch *sp, const char *name, const char *label, ISState s)
{
    strncpy(sp->name, name, sizeof(sp->name));

    if (label[0])
        strncpy(sp->label, label, sizeof(sp->label));
    else
        strncpy(sp->label, name, sizeof(sp->label));

    sp->s   = s;
    sp->svp = NULL;
    sp->aux = NULL;
}

void IUFillLight(ILight *lp, const char *name, const char *label, IPState s)
{
    strncpy(lp->name, name, sizeof(lp->name));

    if (label[0])
        strncpy(lp->label, label, sizeof(lp->label));
    else
        strncpy(lp->label, name, sizeof(lp->label));

    lp->s   = s;
    lp->lvp = NULL;
    lp->aux = NULL;
}

void IUFillNumber(INumber *np, const char *name, const char *label, const char *format, double min, double max,
                  double step, double value)
{
    strncpy(np->name, name, sizeof(np->name));

    if (label[0])
        strncpy(np->label, label, sizeof(np->label));
    else
        strncpy(np->label, name, sizeof(np->label));

    strncpy(np->format, format, sizeof(np->format));

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
    strncpy(tp->name, name, sizeof(tp->name));

    if (label[0])
        strncpy(tp->label, label, sizeof(tp->label));
    else
        strncpy(tp->label, name, sizeof(tp->label));

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

    strncpy(bp->name, name, sizeof(bp->name));

    if (label[0])
        strncpy(bp->label, label, sizeof(bp->label));
    else
        strncpy(bp->label, name, sizeof(bp->label));

    strncpy(bp->format, format, sizeof(bp->format));

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
    strncpy(svp->device, dev, sizeof(svp->device));

    strncpy(svp->name, name, sizeof(svp->name));

    if (label[0])
        strncpy(svp->label, label, sizeof(svp->label));
    else
        strncpy(svp->label, name, sizeof(svp->label));

    strncpy(svp->group, group, sizeof(svp->group));
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
    strncpy(lvp->device, dev, sizeof(lvp->device));

    strncpy(lvp->name, name, sizeof(lvp->name));

    if (label[0])
        strncpy(lvp->label, label, sizeof(lvp->label));
    else
        strncpy(lvp->label, name, sizeof(lvp->label));

    strncpy(lvp->group, group, sizeof(lvp->group));
    lvp->timestamp[0] = '\0';

    lvp->s   = s;
    lvp->lp  = lp;
    lvp->nlp = nlp;
}

void IUFillNumberVector(INumberVectorProperty *nvp, INumber *np, int nnp, const char *dev, const char *name,
                        const char *label, const char *group, IPerm p, double timeout, IPState s)
{
    strncpy(nvp->device, dev, sizeof(nvp->device));

    strncpy(nvp->name, name, sizeof(nvp->name));

    if (label[0])
        strncpy(nvp->label, label, sizeof(nvp->label));
    else
        strncpy(nvp->label, name, sizeof(nvp->label));

    strncpy(nvp->group, group, sizeof(nvp->group));
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
    strncpy(tvp->device, dev, sizeof(tvp->device));

    strncpy(tvp->name, name, sizeof(tvp->name));

    if (label[0])
        strncpy(tvp->label, label, sizeof(tvp->label));
    else
        strncpy(tvp->label, name, sizeof(tvp->label));

    strncpy(tvp->group, group, sizeof(tvp->group));
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
    strncpy(bvp->device, dev, sizeof(bvp->device));

    strncpy(bvp->name, name, sizeof(bvp->name));

    if (label[0])
        strncpy(bvp->label, label, sizeof(bvp->label));
    else
        strncpy(bvp->label, name, sizeof(bvp->label));

    strncpy(bvp->group, group, sizeof(bvp->group));
    bvp->timestamp[0] = '\0';

    bvp->p       = p;
    bvp->timeout = timeout;
    bvp->s       = s;
    bvp->bp      = bp;
    bvp->nbp     = nbp;
}

/*****************************************************************************
 * convenience functions for use in your implementation of ISSnoopDevice().
 */

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
 *   and replace with a newly malloced blob if found.
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
            XMLAtt *ec = findXMLAtt(ep, "enclen");
            if (fa && sa && ec)
            {
                int enclen  = atoi(valuXMLAtt(ec));
                assert_mem(bp->blob = realloc(bp->blob, 3 * enclen / 4));
                bp->bloblen = from64tobits_fast(bp->blob, pcdataXMLEle(ep), enclen);
                strncpy(bp->format, valuXMLAtt(fa), MAXINDIFORMAT);
                bp->size = atoi(valuXMLAtt(sa));
            }
        }
    }

    /* ok */
    return (0);
}

/* return static string corresponding to the given property or light state */
const char *pstateStr(IPState s)
{
    switch (s)
    {
    case IPS_IDLE:  return "Idle";
    case IPS_OK:    return "Ok";
    case IPS_BUSY:  return "Busy";
    case IPS_ALERT: return "Alert";
    default:
        fprintf(stderr, "Impossible IPState %d\n", s);
        return NULL;
    }
}

/* pull out device and name attributes from root.
 * return 0 if ok else -1 with reason in msg[].
 */
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

/* crack string into IPState.
 * return 0 if ok, else -1
 */
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

/* crack string into ISState.
 * return 0 if ok, else -1
 */
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

/* return static string corresponding to the given switch state */
const char *sstateStr(ISState s)
{
    switch (s)
    {
    case ISS_ON:  return "On";
    case ISS_OFF: return "Off";
    default:
        fprintf(stderr, "Impossible ISState %d\n", s);
        return NULL;
    }
}

/* return static string corresponding to the given Rule */
const char *ruleStr(ISRule r)
{
    switch (r)
    {
    case ISR_1OFMANY: return "OneOfMany";
    case ISR_ATMOST1: return "AtMostOne";
    case ISR_NOFMANY: return "AnyOfMany";
    default:
        fprintf(stderr, "Impossible ISRule %d\n", r);
        return NULL;
    }
}

/* return static string corresponding to the given IPerm */
const char *permStr(IPerm p)
{
    switch (p)
    {
    case IP_RO: return "ro";
    case IP_WO: return "wo";
    case IP_RW: return "rw";
    default:
        fprintf(stderr, "Impossible IPerm %d\n", p);
        return NULL;
    }
}
