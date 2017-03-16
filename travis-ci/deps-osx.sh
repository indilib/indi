#!/bin/bash

set -e

brew update
brew tap 'homebrew/homebrew-science'
brew tap 'caskroom/drivers'
brew tap 'seanhoughton/indi'

# Not available in homebrew:
# cdbs
# libgps
# libgsl0 
# libopenal

# Already installed:
# boost


brew install \
			curl \
			dcraw \
			fakeroot \
			gsl \
			libftdi \
			libgphoto2 \
			libjpeg \
			libusb \
			libraw \
			seanhoughton/indi/libcfitsio \
			seanhoughton/indi/libnova 
			
brew cask install \
	sbig-universal-driver

if [ ! -z $BUILD_INSTALL_GTEST ]; then
  DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
  /bin/bash ${DIR}/install-gtest.sh
else
  echo "==> BUILD_INSTALL_GTEST not specified"
fi
