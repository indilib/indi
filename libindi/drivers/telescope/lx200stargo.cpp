/*
    Avalon StarGo driver

    Copyright (C) 2018 Wolfgang Reissenberger

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
#include "lx200stargo.h"

LX200StarGo::LX200StarGo() : LX200Generic::LX200Generic()
{
    setVersion(0, 1);
    /* missing capabilities
     * TELESCOPE_HAS_TIME:
     *    missing commands
     *      :GG# (Get UTC offset time)
     *      :GL# (Get Local Time in 24 hour format)
     *
     * TELESCOPE_HAS_LOCATION
     * reading the location works, setting location not
     *     missing commands
     *       :SgDDD*MM# (Set current site’s longitude)
     *       :StsDD*MM# (Sets the current site latitude)
     *
     * LX200_HAS_ALIGNMENT_TYPE
     *     missing commands
     *        ACK - Alignment Query
     *
     * LX200_HAS_SITES
     *     missing commands
     *        :GM# (Get Site 1 Name)
     *
     * LX200_HAS_TRACKING_FREQ
     *     missing commands
     *        :GT# (Get tracking rate)
     *
     * untested, hence disabled:
     * LX200_HAS_FOCUS
     */

    setLX200Capability(LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TRACK_MODE,
                           4);
}

/**************************************************************************************
**
***************************************************************************************/
const char *LX200StarGo::getDefaultName()
{
    return (const char *)"Avalon StarGo";
}


/**************************************************************************************
**
***************************************************************************************/
bool LX200StarGo::Handshake()
{
    if (getLX200RA(PortFD, &currentRA) != 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error communication with telescope.");
        return false;
    }

    return true;
}


/**************************************************************************************
**
***************************************************************************************/
bool LX200StarGo::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        // sync home position
        if (!strcmp(name, SyncHomeSP.name))
        {
            syncHomePosition();
        }
        // tracking mode
        else if (!strcmp(name, TrackModeSP.name))
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Changing tracking mode not implemented!");
        }
    }

    //  Nobody has claimed this, so pass it to the parent
    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}


/**************************************************************************************
**
***************************************************************************************/
bool LX200StarGo::initProperties()
{
    /* Make sure to init parent properties first */
    if (!LX200Generic::initProperties()) return false;

    IUFillSwitch(&SyncHomeS[0], "SYNC_HOME", "Sync Home", ISS_OFF);
    IUFillSwitchVector(&SyncHomeSP, SyncHomeS, 1, getDeviceName(), "TELESCOPE_SYNC_HOME", "Home Position", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200StarGo::updateProperties()
{
    if (! LX200Generic::updateProperties()) return false;

    if (isConnected())
    {
        defineSwitch(&SyncHomeSP);
    }
    else
    {
        deleteProperty(SyncHomeSP.name);
    }
    return true;
}


bool LX200StarGo::syncHomePosition()
{
    double siteLong = 0.0;
    double lst;

    // step one: determine site longitude
    if (getSiteLongitude(&siteLong) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Failed to get site latitude from device.");
        return false;
    }

    // determine local sidereal time
    lst = get_local_sidereal_time(siteLong);
    LOGF_DEBUG("Current local sidereal time = %.8f", lst);

    // translate into hh:mm:ss
    int h=0, m=0, s=0;
    getSexComponents(lst, &h, &m, &s);

    char cmd[12];
    sprintf(cmd, ":X31%02d%02d%02d#", h, m, s);
    LOGF_DEBUG("Executing CMD <%s>", cmd);

    int result = TTY_OK;
    result = setStandardProcedure(PortFD, cmd);

    if (result == TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_DEBUG, "Synching home position succeeded.");
        SyncHomeSP.s = IPS_OK;
    } else
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Synching home position failed.");
        SyncHomeSP.s = IPS_ALERT;
    }
    IDSetSwitch(&SyncHomeSP, nullptr);
    return (result == TTY_OK);
}

/*
 * Determine the site latitude. In contrast to a standard LX200 implementation,
 * StarGo returns the location in arc seconds precision.
 */

int LX200StarGo::getSiteLatitude(double *siteLat) {
    return getCommandSexa(PortFD, siteLat, ":Gg#");
}

/*
 * Determine the site longitude. In contrast to a standard LX200 implementation,
 * StarGo returns the location in arc seconds precision.
 */

int LX200StarGo::getSiteLongitude(double *siteLong) {
    return getCommandSexa(PortFD, siteLong, ":Gg#");
}

