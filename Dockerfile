FROM ubuntu:xenial

RUN apt-get -qq update

RUN apt-get -qqy install \
            libnova-dev libcfitsio-dev libusb-1.0-0-dev zlib1g-dev \
            libgsl-dev build-essential cmake git libjpeg-dev \
            libcurl4-gnutls-dev libtiff-dev libfftw3-dev

WORKDIR /home

ADD . /home

RUN mkdir -p /home/build/libindi
WORKDIR /home/build/libindi
RUN cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ../../libindi
RUN make
RUN make install

EXPOSE 7624
ENTRYPOINT ["indiserver"]
CMD []