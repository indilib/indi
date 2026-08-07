/*
    IKunFocuser protocol helpers
    Copyright (C) 2026 IKunFocuser contributors
    SPDX-License-Identifier: LGPL-2.1-or-later

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
*/

#include "ikunfocuser_protocol.h"

#include <cerrno>
#include <cstdlib>

namespace
{

bool findJsonValue(const std::string &json, const std::string &key, std::size_t &valueStart)
{
    const std::string token = "\"" + key + "\"";
    const auto keyStart = json.find(token);
    if (keyStart == std::string::npos)
        return false;

    const auto colon = json.find(':', keyStart + token.size());
    if (colon == std::string::npos)
        return false;

    valueStart = json.find_first_not_of(" \t\r\n", colon + 1);
    return valueStart != std::string::npos;
}

} // namespace

namespace IKunFocuserProtocol
{

bool parseMotionStatus(const std::string &response, MotionStatus &status)
{
    const auto positionMarker = response.find("P ");
    const auto movingMarker = response.find(";M ");
    if (positionMarker == std::string::npos || movingMarker == std::string::npos || movingMarker <= positionMarker + 2)
        return false;

    const std::string positionText = response.substr(positionMarker + 2, movingMarker - (positionMarker + 2));
    char *end = nullptr;
    errno = 0;
    const long long parsedPosition = std::strtoll(positionText.c_str(), &end, 10);
    if (errno != 0 || end == positionText.c_str() || *end != '\0' || parsedPosition < 0)
        return false;

    const auto movingStart = movingMarker + 3;
    bool parsedMoving = false;
    if (response.compare(movingStart, 4, "true") == 0)
        parsedMoving = true;
    else if (response.compare(movingStart, 5, "false") != 0)
        return false;

    status.position = parsedPosition;
    status.moving = parsedMoving;
    return true;
}

bool parseVersion(const std::string &response, int &version)
{
    const auto marker = response.find("V ");
    if (marker == std::string::npos)
        return false;

    const char *start = response.c_str() + marker + 2;
    char *end = nullptr;
    errno = 0;
    const long parsed = std::strtol(start, &end, 10);
    if (errno != 0 || end == start || parsed <= 0)
        return false;

    version = static_cast<int>(parsed);
    return true;
}

bool parseJsonInteger(const std::string &json, const std::string &key, int64_t &value)
{
    std::size_t start = 0;
    if (!findJsonValue(json, key, start))
        return false;

    char *end = nullptr;
    errno = 0;
    const long long parsed = std::strtoll(json.c_str() + start, &end, 10);
    if (errno != 0 || end == json.c_str() + start)
        return false;

    value = parsed;
    return true;
}

bool parseJsonNumber(const std::string &json, const std::string &key, double &value)
{
    std::size_t start = 0;
    if (!findJsonValue(json, key, start))
        return false;

    char *end = nullptr;
    errno = 0;
    const double parsed = std::strtod(json.c_str() + start, &end);
    if (errno != 0 || end == json.c_str() + start)
        return false;

    value = parsed;
    return true;
}

bool parseJsonBoolean(const std::string &json, const std::string &key, bool &value)
{
    std::size_t start = 0;
    if (!findJsonValue(json, key, start))
        return false;

    if (json.compare(start, 4, "true") == 0)
    {
        value = true;
        return true;
    }

    if (json.compare(start, 5, "false") == 0)
    {
        value = false;
        return true;
    }

    return false;
}

bool isErrorResponse(const std::string &response)
{
    return response.find("ERR:") != std::string::npos;
}

bool isSupportedIdentity(const std::string &response)
{
    return response.find("EFucoser") != std::string::npos;
}

const char *modelForVersion(int version)
{
    if (version >= 1200 && version < 1300)
        return "Arduino Nano ULN2003";
    if (version >= 1100 && version < 1200)
        return "ESP8266 ULN2003";
    if (version >= 1000 && version < 1100)
        return "ESP8266 STEP/DIR";
    return "EFucoser";
}

bool isSupportedVersion(int version)
{
    return (version >= 1201 && version < 1300) ||
           (version >= 1103 && version < 1200) ||
           (version >= 1005 && version < 1100);
}

} // namespace IKunFocuserProtocol
