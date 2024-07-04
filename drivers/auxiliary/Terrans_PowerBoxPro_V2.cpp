/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  TerransPowerBoxProV2

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

#include "config.h"
#include "Terrans_PowerBoxPro_V2.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>

// We declare an auto pointer to TerransPowerBoxProV2.
static std::unique_ptr<TerransPowerBoxProV2> terranspowerboxprov2(new TerransPowerBoxProV2());

#define CMD_LEN 8
#define TIMEOUT 500
#define TAB_INFO "Status"
#define TAB_RENAME "Rename"

int get_count=0;
int init=0;
double ch1_shuntv = 0;
double ch2_shuntv = 0;
double ch3_shuntv = 0;
double ch1_current = 0;
double ch2_current = 0;
double ch3_current = 0;
double ch1_bus = 0;
double ch2_bus = 0;
double ch3_bus = 0;
double ch1_w = 0;
double ch2_w = 0;
double ch3_w = 0;
double chusb_w = 0;
double humidity = 0;
double temperature = 0;
double dewPoint=0;
double mcu_temp = 0;


TerransPowerBoxProV2::TerransPowerBoxProV2() : WI(this)
{
    setVersion(1, 0);
}

bool TerransPowerBoxProV2::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | WEATHER_INTERFACE);

    WI::initProperties(ENVIRONMENT_TAB, ENVIRONMENT_TAB);

    addAuxControls();
    
    ////////////////////////////////////////////////////////////////////////////
    /// Name
    ////////////////////////////////////////////////////////////////////////////
    char DCANAME[MAXINDINAME] = "DC OUT A";
    char DCBNAME[MAXINDINAME] = "DC OUT B";
    char DCCNAME[MAXINDINAME] = "DC OUT C";
    char DCDNAME[MAXINDINAME] = "DC OUT D";
    char DCENAME[MAXINDINAME] = "DC OUT E";
    char DCFNAME[MAXINDINAME] = "DC OUT F";
    char DC19VNAME[MAXINDINAME] = "DC OUT 19V";
    char USBANAME[MAXINDINAME] = "USB3.0 A";
    char USBBNAME[MAXINDINAME] = "USB3.0 B";
    char USBCNAME[MAXINDINAME] = "USB3.0 C";
    char USBDNAME[MAXINDINAME] = "USB3.0 D";
    char USBENAME[MAXINDINAME] = "USB2.0 E";
    char USBFNAME[MAXINDINAME] = "USB2.0 F";
    char DCADJNAME[MAXINDINAME] = "DC ADJ";
    
    IUGetConfigText(getDeviceName(), "RENAME", "DC_A_NAME", DCANAME, MAXINDINAME);
    IUGetConfigText(getDeviceName(), "RENAME", "DC_B_NAME", DCBNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "DC_C_NAME", DCCNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "DC_D_NAME", DCDNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "DC_E_NAME", DCENAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "DC_F_NAME", DCFNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "DC_19V_NAME", DC19VNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "USB_A_NAME", USBANAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "USB_B_NAME", USBBNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "USB_C_NAME", USBCNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "USB_D_NAME", USBDNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "USB_E_NAME", USBENAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "USB_F_NAME", USBFNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "DC_ADJ_NAME", DCADJNAME, MAXINDINAME); 
      
    IUFillText(&RenameT[0], "DC_A_NAME", "DC A NAME", DCANAME);
    IUFillText(&RenameT[1], "DC_B_NAME", "DC B NAME", DCBNAME);
    IUFillText(&RenameT[2], "DC_C_NAME", "DC CNAME", DCCNAME);  
    IUFillText(&RenameT[3], "DC_D_NAME", "DC D NAME", DCDNAME); 
    IUFillText(&RenameT[4], "DC_E_NAME", "DC E NAME", DCENAME); 
    IUFillText(&RenameT[5], "DC_F_NAME", "DC F NAME", DCFNAME); 
    IUFillText(&RenameT[6], "DC_19V_NAME", "DC 19V NAME", DC19VNAME); 
    IUFillText(&RenameT[7], "USB_A_NAME", "USB A NAME", USBANAME); 
    IUFillText(&RenameT[8], "USB_B_NAME", "USB B NAME", USBBNAME); 
    IUFillText(&RenameT[9], "USB_C_NAME", "USB C NAME", USBCNAME); 
    IUFillText(&RenameT[10], "USB_D_NAME", "USB D NAME", USBDNAME); 
    IUFillText(&RenameT[11], "USB_E_NAME", "USB E NAME", USBENAME); 
    IUFillText(&RenameT[12], "USB_F_NAME", "USB F NAME", USBFNAME); 
    IUFillText(&RenameT[13], "DC_ADJ_NAME", "DC ADJ NAME", DCADJNAME);     
    
    IUFillTextVector(&RenameTP, RenameT, 14, getDeviceName(), "RENAME", "Rename",
                     ADD_SETTING_TAB, IP_RW, 60, IPS_IDLE);
              
    ConfigRenameDCA  = RenameT[0].text;
    ConfigRenameDCB  = RenameT[1].text;
    ConfigRenameDCC  = RenameT[2].text;
    ConfigRenameDCD  = RenameT[3].text;
    ConfigRenameDCE  = RenameT[4].text;
    ConfigRenameDCF  = RenameT[5].text;
    ConfigRenameDC19V  = RenameT[6].text;
    ConfigRenameUSBA  = RenameT[7].text;
    ConfigRenameUSBB  = RenameT[8].text;
    ConfigRenameUSBC  = RenameT[9].text;
    ConfigRenameUSBD  = RenameT[10].text;
    ConfigRenameUSBE  = RenameT[11].text;
    ConfigRenameUSBF  = RenameT[12].text;
    ConfigRenameADJ  = RenameT[13].text;
    
    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&DCAS[0], "DC OUT A ON", "ON", ISS_OFF);
    IUFillSwitch(&DCBS[0], "DC OUT B ON", "ON", ISS_OFF);
    IUFillSwitch(&DCCS[0], "DC OUT C ON", "ON", ISS_OFF);
    IUFillSwitch(&DCDS[0], "DC OUT D ON", "ON", ISS_OFF);
    IUFillSwitch(&DCES[0], "DC OUT E ON", "ON", ISS_OFF);
    IUFillSwitch(&DCFS[0], "DC OUT F ON", "ON", ISS_OFF);
    IUFillSwitch(&DC19VS[0], "DC OUT 19V ON", "ON", ISS_OFF);
    
    IUFillSwitch(&DCAS[1], "DC OUT A OFF", "OFF", ISS_OFF);
    IUFillSwitch(&DCBS[1], "DC OUT B OFF", "OFF", ISS_OFF);
    IUFillSwitch(&DCCS[1], "DC OUT C OFF", "OFF", ISS_OFF);
    IUFillSwitch(&DCDS[1], "DC OUT D OFF", "OFF", ISS_OFF);
    IUFillSwitch(&DCES[1], "DC OUT E OFF", "OFF", ISS_OFF);
    IUFillSwitch(&DCFS[1], "DC OUT F OFF", "OFF", ISS_OFF);
    IUFillSwitch(&DC19VS[1], "DC OUT 19V OFF", "OFF", ISS_OFF);
    
    IUFillSwitch(&USBAS[0], "USB3.0 A ON", "ON", ISS_OFF);
    IUFillSwitch(&USBBS[0], "USB3.0 B ON", "ON", ISS_OFF);
    IUFillSwitch(&USBCS[0], "USB3.0 C ON", "ON", ISS_OFF);
    IUFillSwitch(&USBDS[0], "USB3.0 D ON", "ON", ISS_OFF);
    IUFillSwitch(&USBES[0], "USB2.0 E ON", "ON", ISS_OFF);
    IUFillSwitch(&USBFS[0], "USB2.0 F ON", "ON", ISS_OFF);
    
    IUFillSwitch(&USBAS[1], "USB3.0 A OFF", "OFF", ISS_OFF);
    IUFillSwitch(&USBBS[1], "USB3.0 B OFF", "OFF", ISS_OFF);
    IUFillSwitch(&USBCS[1], "USB3.0 C OFF", "OFF", ISS_OFF);
    IUFillSwitch(&USBDS[1], "USB3.0 D OFF", "OFF", ISS_OFF);
    IUFillSwitch(&USBES[1], "USB2.0 E OFF", "OFF", ISS_OFF);
    IUFillSwitch(&USBFS[1], "USB2.0 F OFF", "OFF", ISS_OFF);
    
    
    IUFillSwitch(&DCADJS[0], "DC OUT ADJ OFF", "OFF", ISS_OFF);
    IUFillSwitch(&DCADJS[1], "DC OUT ADJ 5V", "5V", ISS_OFF);   
    IUFillSwitch(&DCADJS[2], "DC OUT ADJ 9V", "9V", ISS_OFF);
    IUFillSwitch(&DCADJS[3], "DC OUT ADJ 12V", "12V", ISS_OFF);
    
    IUFillSwitch(&StateSaveS[0], "Save ON", "ON", ISS_OFF);
    IUFillSwitch(&StateSaveS[1], "Save OFF", "OFF", ISS_OFF);    
    
    IUFillSwitchVector(&DCASP, DCAS, 2, getDeviceName(), "DC_OUT_A", ConfigRenameDCA, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    IUFillSwitchVector(&DCBSP, DCBS, 2, getDeviceName(), "DC_OUT_B", ConfigRenameDCB, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);     
    IUFillSwitchVector(&DCCSP, DCCS, 2, getDeviceName(), "DC_OUT_C", ConfigRenameDCC, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    IUFillSwitchVector(&DCDSP, DCDS, 2, getDeviceName(), "DC_OUT_D", ConfigRenameDCD, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE); 
    IUFillSwitchVector(&DCESP, DCES, 2, getDeviceName(), "DC_OUT_E", ConfigRenameDCE, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    IUFillSwitchVector(&DCFSP, DCFS, 2, getDeviceName(), "DC_OUT_F", ConfigRenameDCF, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);  
    IUFillSwitchVector(&DC19VSP, DC19VS, 2, getDeviceName(), "DC_19V", ConfigRenameDC19V, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    IUFillSwitchVector(&DCADJSP, DCADJS, 4, getDeviceName(), "DC_ADJ", ConfigRenameADJ, MAIN_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);    
                       
    IUFillSwitchVector(&USBASP, USBAS, 2, getDeviceName(), "USB3.0_A", ConfigRenameUSBA, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);  
    IUFillSwitchVector(&USBBSP, USBBS, 2, getDeviceName(), "USB3.0_B", ConfigRenameUSBB, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);                                                       
    IUFillSwitchVector(&USBCSP, USBCS, 2, getDeviceName(), "USB3.0_C", ConfigRenameUSBC, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);  
    IUFillSwitchVector(&USBDSP, USBDS, 2, getDeviceName(), "USB3.0_D", ConfigRenameUSBD, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE); 
    IUFillSwitchVector(&USBESP, USBES, 2, getDeviceName(), "USB2.0_E", ConfigRenameUSBE, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);  
    IUFillSwitchVector(&USBFSP, USBFS, 2, getDeviceName(), "USB2.0_F", ConfigRenameUSBF, MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE); 
                       
    IUFillSwitchVector(&StateSaveSP, StateSaveS, 2, getDeviceName(), "State_Save", "State memory", ADD_SETTING_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);                                             
    
    ////////////////////////////////////////////////////////////////////////////
    /// Auto Heater
    ////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&AutoHeater12VS[0], "HEATER_DCA", "DC A", ISS_OFF);
    IUFillSwitch(&AutoHeater12VS[1], "HEATER_DCB", "DC B", ISS_OFF);
    IUFillSwitch(&AutoHeater12VS[2], "HEATER_DCC", "DC C", ISS_OFF);
    IUFillSwitch(&AutoHeater12VS[3], "HEATER_DCD", "DC D", ISS_OFF);
    IUFillSwitch(&AutoHeater12VS[4], "HEATER_DCE", "DC E", ISS_OFF);
    IUFillSwitch(&AutoHeater12VS[5], "HEATER_DCF", "DC F", ISS_OFF);
    
    IUFillSwitch(&AutoHeater5VS[0], "HEATER_USBA", "USB A", ISS_OFF);
    IUFillSwitch(&AutoHeater5VS[1], "HEATER_USBB", "USB B", ISS_OFF);
    IUFillSwitch(&AutoHeater5VS[2], "HEATER_USBC", "USB C", ISS_OFF);
    IUFillSwitch(&AutoHeater5VS[3], "HEATER_USBD", "USB D", ISS_OFF);
    IUFillSwitch(&AutoHeater5VS[4], "HEATER_USBE", "USB E", ISS_OFF);
    IUFillSwitch(&AutoHeater5VS[5], "HEATER_USBF", "USB F", ISS_OFF);
    
    IUFillSwitchVector(&AutoHeater12VSP, AutoHeater12VS, 6, getDeviceName(), "12V_Auto_Heater", "12V Auto Heater", ADD_SETTING_TAB,
                       IP_RW, ISR_NOFMANY, 60, IPS_IDLE); 
    IUFillSwitchVector(&AutoHeater5VSP, AutoHeater5VS, 6, getDeviceName(), "5V_Auto_Heater", "5V Auto Heater", ADD_SETTING_TAB,
                       IP_RW, ISR_NOFMANY, 60, IPS_IDLE);   
        
    ////////////////////////////////////////////////////////////////////////////
    /// Sensor Data
    ////////////////////////////////////////////////////////////////////////////
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -15, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15);
    
    IUFillNumber(&InputVotageN[0], "Input_Votage", "InputVotage (V)", "%.2f", 0, 20, 0.01, 0);
    IUFillNumber(&InputCurrentN[0], "Input_Current", "InputCurrent (V)", "%.2f", 0, 30, 0.01, 0);
    
    IUFillNumber(&PowerN[0], "Total_Power", "Total Power (W)", "%.2f", 0, 100, 10, 0); 
    IUFillNumber(&PowerN[1], "12V_Power", "12V Power (W)", "%.2f", 0, 200, 0.01, 0);  
    IUFillNumber(&PowerN[2], "19V_Power", "19V Power (W)", "%.2f", 0, 200, 0.01, 0);  
    IUFillNumber(&PowerN[3], "USB_Power", "USB Power (W)", "%.2f", 0, 200, 0.01, 0);      
 
    IUFillNumber(&MCUTempN[0], "MCU_Temp", "MCU Temperature (C)", "%.2f", 0, 200, 0.01, 0);   
    
    IUFillNumberVector(&InputVotageNP, InputVotageN, 1, getDeviceName(), "Input_Votage", "InputVotage", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    IUFillNumberVector(&InputCurrentNP, InputCurrentN, 1, getDeviceName(), "Input_Current", "InputCurrent", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);  
    IUFillNumberVector(&PowerNP, PowerN, 4, getDeviceName(), "Power_Sensor", "Power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);     
    IUFillNumberVector(&MCUTempNP, MCUTempN, 1, getDeviceName(), "MCU_Temp", "MCU", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);        
    
    ////////////////////////////////////////////////////////////////////////////
    /// Serial Connection
    ////////////////////////////////////////////////////////////////////////////
    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&](){return Handshake();});
    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
    registerConnection(serialConnection);

    return true;
}

bool TerransPowerBoxProV2::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Main Control
        defineProperty(&InputVotageNP);
        defineProperty(&InputCurrentNP);
        defineProperty(&PowerNP);
        defineProperty(&MCUTempNP);  
            
        defineProperty(&DCASP);
        defineProperty(&DCBSP);
        defineProperty(&DCCSP);
        defineProperty(&DCDSP);
        defineProperty(&DCESP);
        defineProperty(&DCFSP);
        defineProperty(&DC19VSP);

        defineProperty(&USBASP);
        defineProperty(&USBBSP);
        defineProperty(&USBCSP);
        defineProperty(&USBDSP);
        defineProperty(&USBESP);
        defineProperty(&USBFSP);

        defineProperty(&DCADJSP);
        
        WI::updateProperties();
        
        defineProperty(&AutoHeater12VSP);  
        defineProperty(&AutoHeater5VSP);  
        defineProperty(&StateSaveSP);  
        defineProperty(&RenameTP);
        
        setupComplete = true;
    }
    else
    {
        // Main Control               
        deleteProperty(InputVotageNP.name);
        deleteProperty(InputCurrentNP.name);
        deleteProperty(PowerNP.name);
        deleteProperty(MCUTempNP.name);        
        
        deleteProperty(DCASP.name);
        deleteProperty(DCBSP.name);
        deleteProperty(DCCSP.name);
        deleteProperty(DCDSP.name);
        deleteProperty(DCESP.name);
        deleteProperty(DCFSP.name);
        deleteProperty(DC19VSP.name);
        
        deleteProperty(USBASP.name);
        deleteProperty(USBBSP.name);
        deleteProperty(USBCSP.name);
        deleteProperty(USBDSP.name);
        deleteProperty(USBESP.name);
        deleteProperty(USBFSP.name);
              
        deleteProperty(DCADJSP.name);
        
        WI::updateProperties();
        
        deleteProperty(AutoHeater12VSP.name);
        deleteProperty(AutoHeater5VSP.name);
        deleteProperty(StateSaveSP.name);
        deleteProperty(RenameTP.name);
        
        setupComplete = false;
    }

    return true;
}

