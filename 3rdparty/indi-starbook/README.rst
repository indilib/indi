Starbook INDI Driver
====================

This package provides the INDI driver for the Vixen Starbook telescope controllers. This driver aims for compatibility
with first generation Starbook.
Starbook 10 is not tested with this driver.

Requirements
------------

In order to build this package you need:

+ INDI >= v1.7 (http://www.indilib.org)

    You need to install both `indi` and `indi-devel` to build this package.

+ cURL >= v7.58.0 (https://curl.haxx.se/)

    You need to install `curl`

+ libnova

    You need to install both `libnova` and `libnova-devel` to build this package.

Usage
-----

Starbook is easy to hang up. You won't get smooth operation by design.
Driver was written with this in mind so it will put few limitations on a user.
Starbook is a state machine, that means it will allow different functions in specified states.
During INIT state it won't react to GOTORADEC commands and will wait for
user to manually enter SCOPE state or by START command.
In INDI control panel it's called `Initialize`

In later version I expect to implement abstract state machine,
to keep Starbook state and allowed transitions.
From version 0.4 you can see Starbooks state in INDI control panel.

Internal HTTP server is very fragile, so any wrong request might break it.
Expect to frequently restart your Starbook.


*Connecting*

IMPORTANT NOTE: watch your mount at all times, don't trust this driver.
    Starbook is known for its erratic behaviour.
    Please report any troubles, so we can stabilize drivers operation.
    Go to GitHub issues and report them here.

1. Connect Starbook's LAN port to your network:
    - It can be connected directly with _cross-over cable_.
    - To standard router with DHCP enabled, you should give starbook static IP in your router.
2. Boot up Starbook. It will enter INIT state. (now you can setup date, location, etc.)
3. Check acquired IP address in `About STARBOOK` option in INIT menu.
4. Check internal HTTP server by going to specified IP address in browser.
5. In `Connection` tab in Kstars enter IP address
6. Connect from `Main control` tab
7. If Starbook is in INIT state, initialize it by:
    - Navigate through initial menu to SCOPE mode
    - Go to http://starbook-ip/START
    - Hit `Initialize` button in `Main control` tab
8. ...
9. Profit?

Have fun and don't look at the Sun!
