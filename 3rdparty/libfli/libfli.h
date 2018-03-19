/*

  Copyright (c) 2002 Finger Lakes Instrumentation (FLI), L.L.C.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

        Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

        Redistributions in binary form must reproduce the above
        copyright notice, this list of conditions and the following
        disclaimer in the documentation and/or other materials
        provided with the distribution.

        Neither the name of Finger Lakes Instrumentation (FLI), LLC
        nor the names of its contributors may be used to endorse or
        promote products derived from this software without specific
        prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  ======================================================================

  Finger Lakes Instrumentation, L.L.C. (FLI)
  web: http://www.fli-cam.com
  email: support@fli-cam.com

*/

#ifndef _LIBFLI_H_
#define _LIBFLI_H_

#include <sys/types.h>

/**
   An opaque handle used by library functions to refer to FLI
   hardware.
*/
#define FLI_INVALID_DEVICE (-1)
typedef long flidev_t;

/**
   The domain of an FLI device.  This consists of a bitwise ORed
   combination of interface method and device type.  Valid interfaces
   are \texttt{FLIDOMAIN_PARALLEL_PORT}, \texttt{FLIDOMAIN_USB},
   \texttt{FLIDOMAIN_SERIAL}, and \texttt{FLIDOMAIN_INET}.  Valid
   device types are \texttt{FLIDEVICE_CAMERA},
   \texttt{FLIDOMAIN_FILTERWHEEL}, and \texttt{FLIDOMAIN_FOCUSER}.

   @see FLIOpen
   @see FLIList
 */
typedef long flidomain_t;

#define FLIDOMAIN_NONE (0x00)
#define FLIDOMAIN_PARALLEL_PORT (0x01)
#define FLIDOMAIN_USB (0x02)
#define FLIDOMAIN_SERIAL (0x03)
#define FLIDOMAIN_INET (0x04)
#define FLIDOMAIN_SERIAL_19200 (0x05)
#define FLIDOMAIN_SERIAL_1200 (0x06)
#define FLIDOMAIN_INTERFACE_MASK (0x000f)


#define FLIDEVICE_NONE (0x000)
#define FLIDEVICE_CAMERA (0x100)
#define FLIDEVICE_FILTERWHEEL (0x200)
#define FLIDEVICE_FOCUSER (0x300)
#define FLIDEVICE_HS_FILTERWHEEL (0x0400)
#define FLIDEVICE_RAW (0x0f00)
#define FLIDOMAIN_DEVICE_MASK (0x0f00)

/* The following two are really the same. ..._CONNECTION is old (deprecated) */
#define FLIDEVICE_ENUMERATE_BY_CONNECTION (0x8000)
#define FLIDEVICE_ENUMERATE_BY_SERIAL (0x8000)
#define FLIDOMAIN_OPTIONS_MASK (0xf000)


/**
   The frame type for an FLI CCD camera device.  Valid frame types are
   \texttt{FLI_FRAME_TYPE_NORMAL} and \texttt{FLI_FRAME_TYPE_DARK}.

   @see FLISetFrameType
*/
typedef long fliframe_t;

#define FLI_FRAME_TYPE_NORMAL (0)
#define FLI_FRAME_TYPE_DARK (1)
#define FLI_FRAME_TYPE_FLOOD (2)
#define FLI_FRAME_TYPE_RBI_FLUSH (FLI_FRAME_TYPE_FLOOD | FLI_FRAME_TYPE_DARK)

/**
   The gray-scale bit depth for an FLI camera device.  Valid bit
   depths are \texttt{FLI_MODE_8BIT} and \texttt{FLI_MODE_16BIT}.

   @see FLISetBitDepth
*/
typedef long flibitdepth_t;

#define FLI_MODE_8BIT (0)
#define FLI_MODE_16BIT (1)

/**
   Type used for shutter operations for an FLI camera device.  Valid
   shutter types are \texttt{FLI_SHUTTER_CLOSE},
   \texttt{FLI_SHUTTER_OPEN},
   \texttt{FLI_SHUTTER_EXTERNAL_TRIGGER},
   \texttt{FLI_SHUTTER_EXTERNAL_TRIGGER_LOW}, and
   \texttt{FLI_SHUTTER_EXTERNAL_TRIGGER_HIGH}.

   @see FLIControlShutter
*/
typedef long flishutter_t;

