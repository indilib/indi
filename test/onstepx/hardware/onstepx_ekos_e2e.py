#!/usr/bin/env python3
# OnStepX INDI Driver — onstepx_ekos_e2e.py
# Copyright (C) 2026 Ed Lee
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

"""
End-to-end test of indi_onstepx driven by KStars/Ekos over D-Bus.

Everything a user's session would involve runs for real: KStars and Ekos,
an INDI server with the driver, and an OnStepX controller.  Ekos commands
the mount through its Mount module.  Each step is then checked against the
firmware itself, through onstepx_tap.py, while the driver stays connected.

    KStars/Ekos --D-Bus-- this script
         |                    |
     indiserver               | tap.query(":GU#") between driver commands
         |                    |
    indi_onstepx --pty-- onstepx_tap --/dev/ttyUSB0-- OnStepX

Isolation: KStars runs offscreen on a private D-Bus daemon, with its own
config and data directories, and indiserver runs with its own HOME.  Your
KStars profiles, settings and ~/.indi are not touched, and a KStars you have
open is not disturbed.

Prerequisites:
  * a TEST controller with no mechanics attached, running the GEM test
    firmware (see the README in this directory).  This test slews, syncs,
    parks, flips at the meridian and changes the site and pier preference.
  * KStars (tested with 3.8.4), indiserver, python3-dbus, python3-serial.
  * the driver built at build/drivers/telescope/onstepx/indi_onstepx, or
    passed with --driver.

Usage:
  onstepx_ekos_e2e.py --port /dev/ttyUSB0 --test-controller [--baud 9600] [--flip [CASE ...]]

Takes up to about 20 minutes, most of it in the meridian flip tests: each
waits for the target to cross the meridian, then for a 180 degree flip slew.
Ekos flips with the driver's Flip command (FLIP_COMMAND, its "use flip
command" option switched on over D-Bus) or by re-sending a goto with a given
firmware pier preference; Best and West keep the pier side there, and the
test then checks that Ekos notices.
Meridian flips are enabled (offset 0) for the whole run through kstarsrc,
because Ekos ignores setMeridianFlipValues() over D-Bus.  Logs (wire log, KStars, indiserver) are left
in the work directory printed at the start.
"""

import argparse
import datetime
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time

import dbus

sys.dont_write_bytecode = True      # importing the sibling scripts leaves no __pycache__

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import onstepx_tap                       # noqa: E402
from onstepx_e2e import E2E, DEV, ROT    # noqa: E402
import onstepx_safety  # noqa: E402

DEFAULT_DRIVER = os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "build", "drivers",
                                               "telescope", "onstepx", "indi_onstepx"))
PROFILE = "OnStepX test"
LAT, LON_EAST, ELEV = 51.5, -0.1, 50.0

# Ekos enums (KStars indi/indimount.h, indi/indistd.h, ekos/ekos.h)
MOUNT_STATUS = ["IDLE", "MOVING", "SLEWING", "TRACKING", "PARKING", "PARKED", "ERROR"]
PARK_STATUS = ["UNKNOWN", "PARKED", "PARKING", "UNPARKING", "UNPARKED", "ERROR"]
PIER = {-1: "UNKNOWN", 0: "WEST", 1: "EAST"}
EKOS_SUCCESS = 2

BUS_CONFIG = """<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:dir=%s</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
"""
# No <servicedir>: nothing is auto-started on this bus, so KStars cannot
# activate your keyring or other desktop services through it.


def sexa(s):
    """'+51*30:00.000', '12:52:05.1234', '-005*30:00' -> float."""
    s = s.strip().rstrip("#")
    neg = s.startswith("-")
    parts = s.lstrip("+-").replace("*", ":").replace("'", ":").split(":")
    v = 0.0
    for i, p in enumerate(parts):
        v += float(p) / (60 ** i)
    return -v if neg else v


def wrap_hours(h):
    return (h + 12.0) % 24.0 - 12.0


