#!/bin/bash

set -e

brew update
brew tap 'homebrew/homebrew-science'
brew tap 'caskroom/drivers'
brew tap 'jochym/indi'

# Not available in homebrew:
# cdbs
# libgps
# libgsl0 
# libopenal

# Already installed:
# boost


brew install \
			curl \
			cmake \
			dcraw \
			fakeroot \
			gsl \
			libftdi \
			libftdi1 \
			libgphoto2 \
			libjpeg \
			libusb \
			gpsd \
			libraw \
			libcfitsio \
			libnova 
			
brew cask install \
	sbig-universal-driver

if [ ! -z $BUILD_INSTALL_GTEST ]; then
  DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
  /bin/bash ${DIR}/install-gtest.sh
else
  echo "==> BUILD_INSTALL_GTEST not specified"
fi
