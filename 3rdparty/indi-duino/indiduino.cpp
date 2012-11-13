#if 0
    Induino general propose driver. Allow using arduino boards
    as general I/O 	
    Copyright 2012 (c) Nacho Mas (mas.ignacio at gmail.com)


    Base on Tutorial Four
    Demonstration of libindi v0.7 capabilities.

    Copyright (C) 2010 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include <memory>
#include <sys/types.h>
#include <sys/stat.h>

#include <duconfig.h>

/* Our driver header */
#include "indiduino.h"

using namespace std;

/* Our indiduino auto pointer */
auto_ptr<indiduino> indiduino_prt(0);

const int POLLMS = 500;				// Period of update, 1 second.
static void ISPoll(void *);

/**************************************************************************************
** Send client definitions of all properties.
***************************************************************************************/
void ISInit()
{
 static int isInit=0;

 if (isInit)
  return;

 if (indiduino_prt.get() == 0)
 {
     isInit = 1;
     indiduino_prt.reset(new indiduino());
     IEAddTimer (POLLMS, ISPoll, NULL);
     srand (time(NULL));
 }
 
}

void ISPoll (void *p)
{
 INDI_UNUSED(p);
 indiduino_prt->ISPoll(); 
 IEAddTimer (POLLMS, ISPoll, NULL);
}

/**************************************************************************************
**
***************************************************************************************/
void ISGetProperties (const char *dev)
{
 ISInit(); 
 indiduino_prt->ISGetProperties(dev);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
 ISInit();
 indiduino_prt->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
 ISInit();
 indiduino_prt->ISNewText(dev, name, texts, names, n);

}

/**************************************************************************************
**
***************************************************************************************/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
 ISInit();
 indiduino_prt->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    ISInit();

    indiduino_prt->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);


}

/**************************************************************************************
**
***************************************************************************************/
void ISSnoopDevice (XMLEle *root) 
{
  INDI_UNUSED(root);
}

/**************************************************************************************
** LX200 Basic constructor
***************************************************************************************/
indiduino::indiduino()
{
    IDLog("Indiduino start...\n");

}

/**************************************************************************************
**
***************************************************************************************/
indiduino::~indiduino()
{

}

void indiduino::ISPoll() {
  if (is_connected()) {
    //IDLog("Polling...\n");
    sf->OnIdle();
    std::vector<INDI::Property *> *pAll = getProperties();
    for (int i=0; i < pAll->size(); i++) {
	const char *name;
	const char *label;
	INDI_TYPE type;
	name=pAll->at(i)->getName();
	label=pAll->at(i)->getLabel();
	type=pAll->at(i)->getType();
	//IDLog("PROP:%s\n",name);
	//DIGITAL INPUT
        if (type == INDI_LIGHT ) { 
		ILightVectorProperty *svp = getLight(name);
        	char s[10];
		strncpy(s,svp->name,7);
		//IDLog("DIGITAL PROP:%s\n",s);
		if (!strcmp(s, PROP_NAME_DINPUT)) {
			int pin;
		        s[0]=svp->name[8];
		        s[1]=svp->name[9];
		        s[2]='\0';	
			pin=atoi(s);
			//sf->askPinState(pin);
			//IDLog("DIGITAL PIN:%u.Value:%u\n",pin,sf->pin_info[pin].value);
			if (sf->pin_info[pin].mode == FIRMATA_MODE_INPUT) {
				if (sf->pin_info[pin].value==1) {
					//strcpy(svp->name,"ON");
					svp->s = IPS_OK;
				} else {
					//strcpy(svp->name,"");
					svp->s = IPS_IDLE;
				}
				IDSetLight(svp, NULL);
			}
        	 } 
        }
	//ANALOG
        if (type == INDI_NUMBER) {     
	        INumberVectorProperty *nvp = getNumber(name);
		//IDLog("%s\n",nvp->name);
	   for (int i=0;i<nvp->nnp;i++) {
	        INumber *eqp = &nvp->np[i];
        	if (!eqp)
	            return;
		//IDLog("%s\n",eqp->name);
        	char s[10];
		strncpy(s,eqp->name,7);	
		//IDLog("PROP:%s\n",s);
		if (!strcmp(s, PROP_NAME_AINPUT)) {
			int pin;
		        s[0]=eqp->name[8];
		        s[1]=eqp->name[9];
		        s[2]='\0';	
			pin=atoi(s);
			//sf->askPinState(pin);
			//IDLog("PIN:%u MODE:%u\n",pin,sf->pin_info[pin].mode);
			if (sf->pin_info[pin].mode == FIRMATA_MODE_ANALOG) {
				//IDLog("ANALOG PIN:%u.Value:%u\n",pin,sf->pin_info[pin].value);
        			eqp->value=sf->pin_info[pin].value;
				IDSetNumber(nvp, NULL);
			}
        	 } 
            } 
	 }
    }	
  }
}
/**************************************************************************************
** Initialize all properties & set default values.
**************************************************************************************/
bool indiduino::initProperties()
{
    // This is the default driver skeleton file location
    // Convention is: drivername_sk_xml
    // Default location is /usr/share/indi  
    const char *skelFileName = DEFAULT_SKELETON_FILE;
    struct stat st;


    setVersion(VERSION_MAJOR, VERSION_MINOR);

    char *skel = getenv("INDISKEL");
    if (skel) {
        buildSkeleton(skel);
    IDLog("Building from %s skeleton\n",skel);
    } else if (stat(skelFileName,&st) == 0) {
        buildSkeleton(skelFileName);
    IDLog("Building from %s skeleton\n",skelFileName);
    } else {
        IDLog("No skeleton file was specified. Set environment variable INDISKEL to the skeleton path and try again.\n");
    } 
    // Optional: Add aux controls for configuration, debug & simulation that get added in the Options tab
    //           of the driver.
    //addAuxControls();

    DefaultDevice::initProperties();

    return true;

}

