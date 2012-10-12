/*
    RoboFocus
    Copyright (C) 2006 Markus Wildi (markus.wildi@datacomm.ch)
                  2011 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "robofocus.h"
#include "indicom.h"

#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <memory>

#define RF_MAX_CMD   9
#define RF_TIMEOUT   15
#define RF_STEP_RES  5

#define RF_MAX_TRIES    3
#define RF_MAX_DELAY    100000

#define BACKLASH_READOUT 99999
#define MAXTRAVEL_READOUT 99999

#define currentSpeed            SpeedN[0].value
#define currentPosition         FocusAbsPosN[0].value
#define currentTemperature      TemperatureN[0].value
#define currentBacklash         SetBacklashN[0].value
#define currentDuty             SettingsN[0].value
#define currentDelay            SettingsN[1].value
#define currentTicks            SettingsN[2].value
#define currentRelativeMovement FocusRelPosN[0].value
#define currentAbsoluteMovement FocusAbsPosN[0].value
#define currentSetBacklash      SetBacklashN[0].value
#define currentMinPosition      MinMaxPositionN[0].value
#define currentMaxPosition      MinMaxPositionN[1].value
#define currentMaxTravel        MaxTravelN[0].value

std::auto_ptr<RoboFocus> roboFocus(0);

 void ISInit()
 {
    static int isInit =0;

    if (isInit == 1)
        return;

     isInit = 1;
     if(roboFocus.get() == 0) roboFocus.reset(new RoboFocus());
     //IEAddTimer(POLLMS, ISPoll, NULL);

 }

 void ISGetProperties(const char *dev)
 {
         ISInit();
         roboFocus->ISGetProperties(dev);
 }

 void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
 {
         ISInit();
         roboFocus->ISNewSwitch(dev, name, states, names, num);
 }

 void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
 {
         ISInit();
         roboFocus->ISNewText(dev, name, texts, names, num);
 }

 void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
 {
         ISInit();
         roboFocus->ISNewNumber(dev, name, values, names, num);
 }

 void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
 {
   INDI_UNUSED(dev);
   INDI_UNUSED(name);
   INDI_UNUSED(sizes);
   INDI_UNUSED(blobsizes);
   INDI_UNUSED(blobs);
   INDI_UNUSED(formats);
   INDI_UNUSED(names);
   INDI_UNUSED(n);
 }

 void ISSnoopDevice (XMLEle *root)
 {
     ISInit();
 }


RoboFocus::RoboFocus()
{

}

RoboFocus::~RoboFocus()
{

}

bool RoboFocus::initProperties()
{
    INDI::Focuser::initProperties();

    // No speed for robofocus
    FocusSpeedN[0].min = FocusSpeedN[0].max = FocusSpeedN[0].value = 1;

    IUUpdateMinMax(&FocusSpeedNP);

    /* Port */
    IUFillText(&PortT[0], "PORT", "Port", "/dev/ttyUSB0");
    IUFillTextVector(&PortTP, PortT, 1, getDeviceName(), "DEVICE_PORT", "Ports", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);


    /* Focuser temperature */
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", 0, 65000., 0., 10000.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Settings of the Robofocus */
    IUFillNumber(&SettingsN[0], "Duty cycle", "Duty cycle", "%6.0f", 0., 255., 0., 1.0);
    IUFillNumber(&SettingsN[1], "Step Delay", "Step delay", "%6.0f", 0., 255., 0., 1.0);
    IUFillNumber(&SettingsN[2], "Motor Steps", "Motor steps per tick", "%6.0f", 0., 255., 0., 1.0);
    IUFillNumberVector(&SettingsNP, SettingsN, 3, getDeviceName(), "FOCUS_SETTINGS", "Settings", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /* Power Switches of the Robofocus */
    IUFillSwitch(&PowerSwitchesS[0], "1", "Switch 1", ISS_OFF);
    IUFillSwitch(&PowerSwitchesS[1], "2", "Switch 2", ISS_OFF);
    IUFillSwitch(&PowerSwitchesS[2], "3", "Switch 3", ISS_OFF);
    IUFillSwitch(&PowerSwitchesS[3], "4", "Switch 4", ISS_ON);
    IUFillSwitchVector(&PowerSwitchesSP, PowerSwitchesS, 4, getDeviceName(), "SWTICHES", "Power", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Robofocus should stay within these limits */
    IUFillNumber(&MinMaxPositionN[0], "MINPOS", "Minimum Tick", "%6.0f", 1., 65000., 0., 100. );
    IUFillNumber(&MinMaxPositionN[1], "MAXPOS", "Maximum Tick", "%6.0f", 1., 65000., 0., 55000.);
    IUFillNumberVector(&MinMaxPositionNP, MinMaxPositionN, 2, getDeviceName(), "FOCUS_MINMAXPOSITION", "Extrama", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&MaxTravelN[0], "MAXTRAVEL", "Maximum travel", "%6.0f", 1., 64000., 0., 10000.);
    IUFillNumberVector(&MaxTravelNP, MaxTravelN, 1, getDeviceName(), "FOCUS_MAXTRAVEL", "Max. travel", OPTIONS_TAB, IP_RW, 0, IPS_IDLE );

    /* Set Robofocus position register to the this position */
    IUFillNumber(&SetRegisterPositionN[0], "SETPOS", "Position", "%6.0f", 0, 64000., 0., 0. );
    IUFillNumberVector(&SetRegisterPositionNP, SetRegisterPositionN, 1, getDeviceName(), "FOCUS_REGISTERPOSITION", "Set register", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /* Backlash */
    IUFillNumber(&SetBacklashN[0], "SETBACKLASH", "Backlash", "%6.0f", -255., 255., 0., 0.);
    IUFillNumberVector(&SetBacklashNP, SetBacklashN, 1, getDeviceName(), "FOCUS_BACKLASH", "Set Register", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosN[0].min = -65000.;
    FocusRelPosN[0].max = 65000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step = 100;

    FocusAbsPosN[0].min = 0.;
    FocusAbsPosN[0].max = 65000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step = 10000;

    addDebugControl();

    return true;

}

void RoboFocus::ISGetProperties(const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);

    defineText(&PortTP);


}

bool RoboFocus::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&TemperatureNP);
        defineSwitch(&PowerSwitchesSP);
        defineNumber(&SettingsNP);
        defineNumber(&MinMaxPositionNP);
        defineNumber(&MaxTravelNP);
        defineNumber(&SetRegisterPositionNP);
        defineNumber(&SetBacklashNP);
        defineNumber(&FocusRelPosNP);
        defineNumber(&FocusAbsPosNP);
    }
    else
    {

        deleteProperty(TemperatureNP.name);
        deleteProperty(SettingsNP.name);
        deleteProperty(PowerSwitchesSP.name);
        deleteProperty(MinMaxPositionNP.name);
        deleteProperty(MaxTravelNP.name);
        deleteProperty(SetRegisterPositionNP.name);
        deleteProperty(SetBacklashNP.name);
        deleteProperty(FocusRelPosNP.name);
        deleteProperty(FocusAbsPosNP.name);

    }

    return true;

}

