/*
    NFocus
    Copyright (C) 2013 Felix Krämer (rigelsys@felix-kraemer.de)

    Based on the work for robofocus by
                  2006 Markus Wildi (markus.wildi@datacomm.ch)
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

#include "nfocus.h"
#include "indicom.h"

#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <memory>
#include <stdlib.h>

#define NF_MAX_CMD   8 		/* cmd length */
#define NF_TIMEOUT   15		/* com timeout */
#define NF_STEP_RES  5		/* step res for a given time period */

#define NF_MAX_TRIES    3
#define NF_MAX_DELAY    100000

#define BACKLASH_READOUT 0
#define MAXTRAVEL_READOUT 99999
#define INOUTSCALAR_READOUT 1

#define currentSpeed            SpeedN[0].value
#define currentPosition         FocusAbsPosN[0].value
#define currentTemperature      TemperatureN[0].value
#define currentBacklash         SetBacklashN[0].value
#define currentOnTime           SettingsN[0].value
#define currentOffTime          SettingsN[1].value
#define currentFastDelay        SettingsN[2].value
#define currentInOutScalar	InOutScalarN[0].value
#define currentRelativeMovement FocusRelPosN[0].value
#define currentAbsoluteMovement FocusAbsPosN[0].value
#define currentSetBacklash      SetBacklashN[0].value
#define currentMinPosition      MinMaxPositionN[0].value
#define currentMaxPosition      MinMaxPositionN[1].value
#define currentMaxTravel        MaxTravelN[0].value

std::auto_ptr<NFocus> nFocus(0);

 void ISInit()
 {
    static int isInit =0;

    if (isInit == 1)
        return;

     isInit = 1;
     if(nFocus.get() == 0) nFocus.reset(new NFocus());

 }

 void ISGetProperties(const char *dev)
 {
         ISInit();
         nFocus->ISGetProperties(dev);
 }

 void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
 {
         ISInit();
         nFocus->ISNewSwitch(dev, name, states, names, num);
 }

 void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
 {
         ISInit();
         nFocus->ISNewText(dev, name, texts, names, num);
 }

 void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
 {
         ISInit();
         nFocus->ISNewNumber(dev, name, values, names, num);
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


NFocus::NFocus()
{

}

NFocus::~NFocus()
{

}

bool NFocus::initProperties()
{
    INDI::Focuser::initProperties();

    // No speed for nfocus
    FocusSpeedN[0].min = FocusSpeedN[0].max = FocusSpeedN[0].value = 1;

    IUUpdateMinMax(&FocusSpeedNP);

    /* Port */
    IUFillText(&PortT[0], "PORT", "Port", "/dev/ttyACM0");
    IUFillTextVector(&PortTP, PortT, 1, getDeviceName(), "DEVICE_PORT", "Ports", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);


    /* Focuser temperature */
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", 0, 65000., 0., 10000.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Timed move Settings */
    IUFillNumber(&FocusTimerN[0],"FOCUS_TIMER_VALUE","Focus Timer","%5.0f",0.0,10000.0,10.0,10000.0);
    IUFillNumberVector(&FocusTimerNP,FocusTimerN,1,getDeviceName(),"FOCUS_TIMER","Timer",MAIN_CONTROL_TAB,IP_RW,60,IPS_OK);

    /* Settings of the Nfocus */
    IUFillNumber(&SettingsN[0], "ON time", "ON waiting time", "%6.0f", 10., 250., 0., 73.);
    IUFillNumber(&SettingsN[1], "OFF time", "OFF waiting time", "%6.0f", 1., 250., 0., 15.);
    IUFillNumber(&SettingsN[2], "Fast Mode Delay", "Fast Mode Delay", "%6.0f", 0., 255., 0., 9.);
    IUFillNumberVector(&SettingsNP, SettingsN, 3, getDeviceName(), "FOCUS_SETTINGS", "Settings", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /* tick scaling factor. Inward dir ticks times this factor is considered to 
    be the same as outward dir tick numbers. This is to compensate  for DC motor 
    incapabilities with regards to exact positioning at least a bit on the mass
    dependent path.... */
    IUFillNumber(&InOutScalarN[0], "In/Out Scalar", "In/Out Scalar", "%1.2f", 0., 2., 1., 1.0);
    IUFillNumberVector(&InOutScalarNP, InOutScalarN, 1, getDeviceName(), "FOCUS_DIRSCALAR", "Direction scaling factor", OPTIONS_TAB, IP_RW, 0, IPS_IDLE );

    /* Nfocus should stay within these limits */
    IUFillNumber(&MinMaxPositionN[0], "MINPOS", "Minimum Tick", "%6.0f", -65000., 65000., 0., -65000. );
    IUFillNumber(&MinMaxPositionN[1], "MAXPOS", "Maximum Tick", "%6.0f", 1., 65000., 0., 65000.);
    IUFillNumberVector(&MinMaxPositionNP, MinMaxPositionN, 2, getDeviceName(), "FOCUS_MINMAXPOSITION", "Extrema", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&MaxTravelN[0], "MAXTRAVEL", "Maximum travel", "%6.0f", 1., 64000., 0., 10000.);
    IUFillNumberVector(&MaxTravelNP, MaxTravelN, 1, getDeviceName(), "FOCUS_MAXTRAVEL", "Max. travel", OPTIONS_TAB, IP_RW, 0, IPS_IDLE );

    /* Set Nfocus position register to this position */
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

void NFocus::ISGetProperties(const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);

    defineText(&PortTP);


}

bool NFocus::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&TemperatureNP);
        defineNumber(&SettingsNP);
        defineNumber(&InOutScalarNP);
        defineNumber(&MinMaxPositionNP);
        defineNumber(&MaxTravelNP);
        defineNumber(&SetRegisterPositionNP);
        defineNumber(&SetBacklashNP);
        defineNumber(&FocusRelPosNP);
        defineNumber(&FocusAbsPosNP);

        GetFocusParams();

       IDMessage(getDeviceName(), "NFocus paramaters readout complete, focuser ready for use.");
    }
    else
    {

        deleteProperty(TemperatureNP.name);
        deleteProperty(SettingsNP.name);
        deleteProperty(InOutScalarNP.name);
        deleteProperty(MinMaxPositionNP.name);
        deleteProperty(MaxTravelNP.name);
        deleteProperty(SetRegisterPositionNP.name);
        deleteProperty(SetBacklashNP.name);
        deleteProperty(FocusRelPosNP.name);
        deleteProperty(FocusAbsPosNP.name);

    }

    return true;

}

