#!/bin/bash

set -x -e

sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test

sudo apt-get -qq update

# try to use gcc5 for the build
sudo apt-get -q -y install gcc-5 g++-5
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 60 --slave /usr/bin/g++ g++ /usr/bin/g++-5

sudo apt-get -q -y install \
 cdbs \
 curl \
 dcraw \
 fakeroot \
 libboost-dev \
 libboost-regex-dev \
 libcfitsio3-dev \
 libftdi-dev \
 libftdi1 \
 libgphoto2-dev \
 libgps-dev \
 libgsl0-dev \
 libjpeg-dev \
 libnova-dev \
 libopenal-dev \
 libraw-dev \
 libusb-1.0-0-dev \
 wget

if [ ! -z $BUILD_INSTALL_GTEST ]; then
  DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
  /bin/bash ${DIR}/install-gtest.sh
else
  echo "==> BUILD_INSTALL_GTEST not specified"
fi

exit 0

