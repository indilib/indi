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
#include <libnova.h>

class DSC : public INDI::Telescope
{
public:
    DSC();
    virtual ~DSC();

    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

protected:
    virtual const char *getDefaultName();
    virtual bool Handshake();

    virtual bool initProperties();
    virtual bool updateProperties();
    virtual bool saveConfigItems(FILE *fp);
    virtual bool ReadScopeStatus();

    virtual bool Sync(double ra, double dec) override;
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;

    virtual void simulationTriggered(bool enable) override;

private:
    INumber EncoderN[4];
    INumberVectorProperty EncoderNP;
    enum { AXIS1_ENCODER, AXIS2_ENCODER, AXIS1_RAW_ENCODER, AXIS2_RAW_ENCODER };

    INumber AxisSettingsN[4];
    INumberVectorProperty AxisSettingsNP;
    enum { AXIS1_TICKS, AXIS1_DEGREE_OFFSET, AXIS2_TICKS, AXIS2_DEGREE_OFFSET };

    ISwitch ReverseS[2];
    ISwitchVectorProperty ReverseSP;

    ISwitch MountTypeS[2];
    ISwitchVectorProperty MountTypeSP;
    enum { MOUNT_EQUATORIAL, MOUNT_ALTAZ };

    INumber EncoderOffsetN[4];
    INumberVectorProperty EncoderOffsetNP;
    enum { OFFSET_AXIS1_SCALE, OFFSET_AXIS1_OFFSET, OFFSET_AXIS2_SCALE, OFFSET_AXIS2_OFFSET };

    // Simulation Only
    INumber SimEncoderN[2];
    INumberVectorProperty SimEncoderNP;

    ln_lnlat_posn observer;
};

#endif
