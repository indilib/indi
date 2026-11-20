#!/usr/bin/env python3
# OnStepX INDI Driver — onstepx_e2e.py
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
End-to-end test of indi_onstepx against a real OnStepX controller.

Drives the driver through INDI (indi_setprop/indi_getprop) exactly as a
client would, then disconnects and reads the controller directly over serial
to confirm what actually reached the firmware.

Prerequisites:
  * indiserver running with the driver, e.g.
        indiserver -p 7625 build/drivers/telescope/onstepx/indi_onstepx
  * a TEST controller with no mechanics attached: this slews, parks, homes,
    changes site/time/limits and writes the pointing model.
  * Expected controller features (see the README in this directory): GEM
    mount with PEC, rotator, focusers 1 and 2, aux features
    1 Switch / 2 Analog / 3 Dew heater / 4 Intervalometer / 5 Switch,
    USB plugin ports 1 and 3.  Missing features are reported as SKIP.

Usage:
  onstepx_e2e.py --port /dev/ttyUSB0 --test-controller [--baud 9600] [--indi-port 7625]
  onstepx_e2e.py --port 192.168.7.49:9998 --test-controller   # network command channel
"""

import argparse
import datetime
import html
import re
import socket
import subprocess
import sys
import threading
import time

sys.dont_write_bytecode = True      # importing onstepx_safety leaves no __pycache__
import onstepx_safety  # noqa: E402

DEV = "OnStepX"
FOC = DEV + " Focuser %d"
ROT = DEV + " Rotator"      # the rotator is a child device
PWR = DEV + " Power"        # aux outputs and USB ports; switch slots 1 and 5 are DC channels 1 and 2
BAUDS = ("9600", "19200", "38400", "57600", "115200", "230400")


def network_port(port):
    """(host, port) for a network command channel given as host:port, else None."""
    if not port.startswith("/") and ":" in port:
        host, _, tcp = port.rpartition(":")
        return host, int(tcp)
    return None


class _Socket:
    """The non-blocking read()/write()/close() of a pyserial port, over TCP."""

    def __init__(self, host, port):
        self.sock = socket.create_connection((host, port), timeout=10)
        self.sock.setblocking(False)

    def read(self, n):
        try:
            return self.sock.recv(n)
        except BlockingIOError:
            return b""

    def write(self, data):
        self.sock.setblocking(True)
        self.sock.sendall(data)
        self.sock.setblocking(False)

    def close(self):
        self.sock.close()


def open_controller(port, baud):
    """A direct link to the controller: a serial device, or host:port."""
    net = network_port(port)
    if net:
        return _Socket(*net)
    import serial
    return serial.Serial(port, int(baud), timeout=0)


class MessageListener:
    """Collects driver log messages (as an INDI client sees them)."""

    def __init__(self, port):
        self.messages = []
        self.sock = socket.create_connection(("localhost", port))
        self.sock.sendall(b"<getProperties version='1.7'/>")
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        buf = b""
        pat = re.compile(rb'timestamp="([^"]*)"[^>]*message="([^"]*)"')
        while True:
            try:
                data = self.sock.recv(65536)
            except OSError:
                return
            if not data:
                return
            buf += data
            last = 0
            for m in pat.finditer(buf):
                self.messages.append((m.group(1).decode(), html.unescape(m.group(2).decode(errors="replace"))))
                last = m.end()
            buf = buf[last:] if last else buf[-8000:]

    def problems(self):
        return [(t, m) for t, m in self.messages if "[ERROR]" in m or "[WARNING]" in m]


class E2E:
    def __init__(self, a):
        self.a = a
        self.results = []
        self.listener = MessageListener(a.indi_port)

    # ------------------------------------------------------------------ INDI
    def get(self, spec, timeout=3):
        r = subprocess.run(["indi_getprop", "-p", str(self.a.indi_port), "-t", str(timeout), "-1", spec],
                           capture_output=True, text=True)
        return r.stdout.strip()

    def set(self, spec):
        subprocess.run(["indi_setprop", "-p", str(self.a.indi_port), spec], capture_output=True, text=True)

    def state(self, prop, dev=DEV):
        return self.get("%s.%s._STATE" % (dev, prop))

    def wait_state(self, prop, want=("Ok",), timeout=30.0, dev=DEV, settle=0.3):
        time.sleep(settle)
        end = time.time() + timeout
        st = ""
        while time.time() < end:
            st = self.state(prop, dev)
            if st in want or st == "Alert":
                return st
            time.sleep(0.5)
        return st

    def num(self, spec):
        try:
            return float(self.get(spec))
        except ValueError:
            return float("nan")

    # ---------------------------------------------------------------- results
    def check(self, name, ok, detail=""):
        status = "PASS" if ok else "FAIL"
        self.results.append((status, name, detail))
        print("%-5s %-46s %s" % (status, name, detail))
        return ok

    def skip(self, name, why):
        self.results.append(("SKIP", name, why))
        print("%-5s %-46s %s" % ("SKIP", name, why))

    # -------------------------------------------------------------- lifecycle
    def connect(self):
        net = network_port(self.a.port)
        if net:
            self.set("%s.CONNECTION_MODE.CONNECTION_TCP=On" % DEV)
            self.set("%s.CONNECTION_TYPE.TCP=On" % DEV)
            self.set("%s.DEVICE_ADDRESS.ADDRESS;PORT=%s;%d" % (DEV, net[0], net[1]))
        else:
            self.set("%s.CONNECTION_MODE.CONNECTION_SERIAL=On" % DEV)
            self.set("%s.DEVICE_PORT.PORT=%s" % (DEV, self.a.port))
            self.set("%s.DEVICE_BAUD_RATE.%s=On" % (DEV, self.a.baud))
        self.set("%s.CONNECTION.CONNECT=On" % DEV)
        st = self.wait_state("CONNECTION", timeout=40)
        time.sleep(3)   # let updateProperties and focuser children settle
        return st == "Ok" and self.get("%s.CONNECTION.CONNECT" % DEV) == "On"

    def disconnect(self):
        self.set("%s.CONNECTION.DISCONNECT=On" % DEV)
        time.sleep(2)
        return self.get("%s.CONNECTION.DISCONNECT" % DEV) == "On"

    def serial_queries(self, cmds):
        """Disconnect the driver, query the controller directly, reconnect."""
        self.disconnect()
        out = {}
        s = open_controller(self.a.port, self.a.baud)
        time.sleep(3.0)
        s.write(b":GVP#")               # warm-up: the first reply after a reset can lose a byte
        time.sleep(0.5)
        s.read(65536)
        for c in cmds:
            s.write(c.encode())
            t, d = time.time(), b""
            while time.time() - t < 1.0:
                d += s.read(256)
                if d.endswith(b"#") or (len(d) == 1 and d in (b"0", b"1") and time.time() - t > 0.3):
                    break
                time.sleep(0.01)
            out[c] = d.replace(b"\xff", b"").rstrip(b"#").decode("latin-1")
        s.close()
        if not self.connect():
            raise RuntimeError("reconnect failed")
        return out

    # ------------------------------------------------------------------ tests
    def run(self):
        print("== connect")
        self.check("connect", self.connect())
        mount_type = self.get("%s.TELESCOPE_MOUNT_TYPE.EQ_GEM" % DEV)
        self.check("mount type reported as GEM", mount_type == "On", mount_type)
        self.check("focuser 1 child device connected",
                   self.get("%s.CONNECTION.CONNECT" % (FOC % 1)) == "On")
        self.check("focuser 2 child device connected",
                   self.get("%s.CONNECTION.CONNECT" % (FOC % 2)) == "On")
        wp = subprocess.run(["indi_getprop", "-p", str(self.a.indi_port), "-t", "3", "%s.WEATHER_PARAMETERS.*" % DEV],
                            capture_output=True, text=True).stdout.splitlines()
        n_weather = len(wp)

        print("== time and location")
        now = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0)
        self.set("%s.TIME_UTC.UTC;OFFSET=%s;1" % (DEV, now.strftime("%Y-%m-%dT%H:%M:%S")))
        self.wait_state("TIME_UTC", timeout=10)
        self.set("%s.GEOGRAPHIC_COORD.LAT;LONG;ELEV=51.5;359.9;50" % DEV)
        self.wait_state("GEOGRAPHIC_COORD", timeout=10)

        print("== mount state")
        self.set("%s.TELESCOPE_PARK.UNPARK=On" % DEV)
        self.wait_state("TELESCOPE_PARK", timeout=15)
        self.set("%s.TELESCOPE_TRACK_STATE.TRACK_ON=On" % DEV)
        time.sleep(3)
        # INDI convention: TELESCOPE_TRACK_STATE is Busy while tracking.
        self.check("tracking on", self.state("TELESCOPE_TRACK_STATE") == "Busy"
                   and self.get("%s.TELESCOPE_TRACK_STATE.TRACK_ON" % DEV) == "On",
                   self.state("TELESCOPE_TRACK_STATE"))

        print("== max slew rate")
        self.set("%s.OSX_SLEW_RATE_MAX.SLEW_RATE_MAX=2" % DEV)
        st = self.wait_state("OSX_SLEW_RATE_MAX", timeout=10)
        rate = self.num("%s.OSX_SLEW_RATE_MAX.SLEW_RATE_MAX" % DEV)
        self.check("max slew rate applied", st == "Ok" and rate > 1.0, "state %s, now %.2f deg/s" % (st, rate))

        print("== sync in all coordinate quadrants (format regression)")
        lst = self.lst()
        # East of the meridian (negative hour angle) so meridian limits do
        # not stop tracking; signs and magnitudes cover the old format bug.
        for ha, dec in ((-1.0, 45.5), (-2.0, 5.5), (-1.5, -5.5), (-2.5, 0.25)):
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
            self.check("sync RA %6.3fh Dec %+6.2f" % (ra, dec), st == "Ok" and dra < 0.1 and abs(d - dec) < 0.1,
                       "state %s, reads %.4fh %+.4f" % (st, r, d))

        print("== goto")
        ra, dec = (lst + 1.5) % 24.0, 30.0
        self.set("%s.ON_COORD_SET.TRACK=On" % DEV)
        self.set("%s.EQUATORIAL_EOD_COORD.RA;DEC=%.6f;%.6f" % (DEV, ra, dec))
        st = self.wait_state("EQUATORIAL_EOD_COORD", timeout=180, settle=2.0)
        r = self.num("%s.EQUATORIAL_EOD_COORD.RA" % DEV)
        d = self.num("%s.EQUATORIAL_EOD_COORD.DEC" % DEV)
        dra = abs(((r - ra + 12) % 24) - 12) * 15
        self.check("goto completes on target", st == "Ok" and dra < 0.05 and abs(d - dec) < 0.05,
                   "state %s, reads %.4fh %+.4f" % (st, r, d))
        pier = [e for e in ("PIER_EAST", "PIER_WEST") if self.get("%s.TELESCOPE_PIER_SIDE.%s" % (DEV, e)) == "On"]
        self.check("pier side reported", len(pier) == 1, ",".join(pier) or "none")

        print("== abort")
        self.set("%s.EQUATORIAL_EOD_COORD.RA;DEC=%.6f;%.6f" % (DEV, (lst - 2) % 24, 60.0))
        time.sleep(2)
        self.set("%s.TELESCOPE_ABORT_MOTION.ABORT=On" % DEV)
        # The firmware decelerates after :Q# and reports the goto active until
        # the mount stops, which takes longer at higher slew rates.
        t0 = time.time()
        while time.time() - t0 < 10 and self.state("EQUATORIAL_EOD_COORD") == "Busy":
            time.sleep(0.5)
        stopped = time.time() - t0
        self.check("abort stops slew", self.state("EQUATORIAL_EOD_COORD") != "Busy",
                   "state %s after %.1f s" % (self.state("EQUATORIAL_EOD_COORD"), stopped))

        print("== tracking modes (regression: Lunar/Solar were swapped)")
        for mode, hz in (("TRACK_LUNAR", 57.9), ("TRACK_SOLAR", 60.0), ("TRACK_KING", 60.136), ("TRACK_SIDEREAL", 60.164)):
            self.set("%s.TELESCOPE_TRACK_MODE.%s=On" % (DEV, mode))
            time.sleep(14)              # OSX_TRACK_FREQ refreshes every 10 polls
            f = self.num("%s.OSX_TRACK_FREQ.TRACK_FREQ" % DEV)
            self.check("%s -> %.3f Hz" % (mode, hz), abs(f - hz) < 0.01, "reads %.4f Hz" % f)
        self.set("%s.TELESCOPE_TRACK_MODE.TRACK_CUSTOM=On" % DEV)
        time.sleep(1)
        self.set("%s.TELESCOPE_TRACK_RATE.TRACK_RATE_RA;TRACK_RATE_DE=15.0;1.0" % DEV)
        self.check("custom track rate accepted", self.wait_state("TELESCOPE_TRACK_RATE", timeout=10) == "Ok",
                   self.state("TELESCOPE_TRACK_RATE"))
        q = self.serial_queries([":GXTR#", ":GXTD#"])
        self.check_offsets(q, "custom rate offsets reach firmware")
        # serial_queries() reconnected the driver: it must show the firmware's
        # offsets as Custom, not Sidereal (regression: hidden Dec drift).
        self.check("track mode mirrors firmware offsets on connect",
                   self.get("%s.TELESCOPE_TRACK_MODE.TRACK_CUSTOM" % DEV) == "On" and
                   abs(self.num("%s.TELESCOPE_TRACK_RATE.TRACK_RATE_DE" % DEV) - 1.0) < 0.01,
                   "mode %s, Dec rate %s" % (self.get("%s.TELESCOPE_TRACK_MODE.TRACK_CUSTOM" % DEV),
                                             self.get("%s.TELESCOPE_TRACK_RATE.TRACK_RATE_DE" % DEV)))

        print("== guiding")
        self.set("%s.GUIDE_RATE.GUIDE_RATE_WE=0.25" % DEV)
        self.wait_state("GUIDE_RATE", timeout=5)
        g = self.num("%s.GUIDE_RATE.GUIDE_RATE_WE" % DEV)
        self.check("guide rate snapped to firmware rate", abs(g - 0.25) < 1e-6, "%.2f" % g)
        t0 = time.time()
        self.set("%s.TELESCOPE_TIMED_GUIDE_NS.TIMED_GUIDE_N=500" % DEV)
        # GuiderInterface::GuideComplete() returns the property to Idle.
        st = self.wait_state("TELESCOPE_TIMED_GUIDE_NS", want=("Idle", "Ok"), timeout=5, settle=0.05)
        dt = time.time() - t0
        self.check("500 ms guide pulse completes promptly", st in ("Idle", "Ok") and dt < 1.2,
                   "%s after %.2f s" % (st, dt))

        print("== manual motion")
        self.set("%s.TELESCOPE_MOTION_NS.MOTION_NORTH=On" % DEV)
        time.sleep(1.5)
        self.set("%s.TELESCOPE_MOTION_NS.MOTION_NORTH=Off" % DEV)
        time.sleep(1.5)
        self.check("manual move N start/stop", self.state("TELESCOPE_MOTION_NS") in ("Idle", "Ok"),
                   self.state("TELESCOPE_MOTION_NS"))

        print("== focusers")
        for n, target in ((1, 1000), (2, 500)):
            dev = FOC % n
            if self.get("%s.CONNECTION.CONNECT" % dev) != "On":
                self.skip("focuser %d" % n, "not present")
                continue
            self.set("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION=%d" % (dev, target))
            st = self.wait_state("ABS_FOCUS_POSITION", timeout=60, dev=dev, settle=1.0)
            p = self.num("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION" % dev)
            self.check("focuser %d absolute move" % n, st == "Ok" and abs(p - target) < 1, "%s at %g" % (st, p))
            self.set("%s.FOCUS_MOTION.FOCUS_INWARD=On" % dev)
            self.set("%s.REL_FOCUS_POSITION.FOCUS_RELATIVE_POSITION=100" % dev)
            st = self.wait_state("REL_FOCUS_POSITION", timeout=60, dev=dev, settle=1.0)
            time.sleep(1.5)
            p = self.num("%s.ABS_FOCUS_POSITION.FOCUS_ABSOLUTE_POSITION" % dev)
            self.check("focuser %d inward 100 decreases position" % n, st == "Ok" and abs(p - (target - 100)) < 1,
                       "%s at %g" % (st, p))

        print("== rotator")
        for ang in (30.0, 270.0):
            self.set("%s.ABS_ROTATOR_ANGLE.ANGLE=%g" % (ROT, ang))
            st = self.wait_state("ABS_ROTATOR_ANGLE", timeout=360, settle=1.5, dev=ROT)
            time.sleep(6)               # rotator state refreshes every 5 polls
            a = self.num("%s.ABS_ROTATOR_ANGLE.ANGLE" % ROT)
            self.check("rotator to %g deg" % ang, st == "Ok" and abs(a - ang) < 0.1, "%s at %.2f" % (st, a))

        print("== aux features, USB ports, weather")
        self.set("%s.POWER_CHANNELS.POWER_CHANNEL_1=On" % PWR)
        self.set("%s.OSX_ANALOG_AN.VALUE=128" % PWR)
        self.set("%s.OSX_DEW_HEAT_EN.ENABLE=On" % PWR)
        self.set("%s.OSX_DEW_HEAT_NP.ZERO;SPAN=2;6" % PWR)
        self.set("%s.OSX_INTERVAL_NP.EXPOSURE;DELAY;COUNT=1.5;2;5" % PWR)
        self.set("%s.USB_PORTS.USB_PORT_1=On" % PWR)
        self.set("%s.OSX_WEATHER_SET.WEATHER_SET_TEMPERATURE;WEATHER_SET_PRESSURE;WEATHER_SET_HUMIDITY=12;1000;60" % DEV)
        time.sleep(2)
        for p in ("POWER_CHANNELS", "OSX_ANALOG_AN", "OSX_DEW_HEAT_EN", "OSX_DEW_HEAT_NP", "OSX_INTERVAL_NP",
                  "USB_PORTS"):
            self.check("%s accepted" % p, self.state(p, dev=PWR) == "Ok", self.state(p, dev=PWR))
        self.check("OSX_WEATHER_SET accepted", self.state("OSX_WEATHER_SET") == "Ok", self.state("OSX_WEATHER_SET"))

        print("== park / home")
        self.set("%s.TELESCOPE_PARK_OPTION.PARK_CURRENT=On" % DEV)
        time.sleep(1)
        self.set("%s.TELESCOPE_PARK.PARK=On" % DEV)
        st = self.wait_state("TELESCOPE_PARK", timeout=120, settle=1.0)
        self.check("park", st == "Ok" and self.get("%s.TELESCOPE_PARK.PARK" % DEV) == "On", st)
        self.set("%s.TELESCOPE_PARK.UNPARK=On" % DEV)
        st = self.wait_state("TELESCOPE_PARK", timeout=30, settle=1.0)
        self.check("unpark", st == "Ok" and self.get("%s.TELESCOPE_PARK.UNPARK" % DEV) == "On", st)
        self.set("%s.TELESCOPE_HOME.FIND=On" % DEV)
        st = self.wait_state("TELESCOPE_HOME", timeout=180, settle=2.0)
        self.check("home find completes (switch leaves Busy)", st == "Ok", st)
        time.sleep(2)
        self.set("%s.TELESCOPE_TRACK_STATE.TRACK_ON=On" % DEV)
        self.wait_state("TELESCOPE_TRACK_STATE", timeout=10)

        print("== site profiles")
        self.set("%s.OSX_SITE_SELECT.SITE_2=On" % DEV)
        st = self.wait_state("OSX_SITE_SELECT", timeout=10)
        lat2 = self.num("%s.GEOGRAPHIC_COORD.LAT" % DEV)
        self.check("select site 2", st == "Ok", "%s, GEOGRAPHIC_COORD.LAT now %.3f" % (st, lat2))
        self.set("%s.OSX_SITE_SELECT.SITE_1=On" % DEV)
        self.wait_state("OSX_SITE_SELECT", timeout=10)
        lat1 = self.num("%s.GEOGRAPHIC_COORD.LAT" % DEV)
        self.check("site 1 restores GEOGRAPHIC_COORD", abs(lat1 - 51.5) < 0.01, "LAT %.3f" % lat1)

        print("== verify on the controller (driver disconnected)")
        q = self.serial_queries([":GG#", ":GL#", ":GtH#", ":GgH#", ":Gv#", ":GT#", ":GXTR#", ":GXTD#", ":GX90#",
                                 ":GXX1#", ":GXX2#", ":GXX3#", ":GXX4#", ":GUX1#", ":GX9A#", ":W?#",
                                 ":F1G#", ":F2G#", ":rG#"])
        for k, v in q.items():
            print("      %-7s %r" % (k, v))
        self.check("timezone sent as -01:00 for UTC+1", q[":GG#"] == "-01:00", q[":GG#"])
        loc = (datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(hours=1))
        try:
            hh, mm, ss = [int(float(x)) for x in q[":GL#"].split(":")[:3]]
            diff = abs((hh * 3600 + mm * 60 + ss) - (loc.hour * 3600 + loc.minute * 60 + loc.second))
            diff = min(diff, 86400 - diff)
        except ValueError:
            diff = 9999
        self.check("controller local time = UTC+1", diff < 120, "%s (diff %d s)" % (q[":GL#"], diff))
        self.check("latitude 51.5", abs(self.sexa(q[":GtH#"]) - 51.5) < 0.01, q[":GtH#"])
        self.check("longitude 0.1 W", abs(self.sexa(q[":GgH#"]) - 0.1) < 0.01, q[":GgH#"])
        self.check("elevation 50", q[":Gv#"].startswith("50"), q[":Gv#"])
        self.check_offsets(q, "custom rate re-applied after home/unpark")
        # Leave the firmware at the sidereal rate: the offsets stay in its RAM
        # until it reboots, and would make later sessions drift.
        self.set("%s.TELESCOPE_TRACK_MODE.TRACK_SIDEREAL=On" % DEV)
        time.sleep(1)
        self.check("guide rate 0.25x", q[":GX90#"] == "0.25", q[":GX90#"])
        self.check("aux 1 switch on", q[":GXX1#"].split(";")[0] == "1", q[":GXX1#"])
        self.check("aux 2 analog 128", q[":GXX2#"].split(";")[0] == "128", q[":GXX2#"])
        dew = q[":GXX3#"].split(";")[0].split(",")
        self.check("aux 3 dew enabled, zero 2, span 6",
                   len(dew) >= 3 and dew[0] == "1" and abs(float(dew[1]) - 2) < 0.05 and abs(float(dew[2]) - 6) < 0.05,
                   q[":GXX3#"])
        ivo = q[":GXX4#"].split(",")
        self.check("aux 4 intervalometer exp 1.5, delay 2, count 5",
                   len(ivo) == 4 and abs(float(ivo[1]) - 1.5) < 0.01 and abs(float(ivo[2]) - 2) < 0.01 and ivo[3] == "5",
                   q[":GXX4#"])
        self.check("USB port 1 on", q[":GUX1#"] == "1", q[":GUX1#"])
        self.check("manual temperature 12", q[":GX9A#"].strip() == "12.0", q[":GX9A#"])
        self.check("site 1 active (firmware 0)", q[":W?#"] == "0", q[":W?#"])

        print("== reconnect")
        self.check("focuser 1 reconnected", self.get("%s.CONNECTION.CONNECT" % (FOC % 1)) == "On")
        # Readings may appear only now (a fresh no-sensor controller reports
        # "nan" until manual values are set), but must never be duplicated.
        wp2 = subprocess.run(["indi_getprop", "-p", str(self.a.indi_port), "-t", "3", "%s.WEATHER_PARAMETERS.*" % DEV],
                             capture_output=True, text=True).stdout.splitlines()
        names = [l.split("=")[0] for l in wp2]
        self.check("weather parameters not duplicated", len(names) == len(set(names)) and len(names) >= n_weather,
                   "%d -> %d elements" % (n_weather, len(names)))

        print("== driver errors and warnings during the run")
        probs = self.listener.problems()
        for t, m in probs:
            print("      %s %s" % (t, m))
        self.check("no driver errors or warnings", not probs, "%d message(s)" % len(probs))

        print("\n" + "  ".join("%s: %d" % (k, sum(1 for r in self.results if r[0] == k))
                               for k in ("PASS", "FAIL", "SKIP")))
        return sum(1 for r in self.results if r[0] == "FAIL")

    # -------------------------------------------------------------- helpers
    def check_offsets(self, q, name):
        # RA 15.0"/s requested: offset = (15.041067 - 15.0) * 0.99726957 arcsec/sidereal s
        exp_ra = (15.041067 - 15.0) * 0.99726957
        try:
            ra, de = float(q[":GXTR#"]), float(q[":GXTD#"])
        except ValueError:
            ra = de = float("nan")
        self.check(name, abs(ra - exp_ra) < 1e-3 and abs(de - 0.99726957) < 1e-3,
                   "RA %s (expect %.5f), Dec %s (expect 0.99727)" % (q[":GXTR#"], exp_ra, q[":GXTD#"]))

    def lst(self):
        """Local sidereal time from the controller via the driver's coordinates."""
        # Read :GS# directly would need the port; use the driver's own clock:
        # LST = RA of the meridian; derive from UTC and the longitude we set.
        now = datetime.datetime.now(datetime.timezone.utc)
        jd = now.timestamp() / 86400.0 + 2440587.5
        d = jd - 2451545.0
        gmst = (18.697374558 + 24.06570982441908 * d) % 24.0
        lon_east = 359.9
        return (gmst + lon_east / 15.0) % 24.0

    @staticmethod
    def sexa(s):
        neg = s.strip().startswith("-")
        parts = [float(p) for p in re.split(r"[^0-9.]+", s.strip().lstrip("+-")) if p]
        v = sum(p / (60 ** i) for i, p in enumerate(parts[:3]))
        return -v if neg else v


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True, help="serial device, or host:port for a network command channel")
    ap.add_argument("--baud", default="9600", choices=BAUDS)
    ap.add_argument("--indi-port", type=int, default=7624)
    onstepx_safety.add_flag(ap)
    a = ap.parse_args()
    onstepx_safety.require(a)
    return 1 if E2E(a).run() else 0


if __name__ == "__main__":
    sys.exit(main())
