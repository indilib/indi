/*******************************************************************************
 Talon6
 Copyright(c) 2017 Rozeware Development Ltd. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/
#include "talon6.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <memory>
#include <indicom.h>
#include <connectionplugins/connectionserial.h>
#include <termios.h>

// We declare an auto pointer to talon6.
std::unique_ptr<Talon6> talon6(new Talon6());

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
        talon6->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        talon6->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        talon6->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        talon6->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    talon6->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice (XMLEle *root)
{
    talon6->ISSnoopDevice(root);
}

Talon6::Talon6()
{
    //Talon6 is a Roll Off Roof. We implement only basic Dome functions to open / close the roof
    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);

    // Talon6 works with serial connection only
    setDomeConnection(CONNECTION_SERIAL);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool Talon6::initProperties()
{

    INDI::Dome::initProperties();
    addDebugControl();
    addAuxControls();
    SetParkDataType(PARK_NONE);  /*!< 2-state parking (Open or Closed only)  */

    // Switch to read parameters from device
    IUFillSwitch(&StatusS[0], "STATUS", "Read", ISS_OFF);
    IUFillSwitchVector(&StatusSP, StatusS, 1, getDeviceName(), "STATUS", "Device Status", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    IUFillNumber(&GoToN[0], "GOTO", "% Open", "%g", 0, 100, 1, 0);
    IUFillNumberVector(&GoToNP, GoToN, 1, getDeviceName(), "PERC_GOTO", "Go To",MAIN_CONTROL_TAB,
                       IP_RW, 0, IPS_IDLE);

    // Firmware version
    IUFillText(&FirmwareVersionT[0], "FIRMWARE_VERSION", "Firmware version", "");
    IUFillTextVector(&FirmwareVersionTP, FirmwareVersionT, 1, getDeviceName(), "FIRMWARE", "Firmware", MAIN_CONTROL_TAB, IP_RO,
                     60, IPS_IDLE);

    // Device status parameters read from device are showed to the user
    IUFillText(&StatusValueT[0], "ROOF_STATUS", "Roof status", "");
    IUFillText(&StatusValueT[1], "LAST_ACTION", "Last Action", "");
    IUFillText(&StatusValueT[2], "CURRENT_POSITION", "Current Position Ticks", "");
    IUFillText(&StatusValueT[3], "REL_POSITION", "Current Position %", "");
    IUFillText(&StatusValueT[4], "POWER", "Power supply", "");
    IUFillText(&StatusValueT[5], "CLOSING_TIMER", "Closing Timer", "");
    IUFillText(&StatusValueT[6], "POWER_LOST_TIMER", "Power Lost Timer", "");
    IUFillText(&StatusValueT[7], "WEATHER_COND_TIMER", "Weather Condition Timer", "");
    //IUFillText(&StatusValueT[8], "SENSOR_STATUS", "Sensors Status", "");
    IUFillTextVector(&StatusValueTP, StatusValueT, 8, getDeviceName(), "STATUSVALUE", "Status Values", MAIN_CONTROL_TAB, IP_RO,
                     60, IPS_IDLE);

    // Encoder Ticks
    IUFillNumber(&EncoderTicksN[0], "ENCODER_TICKS", "Encoder Ticks", "%6.0f", 0, 100000, 1, 0);
    IUFillNumberVector(&EncoderTicksNP, EncoderTicksN, 1, getDeviceName(), "ENCODER_TICKS", "Max Roof Travel", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

    // Switch enable / disable Safety Condition control
    IUFillSwitch(&SafetyS[0], "Enable", "Enable", ISS_OFF);
    IUFillSwitch(&SafetyS[1], "Disable", "Disable", ISS_ON);
    IUFillSwitchVector(&SafetySP, SafetyS, 2, getDeviceName(), "SAFETY", "Safety Conditions", OPTIONS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Sensors
    IUFillLight(&SensorsL[0], "PWL_SENSOR", "Power Lost - PWL", IPS_IDLE);
    IUFillLight(&SensorsL[1], "CWL_SENSOR", "Cloudwatcher Relay Closed - CWL", IPS_IDLE);
    IUFillLight(&SensorsL[2], "MAP_SENSOR", "Mount At Park - MAP", IPS_IDLE);
    IUFillLight(&SensorsL[3], "ROP_SENSOR", "Roof Totally Open - ROP", IPS_IDLE);
    IUFillLight(&SensorsL[4], "RCL_SENSOR", "Roof Totally Closed - RCL", IPS_IDLE);
    IUFillLightVector(&SensorsLP, SensorsL, 5, getDeviceName(), "SENSORS", "Sensors", "Sensors and Switches",  IPS_IDLE);

    // Switches
    IUFillLight(&SwitchesL[0], "OPEN_SWITCH", "Open Switch - OPEN", IPS_IDLE);
    IUFillLight(&SwitchesL[1], "STOP_SWITCH", "Stop Switch - STOP", IPS_IDLE);
    IUFillLight(&SwitchesL[2], "CLOSE_SWITCH", "Close Switch - CLOSE", IPS_IDLE);
    IUFillLight(&SwitchesL[3], "COM_SWITCH", "Direct Command - COM", IPS_IDLE);
    IUFillLight(&SwitchesL[4], "MGM_SWITCH", "Management - MGM", IPS_IDLE);
    IUFillLightVector(&SwitchesLP, SwitchesL, 5, getDeviceName(), "SWITCHES", "Switches", "Sensors and Switches",  IPS_IDLE);

     return true;
}

bool Talon6::Handshake()
{
    if (isSimulation())
    {
        LOGF_INFO("Connected successfully to simulated %s.", getDeviceName());
        return true;
    }

    PortFD = serialConnection->getPortFD();

    return true;
}

Talon6::~Talon6()
{

}

const char * Talon6::getDefaultName()
{
        return (char *)"Talon6";
}

bool Talon6::updateProperties()
{
    INDI::Dome::updateProperties();

     if (isConnected())
     {
        getDeviceStatus();
        getFirmwareVersion();
        defineNumber(&GoToNP);
        defineSwitch(&StatusSP);
        defineSwitch(&SafetySP);
        defineText(&StatusValueTP);
        defineText(&FirmwareVersionTP);
        defineNumber(&EncoderTicksNP);
        defineLight(&SensorsLP);
        defineLight(&SwitchesLP);
     }
     else
     {
         deleteProperty(GoToNP.name);
         deleteProperty(StatusSP.name);
         deleteProperty(SafetySP.name);
         deleteProperty(StatusValueTP.name);
         deleteProperty(FirmwareVersionTP.name);
         deleteProperty(EncoderTicksNP.name);
         deleteProperty(SensorsLP.name);
         deleteProperty(SwitchesLP.name);
     }
     // We do not need some of the properties defined in parent class
     deleteProperty(DomeMotionSP.name);
     deleteProperty(AutoParkSP.name);
     deleteProperty(TelescopeClosedLockTP.name);

    return true;
}

void Talon6::ISGetProperties(const char *dev)
{
    INDI::Dome::ISGetProperties(dev);

    // Load Sync position
    defineNumber(&EncoderTicksNP);
    loadConfig(true, "ENCODER_TICKS");

    // Load Safety configuration
    defineSwitch(&SafetySP);
    loadConfig(true, "SAFETY");
}

bool Talon6::ISNewSwitch(const char *dev,const char *name,ISState *states,char *names[], int n)
{

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Force read the status from device
        if (!strcmp(name, StatusSP.name))
        {
            StatusS[0].s = ISS_OFF;
            StatusSP.s   = IPS_OK;
            IDSetSwitch(&StatusSP, nullptr);

            getDeviceStatus();
        }
        // Safety Condition switch
        if (!strcmp(name, SafetySP.name))
        {
            IUUpdateSwitch(&SafetySP, states, names, n);

            SafetySP.s = IPS_OK;

            if (SafetyS[0].s == ISS_OFF)
                DEBUGF(INDI::Logger::DBG_SESSION, "Warning: Safety conditions are now disabled. You will"
                                           "be able  to freely open and close the roof manually from the driver,"
                                           "even if there is a safety condition active.This may cause damage to your equipment.", NULL);
            else
                DEBUGF(INDI::Logger::DBG_SESSION,  "Safety Conditions are enabled",NULL);

            IDSetSwitch(&SafetySP, nullptr);

            return true;
        }
    }

    return Dome::ISNewSwitch(dev, name, states, names, n);
}

bool Talon6::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
     if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
    }

    return  INDI::Dome::ISNewText(dev, name, texts, names, n);
}

