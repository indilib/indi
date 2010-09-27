#if 0
    INDI
    Copyright (C) 2003-2006 Elwood C. Downey

                        Updated by Jasem Mutlaq (2003-2010)

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

#endif

#ifndef INDIDRIVER_H
#define INDIDRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

/* insure RO properties are never modified. RO Sanity Check */
typedef struct
{
 char propName[MAXINDINAME];
 IPerm perm;
} ROSC;

extern ROSC *roCheck;
extern int nroCheck;			/* # of elements in roCheck */

extern int verbose;			/* chatty */
extern char *me;				/* a.out name */
extern LilXML *clixml;			/* XML parser context */

extern int dispatch (XMLEle *root, char msg[]);
extern void clientMsgCB(int fd, void *arg);
extern int readConfig(const char *filename, const char *dev, char errmsg[]);
extern void IUSaveDefaultConfig(const char *source_config, const char *dest_config, const char *dev);
extern FILE * IUGetConfigFP(const char *filename, const char *dev, char errmsg[]);
extern void IUSaveConfigTag(FILE *fp, int ctag);
extern void IUSaveConfigNumber (FILE *fp, const INumberVectorProperty *nvp);
extern void IUSaveConfigText (FILE *fp, const ITextVectorProperty *tvp);
extern void IUSaveConfigSwitch (FILE *fp, const ISwitchVectorProperty *svp);
extern void IUSaveConfigBLOB (FILE *fp, const IBLOBVectorProperty *bvp);

#ifdef __cplusplus
}
#endif

#endif
