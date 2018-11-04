INDIGO driver for PegausAstro Ultimate Powerbox
================

Copyright (c) 2018 CloudMakers, s. r. o.
All rights reserved.

You can use this software under the terms of 'INDIGO Astronomy open-source
license' (see LICENSE.txt).

Source code is available from INDIGO project repository:

https://github.com/indigo-astronomy/indigo/tree/master/indigo_drivers/aux_upb

Requirtments
============

+ libusb is required.
	
Installation
============

You must have CMake >= 2.7.0 in order to build this package.

1) $ cmake -DCMAKE_INSTALL_PREFIX=/usr .
2) $ make
3) $ su -c 'make install' or sudo make install
	
How to Use
==========

You can use the driver in any INDIGO- or INDI-compatible client. 

To run the driver from the command line:

$ indiserver indigo_aux_upb

You can then connect to the driver from any client, the default port is 7624.
If you're using KStars, the driver will be automatically listed in KStars'
Device Manager, no further configuration is necessary.
