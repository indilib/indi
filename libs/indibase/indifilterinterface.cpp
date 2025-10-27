/*
    Filter Interface
    Copyright (C) 2011 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indifilterinterface.h"
#include <cstring>
#include "indilogger.h"
#include "indipropertytext.h"
#include "indipropertynumber.h"

namespace INDI
{

FilterInterface::FilterInterface(DefaultDevice *defaultDevice) : m_defaultDevice(defaultDevice)
{
}

FilterInterface::~FilterInterface()
{
}

void FilterInterface::initProperties(const char *groupName)
{
    // @INDI_STANDARD_PROPERTY@
    FilterSlotNP[0].fill("FILTER_SLOT_VALUE", "Filter", "%.f", 1.0, 12, 1.0, 1.0);
    FilterSlotNP.fill(m_defaultDevice->getDeviceName(), "FILTER_SLOT", "Filter Slot", groupName, IP_RW, 60, IPS_IDLE);

    loadFilterNames();
}

bool FilterInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
        // Define the Filter Slot and name properties
        m_defaultDevice->defineProperty(FilterSlotNP);
        if (FilterNameTP.size() == 0)
        {
            if (GetFilterNames() == true)
                m_defaultDevice->defineProperty(FilterNameTP);
        }
        else
            m_defaultDevice->defineProperty(FilterNameTP);
    }
    else
    {
        m_defaultDevice->deleteProperty(FilterSlotNP);
        m_defaultDevice->deleteProperty(FilterNameTP);
    }

    return true;
}

bool FilterInterface::processNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI_UNUSED(n);

    if (dev && !strcmp(dev, m_defaultDevice->getDeviceName()) && FilterSlotNP.isNameMatch(name))
    {
        TargetFilter = values[0];

        auto np = FilterSlotNP.findWidgetByName(names[0]);

        if (!np)
        {
            FilterSlotNP.setState(IPS_ALERT);
            DEBUGFDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Unknown error. %s is not a member of %s property.",
                         names[0], FilterSlotNP.getName());
            FilterSlotNP.apply();
            return false;
        }

        if (TargetFilter < FilterSlotNP[0].getMin() || TargetFilter > FilterSlotNP[0].getMax())
        {
            FilterSlotNP.setState(IPS_ALERT);
            DEBUGFDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR,
                         "Error: requested slot %d is invalid (Range from %.f to %.f",
                         static_cast<int>(TargetFilter), FilterSlotNP[0].getMin(), FilterSlotNP[0].getMax());
            FilterSlotNP.apply();
            return false;
        }

        FilterSlotNP.setState(IPS_BUSY);
        DEBUGFDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_SESSION, "Setting current filter to slot %d", TargetFilter);

        if (SelectFilter(TargetFilter) == false)
        {
            FilterSlotNP.setState(IPS_ALERT);
        }

        FilterSlotNP.apply();
        return true;
    }

    return false;
}

bool FilterInterface::processText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev && !strcmp(dev, m_defaultDevice->getDeviceName()) && !strcmp(name, "FILTER_NAME"))
    {
        FilterNameTP.update(texts, names, n);
        FilterNameTP.setState(IPS_OK);

        if (m_defaultDevice->isConfigLoading() || SetFilterNames() == true)
        {
            FilterNameTP.apply();
            return true;
        }
        else
        {
            FilterNameTP.setState(IPS_ALERT);
            DEBUGDEVICE(m_defaultDevice->getDeviceName(), Logger::DBG_ERROR, "Error updating names of filters.");
            FilterNameTP.apply();
            return false;
        }
    }

    return false;
}

bool FilterInterface::saveConfigItems(FILE *fp)
{
    FilterSlotNP.save(fp);
    if (FilterNameTP.size() > 0)
        FilterNameTP.save(fp);

    return true;
}

void FilterInterface::SelectFilterDone(int f)
{
    //  The hardware has finished changing  filters
    FilterSlotNP[0].setValue(f);
    FilterSlotNP.setState(IPS_OK);
    FilterSlotNP.apply();
}

void FilterInterface::generateSampleFilters()
{
    char filterName[MAXINDINAME];
    char filterLabel[MAXINDILABEL];
    int MaxFilter = FilterSlotNP[0].getMax();

    const char *filterDesignation[8] = { "Red", "Green", "Blue", "H_Alpha", "SII", "OIII", "LPR", "Luminance" };

    FilterNameTP.resize(0);

    for (int i = 0; i < MaxFilter; i++)
    {
        snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i + 1);
        snprintf(filterLabel, MAXINDILABEL, "Filter#%d", i + 1);

        INDI::WidgetText oneText;
        oneText.fill(filterName, filterLabel, i < 8 ? filterDesignation[i] : filterLabel);
        FilterNameTP.push(std::move(oneText));
    }

    FilterNameTP.fill(m_defaultDevice->getDeviceName(), "FILTER_NAME", "Filter",
                      FilterSlotNP.getGroupName(), IP_RW, 0, IPS_IDLE);
    FilterNameTP.shrink_to_fit();
}

bool FilterInterface::GetFilterNames()
{
    // Generate sample filters if nothing available.
    if (FilterNameTP.isEmpty())
        generateSampleFilters();

    return true;
}

bool FilterInterface::SetFilterNames()
{
    return m_defaultDevice->saveConfig(FilterNameTP);
}

bool FilterInterface::loadFilterNames()
{
    if (FilterNameTP.size() > 0)
        return true;

    char *rname, *rdev;
    XMLEle *root = nullptr, *fproot = nullptr;
    char filterName[MAXINDINAME] = {0};
    char errmsg[MAXRBUF];
    LilXML *lp = newLilXML();

    FILE *fp = IUGetConfigFP(nullptr, m_defaultDevice->getDefaultName(), "r", errmsg);

    if (fp == nullptr)
    {
        delLilXML(lp);
        return false;
    }

    fproot = readXMLFile(fp, lp, errmsg);

    if (fproot == nullptr)
    {
        delLilXML(lp);
        fclose(fp);
        return false;
    }

    for (root = nextXMLEle(fproot, 1); root != nullptr; root = nextXMLEle(fproot, 0))
    {
        /* pull out device and name */
        if (crackDN(root, &rdev, &rname, errmsg) < 0)
        {
            fclose(fp);
            delXMLEle(fproot);
            delLilXML(lp);
            return false;
        }

        // It doesn't belong to our device??
        if (strcmp(m_defaultDevice->getDeviceName(), rdev))
            continue;

        if (!strcmp("FILTER_NAME", rname))
        {
            FilterNameTP.resize(0);

            XMLEle *oneText = nullptr;
            uint8_t counter = 0;

            for (oneText = nextXMLEle(root, 1); oneText != nullptr; oneText = nextXMLEle(root, 0))
            {
                const char *filter = pcdataXMLEle(oneText);
                snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", counter + 1);

                INDI::WidgetText oneWidget;
                oneWidget.fill(filterName, filter, filter);
                FilterNameTP.push(std::move(oneWidget));
                counter++;
            }

            break;
        }
    }

    FilterNameTP.fill(m_defaultDevice->getDeviceName(), "FILTER_NAME", "Filter",
                      FilterSlotNP.getGroupName(), IP_RW, 0, IPS_IDLE);
    FilterNameTP.shrink_to_fit();

    // Filter Slot must match and agree with filter name if it was loaded successfully.
    if (FilterNameTP.count() > 0)
        FilterSlotNP[0].setMax(FilterNameTP.count());

    fclose(fp);
    delXMLEle(fproot);
    delLilXML(lp);

    return true;
}
}
