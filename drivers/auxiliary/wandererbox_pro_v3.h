/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  Simple GPS Simulator

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

class WandererBoxProV3 : public INDI::DefaultDevice
{
    public:
        WandererBoxProV3();
        virtual ~WandererBoxProV3() = default;

        virtual bool initProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool updateProperties() override;



    protected:
        const char *getDefaultName() override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual void TimerHit() override;


    private:

        char version[64] = {0};
        bool sendCommand(std::string command);
        bool DC5DIFFMODE=false;
        bool DC5CONSTMODE=false;
        bool DC6DIFFMODE=false;
        bool DC6CONSTMODE=false;
        bool DC7DIFFMODE=false;
        bool DC7CONSTMODE=false;
        bool getData();
        static constexpr const char *ENVIRONMENT_TAB {"Sensors"};
        static constexpr const char *DC5_TAB {"DC5"};
        static constexpr const char *DC6_TAB {"DC6"};
        static constexpr const char *DC7_TAB {"DC7"};
        //Current Calibrate
        ISwitch CalibrateS[1];
        ISwitchVectorProperty CalibrateSP;

        // Firmware version
        ITextVectorProperty FirmwareTP;
        IText FirmwareT[1] {};
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
        //Total Current
        double Tcurrentread = 0;
        //19V Current
        double V19currentread = 0;
        //Adjustable Regulator Current
        double ARcurrentread = 0;
        //Power Monitor
        void updatePower(double Tcurrent,double V19_current,double AR_current,double voltage);
        double voltageread = 0;
        //USB31
        void updateUSB31(int value);
        int USB31read = 0;
        //USB32
        void updateUSB32(int value);
        int USB32read = 0;
        //USB33
        void updateUSB33(int value);
        int USB33read = 0;
        //USB21
        void updateUSB21(int value);
        int USB21read = 0;
        //USB22
        void updateUSB22(int value);
        int USB22read = 0;
        //DC34
        void updateDC34(int value);
        int DC34read = 0;
        //DC5
        void updateDC5(int value);
        int DC5read = 0;
        //DC6
        void updateDC6(int value);
        int DC6read = 0;
        //DC7
        void updateDC7(int value);
        int DC7read = 0;
        //DC8_9
        void updateDC8_9(int value);
        int DC8_9read = 0;
        //DC10_11
        void updateDC10_11(int value);
        int DC10_11read = 0;
        //DC34set
        void updateDC34SET(double value);
        int DC34SETread = 0;
        
        bool setDewPWM(uint8_t id, uint8_t value);

        //DC Control//////////////////////////////////////////////////////////////////////////////////
        ISwitchVectorProperty dc3_4ControlSP;
        ISwitch dc3_4ControlS[2];
        ISwitchVectorProperty dc8_9ControlSP;
        ISwitch dc8_9ControlS[2];
        ISwitchVectorProperty dc10_11ControlSP;
        ISwitch dc10_11ControlS[2];
        //USB Control//////////////////////////////////////////////////////////////////////////////////
        ISwitchVectorProperty usb31ControlSP;
        ISwitch usb31ControlS[2];
        ISwitchVectorProperty usb32ControlSP;
        ISwitch usb32ControlS[2];
        ISwitchVectorProperty usb33ControlSP;
        ISwitch usb33ControlS[2];
        ISwitchVectorProperty usb21ControlSP;
        ISwitch usb21ControlS[2];
        ISwitchVectorProperty usb22ControlSP;
        ISwitch usb22ControlS[2];
        //DC5 Control////////////////////////////////////////////////////////////////
        INumber dc5ControlN[1];
        INumberVectorProperty dc5ControlNP;
        enum
        {
            DC5,
        };
        //DC6 Control////////////////////////////////////////////////////////////////
        INumber dc6ControlN[1];
        INumberVectorProperty dc6ControlNP;
        enum
        {
            DC6,
        };
        //DC7 Control////////////////////////////////////////////////////////////////
        INumber dc7ControlN[1];
        INumberVectorProperty dc7ControlNP;
        enum
        {
            DC7,
        };
        //DC5 DIFF////////////////////////////////////////////////////////////////
        ISwitchVectorProperty dc5dewSP;
        ISwitch dc5diffS[3];
        enum
        {
            DC5_Manual,
            DC5_DPD_Mode,
            DC5_CT_Mode,
        };

        INumber dc5diffSETN[1];
        INumberVectorProperty dc5diffSETNP;
        enum
        {
            DC5DIFFSET,
        };
        INumber dc5constSETN[1];
        INumberVectorProperty dc5constSETNP;
        enum
        {
            DC5CONSTSET,
        };

        //DC6 DIFF////////////////////////////////////////////////////////////////
        ISwitchVectorProperty dc6dewSP;
        ISwitch dc6diffS[3];
        enum
        {
            DC6_Manual,
            DC6_DPD_Mode,
            DC6_CT_Mode,
        };

        INumber dc6diffSETN[1];
        INumberVectorProperty dc6diffSETNP;
        enum
        {
            DC6DIFFSET,
        };
        INumber dc6constSETN[1];
        INumberVectorProperty dc6constSETNP;
        enum
        {
            DC6CONSTSET,
        };

        //DC7 DIFF////////////////////////////////////////////////////////////////
        ISwitchVectorProperty dc7dewSP;
        ISwitch dc7diffS[3];
        enum
        {
            DC7_Manual,
            DC7_DPD_Mode,
            DC7_CT_Mode,
        };

        INumber dc7diffSETN[1];
        INumberVectorProperty dc7diffSETNP;
        enum
        {
            DC7DIFFSET,
        };
        INumber dc7constSETN[1];
        INumberVectorProperty dc7constSETNP;
        enum
        {
            DC7CONSTSET,
        };

        //DC34 Voltage Control////////////////////////////////////////////////////////////////
        INumber setDC34voltageN[1];
        INumberVectorProperty setDC34voltageNP;
        enum
        {
            setDC34voltage,
        };

        // Power Monitor
        INumber PowerMonitorN[4];
        INumberVectorProperty PowerMonitorNP;
        enum
        {
            VOLTAGE,
            TOTAL_CURRENT,
            V19_CURRENT,
            AR_CURRENT,
        };

        // ENV Monitor
        INumber ENVMonitorN[6];
        INumberVectorProperty ENVMonitorNP;
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
