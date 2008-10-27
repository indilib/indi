 #if 0
    Robofocus driver
    Copyright (C) 2006 Markus Wildi, markus.wildi@datacomm.ch

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

#endif

/* Standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* INDI Core headers */
/* indidevapi.h contains API declerations */
#include "indidevapi.h"

/* INDI Eventloop mechanism */
#include "eventloop.h"

/* INDI Common Routines/RS232 */
#include "indicom.h"

#include "robofocusdriver.h"

/* Definitions */

#define mydev	           "Robofocus"     /* Device name */
#define CONNECTION_GROUP   "Connection"  /* Group name */
#define MOTION_GROUP       "Motion"
 
#define currentSpeed            SpeedN[0].value
#define currentPosition         PositionN[0].value
#define currentTemperature      TemperatureN[0].value
#define currentBacklash         SetBacklashN[0].value
#define currentDuty             SettingsN[0].value
#define currentDelay            SettingsN[1].value
#define currentTicks            SettingsN[2].value
#define currentRelativeMovement RelMovementN[0].value
#define currentAbsoluteMovement AbsMovementN[0].value
#define currentSetBacklash      SetBacklashN[0].value
#define currentMinPosition      MinMaxPositionN[0].value
#define currentMaxPosition      MinMaxPositionN[1].value
#define currentMaxTravel        MaxTravelN[0].value

int fd; /* Rs 232 file handle */
int wp ;  /* Work proc ID */


/* Function protptypes */

void connectRobofocus(void);

static void ISInit() ;
void ISWorkProc (void) ;
int updateRFPosition(int fd, double *value) ;
int updateRFTemperature(int fd, double *value) ;
int updateRFBacklash(int fd, double *value) ;
int updateRFFirmware(int fd, char *rf_cmd) ;
int updateRFMotorSettings(int fd, double *duty, double *delay, double *ticks) ;
int updateRFPositionRelativeInward(int fd, double *value) ;
int updateRFPositionRelativeOutward(int fd, double *value) ;
int updateRFPositionAbsolute(int fd, double *value) ;
int updateRFPowerSwitches(int fd, int s, int  i, int *cur_s1LL, int *cur_s2LR, int *cur_s3RL, int *cur_s4RR) ;
int updateRFMaxPosition(int fd, double *value) ;
int updateRFSetPosition(int fd, double *value) ;

/* operational info */


/* RS 232 Connection */

static ISwitch PowerS[] = {
  {"CONNECT", "Connect", ISS_OFF, 0, 0},                         
  {"DISCONNECT", "Disconnect", ISS_OFF, 0, 0},
};

static ISwitchVectorProperty PowerSP    = { 
  mydev			/* Device name */
  , "CONNECTION"  	/* Property name */
  , "Connection"	/* Property label */
  , CONNECTION_GROUP	/* Property group */
  , IP_RW		/* Property premission, it's both read and write */
  , ISR_1OFMANY	        /* Switch behavior. Only 1 of many switches are allowed to be on at the same time */
  , 0			/* Timeout, 0 seconds */
  , IPS_IDLE		/* Initial state is idle */
  , PowerS		/* Switches comprimising this vector that we defined above */
  , NARRAY(PowerS)	/* Number of Switches. NARRAY is defined in indiapi.h */
  , ""			/* Timestamp, set to 0 */
  , 0};			/* auxiluary, set to 0 for now */

/* Serial */

static IText PortT[]= {{"PORT", "Port", 0, 0, 0, 0}}; /* Attention malloc */

static ITextVectorProperty PortTP= { 
  mydev, 
  "DEVICE_PORT", 
  "Ports", 
  CONNECTION_GROUP, 
  IP_RW, 
  0, 
  IPS_IDLE, 
  PortT, 
  NARRAY(PortT), 
  "", 
  0
} ;

/* Focuser temperature */

static INumber TemperatureN[] = {

  { "TEMPERATURE", "Celsius", "%6.2f", 0, 65000., 0., 10000., 0, 0, 0},				
} ;

static INumberVectorProperty TemperatureNP = { 
  mydev, 
  "FOCUS_TEMPERATURE", 
  "Temperature", 
  CONNECTION_GROUP, 
  IP_RO, 
  0, 
  IPS_IDLE, 
  TemperatureN, 
  NARRAY(TemperatureN), 
  "", 
  0
} ;

/* Settings of the Robofocus */

static INumber SettingsN[] = {

  { "Duty cycle", "Duty cycle", "%6.0f", 0., 255., 0., 1., 0, 0, 0},
  { "Step delay", "Step delay", "%6.0f", 0., 255., 0., 1., 0, 0, 0},
  { "Motor Steps", "Motor steps per tick", "%6.0f", 0, 255., 0., 1., 0, 0, 0},
};

