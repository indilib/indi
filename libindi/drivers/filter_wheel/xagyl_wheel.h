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

typedef struct
{
    int speed;
    int position;
    int pulseWidth;
    int jitter;
    int threshold;
    int offset[5];
    char product[16];
    char version[16];
    char serial[16];
} SimData;

class XAGYLWheel : public INDI::FilterWheel
{    
public:
    typedef enum { INFO_PRODUCT_NAME, INFO_FIRMWARE_VERSION, INFO_FILTER_POSITION, INFO_SERIAL_NUMBER, INFO_MAX_SPEED, INFO_JITTER, INFO_OFFSET, INFO_THRESHOLD, INFO_MAX_SLOTS, INFO_PULSE_WIDTH} GET_COMMAND;
    typedef enum { SET_SPEED, SET_JITTER, SET_THRESHOLD, SET_PULSE_WITDH, SET_POSITION} SET_COMMAND;

    XAGYLWheel();
    virtual ~XAGYLWheel();

    virtual bool initProperties();
    virtual void ISGetProperties (const char *dev);
    virtual bool updateProperties();

    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);

protected:
    const char *getDefaultName();

    bool Connect();
    bool Disconnect();
    void TimerHit();

    bool SelectFilter(int);
    virtual bool SetFilterNames() { return true; }
    virtual bool GetFilterNames(const char* groupName);

    virtual bool saveConfigItems(FILE *fp);

private:
    bool getCommand(GET_COMMAND cmd, char *result);
    bool setCommand(SET_COMMAND cmd, int value);

    void initOffset();

    bool getStartupData();
    bool getFirmwareInfo();
    bool getSettingInfo();

    bool getFilterPosition();
    bool getMaximumSpeed();
    bool getJitter();
    bool getThreshold();    
    bool getMaxFilterSlots();
    bool getPulseWidth();

    // Calibration offset
    bool getOffset(int filter);
    bool setOffset(int filter, int value);

    // Reset
    bool reset(int value);

    // Device physical port
    ITextVectorProperty PortTP;
    IText PortT[1];

    // Firmware info
    ITextVectorProperty FirmwareInfoTP;
    IText FirmwareInfoT[3];

    // Settings
    INumberVectorProperty SettingsNP;
    INumber SettingsN[4];        

    // Filter Offset
    INumberVectorProperty OffsetNP;
    INumber *OffsetN;

    // Reset
    ISwitchVectorProperty ResetSP;
    ISwitch ResetS[4];

    int PortFD;
    bool sim;
    SimData simData;
    uint8_t firmwareVersion;
};

#endif
