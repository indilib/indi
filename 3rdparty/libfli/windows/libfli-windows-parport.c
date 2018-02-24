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

#include <windows.h>
#include <stdio.h>
#include <winioctl.h>
#include <errno.h>
//#include <conio.h>

#include "../libfli-libfli.h"
#include "../libfli-debug.h"
#include "../libfli-camera.h"

#pragma intrinsic(_inp,_inpw,_inpd,_outp,_outpw,_outpd)

#define CCDPAR_TYPE ((ULONG) 43000)
#define CCDPAR_IOCTL_BASE ((USHORT) 2833)

#define IOCTL_SET_READ_TIMEOUT CTL_CODE(CCDPAR_TYPE, CCDPAR_IOCTL_BASE+1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_WRITE_TIMEOUT CTL_CODE(CCDPAR_TYPE, CCDPAR_IOCTL_BASE+2, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_DIRECTION_TIMEOUT CTL_CODE(CCDPAR_TYPE, CCDPAR_IOCTL_BASE+3, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_TIMEOUT_COUNT CTL_CODE(CCDPAR_TYPE, CCDPAR_IOCTL_BASE+4, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define DPORT (io->port+0x000) /* Data port */
#define SPORT (io->port+0x001)
#define CPORT (io->port+0x002)
#define FPORT (io->port+0x400)
#define BPORT (io->port+0x401)
#define EPORT (io->port+0x402)

#define DIR_FORWARD 0x01
#define DIR_REVERSE 0x02

#define C2 0x04 /* Bit 2 on the control port */
#define S5 0x20 /* Bit 5 on the status port */
#define S3 0x08 /* Bit 3 on the status port */
#define C5 0x20 /* Bit 5 on the control port */

static long ECPSetReverse(flidev_t dev)
{
  unsigned char byte;
  long timeout;

	fli_io_t *io = DEVICE->io_data;
	flicamdata_t *cam = DEVICE->device_data;

  if(io->dir == DIR_REVERSE)
		return 0;
  
  byte = _inp(EPORT);				/* Switch to PS/2 mode */
  byte &= ~0xe0;
  byte |= 0x20;
  _outp(EPORT, byte);

  /* Set reverse mode */
  byte = _inp(CPORT);
  byte |= C5;								/* Program for input */
  _outp(CPORT, byte);
  byte &= ~C2;							/* Assert nReverseReq */

	if(io->notecp > 0)
		byte |= 0x02;
  _outp(CPORT, byte);
  
  timeout = 0;
  while((_inp(SPORT) & S5) > 0) /* Wait for nAckReverse */
  {
    timeout++;
    if(timeout > cam->dirto)
    {
			debug(FLIDEBUG_FAIL, "ECP: Write timeout during reverse.");
      return -EIO;
    }
  }

  byte = _inp(EPORT); /* Switch to ECP mode */
  byte &= ~0xe0;

	if(io->notecp > 0)
		byte |= 0x20;
	else
		byte |= 0x60;
  _outp(EPORT, byte);
  
  io->dir = DIR_REVERSE; /* We are now set in reverse transfer mode */
  return 0;
}

long ECPReadByte(flidev_t dev, unsigned char *byte, unsigned long *timeout)
{
  unsigned char pdata;
	fli_io_t *io = DEVICE->io_data;
	flicamdata_t *cam = DEVICE->device_data;

  if(ECPSetReverse(dev) != 0)
	{
		return -EIO;
	}

	if(io->notecp > 0)
	{
		pdata = _inp(CPORT);
		while((_inp(SPORT) & 0x40) > 0)
		{
			if((*timeout) == 0)
			{
				debug(FLIDEBUG_FAIL, "ECP: Timeout during read.");
		   return -EIO;
		 }
		 (*timeout)--;
		}
		*byte = _inp(DPORT);
		pdata &= ~0x02;
		outp(CPORT,pdata);
		while((_inp(SPORT) & 0x40) == 0)
		{
			if((*timeout) == 0)
			{
				debug(FLIDEBUG_FAIL, "ECP: Timeout during read.");
		   return -EIO;
		 }
		 (*timeout)--;
		}
		pdata |= 0x02;
		outp(CPORT,pdata);
	}
	else
	{
		while(((pdata = _inp(EPORT)) & 0x01) > 0)
		{
			if((*timeout) == 0)
			{
				debug(FLIDEBUG_FAIL, "ECP: Timeout during read.");
				return -EIO;
			}
			(*timeout)--;
		}
		*byte = _inp(FPORT);
	}
  return 0;
}

long ECPReadWord(flidev_t dev, unsigned short *word, unsigned long *timeout)
{
  unsigned char byte;

  if(ECPReadByte(dev, &byte, timeout) != 0)
  {
		debug(FLIDEBUG_FAIL, "ECP: Error during read (high byte).");
		return -EIO;
  }

  *word = (unsigned short) byte;
  if(ECPReadByte(dev, &byte, timeout) != 0)
  {
		debug(FLIDEBUG_FAIL, "ECP: Error during read (low byte).");
		return -EIO;
  }
  *word += (byte << 8);
  return 0;
}

