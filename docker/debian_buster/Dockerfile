FROM debian:buster

RUN apt-get -qq update && apt-get -qqy dist-upgrade

RUN apt-get -qqy install \
        cdbs dpkg-dev debhelper cmake curl dcraw fakeroot wget git ssh \
        libcurl4-gnutls-dev libboost-dev libboost-regex-dev libcfitsio-dev \
        libftdi-dev libgphoto2-dev libgps-dev libgsl-dev libjpeg-dev \
        libnova-dev libopenal-dev libraw-dev libusb-1.0-0-dev librtlsdr-dev \
        libfftw3-dev zlib1g-dev libconfuse-dev python3-all-dev doxygen \
        libboost-test-dev python-all-dev swig g++ libftdi1-dev \
        googletest google-mock clang lsb-release dirmngr vim

WORKDIR /home

RUN printf '%s\n' \
    "#!/bin/bash" \
    "if [ .\$1 == '.' ] ; then export REPO=indilib ; else export REPO=\$1 ; fi "\
    "git clone https://github.com/\$REPO/indi.git " \
    "cd indi " \
    "if [ .\$2 == '.' ] ; then export CIRCLE_BRANCH=master ; else export CIRCLE_BRANCH=\$2 ; fi "\
    "git checkout \$CIRCLE_BRANCH" \
    "./.circleci/build-all.sh" \
    "./.circleci/run-tests.sh" > /home/run-build.sh && \
    chmod a+x /home/run-build.sh
