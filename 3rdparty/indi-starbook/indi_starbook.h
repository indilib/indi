//
// Created by not7cd on 06/12/18.
//

#pragma once

#include <inditelescope.h>
#include <bits/unique_ptr.h>
#include <curl/curl.h>
#include "starbook_device.h"

class Starbook : public INDI::Telescope
{
public:
    Starbook();

    ~Starbook();

    bool initProperties() override;

    bool ReadScopeStatus() override;

private:
    std::unique_ptr<StarbookDevice> device;

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
