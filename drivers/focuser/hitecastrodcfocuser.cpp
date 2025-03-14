/*******************************************************************************
  Copyright(c) 2016 Andy Kirkham. All rights reserved.

 HitecAstroDCFocuser Focuser

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

#include "hitecastrodcfocuser.h"

#include <cstring>
#include <memory>

#define HID_TIMEOUT    10000 /* 10s */
#define FUDGE_FACTOR_H 1000
#define FUDGE_FACTOR_L 885

#define FOCUS_SETTINGS_TAB "Settings"

static std::unique_ptr<HitecAstroDCFocuser> hitecastroDcFocuser(new HitecAstroDCFocuser());

HitecAstroDCFocuser::HitecAstroDCFocuser() : m_HIDHandle(nullptr)
{
    FI::SetCapability(FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_REVERSE);
    setSupportedConnections(CONNECTION_NONE);
    setVersion(0, 2);
}

HitecAstroDCFocuser::~HitecAstroDCFocuser()
{
    if (m_HIDHandle != nullptr)
    {
        hid_close(m_HIDHandle);
        m_HIDHandle = nullptr;
    }
}

bool HitecAstroDCFocuser::Connect()
{
    if (hid_init() != 0)
    {
        LOG_ERROR("hid_init() failed.");
    }

    m_HIDHandle = hid_open(0x04D8, 0xFAC2, nullptr);

    if (m_HIDHandle == nullptr)
        m_HIDHandle = hid_open(0x04D8, 0xF53A, nullptr);

    LOG_DEBUG(m_HIDHandle ? "HitecAstroDCFocuser opened." : "HitecAstroDCFocuser failed.");

    if (m_HIDHandle != nullptr)
    {
        LOG_INFO("Experimental driver. Report issues to https://github.com/A-j-K/hitecastrodcfocuser/issues");
        SetTimer(getCurrentPollingPeriod());
        return true;
    }

    LOGF_ERROR("Failed to connect to focuser: %s", hid_error(m_HIDHandle));
    return false;
}

bool HitecAstroDCFocuser::Disconnect()
{
    if (m_HIDHandle != nullptr)
    {
        hid_close(m_HIDHandle);
        m_HIDHandle = nullptr;
    }

    LOG_DEBUG("focuser is offline.");
    return true;
}

const char *HitecAstroDCFocuser::getDefaultName()
{
    return "HitecAstro DC";
}

bool HitecAstroDCFocuser::initProperties()
{
    INDI::Focuser::initProperties();

    //IDMessage(getDeviceName(), "HitecAstroDCFocuser::initProperties()");

    addDebugControl();
    //addSimulationControl();

    //    IUFillNumber(&MaxPositionN[0], "Steps", "", "%.f", 0, 500000, 0., 10000);
    //    IUFillNumberVector(&MaxPositionNP, MaxPositionN, 1, getDeviceName(), "MAX_POSITION", "Max position",
    //                       FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    SlewSpeedNP[0].fill("Steps/sec", "", "%.f", 1, 100, 0., 50);
    SlewSpeedNP.fill(getDeviceName(), "SLEW_SPEED", "Slew speed", MAIN_CONTROL_TAB,
                       IP_RW, 0, IPS_IDLE);

    //    IUFillSwitch(&ReverseDirectionS[0], "ENABLED", "Reverse direction", ISS_OFF);
    //    IUFillSwitchVector(&ReverseDirectionSP, ReverseDirectionS, 1, getDeviceName(), "REVERSE_DIRECTION",
    //                       "Reverse direction", OPTIONS_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    FocusSpeedNP[0].setValue(100.);
    FocusSpeedNP[0].setMin(1.);
    FocusSpeedNP[0].setMax(100.);
    FocusSpeedNP[0].setValue(100.);

    FocusRelPosNP[0].setMin(1);
    FocusRelPosNP[0].setMax(50000);
    FocusRelPosNP[0].setStep(1000);
    FocusRelPosNP[0].setValue(1000);

    setDefaultPollingPeriod(500);

    return true;
}

bool HitecAstroDCFocuser::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        //defineProperty(&MaxPositionNP);
        defineProperty(SlewSpeedNP);
        //defineProperty(&ReverseDirectionSP);
    }
    else
    {
        //deleteProperty(MaxPositionNP.name);
        deleteProperty(SlewSpeedNP);
        //deleteProperty(ReverseDirectionSP.name);
    }

    return true;
}

void HitecAstroDCFocuser::TimerHit()
{
    if (m_State == SLEWING && m_Duration > 0)
    {
        --m_Duration;
        if (m_Duration == 0)
        {
            int rc;
            unsigned char command[8] = {0};
            m_State = IDLE;
            memset(command, 0, 8);
            command[0] = m_StopChar;
            rc         = hid_write(m_HIDHandle, command, 8);
            if (rc < 0)
            {
                LOGF_DEBUG("::MoveFocuser() fail (%s)", hid_error(m_HIDHandle));
            }
            hid_read_timeout(m_HIDHandle, command, 8, 1000);
        }
    }
    SetTimer(1);
}

bool HitecAstroDCFocuser::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //        if (strcmp(name, MaxPositionNP.name) == 0)
        //        {
        //            IUUpdateNumber(&MaxPositionNP, values, names, n);
        //            MaxPositionNP.s = IPS_OK;
        //            IDSetNumber(&MaxPositionNP, nullptr);
        //            return true;
        //        }
        if (SlewSpeedNP.isNameMatch(name))
        {
            if (values[0] > 100)
            {
                SlewSpeedNP.setState(IPS_ALERT);
                return false;
            }
            SlewSpeedNP.update(values, names, n);
            SlewSpeedNP.setState(IPS_OK);
            SlewSpeedNP.apply();
            return true;
        }
    }
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

