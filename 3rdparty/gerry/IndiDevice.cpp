/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "IndiDevice.h"

#include <string.h>

IndiDevice *device=NULL;


/**************************************************************************************
**
***************************************************************************************/
void ISGetProperties (const char *dev)
{
    fprintf(stderr,"Enter ISGetProperties '%s'\n",dev);
    if(device==NULL) {
        IDLog("Create device for %s\n",dev);
        device=_create_device();
        if(dev != NULL) {
            //fprintf(stderr,"Calling setDeviceName %s\n",dev);
            device->setDeviceName(dev);
            //fprintf(stderr,"deviceName() returns  %s\n",device->deviceName());
            //fprintf(stderr,"getDefaultName() returns  %s\n",device->getDefaultName());
        } else {
            //device->setDeviceName("junker");
            device->setDeviceName(device->getDefaultName());
        }
        device->init_properties();
    }
    device->ISGetProperties(dev);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
 fprintf(stderr,"Enter ISNewSwitch %s\n",dev);

 //ISInit();
 //fprintf(stderr,"Calling Device->IsNewSwitch for device %0x\n",(void *)device);
 device->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
 fprintf(stderr,"Enter ISNewText\n");
 //ISInit();
 device->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //fprintf(stderr,"OutsideClass::Enter ISNewNumber\n");
 //ISInit();
 device->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
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

/**************************************************************************************
**
***************************************************************************************/
void ISSnoopDevice (XMLEle *root)
{
    return device->ISSnoopDevice(root);
}

void timerfunc(void *t)
{
    //fprintf(stderr,"Got a timer hit with %x\n",t);
    if(t==device) {
        //  this was for my device
        //  but we dont have a way of telling
        //  WHICH timer was hit :(
        device->TimerHit();
    }
    return;
}


IndiDevice::IndiDevice()
{
    //ctor
    Connected=false;
}

IndiDevice::~IndiDevice()
{
    //dtor
}


int IndiDevice::init_properties()
{
    //  All devices should have a connection switch defined
    //  so lets create it

    IDLog("IndiDevice::init_properties()  MyDev=%s\n",deviceName());
    IUFillSwitch(&ConnectionS[0],"CONNECT","Connect",ISS_OFF);
    IUFillSwitch(&ConnectionS[1],"DISCONNECT","Disconnect",ISS_ON);
    IUFillSwitchVector(&ConnectionSV,ConnectionS,2,deviceName(),"CONNECTION","Connection","Main Control",IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    return 0;
}

bool IndiDevice::DeleteProperty(char *n)
{
    IDDelete(deviceName(),n,NULL);
    return true;
}

void IndiDevice::ISGetProperties(const char *dev)
{

    //  Now lets send the ones we have defined
    IDLog("IndiDevice::ISGetProperties %s\n",dev);
    IDDefSwitch (&ConnectionSV, NULL);

    if(Connected) UpdateProperties();   //  If already connected, send the rest
    //  And now get the default driver to send what it wants to send
    INDI::DefaultDriver::ISGetProperties(dev);


}

/**************************************************************************************
** Process Text properties
***************************************************************************************/
bool IndiDevice::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{

    //  And this base class doesn't actually process anything
    return INDI::DefaultDriver::ISNewText(dev,name,texts,names,n);
}

/**************************************************************************************
**  Process Numbers
***************************************************************************************/
bool IndiDevice::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{

    //  Our base class doesn't actually do any processing
    return INDI::DefaultDriver::ISNewNumber(dev,name,values,names,n);
}

/**************************************************************************************
**  Process switches
***************************************************************************************/
bool IndiDevice::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    //  Ok, lets Process any switches we actually handle here
    if(strcmp(dev,deviceName())==0) {
        //  it's for this device

        if(strcmp(name,ConnectionSV.name)==0) {
                bool rc;
                //IDLog("IndiDevice Switch %s\n",names[x]);

                IUUpdateSwitch(&ConnectionSV,states,names,n);

                if(ConnectionS[0].s==ISS_ON) {
                    if(!Connected) {
                        rc=Connect();
                        if(rc) {
                            ConnectionSV.s=IPS_OK;
                            //setConnected(true,"calling setconnected");
                            //ConnectionS[0].s=ISS_ON;
                            //ConnectionS[1].s=ISS_OFF;
                            Connected=true;
                        } else {
                            //ConnectionS[0].s=ISS_OFF;
                            //ConnectionS[1].s=ISS_ON;
                            ConnectionSV.s=IPS_ALERT;
                            Connected=false;
                        }
                    }
                    UpdateProperties();
                    IDSetSwitch(&ConnectionSV,NULL);
                } else {
                    if(Connected) rc=Disconnect();
                    //ConnectionS[0].s=ISS_OFF;
                    //ConnectionS[1].s=ISS_ON;
                    ConnectionSV.s=IPS_IDLE;
                    Connected=false;
                    UpdateProperties();
                    IDSetSwitch(&ConnectionSV,NULL);
                }

                /*
                if(strcmp(names[x],"CONNECT")==0) {
                    //  We are being requested to make a physical connection to the device
                    rc=Connect();
                    if(rc) {
                        //  Connection Succeeded
                        ConnectionSV.s=IPS_OK;
                        Connected=true;

                    } else {
                        //  Connection Failed
                        ConnectionSV.s=IPS_ALERT;
                        Connected=false;
                    }
                    IUUpdateSwitch(&ConnectionSV,states,names,n);
                    IDSetSwitch(&ConnectionSV,NULL);
                    IDLog("Connect ccalling update properties\n");
                    UpdateProperties();
                    //return true;
                }
                if(strcmp(names[x],"DISCONNECT")==0) {
                    //  We are being told to disconnect from the device
                    rc=Disconnect();
                    if(rc) {
                        ConnectionSV.s=IPS_IDLE;
                    } else {
                        ConnectionSV.s=IPS_ALERT;
                    }
                    Connected=false;
                    IDLog("Disconnect calling update properties\n");
                    UpdateProperties();
                    //  And now lets tell everybody how it went
                    IUUpdateSwitch(&ConnectionSV,states,names,n);
                    IDSetSwitch(&ConnectionSV,NULL);
                    return true;
                }
                */
            //}
        }
    }


    // let the default driver have a crack at it
    return INDI::DefaultDriver::ISNewSwitch(dev, name, states, names, n);
}

//  This is a helper function
//  that just encapsulates the Indi way into our clean c++ way of doing things
int IndiDevice::SetTimer(int t)
{
    return IEAddTimer(t,timerfunc,this);
}

//  Just another helper to help encapsulate indi into a clean class
void IndiDevice::RemoveTimer(int t)
{
    IERmTimer(t);
    return;
}
//  This is just a placeholder
//  This function should be overriden by child classes if they use timers
//  So we should never get here
void IndiDevice::TimerHit()
{
    return;
}

bool IndiDevice::Connect()
{
    //  We dont actually implement a device here
    //  So we cannot connect to it
    IDMessage(deviceName(),"IndiDevice:: has no device attached....");
    return false;
}

bool IndiDevice::Disconnect()
{
    //  Since we cannot connect, we cant disconnect either
    IDMessage(deviceName(),"IndiDevice:: has no device to detach....");
    return false;
}

bool IndiDevice::UpdateProperties()
{
    //  The base device has no properties to update
    return true;
}

void IndiDevice::ISSnoopDevice (XMLEle *root)
{
      INDI_UNUSED(root);
}