static INumberVectorProperty SettingsNP = { 
  mydev, 
  "FOCUS_SETTINGS", 
  "Settings", 
  CONNECTION_GROUP, 
  IP_RW, 
  0, 
  IPS_IDLE, 
  SettingsN, 
  NARRAY(SettingsN), 
  "", 
  0
} ;

/* Power Switches of the Robofocus */
static ISwitch PowerSwitchesS[]= {

  {"1", "Switch 1", ISS_OFF, 0, 0}, 
  {"2", "Switch 2", ISS_OFF, 0, 0}, 
  {"3", "Switch 3", ISS_OFF, 0, 0}, 
  {"4", "Switch 4", ISS_ON , 0, 0}
};

static ISwitchVectorProperty PowerSwitchesSP      = { 
  mydev, 
  "SWITCHES", 
  "Power", 
  CONNECTION_GROUP, 
  IP_RW,
  ISR_1OFMANY, 
  0, 
  IPS_IDLE, 
  PowerSwitchesS, 
  NARRAY(PowerSwitchesS), 
  "", 
  0 
} ;

/* Direction of the focuser movement - or IGNORE */

static ISwitch DirectionS[] = {

  {"FOCUSIN", "inward", ISS_OFF, 0, 0},	
  {"FOCUSOUT", "outward", ISS_OFF, 0, 0},	
  {"FOCUSIGNORE", "IGNORE", ISS_ON, 0, 0},
} ;

static ISwitchVectorProperty DirectionSP    = { 
 
  mydev, 
  "DIRECTION", 
  "Movement",
  MOTION_GROUP,	   
  IP_RW,	
  ISR_ATMOST1,
  0,
  IPS_IDLE,
  DirectionS,
  NARRAY(DirectionS),
  "",	
  0
} ;	

/* Robofocus should stay within these limits */

static INumber MinMaxPositionN[] = {

  { "MINPOS", "Minimum Tick", "%6.0f", 1., 65000., 0., 100., 0, 0, 0},
  { "MAXPOS", "Maximum Tick", "%6.0f", 1., 65000., 0., 55000., 0, 0, 0},
};

static INumberVectorProperty MinMaxPositionNP = { 
  mydev, 
  "FOCUS_MINMXPOSITION", 
  "Extrema", 
  MOTION_GROUP, 
  IP_RW, 
  0, 
  IPS_IDLE, 
  MinMaxPositionN, 
  NARRAY(MinMaxPositionN), 
  "", 
  0
};

static INumber MaxTravelN[] = {

  { "MAXTRAVEL", "Maximum travel", "%6.0f", 1., 64000., 0., 10000., 0, 0, 0},
};

static INumberVectorProperty MaxTravelNP = { 
  mydev, 
  "FOCUS_MAXTRAVEL", 
  "Max. travel", 
  MOTION_GROUP, 
  IP_RW, 
  0, 
  IPS_IDLE, 
  MaxTravelN, 
  NARRAY(MaxTravelN), 
  "", 
  0
};

/* Set Robofocus position register to the this position */

static INumber SetRegisterPositionN[] = {

  /* 64000: see Robofocus manual */
  { "SETPOS", "Position", "%6.0f", 0, 64000., 0., 0., 0, 0, 0},	
};

static INumberVectorProperty SetRegisterPositionNP = { 
  mydev, 
  "FOCUS_REGISTERPOSITION", 
  "Set register", 
  MOTION_GROUP, 
  IP_RW, 
  0, 
  IPS_IDLE, 
  SetRegisterPositionN, 
  NARRAY(SetRegisterPositionN), 
  "", 
  0
};
/* Set Robofocus' backlash behaviour */

static INumber SetBacklashN[] = {

  { "SETBACKLASH", "Backlash", "%6.0f", -255., 255., 0., 0., 0, 0, 0},
};

static INumberVectorProperty SetBacklashNP = { 
  mydev, 
  "FOCUS_Backlash", 
  "Set register", 
  MOTION_GROUP, 
  IP_RW, 
  0, 
  IPS_IDLE, 
  SetBacklashN, 
  NARRAY(SetBacklashN), 
  "", 
  0
};

/* Speed */
static INumber SpeedN[] = {

  {"SPEED", "Ticks/sec", "%6.0f",0., 999., 0., 50., 0, 0, 0},
} ;

static INumberVectorProperty SpeedNP = { 
  mydev, 
  "FOCUS_SPEED", 
  "Speed", 
  MOTION_GROUP, 
  IP_RW, 
  0, 
  IPS_IDLE, 
  SpeedN, 
  NARRAY(SpeedN), 
  "", 
  0
} ;


/*  Timer */
static INumber TimerN[] = {

  {"TIMER","sec", "%6.0f",0., 999., 0., 0., 0, 0, 0},
} ;

static INumberVectorProperty TimerNP = { 

  mydev, 
  "FOCUS_TIMER", 
  "Timer", 
  MOTION_GROUP, 
  IP_RW, 
  0, 
  IPS_IDLE, 
  TimerN, 
  NARRAY(TimerN), 
  "", 
  0
} ;

