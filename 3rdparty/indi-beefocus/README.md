Beefocus
========

Beefocus is an Open Source Hardware/ Software Telescope Focuser
The goal of the project is to create a high quality DIY focuser that's
relatively easy to build and modify.

The Github page for the project is https://github.com/glowmouse/beefocus
The page contains the focuser's firmware and hardware builds.

The connection to the focuser hardware is done using WiFi.  WiFi telescope
setups are becoming common as astronomers mount computers directly onto
their telescopes to reduce cabling.  The WiFi connection means one less cable
and one less USB port.  

The Focuser's firmware runs on an ESP8266 - these are <$10 micro-controllers
similar to Arduinos (in fact, the Arduino SDK is used to compile the firmware)
   
Actual and Simulated Focuser
============================

The Beefocus INDI driver supports two modes,

1. Connect to an actual focuser using a TCP/IP connection
2. Simulate the focuser in software, using a virtual "software-only" connection

The actual hardware focuser is controlled by firmware that's compiled and 
loaded onto the EPS8266 micro-controller.  The firmware handles the connection
to the driver, accepts new commands from the driver, controls the stepper
motor, and sends responses.

The same firmware is used when the driver is in simulation mode.  The simulated
firmware handles the virtual "software-only" connection to the driver, 
accepts new commands from the driver, controls a simulated stepper motor,
and sends responses. 

Having simulated hardware that's a close match to the actual hardware is useful
for testing. The unit tests create an INDI Beefocus driver using a simulated 
focuser, feed the driver commands (i.e. move to an absolute position), and 
check the driver's output to make sure it's correct.  The tests run in 
"accelerated time";  a test sequence that would take minutes on an actual 
focuser takes seconds on the simulated focuser.

To try out the simulator to to the Focuser's Connection menu, select 
Simulated Connection instead of TCP Connect,  and then press Connect in 
Main Control.

INDI Driver Directory Layout
============================

File                    | Description
----                    | -----------
driver/beefocus         | The core INDI driver
driver/beeconnect       | Connection interface to actual or simulated focuser
driver/beesimfirmware   | Uses the Firmware to simulate the focuser
unit_tests/test_helpers | General Utilities for unit testing INDI drivers
unit_tests/test_driver  | Beefocus unit tests (using the simulated firmware)
firmware/*              | ESP8266 Firmware.  Also used for simulation.

