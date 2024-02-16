/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

WandererBox Plus V3

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

class WandererBoxPlusV3 : public INDI::DefaultDevice
{
    public:
        WandererBoxPlusV3();
        virtual ~WandererBoxPlusV3() = default;

        virtual bool initProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool updateProperties() override;



    protected:
        const char *getDefaultName() override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual void TimerHit() override;


    private:

        bool sendCommand(std::string command);
        bool DC3DIFFMODE=false;
        bool DC3CONSTMODE=false;
        bool getData();
        static constexpr const char *ENVIRONMENT_TAB {"Sensors"};
        static constexpr const char *DC3_TAB {"DC3"};
        //Current Calibrate
        ISwitch CalibrateS[1];
        ISwitchVectorProperty CalibrateSP;

        // Firmware version
        ITextVectorProperty FirmwareTP;
        IText FirmwareT[1] {};
        //Temp1
        double temp1read = 0;
        //DHTH
        double DHTHread = 0;
        //DHTT
        void updateENV(double temp1,double DHTH,double DHTT);
        double DHTTread = 0;
        //Total Current
        double Tcurrentread = 0;
        //Power Monitor
        void updatePower(double Tcurrent,double voltage);
        double voltageread = 0;
        //USB
        void updateUSB(int value);
        int USBread = 0;
        //DC2
        void updateDC2(int value);
        int DC2read = 0;
        //DC3
        void updateDC3(int value);
        int DC3read = 0;
        //DC4_6
        void updateDC4_6(int value);
        int DC4_6read = 0;
        //DC2set
        void updateDC2SET(double value);
        int DC2SETread = 0;
        
        bool setDewPWM(uint8_t id, uint8_t value);

        //DC Control//////////////////////////////////////////////////////////////////////////////////
        ISwitchVectorProperty DC2ControlSP;
        ISwitch DC2ControlS[2];
        ISwitchVectorProperty DC4_6ControlSP;
        ISwitch DC4_6ControlS[2];
        //USB Control//////////////////////////////////////////////////////////////////////////////////
        ISwitchVectorProperty USBControlSP;
        ISwitch USBControlS[2];
        //DC3 Control////////////////////////////////////////////////////////////////
        INumber DC3ControlN[1];
        INumberVectorProperty DC3ControlNP;
        enum
        {
            DC3,
        };
        ISwitchVectorProperty DC3dewSP;
        ISwitch DC3diffS[3];
        enum
        {
            DC3_Manual,
            DC3_DPD_Mode,
            DC3_CT_Mode,
        };

        INumber DC3diffSETN[1];
        INumberVectorProperty DC3diffSETNP;
        enum
        {
            DC3DIFFSET,
        };
        INumber DC3constSETN[1];
        INumberVectorProperty DC3constSETNP;
        enum
        {
            DC3CONSTSET,
        };
    
        //DC2 Voltage Control////////////////////////////////////////////////////////////////
        INumber setDC2voltageN[1];
        INumberVectorProperty setDC2voltageNP;
        enum
        {
            setDC2voltage,
        };

        // Power Monitor
        INumber PowerMonitorN[2];
        INumberVectorProperty PowerMonitorNP;
        enum
        {
            VOLTAGE,
            TOTAL_CURRENT,
        };

        // ENV Monitor
        INumber ENVMonitorN[4];
        INumberVectorProperty ENVMonitorNP;
        enum
        {
            Probe1_Temp,
            ENV_Humidity,
            ENV_Temp,
            DEW_Point,
        };



        int PortFD{ -1 };

        Connection::Serial *serialConnection{ nullptr };
};