#define FLI_SHUTTER_CLOSE (0x0000)
#define FLI_SHUTTER_OPEN (0x0001)
#define FLI_SHUTTER_EXTERNAL_TRIGGER (0x0002)
#define FLI_SHUTTER_EXTERNAL_TRIGGER_LOW (0x0002)
#define FLI_SHUTTER_EXTERNAL_TRIGGER_HIGH (0x0004)
#define FLI_SHUTTER_EXTERNAL_EXPOSURE_CONTROL (0x0008)

/**
   Type used for background flush operations for an FLI camera device.  Valid
   bgflush types are \texttt{FLI_BGFLUSH_STOP} and
   \texttt{FLI_BGFLUSH_START}.

   @see FLIControlBackgroundFlush
*/
typedef long flibgflush_t;

#define FLI_BGFLUSH_STOP (0x0000)
#define FLI_BGFLUSH_START (0x0001)

/**
   Type used to determine which temperature channel to read.  Valid
   channel types are \texttt{FLI_TEMPERATURE_INTERNAL} and
   \texttt{FLI_TEMPERATURE_EXTERNAL}.

   @see FLIReadTemperature
*/
typedef long flichannel_t;

#define FLI_TEMPERATURE_INTERNAL (0x0000)
#define FLI_TEMPERATURE_EXTERNAL (0x0001)
#define FLI_TEMPERATURE_CCD (0x0000)
#define FLI_TEMPERATURE_BASE (0x0001)

/**
   Type specifying library debug levels.  Valid debug levels are
   \texttt{FLIDEBUG_NONE}, \texttt{FLIDEBUG_INFO},
   \texttt{FLIDEBUG_WARN}, and \texttt{FLIDEBUG_FAIL}.

   @see FLISetDebugLevel
*/
typedef long flidebug_t;
typedef long flimode_t;
typedef long flistatus_t;
typedef long flitdirate_t;
typedef long flitdiflags_t;

/* Status settings */
#define FLI_CAMERA_STATUS_UNKNOWN (0xffffffff)
#define FLI_CAMERA_STATUS_MASK (0x00000003)
#define FLI_CAMERA_STATUS_IDLE (0x00)
#define FLI_CAMERA_STATUS_WAITING_FOR_TRIGGER (0x01)
#define FLI_CAMERA_STATUS_EXPOSING (0x02)
#define FLI_CAMERA_STATUS_READING_CCD (0x03)
#define FLI_CAMERA_DATA_READY (0x80000000)

#define FLI_FOCUSER_STATUS_UNKNOWN (0xffffffff)
#define FLI_FOCUSER_STATUS_HOMING (0x00000004)
#define FLI_FOCUSER_STATUS_MOVING_IN (0x00000001)
#define FLI_FOCUSER_STATUS_MOVING_OUT (0x00000002)
#define FLI_FOCUSER_STATUS_MOVING_MASK (0x00000007)
#define FLI_FOCUSER_STATUS_HOME (0x00000080)
#define FLI_FOCUSER_STATUS_LIMIT (0x00000040)
#define FLI_FOCUSER_STATUS_LEGACY (0x10000000)

#define FLI_FILTER_WHEEL_PHYSICAL (0x100)
#define FLI_FILTER_WHEEL_VIRTUAL (0)
#define FLI_FILTER_WHEEL_LEFT (FLI_FILTER_WHEEL_PHYSICAL | 0x00)
#define FLI_FILTER_WHEEL_RIGHT (FLI_FILTER_WHEEL_PHYSICAL | 0x01)
#define FLI_FILTER_STATUS_MOVING_CCW (0x01)
#define FLI_FILTER_STATUS_MOVING_CW (0x02)
#define FLI_FILTER_POSITION_UNKNOWN (0xff)
#define FLI_FILTER_POSITION_CURRENT (0x200)
#define FLI_FILTER_STATUS_HOMING (0x00000004)
#define FLI_FILTER_STATUS_HOME (0x00000080)
#define FLI_FILTER_STATUS_HOME_LEFT (0x00000080)
#define FLI_FILTER_STATUS_HOME_RIGHT (0x00000040)
#define FLI_FILTER_STATUS_HOME_SUCCEEDED (0x00000008)

#define FLIDEBUG_NONE (0x00)
#define FLIDEBUG_INFO (0x01)
#define FLIDEBUG_WARN (0x02)
#define FLIDEBUG_FAIL (0x04)
#define FLIDEBUG_IO		(0x08)
#define FLIDEBUG_ALL (FLIDEBUG_INFO | FLIDEBUG_WARN | FLIDEBUG_FAIL)

