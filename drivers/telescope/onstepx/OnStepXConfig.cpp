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
*/

#include "OnStepXConfig.h"

#include <cstdlib>

namespace OnStepXConfig
{

std::string withoutProperties(const std::string &xml, const std::set<std::string> &exclude)
{
    // INDI writes each saved property as
    //   <newNumberVector device='D' name='N'>\n ... </newNumberVector>\n
    std::string out;
    size_t pos = 0;
    while (pos < xml.size())
    {
        const size_t start = xml.find("<new", pos);
        if (start == std::string::npos)
        {
            out.append(xml, pos, std::string::npos);
            break;
        }
        const size_t tagEnd = xml.find('>', start);
        const size_t space  = xml.find_first_of(" \t\n>", start);
        if (tagEnd == std::string::npos || space == std::string::npos)
        {
            out.append(xml, pos, std::string::npos);
            break;
        }
        const std::string kind = xml.substr(start + 1, space - start - 1);     // e.g. newNumberVector
        const std::string close = "</" + kind + ">";
        size_t blockEnd = xml.find(close, tagEnd);
        if (blockEnd == std::string::npos)
        {
            out.append(xml, pos, std::string::npos);
            break;
        }
        blockEnd += close.size();
        if (blockEnd < xml.size() && xml[blockEnd] == '\n')
            blockEnd++;

        std::string name;
        const std::string tag = xml.substr(start, tagEnd - start);
        const size_t at = tag.find("name=");
        if (at != std::string::npos && at + 5 < tag.size())
        {
            const char q = tag[at + 5];
            const size_t end = tag.find(q, at + 6);
            if (end != std::string::npos)
                name = tag.substr(at + 6, end - at - 6);
        }

        out.append(xml, pos, start - pos);
        if (!exclude.count(name))
            out.append(xml, start, blockEnd - start);
        pos = blockEnd;
    }
    return out;
}

bool saveExcept(FILE *fp, const std::set<std::string> &exclude, const std::function<bool(FILE *)> &save)
{
    char *buf = nullptr;
    size_t len = 0;
    FILE *mem = open_memstream(&buf, &len);
    if (!mem)
        return false;
    const bool ok = save(mem);
    fclose(mem);
    const std::string kept = withoutProperties(std::string(buf ? buf : "", len), exclude);
    free(buf);
    fputs(kept.c_str(), fp);
    return ok;
}

}
