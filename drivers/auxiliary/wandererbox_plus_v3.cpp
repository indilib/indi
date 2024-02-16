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

#include "wandererbox_plus_v3.h"
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

// We declare an auto pointer to WandererBoxPlusV3.
static std::unique_ptr<WandererBoxPlusV3> wandererboxplusv3(new WandererBoxPlusV3());



WandererBoxPlusV3::WandererBoxPlusV3()
{
    setVersion(1, 0);
}

bool WandererBoxPlusV3::initProperties()
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
    IUFillNumberVector(&PowerMonitorNP, PowerMonitorN, 2, getDeviceName(), "POWER_Monitor", "Power Monitor",  MAIN_CONTROL_TAB, IP_RO,60, IPS_IDLE);



    // Firmware version
    IUFillText(&FirmwareT[0], "Version", "Version", nullptr);


    // USB Control
    IUFillSwitch(&USBControlS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&USBControlS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&USBControlSP, USBControlS, 2, getDeviceName(), "USB3.0_1", "USB3.0_1", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);


    // DC3
    IUFillNumber(&DC3ControlN[DC3], "DC3", "Dew Heater (PWM)", "%.2f", 0, 255, 5, 0);
    IUFillNumberVector(&DC3ControlNP, DC3ControlN, 1, getDeviceName(), "PWM", "DC3", DC3_TAB, IP_RW, 60, IPS_IDLE);

      // DC2SET
    IUFillNumber(&setDC2voltageN[setDC2voltage], "DC2SET", "Adjustable Voltage", "%.2f", 5, 13.2, 0.1, 0);
    IUFillNumberVector(&setDC2voltageNP, setDC2voltageN, 1, getDeviceName(), "DC2voltageSET", "Set DC2", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    // DC2 Control
    IUFillSwitch(&DC2ControlS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&DC2ControlS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&DC2ControlSP, DC2ControlS, 2, getDeviceName(), "DC2", "DC2", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // DC4-6 Control
    IUFillSwitch(&DC4_6ControlS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&DC4_6ControlS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&DC4_6ControlSP, DC4_6ControlS, 2, getDeviceName(), "DC4-6", "DC4-6", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // DC3 TEMP Difference Control
    IUFillSwitch(&DC3diffS[DC3_Manual], "Manual", "Manual", ISS_ON);
    IUFillSwitch(&DC3diffS[DC3_DPD_Mode], "DPD_Mode", "DPD Mode", ISS_OFF);
    IUFillSwitch(&DC3diffS[DC3_CT_Mode], "CT_Mode", "CT Mode", ISS_OFF);
    IUFillSwitchVector(&DC3dewSP, DC3diffS, 3, getDeviceName(), "DC3_DIFF", "DC3 Dew Mode", DC3_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&DC3diffSETN[DC3DIFFSET], "DC3 Auto Control", "Dew Point Difference(C)", "%.2f", 10, 30, 1, 0);
    IUFillNumberVector(&DC3diffSETNP, DC3diffSETN, 1, getDeviceName(), "DC3_DIFF_SET", "DPD Mode", DC3_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&DC3constSETN[DC3CONSTSET], "DC3 Auto Control", "Temperature(C)", "%.2f", 0, 40, 1, 0);
    IUFillNumberVector(&DC3constSETNP, DC3constSETN, 1, getDeviceName(), "DC3_CONST_SET", "CT Mode", DC3_TAB, IP_RW, 60, IPS_IDLE);

    //ENV
    IUFillNumber(&ENVMonitorN[Probe1_Temp], "Probe1_Temp", "Probe1 Temperature (C)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&ENVMonitorN[ENV_Humidity], "ENV_Humidity", "Ambient Humidity %", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&ENVMonitorN[ENV_Temp], "ENV_Temp", "Ambient Temperature (C)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&ENVMonitorN[DEW_Point], "DEW_Point", "Dew Point (C)", "%4.2f", 0, 999, 100, 0);
    IUFillNumberVector(&ENVMonitorNP, ENVMonitorN, 4, getDeviceName(), "ENV_Monitor", "Environment",ENVIRONMENT_TAB, IP_RO,60, IPS_IDLE);
    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    serialConnection->registerHandshake([&]()
    {
        return getData();
    });
    registerConnection(serialConnection);

    return true;
}

bool WandererBoxPlusV3::getData()
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
    if(strcmp(name, "ZXWBProV3")==0||strcmp(name, "WandererCoverV4")==0||strcmp(name, "UltimateV2")==0||strcmp(name, "PlusV2")==0)
    {
        LOGF_INFO("The device is not WandererBox Pro V3!","Updated");
        return false;
    }
    if(strcmp(name, "ZXWBPlusV3")!=0)
            throw std::exception();
    // Frimware version/////////////////////////////////////////////////////////////////////////////////////////////
    int nbytes_read_version = 0;
    char version[64] = {0};
    tty_read_section(PortFD, version, 'A', 5, &nbytes_read_version);

    version[nbytes_read_version - 1] = '\0';
    IUSaveText(&FirmwareT[0], version);
    IDSetText(&FirmwareTP, nullptr);
    if(std::stoi(version)<=20240216)
    {
        deleteProperty(CalibrateSP.name);
    }

    // Temp probe 1//////////////////////////////////////////////////////////////////////////////////////////
    char temp1[64] = {0};
    int nbytes_read_temp1= 0;
    tty_read_section(PortFD, temp1, 'A', 5, &nbytes_read_temp1);
    temp1[nbytes_read_temp1 - 1] = '\0';
    temp1read = std::strtod(temp1,NULL);

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
    updateENV(temp1read,DHTHread,DHTTread);

    // Total current//////////////////////////////////////////////////////////////////////////////////////////
    char Tcurrent[64] = {0};
    int nbytes_read_Tcurrent= 0;
    tty_read_section(PortFD, Tcurrent, 'A', 5, &nbytes_read_Tcurrent);
    Tcurrent[nbytes_read_Tcurrent - 1] = '\0';
    Tcurrentread = std::strtod(Tcurrent,NULL);


    // Voltage//////////////////////////////////////////////////////////////////////////////////////////
    char voltage[64] = {0};
    int nbytes_read_voltage= 0;
    tty_read_section(PortFD, voltage, 'A', 5, &nbytes_read_voltage);
    voltage[nbytes_read_voltage - 1] = '\0';
    voltageread = std::strtod(voltage,NULL);
    updatePower(Tcurrentread,voltageread);

    // USB//////////////////////////////////////////////////////////////////////////////////////////
    char USB[64] = {0};
    int nbytes_read_USB= 0;
    tty_read_section(PortFD, USB, 'A', 5, &nbytes_read_USB);
    USB[nbytes_read_USB - 1] = '\0';
    USBread = std::stoi(USB);
    updateUSB(USBread);

    // DC2//////////////////////////////////////////////////////////////////////////////////////////
    char DC2[64] = {0};
    int nbytes_read_DC2= 0;
    tty_read_section(PortFD, DC2, 'A', 5, &nbytes_read_DC2);
    DC2[nbytes_read_DC2 - 1] = '\0';
    DC2read = std::stoi(DC2);
    updateDC2(DC2read);

    // DC3//////////////////////////////////////////////////////////////////////////////////////////
    char DC3[64] = {0};
    int nbytes_read_DC3= 0;
    tty_read_section(PortFD, DC3, 'A', 5, &nbytes_read_DC3);
    DC3[nbytes_read_DC3 - 1] = '\0';
    DC3read = std::stoi(DC3);
    updateDC3(DC3read);


    // DC4_6//////////////////////////////////////////////////////////////////////////////////////////
    char DC4_6[64] = {0};
    int nbytes_read_DC4_6= 0;
    tty_read_section(PortFD, DC4_6, 'A', 5, &nbytes_read_DC4_6);
    DC4_6[nbytes_read_DC4_6 - 1] = '\0';
    DC4_6read =std::stoi(DC4_6);
    updateDC4_6(DC4_6read);

    // DC2SET//////////////////////////////////////////////////////////////////////////////////////////
    char DC2SET[64] = {0};
    int nbytes_read_DC2SET= 0;
    tty_read_section(PortFD, DC2SET, 'A', 5, &nbytes_read_DC2SET);
    DC2SET[nbytes_read_DC2SET - 1] = '\0';
    //LOGF_INFO("All Data Updated","Updated");
    DC2SETread = std::stoi(DC2SET);
    updateDC2SET(DC2SETread);

    //DC3 DEW CONTROL
    if(DC3DIFFMODE==true )
    {
        if(temp1read<ENVMonitorN[DEW_Point].value+DC3diffSETN[DC3DIFFSET].value)
            sendCommand("3255");
        else
            sendCommand("3000");
    }
    if(DC3CONSTMODE==true )
    {
        if(temp1read<DC3constSETN[DC3CONSTSET].value)
            sendCommand("3255");
        else
            sendCommand("3000");
    }
    if (DC3diffS[DC3_DPD_Mode].s == ISS_ON&&ENVMonitorN[Probe1_Temp].value==-127)
    {
        DC3DIFFMODE=false;
        DC3CONSTMODE=false;
        defineProperty(&DC3ControlNP);
        deleteProperty(DC3diffSETNP.name);
        deleteProperty(DC3constSETNP.name);
        LOGF_INFO("Temp probe is not connected, Dew Point Difference Mode for DC3 has exited!","Updated");
        LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
        DC3diffS[DC3_Manual].s = ISS_ON;
        DC3diffS[DC3_DPD_Mode].s = ISS_OFF;
        DC3diffS[DC3_CT_Mode].s = ISS_OFF;
        DC3dewSP.s = IPS_OK;
        IDSetSwitch(&DC3dewSP, nullptr);
    }
    if (DC3diffS[DC3_DPD_Mode].s == ISS_ON&&isnan(ENVMonitorN[DEW_Point].value)==1)
    {
        DC3DIFFMODE=false;
        DC3CONSTMODE=false;
        defineProperty(&DC3ControlNP);
        deleteProperty(DC3diffSETNP.name);
        deleteProperty(DC3constSETNP.name);
        LOGF_INFO("DHT22 Humidity&Temperature sensor is not connected, Dew Point Difference Mode for DC3 has exited!","Updated");
        DC3diffS[DC3_Manual].s = ISS_ON;
        DC3diffS[DC3_DPD_Mode].s = ISS_OFF;
        DC3diffS[DC3_CT_Mode].s = ISS_OFF;
        DC3dewSP.s = IPS_OK;
        IDSetSwitch(&DC3dewSP, nullptr);
    }
    if (DC3diffS[DC3_CT_Mode].s == ISS_ON&&ENVMonitorN[Probe1_Temp].value==-127)
    {
        DC3DIFFMODE=false;
        DC3CONSTMODE=false;
        defineProperty(&DC3ControlNP);
        deleteProperty(DC3diffSETNP.name);
        deleteProperty(DC3constSETNP.name);
        LOGF_INFO("Temp probe is not connected, Constant Temperature Mode for DC3 has exited!","Updated");
                LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
        DC3diffS[DC3_Manual].s = ISS_ON;
        DC3diffS[DC3_DPD_Mode].s = ISS_OFF;
        DC3diffS[DC3_CT_Mode].s = ISS_OFF;
        DC3dewSP.s = IPS_OK;
        IDSetSwitch(&DC3dewSP, nullptr);
    }

    }
    catch(std::exception& e)
    {
    //LOGF_INFO("Data read failed","failed");
    }
    return true;
}



