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

#include "lx200stargofocuser.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>

// We declare an auto pointer to LX200StarGo
std::unique_ptr<LX200StarGo> telescope;
std::unique_ptr<LX200StarGoFocuser> focuser;


void ISInit()
{
    static int isInit = 0;

    if (isInit)
        return;

    isInit = 1;
    if (telescope.get() == 0) {
        LX200StarGo* myScope = new LX200StarGo();
        telescope.reset(myScope);
        focuser.reset(new LX200StarGoFocuser(myScope, "AUX1 Focuser"));
    }

}

void ISGetProperties(const char *dev)
{
    ISInit();
    telescope->ISGetProperties(dev);
//    focuser->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    ISInit();
    telescope->ISNewSwitch(dev, name, states, names, n);
    focuser->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    ISInit();
    telescope->ISNewText(dev, name, texts, names, n);
//    focuser->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    ISInit();
    telescope->ISNewNumber(dev, name, values, names, n);
    focuser->ISNewNumber(dev, name, values, names, n);
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
    ISInit();
    telescope->ISSnoopDevice(root);
//    focuser->ISSnoopDevice(root);
}

/**************************************************
*** LX200 Generic Implementation
***************************************************/


LX200StarGo::LX200StarGo()
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

    setLX200Capability(LX200_HAS_PULSE_GUIDING | LX200_HAS_SITES);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TRACK_MODE | TELESCOPE_HAS_LOCATION| TELESCOPE_HAS_PIER_SIDE, 4);
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
        LOG_ERROR("Error communication with telescope.");
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
            if (IUUpdateSwitch(&TrackModeSP, states, names, n) < 0)
                return false;
            int trackMode = IUFindOnSwitchIndex(&TrackModeSP);

            bool result;
            if (trackMode != 3) {
                result = SetTrackMode(trackMode);
            } else {
                result = querySetTracking(false);
            }
            if (result) TrackModeSP.s = IPS_OK;
            else TrackModeSP.s = IPS_ALERT;

            IDSetSwitch(&TrackModeSP, nullptr);
            return result;
         }
    }

    //  Nobody has claimed this, so pass it to the parent
    return LX200Telescope::ISNewSwitch(dev, name, states, names, n);
}


