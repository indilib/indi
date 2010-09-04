#if 0
    INDI
    Copyright (C) 2003-2006 Elwood C. Downey

                        Modified by Jasem Mutlaq (2003-2006)

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

#ifndef INDI_DRIVERMAIN_H
#define INDI_DRIVERMAIN_H

#include "indiapi.h"
#include "lilxml.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int crackDN (XMLEle *root, char **dev, char **name, char msg[]);
extern int isPropDefined(const char *property_name);
extern int crackIPState (const char *str, IPState *ip);
extern int crackISState (const char *str, ISState *ip);
extern int crackIPerm (const char *str, IPerm *ip);
extern int crackISRule (const char *str, ISRule *ip);
extern void xmlv1(void);
extern const char *pstateStr(IPState s);
extern const char *sstateStr(ISState s);
extern const char *ruleStr(ISRule r);
extern const char *permStr(IPerm p);

#ifdef __cplusplus
}
#endif

#endif