bool RoboFocus::Connect()
{
    int connectrc=0;
    char errorMsg[MAXRBUF];
    char firmeware[]="FV0000000";


    if (isDebug())
        IDLog("connecting to %s\n",PortT[0].text);

    if ( (connectrc = tty_connect(PortT[0].text, 9600, 8, 0, 1, &PortFD)) != TTY_OK)
    {
        tty_error_msg(connectrc, errorMsg, MAXRBUF);

        if (isDebug())
            IDLog("Failed to connect o port %s. Error: %s", PortT[0].text, errorMsg);

        IDMessage(getDeviceName(), "Failed to connect to port %s. Error: %s", PortT[0].text, errorMsg);

        return false;

    }

    if((updateRFFirmware(firmeware)) < 0)
    {
      /* This would be the end*/
        IDMessage( getDeviceName(), "Unknown error while reading Robofocus firmware\n");
        return false;
    }

    IDMessage(getDeviceName(), "Robofocus is online. Getting focus parameters...");

    GetFocusParams();

    IDMessage(getDeviceName(), "RoboFocus paramaters readout complete, focuser ready for use.");

    return true;
}

bool RoboFocus::Disconnect()
{
    tty_disconnect(PortFD);
    IDMessage(getDeviceName(), "RoboFocus is offline.");
    return true;
}

const char * RoboFocus::getDefaultName()
{
    return "RoboFocus";
}

unsigned char RoboFocus::CheckSum(char *rf_cmd)
{

  char substr[255] ;
  unsigned char val= 0 ;
  int i= 0 ;

  for(i=0; i < 8; i++) {

    substr[i]=rf_cmd[i] ;
  }

  val = CalculateSum( substr) ;
  if( val==  (unsigned char) rf_cmd[8])
  {

  } else {

      if (isDebug())
          fprintf(stderr,  "Wrong (%s,%d), %x != %x\n",  rf_cmd, strlen(rf_cmd), val, (unsigned char) rf_cmd[8]) ;
  }

  return val ;
}

unsigned char RoboFocus::CalculateSum(char *rf_cmd)
{

  unsigned char val= 0 ;
  int i=0 ;

  for(i=0; i < 8; i++) {

    val = val + (unsigned char) rf_cmd[i] ;
  }

  return val % 256 ;
}

int RoboFocus::SendCommand(char *rf_cmd)
{

  int nbytes_written=0, nbytes_read=0, check_ret=0, err_code=0;
  char rf_cmd_cks[32],robofocus_error[MAXRBUF];

  unsigned char val= 0 ;

  val = CalculateSum( rf_cmd );

  for(int i=0; i < 8; i++)
  {

    rf_cmd_cks[i]= rf_cmd[i] ;
  }

  rf_cmd_cks[8]=  (unsigned char) val ;
  rf_cmd_cks[9]= 0 ;

  if (isDebug())
  {
   fprintf(stderr, "WRITE: ") ;
   for(int i=0; i < 9; i++)
   {

     fprintf(stderr, "0x%2x ", (unsigned char) rf_cmd_cks[i]) ;
   }
   fprintf(stderr, "\n") ;
  }


  tcflush(PortFD, TCIOFLUSH);

   if  ( (err_code = tty_write(PortFD, rf_cmd_cks, RF_MAX_CMD, &nbytes_written) != TTY_OK))
   {
        tty_error_msg(err_code, robofocus_error, MAXRBUF);
        if (isDebug())
            IDLog("TTY error detected: %s\n", robofocus_error);
        return -1;
   }

   nbytes_read= ReadResponse(rf_cmd, RF_MAX_CMD, RF_TIMEOUT) ;

   if (nbytes_read < 1)
     return nbytes_read;

   check_ret= CheckSum(rf_cmd) ;

   rf_cmd[ nbytes_read - 1] = 0 ;

   if (isDebug())
       IDLog("Reply is (%s)\n", rf_cmd);

   return 0;

}

