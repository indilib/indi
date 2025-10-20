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


TerransPowerBoxProV2::TerransPowerBoxProV2() : INDI::PowerInterface(this), INDI::WeatherInterface(this)
{
    setVersion(1, 0);
}

bool TerransPowerBoxProV2::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | POWER_INTERFACE | WEATHER_INTERFACE);

    // Note: PI::initProperties will be called in Handshake() after device identification
    INDI::WeatherInterface::initProperties(MAIN_CONTROL_TAB, MAIN_CONTROL_TAB);

    addAuxControls();
    
    // Retain MCUTempNP as it's not power-related
    IUFillNumber(&MCUTempN[0], "MCU_Temp", "MCU Temperature (C)", "%.2f", 0, 200, 0.01, 0);   
    IUFillNumberVector(&MCUTempNP, MCUTempN, 1, getDeviceName(), "MCU_Temp", "MCU", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);        
    
    ////////////////////////////////////////////////////////////////////////////
    /// Sensor Data
    ////////////////////////////////////////////////////////////////////////////
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -15, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15);

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
        defineProperty(&MCUTempNP);
        INDI::PowerInterface::updateProperties();
        INDI::WeatherInterface::updateProperties();
        setupComplete = true;
    }
    else
    {
        deleteProperty(MCUTempNP.name);
        INDI::PowerInterface::updateProperties();
        INDI::WeatherInterface::updateProperties();
        setupComplete = false;
    }

    return true;
}

bool TerransPowerBoxProV2::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    INDI::PowerInterface::saveConfigItems(fp);
    return true;
}

