FROM fedora:latest

RUN dnf -y upgrade

RUN dnf -y install \
        cdbs cmake curl dcraw wget git openssh \
        libcurl-devel boost-devel cfitsio-devel \
        libftdi-devel libgphoto2-devel gpsd-devel gsl-devel libjpeg-turbo-devel \
        libnova-devel openal-soft-devel LibRaw-devel libusb-devel rtl-sdr-devel \
        fftw-devel zlib-devel libconfuse-devel python3-devel doxygen \
        python-devel swig gcc-c++ gtest gmock clang

WORKDIR /home

RUN printf '%s\n' \
    "#!/bin/bash" \
    "git clone https://github.com/jochym/indi.git " \
    "cd indi " \
    "CIRCLE_BRANCH=master ./.circleci/build-all.sh" \
    "CIRCLE_BRANCH=master ./.circleci/run-tests.sh" > /home/run-build.sh && \
    chmod a+x /home/run-build.sh
