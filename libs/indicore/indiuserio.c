/*
    Copyright (C) 2021 by Pawel Soja <kernel32.pl@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "indiuserio.h"
#include "indiapi.h"
#include "indidevapi.h"
#include "indicom.h"
#include "locale_compat.h"
#include "base64.h"

#include <stdlib.h>
#include <string.h>

static void s_userio_xml_message_vprintf(const userio *io, void *user, const char *fmt, va_list ap)
{
    char message[MAXINDIMESSAGE];
    if (fmt)
    {
        vsnprintf(message, MAXINDIMESSAGE, fmt, ap);

        userio_prints    (io, user, "  message='");
        userio_xml_escape(io, user, message);
        userio_prints    (io, user, "'\n");
    }
}


void IUUserIONumberContext(const userio *io, void *user, const INumberVectorProperty *nvp)
{
    for (int i = 0; i < nvp->nnp; i++)
    {
        INumber *np = &nvp->np[i];
        userio_prints    (io, user, "  <oneNumber name='");
        userio_xml_escape(io, user, np->name);
        userio_prints    (io, user, "'>\n");
        userio_printf    (io, user, "      %.20g\n", np->value); // safe
        userio_prints    (io, user, "  </oneNumber>\n");
    }
}

void IUUserIOTextContext(const userio *io, void *user, const ITextVectorProperty *tvp)
{
    for (int i = 0; i < tvp->ntp; i++)
    {
        IText *tp = &tvp->tp[i];
        userio_prints    (io, user, "  <oneText name='");
        userio_xml_escape(io, user, tp->name);
        userio_prints    (io, user, "'>\n"
                                    "      ");
        if (tp->text)
            userio_xml_escape(io, user, tp->text);
        userio_prints    (io, user, "\n"
                                    "  </oneText>\n");
    }
}

void IUUserIOSwitchContextOne(const userio *io, void *user, const ISwitch *sp)
{
    userio_prints    (io, user, "  <oneSwitch name='");
    userio_xml_escape(io, user, sp->name);
    userio_prints    (io, user, "'>\n"
                                "      ");
    userio_prints    (io, user, sstateStr(sp->s));
    userio_prints    (io, user, "\n"
                                "  </oneSwitch>\n");
}

void IUUserIOSwitchContextFull(const userio *io, void *user, const ISwitchVectorProperty *svp)
{
    for (int i = 0; i < svp->nsp; i++)
    {
        ISwitch *sp = &svp->sp[i];
        IUUserIOSwitchContextOne(io, user, sp);
    }
}

void IUUserIOSwitchContext(const userio *io, void *user, const ISwitchVectorProperty *svp)
{
    ISwitch *onSwitch = IUFindOnSwitch(svp);

    if (svp->r == ISR_1OFMANY && onSwitch)
        IUUserIOSwitchContextOne(io, user, onSwitch);
    else
        IUUserIOSwitchContextFull(io, user, svp);
}

void IUUserIOBLOBContextOne(
    const userio *io, void *user,
    const char *name, unsigned int size, unsigned int bloblen, const void *blob, const char *format
)
{
    unsigned char *encblob;
    int l;

    userio_prints    (io, user, "  <oneBLOB\n"
                                "    name='");
    userio_xml_escape(io, user, name);
    userio_prints    (io, user, "'\n");
    userio_printf    (io, user, "    size='%d'\n", size); // safe

    // If size is zero, we are only sending a state-change
    if (size == 0)
    {
        userio_prints    (io, user, "    enclen='0'\n"
                                    "    format='");
        userio_xml_escape(io, user, format);
        userio_prints    (io, user, "'>\n");
    }
    else
    {
        if (io->joinbuff) {
            userio_prints    (io, user, "    format='");
            userio_xml_escape(io, user, format);
            userio_prints    (io, user, "'\n");
            userio_printf    (io, user, "    len='%d'\n", bloblen);

            io->joinbuff(user, "    attached='true'>\n", (void*)blob, bloblen);
        } else {
            size_t sz = 4 * bloblen / 3 + 4;
            assert_mem(encblob = (unsigned char *)malloc(sz)); // #PS: TODO
            l = to64frombits_s(encblob, blob, bloblen, sz);
            if (l == 0) {
                fprintf(stderr, "%s: Not enough memory for decoding.\n", __func__);
                exit(1);
            }
            userio_printf    (io, user, "    enclen='%d'\n", l); // safe
            userio_prints    (io, user, "    format='");
            userio_xml_escape(io, user, format);
            userio_prints    (io, user, "'>\n");
            size_t written = 0;
            // FIXME: this is not efficient. The CR/LF imply one more copy... Do we need them ?
            while ((int)written < l)
            {
                size_t towrite = ((l - written) > 72) ? 72 : l - written;
                size_t wr      = userio_write(io, user, encblob + written, towrite);

                if (wr == 0)
                {
                    free(encblob);
                    return;
                }

                written += wr;
                if ((written % 72) == 0)
                    userio_putc(io, user, '\n');
            }

            if ((written % 72) != 0)
                userio_putc(io, user, '\n');

            free(encblob);
        }
    }

    userio_prints    (io, user, "  </oneBLOB>\n");
}

void IUUserIOBLOBContext(const userio *io, void *user, const IBLOBVectorProperty *bvp)
{
    for (int i = 0; i < bvp->nbp; i++)
    {
        IBLOB *bp = &bvp->bp[i];
        IUUserIOBLOBContextOne(
            io, user,
            bp->name, bp->size, bp->bloblen, bp->blob, bp->format
        );
    }
}

void IUUserIOLightContext(const userio *io, void *user, const ILightVectorProperty *lvp)
{
    for (int i = 0; i < lvp->nlp; i++)
    {
        ILight *lp = &lvp->lp[i];
        userio_prints    (io, user, "  <oneLight name='");
        userio_xml_escape(io, user, lp->name);
        userio_prints    (io, user, "'>\n"
                                    "      ");
        userio_prints    (io, user, pstateStr(lp->s));
        userio_prints    (io, user, "\n"
                                    "  </oneLight>\n");
    }
}

void IUUserIONewNumber(const userio *io, void *user, const INumberVectorProperty *nvp)
{
    locale_char_t *orig = indi_locale_C_numeric_push();

    userio_prints    (io, user, "<newNumberVector device='");
    userio_xml_escape(io, user, nvp->device);
    userio_prints    (io, user, "' name='");
    userio_xml_escape(io, user, nvp->name);
    userio_prints    (io, user, "'>\n");

    IUUserIONumberContext(io, user, nvp);

    userio_prints    (io, user, "</newNumberVector>\n");

    indi_locale_C_numeric_pop(orig);
}

void IUUserIONewText(const userio *io, void *user, const ITextVectorProperty *tvp)
{
    userio_prints    (io, user, "<newTextVector device='");
    userio_xml_escape(io, user, tvp->device);
    userio_prints    (io, user, "' name='");
    userio_xml_escape(io, user, tvp->name);
    userio_prints    (io, user, "'>\n");

    IUUserIOTextContext(io, user, tvp);

    userio_prints    (io, user, "</newTextVector>\n");
}

void IUUserIONewSwitchFull(const userio *io, void *user, const ISwitchVectorProperty *svp)
{
    userio_prints    (io, user, "<newSwitchVector device='");
    userio_xml_escape(io, user, svp->device);
    userio_prints    (io, user, "' name='");
    userio_xml_escape(io, user, svp->name);
    userio_prints    (io, user, "'>\n");
    IUUserIOSwitchContextFull(io, user, svp);
    userio_prints    (io, user, "</newSwitchVector>\n");
}

void IUUserIONewSwitch(const userio *io, void *user, const ISwitchVectorProperty *svp)
{
    userio_prints    (io, user, "<newSwitchVector device='");
    userio_xml_escape(io, user, svp->device);
    userio_prints    (io, user, "' name='");
    userio_xml_escape(io, user, svp->name);
    userio_prints    (io, user, "'>\n");
    IUUserIOSwitchContext(io, user, svp);
    userio_prints    (io, user, "</newSwitchVector>\n");
}

void IUUserIONewBLOB(const userio *io, void *user, const IBLOBVectorProperty *bvp)
{
    IUUserIONewBLOBStart(io, user, bvp->device, bvp->name, NULL);
    
    IUUserIOBLOBContext(io, user, bvp);

    IUUserIONewBLOBFinish(io, user);
}

void IUUserIONewBLOBStart(
    const userio *io, void *user,
    const char *dev, const char *name, const char *timestamp
)
{
    userio_prints    (io, user, "<newBLOBVector\n"
                                "  device='");
    userio_xml_escape(io, user, dev);
    userio_prints    (io, user, "'\n"
                                "  name='");
    userio_xml_escape(io, user, name);
    userio_prints    (io, user, "'\n");
    if (timestamp != NULL)
    {
        userio_prints    (io, user, "  timestamp='");
        userio_xml_escape(io, user, timestamp);
        userio_prints    (io, user, "'\n");
    }
    userio_prints    (io, user, ">\n");
}

void IUUserIONewBLOBFinish(const userio *io, void *user)
{
    userio_prints    (io, user, "</newBLOBVector>\n");
}

void IUUserIODeleteVA(
    const userio *io, void *user,
    const char *dev, const char *name, const char *fmt, va_list ap
)
{
    userio_prints    (io, user, "<delProperty\n  device='");
    userio_xml_escape(io, user, dev);
    userio_prints    (io, user, "'\n");
    if (name)
    {
        userio_prints    (io, user, " name='");
        userio_xml_escape(io, user, name);
        userio_prints    (io, user, "'\n");
    }
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe

    s_userio_xml_message_vprintf(io, user, fmt, ap);

    userio_prints    (io, user, "/>\n");
}

void IUUserIOGetProperties(
    const userio *io, void *user,
    const char *dev, const char *name
)
{
    userio_printf    (io, user, "<getProperties version='%g'", INDIV); // safe
    // special case for INDI::BaseClient::listenINDI INDI::BaseClientQt::connectServer
    if (dev && dev[0])
    {
        userio_prints    (io, user, " device='");
        userio_xml_escape(io, user, dev);
        userio_prints    (io, user, "'");
    }
    if (name && name[0])
    {
        userio_prints    (io, user, " name='");
        userio_xml_escape(io, user, name);
        userio_prints    (io, user, "'");
    }
    userio_prints    (io, user, "/>\n");
}

// temporary
static const char *s_BLOBHandlingtoString(BLOBHandling bh)
{
    switch (bh)
    {
    case B_NEVER: return "Never";
    case B_ALSO:  return "Also";
    case B_ONLY:  return "Only";
    default:      return "Unknown";
    }
}

void IUUserIOEnableBLOB(
    const userio *io, void *user,
    const char *dev, const char *name, BLOBHandling blobH
)
{
    userio_prints(io, user, "<enableBLOB device='");
    userio_xml_escape(io, user, dev);
    if (name != NULL)
    {
        userio_prints(io, user, "' name='");
        userio_xml_escape(io, user, name);
    }
    userio_prints(io, user, "'>");
    userio_prints(io, user, s_BLOBHandlingtoString(blobH));
    userio_prints(io, user, "</enableBLOB>\n");
}

void IDUserIOMessageVA(
    const userio *io, void *user,
    const char *dev, const char *fmt, va_list ap
)
{
    userio_prints    (io, user, "<message\n");
    if (dev)
    {
        userio_prints    (io, user, " device='");
        userio_xml_escape(io, user, dev);
        userio_prints    (io, user, "'\n");
    }
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe
    s_userio_xml_message_vprintf(io, user, fmt, ap);
    userio_prints    (io, user, "/>\n");
}

void IDUserIOMessage(
    const userio *io, void *user,
    const char *dev, const char *fmt, ...
)
{
    va_list ap;
    va_start(ap, fmt);
    IDUserIOMessageVA(io, user, dev, fmt, ap);
    va_end(ap);
}

void IUUserIOConfigTag(
    const userio *io, void *user, int ctag
)
{
    /* Opening tag */
    if (ctag == 0)
    {
        userio_prints(io, user, "<INDIDriver>\n");
    }
    /* Closing tag */
    else
    {
        userio_prints(io, user, "</INDIDriver>\n");
    }
}

