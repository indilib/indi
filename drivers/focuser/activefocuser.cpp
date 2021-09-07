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


#include "activefocuser.h"
#include "activefocuser_utils.h"

static std::unique_ptr<ActiveFocuser> activeFocuser(new ActiveFocuser());

#define DRIVER_NAME "ActiveFocuser"
#define DRIVER_VERSION_MAJOR 1
#define DRIVER_VERSION_MINOR 0

#define VENDOR_ID 0x20E1
#define PRODUCT_ID 0x0002

int MAX_TICKS = 192307;

ActiveFocuser::ActiveFocuser() {

    hid_handle = nullptr;
    setSupportedConnections(CONNECTION_NONE);
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);

}

ActiveFocuser::~ActiveFocuser() {

    if (hid_handle) {

        ActiveFocuserUtils::Poller *poller = ActiveFocuserUtils::Poller::GetInstance(*hid_handle);
        if(poller->IsRunning){
            poller->Stop() ;
        }

        hid_close(hid_handle);

    }

    hid_exit();

}

bool ActiveFocuser::Connect() {

    if (!hid_handle) {

        hid_handle = hid_open(VENDOR_ID, PRODUCT_ID, nullptr);

        if (hid_handle) {

            hid_set_nonblocking(hid_handle, 1);

            ActiveFocuserUtils::Poller *poller = ActiveFocuserUtils::Poller::GetInstance(*hid_handle);
            if (!poller->IsRunning) {
                poller->Start();
            }

            return true;

        } else {

            return false;

        }

    }

    return hid_handle != nullptr;

}

bool ActiveFocuser::Disconnect() {

    if (hid_handle) {

        ActiveFocuserUtils::Poller *poller = ActiveFocuserUtils::Poller::GetInstance(*hid_handle);
        if (poller->IsRunning) {
            poller->Stop();
        }

        hid_close(hid_handle);

    }

    hid_handle = nullptr;

    return true;


}

const char *ActiveFocuser::getDefaultName() {

    return DRIVER_NAME;

}

void ActiveFocuser::ISGetProperties(const char *dev) {

    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    INDI::Focuser::ISGetProperties(dev);

}

