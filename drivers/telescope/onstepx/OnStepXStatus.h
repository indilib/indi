/*
    OnStepX INDI Driver — Mount status struct and parsers (mount binary only)
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

    MountStatus is populated by one of two parsers each ReadScopeStatus cycle;
    both produce equivalent output so ReadScopeStatus() is path-agnostic.

    Protocol (OnStepX v10.24c):
      :GU#  — ASCII status string; each character position encodes a flag or enum.
               Full character map documented in OnStepXStatus.cpp::parseGU().
      :Gu#  — 9-byte binary status (faster).  Bit layout matches the firmware's
               Status.command.cpp; documented in OnStepXStatus.cpp::parseGu().
               Driver falls back to :GU# if the read or parse fails.
*/

#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// MountStatus
// ---------------------------------------------------------------------------
struct MountStatus
{
    enum class TrackComp  { NONE, REFRACTION_SINGLE, REFRACTION_DUAL,
                            ONTRACK_SINGLE, ONTRACK_DUAL
                          };
    enum class TrackRate  { SIDEREAL, LUNAR, SOLAR, KING, CUSTOM };
    enum class ParkState  { UNPARKED, PARKING, PARKED, FAILED };
    enum class PierSide   { NONE, EAST, WEST };
    enum class PecState   { IGNORE, READY_PLAY, PLAYING, READY_RECORD, RECORDING };
    enum class MountType  { GEM, FORK, ALTAZM, ALTALT, UNKNOWN };

    bool      tracking             = false;
    TrackComp trackComp            = TrackComp::NONE;
    TrackRate trackRate            = TrackRate::SIDEREAL;
    bool      gotoActive           = false;
    bool      waitingAtHome        = false;
    bool      pauseAtHomeEnabled   = false;
    ParkState parkState            = ParkState::UNPARKED;
    bool      atHome               = false;
    bool      homing               = false;
    bool      autoHomeAtBoot       = false;
    bool      guideActive          = false;
    bool      pulseGuideActive     = false;
    PierSide  pierSide             = PierSide::NONE;
    bool      pecRecorded          = false;
    PecState  pecState             = PecState::IGNORE;
    MountType mountType            = MountType::UNKNOWN;
    bool      ppsSynced            = false;
    bool      buzzerEnabled        = false;
    bool      autoMeridianFlip     = false;
    bool      syncToEncoders       = false;
    int       pulseGuideRateSelect = 0;
    int       guideRateSelect      = 0;
    int       errorCode            = 0;
};

// ---------------------------------------------------------------------------
// OnStepXStatus — pure static parsers, no INDI base class, fully unit-testable.
// ---------------------------------------------------------------------------
class OnStepXStatus
{
    public:
        // Parse ASCII :GU# reply into out.
        // Returns false if the reply is empty or too short to be valid.
        static bool parseGU(const char *reply, MountStatus &out);

        // Parse binary :Gu# reply (exactly 9 bytes, all >= 0x80) into out.
        // Returns false if len != 9 or any byte < 0x80.
        static bool parseGu(const uint8_t *bytes, int len, MountStatus &out);

        // Full :Gu# reply as read from the wire: 9 status bytes + '#'.
        // Read all of it so the '#' is not left behind for the next command.
        static constexpr int GU_REPLY_LEN = 10;

        // Validate the trailing '#' and parse the 9 status bytes.
        // Text for a goto/sync/flip error digit ('1'..'9', as :MS# replies).
        static const char *gotoErrorText(char code);

        // Text for MountStatus::errorCode (the firmware's general error).
        static const char *errorText(int code);

        // True for errors raised by a limit or fault that stops the mount.
        static bool errorStopsMotion(int code);

        static bool parseGuReply(const uint8_t *bytes, int len, MountStatus &out)
        {
            if (!bytes || len != GU_REPLY_LEN || bytes[GU_REPLY_LEN - 1] != '#')
                return false;
            return parseGu(bytes, GU_REPLY_LEN - 1, out);
        }
};
