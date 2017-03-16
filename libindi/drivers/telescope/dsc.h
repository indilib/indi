/*******************************************************************************
 Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 Generic Digital Setting Circles Driver

 It just gets the encoder positoin and outputs current coordinates.
 Calibratoin and syncing not supported yet.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef DSC_H
#define DSC_H

#include "indibase/inditelescope.h"

class DSC : public INDI::Telescope
{
public:
    DSC();
    virtual ~DSC();

    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);

protected:
    virtual const char *getDefaultName();
    virtual bool Handshake();

    virtual bool initProperties();
    virtual bool updateProperties();
    virtual bool saveConfigItems(FILE *fp);
    virtual bool ReadScopeStatus();

private:
    INumber EncoderN[2];
    INumberVectorProperty EncoderNP;
    enum { RA_ENCODER, DE_ENCODER };

    INumber OffsetN[4];
    INumberVectorProperty OffsetNP;
    enum { OFFSET_RA_SCALE, OFFSET_RA_OFFSET, OFFSET_DE_SCALE, OFFSET_DE_OFFSET };
};

#endif
