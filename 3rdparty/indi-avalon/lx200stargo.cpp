/*
    Avalon StarGo driver

    Copyright (C) 2018 Christopher Contaxis, Wolfgang Reissenberger
    and Tonino Tasselli

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

const char *RA_DEC_TAB = "RA / DEC";

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
    setVersion(0, 5);
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
     *    Makes no sense in combination with KStars?
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
                           TELESCOPE_HAS_TRACK_MODE | TELESCOPE_HAS_LOCATION | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_PIER_SIDE, 4);
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
            return syncHomePosition();
        }

        // goto home position
        if (!strcmp(name, MountGotoHomeSP.name))
        {
            return slewToHome(states, names, n);
        }
        // parking position
        else if (!strcmp(name, MountSetParkSP.name))
        {
            return setParkPosition(states, names, n);
        }
        // tracking mode
        else if (!strcmp(name, TrackModeSP.name))
        {
            if (IUUpdateSwitch(&TrackModeSP, states, names, n) < 0)
                return false;
            int trackMode = IUFindOnSwitchIndex(&TrackModeSP);

            bool result;
            if (trackMode != 3) result = SetTrackMode(trackMode);

            switch (trackMode) {
            case TRACK_SIDEREAL:
                LOG_INFO("Sidereal tracking rate selected.");
                break;
            case TRACK_SOLAR:
                LOG_INFO("Solar tracking rate selected.");
                break;
            case TRACK_LUNAR:
                LOG_INFO("Lunar tracking rate selected");
                break;
            case 3:
                LOG_INFO("Tracking stopped.");
                result = querySetTracking(false);
                break;
            }
            TrackModeSP.s = result ? IPS_OK : IPS_ALERT;

            IDSetSwitch(&TrackModeSP, nullptr);
            return result;
         } else if (!strcmp(name, ST4StatusSP.name)) {
            bool enabled = (states[0] == ISS_OFF);
            bool result = setST4Enabled(enabled);

            if(result) {
                ST4StatusS[0].s = enabled ? ISS_OFF : ISS_ON;
                ST4StatusS[1].s = enabled ? ISS_ON : ISS_OFF;
                ST4StatusSP.s = IPS_OK;
            } else {
                ST4StatusSP.s = IPS_ALERT;
            }
            IDSetSwitch(&ST4StatusSP, nullptr);
            return result;
        } else if (!strcmp(name, MeridianFlipEnabledSP.name)) {
            return setMeridianFlipEnabled(states[0] == ISS_ON);
        } else if (!strcmp(name, MeridianFlipForcedSP.name)) {
            return setMeridianFlipForced(states[0] == ISS_OFF);
        }

    }

    //  Nobody has claimed this, so pass it to the parent
    return LX200Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool LX200StarGo::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0) {

        // sync home position
        if (!strcmp(name, GuidingSpeedNP.name)) {
            int raSpeed  = round(values[0] * 100);
            int decSpeed = round(values[1] * 100);
            bool result  = setGuidingSpeeds(raSpeed, decSpeed);

            if(result) {
                GuidingSpeedP[0].value = static_cast<double>(raSpeed) / 100.0;
                GuidingSpeedP[1].value = static_cast<double>(decSpeed) / 100.0;
                GuidingSpeedNP.s = IPS_OK;
            } else {
                GuidingSpeedNP.s = IPS_ALERT;
            }
            IDSetNumber(&GuidingSpeedNP, nullptr);
            return result;
        }
    }
        //  Nobody has claimed this, so pass it to the parent
    return LX200Telescope::ISNewNumber(dev, name, values, names, n);
}



/**************************************************************************************
**
***************************************************************************************/
bool LX200StarGo::initProperties()
{
    /* Make sure to init parent properties first */
    if (!LX200Telescope::initProperties()) return false;

    IUFillSwitch(&MountGotoHomeS[0], "MOUNT_GOTO_HOME_VALUE", "Goto Home", ISS_OFF);
    IUFillSwitchVector(&MountGotoHomeSP, MountGotoHomeS, 1, getDeviceName(), "MOUNT_GOTO_HOME", "Goto Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    IUFillLight(&MountParkingStatusL[0], "MOUNT_IS_PARKED_VALUE", "Parked", IPS_IDLE);
    IUFillLight(&MountParkingStatusL[1], "MOUNT_IS_UNPARKED_VALUE", "Unparked", IPS_IDLE);
    IUFillLightVector(&MountParkingStatusLP, MountParkingStatusL, 2, getDeviceName(), "PARKING_STATUS", "Parking Status", MAIN_CONTROL_TAB, IPS_IDLE);

    IUFillSwitch(&MountSetParkS[0], "MOUNT_SET_PARK_VALUE", "Set Park", ISS_OFF);
    IUFillSwitchVector(&MountSetParkSP, MountSetParkS, 1, getDeviceName(), "MOUNT_SET_PARK", "Set Park", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    IUFillSwitch(&SyncHomeS[0], "SYNC_HOME", "Sync Home", ISS_OFF);
    IUFillSwitchVector(&SyncHomeSP, SyncHomeS, 1, getDeviceName(), "TELESCOPE_SYNC_HOME", "Home Position", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillText(&MountFirmwareInfoT[0], "MOUNT_FIRMWARE_INFO", "Firmware", "");
    IUFillTextVector(&MountInfoTP, MountFirmwareInfoT, 1, getDeviceName(), "MOUNT_INFO", "Mount Info", INFO_TAB, IP_RO, 60, IPS_OK);

    // Guiding settings
    IUFillNumber(&GuidingSpeedP[0], "GUIDING_SPEED_RA", "RA Speed", "%.2f", 0.0, 2.0, 0.1, 0);
    IUFillNumber(&GuidingSpeedP[1], "GUIDING_SPEED_DEC", "DEC Speed", "%.2f", 0.0, 2.0, 0.1, 0);
    IUFillNumberVector(&GuidingSpeedNP, GuidingSpeedP, 2, getDeviceName(), "GUIDING_SPEED","Autoguiding", RA_DEC_TAB, IP_RW, 60, IPS_IDLE);

    IUFillSwitch(&ST4StatusS[0], "ST4_DISABLED", "disabled", ISS_OFF);
    IUFillSwitch(&ST4StatusS[1], "ST4_ENABLED", "enabled", ISS_OFF);
    IUFillSwitchVector(&ST4StatusSP, ST4StatusS, 2, getDeviceName(), "ST4", "ST4", RA_DEC_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // meridian flip
    IUFillSwitch(&MeridianFlipEnabledS[0], "MERIDIAN_FLIP_ENABLED", "enabled", ISS_OFF);
    IUFillSwitch(&MeridianFlipEnabledS[1], "MERIDIAN_FLIP_DISABLED", "disabled", ISS_OFF);
    IUFillSwitchVector(&MeridianFlipEnabledSP, MeridianFlipEnabledS, 2, getDeviceName(), "ENABLE_MERIDIAN_FLIP", "Meridian Flip", RA_DEC_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);


    IUFillSwitch(&MeridianFlipForcedS[0], "MERIDIAN_FLIP_AUTOMATIC", "automatic", ISS_OFF);
    IUFillSwitch(&MeridianFlipForcedS[1], "MERIDIAN_FLIP_FORCED", "forced", ISS_OFF);
    IUFillSwitchVector(&MeridianFlipForcedSP, MeridianFlipForcedS, 2, getDeviceName(), "FORCE_MERIDIAN_FLIP", "Flip Mode", RA_DEC_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

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
        defineLight(&MountParkingStatusLP);
        defineSwitch(&SyncHomeSP);
        defineSwitch(&MountGotoHomeSP);
        defineSwitch(&MountSetParkSP);
        defineNumber(&GuidingSpeedNP);
        defineSwitch(&ST4StatusSP);
        defineSwitch(&MeridianFlipEnabledSP);
        defineSwitch(&MeridianFlipForcedSP);

        if (queryFirmwareInfo(firmwareInfo)) {
            MountFirmwareInfoT[0].text = firmwareInfo;
            defineText(&MountInfoTP);
        }
        bool isParked, isSynched;
        if (queryParkSync(&isParked, &isSynched)) {
            SetParked(isParked);
            if (isSynched) {
                SyncHomeS[0].s = ISS_ON;
                SyncHomeSP.s = IPS_OK;
                IDSetSwitch(&SyncHomeSP, nullptr);
            }
        }
        bool isEnabled;
        if (queryGetST4Status(&isEnabled)) {
            ST4StatusS[0].s = isEnabled ? ISS_OFF : ISS_ON;
            ST4StatusS[1].s = isEnabled ? ISS_ON : ISS_OFF;
            ST4StatusSP.s = IPS_OK;
        } else {
            ST4StatusSP.s = IPS_ALERT;
        }
        IDSetSwitch(&ST4StatusSP, nullptr);

        if (queryGetMeridianFlipEnabledStatus(&isEnabled)) {
            MeridianFlipEnabledS[0].s = isEnabled ? ISS_ON  : ISS_OFF;
            MeridianFlipEnabledS[1].s = isEnabled ? ISS_OFF : ISS_ON;
            MeridianFlipEnabledSP.s = IPS_OK;
        } else {
            MeridianFlipEnabledSP.s = IPS_ALERT;
        }
        IDSetSwitch(&MeridianFlipEnabledSP, nullptr);

        int raSpeed, decSpeed;
        if (queryGetGuidingSpeeds(&raSpeed, &decSpeed)) {
            GuidingSpeedP[0].value = static_cast<double>(raSpeed) / 100.0;
            GuidingSpeedP[1].value = static_cast<double>(decSpeed) / 100.0;
            GuidingSpeedNP.s = IPS_OK;
        } else {
            GuidingSpeedNP.s = IPS_ALERT;
        }
        IDSetNumber(&GuidingSpeedNP, nullptr);
    }
    else
    {
        deleteProperty(MountParkingStatusLP.name);
        deleteProperty(MountGotoHomeSP.name);
        deleteProperty(MountSetParkSP.name);
        deleteProperty(SyncHomeSP.name);
        deleteProperty(MountInfoTP.name);
        deleteProperty(GuidingSpeedNP.name);
        deleteProperty(ST4StatusSP.name);
        deleteProperty(MeridianFlipEnabledSP.name);
        deleteProperty(MeridianFlipForcedSP.name);
    }

    focuser->updateProperties();

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200StarGo::ReadScopeStatus()
{
    if (!isConnected())
        return false;

    if (isSimulation())
    {
        mountSim();
        return true;
    }

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if LX200 is done slewing
        if (isSlewComplete()) {
            if (isIdle()) {
                TrackState = SCOPE_IDLE;
                LOG_INFO("Slew is complete. Tracking is off." );
            }  else {
                TrackState = SCOPE_TRACKING;
                LOG_INFO("Slew is complete. Tracking...");
            }

            if (MountGotoHomeSP.s == IPS_BUSY) {
                MountGotoHomeSP.s = IPS_OK;
                IDSetSwitch(&MountGotoHomeSP, nullptr);
            }

        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (isSlewComplete()) SetParked(true);
    }

    // update meridian flip status
    bool isEnabled;
    if (queryGetMeridianFlipForcedStatus(&isEnabled)) {
        MeridianFlipForcedS[0].s = isEnabled ? ISS_OFF : ISS_ON;
        MeridianFlipForcedS[1].s = isEnabled ? ISS_ON : ISS_OFF;
        MeridianFlipForcedSP.s = IPS_OK;
    } else {
        MeridianFlipForcedSP.s = IPS_ALERT;
    }
    IDSetSwitch(&MeridianFlipForcedSP, nullptr);

    UpdateMotionStatus();

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading RA/DEC.");
        return false;
    }

    NewRaDec(currentRA, currentDEC);
    return syncSideOfPier();
}

/**************************************************************************************
**
***************************************************************************************/

bool LX200StarGo::isIdle() {
    int motorsState, speedState, nrTrackingSpeed;
    queryMountMotionState(&motorsState, &speedState, &nrTrackingSpeed);

    return (speedState == 0);
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
    if (result) UpdateMotionStatus(motorsState, speedState, nrTrackingSpeed);

    return result;
}

void LX200StarGo::UpdateMotionStatus(int motorsState, int speedState, int nrTrackingSpeed) {

    // m = 0 both motors are OFF (no power)
    // m = 1 RA motor OFF DEC motor ON
    // m = 2 RA motor ON DEC motor OFF
    // m = 3 both motors are ON
    switch (motorsState) {
    case 0:
        if (TrackState == SCOPE_PARKING) {
            SetParked(true);
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
        if (TrackState != SCOPE_PARKED && TrackState != SCOPE_PARKING && TrackState != SCOPE_SLEWING) {
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
        if (TrackState != SCOPE_PARKED && TrackState != SCOPE_PARKING && TrackState != SCOPE_SLEWING) {
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
        if (TrackState != SCOPE_PARKED && TrackState != SCOPE_PARKING && TrackState != SCOPE_SLEWING) {
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
        if (TrackState != SCOPE_PARKED && TrackState != SCOPE_PARKING && TrackState != SCOPE_SLEWING) {
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
}


/**************************************************************************************
**
***************************************************************************************/

bool LX200StarGo::syncHomePosition()
{
    char input[12], cmd[12];
    int result = TTY_OK;
    if (!getLST_String(input)) {
        LOG_WARN("Synching home position failed.");
        SyncHomeSP.s = IPS_ALERT;
        return false;
    }

    sprintf(cmd, ":X31%s#", input);

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
* @author CanisUrsa
***************************************************************************************/

bool LX200StarGo::slewToHome(ISState* states, char* names[], int n) {
    IUUpdateSwitch(&MountGotoHomeSP, states, names, n);
    if (querySendMountGotoHome()) {
        MountGotoHomeSP.s = IPS_BUSY;
        TrackState = SCOPE_SLEWING;
    } else {
        MountGotoHomeSP.s = IPS_ALERT;
    }
    MountGotoHomeS[0].s = ISS_OFF;
    IDSetSwitch(&MountGotoHomeSP, nullptr);

    LOG_INFO("Slewing to home position...");
    return true;
}


bool LX200StarGo::setParkPosition(ISState* states, char* names[], int n) {
    IUUpdateSwitch(&MountSetParkSP, states, names, n);
    MountSetParkSP.s = querySendMountSetPark() ? IPS_OK : IPS_ALERT;
    MountSetParkS[0].s = ISS_OFF;
    IDSetSwitch(&MountSetParkSP, nullptr);
    return true;
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
* @author CanisUrsa
***************************************************************************************/
bool LX200StarGo::querySendMountGotoHome() {
    // Command  - :X361#
    // Response - pA#
    //            :Z1303#
    //            p0#
    //            :Z1003#
    //            p0#
    flush();
    if (!transmit(":X361#")) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to send mount goto home command.", getDeviceName());
        return false;
    }
    char response[AVALON_COMMAND_BUFFER_LENGTH] = {0};
    int bytesReceived = 0;
    if (!receive(response, &bytesReceived)) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Failed to receive mount goto home response.", getDeviceName());
        return false;
    }
    if (strcmp(response, "pA#") != 0) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: Invalid mount sync goto response '%s'.", getDeviceName(), response);
        return false;
    }
    return true;
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

/**
 * @brief Set parking state to "parked" and reflect the state
 *        in the UI.
 * @param isparked true iff the scope has been parked
 * @return
 */
void LX200StarGo::SetParked(bool isparked) {
    INDI::Telescope::SetParked(isparked);

    TrackState = isparked ? SCOPE_PARKED : SCOPE_TRACKING;
    ParkS[0].s = isparked ? ISS_ON : ISS_OFF;
    ParkS[1].s = isparked ? ISS_OFF : ISS_ON;
    ParkSP.s   = IPS_OK;
    IDSetSwitch(&ParkSP, nullptr);
    MountParkingStatusL[0].s = isparked ? IPS_OK : IPS_IDLE;
    MountParkingStatusL[1].s = isparked ? IPS_IDLE : IPS_OK;
    IDSetLight(&MountParkingStatusLP, nullptr);
}

bool LX200StarGo::UnPark() {
    // in: :X370#
    // out: "p0#"

    // set LST to avoid errors
    char input[12], cmd[12];
    if (!getLST_String(input))
    {
        LOG_WARN("Obtaining LST before unparking failed.");
        return false;
    }
    sprintf(cmd, ":X32%s#", input);

    char response[AVALON_COMMAND_BUFFER_LENGTH] = {0};
    bool result = sendQuery(cmd, response);

    if (!result || response[0] != '0')
    {
        LOG_WARN("Setting LST before unparking failed.");
        return false;
    }

    // and now execute unparking
    response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};
    if (sendQuery(":X370#", response) && strcmp(response, "p0#") == 0) {
        LOGF_INFO("%s: Scope Unparked.", getDeviceName());
        TrackState = SCOPE_TRACKING;
        SetParked(false);
        MountParkingStatusL[1].s = IPS_OK;
        MountParkingStatusL[0].s = IPS_IDLE;
        IDSetLight(&MountParkingStatusLP, nullptr);

        return true;
    } else {
        LOGF_ERROR("%s: Unpark failed.", getDeviceName());
        return false;
    }

}

/**
 * @brief Determine the LST with format HHMMSS
 * @return LST value for the current scope locateion
 */

bool LX200StarGo::getLST_String(char* input) {
    double siteLong;

    // step one: determine site longitude
    if (getSiteLongitude(&siteLong) != TTY_OK)
    {
        LOG_WARN("Failed to get site latitude from device.");
        return false;
    }

    // determine local sidereal time
    double lst = get_local_sidereal_time(siteLong);
    LOGF_DEBUG("Current local sidereal time = %.8f", lst);

    // translate into hh:mm:ss
    int h=0, m=0, s=0;
    getSexComponents(lst, &h, &m, &s);

    sprintf(input, "%02d%02d%02d", h, m, s);
    return true;

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

bool LX200StarGo::querySendMountSetPark() {
    // Command  - :X352#
    // Response - 0#
    flush();
    if (!transmit(":X352#")) {
        LOGF_ERROR("%s: Failed to send mount set park position command.", getDeviceName());
        return false;
    }
    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};
    int bytesReceived = 0;
    if (!receive(response, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive mount set park position response.", getDeviceName());
        return false;
    }
    if (response[0] != '0') {
        LOGF_ERROR("%s: Invalid mount set park position response '%s'.", getDeviceName(), response);
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

    LOGF_DEBUG("%s: Sending set site latitude request '%s'", getDeviceName(), command);

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
    return parseMotionState(response, motorsState, speedState, nrTrackingSpeed);
}


/**
 * @brief Parse the motion state response string :Z1mmssnn#
 * @param response string to be parsed
 * @param motorsState state of the motors
 * @param speedState tracking on / off
 * @param nrTrackingSpeed tracking speed
 * @return true iff parsing succeeded
 */
bool LX200StarGo::parseMotionState (char* response, int* motorsState, int* speedState, int* nrTrackingSpeed) {
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
 * @brief Check whether the mount is synched or parked.
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
    if (! transmit(enable ? ":X122#" : ":X120#")) {
        LOGF_ERROR("%s: Failed to send query for %s tracking.", getDeviceName(), enable ? "enable" : "disable");
        return false;
    }
    return true;
}

/**
 * @brief Check if the ST4 port is enabled
 * @param isEnabled - true iff the ST4 port is enabled
 * @return
 */
bool LX200StarGo::queryGetST4Status (bool *isEnabled) {
    // Command query ST4 status  - :TTGFh#
    //         response enabled  - vh1
    //                  disabled - vh0

    int bytesReceived = 0;
    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};

    flush();
    if (!transmit(":TTGFh#")) {
        LOGF_ERROR("%s: Failed to send query ST4 status request.", getDeviceName());
        return false;
    }
    if (!receive(response, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive query ST4 status response.", getDeviceName());
        return false;
    }
    int answer = 0;
    if (! sscanf(response, "vh%01d", &answer)) {
        LOGF_ERROR("%s: Unexpected ST4 status response '%s'.", getDeviceName(), response);
        return false;
    }

    *isEnabled = (answer == 1);
    return true;
}

/**
 * @brief Determine the guiding speeds for RA and DEC axis
 * @param raSpeed percentage for RA axis
 * @param decSpeed percenage for DEC axis
 * @return
 */
bool LX200StarGo::queryGetGuidingSpeeds (int *raSpeed, int *decSpeed) {
    // Command query guiding speeds  - :X22#
    //         response              - rrbdd#
    //         rr RA speed percentage, dd DEC speed percentage

    int bytesReceived = 0;
    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};

    flush();
    if (!transmit(":X22#")) {
        LOGF_ERROR("%s: Failed to send query guiding speeds request.", getDeviceName());
        return false;
    }
    if (!receive(response, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive query guiding speeds response.", getDeviceName());
        return false;
    }

    if (! sscanf(response, "%02db%2d", raSpeed, decSpeed)) {
        LOGF_ERROR("%s: Unexpected guiding speed response '%s'.", getDeviceName(), response);
        return false;
    }

    return true;
}


/**
 * @brief Set the guiding speeds for RA and DEC axis
 * @param raSpeed percentage for RA axis
 * @param decSpeed percenage for DEC axis
 * @return
 */
bool LX200StarGo::setGuidingSpeeds (int raSpeed, int decSpeed) {
    // in RA guiding speed  -  :X20rr#
    // in DEC guiding speed - :X21dd#

    char cmd[AVALON_COMMAND_BUFFER_LENGTH];

    sprintf(cmd, ":X20%2d#", raSpeed);

    flush();
    if (transmit(cmd)) {
        LOGF_INFO("Setting RA speed to %2d%%.", raSpeed);
    } else {
        LOGF_ERROR("Setting RA speed to %2d %% FAILED", raSpeed);
        return false;
    }

    sprintf(cmd, ":X21%2d#", decSpeed);

    if (transmit(cmd)) {
        LOGF_INFO("Setting DEC speed to %2d%%.", decSpeed);
    } else {
        LOGF_ERROR("Setting DEC speed to %2d%% FAILED", decSpeed);
        return false;
    }

    return true;
}

/**
 * @brief Enable or disable the ST4 guiding port
 * @param enabled flag whether enable or disable
 * @return
 */

bool LX200StarGo::setST4Enabled(bool enabled) {

    const char *cmd = enabled ? ":TTSFh#" : ":TTRFh#";

    flush();
    if (setStandardProcedureAvalon(cmd, 0)) {
        LOG_INFO(enabled ? "ST4 port enabled." : "ST4 port disabled.");
        return true;
    } else {
        LOG_ERROR("Setting ST4 port FAILED");
        return false;
    }

}

/**
 * @brief Determine whether the meridian flip is enabled
 * @param isEnabled - true iff flip is enabled
 * @return true iff check succeeded
 */
bool LX200StarGo::queryGetMeridianFlipEnabledStatus (bool *isEnabled) {
    // Command query meridian flip enabled status  - :TTGFs#
    //                           response enabled  - vs0
    //                                    disabled - vs1

    int bytesReceived = 0;
    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};

    flush();
    if (!transmit(":TTGFs#")) {
        LOGF_ERROR("%s: Failed to send query meridian flip enabled status request.", getDeviceName());
        return false;
    }
    if (!receive(response, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive query meridian flip enabled status response.", getDeviceName());
        return false;
    }
    int answer = 0;
    if (! sscanf(response, "vs%01d", &answer)) {
        LOGF_ERROR("%s: Unexpected meridian flip enabled status response '%s'.", getDeviceName(), response);
        return false;
    }

    *isEnabled = (answer == 0);
    return true;
}

/**
 * @brief Enabling and disabling meridian flip
 * @param enabled - setting enabled iff true
 * @return
 */
bool LX200StarGo::setMeridianFlipEnabled(bool enabled) {

    const char *cmd = enabled ? ":TTRFs#" : ":TTSFs#";

    flush();
    bool success = setStandardProcedureAvalon(cmd, 0);

    if (success){
        if (enabled) LOG_INFO(enabled ? "Meridian flip enabled." : "Meridian flip disabled.");
        else LOG_WARN("Meridian flip disabled. BE CAREFUL, THIS MAY CAUSE DAMAGE TO YOUR MOUNT!");

        MeridianFlipEnabledS[0].s = enabled ? ISS_ON  : ISS_OFF;
        MeridianFlipEnabledS[1].s = enabled ? ISS_OFF : ISS_ON;
        MeridianFlipEnabledSP.s = IPS_OK;
    } else {
        LOG_ERROR("Setting Meridian flip FAILED");
        MeridianFlipEnabledSP.s = IPS_ALERT;
    }

    IDSetSwitch(&MeridianFlipEnabledSP, nullptr);
    return success;
}


/**
 * @brief Determine whether the forcing meridian flip is enabled
 * @param isEnabled - true iff forcing flip is enabled
 * @return true iff check succeeded
 */
bool LX200StarGo::queryGetMeridianFlipForcedStatus (bool *isEnabled) {
    // Command query meridian flip enabled status  - :TTGFd#
    //                           response enabled  - vd1
    //                                    disabled - vd0

    int bytesReceived = 0;
    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};

    flush();
    if (!transmit(":TTGFd#")) {
        LOGF_ERROR("%s: Failed to send query meridian flip forced status request.", getDeviceName());
        return false;
    }
    if (!receive(response, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive query meridian flip forced status response.", getDeviceName());
        return false;
    }
    int answer = 0;
    if (! sscanf(response, "vd%01d", &answer)) {
        LOGF_ERROR("%s: Unexpected meridian flip forced status response '%s'.", getDeviceName(), response);
        return false;
    }

    *isEnabled = (answer == 1);
    return true;
}

/**
 * @brief Enabling and disabling forced meridian flip
 * @param enabled - setting enabled iff true
 * @return
 */
bool LX200StarGo::setMeridianFlipForced(bool enabled) {

    const char *cmd = enabled ? ":TTSFd#" : ":TTRFd#";

    flush();
    bool success = setStandardProcedureAvalon(cmd, 0);

    if (success){
        if (enabled) LOG_WARN("Meridian flip forced. BE CAREFUL, THIS MAY CAUSE DAMAGE TO YOUR MOUNT!");
        else LOG_INFO("Meridian flip automatic.");

        MeridianFlipForcedS[0].s = enabled ? ISS_OFF : ISS_ON;
        MeridianFlipForcedS[1].s = enabled ? ISS_ON : ISS_OFF;
        MeridianFlipForcedSP.s = IPS_OK;
    } else {
        LOG_ERROR("Forcing Meridian flip FAILED");
        MeridianFlipForcedSP.s = IPS_ALERT;
    }

    IDSetSwitch(&MeridianFlipForcedSP, nullptr);
    return success;
}

/**
 * @brief Retrieve pier side of the mount and sync it back to the client
 * @return true iff synching succeeds
 */
bool LX200StarGo::syncSideOfPier() {
    // Command query side of pier - :X39#
    //         side unknown       - PX#
    //         east pointing west - PE#
    //         west pointing east - PW#

    int bytesReceived = 0;
    char response[AVALON_RESPONSE_BUFFER_LENGTH] = {0};


    if (!transmit(":X39#")) {
        LOGF_ERROR("%s: Failed to send query pier side.", getDeviceName());
        return false;
    }
    if (!receive(response, &bytesReceived)) {
        LOGF_ERROR("%s: Failed to receive query pier side.", getDeviceName());
        return false;
    }

    char answer;

    if (! sscanf(response, "P%c#", &answer)) {
        LOGF_ERROR("%s: Unexpected query pier side response '%s'.", getDeviceName(), response);
        return false;
    }

    switch (answer) {
    case 'X':
        LOGF_DEBUG("%s: Detected pier side unknown.", getDeviceName());
        setPierSide(INDI::Telescope::PIER_UNKNOWN);
        break;
    case 'E':
        // seems to be vice versa
        LOGF_DEBUG("%s: Detected pier side west.", getDeviceName());
        setPierSide(INDI::Telescope::PIER_WEST);
        break;
    case 'W':
        LOGF_DEBUG("%s: Detected pier side east.", getDeviceName());
        setPierSide(INDI::Telescope::PIER_EAST);
        break;
    default:
        break;
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
        LOGF_WARN("Failed to receive full response: %s. (Return code: %d)", errorString, returnCode);
        return false;
    }
    LOGF_DEBUG("LX200 answer: '%s'", buffer);
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
    LOGF_DEBUG("LX200 CMD: '%s'", buffer);
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