void IUUserIODefTextVA(
    const userio *io, void *user,
    const ITextVectorProperty *tvp, const char *fmt, va_list ap
)
{
    locale_char_t *orig = indi_locale_C_numeric_push();
    userio_prints    (io, user, "<defTextVector\n"
                                "  device='");
    userio_xml_escape(io, user, tvp->device);
    userio_prints    (io, user, "'\n"
                                "  name='");
    userio_xml_escape(io, user, tvp->name);
    userio_prints    (io, user, "'\n"
                                "  label='");
    userio_xml_escape(io, user, tvp->label);
    userio_prints    (io, user, "'\n"
                                "  group='");
    userio_xml_escape(io, user, tvp->group);
    userio_prints    (io, user, "'\n");
    userio_printf    (io, user, "  state='%s'\n", pstateStr(tvp->s)); // safe
    userio_printf    (io, user, "  perm='%s'\n", permStr(tvp->p)); // safe
    userio_printf    (io, user, "  timeout='%g'\n", tvp->timeout); // safe
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe
    s_userio_xml_message_vprintf(io, user, fmt, ap);
    userio_prints    (io, user, ">\n");

    for (int i = 0; i < tvp->ntp; i++)
    {
        IText *tp = &tvp->tp[i];
        userio_prints    (io, user, "  <defText\n"
                                    "    name='");
        userio_xml_escape(io, user, tp->name);
        userio_prints    (io, user, "'\n"
                                    "    label='");
        userio_xml_escape(io, user, tp->label);
        userio_prints    (io, user, "'>\n"
                                    "      ");
        if (tp->text)
            userio_xml_escape(io, user, tp->text);
        userio_prints    (io, user, "\n"
                                    "  </defText>\n");
    }

    userio_prints    (io, user, "</defTextVector>\n");
    indi_locale_C_numeric_pop(orig);
}