/**************************************************************************************
**
***************************************************************************************/
bool LX200StarGo::initProperties()
{
    /* Make sure to init parent properties first */
    if (!LX200Telescope::initProperties()) return false;

    IUFillSwitch(&SyncHomeS[0], "SYNC_HOME", "Sync Home", ISS_OFF);
    IUFillSwitchVector(&SyncHomeSP, SyncHomeS, 1, getDeviceName(), "TELESCOPE_SYNC_HOME", "Home Position", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillText(&MountFirmwareInfoT[0], "MOUNT_FIRMWARE_INFO", "Firmware", "");
    IUFillTextVector(&MountInfoTP, MountFirmwareInfoT, 1, getDeviceName(), "MOUNT_INFO", "Mount Info", INFO_TAB, IP_RO, 60, IPS_OK);

    // overwrite the custom tracking mode button
    IUFillSwitch(&TrackModeS[3], "TRACK_NONE", "None", ISS_OFF);

    focuser->initProperties("AUX1 Focuser");

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200StarGo::updateProperties()
{
    char firmwareInfo[48] = {0};

    if (! LX200Telescope::updateProperties()) return false;

    if (isConnected())
    {
        defineSwitch(&SyncHomeSP);
        if (queryFirmwareInfo(firmwareInfo)) {
            MountFirmwareInfoT[0].text = firmwareInfo;
            defineText(&MountInfoTP);
        }
        bool isParked, isSynched;
        if (queryParkSync(&isParked, &isSynched)) {
            if (isParked) {
                TrackState = SCOPE_PARKED;
                ParkS[0].s = ISS_ON;
                ParkS[1].s = ISS_OFF;
            } else {
                ParkS[1].s = ISS_ON;
                ParkS[0].s = ISS_OFF;
            }
            IDSetSwitch(&ParkSP, nullptr);
            if (isSynched) {
                SyncHomeS[0].s = ISS_ON;
                SyncHomeSP.s = IPS_OK;
                IDSetSwitch(&SyncHomeSP, nullptr);
            }
        }
    }
    else
    {
        deleteProperty(SyncHomeSP.name);
        deleteProperty(MountInfoTP.name);
    }

    focuser->updateProperties();

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200StarGo::ReadScopeStatus()
{
    bool result;

    result = LX200Telescope::ReadScopeStatus();
    result = UpdateMotionStatus();

    return result;
}

bool LX200StarGo::isSlewComplete()
{
    return (queryIsSlewComplete());
}


/**************************************************************************************
**
***************************************************************************************/

bool LX200StarGo::UpdateMotionStatus() {

    int motorsState, speedState, nrTrackingSpeed = -1;
    bool result;

    result = queryMountMotionState(&motorsState, &speedState, &nrTrackingSpeed);

    // m = 0 both motors are OFF (no power)
    // m = 1 RA motor OFF DEC motor ON
    // m = 2 RA motor ON DEC motor OFF
    // m = 3 both motors are ON
    switch (motorsState) {
    case 0:
        if (TrackState == SCOPE_PARKING) {
            TrackState = SCOPE_PARKED;
            ParkS[0].s = ISS_ON;
            ParkS[1].s = ISS_OFF;
            ParkSP.s   = IPS_OK;
            IDSetSwitch(&ParkSP, nullptr);
        }
        break;
    case 1:
    case 2:
    case 3:
        if (TrackState != SCOPE_PARKING && TrackState != SCOPE_SLEWING) {
            TrackState = SCOPE_TRACKING;
        }
        break;
    }

    // Tracking speeds
    // t = 0 no tracking at all
    // t = 1 tracking at moon speed
    // t = 2 tracking at sun speed
    // t = 3 tracking at stars speed (sidereal speed)
    switch (speedState) {
    case 0:
        if (TrackModeS[TRACK_CUSTOM].s != ISS_ON) {
            IUResetSwitch(&TrackModeSP);
            TrackModeS[TRACK_CUSTOM].s = ISS_ON;
            TrackModeSP.s   = IPS_OK;
            IDSetSwitch(&TrackModeSP, nullptr);
        }
        if (TrackState != SCOPE_PARKING && TrackState != SCOPE_SLEWING) {
            TrackState = SCOPE_IDLE;
        }
        break;
    case 1:
        if (TrackModeS[TRACK_LUNAR].s != ISS_ON) {
            IUResetSwitch(&TrackModeSP);
            TrackModeS[TRACK_LUNAR].s = ISS_ON;
            TrackModeSP.s   = IPS_OK;
            IDSetSwitch(&TrackModeSP, nullptr);
        }
        if (TrackState != SCOPE_PARKING && TrackState != SCOPE_SLEWING) {
            TrackState = (motorsState == 0) ? SCOPE_IDLE : SCOPE_TRACKING;
        }
        break;
    case 2:
        if (TrackModeS[TRACK_SOLAR].s != ISS_ON) {
            IUResetSwitch(&TrackModeSP);
            TrackModeS[TRACK_SOLAR].s = ISS_ON;
            TrackModeSP.s   = IPS_OK;
            IDSetSwitch(&TrackModeSP, nullptr);
        }
        if (TrackState != SCOPE_PARKING && TrackState != SCOPE_SLEWING) {
            TrackState = (motorsState == 0) ? SCOPE_IDLE : SCOPE_TRACKING;
        }
        break;
    case 3:
        if (TrackModeS[TRACK_SIDEREAL].s != ISS_ON) {
            IUResetSwitch(&TrackModeSP);
            TrackModeS[TRACK_SIDEREAL].s = ISS_ON;
            TrackModeSP.s   = IPS_OK;
            IDSetSwitch(&TrackModeSP, nullptr);
        }
        if (TrackState != SCOPE_PARKING && TrackState != SCOPE_SLEWING) {
            TrackState = (motorsState == 0) ? SCOPE_IDLE : SCOPE_TRACKING;
        }
        break;
    }

    // No tracking speeds
    // s = 0 GUIDE speed
    // s = 1 CENTERING speed
    // s = 2 FINDING speed
    // s = 3 SLEWING speed
    switch (nrTrackingSpeed) {
    case 0:
        if (CurrentSlewRate != SLEW_GUIDE) {
            IUResetSwitch(&SlewRateSP);
            CurrentSlewRate = SLEW_GUIDE;
            SlewRateS[SLEW_GUIDE].s = ISS_ON;
            SlewRateSP.s   = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);
        }
        break;
    case 1:
        if (CurrentSlewRate != SLEW_CENTERING) {
            IUResetSwitch(&SlewRateSP);
            CurrentSlewRate = SLEW_CENTERING;
            SlewRateS[SLEW_CENTERING].s = ISS_ON;
            SlewRateSP.s   = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);
        }
        break;
    case 2:
        if (CurrentSlewRate != SLEW_FIND) {
            IUResetSwitch(&SlewRateSP);
            CurrentSlewRate = SLEW_FIND;
            SlewRateS[SLEW_FIND].s = ISS_ON;
            SlewRateSP.s   = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);
        }
        break;
    case 3:
        if (CurrentSlewRate != SLEW_MAX) {
            IUResetSwitch(&SlewRateSP);
            CurrentSlewRate = SLEW_MAX;
            SlewRateS[SLEW_MAX].s = ISS_ON;
            SlewRateSP.s   = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);
        }
        break;
    }


    return result;
}


