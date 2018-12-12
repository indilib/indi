//
// Created by not7cd on 06/12/18.
//

#include "indi_starbook.h"
#include "config.h"

#include <indicom.h>
#include <connectionplugins/connectiontcp.h>
#include <memory>
#include <cstring>

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
}

bool Starbook::Connect() {
    bool res = Telescope::Connect();
    IDMessage(getDeviceName(), "Starbook connected successfully!");

    // HACK: Is created during handshake ...
//    if (getActiveConnection() == tcpConnection) {
//        device = std::unique_ptr<StarbookDevice>(new StarbookDevice(tcpConnection->getPortFD()));
//
//        LOG_INFO("Created new device!");
//    } else {
//        LOG_ERROR("Didn't create device!");
//    }

    return res;
}

bool Starbook::Disconnect() {
    IDMessage(getDeviceName(), "Starbook disconnected successfully!");
    return true;
}

const char *Starbook::getDefaultName() {
    return "Starbook mount controller";
}

bool Starbook::ReadScopeStatus() {
    StarbookStatus status = {0, 0, 0, "SCOPE"};
    LOG_INFO("Status! Sending GETSTATUS command");
    DEBUG(INDI::Logger::DBG_WARNING, isConnected() ? "connected" : "disconnected");
    bool res = device->GetStatus(status);
//    bool res = SendCommand("GETSTATUS");
    if (res) {
        NewRaDec(status.ra, status.dec);
        LOG_INFO("Send status");
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
//    bool res = device->Home();
    return SendCommand("HOME");
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


bool Starbook::SendCommand(std::string cmd) {
    DEBUG(INDI::Logger::DBG_WARNING, isConnected() ? "connected" : "disconnected");
    if (isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, tcpConnection->host());
    }
    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    char pCMD[MAXRBUF] = {0}, pRES[MAXRBUF] = {0};
    std::ostringstream request;

    // I'm bad human being and I shouldn't do it by hand
    request << "GET" << " " << cmd;

    request << " " << "HTTP/1.1" << "\r\n" << "\r\n";

    strncpy(pCMD,
            "GET /STATUS HTTP/1.1\r\n",
            MAXRBUF);

    DEBUGF(INDI::Logger::DBG_WARNING, "CMD: %s", pCMD);

    LOGF_INFO("socc %i", PortFD);
    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK) {
//        strncpy(pCMD,
//            "/* Java Script */"
//            "var Out;"
//            "sky6RASCOMTele.ConnectAndDoNotUnpark();"
//            "Out = sky6RASCOMTele.IsConnected;",
//            MAXRBUF);
        LOG_ERROR(pCMD);
        return false;
    }

    // Should we read until we encounter string terminator? or what?
    if (static_cast<int>(rc == tty_read_section(PortFD, pRES, '\0', 2, &nbytes_read)) != TTY_OK) {
        LOG_ERROR("Error reading from Starbook TCP server.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_WARNING, "RES: %s", pRES);
    return true;
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

bool Starbook::Handshake() {
    DEBUG(INDI::Logger::DBG_WARNING, ("Handshake"));
    // Somehow handshake is send during connection creation. When device wrapper doesn't exist. There is need to change it's behaviour
    if (device == nullptr) {
        device = std::unique_ptr<StarbookDevice>(new StarbookDevice(tcpConnection->getPortFD()));
        LOG_ERROR("Created new device!");
    } else {
    }
    return Telescope::Handshake();
}