bool NFocus::Connect()
{
    int connectrc=0;
    char errorMsg[MAXRBUF];


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


    IDMessage(getDeviceName(), "Nfocus is online. Getting focus parameters...");

    return true;
}

bool NFocus::Disconnect()
{
    tty_disconnect(PortFD);
    IDMessage(getDeviceName(), "NFocus is offline.");
    return true;
}

const char * NFocus::getDefaultName()
{
    return "NFocus";
}

int NFocus::SendCommand(char *rf_cmd)
{

  int nbytes_written=0, nbytes_read=0, check_ret=0, err_code=0;
  char rf_cmd_cks[32],nfocus_error[MAXRBUF];


  if (isDebug())
  {
   fprintf(stderr, "strlen(rf_cmd) %d\n", strlen(rf_cmd)) ;
   fprintf(stderr, "WRITE: ") ;
   for(int i=0; i < strlen(rf_cmd); i++)
   {

     fprintf(stderr, "0x%2x ", (unsigned char) rf_cmd[i]) ;
   }
   fprintf(stderr, "\n") ;
  }


  tcflush(PortFD, TCIOFLUSH);

   if  ( (err_code = tty_write(PortFD, rf_cmd, strlen(rf_cmd)+1, &nbytes_written) != TTY_OK))
   {
        tty_error_msg(err_code, nfocus_error, MAXRBUF);
        if (isDebug())
            IDLog("TTY error detected: %s\n", nfocus_error);
        return -1;
   }

   return 0;

}

int NFocus::SendRequest(char *rf_cmd)
{

  int nbytes_written=0, nbytes_read=0, check_ret=0, err_code=0;
  char rf_cmd_cks[32],nfocus_error[MAXRBUF];

  unsigned char val= 0 ;

  SendCommand(rf_cmd);


  nbytes_read= ReadResponse(rf_cmd, strlen(rf_cmd), NF_TIMEOUT) ;

  if (nbytes_read < 1)
   return nbytes_read;

  rf_cmd[ nbytes_read - 1] = 0 ;

  if (isDebug())
      IDLog("Reply is (%s)\n", rf_cmd);

   return 0;

}

