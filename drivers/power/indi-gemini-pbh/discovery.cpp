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

#include "indi-gemini-pbh/discovery.h"

#include "indi-gemini-pbh/protocol.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <limits.h>
#include <string>
#include <thread>
#include <utility>
#include <unistd.h>
#include <vector>

namespace geminipbh
{

namespace
{
constexpr const char *kDevDirectory = "/dev";
constexpr const char *kByIdDirectory = "/dev/serial/by-id";
constexpr const char *kByPathDirectory = "/dev/serial/by-path";

struct Candidate
{
    std::string requested;
    std::string canonical;
    std::string preferred;
};

bool startsWith(const std::string &value, const std::string &prefix)
{
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool containsPathComponent(const std::string &value, const std::string &component)
{
    return value.find(component) != std::string::npos;
}

std::string baseName(const std::string &path)
{
    const std::string::size_type pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return path;
    return path.substr(pos + 1);
}

bool isLikelySerialTty(const std::string &path)
{
    const std::string name = baseName(path);
    return startsWith(name, "ttyUSB") || startsWith(name, "ttyACM");
}

std::vector<std::string> listDirectory(const std::string &directory)
{
    std::vector<std::string> entries;
    DIR *dir = ::opendir(directory.c_str());
    if (!dir)
        return entries;

    while (dirent *entry = ::readdir(dir))
    {
        const std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue;
        std::string fullPath;
        if (directory.empty() || directory == "/")
            fullPath = "/" + name;
        else if (directory.back() == '/')
            fullPath = directory + name;
        else
            fullPath = directory + "/" + name;
        entries.push_back(fullPath);
    }
    ::closedir(dir);
    std::sort(entries.begin(), entries.end());
    return entries;
}

std::string canonicalPath(const std::string &path)
{
    char resolved[PATH_MAX] = {0};
    if (::realpath(path.c_str(), resolved) != nullptr)
        return resolved;
    return path;
}

int preferredPathRank(const std::string &path)
{
    if (containsPathComponent(path, "/serial/by-id/"))
        return 0;
    if (containsPathComponent(path, "/serial/by-path/"))
        return 1;
    if (isLikelySerialTty(path))
        return 2;
    return 3;
}

bool preferPath(const std::string &candidate, const std::string &current)
{
    const int candidateRank = preferredPathRank(candidate);
    const int currentRank = preferredPathRank(current);
    if (candidateRank != currentRank)
        return candidateRank < currentRank;
    return candidate < current;
}

std::vector<std::string> enumerateSerialCandidatesFromDirectories(const std::string &devDirectory,
        const std::string &byIdDirectory,
        const std::string &byPathDirectory)
{
    std::vector<std::string> candidates;

    for (const auto &path : listDirectory(devDirectory))
    {
        if (isLikelySerialTty(path))
            candidates.push_back(path);
    }

    auto addSerialLinks = [&](const std::string & directory)
    {
        for (const auto &path : listDirectory(directory))
        {
            const std::string canonical = canonicalPath(path);
            if (isLikelySerialTty(canonical))
                candidates.push_back(path);
        }
    };
    addSerialLinks(byIdDirectory);
    addSerialLinks(byPathDirectory);

    return candidates;
}

std::vector<Candidate> canonicalizeCandidates(const std::vector<std::string> &ports)
{
    std::vector<Candidate> candidates;
    for (const std::string &port : ports)
    {
        if (port.empty())
            continue;

        const std::string canonical = canonicalPath(port);
        auto existing = std::find_if(candidates.begin(), candidates.end(), [&](const Candidate & candidate)
        {
            return candidate.canonical == canonical;
        });

        if (existing == candidates.end())
        {
            candidates.push_back(Candidate{port, canonical, port});
        }
        else if (preferPath(port, existing->preferred))
        {
            existing->preferred = port;
        }
    }

    for (Candidate &candidate : candidates)
    {
        if (preferPath(candidate.canonical, candidate.preferred))
            candidate.preferred = candidate.canonical;
    }

    return candidates;
}

bool cancelled(const DiscoveryOptions &options)
{
    return options.shouldCancel && options.shouldCancel();
}

bool sleepWithCancellation(std::chrono::milliseconds delay, const DiscoveryOptions &options)
{
    auto remaining = delay;
    while (remaining.count() > 0)
    {
        if (cancelled(options))
            return false;
        const auto slice = std::min(remaining, std::chrono::milliseconds(50));
        std::this_thread::sleep_for(slice);
        remaining -= slice;
    }
    return !cancelled(options);
}

ProbeResult makeResult(const Candidate &candidate, ProbeStatus status, const std::string &reason = std::string())
{
    ProbeResult result;
    result.status = status;
    result.requestedPort = candidate.requested;
    result.canonicalPort = candidate.canonical;
    result.preferredPort = candidate.preferred;
    result.failureReason = reason;
    return result;
}

ProbeStatus identityStatusFromQuery(GeminiProtocol::DirectQueryStatus status)
{
    switch (status)
    {
        case GeminiProtocol::DirectQueryStatus::Success:
            return ProbeStatus::IdentityMatched;
        case GeminiProtocol::DirectQueryStatus::Timeout:
            return ProbeStatus::IdentityTimeout;
        case GeminiProtocol::DirectQueryStatus::UnexpectedFrame:
            return ProbeStatus::IdentityMismatch;
        case GeminiProtocol::DirectQueryStatus::WriteFailed:
        case GeminiProtocol::DirectQueryStatus::MalformedResponse:
            return ProbeStatus::ProtocolError;
    }
    return ProbeStatus::ProtocolError;
}

ProbeStatus firmwareStatusFromQuery(GeminiProtocol::DirectQueryStatus status)
{
    switch (status)
    {
        case GeminiProtocol::DirectQueryStatus::Success:
            return ProbeStatus::Identified;
        case GeminiProtocol::DirectQueryStatus::Timeout:
            return ProbeStatus::FirmwareTimeout;
        case GeminiProtocol::DirectQueryStatus::MalformedResponse:
            return ProbeStatus::FirmwareMalformed;
        case GeminiProtocol::DirectQueryStatus::UnexpectedFrame:
        case GeminiProtocol::DirectQueryStatus::WriteFailed:
            return ProbeStatus::ProtocolError;
    }
    return ProbeStatus::ProtocolError;
}

ProbeResult probeCandidate(const Candidate &candidate, const DiscoveryOptions &options)
{
    if (cancelled(options))
        return makeResult(candidate, ProbeStatus::Cancelled, "cancelled before opening port");

    ProbeResult result = makeResult(candidate, ProbeStatus::NotTried);
    GeminiProtocol protocol;
    protocol.setCancelFunction(options.shouldCancel);

    if (!protocol.open(candidate.preferred, 0, true))
    {
        const int openErrno = protocol.lastOpenErrno();
        const std::string reason = openErrno != 0 ? std::strerror(openErrno) : "open failed";
        result.status = protocol.lastOpenWasBusy() ? ProbeStatus::PortBusy : ProbeStatus::OpenFailed;
        result.failureReason = reason;
        return result;
    }

    if (!sleepWithCancellation(options.settleDelay, options))
    {
        protocol.close();
        result.status = ProbeStatus::Cancelled;
        result.failureReason = "cancelled after settle delay";
        return result;
    }

    std::string detail;
    GeminiProtocol::DirectQueryStatus identityStatus = protocol.queryIdentity(
            result.identity,
            static_cast<int>(options.identityTimeout.count()),
            &detail);
    if (cancelled(options))
    {
        protocol.close();
        result.status = ProbeStatus::Cancelled;
        result.failureReason = "cancelled during identity query";
        return result;
    }
    if (identityStatus != GeminiProtocol::DirectQueryStatus::Success)
    {
        protocol.close();
        result.status = identityStatusFromQuery(identityStatus);
        result.failureReason = detail;
        return result;
    }

    result.status = ProbeStatus::IdentityMatched;
    if (cancelled(options))
    {
        protocol.close();
        result.status = ProbeStatus::Cancelled;
        result.failureReason = "cancelled before firmware query";
        return result;
    }

    int firmwareNumeric = 0;
    GeminiProtocol::DirectQueryStatus firmwareStatus = protocol.queryFirmware(
            result.firmware.raw,
            firmwareNumeric,
            static_cast<int>(options.firmwareTimeout.count()),
            &detail);
    if (cancelled(options))
    {
        protocol.close();
        result.status = ProbeStatus::Cancelled;
        result.failureReason = "cancelled during firmware query";
        return result;
    }

    protocol.close();
    result.status = firmwareStatusFromQuery(firmwareStatus);
    if (firmwareStatus == GeminiProtocol::DirectQueryStatus::Success)
    {
        result.firmware.numeric = firmwareNumeric;
    }
    else
    {
        result.failureReason = detail;
    }
    return result;
}
}

std::vector<std::string> enumerateSerialCandidates()
{
    return enumerateSerialCandidatesFromDirectories(kDevDirectory, kByIdDirectory, kByPathDirectory);
}

ProbeResult probeGeminiDevice(const std::string &port, const DiscoveryOptions &options)
{
    std::vector<Candidate> candidates = canonicalizeCandidates({port});
    if (candidates.empty())
    {
        Candidate candidate{port, port, port};
        return makeResult(candidate, ProbeStatus::OpenFailed, "empty port");
    }
    return probeCandidate(candidates.front(), options);
}

DiscoveryResult discoverGeminiDevices(const DiscoveryOptions &options)
{
    DiscoveryResult result;
    const std::vector<std::string> ports = options.candidatePorts.empty() ? enumerateSerialCandidates() :
                                           options.candidatePorts;
    const std::vector<Candidate> candidates = canonicalizeCandidates(ports);

    for (const Candidate &candidate : candidates)
    {
        if (cancelled(options))
        {
            result.cancelled = true;
            result.probes.push_back(makeResult(candidate, ProbeStatus::Cancelled, "cancelled before probing candidate"));
            break;
        }

        ProbeResult probe = probeCandidate(candidate, options);
        if (probe.status == ProbeStatus::Cancelled)
            result.cancelled = true;
        if (probe.status == ProbeStatus::Identified)
            result.devices.push_back(probe);
        result.probes.push_back(std::move(probe));

        if (result.cancelled)
            break;
    }

    return result;
}

const char *probeStatusName(ProbeStatus status)
{
    switch (status)
    {
        case ProbeStatus::NotTried:
            return "NotTried";
        case ProbeStatus::OpenFailed:
            return "OpenFailed";
        case ProbeStatus::PortBusy:
            return "PortBusy";
        case ProbeStatus::IdentityTimeout:
            return "IdentityTimeout";
        case ProbeStatus::IdentityMismatch:
            return "IdentityMismatch";
        case ProbeStatus::IdentityMatched:
            return "IdentityMatched";
        case ProbeStatus::FirmwareTimeout:
            return "FirmwareTimeout";
        case ProbeStatus::FirmwareMalformed:
            return "FirmwareMalformed";
        case ProbeStatus::ProtocolError:
            return "ProtocolError";
        case ProbeStatus::Identified:
            return "Identified";
        case ProbeStatus::Cancelled:
            return "Cancelled";
    }
    return "Unknown";
}

#ifdef GEMINI_TESTING
std::vector<std::string> enumerateSerialCandidatesForTesting(const std::string &devDirectory,
        const std::string &byIdDirectory,
        const std::string &byPathDirectory)
{
    return enumerateSerialCandidatesFromDirectories(devDirectory, byIdDirectory, byPathDirectory);
}
#endif

} // namespace geminipbh
