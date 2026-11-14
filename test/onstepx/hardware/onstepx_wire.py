#!/usr/bin/env python3
# OnStepX INDI Driver — onstepx_wire.py
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
Reply type of every OnStepX command the indi_onstepx driver sends.

The firmware frames replies in five ways (OnStepX ProcessCmds.cpp, poll()):
  TEXT  "value#"; often a bare "0" (no '#') on error  -> OnStepXComm::sendText
  BOOL  one bare '1' / '0', no '#'                     -> OnStepXComm::sendBool
  NONE  nothing at all, even on failure                -> OnStepXComm::sendNoReply
                                                          (sendUrgent for :Q#)
  CHAR  one unframed character                         -> OnStepXComm::sendChar
  BIN   9 bytes >= 0x80, then '#'                      -> OnStepXComm::sendReadN

onstepx_tap.py uses this table to tell where each driver transaction ends, so
it can put its own queries on the line without corrupting the driver's
replies.  onstepx_conformance.py checks every entry against real firmware: a
check whose expected type differs from reply_type() fails.

Usage:
  onstepx_wire.py --self-test       # classify the sample commands below
"""

import re
import sys

TEXT, BOOL, NONE, CHAR, BIN = "TEXT", "BOOL", "NONE", "CHAR", "BIN"

# Ordered: the first matching pattern wins, so specific forms come before
# general ones (":GU#" before ":G...", ":F1B#" before ":F1B10#").
_RULES = [
    # binary and single-character replies
    (r":Gu#", BIN),
    (r":MS#", CHAR),
    (r":MN#", CHAR),
    (r":MP#", CHAR),

    # focusers, addressed as :F<slot><body>#
    (r":F\da#", BOOL),
    (r":F\d[GIMTtB]#", TEXT),
    (r":F\dc[01]?#", BOOL),
    (r":F\d[Cde]#", TEXT),
    (r":F\dC-?[\d.]+#", BOOL),
    (r":F\dd\d+#", BOOL),
    (r":F\d[Hh]#", NONE),
    (r":F\d[BS]\d+#", BOOL),
    (r":F\dR[+-]?\d+#", NONE),
    (r":F\d[QZ1-9]#", NONE),

    # rotator
    (r":r[GTIM]#", TEXT),
    (r":rb#", TEXT),
    (r":rb\d+#", BOOL),
    (r":rS[+-]\d{3}:\d{2}:\d{2}#", BOOL),
    (r":r[QC+\-PRZF]#", NONE),

    # aux features and USB plugin ports
    (r":GX[XY]\d+#", TEXT),
    (r":GU[XY]\d+#", TEXT),
    (r":SXX\d+,[A-Z]-?[\d.]+#", BOOL),
    (r":SUX\d+,[A-Z]\d+#", BOOL),

    # site, time, limits, backlash, weather, DUT1
    (r":S[tg][+-]?[\d:.]+#", BOOL),
    (r":Sv-?\d+#", BOOL),
    (r":S[MNOP][^#]*#", BOOL),
    (r":SG[+-]\d{2}(:\d{2})?#", BOOL),
    (r":SL\d{2}:\d{2}:\d{2}#", BOOL),
    (r":SC\d{2}/\d{2}/\d{2}#", BOOL),
    (r":S[ho]-?\d+#", BOOL),
    (r":SXE[9A],-?\d+#", BOOL),
    (r":%B[RD]#", TEXT),
    (r":\$B[RD]\d+#", BOOL),
    (r":SX9[ABC],-?[\d.]+#", BOOL),
    (r":SU[+-][\d.]+#", BOOL),
    (r":W\?#", TEXT),
    (r":W\d#", NONE),

    # tracking
    (r":T[edorn12]#", BOOL),
    (r":T[QLSK+\-R]#", NONE),
    (r":SXT[RD],-?[\d.]+#", BOOL),
    (r":SX9[56],[A-Z0-9]#", BOOL),
    (r":SX99,1#", BOOL),
    (r":SX98,[01]#", BOOL),
    (r":SX92,[\d.]+#", BOOL),

    # goto, sync, coordinates
    (r":S[rd][+-]?[\d:.*]+#", BOOL),
    (r":CM#", TEXT),
    (r":Q#", NONE),
    (r":Q[nsew]#", NONE),
    (r":M[nsew]#", NONE),
    (r":Mg[nsew]\d+#", NONE),
    (r":R[GCMS\d]#", NONE),

    # alignment and pointing model
    (r":A\?#", TEXT),
    (r":A(\d|\+|W)#", BOOL),
    (r":SX0[0-9A-Za-z],-?\d+#", BOOL),

    # PEC
    (r":\$QZ\?#", TEXT),
    (r":\$QZ[+\-/Z!]#", NONE),
    (r":VW#", TEXT),

    # park and home
    (r":h\?#", TEXT),
    (r":h[QPR]#", BOOL),
    (r":hA\d#", NONE),
    (r":hC[12],-?\d+#", NONE),
    (r":h[CF]#", NONE),

    # reticle
    (r":B[+-]#", NONE),

    # every other :G...# query (identity, status, site, limits, GX...) is TEXT
    (r":G[^#]*#", TEXT),
]

_COMPILED = [(re.compile(p), t) for p, t in _RULES]


def reply_type(cmd):
    """Reply type for a complete command such as ':GU#', or None if unknown."""
    if isinstance(cmd, (bytes, bytearray)):
        cmd = cmd.decode("latin-1")
    for rx, t in _COMPILED:
        if rx.fullmatch(cmd):
            return t
    return None


# Samples of every command form the driver sends, with the type the firmware
# handler gives it.  Kept in step with onstepx_conformance.py.
SAMPLES = {
    ":GVP#": TEXT, ":GVN#": TEXT, ":GVD#": TEXT, ":GVT#": TEXT, ":GVC#": TEXT, ":GX98#": TEXT,
    ":F1a#": BOOL, ":GXY0#": TEXT, ":GUY0#": TEXT, ":GX9A#": TEXT, ":GX9F#": TEXT, ":Gv#": TEXT,
    ":GW#": TEXT, ":Gu#": BIN, ":GU#": TEXT, ":h?#": TEXT, ":Gm#": TEXT, ":GX94#": TEXT,
    ":GRH#": TEXT, ":GDH#": TEXT, ":GSH#": TEXT, ":Sr05:30:00.0000#": BOOL, ":Sd+45*30:00.000#": BOOL,
    ":Sd-05*30:00.000#": BOOL, ":MS#": CHAR, ":MN#": CHAR, ":MP#": CHAR, ":GXU1#": TEXT, ":GXU9#": TEXT, ":CM#": TEXT, ":Q#": NONE,
    ":hP#": BOOL, ":hR#": BOOL, ":hQ#": BOOL, ":hC#": NONE, ":hF#": NONE, ":hA0#": NONE, ":hA1#": NONE,
    ":hC1,0#": NONE, ":hC2,-30#": NONE,
    ":Te#": BOOL, ":Td#": BOOL, ":TQ#": NONE, ":TL#": NONE, ":TS#": NONE, ":TK#": NONE,
    ":T+#": NONE, ":T-#": NONE, ":TR#": NONE, ":To#": BOOL, ":Tr#": BOOL, ":Tn#": BOOL,
    ":T1#": BOOL, ":T2#": BOOL, ":SXTR,0.04095#": BOOL, ":SXTD,-0.00100#": BOOL, ":GT#": TEXT,
    ":GX95#": TEXT, ":GXTR#": TEXT, ":GXTD#": TEXT, ":SX95,0#": BOOL, ":SX95,1#": BOOL, ":GX96#": TEXT, ":SX96,W#": BOOL,
    ":SX96,E#": BOOL, ":SX96,B#": BOOL, ":SX96,A#": BOOL, ":GX97#": TEXT, ":GX92#": TEXT,
    ":SX92,2.500#": BOOL, ":SX99,1#": BOOL, ":SX98,0#": BOOL, ":SX98,1#": BOOL,
    ":F2c#": BOOL, ":F2c1#": BOOL, ":F2C#": TEXT, ":F2C-1.500#": BOOL, ":F2C2.250#": BOOL, ":F2d#": TEXT,
    ":F2d20#": BOOL, ":F2e#": TEXT, ":F2h#": NONE, ":F2H#": NONE, ":rR#": NONE, ":rZ#": NONE, ":rF#": NONE,
    ":RG#": NONE, ":RC#": NONE, ":RM#": NONE, ":RS#": NONE, ":R2#": NONE, ":R3#": NONE, ":Mn#": NONE, ":Ms#": NONE,
    ":Me#": NONE, ":Mw#": NONE, ":Qn#": NONE, ":Qs#": NONE, ":Qe#": NONE, ":Qw#": NONE,
    ":Mgn100#": NONE, ":Mgw2500#": NONE, ":GX90#": TEXT,
    ":GtH#": TEXT, ":GgH#": TEXT, ":St+51:30:00.00#": BOOL, ":St-33:52:00.00#": BOOL,
    ":Sg359:54:00.00#": BOOL, ":Sv50#": BOOL, ":SG-01#": BOOL, ":SG+05:30#": BOOL,
    ":SL12:00:00#": BOOL, ":SC04/01/26#": BOOL, ":GM#": TEXT, ":GN#": TEXT, ":GO#": TEXT,
    ":GP#": TEXT, ":W?#": TEXT, ":W0#": NONE, ":SMDark_Site#": BOOL, ":SP#": BOOL,
    ":Gh#": TEXT, ":Go#": TEXT, ":Sh-10#": BOOL, ":So89#": BOOL, ":GXE9#": TEXT, ":GXEA#": TEXT,
    ":SXE9,15#": BOOL, ":SXEA,-30#": BOOL, ":%BR#": TEXT, ":%BD#": TEXT, ":$BR120#": BOOL,
    ":$BD45#": BOOL,
    ":SX9A,10.00#": BOOL, ":SX9B,1010.00#": BOOL, ":SX9C,70.00#": BOOL, ":SU+0.000#": BOOL,
    ":SU-0.150#": BOOL,
    ":A?#": TEXT, ":A1#": BOOL, ":A+#": BOOL, ":AW#": BOOL, ":GX02#": TEXT, ":GX03#": TEXT,
    ":GX04#": TEXT, ":SX04,-600#": BOOL, ":SX09,2#": BOOL, ":GX0d#": TEXT, ":SX0a,12#": BOOL,
    ":$QZ?#": TEXT, ":$QZ+#": NONE, ":$QZ-#": NONE, ":$QZ/#": NONE, ":$QZZ#": NONE, ":$QZ!#": NONE,
    ":VW#": TEXT, ":B+#": NONE, ":B-#": NONE,
    ":rG#": TEXT, ":rT#": TEXT, ":rI#": TEXT, ":rM#": TEXT, ":rb#": TEXT, ":rb5#": BOOL,
    ":rS+010:00:00#": BOOL, ":rQ#": NONE, ":rC#": NONE, ":r+#": NONE, ":r-#": NONE, ":rP#": NONE,
    ":F1G#": TEXT, ":F2I#": TEXT, ":F1M#": TEXT, ":F1T#": TEXT, ":F1t#": TEXT, ":F1B#": TEXT,
    ":F1B10#": BOOL, ":F2S900#": BOOL, ":F1R+10#": NONE, ":F1R-250#": NONE, ":F1Q#": NONE,
    ":F15#": NONE, ":F19#": NONE, ":F1Z#": NONE,
    ":GXX1#": TEXT, ":GXY3#": TEXT, ":SXX1,V1.000#": BOOL, ":SXX3,Z-2.000#": BOOL, ":GUX1#": TEXT,
    ":GUY3#": TEXT, ":SUX1,V1#": BOOL,
}


def self_test():
    bad = [(c, t, reply_type(c)) for c, t in SAMPLES.items() if reply_type(c) != t]
    for c, want, got in bad:
        print(f"  {c:24} expected {want}, table gives {got}")
    print(f"{len(SAMPLES) - len(bad)}/{len(SAMPLES)} sample commands classified correctly")
    return 1 if bad else 0


if __name__ == "__main__":
    if sys.argv[1:] == ["--self-test"]:
        sys.exit(self_test())
    print(__doc__)
    sys.exit(2)