bool TerransPowerBoxProV2::Handshake()
{
    char res[CMD_LEN] = {0};
    if (isSimulation())
    {
        LOGF_INFO("Connected successfuly to simulated %s.", getDeviceName());
        // Set capabilities and initialize PI properties for simulation
        PI::SetCapability(POWER_HAS_DC_OUT | POWER_HAS_USB_TOGGLE | POWER_HAS_VOLTAGE_SENSOR |
                          POWER_HAS_OVERALL_CURRENT | POWER_HAS_VARIABLE_OUT);
        PI::initProperties(POWER_TAB, 7, 0, 1, 0, 6);
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
               // Set capabilities and initialize PI properties
               PI::SetCapability(POWER_HAS_DC_OUT | POWER_HAS_USB_TOGGLE | POWER_HAS_VOLTAGE_SENSOR |
                                 POWER_HAS_OVERALL_CURRENT | POWER_HAS_VARIABLE_OUT | POWER_HAS_PER_PORT_CURRENT);
               PI::initProperties(POWER_TAB, 7, 0, 1, 0, 6);
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
        // Let PowerInterface handle its switches first
        if (PI::processSwitch(dev, name, states, names, n))
            return true;

        if (processButtonSwitch(dev, name, states, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool TerransPowerBoxProV2::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Let PowerInterface handle its text properties
        if (PI::processText(dev, name, texts, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool TerransPowerBoxProV2::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Let PowerInterface handle its number properties  
        if (PI::processNumber(dev, name, values, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
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
            PI::PowerChannelsSP[0].setState(ISS_ON);
         }else if(!strcmp(res, "*DA0NNN"))
         {
            PI::PowerChannelsSP[0].setState(ISS_OFF);
         }else
         {
            PI::PowerChannelsSP[0].setState(ISS_OFF);
         }               
       } 
      get_count++;
    }else if(get_count == 1)
    {
       if (sendCommand(">GDB#",res))
       {        
         if(!strcmp(res, "*DB1NNN"))
         {
            PI::PowerChannelsSP[1].setState(ISS_ON);
         }else if(!strcmp(res, "*DB0NNN"))
         {
            PI::PowerChannelsSP[1].setState(ISS_OFF);
         }else
         {
            PI::PowerChannelsSP[1].setState(ISS_OFF);
         }               
       } 
      get_count++;
    }else if(get_count == 2)
    {
       if (sendCommand(">GDC#",res))
       {        
         if(!strcmp(res, "*DC1NNN"))
         {
            PI::PowerChannelsSP[2].setState(ISS_ON);
         }else if(!strcmp(res, "*DC0NNN"))
         {
            PI::PowerChannelsSP[2].setState(ISS_OFF);
         }else
         {
            PI::PowerChannelsSP[2].setState(ISS_OFF);
         }               
       } 
      get_count++;
    }else if(get_count == 3)
    {
       if (sendCommand(">GDD#",res))
       {        
         if(!strcmp(res, "*DD1NNN"))
         {
            PI::PowerChannelsSP[3].setState(ISS_ON);
         }else if(!strcmp(res, "*DD0NNN"))
         {
            PI::PowerChannelsSP[3].setState(ISS_OFF);
         }else
         {
            PI::PowerChannelsSP[3].setState(ISS_OFF);
         }               
       } 
      get_count++;
    }else if(get_count == 4)
    {
       if (sendCommand(">GDE#",res))
       {        
         if(!strcmp(res, "*DE1NNN"))
         {
            PI::PowerChannelsSP[4].setState(ISS_ON);
         }else if(!strcmp(res, "*DE0NNN"))
         {
            PI::PowerChannelsSP[4].setState(ISS_OFF);
         }else
         {
            PI::PowerChannelsSP[4].setState(ISS_OFF);
         }               
       } 
      get_count++;
    }else if(get_count == 5)
    {
       if (sendCommand(">GDF#",res))
       {        
         if(!strcmp(res, "*DF1NNN"))
         {
            PI::PowerChannelsSP[5].setState(ISS_ON);
         }else if(!strcmp(res, "*DF0NNN"))
         {
            PI::PowerChannelsSP[5].setState(ISS_OFF);
         }else
         {
            PI::PowerChannelsSP[5].setState(ISS_OFF);
         }               
       } 
      get_count++;
    }else if(get_count == 6)
    {
      if (sendCommand(">GDG#",res))
      {        
        if(!strcmp(res, "*DG1NNN"))
        {
          PI::PowerChannelsSP[6].setState(ISS_ON);
        }else if(!strcmp(res, "*DG0NNN"))
        {
          PI::PowerChannelsSP[6].setState(ISS_OFF);
        }else
        {
          PI::PowerChannelsSP[6].setState(ISS_OFF);
        }               
      } 
      PI::PowerChannelsSP.apply();
      get_count++;
    }else if(get_count == 7)
    {
      if (sendCommand(">GUA#",res))
      {        
        if(!strcmp(res, "*UA111N"))
        {
          PI::USBPortSP[0].setState(ISS_ON);
        }else if(!strcmp(res, "*UA000N"))
        {
          PI::USBPortSP[0].setState(ISS_OFF);
        }else
        {
          PI::USBPortSP[0].setState(ISS_OFF);
        }               
      } 
      get_count++;
    }else if(get_count == 8)
    {
      if (sendCommand(">GUB#",res))
      {        
        if(!strcmp(res, "*UB111N"))
        {
          PI::USBPortSP[1].setState(ISS_ON);
        }else if(!strcmp(res, "*UB000N"))
        {
          PI::USBPortSP[1].setState(ISS_OFF);
        }else
        {
          PI::USBPortSP[1].setState(ISS_OFF);
        }               
      } 
      get_count++;
    }else if(get_count == 9)
    {
      if (sendCommand(">GUC#",res))
      {        
        if(!strcmp(res, "*UC111N"))
        {
          PI::USBPortSP[2].setState(ISS_ON);
        }else if(!strcmp(res, "*UC000N"))
        {
          PI::USBPortSP[2].setState(ISS_OFF);
        }else
        {
          PI::USBPortSP[2].setState(ISS_OFF);
        }               
      } 
      get_count++;
    }else if(get_count == 10)
    {
      if (sendCommand(">GUD#",res))
      {        
        if(!strcmp(res, "*UD111N"))
        {
          PI::USBPortSP[3].setState(ISS_ON);
        }else if(!strcmp(res, "*UD000N"))
        {
          PI::USBPortSP[3].setState(ISS_OFF);
        }else
        {
          PI::USBPortSP[3].setState(ISS_OFF);
        }               
      } 
      get_count++;
    }else if(get_count == 11)
    {
      if (sendCommand(">GUE#",res))
      {        
        if(!strcmp(res, "*UE11NN"))
        {
          PI::USBPortSP[4].setState(ISS_ON);
        }else if(!strcmp(res, "*UE00NN"))
        {
          PI::USBPortSP[4].setState(ISS_OFF);
        }else
        {
          PI::USBPortSP[4].setState(ISS_OFF);
        }               
      } 
      get_count++;
    }else if(get_count == 12)
    {
      if (sendCommand(">GUF#",res))
      {        
        if(!strcmp(res, "*UF11NN"))
        {
          PI::USBPortSP[5].setState(ISS_ON);
        }else if(!strcmp(res, "*UF00NN"))
        {
          PI::USBPortSP[5].setState(ISS_OFF);
        }else
        {
          PI::USBPortSP[5].setState(ISS_OFF);
        }               
      } 
      PI::USBPortSP.apply();
      get_count++;
    }else if(get_count == 13)
    {
      if (sendCommand(">GS#",res))
      {        
        // State save status - could be tracked separately if needed
      } 
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
        
        PI::PowerSensorsNP[PI::SENSOR_VOLTAGE].setValue(ch3_bus);
        PI::PowerSensorsNP[PI::SENSOR_POWER].setValue(ch3_w);
      } 
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
        
        PI::PowerSensorsNP[PI::SENSOR_CURRENT].setValue(ch3_current);
        PI::PowerSensorsNP[PI::SENSOR_POWER].setValue(ch3_w);
        PI::PowerSensorsNP.setState(IPS_OK);
        PI::PowerSensorsNP.apply();
        PI::PowerChannelCurrentNP[2].setValue(ch3_current); // Assuming ch3 is port 2
      } 
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
        
        PI::PowerChannelCurrentNP[1].setValue(ch2_current); // Assuming ch2 is port 1
      } 
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
        PI::PowerChannelCurrentNP[0].setValue(ch1_current); // Assuming ch1 is port 0
        PI::PowerChannelCurrentNP.setState(IPS_OK);
        PI::PowerChannelCurrentNP.apply();
      } 
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
      // Skip auto heater status for now
      get_count++;
    }else if(get_count == 23)
    {
      // Skip auto heater status for now
      get_count++;
    }else if(get_count == 24)
    {
      // Skip auto heater status for now
      get_count = 0;
    }
    init = 1;
}

