/*******************************************************************************
  Copyright(c) 2024 Frank Wang. All rights reserved.

  WandererBox Pro V3

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
#include "indiweatherinterface.h"
#include <vector>
#include <stdint.h>

namespace Connection
{
class Serial;
}

class WandererBoxProV3 : public INDI::DefaultDevice, public INDI::WeatherInterface
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
    virtual IPState updateWeather() override;


private:

    int firmware=0;
    bool sendCommand(std::string command);
    bool DC5DIFFMODE=false;
    bool DC5CONSTMODE=false;
    bool DC6DIFFMODE=false;
    bool DC6CONSTMODE=false;
    bool DC7DIFFMODE=false;
    bool DC7CONSTMODE=false;
    bool getData();
    static constexpr const char *ENVIRONMENT_TAB {"Environment"};
    static constexpr const char *SENSORS_TAB {"Sensors"};
    static constexpr const char *DC5_TAB {"DC5"};
    static constexpr const char *DC6_TAB {"DC6"};
    static constexpr const char *DC7_TAB {"DC7"};
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
    INDI::PropertySwitch dc3_4ControlSP{2};
    INDI::PropertySwitch dc8_9ControlSP{2};
    INDI::PropertySwitch dc10_11ControlSP{2};
    //USB Control//////////////////////////////////////////////////////////////////////////////////
    INDI::PropertySwitch usb31ControlSP{2};
    INDI::PropertySwitch usb32ControlSP{2};
    INDI::PropertySwitch usb33ControlSP{2};
    INDI::PropertySwitch usb21ControlSP{2};
    INDI::PropertySwitch usb22ControlSP{2};
    //DC5 Control////////////////////////////////////////////////////////////////
    INDI::PropertyNumber dc5ControlNP{1};
    enum
    {
        DC5,
    };
    //DC6 Control////////////////////////////////////////////////////////////////
    INDI::PropertyNumber dc6ControlNP{1};
    enum
    {
        DC6,
    };
    //DC7 Control////////////////////////////////////////////////////////////////
    INDI::PropertyNumber dc7ControlNP{1};
    enum
    {
        DC7,
    };
    //DC5 DIFF////////////////////////////////////////////////////////////////
    INDI::PropertySwitch dc5diffSP{3};
    enum
    {
        DC5_Manual,
        DC5_DPD_Mode,
        DC5_CT_Mode,
    };

    INDI::PropertyNumber dc5diffSETNP{1};
    enum
    {
        DC5DIFFSET,
    };
    INDI::PropertyNumber dc5constSETNP{1};
    enum
    {
        DC5CONSTSET,
    };

    //DC6 DIFF////////////////////////////////////////////////////////////////
    INDI::PropertySwitch dc6diffSP{3};
    enum
    {
        DC6_Manual,
        DC6_DPD_Mode,
        DC6_CT_Mode,
    };

    INDI::PropertyNumber dc6diffSETNP{1};
    enum
    {
        DC6DIFFSET,
    };
    INDI::PropertyNumber dc6constSETNP{1};
    enum
    {
        DC6CONSTSET,
    };

    //DC7 DIFF////////////////////////////////////////////////////////////////
    INDI::PropertySwitch dc7diffSP{3};
    enum
    {
        DC7_Manual,
        DC7_DPD_Mode,
        DC7_CT_Mode,
    };

    INDI::PropertyNumber dc7diffSETNP{1};
    enum
    {
        DC7DIFFSET,
    };
    INDI::PropertyNumber dc7constSETNP{1};
    enum
    {
        DC7CONSTSET,
    };

    //DC34 Voltage Control////////////////////////////////////////////////////////////////
    INDI::PropertyNumber setDC34voltageNP{1};
    enum
    {
        setDC34voltage,
    };

    // Power Monitor

    INDI::PropertyNumber PowerMonitorNP{4};
    enum
    {
        VOLTAGE,
        TOTAL_CURRENT,
        V19_CURRENT,
        AR_CURRENT,
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
