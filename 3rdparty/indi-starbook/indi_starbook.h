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

private:
    std::unique_ptr<StarbookDevice> device;

protected:

    bool Connect() override;

    bool Disconnect() override;

    const char *getDefaultName() override;

    bool Goto(double ra, double dec) override;

    bool Abort() override;

    bool Park() override;

    bool UnPark() override;
//    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
//    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

    bool ReadScopeStatus() override;

};
