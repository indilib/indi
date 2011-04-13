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

#ifndef SXWHEEL_H
#define SXWHEEL_H

#include "IndiFilterWheel.h"
//#include "UsbDevice.h"

//  required linux headers
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

// some unix headers
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
//  some C headers
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

class SxWheel : public IndiFilterWheel
{
    protected:
    private:

        int wheelfd;


        int SendWheelMessage(int,int);
        int ReadWheelMessage();

    public:
        SxWheel();
        virtual ~SxWheel();

        bool Connect();
        bool Disconnect();
        char *getDefaultName();

        int init_properties();

        void ISGetProperties (const char *dev);

        int QueryFilter();
        int SelectFilter(int);
        void TimerHit();

};

#endif // SXWHEEL_H
