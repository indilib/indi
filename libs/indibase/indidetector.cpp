/*******************************************************************************
 Copyright(c) 2010, 2017 Ilia Platone, Jasem Mutlaq. All rights reserved.


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

#include "defaultdevice.h"
#include "indidetector.h"

#include "indicom.h"
#include "stream/streammanager.h"
#include "locale_compat.h"

#include <fitsio.h>

#include <libnova/julian_day.h>
#include <libnova/ln_types.h>
#include <libnova/precession.h>

#include <regex>

#include <dirent.h>
#include <cerrno>
#include <locale.h>
#include <cstdlib>
#include <zlib.h>
#include <sys/stat.h>

namespace INDI
{

Detector::Detector()
{
}

Detector::~Detector()
{
}

bool Detector::initProperties()
{
    return SensorInterface::initProperties();
}

void Detector::ISGetProperties(const char *dev)
{
    return SensorInterface::processProperties(dev);
}

bool Detector::updateProperties()
{
    return SensorInterface::updateProperties();
}

bool Detector::ISSnoopDevice(XMLEle *root)
{
    return SensorInterface::processSnoopDevice(root);
}

bool Detector::ISNewText(const char *dev, const char *name, char *values[], char *names[], int n)
{
    return SensorInterface::processText(dev, name, values, names, n);
}

bool Detector::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    return SensorInterface::processNumber(dev, name, values, names, n);
}

bool Detector::ISNewSwitch(const char *dev, const char *name, ISState *values, char *names[], int n)
{
    return SensorInterface::processSwitch(dev, name, values, names, n);
}

}
