//
// Created by not7cd on 06/12/18.
//

#include "indi_starbook.h"
#include "config.h"

#include <memory>
#include <iomanip>

#include <libnova/utility.h>

std::unique_ptr<Starbook> starbook(new Starbook());

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *) userp)->append((char *) contents, size * nmemb);
    return size * nmemb;
}

void ISGetProperties (const char *dev)
{
    starbook->ISGetProperties(dev);
}

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    starbook->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    starbook->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    starbook->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    starbook->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice (XMLEle *root)
{
    INDI_UNUSED(root);
}


Starbook::Starbook()
{
    setVersion(STARBOOK_DRIVER_VERSION_MAJOR, STARBOOK_DRIVER_VERSION_MAJOR);
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT,
                           1);

}

bool Starbook::Connect()
{
    IDMessage(getDeviceName(), "Starbook connected successfully!");
    return true;
}

bool Starbook::Disconnect()
{
    IDMessage(getDeviceName(), "Starbook disconnected successfully!");
    return true;
}

const char * Starbook::getDefaultName()
{
    return "Starbook mount controller";
}

bool Starbook::ReadScopeStatus() {

    return true;
}

bool Starbook::Goto(double ra, double dec) {

    std::string read_buffer;
    std::string url_str;
    std::ostringstream url_stream;
    std::string starbook_ip = "192.168.0.61";

    url_stream << "http://" << starbook_ip << "/";
    url_stream << "GOTORADEC" << "?" << build_goto_params(ra, dec);
    url_str = url_stream.str();

    SendCommand(read_buffer, url_stream.str());

    return true;
}

CURLcode Starbook::SendCommand(std::string &read_buffer, const std::string &url_str) const {
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return res;
}

std::string Starbook::build_goto_params(double ra, double dec) const {
    ln_equ_posn target_equ = {ra, dec};
    lnh_equ_posn h_target_equ = {{0, 0, 0},
                                 {0, 0, 0, 0}};

    ln_equ_to_hequ(&target_equ, &h_target_equ);

    std::ostringstream params_stream;

    params_stream << std::fixed << std::setprecision(0);
    params_stream << "RA=";
    params_stream << h_target_equ.ra.hours << "+" << h_target_equ.ra.minutes << "." << h_target_equ.ra.seconds;

    std::__cxx11::string dec_sign = h_target_equ.dec.neg != 0 ? "-" : "";
    params_stream << "&DEC=";
    params_stream << dec_sign << h_target_equ.dec.degrees << "+" << h_target_equ.dec.minutes;
    // TODO: check if starbook accepts seconds in param
// params_stream << "." << h_target_equ.dec.seconds;

    return params_stream.str();
}

bool Starbook::Abort() {
    return Telescope::Abort();
}

bool Starbook::Park() {
    return Telescope::Park();
}

bool Starbook::UnPark() {
    return Telescope::UnPark();
}
