/*
    Gemini Power Box Hub Advanced v3 INDI Driver

    Copyright (C) 2026 Dieter R Kedrowitsch <dieter@kedrowitsch.net>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA  02111-1307, USA.

    The full GNU General Public License is included in this distribution in the
    file called LICENSE.
*/

#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace geminipbh
{

struct FirmwareInfo
{
    std::string raw;
    std::optional<int> numeric;
};

enum class ProbeStatus
{
    NotTried,
    OpenFailed,
    PortBusy,
    IdentityTimeout,
    IdentityMismatch,
    IdentityMatched,
    FirmwareTimeout,
    FirmwareMalformed,
    ProtocolError,
    Identified,
    Cancelled
};

struct ProbeResult
{
    ProbeStatus status = ProbeStatus::NotTried;

    std::string requestedPort;
    std::string canonicalPort;
    std::string preferredPort;

    std::string identity;
    FirmwareInfo firmware;

    std::string failureReason;
};

struct DiscoveryOptions
{
    std::vector<std::string> candidatePorts;
    std::function<bool()> shouldCancel;
    std::chrono::milliseconds settleDelay{100};
    std::chrono::milliseconds identityTimeout{1000};
    std::chrono::milliseconds firmwareTimeout{1000};
};

struct DiscoveryResult
{
    std::vector<ProbeResult> probes;
    std::vector<ProbeResult> devices;
    bool cancelled = false;
};

std::vector<std::string> enumerateSerialCandidates();
ProbeResult probeGeminiDevice(const std::string &port, const DiscoveryOptions &options = DiscoveryOptions{});
DiscoveryResult discoverGeminiDevices(const DiscoveryOptions &options = DiscoveryOptions{});
const char *probeStatusName(ProbeStatus status);

#ifdef GEMINI_TESTING
std::vector<std::string> enumerateSerialCandidatesForTesting(const std::string &devDirectory,
        const std::string &byIdDirectory,
        const std::string &byPathDirectory);
#endif

} // namespace geminipbh
