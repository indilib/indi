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

#include "wandererbox_pro_v3.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"
#include <cerrno>
#include <cstring>
#include <string>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <cmath>

// We declare an auto pointer to WandererBoxProV3.
static std::unique_ptr<WandererBoxProV3> wandererboxprov3(new WandererBoxProV3());



WandererBoxProV3::WandererBoxProV3() : INDI::DefaultDevice(), INDI::WeatherInterface(this)
{
    setVersion(1, 1);
}

bool WandererBoxProV3::initProperties()
{

    INDI::DefaultDevice::initProperties();
    setDriverInterface(AUX_INTERFACE | WEATHER_INTERFACE);

    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

    addAuxControls();


    // Calibrate
    CalibrateSP[0].fill("Calibrate", "Calibrate Current", ISS_OFF);
    CalibrateSP.fill(getDeviceName(), "Calibrate_DEVICE", "Calibrate Current", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,60, IPS_IDLE);

    // Power Monitor
    PowerMonitorNP[VOLTAGE].fill("VOLTAGE", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    PowerMonitorNP[TOTAL_CURRENT].fill("TOTAL_CURRENT", "Total Current (A)", "%4.2f", 0, 999, 100, 0);
    PowerMonitorNP[V19_CURRENT].fill("V19_CURRENT", "DC2 Current (A))", "%4.2f", 0, 999, 100, 0);
    PowerMonitorNP[AR_CURRENT].fill("AR_CURRENT", "DC3-4 Current (A)", "%4.2f", 0, 999, 100, 0);
    PowerMonitorNP.fill(getDeviceName(), "POWER_Monitor", "Power Monitor",  MAIN_CONTROL_TAB, IP_RO,60, IPS_IDLE);


    // USB31 Control
    usb31ControlSP[INDI_ENABLED].fill("INDI_ENABLED", "On", ISS_OFF);
    usb31ControlSP[INDI_DISABLED].fill("INDI_DISABLED", "Off", ISS_OFF);
    usb31ControlSP.fill(getDeviceName(), "USB3.0_1", "USB3.0_1", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);


    // USB32 Control
    usb32ControlSP[INDI_ENABLED].fill("INDI_ENABLED", "On", ISS_OFF);
    usb32ControlSP[INDI_DISABLED].fill("INDI_DISABLED", "Off", ISS_OFF);
    usb32ControlSP.fill(getDeviceName(), "USB3.0_2", "USB3.0_2", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);


    // USB33 Control
    usb33ControlSP[INDI_ENABLED].fill("INDI_ENABLED", "On", ISS_OFF);
    usb33ControlSP[INDI_DISABLED].fill("INDI_DISABLED", "Off", ISS_OFF);
    usb33ControlSP.fill(getDeviceName(), "USB3.0_3", "USB3.0_3", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // USB21 Control
    usb21ControlSP[INDI_ENABLED].fill("INDI_ENABLED", "On", ISS_OFF);
    usb21ControlSP[INDI_DISABLED].fill("INDI_DISABLED", "Off", ISS_OFF);
    usb21ControlSP.fill(getDeviceName(), "USB2.0_1-3", "USB2.0_1-3", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // USB22 Control
    usb22ControlSP[INDI_ENABLED].fill("INDI_ENABLED", "On", ISS_OFF);
    usb22ControlSP[INDI_DISABLED].fill("INDI_DISABLED", "Off", ISS_ON);
    usb22ControlSP.fill(getDeviceName(), "USB2.0_4-6", "USB2.0_4-6", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // DC567


    dc5ControlNP[DC5].fill( "DC5", "Dew Heater (PWM)", "%.2f", 0, 255, 5, 0);
    dc5ControlNP.fill(getDeviceName(), "PWM", "DC5", DC5_TAB, IP_RW, 60, IPS_IDLE);

    dc6ControlNP[DC6].fill( "DC6", "Dew Heater (PWM)", "%.2f", 0, 255, 5, 0);
    dc6ControlNP.fill( getDeviceName(), "DC6", "DC6", DC6_TAB, IP_RW, 60, IPS_IDLE);

    dc7ControlNP[DC7].fill( "DC7", "Dew Heater (PWM)", "%.2f", 0, 255, 5, 0);
    dc7ControlNP.fill(getDeviceName(), "DC7", "DC7", DC7_TAB, IP_RW, 60, IPS_IDLE);
    // DC34SET
    setDC34voltageNP[setDC34voltage].fill( "DC34SET", "Adjustable Voltage", "%.2f", 5, 13.2, 0.1, 0);
    setDC34voltageNP.fill(getDeviceName(), "DC34voltageSET", "Set DC3-4", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    // DC3-4 Control
    dc3_4ControlSP[INDI_ENABLED].fill( "INDI_ENABLED", "On", ISS_OFF);
    dc3_4ControlSP[INDI_DISABLED].fill( "INDI_DISABLED", "Off", ISS_ON);
    dc3_4ControlSP.fill(getDeviceName(), "DC3-4", "DC3-4", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // DC8-9 Control
    dc8_9ControlSP[INDI_ENABLED].fill( "INDI_ENABLED", "On", ISS_OFF);
    dc8_9ControlSP[INDI_DISABLED].fill( "INDI_DISABLED", "Off", ISS_ON);
    dc8_9ControlSP.fill(getDeviceName(), "DC8-9", "DC8-9", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // DC10-11 Control
    dc10_11ControlSP[INDI_ENABLED].fill( "INDI_ENABLED", "On", ISS_OFF);
    dc10_11ControlSP[INDI_DISABLED].fill( "INDI_DISABLED", "Off", ISS_ON);
    dc10_11ControlSP.fill(getDeviceName(), "DC10-11", "DC10-11", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // DC5 TEMP Difference Control
    dc5diffSP[DC5_Manual].fill( "Manual", "Manual", ISS_ON);
    dc5diffSP[DC5_DPD_Mode].fill( "DPD_Mode", "DPD Mode", ISS_OFF);
    dc5diffSP[DC5_CT_Mode].fill( "CT_Mode", "CT Mode", ISS_OFF);
    dc5diffSP.fill(getDeviceName(), "DC5_DIFF", "DC5 Dew Mode", DC5_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    dc5diffSETNP[DC5DIFFSET].fill( "DC5 Auto Control", "Dew Point Difference(C)", "%.2f", 10, 30, 1, 0);
    dc5diffSETNP.fill(getDeviceName(), "DC5_DIFF_SET", "DPD Mode", DC5_TAB, IP_RW, 60, IPS_IDLE);

    dc5constSETNP[DC5CONSTSET].fill( "DC5 Auto Control", "Temperature(C)", "%.2f", 0, 40, 1, 0);
    dc5constSETNP.fill(getDeviceName(), "DC5_CONST_SET", "CT Mode", DC5_TAB, IP_RW, 60, IPS_IDLE);

    // DC6 TEMP Difference Control
    dc6diffSP[DC6_Manual].fill( "Manual", "Manual", ISS_ON);
    dc6diffSP[DC6_DPD_Mode].fill( "DPD_Mode", "DPD Mode", ISS_OFF);
    dc6diffSP[DC6_CT_Mode].fill( "CT_Mode", "CT Mode", ISS_OFF);
    dc6diffSP.fill(getDeviceName(), "DC6_DIFF", "DC6 Dew Mode", DC6_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    dc6diffSETNP[DC6DIFFSET].fill( "DC6 Auto Control", "Dew Point Difference(C)", "%.2f", 10, 30, 1, 0);
    dc6diffSETNP.fill(getDeviceName(), "DC6_DIFF_SET", "DPD Mode", DC6_TAB, IP_RW, 60, IPS_IDLE);

    dc6constSETNP[DC6CONSTSET].fill( "DC6 Auto Control", "Temperature(C)", "%.2f", 0, 40, 1, 0);
    dc6constSETNP.fill(getDeviceName(), "DC6_CONST_SET", "CT Mode", DC6_TAB, IP_RW, 60, IPS_IDLE);

    // DC7 TEMP Difference Control
    dc7diffSP[DC7_Manual].fill( "Manual", "Manual", ISS_ON);
    dc7diffSP[DC7_DPD_Mode].fill( "DPD_Mode", "DPD Mode", ISS_OFF);
    dc7diffSP[DC7_CT_Mode].fill( "CT_Mode", "CT Mode", ISS_OFF);
    dc7diffSP.fill(getDeviceName(), "DC7_DIFF", "DC7 Dew Mode", DC7_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    dc7diffSETNP[DC7DIFFSET].fill( "DC7 Auto Control", "Dew Point Difference(C)", "%.2f", 10, 30, 1, 0);
    dc7diffSETNP.fill(getDeviceName(), "DC7_DIFF_SET", "DPD Mode", DC7_TAB, IP_RW, 60, IPS_IDLE);

    dc7constSETNP[DC7CONSTSET].fill( "DC7 Auto Control", "Temperature(C)", "%.2f", 0, 40, 1, 0);
    dc7constSETNP.fill(getDeviceName(), "DC7_CONST_SET", "CT Mode", DC7_TAB, IP_RW, 60, IPS_IDLE);

    //ENV
    ENVMonitorNP[Probe1_Temp].fill( "Probe1_Temp", "Probe1 Temperature (C)", "%4.2f", 0, 999, 100, 0);
    ENVMonitorNP[Probe2_Temp].fill( "Probe2_Temp", "Probe2 Temperature (C)", "%4.2f", 0, 999, 100, 0);
    ENVMonitorNP[Probe3_Temp].fill( "Probe3_Temp", "Probe3 Temperature (C)", "%4.2f", 0, 999, 100, 0);
    ENVMonitorNP[ENV_Humidity].fill( "ENV_Humidity", "Ambient Humidity %", "%4.2f", 0, 999, 100, 0);
    ENVMonitorNP[ENV_Temp].fill( "ENV_Temp", "Ambient Temperature (C)", "%4.2f", 0, 999, 100, 0);
    ENVMonitorNP[DEW_Point].fill( "DEW_Point", "Dew Point (C)", "%4.2f", 0, 999, 100, 0);
    ENVMonitorNP.fill(getDeviceName(), "ENV_Monitor", "Environment", SENSORS_TAB, IP_RO,60, IPS_IDLE);

    // Weather Interface Parameters
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -15, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15);
    setCriticalParameter("WEATHER_TEMPERATURE");
    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    serialConnection->registerHandshake([&]()
                                        {
                                            return getData();
                                        });
    registerConnection(serialConnection);

    return true;
}

bool WandererBoxProV3::getData()
{
    try
    {
        PortFD = serialConnection->getPortFD();
        tcflush(PortFD, TCIOFLUSH);
        int nbytes_read_name = 0,rc=-1;
        char name[64] = {0};

        //Device Model//////////////////////////////////////////////////////////////////////////////////////////////////////
        if ((rc = tty_read_section(PortFD, name, 'A', 3, &nbytes_read_name)) != TTY_OK)
        {
            char errorMessage[MAXRBUF];
            tty_error_msg(rc, errorMessage, MAXRBUF);
            LOGF_INFO("No data received, the device may not be WandererBox Pro V3, please check the serial port!","Updated");
            LOGF_ERROR("Device read error: %s", errorMessage);
            return false;
        }
        name[nbytes_read_name - 1] = '\0';
        if(strcmp(name, "ZXWBPlusV3")==0||strcmp(name, "WandererCoverV4")==0||strcmp(name, "UltimateV2")==0||strcmp(name, "PlusV2")==0)
        {
            LOGF_INFO("The device is not WandererBox Pro V3!","Updated");
            return false;
        }
        if(strcmp(name, "ZXWBProV3")!=0)
            throw std::exception();
        // Frimware version/////////////////////////////////////////////////////////////////////////////////////////////
        int nbytes_read_version = 0;
        char version[64] = {0};
        tty_read_section(PortFD, version, 'A', 5, &nbytes_read_version);

        version[nbytes_read_version - 1] = '\0';
        firmware=std::atoi(version);


        // Temp probe 1//////////////////////////////////////////////////////////////////////////////////////////
        char temp1[64] = {0};
        int nbytes_read_temp1= 0;
        tty_read_section(PortFD, temp1, 'A', 5, &nbytes_read_temp1);
        temp1[nbytes_read_temp1 - 1] = '\0';
        temp1read = std::strtod(temp1,NULL);

        // Temp probe 2//////////////////////////////////////////////////////////////////////////////////////////
        char temp2[64] = {0};
        int nbytes_read_temp2= 0;
        tty_read_section(PortFD, temp2, 'A', 5, &nbytes_read_temp2);
        temp2[nbytes_read_temp2 - 1] = '\0';
        temp2read = std::strtod(temp2,NULL);

        // Temp probe 3//////////////////////////////////////////////////////////////////////////////////////////
        char temp3[64] = {0};
        int nbytes_read_temp3= 0;
        tty_read_section(PortFD, temp3, 'A', 5, &nbytes_read_temp3);
        temp3[nbytes_read_temp3 - 1] = '\0';
        temp3read = std::strtod(temp3,NULL);

        // DHTH//////////////////////////////////////////////////////////////////////////////////////////
        char DHTH[64] = {0};
        int nbytes_read_DHTH= 0;
        tty_read_section(PortFD, DHTH, 'A', 5, &nbytes_read_DHTH);
        DHTH[nbytes_read_DHTH - 1] = '\0';
        DHTHread = std::strtod(DHTH,NULL);

        // DHTT//////////////////////////////////////////////////////////////////////////////////////////
        char DHTT[64] = {0};
        int nbytes_read_DHTT= 0;
        tty_read_section(PortFD, DHTT, 'A', 5, &nbytes_read_DHTT);
        DHTT[nbytes_read_DHTT - 1] = '\0';
        DHTTread = std::strtod(DHTT,NULL);
        updateENV(temp1read,temp2read,temp3read,DHTHread,DHTTread);

        // Total current//////////////////////////////////////////////////////////////////////////////////////////
        char Tcurrent[64] = {0};
        int nbytes_read_Tcurrent= 0;
        tty_read_section(PortFD, Tcurrent, 'A', 5, &nbytes_read_Tcurrent);
        Tcurrent[nbytes_read_Tcurrent - 1] = '\0';
        Tcurrentread = std::strtod(Tcurrent,NULL);

        // 19V current//////////////////////////////////////////////////////////////////////////////////////////
        char V19current[64] = {0};
        int nbytes_read_V19current= 0;
        tty_read_section(PortFD, V19current, 'A', 5, &nbytes_read_V19current);
        V19current[nbytes_read_V19current - 1] = '\0';
        V19currentread = std::strtod(V19current,NULL);


        // Adjustable Regulator current//////////////////////////////////////////////////////////////////////////////////////////
        char ARcurrent[64] = {0};
        int nbytes_read_ARcurrent= 0;
        tty_read_section(PortFD, ARcurrent, 'A', 5, &nbytes_read_ARcurrent);
        ARcurrent[nbytes_read_ARcurrent - 1] = '\0';
        ARcurrentread = std::strtod(ARcurrent,NULL);


        // Voltage//////////////////////////////////////////////////////////////////////////////////////////
        char voltage[64] = {0};
        int nbytes_read_voltage= 0;
        tty_read_section(PortFD, voltage, 'A', 5, &nbytes_read_voltage);
        voltage[nbytes_read_voltage - 1] = '\0';
        voltageread = std::strtod(voltage,NULL);
        updatePower(Tcurrentread,V19currentread,ARcurrentread,voltageread);

        // USB31//////////////////////////////////////////////////////////////////////////////////////////
        char USB31[64] = {0};
        int nbytes_read_USB31= 0;
        tty_read_section(PortFD, USB31, 'A', 5, &nbytes_read_USB31);
        USB31[nbytes_read_USB31 - 1] = '\0';
        USB31read = std::stoi(USB31);
        updateUSB31(USB31read);

        // USB32//////////////////////////////////////////////////////////////////////////////////////////
        char USB32[64] = {0};
        int nbytes_read_USB32= 0;
        tty_read_section(PortFD, USB32, 'A', 5, &nbytes_read_USB32);
        USB32[nbytes_read_USB32 - 1] = '\0';
        USB32read = std::stoi(USB32);
        updateUSB32(USB32read);

        // USB33//////////////////////////////////////////////////////////////////////////////////////////
        char USB33[64] = {0};
        int nbytes_read_USB33= 0;
        tty_read_section(PortFD, USB33, 'A', 5, &nbytes_read_USB33);
        USB33[nbytes_read_USB33 - 1] = '\0';
        USB33read = std::stoi(USB33);
        updateUSB33(USB33read);

        // USB21//////////////////////////////////////////////////////////////////////////////////////////
        char USB21[64] = {0};
        int nbytes_read_USB21= 0;
        tty_read_section(PortFD, USB21, 'A', 5, &nbytes_read_USB21);
        USB21[nbytes_read_USB21 - 1] = '\0';
        USB21read = std::stoi(USB21);
        updateUSB21(USB21read);

        // USB22//////////////////////////////////////////////////////////////////////////////////////////
        char USB22[64] = {0};
        int nbytes_read_USB22= 0;
        tty_read_section(PortFD, USB22, 'A', 5, &nbytes_read_USB22);
        USB22[nbytes_read_USB22 - 1] = '\0';
        USB22read = std::stoi(USB22);
        updateUSB22(USB22read);

        // DC34//////////////////////////////////////////////////////////////////////////////////////////
        char DC34[64] = {0};
        int nbytes_read_DC34= 0;
        tty_read_section(PortFD, DC34, 'A', 5, &nbytes_read_DC34);
        DC34[nbytes_read_DC34 - 1] = '\0';
        DC34read = std::stoi(DC34);
        updateDC34(DC34read);

        // DC5//////////////////////////////////////////////////////////////////////////////////////////
        char DC5[64] = {0};
        int nbytes_read_DC5= 0;
        tty_read_section(PortFD, DC5, 'A', 5, &nbytes_read_DC5);
        DC5[nbytes_read_DC5 - 1] = '\0';
        DC5read = std::stoi(DC5);
        updateDC5(DC5read);

        // DC6//////////////////////////////////////////////////////////////////////////////////////////
        char DC6[64] = {0};
        int nbytes_read_DC6= 0;
        tty_read_section(PortFD, DC6, 'A', 5, &nbytes_read_DC6);
        DC6[nbytes_read_DC6 - 1] = '\0';
        DC6read = std::stoi(DC6);
        updateDC6(DC6read);

        // DC7//////////////////////////////////////////////////////////////////////////////////////////
        char DC7[64] = {0};
        int nbytes_read_DC7= 0;
        tty_read_section(PortFD, DC7, 'A', 5, &nbytes_read_DC7);
        DC7[nbytes_read_DC7 - 1] = '\0';
        DC7read = std::stoi(DC7);
        updateDC7(DC7read);

        // DC8_9//////////////////////////////////////////////////////////////////////////////////////////
        char DC8_9[64] = {0};
        int nbytes_read_DC8_9= 0;
        tty_read_section(PortFD, DC8_9, 'A', 5, &nbytes_read_DC8_9);
        DC8_9[nbytes_read_DC8_9 - 1] = '\0';
        DC8_9read =std::stoi(DC8_9);
        updateDC8_9(DC8_9read);

        // DC10_11//////////////////////////////////////////////////////////////////////////////////////////
        char DC10_11[64] = {0};
        int nbytes_read_DC10_11= 0;
        tty_read_section(PortFD, DC10_11, 'A', 5, &nbytes_read_DC10_11);
        DC10_11[nbytes_read_DC10_11 - 1] = '\0';
        DC10_11read =std::stoi(DC10_11);
        updateDC10_11(DC10_11read);

        // DC34SET//////////////////////////////////////////////////////////////////////////////////////////
        char DC34SET[64] = {0};
        int nbytes_read_DC34SET= 0;
        tty_read_section(PortFD, DC34SET, 'A', 5, &nbytes_read_DC34SET);
        DC34SET[nbytes_read_DC34SET - 1] = '\0';
        //LOGF_INFO("All Data Updated","Updated");
        DC34SETread = std::stoi(DC34SET);
        updateDC34SET(DC34SETread);


        //DC5 DEW CONTROL
        if(dc5diffSP[DC5_DPD_Mode].getState() == ISS_ON)
        {
            if(temp1read<ENVMonitorNP[DEW_Point].value+dc5diffSETNP[DC5DIFFSET].value)
                sendCommand("5255");
            else
                sendCommand("5000");
        }

        if(DC5CONSTMODE==true )
        {
            if(temp1read<dc5constSETNP[DC5CONSTSET].value)
                sendCommand("5255");
            else
                sendCommand("5000");
        }
        if (dc5diffSP[DC5_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value==-127)
        {
            DC5DIFFMODE=false;
            DC5CONSTMODE=false;
            defineProperty(dc5ControlNP);
            deleteProperty(dc5diffSETNP);
            deleteProperty(dc5constSETNP);
            LOGF_ERROR("Temp probe 1 not connected, Dew Point Difference Mode for DC5 has exited!","Updated");
            LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
            dc5diffSP[DC5_Manual].setState( ISS_ON);
            dc5diffSP[DC5_DPD_Mode].setState( ISS_OFF);
            dc5diffSP[DC5_CT_Mode].setState( ISS_OFF);
            dc5diffSP.setState(IPS_OK);
            dc5diffSP.apply();
        }
        if (dc5diffSP[DC5_DPD_Mode].getState() == ISS_ON&&isnan(ENVMonitorNP[DEW_Point].value)==1)
        {
            DC5DIFFMODE=false;
            DC5CONSTMODE=false;
            defineProperty(dc5ControlNP);
            deleteProperty(dc5diffSETNP);
            deleteProperty(dc5constSETNP);
            LOGF_ERROR("DHT22 Humidity&Temperature sensor not connected, Dew Point Difference Mode for DC5 has exited!","Updated");
            dc5diffSP[DC5_Manual].setState( ISS_ON);
            dc5diffSP[DC5_DPD_Mode].setState( ISS_OFF);
            dc5diffSP[DC5_CT_Mode].setState( ISS_OFF);
            dc5diffSP.setState(IPS_OK);
            dc5diffSP.apply();
        }
        if (dc5diffSP[DC5_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value==-127)
        {
            DC5DIFFMODE=false;
            DC5CONSTMODE=false;
            defineProperty(dc5ControlNP);
            deleteProperty(dc5diffSETNP);
            deleteProperty(dc5constSETNP);
            LOGF_ERROR("Temp probe 1 not connected, Constant Temperature Mode for DC5 has exited!","Updated");
            LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
            dc5diffSP[DC5_Manual].setState( ISS_ON);
            dc5diffSP[DC5_DPD_Mode].setState( ISS_OFF);
            dc5diffSP[DC5_CT_Mode].setState( ISS_OFF);
            dc5diffSP.setState(IPS_OK);
            dc5diffSP.apply();
        }



        //DC6 DEW CONTROL
        if(DC6DIFFMODE==true )
        {
            if(temp2read<ENVMonitorNP[DEW_Point].value+dc6diffSETNP[DC6DIFFSET].value)
                sendCommand("6255");
            else
                sendCommand("6000");
        }
        if(DC6CONSTMODE==true )
        {
            if(temp2read<dc6constSETNP[DC6CONSTSET].value)
                sendCommand("6255");
            else
                sendCommand("6000");
        }
        if (dc6diffSP[DC6_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe2_Temp].value==-127)
        {
            DC6DIFFMODE=false;
            DC6CONSTMODE=false;
            defineProperty(dc6ControlNP);
            deleteProperty(dc6diffSETNP);
            deleteProperty(dc6constSETNP);
            LOGF_ERROR("Temp probe 2 not connected, Dew Point Difference Mode for DC6 has exited!","Updated");
            LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
            dc6diffSP[DC6_Manual].setState( ISS_ON);
            dc6diffSP[DC6_DPD_Mode].setState( ISS_OFF);
            dc6diffSP[DC6_CT_Mode].setState( ISS_OFF);
            dc6diffSP.setState(IPS_OK);
            dc6diffSP.apply();
        }
        if (dc6diffSP[DC6_DPD_Mode].getState() == ISS_ON&&isnan(ENVMonitorNP[DEW_Point].value)==1)
        {
            DC6DIFFMODE=false;
            DC6CONSTMODE=false;
            defineProperty(dc6ControlNP);
            deleteProperty(dc6diffSETNP);
            deleteProperty(dc6constSETNP);
            LOGF_ERROR("DHT22 Humidity&Temperature sensor not connected, Dew Point Difference Mode for DC6 has exited!","Updated");
            dc6diffSP[DC6_Manual].setState( ISS_ON);
            dc6diffSP[DC6_DPD_Mode].setState( ISS_OFF);
            dc6diffSP[DC6_CT_Mode].setState( ISS_OFF);
            dc6diffSP.setState(IPS_OK);
            dc6diffSP.apply();
        }
        if (dc6diffSP[DC6_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe2_Temp].value==-127)
        {
            DC6DIFFMODE=false;
            DC6CONSTMODE=false;
            defineProperty(dc6ControlNP);
            deleteProperty(dc6diffSETNP);
            deleteProperty(dc6constSETNP);
            LOGF_ERROR("Temp probe 2 not connected, Constant Temperature Mode for DC6 has exited!","Updated");
            LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
            dc6diffSP[DC6_Manual].setState( ISS_ON);
            dc6diffSP[DC6_DPD_Mode].setState( ISS_OFF);
            dc6diffSP[DC6_CT_Mode].setState( ISS_OFF);
            dc6diffSP.setState(IPS_OK);
            dc6diffSP.apply();
        }


        //DC7 DEW CONTROL
        if(DC7DIFFMODE==true )
        {
            if(temp3read<ENVMonitorNP[DEW_Point].value+dc7diffSETNP[DC7DIFFSET].value)
                sendCommand("7255");
            else
                sendCommand("7000");
        }
        if(DC7CONSTMODE==true )
        {
            if(temp3read<dc7constSETNP[DC7CONSTSET].value)
                sendCommand("7255");
            else
                sendCommand("7000");
        }
        if (dc7diffSP[DC7_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe3_Temp].value==-127)
        {
            DC7DIFFMODE=false;
            DC7CONSTMODE=false;
            defineProperty(dc7ControlNP);
            deleteProperty(dc7diffSETNP);
            deleteProperty(dc7constSETNP);
            LOGF_ERROR("Temp probe 3 not connected, Dew Point Difference Mode for DC7 has exited!","Updated");
            LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
            dc7diffSP[DC7_Manual].setState( ISS_ON);
            dc7diffSP[DC7_DPD_Mode].setState( ISS_OFF);
            dc7diffSP[DC7_CT_Mode].setState( ISS_OFF);
            dc7diffSP.setState(IPS_OK);
            dc7diffSP.apply();
        }
        if (dc7diffSP[DC7_DPD_Mode].getState() == ISS_ON&&isnan(ENVMonitorNP[DEW_Point].value)==1)
        {
            DC7DIFFMODE=false;
            DC7CONSTMODE=false;
            defineProperty(dc7ControlNP);
            deleteProperty(dc7diffSETNP);
            deleteProperty(dc7constSETNP);
            LOGF_ERROR("DHT22 Humidity&Temperature sensor not connected, Dew Point Difference Mode for DC7 has exited!","Updated");
            dc7diffSP[DC7_Manual].setState( ISS_ON);
            dc7diffSP[DC7_DPD_Mode].setState( ISS_OFF);
            dc7diffSP[DC7_CT_Mode].setState( ISS_OFF);
            dc7diffSP.setState(IPS_OK);
            dc7diffSP.apply();
        }
        if (dc7diffSP[DC7_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe3_Temp].value==-127)
        {
            DC7DIFFMODE=false;
            DC7CONSTMODE=false;
            defineProperty(dc7ControlNP);
            deleteProperty(dc7diffSETNP);
            deleteProperty(dc7constSETNP);
            LOGF_ERROR("Temp probe 3 not connected, Constant Temperature Mode for DC7 has exited!","Updated");
            LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
            dc7diffSP[DC7_Manual].setState( ISS_ON);
            dc7diffSP[DC7_DPD_Mode].setState( ISS_OFF);
            dc7diffSP[DC7_CT_Mode].setState( ISS_OFF);
            dc7diffSP.setState(IPS_OK);
            dc7diffSP.apply();
        }

    }
    catch(std::exception& e)
    {
        //LOGF_INFO("Data read failed","failed");
    }
    return true;
}



void WandererBoxProV3::updateENV(double temp1,double temp2,double temp3,double DHTH,double DHTT)
{
    ENVMonitorNP[Probe1_Temp].setValue(temp1);
    ENVMonitorNP[Probe2_Temp].setValue(temp2);
    ENVMonitorNP[Probe3_Temp].setValue(temp3);
    ENVMonitorNP[ENV_Humidity].setValue(DHTH);
    ENVMonitorNP[ENV_Temp].setValue(DHTT);
    double dewPoint = (237.7 * ((17.27 * DHTT) / (237.7 + DHTT) + std::log((DHTH / 100)))) / (17.27 - ((17.27 * DHTT) / (237.7 + DHTT) + std::log((DHTH / 100))));
    ENVMonitorNP[DEW_Point].setValue(dewPoint);
    ENVMonitorNP.setState(IPS_OK);
    ENVMonitorNP.apply();

    // Update Weather Interface parameters
    setParameterValue("WEATHER_TEMPERATURE", DHTT);
    setParameterValue("WEATHER_HUMIDITY", DHTH);
    setParameterValue("WEATHER_DEWPOINT", dewPoint);
    ParametersNP.setState(IPS_OK);
    ParametersNP.apply();
    if (syncCriticalParameters())
        critialParametersLP.apply();
}


void WandererBoxProV3::updatePower(double Tcurrent,double V19_current,double AR_current,double voltage)
{
    // Power Monitor
    PowerMonitorNP[VOLTAGE].setValue(voltage);
    PowerMonitorNP[TOTAL_CURRENT].setValue(Tcurrent);
    PowerMonitorNP[V19_CURRENT].setValue(V19_current);
    PowerMonitorNP[AR_CURRENT].setValue(AR_current);
    PowerMonitorNP.setState(IPS_OK);
    PowerMonitorNP.apply();
}

void WandererBoxProV3::updateUSB31(int res)
{
    usb31ControlSP[INDI_ENABLED].setState( (res== 1) ? ISS_ON : ISS_OFF);
    usb31ControlSP[INDI_DISABLED].setState( (res== 0) ? ISS_ON : ISS_OFF);
    usb31ControlSP.setState((res== 1) ? IPS_OK : IPS_IDLE);
    usb31ControlSP.apply();
}

void WandererBoxProV3::updateUSB32(int res)
{
    usb32ControlSP[INDI_ENABLED].setState( (res== 1) ? ISS_ON : ISS_OFF);
    usb32ControlSP[INDI_DISABLED].setState( (res== 0) ? ISS_ON : ISS_OFF);
    usb32ControlSP.setState((res== 1) ? IPS_OK : IPS_IDLE);
    usb32ControlSP.apply();
}

void WandererBoxProV3::updateUSB33(int res)
{
    usb33ControlSP[INDI_ENABLED].setState( (res== 1) ? ISS_ON : ISS_OFF);
    usb33ControlSP[INDI_DISABLED].setState( (res== 0) ? ISS_ON : ISS_OFF);
    usb33ControlSP.setState((res== 1) ? IPS_OK : IPS_IDLE);
    usb33ControlSP.apply();
}

void WandererBoxProV3::updateUSB21(int res)
{
    usb21ControlSP[INDI_ENABLED].setState( (res== 1) ? ISS_ON : ISS_OFF);
    usb21ControlSP[INDI_DISABLED].setState( (res== 0) ? ISS_ON : ISS_OFF);
    usb21ControlSP.setState((res== 1) ? IPS_OK : IPS_IDLE);
    usb21ControlSP.apply();
}

void WandererBoxProV3::updateUSB22(int res)
{
    usb22ControlSP[INDI_ENABLED].setState( (res== 1) ? ISS_ON : ISS_OFF);
    usb22ControlSP[INDI_DISABLED].setState( (res== 0) ? ISS_ON : ISS_OFF);
    usb22ControlSP.setState((res== 1) ? IPS_OK : IPS_IDLE);
    usb22ControlSP.apply();
}

void WandererBoxProV3::updateDC34(int res)
{
    dc3_4ControlSP[INDI_ENABLED].setState( (res== 1) ? ISS_ON : ISS_OFF);
    dc3_4ControlSP[INDI_DISABLED].setState( (res== 0) ? ISS_ON : ISS_OFF);
    dc3_4ControlSP.setState((res== 1) ? IPS_OK : IPS_IDLE);
    dc3_4ControlSP.apply();
}

void WandererBoxProV3::updateDC5(int res)
{
    dc5ControlNP[0].setValue(res);
    dc5ControlNP.setState(IPS_OK);
    dc5ControlNP.apply();
}

void WandererBoxProV3::updateDC6(int res)
{
    dc6ControlNP[0].setValue(res);
    dc6ControlNP.setState(IPS_OK);
    dc6ControlNP.apply();
}

void WandererBoxProV3::updateDC7(int res)
{
    dc7ControlNP[0].setValue(res);
    dc7ControlNP.setState(IPS_OK);
    dc7ControlNP.apply();
}

void WandererBoxProV3::updateDC8_9(int res)
{
    dc8_9ControlSP[INDI_ENABLED].setState( (res== 1) ? ISS_ON : ISS_OFF);
    dc8_9ControlSP[INDI_DISABLED].setState( (res== 0) ? ISS_ON : ISS_OFF);
    dc8_9ControlSP.setState((res== 1) ? IPS_OK : IPS_IDLE);
    dc8_9ControlSP.apply();
}

void WandererBoxProV3::updateDC10_11(int res)
{
    dc10_11ControlSP[INDI_ENABLED].setState( (res== 1) ? ISS_ON : ISS_OFF);
    dc10_11ControlSP[INDI_DISABLED].setState( (res== 0) ? ISS_ON : ISS_OFF);
    dc10_11ControlSP.setState((res== 1) ? IPS_OK : IPS_IDLE);
    dc10_11ControlSP.apply();
}

void WandererBoxProV3::updateDC34SET(double res)
{
    setDC34voltageNP[setDC34voltage].setValue(res/10);
    setDC34voltageNP.setState(IPS_OK);
    setDC34voltageNP.apply();
}



bool WandererBoxProV3::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {

        if(firmware>=20240216)
        {
            defineProperty(CalibrateSP);
            LOGF_INFO("Firmware version: %d", firmware);
        }
        else
        {
            LOGF_INFO("The firmware is outdated, please upgrade to the latest firmware, or power reading calibration will be unavailable.","failed");
        }
        defineProperty(PowerMonitorNP);


        defineProperty(usb31ControlSP);
        defineProperty(usb32ControlSP);
        defineProperty(usb33ControlSP);
        defineProperty(usb21ControlSP);
        defineProperty(usb22ControlSP);

        defineProperty(setDC34voltageNP);
        defineProperty(dc3_4ControlSP);

        defineProperty(dc8_9ControlSP);
        defineProperty(dc10_11ControlSP);

        defineProperty(dc5diffSP);
        defineProperty(dc6diffSP);
        defineProperty(dc7diffSP);


        //DC5////////////////////
        if(dc5diffSP[DC5_DPD_Mode].getState() == ISS_ON)
        {
            deleteProperty(dc5constSETNP);
            deleteProperty(dc5ControlNP);
            defineProperty(dc5diffSETNP);
        }
        else if(dc5diffSP[DC5_CT_Mode].getState() == ISS_ON)
        {
            deleteProperty(dc5ControlNP);
            deleteProperty(dc5diffSETNP);
            defineProperty(dc5constSETNP);
        }
        else
        {
            defineProperty(dc5ControlNP);
            deleteProperty(dc5diffSETNP);
            deleteProperty(dc5constSETNP);
        }
        //DC6////////////////
        if(dc6diffSP[DC6_DPD_Mode].getState() == ISS_ON)
        {
            deleteProperty(dc6constSETNP);
            deleteProperty(dc6ControlNP);
            defineProperty(dc6diffSETNP);
        }
        else if(dc6diffSP[DC6_CT_Mode].getState() == ISS_ON)
        {
            deleteProperty(dc6ControlNP);
            deleteProperty(dc6diffSETNP);
            defineProperty(dc6constSETNP);
        }
        else
        {
            defineProperty(dc6ControlNP);
            deleteProperty(dc6diffSETNP);
            deleteProperty(dc6constSETNP);
        }
        //DC7////////////////
        if(dc7diffSP[DC7_DPD_Mode].getState() == ISS_ON)
        {
            deleteProperty(dc7constSETNP);
            deleteProperty(dc7ControlNP);
            defineProperty(dc7diffSETNP);
        }
        else if(dc7diffSP[DC7_CT_Mode].getState() == ISS_ON)
        {
            deleteProperty(dc7ControlNP);
            deleteProperty(dc7diffSETNP);
            defineProperty(dc7constSETNP);
        }
        else
        {
            defineProperty(dc7ControlNP);
            deleteProperty(dc7diffSETNP);
            deleteProperty(dc7constSETNP);
        }


        defineProperty(ENVMonitorNP);

        // Weather
        WI::updateProperties();

    }
    else
    {

        deleteProperty(CalibrateSP);
        deleteProperty(PowerMonitorNP);
        deleteProperty(ENVMonitorNP);

        // Weather
        WI::updateProperties();

        deleteProperty(dc3_4ControlSP);
        deleteProperty(setDC34voltageNP);
        deleteProperty(dc8_9ControlSP);
        deleteProperty(dc10_11ControlSP);
        deleteProperty(usb31ControlSP);
        deleteProperty(usb32ControlSP);
        deleteProperty(usb33ControlSP);
        deleteProperty(usb21ControlSP);
        deleteProperty(usb22ControlSP);

        deleteProperty(dc5ControlNP);
        deleteProperty(dc6ControlNP);
        deleteProperty(dc7ControlNP);

        deleteProperty(dc5diffSP);
        deleteProperty(dc5diffSETNP);

        deleteProperty(dc6diffSP);
        deleteProperty(dc6diffSETNP);

        deleteProperty(dc7diffSP);
        deleteProperty(dc7diffSETNP);


    }
    return true;
}

bool WandererBoxProV3::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (strstr(name, "WEATHER_"))
            return WI::processSwitch(dev, name, states, names, n);
    }

    // Calibrate
    if (CalibrateSP.isNameMatch(name))
    {
        CalibrateSP.setState(sendCommand("66300744") ? IPS_OK : IPS_ALERT);
        CalibrateSP.apply();
        LOG_INFO("Calibrating Current Readings...");
        return true;
    }


    // DC3-4 Control
    if (dc3_4ControlSP.isNameMatch(name))
    {
        dc3_4ControlSP.update(states, names, n);
        dc3_4ControlSP.setState(IPS_ALERT);
        char cmd[128] = {0};
        snprintf(cmd, 128, "10%d", (dc3_4ControlSP[INDI_ENABLED].getState() == ISS_ON) ? 1 : 0);
        dc3_4ControlSP.setState( sendCommand(cmd) ? IPS_OK : IPS_ALERT);
        dc3_4ControlSP.apply();

        return true;
    }
    // DC8-9 Control
    if (dc8_9ControlSP.isNameMatch(name))
    {
        dc8_9ControlSP.update(states, names, n);
        dc8_9ControlSP.setState(IPS_ALERT);
        char cmd[128] = {0};
        snprintf(cmd, 128, "20%d", (dc8_9ControlSP[INDI_ENABLED].getState() == ISS_ON) ? 1 : 0);
        dc8_9ControlSP.setState( sendCommand(cmd) ? IPS_OK : IPS_ALERT);
        dc8_9ControlSP.apply();

        return true;
    }
    // DC10-11 Control
    if (dc10_11ControlSP.isNameMatch(name))
    {
        dc10_11ControlSP.update(states, names, n);
        dc10_11ControlSP.setState(IPS_ALERT);
        char cmd[128] = {0};
        snprintf(cmd, 128, "21%d", (dc10_11ControlSP[INDI_ENABLED].getState() == ISS_ON) ? 1 : 0);
        dc10_11ControlSP.setState( sendCommand(cmd) ? IPS_OK : IPS_ALERT);
        dc10_11ControlSP.apply();

        return true;
    }

    // USB31 Control
    if (usb31ControlSP.isNameMatch(name))
    {
        usb31ControlSP.update(states, names, n);
        usb31ControlSP.setState(IPS_ALERT);
        char cmd[128] = {0};
        snprintf(cmd, 128, "11%d", (usb31ControlSP[INDI_ENABLED].getState() == ISS_ON) ? 1 : 0);
        usb31ControlSP.setState( sendCommand(cmd) ? IPS_OK : IPS_ALERT);

        return true;
    }
    // USB32 Control
    if (usb32ControlSP.isNameMatch(name))
    {
        usb32ControlSP.update(states, names, n);
        usb32ControlSP.setState(IPS_ALERT);
        char cmd[128] = {0};
        snprintf(cmd, 128, "12%d", (usb32ControlSP[INDI_ENABLED].getState() == ISS_ON) ? 1 : 0);
        usb32ControlSP.setState( sendCommand(cmd) ? IPS_OK : IPS_ALERT);
        usb32ControlSP.apply();

        return true;
    }
    // USB33 Control
    if (usb33ControlSP.isNameMatch(name))
    {
        usb33ControlSP.update(states, names, n);
        usb33ControlSP.setState(IPS_ALERT);
        char cmd[128] = {0};
        snprintf(cmd, 128, "13%d", (usb33ControlSP[INDI_ENABLED].getState() == ISS_ON) ? 1 : 0);
        usb33ControlSP.setState( sendCommand(cmd) ? IPS_OK : IPS_ALERT);
        usb33ControlSP.apply();

        return true;
    }
    // USB21 Control
    if (usb21ControlSP.isNameMatch(name))
    {
        usb21ControlSP.update(states, names, n);
        usb21ControlSP.setState(IPS_ALERT);
        char cmd[128] = {0};
        snprintf(cmd, 128, "14%d", (usb21ControlSP[INDI_ENABLED].getState() == ISS_ON) ? 1 : 0);
        usb21ControlSP.setState( sendCommand(cmd) ? IPS_OK : IPS_ALERT);
        usb21ControlSP.apply();

        return true;
    }
    // USB22 Control
    if (usb22ControlSP.isNameMatch(name))
    {
        usb22ControlSP.update(states, names, n);
        usb22ControlSP.setState(IPS_ALERT);
        char cmd[128] = {0};
        snprintf(cmd, 128, "15%d", (usb22ControlSP[INDI_ENABLED].getState() == ISS_ON) ? 1 : 0);
        usb22ControlSP.setState( sendCommand(cmd) ? IPS_OK : IPS_ALERT);
        usb22ControlSP.apply();

        return true;
    }

    // DC5 DEW
    if (dc5diffSP.isNameMatch(name))
    {
        dc5diffSP.update(states, names, n);
        dc5diffSP.setState(IPS_ALERT);
        if(dc5diffSP[DC5_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value!=-127&&isnan(ENVMonitorNP[DEW_Point].value)==0)
        {
            DC5DIFFMODE=true;
            DC5CONSTMODE=false;
            deleteProperty(dc5ControlNP);
            deleteProperty(dc5constSETNP);
            defineProperty(dc5diffSETNP);

            dc5diffSETNP.setState(IPS_OK);
            dc5diffSETNP.apply();
            dc5diffSP.setState(IPS_OK) ;
            dc5diffSP.apply();
            LOGF_INFO("Dew Point Difference Mode for DC5 activated! WandererBox will keep the dew heater at the temperature higher than the dew point by the set value.","Updated");
            return true;
        }
        else if(dc5diffSP[DC5_DPD_Mode].getState() == ISS_ON&&(ENVMonitorNP[Probe1_Temp].value==-127||isnan(ENVMonitorNP[DEW_Point].value)==1))
        {
            DC5DIFFMODE=false;
            DC5CONSTMODE=false;
            dc5diffSP[DC5_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC5 activated! Please adjust the duty cycle manually, you can also use DC5 as an ordinary switch.","Updated");
            dc5diffSP.apply();
        }
        else if(dc5diffSP[DC5_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value!=-127)
        {
            DC5CONSTMODE=true;
            DC5DIFFMODE=false;
            deleteProperty(dc5diffSETNP);
            deleteProperty(dc5ControlNP);
            defineProperty(dc5constSETNP);

            dc5constSETNP.setState(IPS_OK);
            dc5constSETNP.apply();
            dc5diffSP.setState(IPS_OK) ;
            dc5diffSP.apply();
            LOGF_INFO("Constant Temperature Mode for DC5 activated! WandererBox will keep the dew heater at the set temperature.","Updated");
            return true;
        }
        else if(dc5diffSP[DC5_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value==-127)
        {
            DC5DIFFMODE=false;
            DC5CONSTMODE=false;
            dc5diffSP[DC5_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC5 activated! Please adjust the duty cycle manually, you can also use DC5 as an ordinary switch.","Updated");
            dc5diffSP.apply();
        }
        else
        {
            DC5DIFFMODE=false;
            DC5CONSTMODE=false;
            defineProperty(dc5ControlNP);
            deleteProperty(dc5diffSETNP);
            deleteProperty(dc5constSETNP);
            dc5diffSP.setState(IPS_OK) ;
            dc5diffSP.apply();
            LOGF_INFO("Manual Mode for DC5 activated! Please adjust the duty cycle manually, you can also use DC5 as an ordinary switch.","Updated");
            return true;
        }

    }

    // DC6 DEW
    if (dc6diffSP.isNameMatch(name))
    {
        dc6diffSP.update(states, names, n);
        dc6diffSP.setState(IPS_ALERT);
        if(dc6diffSP[DC6_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe2_Temp].value!=-127&&isnan(ENVMonitorNP[DEW_Point].value)==0)
        {
            DC6DIFFMODE=true;
            DC6CONSTMODE=false;
            deleteProperty(dc6ControlNP);
            deleteProperty(dc6constSETNP);
            defineProperty(dc6diffSETNP);

            dc6diffSETNP.setState(IPS_OK);
            dc6diffSETNP.apply();
            dc6diffSP.setState(IPS_OK) ;
            dc6diffSP.apply();
            LOGF_INFO("Dew Point Difference Mode for DC6 activated! WandererBox will keep the dew heater at the temperature higher than the dew point by the set value.","Updated");
            return true;
        }
        else if(dc6diffSP[DC6_DPD_Mode].getState() == ISS_ON&&(ENVMonitorNP[Probe2_Temp].value==-127||isnan(ENVMonitorNP[DEW_Point].value)==1))
        {
            DC6DIFFMODE=false;
            DC6CONSTMODE=false;
            dc6diffSP[DC6_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC6 activated! Please adjust the duty cycle manually, you can also use DC6 as an ordinary switch.","Updated");
            dc6diffSP.apply();
        }
        else if(dc6diffSP[DC6_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe2_Temp].value!=-127)
        {
            DC6CONSTMODE=true;
            DC6DIFFMODE=false;
            deleteProperty(dc6diffSETNP);
            deleteProperty(dc6ControlNP);
            defineProperty(dc6constSETNP);

            dc6constSETNP.setState(IPS_OK);
            dc6constSETNP.apply();
            dc6diffSP.setState(IPS_OK) ;
            dc6diffSP.apply();
            LOGF_INFO("Constant Temperature Mode for DC6 activated! WandererBox will keep the dew heater at the set temperature.","Updated");
            return true;
        }
        else if(dc6diffSP[DC6_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe2_Temp].value==-127)
        {
            DC6DIFFMODE=false;
            DC6CONSTMODE=false;
            dc6diffSP[DC6_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC6 activated! Please adjust the duty cycle manually, you can also use DC6 as an ordinary switch.","Updated");
            dc6diffSP.apply();
        }
        else
        {
            DC6DIFFMODE=false;
            DC6CONSTMODE=false;
            defineProperty(dc6ControlNP);
            deleteProperty(dc6diffSETNP);
            deleteProperty(dc6constSETNP);
            dc6diffSP.setState(IPS_OK) ;
            dc6diffSP.apply();
            LOGF_INFO("Manual Mode for DC6 activated! Please adjust the duty cycle manually, you can also use DC6 as an ordinary switch.","Updated");
            return true;
        }

    }

    // DC7 DEW
    if (dc7diffSP.isNameMatch(name))
    {
        dc7diffSP.update(states, names, n);
        dc7diffSP.setState(IPS_ALERT);
        if(dc7diffSP[DC7_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe3_Temp].value!=-127&&isnan(ENVMonitorNP[DEW_Point].value)==0)
        {
            DC7DIFFMODE=true;
            DC7CONSTMODE=false;
            deleteProperty(dc7ControlNP);
            deleteProperty(dc7constSETNP);
            defineProperty(dc7diffSETNP);

            dc7diffSETNP.setState(IPS_OK);
            dc7diffSETNP.apply();
            dc7diffSP.setState(IPS_OK) ;
            dc7diffSP.apply();
            LOGF_INFO("Dew Point Difference Mode for DC7 activated! WandererBox will keep the dew heater at the temperature higher than the dew point by the set value.","Updated");
            return true;
        }
        else if(dc7diffSP[DC7_DPD_Mode].getState() == ISS_ON&&(ENVMonitorNP[Probe3_Temp].value==-127||isnan(ENVMonitorNP[DEW_Point].value)==1))
        {
            DC7DIFFMODE=false;
            DC7CONSTMODE=false;
            dc7diffSP[DC7_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC7 activated! Please adjust the duty cycle manually, you can also use DC7 as an ordinary switch.","Updated");
            dc7diffSP.apply();
        }
        else if(dc7diffSP[DC7_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe3_Temp].value!=-127)
        {
            DC7CONSTMODE=true;
            DC7DIFFMODE=false;
            deleteProperty(dc7diffSETNP);
            deleteProperty(dc7ControlNP);
            defineProperty(dc7constSETNP);

            dc7constSETNP.setState(IPS_OK);
            dc7constSETNP.apply();
            dc7diffSP.setState(IPS_OK) ;
            dc7diffSP.apply();
            LOGF_INFO("Constant Temperature Mode for DC7 activated! WandererBox will keep the dew heater at the set temperature.","Updated");
            return true;
        }
        else if(dc7diffSP[DC7_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe3_Temp].value==-127)
        {
            DC7DIFFMODE=false;
            DC7CONSTMODE=false;
            dc7diffSP[DC7_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC7 activated! Please adjust the duty cycle manually, you can also use DC7 as an ordinary switch.","Updated");
            dc7diffSP.apply();
        }
        else
        {
            DC7DIFFMODE=false;
            DC7CONSTMODE=false;
            defineProperty(dc7ControlNP);
            deleteProperty(dc7diffSETNP);
            deleteProperty(dc7constSETNP);
            dc7diffSP.setState(IPS_OK) ;
            dc7diffSP.apply();
            LOGF_INFO("Manual Mode for DC7 activated! Please adjust the duty cycle manually, you can also use DC7 as an ordinary switch.","Updated");
            return true;
        }

    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool WandererBoxProV3::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (strstr(name, "WEATHER_"))
            return WI::processNumber(dev, name, values, names, n);
        // DC5
        if (dc5ControlNP.isNameMatch(name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                rc1 = setDewPWM(5, static_cast<uint8_t>(values[i]));
            }

            dc5ControlNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (dc5ControlNP.getState() == IPS_OK)
                dc5ControlNP.update(values, names, n);
            dc5ControlNP.apply();
            return true;
        }
        if (dc5diffSETNP.isNameMatch(name))
        {

            dc5diffSETNP.setState(IPS_OK) ;
            if (dc5diffSETNP.getState() == IPS_OK)
                dc5diffSETNP.update(values, names, n);
            dc5diffSETNP.apply();
            return true;
        }
        if (dc5constSETNP.isNameMatch(name))
        {

            dc5constSETNP.setState(IPS_OK) ;
            if (dc5constSETNP.getState() == IPS_OK)
                dc5constSETNP.update(values, names, n);
            dc5constSETNP.apply();
            return true;
        }
        // DC6
        if (dc6ControlNP.isNameMatch(name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                    rc1 = setDewPWM(6, static_cast<uint8_t>(values[i]));
            }

            dc6ControlNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (dc6ControlNP.getState() == IPS_OK)
                dc6ControlNP.update(values, names, n);
            dc6ControlNP.apply();
            return true;
        }
        if (dc6diffSETNP.isNameMatch(name))
        {

            dc6diffSETNP.setState(IPS_OK) ;
            if (dc6diffSETNP.getState() == IPS_OK)
                dc6diffSETNP.update(values, names, n);
            dc6diffSETNP.apply();
            return true;
        }
        if (dc6constSETNP.isNameMatch(name))
        {

            dc6constSETNP.setState(IPS_OK) ;
            if (dc6constSETNP.getState() == IPS_OK)
                dc6constSETNP.update(values, names, n);
            dc6constSETNP.apply();
            return true;
        }
        // DC7
        if (dc7ControlNP.isNameMatch(name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                    rc1 = setDewPWM(7, static_cast<uint8_t>(values[i]));
            }

            dc7ControlNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (dc7ControlNP.getState() == IPS_OK)
                dc7ControlNP.update(values, names, n);
            dc7ControlNP.apply();
            return true;
        }
        if (dc7diffSETNP.isNameMatch(name))
        {

            dc7diffSETNP.setState(IPS_OK) ;
            if (dc7diffSETNP.getState() == IPS_OK)
                dc7diffSETNP.update(values, names, n);
            dc7diffSETNP.apply();
            return true;
        }
        if (dc7constSETNP.isNameMatch(name))
        {

            dc7constSETNP.setState(IPS_OK) ;
            if (dc7constSETNP.getState() == IPS_OK)
                dc7constSETNP.update(values, names, n);
            dc7constSETNP.apply();
            return true;
        }
        // DC34voltageSET
        if (setDC34voltageNP.isNameMatch(name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                    rc1 = setDewPWM(20, static_cast<uint8_t>(10*values[i]));
            }

            setDC34voltageNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (setDC34voltageNP.getState() == IPS_OK)
                setDC34voltageNP.update(values, names, n);
            setDC34voltageNP.apply();
            return true;
        }


    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool WandererBoxProV3::setDewPWM(uint8_t id, uint8_t value)
{
    char cmd[64] = {0};
    snprintf(cmd, 64, "%d%03d", id, value);
    if (sendCommand(cmd))
    {
        return true;
    }

    return false;
}

const char *WandererBoxProV3::getDefaultName()
{
    return "WandererBox Pro V3";
}


bool WandererBoxProV3::sendCommand(std::string command)
{
    int nbytes_written = 0, rc = -1;
    std::string command_termination = "\n";
    LOGF_DEBUG("CMD: %s", command.c_str());
    if ((rc = tty_write_string(PortFD, (command + command_termination).c_str(), &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }
    return true;
}

void WandererBoxProV3::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(2500);
        return;
    }

    getData();
    SetTimer(2500);
}

bool WandererBoxProV3::saveConfigItems(FILE * fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    WI::saveConfigItems(fp);

    dc5diffSP.save(fp);
    dc5diffSETNP.save(fp);
    dc5constSETNP.save(fp);
    dc5ControlNP.save(fp);

    dc6diffSP.save(fp);
    dc6diffSETNP.save(fp);
    dc6constSETNP.save(fp);
    dc6ControlNP.save(fp);

    dc7diffSP.save(fp);
    dc7diffSETNP.save(fp);
    dc7constSETNP.save(fp);
    dc7ControlNP.save(fp);

    setDC34voltageNP.save(fp);
    return true;
}

IPState WandererBoxProV3::updateWeather()
{
    // Weather is updated in updateENV() which is called from getData()
    // This method is required by WeatherInterface but we update weather
    // synchronously with sensor data, so we just return OK
    return IPS_OK;
}

