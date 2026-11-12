# OnStepX INDI Driver

INDI driver for the [OnStepX](https://github.com/hjd1964/OnStepX) telescope controller firmware.

## Overview

OnStepX is a general-purpose telescope controller. A mount is one optional feature — the
controller may also run without a mount (focuser-only, rotator-only, or accessory-only
installations). This driver provides two binaries to match:

| Binary | Group | Use for |
|---|---|---|
| `indi_onstepx` | Telescopes | OnStepX with a mount (GEM, Fork, AltAz, AltAlt) |
| `indi_onstepx_aux` | Auxiliary | OnStepX without a mount |

Both binaries share all source files except a single entry-point.

## Hardware Requirements

- OnStepX firmware v10.24c or later (earlier versions may work but are untested)
- Serial (USB) or TCP/IP connection to the controller
- At least one of: mount, focuser, rotator, or aux output configured in the firmware

## Features

### Both Binaries
- Focusers (up to 6 slots, each appears as a separate INDI Focuser device), with home
  (go to / set) and the firmware's temperature compensation when the focuser has a
  temperature reading
- Rotator (a separate INDI Rotator device): goto, abort, home, backlash, presets and sync
  (to 0 deg or half travel), with optional de-rotator, its direction, and parallactic
  tracking (AltAz only)
- Auxiliary outputs and USB plugin ports as a separate INDI Power device: switches and
  USB ports through INDI's standard PowerInterface (labelled with the firmware names),
  plus PWM analog outputs, dew heaters and intervalometers
- Weather sensors (temperature, pressure, humidity, dew point, MCU temperature)
- Motor driver status per axis (TMC stepper drivers, driver fault pins, servo drivers), with
  faults logged when they appear and clear

### Mount Binary Only
- Equatorial and AltAz mounts (GEM, Fork, AltAz, AltAlt)
- Goto, sync, park, abort, home find/set
- Pulse guiding (GuiderInterface), with the guide rate in INDI's standard `GUIDE_RATE`
- Tracking: sidereal, lunar, solar, King; refraction/full compensation; rate nudge
- Meridian flip: INDI's Flip slew mode, auto-flip, preferred pier side, a notice before
  the west meridian limit, and Continue for a goto paused at home (see below)
- Horizon and meridian limits
- PEC playback and recording
- N-star geometric alignment, and Refine Polar Align (`:MP#`) on equatorial mounts
- Pointing-model builder: collect Sync observations, fit the full OnStepX pointing model
  in the driver and upload it to the firmware (validated, with rollback on failure)
  While a session is open, each Sync is recorded as an observation and does not sync the mount;
  the Model tab shows the count (Busy while the session is open).
  Like the firmware's own alignment, the model gains terms as observations arrive: index
  offsets from 2, polar axis alignment from 3, cone error from 4, axis non-perpendicularity
  and flexure from 6, and periodic errors from 12. A level is used only if its terms improve
  the fit significantly, so a few observations don't produce terms that just fit their noise.
  After each Sync the Model tab shows the model so far (terms, RMS and maximum residual) and
  what more observations would add.
  A session suspends the firmware model, so the original is also saved to
  `~/.indi/<device>_model_backup.txt`; if the driver dies mid-session, the next connect puts it back.
- Polar error display

## Building

### Dependencies

```
libindi-dev >= 2.0
libnova-dev
cmake >= 3.13
```

Install on Debian/Ubuntu:

```bash
sudo apt install libindi-dev libnova-dev cmake
```

### Build

```bash
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DINDI_BUILD_UNITTESTS=ON
cmake --build . --target indi_onstepx --target indi_onstepx_aux
```

With `INDI_BUILD_UNITTESTS=ON`, INDI downloads and builds its pinned GoogleTest release;
set `GTEST_ROOT` to a local googletest source tree to build from that instead.

### Run Unit Tests

```bash
ctest -R test-onstepx --output-on-failure
```

## Architecture

```
indi_onstepx (mount)          indi_onstepx_aux (auxiliary)
     |                               |
OnStepXMount                   OnStepXAux
     |                               |
     +---- OnStepXCore (shared, owns OnStepXComm + Capabilities)
     |
     +---- OnStepXAlignment   (N-star alignment)
     +---- OnStepXModelBuilder (pointing-model build sessions)
     +---- OnStepXPowerDevice  (child INDI device; owns OnStepXAuxFeatures + OnStepXUsbPlugin)
     +---- OnStepXDriverStatus (motor driver status per axis)
     +---- OnStepXFocuser x6   (child INDI devices)
     +---- OnStepXLimits       (horizon/meridian/home)
     +---- OnStepXMeridian     (Flip, meridian notice, paused-at-home)
     +---- OnStepXPec          (PEC playback/recording)
     +---- OnStepXRotatorDevice (child INDI device; owns OnStepXRotator: rotator + de-rotator)
     +---- OnStepXSite         (location/time sync)
     +---- OnStepXStatus       (mount status parser)
     +---- OnStepXTracking     (advanced tracking control)
     +---- OnStepXWeather      (sensor polling)
```

All protocol commands are implemented in the individual helper classes. All I/O goes
through `OnStepXComm`, which serialises access with a mutex and handles LX200 framing.
It has one call per firmware reply type (text `value#`, bare `1`/`0`, no reply, single
character, binary); see the header comment in `OnStepXComm.h`.

## Meridian flips

The firmware and the client can each manage flips, so set them up to work together.

- **The firmware.** While tracking west of the pier, the mount reaches the west meridian
  limit (Limits tab, default 60 min past the meridian). There it flips by itself if
  Meridian Auto Flip is on, and stops tracking if it is off. A goto chooses its pier
  side from the pier preference (Limits tab): Best (the default) keeps the current
  side while the target is reachable; East and Auto change to the east side once the
  target is past the meridian.
- **Ekos.** Ekos flips after its own offset past the meridian, and pauses capture and
  guiding around the flip. By default it flips by re-sending a goto to the target, and
  with pier preference Best that goto does not change sides. Ekos then reports the flip
  as failed and retries every 4 minutes.
- **The Flip slew mode.** The driver offers INDI's Flip (`ON_COORD_SET` `FLIP`), which
  sends `:MN#`: a goto to the current position on the other side of the pier, whatever
  the pier preference. Ekos uses it when "Use flip command if supported by mount"
  (Capture options) is on.

For imaging with Ekos, either turn on Ekos's flip command option or set the pier
preference to East. Keep Ekos's flip offset well inside the west meridian limit. Leave
auto flip off, or on only as a backstop: a flip by the firmware is not coordinated with
capture or guiding. The pier preference and auto flip are saved in the INDI config.

When tracking west of the pier passes the meridian, the driver logs when the west limit
will be reached, what the firmware will do there, and whether a goto will change sides
with the current pier preference. Controller errors (a limit reached, a motor fault,
a start-up failure) are logged when they appear and when they clear.

With Pause at Home on (Limits tab, or the hand controller or web UI), a goto that
changes sides stops at home and waits. The driver warns, shows Paused at Home as busy,
and its Continue button resumes the goto. Leave it off for unattended flips.

## Connection

Both drivers support Serial and TCP connections via the standard INDI connection plugins.
The driver verifies the firmware identity (`:GVP#` must return `"On-Step"` and `:GVN#`
a major version of 10 or more) and probes capabilities at connect time. Probing is
read-only. No manual configuration of feature flags is required.

The default TCP port is 9999, the SmartWebServer command channel (9996–9998 are its
persistent channels).

If the controller stops replying (power lost, cable pulled, network dropped), the driver
warns once and keeps retrying, so a reboot of the controller is ridden out. After 30 s
with no reply it logs an error and disconnects; the connection shows Alert. Reconnect
once the controller is back.

## Trying it without hardware

The driver has no simulation mode of its own; a second model of the firmware inside the
driver would drift from the real one. To try the driver without a controller, use the
OnStepX simulator (https://github.com/LuckyEddie47/onstepx-simulator), which answers the
command protocol on a pseudo-terminal:

```bash
onstepx-sim configs/gem_full.h --verbose     # prints "[sim] PTY: /dev/pts/N"
```

Then connect `indi_onstepx` (or `indi_onstepx_aux` with a mount-less configuration) to
that `/dev/pts/N` as its serial port. The simulator covers a subset of the protocol and
may lag the firmware, so it can produce errors that real hardware does not. For testing
changes to the driver, the unit tests and the hardware tests in `test/onstepx/hardware`
are the reference.

## Notes

- The mount binary expects a mount to be configured in OnStepX firmware. If no mount is
  detected, connection is refused with an error message directing the user to
  `indi_onstepx_aux`.
- The outputs are a child device ("OnStepX Power") because INDI's PowerInterface needs the
  output count at `initProperties()` time, before the parent device has queried the
  firmware; the child is created after the probe.
- The `:Gu#` binary status layout follows the firmware's `Status.command.cpp`
  (see `OnStepXStatus::parseGu()`). The driver falls back to the ASCII `:GU#` path
  automatically if the binary read or parse fails.