void WandererBoxPlusV3::updateENV(double temp1,double DHTH,double DHTT)
{
    ENVMonitorN[Probe1_Temp].value = temp1;
    ENVMonitorN[ENV_Humidity].value = DHTH;
    ENVMonitorN[ENV_Temp].value = DHTT;
    ENVMonitorN[DEW_Point].value = (237.7 * ((17.27 * DHTT) / (237.7 + DHTT) + log((DHTH / 100)))) / (17.27 - ((17.27 * DHTT) / (237.7 + DHTT) + log((DHTH / 100))));;
    ENVMonitorNP.s = IPS_OK;
    IDSetNumber(&ENVMonitorNP, nullptr);

}


void WandererBoxPlusV3::updatePower(double Tcurrent,double voltage)
{
    // Power Monitor
    PowerMonitorN[VOLTAGE].value = voltage;
    PowerMonitorN[TOTAL_CURRENT].value = Tcurrent;
    PowerMonitorNP.s = IPS_OK;
    IDSetNumber(&PowerMonitorNP, nullptr);
}

void WandererBoxPlusV3::updateUSB(int res)
{
    USBControlS[INDI_ENABLED].s = (res== 1) ? ISS_ON : ISS_OFF;
    USBControlS[INDI_DISABLED].s = (res== 0) ? ISS_ON : ISS_OFF;
    USBControlSP.s = (res== 1) ? IPS_OK : IPS_IDLE;
    IDSetSwitch(&USBControlSP, nullptr);
}