void IUUserIODefNumberVA(
    const userio *io, void *user,
    const INumberVectorProperty *n, const char *fmt, va_list ap
)
{
    locale_char_t *orig = indi_locale_C_numeric_push();
    userio_prints    (io, user, "<defNumberVector\n"
                                "  device='");
    userio_xml_escape(io, user, n->device);
    userio_prints    (io, user, "'\n"
                                "  name='");
    userio_xml_escape(io, user, n->name);
    userio_prints    (io, user, "'\n"
                                "  label='");
    userio_xml_escape(io, user, n->label);
    userio_prints    (io, user, "'\n"
                                "  group='");
    userio_xml_escape(io, user, n->group);
    userio_prints    (io, user, "'\n");
    userio_printf    (io, user, "  state='%s'\n", pstateStr(n->s)); // safe
    userio_printf    (io, user, "  perm='%s'\n", permStr(n->p)); // safe
    userio_printf    (io, user, "  timeout='%g'\n", n->timeout); // safe
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe
    s_userio_xml_message_vprintf(io, user, fmt, ap);
    userio_prints    (io, user, ">\n");

    for (int i = 0; i < n->nnp; i++)
    {
        INumber *np = &n->np[i];

        userio_prints    (io, user, "  <defNumber\n"
                                    "    name='");
        userio_xml_escape(io, user, np->name);
        userio_prints    (io, user, "'\n"
                                    "    label='");
        userio_xml_escape(io, user, np->label);
        userio_prints    (io, user, "'\n"
                                    "    format='");
        userio_xml_escape(io, user, np->format);
        userio_prints    (io, user, "'\n");
        userio_printf    (io, user, "    min='%.20g'\n", np->min); // safe
        userio_printf    (io, user, "    max='%.20g'\n", np->max); // safe
        userio_printf    (io, user, "    step='%.20g'>\n", np->step); // safe
        userio_printf    (io, user, "      %.20g\n", np->value); // safe

        userio_prints    (io, user, "  </defNumber>\n");
    }

    userio_prints    (io, user, "</defNumberVector>\n");
    indi_locale_C_numeric_pop(orig);
}

