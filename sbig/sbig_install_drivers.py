#!/usr/bin/env python

# Name: 	sbig_install_drivers.py
# Version: 	1.2

# This script installs all the necessary files so, that the dynamically 
# plugged SBIG USB CCD cameras should run under Linux kernels >= 2.6.13.
#
# Author:
# Jan Soldan (jsoldan AT asu DOT cas DOT cz)
# Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

# History:

# 2007-12-22 - Added x86_64 support, shared library naming made consistent with SUSE policies.
# 2007-12-20 - Step one enabled again for RPM/Deb distirbution. (JM)
# 2007-11-04 - revision.
# 2007-08-08 - step one disabled.
# 2006-08-23 - move all firmware files to the /lib/firmware directory (JS)
# 2006-08-20 - the version 1.0 released. (JS)
#====================================================================
import sys, os, shutil

# Build Dir should be passed from SPEC
build_dir = sys.argv[1]
# Arch parameter should be passed from SPEC
arch_param = sys.argv[2]
# version
version = sys.argv[3]

# 1. Copy libsbigudrv.so to /usr/lib or /usr/lib64 directory depending on architecture:

# We're building as nonroot, so create directories if they don't exist as we go along
if (os.path.exists(build_dir + "/etc/udev/rules.d") == False):
	os.makedirs(build_dir + "/etc/udev/rules.d")
if (os.path.exists(build_dir + "/lib") == False):
	os.mkdir(build_dir + "/lib")
	
if (arch_param.endswith("64")):
	fileName = "libsbigudrv64.so"
	arch_dir = "/usr/lib64"
else:
	fileName = "libsbigudrv32.so"
	arch_dir = "/usr/lib"

# Target lib directory
target_lib_dir = build_dir + arch_dir

if (os.path.exists(target_lib_dir) == False):
	os.makedirs(target_lib_dir)
	
# SO, major lib, and major minor lib names in accord to SUSE library naming policies.
so_lib = "/libsbigudrv.so"
major_lib = so_lib + "." + version[0]
major_minor_lib = so_lib + "." + version

target_lib = target_lib_dir + major_minor_lib

print "Architecture: " + arch_param + " - Library: " + fileName

shutil.copyfile(fileName, target_lib)
shutil.copymode(fileName, target_lib)

# Install symbolic links
os.popen("ln -sf " + arch_dir + major_lib + " " + target_lib_dir + so_lib)
os.popen("ln -sf " + arch_dir + major_minor_lib + " " + target_lib_dir + major_lib)

# Add the installed library to INSTALLED_FILES
os.popen("echo " + arch_dir + major_minor_lib + " >> INSTALLED_FILES")
os.popen("echo " + arch_dir + so_lib + " >> INSTALLED_FILES")
os.popen("echo " + arch_dir + major_lib + " >> INSTALLED_FILES")

# 2. Copy 10-sbig.rules file to the /etc/udev/rules.d directory:
dirName  = build_dir + "/etc/udev/rules.d/"
fileName = "10-sbig.rules"
shutil.copyfile(fileName, dirName + fileName)

# 3. Copy *.hex, *.bin and sbig_dev_permission.py files to the /lib/firmware directory.
bdir = 0;
filenames  = os.listdir(build_dir + "/lib")
for filename in filenames:
	if filename == "firmware":
  		bdir = 1

dirName = build_dir + "/lib/firmware/"
if bdir == 0:
		os.mkdir(dirName)

fileName = "sbigfcam.hex"
shutil.copyfile(fileName, dirName + fileName)
fileName = "sbiglcam.hex"
shutil.copyfile(fileName, dirName + fileName)
fileName = "sbigucam.hex"
shutil.copyfile(fileName, dirName + fileName)
fileName = "stfga.bin"
shutil.copyfile(fileName, dirName + fileName)
fileName = "sbig_load.sh"
shutil.copyfile(fileName, dirName + fileName)
shutil.copymode(fileName, dirName + fileName)
fileName = "sbig_perm.sh"
shutil.copyfile(fileName, dirName + fileName)
shutil.copymode(fileName, dirName + fileName)

print "SBIG linux universal driver installation OK."		
#====================================================================
