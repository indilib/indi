# Proxisky UMi

## Device Overview

[Proxisky](https://www.proxisky.com) UMi mounts (UMi17X, UMi20S and relatives) are compact
strain-wave star trackers / GoTo mounts. They run **OnStep-derived firmware**, so all of the
standard telescope behaviour — slewing, tracking, guiding, parking, alignment, meridian flips, the
focuser and rotator interfaces — is exactly OnStep's and is handled by INDI's existing
`LX200_OnStep` driver.

`indi_lx200_proxisky` is a thin subclass of that driver. It adds only the vendor-specific `:P...`
serial commands that Proxisky layered on top of OnStep and that stock OnStep does not implement:
travel limits, the anti-collision system, the servo PID gains, and a handful of board settings.

**If you are looking for how to slew, track or guide, the OnStep driver's documentation applies
unchanged** — see the [OnStep project documentation](https://onstep.groups.io/g/main/wiki) and
INDI's own `LX200 OnStep` driver page. This page covers only what the Proxisky layer adds.

## Features

Everything below appears on one of four `Proxisky *` tabs in the INDI Control Panel. Each control is
published **only if the mount answers the corresponding query**, so the panel reflects what your
particular model and firmware actually support (see
[If a control is missing](#-if-a-control-is-missing) — this detection has one important caveat).

| Feature | What it does |
|---|---|
| **Model / firmware** | Reports the mount's own identification string, e.g. `UMi20S\|1.0.6` |
| **RA travel limits** | Left/right rotation limits in degrees, plus an enable switch |
| **Dec travel limits** | CCW/CW limits in degrees, plus an enable switch |
| **Anti-collision (ACS)** | Enable, per-axis sensitivity thresholds, collision counters, counter reset |
| **Servo PID gains** | Four gains per axis (angle Kp/Ki, speed Kp/Ki) for RA/Azm and Dec/Alt |
| **Supervised GOTO** | Enable plus a tolerance in degrees |
| **Supervised Home** | Enable (see the [caveat](#supervised-home-may-be-refused)) |
| **Dec second home** | Enable |
| **Status LED** | Turns the board's status LED on or off |
| **Auto tracking** | Whether the mount starts tracking by itself |
| **Power-loss memory** | Whether the mount restores its position after a power cut |
| **ASIAIR homing** | Compatibility behaviour for ASIAIR controllers |

## Installation

The driver ships with INDI core. There is nothing extra to build and no dependencies beyond INDI
itself.

No additional udev rules are needed. UMi mounts present themselves through a standard USB-serial
adapter, which Linux exposes as `/dev/ttyUSB*` and INDI detects automatically.

To run it directly:

```shell
indiserver -v indi_lx200_proxisky
```

In KStars/Ekos, choose **Proxisky › UMi** in the profile editor.

### Migrating from an older profile

**Read this if you used a UMi with INDI before this driver existed.**

The `Proxisky › UMi` catalogue entry previously launched the generic `indi_lx200_OnStep` binary
under the device name **`LX200 OnStep`**. It now launches `indi_lx200_proxisky` under the device
name **`Proxisky UMi`**.

INDI keys saved settings by *device name*, so nothing carries over automatically:

| What | Old location | New location |
|---|---|---|
| Driver settings | `~/.indi/LX200 OnStep_config.xml` | `~/.indi/Proxisky UMi_config.xml` |
| Park position | `~/.indi/ParkData.xml`, `<device name="LX200 OnStep">` | same file, `<device name="Proxisky UMi">` |

Practically this means: **re-select the port, re-save your configuration, and re-set your park
position** the first time you connect. Your old files are not deleted and the old driver still works
if you select `LX200 OnStep` manually — you simply get no vendor controls.

## Configuration

### Connection

| | |
|---|---|
| **Serial** | Default. **9600 baud, 8N1** — this is what the mount uses; leave the baud rate at its default unless you have a specific reason. |
| **TCP/IP** | Supported (inherited from `LX200_OnStep`) for network-attached mounts. |

Connect as usual from the **Main Control** tab. Vendor detection runs once during connect and adds a
second or so on serial.

### The Proxisky tabs

| Tab | Contents |
|---|---|
| **Proxisky Settings** | Model, Status LED, Auto Tracking, Power-loss Memory, ASIAIR Homing, Supervised Home, Dec Second Home |
| **Proxisky Limits** | RA and Dec travel limits and their enable switches |
| **Proxisky ACS** | Anti-collision enable, sensitivity, collision counters, counter reset |
| **Proxisky Advanced** | Supervised GOTO and tolerance, RA/Azm and Dec/Alt PID gains |

### What Left / Right and CCW / CW mean

The travel-limit fields use **the vendor's own terms**, taken directly from the Proxisky Windows
tool. Nothing in the protocol establishes a mapping to East/West or to any sky direction, and the
mapping may well differ between mount models and between EQ and Alt-Az configurations.

**Determine empirically which way each limit points before relying on it**, with the mount somewhere
safe and nothing attached that can collide. The driver reports what the mount stores; it does not
reinterpret it.

### Value ranges

The driver enforces the same ranges as the vendor's own tool:

| Setting | Range |
|---|---|
| RA limits (left, right) | 1 – 180° |
| Dec limits (CCW, CW) | 90 – 200° |
| ACS threshold, RA | 10 – 59999 |
| ACS threshold, Dec | 100 – 59999 (note: **not** the same floor as RA) |
| Supervised GOTO tolerance | 5 – 30° |
| PID angle Kp/Ki, speed Kp | 11 – 1999 |
| PID speed Ki | 11 – 80 |

These come from the vendor tool's validation, **not** from the firmware. An in-range
value can still be refused by the mount — which is why every write is read back and verified. If the
mount refuses or stores something different, the property turns red (`Alert`) and the log says what
the mount actually holds.

## Usage & Tips

### ⚡ Settings that need a power cycle

Some settings are stored immediately but only take effect at the next power up. For these the
property goes green (`Ok`) — the write itself succeeded — and the message says the rest:

> *Status LED will be enabled after the mount is power cycled.*
>
> *RA/Azm PID gains stored. Power cycle the mount for them to take effect.*

**Power cycle required:** Status LED · Auto Tracking · both PID vectors.

**Effective immediately:** Power-loss Memory · ASIAIR Homing · Supervised Home · Dec Second Home ·
Supervised GOTO · RA/Dec limit enables · ACS enable and thresholds.

This is why the panel can legitimately disagree with the mount's current behaviour: it shows what is
*stored*. That is deliberate, not a bug.

### PID gains

Changing PID gains is a genuine servo-tuning operation and can leave the mount unable to track if
you get it wrong. **Write down the values before you change them.**

Two behaviours to expect:

- **The driver refuses to write while the mount is moving.** The commit command restarts the axis,
  so writes are only accepted with the mount idle or parked and no manual motion in progress.
- **A brief pause after committing.** The axis restart makes the mount report all four gains as zero
  for roughly 70 ms. The driver waits this out before verifying, so you will not see a spurious
  mismatch.

Gains take effect after a power cycle.

### If a control is missing

This is the most likely surprise, so it is worth understanding.

Most of this protocol has **no capability query**. The only way to find out whether a mount supports
a feature is to ask for its current value and see whether anything comes back. An unsupported
feature and a *dropped reply* therefore look identical.

Detection runs **once, at connect**. The driver retries each query once before giving up, which
makes a spurious miss unlikely — but if a control you expect is absent:

> **Disconnect and reconnect.** That re-runs detection.

The log records every feature that stayed silent, at `Warning` level:

```
[WARNING] Dec second home did not answer (:PsDgb#), even after a retry;
          its control is hidden. Reconnect to probe again.
```

If a control is *consistently* absent across reconnects, your mount or firmware genuinely does not
have it.

Note also that Proxisky's own documentation labels some features "UMi17X only" that a UMi20S
demonstrably seem to support.

### Supervised Home may be refused

On at least some UMi20S firmware, the mount **answers** the Supervised Home query — so the control
is published — but **refuses every attempt to change it**:

```
[ERROR] The mount refused the Supervised Home change.
```

The driver is behaving correctly: it reports the refusal and puts the switch back to what the mount
still holds. There is nothing to fix on the INDI side; the firmware is declining the write.

### Anti-collision counters

The collision counters are read-only and polled in the background (every fifth status cycle, so
roughly every five seconds at the default polling rate) while ACS is enabled. If the mount stops
answering the counter query, the driver stops asking for the rest of the session rather than
stalling every status update — reconnect to resume.

### Simulation mode

**Simulation mode exposes no Proxisky properties at all**, and this is deliberate.

Every vendor property mirrors a value read back from real hardware; in simulation there is nothing to
read, and inventing values would mean the panel showed settings that no mount holds. In simulation
you get the standard OnStep simulated telescope and nothing else. This is not a bug.

### Do not use the plain "LX200 OnStep" driver

A UMi will connect and work under the generic `LX200 OnStep` driver — you simply get none of the
vendor controls, with nothing in the UI to indicate they exist. If your Proxisky tabs are missing
entirely, check which driver your profile actually selected.

Conversely, do **not** select `Proxisky UMi` for a non-Proxisky OnStep mount. The vendor commands
are meaningless to stock OnStep firmware.

### Logging

To see the raw vendor exchange, enable debug output on the **Options** tab, or:

```shell
indi_setprop 'Proxisky UMi.DEBUG.ENABLE=On'
indi_setprop 'Proxisky UMi.DEBUG_LEVEL.DBG_DEBUG=On'
```

Every vendor command and its reply is then logged, along with the reason any probe concluded a
feature was unsupported.

Note that `indiserver -v` does **not** relay driver messages to its own output — they go to
connected clients. Read them in the KStars INDI log window, or with a client such as
`indi_getprop`.

## Tested against

| | |
|---|---|
| Mount | Proxisky UMi20S |
| Firmware | 1.0.6 |
| Transport | Serial, 9600 8N1 |

All 19 vendor properties, every write path, the refusal and out-of-range paths, and connect /
reconnect / disconnect were exercised against this hardware. TCP transport and models other than the
UMi20S are supported by the same code but have not been tested on hardware — reports welcome.