static long ECPSetForward(flidev_t dev)
{
  unsigned char byte;
  long timeout;
 	fli_io_t *io = DEVICE->io_data;
	flicamdata_t *cam = DEVICE->device_data;

  if(io->dir == DIR_FORWARD)
		return 0;

  byte = _inp(EPORT); /* Switch to PS/2 mode */
  byte &= ~0xe0;
  byte |= 0x20;
  _outp(EPORT, byte);

  /* Switch to forward mode */
  byte = _inp(CPORT);
  byte |= C2;                /* Deassert nReverseReq */
	if(io->notecp > 0)
		byte &= ~0x03;
  _outp(CPORT, byte);
  
  timeout=0;
  while((_inp(SPORT) & S5) == 0) /* Wait for nAckReverse */ 
  {
    timeout++;
    if(timeout > cam->dirto)
    {
			debug(FLIDEBUG_FAIL, "ECP: Error setting forward direction.");
      return -EIO;
    }
  }
  
  byte &= ~C5;               /* Set for forward transfers */
  _outp(CPORT, byte);

  byte=_inp(EPORT); /* Switch back to ECP mode */
  byte &= ~0xe0;
	if(io->notecp > 0)
		byte |= 0x20;
	else
		byte |= 0x60;
  _outp(EPORT, byte);

  io->dir = DIR_FORWARD;
  return 0;
}

long ECPWriteByte(flidev_t dev, unsigned char byte, unsigned long *timeout)
{
	unsigned char pdata;
	fli_io_t *io = DEVICE->io_data;
	flicamdata_t *cam = DEVICE->device_data;

  if (ECPSetForward(dev) != 0)
	{
		return -EIO;
	}
  
	if (io->notecp == TRUE)
	{
		outp(DPORT,byte);
		pdata = _inp(CPORT);
		pdata |= 0x01;
		outp(CPORT,pdata);
		while((_inp(SPORT) & 0x80) > 0)
		{
			if((*timeout) == 0)
			{
				debug(FLIDEBUG_FAIL, "ECP: Write Timeout.");
				return -EIO;
			}
			(*timeout)--;
		}
		pdata &= ~0x01;
		outp(CPORT,pdata);
		while((_inp(SPORT) & 0x80) == 0)
		{
			if((*timeout) == 0)
			{
				debug(FLIDEBUG_FAIL, "ECP: Write Timeout.");
				return -EIO;
			}
			(*timeout)--;
		}
	}
	else
	{
		outp(FPORT, byte);
		/* Check FIFO status */
		while((_inp(EPORT) & 0x01) == 0)
		{
			if((*timeout) == 0)
			{
				debug(FLIDEBUG_FAIL, "ECP: Write Timeout.");
				return -EIO;
			}
			(*timeout)--;
		}
	}
  return 0;
}

long ECPWrite(flidev_t dev, void *buffer, long length)
{
	int i = 0;
	long retval = 0;
	unsigned long to;
	fli_io_t *io = DEVICE->io_data;
	flicamdata_t *cam = DEVICE->device_data;
	fli_sysinfo_t *sys= DEVICE->sys_data;

	if(length == 0)
		return 0;

	debug(FLIDEBUG_INFO, "Write: %02x [%02x %02x]", length,
		((unsigned char *) buffer)[0], ((unsigned char *) buffer)[1]);

	if(sys->OS == VER_PLATFORM_WIN32_NT)
	{
		if(WriteFile(io->fd, buffer, length, &retval, NULL) == FALSE)
		{
			debug(FLIDEBUG_WARN, "Write failed: %d", GetLastError());
		}
	}
	else
	{
		while(i < length)
		{
			to = cam->writeto;
			if((retval = ECPWriteByte(dev, ((unsigned char *) buffer)[i], &to)) != 0)
			{
				debug(FLIDEBUG_FAIL, "ECP: Error during write.");
				break;
			}
			i++;
		}
		retval = i;
	}
	return retval;
}

long ECPWriteWord(flidev_t dev, unsigned short word, unsigned long *timeout)
{
  if(ECPWriteByte(dev, (unsigned char) (word & 0x00ff), timeout)==FALSE)
  {
		debug(FLIDEBUG_FAIL, "ECP: Write timeout on low byte.");
    return -EIO;
  }  
  if(ECPWriteByte(dev, (unsigned char) (word >> 8), timeout)==FALSE)
  {
		debug(FLIDEBUG_FAIL, "ECP: Write timeout on high byte.");
    return -EIO;
  }
  return 0;
}

