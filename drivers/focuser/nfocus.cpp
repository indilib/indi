/*
    NFocus DC Relative Focuser

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

#include "nfocus.h"

#include "indicom.h"

#include <cstring>
#include <memory>
#include <termios.h>

static std::unique_ptr<NFocus> nFocus(new NFocus());

NFocus::NFocus()
{
    setVersion(1, 1);
    FI::SetCapability(FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
}

bool NFocus::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -100, 100, 0, 0);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Settings of the Nfocus
    IUFillNumber(&SettingsN[SETTING_ON_TIME], "ON time", "ON waiting time", "%6.0f", 10., 250., 0., 73.);
    IUFillNumber(&SettingsN[SETTING_OFF_TIME], "OFF time", "OFF waiting time", "%6.0f", 1., 250., 0., 15.);
    IUFillNumber(&SettingsN[SETTING_MODE_DELAY], "Fast Mode Delay", "Fast Mode Delay", "%6.0f", 0., 255., 0., 9.);
    IUFillNumberVector(&SettingsNP, SettingsN, 3, getDeviceName(), "FOCUS_SETTINGS", "Settings", SETTINGS_TAB, IP_RW, 0,
                       IPS_IDLE);

    FocusRelPosNP[0].setMin(0);
    FocusRelPosNP[0].setMax(50000);
    FocusRelPosNP[0].setStep(1000);
    FocusRelPosNP[0].setValue(0);

    // Set polling to 500ms
    setDefaultPollingPeriod(500);

    return true;
}

bool NFocus::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        if (readTemperature())
            defineProperty(&TemperatureNP);
        defineProperty(&SettingsNP);

        if (getStartupValues())
            LOG_INFO("NFocus is ready.");
    }
    else
    {
        deleteProperty(TemperatureNP.name);
        deleteProperty(SettingsNP.name);
    }

    return true;
}

const char *NFocus::getDefaultName()
{
    return "NFocus";
}

bool NFocus::Handshake()
{
    char cmd[NFOCUS_LEN] = {0}, res[NFOCUS_LEN] = {0};

    // Ack
    cmd[0] = 0x6;

    bool rc = sendCommand(cmd, res, 1, 1);
    if (rc == false)
        return false;

    return res[0] == 'n';
}

bool NFocus::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[NFOCUS_LEN * 3] = {0};
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
        rc = tty_read(PortFD, res, res_len, NFOCUS_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, NFOCUS_LEN, NFOCUS_STOP_CHAR, NFOCUS_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[NFOCUS_LEN * 3] = {0};
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

void NFocus::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

bool NFocus::readTemperature()
{
    char res[NFOCUS_LEN] = {0};

    if (sendCommand(":RT", res, 3, 4) == false)
        return false;

    float temperature = -1000;
    sscanf(res, "%f", &temperature);

    temperature /= 10.0;

    if (temperature <= -80)
        return false;

    TemperatureN[0].value = temperature;
    TemperatureNP.s = IPS_OK;

    return true;
}

bool NFocus::setMotorSettings(double onTime, double offTime, double fastDelay)
{
    char on_cmd[NFOCUS_LEN] = {0}, off_cmd[NFOCUS_LEN] = {0}, fast_cmd[NFOCUS_LEN] = {0};

    snprintf(on_cmd, NFOCUS_LEN, ":CO%03d#", static_cast<int>(onTime));
    snprintf(off_cmd, NFOCUS_LEN, ":CF%03d#", static_cast<int>(offTime));
    snprintf(fast_cmd, NFOCUS_LEN, ":CS%03d#", static_cast<int>(fastDelay));

    bool on_rc   = sendCommand(on_cmd);
    bool off_rc  = sendCommand(off_cmd);
    bool fast_rc = sendCommand(fast_cmd);

    return (on_rc && off_rc && fast_rc);
}

bool NFocus::readMotorSettings()
{
    char on_res[NFOCUS_LEN] = {0}, off_res[NFOCUS_LEN] = {0}, fast_res[NFOCUS_LEN] = {0};

    bool on_rc = sendCommand(":RO", on_res, 3, 3);
    bool off_rc = sendCommand(":RF", off_res, 3, 3);
    bool fast_rc = sendCommand(":RS", fast_res, 3, 3);

    if (on_rc && off_rc && fast_rc)
    {
        SettingsN[SETTING_ON_TIME].value = std::stoi(on_res);
        SettingsN[SETTING_OFF_TIME].value = std::stoi(off_res);
        SettingsN[SETTING_MODE_DELAY].value = std::stoi(fast_res);
        return true;
    }

    return false;
}

bool NFocus::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    int nset = 0, i = 0;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Settings
        if (strcmp(name, SettingsNP.name) == 0)
        {
            // New Settings
            double new_onTime    = 0;
            double new_offTime   = 0;
            double new_fastDelay = 0;

            for (nset = i = 0; i < n; i++)
            {
                /* Find numbers with the passed names in the SettingsNP property */
                INumber *eqp = IUFindNumber(&SettingsNP, names[i]);

                /* If the number found is  (SettingsN[0]) then process it */
                if (eqp == &SettingsN[SETTING_ON_TIME])
                {
                    new_onTime = (values[i]);
                    nset += static_cast<int>(new_onTime >= 10 && new_onTime <= 250);
                }
                else if (eqp == &SettingsN[SETTING_OFF_TIME])
                {
                    new_offTime = (values[i]);
                    nset += static_cast<int>(new_offTime >= 1 && new_offTime <= 250);
                }
                else if (eqp == &SettingsN[SETTING_MODE_DELAY])
                {
                    new_fastDelay = (values[i]);
                    nset += static_cast<int>(new_fastDelay >= 1 && new_fastDelay <= 9);
                }
            }

            /* Did we process the three numbers? */
            if (nset == 3)
            {
                if (setMotorSettings(new_onTime, new_offTime, new_fastDelay) == false)
                {
                    LOG_ERROR("Changing to new settings failed");
                    SettingsNP.s = IPS_ALERT;
                    IDSetNumber(&SettingsNP, nullptr);
                    return false;
                }

                IUUpdateNumber(&SettingsNP, values, names, n);
                SettingsNP.s = IPS_OK;
                IDSetNumber(&SettingsNP, nullptr);
                return true;
            }
            else
            {
                SettingsNP.s = IPS_IDLE;
                LOG_WARN("Settings invalid.");
                IDSetNumber(&SettingsNP, nullptr);
                return false;
            }
        }

    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

