/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

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

// We declare an auto pointer to WandererBoxProV3.
static std::unique_ptr<WandererBoxProV3> wandererboxprov3(new WandererBoxProV3());



WandererBoxProV3::WandererBoxProV3()
{
    setVersion(1, 0);
}

bool WandererBoxProV3::initProperties()
{

    INDI::DefaultDevice::initProperties();
    setDriverInterface(AUX_INTERFACE);


    addAuxControls();


    // Calibrate
    IUFillSwitch(&CalibrateS[0], "Calibrate", "Calibrate Current", ISS_OFF);
    IUFillSwitchVector(&CalibrateSP, CalibrateS, 1, getDeviceName(), "Calibrate_DEVICE", "Calibrate Current", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,60, IPS_IDLE);

    // Power Monitor
    IUFillNumber(&PowerMonitorN[VOLTAGE], "VOLTAGE", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerMonitorN[TOTAL_CURRENT], "TOTAL_CURRENT", "Total Current (A)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerMonitorN[V19_CURRENT], "V19_CURRENT", "DC2 Current (A))", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerMonitorN[AR_CURRENT], "AR_CURRENT", "DC3-4 Current (A)", "%4.2f", 0, 999, 100, 0);
    IUFillNumberVector(&PowerMonitorNP, PowerMonitorN, 4, getDeviceName(), "POWER_Monitor", "Power Monitor",  MAIN_CONTROL_TAB, IP_RO,60, IPS_IDLE);



    // Firmware version
    IUFillText(&FirmwareT[0], "Version", "Version", nullptr);


    // USB31 Control
    IUFillSwitch(&usb31ControlS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&usb31ControlS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&usb31ControlSP, usb31ControlS, 2, getDeviceName(), "USB3.0_1", "USB3.0_1", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);


    // USB32 Control
    IUFillSwitch(&usb32ControlS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&usb32ControlS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&usb32ControlSP, usb32ControlS, 2, getDeviceName(), "USB3.0_2", "USB3.0_2", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);


    // USB33 Control
    IUFillSwitch(&usb33ControlS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&usb33ControlS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&usb33ControlSP, usb33ControlS, 2, getDeviceName(), "USB3.0_3", "USB3.0_3", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // USB21 Control
    IUFillSwitch(&usb21ControlS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&usb21ControlS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&usb21ControlSP, usb21ControlS, 2, getDeviceName(), "USB2.0_1-3", "USB2.0_1-3", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // USB22 Control
    IUFillSwitch(&usb22ControlS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&usb22ControlS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&usb22ControlSP, usb22ControlS, 2, getDeviceName(), "USB2.0_4-6", "USB2.0_4-6", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // DC567
    IUFillNumber(&dc5ControlN[DC5], "DC5", "Dew Heater (PWM)", "%.2f", 0, 255, 5, 0);
    IUFillNumberVector(&dc5ControlNP, dc5ControlN, 1, getDeviceName(), "PWM", "DC5", DC5_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&dc6ControlN[DC6], "DC6", "Dew Heater (PWM)", "%.2f", 0, 255, 5, 0);
    IUFillNumberVector(&dc6ControlNP, dc6ControlN, 1, getDeviceName(), "DC6", "DC6", DC6_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&dc7ControlN[DC7], "DC7", "Dew Heater (PWM)", "%.2f", 0, 255, 5, 0);
    IUFillNumberVector(&dc7ControlNP, dc7ControlN, 1, getDeviceName(), "DC7", "DC7", DC7_TAB, IP_RW, 60, IPS_IDLE);
    // DC34SET
    IUFillNumber(&setDC34voltageN[setDC34voltage], "DC34SET", "Adjustable Voltage", "%.2f", 5, 13.2, 0.1, 0);
    IUFillNumberVector(&setDC34voltageNP, setDC34voltageN, 1, getDeviceName(), "DC34voltageSET", "Set DC3-4", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    // DC3-4 Control
    IUFillSwitch(&dc3_4ControlS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&dc3_4ControlS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&dc3_4ControlSP, dc3_4ControlS, 2, getDeviceName(), "DC3-4", "DC3-4", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // DC8-9 Control
    IUFillSwitch(&dc8_9ControlS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&dc8_9ControlS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&dc8_9ControlSP, dc8_9ControlS, 2, getDeviceName(), "DC8-9", "DC8-9", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // DC10-11 Control
    IUFillSwitch(&dc10_11ControlS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&dc10_11ControlS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&dc10_11ControlSP, dc10_11ControlS, 2, getDeviceName(), "DC10-11", "DC10-11", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // DC5 TEMP Difference Control
    IUFillSwitch(&dc5diffS[DC5_Manual], "Manual", "Manual", ISS_ON);
    IUFillSwitch(&dc5diffS[DC5_DPD_Mode], "DPD_Mode", "DPD Mode", ISS_OFF);
    IUFillSwitch(&dc5diffS[DC5_CT_Mode], "CT_Mode", "CT Mode", ISS_OFF);
    IUFillSwitchVector(&dc5dewSP, dc5diffS, 3, getDeviceName(), "DC5_DIFF", "DC5 Dew Mode", DC5_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&dc5diffSETN[DC5DIFFSET], "DC5 Auto Control", "Dew Point Difference(C)", "%.2f", 10, 30, 1, 0);
    IUFillNumberVector(&dc5diffSETNP, dc5diffSETN, 1, getDeviceName(), "DC5_DIFF_SET", "DPD Mode", DC5_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&dc5constSETN[DC5CONSTSET], "DC5 Auto Control", "Temperature(C)", "%.2f", 0, 40, 1, 0);
    IUFillNumberVector(&dc5constSETNP, dc5constSETN, 1, getDeviceName(), "DC5_CONST_SET", "CT Mode", DC5_TAB, IP_RW, 60, IPS_IDLE);

    // DC6 TEMP Difference Control
    IUFillSwitch(&dc6diffS[DC6_Manual], "Manual", "Manual", ISS_ON);
    IUFillSwitch(&dc6diffS[DC6_DPD_Mode], "DPD_Mode", "DPD Mode", ISS_OFF);
    IUFillSwitch(&dc6diffS[DC6_CT_Mode], "CT_Mode", "CT Mode", ISS_OFF);
    IUFillSwitchVector(&dc6dewSP, dc6diffS, 3, getDeviceName(), "DC6_DIFF", "DC6 Dew Mode", DC6_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&dc6diffSETN[DC6DIFFSET], "DC6 Auto Control", "Dew Point Difference(C)", "%.2f", 10, 30, 1, 0);
    IUFillNumberVector(&dc6diffSETNP, dc6diffSETN, 1, getDeviceName(), "DC6_DIFF_SET", "DPD Mode", DC6_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&dc6constSETN[DC6CONSTSET], "DC6 Auto Control", "Temperature(C)", "%.2f", 0, 40, 1, 0);
    IUFillNumberVector(&dc6constSETNP, dc6constSETN, 1, getDeviceName(), "DC6_CONST_SET", "CT Mode", DC6_TAB, IP_RW, 60, IPS_IDLE);

    // DC7 TEMP Difference Control
    IUFillSwitch(&dc7diffS[DC7_Manual], "Manual", "Manual", ISS_ON);
    IUFillSwitch(&dc7diffS[DC7_DPD_Mode], "DPD_Mode", "DPD Mode", ISS_OFF);
    IUFillSwitch(&dc7diffS[DC7_CT_Mode], "CT_Mode", "CT Mode", ISS_OFF);
    IUFillSwitchVector(&dc7dewSP, dc7diffS, 3, getDeviceName(), "DC7_DIFF", "DC7 Dew Mode", DC7_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&dc7diffSETN[DC7DIFFSET], "DC7 Auto Control", "Dew Point Difference(C)", "%.2f", 10, 30, 1, 0);
    IUFillNumberVector(&dc7diffSETNP, dc7diffSETN, 1, getDeviceName(), "DC7_DIFF_SET", "DPD Mode", DC7_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&dc7constSETN[DC7CONSTSET], "DC7 Auto Control", "Temperature(C)", "%.2f", 0, 40, 1, 0);
    IUFillNumberVector(&dc7constSETNP, dc7constSETN, 1, getDeviceName(), "DC7_CONST_SET", "CT Mode", DC7_TAB, IP_RW, 60, IPS_IDLE);

    //ENV
    IUFillNumber(&ENVMonitorN[Probe1_Temp], "Probe1_Temp", "Probe1 Temperature (C)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&ENVMonitorN[Probe2_Temp], "Probe2_Temp", "Probe2 Temperature (C)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&ENVMonitorN[Probe3_Temp], "Probe3_Temp", "Probe3 Temperature (C)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&ENVMonitorN[ENV_Humidity], "ENV_Humidity", "Ambient Humidity %", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&ENVMonitorN[ENV_Temp], "ENV_Temp", "Ambient Temperature (C)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&ENVMonitorN[DEW_Point], "DEW_Point", "Dew Point (C)", "%4.2f", 0, 999, 100, 0);
    IUFillNumberVector(&ENVMonitorNP, ENVMonitorN, 6, getDeviceName(), "ENV_Monitor", "Environment",ENVIRONMENT_TAB, IP_RO,60, IPS_IDLE);
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
    version[64] = {0};
    tty_read_section(PortFD, version, 'A', 5, &nbytes_read_version);

    version[nbytes_read_version - 1] = '\0';
    IUSaveText(&FirmwareT[0], version);
    IDSetText(&FirmwareTP, nullptr);


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
    if(DC5DIFFMODE==true )
    {
        if(temp1read<ENVMonitorN[DEW_Point].value+dc5diffSETN[DC5DIFFSET].value)
            sendCommand("5255");
        else
            sendCommand("5000");
    }
    if(DC5CONSTMODE==true )
    {
        if(temp1read<dc5constSETN[DC5CONSTSET].value)
            sendCommand("5255");
        else
            sendCommand("5000");
    }
    if (dc5diffS[DC5_DPD_Mode].s == ISS_ON&&ENVMonitorN[Probe1_Temp].value==-127)
    {
        DC5DIFFMODE=false;
        DC5CONSTMODE=false;
        defineProperty(&dc5ControlNP);
        deleteProperty(dc5diffSETNP.name);
        deleteProperty(dc5constSETNP.name);
        LOGF_INFO("Temp probe 1 not connected, Dew Point Difference Mode for DC5 has exited!","Updated");
        LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
        dc5diffS[DC5_Manual].s = ISS_ON;
        dc5diffS[DC5_DPD_Mode].s = ISS_OFF;
        dc5diffS[DC5_CT_Mode].s = ISS_OFF;
        dc5dewSP.s = IPS_OK;
        IDSetSwitch(&dc5dewSP, nullptr);
    }
    if (dc5diffS[DC5_DPD_Mode].s == ISS_ON&&isnan(ENVMonitorN[DEW_Point].value)==1)
    {
        DC5DIFFMODE=false;
        DC5CONSTMODE=false;
        defineProperty(&dc5ControlNP);
        deleteProperty(dc5diffSETNP.name);
        deleteProperty(dc5constSETNP.name);
        LOGF_INFO("DHT22 Humidity&Temperature sensor not connected, Dew Point Difference Mode for DC5 has exited!","Updated");
        dc5diffS[DC5_Manual].s = ISS_ON;
        dc5diffS[DC5_DPD_Mode].s = ISS_OFF;
        dc5diffS[DC5_CT_Mode].s = ISS_OFF;
        dc5dewSP.s = IPS_OK;
        IDSetSwitch(&dc5dewSP, nullptr);
    }
    if (dc5diffS[DC5_CT_Mode].s == ISS_ON&&ENVMonitorN[Probe1_Temp].value==-127)
    {
        DC5DIFFMODE=false;
        DC5CONSTMODE=false;
        defineProperty(&dc5ControlNP);
        deleteProperty(dc5diffSETNP.name);
        deleteProperty(dc5constSETNP.name);
        LOGF_INFO("Temp probe 1 not connected, Constant Temperature Mode for DC5 has exited!","Updated");
                LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
        dc5diffS[DC5_Manual].s = ISS_ON;
        dc5diffS[DC5_DPD_Mode].s = ISS_OFF;
        dc5diffS[DC5_CT_Mode].s = ISS_OFF;
        dc5dewSP.s = IPS_OK;
        IDSetSwitch(&dc5dewSP, nullptr);
    }


    //DC6 DEW CONTROL
    if(DC6DIFFMODE==true )
    {
        if(temp2read<ENVMonitorN[DEW_Point].value+dc6diffSETN[DC6DIFFSET].value)
            sendCommand("6255");
        else
            sendCommand("6000");
    }
    if(DC6CONSTMODE==true )
    {
        if(temp2read<dc6constSETN[DC6CONSTSET].value)
            sendCommand("6255");
        else
            sendCommand("6000");
    }
    if (dc6diffS[DC6_DPD_Mode].s == ISS_ON&&ENVMonitorN[Probe2_Temp].value==-127)
    {
        DC6DIFFMODE=false;
        DC6CONSTMODE=false;
        defineProperty(&dc6ControlNP);
        deleteProperty(dc6diffSETNP.name);
        deleteProperty(dc6constSETNP.name);
        LOGF_INFO("Temp probe  not connected, Dew Point Difference Mode for DC6 has exited!","Updated");
        LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
        dc6diffS[DC6_Manual].s = ISS_ON;
        dc6diffS[DC6_DPD_Mode].s = ISS_OFF;
        dc6diffS[DC6_CT_Mode].s = ISS_OFF;
        dc6dewSP.s = IPS_OK;
        IDSetSwitch(&dc6dewSP, nullptr);
    }
    if (dc6diffS[DC6_DPD_Mode].s == ISS_ON&&isnan(ENVMonitorN[DEW_Point].value)==1)
    {
        DC6DIFFMODE=false;
        DC6CONSTMODE=false;
        defineProperty(&dc6ControlNP);
        deleteProperty(dc6diffSETNP.name);
        deleteProperty(dc6constSETNP.name);
        LOGF_INFO("DHT22 Humidity&Temperature sensor not connected, Dew Point Difference Mode for DC6 has exited!","Updated");
        dc6diffS[DC6_Manual].s = ISS_ON;
        dc6diffS[DC6_DPD_Mode].s = ISS_OFF;
        dc6diffS[DC6_CT_Mode].s = ISS_OFF;
        dc6dewSP.s = IPS_OK;
        IDSetSwitch(&dc6dewSP, nullptr);
    }
    if (dc6diffS[DC6_CT_Mode].s == ISS_ON&&ENVMonitorN[Probe2_Temp].value==-127)
    {
        DC6DIFFMODE=false;
        DC6CONSTMODE=false;
        defineProperty(&dc6ControlNP);
        deleteProperty(dc6diffSETNP.name);
        deleteProperty(dc6constSETNP.name);
        LOGF_INFO("Temp probe 2 not connected, Constant Temperature Mode for DC6 has exited!","Updated");
        LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
        dc6diffS[DC6_Manual].s = ISS_ON;
        dc6diffS[DC6_DPD_Mode].s = ISS_OFF;
        dc6diffS[DC6_CT_Mode].s = ISS_OFF;
        dc6dewSP.s = IPS_OK;
        IDSetSwitch(&dc6dewSP, nullptr);
    }


    //DC7 DEW CONTROL
    if(DC7DIFFMODE==true )
    {
        if(temp3read<ENVMonitorN[DEW_Point].value+dc7diffSETN[DC7DIFFSET].value)
            sendCommand("7255");
        else
            sendCommand("7000");
    }
    if(DC7CONSTMODE==true )
    {
        if(temp3read<dc7constSETN[DC7CONSTSET].value)
            sendCommand("7255");
        else
            sendCommand("7000");
    }
    if (dc7diffS[DC7_DPD_Mode].s == ISS_ON&&ENVMonitorN[Probe3_Temp].value==-127)
    {
        DC7DIFFMODE=false;
        DC7CONSTMODE=false;
        defineProperty(&dc7ControlNP);
        deleteProperty(dc7diffSETNP.name);
        deleteProperty(dc7constSETNP.name);
        LOGF_INFO("Temp probe 3 not connected, Dew Point Difference Mode for DC7 has exited!","Updated");
        LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
        dc7diffS[DC7_Manual].s = ISS_ON;
        dc7diffS[DC7_DPD_Mode].s = ISS_OFF;
        dc7diffS[DC7_CT_Mode].s = ISS_OFF;
        dc7dewSP.s = IPS_OK;
        IDSetSwitch(&dc7dewSP, nullptr);
    }
    if (dc7diffS[DC7_DPD_Mode].s == ISS_ON&&isnan(ENVMonitorN[DEW_Point].value)==1)
    {
        DC7DIFFMODE=false;
        DC7CONSTMODE=false;
        defineProperty(&dc7ControlNP);
        deleteProperty(dc7diffSETNP.name);
        deleteProperty(dc7constSETNP.name);
        LOGF_INFO("DHT22 Humidity&Temperature sensor not connected, Dew Point Difference Mode for DC7 has exited!","Updated");
        dc7diffS[DC7_Manual].s = ISS_ON;
        dc7diffS[DC7_DPD_Mode].s = ISS_OFF;
        dc7diffS[DC7_CT_Mode].s = ISS_OFF;
        dc7dewSP.s = IPS_OK;
        IDSetSwitch(&dc7dewSP, nullptr);
    }
    if (dc7diffS[DC7_CT_Mode].s == ISS_ON&&ENVMonitorN[Probe3_Temp].value==-127)
    {
        DC7DIFFMODE=false;
        DC7CONSTMODE=false;
        defineProperty(&dc7ControlNP);
        deleteProperty(dc7diffSETNP.name);
        deleteProperty(dc7constSETNP.name);
        LOGF_INFO("Temp probe 3 not connected, Constant Temperature Mode for DC7 has exited!","Updated");
        LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
        dc7diffS[DC7_Manual].s = ISS_ON;
        dc7diffS[DC7_DPD_Mode].s = ISS_OFF;
        dc7diffS[DC7_CT_Mode].s = ISS_OFF;
        dc7dewSP.s = IPS_OK;
        IDSetSwitch(&dc7dewSP, nullptr);
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
    ENVMonitorN[Probe1_Temp].value = temp1;
    ENVMonitorN[Probe2_Temp].value = temp2;
    ENVMonitorN[Probe3_Temp].value = temp3;
    ENVMonitorN[ENV_Humidity].value = DHTH;
    ENVMonitorN[ENV_Temp].value = DHTT;
    ENVMonitorN[DEW_Point].value = (237.7 * ((17.27 * DHTT) / (237.7 + DHTT) + log((DHTH / 100)))) / (17.27 - ((17.27 * DHTT) / (237.7 + DHTT) + log((DHTH / 100))));;
    ENVMonitorNP.s = IPS_OK;
    IDSetNumber(&ENVMonitorNP, nullptr);

}


void WandererBoxProV3::updatePower(double Tcurrent,double V19_current,double AR_current,double voltage)
{
    // Power Monitor
    PowerMonitorN[VOLTAGE].value = voltage;
    PowerMonitorN[TOTAL_CURRENT].value = Tcurrent;
    PowerMonitorN[V19_CURRENT].value = V19_current;
    PowerMonitorN[AR_CURRENT].value = AR_current;
    PowerMonitorNP.s = IPS_OK;
    IDSetNumber(&PowerMonitorNP, nullptr);
}

void WandererBoxProV3::updateUSB31(int res)
{
    usb31ControlS[INDI_ENABLED].s = (res== 1) ? ISS_ON : ISS_OFF;
    usb31ControlS[INDI_DISABLED].s = (res== 0) ? ISS_ON : ISS_OFF;
    usb31ControlSP.s = (res== 1) ? IPS_OK : IPS_IDLE;
    IDSetSwitch(&usb31ControlSP, nullptr);
}

void WandererBoxProV3::updateUSB32(int res)
{
    usb32ControlS[INDI_ENABLED].s = (res== 1) ? ISS_ON : ISS_OFF;
    usb32ControlS[INDI_DISABLED].s = (res== 0) ? ISS_ON : ISS_OFF;
    usb32ControlSP.s = (res== 1) ? IPS_OK : IPS_IDLE;
    IDSetSwitch(&usb32ControlSP, nullptr);
}

void WandererBoxProV3::updateUSB33(int res)
{
    usb33ControlS[INDI_ENABLED].s = (res== 1) ? ISS_ON : ISS_OFF;
    usb33ControlS[INDI_DISABLED].s = (res== 0) ? ISS_ON : ISS_OFF;
    usb33ControlSP.s = (res== 1) ? IPS_OK : IPS_IDLE;
    IDSetSwitch(&usb33ControlSP, nullptr);
}

void WandererBoxProV3::updateUSB21(int res)
{
    usb21ControlS[INDI_ENABLED].s = (res== 1) ? ISS_ON : ISS_OFF;
    usb21ControlS[INDI_DISABLED].s = (res== 0) ? ISS_ON : ISS_OFF;
    usb21ControlSP.s = (res== 1) ? IPS_OK : IPS_IDLE;
    IDSetSwitch(&usb21ControlSP, nullptr);
}

void WandererBoxProV3::updateUSB22(int res)
{
    usb22ControlS[INDI_ENABLED].s = (res== 1) ? ISS_ON : ISS_OFF;
    usb22ControlS[INDI_DISABLED].s = (res== 0) ? ISS_ON : ISS_OFF;
    usb22ControlSP.s = (res== 1) ? IPS_OK : IPS_IDLE;
    IDSetSwitch(&usb22ControlSP, nullptr);
}

void WandererBoxProV3::updateDC34(int res)
{
    dc3_4ControlS[INDI_ENABLED].s = (res== 1) ? ISS_ON : ISS_OFF;
    dc3_4ControlS[INDI_DISABLED].s = (res== 0) ? ISS_ON : ISS_OFF;
    dc3_4ControlSP.s = (res== 1) ? IPS_OK : IPS_IDLE;
    IDSetSwitch(&dc3_4ControlSP, nullptr);
}

void WandererBoxProV3::updateDC5(int res)
{
    dc5ControlN[0].value = res;
    dc5ControlNP.s = IPS_OK;
    IDSetNumber(&dc5ControlNP, nullptr);
}

void WandererBoxProV3::updateDC6(int res)
{
    dc6ControlN[0].value = res;
    dc6ControlNP.s = IPS_OK;
    IDSetNumber(&dc6ControlNP, nullptr);
}

void WandererBoxProV3::updateDC7(int res)
{
    dc7ControlN[0].value = res;
    dc7ControlNP.s = IPS_OK;
    IDSetNumber(&dc7ControlNP, nullptr);
}

void WandererBoxProV3::updateDC8_9(int res)
{
    dc8_9ControlS[INDI_ENABLED].s = (res== 1) ? ISS_ON : ISS_OFF;
    dc8_9ControlS[INDI_DISABLED].s = (res== 0) ? ISS_ON : ISS_OFF;
    dc8_9ControlSP.s = (res== 1) ? IPS_OK : IPS_IDLE;
    IDSetSwitch(&dc8_9ControlSP, nullptr);
}

void WandererBoxProV3::updateDC10_11(int res)
{
    dc10_11ControlS[INDI_ENABLED].s = (res== 1) ? ISS_ON : ISS_OFF;
    dc10_11ControlS[INDI_DISABLED].s = (res== 0) ? ISS_ON : ISS_OFF;
    dc10_11ControlSP.s = (res== 1) ? IPS_OK : IPS_IDLE;
    IDSetSwitch(&dc10_11ControlSP, nullptr);
}

void WandererBoxProV3::updateDC34SET(double res)
{
    setDC34voltageN[setDC34voltage].value = res/10.0;
    setDC34voltageNP.s = IPS_OK;
    IDSetNumber(&setDC34voltageNP, nullptr);
}



bool WandererBoxProV3::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {

        defineProperty(&FirmwareTP);
        if(std::stoi(version)>=20240216)
        {
            defineProperty(&CalibrateSP);
        }
        else
        {
            LOGF_INFO("The firmware is outdated, please upgrade to the latest firmware, or power reading calibration will be unavailable.","failed");
        }
        defineProperty(&PowerMonitorNP);


        defineProperty(&usb31ControlSP);
        defineProperty(&usb32ControlSP);
        defineProperty(&usb33ControlSP);
        defineProperty(&usb21ControlSP);
        defineProperty(&usb22ControlSP);

        defineProperty(&setDC34voltageNP);
        defineProperty(&dc3_4ControlSP);

        defineProperty(&dc8_9ControlSP);
        defineProperty(&dc10_11ControlSP);

        defineProperty(&dc5dewSP);
        defineProperty(&dc6dewSP);
        defineProperty(&dc7dewSP);


            //DC5////////////////////
        if(dc5diffS[DC5_DPD_Mode].s == ISS_ON)
        {
            deleteProperty(dc5constSETNP.name);
            deleteProperty(dc5ControlNP.name);
            defineProperty(&dc5diffSETNP);
        }
        else if(dc5diffS[DC5_CT_Mode].s == ISS_ON)
        {
            deleteProperty(dc5ControlNP.name);
            deleteProperty(dc5diffSETNP.name);
            defineProperty(&dc5constSETNP);
        }
        else
        {
            defineProperty(&dc5ControlNP);
            deleteProperty(dc5diffSETNP.name);
            deleteProperty(dc5constSETNP.name);
        }
        //DC6////////////////
        if(dc6diffS[DC6_DPD_Mode].s == ISS_ON)
        {
            deleteProperty(dc6constSETNP.name);
            deleteProperty(dc6ControlNP.name);
            defineProperty(&dc6diffSETNP);
        }
        else if(dc6diffS[DC6_CT_Mode].s == ISS_ON)
        {
            deleteProperty(dc6ControlNP.name);
            deleteProperty(dc6diffSETNP.name);
            defineProperty(&dc6constSETNP);
        }
        else
        {
            defineProperty(&dc6ControlNP);
            deleteProperty(dc6diffSETNP.name);
            deleteProperty(dc6constSETNP.name);
        }
        //DC7////////////////
        if(dc7diffS[DC7_DPD_Mode].s == ISS_ON)
        {
            deleteProperty(dc7constSETNP.name);
            deleteProperty(dc7ControlNP.name);
            defineProperty(&dc7diffSETNP);
        }
        else if(dc7diffS[DC7_CT_Mode].s == ISS_ON)
        {
            deleteProperty(dc7ControlNP.name);
            deleteProperty(dc7diffSETNP.name);
            defineProperty(&dc7constSETNP);
        }
        else
        {
            defineProperty(&dc7ControlNP);
            deleteProperty(dc7diffSETNP.name);
            deleteProperty(dc7constSETNP.name);
        }


        defineProperty(&ENVMonitorNP);


    }
    else
    {

        deleteProperty(FirmwareTP.name);
        deleteProperty(CalibrateSP.name);
        deleteProperty(PowerMonitorNP.name);
        deleteProperty(ENVMonitorNP.name);

        deleteProperty(dc3_4ControlSP.name);
        deleteProperty(setDC34voltageNP.name);
        deleteProperty(dc8_9ControlSP.name);
        deleteProperty(dc10_11ControlSP.name);
        deleteProperty(usb31ControlSP.name);
        deleteProperty(usb32ControlSP.name);
        deleteProperty(usb33ControlSP.name);
        deleteProperty(usb21ControlSP.name);
        deleteProperty(usb22ControlSP.name);

        deleteProperty(dc5ControlNP.name);
        deleteProperty(dc6ControlNP.name);
        deleteProperty(dc7ControlNP.name);

        deleteProperty(dc5dewSP.name);
        deleteProperty(dc5diffSETNP.name);

        deleteProperty(dc6dewSP.name);
        deleteProperty(dc6diffSETNP.name);

        deleteProperty(dc7dewSP.name);
        deleteProperty(dc7diffSETNP.name);


    }
    return true;
}

bool WandererBoxProV3::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Calibrate
    if (!strcmp(name, CalibrateSP.name))
    {
        CalibrateSP.s = sendCommand("66300744") ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&CalibrateSP, nullptr);
        LOG_INFO("Calibrating Current Readings...");
        return true;
    }


    // DC3-4 Control
    if (!strcmp(name, dc3_4ControlSP.name))
    {
        IUUpdateSwitch(&dc3_4ControlSP, states, names, n);
        dc3_4ControlSP.s = IPS_ALERT;
        char cmd[128] = {0};
        snprintf(cmd, 128, "10%d", (dc3_4ControlS[INDI_ENABLED].s == ISS_ON) ? 1 : 0);
        dc3_4ControlSP.s = sendCommand(cmd) ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&dc3_4ControlSP, nullptr);

        return true;
    }
    // DC8-9 Control
    if (!strcmp(name, dc8_9ControlSP.name))
    {
        IUUpdateSwitch(&dc8_9ControlSP, states, names, n);
        dc8_9ControlSP.s = IPS_ALERT;
        char cmd[128] = {0};
        snprintf(cmd, 128, "20%d", (dc8_9ControlS[INDI_ENABLED].s == ISS_ON) ? 1 : 0);
        dc8_9ControlSP.s = sendCommand(cmd) ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&dc8_9ControlSP, nullptr);

        return true;
    }
    // DC10-11 Control
    if (!strcmp(name, dc10_11ControlSP.name))
    {
        IUUpdateSwitch(&dc10_11ControlSP, states, names, n);
        dc10_11ControlSP.s = IPS_ALERT;
        char cmd[128] = {0};
        snprintf(cmd, 128, "21%d", (dc10_11ControlS[INDI_ENABLED].s == ISS_ON) ? 1 : 0);
        dc10_11ControlSP.s = sendCommand(cmd) ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&dc10_11ControlSP, nullptr);

        return true;
    }

    // USB31 Control
    if (!strcmp(name, usb31ControlSP.name))
    {
        IUUpdateSwitch(&usb31ControlSP, states, names, n);
        usb31ControlSP.s = IPS_ALERT;
        char cmd[128] = {0};
        snprintf(cmd, 128, "11%d", (usb31ControlS[INDI_ENABLED].s == ISS_ON) ? 1 : 0);
        usb31ControlSP.s = sendCommand(cmd) ? IPS_OK : IPS_ALERT;

        return true;
    }
    // USB32 Control
    if (!strcmp(name, usb32ControlSP.name))
    {
        IUUpdateSwitch(&usb32ControlSP, states, names, n);
        usb32ControlSP.s = IPS_ALERT;
        char cmd[128] = {0};
        snprintf(cmd, 128, "12%d", (usb32ControlS[INDI_ENABLED].s == ISS_ON) ? 1 : 0);
        usb32ControlSP.s = sendCommand(cmd) ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&usb32ControlSP, nullptr);

        return true;
    }
    // USB33 Control
    if (!strcmp(name, usb33ControlSP.name))
    {
        IUUpdateSwitch(&usb33ControlSP, states, names, n);
        usb33ControlSP.s = IPS_ALERT;
        char cmd[128] = {0};
        snprintf(cmd, 128, "13%d", (usb33ControlS[INDI_ENABLED].s == ISS_ON) ? 1 : 0);
        usb33ControlSP.s = sendCommand(cmd) ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&usb33ControlSP, nullptr);

        return true;
    }
    // USB21 Control
    if (!strcmp(name, usb21ControlSP.name))
    {
        IUUpdateSwitch(&usb21ControlSP, states, names, n);
        usb21ControlSP.s = IPS_ALERT;
        char cmd[128] = {0};
        snprintf(cmd, 128, "14%d", (usb21ControlS[INDI_ENABLED].s == ISS_ON) ? 1 : 0);
        usb21ControlSP.s = sendCommand(cmd) ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&usb21ControlSP, nullptr);

        return true;
    }
    // USB22 Control
    if (!strcmp(name, usb22ControlSP.name))
    {
        IUUpdateSwitch(&usb22ControlSP, states, names, n);
        usb22ControlSP.s = IPS_ALERT;
        char cmd[128] = {0};
        snprintf(cmd, 128, "15%d", (usb22ControlS[INDI_ENABLED].s == ISS_ON) ? 1 : 0);
        usb22ControlSP.s = sendCommand(cmd) ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&usb22ControlSP, nullptr);

        return true;
    }

    // DC5 DEW
    if (!strcmp(name, dc5dewSP.name))
    {
        IUUpdateSwitch(&dc5dewSP, states, names, n);
        dc5dewSP.s = IPS_ALERT;
        if(dc5diffS[DC5_DPD_Mode].s == ISS_ON&&ENVMonitorN[Probe1_Temp].value!=-127&&isnan(ENVMonitorN[DEW_Point].value)==0)
        {
            DC5DIFFMODE=true;
            DC5CONSTMODE=false;
            deleteProperty(dc5ControlNP.name);
            deleteProperty(dc5constSETNP.name);
            defineProperty(&dc5diffSETNP);

            dc5diffSETNP.s = IPS_OK;
            IDSetNumber(&dc5diffSETNP, nullptr);
            dc5dewSP.s = IPS_OK ;
            IDSetSwitch(&dc5dewSP, nullptr);
            LOGF_INFO("Dew Point Difference Mode for DC5 activated! WandererBox will keep the dew heater at the temperature higher than the dew point by the set value.","Updated");
            return true;
        }
        else if(dc5diffS[DC5_DPD_Mode].s == ISS_ON&&(ENVMonitorN[Probe1_Temp].value==-127||isnan(ENVMonitorN[DEW_Point].value)==1))
        {
            DC5DIFFMODE=false;
            DC5CONSTMODE=false;
            dc5diffS[DC5_Manual].s = ISS_ON;
                        LOGF_INFO("Manual Mode for DC5 activated! Please adjust the duty cycle manually, you can also use DC5 as an ordinary switch.","Updated");
            IDSetSwitch(&dc5dewSP, nullptr);
        }
        else if(dc5diffS[DC5_CT_Mode].s == ISS_ON&&ENVMonitorN[Probe1_Temp].value!=-127)
        {
            DC5CONSTMODE=true;
            DC5DIFFMODE=false;
            deleteProperty(dc5diffSETNP.name);
            deleteProperty(dc5ControlNP.name);
            defineProperty(&dc5constSETNP);

            dc5constSETNP.s = IPS_OK;
            IDSetNumber(&dc5constSETNP, nullptr);
            dc5dewSP.s = IPS_OK ;
            IDSetSwitch(&dc5dewSP, nullptr);
            LOGF_INFO("Constant Temperature Mode for DC5 activated! WandererBox will keep the dew heater at the set temperature.","Updated");
            return true;
        }
        else if(dc5diffS[DC5_CT_Mode].s == ISS_ON&&ENVMonitorN[Probe1_Temp].value==-127)
        {
            DC5DIFFMODE=false;
            DC5CONSTMODE=false;
            dc5diffS[DC5_Manual].s = ISS_ON;
            LOGF_INFO("Manual Mode for DC5 activated! Please adjust the duty cycle manually, you can also use DC5 as an ordinary switch.","Updated");
            IDSetSwitch(&dc5dewSP, nullptr);
        }
        else
        {
            DC5DIFFMODE=false;
            DC5CONSTMODE=false;
            defineProperty(&dc5ControlNP);
            deleteProperty(dc5diffSETNP.name);
            deleteProperty(dc5constSETNP.name);
            dc5dewSP.s = IPS_OK ;
            IDSetSwitch(&dc5dewSP, nullptr);
            LOGF_INFO("Manual Mode for DC5 activated! Please adjust the duty cycle manually, you can also use DC5 as an ordinary switch.","Updated");
            return true;
        }

    }

    // DC6 DEW
    if (!strcmp(name, dc6dewSP.name))
    {
        IUUpdateSwitch(&dc6dewSP, states, names, n);
        dc6dewSP.s = IPS_ALERT;
        if(dc6diffS[DC6_DPD_Mode].s == ISS_ON&&ENVMonitorN[Probe2_Temp].value!=-127&&isnan(ENVMonitorN[DEW_Point].value)==0)
        {
            DC6DIFFMODE=true;
            DC6CONSTMODE=false;
            deleteProperty(dc6ControlNP.name);
            deleteProperty(dc6constSETNP.name);
            defineProperty(&dc6diffSETNP);

            dc6diffSETNP.s = IPS_OK;
            IDSetNumber(&dc6diffSETNP, nullptr);
            dc6dewSP.s = IPS_OK ;
            IDSetSwitch(&dc6dewSP, nullptr);
            LOGF_INFO("Dew Point Difference Mode for DC6 activated! WandererBox will keep the dew heater at the temperature higher than the dew point by the set value.","Updated");
            return true;
        }
        else if(dc6diffS[DC6_DPD_Mode].s == ISS_ON&&(ENVMonitorN[Probe2_Temp].value==-127||isnan(ENVMonitorN[DEW_Point].value)==1))
        {
            DC6DIFFMODE=false;
            DC6CONSTMODE=false;
            dc6diffS[DC6_Manual].s = ISS_ON;
            LOGF_INFO("Manual Mode for DC6 activated! Please adjust the duty cycle manually, you can also use DC6 as an ordinary switch.","Updated");
            IDSetSwitch(&dc6dewSP, nullptr);
        }
        else if(dc6diffS[DC6_CT_Mode].s == ISS_ON&&ENVMonitorN[Probe2_Temp].value!=-127)
        {
            DC6CONSTMODE=true;
            DC6DIFFMODE=false;
            deleteProperty(dc6diffSETNP.name);
            deleteProperty(dc6ControlNP.name);
            defineProperty(&dc6constSETNP);

            dc6constSETNP.s = IPS_OK;
            IDSetNumber(&dc6constSETNP, nullptr);
            dc6dewSP.s = IPS_OK ;
            IDSetSwitch(&dc6dewSP, nullptr);
            LOGF_INFO("Constant Temperature Mode for DC6 activated! WandererBox will keep the dew heater at the set temperature.","Updated");
            return true;
        }
        else if(dc6diffS[DC6_CT_Mode].s == ISS_ON&&ENVMonitorN[Probe2_Temp].value==-127)
        {
            DC6DIFFMODE=false;
            DC6CONSTMODE=false;
            dc6diffS[DC6_Manual].s = ISS_ON;
            LOGF_INFO("Manual Mode for DC6 activated! Please adjust the duty cycle manually, you can also use DC6 as an ordinary switch.","Updated");
            IDSetSwitch(&dc6dewSP, nullptr);
        }
        else
        {
            DC6DIFFMODE=false;
            DC6CONSTMODE=false;
            defineProperty(&dc6ControlNP);
            deleteProperty(dc6diffSETNP.name);
            deleteProperty(dc6constSETNP.name);
            dc6dewSP.s = IPS_OK ;
            IDSetSwitch(&dc6dewSP, nullptr);
            LOGF_INFO("Manual Mode for DC6 activated! Please adjust the duty cycle manually, you can also use DC6 as an ordinary switch.","Updated");
            return true;
        }

    }

    // DC7 DEW
    if (!strcmp(name, dc7dewSP.name))
    {
        IUUpdateSwitch(&dc7dewSP, states, names, n);
        dc7dewSP.s = IPS_ALERT;
        if(dc7diffS[DC7_DPD_Mode].s == ISS_ON&&ENVMonitorN[Probe3_Temp].value!=-127&&isnan(ENVMonitorN[DEW_Point].value)==0)
        {
            DC7DIFFMODE=true;
            DC7CONSTMODE=false;
            deleteProperty(dc7ControlNP.name);
            deleteProperty(dc7constSETNP.name);
            defineProperty(&dc7diffSETNP);

            dc7diffSETNP.s = IPS_OK;
            IDSetNumber(&dc7diffSETNP, nullptr);
            dc7dewSP.s = IPS_OK ;
            IDSetSwitch(&dc7dewSP, nullptr);
            LOGF_INFO("Dew Point Difference Mode for DC7 activated! WandererBox will keep the dew heater at the temperature higher than the dew point by the set value.","Updated");
            return true;
        }
        else if(dc7diffS[DC7_DPD_Mode].s == ISS_ON&&(ENVMonitorN[Probe3_Temp].value==-127||isnan(ENVMonitorN[DEW_Point].value)==1))
        {
            DC7DIFFMODE=false;
            DC7CONSTMODE=false;
            dc7diffS[DC7_Manual].s = ISS_ON;
            LOGF_INFO("Manual Mode for DC7 activated! Please adjust the duty cycle manually, you can also use DC7 as an ordinary switch.","Updated");
            IDSetSwitch(&dc7dewSP, nullptr);
        }
        else if(dc7diffS[DC7_CT_Mode].s == ISS_ON&&ENVMonitorN[Probe3_Temp].value!=-127)
        {
            DC7CONSTMODE=true;
            DC7DIFFMODE=false;
            deleteProperty(dc7diffSETNP.name);
            deleteProperty(dc7ControlNP.name);
            defineProperty(&dc7constSETNP);

            dc7constSETNP.s = IPS_OK;
            IDSetNumber(&dc7constSETNP, nullptr);
            dc7dewSP.s = IPS_OK ;
            IDSetSwitch(&dc7dewSP, nullptr);
            LOGF_INFO("Constant Temperature Mode for DC7 activated! WandererBox will keep the dew heater at the set temperature.","Updated");
            return true;
        }
        else if(dc7diffS[DC7_CT_Mode].s == ISS_ON&&ENVMonitorN[Probe3_Temp].value==-127)
        {
            DC7DIFFMODE=false;
            DC7CONSTMODE=false;
            dc7diffS[DC7_Manual].s = ISS_ON;
            LOGF_INFO("Manual Mode for DC7 activated! Please adjust the duty cycle manually, you can also use DC7 as an ordinary switch.","Updated");
            IDSetSwitch(&dc7dewSP, nullptr);
        }
        else
        {
            DC7DIFFMODE=false;
            DC7CONSTMODE=false;
            defineProperty(&dc7ControlNP);
            deleteProperty(dc7diffSETNP.name);
            deleteProperty(dc7constSETNP.name);
            dc7dewSP.s = IPS_OK ;
            IDSetSwitch(&dc7dewSP, nullptr);
            LOGF_INFO("Manual Mode for DC7 activated! Please adjust the duty cycle manually, you can also use DC7 as an ordinary switch.","Updated");
            return true;
        }

    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool WandererBoxProV3::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // DC5
        if (!strcmp(name, dc5ControlNP.name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], dc5ControlN[DC5].name))
                    rc1 = setDewPWM(5, static_cast<uint8_t>(values[i]));
            }

            dc5ControlNP.s = (rc1) ? IPS_OK : IPS_ALERT;
            if (dc5ControlNP.s == IPS_OK)
                IUUpdateNumber(&dc5ControlNP, values, names, n);
            IDSetNumber(&dc5ControlNP, nullptr);
            return true;
        }
        if (!strcmp(name, dc5diffSETNP.name))
        {

            dc5diffSETNP.s =  IPS_OK ;
            if (dc5diffSETNP.s == IPS_OK)
                IUUpdateNumber(&dc5diffSETNP, values, names, n);
            IDSetNumber(&dc5diffSETNP, nullptr);
            return true;
        }
        if (!strcmp(name, dc5constSETNP.name))
        {

            dc5constSETNP.s =  IPS_OK ;
            if (dc5constSETNP.s == IPS_OK)
                IUUpdateNumber(&dc5constSETNP, values, names, n);
            IDSetNumber(&dc5constSETNP, nullptr);
            return true;
        }
        // DC6
        if (!strcmp(name, dc6ControlNP.name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], dc6ControlN[DC6].name))
                    rc1 = setDewPWM(6, static_cast<uint8_t>(values[i]));
            }

            dc6ControlNP.s = (rc1) ? IPS_OK : IPS_ALERT;
            if (dc6ControlNP.s == IPS_OK)
                IUUpdateNumber(&dc6ControlNP, values, names, n);
            IDSetNumber(&dc6ControlNP, nullptr);
            return true;
        }
        if (!strcmp(name, dc6diffSETNP.name))
        {

            dc6diffSETNP.s =  IPS_OK ;
            if (dc6diffSETNP.s == IPS_OK)
                IUUpdateNumber(&dc6diffSETNP, values, names, n);
            IDSetNumber(&dc6diffSETNP, nullptr);
            return true;
        }
        if (!strcmp(name, dc6constSETNP.name))
        {

            dc6constSETNP.s =  IPS_OK ;
            if (dc6constSETNP.s == IPS_OK)
                IUUpdateNumber(&dc6constSETNP, values, names, n);
            IDSetNumber(&dc6constSETNP, nullptr);
            return true;
        }
        // DC7
        if (!strcmp(name, dc7ControlNP.name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], dc7ControlN[DC6].name))
                    rc1 = setDewPWM(7, static_cast<uint8_t>(values[i]));
            }

            dc7ControlNP.s = (rc1) ? IPS_OK : IPS_ALERT;
            if (dc7ControlNP.s == IPS_OK)
                IUUpdateNumber(&dc7ControlNP, values, names, n);
            IDSetNumber(&dc7ControlNP, nullptr);
            return true;
        }
        if (!strcmp(name, dc7diffSETNP.name))
        {

            dc7diffSETNP.s =  IPS_OK ;
            if (dc7diffSETNP.s == IPS_OK)
                IUUpdateNumber(&dc7diffSETNP, values, names, n);
            IDSetNumber(&dc7diffSETNP, nullptr);
            return true;
        }
        if (!strcmp(name, dc7constSETNP.name))
        {

            dc7constSETNP.s =  IPS_OK ;
            if (dc7constSETNP.s == IPS_OK)
                IUUpdateNumber(&dc7constSETNP, values, names, n);
            IDSetNumber(&dc7constSETNP, nullptr);
            return true;
        }
        // DC34voltageSET
        if (!strcmp(name, setDC34voltageNP.name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], setDC34voltageN[setDC34voltage].name))
                    rc1 = setDewPWM(20, static_cast<uint8_t>(10*values[i]));
            }

            setDC34voltageNP.s = (rc1) ? IPS_OK : IPS_ALERT;
            if (setDC34voltageNP.s == IPS_OK)
                IUUpdateNumber(&setDC34voltageNP, values, names, n);
            IDSetNumber(&setDC34voltageNP, nullptr);
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

    IUSaveConfigSwitch(fp, &dc5dewSP);
    IUSaveConfigNumber(fp, &dc5diffSETNP);
    IUSaveConfigNumber(fp, &dc5constSETNP);
    IUSaveConfigNumber(fp, &dc5ControlNP);

    IUSaveConfigSwitch(fp, &dc6dewSP);
    IUSaveConfigNumber(fp, &dc6diffSETNP);
    IUSaveConfigNumber(fp, &dc6constSETNP);
    IUSaveConfigNumber(fp, &dc6ControlNP);

    IUSaveConfigSwitch(fp, &dc7dewSP);
    IUSaveConfigNumber(fp, &dc7diffSETNP);
    IUSaveConfigNumber(fp, &dc7constSETNP);
    IUSaveConfigNumber(fp, &dc7ControlNP);

    IUSaveConfigNumber(fp, &setDC34voltageNP);
    return true;
}