/**************************************************************************************
**
***************************************************************************************/

bool LX200StarGo::syncHomePosition()
{
    double siteLong = 0.0;
    double lst;

    // step one: determine site longitude
    if (getSiteLongitude(&siteLong) != TTY_OK)
    {
        LOG_WARN("Failed to get site latitude from device.");
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
        LOGF_INFO("%s: Synching home position succeeded.", getDeviceName());
        SyncHomeSP.s = IPS_OK;
    } else
    {
        LOG_WARN("Synching home position failed.");
        SyncHomeSP.s = IPS_ALERT;
    }
    IDSetSwitch(&SyncHomeSP, nullptr);
    return (result == TTY_OK);
}


/**************************************************************************************
**
***************************************************************************************/

void LX200StarGo::getBasicData()
{
    if (!isSimulation())
    {
        checkLX200Format(PortFD);

        if (genericCapability & LX200_HAS_ALIGNMENT_TYPE)
            getAlignment();


        if (genericCapability & LX200_HAS_TRACKING_FREQ)
        {
            if (getTrackFreq(PortFD, &TrackFreqN[0].value) < 0)
                LOG_ERROR("Failed to get tracking frequency from device.");
            else
                IDSetNumber(&TrackingFreqNP, nullptr);
        }

    }

    if (sendLocationOnStartup && (GetTelescopeCapability() & TELESCOPE_HAS_LOCATION))
        sendScopeLocation();
    if (sendTimeOnStartup && (GetTelescopeCapability() & TELESCOPE_HAS_TIME))
        sendScopeTime();
}


/**************************************************************************************
**
***************************************************************************************/
bool LX200StarGo::sendScopeLocation()
{
    if (isSimulation())
    {
        return LX200Telescope::sendScopeLocation();
    }

    double siteLat = 0.0, siteLong = 0.0;
    if (getSiteLatitude(&siteLat) != TTY_OK)
    {
        LOG_WARN("Failed to get site latitude from device.");
        return false;
    }
    if (getSiteLongitude(&siteLong) != TTY_OK)
    {
        LOG_WARN("Failed to get site longitude from device.");
        return false;
    }
    LocationNP.np[LOCATION_LATITUDE].value = siteLat;
    LocationNP.np[LOCATION_LONGITUDE].value = siteLong;

    LOGF_DEBUG("Mount Controller Latitude: %g Longitude: %g", LocationN[LOCATION_LATITUDE].value, LocationN[LOCATION_LONGITUDE].value);

    IDSetNumber(&LocationNP, nullptr);

    return true;
}


/**************************************************************************************
**
***************************************************************************************/

bool LX200StarGo::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (isSimulation())
        return true;

    LOGF_DEBUG("Setting site longitude '%lf'", longitude);
    if (!isSimulation() && ! querySetSiteLongitude(longitude))
    {
        LOG_ERROR("Error setting site longitude coordinates");
        return false;
    }

    if (!isSimulation() && ! querySetSiteLatitude(latitude))
    {
        LOG_ERROR("Error setting site latitude coordinates");
        return false;
    }

    char l[32]={0}, L[32]={0};
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    LOGF_INFO("Site location updated to Lat %.32s - Long %.32s", l, L);

    return true;
}

