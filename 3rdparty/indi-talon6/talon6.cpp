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
    SetParkDataType(PARK_NONE);

    // Switch to read parameters from device
    IUFillSwitch(&StatusS[0], "STATUS", "Read", ISS_OFF);
    IUFillSwitchVector(&StatusSP, StatusS, 1, getDeviceName(), "STATUS", "Device Status", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    // Device status parameters read from device are showed to the user
    IUFillText(&StatusValueT[0], "ROOF_STATUS", "Roof status", "");
    IUFillText(&StatusValueT[1], "LAST_ACTION", "Last Action", "");
    IUFillText(&StatusValueT[2], "CURRENT_POSITION", "Current Position", "");
    IUFillText(&StatusValueT[3], "POWER", "Power supply", "");
    IUFillText(&StatusValueT[4], "CLOSING_TIMER", "Closing Timer", "");
    IUFillText(&StatusValueT[5], "POWER_LOST_TIMER", "Power Lost Timer", "");
    IUFillText(&StatusValueT[6], "WEATHER_COND_TIMER", "Weather Condition Timer", "");
  //  IUFillText(&StatusValueT[7], "SENSOR_STATUS", "Sensors Status", "");
    IUFillTextVector(&StatusValueTP, StatusValueT, 7, getDeviceName(), "STATUSVALUE", "Status Values", MAIN_CONTROL_TAB, IP_RO,
                     60, IPS_IDLE);

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

        defineSwitch(&StatusSP);
        defineText(&StatusValueTP);

     }
     else
     {
         deleteProperty(StatusSP.name);
         deleteProperty(StatusValueTP.name);
     }

    return true;
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

        // Stop motion
        if (!strcmp(name, AbortSP.name))
        {
            Abort();
            return true;
        }

       // Park and UnPark roof
        if (!strcmp(name, ParkSP.name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);

            // If dome is the same state as actionName, then we do nothing.
            int currentParkIndex = IUFindOnSwitchIndex(&ParkSP);

            if (!strcmp(actionName, ParkS[currentParkIndex].name))
            {
               DEBUGF(INDI::Logger::DBG_SESSION, "Dome is already %s", ParkS[currentParkIndex].label);
               ParkSP.s = IPS_IDLE;
               IDSetSwitch(&ParkSP, NULL);
               return true;
            }

            // Check if any switch is ON
            for (int i = 0; i < n; i++)
            {
                 if (states[i] == ISS_ON)
                {
                    if (!strcmp(ParkS[0].name, names[i]))
                    {
                        if (domeState == DOME_PARKING)
                            return false;

                        return ( Park() );
                    }
                    else
                    {
                        if (domeState == DOME_UNPARKING)
                            return false;

                        return (UnPark() != IPS_ALERT);
                    }
                }
            }
            // Otherwise, let us update the switch state

            IUUpdateSwitch(&ParkSP, states, names, n);
            currentParkIndex = IUFindOnSwitchIndex(&ParkSP);
            DEBUGF(INDI::Logger::DBG_SESSION, "Dome is now %s", ParkS[currentParkIndex].label);
            ParkSP.s = IPS_OK;
            IDSetSwitch(&ParkSP, NULL);

            getDeviceStatus();
            return true;
        }
    }

    return Dome::ISNewSwitch(dev, name, states, names, n);
}

bool Talon6::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
     if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
         if (!strcmp(StatusValueTP.name, name))
        {
            getDeviceStatus();

            StatusValueTP.s = IPS_OK;
            IDSetText(&StatusValueTP, nullptr);
            return true;
        }
    }

    return  INDI::Dome::ISNewText(dev, name, texts, names, n);
}

bool Talon6::ISNewNumber(const char *dev,const char *name,double values[],char *names[],int n)
{
    return INDI::Dome::ISNewNumber(dev,name,values,names,n);
}

bool Talon6::Disconnect()
{
  return INDI::Dome::Disconnect();
}

void Talon6::getDeviceStatus()
{
    if (!isConnected())
        return;

    // &G# is the command to read the status from device
    WriteString("&G#");
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
    int nexttimer = 1000;
    if(isConnected()) getDeviceStatus();
     SetTimer(nexttimer);
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
          break;
      case 1:
          statusString = "CLOSED";
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
    std::stringstream streamx1;
    std::stringstream streamx2;
    std::stringstream streamx3;
    std::string xxxString;

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
    xxxString = std::to_string(xxx);
    char xxxChar[xxxString.size()+1];
    strcpy(xxxChar, xxxString.c_str());

    IUSaveText(&StatusValueT[2], xxxChar);

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

    IUSaveText(&StatusValueT[3], bbChar);

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

    IUSaveText(&StatusValueT[4], tttChar);

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

    IUSaveText(&StatusValueT[5], ppChar);

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

    IUSaveText(&StatusValueT[6], ccChar);

    // Sensor Status ! NOT IMPLEMENTED YET
    int m1, m2, mm1, mm2;
    int mm;
    std::string mmString;
    std::stringstream streamm1;
    std::stringstream streamm2;
    //Sensors status  is encoded using a custom 2 hex bytes encoding (see Talon6 documentation).
    m1 = (buf[15] & 0xFF & 0x7) << 7 ;
    m2 = buf[16] & 0xFF & 0x7F;
    // ENCODING OF SENSOR STATUS NOT IMPLEMENTED
   // IUSaveText(&StatusValueT[7], mmChar);

    IDSetText(&StatusValueTP, NULL);

  }

    return;
}

IPState Talon6::Park()
{

    WriteString("&C#");
    return IPS_BUSY;
}

IPState Talon6::UnPark()
{
    WriteString("&O#");
    return IPS_BUSY;

}

bool Talon6::Abort()
{
    WriteString("&S#");
    return true;
}


