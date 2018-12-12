//
// Created by not7cd on 09/12/18.
//
#include "starbook_device.h"

#include <utility>
#include <iomanip>

#include <libnova/utility.h>
#include <sstream>

#include <indilogger.h>


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

static int closecb(void *clientp, curl_socket_t item) {
    (void) clientp;
    printf("libcurl wants to close %d now\n", (int) item);
//    INDI::Logger::getInstance().print("Starbook mount driver", INDI::Logger::DBG_WARNING, __FILE__, __LINE__, "closing");
    return 0;
}

static curl_socket_t opensocket(void *clientp,
                                curlsocktype purpose,
                                struct curl_sockaddr *address) {
    curl_socket_t sockfd;
    (void) purpose;
    (void) address;
    sockfd = *(curl_socket_t *) clientp;
    printf("libcurl wants to open socket now\n");
    /* the actual externally set socket is passed in via the OPENSOCKETDATA
       option */
    return sockfd;
}

static int sockopt_callback(void *clientp, curl_socket_t curlfd,
                            curlsocktype purpose) {
    (void) clientp;
    (void) curlfd;
    (void) purpose;
    /* This return code was added in libcurl 7.21.5 */
    return CURL_SOCKOPT_ALREADY_CONNECTED;
}

StarbookDevice::StarbookDevice(int sockfd) {
    sockfd_ = (curl_socket_t) sockfd;
    ip_addr_ = "localhost:5000";

    curl_global_init(CURL_GLOBAL_ALL);
    handle_ = curl_easy_init();
    if (handle_ == nullptr) {
        // TODO: Logging
        DEBUG(INDI::Logger::DBG_WARNING, "Failed to create CURL connection\n");
        exit(EXIT_FAILURE);
    }
    DEBUG(INDI::Logger::DBG_WARNING, "Created CURL connection\n");


    /* no progress meter please */
    curl_easy_setopt(handle_, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(handle_, CURLOPT_TIMEOUT, 2L);

    /* send all data to this function  */
//    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, write_data);

    /* call this function to get a socket */
    curl_easy_setopt(handle_, CURLOPT_OPENSOCKETFUNCTION, opensocket);
    curl_easy_setopt(handle_, CURLOPT_OPENSOCKETDATA, &sockfd_);

    /* call this function to close sockets */
    curl_easy_setopt(handle_, CURLOPT_CLOSESOCKETFUNCTION, closecb);
    curl_easy_setopt(handle_, CURLOPT_CLOSESOCKETDATA, &sockfd_);

    /* call this function to set options for the socket */
    curl_easy_setopt(handle_, CURLOPT_SOCKOPTFUNCTION, sockopt_callback);

    curl_easy_setopt(handle_, CURLOPT_VERBOSE, 1);
}

StarbookDevice::~StarbookDevice() {
    curl_easy_cleanup(handle_);
    curl_global_cleanup();
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
    DEBUG(INDI::Logger::DBG_WARNING, "Sending request\n");
    res = curl_easy_perform(handle_);
    if (res) {
        DEBUGF(INDI::Logger::DBG_WARNING, "RES: %s\n", read_buffer.c_str());
    }
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
    status.ra = 10;
    status.dec = 0;
    status.goto_ = 0;
    status.state = "INIT";
    return true;
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

const char *StarbookDevice::getDeviceName() {
    return "Starbook mount controller";
}

