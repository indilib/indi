/*
    LX200 AP Driver
    Copyright (C) 2007 Markus Wildi

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#pragma once

// Used by cp2 driver
#define setAPPark(fd)                          write(fd, "#:KA#", 4)
#define setAPUnPark(fd)                        write(fd, "#:PO#", 4)

// Used by several drivers
#define getAPVersionNumber(fd, x)              getCommandString(fd, x, "#:V#")
#define setAPClearBuffer(fd)                   write(fd, "#", 1) /* AP key pad manual startup sequence */
#define setAPBackLashCompensation(fd, x, y, z) setCommandXYZ(fd, x, y, z, "#:Br")

#define AP_TRACKING_SIDEREAL     0
#define AP_TRACKING_SOLAR       1
#define AP_TRACKING_LUNAR       2
#define AP_TRACKING_CUSTOM      3
#define AP_TRACKING_OFF         4

#define AP_PEC_OFF 0
#define AP_PEC_ON 1
#define AP_PEC_RECORD 2
#define AP_PEC_ENCODER 3

#ifdef __cplusplus
extern "C" {
#endif

void set_lx200ap_name(const char *deviceName, unsigned int debug_level);
int check_lx200ap_connection(int fd);
int getAPUTCOffset(int fd, double *value);
int setAPObjectAZ(int fd, double az);
int setAPObjectAlt(int fd, double alt);
int setAPUTCOffset(int fd, double hours);
int setAPSlewMode(int fd, int slewMode);
int APSyncCM(int fd, char *matchedObject);
int APSyncCMR(int fd, char *matchedObject);
int selectAPMoveToRate(int fd, int moveToRate);
int selectAPSlewRate(int fd, int slewRate);
int selectAPTrackingMode(int fd, int trackMode);
int selectAPGuideRate(int fd, int guideRate);
int selectAPPECState(int fd, int pecstate);
int swapAPButtons(int fd, int currentSwap);
int setAPObjectRA(int fd, double ra);
int setAPObjectDEC(int fd, double dec);
int setAPSiteLongitude(int fd, double Long);
int setAPSiteLatitude(int fd, double Lat);
int setAPRATrackRate(int fd, double rate);
int setAPDETrackRate(int fd, double rate);
int APSendPulseCmd(int fd, int direction, int duration_msec);
void set_lx200ap_exp_name(const char *deviceName, unsigned int debug_level);
int selectAPCenterRate(int fd, int centerRate);
int check_lx200ap_status(int fd, char *parkStatus, char *slewStatus);
int APParkMount(int fd);
int APUnParkMount(int fd);
int getAPPECState(int fd, int *pecState);
int getAPWormPosition(int fd, int *position);

// Make sure the buffer passed in to getApStatusString (statusString) is at least 32 bytes.
int getApStatusString(int fd, char *statusString);
bool apStatusParked(char *statusString);
bool apStatusSlewing(char *statusString);
int isAPInitialized(int fd, bool *isInitialized);

#ifdef __cplusplus
}
#endif