void IUUserIODefSwitchVA(
    const userio *io, void *user,
    const ISwitchVectorProperty *s, const char *fmt, va_list ap
)
{
    locale_char_t *orig = indi_locale_C_numeric_push();
    userio_prints    (io, user, "<defSwitchVector\n"
                                "  device='");
    userio_xml_escape(io, user, s->device);
    userio_prints    (io, user, "'\n"
                                "  name='");
    userio_xml_escape(io, user, s->name);
    userio_prints    (io, user, "'\n"
                                "  label='");
    userio_xml_escape(io, user, s->label);
    userio_prints    (io, user, "'\n"
                                "  group='");
    userio_xml_escape(io, user, s->group);
    userio_prints    (io, user, "'\n");
    userio_printf    (io, user, "  state='%s'\n", pstateStr(s->s)); // safe
    userio_printf    (io, user, "  perm='%s'\n", permStr(s->p)); // safe
    userio_printf    (io, user, "  rule='%s'\n", ruleStr(s->r)); // safe
    userio_printf    (io, user, "  timeout='%g'\n", s->timeout); // safe
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe
    s_userio_xml_message_vprintf(io, user, fmt, ap);
    userio_prints    (io, user, ">\n");

    for (int i = 0; i < s->nsp; i++)
    {
        ISwitch *sp = &s->sp[i];
        userio_prints    (io, user, "  <defSwitch\n"
                                    "    name='");
        userio_xml_escape(io, user, sp->name);
        userio_prints    (io, user, "'\n"
                                    "    label='");
        userio_xml_escape(io, user, sp->label);
        userio_prints    (io, user, "'>\n");
        userio_printf    (io, user, "      %s\n", sstateStr(sp->s)); // safe
        userio_prints    (io, user, "  </defSwitch>\n");
    }

    userio_prints    (io, user, "</defSwitchVector>\n");
    indi_locale_C_numeric_pop(orig);
}