/* Robofocus` position (RO) */

static INumber PositionN[] = {

  { "POSITION", "Tick", "%6.0f", 0, 65000., 0., 10000., 0, 0, 0},				
} ;

static INumberVectorProperty PositionNP = { 
  mydev, 
  "FOCUS_POSITION", 
  "Position", 
  MOTION_GROUP, 
  IP_RO, 
  0, 
  IPS_IDLE, 
  PositionN, 
  NARRAY(PositionN), 
  "", 
  0
} ;

/* Relative and absolute movement */
static INumber RelMovementN[] = {

  { "RELMOVEMENT", "Ticks", "%6.0f", -65000., 65000., 0., 100., 0, 0, 0},
				
};

static INumberVectorProperty RelMovementNP = { 

  mydev, 
  "FOCUS_RELMOVEMENT", 
  "Relative goto", 
  MOTION_GROUP, 
  IP_RW, 
  0, 
  IPS_IDLE, 
  RelMovementN, 
  NARRAY(RelMovementN), 
  "", 
  0
} ;


static INumber AbsMovementN[] = {

  { "ABSMOVEMENT", "Tick", "%6.0f", 0, 65000., 0., 10000., 0, 0, 0},				
} ;

static INumberVectorProperty AbsMovementNP = { 
  mydev, 
  "FOCUS_ABSMOVEMENT", 
  "Absolute goto", 
  MOTION_GROUP, 
  IP_RW, 
  0, 
  IPS_IDLE, 
  AbsMovementN, 
  NARRAY(AbsMovementN), 
  "", 
  0
} ;


/* Initlization routine */
static void ISInit() {

  static int isInit=0;		/* set once mountInit is called */
  /*fprintf(stderr, "ISInit\n") ;*/

  if (isInit)
    return;

  /* stolen from temmadriver.c :-)*/

  PortT[0].text = malloc( 13);
  if (!PortT[0].text ){
    fprintf(stderr,"Memory allocation error");
    return;
  }
  IUSaveText( &PortT[0],  "/dev/ttyUSB0");

  /* fprintf(stderr, "PORT:%s<\n", PortT->text) ; */
  IDSetText( &PortTP, NULL) ;

  fd=-1;
  isInit = 1;
}


