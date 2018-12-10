//
// Created by not7cd on 09/12/18.
//

#pragma once

#include <libnova/ln_types.h>
#include <string>
#include <curl/curl.h>

struct StarbookStatus {
    double ra;
    double dec;
    int goto_;
    std::string state;
};

class StarbookDevice {
public:

    explicit StarbookDevice(const std::string &ip_addr);

    ~StarbookDevice();

    bool GoToRaDec(double ra, double dec);

    bool GoToRaDec(lnh_equ_posn target);

    bool Stop();

    bool Home();

    bool GetStatus(StarbookStatus &status);

    const std::string GetIpAddr();

private:
    CURL *handle_;

    std::string ip_addr_;

    bool SendCommand(const std::string &cmd, const std::string &params_str);

};

