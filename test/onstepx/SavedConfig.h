/*
    OnStepX INDI Driver — SavedConfig.h
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

/*
    OnStepX INDI Driver — shared test helper: capture what a saveConfig writes

    The rule (CLAUDE.md, "INDI config"): settings the firmware keeps in NV are
    never saved; RAM-only settings, the USB plugin's port states and
    client-side settings are.
*/

#pragma once

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

inline std::string savedConfig(const std::function<void(FILE *)> &save)
{
    char *buf = nullptr;
    size_t len = 0;
    FILE *fp = open_memstream(&buf, &len);
    save(fp);
    fclose(fp);
    std::string s(buf ? buf : "", len);
    free(buf);
    return s;
}

inline bool savesProperty(const std::string &config, const char *name)
{
    return config.find(std::string("name='") + name + "'") != std::string::npos;
}
