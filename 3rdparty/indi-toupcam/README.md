This is the INDI driver for the Touptek cameras. It was tested
with ToupCam GCMOS01200KPB but, hopefully, should work with
other cameras from Touptek too.

COMPILING

Go to the directory where  you unpacked indi_asi sources and do:

mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ..
make

should build the indi_touptek_ccd executable.

RUNNING

The Driver can run multiple devices if required, to run the driver:

indiserver -v indi_touptek_ccd

AVAILABLE CONTROLS

You can set many controls including gain, white and black balance, hue, saturation..etc

When taking an exposure, the camera switches to software trigger mode. When streaming video, the camera switches to video mode.

TESTING

The driver was tested with KStars/EKOS as a remote INDI
server (just select remote driver, the IP of machine where indi_touptek
is running and the default port 7624). Connect to the camera you want
to use and have fun!

