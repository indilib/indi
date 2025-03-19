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

ActiveFocuser::ActiveFocuser()
{

    hid_handle = nullptr;
    setSupportedConnections(CONNECTION_NONE);
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);

}

ActiveFocuser::~ActiveFocuser()
{

    if (hid_handle)
    {

        ActiveFocuserUtils::Poller *poller = ActiveFocuserUtils::Poller::GetInstance(*hid_handle);
        if(poller->IsRunning)
        {
            poller->Stop() ;
        }

        hid_close(hid_handle);

    }

    hid_exit();

}

bool ActiveFocuser::Connect()
{

    if (!hid_handle)
    {

        hid_handle = hid_open(VENDOR_ID, PRODUCT_ID, nullptr);

        if (hid_handle)
        {

            hid_set_nonblocking(hid_handle, 1);

            ActiveFocuserUtils::Poller *poller = ActiveFocuserUtils::Poller::GetInstance(*hid_handle);
            if (!poller->IsRunning)
            {
                poller->Start();
            }

            return true;

        }
        else
        {

            return false;

        }

    }

    return hid_handle != nullptr;

}

bool ActiveFocuser::Disconnect()
{

    if (hid_handle)
    {

        ActiveFocuserUtils::Poller *poller = ActiveFocuserUtils::Poller::GetInstance(*hid_handle);
        if (poller->IsRunning)
        {
            poller->Stop();
        }

        hid_close(hid_handle);

    }

    hid_handle = nullptr;

    return true;


}

const char *ActiveFocuser::getDefaultName()
{

    return DRIVER_NAME;

}

void ActiveFocuser::ISGetProperties(const char *dev)
{

    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    INDI::Focuser::ISGetProperties(dev);

}

