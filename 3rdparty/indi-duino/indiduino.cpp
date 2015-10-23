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
unique_ptr<indiduino> indiduino_prt(new indiduino());

const int POLLMS = 500;				// Period of update, 1 second.

/**************************************************************************************
** Send client definitions of all properties.
***************************************************************************************/
/**************************************************************************************
**
***************************************************************************************/
void ISGetProperties (const char *dev)
{
 indiduino_prt->ISGetProperties(dev);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
 indiduino_prt->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
 indiduino_prt->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
 indiduino_prt->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
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
**
***************************************************************************************/
indiduino::indiduino()
{
    IDLog("Indiduino driver start...\n");
}

/**************************************************************************************
**
***************************************************************************************/
indiduino::~indiduino()
{

}

void indiduino::TimerHit()
{
    if (isConnected() == false)
        return;

    sf->OnIdle();

    std::vector<INDI::Property *> *pAll = getProperties();
    for (int i=0; i < pAll->size(); i++) {
	const char *name;
	const char *label;
    INDI_PROPERTY_TYPE type;
	name=pAll->at(i)->getName();
	label=pAll->at(i)->getLabel();
	type=pAll->at(i)->getType();
	//IDLog("PROP:%s\n",name);
	//DIGITAL INPUT
        if (type == INDI_LIGHT )
        {
		ILightVectorProperty *lvp = getLight(name);
        for (int i=0;i<lvp->nlp;i++)
        {
	        	ILight *lqp = &lvp->lp[i];
        		if (!lqp)
	        	    return;

			IO *pin_config=(IO*)lqp->aux;
			if (pin_config == NULL) continue;
				if (pin_config->IOType==DI) {
					int pin=pin_config->pin;
					if (sf->pin_info[pin].mode == FIRMATA_MODE_INPUT) {
						if (sf->pin_info[pin].value==1) {
							//IDLog("%s.%s on pin %u change to  ON\n",lvp->name,lqp->name,pin);
							//IDSetLight (lvp, "%s.%s change to ON\n",lvp->name,lqp->name);
							lqp->s = IPS_OK;
						} else {
							//IDLog("%s.%s on pin %u change to  OFF\n",lvp->name,lqp->name,pin);
							//IDSetLight (lvp, "%s.%s change to OFF\n",lvp->name,lqp->name);
							lqp->s = IPS_IDLE;
						}
					}
	        	 	} 
		}
		IDSetLight(lvp, NULL);
        }

	//ANALOG
        if (type == INDI_NUMBER) {     
	        INumberVectorProperty *nvp = getNumber(name);

	   	for (int i=0;i<nvp->nnp;i++) {
	        	INumber *eqp = &nvp->np[i];
        		if (!eqp)
	        	    return;
			
	        	IO *pin_config=(IO*)eqp->aux0;
			if (pin_config == NULL) continue;
				if (pin_config->IOType==AI) {
					int pin=pin_config->pin;
					if (sf->pin_info[pin].mode == FIRMATA_MODE_ANALOG) {
        					eqp->value=pin_config->MulScale*(double)(sf->pin_info[pin].value)+pin_config->AddScale;
						//IDLog("%f\n",eqp->value);
						IDSetNumber(nvp, NULL);
					}
        	 		} 
            	} 
	}
    		
	//TEXT
        if (type == INDI_TEXT) {     
	        ITextVectorProperty *tvp = getText(name);

	   	for (int i=0;i<tvp->ntp;i++) {
	        	IText *eqp = &tvp->tp[i];
        		if (!eqp)
	        	    return;

			if (eqp->aux0 == NULL) continue;			
      			strcpy(eqp->text,(char*)eqp->aux0);
			//IDLog("%s.%s TEXT: %s \n",tvp->name,eqp->name,eqp->text);
			IDSetText(tvp, NULL);
	 	}
    	}  
    }

    SetTimer(POLLMS);
}


/**************************************************************************************
** Initialize all properties & set default values.
**************************************************************************************/
bool indiduino::initProperties()
{
    // This is the default driver skeleton file location
    // Convention is: drivername_sk_xml
    // Default location is /usr/share/indi  
    
    struct stat st;

    strcpy(skelFileName, DEFAULT_SKELETON_FILE);

    setVersion(VERSION_MAJOR, VERSION_MINOR);

    char *skel = getenv("INDISKEL");
    if (skel) {
	IDLog("Building from %s skeleton\n",skel);
	strcpy(skelFileName, skel);
        buildSkeleton(skel);  
    } else if (stat(skelFileName,&st) == 0) {
	IDLog("Building from %s skeleton\n",skelFileName);
        buildSkeleton(skelFileName);
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
	//IDLog("%s\n",nvp->name);

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
        
		IO *pin_config=(IO*)eqp->aux0;
		if (pin_config == NULL) continue;
	        //IDLog("%s\n",s);
		if (pin_config->IOType=AO) {      
		    int pin=pin_config->pin;
	            IUUpdateNumber(nvp, values, names, n);
		    IDLog("Setting output %s.%s on pin %u to %f\n",nvp->name,eqp->name,pin,eqp->value);
	   	    sf->setPwmPin(pin,(int)(pin_config->MulScale*eqp->value + pin_config->AddScale));
	            IDSetNumber (nvp, "%s.%s change to %f\n",nvp->name,eqp->name,eqp->value);
	            nvp->s = IPS_IDLE;           
		    change=true;
	        }
		if (pin_config->IOType=AI) {      
		    int pin=pin_config->pin;
	            IUUpdateNumber(nvp, values, names, n);
		    //IDLog("%s:%f\n",eqp->name,eqp->value);  	    
        	    nvp->s = IPS_IDLE;
		    change=true;
        	}
        }

	if (change) {
		IDSetNumber(nvp, NULL);
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
	//IDLog("NAME:%s\n",name);
        //IDLog("%s\n",svp->name);

        if (isConnected() == false)
        {
            svp->s = IPS_ALERT;
            IDSetSwitch(svp, "Cannot change property while device is disconnected.");
            return false;
        }


        if (!svp )
            return false;

	for (int i=0;i<svp->nsp;i++) {
               	ISwitch *sqp = &svp->sp[i];
		IO *pin_config=(IO*)sqp->aux;
		if (pin_config == NULL) continue;
		//IDLog("Switching ON %s pin %u type %u %u\n",sqp->label,pin_config->pin,pin_config->IOType,pin_config->IOType==DI);
		if (  pin_config->IOType == DO) {
			int pin=pin_config->pin;
			IUUpdateSwitch(svp, states, names, n);
			if (sqp->s == ISS_ON) {
				IDLog("Switching ON %s.%s on pin %u\n",svp->name,sqp->name,pin);
        			sf->writeDigitalPin(pin,ARDUINO_HIGH);
				IDSetSwitch (svp, "%s.%s ON\n",svp->name,sqp->name);
			} else {
				IDLog("Switching OFF %s.%s on pin %u\n",svp->name,sqp->name,pin);
				sf->writeDigitalPin(pin,ARDUINO_LOW);
				IDSetSwitch (svp, "%s.%s OFF\n",svp->name,sqp->name);
			}       		
		}
	}

    IUUpdateSwitch(svp, states, names, n);
    return true;


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
    if (sf->portOpen)
    {
        IDLog("ARDUINO BOARD CONNECTED.\n");
        IDLog("FIRMATA VERSION:%s\n",sf->firmata_name);
        IDSetSwitch (getSwitch("CONNECTION"),"CONNECTED.FIRMATA VERSION:%s\n",sf->firmata_name);
        if (!setPinModesFromSKEL())
        {
            IDLog("FAIL TO MAP ARDUINO PINS. CHECK SKELETON FILE SINTAX\n");
            IDSetSwitch (getSwitch("CONNECTION"),"FAIL TO MAP ARDUINO PINS. CHECK SKELETON FILE SINTAX\n");
            delete sf;
            return false;
        }
        else
        {
            SetTimer(POLLMS);
            return true;
        }
    }
    else
    {
        IDLog("ARDUINO BOARD FAIL TO CONNECT. CHECK PORT NAME\n");
        IDSetSwitch (getSwitch("CONNECTION"),"ARDUINO BOARD FAIL TO CONNECT. CHECK PORT NAME\n");
        delete sf;
        return false;
    }
    
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

bool indiduino::setPinModesFromSKEL()
{
    char   errmsg[MAXRBUF];
    FILE   *fp = NULL;
    LilXML *lp = newLilXML();
    XMLEle *fproot = NULL;
    XMLEle *ep=NULL, *ioep=NULL, *xmlp=NULL; 
    int    numiopin=0;


    fp = fopen(skelFileName, "r");

    if (fp == NULL)
    {
        IDLog("Unable to build skeleton. Error loading file %s: %s\n", skelFileName, strerror(errno));
        return false;
    }

    fproot = readXMLFile(fp, lp, errmsg);

    if (fproot == NULL)
    {
        IDLog("Unable to parse skeleton XML: %s", errmsg);
        return false;
    }

    IDLog("Setting pins behaviour from <indiduino> tags\n");
    std::vector<INDI::Property *> *pAll = getProperties();
    for (int i=0; i < pAll->size(); i++) {
	const char *name;
	const char *label;
	const char *aux;
    INDI_PROPERTY_TYPE type;
	name=pAll->at(i)->getName();
	label=pAll->at(i)->getLabel();
	type=pAll->at(i)->getType();

	if (ep==NULL) {
		ep = nextXMLEle (fproot, 1);
	} else {
		ep = nextXMLEle (fproot, 0);
	}
	if (ep==NULL) {
		break;
	}	
	//IDLog("XML ELEMENT:%s\n",findXMLAttValu(ep,"name"));
        //IDLog("Property #%d: %s %s \n", i, name,label,type);
	ioep == NULL;
        if (type == INDI_SWITCH) { 
		ISwitchVectorProperty *svp = getSwitch(name);
		//IDLog("%s\n",svp->name);
        	ioep==NULL;
		for (int i=0;i<svp->nsp;i++) {
                	ISwitch *sqp = &svp->sp[i];			
			if (ioep==NULL) {
				ioep = nextXMLEle (ep, 1);
			} else {
				ioep = nextXMLEle (ep, 0);
			}
			xmlp = findXMLEle(ioep,"indiduino");
			if (xmlp !=NULL) {
				if (!readInduinoXml(xmlp,numiopin)) {
					IDLog("Malforme <indiduino> XML\n");
					return false;
				}
				sqp->aux=(void*) &iopin[numiopin] ;	
				int pin=iopin[numiopin].pin;
				IDLog("%s.%s  pin %u set as DIGITAL OUTPUT\n",svp->name,sqp->name,pin);
				sf->setPinMode(pin,FIRMATA_MODE_OUTPUT);
				IDLog("numiopin:%u\n",numiopin);
				numiopin++;
			}
		}
        }
	if (type == INDI_TEXT) { 
		ITextVectorProperty *tvp = getText(name);
		//IDLog("%s\n",svp->name);
        	ioep==NULL;
		for (int i=0;i<tvp->ntp;i++) {
                	IText *tqp = &tvp->tp[i];			

			if (ioep==NULL) {
				ioep = nextXMLEle (ep, 1);
			} else {
				ioep = nextXMLEle (ep, 0);
			}
			xmlp = findXMLEle(ioep,"indiduino");
			if (xmlp !=NULL) {

				if (!readInduinoXml(xmlp,0)) {
					IDLog("Malforme <indiduino> XML\n");
					return false;
				}
				tqp->aux0=(void*) &sf->string_buffer ;	
				IDLog("%s.%s ARDUINO TEXT\n",tvp->name,tqp->name);
				IDLog("numiopin:%u\n",numiopin);
			}
		}
        }
	if (type == INDI_LIGHT) { 
		ILightVectorProperty *lvp = getLight(name);
		//IDLog("%s\n",svp->name);
        	ioep==NULL;
		for (int i=0;i<lvp->nlp;i++) {
                	ILight *lqp = &lvp->lp[i];			
			if (ioep==NULL) {
				ioep = nextXMLEle (ep, 1);
			} else {
				ioep = nextXMLEle (ep, 0);
			}
			xmlp = findXMLEle(ioep,"indiduino");
			if (xmlp !=NULL) {
				if (!readInduinoXml(xmlp,numiopin)) {
					IDLog("Malforme <indiduino> XML\n");
					return false;
				}
				lqp->aux=(void*) &iopin[numiopin] ;	
				int pin=iopin[numiopin].pin;
				IDLog("%s.%s  pin %u set as DIGITAL INPUT\n",lvp->name,lqp->name,pin);
				sf->setPinMode(pin,FIRMATA_MODE_INPUT);
				IDLog("numiopin:%u\n",numiopin);
				numiopin++;
			}
		}
        }
        if (type == INDI_NUMBER) {     
	        INumberVectorProperty *nvp = getNumber(name);
		//IDLog("%s\n",nvp->name);
        	ioep==NULL;
	   	for (int i=0;i<nvp->nnp;i++) {
	        	INumber *eqp = &nvp->np[i];
        		//IDLog("%s\n",eqp->name);
        		if (ioep==NULL) {
				ioep = nextXMLEle (ep, 1);
			} else {
				ioep = nextXMLEle (ep, 0);
			}
			xmlp = findXMLEle(ioep,"indiduino");
			if (xmlp !=NULL) {
				if (!readInduinoXml(xmlp,numiopin)) {
					IDLog("Malforme <indiduino> XML\n");
					return false;
				}
				eqp->aux0=(void*) &iopin[numiopin] ;	
				int pin=iopin[numiopin].pin;
				if (iopin[numiopin].IOType==AO) {
					IDLog("%s.%s  pin %u set as ANALOG OUTPUT\n",nvp->name,eqp->name,pin);
					sf->setPinMode(pin,FIRMATA_MODE_PWM);
				} else if( iopin[numiopin].IOType==AI) {
					IDLog("%s.%s  pin %u set as ANALOG INPUT\n",nvp->name,eqp->name,pin);
					sf->setPinMode(pin,FIRMATA_MODE_ANALOG);
				}
				IDLog("numiopin:%u\n",numiopin);
				numiopin++;
			}
            	} 
	 }
    }
    sf->setSamplingInterval(POLLMS/2); 
    sf->reportAnalogPorts(1);
    sf->reportDigitalPorts(1);
    return true;
}

bool indiduino::readInduinoXml(XMLEle *ioep, int npin)
{
	char *propertyTag;
	int  pin;

	if (ioep ==NULL ) 
		return false;

	propertyTag = tagXMLEle(parentXMLEle(ioep));



	if (!strcmp(propertyTag,"defSwitch") || !strcmp(propertyTag,"defLight") || !strcmp(propertyTag,"defNumber")) {

		pin=atoi(findXMLAttValu(ioep, "pin"));

		if (pin >=1 && pin <= 40) { //19 hardware pins. 
			iopin[npin].pin= pin;
		} else {
	 		IDLog("induino: pin number is required. Check pin attrib value (1-19)\n");
			return false;
		}


	   	if ( !strcmp(propertyTag,"defSwitch") ) {			
				iopin[npin].IOType=DO;     			
		}

   		if ( !strcmp(propertyTag,"defLight") ) {
				iopin[npin].IOType=DI;      			
		}

		if ( !strcmp(propertyTag,"defNumber") ) {
					
				if (strcmp(findXMLAttValu(ioep, "mul"),""))	{
					iopin[npin].MulScale= atof(findXMLAttValu(ioep, "mul"));
				} else {
					iopin[npin].MulScale= 1;
				}
				if (strcmp(findXMLAttValu(ioep, "add"),"")) {
					iopin[npin].AddScale= atof(findXMLAttValu(ioep, "add"));
				} else {
					iopin[npin].AddScale= 0;
				}
				if ( !strcmp(findXMLAttValu(ioep,"type"),"output")) {
					iopin[npin].IOType=AO;
				} else if ( !strcmp(findXMLAttValu(ioep,"type"),"input")) {
					iopin[npin].IOType=AI;
				} else {
	 				IDLog("induino: Setting type (input or output) is required for analogs\n");
					return false;
				}
		 }	

		if (false) {
				IDLog("arduino IO Match:Property:(%s)\n", findXMLAttValu(parentXMLEle(ioep),"name"));
				IDLog("arduino IO pin:(%u)\n", iopin[npin].pin);
 				IDLog("arduino IO type:(%u)\n", iopin[npin].IOType);
 				IDLog("arduino IO scale: mul:%g add:%g\n", iopin[npin].MulScale,iopin[npin].AddScale);
		}
	}
	return true;
}



