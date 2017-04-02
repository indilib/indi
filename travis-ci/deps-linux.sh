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