void WandererBoxPlusV3::updateDC2(int res)
{
    DC2ControlS[INDI_ENABLED].s = (res== 1) ? ISS_ON : ISS_OFF;
    DC2ControlS[INDI_DISABLED].s = (res== 0) ? ISS_ON : ISS_OFF;
    DC2ControlSP.s = (res== 1) ? IPS_OK : IPS_IDLE;
    IDSetSwitch(&DC2ControlSP, nullptr);
}

void WandererBoxPlusV3::updateDC3(int res)
{
    DC3ControlN[0].value = res;
    DC3ControlNP.s = IPS_OK;
    IDSetNumber(&DC3ControlNP, nullptr);
}


void WandererBoxPlusV3::updateDC4_6(int res)
{
    DC4_6ControlS[INDI_ENABLED].s = (res== 1) ? ISS_ON : ISS_OFF;
    DC4_6ControlS[INDI_DISABLED].s = (res== 0) ? ISS_ON : ISS_OFF;
    DC4_6ControlSP.s = (res== 1) ? IPS_OK : IPS_IDLE;
    IDSetSwitch(&DC4_6ControlSP, nullptr);
}

void WandererBoxPlusV3::updateDC2SET(double res)
{
    setDC2voltageN[setDC2voltage].value = res/10.0;
    setDC2voltageNP.s = IPS_OK;
    IDSetNumber(&setDC2voltageNP, nullptr);
}



