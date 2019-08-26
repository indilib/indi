/*
    ZWO EFA Focuser
    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "asi_focuser.h"
#include "EAF_focuser.h"
#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

#define MAX_DEVICES 4
#define FOCUS_SETTINGS_TAB "Settings"

static int iAvailableFocusersCount;
static ASIEAF * focusers[MAX_DEVICES];

void ASI_EAF_ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        iAvailableFocusersCount = 0;

        iAvailableFocusersCount = EAFGetNum();
        if (iAvailableFocusersCount > MAX_DEVICES)
            iAvailableFocusersCount = MAX_DEVICES;

        if (iAvailableFocusersCount <= 0)
        {
            IDLog("No ASI EAF detected.");
        }
        else
        {
            int iAvailableFocusersCount_ok = 0;
            for (int i = 0; i < iAvailableFocusersCount; i++)
            {
                int id;
                EAF_ERROR_CODE result = EAFGetID(i, &id);
                if (result != EAF_SUCCESS)
                {
                    IDLog("ERROR: ASI EAF %d EAFGetID error %d.", i + 1, result);
                    continue;
                }

                // Open device
                result = EAFOpen(id);
                if (result != EAF_SUCCESS)
                {
                    IDLog("ERROR: ASI EAF %d Failed to open device %d.", i + 1, result);
                    continue;
                }

                EAF_INFO info;
                result = EAFGetProperty(id, &info);
                if (result != EAF_SUCCESS)
                {
                    IDLog("ERROR: ASI EAF %d EAFGetProperty error %d.", i + 1, result);
                    continue;
                }
                EAFClose(id);
                focusers[i] = new ASIEAF(id, info.Name, info.MaxStep);
                iAvailableFocusersCount_ok++;
            }
            IDLog("%d ASI EAF attached out of %d detected.", iAvailableFocusersCount_ok, iAvailableFocusersCount);
            if (iAvailableFocusersCount == iAvailableFocusersCount_ok)
                isInit = true;
        }
    }
}

void ISGetProperties(const char * dev)
{
    ASI_EAF_ISInit();
    for (int i = 0; i < iAvailableFocusersCount; i++)
    {
        ASIEAF * focuser = focusers[i];
        if (dev == nullptr || !strcmp(dev, focuser->m_Name))
        {
            focuser->ISGetProperties(dev);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int num)
{
    ASI_EAF_ISInit();
    for (int i = 0; i < iAvailableFocusersCount; i++)
    {
        ASIEAF * focuser = focusers[i];
        if (dev == nullptr || !strcmp(dev, focuser->m_Name))
        {
            focuser->ISNewSwitch(dev, name, states, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewText(const char * dev, const char * name, char * texts[], char * names[], int num)
{
    ASI_EAF_ISInit();
    for (int i = 0; i < iAvailableFocusersCount; i++)
    {
        ASIEAF * focuser = focusers[i];
        if (dev == nullptr || !strcmp(dev, focuser->m_Name))
        {
            focuser->ISNewText(dev, name, texts, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewNumber(const char * dev, const char * name, double values[], char * names[], int num)
{
    ASI_EAF_ISInit();
    for (int i = 0; i < iAvailableFocusersCount; i++)
    {
        ASIEAF * focuser = focusers[i];
        if (dev == nullptr || !strcmp(dev, focuser->m_Name))
        {
            focuser->ISNewNumber(dev, name, values, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewBLOB(const char * dev, const char * name, int sizes[], int blobsizes[], char * blobs[], char * formats[],
               char * names[], int n)
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

void ISSnoopDevice(XMLEle * root)
{
    ASI_EAF_ISInit();
    for (int i = 0; i < iAvailableFocusersCount; i++)
    {
        ASIEAF * focuser = focusers[i];
        focuser->ISSnoopDevice(root);
    }
}

ASIEAF::ASIEAF(int id, const char * name, const int maxSteps) : m_ID(id), m_MaxSteps(maxSteps)
{
    // Can move in Absolute & Relative motions, can AbortFocuser motion, and can reverse.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT    |
                      FOCUSER_CAN_REVERSE  |
                      FOCUSER_CAN_SYNC     |
                      FOCUSER_HAS_BACKLASH);

    // Just USB
    setSupportedConnections(CONNECTION_NONE);

    if (iAvailableFocusersCount > 1)
        snprintf(m_Name, MAXINDINAME, "ASI %s %d", name, id);
    else
        snprintf(m_Name, MAXINDIDEVICE, "ASI %s", name);

    FocusAbsPosN[0].max = maxSteps;
}

bool ASIEAF::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Focus motion beep
    IUFillSwitch(&BeepS[BEEP_ON], "ON", "On", ISS_ON);
    IUFillSwitch(&BeepS[BEEL_OFF], "OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&BeepSP, BeepS, 2, getDeviceName(), "FOCUS_BEEP", "Beep", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Enable/Disable backlash
    //    IUFillSwitch(&BacklashCompensationS[BACKLASH_ENABLED], "Enable", "", ISS_OFF);
    //    IUFillSwitch(&BacklashCompensationS[BACKLASH_DISABLED], "Disable", "", ISS_ON);
    //    IUFillSwitchVector(&FocuserBacklashSP, BacklashCompensationS, 2, getDeviceName(), "Backlash Compensation", "",
    //                       FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    //    // Backlash Value
    //    IUFillNumber(&BacklashN[0], "Value", "", "%.f", 0, 9999, 100., 0.);
    //    IUFillNumberVector(&BacklashNP, BacklashN, 1, getDeviceName(), "Backlash", "", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);
    FocusBacklashN[0].min = 0;
    FocusBacklashN[0].max = 9999;
    FocusBacklashN[0].step = 100;
    FocusBacklashN[0].value = 0;

    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = m_MaxSteps / 2.0;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = FocusRelPosN[0].max / 20;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = m_MaxSteps;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = m_MaxSteps / 20.0;

    setDefaultPollingPeriod(500);

    addDebugControl();

    return true;
}

bool ASIEAF::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        float temperature = -273;
        EAFGetTemp(m_ID, &temperature);

        if (temperature != -273)
        {
            TemperatureN[0].value = temperature;
            TemperatureNP.s = IPS_OK;
            defineNumber(&TemperatureNP);
        }

        defineSwitch(&BeepSP);
        //        defineSwitch(&FocuserBacklashSP);
        //        defineNumber(&BacklashNP);

        GetFocusParams();

        LOG_INFO("ASI EAF parameters updated, focuser ready for use.");

        SetTimer(POLLMS);
    }
    else
    {
        if (TemperatureNP.s != IPS_IDLE)
            deleteProperty(TemperatureNP.name);
        deleteProperty(BeepSP.name);
        //        deleteProperty(FocuserBacklashSP.name);
        //        deleteProperty(BacklashNP.name);
    }

    return true;
}

const char * ASIEAF::getDefaultName()
{
    return "ASI EAF";
}

bool ASIEAF::Connect()
{
    EAF_ERROR_CODE rc = EAFOpen(m_ID);

    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to connect to ASI EAF focuser ID: %d (%d)", m_ID, rc);
        return false;
    }
    AbortFocuser();
    return readMaxPosition();
}

bool ASIEAF::Disconnect()
{
    return (EAFClose(m_ID) == EAF_SUCCESS);
}

bool ASIEAF::readTemperature()
{
    float temperature = 0;
    EAF_ERROR_CODE rc = EAFGetTemp(m_ID, &temperature);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read temperature. Error: %d", rc);
        return false;
    }

    TemperatureN[0].value = temperature;
    return true;
}

bool ASIEAF::readPosition()
{
    int step = 0;
    EAF_ERROR_CODE rc = EAFGetPosition(m_ID, &step);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read position. Error: %d", rc);
        return false;
    }
    FocusAbsPosN[0].value = step;
    return true;
}

bool ASIEAF::readMaxPosition()
{
    int max = 0;
    EAF_ERROR_CODE rc = EAFGetMaxStep(m_ID, &max);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read max step. Error: %d", rc);
        return false;
    }
    FocusAbsPosN[0].max = max;
    return true;
}

bool ASIEAF::SetFocuserMaxPosition(uint32_t ticks)
{
    EAF_ERROR_CODE rc = EAFSetMaxStep(m_ID, ticks);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set max step. Error: %d", rc);
        return false;
    }
    return true;
}

bool ASIEAF::readReverse()
{
    bool reversed = false;
    EAF_ERROR_CODE rc = EAFGetReverse(m_ID, &reversed);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read reversed status. Error: %d", rc);
        return false;
    }

    FocusReverseS[REVERSED_ENABLED].s  = reversed ? ISS_ON : ISS_OFF;
    FocusReverseS[REVERSED_DISABLED].s = reversed ? ISS_OFF : ISS_ON;
    FocusReverseSP.s = IPS_OK;
    return true;
}

bool ASIEAF::readBacklash()
{
    int backv = 0;
    EAF_ERROR_CODE rc = EAFGetBacklash(m_ID, &backv);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read backlash. Error: %d", rc);
        return false;
    }
    FocusBacklashN[0].value = backv;
    FocusBacklashNP.s = IPS_OK;
    return true;
}

//bool ASIEAF::setBacklash(uint32_t ticks)
bool ASIEAF::SetFocuserBacklash(int32_t steps)
{
    EAF_ERROR_CODE rc = EAFSetBacklash(m_ID, steps);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set backlash compensation. Error: %d", rc);
        return false;
    }
    return true;
}

bool ASIEAF::readBeep()
{
    bool beep = false;
    EAF_ERROR_CODE rc = EAFGetBeep(m_ID, &beep);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read beep status. Error: %d", rc);
        return false;
    }

    BeepS[REVERSED_ENABLED].s  = beep ? ISS_ON : ISS_OFF;
    BeepS[REVERSED_DISABLED].s = beep ? ISS_OFF : ISS_ON;
    BeepSP.s = IPS_OK;

    return true;
}

bool ASIEAF::ReverseFocuser(bool enabled)
{
    EAF_ERROR_CODE rc = EAFSetReverse(m_ID, enabled);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set reversed status. Error: %d", rc);
        return false;
    }

    return true;
}

bool ASIEAF::isMoving()
{
    bool moving = false;
    EAF_ERROR_CODE rc = EAFIsMoving(m_ID, &moving);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read motion status. Error: %d", rc);
        return false;
    }
    return moving;
}

bool ASIEAF::SyncFocuser(uint32_t ticks)
{
    EAF_ERROR_CODE rc = EAFResetPostion(m_ID, ticks);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to sync focuser. Error: %d", rc);
        return false;
    }
    return true;
}

bool ASIEAF::gotoAbsolute(uint32_t position)
{
    EAF_ERROR_CODE rc = EAFMove(m_ID, position);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set position. Error: %d", rc);
        return false;
    }
    return true;
}


bool ASIEAF::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Turn on/off beep
        if (!strcmp(name, BeepSP.name))
        {
            EAF_ERROR_CODE rc = EAF_SUCCESS;
            if (!strcmp(BeepS[BEEP_ON].name, IUFindOnSwitchName(states, names, n)))
                rc = EAFSetBeep(m_ID, true);
            else
                rc = EAFSetBeep(m_ID, false);

            if (rc == EAF_SUCCESS)
            {
                IUUpdateSwitch(&BeepSP, states, names, n);
                BeepSP.s = IPS_OK;
            }
            else
            {
                BeepSP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set beep state. Error: %d", rc);
            }

            IDSetSwitch(&BeepSP, nullptr);
            return true;
        }
        // Backlash
        //        else if (!strcmp(name, FocuserBacklashSP.name))
        //        {
        //            IUUpdateSwitch(&FocuserBacklashSP, states, names, n);
        //            bool rc = false;
        //            if (IUFindOnSwitchIndex(&FocuserBacklashSP) == BACKLASH_ENABLED)
        //                rc = setBacklash(BacklashN[0].value);
        //            else
        //                rc = setBacklash(0);

        //            FocuserBacklashSP.s = rc ? IPS_OK : IPS_ALERT;
        //            IDSetSwitch(&FocuserBacklashSP, nullptr);
        //            return true;
        //        }

    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

//bool ASIEAF::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
//{
//    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
//    {
//        if (strcmp(name, BacklashNP.name) == 0)
//        {
//            IUUpdateNumber(&BacklashNP, values, names, n);
//            bool rc = setBacklash(BacklashN[0].value);
//            BacklashNP.s = rc ? IPS_OK : IPS_ALERT;

//            IDSetNumber(&BacklashNP, nullptr);
//            return true;
//        }
//    }

//    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
//}

void ASIEAF::GetFocusParams()
{
    if (readPosition())
        IDSetNumber(&FocusAbsPosNP, nullptr);

    if (readReverse())
        IDSetSwitch(&FocusReverseSP, nullptr);

    if (readBeep())
        IDSetSwitch(&BeepSP, nullptr);

    if (readBacklash())
        IDSetNumber(&FocusBacklashNP, nullptr);
}

IPState ASIEAF::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    if (!gotoAbsolute(targetPos))
        return IPS_ALERT;

    return IPS_BUSY;
}

IPState ASIEAF::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosN[0].max), newPosition));
    if (!gotoAbsolute(newPosition))
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s       = IPS_BUSY;

    return IPS_BUSY;
}

void ASIEAF::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(POLLMS);
        return;
    }

    bool rc = readPosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 5)
        {
            IDSetNumber(&FocusAbsPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    rc = readTemperature();
    if (rc)
    {
        if (fabs(lastTemperature - TemperatureN[0].value) >= 0.1)
        {
            IDSetNumber(&TemperatureNP, nullptr);
            lastTemperature = TemperatureN[0].value;
        }
    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            IDSetNumber(&FocusRelPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(POLLMS);
}

bool ASIEAF::AbortFocuser()
{
    EAF_ERROR_CODE rc = EAFStop(m_ID);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to stop focuser. Error: %d", rc);
        return false;
    }
    return true;
}