#define FLI_IO_P0 (0x01)
#define FLI_IO_P1 (0x02)
#define FLI_IO_P2 (0x04)
#define FLI_IO_P3 (0x08)

#define FLI_FAN_SPEED_OFF (0x00)
#define FLI_FAN_SPEED_ON (0xffffffff)

#define FLI_EEPROM_USER (0x00)
#define FLI_EEPROM_PIXEL_MAP (0x01)

#define FLI_PIXEL_DEFECT_COLUMN (0x00)
#define FLI_PIXEL_DEFECT_CLUSTER (0x10)
#define FLI_PIXEL_DEFECT_POINT_BRIGHT (0x20)
#define FLI_PIXEL_DEFECT_POINT_DARK (0x30)

#ifndef LIBFLIAPI
#  ifdef _WIN32
#    ifdef _LIB
#      define LIBFLIAPI long __stdcall
#    else
#      ifdef _USRDLL
/* The module definition file precludes using __declspec(dllexport) */
/*#        define LIBFLIAPI __declspec(dllexport) long __stdcall    */
#      define LIBFLIAPI long __stdcall
#      else
#        define LIBFLIAPI __declspec(dllimport) long __stdcall
#      endif
#    endif
#  else
#    define LIBFLIAPI long
#  endif
#endif

/* Library API Function prototypes */