int RoboFocus::ReadResponse(char *buf, int nbytes, int timeout)
{

  char robofocus_error[MAXRBUF];
  int bytesRead = 0;
  int totalBytesRead = 0;
  int err_code;

  if (isDebug())
      IDLog("##########################################\n")
;
  while (totalBytesRead < nbytes)
  {
      //IDLog("Nbytes: %d\n", nbytes);

      if ( (err_code = tty_read(PortFD, buf+totalBytesRead, nbytes-totalBytesRead, timeout, &bytesRead)) != TTY_OK)
      {
              tty_error_msg(err_code, robofocus_error, MAXRBUF);
              if (isDebug())
              {
                  IDLog("TTY error detected: %s\n", robofocus_error);
                  IDMessage(getDeviceName(), "TTY error detected: %s\n", robofocus_error);
              }
              return -1;
      }

      if (isDebug())
        IDLog("Bytes Read: %d\n", bytesRead);

      if (bytesRead < 0 )
      {
          if (isDebug())
            IDLog("Bytesread < 1\n");
          return -1;
      }

      if((  buf[0]== 'I')||  (  buf[0]== 'O'))
      {

          if (isDebug())
              IDLog("Moving... with buf[0]=(%c)\n", buf[0]) ;

        for(int i=0; i < 9; i++)
        {
            if (isDebug())
                IDLog("buf[%d]=(%c)\n", i, buf[i]);

            // Catch the F in stream of I's and O's
            // The robofocus reply might get mixed with the movement strings. For example:
            // 0x49 0x49 0x49 0x49 0x49 0x46 0x44 0x30 0x32
            // The 0x46 above is the START for the rofofocus response string, but it's lost unless we catch it here
            if (buf[i] == 0x46)
            {
                totalBytesRead = bytesRead - i;
                strncpy(buf, buf+i, totalBytesRead);
                break;
            }
        }

         usleep(100000) ;


      }
      else
      {
        //buf += bytesRead;
        totalBytesRead += bytesRead;
        //nbytes -= bytesRead;
      }
    }

    tcflush(PortFD, TCIOFLUSH);

   if (isDebug())
   {
       fprintf(stderr, "READ : (%s,%d), %d\n", buf, 9, totalBytesRead) ;
        fprintf(stderr, "READ : ") ;
        for(int i=0; i < 9; i++)
        {

         fprintf(stderr, "0x%2x ", (unsigned char)buf[i]) ;
        }
        fprintf(stderr, "\n") ;
   }

  return 9;
}

int RoboFocus::updateRFPosition(double *value)
{

  float temp ;
  char rf_cmd[32] ;
  int ret_read_tmp ;

  strcpy(rf_cmd, "FG000000" ) ;

  if ((ret_read_tmp= SendCommand(  rf_cmd)) < 0){

    return ret_read_tmp;
  }
   if (sscanf(rf_cmd, "FD%6f", &temp) < 1){

    return -1;
  }

  *value = (double) temp;

  return 0;

}

int RoboFocus::updateRFTemperature(double *value)
{

  float temp ;
  char rf_cmd[32] ;
  int ret_read_tmp ;

  strcpy(rf_cmd, "FT000000" ) ;

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp;


  if (sscanf(rf_cmd, "FT%6f", &temp) < 1)
    return -1;

  *value = (double) temp/2.- 273.15;

  return 0;
}

int RoboFocus::updateRFBacklash(double *value)
{

  float temp ;
  char rf_cmd[32] ;
  char vl_tmp[4] ;
  int ret_read_tmp ;
  int sign= 0 ;

  if(*value== BACKLASH_READOUT) {

    strcpy(rf_cmd, "FB000000" ) ;

  } else {

    rf_cmd[0]=  'F' ;
    rf_cmd[1]=  'B' ;

    if( *value > 0) {

      rf_cmd[2]= '3' ;

    } else {
      *value= - *value ;
      rf_cmd[2]= '2' ;
    }
    rf_cmd[3]= '0' ;
    rf_cmd[4]= '0' ;

    if(*value > 99) {
      sprintf( vl_tmp, "%3d", (int) *value) ;

    } else if(*value > 9) {
      sprintf( vl_tmp, "0%2d", (int) *value) ;
    } else {
      sprintf( vl_tmp, "00%1d", (int) *value) ;
    }
    rf_cmd[5]= vl_tmp[0] ;
    rf_cmd[6]= vl_tmp[1] ;
    rf_cmd[7]= vl_tmp[2] ;
  }

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp;


  if (sscanf(rf_cmd, "FB%1d%5f", &sign, &temp) < 1)
    return -1;

  *value = (double) temp  ;

  if(( sign== 2) && ( *value > 0)) {
    *value = - (*value) ;
  }
  return 0;
}

int RoboFocus::updateRFFirmware(char *rf_cmd)
{

  int ret_read_tmp ;

  strcpy(rf_cmd, "FV000000" ) ;

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp;

  return 0;
}

