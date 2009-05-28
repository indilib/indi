/*
   INDI Developers Manual
   Tutorial #1
   
   "Hello INDI"
   
   We construct a most basic (and useless) device driver to illustate INDI.
   
   Refer to README, which contains instruction on how to build this driver, and use it 
   with an INDI-compatible client.

*/

/** \file tutorial_one.c
    \brief Construct a basic INDI driver with only one property.
    \author Jasem Mutlaq
*/

/* Standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

/* INDI Core headers */

/* indidevapi.h contains API declerations */
#include "indidevapi.h"

/* INDI Eventloop mechanism */
#include "eventloop.h"

/* INDI Common Routines */
#include "indicom.h"

/* Definitions */

#define mydev		"Simple Device"				/* Device name */
#define MAIN_GROUP	"Main Control"				/* Group name */

/* Function protptypes */
void connectDevice(void);

/*INDI controls */

/* We will define only one vector switch property. The CONNECTION property is an INDI Standard Property and should be implemented in all INDI drivers.
 * A vector property may have one or more members. */

/* First, we define and initilize the members of the vector property */
static ISwitch PowerS[]          	= {
							  {"CONNECT"			/* 1st Swtich name */
							  , "Connect" 			/* Switch Label, i.e. what the GUI displays */
							  , ISS_OFF 			/* State of the switch, initially off */
						          , 0					/* auxiluary, set to 0 for now.*/
						          , 0}					/* auxiluary, set to 0 for now */
							 
						         ,{"DISCONNECT"		/* 2nd Swtich name */
						         , "Disconnect"			/* Switch Label, i.e. what the GUI displays */
						         , ISS_ON				/* State of the switch, initially on */
						         , 0					/* auxiluary, set to 0 for now.*/
						         , 0}					/* auxiluary, set to 0 for now */
						     };
						     
/* Next, we define and initlize the vector switch property that contains the two switches we defined above */
static ISwitchVectorProperty PowerSP    = { 
								mydev			/* Device name */
								, "CONNECTION"  	/* Property name */
								, "Connection"		/* Property label */
								, MAIN_GROUP	/* Property group */
								, IP_RW			/* Property premission, it's both read and write */
								, ISR_1OFMANY	/* Switch behavior. Only 1 of many switches are allowed to be on at the same time */
								, 0				/* Timeout, 0 seconds */
								, IPS_IDLE		/* Initial state is idle */
								, PowerS			/* Switches comprimising this vector that we defined above */
								, NARRAY(PowerS)	/* Number of Switches. NARRAY is defined in indiapi.h */
								, ""				/* Timestamp, set to 0 */
								, 0};				/* auxiluary, set to 0 for now */


/* void ISGetProperties (const char *dev)
*  INDI will call this function when the client inquires about the device properties.
*  Here we will use IDxxx functions to define new properties to the client */
void ISGetProperties (const char *dev)
{ 
  /* Tell the client to create a new Switch property PowerSP */
  IDDefSwitch(&PowerSP, NULL); 
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
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
      /* Let's check if the property the client wants to change is the PowerSP (name: CONNECTION) property*/
     if (!strcmp (name, PowerSP.name))
     {
          /* If the clients wants to update this property, let's perform the following */
	  
	  /* A. We update the switches by sending their names and updated states IUUpdateSwitches function. If there is an error, we return */
	  if (IUUpdateSwitch(&PowerSP, states, names, n) < 0) return;
	  
	  /* B. We try to establish a connection to our device */
   	  connectDevice();
	  return;
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
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
        /* Even though we didn't define any text members, we need to define this function. Otherwise, the driver will not compile */
	/* Since there is nothing to do, we simply return */
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
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
         /* Even though we didn't define any number members, we need to define this function. Otherwise, the driver will not compile */
	/* Since there is nothing to do, we simply return */
        return;
}

/* Note that we must define ISNewBLOB and ISSnoopDevice even if we don't use them, otherwise, the driver will NOT compile */
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {}
void ISSnoopDevice (XMLEle *root) {}


/* void connectDevice(void)
 * This function is called when the state of PowerSP is changed in the ISNewSwitch() function.
 * We check the state of CONNECT and DISCONNECT switches, and connect or disconnect our fake device accordingly */
void connectDevice(void)
{

  /* Now we check the state of CONNECT, the 1st member of the PowerSP property we defined earliar */
  switch (PowerS[0].s)
  {
     /* If CONNECT is on, then try to establish a connection to the device */
     case ISS_ON:
      
      /* The IDLog function is a very useful function that will log time-stamped messages to stderr. This function is invaluable to debug your drivers.
       * It operates like printf */
     IDLog ("Establishing a connection to %s...\n", mydev);

      /* Change the state of the PowerSP (CONNECTION) property to OK */
      PowerSP.s = IPS_OK;
      
      /* Tell the client to update the states of the PowerSP property, and send a message to inform successful connection */
      IDSetSwitch(&PowerSP, "Connection to %s is successful.", mydev);
      break;
      
    /* If CONNECT is off (which is the same thing as DISCONNECT being on), then try to disconnect the device */
    case ISS_OFF:
    
       IDLog ("Terminating connection to %s...\n", mydev);
       
      /* The device is disconnected, change the state to IDLE */
      PowerSP.s = IPS_IDLE;
      
      /* Tell the client to update the states of the PowerSP property, and send a message to inform successful disconnection */
      IDSetSwitch(&PowerSP, "%s has been disconneced.", mydev);
      
      break;
     }
     
}

/* That's it! check out tutorial_two where we simulate a simple telescope! */