bool TerransPowerBoxProV2::processButtonSwitch(const char *dev, const char *name, ISState *states, char *names[],int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(states);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
    
    // Auto heater handling would go here if needed
    return false;
}

// Implement PowerInterface virtual methods
bool TerransPowerBoxProV2::SetPowerPort(size_t port, bool enabled)
{
    char res[CMD_LEN] = {0};
    const char* commands[][2] = {
        {">SDA1#", ">SDA0#"},  // Port 0: DC A
        {">SDB1#", ">SDB0#"},  // Port 1: DC B
        {">SDC1#", ">SDC0#"},  // Port 2: DC C
        {">SDD1#", ">SDD0#"},  // Port 3: DC D
        {">SDE1#", ">SDE0#"},  // Port 4: DC E
        {">SDF1#", ">SDF0#"},  // Port 5: DC F
        {">SDG1#", ">SDG0#"}   // Port 6: DC 19V
    };
    
    if (port >= 7)
        return false;
        
    const char* cmd = enabled ? commands[port][0] : commands[port][1];
    
    if (sendCommand(cmd, res))
    {
        // Verify response matches expected state
        return true;
    }
    
    return false;
}

bool TerransPowerBoxProV2::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    INDI_UNUSED(dutyCycle);
    // No dew ports on this device
    return false;
}

bool TerransPowerBoxProV2::SetVariablePort(size_t port, bool enabled, double voltage)
{
    INDI_UNUSED(port);
    
    if (!enabled)
    {
        sendCommand(">SA10#", nullptr);
        return true;
    }
    
    // Map voltage to command
    if (voltage < 7)  // 5V
    {
        sendCommand(">SA20#", nullptr);
    }
    else if (voltage < 10.5)  // 9V
    {
        sendCommand(">SA40#", nullptr);
    }
    else  // 12V
    {
        sendCommand(">SA550#", nullptr);
    }
    
    return true;
}

bool TerransPowerBoxProV2::SetLEDEnabled(bool enabled)
{
    INDI_UNUSED(enabled);
    // Not implemented on this device
    return false;
}

bool TerransPowerBoxProV2::SetAutoDewEnabled(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    // No auto dew on this device
    return false;
}

bool TerransPowerBoxProV2::CyclePower()
{
    // Not implemented on this device
    return false;
}

bool TerransPowerBoxProV2::SetUSBPort(size_t port, bool enabled)
{
    char res[CMD_LEN] = {0};
    const char* commands[][2] = {
        {">SUA1A#", ">SUA0A#"},  // Port 0: USB A
        {">SUB1A#", ">SUB0A#"},  // Port 1: USB B
        {">SUC1A#", ">SUC0A#"},  // Port 2: USB C
        {">SUD1A#", ">SUD0A#"},  // Port 3: USB D
        {">SUE1A#", ">SUE0A#"},  // Port 4: USB E
        {">SUF1A#", ">SUF0A#"}   // Port 5: USB F
    };
    
    if (port >= 6)
        return false;
        
    const char* cmd = enabled ? commands[port][0] : commands[port][1];
    
    if (sendCommand(cmd, res))
    {
        // Verify response matches expected state
        return true;
    }
    
    return false;
}