int RoboFocus::updateRFMotorSettings(double *duty, double *delay, double *ticks)
{

  char rf_cmd[32] ;
  int ret_read_tmp ;

  if(( *duty== 0 ) && (*delay== 0) && (*ticks== 0) ){

    strcpy(rf_cmd, "FC000000" ) ;

  } else {

    rf_cmd[0]=  'F' ;
    rf_cmd[1]=  'C' ;
    rf_cmd[2]= (char) *duty ;
    rf_cmd[3]= (char) *delay ;
    rf_cmd[4]= (char) *ticks ;
    rf_cmd[5]= '0' ;
    rf_cmd[6]= '0' ;
    rf_cmd[7]= '0' ;
    rf_cmd[8]=  0 ;

  }

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp;

  *duty=  (float) rf_cmd[2] ;
  *delay= (float) rf_cmd[3] ;
  *ticks= (float) rf_cmd[4] ;

  return 0;
}

int RoboFocus::updateRFPositionRelativeInward(double *value)
{

  char rf_cmd[32] ;
  int ret_read_tmp ;
  float temp ;
  rf_cmd[0]= 0 ;

  if(*value > 9999) {
    sprintf( rf_cmd, "FI0%5d", (int) *value) ;
  } else if(*value > 999) {
    sprintf( rf_cmd, "FI00%4d", (int) *value) ;
  } else if(*value > 99) {
    sprintf( rf_cmd, "FI000%3d", (int) *value) ;
  } else if(*value > 9) {
    sprintf( rf_cmd, "FI0000%2d", (int) *value) ;
  } else {
    sprintf( rf_cmd, "FI00000%1d", (int) *value) ;
  }

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp;


  if (sscanf(rf_cmd, "FD0%5f", &temp) < 1)
    return -1;

  *value = (double) temp  ;

  return 0;
}

int RoboFocus::updateRFPositionRelativeOutward(double *value)
{

  char rf_cmd[32] ;
  int ret_read_tmp ;
  float temp ;

  rf_cmd[0]= 0 ;

  if(*value > 9999) {
    sprintf( rf_cmd, "FO0%5d", (int) *value) ;
  } else if(*value > 999) {
    sprintf( rf_cmd, "FO00%4d", (int) *value) ;
  } else if(*value > 99) {
    sprintf( rf_cmd, "FO000%3d", (int) *value) ;
  } else if(*value > 9) {
    sprintf( rf_cmd, "FO0000%2d", (int) *value) ;
  } else {
    sprintf( rf_cmd, "FO00000%1d", (int) *value) ;
  }

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp;



  if (sscanf(rf_cmd, "FD0%5f", &temp) < 1)
    return -1;

  *value = (double) temp  ;

  return 0;
}

int RoboFocus::updateRFPositionAbsolute(double *value)
{

  char rf_cmd[32] ;
  int ret_read_tmp ;
  float temp ;

  rf_cmd[0]= 0 ;

  if(*value > 9999) {
    sprintf( rf_cmd, "FG0%5d", (int) *value) ;
  } else if(*value > 999) {
    sprintf( rf_cmd, "FG00%4d", (int) *value) ;
  } else if(*value > 99) {
    sprintf( rf_cmd, "FG000%3d", (int) *value) ;
  } else if(*value > 9) {
    sprintf( rf_cmd, "FG0000%2d", (int) *value) ;
  } else {
    sprintf( rf_cmd, "FG00000%1d", (int) *value) ;
  }

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp;

  if (sscanf(rf_cmd, "FD0%5f", &temp) < 1)
    return -1;

  *value = (double) temp  ;

  return 0;
}

int RoboFocus::updateRFPowerSwitches(int s, int  new_sn, int *cur_s1LL, int *cur_s2LR, int *cur_s3RL, int *cur_s4RR)
{

  char rf_cmd[32] ;
  char rf_cmd_tmp[32] ;
  int ret_read_tmp ;
  int i = 0 ;


    /* Get first the status */
    strcpy(rf_cmd_tmp, "FP000000" ) ;

    if ((ret_read_tmp= SendCommand( rf_cmd_tmp)) < 0)
      return ret_read_tmp ;

    for(i= 0; i < 9; i++) {
      rf_cmd[i]= rf_cmd_tmp[i] ;
    }


    if( rf_cmd[new_sn + 4]== '2') {
      rf_cmd[new_sn + 4]= '1' ;
    } else {
      rf_cmd[new_sn + 4]= '2' ;
    }


 /*    if( s== ISS_ON) { */

/*       rf_cmd[new_sn + 4]= '2' ; */
/*     } else { */
/*       rf_cmd[new_sn + 4]= '1' ; */
/*     } */

    rf_cmd[8]= 0 ;

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp ;


  *cur_s1LL= *cur_s2LR= *cur_s3RL= *cur_s4RR= ISS_OFF ;

  if(rf_cmd[4]== '2' ) {
    *cur_s1LL= ISS_ON ;
  }

  if(rf_cmd[5]== '2' ) {
    *cur_s2LR= ISS_ON ;
  }

  if(rf_cmd[6]== '2' ) {
    *cur_s3RL= ISS_ON ;
  }

  if(rf_cmd[7]== '2' ) {
    *cur_s4RR= ISS_ON ;
  }
  return 0 ;
}


