# INDI Core Library
[![Linux](https://github.com/indilib/indi/actions/workflows/default.yml/badge.svg)](https://github.com/indilib/indi/actions)
[![MacOS](https://github.com/indilib/indi/actions/workflows/macos.yml/badge.svg)](https://github.com/indilib/indi/actions)
[![PyIndi](https://github.com/indilib/indi/actions/workflows/pyindi.yml/badge.svg)](https://github.com/indilib/indi/actions)

INDI is a standard for astronomical instrumentation control. INDI Library is an Open Source POSIX implementation of the
[Instrument-Neutral-Device-Interface protocol](http://www.clearskyinstitute.com/INDI/INDI.pdf).

INDI core library is composed of the following components:

1. INDI Server.
2. INDI Core Drivers: Hardware drivers that communicate with the equipment. Many devices are supported including:
+ Mounts
+ CCDs, CMOS, Webcams, DSLRs (Canon, Nikon, Sony, Pentax..etc).
+ Focusers.
+ Filter Wheels.
+ Adaptive Optics.
+ Domes.
+ GPS.
+ Weather Stations.
+ Controllers.
+ Auxiliary Devices (switches, watchdog, relays, light sources, measurement devices..etc).
3. Client Library: Cross-platform POSIX and Qt5-based client libraries. The client libraries can be embedded in 3rd party applications to communicate with INDI server and devices.

INDI core device drivers are shipped with INDI library by default.

INDI 3rd party drivers are available in a [dedicated 3rdparty repository](https://github.com/indilib/indi-3rdparty) and maintained by their respective owners.

Learn more about INDI:
+ [Features](http://indilib.org/about/features.html)
+ [Discover INDI](http://indilib.org/about/discover-indi.html)
+ [Supported Devices](http://indilib.org/devices/)
+ [Clients](http://indilib.org/about/clients.html)

# Building

## Install Pre-requisites

On Debian/Ubuntu:

```bash
sudo apt-get install -y \
  git \
  cdbs \
  dkms \
  cmake \
  fxload \
  libgps-dev \
  libgsl-dev \
  libraw-dev \
  libusb-dev \
  zlib1g-dev \
  libftdi-dev \
  libgsl0-dev \
  libjpeg-dev \
  libkrb5-dev \
  libnova-dev \
  libtiff-dev \
  libfftw3-dev \
  librtlsdr-dev \
  libcfitsio-dev \
  libgphoto2-dev \
  build-essential \
  libusb-1.0-0-dev \
  libdc1394-22-dev \
  libboost-regex-dev \
  libcurl4-gnutls-dev
```

## Create Project Directory
```bash
mkdir -p ~/Projects
cd ~/Projects
```

## Get the code
To build INDI in order to run drivers, then it is recommended to perform a quick shallow clone that will save lots of bandwidth and space:
```bash
git clone --depth 1 https://github.com/indilib/indi.git
```

On the other hand, if you plan to submit a PR or engage in INDI driver development, then getting a full clone is recommended:
```bash
git clone https://github.com/indilib/indi.git
```
It is worth making your own fork of indi in your own personal repository and cloning from that rather than cloning directly from the root indi.

## Build indi-core (cmake)

```bash
mkdir -p ~/Projects/build/indi-core
cd ~/Projects/build/indi-core
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ~/Projects/indi
make -j4
sudo make install
```

## Build indi-core (script)

You can use the `developer-build.bash` script for faster building.

```bash
cd ~/Projects/indi
./developer-build.bash
```

By default, this script builds the `indi-core` inside machine's `RAM`, i.e. `/dev/shm`.
To change the default build directory, just pass in the desired directory after calling the script, for instance:

```bash
cd ~/Projects/indi
./developer-build.bash /path/to/new/build/dir
```

Also, this script checks if the target build directory is a subdirectory of `/dev/shm`.
If so, the build process aborts if there is less than `1Gb` free space in `RAM`.
One can reset this limitation with changing the `NEED_MEMORY` variable to his/her liking.

In addition, this script creates a soft symbolic link file named `build` from the build directory to the source directory.
This helps with accessing the build directory by simply following the link inside the source folder like any other directory, i.e. `cd ~/Projects/indi/build`.

## Build indi-core (Qt Creator)

If your are planning to develop using Qt Creator then still follow this process and do a manual build first.  Then in QT Creator:
+ Open the project using File - Open File or Project.
+ Navigate to Projects/indi and selec the CMakeLists.txt file.
+ Qt Creator will open your project but will probably configure it incorrectly, select the Projects tab and change to the Projects/build/indi-core directory that you used to do the initial build.  The project display may be blank but click on the build button (the geological hammer) anyway.  The project should build.

It is very easy to get this process wrong and all sorts of subtle things can happen, such as everything appearing to build but your new functionality not being present.

# Architecture


Typical INDI Client / Server / Driver / Device connectivity:


    INDI Client 1 ----|                  |---- INDI Driver A  ---- Dev X
                      |                  |
    INDI Client 2 ----|                  |---- INDI Driver B  ---- Dev Y
                      |                  |                     |
     ...              |--- indiserver ---|                     |-- Dev Z
                      |                  |
                      |                  |
    INDI Client n ----|                  |---- INDI Driver C  ---- Dev T


     Client       INET       Server       UNIX     Driver          Hardware
     processes    sockets    process      pipes    processes       devices



INDI server is the public network access point where one or more INDI Clients may contact one or more INDI Drivers.
indiserver launches each driver process and arranges for it to receive the INDI protocol from clients on its stdin and expects to find commands destined for clients on the driver's stdout.
Anything arriving from a driver process' stderr is copied to indiserver's stderr.
INDI server only provides convenient port, fork and data steering services. If desired, a client may run and connect to INDI Drivers directly.

# Support

+ [FAQ](http://indilib.org/support/faq.html)
+ [Forum](http://indilib.org/forum.html)
+ [Tutorials](http://indilib.org/support/tutorials.html)

# Development

+ [INDI API](http://www.indilib.org/api/index.html)
+ [INDI Developer Manual](https://docs.indilib.org/)
+ [Tutorials](http://indilib.org/develop/tutorials.html)
+ [Developers Forum](http://indilib.org/forum/development.html)
+ [Developers Chat](https://riot.im/app/#/room/#kstars:matrix.org)
+ Sample drivers are available under examples and drivers/skeleton directories. They can be used as a starting point for your driver development.

### Code Style

INDI uses [Artistic Style](http://astyle.sourceforge.net) to format all the C++ source files. Please make sure to apply the following astyle rules to any code that is submitted to INDI. On Linux, you can create ***~/.astylerc*** file containing the following rules:
```
--style=allman
--align-reference=name
--indent-switches
--indent-modifiers
--indent-classes
--pad-oper
--indent-col1-comments
--lineend=linux
--max-code-length=124
```

Some IDEs (e.g. QtCreator) support automatic formatting for the code everytime you save the file to disk.

### How to create Github pull request (PR)

[How to contribute to INDI full guide](http://indilib.org/develop/tutorials/181-how-to-contribute-to-indi-github-development.html)

Here is the short version on how to submit a PR:
1. Login with a Github account and fork the official INDI repository.
2. Clone the official INDI repository and add the forked INDI repository as a remote (git remote add ...).
3. Create a local Git branch (git checkout -b my_branch).
4. Work on the patch and commit the changes.
5. If it is ready push this branch to your fork repo (git push -f my_fork my_branch:my_branch).
6. Go to the official repo's github website in a browser, it will popup a message to create a PR. Create it.
7. Pushing updates to the PR: just update your branch (git push -f my_fork my_branch:my_branch)..

If you would like to make cleaner PR (recommended!) please read this [tutorial](https://blog.adamspiers.org/2015/03/24/why-and-how-to-correctly-amend-github-pull-requests/) and follow it. The best way is to keep *one logical change per commit* and not pollute the history by multiple small fixes to the PR.

### Driver Documentation

When submitting a new driver, the driver user **documentation** is required as part of the submission process.

* Installation: Driver name, executable name, version, required INDI version.
* Features: What features does it support exactly?
* Operation: How to operate the driver? Each sub section should come with a screen shot of the various tabs..etc.
  Preferably annotated to make it easier for new users to follow.
  * Connecting: How to establish connection? How to set port if any?
  * Main Control: Primary control tab and its functions.
  * Options: Explanation for the various options available.
  * Etc: Any other tabs created by the driver.
* Issues: Any problems or issues or warnings the users should be aware about when using this driver.

### Sample Drivers

You can base a new driver from an existing driver. Look in either the examples or drivers/skeleton directories on how to get started.

# Unit tests

In order to run the unit test suite you must first install the [Google Test Framework](https://github.com/google/googletest). You will need to build and install this from source code as Google does not recommend package managers for distributing distros.(This is because each build system is often unique and a one size fits all aproach does not work well).

Once you have the Google Test Framework installed follow this alternative build sequence:-

```
mkdir -p build/indi
cd build/indi
cmake -DINDI_BUILD_UNITTESTS=ON -DCMAKE_BUILD_TYPE=Debug ../../indi
make
cd test
ctest -V
```

For more details refer to the scripts in the travis-ci directory.
