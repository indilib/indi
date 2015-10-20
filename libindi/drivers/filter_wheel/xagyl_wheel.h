/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef XAGYLWHEEL_H
#define XAGYLWHEEL_H

#include "indibase/indifilterwheel.h"

class XAGYLWheel : public INDI::FilterWheel
{    
public:
    FilterSim();
    virtual ~FilterSim();

protected:
    const char *getDefaultName();

    bool Connect();
    bool Disconnect();
    void TimerHit();

    virtual bool initProperties();
    virtual void ISGetProperties (const char *dev);
    virtual bool updateProperties();

    bool SelectFilter(int);
    virtual bool SetFilterNames() { return true; }
    virtual bool GetFilterNames(const char* groupName);

    virtual bool saveConfigItems(FILE *fp);

private:
    bool getMaxFilterSlots();
    bool getBasicData();

    // Device physical port
    ITextVectorProperty PortTP;
    IText PortT[1];

    // Firmware info
    ITextVectorProperty FirmwareInfoTP;
    IText FirmwareInfoT[3];

    // Settings
    INumberVectorProperty SettingsNP;
    INumber SettingsN[4];

    // Max Speed
    ISwitchVectorProperty MaxSpeedSP;
    ISwitch MaxSpeedS[2];

    // Jitter
    ISwitchVectorProperty JitterSP;
    ISwitch JitterS[2];

    // Threshold
    ISwitchVectorProperty ThresholdSP;
    ISwitch ThresholdS[2];

    // Pulse Width
    ISwitchVectorProperty PulseWidthSP;
    ISwitch PulseWidthS[2];


    int PortFD;
    bool sim;
};

#endif
