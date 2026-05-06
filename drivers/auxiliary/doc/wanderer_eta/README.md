# Wanderer ETA M54 — INDI Driver Testing Guide

## Overview

INDI driver for the WandererAstro ETA M54 Electronic Tilt Adjuster. Communicates via USB serial (CH340 chip, 19200 baud) and provides control of the three motorized tilt adjustment points.

## Files Included in This Patch

**New files (copy into `indi/drivers/auxiliary/`):**
- `wanderer_eta.h`
- `wanderer_eta.cpp`

**Modified files (overwrite the upstream versions):**
- `indi/drivers/auxiliary/CMakeLists.txt`
- `indi/drivers.xml`

## Setup on Arch Linux

### 1. Install Build Dependencies

```bash
sudo pacman -S --needed base-devel cmake git \
  cfitsio libnova libusb gsl libjpeg-turbo curl
```

### 2. Clone INDI Core

```bash
mkdir -p ~/Projects
cd ~/Projects
git clone https://github.com/indilib/indi.git
cd indi
```

### 3. Apply the ETA Driver Patch

Copy the patched/new files on top of the fresh clone. Assuming you extracted `eta-patch.tar.gz` into `~/Projects/`:

```bash
cd ~/Projects/indi

# New driver files
cp ~/Projects/eta-patch/wanderer_eta.h   drivers/auxiliary/
cp ~/Projects/eta-patch/wanderer_eta.cpp drivers/auxiliary/

# Modified files (overwrite upstream)
cp ~/Projects/eta-patch/CMakeLists.txt   drivers/auxiliary/CMakeLists.txt
cp ~/Projects/eta-patch/drivers.xml      drivers.xml
```

Verify the files are in place:
```bash
ls -la ~/Projects/indi/drivers/auxiliary/wanderer_eta.*
grep -n "wanderer_eta" ~/Projects/indi/drivers/auxiliary/CMakeLists.txt
grep -n "Wanderer ETA" ~/Projects/indi/drivers.xml
```

### 4. Build

```bash
mkdir -p ~/Projects/build/indi-core
cd ~/Projects/build/indi-core

cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ~/Projects/indi

# Build just the ETA driver (fast)
make -j$(nproc) indi_wanderer_eta

# Install
sudo make install
```

Verify:
```bash
which indi_wanderer_eta
```

## Note on StellarMate / Flatpak Setups

On StellarMate, KStars typically runs as a flatpak while INDI drivers run natively on the host. For testing this driver you only need to build and install INDI core natively — you do **not** need to rebuild indi-3rdparty or KStars.

The workflow is:
1. Build & install the patched INDI core natively (gives you `indi_wanderer_eta` + `indiserver`)
2. Run `indiserver -v indi_wanderer_eta` manually on the host
3. Connect using `indi_control_panel` or any INDI client pointing at localhost:7624

**Important**: When KStars flatpak starts an Ekos profile in local mode, it launches its own `indiserver` with the drivers bundled inside the flatpak — our ETA driver won't be there. For initial testing, don't use KStars/Ekos. Instead, start `indiserver` manually on the host and connect with `indi_control_panel`. If you want to test through KStars, set the Ekos profile to **Remote** mode with the machine's **actual IP address** (e.g., `192.168.0.69`) and port `7624`, then start your native `indiserver` separately. Do NOT use `localhost` — that triggers Ekos to launch its own bundled indiserver which will conflict.

If you need other INDI drivers (cameras, mount, etc.) running alongside the ETA, make sure your existing native INDI installation is compatible. If you're currently using system-packaged INDI, installing our patched build to `/usr` will overwrite `libindi`. To avoid that, you can install to a separate prefix:
```bash
cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Debug ~/Projects/indi
make -j$(nproc) indi_wanderer_eta
sudo make install
# Then run with full path:
/usr/local/bin/indiserver /usr/local/bin/indi_wanderer_eta
```

## Testing with Hardware

### 1. Connect the ETA

Plug the Wanderer ETA M54 into a USB port. Verify the CH340 serial adapter appears:
```bash
dmesg | tail -5
# Should show: ch341-uart converter now attached to ttyUSB0

ls /dev/ttyUSB*
```

If you get permission errors later, add yourself to the `uucp` group (Arch uses `uucp` instead of `dialout`):
```bash
sudo usermod -aG uucp $USER
# Then log out and back in
```

### 2. Start the INDI Server

```bash
indiserver -v indi_wanderer_eta indi_simulator_ccd indi_simulator_telescope
```

This starts the ETA driver alongside a CCD and telescope simulator so you have a complete setup for testing in Ekos.

The `-v` flag enables verbose logging so you can see serial communication.

### 3. Connect with a Client

**Using INDI Control Panel:**
```bash
indi_control_panel &
```

**Using KStars/Ekos:**
1. Ekos → Profile Editor
2. Set mode to **Remote**
3. Set the host to the machine's **actual IP address** (e.g., `192.168.0.69`), port `7624`
4. **Important**: Do NOT use `localhost` — that will make Ekos try to start its own indiserver, which will fail because one is already running on port 7624
5. Start the profile — you should see the ETA, simulator CCD, and simulator telescope

### 4. Test Sequence

#### a) Connection & Handshake
1. In the Connection tab, select the correct serial port (e.g., `/dev/ttyUSB0`)
2. Click "Connect"
3. **Expected**: Driver reads the continuous status stream, validates `WandererTilterM54` identifier, connects
4. **Verify**: Firmware version appears (e.g., `20250318`)

#### b) Position Readback
1. Check the "Current Positions" fields after connecting
2. **Expected**: Three Point values show current encoder positions (likely near 0.000 if freshly powered)
3. **Verify**: Values update every ~2 seconds

#### c) Move Points
1. Set Point 1 target to `0.500`, click Set
2. **Expected**: Driver sends `10.500` to device, readback updates to ~0.500
3. Repeat for Point 2 (`0.250`) and Point 3 (`0.750`)
4. **Verify**: Readback matches target within ~0.002mm

#### d) Range Validation
1. Try setting a point to `1.500` (above max)
2. **Expected**: Error in log, property goes Alert, no command sent
3. Try `-0.1` (below min) — same expected behavior

#### e) Zero All
1. With points at various positions, click "Zero All Points"
2. **Expected**: All three points move to 0.000mm
3. **Verify**: Both target and readback show 0.000

#### f) Config Save/Load
1. Set points to known values (e.g., 0.100, 0.200, 0.300)
2. Options → Save Configuration
3. Disconnect, reconnect, Options → Load Configuration
4. **Expected**: Target positions restored

### 5. Debug Logs

With `-v` on indiserver you'll see:
- `CMD: 10.500` — commands sent
- `Data: WandererTilterM54A20250318A0.500A0.250A0.750A` — raw status received

For more detail, enable Debug in the driver's Options tab.

## Troubleshooting

- **"Timeout reading from device"**: ETA not powered or wrong serial port. Check `dmesg`.
- **"Unknown device: ..."**: Wrong WandererAstro device on that port (cover, rotator, etc.).
- **Permission denied**: Add user to `uucp` group (see above).
- **Build error "indidriver not found"**: Make sure you ran cmake from the indi root, not just the auxiliary directory.

## What to Report Back

1. Does the handshake succeed? (firmware version displayed)
2. Do position readbacks update correctly?
3. Do move commands work? (points reach requested positions)
4. Readback accuracy — within ~0.002mm of target?
5. Does Zero All work?
6. Any serial errors or unexpected behavior?
7. Exact firmware version string shown
