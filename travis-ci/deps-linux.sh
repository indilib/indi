#!/bin/bash

set -x -e

sudo apt-get -qq update

sudo apt-get -q -y install \
 cdbs \
 curl \
 dcraw \
 fakeroot \
 libboost-dev \
 libboost-regex-dev \
 libcfitsio3-dev \
 libftdi-dev \
 libdc1394-22-dev \
 libgphoto2-dev \
 libgps-dev \
 libgsl0-dev \
 libjpeg-dev \
 libnova-dev \
 libopenal-dev \
 libraw-dev \
 libusb-1.0-0-dev \
 librtlsdr-dev \
 libfftw3-dev \
 wget

 #To get the up to date FFMpeg Libraries
 sudo add-apt-repository -y ppa:jonathonf/ffmpeg-3
 sudo apt-get -qq update
 sudo apt-get -q -y install \
 libavcodec-dev \
 libavdevice-dev \
 libavformat-dev \
 libswscale-dev \
 libavutil-dev \

# Install libftdi for MGen driver
CURDIR="$(pwd)"
sudo apt-get -q -y install libusb-1.0-0-dev libconfuse-dev python3-all-dev doxygen libboost-test-dev python-all-dev swig g++
cd ~
mkdir ftdi_src
cd ftdi_src
wget http://archive.ubuntu.com/ubuntu/pool/universe/libf/libftdi1/libftdi1_1.2-5build1.dsc
wget http://archive.ubuntu.com/ubuntu/pool/universe/libf/libftdi1/libftdi1_1.2.orig.tar.bz2
wget http://archive.ubuntu.com/ubuntu/pool/universe/libf/libftdi1/libftdi1_1.2-5build1.debian.tar.xz
dpkg-source -x libftdi1_1.2-5build1.dsc
cd libftdi1-1.2
# Don't build the unnecessary packages
truncate -s 1668 debian/control
truncate -s 1819 debian/rules
dpkg-buildpackage -rfakeroot -nc -b -j6 -d -uc -us
cd ..
sudo dpkg -i libftdi1-2_1.2-5build1_amd64.deb
sudo dpkg -i libftdi1-dev_1.2-5build1_amd64.deb
cd $CURDIR

# Install soapysdr for liblimesuite
CURDIR="$(pwd)"
sudo apt-get -q -y install cli-common-dev
cd ~
mkdir soapysdr_src
cd soapysdr_src
wget http://archive.ubuntu.com/ubuntu/pool/universe/s/soapysdr/soapysdr_0.6.0-2.dsc
wget http://archive.ubuntu.com/ubuntu/pool/universe/s/soapysdr/soapysdr_0.6.0.orig.tar.gz
wget http://archive.ubuntu.com/ubuntu/pool/universe/s/soapysdr/soapysdr_0.6.0-2.debian.tar.xz
dpkg-source -x soapysdr_0.6.0-2.dsc
cd soapysdr-0.6.0
dpkg-buildpackage -rfakeroot -nc -b -j6 -d -uc -us
cd ..
sudo dpkg -i libsoapysdr0.6_0.6.0-2_amd64.deb
sudo dpkg -i libsoapysdr-dev_0.6.0-2_amd64.deb
cd $CURDIR

# Install liblimesuite for limeSDR driver
CURDIR="$(pwd)"
sudo apt-get -q -y install libwxgtk3.0-dev libi2c-dev freeglut3-dev libglew-dev libsqlite3-dev
cd ~
mkdir limesuite_src
cd limesuite_src
wget http://archive.ubuntu.com/ubuntu/pool/universe/l/limesuite/limesuite_17.06.0+dfsg-1.dsc
wget http://archive.ubuntu.com/ubuntu/pool/universe/l/limesuite/limesuite_17.06.0+dfsg.orig.tar.gz
wget http://archive.ubuntu.com/ubuntu/pool/universe/l/limesuite/limesuite_17.06.0+dfsg-1.debian.tar.xz
dpkg-source -x limesuite_17.06.0+dfsg-1.dsc
cd limesuite-17.06.0+dfsg
dpkg-buildpackage -rfakeroot -nc -b -j6 -d -uc -us
cd ..
sudo dpkg -i liblimesuite17.06-1_17.06.0+dfsg-1_amd64.deb
sudo dpkg -i liblimesuite-dev_17.06.0+dfsg-1_amd64.deb
sudo dpkg -i soapysdr0.6-module-lms7_17.06.0+dfsg-1_amd64.deb
sudo dpkg -i soapysdr-module-lms7_17.06.0+dfsg-1_amd64.deb
sudo dpkg -i limesuite-udev_17.06.0+dfsg-1_all.deb
cd $CURDIR

if [ ! -z $BUILD_INSTALL_GTEST ]; then
  DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
  /bin/bash ${DIR}/install-gtest.sh
else
  echo "==> BUILD_INSTALL_GTEST not specified"
fi

exit 0
