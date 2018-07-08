FROM fedora:latest

RUN dnf -y upgrade

RUN dnf -y install \
        cdbs cmake curl dcraw wget git openssh redhat-lsb-core \
        libcurl-devel boost-devel cfitsio-devel libtiff-devel \
        libftdi-devel libdc1394-22-dev libgphoto2-devel gpsd-devel gsl-devel libjpeg-turbo-devel \
        libnova-devel openal-soft-devel LibRaw-devel libusb-devel rtl-sdr-devel \
        fftw-devel zlib-devel libconfuse-devel python3-devel doxygen \
        libdc1394-devel python-devel swig gcc-c++ clang vim

# Install FFMpeg-devel (for webcam driver, through rpmfusion)
RUN dnf -y install https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm \
        https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm
        
RUN dnf -y install ffmpeg-devel

# Install googletest
WORKDIR /home
RUN git clone https://github.com/google/googletest.git
WORKDIR /home/googletest
RUN cmake .
RUN make install

WORKDIR /home
ADD https://raw.githubusercontent.com/jochym/indi/master/docker/run-build.sh /home/
RUN chmod a+x /home/run-build.sh
