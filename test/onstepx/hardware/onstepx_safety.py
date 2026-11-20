#!/usr/bin/env python3
# OnStepX INDI Driver — onstepx_safety.py
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
Guard for the hardware tests that move or reconfigure a controller.

These tests slew, flip, park and home the mount, and rewrite limits, site,
pointing model and other NV settings.  On a real mount with a telescope on it
a slew past the limits can hit the pier.  Each such test therefore refuses to
run until --test-controller confirms that the device on --port is a test
controller with nothing attached.
"""

import sys

FLAG = "--test-controller"

WARNING = """\
REFUSING TO RUN: this test moves and reconfigures the controller on --port.

It slews, flips, parks and homes the mount, and rewrites limits, site, the
pointing model and other settings stored in the controller.  Run it only on a
test controller with nothing attached (an ESP32 dev board is ideal), never on
a mount carrying a telescope.

If the device on --port is such a test controller, add %s.""" % FLAG


def add_flag(parser):
    """Add --test-controller to an argparse parser."""
    parser.add_argument(FLAG, dest="test_controller", action="store_true",
                        help="confirm the device on --port is a test controller with nothing "
                        "attached; required, because this test moves the mount and rewrites its settings")


def require(args):
    """Exit with a warning unless --test-controller was given."""
    if not getattr(args, "test_controller", False):
        print(WARNING, file=sys.stderr)
        sys.exit(2)
