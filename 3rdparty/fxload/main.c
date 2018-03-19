/*
 * Copyright (c) 2001 Stephen Williams (steve@icarus.com)
 * Copyright (c) 2001-2002 David Brownell (dbrownell@users.sourceforge.net)
 * Copyright (c) 2008 Roger Williams (rawqux@users.sourceforge.net)
 * Copyright (c) 2012 Steve Magnani (steve@digidescorp.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/*
 * This program supports loading firmware into a target USB device
 * that is discovered and referenced by the hotplug usb agent. It can
 * also do other useful things, like set the permissions of the device
 * and create a symbolic link for the benefit of applications that are
 * looking for the device.
 *
 *     -I <path>       -- Download this firmware (intel hex)
 *     -t <type>       -- uController type: an21, fx, fx2, fx2lp, fx3
 *     -s <path>       -- use this second stage loader
 *     -c <byte>       -- Download to EEPROM, with this config byte
 *
 *     -L <path>       -- Create a symbolic link to the device.
 *     -m <mode>       -- Set the permissions on the device after download.
 *     -D <path>       -- Use this device, instead of $DEVICE
 *
 *     -V              -- Print version ID for program
 *
 * This program is intended to be started by hotplug scripts in
 * response to a device appearing on the bus. It therefore also
 * expects these environment variables which are passed by hotplug to
 * its sub-scripts:
 *
 *     DEVICE=<path>
 *         This is the path to the device is /proc/bus/usb. It is the
 *         complete path to the device, that I can pass to open and
 *         manipulate as a USB device.
 */

# include  <stdlib.h>
# include  <stdio.h>
# include  <getopt.h>
# include  <string.h>

# include  <sys/types.h>
# include  <sys/stat.h>
# include  <fcntl.h>
# include  <unistd.h>

# include  "ezusb.h"

#ifndef	FXLOAD_VERSION
#	define FXLOAD_VERSION (__DATE__ " (development)")
#endif

#include <errno.h>
#include <syslog.h>
#include <stdarg.h>


static int dosyslog=0;

void logerror(const char *format, ...)
    __attribute__ ((format (__printf__, 1, 2)));

void logerror(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    if(dosyslog)
	vsyslog(LOG_ERR, format, ap);
    else
	vfprintf(stderr, format, ap);
    va_end(ap);
}

