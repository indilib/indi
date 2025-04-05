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

    defineProperty(MaxFiltersNP);
}

bool ManualFilter::initProperties()
{
    INDI::FilterWheel::initProperties();

    // Max number of filters
    MaxFiltersNP[0].fill("MAX", "Filters", "%.f", 1, 16, 1, 5);
    MaxFiltersNP.fill(getDeviceName(), "MAX_FILTERS", "Max.", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    MaxFiltersNP.load();

    FilterSlotNP[0].setMax(MaxFiltersNP[0].getValue());

    // If exists filter names count is less than our desired max count. Save them then generate samples to fill the rest,
    // then restore the current filters before we overwrote them with generateSampleFilters.
    if (FilterNameTP.count() != MaxFiltersNP[0].getValue())
    {
        if (FilterNameTP.count() < MaxFiltersNP[0].getValue())
        {
            // Save existing labels and texts
            std::vector<std::string> labels, values;
            for (auto oneFilter : FilterNameTP)
            {
                labels.push_back(oneFilter.getLabel());
                values.push_back(oneFilter.getText());
            }

            // Generate all samples to fill the rest
            generateSampleFilters();

            // Now restore the first filter names that we loaded from config
            for (uint8_t i = 0; i < labels.size(); i++)
            {
                FilterNameTP[i].setLabel(labels[i]);
                FilterNameTP[i].setText(values[i]);
            }
        }
        else
        {
            FilterNameTP.resize(MaxFiltersNP[0].getValue());
            FilterNameTP.shrink_to_fit();
        }

        // Make sure to save changes to config immediately.
        saveConfig();
    }

    // User Set Filter
    FilterSetSP[0].fill("FILTER_SET", "Filter is set", ISS_OFF);
    FilterSetSP.fill(getDeviceName(), "CONFIRM_FILTER_SET", "Confirm", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Sync Filter Position. Sets current position to different position without actually changing filter.
    SyncNP[0].fill("TARGET_FILTER", "Target Filter", "%.f", 1, 16, 1, 0);
    SyncNP.fill(getDeviceName(), "SYNC_FILTER", "Sync", MAIN_CONTROL_TAB, IP_WO, 60, IPS_IDLE);

    return true;
}

bool ManualFilter::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        deleteProperty(MaxFiltersNP);

        defineProperty(SyncNP);
        defineProperty(FilterSetSP);
    }
    else
    {
        deleteProperty(SyncNP);
        deleteProperty(FilterSetSP);

        defineProperty(MaxFiltersNP);
    }

    return true;
}

bool ManualFilter::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp(dev, getDeviceName()))
    {
        if (SyncNP.isNameMatch(name))
        {
            SyncNP.update(values, names, n);
            CurrentFilter = SyncNP[0].getValue();
            FilterSlotNP[0].setValue(CurrentFilter);
            FilterSlotNP.apply();
            SyncNP.setState(IPS_OK);
            SyncNP.apply();

            LOGF_INFO("Filter wheel is synced to slot %d", CurrentFilter);

            return true;
        }
        else if (MaxFiltersNP.isNameMatch(name))
        {
            MaxFiltersNP.update(values, names, n);
            FilterSlotNP[0].setMax(MaxFiltersNP[0].getValue());
            MaxFiltersNP.setState(IPS_OK);
            saveConfig(MaxFiltersNP);
            MaxFiltersNP.apply();

            return true;
        }
    }

    return INDI::FilterWheel::ISNewNumber(dev, name, values, names, n);
}

bool ManualFilter::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(dev, getDeviceName()))
    {
        if (FilterSetSP.isNameMatch(name))
        {
            SelectFilterDone(CurrentFilter);
            FilterSetSP.setState(IPS_OK);
            FilterSetSP.apply();
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

    FilterSetSP.setState(IPS_BUSY);
    FilterSetSP.apply();
    LOGF_INFO("Please change filter to %s then click Filter is set when done.", FilterNameTP[f - 1].getText());
    return true;
}

bool ManualFilter::saveConfigItems(FILE *fp)
{
    INDI::FilterWheel::saveConfigItems(fp);
    MaxFiltersNP.save(fp);
    return true;
}
