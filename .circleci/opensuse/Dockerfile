FROM opensuse:tumbleweed

RUN zypper refresh && zypper  --non-interactive update

RUN zypper --non-interactive install \
        cmake curl dcraw wget git openssh gcc gcc-c++ clang swig \
        libcurl-devel boost-devel cfitsio-devel \
        libftdi1-devel libgphoto2-devel gsl-devel libjpeg-devel \
        libnova-devel openal-soft-devel libraw-devel libusb-devel \
        fftw-devel zlib-devel libconfuse-devel python3-devel doxygen \
        python-devel vim rtl-sdr-devel 

RUN zypper -n install gpsd python-gpsd python-curses
RUN zypper -n download gpsd-devel libgps23
RUN rpm -iv --nodeps /var/cache/zypp/packages/OSS/suse/x86_64/*gps*rpm

# No stable packages for gtest & gmock in opensuse
# RUN zypper --non-interactive install gtest gmock

WORKDIR /home

RUN printf '%s\n' \
    "#!/bin/bash" \
    "git clone https://github.com/jochym/indi.git " \
    "cd indi " \
    "CIRCLE_BRANCH=master ./.circleci/build-all.sh" \
    "CIRCLE_BRANCH=master ./.circleci/run-tests.sh" > /home/run-build.sh && \
    chmod a+x /home/run-build.sh


