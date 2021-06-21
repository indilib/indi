/*******************************************************************************
  Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

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

#include "manual_filter.h"

#include <memory>
#include <cstring>

// We declare an auto pointer to ManualFilter.
static std::unique_ptr<ManualFilter> manual_filter(new ManualFilter());

const char *ManualFilter::getDefaultName()
{
    return static_cast<const char *>("Manual Filter");
}

void ManualFilter::ISGetProperties(const char *dev)
{
    INDI::FilterWheel::ISGetProperties(dev);

    defineProperty(&MaxFiltersNP);
    loadConfig(true, MaxFiltersNP.name);
}

bool ManualFilter::initProperties()
{
    INDI::FilterWheel::initProperties();

    // User Set Filter
    IUFillSwitch(&FilterSetS[0], "FILTER_SET", "Filter is set", ISS_OFF);
    IUFillSwitchVector(&FilterSetSP, FilterSetS, 1, getDeviceName(), "CONFIRM_FILTER_SET", "Confirm", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Sync Filter Position. Sets current position to different position without actually changing filter.
    IUFillNumber(&SyncN[0], "TARGET_FILTER", "Target Filter", "%.f", 1, 16, 1, 0);
    IUFillNumberVector(&SyncNP, SyncN, 1, getDeviceName(), "SYNC_FILTER", "Sync", MAIN_CONTROL_TAB, IP_WO, 60, IPS_IDLE);

    // Max number of filters
    IUFillNumber(&MaxFiltersN[0], "MAX", "Filters", "%.f", 1, 16, 1, 5);
    IUFillNumberVector(&MaxFiltersNP, MaxFiltersN, 1, getDeviceName(), "MAX_FILTERS", "Max.", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    return true;
}

bool ManualFilter::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        deleteProperty(MaxFiltersNP.name);

        defineProperty(&SyncNP);
        defineProperty(&FilterSetSP);
    }
    else
    {
        deleteProperty(SyncNP.name);
        deleteProperty(FilterSetSP.name);

        defineProperty(&MaxFiltersNP);
    }

    return true;
}

bool ManualFilter::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, SyncNP.name))
        {
            IUUpdateNumber(&SyncNP, values, names, n);
            CurrentFilter = SyncN[0].value;
            FilterSlotN[0].value = CurrentFilter;
            IDSetNumber(&FilterSlotNP, nullptr);
            SyncNP.s = IPS_OK;
            IDSetNumber(&SyncNP, nullptr);

            LOGF_INFO("Filter wheel is synced to slot %d", CurrentFilter);

            return true;
        }
        else if (!strcmp(MaxFiltersNP.name, name))
        {
            IUUpdateNumber(&MaxFiltersNP, values, names, n);
            FilterSlotN[0].max = MaxFiltersN[0].value;
            MaxFiltersNP.s = IPS_OK;
            saveConfig(true, MaxFiltersNP.name);
            IDSetNumber(&MaxFiltersNP, nullptr);

            return true;
        }

    }

    return INDI::FilterWheel::ISNewNumber(dev, name, values, names, n);
}

bool ManualFilter::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, FilterSetSP.name))
        {
            SelectFilterDone(CurrentFilter);
            FilterSetSP.s = IPS_OK;
            IDSetSwitch(&FilterSetSP, nullptr);
            return true;
        }

    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

bool ManualFilter::Connect()
{
    return true;
}

bool ManualFilter::Disconnect()
{
    return true;
}

bool ManualFilter::SelectFilter(int f)
{
    CurrentFilter = f;

    FilterSetSP.s = IPS_BUSY;
    IDSetSwitch(&FilterSetSP, nullptr);
    LOGF_INFO("Please change filter to %s then click Filter is set when done.", FilterNameT[f-1].text);
    return true;
}

bool ManualFilter::saveConfigItems(FILE *fp)
{
    INDI::FilterWheel::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &MaxFiltersNP);

    return true;
}
