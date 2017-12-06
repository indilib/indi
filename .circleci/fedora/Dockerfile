FROM fedora:latest

RUN dnf -y upgrade

RUN dnf -y install \
        cdbs cmake curl dcraw wget git openssh redhat-lsb-core \
        libcurl-devel boost-devel cfitsio-devel \
        libftdi-devel libgphoto2-devel gpsd-devel gsl-devel libjpeg-turbo-devel \
        libnova-devel openal-soft-devel LibRaw-devel libusb-devel rtl-sdr-devel \
        fftw-devel zlib-devel libconfuse-devel python3-devel doxygen \
        python-devel swig gcc-c++ gtest gmock clang vim

WORKDIR /home

RUN printf '%s\n' \
    "#!/bin/bash" \
    "git clone https://github.com/jochym/indi.git " \
    "cd indi " \
    "if [ .\$1 == '.' ] ; then export CIRCLE_BRANCH=master ; else export CIRCLE_BRANCH=\$1 ; fi "\
    "./.circleci/build-all.sh" \
    "./.circleci/run-tests.sh" > /home/run-build.sh && \
    chmod a+x /home/run-build.sh