int NFocus::ReadResponse(char *buf, int nbytes, int timeout)
{

  char nfocus_error[MAXRBUF];
  int bytesRead = 0;
  int totalBytesRead = 0;
  int err_code;

  if (isDebug())
      IDLog("##########################################\n")
;
  while (totalBytesRead < nbytes)
  {
      if ( (err_code = tty_read(PortFD, buf+totalBytesRead, nbytes-totalBytesRead, timeout, &bytesRead)) != TTY_OK)
      {
              tty_error_msg(err_code, nfocus_error, MAXRBUF);
              if (isDebug())
              {
                  IDLog("TTY error detected: %s\n", nfocus_error);
                  IDMessage(getDeviceName(), "TTY error detected: %s\n", nfocus_error);
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

        totalBytesRead += bytesRead;
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

int NFocus::updateNFPosition(double *value)
{

  double temp=currentPosition ;

  *value = (double) temp;

  return temp;

}

int NFocus::updateNFTemperature(double *value)
{

  float temp ;
  char rf_cmd[32] ;
  int ret_read_tmp ;

  strcpy(rf_cmd, ":RT" ) ;

  if ((ret_read_tmp= SendRequest( rf_cmd)) < 0)
    return ret_read_tmp;


  if (sscanf(rf_cmd, "%6f", &temp) == -888)
    return -1;

  *value = (double) temp/10.;

  return 0;
}

// not used so far
int NFocus::updateNFBacklash(double *value)
{

  return 0;
}

int NFocus::updateNFInOutScalar(double *value)
{

  double temp=currentInOutScalar ;

  *value = (double) temp;

  return temp;
}

//  not used so far
int NFocus::updateNFFirmware(char *rf_cmd)
{

return 0;
}

int NFocus::updateNFMotorSettings(double *onTime, double *offTime, double *fastDelay)
{
  char rf_cmd[32] ;
  int ret_read_tmp ;

  if(( *onTime>= 100 ) && (*onTime<= 250)){
    snprintf(rf_cmd, 32, ":CO%3d#", (int) *onTime ) ;
  } else if(( *onTime >= 10 ) && (*onTime <=99)){
    snprintf(rf_cmd, 32, ":CO0%2d#", (int) *onTime ) ;
  }

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp;

  strncpy(rf_cmd, ":RO\0", 4);
  if ((ret_read_tmp= SendRequest( rf_cmd)) < 0)
    return ret_read_tmp;
 
  *onTime=  (float) atof(rf_cmd) ;

  if(( *offTime>= 100 ) && (*offTime<= 250)){
    snprintf(rf_cmd, 32, ":CF%3d#", (int) *offTime ) ;
  } else if(( *offTime >= 10 ) && (*offTime <=99)){
    snprintf(rf_cmd, 32, ":CF0%2d#", (int) *offTime ) ;
  } else if(( *offTime >= 1 ) && (*offTime <=9)){
    snprintf(rf_cmd, 32, ":CF00%1d#", (int) *offTime ) ;
  }

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp;

  strncpy(rf_cmd, ":RF\0", 4);
  if ((ret_read_tmp= SendRequest( rf_cmd)) < 0)
    return ret_read_tmp;
 
 *offTime= (float) atof(rf_cmd) ;

  if(( *fastDelay >= 1 ) && (*fastDelay <=9)){
    snprintf(rf_cmd, 32, ":CS00%1d#", (int) *fastDelay ) ;
  }

  if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
    return ret_read_tmp;

  strncpy(rf_cmd, ":RS\0", 4);
  if ((ret_read_tmp= SendRequest( rf_cmd)) < 0)
    return ret_read_tmp;
 
 *fastDelay= (float) atof(rf_cmd) ;

  return 0;
}

int NFocus::updateNFPositionRelativeInward(double *value)
{

  char rf_cmd[32] ;
  int ret_read_tmp ;
  float temp ;
  int cont = 1;
  rf_cmd[0]= 0 ;

  int newval = (int)(currentInOutScalar * (*value));
  IDMessage(getDeviceName(),"Moving %d real steps but virtually counting %.0f\n",newval,*value);

  do {
    if ( newval > 999) { 
        strncpy(rf_cmd, ":F01999#\0", 9) ;
      newval -= 999;
    } else if(newval > 99) {
      snprintf( rf_cmd, 32, ":F01%3d#", (int) newval) ;
      newval = 0;
    } else if(newval > 9) {
      snprintf( rf_cmd, 32, ":F010%2d#", (int) newval) ;
      newval = 0;
    } else {
      snprintf( rf_cmd, 32, ":F0100%1d#", (int) newval) ;
      newval = 0;
    }
  
    if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
      return ret_read_tmp;
    
    do {
     strncpy(rf_cmd, "S\0", 2);
     if ((ret_read_tmp= SendRequest( rf_cmd)) < 0)
       return ret_read_tmp;
    } while ( (int)atoi(rf_cmd) );
  } while ( newval > 0 );
  
  currentPosition -= *value;
  return 0;
}

int NFocus::updateNFPositionRelativeOutward(double *value)
{

  char rf_cmd[32] ;
  int ret_read_tmp ;
  float temp ;

  rf_cmd[0]= 0 ;

  int newval = (int)(*value);
  do {
    if ( newval > 999) { 
      strncpy( rf_cmd, ":F11999#\0", 9) ;
      newval -= 999;
    } else if(newval > 99) {
      snprintf( rf_cmd, 32, ":F11%3d#", (int) newval) ;
      newval = 0;
    } else if(newval > 9) {
      snprintf( rf_cmd, 32, ":F110%2d#", (int) newval) ;
      newval = 0;
    } else {
      snprintf( rf_cmd, 32, ":F1100%1d#", (int) newval) ;
      newval = 0;
    }
  
    if ((ret_read_tmp= SendCommand( rf_cmd)) < 0)
      return ret_read_tmp;
  
    do {
     strncpy(rf_cmd, "S\0", 2);
     if ((ret_read_tmp= SendRequest( rf_cmd)) < 0)
       return ret_read_tmp;
    } while ( (int)atoi(rf_cmd) );
  } while ( newval > 0 );

  currentPosition += *value;
  return 0;
}

int NFocus::updateNFPositionAbsolute(double *value)
{
  double newAbsPos = 0;

  if((*value - currentPosition) >= 0) {
	newAbsPos = *value - currentPosition;
	updateNFPositionRelativeOutward(&newAbsPos);
  }
  else if ((*value - currentPosition) < 0) {
	newAbsPos = currentPosition - *value;
	updateNFPositionRelativeInward(&newAbsPos);
  }
  
  currentPosition = *value;

  return 0;
}

int NFocus::updateNFMaxPosition(double *value)
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
      snprintf( vl_tmp, 6, "%5d", (int) *value) ;

    } else if(*value > 999) {

      snprintf( vl_tmp, 6, "0%4d", (int) *value) ;

    } else if(*value > 99) {
      snprintf( vl_tmp, 6, "00%3d", (int) *value) ;

    } else if(*value > 9) {
      snprintf( vl_tmp, 6, "000%2d", (int) *value) ;
    } else {
      snprintf( vl_tmp, 6, "0000%1d", (int) *value) ;
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

int NFocus::updateNFSetPosition(double *value)
{

  char rf_cmd[32] ;
  char vl_tmp[6] ;
  int ret_read_tmp ;

  rf_cmd[0]=  'F' ;
  rf_cmd[1]=  'S' ;
  rf_cmd[2]=  '0' ;

  if(*value > 9999) {
    snprintf( vl_tmp, 6, "%5d", (int) *value) ;
  } else if(*value > 999) {
    snprintf( vl_tmp, 6, "0%4d", (int) *value) ;
  } else if(*value > 99) {
    snprintf( vl_tmp, 6, "00%3d", (int) *value) ;
  } else if(*value > 9) {
    snprintf( vl_tmp, 6, "000%2d", (int) *value) ;
  } else {
    snprintf( vl_tmp, 6, "0000%1d", (int) *value) ;
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

bool NFocus::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
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

bool NFocus::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool NFocus::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    int nset=0,i=0;

    if(strcmp(dev,getDeviceName())==0)
    {

        if (!strcmp (name, SettingsNP.name))
        {
          /* new speed */
          double new_onTime = 0 ;
          double new_offTime = 0 ;
          double new_fastDelay = 0 ;
          int ret = -1 ;

          for (nset = i = 0; i < n; i++)
          {
            /* Find numbers with the passed names in the SettingsNP property */
            INumber *eqp = IUFindNumber (&SettingsNP, names[i]);

            /* If the number found is  (SettingsN[0]) then process it */
            if (eqp == &SettingsN[0])
            {

              new_onTime = (values[i]);
              nset += new_onTime >= 10 && new_onTime <= 250;
            } else if  (eqp == &SettingsN[1])
            {

              new_offTime = (values[i]);
              nset += new_offTime >= 1 && new_offTime <= 250;
            } else if  (eqp == &SettingsN[2])
            {

              new_fastDelay = (values[i]);
              nset += new_fastDelay >= 1 && new_fastDelay <= 9;
            }
          }

          /* Did we process the three numbers? */
          if (nset == 3)
          {

            /* Set the nfocus state to BUSY */
            SettingsNP.s = IPS_BUSY;


            IDSetNumber(&SettingsNP, NULL);

            if(( ret= updateNFMotorSettings(&new_onTime, &new_offTime, &new_fastDelay))< 0)
            {

              IDSetNumber(&SettingsNP, "Changing to new settings failed");
              return false;
            }

            currentOnTime = new_onTime ;
            currentOffTime= new_offTime ;
            currentFastDelay= new_fastDelay ;

            SettingsNP.s = IPS_OK;
            IDSetNumber(&SettingsNP, "Motor settings are now  %3.0f %3.0f %3.0f", currentOnTime, currentOffTime, currentFastDelay);
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

              /* Set the nfocus state to BUSY */
              SetBacklashNP.s = IPS_BUSY;
              IDSetNumber(&SetBacklashNP, NULL);

              if(( ret= updateNFBacklash(&new_back)) < 0) {

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


        if (!strcmp (name, InOutScalarNP.name))
        {

          double new_ioscalar = 0 ;

          for (nset = i = 0; i < n; i++)
          {
            /* Find numbers with the passed names in the InOutScalarNP property */
            INumber *eqp = IUFindNumber (&InOutScalarNP, names[i]);

            /* If the number found is SetBacklash (SetBacklashN[0]) then process it */
            if (eqp == &InOutScalarN[0]){

              new_ioscalar = (values[i]);

              /* limits */
              nset += new_ioscalar >= 0 && new_ioscalar <= 2;
            }

            if (nset == 1) {

              /* Set the nfocus state to BUSY */
              InOutScalarNP.s = IPS_BUSY;
          /* kraemerf
	    IDSetNumber(&InOutScalarNP, NULL);

              if(( ret= updateNFInOutScalar(&new_ioscalar)) < 0) {

                InOutScalarNP.s = IPS_IDLE;
                IDSetNumber(&InOutScalarNP, "Setting new in/out scalar failed.");

                return false ;
              }
	*/

              currentInOutScalar=  new_ioscalar ;
              InOutScalarNP.s = IPS_OK;
              IDSetNumber(&InOutScalarNP, "Direction Scalar is now  %1.2f", currentInOutScalar) ;
              return true;
            } else {

              InOutScalarNP.s = IPS_IDLE;
              IDSetNumber(&InOutScalarNP, "Need exactly one parameter.");

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

            /* Set the nfocus state to BUSY */
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

            if(( ret= updateNFMaxPosition(&new_maxt))< 0 )
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

              /* Set the nfocus state to BUSY */
              SetRegisterPositionNP.s = IPS_BUSY;
              IDSetNumber(&SetRegisterPositionNP, NULL);

              if(( ret= updateNFSetPosition(&new_apos)) < 0)
              {

                SetRegisterPositionNP.s = IPS_OK;
                IDSetNumber(&SetRegisterPositionNP, "Read out of the set position to %3d failed. Trying to recover the position", ret);

                if((ret= updateNFPosition( &currentPosition)) < 0)
                {

                  FocusAbsPosNP.s = IPS_ALERT;
                  IDSetNumber(&FocusAbsPosNP, "Unknown error while reading  Nfocus position: %d", ret);

                  SetRegisterPositionNP.s = IPS_IDLE;
                  IDSetNumber(&SetRegisterPositionNP, "Relative movement failed.");
                }

                SetRegisterPositionNP.s = IPS_OK;
                IDSetNumber(&SetRegisterPositionNP, NULL);


                FocusAbsPosNP.s = IPS_OK;
                IDSetNumber(&FocusAbsPosNP, "Nfocus position recovered %5.0f", currentPosition);
                IDMessage( getDeviceName(), "Nfocus position recovered resuming normal operation");
                /* We have to leave here, because new_apos is not set */
                return true ;
              }
              currentPosition= new_apos ;
              SetRegisterPositionNP.s = IPS_OK;
              IDSetNumber(&SetRegisterPositionNP, "Nfocus register set to %5.0f", currentPosition);

              FocusAbsPosNP.s = IPS_OK;
              IDSetNumber(&FocusAbsPosNP, "Nfocus position is now %5.0f", currentPosition);

              return true ;

            } else
            {

              SetRegisterPositionNP.s = IPS_IDLE;
              IDSetNumber(&SetRegisterPositionNP, "Need exactly one parameter.");

              return false;
            }

            if((ret= updateNFPosition(&currentPosition)) < 0)
            {

              FocusAbsPosNP.s = IPS_ALERT;
              IDSetNumber(&FocusAbsPosNP, "Unknown error while reading  Nfocus position: %d", ret);

              return false ;
            }

            SetRegisterPositionNP.s = IPS_OK;
            IDSetNumber(&SetRegisterPositionNP, "Nfocus has accepted new register setting" ) ;

            FocusAbsPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, "Nfocus new position %5.0f", currentPosition);

            return true;
          }
        }



    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

}

void NFocus::GetFocusParams ()
{

  int ret = -1 ;
  int cur_s1LL=0 ;
  int cur_s2LR=0 ;
  int cur_s3RL=0 ;
  int cur_s4RR=0 ;


      if((ret= updateNFPosition(&currentPosition)) < 0)
      {
        FocusAbsPosNP.s = IPS_ALERT;
        IDSetNumber(&FocusAbsPosNP, "Unknown error while reading  Nfocus position: %d", ret);
        return;
      }

      FocusAbsPosNP.s = IPS_OK;
      IDSetNumber(&FocusAbsPosNP, NULL);

      FocusAbsPosN[0].value = currentPosition;
      IDSetNumber(&FocusAbsPosNP, NULL);

      currentInOutScalar= INOUTSCALAR_READOUT ;
      if(( ret= updateNFInOutScalar(&currentInOutScalar)) < 0)
      {
        InOutScalarNP.s = IPS_ALERT;
        IDSetNumber(&InOutScalarNP, "Unknown error while reading  Nfocus direction tick scalar");
        return;
      }
      InOutScalarNP.s = IPS_OK;
      IDSetNumber(&InOutScalarNP, NULL);

      if(( ret= updateNFTemperature(&currentTemperature)) < 0)
      {
        TemperatureNP.s = IPS_ALERT;
        IDSetNumber(&TemperatureNP, "Unknown error while reading  Nfocus temperature");
        return;
      }
      TemperatureNP.s = IPS_OK;
      IDSetNumber(&TemperatureNP, NULL);


      currentBacklash= BACKLASH_READOUT ;
      if(( ret= updateNFBacklash(&currentBacklash)) < 0)
      {
        SetBacklashNP.s = IPS_ALERT;
        IDSetNumber(&SetBacklashNP, "Unknown error while reading  Nfocus backlash");
        return;
      }
      SetBacklashNP.s = IPS_OK;
      IDSetNumber(&SetBacklashNP, NULL);

      currentOnTime= currentOffTime= currentFastDelay=0 ;

      if(( ret= updateNFMotorSettings(&currentOnTime, &currentOffTime, &currentFastDelay )) < 0)
      {
        SettingsNP.s = IPS_ALERT;
        IDSetNumber(&SettingsNP, "Unknown error while reading  Nfocus motor settings");
        return;
      }

      SettingsNP.s = IPS_OK;
      IDSetNumber(&SettingsNP, NULL);

      currentMaxTravel= MAXTRAVEL_READOUT ;
      if(( ret= updateNFMaxPosition(&currentMaxTravel)) < 0)
      {
        MaxTravelNP.s = IPS_ALERT;
        IDSetNumber(&MaxTravelNP, "Unknown error while reading  Nfocus maximum travel");
        return;
      }
      MaxTravelNP.s = IPS_OK;
      IDSetNumber(&MaxTravelNP, NULL);

}

int NFocus::Move(FocusDirection dir, int speed, int duration)
{
    INDI_UNUSED(speed);
    double pos=0;
    struct timeval tv_start;
    struct timeval tv_finish;
    double dt=0;
    gettimeofday (&tv_start, NULL);

    while (duration > 0)
    {

        pos = NF_STEP_RES;

    if (dir == FOCUS_INWARD)
	{
   		fprintf(stderr, "FOCUS_IN: ") ;
        	updateNFPositionRelativeInward(&pos);
	}
    else
	{
   		fprintf(stderr, "FOCUS_OUT: ") ;
        	updateNFPositionRelativeOutward(&pos);
	}

        gettimeofday (&tv_finish, NULL);

        dt = tv_finish.tv_sec - tv_start.tv_sec + (tv_finish.tv_usec - tv_start.tv_usec)/1e6;



        duration -= dt * 1000;

      // IDLog("dt is: %g --- duration is: %d -- pos: %g\n", dt, duration, pos);
    }

   return true;
}


int NFocus::MoveAbs(int targetTicks)
{
    int ret= -1 ;
    double new_apos = targetTicks;

    if (targetTicks < FocusAbsPosN[0].min || targetTicks > FocusAbsPosN[0].max)
    {
        IDMessage(getDeviceName(), "Error, requested absolute position is out of range.");
        return -1;
    }

    IDMessage(getDeviceName() , "Focuser is moving to requested position...");

    if(( ret= updateNFPositionAbsolute(&new_apos)) < 0)
    {

        IDMessage(getDeviceName(), "Read out of the absolute movement failed %3d, trying to recover position.", ret);

        for (int i=0;; i++)
        {
            if((ret= updateNFPosition(&currentPosition)) < 0)
            {
                IDMessage(getDeviceName(),"Unknown error while reading  Nfocus position: %d.", ret);
                if (i == NF_MAX_TRIES)
                    return false;
                else
                    usleep(NF_MAX_DELAY);
            }
            else break;
      }

      IDMessage( getDeviceName(), "Nfocus position recovered resuming normal operation");
      /* We have to leave here, because new_apos is not set */
      return -1;
    }


    return 0;
}

int NFocus::MoveRel(FocusDirection dir, unsigned int ticks)
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

          if( dir == FOCUS_OUTWARD)
          {
	    if((currentPosition + new_rpos < currentMinPosition) || (currentPosition + new_rpos > currentMaxPosition))
	    {
	      IDMessage(getDeviceName(), "Value out of limits %5.0f", currentPosition +  new_rpos);
	      return -1 ;
  	    }
            ret= updateNFPositionRelativeOutward(&new_rpos) ;
	  }
          else
	  {
	    if((currentPosition - new_rpos < currentMinPosition) || (currentPosition - new_rpos > currentMaxPosition))
	    {
	      IDMessage(getDeviceName(), "Value out of limits %5.0f", currentPosition -  new_rpos);
	      return -1 ;
  	    }
            ret= updateNFPositionRelativeInward(&new_rpos) ;
	  }

          if( ret < 0)
          {

            IDMessage(getDeviceName(), "Read out of the relative movement failed, trying to recover position.");
            if((ret= updateNFPosition(&currentPosition)) < 0)
            {

              IDMessage(getDeviceName(), "Unknown error while reading  Nfocus position: %d", ret);
              return false;
            }

            IDMessage(getDeviceName(), "Nfocus position recovered %5.0f", currentPosition);
            // We have to leave here, because new_rpos is not set
            return -1 ;
          }

          currentRelativeMovement= cur_rpos ;
//          currentAbsoluteMovement= currentPosition;
          return 0;
        }
        {
            IDMessage(getDeviceName(), "Value out of limits.");
            return -1;
        }


}

bool NFocus::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &PortTP);
    IUSaveConfigNumber(fp, &SettingsNP);
    IUSaveConfigNumber(fp, &SetBacklashNP);
    IUSaveConfigNumber(fp, &InOutScalarNP);
}

