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

#include "qhy5_driver.h"

#define STORE_WORD_BE(var, val) *(var) = ((val) >> 8) & 0xff; *((var) + 1) = (val) & 0xff

static const int gain_map[] = {
    0x000,0x004,0x005,0x006,0x007,0x008,0x009,0x00A,0x00B,0x00C,
    0x00D,0x00E,0x00F,0x010,0x011,0x012,0x013,0x014,0x015,0x016,
    0x017,0x018,0x019,0x01A,0x01B,0x01C,0x01D,0x01E,0x01F,0x051,
    0x052,0x053,0x054,0x055,0x056,0x057,0x058,0x059,0x05A,0x05B,
    0x05C,0x05D,0x05E,0x05F,0x6CE,0x6CF,0x6D0,0x6D1,0x6D2,0x6D3,
    0x6D4,0x6D5,0x6D6,0x6D7,0x6D8,0x6D9,0x6DA,0x6DB,0x6DC,0x6DD,
    0x6DE,0x6DF,0x6E0,0x6E1,0x6E2,0x6E3,0x6E4,0x6E5,0x6E6,0x6E7,
    0x6FC,0x6FD,0x6FE,0x6FF
    };
static const int gain_map_size = (sizeof(gain_map) / sizeof(int));

QHY5Driver::QHY5Driver()
{
    imageBuffer = NULL;
    imageBufferSize=0;
    hasGuide=false;
    hasST4=false;

    // Simulation stuff
    simulation=false;
    fptr = NULL;
    fitsBuffer = NULL;
    srand( time(NULL));
}

QHY5Driver::~QHY5Driver()
{
    if (fptr)
    {
        int status=0;
        fits_close_file(fptr, &status);

        free(fitsBuffer);
    }
}

