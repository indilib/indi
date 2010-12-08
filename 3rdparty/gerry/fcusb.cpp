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

#include "fcusb.h"

IndiDevice * _create_device()
{
    IndiDevice *Fcusb;
    IDLog("Create an fcusb device\n");
    Fcusb=new fcusb();
    return Fcusb;
}

fcusb::fcusb()
: PwmRate(16)
, MotorSpeed(255)
, MotorState(0)
, MotorDir(0)
, LedColor(0)
, LedState(0)
{
    //ctor
}

fcusb::~fcusb()
{
    //dtor
}

char * fcusb::getDefaultName()
{
    return (char *)"FcUsb";
}

bool fcusb::Connect()
{
    IDLog("Checking for FCUSB\n");
    dev=FindDevice(0x134a,0x9023,0);
    if(dev==NULL) {
        IDLog("Checking for FCUSB2\n");
        dev=FindDevice(0x134a,0x9023,0);
    }
    if(dev==NULL) {
        IDLog("No shoestring focusser found");
        return false;
    }
    IDLog("Found a shoestring focus gadget\n");
    usb_handle=usb_open(dev);
    if(usb_handle != NULL) {
        int rc;

        rc=FindEndpoints();

        rc=usb_detach_kernel_driver_np(usb_handle,0);
        IDLog("Detach Kernel returns %d\n",rc);

        rc=usb_claim_interface(usb_handle,0);

        IDLog("Claim Interface returns %d\n",rc);
        LedOff();
        WriteState();

        return true;

    }
    return false;
}

bool fcusb::Disconnect()
{
    LedOff();
    WriteState();
    usb_close(usb_handle);
    return true;
}

int fcusb::init_properties()
{

    //setDeviceName("FcUsb");
    IndiFocusser::init_properties();
    //  Lets add one property that makes it easy to test things
    //  Label it led on/off
    return 0;
}
void fcusb::ISGetProperties (const char *dev)
{
    IDLog("fcusb::ISGetProperties %s\n",dev);
    //  First we let our parent class do it's thing
    IndiFocusser::ISGetProperties(dev);

    //  Now we add anything that's specific to this telescope
    //  or we could just load from a skeleton file too
    return;
}

int fcusb::WriteState()
{
    char buf[2];
    int st;

    buf[1]=MotorSpeed;
    if(LedState==0) st=0;
    else st=0x20;
    if(MotorState==0) {

    } else {
        if(MotorDir==1) st |=0x02;
        else st |= 0x01;
    }
    st |=LedColor;
    switch(PwmRate) {
        case 16:
            break;
        case 4:
            st |= 0x40;
            break;
        default:
            st |= 0xc0;
            break;
    }
    buf[0]=st;
    WriteInterrupt(buf,2,1000);

    return 0;
}

int fcusb::SetLedColor(int c)
{
    if(c==0) LedColor=0;
    else LedColor=0x10;
    return 0;
}

int fcusb::LedOff()
{
    LedState=0;
    WriteState();
    return 0;
}

int fcusb::LedOn()
{
    LedState=1;
    WriteState();
    return 0;
}

int fcusb::SetPwm(int r)
{
    PwmRate=r;
    return 0;
}

int fcusb::Move(int dir,int speed, int time)
{
    int startled;
    startled=LedState;

    MotorDir=dir;
    MotorSpeed=speed;
    MotorState=1;
    if(dir==1) SetLedColor(1);
    else SetLedColor(0);
    LedState=1;
    WriteState();
    usleep(time*1000);
    MotorState=0;
    LedState=startled;
    WriteState();
    return 0;
}

