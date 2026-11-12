#!/usr/bin/env python3
# OnStepX INDI Driver — onstepx_aux_e2e.py
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
End-to-end test of indi_onstepx_aux on a real, MOUNT-LESS OnStepX controller.

Needs indiserver running BOTH drivers so the mount driver's refusal can be
checked too:
    indiserver -p 7625 build/drivers/telescope/onstepx/indi_onstepx \\
                       build/drivers/telescope/onstepx/indi_onstepx_aux

Expected firmware: the hardware test configuration with AXIS1/AXIS2 and PEC
off (rotator, focusers 1 and 2, aux features 1-5, USB plugin ports 1 and 3).

Usage:
  onstepx_aux_e2e.py --port /dev/ttyUSB0 --test-controller [--baud 9600] [--indi-port 7625]
"""

import argparse
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from onstepx_e2e import E2E, BAUDS  # noqa: E402
import onstepx_safety  # noqa: E402

AUX = "OnStepX Controller"
AUX_FOC = AUX + " Focuser %d"   # child devices are named after the parent
AUX_ROT = AUX + " Rotator"
AUX_PWR = AUX + " Power"        # switch slots 1 and 5 -> DC channels 1-2, USB ports 1 and 3 -> 1-2
MOUNT = "OnStepX"


class AuxE2E(E2E):
    def connect_dev(self, dev):
        self.set("%s.CONNECTION_MODE.CONNECTION_SERIAL=On" % dev)
        self.set("%s.DEVICE_PORT.PORT=%s" % (dev, self.a.port))
        self.set("%s.DEVICE_BAUD_RATE.%s=On" % (dev, self.a.baud))
        self.set("%s.CONNECTION.CONNECT=On" % dev)
        self.wait_state("CONNECTION", timeout=40, dev=dev)
        time.sleep(3)
        return self.get("%s.CONNECTION.CONNECT" % dev) == "On"

    def disconnect_dev(self, dev):
        self.set("%s.CONNECTION.DISCONNECT=On" % dev)
        time.sleep(2)

    # serial_queries() disconnects and reconnects through these.
    def connect(self):
        return self.connect_dev(AUX)

    def disconnect(self):
        self.disconnect_dev(AUX)
        return True

    def props(self, dev):
        out = subprocess.run(["indi_getprop", "-p", str(self.a.indi_port), "-t", "3", "%s.*._STATE" % dev],
                             capture_output=True, text=True).stdout
        return {l.split(".")[1] for l in out.splitlines() if "." in l}

    def run(self):
        print("== mount driver refuses a mount-less controller")
        ok = self.connect_dev(MOUNT)
        self.check("indi_onstepx does not connect", not ok)
        refusal = [m for _, m in self.listener.messages if "No mount detected" in m]
        self.check("refusal explains to use indi_onstepx_aux", bool(refusal), refusal[0] if refusal else "")
        self.disconnect_dev(MOUNT)
        n_msgs = len(self.listener.messages)

        print("== aux driver")
        self.check("indi_onstepx_aux connects", self.connect_dev(AUX))
        time.sleep(2)
        props = self.props(AUX)
        for p in ("OSX_FIRMWARE", "OSX_WEATHER_SET"):
            self.check("property %s defined" % p, p in props)
        pwr = self.props(AUX_PWR)
        for p in ("POWER_CHANNELS", "USB_PORTS", "OSX_ANALOG_AN", "OSX_DEW_HEAT_EN", "OSX_DEW_HEAT_NP",
                  "OSX_INTERVAL_NP"):
            self.check("power device property %s defined" % p, p in pwr)
        self.check("no mount properties", "EQUATORIAL_EOD_COORD" not in props)
        self.check("rotator is its own device", "ABS_ROTATOR_ANGLE" in self.props(AUX_ROT) and
                   "ABS_ROTATOR_ANGLE" not in props)
        ver = self.get("%s.OSX_FIRMWARE.FIRMWARE_VERSION" % AUX)
        self.check("firmware version shown", ver.startswith("10."), ver)
        for n in (1, 2):
            self.check("focuser %d child connected" % n, self.get("%s.CONNECTION.CONNECT" % (AUX_FOC % n)) == "On")

        print("== polling (TimerHit) drives state updates")
        for ang in (40.0, 0.0):
            self.set("%s.ABS_ROTATOR_ANGLE.ANGLE=%g" % (AUX_ROT, ang))
            st = self.wait_state("ABS_ROTATOR_ANGLE", timeout=180, settle=1.5, dev=AUX_ROT)
            time.sleep(6)
            a = self.num("%s.ABS_ROTATOR_ANGLE.ANGLE" % AUX_ROT)
            self.check("rotator to %g deg reported by polling" % ang, st == "Ok" and abs(a - ang) < 0.1,
                       "%s at %.2f" % (st, a))
        # No sensor: a fresh controller reports "nan" until manual values are
        # set, so readings appear only after setting them and reconnecting.
        readable = "WEATHER_PARAMETERS" in props
        self.set("%s.OSX_WEATHER_SET.WEATHER_SET_TEMPERATURE;WEATHER_SET_PRESSURE;WEATHER_SET_HUMIDITY=7;990;55" % AUX)
        self.check("manual weather values accepted", self.wait_state("OSX_WEATHER_SET", timeout=10, dev=AUX) == "Ok",
                   self.state("OSX_WEATHER_SET", AUX))
        if not readable:
            self.disconnect()
            self.connect()
            props = self.props(AUX)
            self.check("weather readings appear once readable", "WEATHER_PARAMETERS" in props)
        self.set("%s.OSX_WEATHER_SET.WEATHER_SET_TEMPERATURE;WEATHER_SET_PRESSURE;WEATHER_SET_HUMIDITY=8;990;55" % AUX)
        time.sleep(25)          # weather is polled every 10 polls
        t = self.num("%s.WEATHER_PARAMETERS.WEATHER_TEMPERATURE" % AUX)
        self.check("weather reading refreshed by polling", abs(t - 8.0) < 0.05, "%.2f" % t)

        n_weather = len(subprocess.run(["indi_getprop", "-p", str(self.a.indi_port), "-t", "3",
                                        "%s.WEATHER_PARAMETERS.*" % AUX], capture_output=True, text=True)
                        .stdout.splitlines())

        print("== focusers")
        for n, target in ((1, 700), (2, 300)):
            dev = AUX_FOC % n
            self.set("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION=%d" % (dev, target))
            st = self.wait_state("ABS_FOCUS_POSITION", timeout=60, dev=dev, settle=1.0)
            p = self.num("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION" % dev)
            self.check("focuser %d absolute move" % n, st == "Ok" and abs(p - target) < 1, "%s at %g" % (st, p))

        print("== aux features and USB")
        self.set("%s.POWER_CHANNELS.POWER_CHANNEL_2=On" % AUX_PWR)    # feature slot 5
        self.set("%s.OSX_ANALOG_AN.VALUE=64" % AUX_PWR)
        self.set("%s.OSX_DEW_HEAT_NP.ZERO;SPAN=1;4" % AUX_PWR)
        self.set("%s.OSX_INTERVAL_NP.EXPOSURE;DELAY;COUNT=3;5;7" % AUX_PWR)
        self.set("%s.USB_PORTS.USB_PORT_2=On" % AUX_PWR)              # USB port 3
        time.sleep(2)
        for p in ("POWER_CHANNELS", "OSX_ANALOG_AN", "OSX_DEW_HEAT_NP", "OSX_INTERVAL_NP", "USB_PORTS"):
            self.check("%s accepted" % p, self.state(p, AUX_PWR) == "Ok", self.state(p, AUX_PWR))

        print("== verify on the controller (driver disconnected)")
        q = self.serial_queries([":GXX5#", ":GXX2#", ":GXX3#", ":GXX4#", ":GUX3#", ":F1G#", ":F2G#", ":rG#", ":GX9A#"])
        for k, v in q.items():
            print("      %-7s %r" % (k, v))
        self.check("aux 5 switch on", q[":GXX5#"].split(";")[0] == "1", q[":GXX5#"])
        self.check("aux 2 analog 64", q[":GXX2#"].split(";")[0] == "64", q[":GXX2#"])
        dew = q[":GXX3#"].split(";")[0].split(",")
        self.check("aux 3 dew zero 1 span 4", len(dew) >= 3 and abs(float(dew[1]) - 1) < 0.05 and abs(float(dew[2]) - 4) < 0.05,
                   q[":GXX3#"])
        ivo = q[":GXX4#"].split(",")
        self.check("aux 4 intervalometer 3 s / 5 s / 7", len(ivo) == 4 and abs(float(ivo[1]) - 3) < 0.01
                   and abs(float(ivo[2]) - 5) < 0.01 and ivo[3] == "7", q[":GXX4#"])
        self.check("USB port 3 on", q[":GUX3#"] == "1", q[":GUX3#"])
        self.check("focuser positions", q[":F1G#"] == "700" and q[":F2G#"] == "300", "%s %s" % (q[":F1G#"], q[":F2G#"]))

        print("== reconnect")
        self.check("focuser 1 reconnected", self.get("%s.CONNECTION.CONNECT" % (AUX_FOC % 1)) == "On")
        n2 = len(subprocess.run(["indi_getprop", "-p", str(self.a.indi_port), "-t", "3",
                                 "%s.WEATHER_PARAMETERS.*" % AUX], capture_output=True, text=True).stdout.splitlines())
        self.check("weather parameters not duplicated", n2 == n_weather, "%d -> %d" % (n_weather, n2))

        probs = [p for p in self.listener.messages[n_msgs:] if "[ERROR]" in p[1] or "[WARNING]" in p[1]]
        for t, m in probs:
            print("      %s %s" % (t, m))
        self.check("no aux-driver errors or warnings", not probs, "%d message(s)" % len(probs))

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
    return 1 if AuxE2E(a).run() else 0


if __name__ == "__main__":
    sys.exit(main())