int main(int argc, char*argv[])
{
      const char	*link_path = 0;
      const char	*ihex_path = 0;
      const char	*device_path = getenv("DEVICE");
      const char	*type = 0;
      const char	*stage1 = 0;
      mode_t		mode = 0;
      int		opt;
      int		config = -1;

      while ((opt = getopt (argc, argv, "2vV?D:I:L:c:lm:s:t:")) != EOF)
      switch (opt) {

	  case '2':		// original version of "-t fx2"
	    type = "fx2";
	    break;

	  case 'D':
	    device_path = optarg;
	    break;

	  case 'I':
	    ihex_path = optarg;
	    break;

	  case 'L':
	    link_path = optarg;
	    break;

	  case 'V':
	    puts (FXLOAD_VERSION);
	    return 0;

	  case 'c':
	    config = strtoul (optarg, 0, 0);
	    if (config < 0 || config > 255) {
		logerror("illegal config byte: %s\n", optarg);
		goto usage;
	    }
	    break;

	  case 'l':
	    openlog(argv[0], LOG_CONS|LOG_NOWAIT|LOG_PERROR, LOG_USER);
	    dosyslog=1;
	    break;

	  case 'm':
	    mode = strtoul(optarg,0,0);
	    mode &= 0777;
	    break;

	  case 's':
	    stage1 = optarg;
	    break;

	  case 't':
	    if (strcmp (optarg, "an21")		// original AnchorChips parts
		    && strcmp (optarg, "fx")	// updated Cypress versions
		    && strcmp (optarg, "fx2")	// Cypress USB 2.0 versions
		    && strcmp (optarg, "fx2lp")	// updated FX2
		    && strcmp (optarg, "fx3")	// Cypress USB 3.0 versions
		    ) {
		logerror("illegal microcontroller type: %s\n", optarg);
		goto usage;
	    }
	    type = optarg;
	    break;

	  case 'v':
	    verbose++;
	    break;

	  case '?':
	  default:
	    goto usage;

      }

      if (config >= 0) {
	    if (type == 0) {
		logerror("must specify microcontroller type %s",
				"to write EEPROM!\n");
		goto usage;
	    }
	    if (!stage1 || !ihex_path) {
		logerror("need 2nd stage loader and firmware %s",
				"to write EEPROM!\n");
		goto usage;
	    }
	    if (link_path || mode) {
		logerror("links and modes not set up when writing EEPROM\n");
		goto usage;
	    }
      }

      if (!device_path) {
	    logerror("no device specified!\n");
usage:
	    fputs ("usage: ", stderr);
	    fputs (argv [0], stderr);
	    fputs (" [-vV] [-l] [-t type] [-D devpath]\n", stderr);
	    fputs ("\t\t[-I firmware_hexfile] ", stderr);
	    fputs ("[-s loader] [-c config_byte]\n", stderr);
	    fputs ("\t\t[-L link] [-m mode]\n", stderr);
	    fputs ("... [-D devpath] overrides DEVICE= in env\n", stderr);
	    fputs ("... device types:  one of an21, fx, fx2, fx2lp, fx3\n", stderr);
	    fputs ("... at least one of -I, -L, -m is required\n", stderr);
	    return -1;
      }

      if (ihex_path) {
	    int fd = open(device_path, O_RDWR);
	    int status;

	    if (fd == -1) {
		logerror("%s : %s\n", strerror(errno), device_path);
		return -1;
	    }

	    if (type == 0) {
		type = "fx";	/* an21-compatible for most purposes */
	    }

	    if (verbose)
		logerror("microcontroller type: %s\n", type);

	    if (stage1) {
		/* first stage:  put loader into internal memory */
		if (verbose)
		    logerror("1st stage:  load 2nd stage loader\n");
		status = ezusb_load_ram (fd, stage1, type, 0);
		if (status != 0)
		    return status;

		/* second stage ... write either EEPROM, or RAM.  */
		if (config >= 0)
		    status = ezusb_load_eeprom (fd, ihex_path, type, config);
		else
		    status = ezusb_load_ram (fd, ihex_path, type, 1);
		if (status != 0)
		    return status;
	    } else {
		/* single stage, put into internal memory */
		if (verbose)
		    logerror("single stage:  load on-chip memory\n");
		status = ezusb_load_ram (fd, ihex_path, type, 0);
		if (status != 0)
		    return status;
	    }

	    /* some firmware won't renumerate, but typically it will.
	     * link and chmod only make sense without renumeration...
	     */
      }

      if (link_path) {
	    int rc = unlink(link_path);
	    rc = symlink(device_path, link_path);
	    if (rc == -1) {
		  logerror("%s : %s\n", strerror(errno), link_path);
		  return -1;
	    }
      }

      if (mode != 0) {
	    int rc = chmod(device_path, mode);
	    if (rc == -1) {
		  logerror("%s : %s\n", strerror(errno), link_path);
		  return -1;
	    }
      }

      if (!ihex_path && !link_path && !mode) {
	    logerror("missing request! (firmware, link, or mode)\n");
	    return -1;
      }

      return 0;
}


/*
 * $Log: main.c,v $
 * Revision 1.10  2008/10/13 21:25:29  dbrownell
 * Whitespace fixes.
 *
 * Revision 1.9  2008/10/13 21:23:23  dbrownell
 * From Roger Williams <roger@qux.com>:  FX2LP support
 *
 * Revision 1.8  2005/01/11 03:58:02  dbrownell
 * From Dirk Jagdmann <doj@cubic.org>:  optionally output messages to
 * syslog instead of stderr.
 *
 * Revision 1.7  2002/04/12 00:28:22  dbrownell
 * support "-t an21" to program EEPROMs for those microcontrollers
 *
 * Revision 1.6  2002/04/02 05:26:15  dbrownell
 * version display now noiseless (-V);
 * '-?' (usage info) convention now explicit
 *
 * Revision 1.5  2002/02/26 20:10:28  dbrownell
 * - "-s loader" option for 2nd stage loader
 * - "-c byte" option to write EEPROM with 2nd stage
 * - "-V" option to dump version code
 *
 * Revision 1.4  2002/01/17 14:19:28  dbrownell
 * fix warnings
 *
 * Revision 1.3  2001/12/27 17:54:04  dbrownell
 * forgot an important character :)
 *
 * Revision 1.2  2001/12/27 17:43:29  dbrownell
 * fail on firmware download errors; add "-v" flag
 *
 * Revision 1.1  2001/06/12 00:00:50  stevewilliams
 *  Added the fxload program.
 *  Rework root makefile and hotplug.spec to install in prefix
 *  location without need of spec file for install.
 *
 */
