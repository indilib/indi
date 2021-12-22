#!/bin/bash

set -e

command -v realpath >/dev/null 2>&1 || realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}

SRCS=$(dirname $(realpath $0))/..

# TODO
# libtiff-devel ?

OS=$(uname -s)

case "$OS" in
    Darwin)
        brew install \
            git \
            cfitsio libnova libusb curl \
            gsl jpeg fftw librtlsdr
        ;;
    Linux)
        . /etc/os-release
        case $ID in
            debian|ubuntu|raspbian)
                export DEBIAN_FRONTEND=noninteractive
                $(command -v sudo) apt-get update
                $(command -v sudo) apt-get upgrade -y
                $(command -v sudo) apt-get install -y \
                    git \
                    cmake build-essential zlib1g-dev \
                    libcfitsio-dev libnova-dev libusb-1.0-0-dev libcurl4-gnutls-dev \
                    libgsl-dev libjpeg-dev libfftw3-dev librtlsdr-dev
                ;;
            fedora)
                $(command -v sudo) dnf upgrade -y
                $(command -v sudo) dnf install -y \
                    git \
                    cmake gcc-c++ zlib-devel \
                    cfitsio-devel libnova-devel libusb-devel libcurl-devel \
                    gsl-devel libjpeg-devel fftw-devel rtl-sdr-devel
                ;;
            centos)
                # CentOS 8 dont have libnova-devel package
                $(command -v sudo) yum install -y epel-release
                $(command -v sudo) yum upgrade -y
                $(command -v sudo) yum install -y \
                    git \
                    cmake gcc-c++ zlib-devel \
                    cfitsio-devel libnova-devel libusb-devel libcurl-devel \
                    gsl-devel libjpeg-devel fftw-devel rtlsdr-devel
                ;;
            opensuse-tumbleweed)
                # broken git/openssh package
                $(command -v sudo) zypper refresh
                $(command -v sudo) zypper --non-interactive update
                $(command -v sudo) zypper --non-interactive install -y \
                    openssh git \
                    cmake gcc-c++ zlib-devel \
                    cfitsio-devel libnova-devel libusb-devel libcurl-devel \
                    gsl-devel libjpeg-devel fftw-devel rtlsdr-devel libtheora-devel
                ;;
            *)
                echo "Unknown Linux Distribution: $ID"
                cat /etc/os-release
                exit 1
                ;;
        esac
        ;;
    *)
        echo "Unknown System: $OS"
        exit 1
esac

$SRCS/scripts/googletest-build.sh
$SRCS/scripts/googletest-install.sh