/*
 * Determine the site latitude. In contrast to a standard LX200 implementation,
 * StarGo returns the location in arc seconds precision.
 */

int LX200StarGo::getSiteLatitude(double *siteLat) {
    return getCommandSexa(PortFD, siteLat, ":Gt#");
}

/*
 * Determine the site longitude. In contrast to a standard LX200 implementation,
 * StarGo returns the location in arc seconds precision.
 */

int LX200StarGo::getSiteLongitude(double *siteLong) {
    return getCommandSexa(PortFD, siteLong, ":Gg#");
}


/**************************************************************************************
**
***************************************************************************************/

bool LX200StarGo::Park() {
    // in: :X362#
    // out: "pB#"

    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};
    if (sendQuery(":X362#", response) && strcmp(response, "pB#") == 0) {
        LOGF_INFO("%s: Parking scope...", getDeviceName());

        // update state
        TrackState = SCOPE_PARKING;
        return true;
    } else {
        LOGF_ERROR("%s: Parking failed.", getDeviceName());
        return false;
    }


}

bool LX200StarGo::UnPark() {
    // in: :X370#
    // out: "p0#"

    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};
    if (sendQuery(":X370#", response) && strcmp(response, "p0#") == 0) {
        LOGF_INFO("%s: Scope Unparked.", getDeviceName());
        TrackState = SCOPE_TRACKING;
        return true;
    } else {
        LOGF_ERROR("%s: Unpark failed.", getDeviceName());
        return false;
    }

}

/*********************************************************************************
 * config file
 *********************************************************************************/

bool LX200StarGo::saveConfigItems(FILE *fp)
{
    LX200Telescope::saveConfigItems(fp);

    IUSaveConfigText(fp, &SiteNameTP);

    return true;
}

/*********************************************************************************
 * Queries
 *********************************************************************************/

/**
 * @brief Send a LX200 query to the communication port and read the result.
 * @param cmd LX200 query
 * @param response answer
 * @return true if the command succeeded, false otherwise
 */
