// SPDX-FileCopyrightText: 2026 Matteo Beretta
// SPDX-License-Identifier: MIT

// Wheelly - shared definition of the serial protocol.
//
// BOTH sides include this file: the firmware on the XIAO ESP32-S3 and the INDI
// driver on the Raspberry. It is the best idea stolen from frankenwheely, the
// abandoned project this one started from: if commands and fields are defined
// only once and compiled by both sides, the class of bugs "the driver and the
// firmware do not agree on what this message means" cannot exist by
// construction.
//
// Rules for whoever changes it:
//
//   - No Arduino, no INDI, no C++ beyond C++11. It must compile as it is on a
//     microcontroller and on a PC.
//   - The strings in here are IDENTIFIERS, not prose: they are in English and
//     are never translated. The texts for the user live in the translation
//     catalogues, on the host side. See firmware.md, section 13.
//   - Changing a word here means breaking compatibility: PROTOCOL_VERSION goes
//     up and the driver refuses the firmwares it does not know.
//   - This file exists TWICE, byte for byte: firmware/wheelly/ (the source,
//     edit that one) and driver/indi-wheelly/ (a copy, so the driver folder
//     builds on its own inside INDI's tree, where firmware/ does not exist).
//     After editing, copy it over:
//         cp firmware/wheelly/wheelly_protocol.h driver/indi-wheelly/
//     firmware/test/test_driver_standalone.py fails while the two differ.
//
// The reasoned documentation of the protocol is in firmware.md, section 4.

#ifndef WHEELLY_PROTOCOL_H
#define WHEELLY_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