void ISWorkProc () {

  char firmeware[]="FV0000000" ;
  int ret = -1 ;
  int cur_s1LL=0 ;
  int cur_s2LR=0 ;
  int cur_s3RL=0 ;
  int cur_s4RR=0 ;

  /* fprintf(stderr, "ISWorkProc\n") ; */

  if (PowerS[0].s == ISS_ON) {
    switch (PowerSP.s) {

    case IPS_IDLE:
    case IPS_OK:

      if((ret= updateRFFirmware(fd, firmeware)) < 0) {
        /* This would be the end*/
	IDMessage( mydev, "Unknown error while reading Robofocus firmware\n");
	break;
      }
      PowerSP.s = IPS_OK;
      IDSetSwitch(&PowerSP, NULL);


      if((ret= updateRFPosition(fd, &currentPosition)) < 0) {
 	PositionNP.s = IPS_ALERT;
	IDSetNumber(&PositionNP, "Unknown error while reading  Robofocus position: %d", ret);
	break;
      }
      PositionNP.s = IPS_OK;
      IDSetNumber(&PositionNP, NULL); 

      if(( ret= updateRFTemperature(fd, &currentTemperature)) < 0) {
	TemperatureNP.s = IPS_ALERT;
	IDSetNumber(&TemperatureNP, "Unknown error while reading  Robofocus temperature");
	break;
      }
      TemperatureNP.s = IPS_OK;
      IDSetNumber(&TemperatureNP, NULL);


      currentBacklash= BACKLASH_READOUT ;
      if(( ret= updateRFBacklash(fd, &currentBacklash)) < 0) {
	SetBacklashNP.s = IPS_ALERT;
	IDSetNumber(&SetBacklashNP, "Unknown error while reading  Robofocus backlash");
	break;
      }
      SetBacklashNP.s = IPS_OK;
      IDSetNumber(&SetBacklashNP, NULL);

      currentDuty= currentDelay= currentTicks=0 ;

      if(( ret= updateRFMotorSettings(fd, &currentDuty, &currentDelay, &currentTicks )) < 0) {
	SettingsNP.s = IPS_ALERT;
	IDSetNumber(&SettingsNP, "Unknown error while reading  Robofocus motor settings");
	break;
      }
      SettingsNP.s = IPS_OK;
      IDSetNumber(&SettingsNP, NULL);

      if(( ret= updateRFPowerSwitches(fd,  -1, -1,  &cur_s1LL, &cur_s2LR, &cur_s3RL, &cur_s4RR)) < 0) {
	PowerSwitchesSP.s = IPS_ALERT;
	IDSetSwitch(&PowerSwitchesSP, "Unknown error while reading  Robofocus power swicht settings");
	break;
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
      if(( ret= updateRFMaxPosition(fd, &currentMaxTravel)) < 0) {
	MaxTravelNP.s = IPS_ALERT;
	IDSetNumber(&MaxTravelNP, "Unknown error while reading  Robofocus maximum travel");
	break;
      }
      MaxTravelNP.s = IPS_OK;
      IDSetNumber(&MaxTravelNP, NULL);

    case IPS_BUSY:

      break;
	        
    case IPS_ALERT:
      break;
    }
  }	
}

/* void ISGetProperties (const char *dev)
*  INDI will call this function when the client inquires about the device properties.
*  Here we will use IDxxx functions to define new properties to the client */
void ISGetProperties (const char *dev)
{ 
  /* fprintf(stderr, "ISGetProperties\n") ; */

   /* #1 Let's make sure everything has been initialized properly */
   ISInit();
  
  /* #2 Let's make sure that the client is asking for the properties of our device, otherwise ignore */
  if (dev && strcmp (mydev, dev))
    return;
    
  /* #3 Tell the client to create a new Switch property PowerSP */
  IDDefSwitch(&PowerSP, NULL);
  IDDefText(&PortTP, NULL);
  IDDefSwitch(&PowerSwitchesSP, NULL);
  IDDefNumber(&TemperatureNP, NULL);
  IDDefNumber(&SettingsNP, NULL);

  IDDefNumber(&MinMaxPositionNP, NULL);
  IDDefNumber(&MaxTravelNP, NULL);
  IDDefNumber(&SetRegisterPositionNP, NULL);
  IDDefNumber(&SetBacklashNP, NULL);
  IDDefSwitch(&DirectionSP, NULL);

  IDDefNumber(&PositionNP, NULL);
  IDDefNumber(&SpeedNP, NULL);
  IDDefNumber(&TimerNP, NULL);
  IDDefNumber(&AbsMovementNP, NULL);
  IDDefNumber(&RelMovementNP, NULL);
}
  
/* void ISNewSwitch(...)
 * INDI will call this function when the client wants to set a new state of existing switches 
 ** Parameters **
 * dev: the device name
 * name: the property name the client wants to update
 * states: an array of states of members switches (ISS_ON and ISS_OFF)
 * names: names of the switches. The names are parallel to the states. That is, member names[0] has a state states[0], and so on...
 * n: number of switches to update, which is also the dimension of *states and names[]
*/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) {

  /* fprintf(stderr, "ISNewSwitch\n") ; */

  /* #1 Let's make sure everything has been initialized properly */
  ISInit();
	
  /* #2 Let's make sure that the client is asking to update the properties of our device, otherwise ignore */
  if (dev && strcmp (dev, mydev))
    return;
	    
  /* #3 Now let's check if the property the client wants to change is the PowerSP (name: CONNECTION) property*/
  if (!strcmp (name, PowerSP.name)) {


    /* If the clients wants to update this property, let's perform the following */
	  
    /* A. We reset all switches (in this case CONNECT and DISCONNECT) to ISS_OFF */
    IUResetSwitch(&PowerSP);

    /* B. We update the switches by sending their names and updated states IUUpdateSwitch function */
    IUUpdateSwitch(&PowerSP, states, names, n);
	  
    /* C. We try to establish a connection to our device */
    connectRobofocus();
    return ;
  }

  if (!strcmp (name, PowerSwitchesSP.name)) {
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
    if (nset == 1) {
      cur_s1LL= cur_s2LR= cur_s3RL= cur_s4RR= 0 ;

      if(( ret= updateRFPowerSwitches(fd, new_s, new_sn, &cur_s1LL, &cur_s2LR, &cur_s3RL, &cur_s4RR)) < 0) {

	PowerSwitchesSP.s = IPS_ALERT;
	IDSetSwitch(&PowerSwitchesSP, "Unknown error while reading  Robofocus power swicht settings");
	return ;
      }
    } else {
      /* Set property state to idle */
      PowerSwitchesSP.s = IPS_IDLE ;
		
      IDSetNumber(&SettingsNP, "Power switch settings absent or bogus.");
      return ;
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
    IDSetSwitch(&PowerSwitchesSP, "Setting power switches");

   
  }
}


/* void ISNewText(...)
 * INDI will call this function when the client wants to update an existing text.
 ** Parameters **
 
 * dev: the device name
 * name: the property name the client wants to update
 * texts: an array of texts.
 * names: names of the members. The names are parallel to the texts.
 * n: number of texts to update, which is also the dimension of *texts and names[]
*/
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) {

  /* fprintf(stderr, "ISNewText\n") ; */

  /* #1 Let's make sure everything has been initialized properly */
  ISInit();

  /* #2 Let's make sure that the client is asking to update the properties of our device, otherwise ignore */
  if (dev && strcmp (dev, mydev))
    return;
 

  dev=dev; names=names; n=n;

  if (!strcmp(name, PortTP.name)) {
    IUSaveText(&PortT[0], texts[0]);
    PortTP.s = IPS_OK;
    IDSetText(&PortTP, NULL);
    return;
  }

  return;	
}