bool Talon6::ISNewNumber(const char *dev,const char *name,double values[],char *names[],int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
   {
        if (!strcmp(GoToNP.name, name))
        {
           IUUpdateNumber(&GoToNP, values, names, n);
           int requestedPos = values[0];
           DomeGoTo(requestedPos);
           // GoToNP.s = IPS_BUSY;
            IDSetNumber(&GoToNP,nullptr);
             return true;
        }
        if (!strcmp(EncoderTicksNP.name, name))
        {
            IUUpdateNumber(&EncoderTicksNP, values, names, n);
            EncoderTicksNP.s = IPS_OK;
            IDSetNumber(&EncoderTicksNP, nullptr);
            return true;
        }
    }

    return INDI::Dome::ISNewNumber(dev,name,values,names,n);
}

bool Talon6::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);

    // Save Sync position
    IUSaveConfigNumber(fp, &EncoderTicksNP);
    // Save Safety condition switch
    IUSaveConfigSwitch(fp,&SafetySP);

    return true;
}

bool Talon6::Disconnect()
{
  return INDI::Dome::Disconnect();
}

// Reads the device status
void Talon6::getDeviceStatus()
{
    if (!isConnected())
        return;

    // &G# is the command to read the status from device
    WriteString("&G#");
}

void Talon6::getFirmwareVersion()
{
    WriteString("&V#");
}