void IUUserIODefLightVA(
    const userio *io, void *user,
    const ILightVectorProperty *lvp, const char *fmt, va_list ap
)
{
    userio_prints    (io, user, "<defLightVector\n"
                                "  device='");
    userio_xml_escape(io, user, lvp->device);
    userio_prints    (io, user, "'\n"
                                "  name='");
    userio_xml_escape(io, user, lvp->name);
    userio_prints    (io, user, "'\n"
                                "  label='");
    userio_xml_escape(io, user, lvp->label);
    userio_prints    (io, user, "'\n"
                                "  group='");
    userio_xml_escape(io, user, lvp->group);
    userio_prints    (io, user, "'\n");
    userio_printf    (io, user, "  state='%s'\n", pstateStr(lvp->s)); // safe
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe
    s_userio_xml_message_vprintf(io, user, fmt, ap);
    userio_prints    (io, user, ">\n");

    for (int i = 0; i < lvp->nlp; i++)
    {
        ILight *lp = &lvp->lp[i];
        userio_prints    (io, user, "  <defLight\n"
                                    "    name='");
        userio_xml_escape(io, user, lp->name);
        userio_prints    (io, user, "'\n"
                                    "    label='");
        userio_xml_escape(io, user, lp->label);
        userio_prints    (io, user, "'>\n");
        userio_printf    (io, user, "      %s\n", pstateStr(lp->s)); // safe
        userio_prints    (io, user, "  </defLight>\n");
    }

    userio_prints    (io, user, "</defLightVector>\n");
}

