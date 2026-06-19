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

#include "wanderer_dew_terminator.h"
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

// We declare an auto pointer to WandererDewTerminator.
static std::unique_ptr<WandererDewTerminator> wandererdewterminator(new WandererDewTerminator());



WandererDewTerminator::WandererDewTerminator()
{
    setVersion(1, 0);
}

bool WandererDewTerminator::initProperties()
{

    INDI::DefaultDevice::initProperties();
    setDriverInterface(AUX_INTERFACE);


    addAuxControls();




    // Power Monitor
    PowerMonitorNP[VOLTAGE].fill("VOLTAGE", "Voltage (V)", "%4.2f", 0, 999, 100, 0);

    PowerMonitorNP.fill(getDeviceName(), "POWER_Monitor", "Power Monitor",  MAIN_CONTROL_TAB, IP_RO,60, IPS_IDLE);




    // DC123


    DC1ControlNP[DC1].fill( "DC1", "Dew Heater (PWM)", "%.2f", 0, 255, 5, 0);
    DC1ControlNP.fill(getDeviceName(), "PWM", "DC1", DC1_TAB, IP_RW, 60, IPS_IDLE);

    DC2ControlNP[DC2].fill( "DC2", "Dew Heater (PWM)", "%.2f", 0, 255, 5, 0);
    DC2ControlNP.fill( getDeviceName(), "DC2", "DC2", DC2_TAB, IP_RW, 60, IPS_IDLE);

    DC3ControlNP[DC3].fill( "DC3", "Dew Heater (PWM)", "%.2f", 0, 255, 5, 0);
    DC3ControlNP.fill(getDeviceName(), "DC3", "DC3", DC3_TAB, IP_RW, 60, IPS_IDLE);


    // DC1 TEMP Difference Control
    DC1diffSP[DC1_Manual].fill( "Manual", "Manual", ISS_ON);
    DC1diffSP[DC1_DPD_Mode].fill( "DPD_Mode", "DPD Mode", ISS_OFF);
    DC1diffSP[DC1_CT_Mode].fill( "CT_Mode", "CT Mode", ISS_OFF);
    DC1diffSP.fill(getDeviceName(), "DC1_DIFF", "DC1 Dew Mode", DC1_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    DC1diffSETNP[DC1DIFFSET].fill( "DC1 Auto Control", "Dew Point Difference(C)", "%.2f", 10, 30, 1, 0);
    DC1diffSETNP.fill(getDeviceName(), "DC1_DIFF_SET", "DPD Mode", DC1_TAB, IP_RW, 60, IPS_IDLE);

    DC1constSETNP[DC1CONSTSET].fill( "DC1 Auto Control", "Temperature(C)", "%.2f", 0, 40, 1, 0);
    DC1constSETNP.fill(getDeviceName(), "DC1_CONST_SET", "CT Mode", DC1_TAB, IP_RW, 60, IPS_IDLE);

    // DC2 TEMP Difference Control
    DC2diffSP[DC2_Manual].fill( "Manual", "Manual", ISS_ON);
    DC2diffSP[DC2_DPD_Mode].fill( "DPD_Mode", "DPD Mode", ISS_OFF);
    DC2diffSP[DC2_CT_Mode].fill( "CT_Mode", "CT Mode", ISS_OFF);
    DC2diffSP.fill(getDeviceName(), "DC2_DIFF", "DC2 Dew Mode", DC2_TAB, IP_RW,ISR_1OFMANY, 60, IPS_IDLE);

    DC2diffSETNP[DC2DIFFSET].fill( "DC2 Auto Control", "Dew Point Difference(C)", "%.2f", 10, 30, 1, 0);
    DC2diffSETNP.fill(getDeviceName(), "DC2_DIFF_SET", "DPD Mode", DC2_TAB, IP_RW, 60, IPS_IDLE);

    DC2constSETNP[DC2CONSTSET].fill( "DC2 Auto Control", "Temperature(C)", "%.2f", 0, 40, 1, 0);
    DC2constSETNP.fill(getDeviceName(), "DC2_CONST_SET", "CT Mode", DC2_TAB, IP_RW, 60, IPS_IDLE);

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
    ENVMonitorNP[Probe2_Temp].fill( "Probe2_Temp", "Probe2 Temperature (C)", "%4.2f", 0, 999, 100, 0);
    ENVMonitorNP[Probe3_Temp].fill( "Probe3_Temp", "Probe3 Temperature (C)", "%4.2f", 0, 999, 100, 0);
    ENVMonitorNP[ENV_Humidity].fill( "ENV_Humidity", "Ambient Humidity %", "%4.2f", 0, 999, 100, 0);
    ENVMonitorNP[ENV_Temp].fill( "ENV_Temp", "Ambient Temperature (C)", "%4.2f", 0, 999, 100, 0);
    ENVMonitorNP[DEW_Point].fill( "DEW_Point", "Dew Point (C)", "%4.2f", 0, 999, 100, 0);
    ENVMonitorNP.fill(getDeviceName(), "ENV_Monitor", "Environment",ENVIRONMENT_TAB, IP_RO,60, IPS_IDLE);
    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    serialConnection->registerHandshake([&]()
                                        {
                                            return getData();
                                        });
    registerConnection(serialConnection);

    return true;
}

bool WandererDewTerminator::getData()
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
            LOGF_INFO("No data received, the device may not be Wanderer Dew Terminator, please check the serial port!","Updated");
            LOGF_ERROR("Device read error: %s", errorMessage);
            return false;
        }
        name[nbytes_read_name - 1] = '\0';
        if(strcmp(name, "ZXWBPlusV3")==0||strcmp(name, "WandererCoverV4")==0||strcmp(name, "UltimateV2")==0||strcmp(name, "PlusV2")==0||strcmp(name, "ZXWBProV3")==0)
        {
            LOGF_INFO("The device is not Wanderer Dew Terminator!","Updated");
            return false;
        }
        if(strcmp(name, "ZXWBDewTerminator")!=0)
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



        // Voltage//////////////////////////////////////////////////////////////////////////////////////////
        char voltage[64] = {0};
        int nbytes_read_voltage= 0;
        tty_read_section(PortFD, voltage, 'A', 5, &nbytes_read_voltage);
        voltage[nbytes_read_voltage - 1] = '\0';
        voltageread = std::strtod(voltage,NULL);
        updatePower(voltageread);



        // DC1//////////////////////////////////////////////////////////////////////////////////////////
        char DC1[64] = {0};
        int nbytes_read_DC1= 0;
        tty_read_section(PortFD, DC1, 'A', 5, &nbytes_read_DC1);
        DC1[nbytes_read_DC1 - 1] = '\0';
        DC1read = std::stoi(DC1);
        updateDC1(DC1read);

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




        //DC1 DEW CONTROL
        if(DC1diffSP[DC1_DPD_Mode].getState() == ISS_ON)
        {
            if(temp1read<ENVMonitorNP[DEW_Point].value+DC1diffSETNP[DC1DIFFSET].value)
                sendCommand("5255");
            else
                sendCommand("5000");
        }

        if(DC1CONSTMODE==true )
        {
            if(temp1read<DC1constSETNP[DC1CONSTSET].value)
                sendCommand("5255");
            else
                sendCommand("5000");
        }
        if (DC1diffSP[DC1_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value==-127)
        {
            DC1DIFFMODE=false;
            DC1CONSTMODE=false;
            defineProperty(DC1ControlNP);
            deleteProperty(DC1diffSETNP);
            deleteProperty(DC1constSETNP);
            LOGF_ERROR("Temp probe 1 not connected, Dew Point Difference Mode for DC1 has exited!","Updated");
            LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
            DC1diffSP[DC1_Manual].setState( ISS_ON);
            DC1diffSP[DC1_DPD_Mode].setState( ISS_OFF);
            DC1diffSP[DC1_CT_Mode].setState( ISS_OFF);
            DC1diffSP.setState(IPS_OK);
            DC1diffSP.apply();
        }
        if (DC1diffSP[DC1_DPD_Mode].getState() == ISS_ON&&isnan(ENVMonitorNP[DEW_Point].value)==1)
        {
            DC1DIFFMODE=false;
            DC1CONSTMODE=false;
            defineProperty(DC1ControlNP);
            deleteProperty(DC1diffSETNP);
            deleteProperty(DC1constSETNP);
            LOGF_ERROR("DHT22 Humidity&Temperature sensor not connected, Dew Point Difference Mode for DC1 has exited!","Updated");
            DC1diffSP[DC1_Manual].setState( ISS_ON);
            DC1diffSP[DC1_DPD_Mode].setState( ISS_OFF);
            DC1diffSP[DC1_CT_Mode].setState( ISS_OFF);
            DC1diffSP.setState(IPS_OK);
            DC1diffSP.apply();
        }
        if (DC1diffSP[DC1_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value==-127)
        {
            DC1DIFFMODE=false;
            DC1CONSTMODE=false;
            defineProperty(DC1ControlNP);
            deleteProperty(DC1diffSETNP);
            deleteProperty(DC1constSETNP);
            LOGF_ERROR("Temp probe 1 not connected, Constant Temperature Mode for DC1 has exited!","Updated");
            LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
            DC1diffSP[DC1_Manual].setState( ISS_ON);
            DC1diffSP[DC1_DPD_Mode].setState( ISS_OFF);
            DC1diffSP[DC1_CT_Mode].setState( ISS_OFF);
            DC1diffSP.setState(IPS_OK);
            DC1diffSP.apply();
        }



        //DC2 DEW CONTROL
        if(DC2DIFFMODE==true )
        {
            if(temp2read<ENVMonitorNP[DEW_Point].value+DC2diffSETNP[DC2DIFFSET].value)
                sendCommand("6255");
            else
                sendCommand("6000");
        }
        if(DC2CONSTMODE==true )
        {
            if(temp2read<DC2constSETNP[DC2CONSTSET].value)
                sendCommand("6255");
            else
                sendCommand("6000");
        }
        if (DC2diffSP[DC2_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe2_Temp].value==-127)
        {
            DC2DIFFMODE=false;
            DC2CONSTMODE=false;
            defineProperty(DC2ControlNP);
            deleteProperty(DC2diffSETNP);
            deleteProperty(DC2constSETNP);
            LOGF_ERROR("Temp probe 2 not connected, Dew Point Difference Mode for DC2 has exited!","Updated");
            LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
            DC2diffSP[DC2_Manual].setState( ISS_ON);
            DC2diffSP[DC2_DPD_Mode].setState( ISS_OFF);
            DC2diffSP[DC2_CT_Mode].setState( ISS_OFF);
            DC2diffSP.setState(IPS_OK);
            DC2diffSP.apply();
        }
        if (DC2diffSP[DC2_DPD_Mode].getState() == ISS_ON&&isnan(ENVMonitorNP[DEW_Point].value)==1)
        {
            DC2DIFFMODE=false;
            DC2CONSTMODE=false;
            defineProperty(DC2ControlNP);
            deleteProperty(DC2diffSETNP);
            deleteProperty(DC2constSETNP);
            LOGF_ERROR("DHT22 Humidity&Temperature sensor not connected, Dew Point Difference Mode for DC2 has exited!","Updated");
            DC2diffSP[DC2_Manual].setState( ISS_ON);
            DC2diffSP[DC2_DPD_Mode].setState( ISS_OFF);
            DC2diffSP[DC2_CT_Mode].setState( ISS_OFF);
            DC2diffSP.setState(IPS_OK);
            DC2diffSP.apply();
        }
        if (DC2diffSP[DC2_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe2_Temp].value==-127)
        {
            DC2DIFFMODE=false;
            DC2CONSTMODE=false;
            defineProperty(DC2ControlNP);
            deleteProperty(DC2diffSETNP);
            deleteProperty(DC2constSETNP);
            LOGF_ERROR("Temp probe 2 not connected, Constant Temperature Mode for DC2 has exited!","Updated");
            LOGF_INFO("You need to insert the probe firmly to the end!","Updated");
            DC2diffSP[DC2_Manual].setState( ISS_ON);
            DC2diffSP[DC2_DPD_Mode].setState( ISS_OFF);
            DC2diffSP[DC2_CT_Mode].setState( ISS_OFF);
            DC2diffSP.setState(IPS_OK);
            DC2diffSP.apply();
        }


        //DC3 DEW CONTROL
        if(DC3DIFFMODE==true )
        {
            if(temp3read<ENVMonitorNP[DEW_Point].value+DC3diffSETNP[DC3DIFFSET].value)
                sendCommand("7255");
            else
                sendCommand("7000");
        }
        if(DC3CONSTMODE==true )
        {
            if(temp3read<DC3constSETNP[DC3CONSTSET].value)
                sendCommand("7255");
            else
                sendCommand("7000");
        }
        if (DC3diffSP[DC3_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe3_Temp].value==-127)
        {
            DC3DIFFMODE=false;
            DC3CONSTMODE=false;
            defineProperty(DC3ControlNP);
            deleteProperty(DC3diffSETNP);
            deleteProperty(DC3constSETNP);
            LOGF_ERROR("Temp probe 3 not connected, Dew Point Difference Mode for DC3 has exited!","Updated");
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
        if (DC3diffSP[DC3_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe3_Temp].value==-127)
        {
            DC3DIFFMODE=false;
            DC3CONSTMODE=false;
            defineProperty(DC3ControlNP);
            deleteProperty(DC3diffSETNP);
            deleteProperty(DC3constSETNP);
            LOGF_ERROR("Temp probe 3 not connected, Constant Temperature Mode for DC3 has exited!","Updated");
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



void WandererDewTerminator::updateENV(double temp1,double temp2,double temp3,double DHTH,double DHTT)
{
    ENVMonitorNP[Probe1_Temp].setValue(temp1);
    ENVMonitorNP[Probe2_Temp].setValue(temp2);
    ENVMonitorNP[Probe3_Temp].setValue(temp3);
    ENVMonitorNP[ENV_Humidity].setValue(DHTH);
    ENVMonitorNP[ENV_Temp].setValue(DHTT);
    ENVMonitorNP[DEW_Point].setValue((237.7 * ((17.27 * DHTT) / (237.7 + DHTT) + log((DHTH / 100)))) / (17.27 - ((17.27 * DHTT) / (237.7 + DHTT) + log((DHTH / 100)))));;
    ENVMonitorNP.setState(IPS_OK);
    ENVMonitorNP.apply();

}


void WandererDewTerminator::updatePower(double voltage)
{
    // Power Monitor
    PowerMonitorNP[VOLTAGE].setValue(voltage);

    PowerMonitorNP.setState(IPS_OK);
    PowerMonitorNP.apply();
}


void WandererDewTerminator::updateDC1(int res)
{
    DC1ControlNP[0].setValue(res);
    DC1ControlNP.setState(IPS_OK);
    DC1ControlNP.apply();
}

void WandererDewTerminator::updateDC2(int res)
{
    DC2ControlNP[0].setValue(res);
    DC2ControlNP.setState(IPS_OK);
    DC2ControlNP.apply();
}

void WandererDewTerminator::updateDC3(int res)
{
    DC3ControlNP[0].setValue(res);
    DC3ControlNP.setState(IPS_OK);
    DC3ControlNP.apply();
}





bool WandererDewTerminator::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {

        if(firmware>=20240410)
        {

            LOGF_INFO("Firmware version: %d", firmware);
        }
        else
        {
            LOGF_INFO("The firmware is outdated, please upgrade to the latest firmware, or power reading calibration will be unavailable.","failed");
        }
        defineProperty(PowerMonitorNP);




        defineProperty(DC1diffSP);
        defineProperty(DC2diffSP);
        defineProperty(DC3diffSP);


        //DC1////////////////////
        if(DC1diffSP[DC1_DPD_Mode].getState() == ISS_ON)
        {
            deleteProperty(DC1constSETNP);
            deleteProperty(DC1ControlNP);
            defineProperty(DC1diffSETNP);
        }
        else if(DC1diffSP[DC1_CT_Mode].getState() == ISS_ON)
        {
            deleteProperty(DC1ControlNP);
            deleteProperty(DC1diffSETNP);
            defineProperty(DC1constSETNP);
        }
        else
        {
            defineProperty(DC1ControlNP);
            deleteProperty(DC1diffSETNP);
            deleteProperty(DC1constSETNP);
        }
        //DC2////////////////
        if(DC2diffSP[DC2_DPD_Mode].getState() == ISS_ON)
        {
            deleteProperty(DC2constSETNP);
            deleteProperty(DC2ControlNP);
            defineProperty(DC2diffSETNP);
        }
        else if(DC2diffSP[DC2_CT_Mode].getState() == ISS_ON)
        {
            deleteProperty(DC2ControlNP);
            deleteProperty(DC2diffSETNP);
            defineProperty(DC2constSETNP);
        }
        else
        {
            defineProperty(DC2ControlNP);
            deleteProperty(DC2diffSETNP);
            deleteProperty(DC2constSETNP);
        }
        //DC3////////////////
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


    }
    else
    {


        deleteProperty(PowerMonitorNP);
        deleteProperty(ENVMonitorNP);



        deleteProperty(DC1ControlNP);
        deleteProperty(DC2ControlNP);
        deleteProperty(DC3ControlNP);

        deleteProperty(DC1diffSP);
        deleteProperty(DC1diffSETNP);

        deleteProperty(DC2diffSP);
        deleteProperty(DC2diffSETNP);

        deleteProperty(DC3diffSP);
        deleteProperty(DC3diffSETNP);


    }
    return true;
}

bool WandererDewTerminator::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{


    // DC1 DEW
    if (DC1diffSP.isNameMatch(name))
    {
        DC1diffSP.update(states, names, n);
        DC1diffSP.setState(IPS_ALERT);
        if(DC1diffSP[DC1_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value!=-127&&isnan(ENVMonitorNP[DEW_Point].value)==0)
        {
            DC1DIFFMODE=true;
            DC1CONSTMODE=false;
            deleteProperty(DC1ControlNP);
            deleteProperty(DC1constSETNP);
            defineProperty(DC1diffSETNP);

            DC1diffSETNP.setState(IPS_OK);
            DC1diffSETNP.apply();
            DC1diffSP.setState(IPS_OK) ;
            DC1diffSP.apply();
            LOGF_INFO("Dew Point Difference Mode for DC1 activated! WandererBox will keep the dew heater at the temperature higher than the dew point by the set value.","Updated");
            return true;
        }
        else if(DC1diffSP[DC1_DPD_Mode].getState() == ISS_ON&&(ENVMonitorNP[Probe1_Temp].value==-127||isnan(ENVMonitorNP[DEW_Point].value)==1))
        {
            DC1DIFFMODE=false;
            DC1CONSTMODE=false;
            DC1diffSP[DC1_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC1 activated! Please adjust the duty cycle manually, you can also use DC1 as an ordinary switch.","Updated");
            DC1diffSP.apply();
        }
        else if(DC1diffSP[DC1_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value!=-127)
        {
            DC1CONSTMODE=true;
            DC1DIFFMODE=false;
            deleteProperty(DC1diffSETNP);
            deleteProperty(DC1ControlNP);
            defineProperty(DC1constSETNP);

            DC1constSETNP.setState(IPS_OK);
            DC1constSETNP.apply();
            DC1diffSP.setState(IPS_OK) ;
            DC1diffSP.apply();
            LOGF_INFO("Constant Temperature Mode for DC1 activated! WandererBox will keep the dew heater at the set temperature.","Updated");
            return true;
        }
        else if(DC1diffSP[DC1_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe1_Temp].value==-127)
        {
            DC1DIFFMODE=false;
            DC1CONSTMODE=false;
            DC1diffSP[DC1_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC1 activated! Please adjust the duty cycle manually, you can also use DC1 as an ordinary switch.","Updated");
            DC1diffSP.apply();
        }
        else
        {
            DC1DIFFMODE=false;
            DC1CONSTMODE=false;
            defineProperty(DC1ControlNP);
            deleteProperty(DC1diffSETNP);
            deleteProperty(DC1constSETNP);
            DC1diffSP.setState(IPS_OK) ;
            DC1diffSP.apply();
            LOGF_INFO("Manual Mode for DC1 activated! Please adjust the duty cycle manually, you can also use DC1 as an ordinary switch.","Updated");
            return true;
        }

    }

    // DC2 DEW
    if (DC2diffSP.isNameMatch(name))
    {
        DC2diffSP.update(states, names, n);
        DC2diffSP.setState(IPS_ALERT);
        if(DC2diffSP[DC2_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe2_Temp].value!=-127&&isnan(ENVMonitorNP[DEW_Point].value)==0)
        {
            DC2DIFFMODE=true;
            DC2CONSTMODE=false;
            deleteProperty(DC2ControlNP);
            deleteProperty(DC2constSETNP);
            defineProperty(DC2diffSETNP);

            DC2diffSETNP.setState(IPS_OK);
            DC2diffSETNP.apply();
            DC2diffSP.setState(IPS_OK) ;
            DC2diffSP.apply();
            LOGF_INFO("Dew Point Difference Mode for DC2 activated! WandererBox will keep the dew heater at the temperature higher than the dew point by the set value.","Updated");
            return true;
        }
        else if(DC2diffSP[DC2_DPD_Mode].getState() == ISS_ON&&(ENVMonitorNP[Probe2_Temp].value==-127||isnan(ENVMonitorNP[DEW_Point].value)==1))
        {
            DC2DIFFMODE=false;
            DC2CONSTMODE=false;
            DC2diffSP[DC2_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC2 activated! Please adjust the duty cycle manually, you can also use DC2 as an ordinary switch.","Updated");
            DC2diffSP.apply();
        }
        else if(DC2diffSP[DC2_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe2_Temp].value!=-127)
        {
            DC2CONSTMODE=true;
            DC2DIFFMODE=false;
            deleteProperty(DC2diffSETNP);
            deleteProperty(DC2ControlNP);
            defineProperty(DC2constSETNP);

            DC2constSETNP.setState(IPS_OK);
            DC2constSETNP.apply();
            DC2diffSP.setState(IPS_OK) ;
            DC2diffSP.apply();
            LOGF_INFO("Constant Temperature Mode for DC2 activated! WandererBox will keep the dew heater at the set temperature.","Updated");
            return true;
        }
        else if(DC2diffSP[DC2_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe2_Temp].value==-127)
        {
            DC2DIFFMODE=false;
            DC2CONSTMODE=false;
            DC2diffSP[DC2_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC2 activated! Please adjust the duty cycle manually, you can also use DC2 as an ordinary switch.","Updated");
            DC2diffSP.apply();
        }
        else
        {
            DC2DIFFMODE=false;
            DC2CONSTMODE=false;
            defineProperty(DC2ControlNP);
            deleteProperty(DC2diffSETNP);
            deleteProperty(DC2constSETNP);
            DC2diffSP.setState(IPS_OK) ;
            DC2diffSP.apply();
            LOGF_INFO("Manual Mode for DC2 activated! Please adjust the duty cycle manually, you can also use DC2 as an ordinary switch.","Updated");
            return true;
        }

    }

    // DC3 DEW
    if (DC3diffSP.isNameMatch(name))
    {
        DC3diffSP.update(states, names, n);
        DC3diffSP.setState(IPS_ALERT);
        if(DC3diffSP[DC3_DPD_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe3_Temp].value!=-127&&isnan(ENVMonitorNP[DEW_Point].value)==0)
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
        else if(DC3diffSP[DC3_DPD_Mode].getState() == ISS_ON&&(ENVMonitorNP[Probe3_Temp].value==-127||isnan(ENVMonitorNP[DEW_Point].value)==1))
        {
            DC3DIFFMODE=false;
            DC3CONSTMODE=false;
            DC3diffSP[DC3_Manual].setState( ISS_ON);
            LOGF_INFO("Manual Mode for DC3 activated! Please adjust the duty cycle manually, you can also use DC3 as an ordinary switch.","Updated");
            DC3diffSP.apply();
        }
        else if(DC3diffSP[DC3_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe3_Temp].value!=-127)
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
        else if(DC3diffSP[DC3_CT_Mode].getState() == ISS_ON&&ENVMonitorNP[Probe3_Temp].value==-127)
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

bool WandererDewTerminator::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // DC1
        if (DC1ControlNP.isNameMatch(name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                rc1 = setDewPWM(5, static_cast<uint8_t>(values[i]));
            }

            DC1ControlNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (DC1ControlNP.getState() == IPS_OK)
                DC1ControlNP.update(values, names, n);
            DC1ControlNP.apply();
            return true;
        }
        if (DC1diffSETNP.isNameMatch(name))
        {

            DC1diffSETNP.setState(IPS_OK) ;
            if (DC1diffSETNP.getState() == IPS_OK)
                DC1diffSETNP.update(values, names, n);
            DC1diffSETNP.apply();
            return true;
        }
        if (DC1constSETNP.isNameMatch(name))
        {

            DC1constSETNP.setState(IPS_OK) ;
            if (DC1constSETNP.getState() == IPS_OK)
                DC1constSETNP.update(values, names, n);
            DC1constSETNP.apply();
            return true;
        }
        // DC2
        if (DC2ControlNP.isNameMatch(name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                    rc1 = setDewPWM(6, static_cast<uint8_t>(values[i]));
            }

            DC2ControlNP.setState( (rc1) ? IPS_OK : IPS_ALERT);
            if (DC2ControlNP.getState() == IPS_OK)
                DC2ControlNP.update(values, names, n);
            DC2ControlNP.apply();
            return true;
        }
        if (DC2diffSETNP.isNameMatch(name))
        {

            DC2diffSETNP.setState(IPS_OK) ;
            if (DC2diffSETNP.getState() == IPS_OK)
                DC2diffSETNP.update(values, names, n);
            DC2diffSETNP.apply();
            return true;
        }
        if (DC2constSETNP.isNameMatch(name))
        {

            DC2constSETNP.setState(IPS_OK) ;
            if (DC2constSETNP.getState() == IPS_OK)
                DC2constSETNP.update(values, names, n);
            DC2constSETNP.apply();
            return true;
        }
        // DC3
        if (DC3ControlNP.isNameMatch(name))
        {
            bool rc1 = false;
            for (int i = 0; i < n; i++)
            {
                    rc1 = setDewPWM(7, static_cast<uint8_t>(values[i]));
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


    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool WandererDewTerminator::setDewPWM(uint8_t id, uint8_t value)
{
    char cmd[64] = {0};
    snprintf(cmd, 64, "%d%03d", id, value);
    if (sendCommand(cmd))
    {
        return true;
    }

    return false;
}

const char *WandererDewTerminator::getDefaultName()
{
    return "Wanderer Dew Terminator";
}


bool WandererDewTerminator::sendCommand(std::string command)
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

void WandererDewTerminator::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(2500);
        return;
    }

    getData();
    SetTimer(2500);
}

bool WandererDewTerminator::saveConfigItems(FILE * fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    DC1diffSP.save(fp);
    DC1diffSETNP.save(fp);
    DC1constSETNP.save(fp);
    DC1ControlNP.save(fp);

    DC2diffSP.save(fp);
    DC2diffSETNP.save(fp);
    DC2constSETNP.save(fp);
    DC2ControlNP.save(fp);

    DC3diffSP.save(fp);
    DC3diffSETNP.save(fp);
    DC3constSETNP.save(fp);
    DC3ControlNP.save(fp);


    return true;
}

