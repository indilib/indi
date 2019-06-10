#!/bin/bash

set -e

brew update
brew tap 'caskroom/drivers'
brew tap 'indilib/indi'

brew upgrade cmake

brew install \
	dcraw \
	fakeroot \
	gsl \
	libftdi \
	libdc1394 \
	libgphoto2 \
	libusb \
	gpsd \
	libraw \
	libcfitsio \
	libnova  \
	librtlsdr  \
	ffmpeg

# JM 2019-06-10: Disabling this since it always fails.
# brew cask install \
# 	sbig-universal-driver

if [ ! -z $BUILD_INSTALL_GTEST ]; then
  DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
  /bin/bash ${DIR}/install-gtest.sh
else
  echo "==> BUILD_INSTALL_GTEST not specified"
fi
