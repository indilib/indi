/*******************************************************************************
  Copyright(c) 2012 Jasem Mutlaq. All rights reserved.

  Based on driver by Geoffrey Hausheer.

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

#ifndef _QHY5_DRIVER_H_
#define _QHY5_DRIVER_H_

#include <libindi/indiccd.h>
#include <libindi/indiusbdevice.h>

#include <fitsio.h>

enum
{
    QHY_NORTH = 0x20,
    QHY_SOUTH = 0x40,
    QHY_EAST  = 0x10,
    QHY_WEST  = 0x80
};


class QHY5Driver :  public INDI::USBDevice
{
    public:
        QHY5Driver();
        ~QHY5Driver();

        bool Connect();
        bool Disconnect();

        void setSimulation(bool enable) { simulation = enable;}
        void setDebug(bool enable) { debug = enable; }

        int SetParams(int in_width, int in_height, int in_offw, int in_offh, int in_gain, int *pixw, int *pixh);
        int GetDefaultParam(int *width, int *height, int *gain);

        char *GetRow(int row);
        int ReadExposure();
        int StartExposure(unsigned int exposure);

        int Pulse(int direction, int duration_msec);
        int ResetCamera();


    private:
        int width;
        int height;
        int gain;
        int offw;
        int offh;
        int bpp;

        int impixw,impixh;

        bool hasGuide;
        bool hasST4;
        bool simulation;
        bool debug;

        char *imageBuffer;
        int imageBufferSize;

        fitsfile* fptr;
        unsigned char *fitsBuffer;





};

#endif
