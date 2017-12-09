FROM opensuse:tumbleweed

RUN zypper refresh && zypper  --non-interactive update

RUN zypper --non-interactive install lsb-release \
        cmake curl dcraw wget git openssh gcc gcc-c++ clang swig \
        libcurl-devel boost-devel cfitsio-devel \
        libftdi1-devel libgphoto2-devel gsl-devel libjpeg-devel \
        libnova-devel openal-soft-devel libraw-devel libusb-devel \
        fftw-devel zlib-devel libconfuse-devel python3-devel doxygen \
        python-devel vim rtl-sdr-devel vim

RUN zypper -n install gpsd python-gpsd python-curses
RUN zypper -n download gpsd-devel libgps23
RUN rpm -iv --nodeps /var/cache/zypp/packages/OSS/suse/x86_64/*gps*rpm

# No stable packages for gtest & gmock in opensuse
# RUN zypper --non-interactive install gtest gmock

WORKDIR /home

ADD https://raw.githubusercontent.com/jochym/indi/master/docker/run-build.sh /home/
RUN chmod a+x /home/run-build.sh
