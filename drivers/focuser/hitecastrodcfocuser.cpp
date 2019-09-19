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

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
    hitecastroDcFocuser->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    hitecastroDcFocuser->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    hitecastroDcFocuser->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    hitecastroDcFocuser->ISNewNumber(dev, name, values, names, n);
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
    hitecastroDcFocuser->ISSnoopDevice(root);
}

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
        SetTimer(POLLMS);
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

    IUFillNumber(&SlewSpeedN[0], "Steps/sec", "", "%.f", 1, 100, 0., 50);
    IUFillNumberVector(&SlewSpeedNP, SlewSpeedN, 1, getDeviceName(), "SLEW_SPEED", "Slew speed", MAIN_CONTROL_TAB,
                       IP_RW, 0, IPS_IDLE);

    //    IUFillSwitch(&ReverseDirectionS[0], "ENABLED", "Reverse direction", ISS_OFF);
    //    IUFillSwitchVector(&ReverseDirectionSP, ReverseDirectionS, 1, getDeviceName(), "REVERSE_DIRECTION",
    //                       "Reverse direction", OPTIONS_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    FocusSpeedN[0].value = 100.;
    FocusSpeedN[0].min   = 1.;
    FocusSpeedN[0].max   = 100.;
    FocusSpeedN[0].value = 100.;

    FocusRelPosN[0].min   = 1;
    FocusRelPosN[0].max   = 50000;
    FocusRelPosN[0].step  = 1000;
    FocusRelPosN[0].value = 1000;

    setDefaultPollingPeriod(500);

    return true;
}

bool HitecAstroDCFocuser::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        //defineNumber(&MaxPositionNP);
        defineNumber(&SlewSpeedNP);
        //defineSwitch(&ReverseDirectionSP);
    }
    else
    {
        //deleteProperty(MaxPositionNP.name);
        deleteProperty(SlewSpeedNP.name);
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
        if (strcmp(name, SlewSpeedNP.name) == 0)
        {
            if (values[0] > 100)
            {
                SlewSpeedNP.s = IPS_ALERT;
                return false;
            }
            IUUpdateNumber(&SlewSpeedNP, values, names, n);
            SlewSpeedNP.s = IPS_OK;
            IDSetNumber(&SlewSpeedNP, nullptr);
            return true;
        }
    }
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

IPState HitecAstroDCFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int rc, speed = (int)SlewSpeedN[0].value;
    //    int32_t iticks = ticks;
    unsigned char command[8] = {0};
    IPState rval;

    LOGF_DEBUG("::move() begin %d ticks at speed %d", ticks, speed);

    if (m_HIDHandle == nullptr)
    {
        LOG_DEBUG("::move() bad handle");
        return IPS_ALERT;
    }

    //    FocusRelPosNP.s = IPS_BUSY;
    //    IDSetNumber(&FocusRelPosNP, nullptr);

    // JM 2017-03-16: iticks is not used, FIXME.
    //    if (dir == FOCUS_INWARD)
    //    {
    //        iticks = ticks * -1;
    //    }

    //if (ReverseDirectionS[0].s == ISS_ON)
    if (FocusReverseS[REVERSED_ENABLED].s == ISS_ON)
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

    //FocusRelPosNP.s = IPS_BUSY;

    memset(command, 0, 8);
    hid_read_timeout(m_HIDHandle, command, 8, HID_TIMEOUT);
    LOGF_DEBUG("==> RX %2.2x %2.2x%2.2x %2.2x %2.2x %2.2x%2.2x%2.2x", command[0], command[1],
               command[2], command[3], command[4], command[5], command[6], command[7]);

    rval = command[1] == 0x21 ? IPS_OK : IPS_ALERT;

    //FocusRelPosNP.s = rval;
    //IDSetNumber(&FocusRelPosNP, nullptr);

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

    FocusSpeedNP.s = IPS_BUSY;
    IDSetNumber(&FocusSpeedNP, nullptr);

    if (FocusReverseS[REVERSED_ENABLED].s == ISS_ON)
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

    FocusSpeedNP.s = rval;
    IDSetNumber(&FocusSpeedNP, nullptr);

    m_Duration = duration;
    m_State    = SLEWING;

    return IPS_BUSY;
}

bool HitecAstroDCFocuser::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);

    //IUSaveConfigNumber(fp, &MaxPositionNP);
    IUSaveConfigNumber(fp, &SlewSpeedNP);
    //IUSaveConfigSwitch(fp, &ReverseDirectionSP);

    return true;
}