class Session:
    """KStars on a private bus, indiserver and the tap, all in one work directory."""

    def __init__(self, a, work):
        self.a = a
        self.work = work
        self.sock_dir = tempfile.mkdtemp(prefix="osx-ekos-")    # short path for UNIX sockets
        self.procs = []
        self.tap = None
        self.bus = None
        self.kstars = None

    def start_tap(self):
        self.link = os.path.join(self.sock_dir, "tap")
        self.tap = onstepx_tap.Tap(self.a.port, int(self.a.baud), self.link,
                                   log_path=os.path.join(self.work, "wire.log"))
        self.tap.start()
        return self.tap.wait_ready()

    def prepare_controller(self):
        """Stop any goto and unpark, so every run starts the same way.

        A previous run stopped mid-slew leaves a goto running, and opening the
        port does not always reset the controller.  Starting unparked also
        means INDI's park switches begin with neither on, the state behind a
        driver crash on a refused park (fixed in OnStepXMount)."""
        tap = self.tap
        tap.query(":Q#")
        for _ in range(60):
            if "N" in (tap.text(":GU#") or ""):
                break
            time.sleep(0.5)
        if "P" in (tap.text(":GU#") or ""):
            tap.query(":hR#")
            time.sleep(1)
        gu = tap.text(":GU#") or ""
        return "N" in gu and "P" not in gu, gu

    def start_indiserver(self, extra_drivers=()):
        home = os.path.join(self.work, "indi-home")
        os.makedirs(home, exist_ok=True)
        env = dict(os.environ, HOME=home)
        p = subprocess.Popen(["indiserver", "-p", str(self.a.indi_port), "-u",
                              os.path.join(self.sock_dir, "indiserver"), self.a.driver] + list(extra_drivers),
                             env=env, stdout=open(os.path.join(self.work, "indiserver.log"), "w"),
                             stderr=subprocess.STDOUT)
        self.procs.append(p)
        for _ in range(40):
            r = subprocess.run(["indi_getprop", "-p", str(self.a.indi_port), "-t", "1", "-1",
                                "%s.CONNECTION.CONNECT" % DEV], capture_output=True, text=True)
            if r.stdout.strip():
                return True
            time.sleep(0.25)
        return False

    # Ekos reads the flip settings into its flip state machine only at start-up;
    # the Mount module's setMeridianFlipValues() over D-Bus does not reach it.
    KSTARSRC = ("[GUI]\nRunStartupWizard=false\n\n"
                "[Mount]\nExecuteMeridianFlip=true\nMeridianFlipOffsetDegrees=0\n")
    KSTARS_LOGGING = "org.kde.kstars.ekos.mount*=true;org.kde.kstars.indi*=true"

    def kstars_dir(self, kind):
        """The private XDG directory of the given kind ("config", "data", ...)."""
        return os.path.join(self.work, "kstars-" + kind)

    def start_kstars(self, kstarsrc=None):
        """Start the private bus (once) and KStars on it.  kstarsrc is written
        only on the first start, so a restart keeps what KStars saved."""
        if self.bus is None:
            cfg = os.path.join(self.work, "bus.conf")
            with open(cfg, "w") as f:
                f.write(BUS_CONFIG % self.sock_dir)
            bus = subprocess.Popen(["dbus-daemon", "--config-file=" + cfg, "--nofork", "--print-address=1"],
                                   stdout=subprocess.PIPE, text=True)
            self.procs.append(bus)
            self.bus_address = bus.stdout.readline().strip()
            self.bus = dbus.bus.BusConnection(self.bus_address)

        xdg = {k: self.kstars_dir(v) for k, v in
               (("XDG_CONFIG_HOME", "config"), ("XDG_DATA_HOME", "data"),
                ("XDG_CACHE_HOME", "cache"), ("XDG_RUNTIME_DIR", "runtime"))}
        for d in xdg.values():
            os.makedirs(d, mode=0o700, exist_ok=True)
        rc = os.path.join(xdg["XDG_CONFIG_HOME"], "kstarsrc")
        if not os.path.exists(rc):
            with open(rc, "w") as f:
                f.write(self.KSTARSRC if kstarsrc is None else kstarsrc)
        env = dict(os.environ, DBUS_SESSION_BUS_ADDRESS=self.bus_address, QT_QPA_PLATFORM="offscreen",
                   QT_LOGGING_RULES=self.KSTARS_LOGGING, **xdg)
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
        p = subprocess.Popen([self.a.kstars], env=env, stdout=open(os.path.join(self.work, "kstars.log"), "a"),
                             stderr=subprocess.STDOUT)
        self.procs.append(p)
        self.kstars = p
        for _ in range(240):
            if self.bus.name_has_owner("org.kde.kstars") and self.has_node("/KStars", "Ekos"):
                return True
            time.sleep(0.25)
        return False

    def stop_kstars(self):
        """Quit KStars (it saves its settings and database), leaving the bus up."""
        p = self.kstars
        if p and p.poll() is None:
            p.send_signal(signal.SIGTERM)
            try:
                p.wait(timeout=30)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
        for _ in range(40):
            if not self.bus.name_has_owner("org.kde.kstars"):
                break
            time.sleep(0.25)

    # ----------------------------------------------------------------- D-Bus
    def iface(self, path, name):
        return dbus.Interface(self.bus.get_object("org.kde.kstars", path), name)

    def prop(self, path, name, prop):
        return self.iface(path, "org.freedesktop.DBus.Properties").Get(name, prop)

    def has_node(self, path, child):
        try:
            xml = self.iface(path, "org.freedesktop.DBus.Introspectable").Introspect()
        except dbus.DBusException:
            return False
        return ('<node name="%s"' % child) in xml

    def stop(self):
        for p in reversed(self.procs):
            if p.poll() is None:
                p.send_signal(signal.SIGTERM)
                try:
                    p.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    p.kill()
                    p.wait()
        if self.tap:
            self.tap.stop()
        shutil.rmtree(self.sock_dir, ignore_errors=True)


