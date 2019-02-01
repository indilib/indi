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
#include <curl/curl.h>
#include <memory>
#include "starbook_types.h"
#include "connectioncurl.h"
#include "command_interface.h"

class StarbookDriver : public INDI::Telescope
{
public:
    StarbookDriver();

    ~StarbookDriver() override;

    bool initProperties() override;

    bool updateProperties() override;

    bool ReadScopeStatus() override;

    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char **names, int n) override;

private:
    std::unique_ptr<starbook::CommandInterface> cmd_interface;

    Connection::Curl *curlConnection = nullptr;

    starbook::StarbookState last_known_state;

    int failed_res;

    void LogResponse(const std::string &cmd, const starbook::ResponseCode &rc);

protected:
    IText VersionT[1]{};

    ITextVectorProperty VersionTP;

    IText StateT[1]{};

    ITextVectorProperty StateTP;

    ISwitch StartS[1];

    ISwitchVectorProperty StartSP;

    bool Connect() override;

    bool Disconnect() override;

    bool Handshake() override;

    const char *getDefaultName() override;

    bool Goto(double ra, double dec) override;

    bool Sync(double ra, double dec) override;

    bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;

    bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

    bool Abort() override;

    bool Park() override;

    bool UnPark() override;

    bool updateTime(ln_date *utc, double utc_offset) override;

    bool getFirmwareVersion();
};
