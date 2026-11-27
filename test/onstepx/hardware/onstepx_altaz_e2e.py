#!/usr/bin/env python3
# OnStepX INDI Driver — onstepx_altaz_e2e.py
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
End-to-end test of indi_onstepx on an ALTAZM OnStepX controller: AltAz
coordinates (no INDI alignment subsystem; :GR#/:GD# are used for all mount
types), the field de-rotator and "go to parallactic angle".

Expected firmware: the hardware test configuration with MOUNT_TYPE ALTAZM and
PEC off (rotator on Axis3, so :GX98# reports 'D').

Usage:
  onstepx_altaz_e2e.py --port /dev/ttyUSB0 --test-controller [--baud 9600] [--indi-port 7625]
"""

import argparse
import datetime
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from onstepx_e2e import E2E, DEV, ROT, BAUDS  # noqa: E402
import onstepx_safety  # noqa: E402

LAT, LON_EAST = 51.5, 359.9


def lst_now():
    now = datetime.datetime.now(datetime.timezone.utc)
    d = now.timestamp() / 86400.0 + 2440587.5 - 2451545.0
    return ((18.697374558 + 24.06570982441908 * d) % 24.0 + LON_EAST / 15.0) % 24.0


def parallactic(ha_hours, dec_deg, lat_deg=LAT):
    """Firmware Rotator::parallacticAngle(): atan2(sin H, cos d tan phi - sin d cos H), degrees."""
    h, d, p = math.radians(ha_hours * 15.0), math.radians(dec_deg), math.radians(lat_deg)
    return math.degrees(math.atan2(math.sin(h), math.cos(d) * math.tan(p) - math.sin(d) * math.cos(h)))


def angdiff(a, b):
    return (a - b + 180.0) % 360.0 - 180.0


class AltAzE2E(E2E):
    def run(self):
        print("== connect")
        self.check("connect", self.connect())
        mount_type = self.get("%s.TELESCOPE_MOUNT_TYPE.ALTAZ" % DEV)
        self.check("mount type reported as AltAz", mount_type == "On", mount_type)
        props = self.get("%s.OSX_ROT_DEROTATE.DEROTATE_ON" % ROT), self.get("%s.OSX_ROT_PARALLACTIC.PARALLACTIC_GOTO" % ROT)
        self.check("de-rotator property defined", props[0] in ("On", "Off"), props[0])
        self.check("parallactic property defined", props[1] in ("On", "Off"), props[1])

        now = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0)
        self.set("%s.TIME_UTC.UTC;OFFSET=%s;0" % (DEV, now.strftime("%Y-%m-%dT%H:%M:%S")))
        self.wait_state("TIME_UTC", timeout=10)
        self.set("%s.GEOGRAPHIC_COORD.LAT;LONG;ELEV=%g;%g;50" % (DEV, LAT, LON_EAST))
        self.wait_state("GEOGRAPHIC_COORD", timeout=10)
        self.set("%s.TELESCOPE_PARK.UNPARK=On" % DEV)
        time.sleep(2)
        self.set("%s.TELESCOPE_TRACK_STATE.TRACK_ON=On" % DEV)
        self.set("%s.OSX_SLEW_RATE_MAX.SLEW_RATE_MAX=2" % DEV)
        time.sleep(3)

        print("== AltAz coordinates through :GR#/:GD#")
        lst = lst_now()
        for ha, dec in ((-1.0, 45.5), (-2.0, 5.5), (1.5, -5.5), (2.5, 0.25)):
            ra = (lst - ha) % 24.0
            self.set("%s.TELESCOPE_TRACK_STATE.TRACK_ON=On" % DEV)
            time.sleep(1)
            self.set("%s.ON_COORD_SET.SYNC=On" % DEV)
            self.set("%s.EQUATORIAL_EOD_COORD.RA;DEC=%.6f;%.6f" % (DEV, ra, dec))
            st = self.wait_state("EQUATORIAL_EOD_COORD", timeout=15, settle=1.0)
            time.sleep(1.5)
            r = self.num("%s.EQUATORIAL_EOD_COORD.RA" % DEV)
            d = self.num("%s.EQUATORIAL_EOD_COORD.DEC" % DEV)
            dra = abs(((r - ra + 12) % 24) - 12) * 15
            self.check("sync HA %+.1f Dec %+6.2f" % (ha, dec), st == "Ok" and dra < 0.1 and abs(d - dec) < 0.1,
                       "state %s, reads %.4fh %+.4f" % (st, r, d))

        ha_t, dec_t = 2.0, 30.0
        ra_t = (lst_now() - ha_t) % 24.0
        self.set("%s.ON_COORD_SET.TRACK=On" % DEV)
        self.set("%s.EQUATORIAL_EOD_COORD.RA;DEC=%.6f;%.6f" % (DEV, ra_t, dec_t))
        st = self.wait_state("EQUATORIAL_EOD_COORD", timeout=240, settle=2.0)
        r = self.num("%s.EQUATORIAL_EOD_COORD.RA" % DEV)
        d = self.num("%s.EQUATORIAL_EOD_COORD.DEC" % DEV)
        dra = abs(((r - ra_t + 12) % 24) - 12) * 15
        self.check("AltAz goto on target", st == "Ok" and dra < 0.05 and abs(d - dec_t) < 0.05,
                   "state %s, reads %.4fh %+.4f" % (st, r, d))

        print("== go to parallactic angle")
        # The firmware computes the target once, from the position when :rP#
        # arrives, so the expected angle is taken at the button press.
        ha_now = (lst_now() - self.num("%s.EQUATORIAL_EOD_COORD.RA" % DEV)) % 24.0
        ha_now = ha_now - 24.0 if ha_now > 12.0 else ha_now
        q = parallactic(ha_now, self.num("%s.EQUATORIAL_EOD_COORD.DEC" % DEV))
        self.set("%s.OSX_ROT_PARALLACTIC.PARALLACTIC_GOTO=On" % ROT)
        time.sleep(0.5)
        busy = self.state("ABS_ROTATOR_ANGLE", dev=ROT)
        self.check("rotator shows Busy as soon as the move starts", busy == "Busy", busy)
        st = self.wait_state("ABS_ROTATOR_ANGLE", timeout=360, settle=0.5, dev=ROT)
        time.sleep(6)
        a = self.num("%s.ABS_ROTATOR_ANGLE.ANGLE" % ROT)
        self.check("rotator at parallactic angle", st == "Ok" and abs(angdiff(a, q)) < 0.5,
                   "%s at %.2f, expected %.2f" % (st, a, q))

        print("== de-rotator")
        self.set("%s.OSX_ROT_DEROTATE.DEROTATE_ON=On" % ROT)
        time.sleep(8)
        self.check("de-rotator switch stays On after polling",
                   self.get("%s.OSX_ROT_DEROTATE.DEROTATE_ON" % ROT) == "On",
                   self.state("OSX_ROT_DEROTATE", dev=ROT))
        a0, t0 = self.num("%s.ABS_ROTATOR_ANGLE.ANGLE" % ROT), time.time()
        ha0 = (lst_now() - self.num("%s.EQUATORIAL_EOD_COORD.RA" % DEV)) % 24.0
        dec0 = self.num("%s.EQUATORIAL_EOD_COORD.DEC" % DEV)
        time.sleep(90)
        a1, t1 = self.num("%s.ABS_ROTATOR_ANGLE.ANGLE" % ROT), time.time()
        dq = angdiff(parallactic(ha0 + (t1 - t0) / 3600.0 * 1.0027379, dec0), parallactic(ha0, dec0))
        moved = angdiff(a1, a0)
        self.check("de-rotator follows the field rotation", abs(moved) > 0.05 and abs(abs(moved) - abs(dq)) < 0.1,
                   "rotated %+.3f deg in %.0f s, parallactic change %+.3f" % (moved, t1 - t0, dq))

        self.set("%s.OSX_ROT_DEROTATE.DEROTATE_OFF=On" % ROT)
        time.sleep(8)
        a2 = self.num("%s.ABS_ROTATOR_ANGLE.ANGLE" % ROT)
        time.sleep(30)
        a3 = self.num("%s.ABS_ROTATOR_ANGLE.ANGLE" % ROT)
        self.check("de-rotator off stops rotation", abs(angdiff(a3, a2)) < 0.02 and
                   self.get("%s.OSX_ROT_DEROTATE.DEROTATE_OFF" % ROT) == "On",
                   "moved %+.3f deg in 30 s" % angdiff(a3, a2))

        print("== de-rotator direction (:rR# toggles; set while stopped)")
        self.set("%s.OSX_ROT_DEROTATE_REVERSE.DEROTATE_REVERSED=On" % ROT)
        time.sleep(3)
        self.check("de-rotator direction Reversed",
                   self.get("%s.OSX_ROT_DEROTATE_REVERSE.DEROTATE_REVERSED" % ROT) == "On" and
                   self.state("OSX_ROT_DEROTATE_REVERSE", dev=ROT) == "Ok",
                   self.state("OSX_ROT_DEROTATE_REVERSE", dev=ROT))
        self.set("%s.OSX_ROT_DEROTATE_REVERSE.DEROTATE_REVERSED=On" % ROT)   # again: must not toggle back
        time.sleep(3)
        self.check("selecting Reversed twice keeps it reversed",
                   self.get("%s.OSX_ROT_DEROTATE_REVERSE.DEROTATE_REVERSED" % ROT) == "On")
        self.set("%s.OSX_ROT_DEROTATE.DEROTATE_ON=On" % ROT)
        time.sleep(8)
        a4 = self.num("%s.ABS_ROTATOR_ANGLE.ANGLE" % ROT)
        time.sleep(60)
        a5 = self.num("%s.ABS_ROTATOR_ANGLE.ANGLE" % ROT)
        self.set("%s.OSX_ROT_DEROTATE.DEROTATE_OFF=On" % ROT)
        time.sleep(8)
        rev = angdiff(a5, a4)
        self.check("reversed de-rotator turns the other way", rev * moved < 0 and abs(rev) > 0.02,
                   "%+.3f deg in 60 s (normal: %+.3f in 90 s)" % (rev, moved))
        self.set("%s.OSX_ROT_DEROTATE_REVERSE.DEROTATE_NORMAL=On" % ROT)
        time.sleep(3)
        self.check("de-rotator direction Normal again",
                   self.get("%s.OSX_ROT_DEROTATE_REVERSE.DEROTATE_NORMAL" % ROT) == "On" and
                   self.state("OSX_ROT_DEROTATE_REVERSE", dev=ROT) == "Ok")

        probs = self.listener.problems()
        for t, m in probs:
            print("      %s %s" % (t, m))
        self.check("no driver errors or warnings", not probs, "%d message(s)" % len(probs))

        print("\n" + "  ".join("%s: %d" % (k, sum(1 for r in self.results if r[0] == k))
                               for k in ("PASS", "FAIL", "SKIP")))
        return sum(1 for r in self.results if r[0] == "FAIL")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", default="9600", choices=BAUDS)
    ap.add_argument("--indi-port", type=int, default=7624)
    onstepx_safety.add_flag(ap)
    a = ap.parse_args()
    onstepx_safety.require(a)
    return 1 if AltAzE2E(a).run() else 0


if __name__ == "__main__":
    sys.exit(main())
