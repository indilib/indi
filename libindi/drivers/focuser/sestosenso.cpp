/*
    SestoSenso Focuser
    Copyright (C) 2018 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "sestosenso.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

static std::unique_ptr<SestoSenso> sesto(new SestoSenso());

void ISGetProperties(const char *dev)
{
    sesto->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    sesto->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    sesto->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    sesto->ISNewNumber(dev, name, values, names, n);
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
    sesto->ISSnoopDevice(root);
}

SestoSenso::SestoSenso()
{
    setVersion(1, 2);
    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC | FOCUSER_CAN_REVERSE);
}

bool SestoSenso::initProperties()
{
    INDI::Focuser::initProperties();

    // Firmware Information
    IUFillText(&FirmwareT[0], "VERSION", "Version", "");
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "FOCUS_FIRMWARE", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Limits
    IUFillNumber(&LimitsN[SS_MIN_LIMIT], "FOCUS_MIN_STEP", "Min", "%.f", 0, 10000., 0., 0.);
    IUFillNumber(&LimitsN[SS_MAX_LIMIT], "FOCUS_MAX_STEP", "Max", "%.f", 10000,  2097152., 0., 200000.);
    IUFillNumberVector(&LimitsNP, LimitsN, 2, getDeviceName(), "FOCUS_LIMITS", "Limits", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Relative and absolute movement
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 50000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 1000;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 200000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1000;

    FocusMaxPosN[0].value = 2097152;

    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

bool SestoSenso::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        // Only define temperature if there is a probe
        if (updateTemperature())
            defineNumber(&TemperatureNP);
        defineText(&FirmwareTP);
        //defineNumber(&LimitsNP);

        if (getStartupValues())
            LOG_INFO("SestoSenso paramaters updated, focuser ready for use.");
        else
            LOG_WARN("Failed to inquire parameters. Check logs.");
    }
    else
    {
        if (TemperatureNP.s == IPS_OK)
            deleteProperty(TemperatureNP.name);
        deleteProperty(FirmwareTP.name);
        //deleteProperty(LimitsNP.name);
    }

    return true;
}

bool SestoSenso::Handshake()
{
    if (Ack())
    {
        LOG_INFO("SestoSenso is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO(
        "Error retreiving data from SestoSenso, please ensure SestoSenso controller is powered and the port is correct.");
    return false;
}

bool SestoSenso::Disconnect()
{
    // Save current position to memory.
    if (isSimulation() == false)
        sendCommand("#PS!");

    return INDI::Focuser::Disconnect();
}

const char *SestoSenso::getDefaultName()
{
    return "Sesto Senso";
}

bool SestoSenso::Ack()
{
    char res[SESTO_LEN] = {0};

    if (isSimulation())
        strncpy(res, "1.0 Simulation", SESTO_LEN);
    else if (sendCommand("#QF!", res) == false)
        return false;

    IUSaveText(&FirmwareT[0], res);

    return true;
}

bool SestoSenso::updateTemperature()
{
    char res[SESTO_LEN] = {0};
    double temperature = 0;

    if (isSimulation())
        strncpy(res, "23.45", SESTO_LEN);
    else if (sendCommand("#QT!", res) == false)
        return false;

    try
    {
        temperature = std::stod(res);
    }
    catch(...)
    {
        LOGF_WARN("Failed to process temperature response: %s (%d bytes)", res, strlen(res));
        return false;
    }

    if (temperature > 90)
        return false;

    TemperatureN[0].value = temperature;
    TemperatureNP.s = IPS_OK;

    return true;
}

bool SestoSenso::updateMaxLimit()
{
    char res[SESTO_LEN] = {0};

    if (isSimulation())
        return true;

    if (sendCommand("#QM!", res) == false)
        return false;

    int maxLimit = 0;

    sscanf(res, "QM;%d!", &maxLimit);

    if (maxLimit > 0)
    {
        FocusMaxPosN[0].max = maxLimit;
        if (FocusMaxPosN[0].value > maxLimit)
            FocusMaxPosN[0].value = maxLimit;

        FocusMaxPosNP.s = IPS_OK;
        IUUpdateMinMax(&FocusAbsPosNP);
        return true;
    }

    FocusMaxPosNP.s = IPS_ALERT;
    return false;
}

bool SestoSenso::updatePosition()
{
    char res[SESTO_LEN] = {0};
    if (isSimulation())
        snprintf(res, SESTO_LEN, "%d", static_cast<uint32_t>(FocusAbsPosN[0].value));
    else if (sendCommand("#QP!", res) == false)
        return false;

    try
    {
        FocusAbsPosN[0].value = std::stoi(res);
        FocusAbsPosNP.s = IPS_OK;
        return true;
    }
    catch(...)
    {
        LOGF_WARN("Failed to process position response: %s (%d bytes)", res, strlen(res));
        FocusAbsPosNP.s = IPS_ALERT;
        return false;
    }
}

bool SestoSenso::isMotionComplete()
{
    char res[SESTO_LEN] = {0};

    if (isSimulation())
    {
        int32_t nextPos = FocusAbsPosN[0].value;
        int32_t targPos = static_cast<int32_t>(targetPos);

        if (targPos > nextPos)
            nextPos += 250;
        else if (targPos < nextPos)
            nextPos -= 250;

        if (abs(nextPos - targPos) < 250)
            nextPos = targetPos;
        else if (nextPos < 0)
            nextPos = 0;
        else if (nextPos > FocusAbsPosN[0].max)
            nextPos = FocusAbsPosN[0].max;

        snprintf(res, SESTO_LEN, "%d", nextPos);
    }
    else
    {
        int rc = TTY_OK, nbytes_read = 0;

        //while (rc != TTY_TIME_OUT)
        //{
        rc = tty_read_section(PortFD, res, SESTO_STOP_CHAR, 1, &nbytes_read);
        if (rc == TTY_OK)
        {
            res[nbytes_read - 1] = 0;

            if (!strcmp(res, "GTok!"))
                return true;

            try
            {
                uint32_t newPos = std::stoi(res);
                FocusAbsPosN[0].value = newPos;
                //IDSetNumber(&FocusAbsPosNP, nullptr);
                //if (newPos == targetPos)
                //    return true;
            }
            catch (...)
            {
                LOGF_WARN("Failed to process motion response: %s (%d bytes)", res, strlen(res));
            }
        }
        //}
    }

    return false;
}

bool SestoSenso::SyncFocuser(uint32_t ticks)
{
    char cmd[SESTO_LEN] = {0}, res[SESTO_LEN] = {0};
    snprintf(cmd, SESTO_LEN, "#SP%d!", ticks);

    if (isSimulation())
        FocusAbsPosN[0].value = ticks;
    else if (sendCommand(cmd, res) == false)
        return false;

    return !strcmp(res, "SPok!");
}

bool SestoSenso::SetFocuserMaxPosition(uint32_t ticks)
{
    return setMaxLimit(ticks);
}

bool SestoSenso::setMaxLimit(uint32_t limit)
{
    char cmd[SESTO_LEN] = {0}, res[SESTO_LEN] = {0};
    snprintf(cmd, SESTO_LEN, "#SM;%07d!", limit);

    if (!isSimulation())
    {
        if (sendCommand(cmd, res) == false)
            return false;
    }

    if (strstr(res, "SM;"))
    {
        FocusAbsPosN[0].max = limit;
        FocusMaxPosN[0].max = limit;
        return true;
    }

    return false;
}

bool SestoSenso::setMinLimit(uint32_t limit)
{
    char cmd[SESTO_LEN] = {0}, res[SESTO_LEN] = {0};
    snprintf(cmd, SESTO_LEN, "#Sm;%07d!", limit);

    if (!isSimulation())
    {
        if (sendCommand(cmd, res) == false)
            return false;
    }

    if (strstr(res, "Sm;"))
    {
        FocusAbsPosN[0].min = limit;
        FocusMaxPosN[0].min = limit;
        return true;
    }

    return false;
}

bool SestoSenso::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        // Set Focus Limits
        if (!strcmp(name, LimitsNP.name))
        {
            // If connected, we ignore
            if (isConnected())
            {
                LimitsNP.s = IPS_OK;
                IDSetNumber(&LimitsNP, nullptr);
                return true;
            }

            IUUpdateNumber(&LimitsNP, values, names, n);

            if (setMinLimit(static_cast<uint32_t>(LimitsN[SS_MIN_LIMIT].value)) &&
                    setMaxLimit(static_cast<uint32_t>(LimitsN[SS_MAX_LIMIT].value)))
                LimitsNP.s = IPS_OK;
            else
            {
                LimitsNP.s = IPS_ALERT;
                IDSetNumber(&LimitsNP, nullptr);
                LOG_ERROR("Error setting focuser limits.");
                return true;
            }

            FocusAbsPosN[0].min   = LimitsN[SS_MIN_LIMIT].value;
            FocusAbsPosN[0].max   = LimitsN[SS_MAX_LIMIT].value;
            FocusAbsPosN[0].value = 0;
            FocusAbsPosN[0].step  = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;

            FocusRelPosN[0].min   = 0.;
            FocusRelPosN[0].max   = FocusAbsPosN[0].step * 10;
            FocusRelPosN[0].value = 0;
            FocusRelPosN[0].step  = FocusAbsPosN[0].step;

            IUUpdateMinMax(&FocusAbsPosNP);
            IUUpdateMinMax(&FocusMaxPosNP);

            saveConfig(true, LimitsNP.name);

            LOG_INFO("Focuser limits are updated.");

            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

IPState SestoSenso::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, 16, "#GT%d!", targetTicks);
    if (isSimulation() == false)
    {
        if (sendCommand(cmd) == false)
            return IPS_ALERT;
    }

    if (m_MotionProgressTimerID > 0)
        IERmTimer(m_MotionProgressTimerID);
    m_MotionProgressTimerID = IEAddTimer(10, &SestoSenso::checkMotionProgressHelper, this);
    return IPS_BUSY;
}

IPState SestoSenso::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int reversed = (IUFindOnSwitchIndex(&FocusReverseSP) == REVERSED_ENABLED) ? -1 : 1;
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * reversed;
    double newPosition = FocusAbsPosN[0].value + relativeTicks;

    bool rc = MoveAbsFocuser(newPosition);

    return (rc ? IPS_BUSY : IPS_ALERT);
}

bool SestoSenso::AbortFocuser()
{
    if (m_MotionProgressTimerID > 0)
    {
        IERmTimer(m_MotionProgressTimerID);
        m_MotionProgressTimerID = -1;
    }

    if (isSimulation())
        return true;

    return sendCommand("#MA!");
}

void SestoSenso::checkMotionProgressHelper(void *context)
{
    static_cast<SestoSenso*>(context)->checkMotionProgressCallback();
}

void SestoSenso::checkMotionProgressCallback()
{
    if (isMotionComplete())
    {
        FocusAbsPosNP.s = IPS_OK;
        FocusRelPosNP.s = IPS_OK;
        IDSetNumber(&FocusRelPosNP, nullptr);
        IDSetNumber(&FocusAbsPosNP, nullptr);
        LOG_INFO("Focuser reached requested position.");
    }
    else
        IDSetNumber(&FocusAbsPosNP, nullptr);

    lastPos = FocusAbsPosN[0].value;

    IERmTimer(m_MotionProgressTimerID);
    m_MotionProgressTimerID = IEAddTimer(10, &SestoSenso::checkMotionProgressHelper, this);
}

void SestoSenso::TimerHit()
{
    if (!isConnected() || FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        SetTimer(POLLMS);
        return;
    }

    bool rc = updatePosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 0)
        {
            IDSetNumber(&FocusAbsPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    if (m_TemperatureCounter++ == SESTO_TEMPERATURE_FREQ)
    {
        rc = updateTemperature();
        if (rc)
        {
            if (fabs(lastTemperature - TemperatureN[0].value) >= 0.1)
            {
                IDSetNumber(&TemperatureNP, nullptr);
                lastTemperature = TemperatureN[0].value;
            }
        }
    }

    SetTimer(POLLMS);
}

bool SestoSenso::getStartupValues()
{
    bool rc1 = updatePosition();
    if (rc1)
        IDSetNumber(&FocusAbsPosNP, nullptr);

    if (updateMaxLimit() == false)
        LOG_WARN("SestoSenso firmware is old, please update it to benefit from all features.");

    return (rc1);
}

bool SestoSenso::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[SESTO_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);
        rc = tty_write_string(PortFD, cmd, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, SESTO_TIMEOUT, &nbytes_read);
    else
    {
        rc = tty_nread_section(PortFD, res, SESTO_LEN, SESTO_STOP_CHAR, SESTO_TIMEOUT, &nbytes_read);
        res[nbytes_read - 1] = 0;
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[SESTO_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

void SestoSenso::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

bool SestoSenso::ReverseFocuser(bool enable)
{
    INDI_UNUSED(enable);
    return true;
}
