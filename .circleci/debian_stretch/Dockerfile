FROM debian:stretch

RUN apt-get -qq update && apt-get -qqy dist-upgrade

RUN apt-get -qqy install \
        cdbs dpkg-dev debhelper cmake curl dcraw fakeroot wget git ssh \
        libcurl4-gnutls-dev libboost-dev libboost-regex-dev libcfitsio-dev \
        libftdi-dev libgphoto2-dev libgps-dev libgsl-dev libjpeg-dev \
        libnova-dev libopenal-dev libraw-dev libusb-1.0-0-dev librtlsdr-dev \
        libfftw3-dev zlib1g-dev libconfuse-dev python3-all-dev doxygen \
        libboost-test-dev python-all-dev swig g++ libftdi1-dev \
        googletest google-mock clang

WORKDIR /home

RUN echo "#!/bin/bash" > /home/run-build.sh
RUN echo "git clone https://github.com/jochym/indi.git " >> /home/run-build.sh
RUN echo "cd indi && CIRCLE_BRANCH=master ./.circleci/build-all.sh" >> /home/run-build.sh
RUN echo "CIRCLE_BRANCH=master ./.circleci/run-tests.sh" >> /home/run-build.sh
RUN chmod a+x /home/run-build.sh