bool TerransPowerBoxProV2::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    IUSaveConfigText(fp, &RenameTP);
    return true;
}

bool TerransPowerBoxProV2::Handshake()
{
    char res[CMD_LEN] = {0};
    if (isSimulation())
    {
        LOGF_INFO("Connected successfuly to simulated %s.", getDeviceName());
        return true;
    }

    for(int HS = 0 ; HS < 3 ; HS++)
    {
      if (sendCommand(">VR#",res))
      {        
        if(!strcmp(res, "*TPPNNN"))
        {
          if (sendCommand(">VN#",res))
          {        
            if(!strcmp(res, "*V001"))
            {
               LOG_INFO("Handshake successfully!");
               return true;  
            }else
            {
               LOG_INFO("The firmware version does not match the driver. Please use the latest firmware and driver!");
               return false;              
            }
          } 
        }else
        {
           LOG_INFO("Handshake failed!"); 
           LOG_INFO("Retry...");            
        }
      } 
    }
    LOG_INFO("Handshake failed!");
    return false;   
}


const char *TerransPowerBoxProV2::getDefaultName()
{
    LOG_INFO("GET Name");
    return "TerransPowerBoxProV2";
}

bool TerransPowerBoxProV2::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processButtonSwitch(dev, name, states, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool TerransPowerBoxProV2::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Power Labels
        if (!strcmp(name, RenameTP.name))
        {
            IUUpdateText(&RenameTP, texts, names, n);
            RenameTP.s = IPS_OK;
            IDSetText(&RenameTP, nullptr);
            saveConfig(true, RenameTP.label);
            if(init){LOG_INFO("Renaming successful, please restart Ekos to make the name effective!");}
            return true;        
        }               
    }
 return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool TerransPowerBoxProV2::sendCommand(const char * cmd, char * res)
{

    int nbytes_read = 0, nbytes_written = 0, tty_rc = 0;
    char command[CMD_LEN] = {0};
    PortFD = serialConnection->getPortFD();
    LOGF_DEBUG("CMD <%s>", cmd);

    for (int i = 0; i < 2; i++)
    {
        tcflush(PortFD, TCIOFLUSH);
        snprintf(command, CMD_LEN, "%s", cmd);
        if ( (tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
            continue;

        if (!res)
        {
            tcflush(PortFD, TCIOFLUSH);
            return true;
        }

        if ( (tty_rc = tty_nread_section(PortFD, res, CMD_LEN, '#', TIMEOUT, &nbytes_read)) != TTY_OK || nbytes_read == 1)
            continue;

        tcflush(PortFD, TCIOFLUSH);
        res[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES <%s>", res);
        return true;
    }

    if (tty_rc != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(tty_rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial error: %s", errorMessage);
    }

    return false;
}

void TerransPowerBoxProV2::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(100);
        return;
    }
  
    Get_State();
    SetTimer(100);
}

void TerransPowerBoxProV2::Get_State()
{    
    char res[CMD_LEN] = {0};
    if(get_count == 0)
    {
       if (sendCommand(">GDA#",res))
       {        
         if(!strcmp(res, "*DA1NNN"))
         {
            DCASP.s = IPS_OK;
            DCAS[0].s = ISS_ON;
            DCAS[1].s = ISS_OFF;
         }else if(!strcmp(res, "*DA0NNN"))
         {
            DCASP.s = IPS_ALERT;
            DCAS[0].s = ISS_OFF;
            DCAS[1].s = ISS_ON;
         }else
         {
            DCASP.s = IPS_BUSY;
            DCAS[0].s = ISS_OFF;
            DCAS[1].s = ISS_OFF;
         }               
       } 
      IDSetSwitch(&DCASP, nullptr);
      get_count++;
    }else if(get_count == 1)
    {
       if (sendCommand(">GDB#",res))
       {        
         if(!strcmp(res, "*DB1NNN"))
         {
            DCBSP.s = IPS_OK;
            DCBS[0].s = ISS_ON;
            DCBS[1].s = ISS_OFF;
         }else if(!strcmp(res, "*DB0NNN"))
         {
            DCBSP.s = IPS_ALERT;
            DCBS[0].s = ISS_OFF;
            DCBS[1].s = ISS_ON;
         }else
         {
            DCBSP.s = IPS_BUSY;
            DCBS[0].s = ISS_OFF;
            DCBS[1].s = ISS_OFF;
         }               
       } 
      IDSetSwitch(&DCBSP, nullptr);
      get_count++;
    }else if(get_count == 2)
    {
       if (sendCommand(">GDC#",res))
       {        
         if(!strcmp(res, "*DC1NNN"))
         {
            DCCSP.s = IPS_OK;
            DCCS[0].s = ISS_ON;
            DCCS[1].s = ISS_OFF;
         }else if(!strcmp(res, "*DC0NNN"))
         {
            DCCSP.s = IPS_ALERT;
            DCCS[0].s = ISS_OFF;
            DCCS[1].s = ISS_ON;
         }else
         {
            DCCSP.s = IPS_BUSY;
            DCCS[0].s = ISS_OFF;
            DCCS[1].s = ISS_OFF;
         }               
       } 
      IDSetSwitch(&DCCSP, nullptr);
      get_count++;
    }else if(get_count == 3)
    {
       if (sendCommand(">GDD#",res))
       {        
         if(!strcmp(res, "*DD1NNN"))
         {
            DCDSP.s = IPS_OK;
            DCDS[0].s = ISS_ON;
            DCDS[1].s = ISS_OFF;
         }else if(!strcmp(res, "*DD0NNN"))
         {
            DCDSP.s = IPS_ALERT;
            DCDS[0].s = ISS_OFF;
            DCDS[1].s = ISS_ON;
         }else
         {
            DCDSP.s = IPS_BUSY;
            DCDS[0].s = ISS_OFF;
            DCDS[1].s = ISS_OFF;
         }               
       } 
      IDSetSwitch(&DCDSP, nullptr);
      get_count++;
    }else if(get_count == 4)
    {
       if (sendCommand(">GDE#",res))
       {        
         if(!strcmp(res, "*DE1NNN"))
         {
            DCESP.s = IPS_OK;
            DCES[0].s = ISS_ON;
            DCES[1].s = ISS_OFF;
         }else if(!strcmp(res, "*DE0NNN"))
         {
            DCESP.s = IPS_ALERT;
            DCES[0].s = ISS_OFF;
            DCES[1].s = ISS_ON;
         }else
         {
            DCESP.s = IPS_BUSY;
            DCES[0].s = ISS_OFF;
            DCES[1].s = ISS_OFF;
         }               
       } 
      IDSetSwitch(&DCESP, nullptr);
      get_count++;
    }else if(get_count == 5)
    {
       if (sendCommand(">GDF#",res))
       {        
         if(!strcmp(res, "*DF1NNN"))
         {
            DCFSP.s = IPS_OK;
            DCFS[0].s = ISS_ON;
            DCFS[1].s = ISS_OFF;
         }else if(!strcmp(res, "*DF0NNN"))
         {
            DCFSP.s = IPS_ALERT;
            DCFS[0].s = ISS_OFF;
            DCFS[1].s = ISS_ON;
         }else
         {
            DCFSP.s = IPS_BUSY;
            DCFS[0].s = ISS_OFF;
            DCFS[1].s = ISS_OFF;
         }               
       } 
      IDSetSwitch(&DCFSP, nullptr);
      get_count++;
    }else if(get_count == 6)
    {
      if (sendCommand(">GDG#",res))
      {        
        if(!strcmp(res, "*DG1NNN"))
        {
          DC19VSP.s = IPS_OK;
          DC19VS[0].s = ISS_ON;
          DC19VS[1].s = ISS_OFF;
        }else if(!strcmp(res, "*DG0NNN"))
        {
          DC19VSP.s = IPS_ALERT;
          DC19VS[0].s = ISS_OFF;
          DC19VS[1].s = ISS_ON;
        }else
        {
          DC19VSP.s = IPS_BUSY;
          DC19VS[0].s = ISS_OFF;
          DC19VS[1].s = ISS_OFF;
        }               
      } 
      IDSetSwitch(&DC19VSP, nullptr);
      get_count++;
    }else if(get_count == 7)
    {
      if (sendCommand(">GUA#",res))
      {        
        if(!strcmp(res, "*UA111N"))
        {
          USBASP.s = IPS_OK;
          USBAS[0].s = ISS_ON;
          USBAS[1].s = ISS_OFF;
        }else if(!strcmp(res, "*UA000N"))
        {
          USBASP.s = IPS_ALERT;
          USBAS[0].s = ISS_OFF;
          USBAS[1].s = ISS_ON;
        }else
        {
          USBASP.s = IPS_BUSY;
          USBAS[0].s = ISS_OFF;
          USBAS[1].s = ISS_OFF;
        }               
      } 
      IDSetSwitch(&USBASP, nullptr);
      get_count++;
    }else if(get_count == 8)
    {
      if (sendCommand(">GUB#",res))
      {        
        if(!strcmp(res, "*UB111N"))
        {
          USBBSP.s = IPS_OK;
          USBBS[0].s = ISS_ON;
          USBBS[1].s = ISS_OFF;
        }else if(!strcmp(res, "*UB000N"))
        {
          USBBSP.s = IPS_ALERT;
          USBBS[0].s = ISS_OFF;
          USBBS[1].s = ISS_ON;
        }else
        {
          USBBSP.s = IPS_BUSY;
          USBBS[0].s = ISS_OFF;
          USBBS[1].s = ISS_OFF;
        }               
      } 
      IDSetSwitch(&USBBSP, nullptr);
      get_count++;
    }else if(get_count == 9)
    {
      if (sendCommand(">GUC#",res))
      {        
        if(!strcmp(res, "*UC111N"))
        {
          USBCSP.s = IPS_OK;
          USBCS[0].s = ISS_ON;
          USBCS[1].s = ISS_OFF;
        }else if(!strcmp(res, "*UC000N"))
        {
          USBCSP.s = IPS_ALERT;
          USBCS[0].s = ISS_OFF;
          USBCS[1].s = ISS_ON;
        }else
        {
          USBCSP.s = IPS_BUSY;
          USBCS[0].s = ISS_OFF;
          USBCS[1].s = ISS_OFF;
        }               
      } 
      IDSetSwitch(&USBCSP, nullptr);
      get_count++;
    }else if(get_count == 10)
    {
      if (sendCommand(">GUD#",res))
      {        
        if(!strcmp(res, "*UD111N"))
        {
          USBDSP.s = IPS_OK;
          USBDS[0].s = ISS_ON;
          USBDS[1].s = ISS_OFF;
        }else if(!strcmp(res, "*UD000N"))
        {
          USBDSP.s = IPS_ALERT;
          USBDS[0].s = ISS_OFF;
          USBDS[1].s = ISS_ON;
        }else
        {
          USBDSP.s = IPS_BUSY;
          USBDS[0].s = ISS_OFF;
          USBDS[1].s = ISS_OFF;
        }               
      } 
      IDSetSwitch(&USBDSP, nullptr);
      get_count++;
    }else if(get_count == 11)
    {
      if (sendCommand(">GUE#",res))
      {        
        if(!strcmp(res, "*UE11NN"))
        {
          USBESP.s = IPS_OK;
          USBES[0].s = ISS_ON;
          USBES[1].s = ISS_OFF;
        }else if(!strcmp(res, "*UE00NN"))
        {
          USBESP.s = IPS_ALERT;
          USBES[0].s = ISS_OFF;
          USBES[1].s = ISS_ON;
        }else
        {
          USBESP.s = IPS_BUSY;
          USBES[0].s = ISS_OFF;
          USBES[1].s = ISS_OFF;
        }               
      } 
      IDSetSwitch(&USBESP, nullptr);
      get_count++;
    }else if(get_count == 12)
    {
      if (sendCommand(">GUF#",res))
      {        
        if(!strcmp(res, "*UF11NN"))
        {
          USBFSP.s = IPS_OK;
          USBFS[0].s = ISS_ON;
          USBFS[1].s = ISS_OFF;
        }else if(!strcmp(res, "*UF00NN"))
        {
          USBFSP.s = IPS_ALERT;
          USBFS[0].s = ISS_OFF;
          USBFS[1].s = ISS_ON;
        }else
        {
          USBFSP.s = IPS_BUSY;
          USBFS[0].s = ISS_OFF;
          USBFS[1].s = ISS_OFF;
        }               
      } 
      IDSetSwitch(&USBFSP, nullptr);
      get_count++;
    }else if(get_count == 13)
    {
      if (sendCommand(">GS#",res))
      {        
        if(!strcmp(res, "*SS1NNN"))
        {
          StateSaveSP.s = IPS_OK;
          StateSaveS[0].s = ISS_ON;
          StateSaveS[1].s = ISS_OFF;
        }else if(!strcmp(res, "*SS0NNN"))
        {
          StateSaveSP.s = IPS_ALERT;
          StateSaveS[0].s = ISS_OFF;
          StateSaveS[1].s = ISS_ON;
        }else
        {
          StateSaveSP.s = IPS_BUSY;
          StateSaveS[0].s = ISS_OFF;
          StateSaveS[1].s = ISS_OFF;
        }               
      } 
      IDSetSwitch(&StateSaveSP, nullptr);
      get_count++;
    }else if(get_count == 14)
    {
      if (sendCommand(">GPF#",res))
      {        
        ch3_bus = (res[6]-0x30)+((res[5]-0x30)*10)+((res[4]-0x30)*100)+((res[3]-0x30)*1000);
        ch3_bus = ch3_bus * 8 / 1000;
        ch3_w = ch3_current * ch3_bus;
        chusb_w = ch3_w - ch2_w - ch1_w;
        if (chusb_w <= 0)
        {
          chusb_w = 0;
        }
        
        InputVotageN[0].value = ch3_bus;
        PowerN[0].value = ch3_w;
        PowerN[3].value = chusb_w;
        
        InputVotageNP.s = IPS_OK;
      } 
      IDSetNumber(&InputVotageNP, nullptr);
      IDSetNumber(&PowerNP, nullptr);
      get_count++;
    }else if(get_count == 15)
    {
      if (sendCommand(">GPE#",res))
      {        
        ch3_shuntv = (res[6]-0x30)+((res[5]-0x30)*10)+((res[4]-0x30)*100)+((res[3]-0x30)*1000);
        ch3_current = ch3_shuntv * 40 / 1000000 / 0.01;  
        ch3_w = ch3_current * ch3_bus;
        chusb_w = ch3_w - ch2_w - ch1_w;
        if (chusb_w <= 0)
        {
          chusb_w = 0;
        }
        
        InputCurrentN[0].value = ch3_current;
        PowerN[0].value = ch3_w;
        PowerN[3].value = chusb_w;
        
        InputCurrentNP.s = IPS_OK;
      } 
      IDSetNumber(&InputCurrentNP, nullptr);
      IDSetNumber(&PowerNP, nullptr);
      get_count++;
    }else if(get_count == 16)
    {
      if (sendCommand(">GPC#",res))
      {        
        ch2_shuntv = (res[6]-0x30)+((res[5]-0x30)*10)+((res[4]-0x30)*100)+((res[3]-0x30)*1000);
        ch2_current = ch2_shuntv * 40 / 1000000 / 0.002;
        ch2_w = ch2_current * ch3_bus;
        chusb_w = ch3_w - ch2_w - ch1_w;
        if (chusb_w <= 0)
        {
          chusb_w = 0;
        }
        
        PowerN[2].value = ch2_w;
        PowerN[3].value = chusb_w;
      } 
      IDSetNumber(&PowerNP, nullptr);
      get_count++;
    }else if(get_count == 17)
    {
      if (sendCommand(">GPA#",res))
      {        
        ch1_shuntv = (res[6]-0x30)+((res[5]-0x30)*10)+((res[4]-0x30)*100)+((res[3]-0x30)*1000);
        ch1_current = ch1_shuntv * 40 / 1000000 / 0.002;
        ch1_w = ch1_current * ch3_bus;
        chusb_w = ch3_w - ch2_w - ch1_w;
        if (chusb_w <= 0)
        {
          chusb_w = 0;
        }
        PowerN[1].value = ch1_w;
        PowerN[3].value = chusb_w;
        
        PowerNP.s = IPS_OK;
      } 
      IDSetNumber(&PowerNP, nullptr);
      get_count++;
    }else if(get_count == 18)
    {
      if (sendCommand(">GTC#",res))
      {        
        mcu_temp = (res[6]-0x30)+((res[5]-0x30)*10)+((res[4]-0x30)*100)+((res[3]-0x30)*1000);
        if (mcu_temp == 0)
        {
           MCUTempN[0].value = 0;     
        }else
        {
          if (res[2] == 'A')
          {
             mcu_temp = mcu_temp / 100;
             MCUTempN[0].value = mcu_temp; 
          
          }else if (res[2] == 'B')
          {
             mcu_temp = mcu_temp / 100;
             mcu_temp = mcu_temp * (-1);
             MCUTempN[0].value = mcu_temp; 
          }
        }
        MCUTempNP.s = IPS_OK;
      } 
      IDSetNumber(&MCUTempNP, nullptr);
      get_count++;
    }else if(get_count == 19)
    {
      if (sendCommand(">GTH#",res))
      {        
        humidity = (res[5]-0x30)+((res[4]-0x30)*10)+((res[3]-0x30)*100)+((res[2]-0x30)*1000);
        if (humidity == 0)
           {
              setParameterValue("WEATHER_HUMIDITY", 0);
           }
           else
           {
              humidity = humidity / 100;
              setParameterValue("WEATHER_HUMIDITY", humidity);
          }
      } 
      ParametersNP.apply();
      get_count++;
    }else if(get_count == 20)
    {
      if (sendCommand(">GTT#",res))
      {        
        temperature = (res[6]-0x30)+((res[5]-0x30)*10)+((res[4]-0x30)*100)+((res[3]-0x30)*1000);
        if (temperature == 0)
        {
           setParameterValue("WEATHER_TEMPERATURE", 0);   
        }else
        {
          if (res[2] == 'A')
          {
             temperature = temperature / 100;
             setParameterValue("WEATHER_TEMPERATURE", temperature);       
          }else if (res[2] == 'B')
          {
             temperature = temperature / 100;
             temperature = temperature * (-1);
             setParameterValue("WEATHER_TEMPERATURE", temperature);       
          }
        }
      } 
      ParametersNP.apply();
      get_count++;
    }else if(get_count == 21)
    {
      if (sendCommand(">GTD#",res))
      {        
        dewPoint = (res[6]-0x30)+((res[5]-0x30)*10)+((res[4]-0x30)*100)+((res[3]-0x30)*1000);
        if (dewPoint == 0)
        {
           setParameterValue("WEATHER_DEWPOINT", 0);   
        }else
        {
          if (res[2] == 'A')
          {
             dewPoint = dewPoint / 100;
             setParameterValue("WEATHER_DEWPOINT", dewPoint);       
          }else if (res[2] == 'B')
          {
             dewPoint = dewPoint / 100;
             dewPoint = dewPoint * (-1);
             setParameterValue("WEATHER_DEWPOINT", dewPoint);       
          }
        }
        ParametersNP.setState(IPS_OK);    
      } 
      ParametersNP.apply();  
      get_count++;
    }else if(get_count == 22)
    {
      if (sendCommand(">GHa#",res))
      {     
        if(res[1] == 'a')
        {
          if(res[2]-0x30 == 1){AutoHeater12VS[0].s = ISS_ON;}
          else if(res[2]-0x30 == 0){AutoHeater12VS[0].s = ISS_OFF;}    
          
          if(res[3]-0x30 == 1){AutoHeater12VS[1].s = ISS_ON;}
          else if(res[3]-0x30 == 0){AutoHeater12VS[1].s = ISS_OFF;}       
        
          if(res[4]-0x30 == 1){AutoHeater12VS[2].s = ISS_ON;}
          else if(res[4]-0x30 == 0){AutoHeater12VS[2].s = ISS_OFF;}    
          
          if(res[5]-0x30 == 1){AutoHeater12VS[3].s = ISS_ON;}
          else if(res[5]-0x30 == 0){AutoHeater12VS[3].s = ISS_OFF;}                  
        }
      } else
      {

      }
      IDSetSwitch(&AutoHeater12VSP, nullptr);
      get_count++;        
    }else if(get_count == 23)
    {
      if (sendCommand(">GHb#",res))
      {        
        if(res[1] == 'b')
        {       
          if(res[2]-0x30 == 1){AutoHeater12VS[4].s = ISS_ON;}
          else if(res[2]-0x30 == 0){AutoHeater12VS[4].s = ISS_OFF;}        
        
          if(res[3]-0x30 == 1){AutoHeater12VS[5].s = ISS_ON;}
          else if(res[3]-0x30 == 0){AutoHeater12VS[5].s = ISS_OFF;}   
          
          if(res[4]-0x30 == 1){AutoHeater5VS[0].s = ISS_ON;}
          else if(res[4]-0x30 == 0){AutoHeater5VS[0].s = ISS_OFF;}             

          if(res[5]-0x30 == 1){AutoHeater5VS[1].s = ISS_ON;}
          else if(res[5]-0x30 == 0){AutoHeater5VS[1].s = ISS_OFF;}                  
        }
      } 
      IDSetSwitch(&AutoHeater12VSP, nullptr);
      IDSetSwitch(&AutoHeater5VSP, nullptr);
      get_count++;
    }else if(get_count == 24)
    {
      if (sendCommand(">GHc#",res))
      {        
        if(res[1] == 'c')
        {
          if(res[2]-0x30 == 1){AutoHeater5VS[2].s = ISS_ON;}
          else if(res[2]-0x30 == 0){AutoHeater5VS[2].s = ISS_OFF;}          
        
          if(res[3]-0x30 == 1){AutoHeater5VS[3].s = ISS_ON;}
          else if(res[3]-0x30 == 0){AutoHeater5VS[3].s = ISS_OFF;}          
        
          if(res[4]-0x30 == 1){AutoHeater5VS[4].s = ISS_ON;}
          else if(res[4]-0x30 == 0){AutoHeater5VS[4].s = ISS_OFF;}  
          
          if(res[5]-0x30 == 1){AutoHeater5VS[5].s = ISS_ON;}
          else if(res[5]-0x30 == 0){AutoHeater5VS[5].s = ISS_OFF;}             
        }
      } 
      IDSetSwitch(&AutoHeater5VSP, nullptr);
      get_count = 0;
    }
    init = 1;
}

bool TerransPowerBoxProV2::processButtonSwitch(const char *dev, const char *name, ISState *states, char *names[],int n)
{
    char res[CMD_LEN] = {0};
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {        
        if (!strcmp(AutoHeater12VSP.name, name))
        {
            IUUpdateSwitch(&AutoHeater12VSP, states, names, n);
            if(AutoHeater12VS[0].s == ISS_OFF){sendCommand(">Shax#",nullptr);}else if(AutoHeater12VS[0].s == ISS_ON){sendCommand(">Shay#",nullptr);}
            if(AutoHeater12VS[1].s == ISS_OFF){sendCommand(">Shbx#",nullptr);}else if(AutoHeater12VS[1].s == ISS_ON){sendCommand(">Shby#",nullptr);}
            if(AutoHeater12VS[2].s == ISS_OFF){sendCommand(">Shcx#",nullptr);}else if(AutoHeater12VS[2].s == ISS_ON){sendCommand(">Shcy#",nullptr);}
            if(AutoHeater12VS[3].s == ISS_OFF){sendCommand(">Shdx#",nullptr);}else if(AutoHeater12VS[3].s == ISS_ON){sendCommand(">Shdy#",nullptr);}
            if(AutoHeater12VS[4].s == ISS_OFF){sendCommand(">Shex#",nullptr);}else if(AutoHeater12VS[4].s == ISS_ON){sendCommand(">Shey#",nullptr);}
            if(AutoHeater12VS[5].s == ISS_OFF){sendCommand(">Shfx#",nullptr);}else if(AutoHeater12VS[5].s == ISS_ON){sendCommand(">Shfy#",nullptr);}
            AutoHeater12VSP.s = IPS_OK;
            IDSetSwitch(&AutoHeater12VSP, nullptr);   
            LOG_INFO("12V Auto Heater Set");
        }

        if (!strcmp(AutoHeater5VSP.name, name))
        {
            IUUpdateSwitch(&AutoHeater5VSP, states, names, n);                      
            if(AutoHeater5VS[0].s == ISS_OFF){sendCommand(">ShAx#",nullptr);}else if(AutoHeater5VS[0].s == ISS_ON){sendCommand(">ShAy#",nullptr);}
            if(AutoHeater5VS[1].s == ISS_OFF){sendCommand(">ShBx#",nullptr);}else if(AutoHeater5VS[1].s == ISS_ON){sendCommand(">ShBy#",nullptr);}
            if(AutoHeater5VS[2].s == ISS_OFF){sendCommand(">ShCx#",nullptr);}else if(AutoHeater5VS[2].s == ISS_ON){sendCommand(">ShCy#",nullptr);}
            if(AutoHeater5VS[3].s == ISS_OFF){sendCommand(">ShDx#",nullptr);}else if(AutoHeater5VS[3].s == ISS_ON){sendCommand(">ShDy#",nullptr);}
            if(AutoHeater5VS[4].s == ISS_OFF){sendCommand(">ShEx#",nullptr);}else if(AutoHeater5VS[4].s == ISS_ON){sendCommand(">ShEy#",nullptr);}
            if(AutoHeater5VS[5].s == ISS_OFF){sendCommand(">ShFx#",nullptr);}else if(AutoHeater5VS[5].s == ISS_ON){sendCommand(">ShFy#",nullptr);}
            AutoHeater5VSP.s = IPS_OK;
            IDSetSwitch(&AutoHeater5VSP, nullptr);  
            LOG_INFO("5V Auto Heater Set");
        }
            
        if (!strcmp(DCASP.name, name))
        {
            IUUpdateSwitch(&DCASP, states, names, n);
            if (DCAS[0].s == ISS_ON)
            {
              if (sendCommand(">SDA1#",res))
              {        
                if(!strcmp(res, "*DA1NNN"))
                {
                  DCASP.s = IPS_OK;
                  DCAS[0].s = ISS_ON;
                  DCAS[1].s = ISS_OFF;
                  LOG_INFO("DC A ON");
                }else if(!strcmp(res, "*DA0NNN"))
                {
                  DCASP.s = IPS_ALERT;
                  DCAS[0].s = ISS_OFF;
                  DCAS[1].s = ISS_ON;
                  LOG_INFO("DC A OFF");
                }else
                {
                  DCASP.s = IPS_BUSY;
                  DCAS[0].s = ISS_OFF;
                  DCAS[1].s = ISS_OFF;
                  LOG_INFO("DC A Set Fail");
                }               
              }  
            }          
            else if (DCAS[1].s == ISS_ON)
            {      
              if (sendCommand(">SDA0#",res))
              {        
                if(!strcmp(res, "*DA1NNN"))
                {
                  DCASP.s = IPS_OK;
                  DCAS[0].s = ISS_ON;
                  DCAS[1].s = ISS_OFF;
                  LOG_INFO("DC A ON");
                }else if(!strcmp(res, "*DA0NNN"))
                {
                  DCASP.s = IPS_ALERT;
                  DCAS[0].s = ISS_OFF;
                  DCAS[1].s = ISS_ON;
                  LOG_INFO("DC A OFF");
                }else
                {
                  DCASP.s = IPS_BUSY;
                  DCAS[0].s = ISS_OFF;
                  DCAS[1].s = ISS_OFF;
                  LOG_INFO("DC A Set Fail");
                }               
              }  
            }             
            IDSetSwitch(&DCASP, nullptr);
            return true;
        }        
        if (!strcmp(DCBSP.name, name))
        {
            IUUpdateSwitch(&DCBSP, states, names, n);
            if (DCBS[0].s == ISS_ON)
            {
              if (sendCommand(">SDB1#",res))
              {        
                if(!strcmp(res, "*DB1NNN"))
                {
                  DCBSP.s = IPS_OK;
                  DCBS[0].s = ISS_ON;
                  DCBS[1].s = ISS_OFF;
                  LOG_INFO("DC B ON");
                }else if(!strcmp(res, "*DB0NNN"))
                {
                  DCBSP.s = IPS_ALERT;
                  DCBS[0].s = ISS_OFF;
                  DCBS[1].s = ISS_ON;
                  LOG_INFO("DC B OFF");
                }else
                {
                  DCBSP.s = IPS_BUSY;
                  DCBS[0].s = ISS_OFF;
                  DCBS[1].s = ISS_OFF;
                  LOG_INFO("DC B Set Fail");
                }               
              } 
            }          
            else if (DCBS[1].s == ISS_ON)
            {
              if (sendCommand(">SDB0#",res))
              {        
                if(!strcmp(res, "*DB1NNN"))
                {
                  DCBSP.s = IPS_OK;
                  DCBS[0].s = ISS_ON;
                  DCBS[1].s = ISS_OFF;
                  LOG_INFO("DC B ON");
                }else if(!strcmp(res, "*DB0NNN"))
                {
                  DCBSP.s = IPS_ALERT;
                  DCBS[0].s = ISS_OFF;
                  DCBS[1].s = ISS_ON;
                  LOG_INFO("DC B OFF");
                }else
                {
                  DCBSP.s = IPS_BUSY;
                  DCBS[0].s = ISS_OFF;
                  DCBS[1].s = ISS_OFF;
                  LOG_INFO("DC B Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&DCBSP, nullptr);
            return true;
        }        
        if (!strcmp(DCCSP.name, name))
        {
            IUUpdateSwitch(&DCCSP, states, names, n);
            if (DCCS[0].s == ISS_ON)
            {
              if (sendCommand(">SDC1#",res))
              {        
                if(!strcmp(res, "*DC1NNN"))
                {
                  DCCSP.s = IPS_OK;
                  DCCS[0].s = ISS_ON;
                  DCCS[1].s = ISS_OFF;
                  LOG_INFO("DC C ON");
                }else if(!strcmp(res, "*DC0NNN"))
                {
                  DCCSP.s = IPS_ALERT;
                  DCCS[0].s = ISS_OFF;
                  DCCS[1].s = ISS_ON;
                  LOG_INFO("DC C OFF");
                }else
                {
                  DCCSP.s = IPS_BUSY;
                  DCCS[0].s = ISS_OFF;
                  DCCS[1].s = ISS_OFF;
                  LOG_INFO("DC C Set Fail");
                }               
              } 
            }          
            else if (DCCS[1].s == ISS_ON)
            {
              if (sendCommand(">SDC0#",res))
              {        
                if(!strcmp(res, "*DC1NNN"))
                {
                  DCCSP.s = IPS_OK;
                  DCCS[0].s = ISS_ON;
                  DCCS[1].s = ISS_OFF;
                  LOG_INFO("DC C ON");
                }else if(!strcmp(res, "*DC0NNN"))
                {
                  DCCSP.s = IPS_ALERT;
                  DCCS[0].s = ISS_OFF;
                  DCCS[1].s = ISS_ON;
                  LOG_INFO("DC C OFF");
                }else
                {
                  DCCSP.s = IPS_BUSY;
                  DCCS[0].s = ISS_OFF;
                  DCCS[1].s = ISS_OFF;
                  LOG_INFO("DC C Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&DCCSP, nullptr);
            return true;
        }       
        if (!strcmp(DCDSP.name, name))
        {
            IUUpdateSwitch(&DCDSP, states, names, n);
            if (DCDS[0].s == ISS_ON)
            {
              if (sendCommand(">SDD1#",res))
              {        
                if(!strcmp(res, "*DD1NNN"))
                {
                  DCDSP.s = IPS_OK;
                  DCDS[0].s = ISS_ON;
                  DCDS[1].s = ISS_OFF;
                  LOG_INFO("DC D ON");
                }else if(!strcmp(res, "*DD0NNN"))
                {
                  DCDSP.s = IPS_ALERT;
                  DCDS[0].s = ISS_OFF;
                  DCDS[1].s = ISS_ON;
                  LOG_INFO("DC D OFF");
                }else
                {
                  DCDSP.s = IPS_BUSY;
                  DCDS[0].s = ISS_OFF;
                  DCDS[1].s = ISS_OFF;
                  LOG_INFO("DC D Set Fail");
                }               
              } 
            }          
            else if (DCDS[1].s == ISS_ON)
            {
              if (sendCommand(">SDD0#",res))
              {        
                if(!strcmp(res, "*DD1NNN"))
                {
                  DCDSP.s = IPS_OK;
                  DCDS[0].s = ISS_ON;
                  DCDS[1].s = ISS_OFF;
                  LOG_INFO("DC D ON");
                }else if(!strcmp(res, "*DD0NNN"))
                {
                  DCDSP.s = IPS_ALERT;
                  DCDS[0].s = ISS_OFF;
                  DCDS[1].s = ISS_ON;
                  LOG_INFO("DC D OFF");
                }else
                {
                  DCDSP.s = IPS_BUSY;
                  DCDS[0].s = ISS_OFF;
                  DCDS[1].s = ISS_OFF;
                  LOG_INFO("DC D Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&DCDSP, nullptr);
            return true;
        }       
        if (!strcmp(DCESP.name, name))
        {
            IUUpdateSwitch(&DCESP, states, names, n);
            if (DCES[0].s == ISS_ON)
            {
              if (sendCommand(">SDE1#",res))
              {        
                if(!strcmp(res, "*DE1NNN"))
                {
                  DCESP.s = IPS_OK;
                  DCES[0].s = ISS_ON;
                  DCES[1].s = ISS_OFF;
                  LOG_INFO("DC E ON");
                }else if(!strcmp(res, "*DE0NNN"))
                {
                  DCESP.s = IPS_ALERT;
                  DCES[0].s = ISS_OFF;
                  DCES[1].s = ISS_ON;
                  LOG_INFO("DC E OFF");
                }else
                {
                  DCESP.s = IPS_BUSY;
                  DCES[0].s = ISS_OFF;
                  DCES[1].s = ISS_OFF;
                  LOG_INFO("DC E Set Fail");
                }               
              } 
            }          
            else if (DCES[1].s == ISS_ON)
            {
              if (sendCommand(">SDE0#",res))
              {        
                if(!strcmp(res, "*DE1NNN"))
                {
                  DCESP.s = IPS_OK;
                  DCES[0].s = ISS_ON;
                  DCES[1].s = ISS_OFF;
                  LOG_INFO("DC E ON");
                }else if(!strcmp(res, "*DE0NNN"))
                {
                  DCESP.s = IPS_ALERT;
                  DCES[0].s = ISS_OFF;
                  DCES[1].s = ISS_ON;
                  LOG_INFO("DC E OFF");
                }else
                {
                  DCESP.s = IPS_BUSY;
                  DCES[0].s = ISS_OFF;
                  DCES[1].s = ISS_OFF;
                  LOG_INFO("DC E Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&DCESP, nullptr);
            return true;
        }       
        if (!strcmp(DCFSP.name, name))
        {
            IUUpdateSwitch(&DCFSP, states, names, n);
            if (DCFS[0].s == ISS_ON)
            {
              if (sendCommand(">SDF1#",res))
              {        
                if(!strcmp(res, "*DF1NNN"))
                {
                  DCFSP.s = IPS_OK;
                  DCFS[0].s = ISS_ON;
                  DCFS[1].s = ISS_OFF;
                  LOG_INFO("DC F ON");
                }else if(!strcmp(res, "*DF0NNN"))
                {
                  DCFSP.s = IPS_ALERT;
                  DCFS[0].s = ISS_OFF;
                  DCFS[1].s = ISS_ON;
                  LOG_INFO("DC F OFF");
                }else
                {
                  DCFSP.s = IPS_BUSY;
                  DCFS[0].s = ISS_OFF;
                  DCFS[1].s = ISS_OFF;
                  LOG_INFO("DC F Set Fail");
                }               
              } 
            }          
            else if (DCFS[1].s == ISS_ON)
            {
              if (sendCommand(">SDF0#",res))
              {        
                if(!strcmp(res, "*DF1NNN"))
                {
                  DCFSP.s = IPS_OK;
                  DCFS[0].s = ISS_ON;
                  DCFS[1].s = ISS_OFF;
                  LOG_INFO("DC F ON");
                }else if(!strcmp(res, "*DF0NNN"))
                {
                  DCFSP.s = IPS_ALERT;
                  DCFS[0].s = ISS_OFF;
                  DCFS[1].s = ISS_ON;
                  LOG_INFO("DC F OFF");
                }else
                {
                  DCFSP.s = IPS_BUSY;
                  DCFS[0].s = ISS_OFF;
                  DCFS[1].s = ISS_OFF;
                  LOG_INFO("DC F Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&DCFSP, nullptr);
            return true;
        }  
        if (!strcmp(DC19VSP.name, name))
        {
            IUUpdateSwitch(&DC19VSP, states, names, n);
            if (DC19VS[0].s == ISS_ON)
            {
              if (sendCommand(">SDG1#",res))
              {        
                if(!strcmp(res, "*DG1NNN"))
                {
                  DC19VSP.s = IPS_OK;
                  DC19VS[0].s = ISS_ON;
                  DC19VS[1].s = ISS_OFF;
                  LOG_INFO("DC 19V ON");
                }else if(!strcmp(res, "*DG0NNN"))
                {
                  DC19VSP.s = IPS_ALERT;
                  DC19VS[0].s = ISS_OFF;
                  DC19VS[1].s = ISS_ON;
                  LOG_INFO("DC 19V OFF");
                }else
                {
                  DC19VSP.s = IPS_BUSY;
                  DC19VS[0].s = ISS_OFF;
                  DC19VS[1].s = ISS_OFF;
                  LOG_INFO("DC 19V Set Fail");
                }               
              } 
            }          
            else if (DC19VS[1].s == ISS_ON)
            {
              if (sendCommand(">SDG0#",res))
              {        
                if(!strcmp(res, "*DG1NNN"))
                {
                  DC19VSP.s = IPS_OK;
                  DC19VS[0].s = ISS_ON;
                  DC19VS[1].s = ISS_OFF;
                  LOG_INFO("DC 19V ON");
                }else if(!strcmp(res, "*DG0NNN"))
                {
                  DC19VSP.s = IPS_ALERT;
                  DC19VS[0].s = ISS_OFF;
                  DC19VS[1].s = ISS_ON;
                  LOG_INFO("DC 19V OFF");
                }else
                {
                  DC19VSP.s = IPS_BUSY;
                  DC19VS[0].s = ISS_OFF;
                  DC19VS[1].s = ISS_OFF;
                  LOG_INFO("DC 19V Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&DC19VSP, nullptr);
            return true;
        }
        if (!strcmp(USBASP.name, name))
        {
            IUUpdateSwitch(&USBASP, states, names, n);
            if (USBAS[0].s == ISS_ON)
            {
              if (sendCommand(">SUA1A#",res))
              {        
                if(!strcmp(res, "*UA111N"))
                {
                  USBASP.s = IPS_OK;
                  USBAS[0].s = ISS_ON;
                  USBAS[1].s = ISS_OFF;
                  LOG_INFO("USB A ON");
                }else if(!strcmp(res, "*UA000N"))
                {
                  USBASP.s = IPS_ALERT;
                  USBAS[0].s = ISS_OFF;
                  USBAS[1].s = ISS_ON;
                  LOG_INFO("USB A OFF");
                }else
                {
                  USBASP.s = IPS_BUSY;
                  USBAS[0].s = ISS_OFF;
                  USBAS[1].s = ISS_OFF;
                  LOG_INFO("USB A Set Fail");
                }               
              } 
            }          
            else if (USBAS[1].s == ISS_ON)
            {
              if (sendCommand(">SUA0A#",res))
              {        
                if(!strcmp(res, "*UA111N"))
                {
                  USBASP.s = IPS_OK;
                  USBAS[0].s = ISS_ON;
                  USBAS[1].s = ISS_OFF;
                  LOG_INFO("USB A ON");
                }else if(!strcmp(res, "*UA000N"))
                {
                  USBASP.s = IPS_ALERT;
                  USBAS[0].s = ISS_OFF;
                  USBAS[1].s = ISS_ON;
                  LOG_INFO("USB A OFF");
                }else
                {
                  USBASP.s = IPS_BUSY;
                  USBAS[0].s = ISS_OFF;
                  USBAS[1].s = ISS_OFF;
                  LOG_INFO("USB A Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&USBASP, nullptr);
            return true;
        }
        if (!strcmp(USBBSP.name, name))
        {
            IUUpdateSwitch(&USBBSP, states, names, n);
            if (USBBS[0].s == ISS_ON)
            {
              if (sendCommand(">SUB1A#",res))
              {        
                if(!strcmp(res, "*UB111N"))
                {
                  USBBSP.s = IPS_OK;
                  USBBS[0].s = ISS_ON;
                  USBBS[1].s = ISS_OFF;
                  LOG_INFO("USB B ON");
                }else if(!strcmp(res, "*UB000N"))
                {
                  USBBSP.s = IPS_ALERT;
                  USBBS[0].s = ISS_OFF;
                  USBBS[1].s = ISS_ON;
                  LOG_INFO("USB B OFF");
                }else
                {
                  USBBSP.s = IPS_BUSY;
                  USBBS[0].s = ISS_OFF;
                  USBBS[1].s = ISS_OFF;
                  LOG_INFO("USB B Set Fail");
                }               
              } 
            }          
            else if (USBBS[1].s == ISS_ON)
            {
              if (sendCommand(">SUB0A#",res))
              {        
                if(!strcmp(res, "*UB111N"))
                {
                  USBBSP.s = IPS_OK;
                  USBBS[0].s = ISS_ON;
                  USBBS[1].s = ISS_OFF;
                  LOG_INFO("USB B ON");
                }else if(!strcmp(res, "*UB000N"))
                {
                  USBBSP.s = IPS_ALERT;
                  USBBS[0].s = ISS_OFF;
                  USBBS[1].s = ISS_ON;
                  LOG_INFO("USB B OFF");
                }else
                {
                  USBBSP.s = IPS_BUSY;
                  USBBS[0].s = ISS_OFF;
                  USBBS[1].s = ISS_OFF;
                  LOG_INFO("USB B Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&USBBSP, nullptr);
            return true;
        }
        if (!strcmp(USBCSP.name, name))
        {
            IUUpdateSwitch(&USBCSP, states, names, n);
            if (USBCS[0].s == ISS_ON)
            {
              if (sendCommand(">SUC1A#",res))
              {        
                if(!strcmp(res, "*UC111N"))
                {
                  USBCSP.s = IPS_OK;
                  USBCS[0].s = ISS_ON;
                  USBCS[1].s = ISS_OFF;
                  LOG_INFO("USB C ON");
                }else if(!strcmp(res, "*UC000N"))
                {
                  USBCSP.s = IPS_ALERT;
                  USBCS[0].s = ISS_OFF;
                  USBCS[1].s = ISS_ON;
                  LOG_INFO("USB C OFF");
                }else
                {
                  USBCSP.s = IPS_BUSY;
                  USBCS[0].s = ISS_OFF;
                  USBCS[1].s = ISS_OFF;
                  LOG_INFO("USB C Set Fail");
                }               
              } 
            }          
            else if (USBCS[1].s == ISS_ON)
            {
              if (sendCommand(">SUC0A#",res))
              {        
                if(!strcmp(res, "*UC111N"))
                {
                  USBCSP.s = IPS_OK;
                  USBCS[0].s = ISS_ON;
                  USBCS[1].s = ISS_OFF;
                  LOG_INFO("USB C ON");
                }else if(!strcmp(res, "*UC000N"))
                {
                  USBCSP.s = IPS_ALERT;
                  USBCS[0].s = ISS_OFF;
                  USBCS[1].s = ISS_ON;
                  LOG_INFO("USB C OFF");
                }else
                {
                  USBCSP.s = IPS_BUSY;
                  USBCS[0].s = ISS_OFF;
                  USBCS[1].s = ISS_OFF;
                  LOG_INFO("USB C Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&USBCSP, nullptr);
            return true;
        }
        if (!strcmp(USBDSP.name, name))
        {
            IUUpdateSwitch(&USBDSP, states, names, n);
            if (USBDS[0].s == ISS_ON)
            {
              if (sendCommand(">SUD1A#",res))
              {        
                if(!strcmp(res, "*UD111N"))
                {
                  USBDSP.s = IPS_OK;
                  USBDS[0].s = ISS_ON;
                  USBDS[1].s = ISS_OFF;
                  LOG_INFO("USB D ON");
                }else if(!strcmp(res, "*UD000N"))
                {
                  USBDSP.s = IPS_ALERT;
                  USBDS[0].s = ISS_OFF;
                  USBDS[1].s = ISS_ON;
                  LOG_INFO("USB D OFF");
                }else
                {
                  USBDSP.s = IPS_BUSY;
                  USBDS[0].s = ISS_OFF;
                  USBDS[1].s = ISS_OFF;
                  LOG_INFO("USB D Set Fail");
                }               
              } 
            }          
            else if (USBDS[1].s == ISS_ON)
            {
              if (sendCommand(">SUD0A#",res))
              {        
                if(!strcmp(res, "*UD111N"))
                {
                  USBDSP.s = IPS_OK;
                  USBDS[0].s = ISS_ON;
                  USBDS[1].s = ISS_OFF;
                  LOG_INFO("USB D ON");
                }else if(!strcmp(res, "*UD000N"))
                {
                  USBDSP.s = IPS_ALERT;
                  USBDS[0].s = ISS_OFF;
                  USBDS[1].s = ISS_ON;
                  LOG_INFO("USB D OFF");
                }else
                {
                  USBDSP.s = IPS_BUSY;
                  USBDS[0].s = ISS_OFF;
                  USBDS[1].s = ISS_OFF;
                  LOG_INFO("USB D Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&USBDSP, nullptr);
            return true;
        }
        if (!strcmp(USBESP.name, name))
        {
            IUUpdateSwitch(&USBESP, states, names, n);
            if (USBES[0].s == ISS_ON)
            {
              if (sendCommand(">SUE1A#",res))
              {        
                if(!strcmp(res, "*UE11NN"))
                {
                  USBESP.s = IPS_OK;
                  USBES[0].s = ISS_ON;
                  USBES[1].s = ISS_OFF;
                  LOG_INFO("USB E ON");
                }else if(!strcmp(res, "*UE00NN"))
                {
                  USBESP.s = IPS_ALERT;
                  USBES[0].s = ISS_OFF;
                  USBES[1].s = ISS_ON;
                  LOG_INFO("USB E OFF");
                }else
                {
                  USBESP.s = IPS_BUSY;
                  USBES[0].s = ISS_OFF;
                  USBES[1].s = ISS_OFF;
                  LOG_INFO("USB E Set Fail");
                }               
              } 
            }          
            else if (USBES[1].s == ISS_ON)
            {
              if (sendCommand(">SUE0A#",res))
              {        
                if(!strcmp(res, "*UE11NN"))
                {
                  USBESP.s = IPS_OK;
                  USBES[0].s = ISS_ON;
                  USBES[1].s = ISS_OFF;
                  LOG_INFO("USB E ON");
                }else if(!strcmp(res, "*UE00NN"))
                {
                  USBESP.s = IPS_ALERT;
                  USBES[0].s = ISS_OFF;
                  USBES[1].s = ISS_ON;
                  LOG_INFO("USB E OFF");
                }else
                {
                  USBESP.s = IPS_BUSY;
                  USBES[0].s = ISS_OFF;
                  USBES[1].s = ISS_OFF;
                  LOG_INFO("USB E Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&USBESP, nullptr);
            return true;
        }
        if (!strcmp(USBFSP.name, name))
        {
            IUUpdateSwitch(&USBFSP, states, names, n);
            if (USBFS[0].s == ISS_ON)
            {
              if (sendCommand(">SUF1A#",res))
              {        
                if(!strcmp(res, "*UF11NN"))
                {
                  USBFSP.s = IPS_OK;
                  USBFS[0].s = ISS_ON;
                  USBFS[1].s = ISS_OFF;
                  LOG_INFO("USB F ON");
                }else if(!strcmp(res, "*UF00NN"))
                {
                  USBFSP.s = IPS_ALERT;
                  USBFS[0].s = ISS_OFF;
                  USBFS[1].s = ISS_ON;
                  LOG_INFO("USB F OFF");
                }else
                {
                  USBFSP.s = IPS_BUSY;
                  USBFS[0].s = ISS_OFF;
                  USBFS[1].s = ISS_OFF;
                  LOG_INFO("USB F Set Fail");
                }               
              } 
            }          
            else if (USBFS[1].s == ISS_ON)
            {
              if (sendCommand(">SUF0A#",res))
              {        
                if(!strcmp(res, "*UF11NN"))
                {
                  USBFSP.s = IPS_OK;
                  USBFS[0].s = ISS_ON;
                  USBFS[1].s = ISS_OFF;
                  LOG_INFO("USB F ON");
                }else if(!strcmp(res, "*UF00NN"))
                {
                  USBFSP.s = IPS_ALERT;
                  USBFS[0].s = ISS_OFF;
                  USBFS[1].s = ISS_ON;
                  LOG_INFO("USB F OFF");
                }else
                {
                  USBFSP.s = IPS_BUSY;
                  USBFS[0].s = ISS_OFF;
                  USBFS[1].s = ISS_OFF;
                  LOG_INFO("USB F Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&USBFSP, nullptr);
            return true;
        }      
        if (!strcmp(StateSaveSP.name, name))
        {
            IUUpdateSwitch(&StateSaveSP, states, names, n);
            if (StateSaveS[0].s == ISS_ON)
            {
              if (sendCommand(">SS1#",res))
              {        
                if(!strcmp(res, "*SS1NNN"))
                {
                  StateSaveSP.s = IPS_OK;
                  StateSaveS[0].s = ISS_ON;
                  StateSaveS[1].s = ISS_OFF;
                  LOG_INFO("Save Switch State Enable");
                }else if(!strcmp(res, "*SS0NNN"))
                {
                  StateSaveSP.s = IPS_ALERT;
                  StateSaveS[0].s = ISS_OFF;
                  StateSaveS[1].s = ISS_ON;
                  LOG_INFO("Save Switch State Disable");
                }else
                {
                  StateSaveSP.s = IPS_BUSY;
                  StateSaveS[0].s = ISS_OFF;
                  StateSaveS[1].s = ISS_OFF;
                  LOG_INFO("Save Switch State Set Fail");
                }               
              } 
            }          
            else if (StateSaveS[1].s == ISS_ON)
            {
              if (sendCommand(">SS0#",res))
              {        
                if(!strcmp(res, "*SS1NNN"))
                {
                  StateSaveSP.s = IPS_OK;
                  StateSaveS[0].s = ISS_ON;
                  StateSaveS[1].s = ISS_OFF;
                  LOG_INFO("Save Switch State Enable");
                }else if(!strcmp(res, "*SS0NNN"))
                {
                  StateSaveSP.s = IPS_ALERT;
                  StateSaveS[0].s = ISS_OFF;
                  StateSaveS[1].s = ISS_ON;
                  LOG_INFO("Save Switch State Disable");
                }else
                {
                  StateSaveSP.s = IPS_BUSY;
                  StateSaveS[0].s = ISS_OFF;
                  StateSaveS[1].s = ISS_OFF;
                  LOG_INFO("Save Switch State Set Fail");
                }               
              } 
            }             
            IDSetSwitch(&StateSaveSP, nullptr);
            return true;
        }         
        if (!strcmp(DCADJSP.name, name))
        {
            IUUpdateSwitch(&DCADJSP, states, names, n);
            if (DCADJS[0].s == ISS_ON)
            {
              sendCommand(">SA10#",nullptr);
              LOG_INFO("DCADJ OFF");
              DCADJSP.s = IPS_ALERT;
            }          
            else if (DCADJS[1].s == ISS_ON)
            {
              sendCommand(">SA20#",nullptr);
              LOG_INFO("DC ADJ 5V");
              DCADJSP.s = IPS_OK;
            } 
            else if (DCADJS[2].s == ISS_ON)
            {
             sendCommand(">SA40#",nullptr);
              LOG_INFO("DC ADJ 9V");
              DCADJSP.s = IPS_OK;
            }  
            else if (DCADJS[3].s == ISS_ON)
            {
              sendCommand(">SA550#",nullptr);
              LOG_INFO("DC ADJ 12V");
              DCADJSP.s = IPS_OK;
            }  
            IDSetSwitch(&DCADJSP, nullptr);
            return true;
        }
      }
      return false;
}