long ECPRead(flidev_t dev, void *buffer, long length)
{
	int i = 0;
	DWORD retval = 0;
	unsigned long to;
	fli_io_t *io = DEVICE->io_data;
	flicamdata_t *cam = DEVICE->device_data;
	fli_sysinfo_t *sys= DEVICE->sys_data;

	if (length == 0)
		return 0;

	debug(FLIDEBUG_INFO, " Read: %02x", length);
	if(sys->OS == VER_PLATFORM_WIN32_NT)
	{
		if(ReadFile(io->fd, buffer, length, &retval, NULL) == FALSE)
		{
			debug(FLIDEBUG_WARN, "Read failed: %d", GetLastError());
		}
		debug(FLIDEBUG_INFO, " Read: %02x [%02x %02x]", retval, 
			((unsigned char *) buffer)[0], ((unsigned char *) buffer)[1]);
	}
	else
	{
		while(i < length)
		{
			to = cam->readto;
			if((retval = ECPReadByte(dev, &((unsigned char *) buffer)[i], &to)) != 0)
			{
				debug(FLIDEBUG_FAIL, "ECP: Error during read.");
				break;
			}
			i++;
		}
		retval = i;
	}

	return retval;
}

long ECPInit(flidev_t dev)
{
	fli_io_t *io = DEVICE->io_data;
	fli_sysinfo_t *sys= DEVICE->sys_data;
	long retval = 0;

	if(sys->OS == VER_PLATFORM_WIN32_NT)
	{

	}
	else
	{
		if (io->port == 0)
		{
			retval = -EINVAL;
		}
		else
		{
			io->dir = 0;
			io->notecp = 0;

			/* Let's Preserve the context */
			io->portval[0]=_inp(DPORT);
			io->portval[1]=_inp(SPORT);
			io->portval[2]=_inp(CPORT);
			io->portval[3]=_inp(FPORT);
			io->portval[4]=_inp(BPORT);
			io->portval[5]=_inp(EPORT);

			/* Enable the ECP port */
			_outp(CPORT, 0x00);
			_outp(EPORT, 0x24); /* Select ECP mode */
			retval = ECPSetForward(dev);
		}
	}
	return retval;
}

long ECPClose(flidev_t dev)
{
	fli_io_t *io = DEVICE->io_data;
	fli_sysinfo_t *sys = DEVICE->sys_data;
	long retval = 0;

	if(sys->OS == VER_PLATFORM_WIN32_NT)
	{

	}
	else
	{
		_outp(EPORT, io->portval[5]);
		_outp(BPORT, io->portval[4]);
		_outp(FPORT, io->portval[3]);
		_outp(CPORT, io->portval[2]);
		_outp(SPORT, io->portval[1]);
		_outp(DPORT, io->portval[0]);
	}

	return retval;
}

long parportio(flidev_t dev, void *buf, long *wlen, long *rlen)
{
  fli_io_t *io;
  flicamdata_t *cam;
	fli_sysinfo_t *sys;
  int err = 0, locked = 0;
  long org_wlen = *wlen, org_rlen = *rlen;
	unsigned long rto, dto, wto;
	DWORD bytes;

  io = DEVICE->io_data;
  cam = DEVICE->device_data;
	sys = DEVICE->sys_data;

	rto = cam->readto;
	wto = cam->writeto;
	dto = cam->dirto;

  if ((err = fli_lock(dev)))
  {
    debug(FLIDEBUG_WARN, "Lock failed");
    goto done;
  }

  locked = 1;

	if (sys->OS == VER_PLATFORM_WIN32_NT)
	{
		if (DeviceIoControl(io->fd, IOCTL_SET_WRITE_TIMEOUT, &wto,
      sizeof(unsigned long), NULL, 0, &bytes, NULL) == FALSE)
	  {
	   err = -EIO;
	   goto done;
		}

		if (DeviceIoControl(io->fd, IOCTL_SET_DIRECTION_TIMEOUT, &dto,
    sizeof(unsigned long), NULL, 0, &bytes, NULL) == FALSE)
		{
			err = -EIO;
			goto done;
		}

//		rto = 1000000;
		if (DeviceIoControl(io->fd, IOCTL_SET_READ_TIMEOUT, &rto,
     sizeof(unsigned long), NULL, 0, &bytes, NULL) == FALSE)
		{
			err = -EIO;
			goto done;
		}
	}

	if (*wlen > 0)
	{
		if ((*wlen = ECPWrite(dev, buf, *wlen)) != org_wlen)
		{
			debug(FLIDEBUG_WARN, "write failed, only %d of %d bytes written",
			*wlen, org_wlen);
			err = -EIO;
			goto done;
		}
	}

  if (*rlen > 0)
  {
    if ((*rlen = ECPRead(dev, buf, *rlen)) != org_rlen)
    {
      debug(FLIDEBUG_WARN, "read failed, only %d of %d bytes read",
	    *rlen, org_rlen);
      err = -EIO;
      goto done;
    }
  }

 done:

  if (locked)
  {
    int r;

    if ((r = fli_unlock(dev)))
      debug(FLIDEBUG_WARN, "Unlock failed");
    if (err == 0)
      err = r;
  }

  return err;
}

