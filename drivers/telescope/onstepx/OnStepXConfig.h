/*
    OnStepX INDI Driver — INDI config helpers (both binaries)
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

    What the driver keeps in the INDI config (see CLAUDE.md, "INDI config"):
    settings the firmware stores in NV are never saved, because a config
    load (Ekos sends one on connect) would overwrite changes made from the
    hand controller or web UI.  Settings the firmware keeps only in RAM, the
    USB plugin's port states and client-side settings are saved.

    A base class whose saveConfigItems() writes a mix of both is filtered:
    its output is written to a buffer and copied to the config without the
    excluded properties.
*/

#pragma once

#include <cstdio>
#include <functional>
#include <set>
#include <string>

namespace OnStepXConfig
{
// Copy INDI config XML, dropping every <new...Vector name='X'> block whose
// name is in `exclude`.  Text outside property blocks is kept.
std::string withoutProperties(const std::string &xml, const std::set<std::string> &exclude);

// Run `save` into a buffer and write its output to `fp` without `exclude`.
bool saveExcept(FILE *fp, const std::set<std::string> &exclude, const std::function<bool(FILE *)> &save);
}
