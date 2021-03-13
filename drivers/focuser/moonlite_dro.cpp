/*
    Moonlite DRO Dual Focuser
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

#include "moonlite_dro.h"
#include "connectionplugins/connectioninterface.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>

#define MOONLITEDRO_TIMEOUT 3

static std::unique_ptr<MoonLiteDRO> dro1(new MoonLiteDRO(1));
static std::unique_ptr<MoonLiteDRO> dro2(new MoonLiteDRO(2));

void ISGetProperties(const char *dev)
{
    if (dev == nullptr)
    {
        dro1->ISGetProperties(dev);
        dro2->ISGetProperties(dev);
    }
    else if (!strcmp(dev, dro1->getDeviceName()))
        dro1->ISGetProperties(dev);
    else if (!strcmp(dev, dro2->getDeviceName()))
        dro2->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(dev, dro1->getDeviceName()))
        dro1->ISNewSwitch(dev, name, states, names, n);
    else if (!strcmp(dev, dro2->getDeviceName()))
        dro2->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev, dro1->getDeviceName()))
        dro1->ISNewText(dev, name, texts, names, n);
    else if (!strcmp(dev, dro2->getDeviceName()))
        dro2->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp(dev, dro1->getDeviceName()))
        dro1->ISNewNumber(dev, name, values, names, n);
    else if (!strcmp(dev, dro2->getDeviceName()))
        dro2->ISNewNumber(dev, name, values, names, n);
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
    dro1->ISSnoopDevice(root);
    dro2->ISSnoopDevice(root);
}

MoonLiteDRO::MoonLiteDRO(int ID) : m_ID(ID)
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC);
    char name[MAXINDINAME] = {0};
    snprintf(name, MAXINDINAME, "MoonLiteDRO #%d", m_ID);
    setDeviceName(name);
}

bool MoonLiteDRO::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedNP[0].setMin(1);
    FocusSpeedNP[0].setMax(5);
    FocusSpeedNP[0].setValue(1);

    // Step Delay
    StepDelayNP[0].fill("STEP_DELAY", "Delay", "%.f", 1, 5, 1., 1.);
    StepDelayNP.fill(getDeviceName(), "FOCUS_STEP_DELAY", "Step",
                       SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Step Mode
    StepModeSP[FOCUS_HALF_STEP].fill("HALF_STEP", "Half Step", ISS_OFF);
    StepModeSP[FOCUS_FULL_STEP].fill("FULL_STEP", "Full Step", ISS_ON);
    StepModeSP.fill(getDeviceName(), "FOCUS_STEP_MODE", "Step Mode", SETTINGS_TAB, IP_RW,
                       ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Temperature Settings
    TemperatureSettingNP[0].fill("Calibration", "Calibration", "%6.2f", -20, 20, 0.5, 0);
    TemperatureSettingNP[1].fill("Coefficient", "Coefficient", "%6.2f", -20, 20, 0.5, 0);
    TemperatureSettingNP.fill(getDeviceName(), "FOCUS_TEMPERATURE_SETTINGS",
                       "T. Settings",
                       SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Compensate for temperature
    TemperatureCompensateSP[0].fill("Enable", "Enable", ISS_OFF);
    TemperatureCompensateSP[1].fill("Disable", "Disable", ISS_ON);
    TemperatureCompensateSP.fill(getDeviceName(), "FOCUS_TEMPERATURE_COMPENSATION",
                       "T. Compensate", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);


    // Focuser temperature
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Relative and absolute movement
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(50000.);
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(1000);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(100000.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000);

    setDefaultPollingPeriod(500);
    addDebugControl();

    return true;
}

bool MoonLiteDRO::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        // Only display such properties for the first focuser only
        if (m_ID == 1)
        {
            defineProperty(TemperatureNP);
            defineProperty(TemperatureSettingNP);
            defineProperty(TemperatureCompensateSP);
        }

        defineProperty(StepDelayNP);
        defineProperty(StepModeSP);

        GetFocusParams();

        LOGF_INFO("%s parameters updated, focuser ready for use.", getDeviceName());
    }
    else
    {
        if (m_ID == 1)
        {
            deleteProperty(TemperatureNP.getName());
            deleteProperty(TemperatureSettingNP.getName());
            deleteProperty(TemperatureCompensateSP.getName());
        }

        deleteProperty(StepDelayNP.getName());
        deleteProperty(StepModeSP.getName());
    }

    return true;
}

bool MoonLiteDRO::Connect()
{
    if (m_ID == 1)
        return INDI::Focuser::Connect();

    if (dro1->isConnected() == false)
    {
        LOG_ERROR("You must connect DRO Focuser #1 first before connecting to DRO Focuser #2.");
        return false;
    }

    PortFD = dro1->getPortFD();
    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool MoonLiteDRO::Disconnect()
{
    if (m_ID == 1)
    {
        // Also disconnect 2nd focuser
        dro2->remoteDisconnect();
        return INDI::Focuser::Disconnect();
    }

    // No need to do anything for DRO #2
    PortFD = -1;
    return true;
}

void MoonLiteDRO::remoteDisconnect()
{
    // If not DRO #2, then return immediately.
    if (m_ID != 2)
        return;

    if (isConnected())
    {
        // Otherwise, just set PortFD = -1
        PortFD = -1;
        setConnected(false, IPS_IDLE);
        updateProperties();
    }
}
bool MoonLiteDRO::Handshake()
{
    if (Ack())
    {
        LOGF_INFO("%s is online. Getting focus parameters...", getDeviceName());
        return true;
    }

    LOG_INFO("Handshake failed. please ensure MoonLite controller is powered and the port is correct.");
    return false;
}

const char *MoonLiteDRO::getDefaultName()
{
    return "MoonLiteDRO";
}

bool MoonLiteDRO::Ack()
{
    // For First Focuser, try to get the serial/tcp connection port FD
    if (m_ID == 1)
    {
        if (serialConnection == getActiveConnection())
            PortFD = serialConnection->getPortFD();
        else
            PortFD = tcpConnection->getPortFD();
    }
    // For second focuser, try to get the port FD of the first focuser
    else if (m_ID == 2)
    {
        // We need to get Serial Port file descriptor from first focuser
        // Since we need to have only a SINGLE serial connection to the DRO and not two.
        PortFD = dro1->getPortFD();
        if (PortFD == -1)
        {
            LOG_WARN("You must connect DRO Focuser #1 first before connecting to DRO Focuser #2.");
            return false;
        }

        // If we have a valid Port FD then we are good to go
        return true;
    }

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[5] = {0};
    short pos = -1;

    tcflush(PortFD, TCIOFLUSH);

    //Try to request the position of the focuser
    //Test for success on transmission and response
    //If either one fails, try again, up to 3 times, waiting 1 sec each time
    //If that fails, then return false.

    int numChecks = 0;
    bool success = false;
    while(numChecks < 3 && !success)
    {
        numChecks++;
        sleep(1); //wait 1 second between each test.

        bool transmissionSuccess = (rc = tty_write(PortFD, ":GP#", 4, &nbytes_written)) == TTY_OK;
        if(!transmissionSuccess)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Handshake Attempt %i, tty transmission error: %s.", numChecks, errstr);
        }

        bool responseSuccess = (rc = tty_read(PortFD, resp, 5, MOONLITEDRO_TIMEOUT, &nbytes_read)) == TTY_OK;
        if(!responseSuccess)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Handshake Attempt %i, updatePosition response error: %s.", numChecks, errstr);
        }

        success = transmissionSuccess && responseSuccess;
    }

    if(!success)
    {
        LOG_INFO("Handshake failed after 3 attempts");
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    rc = sscanf(resp, "%hX#", &pos);

    return rc > 0;
}

bool MoonLiteDRO::updateStepDelay()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[3] = {0};
    char cmd[DRO_CMD] = {0};
    short speed;

    if (m_ID == 1)
        strncpy(cmd, ":GD#", DRO_CMD);
    else
        strncpy(cmd, ":2GD#", DRO_CMD);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("updateStepDelay error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read_section(PortFD, resp, '#', MOONLITEDRO_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("updateStepDelay error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES <%s>", resp);

    rc = sscanf(resp, "%hX", &speed);

    if (rc > 0)
    {
        int focus_speed = -1;
        while (speed > 0)
        {
            speed >>= 1;
            focus_speed++;
        }

        StepDelayNP[0].setValue(focus_speed);
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser step delay value (%s)", resp);
        return false;
    }

    return true;
}

bool MoonLiteDRO::updateStepMode()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[4] = {0};
    char cmd[DRO_CMD] = {0};

    if (m_ID == 1)
        strncpy(cmd, ":GH#", DRO_CMD);
    else
        strncpy(cmd, ":2GH#", DRO_CMD);

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("updateStepMode error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read_section(PortFD, resp, '#', MOONLITEDRO_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("updateStepMode error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[3] = '\0';

    LOGF_DEBUG("RES <%s>", resp);

    StepModeSP.reset();

    if (strcmp(resp, "FF") == 0)
        StepModeSP[FOCUS_HALF_STEP].setState(ISS_ON);
    else if (strcmp(resp, "00") == 0)
        StepModeSP[FOCUS_FULL_STEP].setState(ISS_ON);
    else
    {
        LOGF_ERROR("Unknown error: focuser step value (%s)", resp);
        return false;
    }

    return true;
}

bool MoonLiteDRO::updateTemperature()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16] = {0};

    tcflush(PortFD, TCIOFLUSH);

    tty_write(PortFD, ":C#", 3, &nbytes_written);

    LOG_DEBUG("CMD <:GT#>");

    if ((rc = tty_write(PortFD, ":GT#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("updateTemperature error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read_section(PortFD, resp, '#', MOONLITEDRO_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("updateTemperature error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read - 1] = '\0';

    LOGF_DEBUG("RES <%s>", resp);

    uint32_t temp = 0;
    rc = sscanf(resp, "%X", &temp);

    if (rc > 0)
    {
        // Signed hex
        TemperatureNP[0].setValue(static_cast<int16_t>(temp) / 2.0);
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser temperature value (%s)", resp);
        return false;
    }

    return true;
}

bool MoonLiteDRO::updatePosition()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[DRO_CMD] = {0};
    int pos = -1;
    char cmd[DRO_CMD] = {0};

    if (m_ID == 1)
        strncpy(cmd, ":GP#", DRO_CMD);
    else
        strncpy(cmd, ":2GP#", DRO_CMD);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("updatePosition error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read_section(PortFD, resp, '#', MOONLITEDRO_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("updatePosition error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES <%s>", resp);

    rc = sscanf(resp, "%X", &pos);

    if (rc > 0)
    {
        FocusAbsPosNP[0].setValue(pos);
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser position value (%s)", resp);
        return false;
    }

    return true;
}

bool MoonLiteDRO::isMoving()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[DRO_CMD] = {0};
    char cmd[DRO_CMD] = {0};

    if (m_ID == 1)
        strncpy(cmd, ":GI#", DRO_CMD);
    else
        strncpy(cmd, ":2GI#", DRO_CMD);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("isMoving error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read_section(PortFD, resp, '#', MOONLITEDRO_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("isMoving error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[3] = '\0';

    LOGF_DEBUG("RES <%s>", resp);

    if (strcmp(resp, "01") == 0)
        return true;
    else if (strcmp(resp, "00") == 0)
        return false;

    LOGF_ERROR("Unknown error: isMoving value (%s)", resp);
    return false;
}

bool MoonLiteDRO::setTemperatureCalibration(double calibration)
{
    int nbytes_written = 0, rc = -1;
    char cmd[DRO_CMD] = {0};

    uint8_t hex = static_cast<int8_t>(calibration * 2) & 0xFF;
    snprintf(cmd, DRO_CMD, ":PO%02X#", hex);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setTemperatureCalibration error: %s.", errstr);
        return false;
    }

    return true;
}

bool MoonLiteDRO::setTemperatureCoefficient(double coefficient)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[DRO_CMD] = {0};
    uint8_t hex = static_cast<int8_t>(coefficient * 2) & 0xFF;
    snprintf(cmd, DRO_CMD, ":SC%02X#", hex);

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setTemperatureCoefficient error: %s.", errstr);
        return false;
    }

    return true;
}

bool MoonLiteDRO::SyncFocuser(uint32_t ticks)
{
    int nbytes_written = 0, rc = -1;
    char cmd[DRO_CMD] = {0};

    if (m_ID == 1)
        snprintf(cmd, DRO_CMD, ":SP%04X#", ticks);
    else
        snprintf(cmd, DRO_CMD, ":2SP%04X#", ticks);

    LOGF_DEBUG("CMD <%s>", cmd);

    // Set Position
    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("reset error: %s.", errstr);
        return false;
    }

    return true;
}

bool MoonLiteDRO::gotoAbsPosition(unsigned int position)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[DRO_CMD] = {0};

    if (position < FocusAbsPosNP[0].min || position > FocusAbsPosNP[0].max)
    {
        LOGF_ERROR("Requested position value out of bound: %d", position);
        return false;
    }

    if (m_ID == 1)
        snprintf(cmd, DRO_CMD, ":SN%04X#", position);
    else
        snprintf(cmd, DRO_CMD, ":2SN%04X#", position);

    LOGF_DEBUG("CMD <%s>", cmd);

    // Set Position
    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setPosition error: %s.", errstr);
        return false;
    }

    memset(cmd, 0, DRO_CMD);
    if (m_ID == 1)
        strncpy(cmd, ":FG#", DRO_CMD);
    else
        strncpy(cmd, ":2FG#", DRO_CMD);

    LOGF_DEBUG("CMD <%s>", cmd);

    // gotoAbsPosition to Position
    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("gotoAbsPosition error: %s.", errstr);
        return false;
    }

    return true;
}

bool MoonLiteDRO::setStepMode(FocusStepMode mode)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[DRO_CMD] = {0};

    tcflush(PortFD, TCIOFLUSH);

    if (mode == FOCUS_HALF_STEP)
    {
        if (m_ID == 1)
            strncpy(cmd, ":SH#", DRO_CMD);
        else
            strncpy(cmd, ":2SH#", DRO_CMD);
    }
    else
    {
        if (m_ID == 1)
            strncpy(cmd, ":SF#", DRO_CMD);
        else
            strncpy(cmd, ":2SF#", DRO_CMD);
    }

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setStepMode error: %s.", errstr);
        return false;
    }

    return true;
}

bool MoonLiteDRO::setStepDelay(uint8_t delay)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[DRO_CMD] = {0};

    int hex_value = 1;

    hex_value <<= delay;

    if (m_ID == 1)
        snprintf(cmd, DRO_CMD, ":SD%02X#", hex_value);
    else
        snprintf(cmd, DRO_CMD, ":2SD%02X#", hex_value);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setStepelay error: %s.", errstr);
        return false;
    }

    return true;
}

bool MoonLiteDRO::setTemperatureCompensation(bool enable)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[DRO_CMD] = {0};

    tcflush(PortFD, TCIOFLUSH);

    if (enable)
        strncpy(cmd, ":+#", DRO_CMD);
    else
        strncpy(cmd, ":-#", DRO_CMD);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setTemperatureCompensation error: %s.", errstr);
        return false;
    }

    return true;
}

bool MoonLiteDRO::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Focus Step Mode
        if (StepModeSP.isNameMatch(name))
        {
            bool rc          = false;
            int current_mode = StepModeSP.findOnSwitchIndex();

            StepModeSP.update(states, names, n);

            int target_mode = StepModeSP.findOnSwitchIndex();

            if (current_mode == target_mode)
            {
                StepModeSP.setState(IPS_OK);
                StepModeSP.apply();
            }

            if (target_mode == 0)
                rc = setStepMode(FOCUS_HALF_STEP);
            else
                rc = setStepMode(FOCUS_FULL_STEP);

            if (!rc)
            {
                StepModeSP.reset();
                StepModeSP[current_mode].setState(ISS_ON);
                StepModeSP.setState(IPS_ALERT);
                StepModeSP.apply();
                return false;
            }

            StepModeSP.setState(IPS_OK);
            StepModeSP.apply();
            return true;
        }

        // Temperature Compensation
        if (TemperatureCompensateSP.isNameMatch(name))
        {
            int last_index = TemperatureCompensateSP.findOnSwitchIndex();
            TemperatureCompensateSP.update(states, names, n);

            bool rc = setTemperatureCompensation((TemperatureCompensateSP[0].getState() == ISS_ON));

            if (!rc)
            {
                TemperatureCompensateSP.setState(IPS_ALERT);
                TemperatureCompensateSP.reset();
                TemperatureCompensateSP[last_index].setState(ISS_ON);
                TemperatureCompensateSP.apply();
                return false;
            }

            TemperatureCompensateSP.setState(IPS_OK);
            TemperatureCompensateSP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool MoonLiteDRO::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Temperature Settings
        if (TemperatureSettingNP.isNameMatch(name))
        {
            TemperatureSettingNP.update(values, names, n);
            if (!setTemperatureCalibration(TemperatureSettingNP[0].value) ||
                    !setTemperatureCoefficient(TemperatureSettingNP[1].value))
            {
                TemperatureSettingNP.setState(IPS_ALERT);
                TemperatureSettingNP.apply();
                return false;
            }

            TemperatureSettingNP.setState(IPS_OK);
            TemperatureSettingNP.apply();
            return true;
        }

        // Step Delay
        if (StepDelayNP.isNameMatch(name))
        {
            if (setStepDelay(values[0]) == false)
            {
                StepDelayNP.setState(IPS_ALERT);
                StepDelayNP.apply();
                return false;
            }

            StepDelayNP.update(values, names, n);
            StepDelayNP.setState(IPS_OK);
            StepDelayNP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void MoonLiteDRO::GetFocusParams()
{
    if (updatePosition())
        FocusAbsPosNP.apply();

    if (updateTemperature())
        TemperatureNP.apply();

    if (updateStepDelay())
        StepDelayNP.apply();

    if (updateStepMode())
        StepModeSP.apply();
}

IPState MoonLiteDRO::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    bool rc = false;

    rc = gotoAbsPosition(targetPos);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

IPState MoonLiteDRO::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    double newPosition = 0;
    bool rc            = false;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    rc = gotoAbsPosition(newPosition);

    if (!rc)
        return IPS_ALERT;

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

void MoonLiteDRO::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = updatePosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosNP[0].getValue()) > 5)
        {
            FocusAbsPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
        }
    }

    // Only query temperature for FIRST focuser
    if (m_ID == 1)
    {
        rc = updateTemperature();
        if (rc)
        {
            if (fabs(lastTemperature - TemperatureNP[0].getValue()) >= 0.5)
            {
                TemperatureNP.apply();
                lastTemperature = TemperatureNP[0].getValue();
            }
        }
    }

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool MoonLiteDRO::AbortFocuser()
{
    int nbytes_written;
    char cmd[DRO_CMD] = {0};

    strncpy(cmd, (m_ID == 1) ? ":FQ#" : "#:2FQ#", DRO_CMD);

    LOGF_DEBUG("CMD <%s>", cmd);

    if (tty_write_string(PortFD, cmd, &nbytes_written) == TTY_OK)
    {
        FocusAbsPosNP.setState(IPS_IDLE);
        FocusRelPosNP.setState(IPS_IDLE);
        FocusAbsPosNP.apply();
        FocusRelPosNP.apply();
        return true;
    }
    else
        return false;
}

bool MoonLiteDRO::saveConfigItems(FILE *fp)
{
    Focuser::saveConfigItems(fp);

    StepModeSP.save(fp);
    StepDelayNP.save(fp);

    return true;
}
