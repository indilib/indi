#!/bin/bash

set -e

brew update
brew tap polakovic/astronomy

# Not available in homebrew:
# cdbs \
# boost-regex \
# libgps \
# libgsl0  \
# libopenal \

# Already installed:
# boost \


brew install \
			curl \
			dcraw \
			fakeroot \
			libftdi \
			libgphoto2 \
			libjpeg \
			libusb \
			polakovic/astronomy/libcfitsio \
			polakovic/astronomy/libnova \
			wget

if [ ! -z $BUILD_INSTALL_GTEST ]; then
  /bin/bash install-gtest.sh
else
  echo "==> BUILD_INSTALL_GTEST not specified"
fi
