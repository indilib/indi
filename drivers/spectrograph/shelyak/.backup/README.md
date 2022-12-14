# Shelyak INDI Driver

This package provides INDI driver for Shelyak spectrographs.

##### Supported devices:
* Shelyak eShel calibration unit
* Shelyak SPOX calibration unit command



## Installation
This package uses CMake to build:
```
$ mkdir build
$ cd build
$ cmake ..
$ make
$ make install # as root
```

## How to Use
You can use the Shelyak INDI driver in any INDI-compatible client such as KStars or Xephem.

To run the eShel driver from the command line:

```bash
indiserver -p 1456 indi_shelyak_usis
```

You can then connect to the driver from any client, the default port is 7624.
