FROM opensuse:tumbleweed

RUN zypper refresh && zypper  --non-interactive update

RUN zypper --non-interactive install \
        cmake curl dcraw wget git openssh gcc gcc-c++ clang swig

RUN zypper --non-interactive install \
        libcurl-devel boost-devel cfitsio-devel 

RUN zypper --non-interactive install \
        libftdi1-devel libgphoto2-devel gsl-devel libjpeg-devel

RUN zypper --non-interactive install \
        libnova-devel openal-soft-devel libraw-devel libusb-devel

RUN zypper --non-interactive install \
        fftw-devel zlib-devel libconfuse-devel python3-devel doxygen 

RUN zypper --non-interactive install python-devel vim

RUN zypper --non-interactive install rtl-sdr-devel 

# No stable packages for gtest & gmock in opensuse
# RUN zypper --non-interactive install gtest gmock

WORKDIR /home

RUN echo "#!/bin/bash" > /home/run-build.sh
RUN echo "git clone https://github.com/jochym/indi.git " >> /home/run-build.sh
RUN echo "cd indi && CIRCLE_BRANCH=master ./.circleci/build-all.sh" >> /home/run-build.sh
RUN echo "CIRCLE_BRANCH=master ./.circleci/run-tests.sh" >> /home/run-build.sh
RUN chmod a+x /home/run-build.sh

RUN zypper -n install gpsd python-gpsd python-curses
RUN zypper -n download gpsd-devel libgps23
RUN rpm -iv --nodeps /var/cache/zypp/packages/OSS/suse/x86_64/*gps*rpm

