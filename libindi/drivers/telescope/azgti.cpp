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

#include "azgti.h"
#include "connectionplugins/connectioninterface.h"
#include "connectionplugins/connectiontcp.h"
#include "indicom.h"

#include <libnova/transform.h>
// libnova specifies round() on old systems and it collides with the new gcc 5.x/6.x headers
#define HAVE_ROUND
#include <libnova/utility.h>

#include <cmath>
#include <memory>
#include <cstring>

// We declare an auto pointer to Synscan.
static std::unique_ptr<AZGTI> gti(new AZGTI());

void ISGetProperties(const char *dev)
{
    gti->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    gti->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    gti->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    gti->ISNewNumber(dev, name, values, names, n);
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
    gti->ISSnoopDevice(root);
}

AZGTI::AZGTI() : SynscanDriver()
{

}

const char *AZGTI::getDefaultName()
{
    return "AZ GTI";
}

bool AZGTI::initProperties()
{
    SynscanDriver::initProperties();

    SetParkDataType(PARK_AZ_ALT);

    setTelescopeConnection(CONNECTION_TCP);
    tcpConnection->setDefaultHost("192.168.4.2");
    tcpConnection->setDefaultPort(11882);

    return true;
}

bool AZGTI::AnalyzeMount()
{
    NewFirmware = true;
    // FIXME Set arbitrary version for AZ-GTI WiFi until we find a reliable way to detect the version.
    FirmwareVersion = 5.0;
    HandsetFwVersion = std::to_string(FirmwareVersion);

    // Set to AZ Mount
    MountCode = 128;

    return SynscanDriver::AnalyzeMount();
}

bool AZGTI::Park()
{
    return true;
}

bool AZGTI::UnPark()
{   
    return true;
}

bool AZGTI::SetCurrentPark()
{
    return false;
}

bool AZGTI::SetDefaultPark()
{
    return true;
}
