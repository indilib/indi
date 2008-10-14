#!/bin/sh 
#
#############################################################################
#
# Jasem Mutlaq (mutlaqja AT ikarustech DOT com) 
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
###
# Get users group GID 
#
group_line=`grep users /etc/group`
gid=`expr match "$group_line" 'users:x:\([0-9]*\):'`

####
# Mount /deb/bus/usb with effective gid
#
mount sbig_ccd /dev/bus/usb -t usbfs -o defaults,devmode=660,devgid=100