bool QHY5Driver::Connect()
{
    if (simulation)
        return true;

   dev=FindDevice(0x16c0, 0x296d,0);

   if(dev==NULL)
    {
        IDLog("Error: No QHY5 Cameras found\n");
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

bool QHY5Driver::Disconnect()
{
    if (simulation)
        return true;

    usb_release_interface(usb_handle, 0);
    usb_close(usb_handle);
    return true;
}

int QHY5Driver::GetDefaultParam(int *width, int *height, int *gain)
{
    *width = 1280;
    *height = 1024;
    *gain = 100;
    return 0;
}


int QHY5Driver::ResetCamera()
{      
    int impixw,impixh;

    if (SetParams(1280, 1024, 0, 0, 100, &impixw, &impixh))
    {
        IDLog("Error: Failed to reset camera\n");
        return -1;
    }

    return 0;
}

int QHY5Driver::Pulse(int direction, int duration_msec)
{
    if (simulation)
        return 0;

    unsigned int ret;
    int duration[2] = {-1, -1};
    int cmd = 0x00;

    if (! (direction & (QHY_NORTH | QHY_SOUTH | QHY_EAST | QHY_WEST))) {
        IDLog( "No direction specified to qhy5_timed_move\n");
        return 1;
    }
    if (duration_msec == 0) {
        //cancel quiding
        if ((direction & (QHY_NORTH | QHY_SOUTH)) &&
            (direction & (QHY_EAST | QHY_WEST)))
        {
            cmd = 0x18;
        } else if(direction & (QHY_NORTH | QHY_SOUTH)) {
            cmd = 0x22;
        } else {
            cmd = 0x21;
                }
        return ControlMessage(0xc2, cmd, 0, 0, (char *)&ret, sizeof(ret));
    }
    if (direction & QHY_NORTH) {
        cmd |= 0x20;
        duration[1] = duration_msec;
    } else if (direction & QHY_SOUTH) {
        cmd |= 0x40;
        duration[1] = duration_msec;
    }
    if (direction & QHY_EAST) {
        cmd |= 0x10;
        duration[0] = duration_msec;
    } else if (direction & QHY_WEST) {
        cmd |= 0x80;
        duration[0] = duration_msec;
    }
    return ControlMessage(0x42, 0x10, 0, cmd, (char *)&duration, sizeof(duration));
}

int QHY5Driver::SetParams(int in_width, int in_height, int in_offw, int in_offh, int in_gain, int *pixw, int *pixh)
{
    char reg[19];
    int offset, value, index;
    int gain_val;

    in_height -=  in_height % 4;
    offset = (1048 - in_height) / 2;
    index = (1558 * (in_height + 26)) >> 16;
    value = (1558 * (in_height + 26)) & 0xffff;
    gain_val = gain_map[(int)(0.5 + in_gain * gain_map_size / 100.0)];
    STORE_WORD_BE(reg + 0,  gain_val);
    STORE_WORD_BE(reg + 2,  gain_val);
    STORE_WORD_BE(reg + 4,  gain_val);
    STORE_WORD_BE(reg + 6,  gain_val);
    STORE_WORD_BE(reg + 8,  offset);
    STORE_WORD_BE(reg + 10, 0);
    STORE_WORD_BE(reg + 12, in_height - 1);
    STORE_WORD_BE(reg + 14, 0x0521);
    STORE_WORD_BE(reg + 16, in_height + 25);
    reg[18] = 0xcc;

    if (simulation == false)
    {
        int rc;
        rc = ControlMessage(0x42, 0x13, value, index, reg, 19);
        if (debug)
            IDLog( "SetParam1 result: %d\n", rc);

        usleep(20000);
        rc = ControlMessage(0x42, 0x14, 0x31a5, 0, reg, 0);

        if (debug)
            IDLog( "SetParam2 result: %d\n", rc);

        usleep(10000);
        rc = ControlMessage(0x42, 0x16, 0, 0, reg, 0);

        if (debug)
            IDLog( "SetParam3 result: %d\n", rc);
    }

    width = in_width;
    height = in_height;
    offw = in_offw;
    offh = in_offh;
    gain = in_gain;
    bpp = 1;

    if (pixw)
        *pixw = width - offw;
    if (pixh)
        *pixh = height - offh;

    if (imageBufferSize < 1558 * (height + 26) * bpp)
    {
        imageBufferSize = 1558 * (height + 26) * bpp;
        imageBuffer = (char *) realloc(imageBuffer, imageBufferSize);
    }

    if (debug)
        IDLog( "Driver imgBufferSize is %d bytes\n", imageBufferSize);
    return(0);
}

int QHY5Driver::StartExposure(unsigned int exposure)
{

    int index, value;
    char buffer[11] = "DEADEADEAD"; // for debug purposes
    index = exposure >> 16;
    value = exposure & 0xffff;

    usleep(20000);


    if (simulation)
    {
        int status=0, nulval=0, anynull=0;


        if (fptr == NULL)
        {
            if (fits_open_image(&fptr, "m42_test.fits", READONLY, &status))
            {
                fits_report_error(stderr, status);
                return -1;
            }

            fitsBuffer = (unsigned char *) malloc(1280*1024);
            if (fitsBuffer == NULL)
                return -1;


            if (fits_read_2d_byt(fptr, 0, nulval, 1280, 1280, 1024, fitsBuffer, &anynull, &status))
            {
                fits_report_error(stderr, status);
                return -1;
            }
        }


        return 0;
    }
    else
    {
        if (debug)
            IDLog( "QHY5Driver: calling start exposure...\n");
        return ControlMessage(0xc2, 0x12, value, index, buffer, 2);
    }
}

int QHY5Driver::ReadExposure()
{
    int result;

    if (simulation)
    {
        if (fitsBuffer == NULL)
        {
            IDLog(" fitsbuffer is NULL\n");
            return -1;
        }


        for (int i=0; i < (height-offh); i++)
            memcpy(GetRow(i), fitsBuffer + (i * 1280) + (offh * height) + offw, 1280-offw);

          return 0;
    }

    if (debug)
        IDLog( "QH5Driver: Reading %08x bytes\n", (unsigned int)imageBufferSize);

    result = usb_bulk_read(usb_handle, 0x82, imageBuffer, imageBufferSize, 20000);
    if (result == imageBufferSize)
    {
        if (debug)
        {

        IDLog( "Bytes: %d\n", result);
        for (int i = 0; i < result; i++)
        {
            if((i% 16) == 0)
            {
                IDLog( "\n%06x:", i);
            }
            fprintf(stderr, " %02x", imageBuffer[i]);
        }
        }

    }
    else
    {
        IDLog("Failed to read image. Got: %d, expected %u\n", result, (unsigned int)imageBufferSize);
        return 1;
    }
    return 0;
}

char * QHY5Driver::GetRow(int row)
{
    return (imageBuffer + 1558 * row + offh + 20);
}