/* void ISNewNumber(...)
 * INDI will call this function when the client wants to update an existing number.
 ** Parameters **
 
 * dev: the device name
 * name: the property name the client wants to update
 * values: an array of values.
 * names: names of the members. The names are parallel to the values.
 * n: number of numbers to update, which is the dimension of *numbers and names[]
*/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) {
  
  int i, nset;
 
  /* fprintf(stderr, "ISNewNumber\n") ; */

  /* Make sure to initialize */
  ISInit();

  /* ignore if not ours */
  if (strcmp (dev, mydev))
    return;


  if (!strcmp (name, SettingsNP.name)) {
    /* new speed */
    double new_duty = 0 ;
    double new_delay = 0 ;
    double new_ticks = 0 ;
    int ret = -1 ;
	    
    /* Check power, if it is off, then return */
    if (PowerS[0].s != ISS_ON){
      SettingsNP.s = IPS_IDLE;
      IDSetNumber(&SettingsNP, "Power is off");
      return;
    }
	    
    for (nset = i = 0; i < n; i++){
      /* Find numbers with the passed names in the SettingsNP property */
      INumber *eqp = IUFindNumber (&SettingsNP, names[i]);
      
      /* If the number found is  (SettingsN[0]) then process it */
      if (eqp == &SettingsN[0]){
	      
	new_duty = (values[i]);
	nset += new_duty >= 0 && new_duty <= 255;
      } else if  (eqp == &SettingsN[1]){

 	new_delay = (values[i]);
	nset += new_delay >= 0 && new_delay <= 255;
      } else if  (eqp == &SettingsN[2]){

	new_ticks = (values[i]);
	nset += new_ticks >= 0 && new_ticks <= 255;
      }
    }
	    
    /* Did we process the three numbers? */
    if (nset == 3) {
		
      /* Set the robofocus state to BUSY */
      SettingsNP.s = IPS_BUSY;
		
		
      IDSetNumber(&SettingsNP, NULL);

      if(( ret= updateRFMotorSettings(fd,  &new_duty, &new_delay, &new_ticks))< 0) {
	
	IDSetNumber(&SettingsNP, "Changing to new settings failed");
	return ;
      } ; 

      currentDuty = new_duty ;
      currentDelay= new_delay ;
      currentTicks= new_ticks ;

      SettingsNP.s = IPS_OK;
      IDSetNumber(&SettingsNP, "Motor settings are now  %3.0f %3.0f %3.0f", currentDuty, currentDelay, currentTicks);

    } else {
      /* Set property state to idle */
      SettingsNP.s = IPS_IDLE;
		
      IDSetNumber(&SettingsNP, "Settings absent or bogus.");
      return ;
    }
  }

  if (!strcmp (name, RelMovementNP.name)) {

    double cur_rpos=0 ;
    double new_rpos = 0 ;
    int nset = 0;
    int ret= -1 ;

    if (PowerS[0].s != ISS_ON){
      RelMovementNP.s = IPS_IDLE;
      IDSetNumber(&RelMovementNP, "Power is off");
      return;
    }
    for (nset = i = 0; i < n; i++) {
      /* Find numbers with the passed names in the RelMovementNP property */
      INumber *eqp = IUFindNumber (&RelMovementNP, names[i]);
		
      /* If the number found is RelMovement (RelMovementN[0]) then process it */
      if (eqp == &RelMovementN[0]){
	      
	cur_rpos= new_rpos = (values[i]);
        
        /* CHECK 2006-01-26, limmits are relative to the actual position */
	nset += new_rpos >= -0xffff && new_rpos <= 0xffff;
      }

      if (nset == 1) {

	/* Set the robofocus state to BUSY */
	RelMovementNP.s = IPS_BUSY;
	IDSetNumber(&RelMovementNP, NULL);

	if((currentPosition + new_rpos < currentMinPosition) || (currentPosition + new_rpos > currentMaxPosition)) {

	  RelMovementNP.s = IPS_ALERT ;
	  IDSetNumber(&RelMovementNP, "Value out of limits %5.0f", currentPosition +  new_rpos);
	  return ;
	}

	if( new_rpos > 0) {

	  ret= updateRFPositionRelativeOutward(fd, &new_rpos) ;

	} else {

          new_rpos= -new_rpos ;
	  ret= updateRFPositionRelativeInward(fd, &new_rpos) ;
	}
	
	if( ret < 0) {

	  RelMovementNP.s = IPS_IDLE;
	  IDSetNumber(&RelMovementNP, "Read out of the relative movement failed, trying to recover position.");
	  if((ret= updateRFPosition(fd, &currentPosition)) < 0) {
	 

	    PositionNP.s = IPS_ALERT;
	    IDSetNumber(&PositionNP, "Unknown error while reading  Robofocus position: %d", ret);
	    return ;
	  }
	  PositionNP.s = IPS_ALERT;
	  IDSetNumber(&PositionNP, "Robofocus position recovered %5.0f", currentPosition); 

	  IDMessage( mydev, "Robofocus position recovered resuming normal operation");

          /* We have to leave here, because new_rpos is not set */
          return ;
	}

	RelMovementNP.s = IPS_OK;
        currentRelativeMovement= cur_rpos ;
	IDSetNumber(&RelMovementNP, NULL) ;

	AbsMovementNP.s = IPS_OK;
        currentAbsoluteMovement=  new_rpos - cur_rpos ;
	IDSetNumber(&AbsMovementNP, NULL) ;

	PositionNP.s = IPS_OK;
        currentPosition= new_rpos ;
	IDSetNumber(&PositionNP,  "Last position was %5.0f", currentAbsoluteMovement);

     } else {

	RelMovementNP.s = IPS_IDLE;
	IDSetNumber(&RelMovementNP, "Need exactly one parameter.");

	return ;
      }
    }
  }

  if (!strcmp (name, AbsMovementNP.name)) {

    double new_apos = 0 ;
    int nset = 0;
    int ret= -1 ;

    if (PowerS[0].s != ISS_ON){
      AbsMovementNP.s = IPS_IDLE;
      IDSetNumber(&AbsMovementNP, "Power is off");
      return;
    }
    for (nset = i = 0; i < n; i++) {
      /* Find numbers with the passed names in the AbsMovementNP property */
      INumber *eqp = IUFindNumber (&AbsMovementNP, names[i]);
      
      /* If the number found is AbsMovement (AbsMovementN[0]) then process it */
      if (eqp == &AbsMovementN[0]){
	
	new_apos = (values[i]);
	
        /* limits are absolute to the actual position */
	nset += new_apos >= 0 && new_apos <= 0xffff;
      }

      if (nset == 1) {

	/* Set the robofocus state to BUSY */
	AbsMovementNP.s = IPS_BUSY;
	IDSetNumber(&AbsMovementNP, NULL);

	if((new_apos < currentMinPosition) || (new_apos > currentMaxPosition)) {

	  AbsMovementNP.s = IPS_ALERT ;
	  IDSetNumber(&AbsMovementNP, "Value out of limits  %5.0f", new_apos);
	  return ;
	}
	
	if(( ret= updateRFPositionAbsolute(fd, &new_apos)) < 0) {

	  AbsMovementNP.s = IPS_IDLE;
	  IDSetNumber(&AbsMovementNP, "Read out of the absolute movement failed %3d, trying to recover position.", ret);

	  if((ret= updateRFPosition(fd, &currentPosition)) < 0) {
 
	    PositionNP.s = IPS_ALERT;
	    IDSetNumber(&PositionNP, "Unknown error while reading  Robofocus position: %d.", ret);

	    return ;
	  }

	  PositionNP.s = IPS_OK;
	  IDSetNumber(&PositionNP, "Robofocus position recovered %5.0f", currentPosition); 
	  IDMessage( mydev, "Robofocus position recovered resuming normal operation");
          /* We have to leave here, because new_apos is not set */
          return ;
	}

        currentAbsoluteMovement=  currentPosition ;
	AbsMovementNP.s = IPS_OK;
	IDSetNumber(&AbsMovementNP, NULL) ;

	PositionNP.s = IPS_OK;
        currentPosition= new_apos ;
	IDSetNumber(&PositionNP,  "Absolute position was  %5.0f", currentAbsoluteMovement);

      } else {
	
	AbsMovementNP.s = IPS_IDLE;
	IDSetNumber(&RelMovementNP, "Need exactly one parameter.");
 
	return ;
      }
    }
  }

  if (!strcmp (name, SetBacklashNP.name)) {

    double new_back = 0 ;
    int nset = 0;
    int ret= -1 ;

    if (PowerS[0].s != ISS_ON){
      SetBacklashNP.s = IPS_IDLE;
      IDSetNumber(&SetBacklashNP, "Power is off");
      return;
    }
    for (nset = i = 0; i < n; i++) {
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
	
	if(( ret= updateRFBacklash(fd, &new_back)) < 0) {

	  SetBacklashNP.s = IPS_IDLE;
	  IDSetNumber(&SetBacklashNP, "Setting new backlash failed.");
	 
	  return ;
	} 
	
        currentSetBacklash=  new_back ;
	SetBacklashNP.s = IPS_OK;
	IDSetNumber(&SetBacklashNP, "Backlash is now  %3.0f", currentSetBacklash) ;
      } else {
	
	SetBacklashNP.s = IPS_IDLE;
	IDSetNumber(&SetBacklashNP, "Need exactly one parameter.");

	return ;
      }
    }
  }
  
  if (!strcmp (name, MinMaxPositionNP.name)) {
    /* new positions */
    double new_min = 0 ;
    double new_max = 0 ;
  
	    
    /* Check power, if it is off, then return */
    if (PowerS[0].s != ISS_ON){
      MinMaxPositionNP.s = IPS_IDLE;
      IDSetNumber(&MinMaxPositionNP, "Power is off");
      return;
    }
	    
    for (nset = i = 0; i < n; i++){
      /* Find numbers with the passed names in the MinMaxPositionNP property */
      INumber *mmpp = IUFindNumber (&MinMaxPositionNP, names[i]);
      
      /* If the number found is  (MinMaxPositionN[0]) then process it */
      if (mmpp == &MinMaxPositionN[0]){
	      
	new_min = (values[i]);
	nset += new_min >= 1 && new_min <= 65000;
      } else if  (mmpp == &MinMaxPositionN[1]){

 	new_max = (values[i]);
	nset += new_max >= 1 && new_max <= 65000;
      }
    }
	    
    /* Did we process the two numbers? */
    if (nset == 2) {
		
      /* Set the robofocus state to BUSY */
      MinMaxPositionNP.s = IPS_BUSY;

      currentMinPosition = new_min ;
      currentMaxPosition= new_max ;
  

      MinMaxPositionNP.s = IPS_OK;
      IDSetNumber(&MinMaxPositionNP, "Minimum and Maximum settings are now  %3.0f %3.0f", currentMinPosition, currentMaxPosition);

    } else {
      /* Set property state to idle */
      MinMaxPositionNP.s = IPS_IDLE;
		
      IDSetNumber(&MinMaxPositionNP, "Minimum and maximum limits absent or bogus.");
      
      return ;
    }
  }

  if (!strcmp (name, MaxTravelNP.name)) {

    double new_maxt = 0 ;
    int ret = -1 ;

    /* Check power, if it is off, then return */
    if (PowerS[0].s != ISS_ON){
      MaxTravelNP.s = IPS_IDLE;
      IDSetNumber(&MaxTravelNP, "Power is off");
      return;
    }
    for (nset = i = 0; i < n; i++){
      /* Find numbers with the passed names in the MinMaxPositionNP property */
      INumber *mmpp = IUFindNumber (&MaxTravelNP, names[i]);
      
      /* If the number found is  (MaxTravelN[0]) then process it */
      if (mmpp == &MaxTravelN[0]){
	      
	new_maxt = (values[i]);
	nset += new_maxt >= 1 && new_maxt <= 64000;
      }
    }
    /* Did we process the one number? */
    if (nset == 1) {

      IDSetNumber(&MinMaxPositionNP, NULL);

      if(( ret= updateRFMaxPosition(fd,  &new_maxt))< 0 ) {
	MaxTravelNP.s = IPS_IDLE;
	IDSetNumber(&MaxTravelNP, "Changing to new maximum travel failed");
	return ;
      } ; 

      currentMaxTravel=  new_maxt ;
      MaxTravelNP.s = IPS_OK;
      IDSetNumber(&MaxTravelNP, "Maximum travel is now  %3.0f", currentMaxTravel) ;

    } else {
      /* Set property state to idle */

      MaxTravelNP.s = IPS_IDLE;
      IDSetNumber(&MaxTravelNP, "Maximum travel absent or bogus.");
      
      return ;
    }
  }

  if (!strcmp (name, SetRegisterPositionNP.name)) {

    double new_apos = 0 ;
    int nset = 0;
    int ret= -1 ;

    if (PowerS[0].s != ISS_ON){
      SetRegisterPositionNP.s = IPS_IDLE;
      IDSetNumber(&SetRegisterPositionNP, "Power is off");
      return;
    }
    for (nset = i = 0; i < n; i++)  {
      /* Find numbers with the passed names in the SetRegisterPositionNP property */
      INumber *srpp = IUFindNumber (&SetRegisterPositionNP, names[i]);
      
      /* If the number found is SetRegisterPosition (SetRegisterPositionN[0]) then process it */
      if (srpp == &SetRegisterPositionN[0]){
	
	new_apos = (values[i]);
	
        /* limits are absolute */
	nset += new_apos >= 0 && new_apos <= 64000;
      }

      if (nset == 1) {

	if((new_apos < currentMinPosition) || (new_apos > currentMaxPosition)) {

	  SetRegisterPositionNP.s = IPS_ALERT ;
	  IDSetNumber(&SetRegisterPositionNP, "Value out of limits  %5.0f", new_apos);
	  return ;
	}

	/* Set the robofocus state to BUSY */
	SetRegisterPositionNP.s = IPS_BUSY;
	IDSetNumber(&SetRegisterPositionNP, NULL);
	
	if(( ret= updateRFSetPosition(fd, &new_apos)) < 0) {

	  SetRegisterPositionNP.s = IPS_OK;
	  IDSetNumber(&SetRegisterPositionNP, "Read out of the set position to %3d failed. Trying to recover the position", ret);

	  if((ret= updateRFPosition(fd, &currentPosition)) < 0) {
 
	    PositionNP.s = IPS_ALERT;
	    IDSetNumber(&PositionNP, "Unknown error while reading  Robofocus position: %d", ret);

	    SetRegisterPositionNP.s = IPS_IDLE;
	    IDSetNumber(&SetRegisterPositionNP, "Relative movement failed.");
	  }

	  SetRegisterPositionNP.s = IPS_OK;
	  IDSetNumber(&SetRegisterPositionNP, NULL); 


	  PositionNP.s = IPS_OK;
	  IDSetNumber(&PositionNP, "Robofocus position recovered %5.0f", currentPosition); 
	  IDMessage( mydev, "Robofocus position recovered resuming normal operation");
          /* We have to leave here, because new_apos is not set */
          return ;
	}
	currentPosition= new_apos ;
	SetRegisterPositionNP.s = IPS_OK;
	IDSetNumber(&SetRegisterPositionNP, "Robofocus register set to %5.0f", currentPosition); 

	PositionNP.s = IPS_OK;
	IDSetNumber(&PositionNP, "Robofocus position is now %5.0f", currentPosition); 

	return ;

      } else {
	
	SetRegisterPositionNP.s = IPS_IDLE;
	IDSetNumber(&SetRegisterPositionNP, "Need exactly one parameter.");
 	
	return ;
      }
      if((ret= updateRFPosition(fd, &currentPosition)) < 0) {
 
	PositionNP.s = IPS_ALERT;
	IDSetNumber(&PositionNP, "Unknown error while reading  Robofocus position: %d", ret);

	return ;
      }
      SetRegisterPositionNP.s = IPS_OK;
      IDSetNumber(&SetRegisterPositionNP, "Robofocus has accepted new register setting" ) ; 

      PositionNP.s = IPS_OK;
      IDSetNumber(&PositionNP, "Robofocus new position %5.0f", currentPosition); 
    }
  }  
  return;
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
  INDI_UNUSED(root);
}

