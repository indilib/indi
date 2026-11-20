#!/usr/bin/env python3
# OnStepX INDI Driver — onstepx_tap.py
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
Serial tap between indi_onstepx and a real OnStepX controller.

The tap holds the controller's serial port and gives the driver a virtual
serial port (a pseudo-terminal, reached through a symlink).  Bytes are
forwarded both ways unchanged.  A test can also put its own commands on the
same line with query().  A query is sent only when no driver transaction is
open, so it never lands inside one.  Driver commands are never held back: one
that arrives while a query is outstanding goes straight on, and the replies
are matched in order, since the firmware answers each channel's commands in
the order they arrive.  The driver therefore never waits on the tap; it only
sees what the firmware does anyway (a slow reply in an NV stall, or rarely a
lost one).  That lets a test read firmware state while the driver is connected.

To know where each driver transaction ends, the tap looks up the reply type
of every command in onstepx_wire.py.  A driver command missing from that
table, or a reply the table does not expect, is recorded as a problem.

The tap keeps the real port open, so connecting the driver does not reset
the controller the way opening /dev/ttyUSB0 directly does.

Library use:
    tap = Tap("/dev/ttyUSB0", 9600, "/tmp/onstepx-tap", log_path="wire.log")
    tap.start()
    ...point the driver at /tmp/onstepx-tap...
    tap.text(":GU#")          # -> "nNpH..." without the '#'
    tap.reset_controller()    # reboot the controller under the driver
    tap.stop()

Manual use (for example with KStars run by hand):
    onstepx_tap.py --port /dev/ttyUSB0 --link /tmp/onstepx-tap --log wire.log

Offline check of the framing against a fake controller (no hardware):
    onstepx_tap.py --self-test
