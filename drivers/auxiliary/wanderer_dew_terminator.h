/*******************************************************************************
  Copyright(c) 2024 Frank Wang. All rights reserved.

  Wanderer Dew Terminator

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
#include <vector>
#include <stdint.h>

namespace Connection
{
class Serial;
}

class WandererDewTerminator : public INDI::DefaultDevice
{
public:
    WandererDewTerminator();
    virtual ~WandererDewTerminator() = default;

    virtual bool initProperties() override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool updateProperties() override;



protected:
    const char *getDefaultName() override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual void TimerHit() override;


private:

    int firmware=0;
    bool sendCommand(std::string command);
    bool DC1DIFFMODE=false;
    bool DC1CONSTMODE=false;
    bool DC2DIFFMODE=false;
    bool DC2CONSTMODE=false;
    bool DC3DIFFMODE=false;
    bool DC3CONSTMODE=false;
    bool getData();
    static constexpr const char *ENVIRONMENT_TAB {"Sensors"};
    static constexpr const char *DC1_TAB {"DC1"};
    static constexpr const char *DC2_TAB {"DC2"};
    static constexpr const char *DC3_TAB {"DC3"};
    //Current Calibrate
    INDI::PropertySwitch CalibrateSP{1};


    //Temp1
    double temp1read = 0;
    //Temp2
    double temp2read = 0;
    //Temp3
    double temp3read = 0;
    //DHTH
    double DHTHread = 0;
    //DHTT
    void updateENV(double temp1,double temp2,double temp3,double DHTH,double DHTT);
    double DHTTread = 0;
    
    //Power Monitor
    void updatePower(double voltage);
    double voltageread = 0;
    
    //DC1
    void updateDC1(int value);
    int DC1read = 0;
    //DC2
    void updateDC2(int value);
    int DC2read = 0;
    //DC3
    void updateDC3(int value);
    int DC3read = 0;
    

    bool setDewPWM(uint8_t id, uint8_t value);

    
    //DC1 Control////////////////////////////////////////////////////////////////
    INDI::PropertyNumber DC1ControlNP{1};
    enum
    {
        DC1,
    };
    //DC2 Control////////////////////////////////////////////////////////////////
    INDI::PropertyNumber DC2ControlNP{1};
    enum
    {
        DC2,
    };
    //DC3 Control////////////////////////////////////////////////////////////////
    INDI::PropertyNumber DC3ControlNP{1};
    enum
    {
        DC3,
    };
    //DC1 DIFF////////////////////////////////////////////////////////////////
    INDI::PropertySwitch DC1diffSP{3};
    enum
    {
        DC1_Manual,
        DC1_DPD_Mode,
        DC1_CT_Mode,
    };

    INDI::PropertyNumber DC1diffSETNP{1};
    enum
    {
        DC1DIFFSET,
    };
    INDI::PropertyNumber DC1constSETNP{1};
    enum
    {
        DC1CONSTSET,
    };

    //DC2 DIFF////////////////////////////////////////////////////////////////
    INDI::PropertySwitch DC2diffSP{3};
    enum
    {
        DC2_Manual,
        DC2_DPD_Mode,
        DC2_CT_Mode,
    };

    INDI::PropertyNumber DC2diffSETNP{1};
    enum
    {
        DC2DIFFSET,
    };
    INDI::PropertyNumber DC2constSETNP{1};
    enum
    {
        DC2CONSTSET,
    };

    //DC3 DIFF////////////////////////////////////////////////////////////////
    INDI::PropertySwitch DC3diffSP{3};
    enum
    {
        DC3_Manual,
        DC3_DPD_Mode,
        DC3_CT_Mode,
    };

    INDI::PropertyNumber DC3diffSETNP{1};
    enum
    {
        DC3DIFFSET,
    };
    INDI::PropertyNumber DC3constSETNP{1};
    enum
    {
        DC3CONSTSET,
    };

    
    // Power Monitor

    INDI::PropertyNumber PowerMonitorNP{4};
    enum
    {
        VOLTAGE,
        
    };

    // ENV Monitor

    INDI::PropertyNumber ENVMonitorNP{6};
    enum
    {
        Probe1_Temp,
        Probe2_Temp,
        Probe3_Temp,
        ENV_Humidity,
        ENV_Temp,
        DEW_Point,
    };



    int PortFD{ -1 };

    Connection::Serial *serialConnection{ nullptr };
};
