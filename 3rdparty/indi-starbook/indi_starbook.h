/*
 Starbook mount driver

 Copyright (C) 2018 Norbert Szulc (not7cd)

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

#pragma once

#include <inditelescope.h>
#include <bits/unique_ptr.h>
#include <curl/curl.h>
#include "starbook_types.h"

class Starbook : public INDI::Telescope
{
public:
    Starbook();

    ~Starbook();

    bool initProperties() override;

    bool ReadScopeStatus() override;

private:

    bool SendCommand(std::string command);

    uint32_t POLLMS = 10;

    CURL *handle;

protected:

    bool Connect() override;

    bool Disconnect() override;

    bool Handshake() override;

    const char *getDefaultName() override;

    bool Goto(double ra, double dec) override;

    bool Sync(double ra, double dec) override;

    bool Abort() override;

    bool Park() override;

    bool UnPark() override;

};