/**************************************************************************************
** Define Basic properties to clients.
***************************************************************************************/
void indiduino::ISGetProperties(const char *dev)
{
    static int configLoaded = 0;

    // Ask the default driver first to send properties.
    INDI::DefaultDevice::ISGetProperties(dev);

    configLoaded = 1;
}

/**************************************************************************************
** Process Text properties
***************************************************************************************/
bool indiduino::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// Ignore if not ours 
        if (strcmp (dev, getDeviceName()))
            return false;
    ITextVectorProperty * tProp = getText(name);
    // Device Port Text
    if (!strcmp(tProp->name, "DEVICE_PORT"))
    {
        if (IUUpdateText(tProp, texts, names, n) < 0)
                        return false;

                tProp->s = IPS_OK;
		tProp->s = IPS_IDLE;
                IDSetText(tProp, "Port updated.");

                return true;
    }
        return false;
}

/**************************************************************************************
**
***************************************************************************************/
bool indiduino::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	
	// Ignore if not ours
        if (strcmp (dev, getDeviceName()))
            return false;

        INumberVectorProperty *nvp = getNumber(name);
	IDLog("%s\n",nvp->name);

        if (!nvp)
            return false;

        if (isConnected() == false)
        {
            nvp->s = IPS_ALERT;
            IDSetNumber(nvp, "Cannot change property while device is disconnected.");
            return false;
        }
	bool change=false;
        for (int i=0;i<n;i++) {
        INumber *eqp = IUFindNumber(nvp, names[i]);
        //IDLog("%s\n",eqp->name);
	
	if (!eqp)
            return false;
        
        char s[10];
	strncpy(s,eqp->name,7);	
        //IDLog("%s\n",s);
	if (!strcmp(s, PROP_NAME_AOUTPUT)) {      
	    int pin;
	    s[0]=eqp->name[8];
	    s[1]=eqp->name[9];
	    s[2]='\0';	
	    pin=atoi(s);
            IUUpdateNumber(nvp, values, names, n);
	    IDLog("Setting output %s:%s on pin %u to %f\n",nvp->label,eqp->label,pin,eqp->value);
            IDSetNumber (nvp, "Setting output %s:%s to %f\n",nvp->label,eqp->label,eqp->value);
   	    sf->setPwmPin(pin,(int)eqp->value);
            nvp->s = IPS_IDLE;
            IDSetNumber(nvp, NULL);
	    change=true;
        }
	if (!strcmp(s, PROP_NAME_AINPUT)) {      
	    int pin;
	    s[0]=eqp->name[8];
	    s[1]=eqp->name[9];
	    s[2]='\0';	
	    pin=atoi(s);
            IUUpdateNumber(nvp, values, names, n);
	    //IDLog("%s:%f\n",eqp->name,eqp->value);  	    
            nvp->s = IPS_IDLE;
            IDSetNumber(nvp, NULL);
	    change=true;
        }
        }

	if (change) {
          	return true;
	} else {
        	return false;
	}
}