/* Read string from serial connection tty.
 Every string has a # (HEX23) as a trailing char*/
int Talon6::ReadString(char *buf,int size)
{
  int count;
  int bytesRead;
  char a;
  int rc;

  count = 0;
  buf[count] = 0;

  rc = tty_read(PortFD, &a, 1, 2, &bytesRead);

  while(rc == TTY_OK) {

    if((a == '\n')||(a == '\r')) return count;

    buf[count] = a;
    count++;

    if(count == size) return count;
    rc = tty_read(PortFD, &a, 1, 1, &bytesRead);
    //fprintf(stderr,"read returns %d %x\n",rc,a&0xFF);

  }
  if(rc != TTY_OK) {
    //fprintf(stderr,"Readstring returns error %d\n",rc);
    return -1;
  }
  return count;
}

// This function sends command to the device through serial connection
int Talon6::WriteString(const char *buf)
{
    char ReadBuf[40];
    int bytesWritten;
    int rc;

    tty_write(PortFD,buf,strlen(buf),&bytesWritten);
    rc = ReadString(ReadBuf,40);
    ProcessDomeMessage(ReadBuf);

    return rc;
}

void Talon6::TimerHit()
{
    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

     getDeviceStatus();
     SetTimer(1000);

    if (DomeMotionSP.s == IPS_BUSY)
     {
         // Abort called
         if (MotionRequest < 0)
         {
             LOG_INFO("Roof motion is stopped.");
             setDomeState(DOME_IDLE);
             return;
         }
         // Roll off is opening
         if (DomeMotionS[DOME_CW].s == ISS_ON)
         {
             if (fullOpenRoofSwitch == ISS_ON)
             {
                 LOG_INFO("Roof is open.");
                 SetParked(false);
                 return;
             }
         }
         // Roll Off is closing
         else if (DomeMotionS[DOME_CCW].s == ISS_ON)
         {
             if (fullClosedRoofSwitch == ISS_ON )
             {
                 LOG_INFO("Roof is closed.");
                 SetParked(true);
                 return;
             }
         }
     }
}

