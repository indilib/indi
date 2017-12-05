FROM centos:latest

RUN yum -y install epel-release

RUN yum -y upgrade

RUN yum -y groupinstall 'Development Tools'

RUN yum -y install \
        cdbs cmake3 curl dcraw wget git openssh \
        libcurl-devel boost-devel cfitsio-devel libusbx-devel \
        libftdi-devel libgphoto2-devel gpsd-devel gsl-devel libjpeg-turbo-devel \
        libnova-devel openal-soft-devel LibRaw-devel libusb-devel rtl-sdr-devel \
        fftw-devel zlib-devel libconfuse-devel python3-devel doxygen \
        python-devel swig gcc-c++ gtest gmock clang vim
        
RUN ln -s /usr/bin/cmake3 /usr/bin/cmake
RUN ln -s /usr/bin/ctest3 /usr/bin/ctest

WORKDIR /home

RUN echo "#!/bin/bash" > /home/run-build.sh
RUN echo "git clone https://github.com/jochym/indi.git " >> /home/run-build.sh
RUN echo "cd indi && CIRCLE_BRANCH=master ./.circleci/build-all.sh" >> /home/run-build.sh
RUN echo "CIRCLE_BRANCH=master ./.circleci/run-tests.sh" >> /home/run-build.sh
RUN chmod a+x /home/run-build.sh
