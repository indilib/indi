#!/usr/bin/env python3
# OnStepX INDI Driver — onstepx_controls_e2e.py
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
End-to-end test of the indi_onstepx controls that onstepx_e2e.py does not
cover, against a real OnStepX controller (the same test controller and
firmware configuration, see the README in this directory).

Covers:
  * INDI config load (Ekos sends one on connect): settings the firmware keeps
    in NV are left as the firmware has them; RAM settings are restored.
  * Slew rates: none of them may change the NV pulse-guide rate.
  * Manual motion and timed guide pulses West/East, reverse motion.
  * Park options: no INDI park position, Default refused.
  * Tracking compensation and its axes, sidereal clock adjust.
  * Meridian and horizon limits, auto flip, backlash, home at boot, site
    name, DUT1, reticle, MCU temperature, decoded status.
  * Two-star alignment through the OnStepX alignment properties.
  * PEC record / save / play / stop / clear.
  * Focuser abort, sync, backlash, presets, read-only max, no temperature
    without a sensor; rotator abort, presets and home.
  * Reconnect cycles, and a controller reset while connected.

Changes NV settings on the controller and restores the defaults it found,
except the alignment model and PEC buffer; erase NV afterwards.

Usage:
  indiserver -p 7625 build/drivers/telescope/onstepx/indi_onstepx &
  onstepx_controls_e2e.py --port /dev/ttyUSB0 --test-controller [--baud 9600] [--indi-port 7625]
