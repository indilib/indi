/*******************************************************************************
 Copyright(c) 2019 Wouter van Reeven. All rights reserved.

 Shoestring FCUSB Focuser

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "fcusb.h"

#include <cmath>
#include <cstring>
#include <memory>

#define FOCUS_SETTINGS_TAB "Settings"

static std::unique_ptr<FCUSB> fcusb(new FCUSB());

const std::map<FCUSB::FC_MOTOR, std::string> FCUSB::MotorMap =
{
    {FCUSB::FC_NOT_MOVING, "Idle"},
    {FCUSB::FC_MOVING_IN, "Moving Inwards"},
    {FCUSB::FC_MOVING_OUT, "Moving Outwards"},
};

void ISGetProperties(const char *dev)
{
    fcusb->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    fcusb->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    fcusb->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    fcusb->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    fcusb->ISSnoopDevice(root);
}

FCUSB::FCUSB()
{
    setVersion(0, 1);

    FI::SetCapability(FOCUSER_HAS_VARIABLE_SPEED | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC);
    setSupportedConnections(CONNECTION_NONE);
}

bool FCUSB::Connect()
{
    if (isSimulation())
    {
        SetTimer(POLLMS);
        return true;
    }

    handle = hid_open(0x134A, 0x9023, nullptr);

    if (handle == nullptr)
    {
        LOG_ERROR("No FCUSB focuser found.");
        return false;
    }
    else
    {
        SetTimer(POLLMS);
    }

    return (handle != nullptr);
}

bool FCUSB::Disconnect()
{
    if (isSimulation() == false)
    {
        hid_close(handle);
        hid_exit();
    }

    return true;
}

const char *FCUSB::getDefaultName()
{
    return "FCUSB";
}

bool FCUSB::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedN[0].min = 0;
    FocusSpeedN[0].max = 255;

    addSimulationControl();

    return true;
}

void FCUSB::TimerHit()
{
    if (!isConnected())
        return;


    SetTimer(POLLMS);
}

bool FCUSB::getStatus()
{
    // 2 bytes response
    uint8_t status[2] = {0};

    int rc = hid_read(handle, status, 2);

    if (rc < 0)
    {
        LOGF_ERROR("getStatus: Error reading from FCUSB to device (%s)", hid_error(handle));
        return false;
    }

    LOGF_DEBUG("RES <%#02X %#02X>", status[0], status[1]);

    // Update speed (PWM) if it was changed.
    if (fabs(FocusSpeedN[0].value - status[1]) > 0)
    {
        FocusSpeedN[0].value = status[1];
        LOGF_DEBUG("PWM: %d%", FocusSpeedN[0].value);
    }

    return true;
}

bool FCUSB::AbortFocuser()
{
    // TODO
    return false;
}

bool FCUSB::SetFocuserSpeed(int speed)
{
    // TODO
    return false;
}

IPState FCUSB::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    // TODO
    return IPS_ALERT;
}
