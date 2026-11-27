/*
    OnStepX INDI Driver — Mount status parsers
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "OnStepXStatus.h"

#include <cstring>

// ---------------------------------------------------------------------------
// parseGU — ASCII :GU# reply
//
// Reply format (ref §5.3 of the master plan):
//   A variable-length string of flag characters followed by exactly 3 digits:
//     str[len-3]  = pulse guide rate select (0-9)
//     str[len-2]  = guide rate select (0-9)
//     str[len-1]  = general error code, sent as '0' + code (codes run to 15,
//                   so the character can be past '9')
//
// The flag characters (anywhere in str[0..len-4]) are:
//   n   not tracking          (absence → tracking)
//   N   no goto               (absence → goto active)
//   p   park: unparked
//   I   park: parking
//   P   park: parked
//   F   park: failed
//   H   at home position
//   h   homing in progress
//   B   auto-home at boot
//   S   PPS synced
//   G   pulse guide active
//   g   guide active
//   r   refraction comp (dual if 's' absent, single if 's' present)
//   t   OnTrack comp    (dual if 's' absent, single if 's' present)
//   s   single-axis modifier for r/t
//   (   lunar rate
//   O   solar rate
//   k   King rate
//   w   waiting at home
//   u   pause-at-home enabled
//   z   buzzer enabled
//   a   auto meridian flip
//   e   sync to encoders only
//   R   PEC data recorded
//   /   PEC: ignore
//   ,   PEC: ready to play
//   ~   PEC: playing
//   ;   PEC: ready to record
//   ^   PEC: recording
//   E   mount type GEM
//   K   mount type FORK
//   A   mount type ALTAZM
//   L   mount type ALTALT
//   o   pier side none
//   T   pier side east
//   W   pier side west
// ---------------------------------------------------------------------------
bool OnStepXStatus::parseGU(const char *reply, MountStatus &out)
{
    if (!reply || reply[0] == '\0')
        return false;

    int len = static_cast<int>(strlen(reply));
    // Need at least 3 trailing digits.
    if (len < 3)
        return false;

    // The reply is printable ASCII.  Anything else (e.g. late :Gu# binary
    // bytes after a controller stall) must not be parsed as status flags.
    for (int i = 0; i < len; i++)
        if (static_cast<unsigned char>(reply[i]) < 0x20 || static_cast<unsigned char>(reply[i]) > 0x7E)
            return false;

    out = MountStatus{};    // reset to defaults

    // Parse the 3 trailing digits.
    out.pulseGuideRateSelect = reply[len - 3] - '0';
    out.guideRateSelect      = reply[len - 2] - '0';
    out.errorCode            = reply[len - 1] - '0';

    // Clamp to valid range in case firmware sends unexpected data.
    if (out.pulseGuideRateSelect < 0 || out.pulseGuideRateSelect > 9) out.pulseGuideRateSelect = 0;
    if (out.guideRateSelect      < 0 || out.guideRateSelect      > 9) out.guideRateSelect      = 0;
    // The error code is '0' + code and runs past '9' (ERR_ALT_MAX is 12);
    // GeneralErrors ends at 15.  Anything else is a garbled reply.
    if (out.errorCode            < 0 || out.errorCode            > 15) out.errorCode            = 0;

    // Scan flag region: str[0..len-4] (inclusive).
    // Default: tracking=true, gotoActive=false (set by absence/presence of flags).
    bool notTracking = false;
    bool noGoto      = false;
    bool hasSingle   = false;    // 's' present — modifier for r/t
    bool hasR        = false;    // 'r' refraction comp
    bool hasT        = false;    // 't' OnTrack comp

    int flagLen = len - 3;       // number of flag characters

    for (int i = 0; i < flagLen; i++)
    {
        char c = reply[i];
        switch (c)
        {
            // Tracking / goto
            case 'n':
                notTracking           = true;
                break;
            case 'N':
                noGoto                = true;
                break;

            // Park states — last one encountered wins if multiple present.
            case 'p':
                out.parkState         = MountStatus::ParkState::UNPARKED;
                break;
            case 'I':
                out.parkState         = MountStatus::ParkState::PARKING;
                break;
            case 'P':
                out.parkState         = MountStatus::ParkState::PARKED;
                break;
            case 'F':
                out.parkState         = MountStatus::ParkState::FAILED;
                break;

            // Home
            case 'H':
                out.atHome            = true;
                break;
            case 'h':
                out.homing            = true;
                break;
            case 'B':
                out.autoHomeAtBoot    = true;
                break;

            // Misc
            case 'S':
                out.ppsSynced         = true;
                break;
            case 'G':
                out.pulseGuideActive  = true;
                break;
            case 'g':
                out.guideActive       = true;
                break;
            case 'w':
                out.waitingAtHome     = true;
                break;
            case 'u':
                out.pauseAtHomeEnabled = true;
                break;
            case 'z':
                out.buzzerEnabled     = true;
                break;
            case 'a':
                out.autoMeridianFlip  = true;
                break;
            case 'e':
                out.syncToEncoders    = true;
                break;

            // Tracking compensation modifiers (resolved after scan)
            case 'r':
                hasR                  = true;
                break;
            case 't':
                hasT                  = true;
                break;
            case 's':
                hasSingle             = true;
                break;

            // Track rates
            case '(':
                out.trackRate         = MountStatus::TrackRate::LUNAR;
                break;
            case 'O':
                out.trackRate         = MountStatus::TrackRate::SOLAR;
                break;
            case 'k':
                out.trackRate         = MountStatus::TrackRate::KING;
                break;

            // PEC
            case 'R':
                out.pecRecorded       = true;
                break;
            case '/':
                out.pecState          = MountStatus::PecState::IGNORE;
                break;
            case ',':
                out.pecState          = MountStatus::PecState::READY_PLAY;
                break;
            case '~':
                out.pecState          = MountStatus::PecState::PLAYING;
                break;
            case ';':
                out.pecState          = MountStatus::PecState::READY_RECORD;
                break;
            case '^':
                out.pecState          = MountStatus::PecState::RECORDING;
                break;

            // Mount type
            case 'E':
                out.mountType         = MountStatus::MountType::GEM;
                break;
            case 'K':
                out.mountType         = MountStatus::MountType::FORK;
                break;
            case 'A':
                out.mountType         = MountStatus::MountType::ALTAZM;
                break;
            case 'L':
                out.mountType         = MountStatus::MountType::ALTALT;
                break;

            // Pier side
            case 'o':
                out.pierSide          = MountStatus::PierSide::NONE;
                break;
            case 'T':
                out.pierSide          = MountStatus::PierSide::EAST;
                break;
            case 'W':
                out.pierSide          = MountStatus::PierSide::WEST;
                break;

            default:
                break;
        }
    }

    // Resolve boolean flags
    out.tracking   = !notTracking;
    out.gotoActive = !noGoto;

    // Resolve tracking compensation (r/t + optional s modifier)
    if (hasR)
        out.trackComp = hasSingle ? MountStatus::TrackComp::REFRACTION_SINGLE
                        : MountStatus::TrackComp::REFRACTION_DUAL;
    else if (hasT)
        out.trackComp = hasSingle ? MountStatus::TrackComp::ONTRACK_SINGLE
                        : MountStatus::TrackComp::ONTRACK_DUAL;

    return true;
}

// ---------------------------------------------------------------------------
// parseGu — binary :Gu# reply (9 bytes, all >= 0x80)
//
// Layout taken from OnStepX src/telescope/mount/status/Status.command.cpp
// (:Gu# handler).  Every byte starts as 0x80; flags are ORed in.  Masks below
// are applied to the low 7 bits.
//
//   Byte 0 — tracking / goto / guiding / compensation
//     0x01 not tracking
//     0x02 no goto in progress
//     0x04 PPS synced
//     0x08 pulse guide active
//     0x10 refraction compensation    } 0x40 additionally set when the
//     0x20 OnTrack (model) compensation } compensation is single-axis
//     0x40 single-axis modifier
//
//   Byte 1 — tracking rate / misc (rate only reported when compensation is off)
//     0x03 rate: 0=sidereal, 1=lunar, 2=solar, 3=king
//     0x04 sync to encoders only
//     0x08 guide active
//     0x10 startup authority trusted (not used by the driver)
//
//   Byte 2 — home / meridian flip / buzzer
//     0x01 at home
//     0x02 waiting at home
//     0x04 pause-at-home enabled
//     0x08 buzzer enabled
//     0x10 auto meridian flip
//     0x20 homing in progress
//     0x40 auto-home at boot
//
//   Byte 3 — mount type / pier side
//     0x01 GEM   0x02 FORK   0x04 ALTALT   0x08 ALTAZM
//     0x10 pier side none   0x20 pier side east   0x40 pier side west
//
//   Byte 4 — PEC (only populated when AXIS1_PEC is ON)
//     0x07 state: 0=ignore, 1=ready play, 2=playing, 3=ready record, 4=recording
//     0x40 PEC data recorded
//
//   Byte 5 : park state (0=unparked, 1=parking, 2=parked, 3=failed)
//   Byte 6 : pulse-guide rate select (0-9)
//   Byte 7 : guide rate select       (0-9)
//   Byte 8 : general error code
//
// The firmware frames the reply with a trailing '#', which is not part of
// the 9 bytes passed here.
// ---------------------------------------------------------------------------
bool OnStepXStatus::parseGu(const uint8_t *bytes, int len, MountStatus &out)
{
    if (!bytes || len != 9)
        return false;

    for (int i = 0; i < 9; i++)
        if (bytes[i] < 0x80)
            return false;

    out = MountStatus{};    // reset to defaults

    // Byte 0 — tracking / goto / guiding / compensation
    const uint8_t b0 = bytes[0] & 0x7F;
    out.tracking         = (b0 & 0x01) == 0;
    out.gotoActive       = (b0 & 0x02) == 0;
    out.ppsSynced        = (b0 & 0x04) != 0;
    out.pulseGuideActive = (b0 & 0x08) != 0;
    {
        const bool single = (b0 & 0x40) != 0;
        if (b0 & 0x10)
            out.trackComp = single ? MountStatus::TrackComp::REFRACTION_SINGLE
                            : MountStatus::TrackComp::REFRACTION_DUAL;
        else if (b0 & 0x20)
            out.trackComp = single ? MountStatus::TrackComp::ONTRACK_SINGLE
                            : MountStatus::TrackComp::ONTRACK_DUAL;
        else
            out.trackComp = MountStatus::TrackComp::NONE;
    }

    // Byte 1 — tracking rate / misc
    const uint8_t b1 = bytes[1] & 0x7F;
    switch (b1 & 0x03)
    {
        case 1:
            out.trackRate = MountStatus::TrackRate::LUNAR;
            break;
        case 2:
            out.trackRate = MountStatus::TrackRate::SOLAR;
            break;
        case 3:
            out.trackRate = MountStatus::TrackRate::KING;
            break;
        default:
            out.trackRate = MountStatus::TrackRate::SIDEREAL;
            break;
    }
    out.syncToEncoders = (b1 & 0x04) != 0;
    out.guideActive    = (b1 & 0x08) != 0;

    // Byte 2 — home / meridian flip / buzzer
    const uint8_t b2 = bytes[2] & 0x7F;
    out.atHome             = (b2 & 0x01) != 0;
    out.waitingAtHome      = (b2 & 0x02) != 0;
    out.pauseAtHomeEnabled = (b2 & 0x04) != 0;
    out.buzzerEnabled      = (b2 & 0x08) != 0;
    out.autoMeridianFlip   = (b2 & 0x10) != 0;
    out.homing             = (b2 & 0x20) != 0;
    out.autoHomeAtBoot     = (b2 & 0x40) != 0;

    // Byte 3 — mount type / pier side (one-hot fields)
    const uint8_t b3 = bytes[3] & 0x7F;
    if      (b3 & 0x01) out.mountType = MountStatus::MountType::GEM;
    else if (b3 & 0x02) out.mountType = MountStatus::MountType::FORK;
    else if (b3 & 0x08) out.mountType = MountStatus::MountType::ALTAZM;
    else if (b3 & 0x04) out.mountType = MountStatus::MountType::ALTALT;
    else                out.mountType = MountStatus::MountType::UNKNOWN;

    if      (b3 & 0x20) out.pierSide = MountStatus::PierSide::EAST;
    else if (b3 & 0x40) out.pierSide = MountStatus::PierSide::WEST;
    else                out.pierSide = MountStatus::PierSide::NONE;

    // Byte 4 — PEC
    const uint8_t b4 = bytes[4] & 0x7F;
    out.pecRecorded = (b4 & 0x40) != 0;
    switch (b4 & 0x07)
    {
        case 1:
            out.pecState = MountStatus::PecState::READY_PLAY;
            break;
        case 2:
            out.pecState = MountStatus::PecState::PLAYING;
            break;
        case 3:
            out.pecState = MountStatus::PecState::READY_RECORD;
            break;
        case 4:
            out.pecState = MountStatus::PecState::RECORDING;
            break;
        default:
            out.pecState = MountStatus::PecState::IGNORE;
            break;
    }

    // Byte 5 — park state
    switch (bytes[5] & 0x7F)
    {
        case 1:
            out.parkState = MountStatus::ParkState::PARKING;
            break;
        case 2:
            out.parkState = MountStatus::ParkState::PARKED;
            break;
        case 3:
            out.parkState = MountStatus::ParkState::FAILED;
            break;
        default:
            out.parkState = MountStatus::ParkState::UNPARKED;
            break;
    }

    // Bytes 6-8 — rate selections and error code
    out.pulseGuideRateSelect = bytes[6] & 0x7F;
    out.guideRateSelect      = bytes[7] & 0x7F;
    out.errorCode            = bytes[8] & 0x7F;

    return true;
}

// ---------------------------------------------------------------------------
// gotoErrorText — :MS#/:MN# reply digit, and the n in :CM#'s "En#" reply
// (firmware Goto.command.cpp: '0' + error - CE_SLEW_ERR_BELOW_HORIZON + 1)
// ---------------------------------------------------------------------------
const char *OnStepXStatus::gotoErrorText(char code)
{
    switch (code)
    {
        case '1':
            return "below the horizon limit";
        case '2':
            return "above the overhead limit";
        case '3':
            return "controller in standby";
        case '4':
            return "mount is parked";
        case '5':
            return "goto in progress";
        case '6':
            return "outside limits";
        case '7':
            return "hardware fault";
        case '8':
            return "already in motion";
        case '9':
            return "unspecified error";
        default:
            return "unknown error";
    }
}

// ---------------------------------------------------------------------------
// errorText — general error code (firmware Limits::errorCode(), GeneralErrors)
// ---------------------------------------------------------------------------
const char *OnStepXStatus::errorText(int code)
{
    switch (code)
    {
        case 0:
            return "none";
        case 1:
            return "motor or driver fault";
        case 2:
            return "below the horizon limit";
        case 3:
            return "limit switch triggered";
        case 4:
            return "Dec axis limit reached";
        case 5:
            return "azimuth axis limit reached";
        case 6:
            return "RA axis limit reached";
        case 7:
            return "meridian limit reached";
        case 8:
            return "sync error";
        case 9:
            return "park error";
        case 10:
            return "goto or sync error";
        case 11:
            return "unspecified error";
        case 12:
            return "above the overhead limit";
        case 13:
            return "weather sensor failed to start";
        case 14:
            return "time or location source failed to start";
        case 15:
            return "settings memory (NV) failed to start";
        default:
            return "unknown error";
    }
}

bool OnStepXStatus::errorStopsMotion(int code)
{
    // Limits::errorCode(): these are raised by a limit or fault that stops
    // the mount; 8-11 are never reported and 13-15 are start-up failures.
    return (code >= 1 && code <= 7) || code == 12;
}
