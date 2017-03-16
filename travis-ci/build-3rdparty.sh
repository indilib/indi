#!/bin/bash

set -x -e

echo ${TRAVIS_OS_NAME}

if [ ! -z $BUILD_THIRD_PARTY ]; then
  echo "==> BUILD_THIRD_PARTY activated"
  mkdir -p build/3rdparty
  pushd build/3rdparty

#  if [ ${TRAVIS_OS_NAME} == "linux" ] ; then
#    LIBS="libapogee libfishcamp libfli libqhy libqsi libsbig"
#  else
#    LIBS="libfishcamp libqsi"
#  fi

#  for lib in $LIBS ; do
#  (
#    echo "Building $lib ..."
#    mkdir build_$lib
#    cd build_$lib
#    cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ . ../../../3rdparty/$lib
#    make
#    sudo make install
#  )
#  done
  
  cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ -DWITH_MI:OPTION=OFF -DWITH_QHY:OPTION=OFF . ../../3rdparty/
  sudo make install
  cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ -DWITH_MI:OPTION=OFF -DWITH_QHY:OPTION=OFF . ../../3rdparty/
  sudo make install
  popd
else
  echo "==> BUILD_THIRD_PARTY not specified"
fi

exit 0

