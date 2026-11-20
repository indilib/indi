#!/usr/bin/env python3
# OnStepX INDI Driver — onstepx_model_sky.py
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
Test-only INDI driver: makes the CCD Simulator image the "true" sky for a
mount with a known pointing error.

The CCD Simulator renders its star field at the EQUATORIAL_PE position of its
ACTIVE_TELESCOPE device when that property exists (true pointing, with mount
errors applied), and at EQUATORIAL_EOD_COORD otherwise.  This driver snoops
OnStepX's EQUATORIAL_EOD_COORD and TELESCOPE_PIER_SIDE and publishes
EQUATORIAL_PE *under the device name OnStepX*, offset by the same synthetic
error as onstepx_model_e2e.py: an axis1 index error (constant RA offset) and
an axis2 index error (Dec offset whose sign follows the pier side).  Ekos then
plate-solves the error, and a pointing-model build should recover it.

Publishing a property for another driver's device is a test hack: indiserver
routes snooped properties by device name.  Run it under indiserver:
  indiserver ... indi_onstepx indi_simulator_ccd onstepx_model_sky.py

Environment: OSX_SKY_RA_ARCSEC (default 600), OSX_SKY_DEC_ARCSEC (default 300).
"""

import os
import sys
import xml.etree.ElementTree as ET

DEV = "OnStepX"
RA_ARCSEC = float(os.environ.get("OSX_SKY_RA_ARCSEC", "600"))
DEC_ARCSEC = float(os.environ.get("OSX_SKY_DEC_ARCSEC", "300"))


def inject(ra, dec, pier_west):
    """Mount -> 'actual' sky position; the same as onstepx_model_e2e.inject()."""
    dra = RA_ARCSEC / 15.0 / 3600.0
    ddec = DEC_ARCSEC / 3600.0 * (-1.0 if pier_west else 1.0)
    return (ra + dra) % 24.0, dec + ddec


def out(xml):
    sys.stdout.write(xml + "\n")
    sys.stdout.flush()


def log(msg):
    sys.stderr.write("osx_model_sky: %s\n" % msg)
    sys.stderr.flush()


class Sky:
    def __init__(self):
        self.ra = self.dec = None
        self.pier_west = False
        self.defined = False

    def pe_xml(self, tag):
        ra, dec = inject(self.ra, self.dec, self.pier_west)
        elem = "defNumber" if tag == "def" else "oneNumber"
        attrs = " label='RA (hh:mm:ss)' format='%010.6m' min='0' max='24' step='0'" if tag == "def" else ""
        attrd = " label='DEC (dd:mm:ss)' format='%010.6m' min='-90' max='90' step='0'" if tag == "def" else ""
        head = ("<defNumberVector device='%s' name='EQUATORIAL_PE' label='Test sky (true pointing)' "
                "group='Test' state='Ok' perm='ro' timeout='0'>" % DEV) if tag == "def" else \
               "<setNumberVector device='%s' name='EQUATORIAL_PE' state='Ok' timeout='0'>" % DEV
        tail = "</defNumberVector>" if tag == "def" else "</setNumberVector>"
        return "%s<%s name='RA_PE'%s>%.8f</%s><%s name='DEC_PE'%s>%.8f</%s>%s" % (
            head, elem, attrs, ra, elem, elem, attrd, dec, elem, tail)

    def publish(self):
        if self.ra is None:
            return
        out(self.pe_xml("set" if self.defined else "def"))
        self.defined = True

    def handle(self, el):
        tag, dev, name = el.tag, el.get("device"), el.get("name")
        if tag == "getProperties":
            if (dev in (None, DEV)) and self.ra is not None:
                out(self.pe_xml("def"))
                self.defined = True
            return
        if dev != DEV:
            return
        if name == "EQUATORIAL_EOD_COORD" and tag in ("defNumberVector", "setNumberVector"):
            vals = {c.get("name"): (c.text or "").strip() for c in el}
            try:
                self.ra, self.dec = float(vals["RA"]), float(vals["DEC"])
            except (KeyError, ValueError):
                return
            self.publish()
        elif name == "TELESCOPE_PIER_SIDE" and tag in ("defSwitchVector", "setSwitchVector"):
            vals = {c.get("name"): (c.text or "").strip() for c in el}
            west = vals.get("PIER_WEST") == "On"
            if west != self.pier_west:
                self.pier_west = west
                self.publish()


def main():
    sky = Sky()
    for prop in ("EQUATORIAL_EOD_COORD", "TELESCOPE_PIER_SIDE"):
        out("<getProperties version='1.7' device='%s' name='%s'/>" % (DEV, prop))
    log("injecting RA %+g\" (axis1 index), Dec %+g\" (axis2 index, sign by pier side)" % (RA_ARCSEC, DEC_ARCSEC))
    parser = ET.XMLPullParser(events=("start", "end"))
    parser.feed("<stream>")
    depth = 0
    while True:
        chunk = sys.stdin.readline()
        if not chunk:
            break
        parser.feed(chunk)
        for event, el in parser.read_events():
            if event == "start":
                depth += 1
            else:
                depth -= 1
                if depth == 1:          # a complete top-level message
                    sky.handle(el)
                    el.clear()


if __name__ == "__main__":
    main()
