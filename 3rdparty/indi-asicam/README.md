
This is the INDI driver for the ZWO Optics ASI cameras. It was tested
with the ASI120MC and the ASI120MM but, hopefully, should work with
other cameras from ZWO Optical too.

COMPILING

Go to the directory where  you unpacked indi_asicam sources and do:

mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/opt/indi/ ..
make

should build the indi_asicam executable.

RUNNING

For a single camera just do, from the directory where you build:

indiserver ./indi_asicam

If you have multiple cameras you must use FIFO mode to differientate
between them:

mkfifo /tmp/myFIFO
indiserver -f /tmp/myFIFO
echo start /mnt/fat/svn/indi-asicam-no/build/indi_asicam -n \"ASICAM0\" > /tmp/myFIFO 
echo start /mnt/fat/svn/indi-asicam-no/build/indi_asicam -n \"ASICAM1\" > /tmp/myFIFO 

When you connect with you client you can select ASICAM0 or ASICAM1
camera. In each of them you have a switch ("Available Cams") to select
which one you want to use for each connection.

If you want to have the ability to auto-start the driver there is an
XML fragment provided, but keep in mind the problem with multiple
cameras.

TESTING

The driver was tested with OpenPHD and KStars/EKOS as a remote INDI
server (just select remote driver, the IP of machine where indi_asicam
is running and the default port 7624). Connect to the camera you want
to use and have fun!

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
error message from the client with the DEBUG switch enabled.

Cieli sereni!
Chrstian Pellegrin <chripell@gmail.com>
