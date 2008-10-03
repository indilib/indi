#if 0
    FLI Precision Digital Focuer 
    Copyright (C) 2005 Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "libfli.h"
#include "indidevapi.h"
#include "eventloop.h"
#include "indicom.h"

void ISInit(void);
void getBasicData(void);
void ISPoll(void *);
void handleExposure(void *);
void connectPDF(void);
int  findPDF(flidomain_t domain);
int  manageDefaults(char errmsg[]);
int  checkPowerS(ISwitchVectorProperty *sp);
int  checkPowerN(INumberVectorProperty *np);
int  checkPowerT(ITextVectorProperty *tp);
int  getOnSwitch(ISwitchVectorProperty *sp);
int  isPDFConnected(void);

double min(void);
double max(void);

extern char* me;
extern int errno;

#define mydev           "FLI PDF"
#define MAIN_GROUP	"Main Control"
#define currentPosition	 FocuserN[0].value
#define POLLMS		1000

typedef struct {
  flidomain_t domain;
  char *dname;
  char *name;
  char *model;
  long HWRevision;
  long FWRevision;
  long current_pos;
  long home;
} pdf_t;


static flidev_t fli_dev;
static pdf_t *FLIPDF;
static int portSwitchIndex;
static int simulation;
static long targetPosition;

long int Domains[] = { FLIDOMAIN_USB, FLIDOMAIN_SERIAL, FLIDOMAIN_PARALLEL_PORT,  FLIDOMAIN_INET };

/*INDI controls */

/* Connect/Disconnect */
static ISwitch PowerS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty PowerSP	= { mydev, "CONNECTION" , "Connection", MAIN_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, PowerS, NARRAY(PowerS), "", 0};

/* Types of Ports */
static ISwitch PortS[]           	= {{"USB", "", ISS_ON, 0, 0}, {"Serial", "", ISS_OFF, 0, 0}, {"Parallel", "", ISS_OFF, 0, 0}, {"INet", "", ISS_OFF, 0, 0}};
static ISwitchVectorProperty PortSP	= { mydev, "Port Type", "", MAIN_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, PortS, NARRAY(PortS), "", 0};
 
/* Focuser control */
static INumber FocuserN[]	  = { {"Position", "", "%2.0f", -10000., 10000., 1., 0, 0, 0, 0}};
static INumberVectorProperty FocuserNP = { mydev, "Focuser", "", MAIN_GROUP, IP_RW, 0, IPS_IDLE, FocuserN, NARRAY(FocuserN), "", 0};

/* Focuser home */
static ISwitch HomeS[]			= { {"Home", "", ISS_OFF, 0, 0} };
static ISwitchVectorProperty HomeSP = { mydev, "Home", "", MAIN_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, HomeS, NARRAY(HomeS), "", 0};

/* send client definitions of all properties */
void ISInit()
{
	static int isInit=0;
	
	if (isInit)
		return;
	
	/* USB by default {USB, SERIAL, PARALLEL, INET} */
	portSwitchIndex = 0;

	targetPosition = 0;

        /* No Simulation by default */
	simulation = 0;

        /* Enable the following for simulation mode */
        /*simulation = 1;
        IDLog("WARNING: Simulation is on\n");*/
        
        IEAddTimer (POLLMS, ISPoll, NULL);
	
	isInit = 1; 
}

