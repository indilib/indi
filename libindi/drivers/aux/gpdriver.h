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

#ifndef GPUSBDRIVER_H
#define GPUSBDRIVER_H

#include "../../libs/indibase/indiusbdevice.h"

/* Standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include <sys/time.h>
#include <time.h>

enum
{
    GPUSB_NORTH = 0x08,
    GPUSB_SOUTH = 0x04,
    GPUSB_EAST  = 0x01,
    GPUSB_WEST  = 0x02,
    GPUSB_LED_RED  = 0x10,
    GPUSB_LED_ON  = 0x20,
    GPUSB_CLEAR_RA = 0xFC,
    GPUSB_CLEAR_DEC= 0xF3
};

class GPUSBDriver : public INDI::USBDevice
{
    public:
    GPUSBDriver();
    virtual ~GPUSBDriver();

    //  Generic indi device entries
    bool Connect();
    bool Disconnect();

    bool startPulse(int direction);
    bool stopPulse(int direction);

    void setDebug(bool enable) { debug = enable; }

private:
    char guideCMD[1];
    bool debug;

};

#endif // GPUSBDriver_H
