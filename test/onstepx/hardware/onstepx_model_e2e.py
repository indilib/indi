#!/usr/bin/env python3
# OnStepX INDI Driver — onstepx_model_e2e.py
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
End-to-end test of the pointing-model builder on a real OnStepX controller.

1. Through INDI: start Build Model, Goto ~18 positions on both pier sides and
   Sync each one to coordinates offset by a KNOWN pointing error (axis1 and
   axis2 index errors: a constant RA offset and a Dec offset whose sign follows
   the pier side), then Calculate Model, which fits, uploads, activates and
   persists (:AW#) the model.
2. Directly on the controller (driver disconnected; opening the port resets an
   ESP32, which also proves the model was persisted): for a subset of points
     a) with the uploaded model, Goto the "actual" coordinates and read the raw
        instrument axes (:GX42#/:GX43#) and LST,
     b) with a zero model, Goto the original mount coordinates and read again.
   If the firmware applies the coefficients the way the driver's fitter meant
   them, the axes agree once the elapsed sidereal time is allowed for.  A sign
   or unit mismatch shows up as roughly twice the injected error.

Same prerequisites as onstepx_e2e.py (indiserver with indi_onstepx, a TEST
controller).  Takes about 40 minutes.

Usage:
  onstepx_model_e2e.py --port /dev/ttyUSB0 --test-controller [--baud 9600] [--indi-port 7625]
