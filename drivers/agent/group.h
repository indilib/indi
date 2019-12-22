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

#pragma once

#include "baseclient.h"
#include "defaultdevice.h"
#include <vector>

class Imager;

class Group
{
  public:
    explicit Group(int id, Imager *imager);

    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

    void defineProperties();
    void deleteProperties();

    int filterSlot() const;
    int binning() const;
    double exposure() const;
    int count() const;
  private:
    std::string groupName;
    std::string groupSettingsName;
    Imager* imager;
    INumberVectorProperty GroupSettingsNP;
    std::vector<INumber> GroupSettingsN;
};