bool WandererBoxPlusV3::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {

        defineProperty(&FirmwareTP);
        defineProperty(&CalibrateSP);

        defineProperty(&PowerMonitorNP);


        defineProperty(&USBControlSP);

        defineProperty(&setDC2voltageNP);
        defineProperty(&DC2ControlSP);

        defineProperty(&DC4_6ControlSP);

        defineProperty(&DC3dewSP);



            //DC3////////////////////
        if(DC3diffS[DC3_DPD_Mode].s == ISS_ON)
        {
            deleteProperty(DC3constSETNP.name);
            deleteProperty(DC3ControlNP.name);
            defineProperty(&DC3diffSETNP);
        }
        else if(DC3diffS[DC3_CT_Mode].s == ISS_ON)
        {
            deleteProperty(DC3ControlNP.name);
            deleteProperty(DC3diffSETNP.name);
            defineProperty(&DC3constSETNP);
        }
        else
        {
            defineProperty(&DC3ControlNP);
            deleteProperty(DC3diffSETNP.name);
            deleteProperty(DC3constSETNP.name);
        }



        defineProperty(&ENVMonitorNP);


    }
    else
    {

        deleteProperty(FirmwareTP.name);
        deleteProperty(CalibrateSP.name);
        deleteProperty(PowerMonitorNP.name);
        deleteProperty(ENVMonitorNP.name);

        deleteProperty(DC2ControlSP.name);
        deleteProperty(setDC2voltageNP.name);
        deleteProperty(DC4_6ControlSP.name);
        deleteProperty(USBControlSP.name);

        deleteProperty(DC3ControlNP.name);


        deleteProperty(DC3dewSP.name);
        deleteProperty(DC3diffSETNP.name);



    }
    return true;
}