"""

import argparse
import datetime
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from onstepx_e2e import E2E, DEV, BAUDS  # noqa: E402
import onstepx_safety  # noqa: E402

LAT, LON_EAST, ELEV = 51.5, 359.9, 50
INJECT_RA_ARCSEC = 600.0      # axis1 index error, arcsec of hour angle
INJECT_DEC_ARCSEC = 300.0     # axis2 index error, arcsec

# (hour angle h, Dec deg); both sides of the meridian, kept > ~15 deg altitude
POINTS = [(ha, dec) for dec in (0.0, 30.0, 60.0) for ha in (-4.0, -2.5, -1.2, 1.2, 2.5, 4.0)]
# Re-checked on the controller ~30 min later, when the sky has turned by ~0.5 h:
# skip HA -1.2 (drifts to the meridian, pier side can change) and HA +4
# (drifts past the western limit).
VERIFY = [0, 3, 7, 10, 13, 16]
AXIS_TOLERANCE_ARCSEC = 30.0


def gmst_hours(when):
    d = when.timestamp() / 86400.0 + 2440587.5 - 2451545.0
    return (18.697374558 + 24.06570982441908 * d) % 24.0


def lst_now():
    return (gmst_hours(datetime.datetime.now(datetime.timezone.utc)) + LON_EAST / 15.0) % 24.0


class ModelE2E(E2E):
    def inject(self, ra, dec, pier_west):
        """Apply the synthetic pointing error: mount -> 'actual' sky position."""
        dra = INJECT_RA_ARCSEC / 15.0 / 3600.0
        ddec = INJECT_DEC_ARCSEC / 3600.0 * (-1.0 if pier_west else 1.0)
        return (ra + dra) % 24.0, dec + ddec

    def goto(self, ra, dec, timeout=240):
        self.set("%s.ON_COORD_SET.TRACK=On" % DEV)
        self.set("%s.EQUATORIAL_EOD_COORD.RA;DEC=%.7f;%.7f" % (DEV, ra, dec))
        return self.wait_state("EQUATORIAL_EOD_COORD", want=("Ok",), timeout=timeout, settle=2.0)

    def run(self):
        print("== setup")
        self.check("connect", self.connect())
        now = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0)
        self.set("%s.TIME_UTC.UTC;OFFSET=%s;0" % (DEV, now.strftime("%Y-%m-%dT%H:%M:%S")))
        self.wait_state("TIME_UTC", timeout=10)
        self.set("%s.GEOGRAPHIC_COORD.LAT;LONG;ELEV=%g;%g;%g" % (DEV, LAT, LON_EAST, ELEV))
        self.wait_state("GEOGRAPHIC_COORD", timeout=10)
        self.set("%s.TELESCOPE_PARK.UNPARK=On" % DEV)
        time.sleep(2)
        self.set("%s.TELESCOPE_TRACK_MODE.TRACK_SIDEREAL=On" % DEV)
        self.set("%s.TELESCOPE_TRACK_STATE.TRACK_ON=On" % DEV)
        self.set("%s.OSX_SLEW_RATE_MAX.SLEW_RATE_MAX=2" % DEV)
        time.sleep(3)

        print("== build session")
        self.set("%s.OSX_MODEL_BUILD.BUILD=On" % DEV)
        st = self.wait_state("OSX_MODEL_BUILD", timeout=30)
        self.check("build session started", st == "Ok" and self.get("%s.OSX_MODEL_BUILD.BUILD" % DEV) == "On", st)

        obs = []
        for i, (ha, dec) in enumerate(POINTS):
            target_ra = (lst_now() - ha) % 24.0
            st = self.goto(target_ra, dec)
            m_ra = self.num("%s.EQUATORIAL_EOD_COORD.RA" % DEV)
            m_dec = self.num("%s.EQUATORIAL_EOD_COORD.DEC" % DEV)
            west = self.get("%s.TELESCOPE_PIER_SIDE.PIER_WEST" % DEV) == "On"
            a_ra, a_dec = self.inject(m_ra, m_dec, west)
            self.set("%s.ON_COORD_SET.SYNC=On" % DEV)
            self.set("%s.EQUATORIAL_EOD_COORD.RA;DEC=%.7f;%.7f" % (DEV, a_ra, a_dec))
            time.sleep(2)
            self.set("%s.ON_COORD_SET.TRACK=On" % DEV)
            obs.append(dict(m_ra=m_ra, m_dec=m_dec, a_ra=a_ra, a_dec=a_dec, west=west))
            print("   %2d  HA %+5.1f Dec %+5.1f  goto %-5s pier %s  mount %.5fh %+.4f" %
                  (i + 1, ha, dec, st, "W" if west else "E", m_ra, m_dec))

        captured = [m for _, m in self.listener.messages if "captured as observation" in m]
        self.check("all observations captured", len(captured) >= len(POINTS),
                   "%d of %d" % (len(captured), len(POINTS)))
        self.check("both pier sides observed",
                   any(o["west"] for o in obs) and any(not o["west"] for o in obs))
        # A captured Sync does not sync the mount; the session shows on the Model tab.
        count = self.get("%s.OSX_MODEL_OBSERVATIONS.COUNT" % DEV)
        self.check("observation count published", count == str(len(captured)), "COUNT=%s" % count)
        self.check("observation count Busy during the session", self.state("OSX_MODEL_OBSERVATIONS") == "Busy")

        print("== calculate and upload")
        self.set("%s.OSX_MODEL_CONTROL.CALCULATE=On" % DEV)
        st = self.wait_state("OSX_MODEL_CONTROL", timeout=120, settle=2.0)
        fit = [m for _, m in self.listener.messages if "Calculate Model" in m]
        for m in fit:
            print("      " + m)
        self.check("calculate model succeeded", st == "Ok", st)
        self.check("build session ended", self.get("%s.OSX_MODEL_BUILD.BUILD" % DEV) == "Off")
        self.check("observation count Idle after the session", self.state("OSX_MODEL_OBSERVATIONS") == "Idle")

        print("== verify on the controller")
        self.disconnect()
        self.verify_on_controller(obs)

        probs = self.listener.problems()
        for t, m in probs:
            print("      %s %s" % (t, m))
        self.check("no driver errors or warnings", not probs, "%d message(s)" % len(probs))

        print("\n" + "  ".join("%s: %d" % (k, sum(1 for r in self.results if r[0] == k))
                               for k in ("PASS", "FAIL", "SKIP")))
        return sum(1 for r in self.results if r[0] == "FAIL")

    # ---------------------------------------------------------------- serial
    def verify_on_controller(self, obs):
        import serial
        s = serial.Serial(self.a.port, int(self.a.baud), timeout=0)
        time.sleep(3.5)
        s.read(65536)

        def q(cmd, wait=1.5):
            s.write(cmd.encode())
            t, d = time.time(), b""
            while time.time() - t < wait:
                d += s.read(256)
                c = d.replace(b"\xff", b"")
                if c.endswith(b"#") or (len(c) == 1 and time.time() - t > 0.25):
                    break
                time.sleep(0.005)
            return d.replace(b"\xff", b"").rstrip(b"#").decode("latin-1")

        q(":GVP#")      # warm-up after reset

        coeffs = {i: q(":GX0%c#" % i) for i in "012345678abcd"}
        print("      model after reset: " + " ".join("%s=%s" % kv for kv in coeffs.items()))
        nonzero = [k for k, v in coeffs.items() if v not in ("0", "")]
        self.check("model persisted across controller reset", bool(nonzero), "non-zero: %s" % ",".join(nonzero))

        # The controller lost its clock on reset (no RTC): set UTC directly.
        now = datetime.datetime.now(datetime.timezone.utc)
        q(":SG+00#")
        q(":SL%02d:%02d:%02d#" % (now.hour, now.minute, now.second))
        q(":SC%02d/%02d/%02d#" % (now.month, now.day, now.year % 100))
        q(":Te#")

        def fmt_ra(h):
            t = round(h * 3600 * 10000) % (24 * 3600 * 10000)
            f, t = t % 10000, t // 10000
            return "%02d:%02d:%02d.%04d" % (t // 3600, (t // 60) % 60, t % 60, f)

        def fmt_dec(d):
            sgn = "-" if d < 0 else "+"
            t = round(abs(d) * 3600 * 1000)
            f, t = t % 1000, t // 1000
            return "%s%02d:%02d:%02d.%03d" % (sgn, t // 3600, (t // 60) % 60, t % 60, f)

        def sexa(x):
            return E2E.sexa(x)

        def goto_and_read(ra, dec):
            q(":Sr%s#" % fmt_ra(ra))
            q(":Sd%s#" % fmt_dec(dec))
            r = q(":MS#")
            if r != "0":
                return None, "MS %r" % r
            end = time.time() + 240
            time.sleep(1)
            while time.time() < end:
                if "N" in q(":GU#"):
                    break
                time.sleep(1)
            time.sleep(1.5)
            ax1, lst, ax2, pier = q(":GX42#"), q(":GSH#"), q(":GX43#"), q(":Gm#")
            try:
                return (float(ax1), float(ax2), sexa(lst), pier), ""
            except ValueError:
                return None, "read %r %r %r" % (ax1, ax2, lst)

        # a) uploaded model active
        with_model = {}
        for i in VERIFY:
            o = obs[i]
            with_model[i] = goto_and_read(o["a_ra"], o["a_dec"])

        # b) zero model
        for idx in "012345678abcd":
            q(":SX0%c,0#" % idx)
        q(":SX09,2#")
        no_model = {}
        for i in VERIFY:
            o = obs[i]
            no_model[i] = goto_and_read(o["m_ra"], o["m_dec"])

        # restore the uploaded model so the controller is left as Calculate set it
        for k, v in coeffs.items():
            if v not in ("",):
                q(":SX0%c,%s#" % (k, v))
        q(":SX09,2#")
        q(":Q#")
        s.close()

        for i in VERIFY:
            (a, ea), (b, eb) = with_model[i], no_model[i]
            name = "point %2d axes agree (model vs uploaded intent)" % (i + 1)
            if a is None or b is None:
                self.check(name, False, ea or eb)
                continue
            if a[3] != b[3]:
                self.check(name, False, "pier side differs %s/%s" % (a[3], b[3]))
                continue
            dlst = ((a[2] - b[2] + 12) % 24 - 12) * 15.0 * 3600.0       # arcsec
            d1 = (a[0] - b[0]) * 3600.0
            d1 = (d1 + 648000) % 1296000 - 648000 - dlst
            d2 = (a[1] - b[1]) * 3600.0
            self.check(name, abs(d1) < AXIS_TOLERANCE_ARCSEC and abs(d2) < AXIS_TOLERANCE_ARCSEC,
                       "pier %s  d_axis1 %+.1f\"  d_axis2 %+.1f\"" % (a[3], d1, d2))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", default="9600", choices=BAUDS)
    ap.add_argument("--indi-port", type=int, default=7624)
    onstepx_safety.add_flag(ap)
    a = ap.parse_args()
    onstepx_safety.require(a)
    return 1 if ModelE2E(a).run() else 0


if __name__ == "__main__":
    sys.exit(main())
