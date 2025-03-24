/*
  FocuserDriver Focuser

  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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

#include "focuser_driver.h"

#include "indicom.h"

#include <cstring>
#include <termios.h>
#include <memory>
#include <thread>
#include <chrono>

static std::unique_ptr<FocuserDriver> focuserDriver(new FocuserDriver());

FocuserDriver::FocuserDriver()
{
    // Let's specify the driver version
    setVersion(1, 0);

    // What capabilities do we support?
    FI::SetCapability(FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_SYNC);
}

bool FocuserDriver::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -100, 100, 0, 0);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Stepping Modes
    IUFillSwitch(&SteppingModeS[STEPPING_FULL], "STEPPING_FULL", "Full", ISS_ON);
    IUFillSwitch(&SteppingModeS[STEPPING_HALF], "STEPPING_HALF", "Half", ISS_OFF);
    IUFillSwitchVector(&SteppingModeSP, SteppingModeS, 2, getDeviceName(), "STEPPING_MODE", "Mode",
                       STEPPING_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);


    addDebugControl();

    // Set limits as per documentation
    FocusAbsPosNP[0].setMin(0);
    FocusAbsPosNP[0].setMax(999999);
    FocusAbsPosNP[0].setStep(1000);

    FocusRelPosNP[0].setMin(0);
    FocusRelPosNP[0].setMax(999);
    FocusRelPosNP[0].setStep(100);

    FocusSpeedNP[0].setMin(1);
    FocusSpeedNP[0].setMax(254);
    FocusSpeedNP[0].setStep(10);

    return true;
}

const char *FocuserDriver::getDefaultName()
{
    return "Focuser Driver";
}

bool FocuserDriver::updateProperties()
{
    if (isConnected())
    {
        // Read these values before defining focuser interface properties
        readPosition();
    }

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        if (readTemperature())
            defineProperty(&TemperatureNP);

        bool rc = getStartupValues();

        // Settings
        defineProperty(&SteppingModeSP);

        if (rc)
            LOG_INFO("FocuserDriver is ready.");
        else
            LOG_WARN("Failed to query startup values.");
    }
    else
    {
        if (TemperatureNP.s == IPS_OK)
            deleteProperty(TemperatureNP.name);

        deleteProperty(SteppingModeSP.name);
    }

    return true;
}

bool FocuserDriver::Handshake()
{
    // This function is ensure that we have communication with the focuser
    // Below we send it 0x6 byte and check for 'S' in the return. Change this
    // to be valid for your driver. It could be anything, you can simply put this below
    // return readPosition()
    // since this will try to read the position and if successful, then communicatoin is OK.
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    // Ack
    cmd[0] = 0x6;

    bool rc = sendCommand(cmd, res, 1, 1);
    if (rc == false)
        return false;

    return res[0] == 'S';
}

bool FocuserDriver::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[DRIVER_LEN * 3] = {0};
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
        rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[DRIVER_LEN * 3] = {0};
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

void FocuserDriver::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

bool FocuserDriver::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Stepping Mode
        if (!strcmp(name, SteppingModeSP.name))
        {
            IUUpdateSwitch(&SteppingModeSP, states, names, n);
            SteppingModeSP.s = IPS_OK;
            IDSetSwitch(&SteppingModeSP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool FocuserDriver::getStartupValues()
{
    bool rc1 = readStepping();
    //bool rc2 = readFoo();
    //bool rc2 = readBar();

    //return (rc1 && rc2 && rc3);

    return rc1;
}

IPState FocuserDriver::MoveAbsFocuser(uint32_t targetTicks)
{
    // Issue here the command necessary to move the focuser to targetTicks
    return IPS_BUSY;
}

IPState FocuserDriver::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    m_TargetDiff = ticks * ((dir == FOCUS_INWARD) ? -1 : 1);
    return MoveAbsFocuser(FocusAbsPosNP[0].getValue() + m_TargetDiff);
}

bool FocuserDriver::AbortFocuser()
{
    return sendCommand("FOOBAR");
}

void FocuserDriver::TimerHit()
{
    if (isConnected() == false)
        return;

    // What is the last read position?
    double currentPosition = FocusAbsPosNP[0].getValue();

    // Read the current position
    readPosition();

    // Check if we have a pending motion
    // if isMoving() is false, then we stopped, so we need to set the Focus Absolute
    // and relative properties to OK
    if ( (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY) && isMoving() == false)
    {
        FocusAbsPosNP.setState(IPS_OK);
        FocusRelPosNP.setState(IPS_OK);
        FocusAbsPosNP.apply();
        FocusRelPosNP.apply();
    }
    // If there was a different between last and current positions, let's update all clients
    else if (currentPosition != FocusAbsPosNP[0].getValue())
    {
        FocusAbsPosNP.apply();
    }

    // Read temperature periodically
    if (TemperatureNP.s == IPS_OK && m_TemperatureCounter++ == DRIVER_TEMPERATURE_FREQ)
    {
        m_TemperatureCounter = 0;
        if (readTemperature())
            IDSetNumber(&TemperatureNP, nullptr);
    }

    SetTimer(getCurrentPollingPeriod());
}

bool FocuserDriver::isMoving()
{
    char res[DRIVER_LEN] = {0};

    bool rc = sendCommand("FOOBAR", res, 1, 1);

    if (rc && !strcmp(res, "STOPPED"))
        return true;

    return false;
}

bool FocuserDriver::readTemperature()
{
    char res[DRIVER_LEN] = {0};

    // This assumes we need to read 4 BYTES for the temperature. It can be anything
    // If the response is terminated by the DRIVER_STOP_CHAR, we can simply call
    // sendCommand("FOOBAR", res)
    if (sendCommand("FOOBAR", res, strlen("FOOBAR"), 4) == false)
        return false;

    float temperature = -1000;
    sscanf(res, "%f", &temperature);

    if (temperature < -100)
        return false;

    TemperatureN[0].value = temperature;
    TemperatureNP.s = IPS_OK;

    return true;
}

bool FocuserDriver::readPosition()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = 0xA;
    cmd[1] = 0xB;
    cmd[2] = 0xC;

    // since the command above is not NULL-TERMINATED, we need to specify the number of bytes (3)
    // in the send command below. We also specify 7 bytes to be read which can be changed to any value.
    if (sendCommand(cmd, res, 3, 7) == false)
        return false;

    // For above, in case instead the response is terminated by DRIVER_STOP_CHAR, then the command would be
    // (sendCommand(cmd, res, 3) == false)
    //    return false;

    int32_t pos = 1e6;
    sscanf(res, "%d", &pos);

    if (pos == 1e6)
        return false;

    FocusAbsPosNP[0].setValue(pos);

    return true;
}

bool FocuserDriver::readStepping()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand("FOOBAR", res, 3, 1) == false)
        return false;

    int32_t mode = 1e6;
    sscanf(res, "%d", &mode);

    if (mode == 1e6)
        return false;

    // Assuming the above function returns 10 for full step, and 11 for half step
    // we can update the switch status as follows
    SteppingModeS[STEPPING_FULL].s = (mode == 10) ? ISS_ON : ISS_OFF;
    SteppingModeS[STEPPING_HALF].s = (mode == 10) ? ISS_OFF : ISS_ON;
    SteppingModeSP.s = IPS_OK;

    return true;
}


bool FocuserDriver::SyncFocuser(uint32_t ticks)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "#:SYNC+%06d#", ticks);
    return sendCommand(cmd);
}

bool FocuserDriver::setStepping(SteppingMode mode)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "#FOOBAR%01d#", mode);
    return sendCommand(cmd);
}

bool FocuserDriver::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);

    // We need to reserve and save stepping mode
    // so that the next time the driver is loaded, it is remembered and applied.
    IUSaveConfigSwitch(fp, &SteppingModeSP);

    return true;
}