int RoboFocus::updateRFMaxPosition(double *value)
{

  float temp ;
  char rf_cmd[32] ;
  char vl_tmp[6] ;
  int ret_read_tmp ;
  char waste[1] ;

  if(*value== MAXTRAVEL_READOUT) {

    strcpy(rf_cmd, "FL000000" ) ;

  } else {

    rf_cmd[0]=  'F' ;
    rf_cmd[1]=  'L' ;
    rf_cmd[2]=  '0' ;

    if(*value > 9999) {
      sprintf( vl_tmp, "%5d", (int) *value) ;

    } else if(*value > 999) {

      sprintf( vl_tmp, "0%4d", (int) *value) ;

    } else if(*value > 99) {
      sprintf( vl_tmp, "00%3d", (int) *value) ;

    } else if(*value > 9) {
      sprintf( vl_tmp, "000%2d", (int) *value) ;
    } else {
      sprintf( vl_tmp, "0000%1d", (int) *value) ;
    }
    rf_cmd[3]= vl_tmp[0] ;
    rf_cmd[4]= vl_tmp[1] ;
    rf_cmd[5]= vl_tmp[2] ;
    rf_cmd[6]= vl_tmp[3] ;
    rf_cmd[7]= vl_tmp[4] ;
    rf_cmd[8]= 0 ;
  }

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp;


  if (sscanf(rf_cmd, "FL%1c%5f", waste, &temp) < 1)
    return -1;

  *value = (double) temp  ;

  return 0;
}

int RoboFocus::updateRFSetPosition(double *value)
{

  char rf_cmd[32] ;
  char vl_tmp[6] ;
  int ret_read_tmp ;

  rf_cmd[0]=  'F' ;
  rf_cmd[1]=  'S' ;
  rf_cmd[2]=  '0' ;

  if(*value > 9999) {
    sprintf( vl_tmp, "%5d", (int) *value) ;
  } else if(*value > 999) {
    sprintf( vl_tmp, "0%4d", (int) *value) ;
  } else if(*value > 99) {
    sprintf( vl_tmp, "00%3d", (int) *value) ;
  } else if(*value > 9) {
    sprintf( vl_tmp, "000%2d", (int) *value) ;
  } else {
    sprintf( vl_tmp, "0000%1d", (int) *value) ;
  }
  rf_cmd[3]= vl_tmp[0] ;
  rf_cmd[4]= vl_tmp[1] ;
  rf_cmd[5]= vl_tmp[2] ;
  rf_cmd[6]= vl_tmp[3] ;
  rf_cmd[7]= vl_tmp[4] ;
  rf_cmd[8]= 0 ;

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp;

  return 0;
}

bool RoboFocus::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // ===================================
        // Port Name
        // ===================================
        if (!strcmp(name, PortTP.name) )
        {
          if (IUUpdateText(&PortTP, texts, names, n) < 0)
                return false;

          PortTP.s = IPS_OK;
          IDSetText (&PortTP, NULL);
          return true;
        }

    }

     return INDI::Focuser::ISNewText(dev, name, texts, names, n);
}

