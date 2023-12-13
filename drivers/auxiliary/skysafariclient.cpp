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

#include "skysafariclient.h"

#include <cmath>
#include <cstring>

/**************************************************************************************
**
***************************************************************************************/
SkySafariClient::SkySafariClient()
{
    isReady = mountOnline = false;
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
void SkySafariClient::newDevice(INDI::BaseDevice dp)
{
    IDLog("Receiving %s Device...\n", dp.getDeviceName());

    if (dp.isDeviceNameMatch(mount))
        mountOnline = true;

    if (mountOnline)
        isReady = true;
}

/**************************************************************************************
**
*************************************************************************************/
void SkySafariClient::newProperty(INDI::Property property)
{
    if (property.isNameMatch("TELESCOPE_PARK"))
        mountParkSP = property.getSwitch();
    else if (property.isNameMatch("EQUATORIAL_EOD_COORD"))
        eqCoordsNP = property.getNumber();
    else if (property.isNameMatch("GEOGRAPHIC_COORD"))
        geoCoordsNP = property.getNumber();
    else if (property.isNameMatch("ON_COORD_SET"))
        gotoModeSP = property.getSwitch();
    else if (property.isNameMatch("TELESCOPE_ABORT_MOTION"))
        abortSP = property.getSwitch();
    else if (property.isNameMatch("TELESCOPE_SLEW_RATE"))
        slewRateSP = property.getSwitch();
    else if (property.isNameMatch("TELESCOPE_MOTION_NS"))
        motionNSSP = property.getSwitch();
    else if (property.isNameMatch("TELESCOPE_MOTION_WE"))
        motionWESP = property.getSwitch();
    else if (property.isNameMatch("TIME_UTC"))
        timeUTC = property.getText();
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
    if (mountParkSP == nullptr)
        return false;

    auto sw = mountParkSP->findWidgetByName("PARK");

    if (sw == nullptr)
        return false;

    mountParkSP->reset();
    sw->setState(ISS_ON);

    mountParkSP->setState(IPS_BUSY);

    sendNewSwitch(mountParkSP);

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
IPState SkySafariClient::getMountParkState()
{
    return mountParkSP->getState();
}

/**************************************************************************************
**
***************************************************************************************/
bool SkySafariClient::sendEquatorialCoords()
{
    if (eqCoordsNP == nullptr)
        return false;

    eqCoordsNP->setState(IPS_BUSY);
    sendNewNumber(eqCoordsNP);
    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool SkySafariClient::sendGeographicCoords()
{
    if (geoCoordsNP == nullptr)
        return false;

    geoCoordsNP->setState(IPS_BUSY);
    sendNewNumber(geoCoordsNP);
    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool SkySafariClient::sendGotoMode()
{
    if (gotoModeSP == nullptr)
        return false;

    sendNewSwitch(gotoModeSP);
    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool SkySafariClient::abort()
{
    if (abortSP == nullptr)
        return false;

    abortSP->at(0)->setState(ISS_ON);

    sendNewSwitch(abortSP);
    return true;
}

/**************************************************************************************
** We get 0 to 3 which we have to map to whatever supported by mount, if any
***************************************************************************************/
bool SkySafariClient::setSlewRate(int slewRate)
{
    if (slewRateSP == nullptr)
        return false;

    int maxSlewRate = slewRateSP->count() - 1;

    int finalSlewRate = slewRate;

    // If slew rate is between min and max, we interpolate
    if (slewRate > 0 && slewRate < maxSlewRate)
        finalSlewRate = static_cast<int>(ceil(slewRate * maxSlewRate / 3.0));

    slewRateSP->reset();
    slewRateSP->at(finalSlewRate)->setState(ISS_ON);

    sendNewSwitch(slewRateSP);

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool SkySafariClient::setMotionNS()
{
    if (motionNSSP == nullptr)
        return false;

    sendNewSwitch(motionNSSP);

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool SkySafariClient::setMotionWE()
{
    if (motionWESP == nullptr)
        return false;

    sendNewSwitch(motionWESP);

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool SkySafariClient::setTimeUTC()
{
    if (timeUTC == nullptr)
        return false;

    sendNewText(timeUTC);

    return true;
}
