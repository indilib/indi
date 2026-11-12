#!/usr/bin/env python3
# OnStepX INDI Driver — onstepx_ekos_imaging_e2e.py
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
End-to-end test of the OnStepX focuser and guide output driven by Ekos:
autofocus with Focuser 1, and calibration and guiding with the mount's
pulse-guide output.  The CCD Simulator is the camera.

    KStars/Ekos --D-Bus-- this script
         |                    |
     indiserver               | tap.query() between driver commands
      |      \\                |
      |   indi_simulator_ccd  |
    indi_onstepx --pty-- onstepx_tap --/dev/ttyUSB0-- OnStepX

The CCD Simulator draws its star field at the mount's coordinates, which it
snoops from the driver, and blurs it by the distance of the OnStepX focuser
from a focus position this test chooses (SIM_FOCUSING).  So Ekos's autofocus
moves the real focuser stepper to find that position, and Ekos's guide pulses
move the real mount axes, which moves the stars in the next frame.

The session is isolated as in onstepx_ekos_e2e.py (private D-Bus, own XDG
dirs, indiserver with its own HOME).  Ekos builds its optical train from the
devices it finds but leaves the focuser out of it, so the test fills it in
(and sets the guider) in KStars's database and restarts KStars once.

Prerequisites:
  * a TEST controller with no mechanics attached, running the GEM test
    firmware with Focuser 1 (see the README in this directory).
  * KStars (tested with 3.8.4), indiserver, the CCD Simulator
    (indi_simulator_ccd) with the GSC star catalogue, python3-dbus,
    python3-serial.

Usage:
  onstepx_ekos_imaging_e2e.py --port /dev/ttyUSB0 --test-controller [--baud 9600]