class EkosE2E(E2E):
    def __init__(self, a, s):
        super().__init__(a)
        self.s = s
        self.tap = s.tap
        self.ekos = s.iface("/KStars/Ekos", "org.kde.kstars.Ekos")
        self.indi = s.iface("/KStars/INDI", "org.kde.kstars.INDI")
        self.expected_messages = []         # substrings of driver errors this test provokes

    # ------------------------------------------------------------ Ekos Mount
    @property
    def mount(self):
        return self.s.iface("/KStars/Ekos/Mount", "org.kde.kstars.Ekos.Mount")

    def mprop(self, name):
        return self.s.prop("/KStars/Ekos/Mount", "org.kde.kstars.Ekos.Mount", name)

    def mstatus(self):
        return MOUNT_STATUS[int(self.mprop("status"))]

    def mpark(self):
        return PARK_STATUS[int(self.mprop("parkStatus"))]

    def mpier(self):
        return PIER.get(int(self.mprop("pierSide")), "?")

    def wait_mount(self, want, timeout, seen=None):
        """Poll Ekos mount status until it is in want; records every status seen."""
        end = time.time() + timeout
        st = None
        while time.time() < end:
            st = self.mstatus()
            if seen is not None and (not seen or seen[-1] != st):
                seen.append(st)
            if st in want:
                return st
            time.sleep(0.25)
        return st

    def wait_park(self, want, timeout):
        end = time.time() + timeout
        st = None
        while time.time() < end:
            st = self.mpark()
            if st == want:
                return st
            time.sleep(0.5)
        return st

    # ------------------------------------------------------------- firmware
    def fw_status(self):
        return self.tap.text(":GU#") or ""

    def fw_radec(self):
        ra, dec = self.tap.text(":GRH#"), self.tap.text(":GDH#")
        return (sexa(ra), sexa(dec)) if ra and dec else (float("nan"), float("nan"))

    def fw_lst(self):
        return sexa(self.tap.text(":GSH#"))

    def fw_pier(self):
        return {"E": "EAST", "W": "WEST"}.get((self.tap.text(":Gm#") or "?")[:1], "NONE")

    def wait_fw_idle(self, timeout=30):
        """Wait until the firmware reports no goto (after an abort it decelerates)."""
        end = time.time() + timeout
        while time.time() < end:
            if "N" in self.fw_status():
                return True
            time.sleep(0.25)
        return False

    def ra_for_ha(self, ha):
        return (self.fw_lst() - ha) % 24.0

    def on_target(self, ra, dec, tol_arcmin=1.0):
        fra, fdec = self.fw_radec()
        d_ra = abs(wrap_hours(fra - ra)) * 15.0 * 60.0
        d_dec = abs(fdec - dec) * 60.0
        return d_ra <= tol_arcmin and d_dec <= tol_arcmin, "firmware %.4fh %+.4f (%.2f', %.2f' off)" % (
            fra, fdec, d_ra, d_dec)

    def set_pier_pref(self, element):
        for e in ("PIER_WEST", "PIER_EAST", "PIER_BEST", "PIER_AUTO"):
            self.indi.setSwitch(DEV, "OSX_PREFERRED_PIER", e, "On" if e == element else "Off")
        self.indi.sendProperty(DEV, "OSX_PREFERRED_PIER")
        want = {"PIER_WEST": "W", "PIER_EAST": "E", "PIER_BEST": "B", "PIER_AUTO": "A"}[element]
        for _ in range(20):
            got = (self.tap.text(":GX96#") or "")[:1]
            if got == want:
                return True, got
            time.sleep(0.25)
        return False, got

    def goto(self, ra, dec, timeout=300):
        """Ekos slew; wait for it to start, then for the firmware to finish and
        Ekos to report tracking.  Returns (ok, Ekos statuses seen).

        Waiting for the start matters: Ekos still says TRACKING from before the
        slew for a moment, and a goto that changes pier side goes through the
        firmware's meridian-flip home waypoint, which takes minutes here."""
        seen = []

        def note():
            st = self.mstatus()
            if not seen or seen[-1] != st:
                seen.append(st)
            return st

        ok = bool(self.mount.slew(dbus.Double(ra), dbus.Double(dec)))
        end = time.time() + 15
        while time.time() < end:
            if note() in ("SLEWING", "MOVING") or "N" not in self.fw_status():
                break
            time.sleep(0.2)
        end = time.time() + timeout
        while time.time() < end:
            if note() == "TRACKING" and "N" in self.fw_status():
                return ok, seen
            time.sleep(0.5)
        return False, seen

    # ------------------------------------------------------------------- run
    def run(self):
        print("== Ekos start (remote profile, driver behind the tap)")
        self.set("%s.DEVICE_PORT.PORT=%s" % (DEV, self.s.link))
        self.set("%s.DEVICE_BAUD_RATE.%s=On" % (DEV, self.a.baud))
        ok = bool(self.s.iface("/KStars", "org.kde.kstars").setGPSLocation(
            dbus.Double(LON_EAST), dbus.Double(LAT), dbus.Double(ELEV), dbus.Double(0.0)))
        self.check("KStars location set", ok, "%.1f N, %.1f E" % (LAT, LON_EAST))
        self.ekos.addProfile('{"name": "%s", "mode": "remote", "remote_host": "localhost", '
                             '"remote_port": "%d", "auto_connect": true}' % (PROFILE, self.a.indi_port),
                             ignore_reply=True)
        time.sleep(1)
        self.check("Ekos profile selected", bool(self.ekos.setProfile(PROFILE)), PROFILE)
        self.ekos.start()
        st = None
        for _ in range(120):
            st = int(self.s.prop("/KStars/Ekos", "org.kde.kstars.Ekos", "indiStatus"))
            if st == EKOS_SUCCESS:
                break
            time.sleep(0.5)
        self.check("Ekos connected to indiserver", st == EKOS_SUCCESS, "indiStatus %s" % st)
        st = self.wait_state("CONNECTION", timeout=40)
        self.check("driver connected by Ekos", self.get("%s.CONNECTION.CONNECT" % DEV) == "On", st)
        mount_module = False
        for _ in range(60):
            if self.s.has_node("/KStars/Ekos", "Mount"):
                try:
                    self.mstatus()
                    mount_module = True
                    break
                except dbus.DBusException:
                    pass
            time.sleep(0.5)
        if not self.check("Ekos Mount module has the mount", mount_module):
            return
        time.sleep(3)          # let KStars push time and location

        print("== time and location from KStars")
        self.check("controller latitude", abs(sexa(self.tap.text(":GtH#")) - LAT) < 1 / 60.0,
                   self.tap.text(":GtH#"))
        # :GgH# is east-negative
        self.check("controller longitude", abs(-sexa(self.tap.text(":GgH#")) - LON_EAST) < 1 / 60.0,
                   self.tap.text(":GgH#"))
        date, local, offset = self.tap.text(":GC#"), self.tap.text(":GL#"), self.tap.text(":GG#")
        try:
            # :GG# is the hours added to local time to give UT
            fw_local = datetime.datetime.strptime(date + " " + local.split(".")[0], "%m/%d/%y %H:%M:%S")
            fw_utc = fw_local + datetime.timedelta(hours=sexa(offset))
            diff = abs((fw_utc - datetime.datetime.now(datetime.timezone.utc).replace(tzinfo=None))
                       .total_seconds())
        except (TypeError, ValueError, AttributeError):
            diff = float("inf")
        self.check("controller UTC matches the host", diff < 5, "%s %s, UT offset %s (diff %.0f s)"
                   % (date, local, offset, diff))

        print("== unpark")
        self.check("Ekos can park", bool(self.mprop("canPark")))
        if "P" in self.fw_status():
            self.mount.unpark()
        self.check("unparked: Ekos", self.wait_park("UNPARKED", 30) == "UNPARKED", self.mpark())
        self.check("unparked: firmware", "P" not in self.fw_status(), self.fw_status())

        print("== goto, sync, abort")
        ok, got = self.set_pier_pref("PIER_WEST")
        self.check("pier preference West via KStars", ok, ":GX96# %s" % got)
        ra, dec = self.ra_for_ha(-1.5), 40.0
        ok, seen = self.goto(ra, dec)
        self.check("goto: Ekos slewing then tracking", ok and "SLEWING" in seen, " > ".join(seen))
        hit, detail = self.on_target(ra, dec)
        self.check("goto: firmware on target", hit, detail)
        self.check("goto: firmware tracking", "n" not in self.fw_status(), self.fw_status())
        self.check("goto: pier side West (Ekos, firmware)", self.mpier() == "WEST" and self.fw_pier() == "WEST",
                   "%s, %s" % (self.mpier(), self.fw_pier()))
        time.sleep(2)
        era, edec = [float(v) for v in self.mprop("equatorialCoords")]
        fra, fdec = self.fw_radec()
        self.check("Ekos coordinates match the firmware",
                   abs(wrap_hours(era - fra)) * 15 * 60 < 1 and abs(edec - fdec) * 60 < 1,
                   "Ekos %.4fh %+.4f, firmware %.4fh %+.4f" % (era, edec, fra, fdec))

        sra, sdec = (ra + 0.2) % 24.0, dec + 1.0
        self.check("sync accepted by Ekos", bool(self.mount.sync(dbus.Double(sra), dbus.Double(sdec))))
        time.sleep(1.5)
        hit, detail = self.on_target(sra, sdec)
        self.check("sync: firmware position", hit, detail)

        before = self.fw_radec()
        self.mount.slew(dbus.Double(self.ra_for_ha(-4.0)), dbus.Double(-10.0))
        self.wait_mount(("SLEWING", "MOVING"), 10)
        time.sleep(1.5)
        gu = self.fw_status()
        self.check("abort: firmware still slewing when Ekos aborts", "N" not in gu, gu)
        self.check("abort accepted by Ekos", bool(self.mount.abort()))
        time.sleep(2)
        gu = self.fw_status()
        moved = abs(wrap_hours(self.fw_radec()[0] - before[0])) * 15
        self.check("abort: firmware goto stopped", "N" in gu, gu)
        self.check("abort: stopped part way", 0.1 < moved < 55, "moved %.1f deg in RA" % moved)
        st = self.wait_mount(("TRACKING", "IDLE"), 10)
        self.check("abort: Ekos no longer slewing", st in ("TRACKING", "IDLE"), st)
        self.wait_fw_idle()

        print("== park refused while slewing")
        refused_park = "Park rejected by firmware"
        self.expected_messages.append(refused_park)
        self.mount.slew(dbus.Double(self.ra_for_ha(-1.0)), dbus.Double(60.0))
        self.wait_mount(("SLEWING", "MOVING"), 10)
        self.mount.park()
        time.sleep(2)
        self.check("refused park: driver reports it", any(refused_park in m for _, m in self.listener.messages))
        self.check("refused park: driver still connected", self.get("%s.CONNECTION.CONNECT" % DEV) == "On")
        self.check("refused park: firmware not parked", "P" not in self.fw_status(), self.fw_status())
        self.mount.abort()
        self.wait_fw_idle()

        print("== goto below the horizon")
        before = self.fw_radec()
        refusal = "Goto rejected by firmware: below the horizon limit"
        self.expected_messages.append(refusal)
        self.mount.slew(dbus.Double(self.ra_for_ha(0.0)), dbus.Double(-60.0))
        time.sleep(3)
        self.check("refused: driver reports the horizon limit",
                   any(refusal in m for _, m in self.listener.messages), refusal)
        after = self.fw_radec()
        self.check("refused: firmware did not move",
                   abs(wrap_hours(after[0] - before[0])) * 15 < 0.5 and abs(after[1] - before[1]) < 0.5,
                   "%.4fh %+.4f" % after)
        self.check("refused: Ekos not slewing", self.mstatus() != "SLEWING", self.mstatus())

        self.driver_status()
        self.refine_polar_align()
        self.standard_properties()
        self.rotator_sync()
        self.focuser_home()
        self.pause_at_home()

        for pref in self.a.flip:
            self.meridian_flip(pref)
        # The firmware keeps the pier preference in NV: leave its default (Best)
        # for the next test, not the last flip case's.
        self.set_pier_pref("PIER_BEST")

        print("== park")
        self.mount.park()
        self.check("park: Ekos parked", self.wait_park("PARKED", 120) == "PARKED", self.mpark())
        self.check("park: firmware parked", "P" in self.fw_status(), self.fw_status())

        print("== disconnect")
        self.ekos.disconnectDevices()
        self.wait_state("CONNECTION", timeout=15)
        self.check("driver disconnected by Ekos", self.get("%s.CONNECTION.DISCONNECT" % DEV) == "On")
        n = sum(self.tap.driver_commands.values())
        time.sleep(3)
        self.check("no driver traffic after disconnect", sum(self.tap.driver_commands.values()) == n)
        self.ekos.stop()

    def wait_message(self, text, since, timeout=10):
        end = time.time() + timeout
        while time.time() < end:
            if self.driver_messages(text) > since:
                return True
            time.sleep(0.25)
        return False

    def driver_status(self):
        """OSX_DRIVER_STATUS exists exactly when the firmware reports driver status."""
        print("== motor driver status")
        fw = self.tap.text(":GXU1#")
        shown = bool(self.get("%s.OSX_DRIVER_STATUS.AXIS1" % DEV))
        self.check("driver status shown only if the firmware reports it", shown == (fw is not None),
                   ":GXU1# %s; property %s" % (fw if fw is not None else "bare 0", "shown" if shown else "absent"))

    def refine_polar_align(self):
        """Refine Polar Align through KStars: the driver sends :MP#, the firmware
        zeroes its polar terms and re-slews.  The polar error is seeded on the
        firmware through the tap, as a star alignment would leave it."""
        print("== refine polar alignment")
        prop = "OSX_POLAR_REFINE"

        def press():
            self.indi.setSwitch(DEV, prop, "REFINE", "On")
            self.indi.sendProperty(DEV, prop)

        refused = "no polar error has been measured"
        self.expected_messages.append(refused)
        saved = {i: self.tap.text(":GX0%s#" % i) for i in "23"}
        for i, v in (("2", 600), ("3", -300)):
            self.tap.query(":SX0%s,%d#" % (i, v))
        seeded = (self.tap.text(":GX02#"), self.tap.text(":GX03#"))
        if not self.check("refine: polar error seeded on the firmware", seeded == ("600", "-300"), str(seeded)):
            return
        sent = self.tap.driver_commands.get(":MP#", 0)
        started = self.driver_messages("Refine polar alignment: removing")
        press()
        self.check("refine: driver reports the refine slew",
                   self.wait_message("Refine polar alignment: removing", started))
        self.check("refine: driver sent :MP#", self.tap.driver_commands.get(":MP#", 0) == sent + 1)
        zeroed = (self.tap.text(":GX02#"), self.tap.text(":GX03#"))
        self.check("refine: firmware polar terms zeroed", zeroed == ("0", "0"), str(zeroed))
        self.check("refine: slew finished", self.wait_fw_idle(120), self.fw_status())

        refusals = self.driver_messages(refused)
        press()
        self.check("refine: refused with no polar error", self.wait_message(refused, refusals))
        self.check("refine: no :MP# sent when refused", self.tap.driver_commands.get(":MP#", 0) == sent + 1)
        for i, v in saved.items():
            if v is not None:
                self.tap.query(":SX0%s,%s#" % (i, v))

    def press(self, prop, element, dev=DEV):
        self.indi.setSwitch(dev, prop, element, "On")
        self.indi.sendProperty(dev, prop)

    def standard_properties(self):
        """INDI standard properties that clients (PHD2, Ekos) look for."""
        print("== standard properties")
        self.check("guide rate as INDI GUIDE_RATE", bool(self.get("%s.GUIDE_RATE.GUIDE_RATE_WE" % DEV)) and
                   bool(self.get("%s.GUIDE_RATE.GUIDE_RATE_NS" % DEV)), self.get("%s.GUIDE_RATE.*" % DEV))
        if self.tap.text(":GX98#") in ("R", "D"):
            for prop in ("ROTATOR_ABORT_MOTION", "ROTATOR_HOME", "ROTATOR_BACKLASH_STEPS",
                         "ROTATOR_BACKLASH_TOGGLE", "SYNC_ROTATOR_ANGLE"):
                self.check("rotator offers %s" % prop, bool(self.state(prop, dev=ROT)), self.state(prop, dev=ROT))

    def rotator_sync(self):
        """Sync to 0 deg through KStars: the firmware's :rZ# (no reply), read back."""
        if self.tap.text(":GX98#") not in ("R", "D"):
            return
        print("== rotator sync")
        # Move 20 deg from wherever the rotator is (about 1 deg/s), so the sync
        # starts from a known, non-zero angle without a long move first.
        start = sexa(self.tap.text(":rG#").replace("*", ":"))
        target = start + 20.0 if start + 20.0 <= 340.0 else start - 20.0
        self.indi.setNumber(ROT, "ABS_ROTATOR_ANGLE", "ANGLE", dbus.Double(target))
        self.indi.sendProperty(ROT, "ABS_ROTATOR_ANGLE")
        # Wait for the angle itself: :rT# still says stopped until the move starts.
        end = time.time() + 90
        before = float("nan")
        while time.time() < end:
            before = sexa(self.tap.text(":rG#").replace("*", ":"))
            if abs(before - target) < 0.5 and (self.tap.text(":rT#") or "M").startswith("S"):
                break
            time.sleep(0.5)
        sent = self.tap.driver_commands.get(":rZ#", 0)
        self.indi.setNumber(ROT, "SYNC_ROTATOR_ANGLE", "ANGLE", dbus.Double(0.0))
        self.indi.sendProperty(ROT, "SYNC_ROTATOR_ANGLE")
        time.sleep(3)
        after = sexa(self.tap.text(":rG#").replace("*", ":"))
        self.check("rotator sync: driver sent :rZ#", self.tap.driver_commands.get(":rZ#", 0) == sent + 1)
        self.check("rotator sync: firmware angle now 0", abs(after) < 0.1 and abs(before - target) < 1,
                   "%.2f deg before (target %.2f), %.2f after" % (before, target, after))
        self.check("rotator sync: property Ok", self.state("SYNC_ROTATOR_ANGLE", dev=ROT) == "Ok",
                   self.state("SYNC_ROTATOR_ANGLE", dev=ROT))

    def focuser_home(self):
        """Focuser 1: Set as Home then Go to Home; compensation only with a temperature."""
        fdev = DEV + " Focuser 1"
        if not self.get("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION" % fdev):
            return
        print("== focuser home and temperature compensation")
        t = self.tap.text(":F1t#")
        has_temp = t is not None and t.strip() not in ("nan", "")
        shown = bool(self.state("OSX_FOCUS_TCF", dev=fdev))
        self.check("focuser: temperature compensation shown only with a temperature", shown == has_temp,
                   ":F1t# %s; property %s" % (t, "shown" if shown else "absent"))
        h0, H0 = self.tap.driver_commands.get(":F1h#", 0), self.tap.driver_commands.get(":F1H#", 0)
        self.press("OSX_FOCUS_HOME", "FOCUS_SET_HOME", dev=fdev)
        time.sleep(2)
        home = self.tap.text(":F1G#")
        self.press("OSX_FOCUS_HOME", "FOCUS_GOTO_HOME", dev=fdev)
        end = time.time() + 60
        while time.time() < end and self.tap.driver_commands.get(":F1h#", 0) == h0:
            time.sleep(0.2)                   # KStars passes the press on asynchronously
        while time.time() < end and not (self.tap.text(":F1T#") or "M").startswith("S"):
            time.sleep(0.5)
        self.check("focuser: driver sent :F1H# and :F1h#",
                   self.tap.driver_commands.get(":F1H#", 0) == H0 + 1 and
                   self.tap.driver_commands.get(":F1h#", 0) == h0 + 1)
        self.check("focuser: at home after Go to Home", self.tap.text(":F1G#") == home,
                   "home %s, now %s" % (home, self.tap.text(":F1G#")))

    def pause_at_home(self):
        """Pause at home on, a goto that changes pier side waits at home until
        Continue is pressed in KStars."""
        print("== pause at home and continue")
        paused_warning = "Goto paused at home"
        self.expected_messages.append(paused_warning)
        self.press("OSX_HOME_PAUSE", "HOME_PAUSE_ON")
        time.sleep(1)
        self.check("pause at home: firmware mode on", "u" in self.fw_status(), self.fw_status())
        self.set_pier_pref("PIER_WEST")
        ok, seen = self.goto(self.ra_for_ha(-1.0), 30.0)
        if not self.check("pause at home: starts west of the pier", ok and self.fw_pier() == "WEST",
                          "%s; %s" % (" > ".join(seen), self.fw_pier())):
            return
        self.set_pier_pref("PIER_EAST")
        ra = self.ra_for_ha(1.0)
        warnings = self.driver_messages(paused_warning)
        self.mount.slew(dbus.Double(ra), dbus.Double(30.0))
        end = time.time() + 240
        while time.time() < end and "w" not in self.fw_status():
            time.sleep(1)
        self.check("pause at home: firmware waiting at home", "w" in self.fw_status(), self.fw_status())
        self.check("pause at home: driver warns", self.wait_message(paused_warning, warnings))
        self.check("pause at home: Continue shown busy", self.state("OSX_HOME_CONTINUE") == "Busy",
                   self.state("OSX_HOME_CONTINUE"))
        time.sleep(3)
        self.check("pause at home: still waiting before Continue", "w" in self.fw_status(), self.fw_status())
        self.press("OSX_HOME_CONTINUE", "HOME_CONTINUE")
        end = time.time() + 240
        while time.time() < end and not ("N" in self.fw_status() and self.mstatus() == "TRACKING"):
            time.sleep(1)
        hit, detail = self.on_target(ra, 30.0)
        self.check("pause at home: goto finished on the east side", hit and self.fw_pier() == "EAST",
                   "%s; %s" % (detail, self.fw_pier()))
        self.check("pause at home: Continue idle again", self.state("OSX_HOME_CONTINUE") == "Idle",
                   self.state("OSX_HOME_CONTINUE"))
        self.press("OSX_HOME_PAUSE", "HOME_PAUSE_OFF")
        time.sleep(1)
        self.check("pause at home: firmware mode off again", "u" not in self.fw_status(), self.fw_status())

    def set_forced_flip(self, on):
        """Ekos's "Use flip command if supported by mount" (Capture options,
        ForcedFlip).  With it on, Ekos flips with ON_COORD_SET FLIP, which the
        driver sends as :MN#; with it off (the default) Ekos re-sends a goto."""
        ks = self.s.iface("/KStars", "org.kde.kstars")
        ks.setOption("ForcedFlip", dbus.Boolean(on, variant_level=1))
        time.sleep(0.5)
        return bool(ks.getOption("ForcedFlip")) == on

    def driver_messages(self, text):
        return sum(1 for _, m in self.listener.messages if text in m)

    def ekos_log_count(self, text):
        """Ekos's flip state machine logs a failed flip to the KStars log, not to
        the Mount module's logText property."""
        with open(os.path.join(self.s.work, "kstars.log"), errors="replace") as f:
            return f.read().count(text)

    def meridian_flip(self, case):
        """Track up to the meridian on the west side, then let Ekos flip.

        case is a pier preference (Ekos flips by goto, its default), or
        FLIP_COMMAND (Ekos uses the driver's Flip, pier preference Best)."""
        forced = case == "FLIP_COMMAND"
        pref = "PIER_BEST" if forced else case
        label = "flip command" if forced else pref.split("_")[1].title()
        # A goto keeps the current side with Best (and West) until the target is
        # past the west limit; :MN# and East/Auto change sides at the meridian.
        expect_flip = forced or pref in ("PIER_EAST", "PIER_AUTO")
        print("== meridian flip by Ekos, %s" % ("Flip command" if forced else
                                                 "goto with firmware pier preference " + label))
        self.check("flip (%s): Ekos flip command option %s" % (label, "on" if forced else "off"),
                   self.set_forced_flip(forced))
        notice = "Meridian: tracking west of the pier"
        notices = self.driver_messages(notice)
        retry = "meridian flip failed, retrying"
        retries = self.ekos_log_count(retry)
        self.set_pier_pref("PIER_WEST")
        ha0 = -45.0 / 3600.0                      # 45 s before the meridian
        ra, dec = self.ra_for_ha(ha0), 20.0
        ok, seen = self.goto(ra, dec)
        start_pier = self.fw_pier()
        if not self.check("flip (%s): starts tracking on the west side" % label,
                          ok and start_pier == "WEST" and self.mpier() == "WEST",
                          "%s; firmware %s, Ekos %s" % (" > ".join(seen), start_pier, self.mpier())):
            return
        ok, got = self.set_pier_pref(pref)
        self.check("flip (%s): pier preference set" % label, ok, ":GX96# %s" % got)
        flips = self.tap.driver_commands.get(":MN#", 0)
        # Ekos flips once the hour angle passes its offset (0, set in kstarsrc).  The
        # flip is a 180 degree slew (~2 min on the test board).  If the pier side has
        # not changed, Ekos says so and retries 4 min later.
        end = time.time() + 45 + (420 if expect_flip else 240)
        seen = []
        while time.time() < end:
            if self.fw_pier() == "EAST" and self.mstatus() == "TRACKING" and "N" in self.fw_status():
                break
            if not expect_flip and self.ekos_log_count(retry) > retries:
                break
            st = self.mstatus()
            if not seen or seen[-1] != st:
                seen.append(st)
            time.sleep(1)
        ha = wrap_hours(self.fw_lst() - ra) * 60
        where = "firmware %s at HA %+.1f min; Ekos %s" % (self.fw_pier(), ha, " > ".join(seen))
        sent = self.tap.driver_commands.get(":MN#", 0) - flips
        self.check("flip (%s): driver %s :MN#" % (label, "sent" if forced else "did not send"),
                   (sent > 0) == forced, "%d sent" % sent)
        if expect_flip:
            self.check("flip (%s): firmware on the east side" % label, self.fw_pier() == "EAST", where)
            hit, detail = self.on_target(ra, dec)
            self.check("flip (%s): still on target" % label, hit, detail)
            self.check("flip (%s): Ekos pier side East" % label, self.mpier() == "EAST", self.mpier())
        else:
            self.check("flip (%s): firmware stayed west, as expected" % label, self.fw_pier() == "WEST", where)
            self.check("flip (%s): Ekos reports the flip failed" % label,
                       self.ekos_log_count(retry) > retries)
            # A flip made at Ekos's offset 0 can come before the driver's next
            # check, so the notice is only required when nothing changes sides.
            self.check("flip (%s): driver gave the meridian notice" % label,
                       self.driver_messages(notice) > notices)
        for line in reversed(list(self.mprop("logText"))[:6]):
            print("      Ekos: %s" % line)
        self.set_forced_flip(False)

    def finish(self):
        print("== tap and driver")
        t = self.tap
        self.check("every driver command classified", not t.problems,
                   "; ".join(t.problems[:3]) if t.problems else
                   "%d commands, %d distinct" % (sum(t.driver_commands.values()), len(t.driver_commands)))
        print("      tap: %d queries, %d driver commands sent while one was outstanding, "
              "%d driver commands timed out, %d NV stalls (0xFF bursts)"
              % (t.queries, t.overlaps, t.abandoned, t.noise_bursts))
        with open(os.path.join(self.s.work, "indiserver.log"), errors="replace") as f:
            log = f.read()
        restarts = log.count(": restart #")
        assertion = next((ln for ln in log.splitlines() if "Assertion" in ln), "")
        self.check("driver never crashed or restarted", restarts == 0,
                   "%d restart(s) %s" % (restarts, assertion[-160:]))
        unexpected = [(ts, m) for ts, m in self.listener.problems()
                      if not any(e in m for e in self.expected_messages)]
        self.check("no unexpected driver errors or warnings", not unexpected, "%d message(s)" % len(unexpected))
        for ts, m in unexpected[:20]:
            print("      %s %s" % (ts, m))
        passed = sum(1 for r in self.results if r[0] == "PASS")
        failed = sum(1 for r in self.results if r[0] == "FAIL")
        print("\nPASS: %d  FAIL: %d  SKIP: %d" % (passed, failed, len(self.results) - passed - failed))
        return failed == 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", required=True, help="controller serial port, e.g. /dev/ttyUSB0")
    ap.add_argument("--baud", default="9600")
    ap.add_argument("--indi-port", type=int, default=7627)
    ap.add_argument("--driver", default=DEFAULT_DRIVER)
    ap.add_argument("--kstars", default=shutil.which("kstars") or "kstars")
    ap.add_argument("--work-dir", help="directory for logs (default: a new temporary directory)")
    ap.add_argument("--flip", nargs="*", default=["FLIP_COMMAND", "PIER_EAST"],
                    choices=["FLIP_COMMAND", "PIER_BEST", "PIER_EAST", "PIER_WEST", "PIER_AUTO"],
                    help="meridian flip tests to run (~4-8 min each): FLIP_COMMAND has Ekos use the "
                         "driver's Flip; a pier preference has Ekos flip by goto with it set (Best "
                         "and West are expected not to change sides). Give none to skip them")
    onstepx_safety.add_flag(ap)
    a = ap.parse_args()
    onstepx_safety.require(a)

    work = a.work_dir or tempfile.mkdtemp(prefix="onstepx-ekos-e2e-")
    os.makedirs(work, exist_ok=True)
    print("work directory: %s" % work)
    s = Session(a, work)
    ok = False
    try:
        print("== setup")
        if not s.start_tap():
            print("FAIL  controller not answering through the tap")
            return 1
        ok, gu = s.prepare_controller()
        if not ok:
            print("FAIL  could not stop and unpark the controller (:GU# %s)" % gu)
            return 1
        if not s.start_indiserver():
            print("FAIL  indiserver did not start (see indiserver.log)")
            return 1
        if not s.start_kstars():
            print("FAIL  KStars did not come up on the private bus (see kstars.log)")
            return 1
        t = EkosE2E(a, s)
        try:
            t.run()
        except dbus.DBusException as e:
            t.check("D-Bus call failed", False, str(e).splitlines()[0])
        ok = t.finish()
    finally:
        s.stop()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