/**************************************************************************************
**
***************************************************************************************/
bool indiduino::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int lightState=0;
    int lightIndex=0;

	// ignore if not ours //
            if (strcmp (dev, getDeviceName()))
            return false;

        if (INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n) == true)
            return true;



        ISwitchVectorProperty *svp = getSwitch(name);
	IDLog("NAME:%s\n",name);
        IDLog("%s\n",svp->name);

        if (isConnected() == false)
        {
            svp->s = IPS_ALERT;
            IDSetSwitch(svp, "Cannot change property while device is disconnected.");
            return false;
        }


        if (!svp )
            return false;


        char s[10];
	strncpy(s,svp->name,7);	
        //IDLog("%s\n",s);
	if (!strcmp(s, PROP_NAME_DOUTPUT)) {
		int pin;
	        s[0]=svp->name[8];
	        s[1]=svp->name[9];
	        s[2]='\0';	
		pin=atoi(s);		
                //IDLog("%s\n",s);
		IUUpdateSwitch(svp, states, names, n);
        	ISwitch *onSW = IUFindOnSwitch(svp);  
		if (!strcmp(onSW->name, "ON")) {
			IDLog("Switching ON %s on pin %u\n",svp->label,pin);
        		sf->writeDigitalPin(pin,ARDUINO_HIGH);
			svp->s = IPS_OK;
			IDSetSwitch (svp, "%s:ON\n",svp->label);
		} else {
			IDLog("Switching OFF %s on pin %u\n",svp->label,pin);
			sf->writeDigitalPin(pin,ARDUINO_LOW);
			svp->s = IPS_IDLE;
			IDSetSwitch (svp, "%s:OFF\n",svp->label);
		}

        	//IUUpdateSwitch(svp, states, names, n);
	        return true;
	}


    return false;


}

bool indiduino::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    const char *testBLOB = "This is a test BLOB from the driver";

    IBLOBVectorProperty *bvp = getBLOB(name);

    if (!bvp)
        return false;

    if (isConnected() == false)
    {
        bvp->s = IPS_ALERT;
        IDSetBLOB(bvp, "Cannot change property while device is disconnected.");
        return false;
    }

    if (!strcmp(bvp->name, "BLOB Test"))
    {

        IUUpdateBLOB(bvp, sizes, blobsizes, blobs, formats, names, n);

        IBLOB *bp = IUFindBLOB(bvp, names[0]);

        if (!bp)
            return false;

        IDLog("Recieved BLOB with name %s, format %s, and size %d, and bloblen %d\n", bp->name, bp->format, bp->size, bp->bloblen);

        char *blobBuffer = new char[bp->bloblen+1];
        strncpy(blobBuffer, ((char *) bp->blob), bp->bloblen);
        blobBuffer[bp->bloblen] = '\0';

        IDLog("BLOB Content:\n##################################\n%s\n##################################\n", blobBuffer);

        delete [] blobBuffer;
    }

    return true;

}


/**************************************************************************************
**
***************************************************************************************/
bool indiduino::Connect()
{
    ITextVectorProperty *tProp = getText("DEVICE_PORT");
    sf = new Firmata(tProp->tp[0].text);
    if (sf->portOpen) {
    	IDLog("ARDUINO BOARD CONNECTED.\n");
	IDLog("FIRMATA VERSION:%s\n",sf->firmata_name);
	IDSetSwitch (getSwitch("CONNECTION"),"CONNECTED.FIRMATA VERSION:%s\n",sf->firmata_name);
	setPinModesFromSK();
	return true;
    } else {
	IDLog("ARDUINO BOARD FAIL TO CONNECT.\n");
	IDSetSwitch (getSwitch("CONNECTION"),"FAIL TO CONNECT\n");
	delete sf;
	return false;
    }
    
}

