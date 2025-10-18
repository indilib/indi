/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  TerransPowerBoxGoV2

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
#include "Terrans_PowerBoxGo_V2.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>

// We declare an auto pointer to TerransPowerBoxGoV2.
static std::unique_ptr<TerransPowerBoxGoV2> terranspowerboxgov2(new TerransPowerBoxGoV2());

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


TerransPowerBoxGoV2::TerransPowerBoxGoV2()
{
    setVersion(1, 0);
}

bool TerransPowerBoxGoV2::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE);

    addAuxControls();
    
    ////////////////////////////////////////////////////////////////////////////
    /// Name
    ////////////////////////////////////////////////////////////////////////////
    char DCANAME[MAXINDINAME] = "DC OUT A";
    char DCBNAME[MAXINDINAME] = "DC OUT B";
    char DCCNAME[MAXINDINAME] = "DC OUT C";
    char DCDNAME[MAXINDINAME] = "DC OUT D";
    char DCENAME[MAXINDINAME] = "DC OUT E";
    char USBANAME[MAXINDINAME] = "USB3.0 A";
    char USBBNAME[MAXINDINAME] = "USB3.0 B";
    char USBENAME[MAXINDINAME] = "USB2.0 E";
    char USBFNAME[MAXINDINAME] = "USB2.0 F";
    
    IUGetConfigText(getDeviceName(), "RENAME", "DC_A_NAME", DCANAME, MAXINDINAME);
    IUGetConfigText(getDeviceName(), "RENAME", "DC_B_NAME", DCBNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "DC_C_NAME", DCCNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "DC_D_NAME", DCDNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "DC_E_NAME", DCENAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "USB_A_NAME", USBANAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "USB_B_NAME", USBBNAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "USB_E_NAME", USBENAME, MAXINDINAME); 
    IUGetConfigText(getDeviceName(), "RENAME", "USB_F_NAME", USBFNAME, MAXINDINAME); 
      
      
    RenameTP[0].fill("DC_A_NAME", "DC A NAME", DCANAME);
    RenameTP[1].fill("DC_B_NAME", "DC B NAME", DCBNAME);
    RenameTP[2].fill("DC_C_NAME", "DC C NAME", DCCNAME);
    RenameTP[3].fill("DC_D_NAME", "DC D NAME", DCDNAME);
    RenameTP[4].fill("DC_E_NAME", "DC E NAME", DCENAME);
    RenameTP[7].fill("USB_A_NAME", "USB A NAME", USBANAME);
    RenameTP[8].fill("USB_B_NAME", "USB B NAME", USBBNAME);
    RenameTP[11].fill("USB_E_NAME", "USB E NAME", USBENAME);
    RenameTP[12].fill("USB_F_NAME", "USB F NAME", USBFNAME);

    RenameTP.fill(getDeviceName(), "RENAME", "Rename",ADD_SETTING_TAB, IP_RW, 60, IPS_IDLE);
                 
    ConfigRenameDCA  = RenameTP[0].text;
    ConfigRenameDCB  = RenameTP[1].text;
    ConfigRenameDCC  = RenameTP[2].text;
    ConfigRenameDCD  = RenameTP[3].text;
    ConfigRenameDCE  = RenameTP[4].text;
    ConfigRenameUSBA  = RenameTP[7].text;
    ConfigRenameUSBB  = RenameTP[8].text;
    ConfigRenameUSBE  = RenameTP[11].text;
    ConfigRenameUSBF  = RenameTP[12].text;
    
    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////
    DCASP[0].fill("DC OUT A ON", "ON", ISS_OFF);
    DCBSP[0].fill("DC OUT B ON", "ON", ISS_OFF);
    DCCSP[0].fill("DC OUT C ON", "ON", ISS_OFF);
    DCDSP[0].fill("DC OUT D ON", "ON", ISS_OFF);
    DCESP[0].fill("DC OUT E ON", "ON", ISS_OFF);
      
    DCASP[1].fill("DC OUT A OFF", "OFF", ISS_OFF);
    DCBSP[1].fill("DC OUT B OFF", "OFF", ISS_OFF); 
    DCCSP[1].fill("DC OUT C OFF", "OFF", ISS_OFF);
    DCDSP[1].fill("DC OUT D OFF", "OFF", ISS_OFF);
    DCESP[1].fill("DC OUT E OFF", "OFF", ISS_OFF);
    
    USBASP[0].fill("USB3.0 A ON", "ON", ISS_OFF);
    USBBSP[0].fill("USB3.0 B ON", "ON", ISS_OFF);    
    USBESP[0].fill("USB2.0 E ON", "ON", ISS_OFF);    
    USBFSP[0].fill("USB2.0 F ON", "ON", ISS_OFF);
    
    USBASP[1].fill("USB3.0 A OFF", "OFF", ISS_OFF);
    USBBSP[1].fill("USB3.0 B OFF", "OFF", ISS_OFF);    
    USBESP[1].fill("USB2.0 E OFF", "OFF", ISS_OFF);    
    USBFSP[1].fill("USB2.0 F OFF", "OFF", ISS_OFF);    
    
    StateSaveSP[0].fill("Save ON", "ON", ISS_OFF);
    StateSaveSP[1].fill("Save OFF", "OFF", ISS_OFF);
       
    DCASP.fill(getDeviceName(), "DC_OUT_A", ConfigRenameDCA, MAIN_CONTROL_TAB,IP_RW, ISR_ATMOST1,60, IPS_IDLE);   
    DCBSP.fill(getDeviceName(), "DC_OUT_B", ConfigRenameDCB, MAIN_CONTROL_TAB,IP_RW, ISR_ATMOST1,60, IPS_IDLE);  
    DCCSP.fill(getDeviceName(), "DC_OUT_C", ConfigRenameDCC, MAIN_CONTROL_TAB,IP_RW, ISR_ATMOST1,60, IPS_IDLE);   
    DCDSP.fill(getDeviceName(), "DC_OUT_D", ConfigRenameDCD, MAIN_CONTROL_TAB,IP_RW, ISR_ATMOST1,60, IPS_IDLE);  
    DCESP.fill(getDeviceName(), "DC_OUT_E", ConfigRenameDCE, MAIN_CONTROL_TAB,IP_RW, ISR_ATMOST1,60, IPS_IDLE);                            
                      
    USBASP.fill(getDeviceName(),"USB3.0_A", ConfigRenameUSBA, MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    USBBSP.fill(getDeviceName(),"USB3.0_B", ConfigRenameUSBB, MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    USBESP.fill(getDeviceName(),"USB2.0_E", ConfigRenameUSBE, MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    USBFSP.fill(getDeviceName(),"USB2.0_F", ConfigRenameUSBF, MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);    
    
    StateSaveSP.fill(getDeviceName(), "State_Save", "State memory", ADD_SETTING_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);                                          
         
    ////////////////////////////////////////////////////////////////////////////
    /// Sensor Data
    ////////////////////////////////////////////////////////////////////////////        
    InputVotageNP[0].fill("Input_Votage", "InputVotage (V)", "%.2f", 0, 20, 0.01, 0);
    InputCurrentNP[0].fill("Input_Current", "InputCurrent (V)", "%.2f", 0, 30, 0.01, 0);
    PowerNP[0].fill("Total_Power", "Total Power (W)", "%.2f", 0, 100, 10, 0);  
    MCUTempNP[0].fill("MCU_Temp", "MCU Temperature (C)", "%.2f", 0, 200, 0.01, 0);   
    
    InputVotageNP.fill(getDeviceName(), "Input_Votage", "InputVotage", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    InputCurrentNP.fill(getDeviceName(), "Input_Current", "InputCurrent", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);  
    PowerNP.fill(getDeviceName(), "Power_Sensor", "Power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);     
    MCUTempNP.fill(getDeviceName(), "MCU_Temp", "MCU", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);      
           
    ////////////////////////////////////////////////////////////////////////////
    /// Serial Connection
    ////////////////////////////////////////////////////////////////////////////
    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&](){return Handshake();});
    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
    registerConnection(serialConnection);

    return true;
}

bool TerransPowerBoxGoV2::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Main Control
        defineProperty(InputVotageNP);
        defineProperty(InputCurrentNP);
        defineProperty(PowerNP);
        defineProperty(MCUTempNP);  
            
        defineProperty(DCASP);
        defineProperty(DCBSP);
        defineProperty(DCCSP);
        defineProperty(DCDSP);
        defineProperty(DCESP);

        defineProperty(USBASP);
        defineProperty(USBBSP);
        defineProperty(USBESP);
        defineProperty(USBFSP);
        
        defineProperty(StateSaveSP);  
        defineProperty(RenameTP);
        
        setupComplete = true;
    }
    else
    {
        // Main Control               
        deleteProperty(InputVotageNP);
        deleteProperty(InputCurrentNP);
        deleteProperty(PowerNP);
        deleteProperty(MCUTempNP);        
        
        deleteProperty(DCASP);
        deleteProperty(DCBSP);
        deleteProperty(DCCSP);
        deleteProperty(DCDSP);
        deleteProperty(DCESP);
        
        deleteProperty(USBASP);
        deleteProperty(USBBSP);
        deleteProperty(USBESP);
        deleteProperty(USBFSP);
                  
        deleteProperty(StateSaveSP);
        deleteProperty(RenameTP);
        
        setupComplete = false;
    }

    return true;
}

