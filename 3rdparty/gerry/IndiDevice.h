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
#ifndef INDIDEVICE_H
#define INDIDEVICE_H

#include <libindi/indidevapi.h>
#include <libindi/indicom.h>
#include <libindi/defaultdriver.h>


class IndiDevice : public INDI::DefaultDriver
{

    private:

    protected:
    public:
        IndiDevice();
        virtual ~IndiDevice();

        //  These are the properties we define, that are generic to pretty much all devices
        //  They are public to make them available to all dervied classes and thier children
        ISwitchVectorProperty ConnectionSV; //  Vector of switches for our connection stuff
        ISwitch ConnectionS[2];

        //  Helper functions that encapsulate the indi way of doing things
        //  and give us a clean c++ class method

        virtual int init_properties();
        //  This will be called after connecting
        //  to flesh out and update properties to the
        //  client when the device is connected
        virtual bool UpdateProperties();

        //  A helper for child classes
        virtual bool DeleteProperty(char *);

        //  A state variable applicable to all devices
        //  and I cant get any intelligent result from the parent
        //  class calling isConnected or setConnected, so, we wont use it
        bool Connected;

        int SetTimer(int);
        void RemoveTimer(int);
        virtual void TimerHit();

        //  The function dispatchers required for all drivers
        virtual void ISGetProperties (const char *dev);
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual void ISSnoopDevice (XMLEle *root);

        //  some virtual functions that our underlying classes are meant to override
        virtual bool Connect();
        virtual bool Disconnect();
        virtual char *getDefaultName()=0;

};

extern IndiDevice *device;
extern IndiDevice *_create_device();

#endif // INDIDEVICE_H
