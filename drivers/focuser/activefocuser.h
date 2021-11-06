/*
    ActiveFocuser driver for Takahashi CCA-250 and Mewlon-250/300CRS
    Driver written by Alvin FREY <https://afrey.fr> for Optique Unterlinden and Takahashi Europe
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
#include <cmath>
#include <memory>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include "hidapi.h"
#include "indifocuser.h"

class ActiveFocuser : public INDI::Focuser {
public:

    ActiveFocuser();

    ~ActiveFocuser();

    const char *getDefaultName() override;

    void ISGetProperties(const char *dev) override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    void TimerHit() override;

protected:

    bool initProperties() override;

    bool updateProperties() override;

    bool Connect() override;

    bool Disconnect() override;

    bool AbortFocuser() override;

    IPState MoveAbsFocuser(uint32_t targetTicks) override;

    IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;

private:

    hid_device *hid_handle;

    double internalTicks{0};
    // double initTicks{0}; // #PS: unused

    // Hardware version display
    ITextVectorProperty HardwareVersionNP;
    IText HardwareVersionN[1];

    // Software version display
    ITextVectorProperty SoftwareVersionNP;
    IText SoftwareVersionN[1];

    // Air Temperature in celsius degrees
    INumberVectorProperty AirTemperatureNP;
    INumber AirTemperatureN[1];

    // Mirror Temperature in celsius degrees
    INumberVectorProperty MirrorTemperatureNP;
    INumber MirrorTemperatureN[1];

    // Tube Temperature in celsius degrees
    INumberVectorProperty TubeTemperatureNP;
    INumber TubeTemperatureN[1];

    // Fan State switch
    ISwitch FanS[2];
    ISwitchVectorProperty FanSP;

};