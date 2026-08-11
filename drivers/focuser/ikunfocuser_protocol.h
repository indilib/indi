/*
    IKunFocuser protocol helpers
    Copyright (C) 2026 IKunFocuser contributors
    SPDX-License-Identifier: LGPL-2.1-or-later

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.
*/

#pragma once

#include <cstdint>
#include <string>

namespace IKunFocuserProtocol
{

struct MotionStatus
{
    int64_t position { 0 };
    bool moving { false };
};

bool parseMotionStatus(const std::string &response, MotionStatus &status);
bool parseVersion(const std::string &response, int &version);
bool parseJsonInteger(const std::string &json, const std::string &key, int64_t &value);
bool parseJsonNumber(const std::string &json, const std::string &key, double &value);
bool parseJsonBoolean(const std::string &json, const std::string &key, bool &value);
bool isErrorResponse(const std::string &response);
bool isSupportedIdentity(const std::string &response);
const char *modelForVersion(int version);
bool isSupportedVersion(int version);

} // namespace IKunFocuserProtocol
