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
#ifndef FCUSB_H
#define FCUSB_H

#include "IndiFocusser.h"
#include "UsbDevice.h"

//  Vid:Pid for shoestring devices
//  134a:9021   dsusb
//  134a:9023   fcusb
//  134a:9024   fcusb2

class fcusb : public IndiFocusser, public UsbDevice
{
    protected:


    private:
        //  Data we store
        int PwmRate;
        int MotorSpeed;
        int MotorState;
        int MotorDir;
        int LedColor;
        int LedState;

        //  fcusb specific entries
        int SetLedColor(int);
        int LedOff();
        int LedOn();
        int SetPwm(int);
        int WriteState();

    public:
        fcusb();
        virtual ~fcusb();

        int init_properties();
        void ISGetProperties (const char *dev);

        //  Generic indi focusser entries
        bool Connect();
        bool Disconnect();
        char * getDefaultName();

        // Parent overrides
        int Move(int dir,int speed, int time);


};

#endif // FCUSB_H
