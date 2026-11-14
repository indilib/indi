#!/usr/bin/env python3
# OnStepX INDI Driver — onstepx_conformance.py
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
OnStepX protocol conformance check for the indi_onstepx driver.

Sends every command the driver uses to a real OnStepX controller and checks
that the reply TYPE on the wire matches the OnStepXComm call the driver uses
for it.  Unit tests cannot catch a misclassified command because their mocks
encode the driver's own assumptions; this checks against real firmware.

Reply types (OnStepX src/libApp/commands/ProcessCmds.cpp):
  TEXT  "value#"                 -> OnStepXComm::sendText
  BOOL  bare '1' / '0', no '#'   -> OnStepXComm::sendBool
  NONE  nothing at all           -> OnStepXComm::sendNoReply (sendUrgent for :Q#)
  CHAR  one unframed character   -> OnStepXComm::sendChar
  BIN   9 bytes >= 0x80 + '#'    -> OnStepXComm::sendReadN (:Gu#)

Commands that change settings write back the value read beforehand.  Motion
commands are only safe on a controller with no mechanics attached.

Usage:
  onstepx_conformance.py --port /dev/ttyUSB0 --test-controller [--baud 9600]
  onstepx_conformance.py --check-source      # every driver command classified?

Exit status is non-zero if any check fails.
"""

import argparse
import os
import re
import sys
import time

sys.dont_write_bytecode = True      # importing the sibling scripts leaves no __pycache__
import onstepx_safety  # noqa: E402
import onstepx_wire  # noqa: E402

TEXT, BOOL, NONE, CHAR, BIN = "TEXT", "BOOL", "NONE", "CHAR", "BIN"

HERE = os.path.dirname(os.path.abspath(__file__))
DRIVER_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "..", "drivers", "telescope", "onstepx"))

# ---------------------------------------------------------------------------
# Driver command literals covered by the checks below (exact source text).
# --check-source fails if the driver sends a literal that is not listed here.
# ---------------------------------------------------------------------------
COVERED = {
    # identity / probe
    ":GVP#", ":GVN#", ":GVD#", ":GVT#", ":GVC#", ":GX98#", ":F%da#", ":GXY0#", ":GUY0#",
    ":GX9A#", ":GX9B#", ":GX9C#", ":GX9E#", ":GX9F#", ":GXU%d#", ":Gv#", ":GW#", ":Gu#", ":GU#",
    ":h?#", ":Gm#", ":GX94#",
    # position / goto / sync
    ":GRH#", ":GDH#", ":GSH#", ":Sr%s#", ":Sd%s#", ":MS#", ":MN#", ":CM#", ":Q#",
    # park / home
    ":hP#", ":hR#", ":hQ#", ":hC#", ":hF#", ":hA0#", ":hA1#", ":hC1,%d#", ":hC2,%d#",
    # tracking
    ":Te#", ":Td#", ":TQ#", ":TL#", ":TS#", ":TK#", ":T+#", ":T-#", ":TR#",
    ":To#", ":Tr#", ":Tn#", ":T1#", ":T2#", ":SXTR,%.5f#", ":SXTD,%.5f#", ":GXTR#", ":GXTD#", ":GT#",
    ":GX95#", ":SX95,0#", ":SX95,1#", ":GX96#", ":SX96,W#", ":SX96,E#", ":SX96,B#", ":SX96,A#",
    ":GX97#", ":GX92#", ":SX92,%.3f#", ":SX99,1#", ":SX98,1#", ":SX98,0#",
    # motion / guiding
    ":R3#", ":RC#", ":RM#", ":RS#", ":R%d#", ":Mn#", ":Ms#", ":Me#", ":Mw#",
    ":Qn#", ":Qs#", ":Qe#", ":Qw#", ":Mg%c%u#", ":GX90#",
    # site / time
    ":GtH#", ":GgH#", ":St%c%02d:%02d:%.2f#", ":Sg%.03d:%02d:%.02f#", ":Sv%d#",
    ":SG%c%02d#", ":SG%c%02d:%02d#", ":SL%02d:%02d:%02d#", ":SC%02d/%02d/%02d#",
    ":GM#", ":GN#", ":GO#", ":GP#", ":W?#", ":W%d#",
    # limits / backlash
    ":Gh#", ":Go#", ":Sh%d#", ":So%d#", ":GXE9#", ":GXEA#", ":SXE9,%d#", ":SXEA,%d#",
    ":%BR#", ":%BD#", ":$BR%d#", ":$BD%d#",
    # weather / DUT1
    ":SX9A,%.2f#", ":SX9B,%.2f#", ":SX9C,%.2f#", ":SU%+.3f#",
    # alignment / model
    ":A?#", ":A%d#", ":A+#", ":AW#", ":MP#", ":GX02#", ":GX03#", ":GX0%c#", ":SX0%c,%lld#", ":SX09,2#",
    # PEC
    ":$QZ?#", ":$QZ+#", ":$QZ-#", ":$QZ/#", ":$QZZ#", ":$QZ!#", ":VW#",
    # reticle
    ":B+#", ":B-#",
    # rotator
    ":rG#", ":rT#", ":rI#", ":rM#", ":rb#", ":rb%d#", ":rS%c%03d:%02d:%02d#", ":rQ#", ":rC#",
    ":r+#", ":r-#", ":rP#", ":rR#", ":rZ#", ":rF#",
    # focuser (bodies are sent as :F<slot><body>#)
    ":F%d%s#",
    # aux features / USB plugin
    ":GXX%d#", ":GXY%d#", ":SXX%d,%c%.3f#", ":GUX%d#", ":GUY%d#", ":SUX%d,%c%d#",
}

# Site-name setters are built as "%s%s#" from {":SM", ":SN", ":SO", ":SP"}.
COVERED_PREFIXES = {":SM", ":SN", ":SO", ":SP"}


def check_source():
    """Every command literal in driver code (not comments) must be in COVERED."""
    literal = re.compile(r'"([:$][^"]*#)"')
    missing = {}
    for name in sorted(os.listdir(DRIVER_DIR)):
        if not name.endswith(".cpp"):
            continue
        in_block = False
        with open(os.path.join(DRIVER_DIR, name)) as f:
            for lineno, line in enumerate(f, 1):
                code = line
                if in_block:
                    if "*/" not in code:
                        continue
                    code = code.split("*/", 1)[1]
                    in_block = False
                if "/*" in code and "*/" not in code.split("/*", 1)[1]:
                    code = code.split("/*", 1)[0]
                    in_block = True
                code = code.split("//", 1)[0]
                for lit in literal.findall(code):
                    if lit not in COVERED:
                        missing.setdefault(lit, []).append(f"{name}:{lineno}")
    if missing:
        print("Driver commands with no conformance check:")
        for lit, where in sorted(missing.items()):
            print(f"  {lit:28} {', '.join(where)}")
        return 1
    print(f"All driver command literals are covered ({len(COVERED)} classified).")
    return check_version()


def check_version():
    """drivers.xml must list both binaries at the version in OnStepXVersion.h."""
    with open(os.path.join(DRIVER_DIR, "OnStepXVersion.h")) as f:
        header = f.read()
    major = re.search(r"ONSTEPX_VERSION_MAJOR\s*=\s*(\d+)", header).group(1)
    minor = re.search(r"ONSTEPX_VERSION_MINOR\s*=\s*(\d+)", header).group(1)
    want = f"{major}.{minor}"
    with open(os.path.join(DRIVER_DIR, "..", "..", "..", "drivers.xml")) as f:
        xml = f.read()
    bad = []
    for binary in ("indi_onstepx", "indi_onstepx_aux"):
        m = re.search(r">%s</driver>\s*<version>([^<]*)</version>" % binary, xml)
        if not m or m.group(1) != want:
            bad.append(f"{binary}: {m.group(1) if m else 'not found'}")
    if bad:
        print(f"drivers.xml does not match OnStepXVersion.h ({want}): " + "; ".join(bad))
        return 1
    print(f"drivers.xml matches the driver version ({want}).")
    return 0


# ---------------------------------------------------------------------------
# Wire access
# ---------------------------------------------------------------------------
class _Tcp:
    """A non-blocking socket with the read()/write() calls Link uses from pyserial."""

    def __init__(self, host, port):
        import socket
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


class Link:
    def __init__(self, port, baud, log=None):
        self.noise = []                 # (time, command, count) of 0xFF bursts
        self.history = []               # recent (time, command, reply)
        self.log = open(log, "w") if log else None
        self.t0 = time.monotonic()
        if not port.startswith("/") and ":" in port:
            # host:port, e.g. a controller's WiFi command channel (9996-9999)
            host, tcp = port.rsplit(":", 1)
            self.s = _Tcp(host, int(tcp))
            time.sleep(0.5)
        else:
            import serial
            self.s = serial.Serial(port, baud, timeout=0)
            time.sleep(2.5)             # opening the port may reset an ESP32
        self.drain()

    def _log(self, direction, data):
        if self.log:
            self.log.write("%9.3f %s %r\n" % (time.monotonic() - self.t0, direction, data))
            self.log.flush()

    def drain(self):
        time.sleep(0.05)
        stale = self.s.read(65536)
        if stale:
            self._log("stale", stale)

    def raw(self, cmd, first_wait=1.5, quiet=0.3):
        """Send cmd, return all bytes until `quiet` s pass with no new data."""
        self.drain()
        self._log(">", cmd)
        self.s.write(cmd.encode("latin-1"))
        data = b""
        start = time.monotonic()
        last = None
        while True:
            chunk = self.s.read(256)
            now = time.monotonic()
            if chunk:
                data += chunk
                last = now
            if (last is None and now - start > first_wait) or (last is not None and now - last > quiet):
                self._log("<", data)
                return self._denoise(cmd, data)
            time.sleep(0.005)

    def _denoise(self, cmd, data):
        # Bursts of 0xFF (lone start bits on TX) seen from some builds: record
        # them as a separate failure and classify the reply without them.
        n = data.count(b"\xff")
        t = time.monotonic() - self.t0
        if n:
            self.noise.append((t, cmd, n, list(self.history[-4:])))
            data = data.replace(b"\xff", b"")
        self.history.append((round(t, 2), cmd, data[:16]))
        return data

    def text(self, cmd):
        """Convenience: payload of a '#'-terminated reply, or None."""
        r = self.raw(cmd)
        return r[:-1].decode("latin-1") if r.endswith(b"#") else None


def classify(reply):
    if reply == b"":
        return NONE
    if len(reply) == 10 and reply.endswith(b"#") and all(b >= 0x80 for b in reply[:9]):
        return BIN
    if reply.endswith(b"#") and b"#" not in reply[:-1]:
        return TEXT
    if len(reply) == 1:
        return BOOL if reply in (b"0", b"1") else CHAR
    return "GARBAGE"


# ---------------------------------------------------------------------------
# Results
# ---------------------------------------------------------------------------
class Results:
    def __init__(self):
        self.rows = []

    def add(self, status, cmd, expected, got, reply, note=""):
        self.rows.append((status, cmd, expected, got, reply, note))
        shown = reply if len(reply) <= 24 else reply[:21] + b"..."
        print(f"{status:5} {cmd:22} expect {expected:5} got {got:7} {shown!r:30} {note}")

    def noise_report(self, link):
        for t, cmd, n, prev in link.noise:
            self.add("FAIL", cmd, "-", "NOISE", b"", "%d x 0xFF at %.1f s; before: %s" % (
                n, t, " ".join(c for _, c, _ in prev)))

    def summary(self):
        counts = {}
        for r in self.rows:
            counts[r[0]] = counts.get(r[0], 0) + 1
        print("\n" + "  ".join(f"{k}: {v}" for k, v in sorted(counts.items())))
        return counts.get("FAIL", 0)


def check(link, res, cmd, expected, absent_ok=False, must_succeed=False, note=""):
    """Send cmd and compare the reply type.

    absent_ok    : a bare '0' is acceptable (feature not compiled in / not present)
    must_succeed : for BOOL, require '1'; for CHAR (:MS#), require '0'
    Returns the raw reply.
    """
    wait = 0.6 if expected == NONE else 1.5
    reply = link.raw(cmd, first_wait=wait)
    got = classify(reply)
    if expected == CHAR and got == BOOL:
        got = CHAR          # a single '0'/'1' is also a valid one-char reply

    if got == expected:
        status = "PASS"
        if must_succeed and expected == BOOL and reply != b"1":
            status, note = "FAIL", (note + " (expected success '1')").strip()
        if must_succeed and expected == CHAR and reply != b"0":
            status, note = "FAIL", (note + " (expected '0' = goto accepted)").strip()
    elif absent_ok and reply == b"0":
        status, note = "ABSENT", note
    else:
        status = "FAIL"
    table = onstepx_wire.reply_type(cmd)
    if table != expected:
        status, note = "FAIL", (note + " (onstepx_wire.py says %s)" % table).strip()
    res.add(status, cmd, expected, got, reply, note)
    return reply


def skip(res, cmd, expected, why):
    res.add("SKIP", cmd, expected, "-", b"", why)


# ---------------------------------------------------------------------------
# Formatting exactly as the driver does
# ---------------------------------------------------------------------------
def fmt_ra(hours):
    t = round(hours * 3600 * 10000) % (24 * 3600 * 10000)
    frac, t = t % 10000, t // 10000
    return "%02d:%02d:%02d.%04d" % (t // 3600, (t // 60) % 60, t % 60, frac)


def fmt_dec(deg):
    sign = "-" if deg < 0 else "+"
    t = round(abs(deg) * 3600 * 1000)
    frac, t = t % 1000, t // 1000
    return "%s%02d:%02d:%02d.%03d" % (sign, t // 3600, (t // 60) % 60, t % 60, frac)


def sexa(s):
    """Parse firmware sexagesimal like '+52*00:00.000' or '12:34:56.0000'."""
    neg = s.strip().startswith("-")
    parts = [float(p) for p in re.split(r"[^0-9.]+", s.strip().lstrip("+-")) if p]
    v = 0.0
    for i, p in enumerate(parts[:3]):
        v += p / (60 ** i)
    return -v if neg else v


def num(s):
    """Leading number of a reply like atof() (e.g. ':Gh#' -> '-10*')."""
    m = re.match(r"\s*[-+]?\d+(\.\d*)?", s or "")
    return float(m.group(0)) if m else 0.0


def dms_parts(value):
    a = abs(value)
    d = int(a)
    m = int((a - d) * 60)
    s = round(((a - d) * 60 - m) * 60, 2)
    if s >= 60:
        s -= 60
        m += 1
    if m >= 60:
        m -= 60
        d += 1
    return d, m, s


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------
def run(link):
    res = Results()
    T = link.text

    print("== identity / probe")
    for c in (":GVP#", ":GVN#", ":GVD#", ":GVT#", ":GVC#"):
        check(link, res, c, TEXT)
    rot = check(link, res, ":GX98#", TEXT, absent_ok=True)
    has_rotator = rot[:1] in (b"R", b"D")
    focusers = []
    for n in range(1, 7):
        if check(link, res, ":F%da#" % n, BOOL) == b"1":
            focusers.append(n)
    fmask = check(link, res, ":GXY0#", TEXT, absent_ok=True)
    features = [i + 1 for i, ch in enumerate(fmask[:8].decode("latin-1")) if ch == "1"] if fmask.endswith(b"#") else []
    pmask = check(link, res, ":GUY0#", TEXT, absent_ok=True)
    ports = [i + 1 for i, ch in enumerate(pmask[:8].decode("latin-1")) if ch == "1"] if pmask.endswith(b"#") else []
    for c in (":GX9A#", ":GX9B#", ":GX9C#", ":GX9E#"):
        check(link, res, c, TEXT)
    check(link, res, ":GX9F#", TEXT, absent_ok=True)
    for axis in range(1, 6):
        check(link, res, ":GXU%d#" % axis, TEXT, absent_ok=True, note="driver status; '0' if not reported")
    check(link, res, ":Gv#", TEXT)

    gw = check(link, res, ":GW#", TEXT, absent_ok=True)
    has_mount = gw.endswith(b"#")
    print(f"   rotator={has_rotator} focusers={focusers} features={features} usb={ports} mount={has_mount}")

    if has_mount:
        run_mount(link, res, T)
    else:
        print("== mount: not present, mount checks skipped")

    if has_rotator:
        run_rotator(link, res, T, derotator=rot[:1] == b"D")
    for n in focusers:
        run_focuser(link, res, T, n)
    for n in features:
        run_feature(link, res, T, n)
    for n in ports:
        run_usb(link, res, T, n)

    print("== weather / DUT1 (no-sensor values)")
    for c, v in ((":SX9A,%.2f#", 10.0), (":SX9B,%.2f#", 1010.0), (":SX9C,%.2f#", 70.0)):
        check(link, res, c % v, BOOL, must_succeed=True)
    check(link, res, ":SU%+.3f#" % 0.0, BOOL, must_succeed=True)

    print("== reticle")
    check(link, res, ":B+#", NONE)
    check(link, res, ":B-#", NONE)

    return res


def run_mount(link, res, T):
    print("== mount status")
    check(link, res, ":Gu#", BIN)
    gu = check(link, res, ":GU#", TEXT).decode("latin-1")
    check(link, res, ":h?#", TEXT)
    check(link, res, ":Gm#", TEXT)
    check(link, res, ":GX94#", TEXT, note="' N' appended if the mount does not flip")
    check(link, res, ":GXTR#", TEXT, note="custom-rate RA offset, read at connect")
    check(link, res, ":GXTD#", TEXT, note="custom-rate Dec offset, read at connect")
    has_pec = any(ch in gu for ch in "/,~;^")

    print("== site / time (values written back)")
    for c in (":GtH#", ":GgH#", ":GSH#", ":W?#", ":GM#", ":GN#", ":GO#", ":GP#"):
        check(link, res, c, TEXT)
    lat = sexa(T(":GtH#"))
    d, m, s = dms_parts(lat)
    check(link, res, ":St%c%02d:%02d:%.2f#" % ("-" if lat < 0 else "+", d, m, s), BOOL, must_succeed=True)
    lon = sexa(T(":GgH#")) % 360.0           # firmware longitude, east negative -> 0..360 west
    d, m, s = dms_parts(lon)
    check(link, res, ":Sg%.03d:%02d:%.02f#" % (d % 360, m, s), BOOL, must_succeed=True)
    elev = T(":Gv#")
    check(link, res, ":Sv%d#" % int(round(num(elev))), BOOL, must_succeed=True)
    site = T(":W?#")
    check(link, res, ":W%s#" % site, NONE)
    name = T(":GM#") if site == "0" else None
    if name is not None:
        check(link, res, ":SM%s#" % name, BOOL, must_succeed=True)
    tz = T(":GG#")                          # "+HH:MM", hours added to local to give UT
    tz_sign, tz_h, tz_m = tz[0], int(tz[1:3]), int(tz[4:6])
    if tz_m:
        check(link, res, ":SG%c%02d:%02d#" % (tz_sign, tz_h, tz_m), BOOL, must_succeed=True)
    else:
        check(link, res, ":SG%c%02d#" % (tz_sign, tz_h), BOOL, must_succeed=True)
    hh, mm, ss = [int(float(x)) for x in T(":GL#").split(":")[:3]]
    check(link, res, ":SL%02d:%02d:%02d#" % (hh, mm, ss), BOOL, must_succeed=True)
    mo, dd, yy = [int(x) for x in T(":GC#").split("/")]
    check(link, res, ":SC%02d/%02d/%02d#" % (mo, dd, yy % 100), BOOL, must_succeed=True)

    print("== limits / backlash (values written back)")
    for c in (":Gh#", ":Go#", ":GXE9#", ":GXEA#", ":%BR#", ":%BD#"):
        check(link, res, c, TEXT)
    check(link, res, ":Sh%d#" % int(num(T(":Gh#"))), BOOL, must_succeed=True)
    check(link, res, ":So%d#" % int(num(T(":Go#"))), BOOL, must_succeed=True)
    check(link, res, ":SXE9,%d#" % int(num(T(":GXE9#"))), BOOL, must_succeed=True)
    check(link, res, ":SXEA,%d#" % int(num(T(":GXEA#"))), BOOL, must_succeed=True)
    check(link, res, ":$BR%d#" % int(num(T(":%BR#"))), BOOL, must_succeed=True)
    check(link, res, ":$BD%d#" % int(num(T(":%BD#"))), BOOL, must_succeed=True)

    print("== tracking")
    check(link, res, ":GT#", TEXT)
    check(link, res, ":Te#", BOOL)
    for c in (":TQ#", ":TL#", ":TS#", ":TK#", ":TQ#"):
        check(link, res, c, NONE)
    for c in (":T+#", ":T-#", ":TR#"):
        check(link, res, c, NONE)
    rc = "t" if "t" in gu else ("r" if "r" in gu else "n")
    single = "s" in gu
    for c in (":To#", ":Tr#", ":Tn#"):
        check(link, res, c, BOOL, must_succeed=True)
    for c in (":T2#", ":T1#"):
        check(link, res, c, BOOL, must_succeed=True)
    check(link, res, {"t": ":To#", "r": ":Tr#", "n": ":Tn#"}[rc], BOOL, must_succeed=True)
    check(link, res, ":T1#" if single else ":T2#", BOOL, must_succeed=True)
    check(link, res, ":SXTR,%.5f#" % 0.0, BOOL, must_succeed=True)
    check(link, res, ":SXTD,%.5f#" % 0.0, BOOL, must_succeed=True)
    flip = check(link, res, ":GX95#", TEXT)
    check(link, res, ":SX95,%s#" % flip[:1].decode(), BOOL, note="'0' if not equatorial")
    pier = check(link, res, ":GX96#", TEXT)[:1].decode()
    for p in "WEBA":
        check(link, res, ":SX96,%s#" % p, BOOL, note="'0' if meridian flips are off")
    check(link, res, ":SX96,%s#" % pier, BOOL)
    check(link, res, ":SX99,1#", BOOL, note="no-op unless paused at home; '0' if the mount does not flip")
    paused = "u" in (T(":GU#") or "")
    check(link, res, ":SX98,1#", BOOL, note="pause at home on")
    check(link, res, ":SX98,%d#" % (1 if paused else 0), BOOL, note="pause at home written back")
    check(link, res, ":GX97#", TEXT)
    us = check(link, res, ":GX92#", TEXT)
    check(link, res, ":SX92,%.3f#" % float(us[:-1]), BOOL, must_succeed=True)

    print("== guiding / manual motion")
    check(link, res, ":GX90#", TEXT)
    for c in (":R0#", ":R1#", ":R2#", ":R3#", ":RG#", ":RC#", ":RM#", ":RS#", ":R1#"):
        check(link, res, c, NONE)
    for d in "nsew":
        check(link, res, ":Mg%c%u#" % (d, 100), NONE)
    for start, stop in ((":Mn#", ":Qn#"), (":Ms#", ":Qs#"), (":Me#", ":Qe#"), (":Mw#", ":Qw#")):
        check(link, res, start, NONE)
        check(link, res, stop, NONE)

    print("== goto / sync")
    ra, dec = sexa(T(":GRH#")), sexa(T(":GDH#"))
    for c in (":GRH#", ":GDH#"):
        check(link, res, c, TEXT)
    for h, dg in ((5.5, 45.5), (13.25, -5.5), (0.1, 0.25), (ra, dec)):
        check(link, res, ":Sr%s#" % fmt_ra(h), BOOL, must_succeed=True, note="driver format")
        check(link, res, ":Sd%s#" % fmt_dec(dg), BOOL, must_succeed=True, note="driver format")
    check(link, res, ":MS#", CHAR, note="digit; '0' = goto started")
    check(link, res, ":Q#", NONE)
    time.sleep(0.5)
    check(link, res, ":MN#", CHAR, note="digit; '0' = flip started")
    check(link, res, ":Q#", NONE)
    time.sleep(0.5)
    check(link, res, ":Sr%s#" % fmt_ra(ra), BOOL, must_succeed=True)
    check(link, res, ":Sd%s#" % fmt_dec(dec), BOOL, must_succeed=True)
    check(link, res, ":CM#", TEXT, note="N/A or En")

    print("== alignment / model (model written back)")
    check(link, res, ":A?#", TEXT)
    check(link, res, ":GX02#", TEXT, absent_ok=True)
    check(link, res, ":GX03#", TEXT, absent_ok=True)
    model = {}
    for i in "012345678abcd":
        r = check(link, res, ":GX0%c#" % i, TEXT, absent_ok=True)
        if r.endswith(b"#") and r[:-1].lstrip(b"-").isdigit():
            model[i] = int(r[:-1])          # not written back if the reply was garbled
    for i, v in model.items():
        if i in "67":
            continue        # only one of 6/7 is meaningful per mount type
        check(link, res, ":SX0%c,%d#" % (i, v), BOOL, must_succeed=True)
    # :MP# zeroes the polar terms (2, 3) and re-slews; write them back after.
    check(link, res, ":MP#", CHAR, note="digit; '0' = refine slew started")
    check(link, res, ":Q#", NONE)
    time.sleep(0.5)
    for i in "23":
        if i in model:
            check(link, res, ":SX0%c,%d#" % (i, model[i]), BOOL, must_succeed=True)
    check(link, res, ":SX09,2#", BOOL, must_succeed=True)
    check(link, res, ":A1#", BOOL)
    check(link, res, ":A+#", BOOL)
    check(link, res, ":AW#", BOOL)

    print("== PEC")
    check(link, res, ":$QZ?#", TEXT)
    check(link, res, ":VW#", TEXT)
    if has_pec:
        for c in (":$QZ-#", ":$QZ/#", ":$QZ-#", ":$QZZ#", ":$QZ+#", ":$QZ-#", ":$QZ!#"):
            check(link, res, c, NONE)
    else:
        for c in (":$QZ+#", ":$QZ-#", ":$QZ/#", ":$QZZ#", ":$QZ!#"):
            skip(res, c, NONE, "PEC not compiled in")

    print("== park / home (state-changing; test controllers only)")
    boot = "B" in gu
    check(link, res, ":hA1#", NONE)
    check(link, res, ":hA%d#" % (1 if boot else 0), NONE)
    check(link, res, ":hC1,%d#" % 0, NONE)
    check(link, res, ":hC2,%d#" % 0, NONE)
    check(link, res, ":hF#", NONE)
    check(link, res, ":hQ#", BOOL, must_succeed=True)
    check(link, res, ":hP#", BOOL)
    for _ in range(40):              # wait for park to finish (no mechanics: quick)
        if "P" in (T(":GU#") or ""):
            break
        time.sleep(0.25)
    check(link, res, ":hR#", BOOL)
    check(link, res, ":hC#", NONE)
    time.sleep(1.0)
    check(link, res, ":Q#", NONE)
    check(link, res, ":Td#", BOOL)


def run_rotator(link, res, T, derotator):
    print("== rotator")
    for c in (":rG#", ":rT#", ":rI#", ":rM#", ":rb#"):
        check(link, res, c, TEXT)
    bl = T(":rb#")
    check(link, res, ":rb%d#" % int(bl), BOOL, must_succeed=True)
    check(link, res, ":rS%c%03d:%02d:%02d#" % ("+", 10, 0, 0), BOOL)
    check(link, res, ":rQ#", NONE)
    check(link, res, ":rC#", NONE)
    check(link, res, ":rQ#", NONE)
    time.sleep(1)
    check(link, res, ":rF#", NONE, note="current position becomes half travel")
    check(link, res, ":rZ#", NONE, note="current position becomes 0")
    if derotator:
        for c in (":r+#", ":r-#", ":rP#", ":rR#", ":rR#"):     # :rR# toggles, so twice
            check(link, res, c, NONE)
    else:
        for c in (":r+#", ":r-#", ":rP#", ":rR#"):
            skip(res, c, NONE, "no de-rotator (not AltAz)")


def run_focuser(link, res, T, n):
    print(f"== focuser {n}")
    for b in ("G", "I", "M", "T", "t"):
        check(link, res, ":F%d%s#" % (n, b), TEXT)
    pos = int(T(":F%dG#" % n))
    bl = T(":F%dB#" % n)
    check(link, res, ":F%dB%d#" % (n, int(num(bl))), BOOL, must_succeed=True)
    check(link, res, ":F%dS%u#" % (n, max(pos, 0)), BOOL, must_succeed=True)
    check(link, res, ":F%dR%+ld#" % (n, 10), NONE)
    check(link, res, ":F%dQ#" % n, NONE)
    for v in range(5, 10):
        check(link, res, ":F%d%d#" % (n, v), NONE)
    check(link, res, ":F%d7#" % n, NONE)
    check(link, res, ":F%dZ#" % n, NONE)
    # temperature compensation, written back
    on = check(link, res, ":F%dc#" % n, BOOL)
    coef = check(link, res, ":F%dC#" % n, TEXT)
    band = check(link, res, ":F%dd#" % n, TEXT)
    check(link, res, ":F%de#" % n, TEXT)
    if coef.endswith(b"#"):
        check(link, res, ":F%dC%.3f#" % (n, float(coef[:-1])), BOOL, must_succeed=True)
    if band.endswith(b"#"):
        check(link, res, ":F%dd%d#" % (n, int(band[:-1])), BOOL, must_succeed=True)
    check(link, res, ":F%dc%s#" % (n, "1" if on == b"1" else "0"), BOOL, note="'1' even if it stays off")
    # home: set the current position as home, then go there
    check(link, res, ":F%dH#" % n, NONE)
    check(link, res, ":F%dh#" % n, NONE)
    check(link, res, ":F%dQ#" % n, NONE)


def run_feature(link, res, T, n):
    print(f"== aux feature {n}")
    info = check(link, res, ":GXY%d#" % n, TEXT)
    purpose = info[:-1].decode("latin-1").rsplit(",", 1)[-1] if info.endswith(b"#") else "?"
    if purpose == "6":
        # HIDDEN_SWITCH: no value branch in Features.command.cpp, so :GXX# is a
        # bare '0'.  The driver skips these slots.
        check(link, res, ":GXX%d#" % n, TEXT, absent_ok=True, note="hidden switch: bare '0'")
        return
    vals = check(link, res, ":GXX%d#" % n, TEXT)
    v = vals[:-1].decode("latin-1").split(";")[0].split(",") if vals.endswith(b"#") else []
    if purpose in ("1", "2"):                      # switch / analog
        check(link, res, ":SXX%d,%c%.3f#" % (n, "V", float(v[0])), BOOL, must_succeed=True)
    elif purpose == "3":                            # dew heater: en,zero,span,dT
        check(link, res, ":SXX%d,%c%.3f#" % (n, "Z", float(v[1])), BOOL, must_succeed=True)
        check(link, res, ":SXX%d,%c%.3f#" % (n, "S", float(v[2])), BOOL, must_succeed=True)
        check(link, res, ":SXX%d,%c%.3f#" % (n, "V", float(v[0])), BOOL, must_succeed=True)
    elif purpose == "4":                            # intervalometer: cur,exp,delay,count
        check(link, res, ":SXX%d,%c%.3f#" % (n, "E", float(v[1])), BOOL, must_succeed=True)
        check(link, res, ":SXX%d,%c%.3f#" % (n, "D", float(v[2])), BOOL, must_succeed=True)
        check(link, res, ":SXX%d,%c%.3f#" % (n, "C", float(v[3])), BOOL, must_succeed=True)
        check(link, res, ":SXX%d,%c%.3f#" % (n, "V", 0.0), BOOL, must_succeed=True)


def run_usb(link, res, T, n):
    print(f"== USB port {n}")
    check(link, res, ":GUY%d#" % n, TEXT)
    v = check(link, res, ":GUX%d#" % n, TEXT)
    if v.endswith(b"#"):
        check(link, res, ":SUX%d,%c%d#" % (n, "V", int(v[:-1])), BOOL, must_succeed=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial device, or host:port for a network command channel")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--check-source", action="store_true")
    ap.add_argument("--log", help="write every command/reply with timestamps to this file")
    onstepx_safety.add_flag(ap)
    a = ap.parse_args()

    if a.check_source:
        return check_source()
    if not a.port:
        ap.error("--port is required unless --check-source is given")
    onstepx_safety.require(a)

    link = Link(a.port, a.baud, a.log)
    res = run(link)
    res.noise_report(link)
    return 1 if res.summary() else 0


if __name__ == "__main__":
    sys.exit(main())