"""

import argparse
import collections
import os
import select
import sys
import termios
import threading
import time
import tty

import serial

import onstepx_wire
from onstepx_wire import TEXT, BOOL, NONE, CHAR, BIN

TEXT_QUIET_S = 0.05       # a TEXT reply with no '#' (bare "0") ends after this gap
REPLY_TIMEOUT_S = 3.0     # longer than the ESP32's ~2 s stall around NV writes
UNKNOWN_QUIET_S = 0.5     # after an unclassified command, wait for this much quiet
LATE_WINDOW_S = 5.0       # after the driver abandons a command, its reply may still arrive


class Transaction:
    def __init__(self, owner, cmd, kind):
        self.owner = owner            # "driver" or "tap"
        self.cmd = cmd                # bytes, including ':' and '#'
        self.kind = kind              # reply type, or None if unknown
        self.reply = bytearray()      # reply bytes without 0xFF noise
        self.sent = time.monotonic()
        self.last_rx = None
        self.result = None            # "ok", "timeout", "short", "abandoned" once complete
        self.done = threading.Event()


class Tap:
    def __init__(self, port, baud, link_path, log_path=None):
        self.port, self.baud, self.link_path = port, baud, link_path
        self.log_path = log_path
        self._log = None
        self._ser = None
        self._master = self._slave = None
        self._thread = None
        self._stop = threading.Event()
        self._lock = threading.Lock()
        self._requests = collections.deque()      # tap transactions waiting to be sent
        self._pending = collections.deque()       # sent, reply not yet complete
        self._driver_buf = bytearray()            # partial driver command
        self._held = collections.deque()          # complete driver commands not yet sent
        self._quiet_until = 0.0
        self._late_until = 0.0                    # late replies to abandoned commands expected
        self._t0 = time.monotonic()
        # what the harness checks afterwards
        self.problems = []            # strings: unknown commands, unexpected replies
        self.driver_commands = collections.Counter()
        self.queries = 0
        self.overlaps = 0             # driver commands sent while a query was outstanding
        self.noise_bursts = 0
        self.abandoned = 0            # driver commands it gave up on (timed out) before a reply

    # ------------------------------------------------------------------ setup
    def start(self):
        if self.log_path:
            self._log = open(self.log_path, "w", buffering=1)
        self._ser = serial.Serial(self.port, self.baud, timeout=0)
        self._master, self._slave = os.openpty()
        tty.setraw(self._master)
        tty.setraw(self._slave)       # held open so the master never sees a hangup
        if os.path.lexists(self.link_path):
            os.unlink(self.link_path)
        os.symlink(os.ttyname(self._slave), self.link_path)
        self._thread = threading.Thread(target=self._run, name="onstepx-tap", daemon=True)
        self._thread.start()
        self._note("tap: %s <-> %s (%s)" % (self.link_path, self.port, os.ttyname(self._slave)))

    def stop(self):
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=2)
        if os.path.islink(self.link_path):
            os.unlink(self.link_path)
        for fd in (self._master, self._slave):
            if fd is not None:
                os.close(fd)
        if self._ser:
            self._ser.close()
        if self._log:
            self._log.close()

    def reset_controller(self, hold_s=0.2):
        """Reboot the controller while the driver stays connected: pulse the
        ESP32's EN line through RTS, as esptool does (DTR low, so it boots
        normally rather than into the bootloader)."""
        self._note("tap: resetting the controller (RTS pulse)")
        self._ser.dtr = False
        self._ser.rts = True
        time.sleep(hold_s)
        self._ser.rts = False

    # ------------------------------------------------------------- side channel
    def query(self, cmd, timeout=None):
        """Send cmd between driver transactions.  Returns (result, reply bytes).

        result is "ok", "timeout" or "short" (a TEXT reply without '#', such as
        a bare "0").  The reply has 0xFF noise removed and keeps any '#'."""
        if isinstance(cmd, str):
            cmd = cmd.encode("latin-1")
        kind = onstepx_wire.reply_type(cmd)
        if kind is None:
            raise ValueError("%r is not in onstepx_wire.py" % cmd)
        t = Transaction("tap", cmd, kind)
        with self._lock:
            self._requests.append(t)
        if not t.done.wait(timeout if timeout is not None else REPLY_TIMEOUT_S + 30):
            raise TimeoutError("tap query %r never completed (%s)" % (cmd, self._state()))
        self.queries += 1
        return t.result, bytes(t.reply)

    def text(self, cmd, retries=2):
        """TEXT query; returns the reply without '#', or None."""
        for _ in range(retries + 1):
            result, reply = self.query(cmd)
            if result == "ok" and reply.endswith(b"#"):
                return reply[:-1].decode("latin-1")
            if result == "short":
                return None
        return None

    def wait_ready(self, timeout=30):
        """After the port opens (which resets the controller), wait for clean replies."""
        end = time.monotonic() + timeout
        good = 0
        while time.monotonic() < end:
            if self.text(":GVP#", retries=0) == "On-Step":
                good += 1
                if good >= 3:
                    return True
            else:
                good = 0
            time.sleep(0.5)
        return False

    # --------------------------------------------------------------- IO thread
    def _run(self):
        ser_fd = self._ser.fileno()
        while not self._stop.is_set():
            r, _, _ = select.select([self._master, ser_fd], [], [], 0.01)
            now = time.monotonic()
            if self._master in r:
                try:
                    data = os.read(self._master, 4096)
                except OSError:
                    data = b""
                if data:
                    self._from_driver(data, now)
            if ser_fd in r:
                data = self._ser.read(self._ser.in_waiting or 1)
                if data:
                    self._from_controller(data, now)
            self._expire(now)
            self._dispatch(now)

    def _from_driver(self, data, now):
        self._driver_buf += data
        while b"#" in self._driver_buf:
            i = self._driver_buf.index(b"#") + 1
            cmd = bytes(self._driver_buf[:i])
            del self._driver_buf[:i]
            self._held.append((cmd, now))
        self._dispatch(now)

    def _dispatch(self, now):
        """Send driver commands at once, and one tap query when the line is free."""
        while self._held:
            cmd, _ = self._held.popleft()
            if any(t.owner == "tap" for t in self._pending):
                self.overlaps += 1
            self._send(Transaction("driver", cmd, onstepx_wire.reply_type(cmd)), now)
        with self._lock:
            ready = (self._requests and not self._pending and not self._held
                     and not self._driver_buf and now >= self._quiet_until)
            if ready:
                self._send(self._requests.popleft(), now)

    def _send(self, t, now):
        t.sent = now
        if t.owner == "driver":
            # OnStepXComm sends one command at a time, so a new one means the
            # driver has given up on any still open (for example during an
            # ESP32 NV stall).  Close them, or every later reply would be
            # matched to the command before it and the line would never look free.
            for old in [p for p in self._pending if p.owner == "driver"]:
                self._pending.remove(old)
                self.abandoned += 1
                self._finish_log(old, "abandoned")
                old.result = "abandoned"
                old.done.set()
                self._late_until = now + LATE_WINDOW_S
                self._quiet_until = max(self._quiet_until, now + UNKNOWN_QUIET_S)
            self.driver_commands[t.cmd.decode("latin-1")] += 1
            if t.kind is None:
                self._problem("driver sent %r, which is not in onstepx_wire.py" % t.cmd)
        self._ser.write(t.cmd)
        self._wire("%s>" % ("D" if t.owner == "driver" else "T"), t.cmd)
        if t.kind == NONE:
            t.result = "ok"
            t.done.set()
            # a NONE command should leave the line silent; let it settle before a query
            self._quiet_until = now + TEXT_QUIET_S
            return
        self._pending.append(t)

    def _from_controller(self, data, now):
        noise = data.count(0xFF)
        if noise:
            self.noise_bursts += 1
            self._wire("FF", b"%d bytes of 0xFF" % noise)
        for b in data:
            head = self._pending[0] if self._pending else None
            if head is None:
                if b != 0xFF:
                    if now < self._late_until:
                        # a late reply to a command the driver abandoned; it is the
                        # driver's to discard, and the line is not free until it stops
                        self._wire("late", bytes([b]))
                        self._quiet_until = max(self._quiet_until, now + UNKNOWN_QUIET_S)
                    else:
                        self._problem("unexpected byte %r with no command outstanding" % bytes([b]))
                        self._wire("??", bytes([b]))
                    os.write(self._master, bytes([b]))
                continue
            if head.owner == "driver":
                os.write(self._master, bytes([b]))     # forward raw, noise included
            if b == 0xFF and head.kind != BIN:
                continue
            head.reply.append(b)
            head.last_rx = now
            if self._complete(head):
                self._finish(head, "ok", now)

    @staticmethod
    def _complete(t):
        n = len(t.reply)
        if t.kind in (BOOL, CHAR):
            return n >= 1
        if t.kind == BIN:
            return n >= 10 and t.reply.endswith(b"#")
        return t.reply.endswith(b"#")          # TEXT or unknown

    def _expire(self, now):
        while self._pending:
            t = self._pending[0]
            if t.kind in (TEXT, None) and t.last_rx and now - t.last_rx >= TEXT_QUIET_S \
                    and not t.reply.endswith(b"#"):
                self._finish(t, "short", now)
            elif now - t.sent >= REPLY_TIMEOUT_S and not t.last_rx:
                self._finish(t, "timeout", now)
            elif t.kind == BIN and t.last_rx and now - t.last_rx >= 0.5:
                self._finish(t, "short", now)
            else:
                break

    def _finish_log(self, t, result):
        self._wire("%s<" % ("D" if t.owner == "driver" else "T"),
                   bytes(t.reply) + (b"" if result == "ok" else b"   [%s]" % result.encode()))

    def _finish(self, t, result, now):
        self._pending.popleft()
        t.result = result
        self._finish_log(t, result)
        if t.kind is None:
            self._quiet_until = now + UNKNOWN_QUIET_S
        t.done.set()

    def _state(self):
        now = time.monotonic()
        return ("pending %s, held %d, partial driver bytes %r, quiet for %.2f s more, %d queued"
                % ([(t.owner, t.cmd, t.kind, bytes(t.reply)) for t in self._pending], len(self._held),
                   bytes(self._driver_buf), max(0.0, self._quiet_until - now), len(self._requests)))

    # ----------------------------------------------------------------- logging
    def _problem(self, msg):
        self.problems.append(msg)
        self._note("PROBLEM: " + msg)

    def _wire(self, tag, data):
        if self._log:
            self._log.write("%9.3f %s %r\n" % (time.monotonic() - self._t0, tag, bytes(data)))

    def _note(self, msg):
        if self._log:
            self._log.write("%9.3f -- %s\n" % (time.monotonic() - self._t0, msg))


class _FakeController:
    """Answers like the firmware on a pseudo-terminal; used by self_test()."""

    REPLIES = {b":GVP#": b"On-Step#", b":GU#": b"nNpH#", b":Gu#": bytes([0x80] * 9) + b"#",
               b":Te#": b"1", b":Q#": b"", b":GVN#": b"10.29c#"}

    def __init__(self):
        self.master, slave = os.openpty()
        tty.setraw(self.master)
        tty.setraw(slave)
        self.path = os.ttyname(slave)
        self._slave = slave
        self.stall_next_gu = None      # "late": hold the next :Gu# reply until another command
                                       # arrives; "lost": never send it
        self._deferred = b""
        self.slow = {}                 # command -> seconds to stall before replying
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        buf = b""
        while not self._stop.is_set():
            r, _, _ = select.select([self.master], [], [], 0.05)
            if not r:
                continue
            buf += os.read(self.master, 1024)
            while b"#" in buf:
                i = buf.index(b"#") + 1
                cmd, buf = buf[:i], buf[i:]
                reply = self.REPLIES.get(cmd, b"0")
                time.sleep(self.slow.pop(cmd, 0.0))     # stall, blocking later commands too
                if cmd == b":Gu#" and self.stall_next_gu:
                    # ESP32 NV stall: noise, then the reply late or not at all
                    self._deferred = b"\xff" * 64 + (reply if self.stall_next_gu == "late" else b"")
                    self.stall_next_gu = None
                    continue
                os.write(self.master, self._deferred + reply)
                self._deferred = b""

    def close(self):
        self._stop.set()
        self._thread.join(timeout=1)
        os.close(self.master)
        os.close(self._slave)


def self_test():
    """Offline check of the tap's framing against a fake controller."""
    import tempfile
    failures = []

    def expect(name, ok, detail=""):
        print("%-5s %-52s %s" % ("PASS" if ok else "FAIL", name, detail))
        if not ok:
            failures.append(name)

    work = tempfile.mkdtemp(prefix="osx-tap-test-")
    ctl = _FakeController()
    tap = Tap(ctl.path, 115200, os.path.join(work, "tap"), log_path=os.path.join(work, "wire.log"))
    tap.start()
    drv = os.open(os.path.join(work, "tap"), os.O_RDWR | os.O_NOCTTY)
    tty.setraw(drv)

    def driver(cmd, n, timeout=1.0):
        termios.tcflush(drv, termios.TCIFLUSH)       # as OnStepXComm does before every send
        os.write(drv, cmd)
        got, end = b"", time.monotonic() + timeout
        while len(got) < n and time.monotonic() < end:
            r, _, _ = select.select([drv], [], [], 0.05)
            if r:
                got += os.read(drv, 1024)
        return got

    try:
        expect("driver command forwarded and answered", driver(b":GVP#", 8) == b"On-Step#")
        expect("BOOL reply forwarded", driver(b":Te#", 1) == b"1")
        expect("tap query between driver commands", tap.text(":GU#") == "nNpH")
        expect("BIN reply forwarded", driver(b":Gu#", 10) == bytes([0x80] * 9) + b"#")

        # The driver times out on :Gu# (NV stall) and moves on; the late reply
        # arrives ahead of the next one.  The tap must not stay one reply behind.
        ctl.stall_next_gu = "late"
        driver(b":Gu#", 10, timeout=0.3)
        got = driver(b":GU#", 64 + 10 + 5, timeout=1.0)
        expect("late reply reaches the driver, then the new one", got.endswith(b"#nNpH#"), repr(got[-16:]))
        expect("abandoned command counted", tap.abandoned == 1, "%d" % tap.abandoned)
        expect("tap query still works after an abandoned command", tap.text(":GVP#") == "On-Step")
        # A reply lost in a stall while the driver keeps polling faster than the
        # tap's timeout: without closing the abandoned command, the tap stays one
        # transaction behind and its queries never find the line free.
        ctl.stall_next_gu = "lost"
        driver(b":Gu#", 10, timeout=0.3)
        polling = threading.Event()

        def poll():
            while not polling.is_set():
                driver(b":GU#", 5, timeout=0.3)
                time.sleep(0.2)

        poller = threading.Thread(target=poll, daemon=True)
        poller.start()
        try:
            got = tap.text(":GVP#", retries=0) if tap.query(":GVP#", timeout=4)[0] else None
            expect("tap query works while the driver polls after a lost reply", got == "On-Step", repr(got))
        except TimeoutError as e:
            expect("tap query works while the driver polls after a lost reply", False, str(e)[:90])
        polling.set()
        poller.join(timeout=2)
        expect("two abandoned commands counted", tap.abandoned == 2, "%d" % tap.abandoned)
        expect("no problems so far", not tap.problems, "; ".join(tap.problems))

        driver(b":XYZ#", 1, timeout=0.6)
        expect("unknown driver command recorded", any("XYZ" in p for p in tap.problems))
        expect("tap query works after an unknown command", tap.text(":GVP#") == "On-Step")
        # A query caught in a stall must not hold up the driver's next command:
        # it goes straight on, and each side gets its own reply.
        ctl.slow[b":GVN#"] = 1.0
        box = {}
        q = threading.Thread(target=lambda: box.update(v=tap.text(":GVN#", retries=0)))
        q.start()
        time.sleep(0.2)
        t0 = time.monotonic()
        got = driver(b":Te#", 1, timeout=3.0)
        q.join(timeout=3)
        expect("driver command passes a stalled query", got == b"1", repr(got))
        expect("driver waits only for the stall itself", time.monotonic() - t0 < 1.2,
               "%.2f s" % (time.monotonic() - t0))
        expect("the stalled query gets its own reply", box.get("v") == "10.29c", repr(box.get("v")))
        expect("overlap counted", tap.overlaps == 1, "%d" % tap.overlaps)
    finally:
        os.close(drv)
        tap.stop()
        ctl.close()
    print("%d failure(s)" % len(failures))
    if failures:
        print("wire log: %s" % os.path.join(work, "wire.log"))
    return 1 if failures else 0


def main():
    ap = argparse.ArgumentParser(description="Serial tap for indi_onstepx")
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--link", default="/tmp/onstepx-tap", help="virtual port path for the driver")
    ap.add_argument("--log", help="wire log file")
    ap.add_argument("--self-test", action="store_true", help="check the tap against a fake controller")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    tap = Tap(a.port, a.baud, a.link, a.log)
    tap.start()
    print("driver port: %s   (Ctrl-C to stop)" % a.link)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        tap.stop()
        for p in tap.problems:
            print("PROBLEM:", p)
        print("%d driver commands, %d queries, %d overlapping, %d abandoned, %d noise bursts"
              % (sum(tap.driver_commands.values()), tap.queries, tap.overlaps, tap.abandoned, tap.noise_bursts))
    return 0


if __name__ == "__main__":
    sys.exit(main())
