/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

  INDI SkySafari Client for INDI Mounts.

  The clients communicates with INDI server to control the mount from SkySafari

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include <memory>
#include <cstring>

#include "skysafariclient.h"

/**************************************************************************************
**
***************************************************************************************/
SkySafariClient::SkySafariClient()
{
    isReady = isRunning = mountOnline = false;
}

/**************************************************************************************
**
***************************************************************************************/
SkySafariClient::~SkySafariClient()
{

}

/**************************************************************************************
**
***************************************************************************************/
void SkySafariClient::newDevice(INDI::BaseDevice *dp)
{
    IDLog("Receiving %s Device...\n", dp->getDeviceName());

    if (std::string(dp->getDeviceName()) == mount)
        mountOnline = true;

    if (mountOnline)
        isReady = true;
}

/**************************************************************************************
**
*************************************************************************************/
void SkySafariClient::newProperty(INDI::Property *property)
{
    if (!strcmp(property->getName(), "TELESCOPE_PARK"))
        mountParkSP = property->getSwitch();    
}

/**************************************************************************************
**
***************************************************************************************/
void SkySafariClient::setMount(const std::string &value)
{
    mount = value;
    watchDevice(mount.c_str());
}

/**************************************************************************************
**
***************************************************************************************/
bool SkySafariClient::parkMount()
{
    if (mountParkSP == NULL)
        return false;

    ISwitch *sw = IUFindSwitch(mountParkSP, "PARK");

    if (sw == NULL)
        return false;

    IUResetSwitch(mountParkSP);
    sw->s = ISS_ON;

    mountParkSP->s = IPS_BUSY;

    sendNewSwitch(mountParkSP);

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
IPState SkySafariClient::getMountParkState()
{
    return mountParkSP->s;
}

/**************************************************************************************
**
***************************************************************************************/
bool SkySafariClient::sendCoords()
{
    return false;
}

/**************************************************************************************
**
***************************************************************************************/
bool SkySafariClient::setGotoMode()
{

}
