#ifndef LX200STARGOFOCUSER_H
#define LX200STARGOFOCUSER_H

/*
    Avalon Star GO Focuser
    Copyright (C) 2018 Christopher Contaxis (chrconta@gmail.com) and
    Wolfgang Reissenberger (sterne-jaeger@t-online.de)

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

#include "indifocuserinterface.h"
#include "defaultdevice.h"


class LX200StarGoFocuser : public INDI::FocuserInterface
{
public:
    LX200StarGoFocuser(INDI::DefaultDevice* defaultDevice, const char* name);
    virtual ~LX200StarGoFocuser() = default;

    void initProperties(const char *groupName);
    bool updateProperties();

    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

    const char *getDeviceName();
    const char *getDefaultName();

    bool isConnected();

protected:

    INumberVectorProperty FocusSyncPosNP;
    INumber FocusSyncPosN[1];

    int targetFocuserPosition;
    bool startMovingFocuserInward;
    bool startMovingFocuserOutward;
    uint32_t moveFocuserDurationRemaining;


private:
    INDI::DefaultDevice* baseDevice;
    const char* deviceName;
    bool isInit = false;


};

#endif // LX200STARGOFOCUSER_H