"""

import argparse
import datetime
import os
import re
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from onstepx_e2e import E2E, DEV, FOC, ROT, PWR, BAUDS   # noqa: E402
import onstepx_safety  # noqa: E402

# Driver messages this test provokes on purpose.
EXPECTED_MESSAGES = (
    "OnStepX has no default park position",
    "OnStepX focusers can only be synced to 0",
)


class Controls(E2E):

    # ------------------------------------------------------------- helpers
    def on(self, prop, elem, dev=DEV):
        return self.get("%s.%s.%s" % (dev, prop, elem)) == "On"

    def text(self, prop, elem, dev=DEV):
        return self.get("%s.%s.%s" % (dev, prop, elem))

    def press(self, prop, elem, dev=DEV, timeout=10, want=("Ok", "Idle")):
        self.set("%s.%s.%s=On" % (dev, prop, elem))
        return self.wait_state(prop, want=want, timeout=timeout, dev=dev)

    def numbers(self, prop, values, dev=DEV, timeout=10):
        names = ";".join(values.keys())
        vals = ";".join(str(v) for v in values.values())
        self.set("%s.%s.%s=%s" % (dev, prop, names, vals))
        return self.wait_state(prop, timeout=timeout, dev=dev)

    def radec(self):
        return self.num("%s.EQUATORIAL_EOD_COORD.RA" % DEV), self.num("%s.EQUATORIAL_EOD_COORD.DEC" % DEV)

    @staticmethod
    def dra_arcsec(a, b):
        return (((b - a + 12) % 24) - 12) * 15 * 3600

    def goto(self, ha, dec, timeout=180):
        ra = (self.lst() - ha) % 24.0
        self.set("%s.ON_COORD_SET.TRACK=On" % DEV)
        self.set("%s.EQUATORIAL_EOD_COORD.RA;DEC=%.6f;%.6f" % (DEV, ra, dec))
        st = self.wait_state("EQUATORIAL_EOD_COORD", timeout=timeout, settle=2.0)
        r, d = self.radec()
        return st == "Ok" and abs(self.dra_arcsec(ra, r)) < 180 and abs(d - dec) < 0.05, ra, dec

    def fw(self, cmds):
        """Query the controller directly (driver disconnected meanwhile)."""
        q = self.serial_queries(cmds)
        for k, v in q.items():
            print("      %-12s %r" % (k, v))
        return q

    @staticmethod
    def deg(reply):
        """Integer degrees from a limit reply such as '-8*'."""
        try:
            return int(reply.rstrip("*"))
        except ValueError:
            return None

    def lst_rate(self, secs=45.0):
        """Controller sidereal time rate minus the sidereal rate, measured
        against the host clock (driver disconnected meanwhile)."""
        import serial
        self.disconnect()
        s = serial.Serial(self.a.port, int(self.a.baud), timeout=0)
        time.sleep(3.0)
        s.read(65536)

        def lst():
            for _ in range(3):
                s.write(b":GSH#")
                t, d = time.time(), b""
                while time.time() - t < 1.0:
                    d += s.read(64)
                    if d.endswith(b"#"):
                        break
                    time.sleep(0.002)
                m = re.match(rb"(\d+):(\d+):([\d.]+)#", d.replace(b"\xff", b""))
                if m:
                    return t, int(m.group(1)) * 3600 + int(m.group(2)) * 60 + float(m.group(3))
                time.sleep(0.5)
            raise RuntimeError("no :GSH# reply")

        t0, s0 = lst()
        time.sleep(secs)
        t1, s1 = lst()
        s.close()
        if not self.connect():
            raise RuntimeError("reconnect failed")
        ds = (s1 - s0) % 86400
        return ds / (t1 - t0) / 1.0027379093 - 1.0

    def gu(self, q):
        return q.get(":GU#", "")

    # ----------------------------------------------------------------- run
    def run(self):
        print("== connect")
        if not self.check("connect", self.connect()):
            return 1
        now = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0)
        self.set("%s.TIME_UTC.UTC;OFFSET=%s;0" % (DEV, now.strftime("%Y-%m-%dT%H:%M:%S")))
        self.wait_state("TIME_UTC", timeout=10)
        self.set("%s.GEOGRAPHIC_COORD.LAT;LONG;ELEV=51.5;359.9;50" % DEV)
        self.wait_state("GEOGRAPHIC_COORD", timeout=10)
        self.set("%s.TELESCOPE_PARK.UNPARK=On" % DEV)
        self.wait_state("TELESCOPE_PARK", timeout=15)
        self.set("%s.TELESCOPE_TRACK_STATE.TRACK_ON=On" % DEV)
        time.sleep(2)

        sections = (self.config_load, self.slew_and_guide_rates, self.motion_we, self.park_options,
                    self.tracking, self.settings, self.status_display, self.alignment, self.pec,
                    self.focuser, self.rotator, self.reconnects, self.controller_reset)
        for s in sections:
            try:
                s()
            except Exception as e:          # keep going: report the rest
                self.check("%s ran" % s.__name__, False, "%s: %s" % (type(e).__name__, e))
                if self.get("%s.CONNECTION.CONNECT" % DEV) != "On":
                    self.connect()

        print("== driver errors and warnings during the run")
        reboot = getattr(self, "reboot_messages", set())
        probs = [(t, m) for t, m in self.listener.problems()
                 if not any(x in m for x in EXPECTED_MESSAGES) and m not in reboot]
        for t, m in probs:
            print("      %s %s" % (t, m))
        self.check("no unexpected driver errors or warnings", not probs, "%d message(s)" % len(probs))

        print("\n" + "  ".join("%s: %d" % (k, sum(1 for r in self.results if r[0] == k))
                               for k in ("PASS", "FAIL", "SKIP")))
        return sum(1 for r in self.results if r[0] == "FAIL")

    # ------------------------------------------------------- config load
    def config_load(self):
        print("== INDI config load: NV settings kept, RAM settings restored")
        # Values A, set through INDI and saved in the INDI config.
        self.numbers("OSX_MOUNT_BACKLASH", {"MOUNT_BACKLASH_AXIS1": 30, "MOUNT_BACKLASH_AXIS2": 40})
        self.numbers("HORIZON_LIMITS", {"HORIZON_MIN": -5, "HORIZON_MAX": 85})
        self.press("OSX_AUTO_FLIP", "AUTO_FLIP_ON")
        self.press("OSX_PREFERRED_PIER", "PIER_EAST")
        self.numbers("GUIDE_RATE", {"GUIDE_RATE_WE": 0.5, "GUIDE_RATE_NS": 0.5})
        self.press("OSX_HOME_PAUSE", "HOME_PAUSE_ON")
        self.press("POWER_CHANNELS", "POWER_CHANNEL_1", dev=PWR)
        self.press("FOCUS_BACKLASH_TOGGLE", "INDI_ENABLED", dev=FOC % 1)
        self.numbers("FOCUS_BACKLASH_STEPS", {"FOCUS_BACKLASH_VALUE": 20}, dev=FOC % 1)
        # RAM settings.
        self.press("TELESCOPE_TRACK_MODE", "TRACK_LUNAR")
        self.press("TELESCOPE_SLEW_RATE", "2x")
        self.numbers("OSX_WEATHER_SET", {"WEATHER_SET_TEMPERATURE": 7, "WEATHER_SET_PRESSURE": 990,
                                         "WEATHER_SET_HUMIDITY": 40})
        self.numbers("OSX_DUT1", {"DUT1": 0.3})
        self.press("USB_PORTS", "USB_PORT_2", dev=PWR)     # firmware port 3
        time.sleep(1)
        for dev in (DEV, PWR, FOC % 1):
            self.press("CONFIG_PROCESS", "CONFIG_SAVE", dev=dev)
        time.sleep(1)

        # Values B, set on the controller behind the driver's back (as the
        # hand controller or web UI would); RAM settings reset too.
        self.fw([":$BR10#", ":$BD15#", ":Sh-8#", ":So82#", ":SX95,0#", ":SX96,W#", ":R0#", ":SX98,0#",
                 ":SXX1,V0#", ":F1B5#", ":TQ#", ":SX9A,20#", ":SUX3,V0#"])

        print("   reconnected; loading the INDI config as Ekos does")
        for dev in (DEV, PWR, FOC % 1):
            self.press("CONFIG_PROCESS", "CONFIG_LOAD", dev=dev)
        time.sleep(8)

        nv = [("backlash", "%s.OSX_MOUNT_BACKLASH.MOUNT_BACKLASH_AXIS1" % DEV, 10),
              ("horizon minimum", "%s.HORIZON_LIMITS.HORIZON_MIN" % DEV, -8),
              ("overhead limit", "%s.HORIZON_LIMITS.HORIZON_MAX" % DEV, 82),
              ("pulse-guide rate", "%s.GUIDE_RATE.GUIDE_RATE_WE" % DEV, 0.25)]
        for name, spec, want in nv:
            v = self.num(spec)
            self.check("config load keeps NV %s" % name, abs(v - want) < 0.01, "INDI %g (firmware %g)" % (v, want))
        for name, prop, elem, dev in (("auto flip", "OSX_AUTO_FLIP", "AUTO_FLIP_OFF", DEV),
                                      ("pier preference", "OSX_PREFERRED_PIER", "PIER_WEST", DEV),
                                      ("pause at home", "OSX_HOME_PAUSE", "HOME_PAUSE_OFF", DEV),
                                      ("DC output", "POWER_CHANNELS", "POWER_CHANNEL_1", None)):
            if dev:
                ok = self.on(prop, elem, dev)
            else:
                ok = not self.on(prop, elem, PWR)
            self.check("config load keeps NV %s" % name, ok, "%s.%s" % (prop, elem))
        self.check("config load restores track mode Lunar", self.on("TELESCOPE_TRACK_MODE", "TRACK_LUNAR"))
        self.check("config load restores slew rate", self.on("TELESCOPE_SLEW_RATE", "2x"))
        self.check("config load restores USB port", self.on("USB_PORTS", "USB_PORT_2", PWR))
        self.check("config load restores DUT1", abs(self.num("%s.OSX_DUT1.DUT1" % DEV) - 0.3) < 1e-3,
                   self.get("%s.OSX_DUT1.DUT1" % DEV))

        foc_backlash = self.num("%s.FOCUS_BACKLASH_STEPS.FOCUS_BACKLASH_VALUE" % (FOC % 1))
        q = self.fw([":%BR#", ":Gh#", ":Go#", ":GX95#", ":GX96#", ":GX90#", ":GXX1#", ":F1B#", ":GT#",
                     ":GX9A#", ":GUX3#"])
        # The firmware keeps focuser backlash in steps: 5 microns reads back
        # rounded, so compare with what it reports.
        self.check("config load keeps NV focuser backlash", q[":F1B#"] not in ("", "20") and
                   abs(foc_backlash - float(q[":F1B#"])) < 0.5,
                   "INDI %g (firmware %s; config had 20)" % (foc_backlash, q[":F1B#"]))
        self.check("firmware NV untouched by the config load",
                   q[":%BR#"] == "10" and self.deg(q[":Gh#"]) == -8 and self.deg(q[":Go#"]) == 82 and
                   q[":GX95#"] == "0" and q[":GX96#"] == "W" and q[":GX90#"] == "0.25" and
                   q[":GXX1#"].split(";")[0] == "0" and q[":F1B#"] != "20",
                   "backlash %s, limits %s/%s, flip %s, pier %s, pulse %s, aux1 %s, focuser %s" %
                   (q[":%BR#"], q[":Gh#"], q[":Go#"], q[":GX95#"], q[":GX96#"], q[":GX90#"], q[":GXX1#"],
                    q[":F1B#"]))
        self.check("firmware RAM restored by the config load",
                   q[":GT#"].startswith("57.9") and q[":GX9A#"].startswith("7.0") and q[":GUX3#"] == "1",
                   "track %s Hz, temperature %s, USB 3 %s" % (q[":GT#"], q[":GX9A#"], q[":GUX3#"]))

        # Back to the defaults.
        self.numbers("OSX_MOUNT_BACKLASH", {"MOUNT_BACKLASH_AXIS1": 0, "MOUNT_BACKLASH_AXIS2": 0})
        self.numbers("HORIZON_LIMITS", {"HORIZON_MIN": -10, "HORIZON_MAX": 80})
        self.press("OSX_PREFERRED_PIER", "PIER_BEST")
        self.press("FOCUS_BACKLASH_TOGGLE", "INDI_DISABLED", dev=FOC % 1)
        self.press("TELESCOPE_TRACK_MODE", "TRACK_SIDEREAL")
        self.set("%s.USB_PORTS.USB_PORT_2=Off" % PWR)
        self.wait_state("USB_PORTS", timeout=10, dev=PWR)

    # ----------------------------------------------- slew and guide rates
    def slew_and_guide_rates(self):
        print("== slew rates leave the pulse-guide rate alone")
        self.numbers("GUIDE_RATE", {"GUIDE_RATE_WE": 0.25, "GUIDE_RATE_NS": 0.25})
        for elem in ("1x", "2x", "3x", "4x"):
            self.press("TELESCOPE_SLEW_RATE", elem)
        # With Guide (1x) selected, a new guide rate must stick: the driver
        # restores the manual rate after :R<n>#.
        self.press("TELESCOPE_SLEW_RATE", "1x")
        self.numbers("GUIDE_RATE", {"GUIDE_RATE_WE": 0.5, "GUIDE_RATE_NS": 0.5})
        g = self.num("%s.GUIDE_RATE.GUIDE_RATE_WE" % DEV)
        q = self.fw([":GX90#"])
        self.check("guide rate 0.5x kept with the Guide slew rate", q[":GX90#"] == "0.50" and abs(g - 0.5) < 1e-6,
                   "firmware %s, INDI %g" % (q[":GX90#"], g))
        self.press("TELESCOPE_SLEW_RATE", "2x")
        q = self.fw([":GX90#"])
        self.check("pulse-guide rate unchanged after each slew rate", q[":GX90#"] == "0.50", q[":GX90#"])

    # ------------------------------------------------------- West / East
    def motion_we(self):
        print("== manual motion and guide pulses West/East, reverse")
        ok, _, _ = self.goto(-1.5, 20.0)
        self.check("goto east of the meridian", ok)
        self.press("TELESCOPE_SLEW_RATE", "2x")        # Centering, 8x

        def move(prop, elem, secs=2.0):
            r0, d0 = self.radec()
            self.set("%s.%s.%s=On" % (DEV, prop, elem))
            time.sleep(secs)
            self.set("%s.%s.%s=Off" % (DEV, prop, elem))
            time.sleep(3)
            r1, d1 = self.radec()
            return self.dra_arcsec(r0, r1), (d1 - d0) * 3600, self.state(prop)

        w = move("TELESCOPE_MOTION_WE", "MOTION_WEST")
        e = move("TELESCOPE_MOTION_WE", "MOTION_EAST")
        self.check("West moves to lower RA, East back", w[0] < -60 and e[0] > 60 and w[2] in ("Idle", "Ok"),
                   "West %+.0f\", East %+.0f\" RA, state %s" % (w[0], e[0], e[2]))
        self.press("TELESCOPE_REVERSE_MOTION", "REVERSE_WE")
        self.press("TELESCOPE_REVERSE_MOTION", "REVERSE_NS")
        w = move("TELESCOPE_MOTION_WE", "MOTION_WEST")
        n = move("TELESCOPE_MOTION_NS", "MOTION_NORTH")
        self.check("reverse West/East", w[0] > 60, "West %+.0f\" RA" % w[0])
        self.check("reverse North/South", n[1] < -60, "North %+.0f\" Dec" % n[1])
        self.set("%s.TELESCOPE_REVERSE_MOTION.REVERSE_NS;REVERSE_WE=Off;Off" % DEV)
        time.sleep(1)
        n = move("TELESCOPE_MOTION_NS", "MOTION_NORTH")
        self.check("reverse off again", n[1] > 60 and not self.on("TELESCOPE_REVERSE_MOTION", "REVERSE_NS"),
                   "North %+.0f\" Dec" % n[1])

        # 2 s pulses at 1x: 30" on the sky, 2 s of RA.
        self.numbers("GUIDE_RATE", {"GUIDE_RATE_WE": 1.0, "GUIDE_RATE_NS": 1.0})
        for elem, sign in (("TIMED_GUIDE_W", -1), ("TIMED_GUIDE_E", 1)):
            r0, _ = self.radec()
            t0 = time.time()
            self.set("%s.TELESCOPE_TIMED_GUIDE_WE.%s=2000" % (DEV, elem))
            st = self.wait_state("TELESCOPE_TIMED_GUIDE_WE", want=("Idle", "Ok"), timeout=6, settle=0.05)
            dt = time.time() - t0
            time.sleep(2.5)
            r1, _ = self.radec()
            moved = self.dra_arcsec(r0, r1)
            self.check("guide pulse %s 2 s" % elem[-1], st in ("Idle", "Ok") and 1.9 < dt < 3.0 and
                       15 < sign * moved < 45, "%.1f s, RA %+.1f\" (expect %+d\")" % (dt, moved, sign * 30))
        self.numbers("GUIDE_RATE", {"GUIDE_RATE_WE": 0.5, "GUIDE_RATE_NS": 0.5})

    # ------------------------------------------------------- park options
    def park_options(self):
        print("== park options")
        self.check("no INDI park position (OnStepX parks at a saved position)",
                   self.get("%s.TELESCOPE_PARK_POSITION.PARK_RA" % DEV) == "")
        st = self.press("TELESCOPE_PARK_OPTION", "PARK_DEFAULT", want=("Ok", "Alert"))
        self.check("park option Default refused", st == "Alert" and
                   any("no default park position" in m for _, m in self.listener.messages), st)
        st = self.press("TELESCOPE_PARK_OPTION", "PARK_CURRENT", want=("Ok", "Alert"))
        self.check("park option Current accepted", st == "Ok", st)

    # ------------------------------------------------------------ tracking
    def tracking(self):
        print("== tracking compensation, axes and sidereal clock")
        results = {}
        for comp, axes in (("TRACK_COMP_FULL", "TRACK_AXIS_SINGLE"), ("TRACK_COMP_FULL", "TRACK_AXIS_DUAL"),
                           ("TRACK_COMP_REFRACTION", "TRACK_AXIS_DUAL"),
                           ("TRACK_COMP_REFRACTION", "TRACK_AXIS_SINGLE")):
            self.press("OSX_TRACK_COMP", comp)
            self.press("OSX_TRACK_COMP_AXES", axes)
            time.sleep(3)
            shown = (self.on("OSX_TRACK_COMP", comp), self.on("OSX_TRACK_COMP_AXES", axes),
                     self.text("OSX_STATUS", "STATUS_TRACK_COMP"))
            q = self.fw([":GU#"])
            results[(comp, axes)] = shown
            flag = "t" if comp == "TRACK_COMP_FULL" else "r"
            single = "s" in self.gu(q)
            self.check("%s %s reaches the firmware" % (comp[11:], axes[11:]),
                       flag in self.gu(q) and single == (axes == "TRACK_AXIS_SINGLE"),
                       ":GU# %s" % self.gu(q))
            self.check("%s %s shown after reconnect" % (comp[11:], axes[11:]),
                       self.on("OSX_TRACK_COMP", comp) and self.on("OSX_TRACK_COMP_AXES", axes),
                       "status text %r" % self.text("OSX_STATUS", "STATUS_TRACK_COMP"))
            # Regression: with compensation on the track mode must not be
            # reset from status (the firmware omits the base rate then).
            self.check("%s %s keeps track mode Sidereal" % (comp[11:], axes[11:]),
                       self.on("TELESCOPE_TRACK_MODE", "TRACK_SIDEREAL"))
        self.press("OSX_TRACK_COMP", "TRACK_COMP_OFF")
        time.sleep(3)
        q = self.fw([":GU#"])
        self.check("compensation off", "t" not in self.gu(q) and "r" not in self.gu(q), self.gu(q))

        # The firmware has no command that reads the sidereal clock (:GT# is
        # the track rate relative to it), so time the controller's sidereal
        # time against the host clock.  5 x :T+# = +0.1 Hz: +1.66e-3.
        base = self.lst_rate()
        for _ in range(5):
            self.press("OSX_FREQ_ADJ", "FREQ_ADJ_INC")
        fast = self.lst_rate()
        self.press("OSX_FREQ_ADJ", "FREQ_ADJ_DEC")
        self.press("OSX_FREQ_ADJ", "FREQ_ADJ_RESET")
        reset = self.lst_rate()
        self.check("sidereal clock +5 steps runs faster", 1.2e-3 < fast - base < 2.1e-3,
                   "rate %+.5f vs %+.5f (expect +0.00166)" % (fast, base))
        self.check("sidereal clock reset", abs(reset - base) < 3e-4, "rate %+.5f vs %+.5f" % (reset, base))
        self.check("clock adjust buttons are momentary",
                   not any(self.on("OSX_FREQ_ADJ", e) for e in ("FREQ_ADJ_DEC", "FREQ_ADJ_INC", "FREQ_ADJ_RESET")))

    # ------------------------------------------------------------ settings
    def settings(self):
        print("== limits, auto flip, backlash, home at boot, site name, DUT1, reticle")
        self.numbers("MERIDIAN_LIMITS", {"MERIDIAN_EAST": 45, "MERIDIAN_WEST": 30})
        self.numbers("HORIZON_LIMITS", {"HORIZON_MIN": -5, "HORIZON_MAX": 85})
        self.press("OSX_AUTO_FLIP", "AUTO_FLIP_ON")
        self.numbers("OSX_MOUNT_BACKLASH", {"MOUNT_BACKLASH_AXIS1": 25, "MOUNT_BACKLASH_AXIS2": 35})
        self.press("AUTO_HOME", "AUTO_HOME_ON")
        self.set("%s.OSX_SITE_NAME.SITE_NAME=Test Site" % DEV)
        st_name = self.wait_state("OSX_SITE_NAME", timeout=10)
        st_dut = self.numbers("OSX_DUT1", {"DUT1": -0.4})
        st_ret = [self.press("OSX_RETICLE", e) for e in ("RETICLE_PLUS", "RETICLE_MINUS")]
        self.check("reticle buttons accepted and momentary", st_ret == ["Ok", "Ok"] and
                   not self.on("OSX_RETICLE", "RETICLE_PLUS") and not self.on("OSX_RETICLE", "RETICLE_MINUS"),
                   str(st_ret))
        self.check("DUT1 accepted", st_dut == "Ok", st_dut)

        q = self.fw([":GXE9#", ":GXEA#", ":Gh#", ":Go#", ":GX95#", ":%BR#", ":%BD#", ":h?#", ":GM#"])
        self.check("meridian limits E 45 / W 30", q[":GXE9#"] == "45" and q[":GXEA#"] == "30",
                   "%s / %s" % (q[":GXE9#"], q[":GXEA#"]))
        self.check("horizon limits -5 / 85", self.deg(q[":Gh#"]) == -5 and self.deg(q[":Go#"]) == 85,
                   "%s / %s" % (q[":Gh#"], q[":Go#"]))
        self.check("auto flip on", q[":GX95#"] == "1", q[":GX95#"])
        self.check("backlash 25 / 35", q[":%BR#"] == "25" and q[":%BD#"] == "35",
                   "%s / %s" % (q[":%BR#"], q[":%BD#"]))
        self.check("site name sent with _ for the space", st_name == "Ok" and q[":GM#"] == "Test_Site",
                   "%s, firmware %r" % (st_name, q[":GM#"]))
        self.check("site name shown after reconnect", self.text("OSX_SITE_NAME", "SITE_NAME") in
                   ("Test Site", "Test_Site"), self.text("OSX_SITE_NAME", "SITE_NAME"))
        self.check("settings shown after reconnect",
                   abs(self.num("%s.MERIDIAN_LIMITS.MERIDIAN_WEST" % DEV) - 30) < 0.5 and
                   abs(self.num("%s.OSX_MOUNT_BACKLASH.MOUNT_BACKLASH_AXIS2" % DEV) - 35) < 0.5 and
                   self.on("OSX_AUTO_FLIP", "AUTO_FLIP_ON") and self.on("AUTO_HOME", "AUTO_HOME_ON"))

        # Back to the defaults (auto home last: it has no read-back command
        # other than the status flag).
        self.numbers("MERIDIAN_LIMITS", {"MERIDIAN_EAST": 60, "MERIDIAN_WEST": 60})
        self.numbers("HORIZON_LIMITS", {"HORIZON_MIN": -10, "HORIZON_MAX": 80})
        self.press("OSX_AUTO_FLIP", "AUTO_FLIP_OFF")
        self.numbers("OSX_MOUNT_BACKLASH", {"MOUNT_BACKLASH_AXIS1": 0, "MOUNT_BACKLASH_AXIS2": 0})
        self.press("AUTO_HOME", "AUTO_HOME_OFF")
        self.set("%s.OSX_SITE_NAME.SITE_NAME=None" % DEV)
        self.wait_state("OSX_SITE_NAME", timeout=10)
        self.numbers("OSX_DUT1", {"DUT1": 0})
        time.sleep(3)
        self.check("home at boot off again", self.on("AUTO_HOME", "AUTO_HOME_OFF"))

    # ------------------------------------------------------ status display
    def status_display(self):
        print("== decoded status and MCU temperature")
        time.sleep(3)
        s = {e: self.text("OSX_STATUS", e) for e in ("STATUS_TRACKING", "STATUS_PARK", "STATUS_PIER",
                                                       "STATUS_SLEWING", "STATUS_ERROR")}
        mcu = self.num("%s.OSX_MCU_TEMP.MCU_TEMPERATURE" % DEV)
        tracking = self.state("TELESCOPE_TRACK_STATE") == "Busy"
        pier = "East" if self.on("TELESCOPE_PIER_SIDE", "PIER_EAST") else "West"
        q = self.fw([":GU#", ":GX9F#"])
        print("      status", s)
        self.check("status: tracking matches", ("n" not in self.gu(q)) == tracking and
                   (s["STATUS_TRACKING"] != "Idle") == tracking, "%s, :GU# %s" % (s["STATUS_TRACKING"], self.gu(q)))
        self.check("status: park, pier, slewing, error",
                   s["STATUS_PARK"] == "Unparked" and pier in s["STATUS_PIER"] and s["STATUS_SLEWING"] == "No" and
                   s["STATUS_ERROR"] == "None", str(s))
        try:
            fw_mcu = float(q[":GX9F#"])
        except ValueError:
            fw_mcu = float("nan")
        self.check("MCU temperature shown", abs(mcu - fw_mcu) < 3, "INDI %g, firmware %s" % (mcu, q[":GX9F#"]))

    # ----------------------------------------------------------- alignment
    def alignment(self):
        print("== two-star alignment")
        self.press("OSX_ALIGN_STARS", "STARS_2")
        st = self.press("OSX_ALIGN_CONTROL", "START")
        time.sleep(3)
        q = self.fw([":A?#"])
        self.check("alignment started", st == "Ok" and q[":A?#"][1:3] == "12", "%s, :A?# %s" % (st, q[":A?#"]))
        self.check("alignment status shown", self.text("OSX_ALIGN_STATUS", "STATUS") == "In progress" and
                   self.text("OSX_ALIGN_STATUS", "TARGET_STARS") == "2",
                   "%s, star %s of %s" % (self.text("OSX_ALIGN_STATUS", "STATUS"),
                                          self.text("OSX_ALIGN_STATUS", "CURRENT_STAR"),
                                          self.text("OSX_ALIGN_STATUS", "TARGET_STARS")))
        for n, (ha, dec) in enumerate(((-2.0, 30.0), (2.0, 50.0)), 1):
            ok, _, _ = self.goto(ha, dec)
            self.check("alignment star %d goto" % n, ok)
            # "Centre" it: a short nudge north, as a user would.
            self.press("TELESCOPE_SLEW_RATE", "1x")
            self.set("%s.TELESCOPE_MOTION_NS.MOTION_NORTH=On" % DEV)
            time.sleep(1.0)
            self.set("%s.TELESCOPE_MOTION_NS.MOTION_NORTH=Off" % DEV)
            time.sleep(2)
            st = self.press("OSX_ALIGN_CONTROL", "ACCEPT")
            time.sleep(2)
            self.check("alignment star %d accepted" % n, st == "Ok", st)
        time.sleep(12)          # status refreshes on the alignment throttle
        self.check("alignment complete", self.text("OSX_ALIGN_STATUS", "STATUS") == "Complete",
                   "%s, star %s of %s" % (self.text("OSX_ALIGN_STATUS", "STATUS"),
                                          self.text("OSX_ALIGN_STATUS", "CURRENT_STAR"),
                                          self.text("OSX_ALIGN_STATUS", "TARGET_STARS")))
        st = self.press("OSX_ALIGN_WRITE", "WRITE")
        q = self.fw([":A?#", ":GX00#", ":GX01#", ":GX02#", ":GX03#"])
        self.check("alignment written", st == "Ok" and not self.on("OSX_ALIGN_WRITE", "WRITE"), st)
        self.check("alignment model has the nudge", any(q[c] not in ("0", "") for c in (":GX00#", ":GX01#",
                                                                                   ":GX02#", ":GX03#")),
                   " ".join(q[c] for c in (":GX00#", ":GX01#", ":GX02#", ":GX03#")))
        self.check("polar error shown", self.text("OSX_ALIGN_ERROR", "POLAR_ALT") != "" and
                   self.text("OSX_ALIGN_ERROR", "POLAR_AZ") != "",
                   "%s / %s" % (self.text("OSX_ALIGN_ERROR", "POLAR_ALT"), self.text("OSX_ALIGN_ERROR", "POLAR_AZ")))

    # ----------------------------------------------------------------- PEC
    def pec_state(self):
        return [e for e in ("IGNORED", "READY_PLAY", "PLAYING", "READY_RECORD", "RECORDING")
                if self.get("%s.OSX_PEC_STATE.%s" % (DEV, e)) != "Idle"]

    def wait_pec(self, want, timeout=20):
        end = time.time() + timeout
        cur = []
        while time.time() < end:
            cur = self.pec_state()
            if cur == [want]:
                return cur
            time.sleep(1)
        return cur

    def pec(self):
        print("== PEC")
        if self.get("%s.OSX_PEC_STATE.IGNORED" % DEV) == "":
            self.skip("PEC", "not offered by this controller")
            return
        self.press("TELESCOPE_TRACK_MODE", "TRACK_SIDEREAL")
        self.check("PEC worm period shown", self.num("%s.OSX_PEC_WORM_STEPS.WORM_STEPS" % DEV) > 0)
        self.press("OSX_PEC_DATA", "CLEAR")
        self.check("PEC cleared: ignored", self.wait_pec("IGNORED") == ["IGNORED"], str(self.pec_state()))
        self.press("OSX_PEC_CONTROL", "PLAY")
        time.sleep(12)
        self.check("PEC play refused with no recording", self.pec_state() == ["IGNORED"], str(self.pec_state()))
        self.press("OSX_PEC_CONTROL", "READY_RECORD")
        cur = self.wait_pec("READY_RECORD", 15)
        if cur != ["READY_RECORD"]:
            cur = self.wait_pec("RECORDING", 5)
        self.check("PEC ready to record", cur in (["READY_RECORD"], ["RECORDING"]), str(cur))
        cur = self.wait_pec("RECORDING", 120)
        self.check("PEC recording at the (virtual) worm index", cur == ["RECORDING"], str(cur))
        self.press("OSX_PEC_CONTROL", "STOP")
        self.check("PEC stop", self.wait_pec("IGNORED") == ["IGNORED"], str(self.pec_state()))
        self.press("OSX_PEC_DATA", "SAVE")
        time.sleep(4)
        self.press("OSX_PEC_CONTROL", "PLAY")
        cur = self.wait_pec("READY_PLAY", 15)
        if cur != ["READY_PLAY"]:
            cur = self.wait_pec("PLAYING", 5)
        self.check("PEC play after save", cur in (["READY_PLAY"], ["PLAYING"]), str(cur))
        q = self.fw([":$QZ?#"])
        self.check("PEC state reaches the firmware", q[":$QZ?#"][:1] in ("p", "P"), q[":$QZ?#"])
        self.press("OSX_PEC_CONTROL", "STOP")
        self.press("OSX_PEC_DATA", "CLEAR")
        cur = self.wait_pec("IGNORED")
        self.check("PEC control and data buttons are momentary",
                   not any(self.on("OSX_PEC_CONTROL", e) for e in ("PLAY", "STOP", "READY_RECORD")) and
                   not any(self.on("OSX_PEC_DATA", e) for e in ("CLEAR", "SAVE")) and cur == ["IGNORED"])

    # ------------------------------------------------------------- focuser
    def focuser(self):
        print("== focuser: abort, sync, backlash, presets, max, temperature")
        f = FOC % 1
        self.check("focuser max read-only from the firmware",
                   self.num("%s.FOCUS_MAX.FOCUS_MAX_VALUE" % f) == 50000 and self.perm("FOCUS_MAX", f) == "ro",
                   "%s, %s" % (self.get("%s.FOCUS_MAX.FOCUS_MAX_VALUE" % f), self.perm("FOCUS_MAX", f)))
        # The focuser's temperature falls back to the ambient (manual
        # weather) temperature; only "nan" means no temperature at all.
        shown = self.get("%s.FOCUS_TEMPERATURE_NP.FOCUS_TEMPERATURE" % f)
        q = self.fw([":F1t#"])
        if q[":F1t#"] == "nan":
            self.check("no focuser temperature without a source", shown == "", shown)
        else:
            self.check("focuser temperature shown", shown != "" and abs(float(shown) - float(q[":F1t#"])) < 0.5,
                       "INDI %r, firmware %s" % (shown, q[":F1t#"]))
        self.set("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION=1000" % f)
        self.wait_state("ABS_FOCUS_POSITION", timeout=60, dev=f, settle=1.0)
        self.set("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION=40000" % f)
        time.sleep(2)
        st = self.press("FOCUS_ABORT_MOTION", "ABORT", dev=f)
        time.sleep(4)
        p = self.num("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION" % f)
        q = self.fw([":F1T#", ":F1G#"])
        self.check("focuser abort stops part way", st == "Ok" and q[":F1T#"].startswith("S") and
                   1000 < float(q[":F1G#"]) < 40000 and abs(p - float(q[":F1G#"])) < 2,
                   "INDI %g, firmware %s %s" % (p, q[":F1T#"], q[":F1G#"]))
        self.check("focuser position not Busy after abort",
                   self.state("ABS_FOCUS_POSITION", dev=f) in ("Ok", "Idle"), self.state("ABS_FOCUS_POSITION", dev=f))

        self.numbers("FOCUS_SYNC", {"FOCUS_SYNC_VALUE": 500}, dev=f)
        st_bad = self.state("FOCUS_SYNC", dev=f)
        st = self.numbers("FOCUS_SYNC", {"FOCUS_SYNC_VALUE": 0}, dev=f)
        q = self.fw([":F1G#"])
        self.check("focuser sync to 500 refused", st_bad == "Alert", st_bad)
        self.check("focuser sync to 0", st == "Ok" and q[":F1G#"] == "0" and
                   self.num("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION" % f) == 0,
                   "%s, firmware %s" % (st, q[":F1G#"]))

        self.press("FOCUS_BACKLASH_TOGGLE", "INDI_ENABLED", dev=f)
        self.numbers("FOCUS_BACKLASH_STEPS", {"FOCUS_BACKLASH_VALUE": 25}, dev=f)
        q1 = self.fw([":F1B#"])
        self.press("FOCUS_BACKLASH_TOGGLE", "INDI_DISABLED", dev=f)
        q2 = self.fw([":F1B#"])
        # Kept in steps by the firmware, so it reads back rounded.
        self.check("focuser backlash 25, then off", q1[":F1B#"] in ("24", "25", "26") and q2[":F1B#"] == "0",
                   "%s, %s" % (q1[":F1B#"], q2[":F1B#"]))

        self.numbers("Presets", {"PRESET_1": 2500, "PRESET_2": 0, "PRESET_3": 0}, dev=f)
        self.set("%s.Goto.Preset 1=On" % f)
        self.wait_state("ABS_FOCUS_POSITION", timeout=60, dev=f, settle=1.0)
        time.sleep(2)
        p = self.num("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION" % f)
        self.check("focuser preset 1 (2500)", abs(p - 2500) < 1, "at %g" % p)
        self.set("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION=0" % f)
        self.wait_state("ABS_FOCUS_POSITION", timeout=60, dev=f, settle=1.0)

    def perm(self, prop, dev):
        """The property's permission, from its definition."""
        sock = socket.create_connection(("localhost", self.a.indi_port))
        sock.sendall(("<getProperties version='1.7' device='%s' name='%s'/>" % (dev, prop)).encode())
        sock.settimeout(3)
        buf = b""
        try:
            while b"perm=" not in buf:
                data = sock.recv(65536)
                if not data:
                    break
                buf += data
        except socket.timeout:
            pass
        sock.close()
        m = re.search(rb"perm=['\"](\w+)['\"]", buf)
        return m.group(1).decode() if m else ""

    # ------------------------------------------------------------- rotator
    def rotator(self):
        print("== rotator: abort, presets, home")
        self.set("%s.ABS_ROTATOR_ANGLE.ANGLE=0" % ROT)
        self.wait_state("ABS_ROTATOR_ANGLE", timeout=360, settle=1.5, dev=ROT)
        self.set("%s.ABS_ROTATOR_ANGLE.ANGLE=180" % ROT)
        time.sleep(4)
        st = self.press("ROTATOR_ABORT_MOTION", "ABORT", dev=ROT)
        time.sleep(7)
        a = self.num("%s.ABS_ROTATOR_ANGLE.ANGLE" % ROT)
        q = self.fw([":rT#", ":rG#"])
        self.check("rotator abort stops part way", st == "Ok" and not q[":rT#"].startswith("M") and 1 < a < 179 and
                   self.state("ABS_ROTATOR_ANGLE", dev=ROT) != "Busy",
                   "%s at %.2f, firmware %s %s" % (st, a, q[":rT#"], q[":rG#"]))
        self.numbers("Presets", {"PRESET_1": 45, "PRESET_2": 0, "PRESET_3": 0}, dev=ROT)
        self.set("%s.Goto.Preset 1=On" % ROT)
        self.wait_state("ABS_ROTATOR_ANGLE", timeout=360, settle=1.5, dev=ROT)
        time.sleep(6)
        a = self.num("%s.ABS_ROTATOR_ANGLE.ANGLE" % ROT)
        self.check("rotator preset 1 (45 deg)", abs(a - 45) < 0.2, "at %.2f" % a)
        # Home is half travel (:rC#).
        st = self.press("ROTATOR_HOME", "HOME", dev=ROT, timeout=360)
        time.sleep(6)
        a = self.num("%s.ABS_ROTATOR_ANGLE.ANGLE" % ROT)
        q = self.fw([":rI#", ":rM#"])
        try:
            half = (float(q[":rI#"]) + float(q[":rM#"])) / 2
        except ValueError:
            half = float("nan")
        self.check("rotator home (half travel)", st == "Ok" and abs(a - half % 360) < 0.2,
                   "%s at %.2f, half travel %.2f" % (st, a, half))

    # ----------------------------------------------------------- robustness
    def children(self):
        return [self.get("%s.CONNECTION.CONNECT" % d) for d in (FOC % 1, FOC % 2, ROT, PWR)]

    def reconnects(self):
        print("== reconnect cycles")
        n_props = None
        for i in range(3):
            self.disconnect()
            gone = [self.get("%s.CONNECTION.DISCONNECT" % d) for d in (FOC % 1, ROT, PWR)]
            self.connect()
            r = subprocess.run(["indi_getprop", "-p", str(self.a.indi_port), "-t", "3", "*.*.*"],
                               capture_output=True, text=True)
            lines = r.stdout.splitlines()
            names = [l.split("=")[0] for l in lines]
            # INDI::Telescope defines DOME_POLICY twice (ISGetProperties and
            # updateProperties), so compare with the first cycle.
            n_props = n_props or sorted(names)
            self.check("cycle %d: children follow the parent" % (i + 1),
                       gone == ["On"] * 3 and self.children() == ["On"] * 4, "%s -> %s" % (gone, self.children()))
            self.check("cycle %d: same properties as the first" % (i + 1), sorted(names) == n_props,
                       "%d elements (first %d)" % (len(names), len(n_props)))

    def controller_reset(self):
        print("== controller reset while connected (driver behind the serial tap)")
        from onstepx_tap import Tap
        link = "/tmp/onstepx-controls-tap"
        self.disconnect()
        tap = Tap(self.a.port, int(self.a.baud), link)
        tap.start()
        try:
            port = self.a.port
            self.a.port = link
            ok = self.connect()
            self.a.port = port
            self.check("driver connected through the tap", ok)
            self.set("%s.TELESCOPE_TRACK_STATE.TRACK_ON=On" % DEV)
            time.sleep(4)
            self.check("tracking before the reset", self.on("TELESCOPE_TRACK_STATE", "TRACK_ON"))
            n_msgs = len(self.listener.messages)
            tap.reset_controller()
            t0 = time.time()
            time.sleep(20)
            # The firmware boots unparked, not tracking, at home.
            ra1, _ = self.radec()
            time.sleep(4)
            ra2, _ = self.radec()
            self.check("driver still connected after the reset", self.get("%s.CONNECTION.CONNECT" % DEV) == "On")
            self.check("polling resumed (RA drifts: not tracking after boot)",
                       abs(self.dra_arcsec(ra1, ra2)) > 30, "%+.0f\" in 4 s" % self.dra_arcsec(ra1, ra2))
            self.check("tracking state mirrors the rebooted firmware", self.on("TELESCOPE_TRACK_STATE", "TRACK_OFF"),
                       self.state("TELESCOPE_TRACK_STATE"))
            self.check("at home shown after the reboot", self.text("OSX_STATUS", "STATUS_AT_HOME") == "Yes",
                       self.text("OSX_STATUS", "STATUS_AT_HOME"))
            gu = tap.text(":GU#")
            self.check("controller rebooted", "n" in gu and "H" in gu, "%.0f s, :GU# %s" % (time.time() - t0, gu))
            during = [m for _, m in self.listener.messages[n_msgs:] if "[ERROR]" in m or "[WARNING]" in m]
            for m in during:
                print("      during the reboot: %s" % m)
            # Errors while the controller is away are expected; they must stop.
            n_after = len(self.listener.messages)
            time.sleep(15)
            later = [m for _, m in self.listener.messages[n_after:] if "[ERROR]" in m or "[WARNING]" in m]
            self.check("no errors once the controller is back", not later, "; ".join(later[:3]))
            self.disconnect()
        finally:
            tap.stop()
        self.connect()
        # Errors logged while the controller rebooted are expected.
        self.reboot_messages = set(m for _, m in self.listener.messages[n_msgs:n_after])

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", default="9600", choices=BAUDS)
    ap.add_argument("--indi-port", type=int, default=7624)
    onstepx_safety.add_flag(ap)
    a = ap.parse_args()
    onstepx_safety.require(a)
    return 1 if Controls(a).run() else 0


if __name__ == "__main__":
    sys.exit(main())
