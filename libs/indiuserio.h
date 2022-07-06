/*
    Copyright (C) 2021 by Pawel Soja <kernel32.pl@gmail.com>
                  2022 by Ludovic Pollet

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
#pragma once

#include "userio.h"
#include "indidevapi.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _ITextVectorProperty;
struct _INumberVectorProperty;
struct _ISwitchVectorProperty;
struct _IBLOBVectorProperty;
struct _ILightVectorProperty;

struct _IBLOB;
struct _ISwitch;

void IUUserIOTextContext(const userio *io, void *user, const struct _ITextVectorProperty *tvp);
void IUUserIONumberContext(const userio *io, void *user, const struct _INumberVectorProperty *nvp);
void IUUserIOSwitchContextOne(const userio *io, void *user, const struct _ISwitch *sp);
void IUUserIOSwitchContextFull(const userio *io, void *user, const ISwitchVectorProperty *svp);
void IUUserIOSwitchContext(const userio *io, void *user, const struct _ISwitchVectorProperty *svp);
void IUUserIOBLOBContext(const userio *io, void *user, const struct _IBLOBVectorProperty *bvp);
void IUUserIOLightContext(const userio *io, void *user, const struct _ILightVectorProperty *lvp);


void IUUserIONewText(const userio *io, void *user, const struct _ITextVectorProperty *tvp);
void IUUserIONewNumber(const userio *io, void *user, const struct _INumberVectorProperty *nvp);
void IUUserIONewSwitchFull(const userio *io, void *user, const ISwitchVectorProperty *svp);
void IUUserIONewSwitch(const userio *io, void *user, const struct _ISwitchVectorProperty *svp);
void IUUserIONewBLOB(const userio *io, void *user, const struct _IBLOBVectorProperty *bvp);

void IUUserIONewBLOBStart(const userio *io, void *user, const char *dev, const char *name, const char *timestamp);

void IUUserIOBLOBContextOne(
    const userio *io, void *user,
    const char *name, unsigned int size, unsigned int bloblen, const void *blob, const char *format
);
void IUUserIONewBLOBFinish(const userio *io, void *user);

void IUUserIOEnableBLOB(
    const userio *io, void *user,
    const char *dev, const char *name, BLOBHandling blobH
);

// Define
void IUUserIODefTextVA(const userio *io, void *user, const struct _ITextVectorProperty *tvp, const char *fmt, va_list ap);
void IUUserIODefNumberVA(const userio *io, void *user, const struct _INumberVectorProperty *n, const char *fmt, va_list ap);
void IUUserIODefSwitchVA(const userio *io, void *user, const struct _ISwitchVectorProperty *s, const char *fmt, va_list ap);
void IUUserIODefLightVA(const userio *io, void *user, const struct _ILightVectorProperty *lvp, const char *fmt, va_list ap);
void IUUserIODefBLOBVA(const userio *io, void *user, const struct _IBLOBVectorProperty *b, const char *fmt, va_list ap);

// Setup
void IUUserIOSetTextVA(const userio *io, void *user, const struct _ITextVectorProperty *tvp, const char *fmt, va_list ap);
void IUUserIOSetNumberVA(const userio *io, void *user, const struct _INumberVectorProperty *nvp, const char *fmt,
                         va_list ap);
void IUUserIOSetSwitchVA(const userio *io, void *user, const struct _ISwitchVectorProperty *svp, const char *fmt,
                         va_list ap);
void IUUserIOSetLightVA(const userio *io, void *user, const struct _ILightVectorProperty *lvp, const char *fmt, va_list ap);
void IUUserIOSetBLOBVA(const userio *io, void *user, const struct _IBLOBVectorProperty *bvp, const char *fmt, va_list ap);

void IUUserIOUpdateMinMax(const userio *io, void *user, const struct _INumberVectorProperty *nvp);

void IUUserIODeleteVA(const userio *io, void *user, const char *dev, const char *name, const char *fmt, va_list ap);

void IUUserIOGetProperties(const userio *io, void *user, const char *dev, const char *name);

void IDUserIOMessage(const userio *io, void *user, const char *dev, const char *fmt, ...);
void IDUserIOMessageVA(const userio *io, void *user, const char *dev, const char *fmt, va_list ap);

void IUUserIOConfigTag(const userio *io, void *user, int ctag);

void IUUserIOPingRequest(const userio * io, void *user, const char * pingUid);
void IUUserIOPingReply(const userio * io, void *user, const char * pingUid);

#ifdef __cplusplus
}
#endif
