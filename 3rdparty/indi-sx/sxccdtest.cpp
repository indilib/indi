/*
 Starlight Xpress CCD INDI Driver

 Copyright (c) 2012-2013 Cloudmakers, s. r. o.
 All Rights Reserved.

 Code is based on SX INDI Driver by Gerry Rozema and Jasem Mutlaq
 Copyright(c) 2010 Gerry Rozema.
 Copyright(c) 2012 Jasem Mutlaq.
 All rights reserved.

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
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <iostream>

#include "sxconfig.h"
#include "sxccdusb.h"

using namespace std;

int n;
DEVICE devices[20];
const char* names[20];

HANDLE handles[20];
struct t_sxccd_params params;
unsigned short pixels[10*10];

int main() {
  int i;
  unsigned int ui;
  unsigned short us;
  unsigned long ul;

	sxDebug(true);

  cout << "sx_ccd_test version " << VERSION_MAJOR << "." << VERSION_MINOR << endl << endl;
  n = sxList(devices, names, 20);
  cout << "sxList() -> " << n << endl << endl;

  for (int j = 0; j < n; j++) {
    HANDLE handle;

    cout << "testing " << names[j] << " -----------------------------------" << endl << endl;

    i = sxOpen(devices[j], &handle);
    cout << "sxOpen() -> " << i << endl << endl;

    //i = sxReset(handle);
    //cout << "sxReset() -> " << i << endl << endl;

    us = sxGetCameraModel(handle);
    cout << "sxGetCameraModel() -> " << us << endl << endl;

    //ul = sxGetFirmwareVersion(handle);
    //cout << "sxGetFirmwareVersion() -> " << ul << endl << endl;

    //us = sxGetBuildNumber(handle);
    //cout << "sxGetBuildNumber() -> " << us << endl << endl;

    memset(&params, 0, sizeof(params));
    i = sxGetCameraParams(handle, 0, &params);
    cout << "sxGetCameraParams(..., 0,...) -> " << i << endl << endl;


    i = sxSetTimer(handle, 900);
    cout << "sxSetTimer(900) -> " << i << endl << endl;

    while ((i = sxGetTimer(handle))>0) {
      cout << "sxGetTimer() -> " << i << endl << endl;
      sleep(1);
    }
    cout << "sxGetTimer() -> " << i << endl << endl;

    if (params.extra_caps & SXUSB_CAPS_SHUTTER) {
      i = sxSetShutter(handle, 0);
      cout << "sxSetShutter(0) -> " << i << endl << endl;
      sleep(1);
      i = sxSetShutter(handle, 1);
      cout << "sxSetShutter(1) -> " << i << endl << endl;
    }

    if (params.extra_caps & SXUSB_CAPS_COOLER) {
      unsigned short int temp;
      unsigned char status;
      i = sxSetCooler(handle, 1, (-10 + 273) * 10, &status, &temp);
      cout << "sxSetCooler() -> " << i << endl << endl;
    }

    i = sxClearPixels(handle, 0, 0);
    cout << "sxClearPixels(..., 0) -> " << i << endl << endl;

    usleep(1000);

    i = sxLatchPixels(handle, 0, 0, 0, 0, 10, 10, 1, 1);
    cout << "sxLatchPixels(..., 0, ...) -> " << i << endl << endl;

    i = sxReadPixels(handle, pixels, 10*10);
    cout << "sxReadPixels() -> " << i << endl << endl;

    for (int i=0; i<10; i++) {
      for (int j=0; j<10; j++)
        cout << pixels[i*10+j] << " ";
      cout << endl;
    }
    cout << endl;

    if (params.extra_caps & SXCCD_CAPS_GUIDER) {
      memset(&params, 0, sizeof(params));
      i = sxGetCameraParams(handle, 1, &params);
      cout << "sxGetCameraParams(..., 1,...) -> " << i << endl << endl;

      i = sxClearPixels(handle, 0, 1);
      cout << "sxClearPixels(..., 1) -> " << i << endl << endl;

      usleep(1000);

      i = sxLatchPixels(handle, 0, 1, 0, 0, 10, 10, 1, 1);
      cout << "sxLatchPixels(..., 1, ...) -> " << i << endl << endl;

      i = sxReadPixels(handle, pixels, 10*10);
      cout << "sxReadPixels() -> " << i << endl << endl;

      for (int i=0; i<10; i++) {
        for (int j=0; j<10; j++)
          cout << pixels[i*10+j] << " ";
        cout << endl;
      }
      cout << endl;
    }

    sxClose(&handle);
    cout << "sxClose() " << endl << endl;
  }
}

