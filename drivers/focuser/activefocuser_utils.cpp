/*
    ActiveFocuser driver for Takahashi CCA-250 and Mewlon-250/300CRS

    Driver written by Alvin FREY <https://afrey.fr> for Optique Unterlinden and Takahashi Europe

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

#include "activefocuser_utils.h"

// Parser method definitions

int ActiveFocuserUtils::Parser::Get32(const unsigned char *buffer, int position) {

    int num = 0;
    for (int i = 0; i < 4; ++i) {
        num = num << 8 | buffer[position + i];
    }
    return num;

}

int ActiveFocuserUtils::Parser::Get16(const unsigned char *buffer, int position) {

    return buffer[position] << 8 | buffer[position + 1];

}

double ActiveFocuserUtils::Parser::TicksToMillimeters(int ticks) {

    return ticks * SystemState::GetMmpp();

}

int ActiveFocuserUtils::Parser::MillimetersToTicks(double millimeters) {

    return (int)(millimeters / SystemState::GetMmpp());

}

void ActiveFocuserUtils::Parser::PrintFrame(const unsigned char *buffer) {

    std::stringstream outputStream;

    for (int i = 0; i < (int) sizeof(buffer); i++) {
        outputStream << std::hex << (int)buffer[i];
    }

    IDLog("%s\r\n", outputStream.str().c_str());

}

void ActiveFocuserUtils::Parser::PrintBasicDeviceData(const unsigned char *buffer) {

    std::stringstream sst;

    sst << "Current device (v " <<
        std::to_string(ActiveFocuserUtils::Parser::Get16(buffer, 17) >> 8) << "." << std::to_string(ActiveFocuserUtils::Parser::Get16(buffer, 17) & 0xFF)
        << ") state : (fan=" << std::to_string((buffer[7] & 32) != 0) <<
        ", position=" << std::to_string(ActiveFocuserUtils::Parser::Get32(buffer, 2)) <<
        ", position_mm=" << std::to_string(ActiveFocuserUtils::Parser::TicksToMillimeters(ActiveFocuserUtils::Parser::Get32(buffer, 2))) << ")";

    IDLog("%s\r\n", sst.str().c_str());

}

// Poller method definitions

ActiveFocuserUtils::Poller *ActiveFocuserUtils::Poller::pinstance_{nullptr};
std::mutex ActiveFocuserUtils::Poller::mutex_;
std::promise<void> exitSignal;
std::future<void> futureObj = exitSignal.get_future();
std::promise<void> exitSignalSender;
std::future<void> futureObjSender = exitSignalSender.get_future();
std::thread th;
std::thread thSender;
int poller_res;
unsigned char poller_buffer[60];
hid_device *device;

ActiveFocuserUtils::Poller *ActiveFocuserUtils::Poller::GetInstance(hid_device &hid_handle) {

    std::lock_guard<std::mutex> lock(mutex_);
    if (pinstance_ == nullptr) {
        device = &hid_handle;
        pinstance_ = new ActiveFocuserUtils::Poller(hid_handle);
    }
    return pinstance_;

}

void threaded_sender(std::future<void> futureObjSender) {

    while (futureObjSender.wait_for(std::chrono::milliseconds(1000)) == std::future_status::timeout) {

        memset(poller_buffer, 0, sizeof(poller_buffer));

        const unsigned char data[3] = {0x01, ActiveFocuserUtils::CommandsMap.at(ActiveFocuserUtils::DUMMY)};
        hid_write(device, data, sizeof(data));

        if (poller_res < 0)
            IDLog("Unable to write \r\n");

    }

}

void threaded_poller(std::future<void> futureObj) {

    while (futureObj.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout) {

        unsigned char buf[256];
        int res = hid_read(device, buf, sizeof(buf));

        if (res > 0) {
            if (buf[0] == 0x3C) {

                ActiveFocuserUtils::SystemState::SetSpan(ActiveFocuserUtils::Parser::Get32(buf, 25));
                ActiveFocuserUtils::SystemState::SetImmpp(ActiveFocuserUtils::Parser::Get16(buf, 23));
                ActiveFocuserUtils::SystemState::SetMmpp(ActiveFocuserUtils::SystemState::GetImmpp() / 1000000.0);
                std::stringstream ssVersion;
                ssVersion << std::to_string(ActiveFocuserUtils::Parser::Get16(buf, 17) >> 8) + "." + std::to_string(ActiveFocuserUtils::Parser::Get16(buf, 17) & 0xFF);
                ActiveFocuserUtils::SystemState::SetHardwareRevision(const_cast<char *>(ssVersion.str().c_str()));
                ActiveFocuserUtils::SystemState::SetIsOrigin((buf[7] & 128) != 0);
                ActiveFocuserUtils::SystemState::SetIsMoving((buf[7] & 64) != 0);
                ActiveFocuserUtils::SystemState::SetIsFanOn((buf[7] & 32) != 0);
                ActiveFocuserUtils::SystemState::SetIsHold((buf[7] & 3) != 0);
                ActiveFocuserUtils::SystemState::SetCurrentPositionStep(ActiveFocuserUtils::Parser::Get32(buf, 2));
                ActiveFocuserUtils::SystemState::SetCurrentPosition(ActiveFocuserUtils::Parser::TicksToMillimeters(ActiveFocuserUtils::SystemState::GetCurrentPositionStep()));
                ActiveFocuserUtils::SystemState::SetAirTemperature(ActiveFocuserUtils::Parser::Get32(buf, 45) / 10.0);
                ActiveFocuserUtils::SystemState::SetTubeTemperature(ActiveFocuserUtils::Parser::Get32(buf, 49) / 10.0);
                ActiveFocuserUtils::SystemState::SetMirrorTemperature(ActiveFocuserUtils::Parser::Get32(buf, 53) / 10.0);

            }
        }
        if (res < 0) {
            IDLog("Unable to read \r\n");
        }


    }

}

bool ActiveFocuserUtils::Poller::Start() {

    th = std::thread(&threaded_poller, std::move(futureObj));
    thSender = std::thread(&threaded_sender, std::move(futureObjSender));

    IsRunning = true;

    IDLog("Poller started\r\n");

    return true;

}

bool ActiveFocuserUtils::Poller::Stop() {

    exitSignal.set_value();
    exitSignalSender.set_value();

    th.join();
    thSender.join();

    IDLog("Poller stopped\r\n");

    IsRunning = false;

    return true;

}

// SystemState variable definitions

static int CurrentPositionStep;
static double CurrentPosition;
static bool IsOrigin;
static bool IsFanOn;
static bool IsHold;
static bool IsMoving;
static char * HardwareRevision;
static int Immpp;
static int Span;
static double Mmpp;
static double AirTemperature;
static double TubeTemperature;
static double MirrorTemperature;

// SystemState method definitions

int ActiveFocuserUtils::SystemState::GetCurrentPositionStep() {
    return CurrentPositionStep;
}

void ActiveFocuserUtils::SystemState::SetCurrentPositionStep(int currentPositionStep) {
    CurrentPositionStep = currentPositionStep;
}

double ActiveFocuserUtils::SystemState::GetCurrentPosition() {
    return CurrentPosition;
}

void ActiveFocuserUtils::SystemState::SetCurrentPosition(double currentPosition) {
    CurrentPosition = currentPosition;
}

bool ActiveFocuserUtils::SystemState::GetIsOrigin() {
    return IsOrigin;
}

void ActiveFocuserUtils::SystemState::SetIsOrigin(bool isOrigin) {
    IsOrigin = isOrigin;
}

bool ActiveFocuserUtils::SystemState::GetIsMoving() {
    return IsMoving;
}

void ActiveFocuserUtils::SystemState::SetIsMoving(bool isMoving) {
    IsMoving = isMoving;
}

bool ActiveFocuserUtils::SystemState::GetIsFanOn() {
    return IsFanOn;
}

void ActiveFocuserUtils::SystemState::SetIsFanOn(bool isFanOn) {
    IsFanOn= isFanOn;
}

bool ActiveFocuserUtils::SystemState::GetIsHold() {
    return IsHold;
}

void ActiveFocuserUtils::SystemState::SetIsHold(bool isHold) {
    IsHold = isHold;
}

char *ActiveFocuserUtils::SystemState::GetHardwareRevision() {
    return HardwareRevision;
}

void ActiveFocuserUtils::SystemState::SetHardwareRevision(char *hardwareRevision) {
    HardwareRevision = hardwareRevision;
}

int ActiveFocuserUtils::SystemState::GetImmpp() {
    return Immpp;
}

void ActiveFocuserUtils::SystemState::SetImmpp(int immpp) {
    Immpp = immpp;
}

int ActiveFocuserUtils::SystemState::GetSpan() {
    return Span;
}

void ActiveFocuserUtils::SystemState::SetSpan(int span) {
    Span = span;
}

double ActiveFocuserUtils::SystemState::GetMmpp() {
    return Mmpp;
}

void ActiveFocuserUtils::SystemState::SetMmpp(double mmpp) {
    Mmpp = mmpp;
}

double ActiveFocuserUtils::SystemState::GetAirTemperature() {
    return AirTemperature;
}

void ActiveFocuserUtils::SystemState::SetAirTemperature(double airTemperature) {
    AirTemperature = airTemperature;
}

double ActiveFocuserUtils::SystemState::GetTubeTemperature() {
    return TubeTemperature;
}

void ActiveFocuserUtils::SystemState::SetTubeTemperature(double tubeTemperature) {
    TubeTemperature = tubeTemperature;
}

double ActiveFocuserUtils::SystemState::GetMirrorTemperature() {
    return MirrorTemperature;
}

void ActiveFocuserUtils::SystemState::SetMirrorTemperature(double mirrorTemperature) {
    MirrorTemperature = mirrorTemperature;
}

// Commands enum map

const std::map<ActiveFocuserUtils::Commands, unsigned char> ActiveFocuserUtils::CommandsMap =
        {
                {ActiveFocuserUtils::Commands::ZERO,              0x03},
                {ActiveFocuserUtils::Commands::RELEASE,           0x04},
                {ActiveFocuserUtils::Commands::FREE,              0x06},
                {ActiveFocuserUtils::Commands::AUTO,              0x07},
                {ActiveFocuserUtils::Commands::MOVE,              0x09},
                {ActiveFocuserUtils::Commands::STOP,              0x0A},
                {ActiveFocuserUtils::Commands::FAN_ON,            0x0B},
                {ActiveFocuserUtils::Commands::FAN_OFF,           0x0C},
                {ActiveFocuserUtils::Commands::RESET,             0x7E},
                {ActiveFocuserUtils::Commands::DUMMY,             0xFF},
        };