namespace wheelly
{

// ----------------------------------------------------------------- version

// It goes up at every incompatible change: a word that changes, a field that
// disappears, the meaning of a value that shifts. Adding a new field is NOT
// incompatible - that is exactly why the replies are made of named fields
// instead of positional ones.
//
// 2: the rotation trim went (`offset`, `offsets`,
// `clear-offsets`, the `o<n>=` fields) and `angle <n> <degrees>` came, so
// `target` is now the slot's angle alone instead of angle + trim. A pairing
// across the change would misunderstand itself in silence: a new driver on an
// old firmware would show the angles while the wheel went to angle + trim,
// and an old driver on a new firmware would ask for trims that no longer
// exist. The driver refuses such a pairing at connection, with the two
// numbers in the message: reflash the wheel and install the driver together.
const int PROTOCOL_VERSION = 2;

// ------------------------------------------------------------------ limits

// Mind the names chosen here: the preprocessor knows nothing of namespaces, and
// this file is included after half the system library. LINE_MAX, NAME_MAX,
// PATH_MAX and company are already macros of <limits.h>, which on Arduino comes
// along by itself with Arduino.h: calling a constant LINE_MAX made the build
// for the XIAO fail with a message that did not even name the conflict. On the
// Mac it went through. Hence the rule: names ending in _MAX get a prefix of our
// own.
// How many slots a wheel can have, at most: it is the size of the arrays, not
// the number of slots of THIS wheel. How many it really has is known by the
// wheel, it lives in its memory and is read with the 'slots' command.
//
// The distinction matters: the firmware is only one and serves different
// wheels, and the driver must take nothing for granted. The alternative - a
// Wheelly5 driver, a Wheelly7 one and so on - would force whoever installs it
// to know how many slots their wheel has in order to pick the right entry,
// i.e. precisely the information the wheel can already tell by itself.
const int MAX_SLOTS = 12;
// At least two: a wheel with one slot never changes filter. When the firmware
// accepted 1 while the INDI panel started from 2, `slots 1` typed at the
// serial monitor left a wheel the panel could not show; so 1 is refused. One
// number here, for firmware, simulator and driver.
const int MIN_SLOTS = 2;
const int FACTORY_SLOTS = 5;         // how many a brand-new wheel has

const size_t FILTER_NAME_MAX = 32;       // see firmware.md, section 6
// The longest line is the reply to 'names': twelve names of 32 characters plus
// the labels. If the buffer cannot hold it, the firmware truncates a reply and
// the driver reads a mangled name.
const size_t WHEELLY_LINE_MAX = 512;

// ------------------------------------------------- how a line begins

const char PREFIX_OK[] = "ok";           // positive outcome, with its fields
const char PREFIX_ERROR[] = "error";     // negative outcome: code, fields, text
const char PREFIX_COMMENT = '#';         // informative, never necessary
const char PREFIX_EVENT = '!';           // spontaneous event, not requested

// ---------------------------------------------------------------- commands

const char CMD_VERSION[] = "version";           // identification and handshake
const char CMD_STATUS[] = "status";             // the truth, always available
const char CMD_GO[] = "go";                     // go <1..slots>, asynchronous
const char CMD_STOP[] = "stop";                 // interrupts and releases
// jog <degrees>: move the disc TO the angle read now plus <degrees>, with the
// same closed loop as `go` - approach legs, the learnt play of the tyre - and
// the same events (it serves the jog buttons of the calibration tab). Not
// raw steps: the TPU tyre swallows ~0.6 degrees before the disc
// moves, so 0.05 degrees of steps would move nothing. See Wheel::jog().
const char CMD_JOG[] = "jog";
// The largest jog, either way: HALF A TURN. The panel's biggest jog button is
// one slot pitch, 360/slots degrees - 72 on a five-slot wheel - and a wheel may
// have as few as MIN_SLOTS = 2 slots, whose pitch is 180. (It was 30 while the
// biggest button was 10 degrees.) A jog of
// exactly half a turn has no shorter way: it goes the way its sign says (see
// Wheel::start_leg), not whichever way the rounding of the target falls.
const int JOG_MAX_DEG = 180;
const char CMD_ANGLES[] = "angles";             // the calibration angles
// angle <n> <degrees>: sets slot n's angle, 0..ANGLE_MAX_DEG, in working
// memory until `save` (the calibration angles are editable in the panel).
// It does not move the wheel: the driver sends `go`
// after it when it wants to, and a serial monitor does the same by hand.
// Refused while the wheel moves, like `teach`: the move under way aims at
// the old angle.
const char CMD_ANGLE[] = "angle";
const int ANGLE_MAX_DEG = 360;
// THERE IS NO ROTATION TRIM (removed in protocol 2). Before, every slot had
// two numbers, the taught angle and a trim of +-45 degrees on top of
// it (`offset <n> <degrees>`, `offsets`, `clear-offsets`, NVS keys o1..o12),
// and the wheel went to angle + trim. Why it went: firmware.md, "One number
// per slot". A wheel that saved trims before gets them folded into its
// angles once, at the first start of this firmware (Wheel::load_or_default).
const char CMD_NAMES[] = "names";               // all the names
const char CMD_NAME[] = "name";                 // name <n> <text>
const char CMD_TEACH[] = "teach";               // teach <n>: "this one is n"
const char CMD_SAVE[] = "save";                 // writes to NVS, an explicit gesture
const char CMD_TOLERANCE[] = "tolerance";       // tolerance [good warn retries]
const char CMD_MOTOR[] = "motor";               // motor [mA [speed [accel]]]

// The limits of the motor settings, in one place for the wheel that checks
// them and the INDI panel that offers them (when the panel kept its own
// numbers, speed and acceleration never reached the wheel - "motor" took only
// mA).
// Current in mA; speed in steps/s; acceleration in steps/s^2.
const unsigned MOTOR_MA_MAX = 800;
const unsigned long MOTOR_SPEED_MAX = 5000;
const unsigned long MOTOR_ACCEL_MAX = 50000;
// The factory speed and acceleration, in full steps: 300 steps/s and 1200
// steps/s^2 (measured on the assembled reference wheel; 200/800 before).
// Slower is not gentler: at 50 steps/s the motor stalled and vibrated against
// the notches of the detent (since removed) even the easy way, while 100, 150,
// 200, 300, 400, 700 and 1000 all arrived, 1-2.5 s per slot in 2-4 legs, with
// no trend in the final error (400 mA, one way down). The motor's pull-out
// curve (datasheet: 24 V, 0.4 A, half step) starts only at 500 half steps/s =
// 75 rpm = 250 full steps/s: below that is a weak zone the datasheet does not
// even draw, and the wheel stalls there. 300/1200 sits just above it.
// One place for the wheel, the simulator and the panel's starting value.
const unsigned long FACTORY_SPEED = 300;
const unsigned long FACTORY_ACCEL = 1200;
// The factory run current, in mA. 350 was DECIDED ON THE PART, not
// calculated: the clutch was tried by hand from 150 to 350 in steps, and 350
// is where it stopped slipping (the motor is rated 400 per phase). With the
// manual wheel's detent in place 400 was needed to climb out of a notch; with
// the detent removed, the speed test (firmware/speed_test.py: 100..1000 full
// steps/s, every move arrived in 2 legs) ran at 350 again. Defined here so the
// wheel, the simulator and the panel's starting value are one number: when
// each wrote its own 350, they could drift apart.
const unsigned FACTORY_RUN_MA = 350;
// THE BOOST: after a leg that STALLED - it made
// (almost) no progress, judged on the angle read, since the TMC2208 has no
// StallGuard - the next leg runs at twice the speed and the acceleration, to
// break out of the notch, and the legs after it go back to the set speed.
// Never above this, nor above MOTOR_SPEED_MAX / MOTOR_ACCEL_MAX.
const unsigned long BOOST_SPEED_MAX = 1000;

// The limits of `tolerance good warn retries`: 0 < good <= warn <= this many
// degrees, and 0..this many retries. Here and not in wheel.cpp because the
// refusal quotes them back in `expected=` and the simulator must quote the
// same string: when each wrote its own, they differed.
const int TOLERANCE_MAX_DEG = 45;
const int RETRIES_MAX = 20;
// The FACTORY tolerances, in degrees of disc: good (a plain success) and
// warn (beyond it the wheel retries; also the "on a slot" band and the drift
// threshold). One place for the wheel, the simulator and the panel.
// How the values were reached:
//   - 0.10 / 0.50, proposed before the wheel was assembled: REJECTED. The
//     last ~0.6-1.0 degrees of a leg are swallowed by the TPU tyre, so 0.10
//     forced legs and retries chasing an error the drive cannot resolve.
//     (1 degree of disc moves a filter about 0.9 mm at a filter circle radius
//     of ~50 mm - ESTIMATE, not measured.)
//   - 0.50 / 1.50: REJECTED as too loose. The speed test
//     (firmware/speed_test.py), on the reference wheel with its detent
//     removed, arrived with errors below 0.15 degrees on four slots out of
//     five at every speed.
//   - 0.30 / 0.80: 0.30 keeps twice that margin; 0.80 still lets a leg the
//     tyre swallowed (up to ~0.6 degrees) end as a warning rather than a
//     retry.
// A wheel that already SAVED its tolerances keeps them: they live in NVS, and
// nothing migrates.
const float FACTORY_GOOD_DEG = 0.3f;
const float FACTORY_WARN_DEG = 0.8f;
const char CMD_HOLD[] = "hold";                 // hold [mA], 0 = release
const char CMD_LED[] = "led";                   // led [on|pulse|off|test]
const char CMD_DIAG[] = "diag";                 // is it talking to the hardware?
const char CMD_SLOTS[] = "slots";               // slots [n]: how many slots it has
// There is NO `detent` command any more. `detent [yes|no]` used to declare
// whether the wheel had a physical detent holding it at rest; why it went:
// "NO DETENT OPTION", below.
// An older driver that still sends it gets ERR_UNKNOWN_COMMAND: its panel
// switch goes to Alert and nothing else happens - acceptable, the switch
// changed no behaviour of the wheel, only warnings.

// direction [shortest|up|down]: which way the wheel may turn. Born for a
// wheel with an ASYMMETRIC detent (285 mN*m on the steep flank, firmware.md
// 2.4): going the shortest way, half the moves climbed the steep flank and the
// motor stalled, humming. "up" turns only towards increasing angles, "down"
// only towards decreasing ones - every move AND every retry, so a retry after
// an overshoot goes round the whole turn instead of backing up against the
// detent. "shortest" is the factory value (see FACTORY_DIRECTION): the
// one-way options stay for a mechanism that turns more easily one way than
// the other. The reply carries the move time cap the wheel
// derives from it (F_CEILING), and `save` keeps it.
const char CMD_DIRECTION[] = "direction";
const char ARG_SHORTEST[] = "shortest";
const char ARG_UP[] = "up";
const char ARG_DOWN[] = "down";
// The factory direction: THE SHORTEST WAY. "down" (one way only) was the
// factory value while the reference wheel still had an ASYMMETRIC detent that
// stalled the motor going up (at about 51, 123, 195, 267, 339 degrees). With
// the detent removed, the speed test arrived every time in 2 legs the
// shortest way: a move is at most half a turn instead of nearly two, and the
// move time cap drops from 15.8 to 12.1 s at the factory speed. The detent is now always
// removed (see NO DETENT OPTION); "up" and "down" stay for a mechanism that
// is stiffer one way than the other, and cost nothing to keep. One constant for the firmware, the simulator and the driver's panel.
const char FACTORY_DIRECTION[] = "shortest";

// NO DETENT OPTION. The wheel used to carry a flag, `detent yes|no` (FACTORY_DETENT = yes), saying whether the manual
// wheel still had its spring click stop. It went because the answer is always
// "no": the motor drives the disc by FRICTION, through the clutch, and it
// cannot climb out of the detent's notches - on the reference wheel it
// stalled and hummed against the steep flank (285 mN*m) and needed 400 mA, the motor's
// rating, to get over the easy one. So the detent is ALWAYS removed when the
// magnet is fitted (assembly guide, chapter 10), and a flag with a single
// right value is not a setting. The flag decided only warnings ("no detent
// and no holding current"), and with the detent always gone that one would
// fire at every connection of every wheel, whose holding current is 0 by
// default: a warning that always sounds is no longer read. What stays is the
// drift watch (EV_DRIFT), which reports a wheel that really moves at rest,
// and its advice is the holding current. A wheel that saved the flag before
// keeps an NVS key "detent" nobody reads any more: harmless, left alone.

// THE DRIVE RATIO IS A SETTING OF THE WHEEL, not a constant of the
// firmware. The motor drives the disc by friction: a TPU tyre on the
// clutch presses on the disc's knurled rim, and how many degrees of motor make
// one degree of disc depends on the diameter the tyre really rolls on - its
// squash under the spring, the knurl's depth - which differs from one wheel to
// the next and is not the drawing's. Project rule: what depends on one
// wheel's mechanics is an option with a default.
//
// The DEFAULT is the model's, DERIVED here from the model's two diameters and
// not typed as 2.9: the disc (Disc_rotating in the CAD's parameters.py,
// MEASURED with a caliper on the reference wheel) and the clutch's contact
// diameter (D_clutch, SET by the design). test_units.py reads the CAD's
// values and fails if these two drift from them. The ratio stays "declared
// from the model" until measured on the wheel with firmware/ratio_test.py.
const float MODEL_DISC_DIAMETER_MM = 145.0f;
const float MODEL_CLUTCH_DIAMETER_MM = 50.0f;
const float FACTORY_DRIVE_RATIO = MODEL_DISC_DIAMETER_MM / MODEL_CLUTCH_DIAMETER_MM;
// ratio [<value>]: reads or sets the ratio the wheel STARTS from (the
// "base"), in working memory until `save`; setting it restarts the learning
// from it. ratio learn on|off: whether the wheel refines the ratio in use from
// its own long legs (Wheel::judge - on by default, kept by `save`). The range only catches a typo (29 for 2.9): a wheel built with
// another clutch or disc may be far from 2.9, and 1..10 is wider than any.
const char CMD_RATIO[] = "ratio";
const char ARG_LEARN[] = "learn";
const float DRIVE_RATIO_MIN = 1.0f;
const float DRIVE_RATIO_MAX = 10.0f;
// legs on|off: a `! leg` event at the end of every leg, for a host that measures the ratio (firmware/ratio_test.py). OFF at every start
// and never saved: nobody but a measuring tool wants a line per leg, and a
// driver that does not know the event would only log it at debug level.
const char CMD_LEGS[] = "legs";
const char F_REPORT[] = "report";        // legs: on or off

// Ekos gives up on a filter change after 30 s by itself (firmware.md 5.3).
// The wheel's move time cap is derived from speed and direction, and the
// driver warns when the worst case would go past this.
const unsigned long EKOS_FILTER_TIMEOUT_MS = 30000;

// The value the project RECOMMENDS to whoever switches on the holding current
// at rest. It is not a firmware default - the default is zero, i.e. the motor
// is released, because a wheel whose disc turns stiffly enough stays where it
// is left - but the number to start from for whoever's wheel drifts at rest
// (the drift watch says so).
//
// It is not the 350 of the run current, and the reason is that the two numbers
// are not chosen by the same criterion: A MOVE LASTS TWO SECONDS, HOLDING LASTS
// ALL NIGHT. At 350 mA it would be 7.4 W forever, a few centimetres from a
// magnetic sensor and a cooled camera; at 150 it is 1.35 W.
const uint16_t RECOMMENDED_HOLD_MA = 150;

// settle [ms]: how long the motor stays energised, AT THE RUN CURRENT, after
// every leg stops, before the angle is read for the verdict and - on the last
// leg - the motor is released. Also with the holding
// current at rest at 0: the drive is by friction, and an energised motor that
// has stopped holds the TPU tyre, which brakes the disc; released at once, the
// disc's inertia could carry it on. The verdict is read at the END of the
// wait, so a disc that went on moving during it is judged where it stopped,
// and the normal approach legs and retries apply.
// It used to be a fixed 300 ms (SETTLE_MS in wheel.h), already energised; it
// is a setting because a wheel with a heavier disc, or a softer tyre, may want
// longer, and the run current is GUARANTEED for all of it. A fixed wait held
// at the run current only by accident: the TMC2208 drops to the hold current
// by itself 0.44 s after the last step (TPOWERDOWN = 20 by default), so a
// longer wait, with the hold at 0, would let the coils freewheel half way
// through.
// 0 = read at once and release at once; the readings are then taken with the
// disc still ringing, and the approach legs pay for it. The reply carries the
// move time cap (F_CEILING), which counts this wait once per leg; `save`
// keeps it. One constant for the firmware, the simulator and the panel.
const char CMD_SETTLE[] = "settle";
const unsigned FACTORY_SETTLE_MS = 300;
const unsigned SETTLE_MS_MAX = 2000;

// arguments of 'led'
const char ARG_ON[] = "on";
const char ARG_OFF[] = "off";
const char ARG_TEST[] = "test";                 // Wheelly in Morse code
// Off at rest, pulsing while the wheel goes to another slot: the default.
// "off" is then OFF for good - no pulse either - for
// whoever wants no light at all near the optics.
const char ARG_PULSE[] = "pulse";

// ------------------------------------------------------------------ fields

// status
const char F_POS[] = "pos";              // 1..slots, or 0 = between two slots
const char F_ANGLE[] = "angle";          // degrees read from the AS5600
const char F_TARGET[] = "target";        // angle[pos] (+ offset[pos] until protocol 1)
const char F_ERR[] = "err";              // residual error in degrees, signed
const char F_MOTION[] = "motion";        // see MOTION_* below
const char F_RETRIES[] = "retries";
// Every leg of the last (or current) positioning, the first included, in
// 'status'. One way the wheel aims short and creeps in, and the
// approach legs are not retries: this is where they are seen.
const char F_LEGS[] = "legs";
// How many legs of the last (or current) positioning ran at the boosted
// speed after a stall, in 'status'.
const char F_BOOSTS[] = "boosts";
// The verdict of the last positioning: "none" if none has been concluded yet,
// otherwise one of the event names below.
//
// It exists because 'status' must be enough on its own. Without it, the driver
// would have had to compare the residual error with the tolerances to tell
// whether the move had succeeded or was only passable - i.e. redo the decision
// that by design belongs to the firmware, in a second copy that diverges over
// time. The same vocabulary as the events, on purpose: one thing, said in two
// ways.
const char F_OUTCOME[] = "outcome";
const char OUTCOME_NONE[] = "none";
const char F_FROM[] = "from";            // starting angle, in reply to 'go'
const char F_DIR[] = "dir";              // chosen direction: "+" or "-"

// sensor
const char F_AGC[] = "agc";
const char F_MAG[] = "mag";              // magnitude: IT is the criterion, not the AGC
const char F_MD[] = "md";                // magnet detected
const char F_ML[] = "ml";                // field too weak
const char F_MH[] = "mh";                // field too strong

// identification
const char F_NAME[] = "name";            // "wheelly": it is the real handshake
const char F_FW[] = "fw";
const char F_PROTO[] = "proto";
const char F_SERIAL[] = "serial";
// How many slots the wheel has. The driver does NOT take it for granted: it
// asks. Hard-wiring it on the other side would mean that a seven-slot wheel
// shows twelve boxes in Ekos, or five, and neither is true.
const char F_SLOTS[] = "slots";

// preferences
const char F_GOOD[] = "good";            // good tolerance, in degrees
const char F_WARN[] = "warn";            // warning tolerance, in degrees
const char F_MA[] = "ma";                // current, in milliampere
const char F_MS[] = "ms";                // settle: the wait after a leg, in ms
const char F_SPEED[] = "speed";
const char F_ACCEL[] = "accel";
const char F_DIRECTION[] = "direction";  // shortest, up or down
// The cap on a whole positioning, in ms, as the wheel derives it from speed,
// acceleration, retries and direction: in the replies of `direction` and
// `motor`, so the driver's own safety net follows the wheel's number.
const char F_CEILING[] = "ceiling";
const char F_STATE[] = "state";          // state of the LED
const char F_ENABLED[] = "enabled";
const char F_MODE[] = "mode";            // LED mode: on, pulse, off
const char F_SAVED[] = "saved";
const char F_DURATION[] = "duration";
// ratio: the one IN USE (the base refined by the learning, or the base alone
// with learning off), the base it started from, the factory value, whether
// it learns, and how many legs it has learnt from since the base was set
const char F_RATIO[] = "ratio";
const char F_BASE[] = "base";
const char F_FACTORY[] = "factory";
const char F_LEARN[] = "learn";
const char F_LEARNT[] = "learnt";

// the `! leg` event: what one leg was sent and what the disc did
const char F_N[] = "n";                  // the leg's number in the positioning, from 1
const char F_KIND[] = "kind";            // first, approach or retry
const char KIND_FIRST[] = "first";
const char KIND_APPROACH[] = "approach";
const char KIND_RETRY[] = "retry";
const char F_JOG[] = "jog";              // yes if the positioning is a jog
const char F_STEPS[] = "steps";          // microsteps sent, signed
const char F_MOTOR[] = "motor";          // the same as motor degrees, signed
const char F_AIM[] = "aim";              // disc degrees it wanted, lost motion not included
const char F_TO[] = "to";                // the angle read after settling (from= is before)
const char F_MOVED[] = "moved";          // disc degrees the way it was sent; negative = back
const char F_LONG[] = "long";            // yes: long enough to tell the ratio (Wheel::judge)

// parameters of the errors
const char F_EXPECTED[] = "expected";
const char F_GOT[] = "got";

// boolean values on the wire: never 0/1 where a human has to read
const char V_YES[] = "yes";
const char V_NO[] = "no";

// values of the 'motion' field
const char MOTION_IDLE[] = "idle";
const char MOTION_MOVING[] = "moving";
const char MOTION_SETTLING[] = "settling";   // stopped but not yet settled
const char MOTION_FAILED[] = "failed";

// ------------------------------------------------------------------ events

const char EV_ARRIVED[] = "arrived";     // within the good tolerance
const char EV_WARNING[] = "warning";     // between good and warning: report and go on
const char EV_FAILED[] = "failed";       // outside, even after the retries
const char EV_SENSOR[] = "sensor";       // the magnet can no longer be seen
// The wheel moved WHILE AT REST beyond the warning tolerance: nobody asked it
// anything and it moved all the same. It is the sign that holding current at
// rest is needed (RECOMMENDED_HOLD_MA): the detent of the manual wheel is
// always removed, so the drift watch is the only thing that
// says whether a released motor is enough.
//
// ADDING an event does not raise PROTOCOL_VERSION, and it is worth saying why:
// the version is there to refuse the pairings that would misunderstand each
// other, and here there are none. An old driver with a new firmware receives an
// event it does not know and ignores it; a new driver with an old firmware
// never receives it. CHANGING a word already in use would be another matter,
// and that is what the rule at the top of the file forbids.
const char EV_DRIFT[] = "drift";         // it moved by itself, at rest
// The end of one leg, only after `legs on`: what was sent, what
// the disc did, and whether the wheel learnt its ratio from it. Sent BEFORE
// the verdict's event when the leg is the last one. It is for measuring
// tools, not for the driver, and it is opt-in so that a driver that does not
// know it never sees it; one that did would only log it at debug level.
const char EV_LEG[] = "leg";
// The motor driver had to be set up again before a leg or in `diag`
// (mechanics.h, "is the driver still ours"): reason=power when it had been
// silent and now answers - the 12 V arrived after the XIAO had started, which
// is the order the assembly guide gives - and reason=reset when it answered
// with its setup gone - the 12 V dropped and came back. Said once per
// setup, not per check. With the driver silent a move is refused with
// ERR_DRIVER_SILENT instead: a leg sent to an unpowered or reset driver
// moved the disc a fraction of the way, or not at all.
const char EV_DRIVER[] = "driver";
const char V_POWER[] = "power";
const char V_RESET[] = "reset";

// ------------------------------------------------------------------ errors

// The code is two things at once: the outcome for the machine and the
// TRANSLATION KEY for the host. The named fields that come with it are the
// parameters the driver builds the sentence from, in the user's language. The
// English text at the end of the line is only for whoever watches the raw
// serial.
enum Error
{
    ERR_NONE = 0,
    ERR_UNKNOWN_COMMAND = 1,
    ERR_BAD_ARGUMENTS = 2,       // missing, too many, or not numeric
    ERR_OUT_OF_RANGE = 3,        // with expected= and got=
    ERR_SENSOR_SILENT = 4,       // the AS5600 does not answer on I2C
    ERR_NO_MAGNET = 5,           // it answers, but does not see the magnet
    ERR_DRIVER_SILENT = 6,       // the TMC2208 (or 2209) does not answer on the UART
    ERR_NOT_NOW = 7,             // for instance 'teach' while the wheel is moving
    ERR_NVS_WRITE = 8,
    ERR_BAD_FILTER_NAME = 9      // with reason= among the NameCheck values
};

const char F_REASON[] = "reason";        // with ERR_BAD_FILTER_NAME, and with EV_DRIVER

// ------------------------------------------------- validation of filter names
//
// Why this rule exists, in short: the filter name ends up as it is in the
// FILTER keyword of the FITS header, which the standard allows only in
// printable ASCII, and in the file name of the frame - where Ekos, unlike what
// it does with every other field, does NOT sanitise it. A character out of
// place gives no error: it silently changes the name and breaks the pairing
// between lights and flats. The full reasoning is in firmware.md, section 6.
//
// It lives here, and not on one side only, because it must hold identically in
// the firmware (command 'name'), in the INDI driver (FILTER_NAME) and one day on
// the ASCOM side.

enum NameCheck
{
    NAME_OK = 0,
    NAME_EMPTY = 1,
    NAME_TOO_LONG = 2,
    NAME_BAD_EDGE = 3,       // does not begin or end with a letter or digit
    NAME_BAD_CHAR = 4,       // outside [A-Za-z0-9._-]
    NAME_RESERVED = 5        // Windows device name: NUL, COM1, ...
};

inline bool name_is_alnum(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

inline bool name_is_allowed(char c)
{
    return name_is_alnum(c) || c == '_' || c == '-' || c == '.';
}

inline char name_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

// Case-insensitive comparison. Needed twice: for the reserved Windows names and
// for uniqueness across the slots.
inline bool name_equal_nocase(const char *a, const char *b)
{
    size_t i = 0;
    for (; a[i] != '\0' && b[i] != '\0'; i++)
    {
        if (name_lower(a[i]) != name_lower(b[i])) return false;
    }
    return a[i] == '\0' && b[i] == '\0';
}

// CON, PRN, AUX, NUL, COM1..COM9, LPT1..LPT9 are device names on Windows, and
// they stay so even with an extension stuck on: "NUL.fits" opens the device,
// not a file. It would look like paranoia for a filter name, were it not that
// in Ekos the filter placeholder is ALSO a folder name.
inline bool name_is_reserved(const char *s)
{
    // both the whole name and the part before the first dot are compared
    char stem[16];
    size_t n = 0;
    while (s[n] != '\0' && s[n] != '.' && n < sizeof(stem) - 1)
    {
        stem[n] = s[n];
        n++;
    }
    stem[n] = '\0';
    if (s[n] != '\0' && s[n] != '.') return false;  // too long to be one

    static const char *reserved[] =
    {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };
    for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++)
    {
        if (name_equal_nocase(stem, reserved[i])) return true;
    }
    return false;
}

// Returns NAME_OK, or the reason for the refusal. It REFUSES, it does not
// silently correct: every layer downstream (cfitsio, astropy, Ekos, the
// filesystem) corrects in its own way, and if we corrected too the user would
// find the same filter with three different names in three places without
// knowing which one is the good one.
inline NameCheck check_filter_name(const char *s)
{
    if (s == NULL || s[0] == '\0') return NAME_EMPTY;

    // The length is checked BEFORE the characters, and the order is not
    // indifferent: the reason for the refusal is shown to the user, and for a
    // name that is both too long and full of odd characters "too long" is the
    // useful information. Mixing the two checks in a single loop gave a reason
    // that depended on where the first bad character happened to be - and made
    // this rule diverge from its Python copy in the simulator.
    size_t n = 0;
    while (s[n] != '\0')
    {
        n++;
        if (n > FILTER_NAME_MAX) return NAME_TOO_LONG;
    }

    for (size_t i = 0; i < n; i++)
    {
        if (!name_is_allowed(s[i])) return NAME_BAD_CHAR;
    }

    if (!name_is_alnum(s[0]) || !name_is_alnum(s[n - 1])) return NAME_BAD_EDGE;
    if (name_is_reserved(s)) return NAME_RESERVED;
    return NAME_OK;
}

// A safety net, not a policy. Validation on input refuses, but the driver is
// not the only source: a third-party INDI client can write FILTER_NAME going
// around our interface, and the NVS may hold names saved before the rule
// existed. So the point where the file name is built and the one where the FITS
// keyword is written both go through here.
//
// A guarantee the rest of the code relies on, and the tests check it: the
// output ALWAYS passes check_filter_name(), whatever the input.
inline void sanitize_filter_name(const char *in, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) return;

