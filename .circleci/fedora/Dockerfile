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

RUN echo "#!/bin/bash" > /home/run-build.sh
RUN echo "git clone https://github.com/jochym/indi.git " >> /home/run-build.sh
RUN echo "cd indi && CIRCLE_BRANCH=master ./.circleci/build-all.sh" >> /home/run-build.sh
RUN echo "CIRCLE_BRANCH=master ./.circleci/run-tests.sh" >> /home/run-build.sh
RUN chmod a+x /home/run-build.sh
