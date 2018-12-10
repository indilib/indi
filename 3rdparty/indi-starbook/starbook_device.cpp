//
// Created by not7cd on 09/12/18.
//
#include "starbook_device.h"

#include <utility>
#include <iomanip>

#include <libnova/utility.h>
#include <sstream>


uint32_t size2uint32(size_t i) {
    if (i > UINT32_MAX) {
        throw;
    }
    return static_cast<uint32_t >(i);
}

static int32_t StringWriter(char *data, size_t size, size_t nmemb,
                            std::string &buffer_str) {

    const size_t n_bytes = size * nmemb;
    buffer_str.append(data, n_bytes);
    return size2uint32(n_bytes);
}

///////////////////////////////////

StarbookDevice::StarbookDevice(const std::string &ip_addr) {
    ip_addr_ = ip_addr;
    handle_ = curl_easy_init();
    if (handle_ == nullptr) {
        // TODO: Logging
        fprintf(stderr, "Failed to create CURL connection\n");
        exit(EXIT_FAILURE);
    }
}

StarbookDevice::~StarbookDevice() {
    curl_easy_cleanup(handle_);
}


static std::string read_buffer;

bool StarbookDevice::SendCommand(const std::string &cmd, const std::string &params_str) {
//    return true;
    CURLcode res;
    // TODO(not7cd): should I build url's like and call setopt every time?
    std::string cmd_url = "http://" + ip_addr_ + "/" + cmd + params_str;

    read_buffer.clear();
    curl_easy_setopt(handle_, CURLOPT_URL, cmd_url.c_str());
    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, StringWriter);
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &read_buffer);

    // TODO: ParseBuffer
    // TODO(not7cd): unknown if Starbook accepts something other than GET request
    res = curl_easy_perform(handle_);
    return res == CURLE_OK;
}

std::string GetCommandResponse() {
    return read_buffer;
}

bool StarbookDevice::Stop() {
    return SendCommand("STOP", "");
}

bool StarbookDevice::Home() {
    return SendCommand("HOME", "");
}

bool StarbookDevice::GoToRaDec(lnh_equ_posn target) {
    std::ostringstream params_stream;

    // strict params_str creation
    params_stream << std::fixed << std::setprecision(0);
    params_stream << "RA=";
    params_stream << target.ra.hours << "+" << target.ra.minutes << "." << target.ra.seconds;

    std::__cxx11::string dec_sign = target.dec.neg != 0 ? "-" : "";
    params_stream << "&DEC=";
    params_stream << dec_sign << target.dec.degrees << "+" << target.dec.minutes;
    // TODO: check if starbook accepts seconds in param
// params_stream << "." << target.dec.seconds;


    return SendCommand("GOTORADEC", params_stream.str());
}

bool StarbookDevice::GoToRaDec(double ra, double dec) {
    ln_equ_posn target_equ = {ra, dec};
    lnh_equ_posn h_target_equ = {{0, 0, 0},
                                 {0, 0, 0, 0}};
    ln_equ_to_hequ(&target_equ, &h_target_equ);
    return GoToRaDec(h_target_equ);
}

bool ParseStatus(StarbookStatus &status) {
    std::string response = GetCommandResponse();
    status.ra = 0;
    status.dec = 0;
    status.goto_ = 0;
    status.state = "INIT";
    return false;
}

bool StarbookDevice::GetStatus(StarbookStatus &status) {
    if (SendCommand("GETSTATUS", "")) {
        return ParseStatus(status);
    }
    return false;
}

const std::string StarbookDevice::GetIpAddr() {
    return ip_addr_;
}