bool RoboFocus::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp (name, PowerSwitchesSP.name))
        {
          int ret= -1 ;
          int nset= 0 ;
          int i= 0 ;
          int new_s= -1 ;
          int new_sn= -1 ;
          int cur_s1LL=0 ;
          int cur_s2LR=0 ;
          int cur_s3RL=0 ;
          int cur_s4RR=0 ;

          ISwitch *sp ;

          PowerSwitchesSP.s = IPS_BUSY ;
          IDSetSwitch(&PowerSwitchesSP, NULL) ;


          for( nset = i = 0; i < n; i++) {
            /* Find numbers with the passed names in the SettingsNP property */
            sp = IUFindSwitch (&PowerSwitchesSP, names[i]) ;

            /* If the state found is  (PowerSwitchesS[0]) then process it */

            if( sp == &PowerSwitchesS[0]){


              new_s = (states[i]) ;
              new_sn= 0;
              nset++ ;
            } else if( sp == &PowerSwitchesS[1]){

              new_s = (states[i]) ;
              new_sn= 1;
              nset++ ;
            } else if( sp == &PowerSwitchesS[2]){

              new_s = (states[i]) ;
              new_sn= 2;
              nset++ ;
            } else if( sp == &PowerSwitchesS[3]){

              new_s = (states[i]) ;
              new_sn= 3;
              nset++ ;
            }
          }
          if (nset == 1)
          {
            cur_s1LL= cur_s2LR= cur_s3RL= cur_s4RR= 0 ;

            if(( ret= updateRFPowerSwitches(new_s, new_sn, &cur_s1LL, &cur_s2LR, &cur_s3RL, &cur_s4RR)) < 0)
            {

              PowerSwitchesSP.s = IPS_ALERT;
              IDSetSwitch(&PowerSwitchesSP, "Unknown error while reading  Robofocus power swicht settings");
              return true;
            }
          }
          else
          {
            /* Set property state to idle */
            PowerSwitchesSP.s = IPS_IDLE ;

            IDSetNumber(&SettingsNP, "Power switch settings absent or bogus.");
            return true ;
          }

      }
    }


    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool RoboFocus::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    int nset=0,i=0;

    if(strcmp(dev,getDeviceName())==0)
    {

        if (!strcmp (name, SettingsNP.name))
        {
          /* new speed */
          double new_duty = 0 ;
          double new_delay = 0 ;
          double new_ticks = 0 ;
          int ret = -1 ;

          for (nset = i = 0; i < n; i++)
          {
            /* Find numbers with the passed names in the SettingsNP property */
            INumber *eqp = IUFindNumber (&SettingsNP, names[i]);

            /* If the number found is  (SettingsN[0]) then process it */
            if (eqp == &SettingsN[0])
            {

              new_duty = (values[i]);
              nset += new_duty >= 0 && new_duty <= 255;
            } else if  (eqp == &SettingsN[1])
            {

              new_delay = (values[i]);
              nset += new_delay >= 0 && new_delay <= 255;
            } else if  (eqp == &SettingsN[2])
            {

              new_ticks = (values[i]);
              nset += new_ticks >= 0 && new_ticks <= 255;
            }
          }

          /* Did we process the three numbers? */
          if (nset == 3)
          {

            /* Set the robofocus state to BUSY */
            SettingsNP.s = IPS_BUSY;


            IDSetNumber(&SettingsNP, NULL);

            if(( ret= updateRFMotorSettings(&new_duty, &new_delay, &new_ticks))< 0)
            {

              IDSetNumber(&SettingsNP, "Changing to new settings failed");
              return false;
            }

            currentDuty = new_duty ;
            currentDelay= new_delay ;
            currentTicks= new_ticks ;

            SettingsNP.s = IPS_OK;
            IDSetNumber(&SettingsNP, "Motor settings are now  %3.0f %3.0f %3.0f", currentDuty, currentDelay, currentTicks);
            return true;

          } else
          {
            /* Set property state to idle */
            SettingsNP.s = IPS_IDLE;

            IDSetNumber(&SettingsNP, "Settings absent or bogus.");
            return false ;
          }
        }





        if (!strcmp (name, SetBacklashNP.name))
        {

          double new_back = 0 ;
          int nset = 0;
          int ret= -1 ;

          for (nset = i = 0; i < n; i++)
          {
            /* Find numbers with the passed names in the SetBacklashNP property */
            INumber *eqp = IUFindNumber (&SetBacklashNP, names[i]);

            /* If the number found is SetBacklash (SetBacklashN[0]) then process it */
            if (eqp == &SetBacklashN[0]){

              new_back = (values[i]);

              /* limits */
              nset += new_back >= -0xff && new_back <= 0xff;
            }

            if (nset == 1) {

              /* Set the robofocus state to BUSY */
              SetBacklashNP.s = IPS_BUSY;
              IDSetNumber(&SetBacklashNP, NULL);

              if(( ret= updateRFBacklash(&new_back)) < 0) {

                SetBacklashNP.s = IPS_IDLE;
                IDSetNumber(&SetBacklashNP, "Setting new backlash failed.");

                return false ;
              }

              currentSetBacklash=  new_back ;
              SetBacklashNP.s = IPS_OK;
              IDSetNumber(&SetBacklashNP, "Backlash is now  %3.0f", currentSetBacklash) ;
              return true;
            } else {

              SetBacklashNP.s = IPS_IDLE;
              IDSetNumber(&SetBacklashNP, "Need exactly one parameter.");

              return false ;
            }
          }
        }



        if (!strcmp (name, MinMaxPositionNP.name))
        {
          /* new positions */
          double new_min = 0 ;
          double new_max = 0 ;

          for (nset = i = 0; i < n; i++)
          {
            /* Find numbers with the passed names in the MinMaxPositionNP property */
            INumber *mmpp = IUFindNumber (&MinMaxPositionNP, names[i]);

            /* If the number found is  (MinMaxPositionN[0]) then process it */
            if (mmpp == &MinMaxPositionN[0])
            {

              new_min = (values[i]);
              nset += new_min >= 1 && new_min <= 65000;
            } else if  (mmpp == &MinMaxPositionN[1])
            {

              new_max = (values[i]);
              nset += new_max >= 1 && new_max <= 65000;
            }
          }

          /* Did we process the two numbers? */
          if (nset == 2)
          {

            /* Set the robofocus state to BUSY */
            MinMaxPositionNP.s = IPS_BUSY;

            currentMinPosition = new_min ;
            currentMaxPosition= new_max ;


            MinMaxPositionNP.s = IPS_OK;
            IDSetNumber(&MinMaxPositionNP, "Minimum and Maximum settings are now  %3.0f %3.0f", currentMinPosition, currentMaxPosition);
            return true;

          } else {
            /* Set property state to idle */
            MinMaxPositionNP.s = IPS_IDLE;

            IDSetNumber(&MinMaxPositionNP, "Minimum and maximum limits absent or bogus.");

            return false;
          }
        }


        if (!strcmp (name, MaxTravelNP.name))
        {

          double new_maxt = 0 ;
          int ret = -1 ;

          for (nset = i = 0; i < n; i++)
          {
            /* Find numbers with the passed names in the MinMaxPositionNP property */
            INumber *mmpp = IUFindNumber (&MaxTravelNP, names[i]);

            /* If the number found is  (MaxTravelN[0]) then process it */
            if (mmpp == &MaxTravelN[0])
            {

              new_maxt = (values[i]);
              nset += new_maxt >= 1 && new_maxt <= 64000;
            }
          }
          /* Did we process the one number? */
          if (nset == 1) {

            IDSetNumber(&MinMaxPositionNP, NULL);

            if(( ret= updateRFMaxPosition(&new_maxt))< 0 )
            {
              MaxTravelNP.s = IPS_IDLE;
              IDSetNumber(&MaxTravelNP, "Changing to new maximum travel failed");
              return false ;
            }

            currentMaxTravel=  new_maxt ;
            MaxTravelNP.s = IPS_OK;
            IDSetNumber(&MaxTravelNP, "Maximum travel is now  %3.0f", currentMaxTravel) ;
            return true;

          } else {
            /* Set property state to idle */

            MaxTravelNP.s = IPS_IDLE;
            IDSetNumber(&MaxTravelNP, "Maximum travel absent or bogus.");

            return false ;
          }
        }


        if (!strcmp (name, SetRegisterPositionNP.name))
        {

          double new_apos = 0 ;
          int nset = 0;
          int ret= -1 ;

          for (nset = i = 0; i < n; i++)
          {
            /* Find numbers with the passed names in the SetRegisterPositionNP property */
            INumber *srpp = IUFindNumber (&SetRegisterPositionNP, names[i]);

            /* If the number found is SetRegisterPosition (SetRegisterPositionN[0]) then process it */
            if (srpp == &SetRegisterPositionN[0])
            {

              new_apos = (values[i]);

              /* limits are absolute */
              nset += new_apos >= 0 && new_apos <= 64000;
            }

            if (nset == 1)
            {

              if((new_apos < currentMinPosition) || (new_apos > currentMaxPosition))
              {

                SetRegisterPositionNP.s = IPS_ALERT ;
                IDSetNumber(&SetRegisterPositionNP, "Value out of limits  %5.0f", new_apos);
                return false ;
              }

              /* Set the robofocus state to BUSY */
              SetRegisterPositionNP.s = IPS_BUSY;
              IDSetNumber(&SetRegisterPositionNP, NULL);

              if(( ret= updateRFSetPosition(&new_apos)) < 0)
              {

                SetRegisterPositionNP.s = IPS_OK;
                IDSetNumber(&SetRegisterPositionNP, "Read out of the set position to %3d failed. Trying to recover the position", ret);

                if((ret= updateRFPosition( &currentPosition)) < 0)
                {

                  FocusAbsPosNP.s = IPS_ALERT;
                  IDSetNumber(&FocusAbsPosNP, "Unknown error while reading  Robofocus position: %d", ret);

                  SetRegisterPositionNP.s = IPS_IDLE;
                  IDSetNumber(&SetRegisterPositionNP, "Relative movement failed.");
                }

                SetRegisterPositionNP.s = IPS_OK;
                IDSetNumber(&SetRegisterPositionNP, NULL);


                FocusAbsPosNP.s = IPS_OK;
                IDSetNumber(&FocusAbsPosNP, "Robofocus position recovered %5.0f", currentPosition);
                IDMessage( getDeviceName(), "Robofocus position recovered resuming normal operation");
                /* We have to leave here, because new_apos is not set */
                return true ;
              }
              currentPosition= new_apos ;
              SetRegisterPositionNP.s = IPS_OK;
              IDSetNumber(&SetRegisterPositionNP, "Robofocus register set to %5.0f", currentPosition);

              FocusAbsPosNP.s = IPS_OK;
              IDSetNumber(&FocusAbsPosNP, "Robofocus position is now %5.0f", currentPosition);

              return true ;

            } else
            {

              SetRegisterPositionNP.s = IPS_IDLE;
              IDSetNumber(&SetRegisterPositionNP, "Need exactly one parameter.");

              return false;
            }

            if((ret= updateRFPosition(&currentPosition)) < 0)
            {

              FocusAbsPosNP.s = IPS_ALERT;
              IDSetNumber(&FocusAbsPosNP, "Unknown error while reading  Robofocus position: %d", ret);

              return false ;
            }

            SetRegisterPositionNP.s = IPS_OK;
            IDSetNumber(&SetRegisterPositionNP, "Robofocus has accepted new register setting" ) ;

            FocusAbsPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, "Robofocus new position %5.0f", currentPosition);

            return true;
          }
        }



    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

}