bool indiduino::is_connected(void) {
	ISwitchVectorProperty *svp = getSwitch("CONNECTION");
        return (svp->s == IPS_OK);
}

void indiduino::setPinModesFromSK() {

    IDLog("Setting pins behaviour from properties names\n");
    std::vector<INDI::Property *> *pAll = getProperties();
    for (int i=0; i < pAll->size(); i++) {
	const char *name;
	const char *label;
	INDI_TYPE type;
	name=pAll->at(i)->getName();
	label=pAll->at(i)->getLabel();
	type=pAll->at(i)->getType();
        //IDLog("Property #%d: %s %s %u\n", i, name,label,type);
        if (type == INDI_SWITCH) { 
		ISwitchVectorProperty *svp = getSwitch(name);
		//IDLog("%s\n",svp->name);
        	char s[10];
		strncpy(s,svp->name,7);	
        
		if (!strcmp(s, PROP_NAME_DOUTPUT)) {
			int pin;
		        s[0]=svp->name[8];
		        s[1]=svp->name[9];
		        s[2]='\0';	
			pin=atoi(s);
			IDLog("%s on pin %u set as DIGITAL OUTPUT\n",svp->label,pin);
			sf->setPinMode(pin,FIRMATA_MODE_OUTPUT);
         	} 
        }
	if (type == INDI_LIGHT) { 
		//IDLog("INDI_LIGHT\n");
		ILightVectorProperty *svp = getLight(name);
        	char s[10];
		strncpy(s,svp->name,7);	
        
		if (!strcmp(s, PROP_NAME_DINPUT)) {
			int pin;
		        s[0]=svp->name[8];
		        s[1]=svp->name[9];
		        s[2]='\0';	
			pin=atoi(s);
			IDLog("%s on pin %u set as DIGITAL INPUT\n",svp->label,pin);
			sf->setPinMode(pin,FIRMATA_MODE_INPUT);
        	 } 
        }
        if (type == INDI_NUMBER) {     
	        INumberVectorProperty *nvp = getNumber(name);
		//IDLog("%s\n",nvp->name);

	   for (int i=0;i<nvp->nnp;i++) {
	        INumber *eqp = &nvp->np[i];
        	//IDLog("%s\n",eqp->name);
		if (!eqp)
	            return;
        	char s[10];
		strncpy(s,eqp->name,7);	
		if (!strcmp(s, PROP_NAME_AOUTPUT)) {
			int pin;
		        s[0]=eqp->name[8];
		        s[1]=eqp->name[9];
		        s[2]='\0';	
			pin=atoi(s);
			IDLog("%s on pin %u set as ANALOG OUTPUT\n",eqp->label,pin);
			sf->setPinMode(pin,FIRMATA_MODE_PWM);
        	 } 
		if (!strcmp(s, PROP_NAME_AINPUT)) {
			int pin;
		        s[0]=eqp->name[8];
		        s[1]=eqp->name[9];
		        s[2]='\0';	
			pin=atoi(s);
			IDLog("%s on pin %u set as ANALOG INPUT\n",eqp->label,pin);
			sf->setPinMode(pin,FIRMATA_MODE_ANALOG);
        	 } 
            } 
	 }
    }
    sf->setSamplingInterval(POLLMS/2); 
    sf->reportAnalogPorts(1);
    sf->reportDigitalPorts(1);
}

bool indiduino::Disconnect()
{
    sf->closePort();
    delete sf;	
    IDLog("ARDUINO BOARD DISCONNECTED.\n");
    IDSetSwitch (getSwitch("CONNECTION"),"DISCONNECTED\n");
    return true;
}



const char * indiduino::getDefaultName()
{
    return "Arduino";
}