void ISGetProperties (const char *dev)
{ 

	ISInit();
	
	if (dev && strcmp (mydev, dev))
		return;
	
	/* Main Control */
	IDDefSwitch(&PowerSP, NULL);
	IDDefSwitch(&PortSP, NULL);
	IDDefSwitch(&HomeSP, NULL);
	IDDefNumber(&FocuserNP, NULL);
	
	
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

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	long err=0;

	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
		return;
	    
	ISInit();
	    
	/* Port type */
	if (!strcmp (name, PortSP.name))
	{
		PortSP.s = IPS_IDLE; 
		IUResetSwitch(&PortSP);
		IUUpdateSwitch(&PortSP, states, names, n);
		portSwitchIndex = getOnSwitch(&PortSP);
		
		PortSP.s = IPS_OK; 
		IDSetSwitch(&PortSP, NULL);
		return;
	}
	
	/* Connection */
	if (!strcmp (name, PowerSP.name))
	{
		IUResetSwitch(&PowerSP);
		IUUpdateSwitch(&PowerSP, states, names, n);
		connectPDF();
		return;
	}

	if (!strcmp(name, HomeSP.name))
	{
		if (!isPDFConnected()) 
		{
			IDMessage(mydev, "Device not connected.");
			HomeSP.s = IPS_IDLE;
			IDSetSwitch(&HomeSP, NULL);
			return;
		}

		
		if ( (err = FLIHomeFocuser(fli_dev)))
		{
			HomeSP.s = IPS_ALERT;
			IDSetSwitch(&HomeSP, "FLIHomeFocuser() failed. %s.", strerror((int)-err));
			IDLog("FLIHomeFocuser() failed. %s.\n", strerror((int)-err));
			return;
		}

		HomeSP.s = IPS_OK;
		IDSetSwitch(&HomeSP, "Focuser at home position.");
		IDLog("Focuser at home position.\n");
		return;
	}
	
     
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	ISInit();
	
	/* ignore if not ours */ 
	if (dev && strcmp (mydev, dev))
		return;

	/* suppress warning */
	n=n; dev=dev; name=name; names=names; texts=texts;
}


void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	long err;
	long newPos;
	names=names;
	n = n;
	
	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
	    return;
	    
	ISInit();
	
	
	if (!strcmp(FocuserNP.name, name)) {
		if (simulation) 
		{
			targetPosition = values[0];
			FocuserNP.s = IPS_BUSY;
			IDSetNumber(&FocuserNP, "Setting focuser position to %ld", targetPosition);
			IDLog("Setting focuser position to %ld", targetPosition);
			return;
 		}


		if (!isPDFConnected()) 
		{
			IDMessage(mydev, "Device not connected.");
			FocuserNP.s = IPS_IDLE;
			IDSetNumber(&FocuserNP, NULL);
			return;
		}
		
		targetPosition = values[0];
		
		FocuserNP.s = IPS_BUSY;
		IDSetNumber(&FocuserNP, "Setting focuser position to %ld", targetPosition);
		IDLog("Setting focuser position to %ld\n", targetPosition);
		
		if ( (err = FLIStepMotor(fli_dev, targetPosition)))
		{
			FocuserNP.s = IPS_ALERT;
			IDSetNumber(&FocuserNP, "FLIStepMotor() failed. %s.", strerror((int)-err));
			IDLog("FLIStepMotor() failed. %s.", strerror((int)-err));
			return;
		}
		
		/* Check current focuser position */
		if (( err = FLIGetStepperPosition(fli_dev, &newPos))) 
		{
			FocuserNP.s = IPS_ALERT;
			IDSetNumber(&FocuserNP, "FLIGetStepperPosition() failed. %s.", strerror((int)-err));
			IDLog("FLIGetStepperPosition() failed. %s.\n", strerror((int)-err));
			return;
		}
		
		if (newPos == targetPosition) 
		{
			FLIPDF->current_pos = targetPosition;
			currentPosition = FLIPDF->current_pos;
			FocuserNP.s = IPS_OK;
			IDSetNumber(&FocuserNP, "Focuser position %ld", targetPosition);
			return;
		}

		return;
	}
}


/* Retrieves basic data from the focuser upon connection like temperature, array size, firmware..etc */
void getBasicData()
{

	char buff[2048];
	long err;
	
	if ((err = FLIGetModel (fli_dev, buff, 2048)))
	{
		IDMessage(mydev, "FLIGetModel() failed. %s.", strerror((int)-err));
		IDLog("FLIGetModel() failed. %s.\n", strerror((int)-err));
		return;
	}
	else
	{
		if ( (FLIPDF->model = malloc (sizeof(char) * 2048)) == NULL)
		{
		IDMessage(mydev, "malloc() failed.");
		IDLog("malloc() failed.");
		return;
		}
		
		strcpy(FLIPDF->model, buff);
	}
  
	if (( err = FLIGetHWRevision(fli_dev, &FLIPDF->HWRevision)))
	{
		IDMessage(mydev, "FLIGetHWRevision() failed. %s.", strerror((int)-err));
		IDLog("FLIGetHWRevision() failed. %s.\n", strerror((int)-err));
		
		return;
	}
	
	if (( err = FLIGetFWRevision(fli_dev, &FLIPDF->FWRevision)))
	{
		IDMessage(mydev, "FLIGetFWRevision() failed. %s.", strerror((int)-err));
		IDLog("FLIGetFWRevision() failed. %s.\n", strerror((int)-err));
		return;
	}
	
	if (( err = FLIGetStepperPosition(fli_dev, &FLIPDF->current_pos)))
	{
		IDSetNumber(&FocuserNP, "FLIGetStepperPosition() failed. %s.", strerror((int)-err));
		IDLog("FLIGetStepperPosition() failed. %s.\n", strerror((int)-err));
		return;
	}
  
	currentPosition = FLIPDF->current_pos;

	IDLog("Model: %s\n", FLIPDF->model);
	IDLog("HW Revision %ld\n", FLIPDF->HWRevision);
	IDLog("FW Revision %ld\n", FLIPDF->FWRevision);
	IDLog("Initial focuser position %ld\n", FLIPDF->current_pos);
	FocuserNP.s = IPS_OK;
	IDSetNumber(&FocuserNP, NULL);
	
	IDLog("Exiting getBasicData()\n");
  
}

