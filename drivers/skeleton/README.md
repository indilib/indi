# Sample Skeleton Drivers

Included in this directory are a few sample template drivers which you can base your driver on.

While INDI driver development might appear intimidating at first glance, do not worry as there are numerous resources online and active community
of users and developers alike that can help you.

## Developer Resources

+ [INDI API](http://www.indilib.org/api/index.html)
+ [INDI Developer Manual](http://indilib.org/develop/developer-manual.html)
+ [Tutorials](http://indilib.org/develop/tutorials.html)
+ [Developers Forum](http://indilib.org/forum/development.html)
+ [Developers Chat](https://riot.im/app/#/room/#kstars:matrix.org)

## Modifying Skeleton Drivers

Start by copying the skeleton driver and renaming it to your designated device name. Then modify the source code and start adding
functionality of the actual hardware device.

## Building Skeleton Drivers

Make sure to build libindi first before building the skeleton drivers. Assuming you are right now in build/libindi, proceed with the following:

```
mkdir skeleton
cd skeleton
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug /path/to/libindi/drivers/skeleton
make
sudo make install
```

''sudo make install'' is not strictly necessary as you can execute the driver locally. For example:

```
indiserver -vv ./indi_focuser_driver_focus
```

You will notice there is no XML drivers to describe the driver to the client like the regular drivers. You can manually modify /usr/share/indi/drivers.xml
to add your driver until it is ready for inclusion in INDI official tree.

Use any INDI client to communicate with the driver running on the INDI server above. The default port is 7624, so simply try to communicate with it on localhost.

Use `indi_getprop` to see a list of property from the driver. Use `indi_setprop` to set any properties as well.
