//
// Created by not7cd on 06/12/18.
//

#include "indi_starbook.h"
#include "config.h"

#include <memory>


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
    setVersion(STARBOOK_DRIVER_VERSION_MAJOR, STARBOOK_DRIVER_VERSION_MAJOR);
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT,
                           1);
    setTelescopeConnection(CONNECTION_TCP);
    device = std::unique_ptr<StarbookDevice>(new StarbookDevice(STARBOOK_DEFAULT_IP));
    LOG_INFO(device->GetIpAddr().c_str());
}

bool Starbook::Connect() {
    LOG_INFO("hello");
    IDMessage(getDeviceName(), "Starbook connected successfully!");
    INDI::Telescope::Connect();
    return true;
}

bool Starbook::Disconnect() {
    LOG_INFO("bye");
    IDMessage(getDeviceName(), "Starbook disconnected successfully!");
    return true;
}

const char *Starbook::getDefaultName() {
    return "Starbook mount controller";
}

bool Starbook::ReadScopeStatus() {
    StarbookStatus status;
    bool res = device->GetStatus(status);

    if (res) {
        NewRaDec(status.ra, status.dec);
        return true;
    }
    return false;
}

bool Starbook::Goto(double ra, double dec) {
    LOG_INFO("Goto! Sending GOTORADEC command");
    bool res = device->GoToRaDec(ra, dec);
    NewRaDec(ra, dec);
    return res;
}


bool Starbook::Abort() {
    LOG_INFO("Aborting! Sending STOP command");
    bool res = device->Stop();
    return res;
}

bool Starbook::Park() {
    LOG_INFO("Parking! Sending HOME command");
    bool res = device->Home();
    return res;
}

bool Starbook::UnPark() {
    LOG_INFO("Un-parking! not sending anything");
    // knock starbook to SCOPE mode from STOP
//    bool res = device->GoToRaDec((double) 0, (double) 0);
    return true;
}

bool Starbook::Sync(double ra, double dec) {
    return Goto(ra, dec);
}

//void Starbook::ISGetProperties(const char *dev) {
//    Telescope::ISGetProperties(dev);
//}
//
//bool Starbook::ISNewNumber(const char *dev, const char *name, double *values, char **names, int n) {
//    return Telescope::ISNewNumber(dev, name, values, names, n);
//}
//
//bool Starbook::ISNewText(const char *dev, const char *name, char **texts, char **names, int n) {
//    return Telescope::ISNewText(dev, name, texts, names, n);
//}
//
//bool Starbook::ISNewSwitch(const char *dev, const char *name, ISState *states, char **names, int n) {
//    return Telescope::ISNewSwitch(dev, name, states, names, n);
//}
//
//bool Starbook::initProperties() {
//    Telescope::initProperties();
//    return true;
//}

