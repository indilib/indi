/*
    INDI alpaca Focuser Driver
    
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

#include <indifocuser.h>
#include <connectionplugins/connectiontcp.h>
#include <memory>
#include <string>

#define HTTPLIB_NO_EXCEPTIONS
#include "httplib.h"

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

class AlpacaFocuser : public INDI::Focuser
{
public:
    AlpacaFocuser();
    virtual ~AlpacaFocuser();

    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual bool Handshake() override;

    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

protected:
    // Focuser interface
    virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
    virtual bool AbortFocuser() override;
    virtual void TimerHit() override;

private:
    // Device info properties
    ITextVectorProperty DeviceInfoTP;
    IText DeviceInfoT[4] {};

    // Temperature property
    INumberVectorProperty TemperatureNP;
    INumber TemperatureN[1] {};

    // Alpaca communication
    std::unique_ptr<httplib::Client> m_AlpacaClient;
    Connection::TCP *tcpConnection { nullptr };
    std::string m_Host = "alpaca.local";
    int m_Port = 32323;
    int m_DeviceNumber = 0;
    uint32_t m_ClientID = 1;
    uint32_t m_TransactionID = 0;

    // Movement state
    uint32_t m_TargetPosition = 0;
    bool m_Moving = false;

    // Helper methods
    bool setupFocuser();
    bool isMoving();
    int getPosition();
    bool sendAlpacaGET(const std::string &endpoint, nlohmann::json &response);
    bool sendAlpacaPUT(const std::string &endpoint, const std::string &data, nlohmann::json &response);
};