IPState HitecAstroDCFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int rc, speed = (int)SlewSpeedNP[0].getValue();
    //    int32_t iticks = ticks;
    unsigned char command[8] = {0};
    IPState rval;

    LOGF_DEBUG("::move() begin %d ticks at speed %d", ticks, speed);

    if (m_HIDHandle == nullptr)
    {
        LOG_DEBUG("::move() bad handle");
        return IPS_ALERT;
    }

    //    FocusRelPosNP.setState(IPS_BUSY);
    //    FocusRelPosNP.apply();

    // JM 2017-03-16: iticks is not used, FIXME.
    //    if (dir == FOCUS_INWARD)
    //    {
    //        iticks = ticks * -1;
    //    }

    //if (ReverseDirectionS[0].s == ISS_ON)
    if (FocusReverseSP[INDI_ENABLED].getState() == ISS_ON)
    {
        dir = dir == FOCUS_INWARD ? FOCUS_OUTWARD : FOCUS_INWARD;
    }

    if (speed > 100)
    {
        LOGF_DEBUG("::move() over speed %d, limiting to 100", ticks, speed);
        speed = 100;
    }

    ticks *= FUDGE_FACTOR_H;
    ticks /= FUDGE_FACTOR_L;

    memset(command, 0, 8);
    command[0] = dir == FOCUS_INWARD ? 0x50 : 0x52;
    command[1] = (unsigned char)((ticks >> 8) & 0xFF);
    command[2] = (unsigned char)(ticks & 0xFF);
    command[3] = 0x03;
    command[4] = (unsigned char)(speed & 0xFF);
    command[5] = 0;
    command[6] = 0;
    command[7] = 0;

    LOGF_DEBUG("==> TX %2.2x %2.2x%2.2x %2.2x %2.2x %2.2x%2.2x%2.2x", command[0], command[1],
               command[2], command[3], command[4], command[5], command[6], command[7]);

    rc = hid_write(m_HIDHandle, command, 8);
    if (rc < 0)
    {
        LOGF_DEBUG("::MoveRelFocuser() fail (%s)", hid_error(m_HIDHandle));
        return IPS_ALERT;
    }

    //FocusRelPosNP.setState(IPS_BUSY);

    memset(command, 0, 8);
    hid_read_timeout(m_HIDHandle, command, 8, HID_TIMEOUT);
    LOGF_DEBUG("==> RX %2.2x %2.2x%2.2x %2.2x %2.2x %2.2x%2.2x%2.2x", command[0], command[1],
               command[2], command[3], command[4], command[5], command[6], command[7]);

    rval = command[1] == 0x21 ? IPS_OK : IPS_ALERT;

    //FocusRelPosNP.setState(rval);
    //FocusRelPosNP.apply();

    return rval;
}

IPState HitecAstroDCFocuser::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    int rc;
    unsigned char command[8] = {0};
    IPState rval;

    LOGF_DEBUG("::MoveFocuser(%d %d %d)", dir, speed, duration);

    if (m_HIDHandle == nullptr)
    {
        return IPS_ALERT;
    }

    FocusSpeedNP.setState(IPS_BUSY);
    FocusSpeedNP.apply();

    if (FocusReverseSP[INDI_ENABLED].getState() == ISS_ON)
    {
        dir = (dir == FOCUS_INWARD) ? FOCUS_OUTWARD : FOCUS_INWARD;
    }

    if (speed > 100)
    {
        LOGF_DEBUG("::MoveFocuser() over speed %d, limiting to 100", speed);
        speed = 100;
    }

    m_StopChar = dir == FOCUS_INWARD ? 0xB0 : 0xBA;

    memset(command, 0, 8);
    command[0] = dir == FOCUS_INWARD ? 0x54 : 0x56;
    command[1] = (unsigned char)((speed >> 8) & 0xFF);
    command[2] = (unsigned char)(speed & 0xFF);
    command[3] = 0x03;
    command[4] = 0;
    command[5] = 0;
    command[6] = 0;
    command[7] = 0;

    LOGF_DEBUG("==> TX %2.2x %2.2x%2.2x %2.2x %2.2x %2.2x%2.2x%2.2x", command[0], command[1],
               command[2], command[3], command[4], command[5], command[6], command[7]);

    rc = hid_write(m_HIDHandle, command, 8);
    if (rc < 0)
    {
        LOGF_DEBUG("::MoveFocuser() fail (%s)", hid_error(m_HIDHandle));
        return IPS_ALERT;
    }

    memset(command, 0, 8);
    hid_read_timeout(m_HIDHandle, command, 8, HID_TIMEOUT);
    LOGF_DEBUG("==> RX %2.2x %2.2x%2.2x %2.2x %2.2x %2.2x%2.2x%2.2x", command[0], command[1],
               command[2], command[3], command[4], command[5], command[6], command[7]);

    rval = command[1] == 0x24 ? IPS_OK : IPS_ALERT;

    FocusSpeedNP.setState(rval);
    FocusSpeedNP.apply();

    m_Duration = duration;
    m_State    = SLEWING;

    return IPS_BUSY;
}

bool HitecAstroDCFocuser::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);

    //IUSaveConfigNumber(fp, &MaxPositionNP);
    SlewSpeedNP.save(fp);
    //IUSaveConfigSwitch(fp, &ReverseDirectionSP);

    return true;
}