void IUUserIODefBLOBVA(
    const userio *io, void *user,
    const IBLOBVectorProperty *b, const char *fmt, va_list ap
)
{
    locale_char_t *orig = indi_locale_C_numeric_push();
    userio_prints    (io, user, "<defBLOBVector\n"
                                "  device='");
    userio_xml_escape(io, user, b->device);
    userio_prints    (io, user, "'\n"
                                "  name='");
    userio_xml_escape(io, user, b->name);
    userio_prints    (io, user, "'\n"
                                "  label='");
    userio_xml_escape(io, user, b->label);
    userio_prints    (io, user, "'\n"
                                "  group='");
    userio_xml_escape(io, user, b->group);
    userio_prints    (io, user, "'\n");
    userio_printf    (io, user, "  state='%s'\n", pstateStr(b->s)); // safe
    userio_printf    (io, user, "  perm='%s'\n", permStr(b->p)); // safe
    userio_printf    (io, user, "  timeout='%g'\n", b->timeout); // safe
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe
    s_userio_xml_message_vprintf(io, user, fmt, ap);
    userio_prints    (io, user, ">\n");

    for (int i = 0; i < b->nbp; i++)
    {
        IBLOB *bp = &b->bp[i];
        userio_prints    (io, user, "  <defBLOB\n"
                                    "    name='");
        userio_xml_escape(io, user, bp->name);
        userio_prints    (io, user, "'\n"
                                    "    label='");
        userio_xml_escape(io, user, bp->label);
        userio_prints    (io, user, "'\n"
                                    "  />\n");
    }

    userio_prints    (io, user, "</defBLOBVector>\n");
    indi_locale_C_numeric_pop(orig);
}

void IUUserIOSetTextVA(
    const userio *io, void *user,
    const ITextVectorProperty *tvp, const char *fmt, va_list ap
)
{
    locale_char_t *orig = indi_locale_C_numeric_push();
    userio_prints    (io, user, "<setTextVector\n"
                                "  device='");
    userio_xml_escape(io, user, tvp->device);
    userio_prints    (io, user, "'\n"
                                "  name='");
    userio_xml_escape(io, user, tvp->name);
    userio_prints    (io, user, "'\n");
    userio_printf    (io, user, "  state='%s'\n", pstateStr(tvp->s)); // safe
    userio_printf    (io, user, "  timeout='%g'\n", tvp->timeout); // safe
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe
    s_userio_xml_message_vprintf(io, user, fmt, ap);
    userio_prints    (io, user, ">\n");

    IUUserIOTextContext(io, user, tvp);

    userio_prints    (io, user, "</setTextVector>\n");
    indi_locale_C_numeric_pop(orig);
}

void IUUserIOSetNumberVA(
    const userio *io, void *user,
    const INumberVectorProperty *nvp, const char *fmt, va_list ap
)
{
    locale_char_t *orig = indi_locale_C_numeric_push();
    userio_prints    (io, user, "<setNumberVector\n"
                                "  device='");
    userio_xml_escape(io, user, nvp->device);
    userio_prints    (io, user, "'\n"
                                "  name='");
    userio_xml_escape(io, user, nvp->name);
    userio_prints    (io, user, "'\n");
    userio_printf    (io, user, "  state='%s'\n", pstateStr(nvp->s)); // safe
    userio_printf    (io, user, "  timeout='%g'\n", nvp->timeout); // safe
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe
    s_userio_xml_message_vprintf(io, user, fmt, ap);
    userio_prints    (io, user, ">\n");

    IUUserIONumberContext(io, user, nvp);

    userio_prints    (io, user, "</setNumberVector>\n");
    indi_locale_C_numeric_pop(orig);
}

void IUUserIOSetSwitchVA(
    const userio *io, void *user,
    const ISwitchVectorProperty *svp, const char *fmt, va_list ap
)
{
    locale_char_t *orig = indi_locale_C_numeric_push();
    userio_prints    (io, user, "<setSwitchVector\n"
                                "  device='");
    userio_xml_escape(io, user, svp->device);
    userio_prints    (io, user, "'\n"
                                "  name='");
    userio_xml_escape(io, user, svp->name);
    userio_prints    (io, user, "'\n");
    userio_printf    (io, user, "  state='%s'\n", pstateStr(svp->s)); // safe
    userio_printf    (io, user, "  timeout='%g'\n", svp->timeout); // safe
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe
    s_userio_xml_message_vprintf(io, user, fmt, ap);
    userio_prints    (io, user, ">\n");

    IUUserIOSwitchContextFull(io, user, svp);

    userio_prints    (io, user, "</setSwitchVector>\n");
    indi_locale_C_numeric_pop(orig);
}

