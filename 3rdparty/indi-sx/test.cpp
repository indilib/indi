#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>

#include "sxconfig.h"
#include "sxccdusb.h"

int count;
HANDLE handles[20];
struct t_sxccd_params params;
unsigned short pixels[100*100];

int main() {
  count = sxOpen(handles);
  for (int i = 0; i < count; i++) {
    HANDLE handle=handles[i];
    sxReset(handle);
    sxGetCameraModel(handle);
//    sxGetFirmwareVersion(handle);
//    sxGetBuildNumber(handle);
    memset(&params, 0, sizeof(params));
    sxGetCameraParams(handle, 0, &params);

    sxSetTimer(handle, 900);

    while (sxGetTimer(handle)>0)
      sleep(1);

//    if (params.extra_caps & SXUSB_CAPS_SHUTTER) {
//      sxSetShutter(handle, 0);
//      sleep(1);
//      sxSetShutter(handle, 1);
//    }

//    if (params.extra_caps & SXUSB_CAPS_COOLER) {
//      unsigned short int temp;
//      unsigned char status;
//      sxSetCooler(handle, 1, (-10 + 273) * 10, &status, &temp);
//    }

    sxClearPixels(handle, 0, 0);
    usleep(1000);
    sxLatchPixels(handle, 0, 0, 50, 50, 100, 100, 1, 1);
    sxReadPixels(handle, pixels, 100*100);

    sxClose(handle);
  }
}