bool NFocus::getStartupValues()
{
    if (readMotorSettings())
    {
        SettingsNP.s = IPS_OK;
        IDSetNumber(&SettingsNP, nullptr);
    }

    return true;
}

IPState NFocus::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    INDI_UNUSED(dir);
    m_TargetPosition = ticks;
    return IPS_BUSY;
}

bool NFocus::AbortFocuser()
{
    return sendCommand("F00000#");
}

void NFocus::TimerHit()
{
    if (isConnected() == false)
        return;

    // Check if we have a pending motion
    // and if we STOPPED, then let's take the next action
    if (FocusRelPosNP.getState() == IPS_BUSY && isMoving() == false)
    {
        // Are we done moving?
        if (m_TargetPosition == 0)
        {
            FocusRelPosNP.setState(IPS_OK);
            FocusRelPosNP.apply();
        }
        else
        {
            // 999 is the max we can go in one command
            // so we need to go 999 or LESS
            // therefore for larger movements, we break it down.
            int nextMotion = (m_TargetPosition > 999) ? 999 : m_TargetPosition;
            int direction = FocusMotionSP.findOnSwitchIndex();
            char cmd[NFOCUS_LEN] = {0};
            snprintf(cmd, NFOCUS_LEN, ":F%d0%03d#", direction, nextMotion);
            if (sendCommand(cmd) == false)
            {
                FocusRelPosNP.setState(IPS_ALERT);
                LOG_ERROR("Failed to issue motion command.");
                FocusRelPosNP.apply();
            }
            else
                m_TargetPosition -= nextMotion;
        }
    }

    // Read temperature
    if (TemperatureNP.s == IPS_OK && m_TemperatureCounter++ == NFOCUS_TEMPERATURE_FREQ)
    {
        m_TemperatureCounter = 0;
        if (readTemperature())
            IDSetNumber(&TemperatureNP, nullptr);
    }

    SetTimer(getCurrentPollingPeriod());
}

bool NFocus::isMoving()
{
    char res[NFOCUS_LEN] = {0};

    bool rc = sendCommand("S", res, 1, 1);

    if (rc && res[0] == '1')
        return true;

    return false;
}

bool NFocus::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);
    IUSaveConfigNumber(fp, &SettingsNP);
    return true;
}