bool TerransPowerBoxGoV2::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    RenameTP.save(fp);
    //IUSaveConfigText(fp, RenameTP);
    return true;
}

bool TerransPowerBoxGoV2::Handshake()
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
        if(!strcmp(res, "*TPGNNN"))
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


const char *TerransPowerBoxGoV2::getDefaultName()
{
    LOG_INFO("GET Name");
    return "TerransPowerBoxGoV2";
}

bool TerransPowerBoxGoV2::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processButtonSwitch(dev, name, states, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool TerransPowerBoxGoV2::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Power Labels
        if (RenameTP.isNameMatch(name))
        {
            RenameTP.update(texts, names, n);
            RenameTP.setState(IPS_OK);
            RenameTP.apply();
            if(init){LOG_INFO("Renaming successful, Please click save button in the option menu and restart Ekos to make the rename effective!");}
            return true;        
        }               
    }
 return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool TerransPowerBoxGoV2::sendCommand(const char * cmd, char * res)
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

void TerransPowerBoxGoV2::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(100);
        return;
    }
  
    Get_State();
    SetTimer(100);
}

void TerransPowerBoxGoV2::Get_State()
{    
    char res[CMD_LEN] = {0};
    if(get_count == 0)
    {
       if (sendCommand(">GDA#",res))
       {        
         if(!strcmp(res, "*DA1NNN"))
         {
            DCASP.setState(IPS_OK);
            DCASP[0].setState(ISS_ON);
            DCASP[1].setState(ISS_OFF);
         }else if(!strcmp(res, "*DA0NNN"))
         {
            DCASP.setState(IPS_ALERT);
            DCASP[0].setState(ISS_OFF);
            DCASP[1].setState(ISS_ON);
         }else
         {
            DCASP.setState(IPS_BUSY);
            DCASP[0].setState(ISS_OFF);
            DCASP[1].setState(ISS_OFF);
         }               
       } 
      DCASP.apply();
      get_count++;
    }else if(get_count == 1)
    {
       if (sendCommand(">GDB#",res))
       {        
         if(!strcmp(res, "*DB1NNN"))
         {
            DCBSP.setState(IPS_OK);
            DCBSP[0].setState(ISS_ON);
            DCBSP[1].setState(ISS_OFF);
         }else if(!strcmp(res, "*DB0NNN"))
         {
            DCBSP.setState(IPS_ALERT);
            DCBSP[0].setState(ISS_OFF);
            DCBSP[1].setState(ISS_ON);
         }else
         {
            DCBSP.setState(IPS_BUSY);
            DCBSP[0].setState(ISS_OFF);
            DCBSP[1].setState(ISS_OFF);
         }               
       } 
      DCBSP.apply();
      get_count++;
    }else if(get_count == 2)
    {
       if (sendCommand(">GDC#",res))
       {        
         if(!strcmp(res, "*DC1NNN"))
         {
            DCCSP.setState(IPS_OK);
            DCCSP[0].setState(ISS_ON);
            DCCSP[1].setState(ISS_OFF);
         }else if(!strcmp(res, "*DC0NNN"))
         {
            DCCSP.setState(IPS_ALERT);
            DCCSP[0].setState(ISS_OFF);
            DCCSP[1].setState(ISS_ON);
         }else
         {
            DCCSP.setState(IPS_BUSY);
            DCCSP[0].setState(ISS_OFF);
            DCCSP[1].setState(ISS_OFF);
         }               
       } 
      DCCSP.apply();
      get_count++;
    }else if(get_count == 3)
    {
       if (sendCommand(">GDD#",res))
       {        
         if(!strcmp(res, "*DD1NNN"))
         {
            DCDSP.setState(IPS_OK);
            DCDSP[0].setState(ISS_ON);
            DCDSP[1].setState(ISS_OFF);
         }else if(!strcmp(res, "*DD0NNN"))
         {
            DCDSP.setState(IPS_ALERT);
            DCDSP[0].setState(ISS_OFF);
            DCDSP[1].setState(ISS_ON);
         }else
         {
            DCDSP.setState(IPS_BUSY);
            DCDSP[0].setState(ISS_OFF);
            DCDSP[1].setState(ISS_OFF);
         }               
       } 
      DCDSP.apply();
      get_count++;
    }else if(get_count == 4)
    {
       if (sendCommand(">GDE#",res))
       {        
         if(!strcmp(res, "*DE1NNN"))
         {
            DCESP.setState(IPS_OK);
            DCESP[0].setState(ISS_ON);
            DCESP[1].setState(ISS_OFF);
         }else if(!strcmp(res, "*DE0NNN"))
         {
            DCESP.setState(IPS_ALERT);
            DCESP[0].setState(ISS_OFF);
            DCESP[1].setState(ISS_ON);
         }else
         {
            DCESP.setState(IPS_BUSY);
            DCESP[0].setState(ISS_OFF);
            DCESP[1].setState(ISS_OFF);
         }               
       } 
      DCESP.apply();
      get_count++;
    }else if(get_count == 5)
    {
      if (sendCommand(">GUA#",res))
      {        
        if(!strcmp(res, "*UA111N"))
        {
          USBASP.setState(IPS_OK);
          USBASP[0].setState(ISS_ON);
          USBASP[1].setState(ISS_OFF);
        }else if(!strcmp(res, "*UA000N"))
        {
          USBASP.setState(IPS_ALERT);
          USBASP[0].setState(ISS_OFF);
          USBASP[1].setState(ISS_ON);
        }else
        {
          USBASP.setState(IPS_BUSY);
          USBASP[0].setState(ISS_OFF);
          USBASP[1].setState(ISS_OFF);
        }               
      } 
      USBASP.apply();
      get_count++;
    }else if(get_count == 6)
    {
      if (sendCommand(">GUB#",res))
      {        
        if(!strcmp(res, "*UB111N"))
        {
          USBBSP.setState(IPS_OK);
          USBBSP[0].setState(ISS_ON);
          USBBSP[1].setState(ISS_OFF);
        }else if(!strcmp(res, "*UB000N"))
        {
          USBBSP.setState(IPS_ALERT);
          USBBSP[0].setState(ISS_OFF);
          USBBSP[1].setState(ISS_ON);
        }else
        {
          USBBSP.setState(IPS_BUSY);
          USBBSP[0].setState(ISS_OFF);
          USBBSP[1].setState(ISS_OFF);
        }               
      } 
      USBBSP.apply();
      get_count++;
    }else if(get_count == 7)
    {
      if (sendCommand(">GUE#",res))
      {        
        if(!strcmp(res, "*UE11NN"))
        {
          USBESP.setState(IPS_OK);
          USBESP[0].setState(ISS_ON);
          USBESP[1].setState(ISS_OFF);
        }else if(!strcmp(res, "*UE00NN"))
        {
          USBESP.setState(IPS_ALERT);
          USBESP[0].setState(ISS_OFF);
          USBESP[1].setState(ISS_ON);
        }else
        {
          USBESP.setState(IPS_BUSY);
          USBESP[0].setState(ISS_OFF);
          USBESP[1].setState(ISS_OFF);
        }               
      } 
      USBESP.apply();
      get_count++;
    }else if(get_count == 8)
    {
      if (sendCommand(">GUF#",res))
      {        
        if(!strcmp(res, "*UF11NN"))
        {
          USBFSP.setState(IPS_OK);
          USBFSP[0].setState(ISS_ON);
          USBFSP[1].setState(ISS_OFF);
        }else if(!strcmp(res, "*UF00NN"))
        {
          USBFSP.setState(IPS_ALERT);
          USBFSP[0].setState(ISS_OFF);
          USBFSP[1].setState(ISS_ON);
        }else
        {
          USBFSP.setState(IPS_BUSY);
          USBFSP[0].setState(ISS_OFF);
          USBFSP[1].setState(ISS_OFF);
        }               
      } 
      USBFSP.apply();
      get_count++;
    }else if(get_count == 9)
    {
      if (sendCommand(">GS#",res))
      {        
        if(!strcmp(res, "*SS1NNN"))
        {
          StateSaveSP.setState(IPS_OK);
          StateSaveSP[0].setState(ISS_ON);
          StateSaveSP[1].setState(ISS_OFF);
        }else if(!strcmp(res, "*SS0NNN"))
        {
          StateSaveSP.setState(IPS_ALERT);
          StateSaveSP[0].setState(ISS_OFF);
          StateSaveSP[1].setState(ISS_ON);
        }else
        {
          StateSaveSP.setState(IPS_BUSY);
          StateSaveSP[0].setState(ISS_OFF);
          StateSaveSP[1].setState(ISS_OFF);
        }               
      } 
      StateSaveSP.apply();
      get_count++; 
    }else if(get_count == 10)
    {
      if (sendCommand(">GPA#",res))
      {        
        ch1_bus = (res[6]-0x30)+((res[5]-0x30)*10)+((res[4]-0x30)*100)+((res[3]-0x30)*1000);
        ch1_bus = ch1_bus * 4 / 1000;
        ch1_w = ch1_current * ch1_bus;
        
        InputVotageNP[0].setValue(ch1_bus);
        PowerNP[0].setValue(ch1_w);
      } 
      
      InputVotageNP.apply();
      PowerNP.apply();
      
      get_count++;
    }else if(get_count == 11)
    {
      if (sendCommand(">GPB#",res))
      {        
        ch1_shuntv = (res[6]-0x30)+((res[5]-0x30)*10)+((res[4]-0x30)*100)+((res[3]-0x30)*1000);
        ch1_current = ch1_shuntv * 10 / 1000000 / 0.002;
        ch1_w = ch1_current * ch1_bus;
        
        InputVotageNP.setState(IPS_OK);
        InputCurrentNP.setState(IPS_OK);
        PowerNP.setState(IPS_OK);
        
        InputCurrentNP[0].setValue(ch1_current);
        InputVotageNP[0].setValue(ch1_bus);
        PowerNP[0].setValue(ch1_w);
      } 
    
      InputVotageNP.apply();
      InputCurrentNP.apply();
      PowerNP.apply();
      
      get_count++;
    }else if(get_count == 12)
    {
      if (sendCommand(">GC#",res))
      {        
        mcu_temp = (res[6]-0x30)+((res[5]-0x30)*10)+((res[4]-0x30)*100)+((res[3]-0x30)*1000);
        if (mcu_temp == 0)
        {
           MCUTempNP[0].setValue(0);     
        }else
        {
          if (res[2] == 'A')
          {
             mcu_temp = mcu_temp / 100;
             MCUTempNP[0].setValue(mcu_temp); 
          
          }else if (res[2] == 'B')
          {
             mcu_temp = mcu_temp / 100;
             mcu_temp = mcu_temp * (-1);
             MCUTempNP[0].setValue(mcu_temp); 
          }
        }
        MCUTempNP.setState(IPS_OK);
        MCUTempNP.apply();
      } 
      get_count = 0;
    }
    init = 1;
}