    size_t w = 0;
    const size_t limit = (out_size - 1 < FILTER_NAME_MAX) ? out_size - 1 : FILTER_NAME_MAX;

    // Alphanumerics are copied; every run of non-alphanumeric characters
    // becomes ONE separator. So leading and trailing separators disappear by
    // themselves - which is what is needed, because the first and the last
    // character must be alphanumeric - and "R (Astrodon)" becomes "R_Astrodon"
    // instead of "R__Astrodon_". A separator that is already allowed is kept as
    // it is, so "O-III" and "1.25" are not mangled for no reason.
    bool separator_pending = false;
    char separator = '_';

    if (in != NULL)
    {
        for (size_t i = 0; in[i] != '\0' && w < limit; i++)
        {
            if (name_is_alnum(in[i]))
            {
                if (separator_pending && w > 0 && w < limit) out[w++] = separator;
                separator_pending = false;
                if (w < limit) out[w++] = in[i];
            }
            else if (!separator_pending)
            {
                separator_pending = true;
                separator = name_is_allowed(in[i]) ? in[i] : '_';
            }
        }
    }
    // if the truncation left a trailing separator, drop it
    while (w > 0 && !name_is_alnum(out[w - 1])) w--;
    out[w] = '\0';

    // if nothing usable is left, a neutral but valid name
    if (w == 0)
    {
        const char fallback[] = "Filter";
        size_t i = 0;
        while (fallback[i] != '\0' && i + 1 < out_size)
        {
            out[i] = fallback[i];
            i++;
        }
        out[i] = '\0';
        w = i;
    }

    // A reserved Windows name is defused with a digit, which has to go at the
    // end of the STEM and not at the end of the name: "nul.fits" stays reserved
    // even written "nul.fits0", because Windows looks at the part before the dot.
    if (name_is_reserved(out) && w + 1 <= limit)
    {
        size_t stem = 0;
        while (out[stem] != '\0' && out[stem] != '.') stem++;
        for (size_t i = w; i > stem; i--) out[i] = out[i - 1];
        out[stem] = '0';
        out[w + 1] = '\0';
    }
}

// Uniqueness has to be checked at the level of the wheel, not of the single
// name: "Ha" and "ha" are two different names for FITS and one single folder on
// Windows and on macOS. Two slots that collapse into the same folder are a
// silent disaster. Returns -1 if all is well, otherwise the index of the first
// name that duplicates an earlier one.
inline int first_duplicate_name(const char *const *names, int count)
{
    for (int i = 1; i < count; i++)
    {
        for (int j = 0; j < i; j++)
        {
            if (names[i] != NULL && names[j] != NULL &&
                    name_equal_nocase(names[i], names[j]))
            {
                return i;
            }
        }
    }
    return -1;
}

}  // namespace wheelly

#endif  // WHEELLY_PROTOCOL_H
