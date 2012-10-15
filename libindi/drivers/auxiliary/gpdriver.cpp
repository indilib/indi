/*******************************************************************************
  Copyright(c) 2012 Jasem Mutlaq. All rights reserved.

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
#include "gpdriver.h"


GPUSBDriver::GPUSBDriver()
{
    //ctor
    guideCMD[0]=0;
    debug=false;
}

GPUSBDriver::~GPUSBDriver()
{
    //dtor
   // usb_close(usb_handle);
}



bool GPUSBDriver::Connect()
{
    if (debug)
        usb_set_debug(2);

    dev=FindDevice(0x134A, 0x9020,0);

    if(dev==NULL)
     {
         IDLog("Error: No GPUSB device found\n");
         return false;
     }

     usb_handle=usb_open(dev);
     if(usb_handle != NULL)
     {
         int rc;

         rc=FindEndpoints();

         rc = usb_detach_kernel_driver_np(usb_handle,0);

         if (debug)
             IDLog("Detach Kernel returns %d\n",rc);

         rc = usb_set_configuration(usb_handle,1);
         if (debug)
             IDLog("Set Configuration returns %d\n",rc);

         rc=usb_claim_interface(usb_handle,0);

         if (debug)
             IDLog("claim interface returns %d\n",rc);

         rc= usb_set_altinterface(usb_handle,0);

         if (debug)
             IDLog("set alt interface returns %d\n",rc);

         if (rc== 0)
             return true;
     }

    return false;
    
}

bool GPUSBDriver::Disconnect()
{
    usb_release_interface(usb_handle, 0);
    usb_close(usb_handle);
    return true;
}

bool GPUSBDriver::startPulse(int direction)
{
    int rc=0;

    switch (direction)
    {
        case GPUSB_NORTH:
        guideCMD[0] &= GPUSB_CLEAR_DEC;
        guideCMD[0] |= (GPUSB_NORTH | GPUSB_LED_ON) & ~GPUSB_LED_RED;
        break;

        case GPUSB_WEST:
        guideCMD[0] &= GPUSB_CLEAR_RA;
        guideCMD[0] |= (GPUSB_WEST | GPUSB_LED_ON) & ~GPUSB_LED_RED;
        break;

        case GPUSB_SOUTH:
        guideCMD[0] &= GPUSB_CLEAR_DEC;
        guideCMD[0] |= GPUSB_SOUTH | GPUSB_LED_ON | GPUSB_LED_RED;
        break;

        case GPUSB_EAST:
        guideCMD[0] &= GPUSB_CLEAR_RA;
        guideCMD[0] |= GPUSB_EAST | GPUSB_LED_ON | GPUSB_LED_RED;
        break;
    }

    if (debug)
        IDLog("start command value is 0x%X\n", guideCMD[0]);

    rc=WriteBulk(guideCMD,1,1000);

    if (debug)
        IDLog("startPulse WriteBulk returns %d\n",rc);
    if(rc==1)
        return true;

    return false;
}

bool GPUSBDriver::stopPulse(int direction)
{
   int rc=0;

    switch (direction)
    {
        case GPUSB_NORTH:
        if (debug) IDLog("Stop North\n");
        guideCMD[0] &= GPUSB_CLEAR_DEC;
        break;

        case GPUSB_WEST:
        if (debug) IDLog("Stop West\n");
        guideCMD[0] &= GPUSB_CLEAR_RA;
        break;

        case GPUSB_SOUTH:
        if (debug) IDLog("Stop South\n");
        guideCMD[0] &= GPUSB_CLEAR_DEC;
        break;

        case GPUSB_EAST:
        if (debug) IDLog("Stop East\n");
        guideCMD[0] &= GPUSB_CLEAR_RA;
        break;
    }

    if ( (guideCMD[0] & GPUSB_NORTH) || (guideCMD[0] & GPUSB_WEST))
        guideCMD[0] &= ~GPUSB_LED_RED;
    else if ( (guideCMD[0] & GPUSB_SOUTH) || (guideCMD[0] & GPUSB_EAST))
        guideCMD[0] |= GPUSB_LED_RED;


    if ( (guideCMD[0] & 0xF) == 0)
            guideCMD[0] = 0;

    if (debug)
        IDLog("stop command value is 0x%X\n", guideCMD[0]);

    rc=WriteBulk(guideCMD,1,1000);

    if (debug)
        IDLog("stopPulse WriteBulk returns %d\n",rc);
    if(rc==1)
        return true;

    return false;

}