bool TerransPowerBoxGoV2::processButtonSwitch(const char *dev, const char *name, ISState *states, char *names[],int n)
{
    char res[CMD_LEN] = {0};
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {                    
        if (DCASP.isNameMatch(name))
        {
            DCASP.update(states, names, n);
            if (DCASP[0].getState() == ISS_ON)
            {
              if (sendCommand(">SDA1#",res))
              {        
                if(!strcmp(res, "*DA1NNN"))
                {
                  DCASP.setState(IPS_OK);
                  DCASP[0].setState(ISS_ON);
                  DCASP[1].setState(ISS_OFF); 
                  LOG_INFO("DC A ON");
                }else if(!strcmp(res, "*DA0NNN"))
                {
                  DCASP.setState(IPS_ALERT);
                  DCASP[0].setState(ISS_OFF);
                  DCASP[1].setState(ISS_ON); 
                  LOG_INFO("DC A OFF");
                }else
                {
                  DCASP.setState(IPS_BUSY);
                  DCASP[0].setState(ISS_OFF);
                  DCASP[1].setState(ISS_OFF); 
                  LOG_INFO("DC A Set Fail");
                }               
              }  
            }          
            else if (DCASP[1].getState() == ISS_ON)
            {      
              if (sendCommand(">SDA0#",res))
              {        
                if(!strcmp(res, "*DA1NNN"))
                {
                  DCASP.setState(IPS_OK);
                  DCASP[0].setState(ISS_ON);
                  DCASP[1].setState(ISS_OFF); 
                  LOG_INFO("DC A ON");
                }else if(!strcmp(res, "*DA0NNN"))
                {
                  DCASP.setState(IPS_ALERT);
                  DCASP[0].setState(ISS_OFF);
                  DCASP[1].setState(ISS_ON); 
                  LOG_INFO("DC A OFF");
                }else
                {
                  DCASP.setState(IPS_BUSY);
                  DCASP[0].setState(ISS_OFF);
                  DCASP[1].setState(ISS_OFF); 
                  LOG_INFO("DC A Set Fail");
                }               
              }  
            }             
            DCASP.apply();
            return true;
        }        
        if (DCBSP.isNameMatch(name))
        {
            DCBSP.update(states, names, n);
            if (DCBSP[0].getState() == ISS_ON)
            {
              if (sendCommand(">SDB1#",res))
              {        
                if(!strcmp(res, "*DB1NNN"))
                {
                  DCBSP.setState(IPS_OK);
                  DCBSP[0].setState(ISS_ON);
                  DCBSP[1].setState(ISS_OFF); 
                  LOG_INFO("DC B ON");
                }else if(!strcmp(res, "*DB0NNN"))
                {
                  DCBSP.setState(IPS_ALERT);
                  DCBSP[0].setState(ISS_OFF);
                  DCBSP[1].setState(ISS_ON); 
                  LOG_INFO("DC B OFF");
                }else
                {
                  DCBSP.setState(IPS_BUSY);
                  DCBSP[0].setState(ISS_OFF);
                  DCBSP[1].setState(ISS_OFF); 
                  LOG_INFO("DC B Set Fail");
                }               
              } 
            }          
            else if (DCBSP[1].getState() == ISS_ON)
            {
              if (sendCommand(">SDB0#",res))
              {        
                if(!strcmp(res, "*DB1NNN"))
                {
                  DCBSP.setState(IPS_OK);
                  DCBSP[0].setState(ISS_ON);
                  DCBSP[1].setState(ISS_OFF); 
                  LOG_INFO("DC B ON");
                }else if(!strcmp(res, "*DB0NNN"))
                {
                  DCBSP.setState(IPS_ALERT);
                  DCBSP[0].setState(ISS_OFF);
                  DCBSP[1].setState(ISS_ON); 
                  LOG_INFO("DC B OFF");
                }else
                {
                  DCBSP.setState(IPS_BUSY);
                  DCBSP[0].setState(ISS_OFF);
                  DCBSP[1].setState(ISS_OFF); 
                  LOG_INFO("DC B Set Fail");
                }               
              } 
            }             
            DCBSP.apply();
            return true;
        }        
        if (DCCSP.isNameMatch(name))
        {
            DCCSP.update(states, names, n);
            if (DCCSP[0].getState() == ISS_ON)
            {
              if (sendCommand(">SDC1#",res))
              {        
                if(!strcmp(res, "*DC1NNN"))
                {
                  DCCSP.setState(IPS_OK);
                  DCCSP[0].setState(ISS_ON);
                  DCCSP[1].setState(ISS_OFF); 
                  LOG_INFO("DC C ON");
                }else if(!strcmp(res, "*DC0NNN"))
                {
                  DCCSP.setState(IPS_ALERT);
                  DCCSP[0].setState(ISS_OFF);
                  DCCSP[1].setState(ISS_ON); 
                  LOG_INFO("DC C OFF");
                }else
                {
                  DCCSP.setState(IPS_BUSY);
                  DCCSP[0].setState(ISS_OFF);
                  DCCSP[1].setState(ISS_OFF); 
                  LOG_INFO("DC C Set Fail");
                }               
              } 
            }          
            else if (DCCSP[1].getState() == ISS_ON)
            {
              if (sendCommand(">SDC0#",res))
              {        
                if(!strcmp(res, "*DC1NNN"))
                {
                  DCCSP.setState(IPS_OK);
                  DCCSP[0].setState(ISS_ON);
                  DCCSP[1].setState(ISS_OFF); 
                  LOG_INFO("DC C ON");
                }else if(!strcmp(res, "*DC0NNN"))
                {
                  DCCSP.setState(IPS_ALERT);
                  DCCSP[0].setState(ISS_OFF);
                  DCCSP[1].setState(ISS_ON); 
                  LOG_INFO("DC C OFF");
                }else
                {
                  DCCSP.setState(IPS_BUSY);
                  DCCSP[0].setState(ISS_OFF);
                  DCCSP[1].setState(ISS_OFF); 
                  LOG_INFO("DC C Set Fail");
                }               
              } 
            }             
            DCCSP.apply();
            return true;
        }       
        if (DCDSP.isNameMatch(name))
        {
            DCDSP.update(states, names, n);
            if (DCDSP[0].getState() == ISS_ON)
            {
              if (sendCommand(">SDD1#",res))
              {        
                if(!strcmp(res, "*DD1NNN"))
                {
                  DCDSP.setState(IPS_OK);
                  DCDSP[0].setState(ISS_ON);
                  DCDSP[1].setState(ISS_OFF); 
                  LOG_INFO("DC D ON");
                }else if(!strcmp(res, "*DD0NNN"))
                {
                  DCDSP.setState(IPS_ALERT);
                  DCDSP[0].setState(ISS_OFF);
                  DCDSP[1].setState(ISS_ON); 
                  LOG_INFO("DC D OFF");
                }else
                {
                  DCDSP.setState(IPS_BUSY);
                  DCDSP[0].setState(ISS_OFF);
                  DCDSP[1].setState(ISS_OFF); 
                  LOG_INFO("DC D Set Fail");
                }               
              } 
            }          
            else if (DCDSP[1].getState() == ISS_ON)
            {
              if (sendCommand(">SDD0#",res))
              {        
                if(!strcmp(res, "*DD1NNN"))
                {
                  DCDSP.setState(IPS_OK);
                  DCDSP[0].setState(ISS_ON);
                  DCDSP[1].setState(ISS_OFF); 
                  LOG_INFO("DC D ON");
                }else if(!strcmp(res, "*DD0NNN"))
                {
                  DCDSP.setState(IPS_ALERT);
                  DCDSP[0].setState(ISS_OFF);
                  DCDSP[1].setState(ISS_ON); 
                  LOG_INFO("DC D OFF");
                }else
                {
                  DCDSP.setState(IPS_BUSY);
                  DCDSP[0].setState(ISS_OFF);
                  DCDSP[1].setState(ISS_OFF); 
                  LOG_INFO("DC D Set Fail");
                }               
              } 
            }             
            DCDSP.apply();
            return true;
        }       
        if (DCESP.isNameMatch(name))
        {
            DCESP.update(states, names, n);
            if (DCESP[0].getState() == ISS_ON)
            {
              if (sendCommand(">SDE1#",res))
              {        
                if(!strcmp(res, "*DE1NNN"))
                {
                  DCESP.setState(IPS_OK);
                  DCESP[0].setState(ISS_ON);
                  DCESP[1].setState(ISS_OFF); 
                  LOG_INFO("DC E ON");
                }else if(!strcmp(res, "*DE0NNN"))
                {
                  DCESP.setState(IPS_ALERT);
                  DCESP[0].setState(ISS_OFF);
                  DCESP[1].setState(ISS_ON); 
                  LOG_INFO("DC E OFF");
                }else
                {
                  DCESP.setState(IPS_BUSY);
                  DCESP[0].setState(ISS_OFF);
                  DCESP[1].setState(ISS_OFF); 
                  LOG_INFO("DC E Set Fail");
                }               
              } 
            }          
            else if (DCESP[1].getState() == ISS_ON)
            {
              if (sendCommand(">SDE0#",res))
              {        
                if(!strcmp(res, "*DE1NNN"))
                {
                  DCESP.setState(IPS_OK);
                  DCESP[0].setState(ISS_ON);
                  DCESP[1].setState(ISS_OFF); 
                  LOG_INFO("DC E ON");
                }else if(!strcmp(res, "*DE0NNN"))
                {
                  DCESP.setState(IPS_ALERT);
                  DCESP[0].setState(ISS_OFF);
                  DCESP[1].setState(ISS_ON); 
                  LOG_INFO("DC E OFF");
                }else
                {
                  DCESP.setState(IPS_BUSY);
                  DCESP[0].setState(ISS_OFF);
                  DCESP[1].setState(ISS_OFF); 
                  LOG_INFO("DC E Set Fail");
                }               
              } 
            }             
            DCESP.apply();
            return true;
        }                            
        if (USBASP.isNameMatch(name))
        {
            USBASP.update(states, names, n);
            if (USBASP[0].getState() == ISS_ON)
            {
              if (sendCommand(">SUA1A#",res))
              {        
                if(!strcmp(res, "*UA111N"))
                {
                  USBASP.setState(IPS_OK);
                  USBASP[0].setState(ISS_ON);
                  USBASP[1].setState(ISS_OFF);
                  LOG_INFO("USB A ON");
                }else if(!strcmp(res, "*UA000N"))
                {
                  USBASP.setState(IPS_ALERT);
                  USBASP[0].setState(ISS_OFF);
                  USBASP[1].setState(ISS_ON);
                  LOG_INFO("USB A OFF");
                }else
                {
                  USBASP.setState(IPS_BUSY);
                  USBASP[0].setState(ISS_OFF);
                  USBASP[1].setState(ISS_OFF);
                  LOG_INFO("USB A Set Fail");
                }               
              } 
            }          
            else if (USBASP[1].getState() == ISS_ON)
            {
              if (sendCommand(">SUA0A#",res))
              {        
                if(!strcmp(res, "*UA111N"))
                {
                  USBASP.setState(IPS_OK);
                  USBASP[0].setState(ISS_ON);
                  USBASP[1].setState(ISS_OFF);
                  LOG_INFO("USB A ON");
                }else if(!strcmp(res, "*UA000N"))
                {
                  USBASP.setState(IPS_ALERT);
                  USBASP[0].setState(ISS_OFF);
                  USBASP[1].setState(ISS_ON);
                  LOG_INFO("USB A OFF");
                }else
                {
                  USBASP.setState(IPS_BUSY);
                  USBASP[0].setState(ISS_OFF);
                  USBASP[1].setState(ISS_OFF);
                  LOG_INFO("USB A Set Fail");
                }               
              } 
            }             
            USBASP.apply();
            return true;
        }
        if (USBBSP.isNameMatch(name))
        {
            USBBSP.update(states, names, n);
            if (USBBSP[0].getState() == ISS_ON)
            {
              if (sendCommand(">SUB1A#",res))
              {        
                if(!strcmp(res, "*UB111N"))
                {
                  USBBSP.setState(IPS_OK);
                  USBBSP[0].setState(ISS_ON);
                  USBBSP[1].setState(ISS_OFF);
                  LOG_INFO("USB B ON");
                }else if(!strcmp(res, "*UB000N"))
                {
                  USBBSP.setState(IPS_ALERT);
                  USBBSP[0].setState(ISS_OFF);
                  USBBSP[1].setState(ISS_ON);
                  LOG_INFO("USB B OFF");
                }else
                {
                  USBBSP.setState(IPS_BUSY);
                  USBBSP[0].setState(ISS_OFF);
                  USBBSP[1].setState(ISS_OFF);
                  LOG_INFO("USB B Set Fail");
                }               
              } 
            }          
            else if (USBBSP[1].getState() == ISS_ON)
            {
              if (sendCommand(">SUB0A#",res))
              {        
                if(!strcmp(res, "*UB111N"))
                {
                  USBBSP.setState(IPS_OK);
                  USBBSP[0].setState(ISS_ON);
                  USBBSP[1].setState(ISS_OFF);
                  LOG_INFO("USB B ON");
                }else if(!strcmp(res, "*UB000N"))
                {
                  USBBSP.setState(IPS_ALERT);
                  USBBSP[0].setState(ISS_OFF);
                  USBBSP[1].setState(ISS_ON);
                  LOG_INFO("USB B OFF");
                }else
                {
                  USBBSP.setState(IPS_BUSY);
                  USBBSP[0].setState(ISS_OFF);
                  USBBSP[1].setState(ISS_OFF);
                  LOG_INFO("USB B Set Fail");
                }               
              } 
            }             
            USBBSP.apply();
            return true;
        }
        if (USBESP.isNameMatch(name))
        {
            USBESP.update(states, names, n);
            if (USBESP[0].getState() == ISS_ON)
            {
              if (sendCommand(">SUE1A#",res))
              {        
                if(!strcmp(res, "*UE11NN"))
                {
                  USBESP.setState(IPS_OK);
                  USBESP[0].setState(ISS_ON);
                  USBESP[1].setState(ISS_OFF);
                  LOG_INFO("USB E ON");
                }else if(!strcmp(res, "*UE00NN"))
                {
                  USBESP.setState(IPS_ALERT);
                  USBESP[0].setState(ISS_OFF);
                  USBESP[1].setState(ISS_ON);
                  LOG_INFO("USB E OFF");
                }else
                {
                  USBESP.setState(IPS_BUSY);
                  USBESP[0].setState(ISS_OFF);
                  USBESP[1].setState(ISS_OFF);
                  LOG_INFO("USB E Set Fail");
                }               
              } 
            }          
            else if (USBESP[1].getState() == ISS_ON)
            {
              if (sendCommand(">SUE0A#",res))
              {        
                if(!strcmp(res, "*UE11NN"))
                {
                  USBESP.setState(IPS_OK);
                  USBESP[0].setState(ISS_ON);
                  USBESP[1].setState(ISS_OFF);
                  LOG_INFO("USB E ON");
                }else if(!strcmp(res, "*UE00NN"))
                {
                  USBESP.setState(IPS_ALERT);
                  USBESP[0].setState(ISS_OFF);
                  USBESP[1].setState(ISS_ON);
                  LOG_INFO("USB E OFF");
                }else
                {
                  USBESP.setState(IPS_BUSY);
                  USBESP[0].setState(ISS_OFF);
                  USBESP[1].setState(ISS_OFF);
                  LOG_INFO("USB E Set Fail");
                }               
              } 
            }             
            USBESP.apply();
            return true;
        }
        if (USBFSP.isNameMatch(name))
        {
            USBFSP.update(states, names, n);
            if (USBFSP[0].getState() == ISS_ON)
            {
              if (sendCommand(">SUF1A#",res))
              {        
                if(!strcmp(res, "*UF11NN"))
                {
                  USBFSP.setState(IPS_OK);
                  USBFSP[0].setState(ISS_ON);
                  USBFSP[1].setState(ISS_OFF);
                  LOG_INFO("USB F ON");
                }else if(!strcmp(res, "*UF00NN"))
                {
                  USBFSP.setState(IPS_ALERT);
                  USBFSP[0].setState(ISS_OFF);
                  USBFSP[1].setState(ISS_ON);
                  LOG_INFO("USB F OFF");
                }else
                {
                  USBFSP.setState(IPS_BUSY);
                  USBFSP[0].setState(ISS_OFF);
                  USBFSP[1].setState(ISS_OFF);
                  LOG_INFO("USB F Set Fail");
                }               
              } 
            }          
            else if (USBFSP[1].getState() == ISS_ON)
            {
              if (sendCommand(">SUF0A#",res))
              {        
                if(!strcmp(res, "*UF11NN"))
                {
                  USBFSP.setState(IPS_OK);
                  USBFSP[0].setState(ISS_ON);
                  USBFSP[1].setState(ISS_OFF);
                  LOG_INFO("USB F ON");
                }else if(!strcmp(res, "*UF00NN"))
                {
                  USBFSP.setState(IPS_ALERT);
                  USBFSP[0].setState(ISS_OFF);
                  USBFSP[1].setState(ISS_ON);
                  LOG_INFO("USB F OFF");
                }else
                {
                  USBFSP.setState(IPS_BUSY);
                  USBFSP[0].setState(ISS_OFF);
                  USBFSP[1].setState(ISS_OFF);
                  LOG_INFO("USB F Set Fail");
                }               
              } 
            }             
            USBFSP.apply();
            return true;
        }      
        if (StateSaveSP.isNameMatch(name))
        {
            StateSaveSP.update(states, names, n);
            if (StateSaveSP[0].getState() == ISS_ON)
            {
              if (sendCommand(">SS1#",res))
              {        
                if(!strcmp(res, "*SS1NNN"))
                {
                  StateSaveSP.setState(IPS_OK);
                  StateSaveSP[0].setState(ISS_ON);
                  StateSaveSP[1].setState(ISS_OFF);
                  LOG_INFO("Save Switch State Enable");
                }else if(!strcmp(res, "*SS0NNN"))
                {
                  StateSaveSP.setState(IPS_ALERT);
                  StateSaveSP[0].setState(ISS_OFF);
                  StateSaveSP[1].setState(ISS_ON);
                  LOG_INFO("Save Switch State Disable");
                }else
                {
                  StateSaveSP.setState(IPS_BUSY);
                  StateSaveSP[0].setState(ISS_OFF);
                  StateSaveSP[1].setState(ISS_OFF);
                  LOG_INFO("Save Switch State Set Fail");
                }               
              } 
            }          
            else if (StateSaveSP[1].getState() == ISS_ON)
            {
              if (sendCommand(">SS0#",res))
              {        
                if(!strcmp(res, "*SS1NNN"))
                {
                  StateSaveSP.setState(IPS_OK);
                  StateSaveSP[0].setState(ISS_ON);
                  StateSaveSP[1].setState(ISS_OFF);
                  LOG_INFO("Save Switch State Enable");
                }else if(!strcmp(res, "*SS0NNN"))
                {
                  StateSaveSP.setState(IPS_ALERT);
                  StateSaveSP[0].setState(ISS_OFF);
                  StateSaveSP[1].setState(ISS_ON);
                  LOG_INFO("Save Switch State Disable");
                }else
                {
                  StateSaveSP.setState(IPS_BUSY);
                  StateSaveSP[0].setState(ISS_OFF);
                  StateSaveSP[1].setState(ISS_OFF);
                  LOG_INFO("Save Switch State Set Fail");
                }               
              } 
            }             
            StateSaveSP.apply();
            return true;
        }         
      }
      return false;
}