void RoboFocus::GetFocusParams ()
{

  int ret = -1 ;
  int cur_s1LL=0 ;
  int cur_s2LR=0 ;
  int cur_s3RL=0 ;
  int cur_s4RR=0 ;


      if((ret= updateRFPosition(&currentPosition)) < 0)
      {
        FocusAbsPosNP.s = IPS_ALERT;
        IDSetNumber(&FocusAbsPosNP, "Unknown error while reading  Robofocus position: %d", ret);
        return;
      }

      FocusAbsPosNP.s = IPS_OK;
      IDSetNumber(&FocusAbsPosNP, NULL);

      FocusAbsPosN[0].value = currentPosition;
      IDSetNumber(&FocusAbsPosNP, NULL);

      if(( ret= updateRFTemperature(&currentTemperature)) < 0)
      {
        TemperatureNP.s = IPS_ALERT;
        IDSetNumber(&TemperatureNP, "Unknown error while reading  Robofocus temperature");
        return;
      }
      TemperatureNP.s = IPS_OK;
      IDSetNumber(&TemperatureNP, NULL);


      currentBacklash= BACKLASH_READOUT ;
      if(( ret= updateRFBacklash(&currentBacklash)) < 0)
      {
        SetBacklashNP.s = IPS_ALERT;
        IDSetNumber(&SetBacklashNP, "Unknown error while reading  Robofocus backlash");
        return;
      }
      SetBacklashNP.s = IPS_OK;
      IDSetNumber(&SetBacklashNP, NULL);

      currentDuty= currentDelay= currentTicks=0 ;

      if(( ret= updateRFMotorSettings(&currentDuty, &currentDelay, &currentTicks )) < 0)
      {
        SettingsNP.s = IPS_ALERT;
        IDSetNumber(&SettingsNP, "Unknown error while reading  Robofocus motor settings");
        return;
      }

      SettingsNP.s = IPS_OK;
      IDSetNumber(&SettingsNP, NULL);

      if(( ret= updateRFPowerSwitches(-1, -1,  &cur_s1LL, &cur_s2LR, &cur_s3RL, &cur_s4RR)) < 0)
      {
        PowerSwitchesSP.s = IPS_ALERT;
        IDSetSwitch(&PowerSwitchesSP, "Unknown error while reading  Robofocus power swicht settings");
        return;
      }

      PowerSwitchesS[0].s= PowerSwitchesS[1].s= PowerSwitchesS[2].s= PowerSwitchesS[3].s= ISS_OFF ;

      if(cur_s1LL== ISS_ON) {

        PowerSwitchesS[0].s= ISS_ON ;
      }
      if(cur_s2LR== ISS_ON) {

        PowerSwitchesS[1].s= ISS_ON ;
      }
      if(cur_s3RL== ISS_ON) {

        PowerSwitchesS[2].s= ISS_ON ;
      }
      if(cur_s4RR== ISS_ON) {

        PowerSwitchesS[3].s= ISS_ON ;
      }
      PowerSwitchesSP.s = IPS_OK ;
      IDSetSwitch(&PowerSwitchesSP, NULL);


      currentMaxTravel= MAXTRAVEL_READOUT ;
      if(( ret= updateRFMaxPosition(&currentMaxTravel)) < 0)
      {
        MaxTravelNP.s = IPS_ALERT;
        IDSetNumber(&MaxTravelNP, "Unknown error while reading  Robofocus maximum travel");
        return;
      }
      MaxTravelNP.s = IPS_OK;
      IDSetNumber(&MaxTravelNP, NULL);

}

