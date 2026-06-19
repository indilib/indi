/*******************************************************************************
  Copyright(c) 2024 Frank Wang/Jérémie Klein. All rights reserved.

  WandererCover V4-EC

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#pragma once

#include "defaultdevice.h"
#include "indidustcapinterface.h"
#include <mutex>

namespace Connection
{
class Serial;
}

class WandererEclipse : public INDI::DefaultDevice, public INDI::DustCapInterface
{
public:
    WandererEclipse();
    virtual ~WandererEclipse() = default;

    virtual bool initProperties() override;
   
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool updateProperties() override;


protected:

    // From Dust Cap
    virtual IPState ParkCap() override;
    virtual IPState UnParkCap() override;

 

    const char *getDefaultName() override;
    virtual void TimerHit() override;


private:

    int firmware=0;
    bool toggleCover(bool open);
    bool sendCommand(std::string command);
    //Current Calibrate
    bool getData();
    bool parseDeviceData(const char *data);
    
    double voltageread=0;
    
    int torqueLevel = -1;
    
    void updateData(double torqueread,double voltageread);

    INDI::PropertyNumber DataNP{4};
    enum
    {
        torque_read,
        voltage_read,
    };

 
    // Torque property (0: low, 1: medium, 2: high)
    INDI::PropertySwitch TorqueSP{3};
    enum
    {
        TORQUE_LOW,
        TORQUE_MEDIUM,
        TORQUE_HIGH,
    };
    
    // Firmware information
    INDI::PropertyText FirmwareTP{1};
    enum
    {
        FIRMWARE_VERSION,
    };

    int PortFD{ -1 };

    Connection::Serial *serialConnection{ nullptr };

    // Mutex for thread safety
    std::timed_mutex serialPortMutex;
};
