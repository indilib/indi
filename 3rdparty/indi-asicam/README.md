
This is the INDI driver for the ZWO Optics ASI cameras. It was tested
with the ASI120MC and the ASI120MM but, hopefully, should work with
other cameras too.

COMPILING

This driver compiles with the trunk SVN version (in the hope to be
accepted once tested by enough people). You should checkout the INDI
sources and compile them the usual way (suppose that you keep the SVN
repositories in /mnt/fat/svn/:

cd /mnt/fat/svn
mkdir indi
cd indi
git svn clone svn://svn.code.sf.net/p/indi/code/trunk --stdlayout
cd code
mkdir libindi_build
cd libindi_build
cmake -DCMAKE_INSTALL_PREFIX=/opt/indi . ../libindi
make
make install

This compiles and install the INDI stuff under /opt/indi/.

Next put the libASICamera.h and libASICamera.a from the SDK available
from http://zwoug.org/viewforum.php?f=17 in a directory called SDK
under that one where indi_asicam.cpp and friends live. Setup some
variables:

export PATH=/opt/indi/bin/:$PATH
export LD_LIBRARY_PATH=/opt/indi/lib/:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=/opt/indi/lib/pkgconfig:$PKG_CONFIG_PATH

now you can build indi_asicam with cmake. Go to the directory where
you unpacked indi_asicam sources and do:

mkdir build
cd build
cmake -DCMAKE_INDI_SOURCE_DIR=/mnt/fat/svn/indi/code/ -DCMAKE_INSTALL_PREFIX=/opt/indi/ ..
make

should build the indi_asicam0 executable.

RUNNING

The ASIcamera driver unfortunately supports just one open camera at a
time. To overcome this limitation you can make as many link called
indi_asicamN to the executable indi_asicam0. By inspecting its command
line the driver guess which camera should handle. So, if you have two
camera, just run:

indiserver -v ./indi_asicam0 ./indi_asicam1

If you want to have the ability to auto-start the driver there is an
XML fragment provided, but keep in mind the problem with multiple
cameras.

TESTING

The driver was tested with OpenPHD and KStars/EKOS as a remote INDI
server (just select remote driver, the IP of machine where
indi_asicam0 is running and the default port 7624). Connect to the
camera you want to use and have fun!

NOTES

The ASICameras are very USB bandwidth hungry when running at high
FPS. If you see "broken frames" with more that one of them running,
keep in mind this important aspect.

ASICameras continuously generate frames and they are buffered in the
driver. To avoid having frames with old parameters when these are
changed, a flushing mechanism is employed. It looks OK, but if you
find any problem with parameters changes not being immediately applied
please report.

I'm new to INDI so I'm not sure I got everything 100% right. Any
feedback or constructive criticizing is *very* appreciated. For this
reason heavy debugging can be output by defining the environment
variable INDI_ASICAM_VERBOSE before running the driver. If you find
any problems please send me the output by running indiserver and the
error message from the client.

Cieli sereni!
Chrstian Pellegrin <chripell@gmail.com>