bool RoboFocus::Move(FocusDirection dir, int speed, int duration)
{
    INDI_UNUSED(speed);
    double pos=0;
    struct timeval tv_start;
    struct timeval tv_finish;
    double dt=0;
    gettimeofday (&tv_start, NULL);

    while (duration > 0)
    {

        pos = RF_STEP_RES;

    if (dir == FOCUS_INWARD)
         updateRFPositionRelativeInward(&pos);
    else
        updateRFPositionRelativeOutward(&pos);

        gettimeofday (&tv_finish, NULL);

        dt = tv_finish.tv_sec - tv_start.tv_sec + (tv_finish.tv_usec - tv_start.tv_usec)/1e6;



        duration -= dt * 1000;

      // IDLog("dt is: %g --- duration is: %d -- pos: %g\n", dt, duration, pos);
    }

   return true;
}


int RoboFocus::MoveAbs(int targetTicks)
{
    int ret= -1 ;
    double new_apos = targetTicks;

    if (targetTicks < FocusAbsPosN[0].min || targetTicks > FocusAbsPosN[0].max)
    {
        IDMessage(getDeviceName(), "Error, requested absolute position is out of range.");
        return -1;
    }

    IDMessage(getDeviceName() , "Focuser is moving to requested position...");

    if(( ret= updateRFPositionAbsolute(&new_apos)) < 0)
    {

        IDMessage(getDeviceName(), "Read out of the absolute movement failed %3d, trying to recover position.", ret);

        for (int i=0;; i++)
        {
            if((ret= updateRFPosition(&currentPosition)) < 0)
            {
                IDMessage(getDeviceName(),"Unknown error while reading  Robofocus position: %d.", ret);
                if (i == RF_MAX_TRIES)
                    return false;
                else
                    usleep(RF_MAX_DELAY);
            }
            else break;
      }

      IDMessage( getDeviceName(), "Robofocus position recovered resuming normal operation");
      /* We have to leave here, because new_apos is not set */
      return -1;
    }


    return 0;
}

bool RoboFocus::MoveRel(FocusDirection dir, unsigned int ticks)
{

      double cur_rpos=0 ;
      double new_rpos = 0 ;
      int ret=0;
      bool nset = false;

        cur_rpos= new_rpos = ticks;
        /* CHECK 2006-01-26, limits are relative to the actual position */
        nset = new_rpos >= -0xffff && new_rpos <= 0xffff;

        if (nset)
        {

          if((currentPosition + new_rpos < currentMinPosition) || (currentPosition + new_rpos > currentMaxPosition))
          {

            IDMessage(getDeviceName(), "Value out of limits %5.0f", currentPosition +  new_rpos);
            return false ;
          }

          if( dir == FOCUS_OUTWARD)
            ret= updateRFPositionRelativeOutward(&new_rpos) ;
          else
            ret= updateRFPositionRelativeInward(&new_rpos) ;

          if( ret < 0)
          {

            IDMessage(getDeviceName(), "Read out of the relative movement failed, trying to recover position.");
            if((ret= updateRFPosition(&currentPosition)) < 0)
            {

              IDMessage(getDeviceName(), "Unknown error while reading  Robofocus position: %d", ret);
              return false;
            }

            IDMessage(getDeviceName(), "Robofocus position recovered %5.0f", currentPosition);
            // We have to leave here, because new_rpos is not set
            return false ;
          }

          currentRelativeMovement= cur_rpos ;
          currentAbsoluteMovement=  new_rpos;
          return true;
        }
        {
            IDMessage(getDeviceName(), "Value out of limits.");
            return false;
        }


}