void ISPoll(void *p)
{
  static int simMTC = 5;

  p=p;
  if (!isPDFConnected())
  {
      IEAddTimer (POLLMS, ISPoll, NULL);
      return;
  }


  switch (FocuserNP.s)
  {
    case IPS_IDLE:
    case IPS_OK:
       break;
  
   
   case IPS_BUSY:
    /* Simulate that it takes 5 seconds to change positoin*/
    if (simulation)
    {
        simMTC--;
        if (simMTC == 0)
        {
           simMTC = 5;
           currentPosition = targetPosition;
           FocuserNP.s = IPS_OK;
           IDSetNumber(&FocuserNP, "Focuser position %ld", targetPosition);
           break;
        }
        IDSetNumber(&FocuserNP, NULL);
        break;
    }


    /*if (( err = FLIGetFilterPos(fli_dev, &currentFilter)))
	{
                FocuserNP.s = IPS_ALERT;
		IDSetNumber(&FocuserNP, "FLIGetFilterPos() failed. %s.", strerror((int)-err));
		IDLog("FLIGetFilterPos() failed. %s.\n", strerror((int)-err));
		return;
	}

        if (targetPosition == currentFilter)
        {
		FLIPDF->current_filter = currentFilter;
		FocuserNP.s = IPS_OK;
		IDSetNumber(&FocuserNP, "Filter set to slot #%2.0f", currentFilter);
                return;
        }
       
        IDSetNumber(&FocuserNP, NULL);*/
	break;

   case IPS_ALERT:
    break;
 }

   IEAddTimer (POLLMS, ISPoll, NULL);

}



int getOnSwitch(ISwitchVectorProperty *sp)
{
  int i=0;
 for (i=0; i < sp->nsp ; i++)
 {
   /*IDLog("Switch %s is %s\n", sp->sp[i].name, sp->sp[i].s == ISS_ON ? "On" : "Off");*/
     if (sp->sp[i].s == ISS_ON)
      return i;
 }

 return -1;
}

