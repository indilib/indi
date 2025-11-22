/*******************************************************************************
  Copyright(c) 2024 Frank Wang. All rights reserved.

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
#include <cmath>

// We declare an auto pointer to WandererBoxPlusV3.
static std::unique_ptr<WandererBoxPlusV3> WandererBoxplusv3(new WandererBoxPlusV3());



WandererBoxPlusV3::WandererBoxPlusV3() : INDI::DefaultDevice(), INDI::WeatherInterface(this)
{
    setVersion(1, 0);
}

bool WandererBoxPlusV3::initProperties()
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
    PowerMonitorNP.fill(getDeviceName(), "POWER_Monitor", "Power Monitor",  MAIN_CONTROL_TAB, IP_RO,60, IPS_IDLE);


    // USB Control
    USBControlSP[INDI_ENABLED].fill("INDI_ENABLED", "On", ISS_OFF);
    USBControlSP[INDI_DISABLED].fill("INDI_DISABLED", "Off", ISS_OFF);
    USBControlSP.fill(getDeviceName(), "USB", "USB", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);


    // DC3
    DC3ControlNP[DC3].fill( "DC3", "Dew Heater (PWM)", "%.2f", 0, 255, 5, 0);
    DC3ControlNP.fill(getDeviceName(), "PWM", "DC3", DC3_TAB, IP_RW, 60, IPS_IDLE);

    // DC2SET
    setDC2voltageNP[setDC2voltage].fill( "DC2SET", "Adjustable Voltage", "%.2f", 5, 13.2, 0.1, 0);
    setDC2voltageNP.fill(getDeviceName(), "DC2voltageSET", "Set DC2", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    // DC2 Control
    dc2ControlSP[INDI_ENABLED].fill( "INDI_ENABLED", "On", ISS_OFF);
    dc2ControlSP[INDI_DISABLED].fill( "INDI_DISABLED", "Off", ISS_ON);
    dc2ControlSP.fill(getDeviceName(), "DC2", "DC2", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    // DC4-6 Control
    DC4_6ControlSP[INDI_ENABLED].fill( "INDI_ENABLED", "On", ISS_OFF);
    DC4_6ControlSP[INDI_DISABLED].fill( "INDI_DISABLED", "Off", ISS_ON);
    DC4_6ControlSP.fill(getDeviceName(), "DC4-6", "DC4-6", MAIN_CONTROL_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

     // DC3 TEMP Difference Control
    DC3diffSP[DC3_Manual].fill( "Manual", "Manual", ISS_ON);
    DC3diffSP[DC3_DPD_Mode].fill( "DPD_Mode", "DPD Mode", ISS_OFF);
    DC3diffSP[DC3_CT_Mode].fill( "CT_Mode", "CT Mode", ISS_OFF);
    DC3diffSP.fill(getDeviceName(), "DC3_DIFF", "DC3 Dew Mode", DC3_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    DC3diffSETNP[DC3DIFFSET].fill( "DC3 Auto Control", "Dew Point Difference(C)", "%.2f", 10, 30, 1, 0);
    DC3diffSETNP.fill(getDeviceName(), "DC3_DIFF_SET", "DPD Mode", DC3_TAB, IP_RW, 60, IPS_IDLE);

    DC3constSETNP[DC3CONSTSET].fill( "DC3 Auto Control", "Temperature(C)", "%.2f", 0, 40, 1, 0);
    DC3constSETNP.fill(getDeviceName(), "DC3_CONST_SET", "CT Mode", DC3_TAB, IP_RW, 60, IPS_IDLE);

    //ENV
    ENVMonitorNP[Probe1_Temp].fill( "Probe1_Temp", "Probe1 Temperature (C)", "%4.2f", 0, 999, 100, 0);
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
            LOGF_INFO("No data received, the device may not be WandererBox Plus V3, please check the serial port!","Updated");
            LOGF_ERROR("Device read error: %s", errorMessage);
            return false;
        }
        name[nbytes_read_name - 1] = '\0';
        if(strcmp(name, "ZXWBProV3")==0||strcmp(name, "WandererCoverV4")==0||strcmp(name, "UltimateV2")==0||strcmp(name, "PlusV2")==0)
        {
            LOGF_INFO("The device is not WandererBox Plus V3!","Updated");
            return false;
        }
        if(strcmp(name, "ZXWBPlusV3")!=0)
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
        if(DC3diffSP[DC3_DPD_Mode].getState() == ISS_ON)
        {
            if(temp1read<ENVMonitorNP[DEW_Point].value+DC3diffSETNP[DC3DIFFSET].value)
                sendCommand("3255");
            else
                sendCommand("3000");
        }

        if(DC3CONSTMODE==true )
        {
            if(temp1read<DC3constSETNP[DC3CONSTSET].value)
                sendCommand("3255");
            else
                sendCommand("3000");
        }
        if (DC3diffSP[DC3_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value==-127)
        {
            DC3DIFFMODE=false;
            DC3CONSTMODE=false;
            defineProperty(DC3ControlNP);
            deleteProperty(DC3diffSETNP);
            deleteProperty(DC3constSETNP);
            LOGF_ERROR("Temp probe 1 not connected, Dew Point Difference Mode for DC3 has exited!","Updated");
            LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
            DC3diffSP[DC3_Manual].setState( ISS_ON);
            DC3diffSP[DC3_DPD_Mode].setState( ISS_OFF);
            DC3diffSP[DC3_CT_Mode].setState( ISS_OFF);
            DC3diffSP.setState(IPS_OK);
            DC3diffSP.apply();
        }
        if (DC3diffSP[DC3_DPD_Mode].getState() == ISS_ON&&isnan(ENVMonitorNP[DEW_Point].value)==1)
        {
            DC3DIFFMODE=false;
            DC3CONSTMODE=false;
            defineProperty(DC3ControlNP);
            deleteProperty(DC3diffSETNP);
            deleteProperty(DC3constSETNP);
            LOGF_ERROR("DHT22 Humidity&Temperature sensor not connected, Dew Point Difference Mode for DC3 has exited!","Updated");
            DC3diffSP[DC3_Manual].setState( ISS_ON);
            DC3diffSP[DC3_DPD_Mode].setState( ISS_OFF);
            DC3diffSP[DC3_CT_Mode].setState( ISS_OFF);
            DC3diffSP.setState(IPS_OK);
            DC3diffSP.apply();
        }
        if (DC3diffSP[DC3_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value==-127)
        {
            DC3DIFFMODE=false;
            DC3CONSTMODE=false;
            defineProperty(DC3ControlNP);
            deleteProperty(DC3diffSETNP);
            deleteProperty(DC3constSETNP);
            LOGF_ERROR("Temp probe 1 not connected, Constant Temperature Mode for DC3 has exited!","Updated");
            LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
            DC3diffSP[DC3_Manual].setState( ISS_ON);
            DC3diffSP[DC3_DPD_Mode].setState( ISS_OFF);
            DC3diffSP[DC3_CT_Mode].setState( ISS_OFF);
            DC3diffSP.setState(IPS_OK);
            DC3diffSP.apply();
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
    ENVMonitorNP[Probe1_Temp].setValue(temp1);
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


void WandererBoxPlusV3::updatePower(double Tcurrent,double voltage)
{
    // Power Monitor
    PowerMonitorNP[VOLTAGE].setValue(voltage);
    PowerMonitorNP[TOTAL_CURRENT].setValue(Tcurrent);

    PowerMonitorNP.setState(IPS_OK);
    PowerMonitorNP.apply();
}

void WandererBoxPlusV3::updateUSB(int res)
{
    USBControlSP[INDI_ENABLED].setState( (res== 1) ? ISS_ON : ISS_OFF);
    USBControlSP[INDI_DISABLED].setState( (res== 0) ? ISS_ON : ISS_OFF);
    USBControlSP.setState((res== 1) ? IPS_OK : IPS_IDLE);
    USBControlSP.apply();
}

void WandererBoxPlusV3::updateDC2(int res)
{
    dc2ControlSP[INDI_ENABLED].setState( (res== 1) ? ISS_ON : ISS_OFF);
    dc2ControlSP[INDI_DISABLED].setState( (res== 0) ? ISS_ON : ISS_OFF);
    dc2ControlSP.setState((res== 1) ? IPS_OK : IPS_IDLE);
    dc2ControlSP.apply();
}

void WandererBoxPlusV3::updateDC3(int res)
{
    DC3ControlNP[0].setValue(res);
    DC3ControlNP.setState(IPS_OK);
    DC3ControlNP.apply();
}


void WandererBoxPlusV3::updateDC4_6(int res)
{
    DC4_6ControlSP[INDI_ENABLED].setState( (res== 1) ? ISS_ON : ISS_OFF);
    DC4_6ControlSP[INDI_DISABLED].setState( (res== 0) ? ISS_ON : ISS_OFF);
    DC4_6ControlSP.setState((res== 1) ? IPS_OK : IPS_IDLE);
    DC4_6ControlSP.apply();
}


void WandererBoxPlusV3::updateDC2SET(double res)
{
    setDC2voltageNP[setDC2voltage].setValue(res/10);
    setDC2voltageNP.setState(IPS_OK);
    setDC2voltageNP.apply();
}



bool WandererBoxPlusV3::updateProperties()
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


        defineProperty(USBControlSP);

        defineProperty(setDC2voltageNP);
        defineProperty(dc2ControlSP);

        defineProperty(DC4_6ControlSP);


        defineProperty(DC3diffSP);



        //DC3////////////////////
        if(DC3diffSP[DC3_DPD_Mode].getState() == ISS_ON)
        {
            deleteProperty(DC3constSETNP);
            deleteProperty(DC3ControlNP);
            defineProperty(DC3diffSETNP);
        }
        else if(DC3diffSP[DC3_CT_Mode].getState() == ISS_ON)
        {
            deleteProperty(DC3ControlNP);
            deleteProperty(DC3diffSETNP);
            defineProperty(DC3constSETNP);
        }
        else
        {
            defineProperty(DC3ControlNP);
            deleteProperty(DC3diffSETNP);
            deleteProperty(DC3constSETNP);
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

        deleteProperty(dc2ControlSP);
        deleteProperty(setDC2voltageNP);
        deleteProperty(DC4_6ControlSP);

        deleteProperty(USBControlSP);


        deleteProperty(DC3ControlNP);


        deleteProperty(DC3diffSP);
        deleteProperty(DC3diffSETNP);



    }
    return true;
}

bool WandererBoxPlusV3::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
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


    // DC2 Control
    if (dc2ControlSP.isNameMatch(name))
    {
        dc2ControlSP.update(states, names, n);
        dc2ControlSP.setState(IPS_ALERT);
        char cmd[128] = {0};
        snprintf(cmd, 128, "12%d", (dc2ControlSP[INDI_ENABLED].getState() == ISS_ON) ? 1 : 0);
        dc2ControlSP.setState( sendCommand(cmd) ? IPS_OK : IPS_ALERT);
        dc2ControlSP.apply();

        return true;
    }
    // DC4-6 Control
    if (DC4_6ControlSP.isNameMatch(name))
    {
        DC4_6ControlSP.update(states, names, n);
        DC4_6ControlSP.setState(IPS_ALERT);
        char cmd[128] = {0};
        snprintf(cmd, 128, "10%d", (DC4_6ControlSP[INDI_ENABLED].getState() == ISS_ON) ? 1 : 0);
        DC4_6ControlSP.setState( sendCommand(cmd) ? IPS_OK : IPS_ALERT);
        DC4_6ControlSP.apply();

        return true;
    }

    // USB Control
    if (USBControlSP.isNameMatch(name))
    {
        USBControlSP.update(states, names, n);
        USBControlSP.setState(IPS_ALERT);
        char cmd[128] = {0};
        snprintf(cmd, 128, "11%d", (USBControlSP[INDI_ENABLED].getState() == ISS_ON) ? 1 : 0);
        USBControlSP.setState( sendCommand(cmd) ? IPS_OK : IPS_ALERT);

        return true;
    }

    // DC3 DEW
    if (DC3diffSP.isNameMatch(name))
    {
        DC3diffSP.update(states, names, n);
        DC3diffSP.setState(IPS_ALERT);
        if(DC3diffSP[DC3_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value!=-127&&isnan(ENVMonitorNP[DEW_Point].value)==0)
        {
            DC3DIFFMODE=true;
            DC3CONSTMODE=false;
            deleteProperty(DC3ControlNP);
            deleteProperty(DC3constSETNP);
            defineProperty(DC3diffSETNP);

            DC3diffSETNP.setState(IPS_OK);
            DC3diffSETNP.apply();
            DC3diffSP.setState(IPS_OK) ;
            DC3diffSP.apply();
            LOGF_INFO("Dew Point Difference Mode for DC3 activated! WandererBox will keep the dew heater at the temperature higher than the dew point by the set value.","Updated");
            return true;
        }
        else if(DC3diffSP[DC3_DPD_Mode].getState() == ISS_ON&&(ENVMonitorNP[Probe1_Temp].value==-127||isnan(ENVMonitorNP[DEW_Point].value)==1))
        {
            DC3DIFFMODE=false;
            DC3CONSTMODE=false;
            DC3diffSP[DC3_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC3 activated! Please adjust the duty cycle manually, you can also use DC3 as an ordinary switch.","Updated");
            DC3diffSP.apply();
        }
        else if(DC3diffSP[DC3_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value!=-127)
        {
            DC3CONSTMODE=true;
            DC3DIFFMODE=false;
            deleteProperty(DC3diffSETNP);
            deleteProperty(DC3ControlNP);
            defineProperty(DC3constSETNP);

            DC3constSETNP.setState(IPS_OK);
            DC3constSETNP.apply();
            DC3diffSP.setState(IPS_OK) ;
            DC3diffSP.apply();
            LOGF_INFO("Constant Temperature Mode for DC3 activated! WandererBox will keep the dew heater at the set temperature.","Updated");
            return true;
        }
        else if(DC3diffSP[DC3_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value==-127)
        {
            DC3DIFFMODE=false;
            DC3CONSTMODE=false;
            DC3diffSP[DC3_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC3 activated! Please adjust the duty cycle manually, you can also use DC3 as an ordinary switch.","Updated");
            DC3diffSP.apply();
        }
        else
        {
            DC3DIFFMODE=false;
            DC3CONSTMODE=false;
            defineProperty(DC3ControlNP);
            deleteProperty(DC3diffSETNP);
            deleteProperty(DC3constSETNP);
            DC3diffSP.setState(IPS_OK) ;
            DC3diffSP.apply();
            LOGF_INFO("Manual Mode for DC3 activated! Please adjust the duty cycle manually, you can also use DC3 as an ordinary switch.","Updated");
            return true;
        }

    }


    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool WandererBoxPlusV3::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (strstr(name, "WEATHER_"))
            return WI::processNumber(dev, name, values, names, n);
        // DC3
        if (DC3ControlNP.isNameMatch(name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                rc1 = setDewPWM(3, static_cast<uint8_t>(values[i]));
            }

            DC3ControlNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (DC3ControlNP.getState() == IPS_OK)
                DC3ControlNP.update(values, names, n);
            DC3ControlNP.apply();
            return true;
        }
        if (DC3diffSETNP.isNameMatch(name))
        {

            DC3diffSETNP.setState(IPS_OK) ;
            if (DC3diffSETNP.getState() == IPS_OK)
                DC3diffSETNP.update(values, names, n);
            DC3diffSETNP.apply();
            return true;
        }
        if (DC3constSETNP.isNameMatch(name))
        {

            DC3constSETNP.setState(IPS_OK) ;
            if (DC3constSETNP.getState() == IPS_OK)
                DC3constSETNP.update(values, names, n);
            DC3constSETNP.apply();
            return true;
        }

        // DC2voltageSET
        if (setDC2voltageNP.isNameMatch(name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                rc1 = setDewPWM(20, static_cast<uint8_t>(10*values[i]));
            }

            setDC2voltageNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (setDC2voltageNP.getState() == IPS_OK)
                setDC2voltageNP.update(values, names, n);
            setDC2voltageNP.apply();
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
    WI::saveConfigItems(fp);

    DC3diffSP.save(fp);
    DC3diffSETNP.save(fp);
    DC3constSETNP.save(fp);
    DC3ControlNP.save(fp);

    setDC2voltageNP.save(fp);
    return true;
}

IPState WandererBoxPlusV3::updateWeather()
{
    // Weather is updated in updateENV() which is called from getData()
    // This method is required by WeatherInterface but we update weather
    // synchronously with sensor data, so we just return OK
    return IPS_OK;
}

