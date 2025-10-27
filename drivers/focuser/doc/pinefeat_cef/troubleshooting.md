# 🛠 Troubleshooting Guide

A list of common problems and their solutions.

## 1: Controller Is Not Detected

The issue is likely caused by a poor connection of the USB cable. Ensure the cable is properly aligned and fully inserted into both the adapter and the host system.

The adapter with a USB port supports USB CDC (Communications Device Class) and does not require any special drivers on most modern operating systems, including Linux, macOS, and Windows. It should appear automatically as a serial port (e.g., /dev/ttyACM0 on Linux).

The device identifies itself as _Lens Controller cef135_ when connected.

## 2. Lens Does Not Focus at All

If the lens does not focus and the image remains static when using INDI/Ekos Focus module, it is likely because the lens has not been calibrated and does not know its full focus range.

Open the INDI Control Panel and locate the _Pinefeat EF Lens Controller_ or _Pinefeat CEF_ tab. On the Main Control tab, click Connect, then click Calibrate.

A valid, calibrated lens will record a positive value (e.g., 1203) in the Max position text field. If you see 0 there, the lens has not been calibrated. 

You only need to do this once per lens. 