int checkPowerS(ISwitchVectorProperty *sp)
{

  if (simulation)
   return 0;

  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(sp->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the focuser is offline.", sp->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the focuser is offline.", sp->label);
	
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int checkPowerN(INumberVectorProperty *np)
{
  if (simulation)
   return 0;

  if (PowerSP.s != IPS_OK)
  {
     if (!strcmp(np->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the focuser is offline.", np->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the focuser is offline.", np->label);
    
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;
}

int checkPowerT(ITextVectorProperty *tp)
{
   if (simulation)
    return 0;

  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(tp->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the focuser is offline.", tp->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the focuser is offline.", tp->label);
	
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void connectPDF()
{
	long err;
	/* USB by default {USB, SERIAL, PARALLEL, INET} */
	switch (PowerS[0].s)
	{
		case ISS_ON:
			
			if (simulation)
			{
	                        /* Success! */
				PowerS[0].s = ISS_ON;
				PowerS[1].s = ISS_OFF;
				PowerSP.s = IPS_OK;
				IDSetSwitch(&PowerSP, "Simulation PDF is online.");
				IDLog("Simulation PDF is online.\n");
				return;
			}

			IDLog("Current portSwitch is %d\n", portSwitchIndex);
			IDLog("Attempting to find the device in domain %ld\n", Domains[portSwitchIndex]);

			if (findPDF(Domains[portSwitchIndex]))
			{
				PowerSP.s = IPS_IDLE;
				PowerS[0].s = ISS_OFF;
				PowerS[1].s = ISS_ON;
				IDSetSwitch(&PowerSP, "Error: no focusers were detected.");
				IDLog("Error: no focusers were detected.\n");
				return;
			}
			
			if ((err = FLIOpen(&fli_dev, FLIPDF->name, FLIPDF->domain | FLIDEVICE_FOCUSER)))
			{
				PowerSP.s = IPS_IDLE;
				PowerS[0].s = ISS_OFF;
				PowerS[1].s = ISS_ON;
				IDSetSwitch(&PowerSP, "Error: FLIOpen() failed. %s.", strerror( (int) -err));
				IDLog("Error: FLIOpen() failed. %s.\n", strerror( (int) -err));
				return;
			}

			/* Success! */
			PowerS[0].s = ISS_ON;
			PowerS[1].s = ISS_OFF;
			PowerSP.s = IPS_OK;
			IDSetSwitch(&PowerSP, "Focuser is online. Retrieving basic data.");
			IDLog("Focuser is online. Retrieving basic data.\n");
			getBasicData();

			break;
		
		case ISS_OFF:

			if (simulation)
			{
				PowerS[0].s = ISS_OFF;
				PowerS[1].s = ISS_ON;
				PowerSP.s = IPS_IDLE;
				IDSetSwitch(&PowerSP, "Focuser is offline.");
				return;
			}

			PowerS[0].s = ISS_OFF;
			PowerS[1].s = ISS_ON;
			PowerSP.s = IPS_IDLE;
			if ((err = FLIClose(fli_dev))) 
			{
				PowerSP.s = IPS_ALERT;
				IDSetSwitch(&PowerSP, "Error: FLIClose() failed. %s.", strerror( (int) -err));
				IDLog("Error: FLIClose() failed. %s.\n", strerror( (int) -err));
				return;
			}
			IDSetSwitch(&PowerSP, "Focuser is offline.");
			break;
	}
}

/* isPDFConnected: return 1 if we have a connection, 0 otherwise */
int isPDFConnected(void)
{
   if (simulation)
     return 1;

   return ((PowerS[0].s == ISS_ON) ? 1 : 0);
}

int findPDF(flidomain_t domain)
{
	char **devlist;
	long err;
	
	IDLog("In findPDF, the domain is %ld\n", domain);
	
	if (( err = FLIList(domain | FLIDEVICE_FOCUSER, &devlist)))
	{
		IDLog("FLIList() failed. %s\n", strerror((int)-err));
		return -1;
	}
	
	if (devlist != NULL && devlist[0] != NULL)
	{
		int i;

		IDLog("Trying to allocate memory to FLIPDF\n");
		if ((FLIPDF = malloc (sizeof (pdf_t))) == NULL)
		{
			IDLog("malloc() failed.\n");
			return -1;
		}
		
		for (i = 0; devlist[i] != NULL; i++)
		{
			int j;
	
			for (j = 0; devlist[i][j] != '\0'; j++)
				if (devlist[i][j] == ';')
				{
					devlist[i][j] = '\0';
					break;
				}
		}

		FLIPDF->domain = domain;
		
		/* Each driver handles _only_ one camera for now */
		switch (domain)
		{
			case FLIDOMAIN_PARALLEL_PORT:
				FLIPDF->dname = strdup("parallel port");
				break;
	
			case FLIDOMAIN_USB:
				FLIPDF->dname = strdup("USB");
				break;
	
			case FLIDOMAIN_SERIAL:
				FLIPDF->dname = strdup("serial");
				break;
	
			case FLIDOMAIN_INET:
				FLIPDF->dname = strdup("inet");
				break;
	
			default:
				FLIPDF->dname = strdup("Unknown domain");
		}
		
		IDLog("Domain set OK\n");
		
		FLIPDF->name = strdup(devlist[0]);
				
		if ((err = FLIFreeList(devlist)))
		{
			IDLog("FLIFreeList() failed. %s.\n", strerror((int)-err));
			return -1;
		}
      
	} /* end if */
	else
	{
		if ((err = FLIFreeList(devlist)))
		{
			IDLog("FLIFreeList() failed. %s.\n", strerror((int)-err));
			return -1;
		}
		
		return -1;
	}

	IDLog("FindPDF() finished successfully.\n");
	return 0;
}
