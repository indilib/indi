This is the INDI driver for the ATIK cameras and filter wheels.

# Requirements

+ INDI
+ cfitsio
+ libatik
+ libnova


# Compiling

Go to the directory where  you unpacked indi_asi sources and do:

mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install

# Running

The Driver can run multiple devices if required, to run the driver:

indiserver -v indi_atik_ccd
