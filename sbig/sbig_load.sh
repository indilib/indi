#!/bin/sh 
#
#############################################################################
#
# Copyright Erik Ridderby, 2007
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#############################################################################
#
# This program is a wrapper so that the fxload-based udev-rules to load the 
# MidiSport can be functional using later versions of UDEV. It soves the 
# problem with fxload uses a path to the device in the /proc/bus/usb hieracy
# but the information avaliable is the device name or the path in the /sys/ 
# file system. This script takes the device name usbdev_X_YY and converts it
# to a path within the /proc system.


# Jasem Mutlaq: Modified this file to work with SBIG CCDs
#
# Sanity check.....
#

if [ ! $# -eq  2 ]
then
	exit -1
fi

###
# Get busnum and devnum, /dev/bus/usb/busnum/devnum. This is a hack until udev developers decide to give us a path we can use.
#

busnum=`expr match "$2" 'usbdev\([0-9]\)\.'`
devnum=`expr match "$2" '[a-z]*[0-9]\.\([0-9]*\)'`
printf -v busnum "%03d" $busnum
printf -v devnum "%03d" $devnum

####
# Load the firmware to the device
#

/sbin/fxload -I $1 -D /dev/bus/usb/$busnum/$devnum

