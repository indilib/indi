FROM centos:latest

RUN yum -y install epel-release

RUN yum -y upgrade

RUN yum -y groupinstall 'Development Tools'

RUN yum -y install \
        cdbs cmake3 curl dcraw wget git openssh redhat-lsb-core \
        libcurl-devel boost-devel cfitsio-devel libusbx-devel \
        libftdi-devel libgphoto2-devel gpsd-devel gsl-devel libjpeg-turbo-devel \
        libnova-devel openal-soft-devel LibRaw-devel libusb-devel rtl-sdr-devel \
        fftw-devel zlib-devel libconfuse-devel python3-devel doxygen \
        python-devel swig gcc-c++ gtest gmock clang vim
        
RUN ln -s /usr/bin/cmake3 /usr/bin/cmake
RUN ln -s /usr/bin/ctest3 /usr/bin/ctest

WORKDIR /home

ADD https://raw.githubusercontent.com/jochym/indi/master/docker/run-build.sh /home/
RUN chmod a+x /home/run-build.sh