void IUUserIOSetLightVA(
    const userio *io, void *user,
    const ILightVectorProperty *lvp, const char *fmt, va_list ap
)
{
    userio_prints    (io, user, "<setLightVector\n"
                                "  device='");
    userio_xml_escape(io, user, lvp->device);
    userio_prints    (io, user, "'\n"
                                "  name='");
    userio_xml_escape(io, user, lvp->name);
    userio_prints    (io, user, "'\n");
    userio_printf    (io, user, "  state='%s'\n", pstateStr(lvp->s)); // safe
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe
    s_userio_xml_message_vprintf(io, user, fmt, ap);
    userio_prints    (io, user, ">\n");

    IUUserIOLightContext(io, user, lvp);

    userio_prints    (io, user, "</setLightVector>\n");
}

void IUUserIOSetBLOBVA(
    const userio *io, void *user,
    const IBLOBVectorProperty *bvp, const char *fmt, va_list ap
)
{
    locale_char_t *orig = indi_locale_C_numeric_push();
    userio_prints    (io, user, "<setBLOBVector\n"
                                "  device='");
    userio_xml_escape(io, user, bvp->device);
    userio_prints    (io, user, "'\n"
                                "  name='");
    userio_xml_escape(io, user, bvp->name);
    userio_prints    (io, user, "'\n");
    userio_printf    (io, user, "  state='%s'\n", pstateStr(bvp->s)); // safe
    userio_printf    (io, user, "  timeout='%g'\n", bvp->timeout); // safe
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe
    s_userio_xml_message_vprintf(io, user, fmt, ap);
    userio_prints    (io, user, ">\n");

    IUUserIOBLOBContext(io, user, bvp);

    userio_prints    (io, user, "</setBLOBVector>\n");
    indi_locale_C_numeric_pop(orig);
}

void IUUserIOUpdateMinMax(
    const userio *io, void *user,
    const INumberVectorProperty *nvp
)
{
    locale_char_t *orig = indi_locale_C_numeric_push();
    userio_prints    (io, user, "<setNumberVector\n"
                                "  device='");
    userio_xml_escape(io, user, nvp->device);
    userio_prints    (io, user, "'\n"
                                "  name='");
    userio_xml_escape(io, user, nvp->name);
    userio_prints    (io, user, "'\n");
    userio_printf    (io, user, "  state='%s'\n", pstateStr(nvp->s)); // safe
    userio_printf    (io, user, "  timeout='%g'\n", nvp->timeout); // safe
    userio_printf    (io, user, "  timestamp='%s'\n", indi_timestamp()); // safe
    userio_prints    (io, user, ">\n");

    for (int i = 0; i < nvp->nnp; i++)
    {
        INumber *np = &nvp->np[i];
        userio_prints    (io, user, "  <oneNumber name='");
        userio_xml_escape(io, user, np->name);
        userio_prints    (io, user, "'\n");
        userio_printf    (io, user, "    min='%g'\n", np->min); // safe
        userio_printf    (io, user, "    max='%g'\n", np->max); // safe
        userio_printf    (io, user, "    step='%g'\n", np->step); // safe
        userio_prints    (io, user, ">\n");
        userio_printf    (io, user, "      %g\n", np->value); // safe
        userio_prints    (io, user, "  </oneNumber>\n");
    }

    userio_prints    (io, user, "</setNumberVector>\n");
    indi_locale_C_numeric_pop(orig);
}

void IUUserIOPingRequest(const userio * io, void *user, const char * pingUid)
{
    userio_prints    (io, user, "<pingRequest uid='");
    userio_xml_escape(io, user, pingUid);
    userio_prints    (io, user, "' />\n");
}

void IUUserIOPingReply(const userio * io, void *user, const char * pingUid)
{
    userio_prints    (io, user, "<pingReply uid='");
    userio_xml_escape(io, user, pingUid);
    userio_prints    (io, user, "' />\n");
}