bool ActiveFocuser::initProperties()
{

    INDI::Focuser::initProperties();

    setVersion(DRIVER_VERSION_MAJOR, DRIVER_VERSION_MINOR);

    // Adding version display

    HardwareVersionNP[0].fill("Version infos", "", "1.04");
    HardwareVersionNP.fill(getDeviceName(), "HARDWARE_VERSION", "Hardware Version",
                     MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    std::stringstream softwareVersionStream;
    softwareVersionStream << DRIVER_VERSION_MAJOR << "." << DRIVER_VERSION_MINOR;

    SoftwareVersionNP[0].fill("Version infos", "", softwareVersionStream.str().c_str());
    SoftwareVersionNP.fill(getDeviceName(), "SOFTWARE_VERSION", "Software Version",
                     MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Adding temperature sensor display

    AirTemperatureNP[0].fill("AIR TEMPERATURE", "Celsius", "%6.2f", -50., 70., 0., 0.);
    AirTemperatureNP.fill(getDeviceName(), "AIR_TEMPERATURE", "Air Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    TubeTemperatureNP[0].fill("TUBE TEMPERATURE", "Celsius", "%6.2f", -50., 70., 0., 0.);
    TubeTemperatureNP.fill(getDeviceName(), "TUBE_TEMPERATURE", "Tube Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    MirrorTemperatureNP[0].fill("MIRROR TEMPERATURE", "Celsius", "%6.2f", -50., 70., 0., 0.);
    MirrorTemperatureNP.fill(getDeviceName(), "MIRROR_TEMPERATURE",
                       "Mirror Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Adding FAN control button

    FanSP[FAN_ON].fill("FAN_ON", "On", ISS_ON);
    FanSP[FAN_OFF].fill("FAN_OFF", "Off", ISS_OFF);
    FanSP.fill(getDeviceName(), "FAN_STATE", "Fan", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    // Setting focus max position constant

    FocusMaxPosNP[0].setValue(MAX_TICKS);
    FocusMaxPosNP.setPermission(IP_RO);
    strncpy(FocusMaxPosNP[0].label, "Steps", MAXINDILABEL);

    // Disabling focuser speed

    FocusSpeedNP[0].setMin(0);
    FocusSpeedNP[0].setMax(0);
    FocusSpeedNP[0].setValue(1);
    FocusSpeedNP.updateMinMax();

    // Setting default absolute position values

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(MAX_TICKS);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000.);
    strncpy(FocusAbsPosNP[0].label, "Steps", MAXINDILABEL);


    // Setting default relative position values

    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(5000);
    FocusRelPosNP[0].setValue(100);
    FocusRelPosNP[0].setStep(1);
    strncpy(FocusRelPosNP[0].label, "Steps", MAXINDILABEL);

    PresetNP[0].setMax(MAX_TICKS);
    PresetNP[1].setMax(MAX_TICKS);
    PresetNP[2].setMax(MAX_TICKS);

    internalTicks = FocusAbsPosNP[0].getValue();

    setDefaultPollingPeriod(750);

    SetTimer(getCurrentPollingPeriod());

    return true;

}

bool ActiveFocuser::updateProperties()
{

    INDI::Focuser::updateProperties();

    if (hid_handle)
    {

        defineProperty(HardwareVersionNP);
        defineProperty(SoftwareVersionNP);
        defineProperty(AirTemperatureNP);
        defineProperty(TubeTemperatureNP);
        defineProperty(MirrorTemperatureNP);
        defineProperty(FanSP);

    }
    else
    {

        deleteProperty(HardwareVersionNP);
        deleteProperty(SoftwareVersionNP);
        deleteProperty(AirTemperatureNP);
        deleteProperty(TubeTemperatureNP);
        deleteProperty(MirrorTemperatureNP);
        deleteProperty(FanSP);

    }

    return true;

}

bool ActiveFocuser::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        if (FanSP.isNameMatch(name))
        {

            FanSP.update(states, names, n);

            if (FanSP[FAN_ON].getState() == ISS_ON && !ActiveFocuserUtils::SystemState::GetIsFanOn())
            {

                if (hid_handle)
                {

                    const unsigned char data[3] = {0x01,
                                                   ActiveFocuserUtils::CommandsMap.at(ActiveFocuserUtils::FAN_ON)
                                                  };
                    hid_write(hid_handle, data, sizeof(data));

                }
                else
                {

                    IDLog("Connection failed");

                }

            }
            else if (FanSP[FAN_ON].getState() == ISS_OFF && ActiveFocuserUtils::SystemState::GetIsFanOn())
            {

                if (hid_handle)
                {

                    const unsigned char data[3] = {0x01,
                                                   ActiveFocuserUtils::CommandsMap.at(ActiveFocuserUtils::FAN_OFF)
                                                  };
                    hid_write(hid_handle, data, sizeof(data));

                }
                else
                {

                    IDLog("Connection failed");

                }

            }

            FanSP.setState(IPS_OK);
            FanSP.apply();

            return true;

        }


    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);

}

bool ActiveFocuser::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{

    if(dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

}

bool ActiveFocuser::AbortFocuser()
{

    if (hid_handle)
    {

        if(ActiveFocuserUtils::SystemState::GetIsMoving())
        {

            const unsigned char data[3] = {0x01, ActiveFocuserUtils::CommandsMap.at(ActiveFocuserUtils::STOP)}; // STOP
            hid_write(hid_handle, data, sizeof(data));

            return true;

        }
        else
        {

            return false;

        }

    }
    else
    {

        IDLog("Connection failed");

        return false;

    }

}

IPState ActiveFocuser::MoveAbsFocuser(uint32_t targetTicks)
{

    internalTicks = targetTicks;

    if (targetTicks > (uint32_t)MAX_TICKS)
    {

        return IPS_ALERT;

    }
    else
    {

        if(ActiveFocuserUtils::SystemState::GetIsHold() && !ActiveFocuserUtils::SystemState::GetIsMoving())
        {

            if(hid_handle)
            {

                unsigned char data[8] = {0x00, 0x05, ActiveFocuserUtils::CommandsMap.at(ActiveFocuserUtils::MOVE), 0x00, 0x00, 0x00, 0x00, 0x00};

                for (int i = 0; i < 4; ++i)
                {
                    auto num = (targetTicks >> 8 * (3 - i) & 255);
                    data[3 + i] = num;
                }

                hid_write(hid_handle, data, sizeof(data));

                return IPS_OK;

            }
            else
            {

                return IPS_ALERT;

            }

        }
        else
        {

            return IPS_BUSY;

        }

    }

}

IPState ActiveFocuser::MoveRelFocuser(INDI::FocuserInterface::FocusDirection dir, uint32_t ticks)
{

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.apply();

    int relativeTicks = ((dir == FOCUS_INWARD ? ticks : -ticks));

    double newTicks = internalTicks + relativeTicks;

    return MoveAbsFocuser(newTicks);

}

void ActiveFocuser::TimerHit()
{

    if(!hid_handle)
    {

        SetTimer(getCurrentPollingPeriod());

    }
    else
    {

        MAX_TICKS = ActiveFocuserUtils::SystemState::GetSpan();
        FocusMaxPosNP[0].setValue(MAX_TICKS);
        FocusMaxPosNP.apply();

        PresetNP[0].setMax(MAX_TICKS);
        PresetNP[1].setMax(MAX_TICKS);
        PresetNP[2].setMax(MAX_TICKS);

        HardwareVersionNP[0].setText(ActiveFocuserUtils::SystemState::GetHardwareRevision());
        HardwareVersionNP.apply();

        FocusAbsPosNP[0].setValue(ActiveFocuserUtils::SystemState::GetCurrentPositionStep());
        FocusAbsPosNP.apply();

        internalTicks = FocusAbsPosNP[0].getValue();

        AirTemperatureNP[0].setValue(ActiveFocuserUtils::SystemState::GetAirTemperature());
        AirTemperatureNP.apply();
        TubeTemperatureNP[0].setValue(ActiveFocuserUtils::SystemState::GetTubeTemperature());
        TubeTemperatureNP.apply();
        MirrorTemperatureNP[0].setValue(ActiveFocuserUtils::SystemState::GetMirrorTemperature());
        MirrorTemperatureNP.apply();

        if(ActiveFocuserUtils::SystemState::GetIsFanOn())
        {
            FanSP[FAN_ON].setState(ISS_ON);
            FanSP.apply(nullptr);
        }
        else
        {
            FanSP[FAN_ON].setState(ISS_OFF);
            FanSP.apply(nullptr);
        }

        if(ActiveFocuserUtils::SystemState::GetIsMoving())
        {
            FocusAbsPosNP.setState(IPS_BUSY);
            FocusRelPosNP.setState(IPS_BUSY);
        }
        else
        {
            FocusAbsPosNP.setState(IPS_IDLE);
            FocusRelPosNP.setState(IPS_IDLE);
        }

        SetTimer(getCurrentPollingPeriod());

    }

}
