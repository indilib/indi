Celestron NexStar AUX protocol driver
-------------------------------------

Author: Paweł T. Jochym <jochym@wolf.ifj.edu.pl>

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
Do not use this driver unattended! It is ALPHA/BETA software!
Always have your power switch ready!
It is NOT SUITABLE for autonomous operation!
You use this software ON YOUR OWN RISK!
THERE ARE NO SLEW LIMITS IMPLEMENTED AT THIS STAGE!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

This is eqmod-style driver for the NexStar and other Celestron AUX-protocol 
mounts. At the moment it only works over Wi-Fi link to the NexStar-Evolution 
mount. It should also work over serial link to AUX port if someone writes the
serial comm procedures. Since I do not have AUX-port adapter for my scope, 
I cannot write and test this part. I intend to implement the hand controller (HC)
serial link mode communication - it is supposedly more reliable than the direct 
AUX port communication and requires only standard USB-RS232 adapter and a 
serial cable with RJ connector. Furthermore RS-232 transmission is probably 
more robust than AUX-port connection.

The driver is in the alpha/beta stage.
It is functional and should work as intended but it is not complete.
I am using it and will be happy to help any brave testers.
I of course welcome any feedback/contribution.

What works:
- N-star alignment (with INDI alignment module)
- Basic tracking, slew, park/unpark
- GPS simulation. If you have HC connected and you have active gps driver 
  it can simulate Celestron GPS device and serve GPS data to HC. Works quite 
  nicely on RaspberryPi with a GPS module. You can actually use it as 
  a replacement for the Celestron GPS.

What does not work/is not implemented:
- Joystick control
- Slew limits
- Serial link
- HC interaction (tracking HC motor commands to function as joystick)
- Probably many other things
