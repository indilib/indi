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

#include "qhycfw2.h"

#include "indicom.h"

#include <cstring>
#include <memory>

// We declare an auto pointer to QHYCFW.
static std::unique_ptr<QHYCFW2> qhycfw(new QHYCFW2());

QHYCFW2::QHYCFW2()
{
    setVersion(1, 2);
    setFilterConnection(CONNECTION_SERIAL | CONNECTION_TCP);
}

const char *QHYCFW2::getDefaultName()
{
    return "QHYCFW2";
}

void QHYCFW2::ISGetProperties(const char *dev)
{
    INDI::FilterWheel::ISGetProperties(dev);

    // Only read value when we're offline
    if (isConnected() == false)
    {
        double maxCount = 5;
        IUGetConfigNumber(getDeviceName(), "MAX_FILTER", "Count", &maxCount);
        FilterSlotNP[0].setMax(maxCount);
        if (FilterNameTP.size() != maxCount)
        {
            char filterName[MAXINDINAME];
            char filterLabel[MAXINDILABEL];

            FilterNameTP.resize(0);

            for (int i = 0; i < maxCount; i++)
            {
                snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i + 1);
                snprintf(filterLabel, MAXINDILABEL, "Filter#%d", i + 1);

                INDI::WidgetText oneText;
                oneText.fill(filterName, filterLabel, filterLabel);
                FilterNameTP.push(std::move(oneText));
            }

            FilterNameTP.fill(getDeviceName(), "FILTER_NAME", "Filter",
                              FilterSlotNP.getGroupName(), IP_RW, 0, IPS_IDLE);
            FilterNameTP.shrink_to_fit();
        }
    }
    defineProperty(&MaxFilterNP);
}

bool QHYCFW2::initProperties()
{
    INDI::FilterWheel::initProperties();

    IUFillNumber(&MaxFilterN[0], "Count", "Count", "%.f", 1, 16, 1, 5);
    IUFillNumberVector(&MaxFilterNP, MaxFilterN, 1, getDeviceName(), "MAX_FILTER", "Filters", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    CurrentFilter      = 1;
    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(5);

    addAuxControls();

    return true;
}

bool QHYCFW2::SelectFilter(int f)
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

bool QHYCFW2::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, MaxFilterNP.name))
        {
            if (values[0] != MaxFilterN[0].value)
            {
                IUUpdateNumber(&MaxFilterNP, values, names, n);
                saveConfig();
                LOG_INFO("Max number of filters updated. You must reconnect for this change to take effect.");
            }

            MaxFilterNP.s = IPS_OK;
            IDSetNumber(&MaxFilterNP, nullptr);
            return true;
        }

    }

    return INDI::FilterWheel::ISNewNumber(dev, name, values, names, n);
}

bool QHYCFW2::saveConfigItems(FILE *fp)
{
    FilterWheel::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &MaxFilterNP);

    return true;
}
