# OAPA (Open Automatic Polar Alignment)

## Device Overview

OAPA is an open-source motorised polar-alignment platform: two stepper motors turn
the azimuth and altitude adjusters of an equatorial mount, so polar alignment can be
corrected without touching the knobs. The reference controller is an ESP32 board
(FYSETC E4 with two TMC2209 drivers) running the open
[OAPA firmware](https://github.com/michelebergo/oapa-firmware). Any device that speaks
the same serial protocol works with this driver.

The driver implements the INDI **PAC (Polar Alignment Correction)** interface, so the
Ekos Polar Alignment Assistant can drive corrections automatically.

## Features

- PAC interface: relative corrections in degrees on azimuth and altitude, both axes in
  one command when they share the same speed
- Motion completion reported when the platform is idle at the commanded target
- Abort (firmware >= 1.2.1)
- Adjustable speed per axis (50-3000 motor steps/s)
- Motor run and hold current per axis, sent on every connection (the controller forgets
  them at power-off)
- Per-axis direction reverse
- Position readout in degrees
- Safety: moves are refused until calibrated; a move that stalls, times out or stops
  short of its target is stopped and reported as an error

## Installation

The driver is part of INDI core and needs no extra dependencies. The controller must
run OAPA firmware **1.2.1 or newer** (1.2.2 recommended). Older firmware connects, but
speed and Abort are ignored and the driver logs a warning.

## Configuration

1. Connect the controller by USB. It appears as `/dev/ttyUSB0` or `/dev/ttyACM0`.
2. In the Ekos profile, select **OAPA** under Auxiliary.
3. In the INDI Control Panel, set the port on the **Connection** tab and click
   **Connect**. The board resets when the port opens; the driver waits for it.
4. Set **Calibration** (steps/arcmin) for both axes. Without it, every correction is
   refused. See *Calibration* below.

| Property | Meaning |
|----------|---------|
| Manual Adjustment | Relative move in degrees. AZ positive = East, ALT positive = North |
| Abort Motion | Stop both axes |
| Position | Platform position in degrees since power-on |
| Speed | Motor speed per axis in steps/s (default 1000) |
| Run Current (Motor tab) | Motor run current per axis in mA (default 600) |
| Hold Current (Motor tab) | Hold current per axis, % of run current (default 25) |
| Azimuth / Altitude Reverse | Invert an axis if it moves the wrong way |
| Calibration | Motor steps per arcminute of correction, per axis |
| Firmware | Firmware version reported by the controller |

## Usage & Tips

### Calibration

Steps per arcminute depend on your mechanics. Typical values range from about 15 to
about 1000. To measure one axis:

1. Set a rough value, e.g. `100`.
2. Run the Polar Alignment Assistant and note the error on that axis.
3. Enter a known step in **Manual Adjustment**, e.g. `0.1` degrees (6 arcminutes).
4. Refresh the solution and note how much the error actually changed.
5. New value = old value x requested change / measured change.

### Direction check

Before the first automatic run, enter `+0.1` in the azimuth element of **Manual
Adjustment** and confirm that the polar axis moves East (and `+0.1` altitude moves it
North). If not, enable the reverse switch for that axis.

### Automatic correction in Ekos

Requires KStars 3.8.2 or newer (where the feature is marked preliminary). In the Polar
Alignment Assistant, enable automatic PAC correction and choose the success threshold. Ekos then measures, commands the correction, waits for this driver to report
completion, and measures again until the error is below the threshold.

### Logging

Enable debug logging for the driver to see every command (`CMD`) and reply (`RES`) on
the serial line.

### Testing without hardware

The [indi-oapa](https://github.com/michelebergo/indi-oapa) repository contains a firmware
emulator on a pseudo-terminal and a contract test that drives `PAC_MANUAL_ADJUSTMENT` the
way Ekos does.
