/*******************************************************************************
 Copyright(c) 2013-2016 CloudMakers, s. r. o. All rights reserved.
 Copyright(c) 2017-2018 Marco Gulino <marco.gulino@gmai.com>

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

#include "group.h"
#include "agent_imager.h"
#include <iomanip>
#include <sstream>

#define GROUP_PREFIX     "GROUP_"
#define GROUP_PREFIX_LEN 6

#define IMAGE_COUNT 0
#define CCD_BINNING 1
#define FILTER_SLOT 2
#define CCD_EXPOSURE_VALUE 3

Group::Group(int id, Imager *imager) : imager{imager}
{
    id++;
    std::stringstream groupNameStream;
    groupNameStream << "Image group " << id;
    groupName = groupNameStream.str();

    std::stringstream groupSettingsNameStream;
    groupSettingsNameStream << GROUP_PREFIX << std::setw(2) << std::setfill('0') << id;
    groupSettingsName = groupSettingsNameStream.str();
    
    GroupSettingsN.resize(4);

    IUFillNumber(&GroupSettingsN[IMAGE_COUNT], "IMAGE_COUNT", "Image count", "%3.0f", 1, 100, 1, 1);
    IUFillNumber(&GroupSettingsN[CCD_BINNING], "CCD_BINNING", "Binning", "%1.0f", 1, 4, 1, 1);
    IUFillNumber(&GroupSettingsN[FILTER_SLOT], "FILTER_SLOT", "Filter", "%2.f", 0, 12, 1, 0);
    IUFillNumber(&GroupSettingsN[CCD_EXPOSURE_VALUE], "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0, 36000, 0, 1.0);
    IUFillNumberVector(&GroupSettingsNP, GroupSettingsN.data(), GroupSettingsN.size(), Imager::DEVICE_NAME.c_str(), groupSettingsName.c_str(), "Image group settings",
                       groupName.c_str(), IP_RW, 60, IPS_IDLE);
}

int Group::binning() const {
    return GroupSettingsN[CCD_BINNING].value;
}

int Group::filterSlot() const {
    return GroupSettingsN[FILTER_SLOT].value;
}

double Group::exposure() const {
    return GroupSettingsN[CCD_EXPOSURE_VALUE].value;
}

int Group::count() const {
    return GroupSettingsN[IMAGE_COUNT].value;
}


bool Group::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI_UNUSED(dev);
    if (groupSettingsName == name)
    {
        IUUpdateNumber(&GroupSettingsNP, values, names, n);
        GroupSettingsNP.s = IPS_OK;
        IDSetNumber(&GroupSettingsNP, nullptr);
        return true;
    }
    return false;
}

void Group::defineProperties()
{
    imager->defineProperty(&GroupSettingsNP);
}

void Group::deleteProperties()
{
    imager->deleteProperty(GroupSettingsNP.name);
}
