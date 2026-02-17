/*
    INDI alpaca FilterWheel Driver
    
    Copyright (C) 2024 Gord Tulloch

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

#include <indifilterwheel.h>
#include <memory>
#include <string>

#define HTTPLIB_NO_EXCEPTIONS
#include "httplib.h"

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

class AlpacaFilterWheel : public INDI::FilterWheel
{
public:
    AlpacaFilterWheel();
    virtual ~AlpacaFilterWheel() = default;

    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool Connect() override;
    virtual bool Disconnect() override;

    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

protected:
    // FilterWheel interface
    virtual bool SelectFilter(int position) override;
    virtual int QueryFilter() override;

private:
    // Connection properties
    ITextVectorProperty ServerAddressTP;
    IText ServerAddressT[2] {};

    // Device info properties
    ITextVectorProperty DeviceInfoTP;
    IText DeviceInfoT[4] {};

    // Filter properties
    INumberVectorProperty FocusOffsetsNP;
    INumber FocusOffsetsN[3] {};

    // Alpaca communication
    std::unique_ptr<httplib::Client> m_AlpacaClient;
    std::string m_Host = "alpaca.local";
    int m_Port = 32323;
    int m_DeviceNumber = 0;
    uint32_t m_ClientID = 1;
    uint32_t m_TransactionID = 0;

    // Helper methods
    bool setupFilterWheel();
    bool sendAlpacaGET(const std::string &endpoint, nlohmann::json &response);
    bool sendAlpacaPUT(const std::string &endpoint, const std::string &data, nlohmann::json &response);
};
