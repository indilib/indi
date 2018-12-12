/*
 Starbook mount driver

 Copyright (C) 2018 Norbert Szulc (not7cd)

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

#include "indi_starbook.h"
#include "config.h"

#include <indicom.h>
#include <connectionplugins/connectiontcp.h>
#include <memory>
#include <cstring>
#include <libnova.h>
#include <iomanip>

#define POLLMS 10

// TODO: decide if this macro is worth it
#define STARBOOK_DEFAULT_IP   "localhost:5000"

std::unique_ptr<Starbook> starbook(new Starbook());

void ISGetProperties(const char *dev) {
    starbook->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) {
    starbook->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) {
    starbook->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {
    starbook->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n) {
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root) {
    INDI_UNUSED(root);
}


Starbook::Starbook() {
    LOG_INFO("Staring driver");
    setVersion(STARBOOK_DRIVER_VERSION_MAJOR, STARBOOK_DRIVER_VERSION_MINOR);
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT,
                           1);

    setTelescopeConnection(CONNECTION_TCP);
    curl_global_init(CURL_GLOBAL_ALL);
    handle = curl_easy_init();
}

Starbook::~Starbook() {
    curl_global_cleanup();
}

bool Starbook::initProperties() {
    Telescope::initProperties();

    if (getTelescopeConnection() & CONNECTION_TCP) {
        tcpConnection->setDefaultHost("127.0.0.1");
        tcpConnection->setDefaultPort(5000);
    }

    addDebugControl();

    return true;
}

bool Starbook::Connect() {
    return Telescope::Connect();
}

bool Starbook::Disconnect() {
    curl_easy_cleanup(handle);
    return Telescope::Disconnect();
}

const char *Starbook::getDefaultName() {
    return "Starbook mount controller";
}


bool Starbook::ReadScopeStatus() {
    LOG_INFO("Status! Sending GETSTATUS command");
    bool res = SendCommand("GETSTATUS");
    if (res) {
        NewRaDec(0, 0);
        return true;
    }
    return false;
}

bool Starbook::Goto(double ra, double dec) {
    LOG_INFO("Goto! Sending GOTORADEC command");

    ln_equ_posn target_d = {ra, dec};
    lnh_equ_posn target = {{0, 0, 0},
                           {0, 0, 0, 0}};
    ln_equ_to_hequ(&target_d, &target);

    std::ostringstream params;

    // strict params_str creation
    params << std::fixed << std::setprecision(0);
    params << "?RA=";
    params << target.ra.hours << "+" << target.ra.minutes << "." << target.ra.seconds;

    params << "&DEC=";
    if (target.dec.neg != 0) params << "-";
    params << target.dec.degrees << "+" << target.dec.minutes;
    // TODO: check if starbook accepts seconds in param
// params << "." << target.dec.seconds;


    bool res = SendCommand("GOTORADEC" + params.str());
    return res;
}

bool Starbook::Abort() {
    LOG_INFO("Aborting! Sending STOP command");
    bool res = SendCommand("STOP");
    return res;
}

bool Starbook::Sync(double ra, double dec) {
    return Goto(ra, dec);
}


bool Starbook::Park() {
    // TODO Park
    LOG_INFO("Parking! Sending HOME command");
    return SendCommand("HOME");
}

bool Starbook::UnPark() {
    // TODO UnPark
    LOG_INFO("Un-parking! not sending anything");
    return true;
}


static std::string readBuffer;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    INDI_UNUSED(userp);
    size_t real_size = size * nmemb;
    readBuffer.append((char *) contents, real_size);
    return real_size;
}

static curl_socket_t opensocket(void *clientp,
                                curlsocktype purpose,
                                struct curl_sockaddr *address) {
    INDI_UNUSED(purpose);
    INDI_UNUSED(address);
    curl_socket_t sockfd;
    sockfd = *(curl_socket_t *) clientp;
    /* the actual externally set socket is passed in via the OPENSOCKETDATA
       option */
    return sockfd;
}

static int closesocket(void *clientp, curl_socket_t item) {
    INDI_UNUSED(clientp);
    printf("libcurl wants to close %d now\n", (int) item);
    return 0;
}

static int sockopt_callback(void *clientp, curl_socket_t curlfd,
                            curlsocktype purpose) {
    INDI_UNUSED(clientp);
    INDI_UNUSED(curlfd);
    INDI_UNUSED(purpose);
    return CURL_SOCKOPT_ALREADY_CONNECTED;
}


bool Starbook::SendCommand(std::string cmd) {

    int rc = 0;

    readBuffer.clear();
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 2L);

    /* send all data to this function  */
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &readBuffer);

    /* call this function to get a socket */
    // TODO: I was unable to reuse tcpConnection->PortFD
//    curl_easy_setopt(handle, CURLOPT_OPENSOCKETFUNCTION, opensocket);
//    curl_easy_setopt(handle, CURLOPT_OPENSOCKETDATA, &PortFD);
//
//    /* call this function to close sockets */
//    curl_easy_setopt(handle, CURLOPT_CLOSESOCKETFUNCTION, closesocket);
//    curl_easy_setopt(handle, CURLOPT_CLOSESOCKETDATA, &PortFD);
//
//    /* call this function to set options for the socket */
//    curl_easy_setopt(handle, CURLOPT_SOCKOPTFUNCTION, sockopt_callback);

    curl_easy_setopt(handle, CURLOPT_VERBOSE, 1);

    // TODO: https://github.com/indilib/indi/pull/779
    // HACK: Just use CURL, and use tcpConnection as input
    std::string cmd_url = "http://" + std::string(tcpConnection->host()) + ":" + "5000" + "/" + cmd;
    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %s", cmd_url.c_str());
    curl_easy_setopt(handle, CURLOPT_URL, cmd_url.c_str());

    rc = curl_easy_perform(handle);

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", readBuffer.c_str());

    return rc == CURLE_OK;
}

bool Starbook::Handshake() {
    DEBUG(INDI::Logger::DBG_WARNING, ("Handshake"));
    return Telescope::Handshake();
}
