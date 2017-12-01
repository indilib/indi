FROM debian:stretch

RUN apt-get -qq update && apt-get -qqy dist-upgrade

RUN apt-get install -qy cdbs dpkg-dev debhelper cmake curl dcraw fakeroot wget 
RUN apt-get -qqy install libcurl4-gnutls-dev libboost-dev libboost-regex-dev libcfitsio-dev libftdi-dev libgphoto2-dev libgps-dev 
RUN apt-get -qqy install libgsl-dev libjpeg-dev libnova-dev libopenal-dev libraw-dev libusb-1.0-0-dev librtlsdr-dev libfftw3-dev zlib1g-dev
RUN apt-get -qqy install libconfuse-dev python3-all-dev doxygen libboost-test-dev python-all-dev swig g++ libftdi1-dev

RUN pwd
RUN ls -l

#RUN ./install-gtest.sh
# Build gtest framework
RUN mkdir -p build \
        pushd build \
        git clone https://github.com/google/googletest.git \
        if [ ! -d googletest ]; then \
            echo "Failed to get googletest.git repo" \
            exit 1 \
        fi \
        pushd googletest \
        mkdir build \
        cd build \
        cmake .. -DCMAKE_CXX_FLAGS="-fPIC" \
        make install \
        popd \
        rm -rf googletest \
        popd \
