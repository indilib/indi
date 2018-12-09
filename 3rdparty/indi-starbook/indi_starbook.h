//
// Created by not7cd on 06/12/18.
//

#pragma once

#include <inditelescope.h>
#include <curl/curl.h>

class Starbook : public INDI::Telescope
{
public:
    Starbook();

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

    std::string build_goto_params(double ra, double dec) const;

    CURLcode SendCommand(std::string &read_buffer, const std::string &url_str) const;
};
