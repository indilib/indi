//
// Created by not7cd on 06/12/18.
//

#include "indi_starbook.h"
#include "config.h"

#include <memory>


// TODO: decide if this macro is worth it
#define STARBOOK_DEFAULT_IP   "192.168.0.61"

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
    starbook->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice(XMLEle *root) {
    INDI_UNUSED(root);
}


Starbook::Starbook() {
    setVersion(STARBOOK_DRIVER_VERSION_MAJOR, STARBOOK_DRIVER_VERSION_MAJOR);
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT,
                           1);

    device = std::unique_ptr<StarbookDevice>(new StarbookDevice(STARBOOK_DEFAULT_IP));
}

bool Starbook::Connect() {
    IDMessage(getDeviceName(), "Starbook connected successfully!");
    return true;
}

bool Starbook::Disconnect() {
    IDMessage(getDeviceName(), "Starbook disconnected successfully!");
    return true;
}

const char *Starbook::getDefaultName() {
    return "Starbook mount controller";
}

bool Starbook::ReadScopeStatus() {

    return true;
}

bool Starbook::Goto(double ra, double dec) {
    bool res = device->GoToRaDec(ra, dec);
    return res;
}


bool Starbook::Abort() {
    bool res = device->Stop();
    return res;
}

bool Starbook::Park() {
    bool res = device->Home();
    return res;
}

bool Starbook::UnPark() {
    bool res = device->GoToRaDec((double) 0, (double) 0);
    return res;
}