bool LX200StarGo::sendQuery(const char* cmd, char* response) {
    flush();
    if(!transmit(cmd)) {
        LOGF_ERROR("%s: query <%s> failed.", getDeviceName(), cmd);
        return false;
    }
    int bytesReceived = 0;
    if (!receive(response, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive response to <%s>.", getDeviceName(), cmd);
        return false;
    }
    return true;
}

/*
 * Determine the site longitude. In contrast to a standard LX200 implementation,
 * StarGo returns the location in arc seconds precision.
 */
bool LX200StarGo::querySetSiteLongitude(double longitude)
{
    int d, m, s;
    char command[32];
    if (longitude > 180) longitude = longitude - 360;
    if (longitude < -180) longitude = 360 + longitude;

    getSexComponents(longitude, &d, &m, &s);

    const char* format = ":Sg+%03d*%02d:%02d#";
    if (d < 0 || m < 0 || s < 0) format = ":Sg%04d*%02u:%02u#";

    snprintf(command, sizeof(command), format, d, m, s);

    LOGF_DEBUG("%s: Sending set site longitude request '%s'", getDeviceName(), command);

    bool result = setStandardProcedureAvalon(command, 500);

    return (result);
}


/**
 * @brief Set the site latitude
 * @param latitude value
 * @return true iff the command succeeded
 */
bool LX200StarGo::querySetSiteLatitude(double Lat)
{
    int d, m, s;
    char command[32];

    getSexComponents(Lat, &d, &m, &s);

    snprintf(command, sizeof(command), ":St%+03d*%02d:%02d#", d, m, s);

    LOGF_DEBUG("%s: Sending set site longitude request '%s'", getDeviceName(), command);

    return (setStandardProcedureAvalon(command, 500));
}

/**
 * @brief LX200 query whether slewing is completed
 * @return true iff both motors are back to tracking or stopped
 */
bool LX200StarGo::queryIsSlewComplete()
{
    // Command  - :X34#
    // the StarGo replies mxy# where x is the RA / AZ motor status and y
    // the DEC / ALT motor status meaning:
    //    x (y) = 0 motor x (y) stopped or unpowered
    //             (use :X3C# if you want  distinguish if stopped or unpowered)
    //    x (y) = 1 motor x (y) returned in tracking mode
    //    x (y) = 2 motor x (y) acelerating
    //    x (y) = 3 motor x (y) decelerating
    //    x (y) = 4 motor x (y) moving at low speed to refine
    //    x (y) = 5 motor x (y) movig at high speed to target

    flush();
    if (!transmit(":X34#")) {
        LOGF_ERROR("%s: Failed to send query slewing state command.", getDeviceName());
        return false;
    }
    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};
    int bytesReceived = 0;
    if (!receive(response, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive query slewing state response.", getDeviceName());
        return false;
    }
    flush();

    int x, y;
    int returnCode = sscanf(response, "m%01d%01d#", &x, &y);
    if (returnCode <= 0) {
       LOGF_ERROR("%s: Failed to parse query slewing state response '%s'.", getDeviceName(), response);
       return false;
    }

    LOGF_DEBUG("%s: slewing state motors = (%d, %d)", getDeviceName(), x, y);

    return (x < 2 && y < 2);
}



/**
 * @brief Query the motion state of the mount.
 * @param motorsState - tracking status of RA and DEC motor
 * @param speedState - 0 no tracking at all, 1 tracking at moon speed
 *        2 tracking at sun speed, 3 tracking at stars speed (sidereal speed)
 * @param nrTrackingSpeed
 * @return true if the command succeeded, false otherwise
 */
bool LX200StarGo::queryMountMotionState(int* motorsState, int* speedState, int* nrTrackingSpeed) {
    // Command  - :X3C#

    flush();
    if (!transmit(":X3C#")) {
        LOGF_ERROR("%s: Failed to send query mount motion state command.", getDeviceName());
        return false;
    }
    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};
    int bytesReceived = 0;
    if (!receive(response, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive query mount motion state response.", getDeviceName());
        return false;
    }
    flush();
    int tempMotorsState = 0;
    int tempSpeedState = 0;
    int tempNrTrackingSpeed = 0;
    int returnCode = sscanf(response, ":Z1%01d%01d%01d", &tempMotorsState, &tempSpeedState, &tempNrTrackingSpeed);
    if (returnCode <= 0) {
       LOGF_ERROR("%s: Failed to parse query mount motion state response '%s'.", getDeviceName(), response);
       return false;
    }
    (*motorsState) = tempMotorsState;
    (*speedState) = tempSpeedState;
    (*nrTrackingSpeed) = tempNrTrackingSpeed;

    return true;
}


/**
 * @brief Enable / disable tracking of the mount
 * @param enable if true, tracking is enabled
 * @return true if the command succeeded, false otherwise
 */
bool LX200StarGo::queryParkSync (bool* isParked, bool* isSynched) {
    // Command   - :X38#
    // Answer unparked         - p0
    // Answer at home position - p1
    // Answer parked           - p2

    int bytesReceived = 0;
    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};

    flush();
    if (!transmit(":X38#")) {
        LOGF_ERROR("%s: Failed to send get parking status request.", getDeviceName());
        return false;
    }
    if (!receive(response, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive get parking status response.", getDeviceName());
        return false;
    }
    int answer = 0;
    if (! sscanf(response, "p%01d", &answer)) {
        LOGF_ERROR("%s: Unexpected parking status response '%s'.", getDeviceName(), response);
        return false;
    }

    switch (answer) {
    case 0: (*isParked) = false; (*isSynched) = false; break;
    case 1: (*isParked) = false; (*isSynched) = true; break;
    case 2: (*isParked) = true; (*isSynched) = true; break;
    }
    return true;
}

bool LX200StarGo::querySetTracking (bool enable) {
    // Command tracking on  - :X122#
    //         tracking off - :X120#

    flush();
    if (enable) {
        if (!transmit(":X122#")) {
            LOGF_ERROR("%s: Failed to send query for enable tracking.", getDeviceName());
            return false;
        }
    } else {
        if (!transmit(":X120#")) {
            LOGF_ERROR("%s: Failed to send query for disable tracking.", getDeviceName());
            return false;
        }
    }
    return true;
}


/**
 * @brief Retrieve the firmware info from the mount
 * @param firmwareInfo - firmware description
 * @return
 */
bool LX200StarGo::queryFirmwareInfo (char* firmwareInfo) {

    int bytesReceived = 0;
    std::string infoStr;
    char manufacturer[AVALON_RESPONSE_BUFFER_LENGTH] = {0};

    // step 1: retrieve manufacturer
    flush();
    if (!transmit(":GVP#")) {
        LOGF_ERROR("%s: Failed to send get manufacturer request.", getDeviceName());
        return false;
    }
    if (!receive(manufacturer, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive get manufacturer response.", getDeviceName());
        return false;
    }
    // Replace # with \0
    infoStr.assign(manufacturer, bytesReceived - 1);
    flush();

    // step 2: retrieve firmware version
    char firmwareVersion[AVALON_RESPONSE_BUFFER_LENGTH] = {0};
    if (!transmit(":GVN#")) {
        LOGF_ERROR("%s: Failed to send get firmware version request.", getDeviceName());
        return false;
    }
    if (!receive(firmwareVersion, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive get firmware version response.", getDeviceName());
        return false;
    }
    infoStr.append(" - ").append(firmwareVersion, bytesReceived -1);
    flush();

    // step 3: retrieve firmware date
    char firmwareDate[AVALON_RESPONSE_BUFFER_LENGTH] = {0};
    if (!transmit(":GVD#")) {
        LOGF_ERROR("%s: Failed to send get firmware date request.", getDeviceName());
        return false;
    }
    if (!receive(firmwareDate, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive get firmware date response.", getDeviceName());
        return false;
    }
    infoStr.append(" - ").append(firmwareDate, 1, bytesReceived - 2);

    strcpy(firmwareInfo, infoStr.c_str());

    return true;
}

/*********************************************************************************
 * Helper functions
 *********************************************************************************/


/**
 * @brief Receive answer from the communication port.
 * @param buffer - buffer holding the answer
 * @param bytes - number of bytes contained in the answer
 * @author CanisUrsa
 * @return true if communication succeeded, false otherwise
 */
bool LX200StarGo::receive(char* buffer, int* bytes) {
    int returnCode = tty_read_section(PortFD, buffer, '#', AVALON_TIMEOUT, bytes);
    if (returnCode != TTY_OK) {
        char errorString[MAXRBUF];
        tty_error_msg(returnCode, errorString, MAXRBUF);
        LOGF_WARN("%s: Failed to receive full response: %s.", getDeviceName(), errorString);
        return false;
    }
    return true;
}

/**
 * @brief Flush the communication port.
 * @author CanisUrsa
 */
void LX200StarGo::flush() {
    tcflush(PortFD, TCIOFLUSH);
}

bool LX200StarGo::transmit(const char* buffer) {
    int bytesWritten = 0;
    int returnCode = tty_write_string(PortFD, buffer, &bytesWritten);
    if (returnCode != TTY_OK) {
        char errorString[MAXRBUF];
        tty_error_msg(returnCode, errorString, MAXRBUF);
        LOGF_WARN("%s: Failed to transmit %s. Wrote %d bytes and got error %s.", getDeviceName(), buffer, bytesWritten, errorString);
        return false;
    }
    return true;
}

/**
 * @brief standard set command expecting '0' als confirmation
 * @param command command to be executed
 * @param wait time in ms the command should wait before reading the result
 * @return true iff the command succeeded
 */
bool LX200StarGo::setStandardProcedureAvalon(const char* command, int wait) {
    int bytesReceived = 0;
    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};

    flush();
    if (!transmit(command)) {
        LOGF_ERROR("%s: Failed to send request.", getDeviceName());
        return false;
    }

    if (wait > 0) usleep(wait*1000);

    if (!receive(response, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive get response.", getDeviceName());
        return false;
    }


    if (bytesReceived != 2 || response[0] != '0') {
        LOGF_ERROR("%s: Unexpected response '%s'", getDeviceName(), response);
        return false;
    }

    return true;

}