bool ActiveFocuser::initProperties() {

    INDI::Focuser::initProperties();

    setVersion(DRIVER_VERSION_MAJOR, DRIVER_VERSION_MINOR);

    // Adding version display

    IUFillText(&HardwareVersionN[0], "Version infos", "", "1.04");
    IUFillTextVector(&HardwareVersionNP, HardwareVersionN, 1, getDeviceName(), "HARDWARE_VERSION", "Hardware Version",
                     MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    std::stringstream softwareVersionStream;
    softwareVersionStream << DRIVER_VERSION_MAJOR << "." << DRIVER_VERSION_MINOR;

    IUFillText(&SoftwareVersionN[0], "Version infos", "", softwareVersionStream.str().c_str());
    IUFillTextVector(&SoftwareVersionNP, SoftwareVersionN, 1, getDeviceName(), "SOFTWARE_VERSION", "Software Version",
                     MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Adding temperature sensor display

    IUFillNumber(&AirTemperatureN[0], "AIR TEMPERATURE", "Celsius", "%6.2f", -50., 70., 0., 0.);
    IUFillNumberVector(&AirTemperatureNP, AirTemperatureN, 1, getDeviceName(), "AIR_TEMPERATURE", "Air Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&TubeTemperatureN[0], "TUBE TEMPERATURE", "Celsius", "%6.2f", -50., 70., 0., 0.);
    IUFillNumberVector(&TubeTemperatureNP, TubeTemperatureN, 1, getDeviceName(), "TUBE_TEMPERATURE", "Tube Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&MirrorTemperatureN[0], "MIRROR TEMPERATURE", "Celsius", "%6.2f", -50., 70., 0., 0.);
    IUFillNumberVector(&MirrorTemperatureNP, MirrorTemperatureN, 1, getDeviceName(), "MIRROR_TEMPERATURE",
                       "Mirror Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Adding FAN control button

    IUFillSwitch(&FanS[0], "FAN_ON", "On", ISS_ON);
    IUFillSwitch(&FanS[1], "FAN_OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&FanSP, FanS, 2, getDeviceName(), "FAN_STATE", "Fan", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    // Setting focus max position constant

    FocusMaxPosN[0].value = MAX_TICKS;
    FocusMaxPosNP.p = IP_RO;
    strncpy(FocusMaxPosN[0].label, "Steps", MAXINDILABEL);

    // Disabling focuser speed

    FocusSpeedN[0].min = 0;
    FocusSpeedN[0].max = 0;
    FocusSpeedN[0].value = 1;
    IUUpdateMinMax(&FocusSpeedNP);

    // Setting default absolute position values

    FocusAbsPosN[0].min = 0.;
    FocusAbsPosN[0].max = MAX_TICKS;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step = 1000.;
    strncpy(FocusAbsPosN[0].label, "Steps", MAXINDILABEL);


    // Setting default relative position values

    FocusRelPosN[0].min = 0.;
    FocusRelPosN[0].max = 5000;
    FocusRelPosN[0].value = 100;
    FocusRelPosN[0].step = 1;
    strncpy(FocusRelPosN[0].label, "Steps", MAXINDILABEL);

    PresetN[0].max = MAX_TICKS;
    PresetN[1].max = MAX_TICKS;
    PresetN[2].max = MAX_TICKS;

    internalTicks = FocusAbsPosN[0].value;

    setDefaultPollingPeriod(750);

    SetTimer(getCurrentPollingPeriod());

    return true;

}

bool ActiveFocuser::updateProperties() {

    INDI::Focuser::updateProperties();

    if (hid_handle) {

        defineProperty(&HardwareVersionNP);
        defineProperty(&SoftwareVersionNP);
        defineProperty(&AirTemperatureNP);
        defineProperty(&TubeTemperatureNP);
        defineProperty(&MirrorTemperatureNP);
        defineProperty(&FanSP);

    } else {

        deleteProperty(HardwareVersionNP.name);
        deleteProperty(SoftwareVersionNP.name);
        deleteProperty(AirTemperatureNP.name);
        deleteProperty(TubeTemperatureNP.name);
        deleteProperty(MirrorTemperatureNP.name);
        deleteProperty(FanSP.name);

    }

    return true;

}

bool ActiveFocuser::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) {

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0) {

        if (strcmp(FanSP.name, name) == 0) {

            IUUpdateSwitch(&FanSP, states, names, n);

            if (FanS[0].s == ISS_ON && !ActiveFocuserUtils::SystemState::GetIsFanOn()) {

                if (hid_handle) {

                    const unsigned char data[3] = {0x01,
                                                   ActiveFocuserUtils::CommandsMap.at(ActiveFocuserUtils::FAN_ON)};
                    hid_write(hid_handle, data, sizeof(data));

                }else{

                    IDLog("Connection failed");

                }

            } else if (FanS[0].s == ISS_OFF && ActiveFocuserUtils::SystemState::GetIsFanOn()) {

                if (hid_handle) {

                    const unsigned char data[3] = {0x01,
                                                   ActiveFocuserUtils::CommandsMap.at(ActiveFocuserUtils::FAN_OFF)};
                    hid_write(hid_handle, data, sizeof(data));

                }else{

                    IDLog("Connection failed");

                }

            }

            FanSP.s = IPS_OK;
            IDSetSwitch(&FanSP, nullptr);

            return true;

        }


    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);

}

bool ActiveFocuser::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {

    if(dev != nullptr && strcmp(dev, getDeviceName()) == 0){
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

}

bool ActiveFocuser::AbortFocuser() {

    if (hid_handle) {

        if(ActiveFocuserUtils::SystemState::GetIsMoving()){

            const unsigned char data[3] = {0x01, ActiveFocuserUtils::CommandsMap.at(ActiveFocuserUtils::STOP)}; // STOP
            hid_write(hid_handle, data, sizeof(data));

            return true;

        }else{

            return false;

        }

    } else {

        IDLog("Connection failed");

        return false;

    }

}

IPState ActiveFocuser::MoveAbsFocuser(uint32_t targetTicks) {

    internalTicks = targetTicks;

    if (targetTicks > (uint32_t)MAX_TICKS) {

        return IPS_ALERT;

    } else {

        if(ActiveFocuserUtils::SystemState::GetIsHold() && !ActiveFocuserUtils::SystemState::GetIsMoving()){

            if(hid_handle){

                unsigned char data[8] = {0x00, 0x05, ActiveFocuserUtils::CommandsMap.at(ActiveFocuserUtils::MOVE), 0x00, 0x00, 0x00, 0x00, 0x00};

                for (int i = 0; i < 4; ++i) {
                    auto num = (targetTicks >> 8 * (3-i) & 255);
                    data[3+i] = num;
                }

                hid_write(hid_handle, data, sizeof(data));

                return IPS_OK;

            }else{

                return IPS_ALERT;

            }

        }else{

            return IPS_BUSY;

        }

    }

}

IPState ActiveFocuser::MoveRelFocuser(INDI::FocuserInterface::FocusDirection dir, uint32_t ticks) {

    FocusRelPosN[0].value = ticks;
    IDSetNumber(&FocusRelPosNP, nullptr);

    int relativeTicks = ((dir == FOCUS_INWARD ? ticks : -ticks));

    double newTicks = internalTicks + relativeTicks;

    return MoveAbsFocuser(newTicks);

}

void ActiveFocuser::TimerHit() {

    if(!hid_handle){

        SetTimer(getCurrentPollingPeriod());

    }else{

        MAX_TICKS = ActiveFocuserUtils::SystemState::GetSpan();
        FocusMaxPosN[0].value = MAX_TICKS;
        IDSetNumber(&FocusMaxPosNP, nullptr);

        PresetN[0].max = MAX_TICKS;
        PresetN[1].max = MAX_TICKS;
        PresetN[2].max = MAX_TICKS;

        HardwareVersionN[0].text = ActiveFocuserUtils::SystemState::GetHardwareRevision();
        IDSetText(&HardwareVersionNP, nullptr);

        FocusAbsPosN[0].value = ActiveFocuserUtils::SystemState::GetCurrentPositionStep();
        IDSetNumber(&FocusAbsPosNP, nullptr);

        internalTicks = FocusAbsPosN[0].value;

        AirTemperatureN[0].value = ActiveFocuserUtils::SystemState::GetAirTemperature();
        IDSetNumber(&AirTemperatureNP, nullptr);
        TubeTemperatureN[0].value = ActiveFocuserUtils::SystemState::GetTubeTemperature();
        IDSetNumber(&TubeTemperatureNP, nullptr);
        MirrorTemperatureN[0].value = ActiveFocuserUtils::SystemState::GetMirrorTemperature();
        IDSetNumber(&MirrorTemperatureNP, nullptr);

        if(ActiveFocuserUtils::SystemState::GetIsFanOn()){
            FanS[0].s = ISS_ON;
            IDSetSwitch(&FanSP, nullptr);
        }else{
            FanS[0].s = ISS_OFF;
            IDSetSwitch(&FanSP, nullptr);
        }

        if(ActiveFocuserUtils::SystemState::GetIsMoving()){
            FocusAbsPosNP.s = IPS_BUSY;
            FocusRelPosNP.s = IPS_BUSY;
        }else{
            FocusAbsPosNP.s = IPS_IDLE;
            FocusRelPosNP.s = IPS_IDLE;
        }

        SetTimer(getCurrentPollingPeriod());

    }

}