void Talon6::ProcessDomeMessage(char *buf)
{
    // Only process not empty messages
    // and messages that start with &
  if(buf[0] == 0 || buf[0] != '&')
  {
    //  only process not empty messages and
    //  messages starts with an &

    return;
  }
    //Parse status respnse, second byte contains specs
  if(buf[1] == 'G')
  {

      //Parse first byte that contains Status and Last Action
      int l;
      int lStatus;
      int lLastAction;

      std::string statusString;
      std::string lastActionString;

      //Parse Roof Status
      l = buf[2] & 0xFF & 0x7F;
      lStatus = l >> 4;
      lLastAction = l  & 0x0F;

      switch (lStatus) {
      case 0:
          statusString = "OPEN";
          // If status is OPEN roof is unparked. That doesn t mean it is fully open,
          // it is fully open when % =100 (see below).
          INDI::Dome::setDomeState(DOME_UNPARKED);
          fullClosedRoofSwitch = ISS_OFF;
          break;
      case 1:
          statusString = "CLOSED";
          //if status is CLOSED roof is parked and it is fully closed.
          fullClosedRoofSwitch = ISS_ON;
          fullOpenRoofSwitch = ISS_OFF;
          INDI::Dome::setDomeState(DOME_PARKED);
          break;
      case 2:
          statusString = "OPENING";
          break;
      case 3:
          statusString = "CLOSING";
          break;
      case 4:
          statusString = "ERROR";
          break;
       default:
          statusString = "UNKOWN";

          break;
      }
      char statusChar[statusString.size()+1];
      strcpy(statusChar, statusString.c_str());
      IUSaveText(&StatusValueT[0], statusChar);

      //Parse roof Last Action
      switch (lLastAction) {
      case 0:
          lastActionString = "NONE";
          break;
      case 1:
          lastActionString = "OPEN BY USER";
          break;
      case 2:
          lastActionString = "CLOSE BY USER";
          break;
      case 3:
         //not used;
          break;
      case 4:
          lastActionString = "GO TO BY USER";
          break;
      case 5:
          lastActionString = "CALIBRATE BY USER";
          break;
      case 6:
          lastActionString = "CLOSED DUE TO RAIN - CLOUD";
          break;
      case 7:
          lastActionString = "CLOSE DUE TO POWER DOWN";
          break;
      case 8:
          lastActionString = "CLOSE DUE TO COMMUNICATION LOST";
          break;
      case 9:
          lastActionString = "CLOSE DUE TO INTERNET LOST";
          break;
      case 10:
          lastActionString = "CLOSE DUE TO TIMEOUT EXPIRED";
          break;
      case 11:
          lastActionString = "CLOSE BY MANAGEMENT";
          break;
      case 12:
          lastActionString = "CLOSE BY AUTOMATION";
          break;
      case 13:
          lastActionString = "STOP --MOTOR STALLED";
          break;
      case 14:
          lastActionString = "EMERGENCY STOP";
          break;
      case 15:
          lastActionString = "ORDERED THE MOUNT TO PARK";
          break;
        default:
          lastActionString = "UNKOWN";

          break;
      }

        char lastActionChar[lastActionString.size()+1];
        strcpy(lastActionChar, lastActionString.c_str());
        IUSaveText(&StatusValueT[1], lastActionChar);

        //Parse roof position
        int x1, x2, x3, xx1,xx2,xx3;
        int xxx;
        int xxxp;
        std::stringstream streamx1;
        std::stringstream streamx2;
        std::stringstream streamx3;
        std::string xxxString, xxxpString;

        // Roof position is encoded using a custom 3 hex bytes encoding (see Talon6 documentation).
        x1 = (buf[3] & 0xFF & 0x7F) << 14;
        x2 = (buf[4] & 0xFF & 0x7F) << 7;
        x3 = buf[5] & 0xFF & 0x7F;

        // Hex values are converted to decimals for further elaboration
        streamx1 << x1;
        streamx1 >> std::dec >> xx1;
        streamx2 << x2;
        streamx2 >> std::dec >> xx2;
        streamx3 << x3;
        streamx3 >> std::dec >> xx3;

        // The 3 bytes are packed together to store the absolute position of the roof
        xxx = xx1 +xx2 + xx3;
        xxxp =(int)100*( xxx / EncoderTicksN[0].value);
        if (xxxp == 100){
            // If %=100 then the roof is set to fully open
            fullClosedRoofSwitch =ISS_OFF;
            fullOpenRoofSwitch = ISS_ON;

        }else{
            fullOpenRoofSwitch = ISS_OFF;
        }
        xxxString = std::to_string(xxx);
        xxxpString = std::to_string(xxxp);
        char xxxChar[xxxString.size()+1];
        strcpy(xxxChar, xxxString.c_str());
        char xxxpChar[xxxpString.size()+1];
        strcpy(xxxpChar, xxxpString.c_str());

        IUSaveText(&StatusValueT[2], xxxChar);

        IUSaveText(&StatusValueT[3], xxxpChar);

        //Parse power supply voltage
        int b1,b2,bb1, bb2;
        int  bb;
        std::string bbString;
        std::stringstream stream1;
        std::stringstream stream2;

        // Voltage is encoded using a custom 2 hex bytes encoding (see Talon6 documentation).
        b1 = (buf[6] & 0xFF & 0x7) << 7 ;
        b2 = buf[7] & 0xFF & 0x7F;

        stream1 << b1;
        stream1 >> std::dec >> bb1;
        stream2 << b2;
        stream2 >> std::dec >> bb2;

        // Values are then added. To get tension in V the result is *15/1024 (see Talon6 documentation)
        bb = (bb1+bb2)*15/1024;
        bbString = std::to_string(bb);
        char bbChar[bbString.size()+1];
        strcpy(bbChar, bbString.c_str());

        IUSaveText(&StatusValueT[4], bbChar);

        //Parse closing timer
        int t1, t2, t3, tt1,tt2,tt3;
        int ttt;
        std::stringstream streamt1;
        std::stringstream streamt2;
        std::stringstream streamt3;
        std::string tttString;

        // Closing timer is encoded using a custom 3 hex bytes encoding (see Talon6 documentation).
        t1 = (buf[8] & 0xFF & 0x7F) << 14;
        t2 = (buf[9] & 0xFF & 0x7F) << 7;
        t3 = buf[10] & 0xFF & 0x7F;

        // Hex values are converted to decimals for further elaboration
        streamt1 << t1;
        streamt1 >> std::dec >> tt1;
        streamt2 << t2;
        streamt2 >> std::dec >> tt2;
        streamt3 << t3;
        streamt3 >> std::dec >> tt3;

        // The 3 bytes are packed together
        ttt = tt1 + tt2 +  tt3;
        tttString = std::to_string(ttt);
        char tttChar[tttString.size()+1];
        strcpy(tttChar, tttString.c_str());

        IUSaveText(&StatusValueT[5], tttChar);

        //Parse power lost timer
        int p1, p2, pp1, pp2;
        int pp;
        std::string ppString;
        std::stringstream streamp1;
        std::stringstream streamp2;

        // Power lost timer is encoded using a custom 2 hex bytes encoding (see Talon6 documentation).
        p1 = (buf[11] & 0xFF & 0x7) << 7 ;
        p2 = buf[12] & 0xFF & 0x7F;

         streamp1 << p1;
         streamp1 >> std::dec >> pp1;
         streamp2 << p2;
         streamp2 >> std::dec >> pp2;

        // Values are then added (see Talon6 documentation)
        pp = (pp1+pp2);
        ppString = std::to_string(pp);
        char ppChar[ppString.size()+1];
        strcpy(ppChar, ppString.c_str());

        IUSaveText(&StatusValueT[6], ppChar);

        // Weather Condition timer
        int c1, c2, cc1, cc2;
        int cc;
        std::string ccString;
        std::stringstream streamc1;
        std::stringstream streamc2;

        // Weather Condition timer is encoded using a custom 2 hex bytes encoding (see Talon6 documentation).
        c1 = (buf[13] & 0xFF & 0x7) << 7 ;
        c2 = buf[14] & 0xFF & 0x7F;

         streamc1 << c1;
         streamc1 >> std::dec >> cc1;
         streamc2 << c2;
         streamc2 >> std::dec >> cc2;

        // Values are then added (see Talon6 documentation)
        cc = (cc1+cc2);
        ccString = std::to_string(cc);
        char ccChar[ccString.size()+1];
        strcpy(ccChar, ccString.c_str());

        IUSaveText(&StatusValueT[7], ccChar);

        // Sensor & Switches Status
        int m1, m2;

        //Sensors status  is encoded using a custom 2 hex bytes encoding (see Talon6 documentation).
        m1 = (buf[15] & 0xFF & 0x7) << 7 ; //Switches
        m2 = buf[16] & 0xFF & 0x7F; //Sensors

       // fprintf(stderr,"Readstring returns buf 15 %x\n",m1);
       // fprintf(stderr,"Readstring returns buf 16 %x\n",m2);

       //First bit contains Power Lost status
       if (m2 & 0x01 ) {
           SensorsL[0].s = IPS_OK;
       }else{
           SensorsL[0].s = IPS_IDLE;
       }
       //Second bit contains Weather Condition status
       if (m2 & 0x02) {
           SensorsL[1].s = IPS_OK;
       }else{
           SensorsL[1].s = IPS_IDLE;
       }
       //Third bit contains Mount At Park status
       if (m2 & 0x04) {
           SensorsL[2].s = IPS_OK;
       }else{
           SensorsL[2].s = IPS_IDLE;
       }
       //Fourth bit contains Open sensor status
       if (m2 & 0x08) {
           SensorsL[3].s = IPS_OK;
       }else{
           SensorsL[3].s = IPS_IDLE;
       }
       //Fifth bit contains Close sensor status
       if (m2 & 0x10) {
           SensorsL[4].s = IPS_OK;
       }else{
           SensorsL[4].s = IPS_IDLE;
       }
       //Sixth bit contains Open switch status
       if (m2 & 0x20) {
           SwitchesL[0].s = IPS_OK;
       }else{
           SwitchesL[0].s = IPS_IDLE;
       }
       //Seventh bit contains Stop switch status
       if (m2 & 0x40) {
           SwitchesL[1].s = IPS_OK;
       }else{
           SwitchesL[1].s = IPS_IDLE;
       }
       //First bit contains Close switch status
       if (m1 & 0x01) {
           SwitchesL[0].s = IPS_OK;
       }else{
           SwitchesL[0].s = IPS_IDLE;
       }
       //Second bit contains Direct Command switch status
       if (m1 & 0x02) {
           SwitchesL[1].s = IPS_OK;
       }else{
           SwitchesL[1].s = IPS_IDLE;
       }
       //Third bit contains Management Command switch status
       if (m1 & 0x04) {
           SwitchesL[2].s = IPS_OK;
       }else{
           SwitchesL[2].s = IPS_IDLE;
       }

        SensorsLP.s = IPS_OK;
        SwitchesLP.s = IPS_OK;
        IDSetLight(&SensorsLP, NULL);
        IDSetLight(&SwitchesLP, NULL);

        StatusValueTP.s = IPS_OK;
        IDSetText(&StatusValueTP, NULL);
    }
    // Get the Firmware version of the device
    if(buf[1] == 'V')
    {
        char v[5];

        v[0] = buf[2];
        v[1] = buf[3];
        v[2] = buf[4];
        v[3] = buf[5];
        v[4] = buf[6];

        FirmwareVersionTP.s = IPS_OK;
        IUSaveText(&FirmwareVersionT[0], v);
        IDSetText(&FirmwareVersionTP, NULL);
    }

    return;
}
IPState Talon6::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        // DOME_CW --> OPEN. If can we are ask to "open" while we are fully opened as the limit switch indicates, then we simply return false.
        if (dir == DOME_CW  && fullOpenRoofSwitch == ISS_ON)
        {
            LOG_WARN("Roof is already fully opened.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CW && getWeatherState() == IPS_ALERT)
        {
            LOG_WARN("Weather conditions are in the danger zone. Cannot open roof.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && fullClosedRoofSwitch == ISS_ON )
        {
            LOG_WARN("Roof is already fully closed.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && INDI::Dome::isLocked())
        {
            DEBUG(INDI::Logger::DBG_WARNING,
                  "Cannot close dome when mount is locking. See: Telescope parkng policy, in options tab");
            return IPS_ALERT;
        }

        fullOpenRoofSwitch   = ISS_OFF;
        fullClosedRoofSwitch = ISS_OFF;

        return IPS_BUSY;
    }

    return (Dome::Abort() ? IPS_OK : IPS_ALERT);
}

IPState Talon6::Park()
{
    IPState rc = INDI::Dome::Move(DOME_CCW, MOTION_START);
    if (rc == IPS_BUSY)
    {
        LOG_INFO("Dome is parking.");
        if (SafetyS[0].s == ISS_OFF){
            WriteString("&C#");
        }else{
            WriteString("&P#");
        }
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
 }

IPState Talon6::UnPark()
{
    IPState rc = INDI::Dome::Move(DOME_CW, MOTION_START);
    if (rc == IPS_BUSY)
    {
        LOG_INFO("Dome is unparking.");
        WriteString("&O#");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

bool Talon6::Abort()
{
    MotionRequest = -1;

    // If both limit switches are off, then we're neither parked nor unparked.
    if (fullOpenRoofSwitch == ISS_OFF && fullClosedRoofSwitch == ISS_OFF)
    {
        IUResetSwitch(&ParkSP);
        ParkSP.s = IPS_IDLE;
        IDSetSwitch(&ParkSP, nullptr);
        WriteString("&S#");
    }

    return true;
}

IPState Talon6::DomeGoTo(int GoTo )
{
    if (SafetyS[0].s == ISS_ON){
        LOG_INFO("Go To needs Safety Condition to be disabled");
        return IPS_IDLE;
    }

    DEBUGF(INDI::Logger::DBG_SESSION,  "Dome is moving to %d percent open.",GoTo);
    // If GoTo < 100 we need to reset OpenSwitch else motion will not start
    if(GoTo < 100){
        fullOpenRoofSwitch   = ISS_OFF;
        fullClosedRoofSwitch = ISS_OFF;
    }
    // From percentage to ticks conversion
    double DoubleTicks = ( GoTo * EncoderTicksN[0].value)/100;
    int IntTicks = (int)DoubleTicks;

    DEBUGF(INDI::Logger::DBG_SESSION,  "Dome is moving to %d ticks.",IntTicks);

    // Transform dec ticks to HEX with leading zeroes
    std::stringstream hexTicks;
    hexTicks << std::hex <<  IntTicks;
    std::string hexTicksString = hexTicks.str();
    std::string paddedHexTicks = std::string(5 - hexTicksString.length(), '0') + hexTicksString;

    //Transform to char and build command string formatted as to documentation
    char hexTicksChar[paddedHexTicks.size()+1];
    strcpy(hexTicksChar, paddedHexTicks.c_str());
    char commandString[8];
    commandString[0] ='&';
    commandString[1] ='A';
    commandString[2] = ShiftChar(hexTicksChar[0]);
    commandString[3] = ShiftChar(hexTicksChar[1]);
    commandString[4] = ShiftChar(hexTicksChar[2]);
    commandString[5] = ShiftChar(hexTicksChar[3]);
    commandString[6] = ShiftChar(hexTicksChar[4]);
    commandString[7] = '#';

  // Send command to the device
    IPState rc = INDI::Dome::Move(DOME_CW, MOTION_START);
    if (rc == IPS_BUSY)
    {
        LOG_INFO("Dome is moving to requested position");
        WriteString(commandString);

        return IPS_BUSY;
    }
    else
        return IPS_ALERT;

}
char Talon6::ShiftChar(char shiftChar){

    char shiftedChar= 0;

        switch (shiftChar) {
        case NULL:
            shiftedChar = '0';
            break;
        case 'a':
            shiftedChar = ':';
            break;
         case 'b':
            shiftedChar = ';';
            break;
        case 'c':
            shiftedChar = '<';
            break;
        case 'd':
            shiftedChar = '=';
            break;
        case 'e':
            shiftedChar = '>';
            break;
        case 'f':
            shiftedChar = '?';
            break;
         default:
            shiftedChar =  shiftChar ;

            break;
        }

    return shiftedChar;
}