#ifdef __cplusplus
extern "C" {  // only need to export C interface if used by C++ source code
#endif

#ifdef WIN32
void __cdecl FLIDebug(int level, char *format, ...);
#else
void FLIDebug(int level, char *format, ...);
#endif

LIBFLIAPI FLIOpen(flidev_t *dev, char *name, flidomain_t domain);
LIBFLIAPI FLISetDebugLevel(char *host, flidebug_t level);
LIBFLIAPI FLIClose(flidev_t dev);
LIBFLIAPI FLIGetLibVersion(char* ver, size_t len);
LIBFLIAPI FLIGetModel(flidev_t dev, char* model, size_t len);
LIBFLIAPI FLIGetPixelSize(flidev_t dev, double *pixel_x, double *pixel_y);
LIBFLIAPI FLIGetHWRevision(flidev_t dev, long *hwrev);
LIBFLIAPI FLIGetFWRevision(flidev_t dev, long *fwrev);
LIBFLIAPI FLIGetArrayArea(flidev_t dev, long* ul_x, long* ul_y,
			  long* lr_x, long* lr_y);
LIBFLIAPI FLIGetVisibleArea(flidev_t dev, long* ul_x, long* ul_y,
			    long* lr_x, long* lr_y);
LIBFLIAPI FLISetExposureTime(flidev_t dev, long exptime);
LIBFLIAPI FLISetImageArea(flidev_t dev, long ul_x, long ul_y,
			  long lr_x, long lr_y);
LIBFLIAPI FLISetHBin(flidev_t dev, long hbin);
LIBFLIAPI FLISetVBin(flidev_t dev, long vbin);
LIBFLIAPI FLISetFrameType(flidev_t dev, fliframe_t frametype);
LIBFLIAPI FLICancelExposure(flidev_t dev);
LIBFLIAPI FLIGetExposureStatus(flidev_t dev, long *timeleft);
LIBFLIAPI FLISetTemperature(flidev_t dev, double temperature);
LIBFLIAPI FLIGetTemperature(flidev_t dev, double *temperature);
LIBFLIAPI FLIGetCoolerPower(flidev_t dev, double *power);
LIBFLIAPI FLIGrabRow(flidev_t dev, void *buff, size_t width);
LIBFLIAPI FLIExposeFrame(flidev_t dev);
LIBFLIAPI FLIFlushRow(flidev_t dev, long rows, long repeat);
LIBFLIAPI FLISetNFlushes(flidev_t dev, long nflushes);
LIBFLIAPI FLISetBitDepth(flidev_t dev, flibitdepth_t bitdepth);
LIBFLIAPI FLIReadIOPort(flidev_t dev, long *ioportset);
LIBFLIAPI FLIWriteIOPort(flidev_t dev, long ioportset);
LIBFLIAPI FLIConfigureIOPort(flidev_t dev, long ioportset);
LIBFLIAPI FLILockDevice(flidev_t dev);
LIBFLIAPI FLIUnlockDevice(flidev_t dev);
LIBFLIAPI FLIControlShutter(flidev_t dev, flishutter_t shutter);
LIBFLIAPI FLIControlBackgroundFlush(flidev_t dev, flibgflush_t bgflush);
LIBFLIAPI FLISetDAC(flidev_t dev, unsigned long dacset);
LIBFLIAPI FLIList(flidomain_t domain, char ***names);
LIBFLIAPI FLIFreeList(char **names);

LIBFLIAPI FLIGetFilterName(flidev_t dev, long filter, char *name, size_t len);
LIBFLIAPI	FLISetActiveWheel(flidev_t dev, long wheel);
LIBFLIAPI	FLIGetActiveWheel(flidev_t dev, long *wheel);

LIBFLIAPI FLISetFilterPos(flidev_t dev, long filter);
LIBFLIAPI FLIGetFilterPos(flidev_t dev, long *filter);
LIBFLIAPI FLIGetFilterCount(flidev_t dev, long *filter);

LIBFLIAPI FLIStepMotor(flidev_t dev, long steps);
LIBFLIAPI FLIStepMotorAsync(flidev_t dev, long steps);
LIBFLIAPI FLIGetStepperPosition(flidev_t dev, long *position);
LIBFLIAPI FLIGetStepsRemaining(flidev_t dev, long *steps);
LIBFLIAPI FLIHomeFocuser(flidev_t dev);
LIBFLIAPI FLICreateList(flidomain_t domain);
LIBFLIAPI FLIDeleteList(void);
LIBFLIAPI FLIListFirst(flidomain_t *domain, char *filename,
		      size_t fnlen, char *name, size_t namelen);
LIBFLIAPI FLIListNext(flidomain_t *domain, char *filename,
		      size_t fnlen, char *name, size_t namelen);
LIBFLIAPI FLIReadTemperature(flidev_t dev,
					flichannel_t channel, double *temperature);
LIBFLIAPI FLIGetFocuserExtent(flidev_t dev, long *extent);
LIBFLIAPI FLIUsbBulkIO(flidev_t dev, int ep, void *buf, long *len);
LIBFLIAPI FLIGetDeviceStatus(flidev_t dev, long *status);
LIBFLIAPI FLIGetCameraModeString(flidev_t dev, flimode_t mode_index, char *mode_string, size_t siz);
LIBFLIAPI FLIGetCameraMode(flidev_t dev, flimode_t *mode_index);
LIBFLIAPI FLISetCameraMode(flidev_t dev, flimode_t mode_index);
LIBFLIAPI FLIHomeDevice(flidev_t dev);
LIBFLIAPI FLIGrabFrame(flidev_t dev, void* buff, size_t buffsize, size_t* bytesgrabbed);
LIBFLIAPI FLISetTDI(flidev_t dev, flitdirate_t tdi_rate, flitdiflags_t flags);
LIBFLIAPI FLIGrabVideoFrame(flidev_t dev, void *buff, size_t size);
LIBFLIAPI FLIStopVideoMode(flidev_t dev);
LIBFLIAPI FLIStartVideoMode(flidev_t dev);
LIBFLIAPI FLIGetSerialString(flidev_t dev, char* serial, size_t len);
LIBFLIAPI FLIEndExposure(flidev_t dev);
LIBFLIAPI FLITriggerExposure(flidev_t dev);
LIBFLIAPI FLISetFanSpeed(flidev_t dev, long fan_speed);
LIBFLIAPI FLISetVerticalTableEntry(flidev_t dev, long index, long height, long bin, long mode);
LIBFLIAPI FLIGetVerticalTableEntry(flidev_t dev, long index, long *height, long *bin, long *mode);
LIBFLIAPI FLIGetReadoutDimensions(flidev_t dev, long *width, long *hoffset, long *hbin, long *height, long *voffset, long *vbin);
LIBFLIAPI FLIEnableVerticalTable(flidev_t dev, long width, long offset, long flags);
LIBFLIAPI FLIReadUserEEPROM(flidev_t dev, long loc, long address, long length, void *rbuf);
LIBFLIAPI FLIWriteUserEEPROM(flidev_t dev, long loc, long address, long length, void *wbuf);

#ifdef __cplusplus
}
#endif

#endif /* _LIBFLI_H_ */
