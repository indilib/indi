/*******************************************************************************
  Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

  QHYCFW2/3 Filter Wheel Driver

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

#include "qhycfw3.h"

#include "indicom.h"

#include <cstring>
#include <memory>

// We declare an auto pointer to QHYCFW3.
static std::unique_ptr<QHYCFW3> qhycfw(new QHYCFW3());

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
    qhycfw->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    qhycfw->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    qhycfw->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    qhycfw->ISNewNumber(dev, name, values, names, n);
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
    qhycfw->ISSnoopDevice(root);
}

QHYCFW3::QHYCFW3()
{
    setVersion(1, 1);
    setFilterConnection(CONNECTION_SERIAL | CONNECTION_TCP);
}

const char *QHYCFW3::getDefaultName()
{
    return static_cast<const char *>("QHYCFW3");
}

bool QHYCFW3::initProperties()
{
    INDI::FilterWheel::initProperties();

    CurrentFilter      = 1;
    FilterSlotN[0].min = 1;
    FilterSlotN[0].max = 4;

    addAuxControls();

    return true;
}

bool QHYCFW3::Handshake()
{
    int rc = -1, nbytes_written = 0, nbytes_read = 0;
    char res[32] = {0};

    if (isSimulation())
        return true;

    LOG_DEBUG("HANDSHAKE");

    if ( (rc = tty_read(PortFD, res, 1, 25, &nbytes_read)) != TTY_OK)
    {
        char error_message[ERRMSG_SIZE];
        tty_error_msg(rc, error_message, ERRMSG_SIZE);

        LOGF_ERROR("Handshake failed: %s. Firmware must be higher than 201409", error_message);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    LOG_DEBUG("CMD <VRS>");

    if ( (rc = tty_write_string(PortFD, "VRS", &nbytes_written)) != TTY_OK)
    {
        char error_message[ERRMSG_SIZE];
        tty_error_msg(rc, error_message, ERRMSG_SIZE);

        LOGF_ERROR("Handshake failed: %s. Firmware must be higher than 201409", error_message);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 8, 3, &nbytes_read)) != TTY_OK)
    {
        char error_message[ERRMSG_SIZE];
        tty_error_msg(rc, error_message, ERRMSG_SIZE);

        LOGF_ERROR("Handshake failed: %s. Firmware must be higher than 201409", error_message);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    LOGF_INFO("Detected firmware version %s", res);

    // Now get maximum positions
    LOG_DEBUG("CMD <MXP>");

    if ( (rc = tty_write_string(PortFD, "MXP", &nbytes_written)) != TTY_OK)
    {
        char error_message[ERRMSG_SIZE];
        tty_error_msg(rc, error_message, ERRMSG_SIZE);

        LOGF_ERROR("Querying maximum position failed: %s.", error_message);
        return false;
    }

    memset(res, 0, 32);

    if ( (rc = tty_read(PortFD, res, 1, 3, &nbytes_read)) != TTY_OK)
    {
        char error_message[ERRMSG_SIZE];
        tty_error_msg(rc, error_message, ERRMSG_SIZE);

        LOGF_ERROR("Reading maximum position failed: %s.", error_message);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    if (res[0] == 'F')
        FilterSlotN[0].max = 16;
    else
        FilterSlotN[0].max = atoi(res) + 1;

    // Now get current position
    LOG_DEBUG("CMD <NOW>");

    if ( (rc = tty_write_string(PortFD, "NOW", &nbytes_written)) != TTY_OK)
    {
        char error_message[ERRMSG_SIZE];
        tty_error_msg(rc, error_message, ERRMSG_SIZE);

        LOGF_ERROR("Querying current position failed: %s.", error_message);
        return false;
    }

    memset(res, 0, 32);

    if ( (rc = tty_read(PortFD, res, 1, 3, &nbytes_read)) != TTY_OK)
    {
        char error_message[ERRMSG_SIZE];
        tty_error_msg(rc, error_message, ERRMSG_SIZE);

        LOGF_ERROR("Reading current position failed: %s.", error_message);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    CurrentFilter = atoi(res) + 1;
    FilterSlotN[0].value = CurrentFilter;

    return true;
}

bool QHYCFW3::SelectFilter(int f)
{
    TargetFilter = f;
    char cmd[8] = {0}, res[8] = {0};
    int rc = -1, nbytes_written = 0, nbytes_read = 0;

    LOGF_DEBUG("CMD <%d>", TargetFilter - 1);

    snprintf(cmd, 2, "%d", TargetFilter - 1);

    if (isSimulation())
        snprintf(res, 8, "%d", TargetFilter - 1);
    else
    {
        if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
        {
            char error_message[ERRMSG_SIZE];
            tty_error_msg(rc, error_message, ERRMSG_SIZE);

            LOGF_ERROR("Sending command select filter failed: %s", error_message);
            return false;
        }

        if ((rc = tty_read(PortFD, res, 1, 30, &nbytes_read)) != TTY_OK)
        {
            char error_message[ERRMSG_SIZE];
            tty_error_msg(rc, error_message, ERRMSG_SIZE);

            LOGF_ERROR("Reading select filter response failed: %s", error_message);
            return false;
        }

        LOGF_DEBUG("RES <%s>", res);
    }

    if (atoi(res) + 1 == TargetFilter)
    {
        CurrentFilter = TargetFilter;
        SelectFilterDone(CurrentFilter);
        return true;
    }

    return false;
}