bool WandererBoxPlusV3::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Calibrate
    if (!strcmp(name, CalibrateSP.name))
    {
        CalibrateSP.s = sendCommand("66300744") ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&CalibrateSP, nullptr);
        LOG_INFO("Calibrating Current Readings...");
        return true;
    }


    // DC2 Control
    if (!strcmp(name, DC2ControlSP.name))
    {
        IUUpdateSwitch(&DC2ControlSP, states, names, n);
        DC2ControlSP.s = IPS_ALERT;
        char cmd[128] = {0};
        snprintf(cmd, 128, "12%d", (DC2ControlS[INDI_ENABLED].s == ISS_ON) ? 1 : 0);
        DC2ControlSP.s = sendCommand(cmd) ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&DC2ControlSP, nullptr);

        return true;
    }

    // DC4-6 Control
    if (!strcmp(name, DC4_6ControlSP.name))
    {
        IUUpdateSwitch(&DC4_6ControlSP, states, names, n);
        DC4_6ControlSP.s = IPS_ALERT;
        char cmd[128] = {0};
        snprintf(cmd, 128, "10%d", (DC4_6ControlS[INDI_ENABLED].s == ISS_ON) ? 1 : 0);
        DC4_6ControlSP.s = sendCommand(cmd) ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&DC4_6ControlSP, nullptr);

        return true;
    }

    // USB Control
    if (!strcmp(name, USBControlSP.name))
    {
        IUUpdateSwitch(&USBControlSP, states, names, n);
        USBControlSP.s = IPS_ALERT;
        char cmd[128] = {0};
        snprintf(cmd, 128, "11%d", (USBControlS[INDI_ENABLED].s == ISS_ON) ? 1 : 0);
        USBControlSP.s = sendCommand(cmd) ? IPS_OK : IPS_ALERT;

        return true;
    }


    // DC3 DEW
    if (!strcmp(name, DC3dewSP.name))
    {
        IUUpdateSwitch(&DC3dewSP, states, names, n);
        DC3dewSP.s = IPS_ALERT;
        if(DC3diffS[DC3_DPD_Mode].s == ISS_ON&&ENVMonitorN[Probe1_Temp].value!=-127&&isnan(ENVMonitorN[DEW_Point].value)==0)
        {
            DC3DIFFMODE=true;
            DC3CONSTMODE=false;
            deleteProperty(DC3ControlNP.name);
            deleteProperty(DC3constSETNP.name);
            defineProperty(&DC3diffSETNP);

            DC3diffSETNP.s = IPS_OK;
            IDSetNumber(&DC3diffSETNP, nullptr);
            DC3dewSP.s = IPS_OK ;
            IDSetSwitch(&DC3dewSP, nullptr);
            LOGF_INFO("Dew Point Difference Mode for DC3 activated! WandererBox will keep the dew heater at the temperature higher than the dew point by the set value.","Updated");
            return true;
        }
        else if(DC3diffS[DC3_DPD_Mode].s == ISS_ON&&(ENVMonitorN[Probe1_Temp].value==-127||isnan(ENVMonitorN[DEW_Point].value)==1))
        {
            DC3DIFFMODE=false;
            DC3CONSTMODE=false;
            DC3diffS[DC3_Manual].s = ISS_ON;
                        LOGF_INFO("Manual Mode for DC3 activated! Please adjust the duty cycle manually, you can also use DC3 as an ordinary switch.","Updated");
            IDSetSwitch(&DC3dewSP, nullptr);
        }
        else if(DC3diffS[DC3_CT_Mode].s == ISS_ON&&ENVMonitorN[Probe1_Temp].value!=-127)
        {
            DC3CONSTMODE=true;
            DC3DIFFMODE=false;
            deleteProperty(DC3diffSETNP.name);
            deleteProperty(DC3ControlNP.name);
            defineProperty(&DC3constSETNP);

            DC3constSETNP.s = IPS_OK;
            IDSetNumber(&DC3constSETNP, nullptr);
            DC3dewSP.s = IPS_OK ;
            IDSetSwitch(&DC3dewSP, nullptr);
            LOGF_INFO("Constant Temperature Mode for DC3 activated! WandererBox will keep the dew heater at the set temperature.","Updated");
            return true;
        }
        else if(DC3diffS[DC3_CT_Mode].s == ISS_ON&&ENVMonitorN[Probe1_Temp].value==-127)
        {
            DC3DIFFMODE=false;
            DC3CONSTMODE=false;
            DC3diffS[DC3_Manual].s = ISS_ON;
            LOGF_INFO("Manual Mode for DC3 activated! Please adjust the duty cycle manually, you can also use DC3 as an ordinary switch.","Updated");
            IDSetSwitch(&DC3dewSP, nullptr);
        }
        else
        {
            DC3DIFFMODE=false;
            DC3CONSTMODE=false;
            defineProperty(&DC3ControlNP);
            deleteProperty(DC3diffSETNP.name);
            deleteProperty(DC3constSETNP.name);
            DC3dewSP.s = IPS_OK ;
            IDSetSwitch(&DC3dewSP, nullptr);
            LOGF_INFO("Manual Mode for DC3 activated! Please adjust the duty cycle manually, you can also use DC3 as an ordinary switch.","Updated");
            return true;
        }

    }


    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool WandererBoxPlusV3::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // DC3
        if (!strcmp(name, DC3ControlNP.name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], DC3ControlN[DC3].name))
                    rc1 = setDewPWM(3, static_cast<uint8_t>(values[i]));
            }

            DC3ControlNP.s = (rc1) ? IPS_OK : IPS_ALERT;
            if (DC3ControlNP.s == IPS_OK)
                IUUpdateNumber(&DC3ControlNP, values, names, n);
            IDSetNumber(&DC3ControlNP, nullptr);
            return true;
        }
        if (!strcmp(name, DC3diffSETNP.name))
        {

            DC3diffSETNP.s =  IPS_OK ;
            if (DC3diffSETNP.s == IPS_OK)
                IUUpdateNumber(&DC3diffSETNP, values, names, n);
            IDSetNumber(&DC3diffSETNP, nullptr);
            return true;
        }
        if (!strcmp(name, DC3constSETNP.name))
        {

            DC3constSETNP.s =  IPS_OK ;
            if (DC3constSETNP.s == IPS_OK)
                IUUpdateNumber(&DC3constSETNP, values, names, n);
            IDSetNumber(&DC3constSETNP, nullptr);
            return true;
        }

        // DC2voltageSET
        if (!strcmp(name, setDC2voltageNP.name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], setDC2voltageN[setDC2voltage].name))
                    rc1 = setDewPWM(20, static_cast<uint8_t>(10*values[i]));
            }

            setDC2voltageNP.s = (rc1) ? IPS_OK : IPS_ALERT;
            if (setDC2voltageNP.s == IPS_OK)
                IUUpdateNumber(&setDC2voltageNP, values, names, n);
            IDSetNumber(&setDC2voltageNP, nullptr);
            return true;
        }


    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool WandererBoxPlusV3::setDewPWM(uint8_t id, uint8_t value)
{
    char cmd[64] = {0};
    snprintf(cmd, 64, "%d%03d", id, value);
    if (sendCommand(cmd))
    {
        return true;
    }

    return false;
}

const char *WandererBoxPlusV3::getDefaultName()
{
    return "WandererBox Pro V3";
}


bool WandererBoxPlusV3::sendCommand(std::string command)
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

void WandererBoxPlusV3::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(2500);
        return;
    }

    getData();
    SetTimer(2500);
}

bool WandererBoxPlusV3::saveConfigItems(FILE * fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &DC3dewSP);
    IUSaveConfigNumber(fp, &DC3diffSETNP);
    IUSaveConfigNumber(fp, &DC3constSETNP);
    IUSaveConfigNumber(fp, &DC3ControlNP);


    IUSaveConfigNumber(fp, &setDC2voltageNP);
    return true;
}


