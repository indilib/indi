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

    explicit StarbookDevice(int sockfd);

    ~StarbookDevice();

    bool GoToRaDec(double ra, double dec);

    bool GoToRaDec(lnh_equ_posn target);

    bool Stop();

    bool Home();

    bool GetStatus(StarbookStatus &status);

    const std::string GetIpAddr();

    const char *getDeviceName();

private:
    CURL *handle_;

    curl_socket_t sockfd_;

    std::string ip_addr_;

    bool SendCommand(const std::string &cmd, const std::string &params_str);

};

