/*
    INDI alpaca Driver
    
    Copyright (C) 2026 Gord Tulloch
    
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
#include <memory>
#include <httplib.h>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

class alpacaTelescopeDriver : public INDI::Telescope
{
public:
    alpacaTelescopeDriver();
    virtual ~alpacaTelescopeDriver() = default;

    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual bool ReadScopeStatus() override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual void TimerHit() override;

protected:
    // Connection
    virtual bool Handshake() override;

    // Telescope Operations
    virtual bool Goto(double ra, double dec) override;
    virtual bool Sync(double ra, double dec) override;
    virtual bool Abort() override;
    
    // Parking
    virtual bool Park() override;
    virtual bool UnPark() override;
    virtual bool SetCurrentPark() override;
    
    // Tracking
    virtual bool SetTrackEnabled(bool enabled) override;
    virtual bool SetTrackMode(uint8_t mode) override;

    // Motion Control
    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

    // Save configuration
    virtual bool saveConfigItems(FILE *fp) override;
    
    // Site location
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;

private:
    // State tracking
    double m_currentRA { 0 };
    double m_currentDEC { 90 };
    double m_currentAz { 180 };
    double m_currentAlt { 0 };
    double m_targetRA { 0 };
    double m_targetDEC { 0 };
    double m_sinLat, m_cosLat;
    bool isParked{false};
    bool isSlewing{false};
    bool isTracking{false};
    double currentSlewRate{0.5};  // Current slew rate in deg/sec

    // used by GoTo and Park
    void StartSlew(double ra, double dec, TelescopeStatus status);

    unsigned int DBG_SCOPE { 0 };
    
    // Scope type and alignment
    INDI::PropertySwitch mountTypeSP {2};
    enum
    {
        ALTAZ,
        EQ_FORK
    };

    // HTTP client for Alpaca communication
    std::unique_ptr<httplib::Client> httpClient;
    int m_DeviceNumber{0};
    int m_ClientID;
    int m_TransactionID{0};
    
    // Device info properties
    INDI::PropertyText DeviceInfoTP {4};
    enum { DESCRIPTION, DRIVER_INFO, DRIVER_VERSION, INTERFACE_VERSION };
    
    // Alpaca helper methods
    std::string getAlpacaURL(const std::string& endpoint);
    int getTransactionId() { return ++m_TransactionID; }
    bool sendAlpacaGET(const std::string& endpoint, nlohmann::json& response);
    bool sendAlpacaPUT(const std::string& endpoint, const nlohmann::json& request, nlohmann::json& response);
    
};
