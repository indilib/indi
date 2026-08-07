# IKun Focuser

## Metadata

- Driver name: IKun Focuser
- Executable: `indi_ikun_focuser`
- Device type: Focuser
- Version: 1.0
- License: LGPL-2.1-or-later
- Connections: USB serial and TCP
- Platforms: Linux and macOS; ARM64 supported by source

## Overview

IKun Focuser is the INDI driver for the open hardware EFucoser electronic
focuser controller. The driver
supports the following controller and motor-interface combinations:

| Controller | Minimum firmware | Connection |
| --- | ---: | --- |
| ESP8266 STEP/DIR | 1005 | USB serial, TCP |
| ESP8266 ULN2003 | 1103 | USB serial, TCP |
| Arduino Nano ULN2003 | 1201 | USB serial |

The driver provides absolute and relative movement, abort, position
synchronization, maximum-travel configuration, direction reversal, speed,
acceleration, motor hold, and temperature reporting.

## Installation

When included in an INDI release, install the package containing INDI core
drivers using the package manager for the operating system. Start the driver
directly for diagnostic use:

```bash
indiserver -vv indi_ikun_focuser
```

## Hardware connection

### USB serial

1. Flash a supported EFucoser firmware.
2. Connect the controller with a USB data cable.
3. Ensure the user running `indiserver` has access to the serial device.
4. Select the controller port in the Connection tab.
5. Keep the serial rate at 9600 baud.

Common port names are `/dev/ttyUSB0` for CH340/FTDI bridges and
`/dev/ttyACM0` for USB CDC devices. USB bridge VID/PID values are shared by
many products, so the driver verifies the EFucoser identity during handshake.

### ESP8266 TCP

1. Connect the INDI host to the EFucoser access point or place both systems on
   the same configured Wi-Fi network.
2. Select TCP in the Connection tab.
3. Enter the controller address. The access-point default is `192.168.4.1`.
4. Set port `4030`.
5. Connect.

## Configuration

### Maximum travel

Set `Max. Position` to the safe mechanical range in motor steps. The controller
rejects targets outside `0..Max. Position`. Establish this value with the
focuser disconnected from delicate optical equipment when calibrating a new
mechanism.

### Speed and acceleration

Maximum speed accepts 1–2000 steps per second. Acceleration accepts 1–10000
steps per second squared. Start with conservative values such as speed 200 and
acceleration 100, then increase them while checking for missed steps or stalls.

### Reverse and motor hold

Reverse changes the physical direction associated with increasing logical
positions. Motor Hold keeps the coils energized after movement. Continuous
hold improves position retention and increases motor and driver temperature.

## Ekos use

1. Add `IKun Focuser` to an Ekos equipment profile.
2. Start INDI and open the IKun Focuser control panel.
3. Select Serial or TCP and connect.
4. Configure maximum travel, speed, acceleration, reverse, and hold.
5. Verify inward and outward movement with small step counts.
6. Save the INDI configuration.
7. Configure the Ekos Focus module step size and autofocus limits.

## Troubleshooting

### Connection fails

- Confirm the selected serial device or TCP address.
- Confirm firmware meets the minimum version.
- Use a USB cable that supports data.
- Check membership in the Linux `dialout` group.
- Run `indiserver -vv indi_ikun_focuser` and inspect the handshake log.

### Motor does not move

- Confirm external motor power.
- Confirm a shared ground between controller, driver, and motor supply.
- Confirm the firmware matches the motor driver type.
- Reduce speed and acceleration.
- Check ULN2003 phase order and STEP/DIR enable polarity.

### Position or direction is incorrect

- Set the focuser to a known mechanical position and use Sync.
- Toggle Reverse when increasing positions move in the undesired direction.
- Recalibrate Max. Position after changing gearing or coupling.

### TCP disconnects

- Confirm the host remains connected to the controller network.
- Confirm port 4030 is reachable.
- Avoid running multiple clients that open separate raw controller sessions
  while INDI is moving the focuser.

## Support and source

Firmware, wiring documentation, protocol details, and issue tracking:
https://github.com/Indigo2233/EFocusor
