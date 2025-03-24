/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

  Tru Technology Filter Wheel

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "trutech_wheel.h"

#include "indicom.h"

#include <cstring>
#include <memory>
#include <termios.h>

#define BUF_SIZE 5
#define CMD_SIZE 4

const uint8_t COMM_INIT = 0xA5;
const uint8_t COMM_FILL = 0x20;

static std::unique_ptr<TruTech> tru_wheel(new TruTech());

TruTech::TruTech()
{
    setVersion(1, 0);
    setFilterConnection(CONNECTION_SERIAL | CONNECTION_TCP);
}

const char *TruTech::getDefaultName()
{
    return "TruTech Wheel";
}

bool TruTech::initProperties()
{
    INDI::FilterWheel::initProperties();

    IUFillSwitch(&HomeS[0], "Find", "Find", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 1, getDeviceName(), "HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60,
                       IPS_IDLE);

    addAuxControls();

    return true;
}

bool TruTech::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
        defineProperty(&HomeSP);
    else
        deleteProperty(HomeSP.name);

    return true;
}

bool TruTech::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(HomeSP.name, name))
        {
            if (home())
            {
                LOG_INFO("Filter set to home position.");
                HomeSP.s = IPS_OK;
                FilterSlotNP.setState(IPS_OK);
                FilterSlotNP.apply();
            }
            else
                HomeSP.s = IPS_ALERT;
            IDSetSwitch(&HomeSP, nullptr);
            return true;
        }
    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

bool TruTech::Handshake()
{
    return home();
}

bool TruTech::home()
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    uint8_t type   = 0x03;
    uint8_t chksum = COMM_INIT + type + COMM_FILL;
    char filter_command[BUF_SIZE] = {0};
    snprintf(filter_command, BUF_SIZE, "%c%c%c%c", COMM_INIT, type, COMM_FILL, chksum);

    LOGF_DEBUG("CMD: %#02X %#02X %#02X %#02X", COMM_INIT, type, COMM_FILL, chksum);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, filter_command, CMD_SIZE, &nbytes_written)) != TTY_OK)
    {
        char error_message[ERRMSG_SIZE];
        tty_error_msg(rc, error_message, ERRMSG_SIZE);
        LOGF_ERROR("Sending command Home to filter failed: %s", error_message);
    }

    char filter_response[BUF_SIZE] = {0};

    if ( (rc = tty_read(PortFD, filter_response, CMD_SIZE, 3, &nbytes_read)) != TTY_OK)
    {
        char error_message[ERRMSG_SIZE];
        tty_error_msg(rc, error_message, ERRMSG_SIZE);
        LOGF_ERROR("Error receiving response from filter: %s", error_message);
    }

    if (static_cast<uint8_t>(filter_response[0]) == COMM_INIT)
    {
        CurrentFilter = 1;
        FilterSlotNP[0].setValue(1);
        FilterSlotNP[0].setMin(1);
        FilterSlotNP[0].setMax(filter_response[2] - 0x30);
    }

    return true;
}

bool TruTech::SelectFilter(int f)
{
    TargetFilter = f;

    int rc = 0, nbytes_written = 0;
    char filter_command[BUF_SIZE] = {0};
    uint8_t type   = 0x01;
    uint8_t chksum = COMM_INIT + type + static_cast<uint8_t>(f);
    snprintf(filter_command, BUF_SIZE, "%c%c%c%c", COMM_INIT, type, f, chksum);

    LOGF_DEBUG("CMD: %#02X %#02X %#02X %#02X", COMM_INIT, type, f, chksum);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, filter_command, CMD_SIZE, &nbytes_written)) != TTY_OK)
    {
        char error_message[ERRMSG_SIZE];
        tty_error_msg(rc, error_message, ERRMSG_SIZE);

        LOGF_ERROR("Sending command select filter failed: %s", error_message);
        return false;
    }

    return true;
}

int TruTech::QueryFilter()
{
    return CurrentFilter;
}

void TruTech::TimerHit()
{
    if (FilterSlotNP.getState() == IPS_BUSY)
    {
        int rc = 0, nbytes_written = 0, nbytes_read = 0;
        uint8_t type   = 0x02;
        uint8_t chksum = COMM_INIT + type + COMM_FILL;
        char filter_command[BUF_SIZE] = {0};
        snprintf(filter_command, BUF_SIZE, "%c%c%c%c", COMM_INIT, type, COMM_FILL, chksum);

        LOGF_DEBUG("CMD: %#02X %#02X %#02X %#02X", COMM_INIT, type, COMM_FILL, chksum);

        tcflush(PortFD, TCIOFLUSH);

        if ( (rc = tty_write(PortFD, filter_command, CMD_SIZE, &nbytes_written)) != TTY_OK)
        {
            char error_message[ERRMSG_SIZE];
            tty_error_msg(rc, error_message, ERRMSG_SIZE);
            LOGF_WARN("Sending filter query failed: %s", error_message);
        }
        else
        {
            char filter_response[BUF_SIZE] = {0};

            if ( (rc = tty_read(PortFD, filter_response, CMD_SIZE, 3, &nbytes_read)) != TTY_OK)
            {
                char error_message[ERRMSG_SIZE];
                tty_error_msg(rc, error_message, ERRMSG_SIZE);
                LOGF_ERROR("Error receiving response from filter: %s", error_message);
            }

            if (static_cast<uint8_t>(filter_response[0]) == COMM_INIT)
            {
                // If filter finished moving
                if (filter_response[2] > 0x30)
                {
                    CurrentFilter = filter_response[2] - 0x30;
                    SelectFilterDone(CurrentFilter);
                }
            }
        }
    }

    SetTimer(getCurrentPollingPeriod());
}