/* void connectRobofocus(void)
 * This function is called when the state of PowerSP is changed in the ISNewSwitch() function.
 * We check the state of CONNECT and DISCONNECT switches, and connect or disconnect our fake device accordingly */

void connectRobofocus(void) {

  /* fprintf(stderr, "connectRobofocus\n") ; */

  switch (PowerS[0].s) {
  case ISS_ON:
    
   if (tty_connect(PortT[0].text, 9600, 8, 0, 1, &fd) != TTY_OK)
   {

      PowerSP.s = IPS_ALERT;
      IUResetSwitch(&PowerSP);
      IDSetSwitch(&PowerSP, "Error connecting to port >%s<", PortT[0].text);
      return;
    }

    PowerSP.s = IPS_OK;
    IDSetSwitch(&PowerSP, "Robofocus is online.");
    wp= IEAddWorkProc( (IE_WPF *)ISWorkProc, NULL) ;
    break;

  case ISS_OFF:

    IERmWorkProc ( wp) ;
    tty_disconnect(fd);
    IUResetSwitch(&PowerSP);    
    IUResetSwitch(&PowerSwitchesSP);
    IUResetSwitch(&DirectionSP);
    AbsMovementNP.s= RelMovementNP.s= TimerNP.s= SpeedNP.s= SetBacklashNP.s= SetRegisterPositionNP.s= MinMaxPositionNP.s= DirectionSP.s= PowerSwitchesSP.s= SettingsNP.s= TemperatureNP.s= PositionNP.s = PortTP.s = PowerSP.s = IPS_IDLE;

    IDSetSwitch(&PowerSP, "Robofocus is offline.");

    IDSetText(&PortTP, NULL);
    
    IDSetSwitch(&DirectionSP, NULL);
    IDSetSwitch(&PowerSwitchesSP, NULL);

    /* Write the last status */

    IDSetNumber(&PositionNP, NULL);
    IDSetNumber(&AbsMovementNP, NULL);
    IDSetNumber(&RelMovementNP, NULL);
    IDSetNumber(&TimerNP, NULL);
    IDSetNumber(&SpeedNP, NULL);
    IDSetNumber(&SetBacklashNP, NULL);
    IDSetNumber(&SetRegisterPositionNP, NULL);
    IDSetNumber(&MinMaxPositionNP, NULL);
    IDSetNumber(&MaxTravelNP, NULL);
    IDSetNumber(&SettingsNP, NULL);
    IDSetNumber(&TemperatureNP, NULL);

    break;
  }
}

