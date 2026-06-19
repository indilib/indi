# MX-HD Equatorial Mount
## Installation
The MX-HD telescope driver is intended to be included with indi-core once merged. Until then, build and install it from source with:
```
$ sudo apt update
$ sudo apt install -y build-essential cmake libindi-dev libnova-dev
$ git clone https://github.com/botchASTRO/mx-hd_indi.git
$ cd mx-hd_indi
$ mkdir -p build
$ cd build
$ cmake ..
$ make -j
$ sudo make install
```

## Features
The MX-HD telescope driver supports:
* USB serial and Bluetooth RFCOMM serial connections
* 4 standard slew rates (guide, centering, find, max)
* Goto coordinates
* Sync coordinates
* Abort movement
* Driver-specific HOME action (`MXHD_HOME`)
* Park / Unpark
* Standard sidereal / solar / lunar tracking modes
* Site and time synchronization from INDI clients
* Pier side reporting for GEM operation
* Timed pulse guiding

## Operation
* Connect the MX-HD mount to the host computer using either a USB serial adapter or a Bluetooth RFCOMM serial device.
* Typical device names are `/dev/ttyUSB0` for USB serial and `/dev/rfcomm0` for Bluetooth RFCOMM.
* Set the serial port in the INDI connection tab and choose the baud rate that matches the mount connection. The driver has been tested with `9600` baud on USB serial.
* Start `indiserver` with `indi_mxhd_telescope`, or select `MX-HD Telescope` in KStars/Ekos after installation.
* On connection, the driver accepts `TIME_UTC` and `GEOGRAPHIC_COORD` from the INDI client and forwards them to the mount.
* Longitude is interpreted from INDI as east-positive and converted to the west-positive format required by the MX-HD `:Sg` command.
* The UTC offset is taken from INDI and converted to the sign convention required by the MX-HD `:SG` command.
* `Unpark` intentionally performs a HOME return before clearing the parked state. This restores a known mount attitude and position solution before remote operation resumes.
* HOME is exposed through the driver-specific `MXHD_HOME` action because this has been more reliable with current clients than standard `TELESCOPE_HOME` handling.
* Status polling follows the INDI `POLLING_PERIOD` property, with a default of 4000 ms.

## Issues
* The mount does not report pier side directly. The driver derives `TELESCOPE_PIER_SIDE` from hour angle using the current UTC and the INDI-provided east-positive longitude.
* `SIMULATE_PIER_SIDE` is enabled because pier side is inferred rather than reported by the mount.
* During slews and HOME recovery, the driver avoids querying RA/Dec until the mount returns to a stable state in order to prevent transient serial timeouts.
* HOME is currently exposed as `MXHD_HOME` rather than standard `TELESCOPE_HOME` due to client compatibility issues observed during testing.