Takes about 8 minutes.  Logs are left in the work directory printed at the
start.
"""

import argparse
import collections
import os
import re
import shutil
import sqlite3
import sys
import tempfile
import time

import dbus

sys.dont_write_bytecode = True      # importing the sibling scripts leaves no __pycache__

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from onstepx_e2e import DEV, FOC                                     # noqa: E402
from onstepx_ekos_e2e import DEFAULT_DRIVER, EKOS_SUCCESS, PROFILE, EkosE2E, Session  # noqa: E402
import onstepx_safety  # noqa: E402

# The CCD Simulator from this tree, if built (indi/build), else the installed one.
BUILT_CCD = os.path.normpath(os.path.join(HERE, "..", "..", "..", "build", "drivers", "ccd",
                                         "indi_simulator_ccd"))

FOCUSER = FOC % 1
CCD = "CCD Simulator"
TRAIN = "Primary"                 # the train Ekos generates for a new profile
# Vendor, model, aperture and focal length (mm).  1000 mm gives the CCD
# Simulator's 5.2 um pixels 1.07"/px, so stars at SIM_SEEING span a few
# pixels.  At 400 mm they were about one pixel: the focus curve was flat and
# Ekos lost the guide star during calibration.
SCOPE = ("OnStepX", "Test", 125, 1000)

# The CCD Simulator's best focus.  Autofocus starts 1500 steps out; Ekos's
# Linear 1 Pass then begins 5 steps further out (4000 from focus).  The
# focuser first goes to FOCUS_FAR so that a long move is seen in progress.
FOCUS_TRUE, FOCUS_START, FOCUS_FAR = 20000, 21500, 29500
FOCUS_STEP = 500
# The simulator blurs by 0.5625 * (20 * (pos - focus) / SIM_FOCUS_MAX)^2 arcsec
# on top of SIM_SEEING.  SIM_FOCUS_MAX is only that scale, not the focuser's
# travel.  20000 makes 4000 steps of defocus about 9" (stars still found)
# and gives a clear minimum; 50000 was too flat and 10000 lost the stars at
# the outer end.
SIM_FOCUS_MAX, SIM_SEEING = 20000, 3.0
# The simulator's guide chip has larger pixels (9.8 x 12.6 um, about 2-2.6"/px
# here), where 3" stars are undersampled and Ekos loses them during
# calibration; at 6" they are too faint.  Guiding runs at 4.5", with
# brighter stars (saturation magnitude 11, focus uses 10).
GUIDE_SEEING, GUIDE_SATURATION = 4.5, 11

# Ekos enums (ekos/ekos.h)
FOCUS_STATE = {0: "IDLE", 1: "COMPLETE", 2: "FAILED", 3: "ABORTED", 4: "WAITING", 5: "PROGRESS",
               6: "FRAMING", 7: "CHANGING_FILTER"}
GUIDE_STATE = {0: "IDLE", 1: "ABORTED", 2: "CONNECTED", 3: "DISCONNECTED", 4: "CAPTURE", 5: "LOOPING",
               6: "DARK", 7: "SUBFRAME", 8: "STAR_SELECT", 9: "CALIBRATING", 10: "CALIBRATION_ERROR",
               11: "CALIBRATION_SUCCESS", 12: "GUIDING", 13: "SUSPENDED", 14: "REACQUIRE", 15: "DITHERING",
               16: "MANUAL_DITHERING", 17: "DITHERING_ERROR", 18: "DITHERING_SUCCESS", 19: "DITHERING_SETTLE"}

KSTARSRC = "[GUI]\nRunStartupWizard=false\n"


class ImagingE2E(EkosE2E):
    def __init__(self, a, s):
        super().__init__(a, s)
        # The CCD Simulator warns until it sees EQUATORIAL_PE, which only its
        # own mount simulator publishes; it then uses EQUATORIAL_EOD_COORD.
        self.expected_messages.append("EQUATORIAL_PE not yet snooped")

    def refresh(self):
        """Proxies bind to KStars's unique bus name, so renew them after a restart."""
        self.ekos = self.s.iface("/KStars/Ekos", "org.kde.kstars.Ekos")
        self.indi = self.s.iface("/KStars/INDI", "org.kde.kstars.INDI")

    def focuser_cmds(self):
        return sum(n for c, n in self.tap.driver_commands.items()
                   if c.startswith(":F1S") or c.startswith(":F1R"))

    def pulse_list(self):
        """Every guide pulse the driver has sent, as (direction, ms)."""
        out = collections.Counter()
        for c, n in self.tap.driver_commands.items():
            m = re.fullmatch(r":Mg([nsew])(\d+)#", c)
            if m:
                out[(m.group(1), int(m.group(2)))] += n
        return out

    def kstars_log_from(self, offset):
        with open(os.path.join(self.s.work, "kstars.log"), errors="replace") as f:
            f.seek(offset)
            return f.read()

    def kstars_log_size(self):
        return os.path.getsize(os.path.join(self.s.work, "kstars.log"))

    def pulse_cmds(self):
        """Guide pulses sent so far, per direction."""
        sent = self.pulse_list()
        return {d: sum(n for (k, _), n in sent.items() if k == d) for d in "nsew"}

    def fw_focuser(self):
        try:
            return int(self.tap.text(":F1G#"))
        except (TypeError, ValueError):
            return None

    # ------------------------------------------------------------- Ekos setup
    def start_ekos(self):
        self.ekos.start()
        st = None
        for _ in range(120):
            st = int(self.s.prop("/KStars/Ekos", "org.kde.kstars.Ekos", "indiStatus"))
            if st == EKOS_SUCCESS:
                break
            time.sleep(0.5)
        return st == EKOS_SUCCESS, st

    def wait_module(self, name, timeout=60):
        end = time.time() + timeout
        while time.time() < end:
            if self.s.has_node("/KStars/Ekos", name):
                return True
            time.sleep(0.5)
        return False

    def userdb(self):
        return os.path.join(self.s.kstars_dir("data"), "kstars", "userdb.sqlite")

    def train_row(self):
        try:
            con = sqlite3.connect(self.userdb())
            con.row_factory = sqlite3.Row
            row = con.execute("SELECT * FROM opticaltrains WHERE name = ?", (TRAIN,)).fetchone()
            con.close()
            return dict(row) if row else None
        except sqlite3.Error:
            return None

    def setup(self):
        print("== Ekos start (remote profile: driver behind the tap, CCD Simulator)")
        self.set("%s.DEVICE_PORT.PORT=%s" % (DEV, self.s.link))
        self.set("%s.DEVICE_BAUD_RATE.%s=On" % (DEV, self.a.baud))
        self.s.iface("/KStars", "org.kde.kstars").setGPSLocation(
            dbus.Double(-0.1), dbus.Double(51.5), dbus.Double(50.0), dbus.Double(0.0))
        # A scope with a focal length: Ekos gives the train's camera its pixel scale from it.
        for _ in range(60):
            if os.path.exists(self.userdb()):
                break
            time.sleep(0.5)
        con = sqlite3.connect(self.userdb())
        con.execute("INSERT INTO telescope (Vendor, Model, Aperture, FocalLength, Type) "
                    "VALUES (?, ?, ?, ?, 'Refractor')", SCOPE)
        con.commit()
        con.close()
        self.ekos.addProfile('{"name": "%s", "mode": "remote", "remote_host": "localhost", '
                             '"remote_port": "%d", "auto_connect": true}' % (PROFILE, self.a.indi_port),
                             ignore_reply=True)
        time.sleep(1)
        self.check("Ekos profile selected", bool(self.ekos.setProfile(PROFILE)), PROFILE)
        ok, st = self.start_ekos()
        self.check("Ekos connected to indiserver", ok, "indiStatus %s" % st)
        self.check("driver connected by Ekos",
                   self.wait_state("CONNECTION", timeout=40) == "Ok" and
                   self.get("%s.CONNECTION.CONNECT" % DEV) == "On")
        row = None
        for _ in range(60):
            row = self.train_row()
            if row and row.get("mount") == DEV and row.get("camera") == CCD:
                break
            time.sleep(0.5)
        if not self.check("Ekos generated an optical train", row is not None, str(row)):
            return False

        print("== optical train: OnStepX focuser and guider (KStars restarted)")
        self.ekos.stop()
        time.sleep(2)
        self.s.stop_kstars()
        scope = "%s %s %d@F/%.1f" % (SCOPE[0], SCOPE[1], SCOPE[3], SCOPE[3] / SCOPE[2])
        con = sqlite3.connect(self.userdb())
        con.execute("UPDATE opticaltrains SET scope = ?, focuser = ?, guider = ?, camera = ? WHERE name = ?",
                    (scope, FOCUSER, DEV, CCD, TRAIN))
        con.commit()
        con.close()
        if not self.check("KStars restarted", self.s.start_kstars()):
            return False
        self.refresh()
        self.s.iface("/KStars", "org.kde.kstars").setGPSLocation(
            dbus.Double(-0.1), dbus.Double(51.5), dbus.Double(50.0), dbus.Double(0.0))
        self.ekos.setProfile(PROFILE)
        ok, st = self.start_ekos()
        self.check("Ekos reconnected", ok, "indiStatus %s" % st)
        return self.check("Ekos Focus, Guide and Mount modules up",
                          all(self.wait_module(m) for m in ("Focus", "Guide", "Mount")))

    def point(self):
        print("== mount: unpark and goto east of the meridian")
        time.sleep(3)
        if "P" in self.fw_status():
            self.mount.unpark()
            self.wait_park("UNPARKED", 30)
        # The pier preference is in the firmware's NV, so an earlier test can
        # leave it at East: the goto then ends counterweight-up and the park
        # slew from there is stopped at the overhead limit.
        ok, got = self.set_pier_pref("PIER_BEST")
        self.check("pier preference Best", ok, ":GX96# %s" % got)
        ra, dec = self.ra_for_ha(-1.0), 40.0
        ok, seen = self.goto(ra, dec)
        hit, detail = self.on_target(ra, dec)
        return self.check("goto by Ekos, tracking on target", ok and hit, "%s; %s" % (" > ".join(seen), detail))

    # ------------------------------------------------------------------ focus
    def focuser_move(self, target, timeout=120):
        """Move Focuser 1 through INDI and wait for the property to say Ok at
        target: Ekos starts from the position it was last sent, and :F1G#
        reaches the target a moment before :F1T# reports the move over.
        Returns the positions published while it was moving."""
        start = self.num("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION" % FOCUSER)
        self.set("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION=%d" % (FOCUSER, target))
        mid = []
        end = time.time() + timeout
        while time.time() < end:
            indi = self.num("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION" % FOCUSER)
            st = self.state("ABS_FOCUS_POSITION", dev=FOCUSER)
            if st == "Busy" and min(start, target) < indi < max(start, target) and indi not in mid:
                mid.append(indi)
            if st == "Ok" and indi == target:
                break
            time.sleep(1)
        return mid

    def autofocus(self):
        print("== Ekos autofocus with %s" % FOCUSER)
        f = self.s.iface("/KStars/Ekos/Focus", "org.kde.kstars.Ekos.Focus")
        names = (str(f.focuser(TRAIN)), str(f.camera(TRAIN)))
        if not self.check("Focus module uses the OnStepX focuser", names == (FOCUSER, CCD), "%s, %s" % names):
            return
        self.check("Ekos can autofocus", bool(f.canAutoFocus(TRAIN)))

        self.set("%s.SIM_FOCUSING.SIM_FOCUS_POSITION;SIM_FOCUS_MAX;SIM_SEEING=%d;%d;%g"
                 % (CCD, FOCUS_TRUE, SIM_FOCUS_MAX, SIM_SEEING))
        # The default saturation magnitude (1) leaves the stars too faint to measure.
        self.set("%s.SIMULATOR_SETTINGS.SIM_SATURATION=10" % CCD)
        self.focuser_move(FOCUS_FAR)
        mid = self.focuser_move(FOCUS_START)
        self.check("focuser position published during the move", len(mid) >= 2,
                   "%d positions between %d and %d" % (len(mid), FOCUS_FAR, FOCUS_START))
        self.check("focuser at the start position", self.fw_focuser() == FOCUS_START and
                   self.state("ABS_FOCUS_POSITION", dev=FOCUSER) == "Ok",
                   "firmware %s, INDI %s, focus at %d" % (self.fw_focuser(), self.state(
                       "ABS_FOCUS_POSITION", dev=FOCUSER), FOCUS_TRUE))

        moves = self.focuser_cmds()
        f.setAutoFocusParameters(TRAIN, 32, FOCUS_STEP, 10000, 1.0)
        f.setAutoStarEnabled(True, TRAIN)
        self.check("autofocus started", bool(f.start(TRAIN)))
        t0, st, seen = time.time(), None, []
        while time.time() - t0 < 900:
            v = f.status(TRAIN)
            st = FOCUS_STATE.get(int(v[0] if isinstance(v, (tuple, list, dbus.Struct)) else v), str(v))
            pos = self.fw_focuser()
            if pos is not None and (not seen or seen[-1] != pos):
                seen.append(pos)
            if st in ("COMPLETE", "FAILED", "ABORTED") and time.time() - t0 > 10:
                break
            time.sleep(2)
        print("      focuser positions: %s" % " ".join(str(p) for p in seen))
        final = self.fw_focuser()
        indi = self.num("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION" % FOCUSER)
        self.check("autofocus complete", st == "COMPLETE", "%s after %.0f s" % (st, time.time() - t0))
        self.check("focuser ends at the simulated focus", final is not None and
                   abs(final - FOCUS_TRUE) <= FOCUS_STEP, "firmware %s, focus at %d" % (final, FOCUS_TRUE))
        self.check("INDI position matches the firmware", final is not None and abs(indi - final) < 1,
                   "INDI %.0f, firmware %s" % (indi, final))
        self.check("Ekos moved the focuser through the driver", self.focuser_cmds() - moves >= 5,
                   "%d move commands" % (self.focuser_cmds() - moves))
        self.check("focuser stopped", (self.tap.text(":F1T#") or "").startswith("S"), self.tap.text(":F1T#"))
        self.check("focuser position property idle or Ok",
                   self.state("ABS_FOCUS_POSITION", dev=FOCUSER) in ("Ok", "Idle"),
                   self.state("ABS_FOCUS_POSITION", dev=FOCUSER))

    # ------------------------------------------------------------------ guide
    def guide(self):
        print("== Ekos calibration and guiding through the OnStepX ST4 output")
        path, name = "/KStars/Ekos/Guide", "org.kde.kstars.Ekos.Guide"
        g = self.s.iface(path, name)

        def p(n):
            return self.s.prop(path, name, n)

        names = (str(p("camera")), str(p("guider")))
        if not self.check("Guide module uses the mount as guider", names == (CCD, DEV), "%s, %s" % names):
            return
        self.s.iface(path, "org.freedesktop.DBus.Properties").Set(name, "exposure", dbus.Double(1.0))
        self.set("%s.SIM_FOCUSING.SIM_FOCUS_POSITION;SIM_FOCUS_MAX;SIM_SEEING=%d;%d;%g"
                 % (CCD, FOCUS_TRUE, SIM_FOCUS_MAX, GUIDE_SEEING))
        self.set("%s.SIMULATOR_SETTINGS.SIM_SATURATION=%g" % (CCD, GUIDE_SATURATION))
        g.setGuiderType(0)              # Ekos's internal guider
        g.setDarkFrameEnabled(False)
        g.setAutoStarEnabled(True)
        g.setSubFrameEnabled(False)
        g.clearCalibration()
        time.sleep(1)

        # Ekos passes through the calibration and dither results in well under
        # the 1 s poll here, so those are read from its log, not its status.
        logs = {k: self.ekos_log_count(k) for k in
                ("Calibration completed.", "Dithering completed successfully.", "Dithering failed")}

        def logged(text):
            return self.ekos_log_count(text) > logs[text]

        pulses = self.pulse_cmds()
        sent_before, log_at = self.pulse_list(), self.kstars_log_size()
        self.check("guiding started", bool(g.guide()))
        t0, seen, guiding_since, dither_at, st = time.time(), [], None, None, None
        while time.time() - t0 < 600:
            st = GUIDE_STATE.get(int(p("status")), "?")
            if not seen or seen[-1] != st:
                seen.append(st)
            if st == "GUIDING" and guiding_since is None:
                guiding_since = time.time()
            if guiding_since and dither_at is None and time.time() - guiding_since > 60:
                # Not checked: the RMS depends on Ekos's guide algorithm and
                # the simulator (Ekos overshoots there with 5 s pulses).
                print("      guiding RMS after 60 s: RA %.2f\", Dec %.2f\"" %
                      tuple(float(x) for x in p("axisSigma")))
                self.check("dither accepted", bool(g.dither()))
                dither_at = time.time()
            if dither_at and st == "GUIDING" and (logged("Dithering completed successfully.") or
                                                   logged("Dithering failed")):
                break
            if st in ("CALIBRATION_ERROR", "ABORTED"):
                break
            time.sleep(1)
        print("      Ekos guide states: %s" % " > ".join(seen))
        sent = {d: n - pulses[d] for d, n in self.pulse_cmds().items()}
        detail = " ".join("%s %d" % (d.upper(), sent[d]) for d in "nsew")
        self.check("Ekos pulses went out through the driver in RA", sent["w"] > 0 and sent["e"] > 0, detail)
        self.check("Ekos pulses went out through the driver in Dec", sent["n"] > 0 and sent["s"] > 0, detail)
        self.check("calibration completed", logged("Calibration completed."), " > ".join(seen[:4]))
        # Every correction Ekos logs while guiding must reach the firmware
        # unchanged: direction (Ekos's RA increase is East, Dec increase
        # North) and duration.  The driver also sends calibration and dither
        # pulses, which Ekos logs differently, so this is a subset check.
        want = collections.Counter()
        for m in re.finditer(r"Guiding pulses: RA: (\d+)ms (Increase|Decrease|NO) \w+\s+DEC: (\d+)ms "
                             r"(Increase|Decrease|NO)", self.kstars_log_from(log_at)):
            for ms, d, letters in ((m.group(1), m.group(2), "ew"), (m.group(3), m.group(4), "ns")):
                if d != "NO" and int(ms) > 0:
                    want[(letters[0] if d == "Increase" else letters[1], int(ms))] += 1
        got = self.pulse_list() - sent_before
        missing = want - got
        self.check("each Ekos guide correction reached the firmware as sent",
                   sum(want.values()) >= 5 and not missing,
                   "%d corrections%s" % (sum(want.values()), "; missing " + ", ".join(
                       ":Mg%s%d#" % k for k in sorted(missing)) if missing else ""))
        self.check("guiding", guiding_since is not None)
        self.check("dither completed", logged("Dithering completed successfully."),
                   "failed" if logged("Dithering failed") else "")
        if "GUIDING" not in seen:
            for line in list(p("logText"))[:8]:
                print("      Ekos: %s" % line)
        g.abort()
        time.sleep(2)
        st = self.wait_state("TELESCOPE_TIMED_GUIDE_WE", want=("Ok", "Idle"), timeout=10)
        self.check("guide pulse properties settle after abort", st in ("Ok", "Idle"), st)

    def run(self):
        if not self.setup() or not self.point():
            return
        self.autofocus()
        self.guide()
        print("== park")
        self.mount.park()
        self.check("parked by Ekos", self.wait_park("PARKED", 180) == "PARKED", self.mpark())
        self.ekos.stop()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", required=True, help="controller serial port, e.g. /dev/ttyUSB0")
    ap.add_argument("--baud", default="9600")
    ap.add_argument("--indi-port", type=int, default=7627)
    ap.add_argument("--driver", default=DEFAULT_DRIVER)
    ccd = BUILT_CCD if os.path.exists(BUILT_CCD) else shutil.which("indi_simulator_ccd") or "indi_simulator_ccd"
    ap.add_argument("--ccd", default=ccd, help="CCD Simulator driver binary (default: %(default)s)")
    ap.add_argument("--kstars", default=shutil.which("kstars") or "kstars")
    ap.add_argument("--work-dir", help="directory for logs (default: a new temporary directory)")
    onstepx_safety.add_flag(ap)
    a = ap.parse_args()
    onstepx_safety.require(a)

    work = a.work_dir or tempfile.mkdtemp(prefix="onstepx-ekos-imaging-")
    os.makedirs(work, exist_ok=True)
    print("work directory: %s" % work)
    s = Session(a, work)
    s.KSTARS_LOGGING += ";org.kde.kstars.ekos.focus*=true;org.kde.kstars.ekos.guide*=true"
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
        if not s.start_indiserver([a.ccd]):
            print("FAIL  indiserver did not start (see indiserver.log)")
            return 1
        if not s.start_kstars(KSTARSRC):
            print("FAIL  KStars did not come up on the private bus (see kstars.log)")
            return 1
        t = ImagingE2E(a, s)
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
