# OnStepX hardware tests

Tests that run the driver against a real OnStepX controller. They catch what
the unit tests cannot: the unit-test mocks script replies from the driver's
point of view, so a misread protocol passes there and fails here.

> [!WARNING]
> **Use a test controller only, never a mount carrying a telescope.** These
> tests slew, flip, park and home the mount, and change the site, time,
> limits, pointing model and other settings the controller keeps in NV. A
> slew past the limits can drive a real telescope into the pier. An ESP32 dev
> board with nothing attached is ideal: OnStepX runs its axes without motors.
>
> Every test that talks to a controller refuses to start without
> `--test-controller`, which confirms the device on `--port` is such a board.
> The checks that need no hardware (`--check-source`, `--self-test`) don't need it.

## Tools

| Script | What it checks |
|---|---|
| `onstepx_conformance.py` | Every command the driver sends gets the reply type the driver expects (text `#`, bare `1`/`0`, none, single char, binary). Settings are read and written back. |
| `onstepx_conformance.py --check-source` | No hardware needed. Fails if the driver sends a command that has no conformance check, or if `drivers.xml` doesn't list the version in `OnStepXVersion.h`. Run it after adding a command. |
| `onstepx_e2e.py` | Drives `indi_onstepx` through INDI as a client would, then disconnects and reads the controller directly to confirm what reached the firmware. Fails on any driver error or warning. |
| `onstepx_controls_e2e.py` | The controls `onstepx_e2e.py` leaves out, on the same firmware. Checks that an INDI config load (as Ekos sends on connect) keeps the firmware's NV settings and restores its RAM ones, that no slew rate changes the NV pulse-guide rate, West/East motion and pulses, reverse motion, park options, tracking compensation, the sidereal clock, limits, backlash, site name, two-star alignment, PEC, focuser and rotator abort/sync/backlash/presets, reconnect cycles and a controller reset while connected. Leaves an alignment model in NV, so erase NV afterwards. Takes about 40 minutes. |
| `onstepx_model_e2e.py` | Full pointing-model session: about 18 Goto/Sync observations with a known injected error, then Calculate and upload. It then checks on the controller that the model persisted and that the firmware applies it as the fitter intended (instrument axes agree within 30″). Takes about 40 minutes. |
| `onstepx_altaz_e2e.py` | Needs **ALTAZM** firmware. Checks AltAz coordinates (Sync and Goto through `:GR#`/`:GD#`), "go to parallactic angle" against the firmware's formula, that the de-rotator follows field rotation and stops when switched off, and that the reversed de-rotator turns the other way. |
| `onstepx_aux_e2e.py` | Needs **mount-less** firmware. Checks that `indi_onstepx` refuses the controller, then exercises `indi_onstepx_aux` (polling, focusers, rotator, aux features, USB, weather). |
| `onstepx_ekos_e2e.py` | KStars/Ekos drives the mount over D-Bus while the driver stays connected, and each step is checked on the firmware through the serial tap. It starts its own isolated KStars and indiserver. See [Ekos tests](#ekos-tests-and-the-serial-tap). |
| `onstepx_ekos_imaging_e2e.py` | Ekos autofocus with Focuser 1, then guide calibration, guiding and a dither through the mount's pulse-guide output, with the CCD Simulator as the camera. Same isolated session and tap as `onstepx_ekos_e2e.py`. See [Ekos focus and guiding](#ekos-focus-and-guiding). |
| `onstepx_tap.py` | Serial tap: gives the driver a virtual port and lets a test query the controller between the driver's commands. Used by the Ekos test, and runs on its own to capture a wire log of any session. `--self-test` checks its framing against a fake controller, with no hardware. |
| `onstepx_wire.py --self-test` | No hardware needed. The reply type of every command form the driver sends, used by the tap. The conformance test checks each entry against real firmware. |
| `onstepx_safety.py` | Not a test. The `--test-controller` guard the tests above share. |

All of them need `pyserial`. The INDI tests also need `indi_getprop` and `indi_setprop`.

```bash
python3 onstepx_conformance.py --port /dev/ttyUSB0 --test-controller [--baud 9600] [--log wire.log]
python3 onstepx_conformance.py --port 192.168.7.49:9998 --test-controller    # a network command channel (WiFi/Ethernet)

indiserver -p 7625 build/drivers/telescope/onstepx/indi_onstepx &
python3 onstepx_e2e.py --port /dev/ttyUSB0 --test-controller --indi-port 7625
python3 onstepx_e2e.py --port 192.168.7.49:9998 --test-controller --indi-port 7625   # over the network
```

The end-to-end test takes about 15 minutes, mostly waiting for simulated slews,
focuser moves and the rotator.  For the aux test, run both drivers under one
server: `indiserver -p 7625 .../indi_onstepx .../indi_onstepx_aux`.  To run a
second server alongside another one, give it its own local socket with
`-u /tmp/some-other-name`.

## Ekos tests and the serial tap

`onstepx_ekos_e2e.py` tests the driver the way most people use it: from
KStars/Ekos.  Ekos drives the mount through its Mount module, and the test
checks each step on the firmware while the driver is still connected.

```text
KStars/Ekos ---- D-Bus ---- onstepx_ekos_e2e.py
     |                            |
 indiserver                       |  tap.query(":GU#")
     |                            |
indi_onstepx --- pty --- onstepx_tap.py --- /dev/ttyUSB0 --- OnStepX
```

```bash
python3 onstepx_ekos_e2e.py --port /dev/ttyUSB0 --test-controller            # up to about 20 minutes
python3 onstepx_ekos_e2e.py --port /dev/ttyUSB0 --test-controller --flip     # no flip tests, about 5 minutes
python3 onstepx_ekos_e2e.py --port /dev/ttyUSB0 --test-controller --flip FLIP_COMMAND PIER_EAST PIER_BEST
```

It needs GEM test firmware, KStars, `indiserver`, `python3-dbus` and
`pyserial`. It starts everything itself and leaves the wire log, the KStars
log and the indiserver log in the work directory it prints.

**Isolation.** KStars runs offscreen on a private D-Bus daemon with its own
config and data directories, and indiserver runs with its own `HOME`. Your
KStars profiles and settings, `~/.indi`, and any KStars you have open are not
touched. The private bus starts no services, so KStars cannot reach your
keyring or other desktop services through it.

**What it checks:** before the driver connects, the test stops any goto
and unparks the controller through the tap, so every run starts the same way.
- Ekos connects through a remote profile, and KStars pushes the site and time
  to the controller.
- A park that the firmware refuses (requested mid-slew) is reported, and the
  driver survives it. This once crashed the driver, because INDI's park switches
  start with neither on.
- Unpark, goto (Ekos sees slewing, then tracking), sync and abort mid-slew.
  Ekos's coordinates and pier side agree with the firmware.
- A goto below the horizon is refused by the firmware, the driver reports it,
  and the mount does not move.
- The motor driver status property appears only if the firmware reports driver
  status (the test board's step/dir drivers do not).
- Refine Polar Align: a polar error is set on the firmware through the tap, then
  pressed through KStars. The driver sends `:MP#`, the firmware zeroes the polar
  terms and re-slews, and a second press is refused with nothing to remove.
- Standard properties clients look for: `GUIDE_RATE`, and the rotator device's abort,
  home, backlash and sync. Rotator sync to 0 deg through KStars.
- Focuser 1: Set as Home then Go to Home; temperature compensation is offered
  only when the focuser has a temperature reading.
- Pause at home: with it on, a goto that changes pier side waits at home until
  Continue is pressed in KStars, then finishes on the other side.
- Meridian flips by Ekos, once per case given to `--flip` (default `FLIP_COMMAND
  PIER_EAST`). `FLIP_COMMAND` turns on Ekos's "use flip command" option over D-Bus,
  so Ekos uses the driver's Flip (`:MN#`). A pier preference has Ekos flip by goto
  with that preference set. East and Auto must change sides. Best and West must not:
  the test then checks that Ekos reports the failed flip and that the driver
  logged its meridian notice.
- Park and disconnect, and the driver stops talking to the controller.
- Every driver command is in `onstepx_wire.py`, indiserver never had to restart
  the driver, and the driver logged no unexpected errors or warnings.

Meridian flips are switched on for the whole run (offset 0) in the isolated
`kstarsrc`, because Ekos ignores the Mount module's `setMeridianFlipValues()`
over D-Bus. Ekos's flip command option (`ForcedFlip`) does work over D-Bus
(`/KStars` `setOption`), so the test switches it per case. See the driver
README for how the firmware and Ekos flip.

### Ekos focus and guiding

`onstepx_ekos_imaging_e2e.py` runs the same isolated session with the CCD
Simulator added to the indiserver. The simulator draws its star field at the
mount's coordinates, snooped from the driver, and blurs it by how far the
OnStepX focuser is from a focus position the test sets (`SIM_FOCUSING`). So
Ekos's autofocus moves the real focuser stepper, and Ekos's guide pulses move
the real mount axes, which moves the stars in the next frame.

```bash
python3 onstepx_ekos_imaging_e2e.py --port /dev/ttyUSB0 --test-controller      # about 8 minutes
```

It needs Focuser 1 on the test firmware, and the CCD Simulator with the GSC
catalogue. By default it uses the simulator built in `indi/build`, if there is
one.

- Ekos builds its optical train from the devices it finds but leaves the
  focuser out, so the test adds the focuser and guider to KStars's database
  and restarts KStars once.
- The focuser goes out to 29500, then back to 21500, 1500 steps from focus at
  20000. Its position is published during the move, not only at the end.
- Ekos autofocus (Linear 1 Pass, 500-step steps) completes within one step of
  the simulated focus, and the firmware and INDI agree on where it ended.
- Ekos calibrates and guides with the mount as the guider, and a dither
  completes. Pulses go out in all four directions, and every correction Ekos
  logs while guiding reaches the firmware as `:Mg<dir><ms>#` with its
  direction and duration unchanged. Ekos passes through its calibration and
  dither results faster than a 1 s poll, so the test reads them from the
  KStars log.
- The guiding RMS is printed but not checked. It depends on Ekos's guide
  algorithm and the simulator, where Ekos overshoots with 5 s pulses (about
  10-14" RMS), not on the driver.
- The mount is parked by Ekos, and the end-of-run checks are as above.

**Simulator settings.** The CCD Simulator draws stars from the mount's
coordinates, but how well Ekos can measure them depends on its settings, and
most failures while developing this test came from them, not the driver:
- A 1000 mm scope (1.07"/px on the main chip). At 400 mm, 2-3" stars were one
  pixel wide: the focus curve was flat and Ekos lost the guide star.
- `SIM_FOCUS_MAX` 20000. It only scales the blur (0.5625 * (20 * offset /
  SIM_FOCUS_MAX)^2 arcsec on top of the seeing), not the focuser's travel.
  At 50000 the curve was too flat; at 10000 the stars vanished at the outer
  end of Ekos's scan.
- 3" seeing and saturation magnitude 10 for focus (the default, 1, leaves the
  stars too faint to measure). 4.5" and 11 for guiding, because the guide
  chip's pixels are larger: at 3" its stars are undersampled, at 6" too faint.

**The tap.** `onstepx_tap.py` holds the controller's port and gives the driver
a pseudo-terminal. It forwards bytes unchanged in both directions. A test's
queries go out only when no driver transaction is open. Driver commands are
never held back: one that arrives while a query is outstanding goes straight
on, and the replies are matched in order, because the firmware answers each
channel's commands in the order they arrive. (Holding them was tried first: a
query caught in an NV stall then made the driver time out on a command it
had not even sent yet.) The
tap knows where each transaction ends from the reply types in
`onstepx_wire.py`. It records a problem if the driver sends a command that is
not in that table, or if a reply arrives when none is expected. The driver
sends one command at a time, so when it sends a new command while an older
one is still unanswered (it timed out, typically in an ESP32 NV stall), the
tap closes the old one and expects its reply may still turn up late.

To capture a wire log of any session, including KStars run by hand, run the
tap on its own and point the driver at its port:

```bash
python3 onstepx_tap.py --port /dev/ttyUSB0 --link /tmp/onstepx-tap --log wire.log
```

When you add a command to the driver, add its form to `onstepx_wire.py`
(rules and `SAMPLES`) as well as to the conformance checks.

## Test firmware

The tests expect this configuration (missing features are reported as SKIP):

- GEM mount with PEC (`PEC_STEPS_PER_WORM_ROTATION 32000`)
- rotator (Axis3), focusers 1 and 2 (Axis4, Axis5)
- aux features: 1 Switch, 2 Analog, 3 Dew heater, 4 Intervalometer, 5 Switch.
  Feature 5 has the same name as feature 1, to test property-name collisions.
- USB plugin ports 1 and 3, leaving a gap at port 2
- no weather sensor, so `:GX9A#` reports `nan` until a manual value is set
- `SERIAL_A` at 9600 (the OnStepX default), `SERIAL_B` off, debug off. All the
  scripts default to `--baud 9600`, where this board's USB link loses fewer
  reply bytes than at 115200 (see below).

For the AltAz variant used by `onstepx_altaz_e2e.py`, set `MOUNT_TYPE ALTAZM`
and `PEC_STEPS_PER_WORM_ROTATION 0`.

For the mount-less variant used by `onstepx_aux_e2e.py`, also set
`AXIS1_DRIVER_MODEL OFF`, `AXIS2_DRIVER_MODEL OFF` and
`PEC_STEPS_PER_WORM_ROTATION 0`.

To build it:

1. Clone OnStepX master and apply `firmware-test-config.patch`.
2. Copy `usb/` from OnStepX-Plugins into `src/plugins/usb`, and set `USB1_PIN 16` and `USB3_PIN 17` in its `Usb.h`.
3. Set `src_dir` in `platformio.ini` to that checkout.
4. Build, erase and flash. **Erase first**, because old NV data changes behaviour:

```bash
pio run && pio run -t erase && pio run -t upload
```

The first boot after an erase initialises NV and is noisy for a minute or so.
Expect stalls and junk on the serial line, and run the tests after that.

Opening the port does not reset this board (tested 2026-11-10 with pyserial, a
plain `open()` and the driver: a setting the firmware forgets on reboot
survived). Boards wired differently may reset when DTR/RTS change. Bytes sent at
the wrong baud rate can leave junk in the firmware's command buffer, so the
next command may get a bare `0`.

The patch makes three changes beyond configuration. Each works around an upstream problem:

| Change | Why |
|---|---|
| `#define SERIAL_TMC Serial1` | Plain step/dir drivers (A4988) don't build on ESP32: the TMC sources are always compiled, and they default to `SoftwareSerial`, which the ESP32 core lacks. |
| Focusers 1 and 2 both enabled | A focuser gap (Axis5 without Axis4) crashes 10.29c at boot: `Focuser::init()` dereferences `axes[index]` for absent lower slots. |
| `Rotator.cpp`: park state constrained from `PS_UNPARKED` | Upstream constrains it from `PS_PARK_FAILED`, so after every boot the rotator refuses moves and backlash changes as "parked". |

## Known firmware behaviour seen in testing (10.29c, ESP32)

- Over USB (CP2102 bridge), a reply sometimes loses one byte, and 64 bytes of
  `0xFF` arrive in its place, sometimes about 2 s later. In targeted tests
  (2026-11-10, 9600 baud, same firmware) it happened 5 times in 500 iterations
  over USB and never in 500 over WiFi, and it was just as likely without any
  motion, focuser or NV commands. At 115200 it was roughly three times as
  frequent. The driver drops `0xFF` from text, bool and char replies and
  resyncs after a short reply.
- `Home::reset()` (Find Home, Set Home, alignment start) clears the custom
  tracking-rate offsets. The driver re-applies them while `TRACK_CUSTOM` is selected.
- Rotator travel limits on this build are 0–360°. The rotator moves at roughly 1°/s.
- Without a weather sensor, a freshly erased controller reports `nan` for
  temperature until manual values are set. The driver always offers the
  manual-weather property, and the readings appear on the next connect.
