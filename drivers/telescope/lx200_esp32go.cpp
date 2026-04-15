/*
    LX200 esp32go
    based on LX200 generic with some bits from LX200_onStep driver 

    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "lx200_esp32go.h"
#include "lx200driver.h"

#include <cstring>
#include <mutex>
#include <unistd.h>

#define FIRMWARE_TAB "Firmware data"
extern std::mutex lx200CommsLock;

LX200_esp32go::LX200_esp32go() : LX200Generic()
{
    setVersion(1, 0);
    trackingMode   = LX200_TRACK_SIDEREAL;
    
    setLX200Capability(LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_PIER_SIDE |
                           TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK ,
                           4);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
}

const char *LX200_esp32go::getDefaultName()
{
    return "LX200 esp32go";
}

bool LX200_esp32go::initProperties()
{
    LX200Generic::initProperties();

    GuideRateNP[0].fill("GUIDE_RATE_WE", "W/E Rate", "%g",  0, 2, 0.05, 0.5);
    GuideRateNP[1].fill("GUIDE_RATE_NS", "N/S Rate", "%g", 0, 2, 0.05, 0.5);
    GuideRateNP.fill(getDeviceName(), "GUIDE_RATE", "Guide Rate", MOTION_TAB, IP_RO, 0, IPS_IDLE);

    // ============== FIRMWARE_TAB
    VersionTP[0].fill("Date", "", "");
    VersionTP[1].fill("Time", "", "");
    VersionTP[2].fill("Number", "", "");
    VersionTP[3].fill("Name", "", "");
    VersionTP.fill(getDeviceName(), "Firmware Info", "", FIRMWARE_TAB, IP_RO, 0, IPS_IDLE);



    if(activeFocus)
    {
        //FocuserInterface
        FI::initProperties(FOCUS_TAB);
        //Initial, these will be updated later.
        FocusRelPosNP[0].setMin(0.);
        FocusRelPosNP[0].setMax(5000.);
        FocusRelPosNP[0].setValue(0);
        FocusRelPosNP[0].setStep(10);
        FocusAbsPosNP[0].setMin(0.);
        FocusAbsPosNP[0].setMax(100000.);
        FocusAbsPosNP[0].setValue(0);
        FocusAbsPosNP[0].setStep(10);

        setDriverInterface(getDriverInterface() | FOCUSER_INTERFACE);

        FI::updateProperties();
    }
    return true;
}

void LX200_esp32go::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    // process parent first
    LX200Generic::ISGetProperties(dev);

}

bool LX200_esp32go::updateProperties()
{
    LX200Generic::updateProperties();

    if(activeFocus)
        FI::updateProperties();

    if (isConnected())
    {
        defineProperty(GuideRateNP);
        defineProperty(VersionTP);
/*
        if(activeFocus)
            defineProperty(FocusTemperatureNP);
*/
        }
    else
    {
        deleteProperty(GuideRateNP);
        deleteProperty(VersionTP);
/*
        if(activeFocus)
            deleteProperty(FocusTemperatureNP);
*/
    }

    return true;
}

bool LX200_esp32go::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200_esp32go::UnPark()
{
    int ret = 0;

    ret = initTelescope(PortFD);

    INDI_UNUSED(ret);
    TrackState = SCOPE_IDLE;
    SetParked(false);
    return true;
}

bool LX200_esp32go::ReadScopeStatus()
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
        if (!picgotoMode && isSlewComplete())
        {
            // Set slew mode to "Centering"
            SlewRateSP.reset();
            SlewRateSP[SLEW_CENTERING].setState(ISS_ON);
            SlewRateSP.apply();

            TrackState = SCOPE_TRACKING;
            LOG_INFO("Slew is complete. Tracking...");
        }
        if(picgotoMode)
        {
            //TrackState = SCOPE_TRACKING;
            //LOG_INFO("Blind slew. Will track when complete...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (!picgotoMode || isSlewComplete())
        {
            SetParked(true);
        }
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.setState(IPS_ALERT);
        LOG_ERROR("Error reading RA/DEC.");
        EqNP.apply();
        return false;
    }
    NewRaDec(currentRA, currentDEC);
    
    // track mode
    if(trackmodeUpdate || TrackModeSP[0].getState() != ISS_ON)
    {
        trackmodeUpdate = false;
        trackingMode   = LX200_TRACK_SIDEREAL;
        TrackModeSP.reset();
        TrackModeSP[0].setState(ISS_ON);
        //TrackModeSP.setState(IPS_ALERT);
        TrackModeSP.apply();
        LOG_INFO("SetTrackMode not supported Yet. Back to Sidereal");
    }

    // guide rate
    if(guideUpdate)
    {
        double pulseguiderate = 0.0;
        if(getCommandSexa(PortFD, &pulseguiderate, ":cg#") < 0)
        {
            LOG_ERROR("Error reading GuideRate for RA.");
            return false;
        }
        GuideRateNP[0].setValue(pulseguiderate);
        if(getCommandSexa(PortFD, &pulseguiderate, ":cj#") < 0)
        {
            LOG_ERROR("Error reading GuideRate for DEC.");
            return false;
        }
        GuideRateNP[1].setValue(pulseguiderate);
        GuideRateNP.apply();
    }

    // pier side
    if(!picgotoMode)
    {
        char resp[64];
        if(getCommandString(PortFD, resp, ":pS#") < 0)
        {
            LOG_ERROR("Error reading Pier side.");
            return false;
        }
        if(!strcmp(resp,"EAST"))
            setPierSide(PIER_EAST);
        else if (!strcmp(resp,"WEST"))
            setPierSide(PIER_WEST);
        else
            setPierSide(PIER_UNKNOWN);
    }
    else
    {
        setPierSide(PIER_UNKNOWN);
    }
 
    // --- Focuser
    if(activeFocus)
    {
        if (UpdateFocuser() != 0)  // Update Focuser Position
        {
            LOG_WARN("Communication error on Focuser Update, this update aborted, will try again...");
            return true; //COMMUNICATION ERROR, BUT DON'T PUT TELESCOPE IN ERROR STATE
        }
    }

    return true;
}


// copied from LX200_OnStep driver
int LX200_esp32go::getCommandSingleCharErrorOrLongResponse(int fd, char *data, const char *cmd)
{
    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    flushIO(fd);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);
    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section_expanded(fd, data, RBB_MAX_LEN, '#', OSTimeoutSeconds, OSTimeoutMicroSeconds, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    term = strchr(data, '#');
    if (term)
        *term = '\0';
    if (nbytes_read < RBB_MAX_LEN) //If within buffer, terminate string with \0 (in case it didn't find the #)
    {
        data[nbytes_read] = '\0'; //Indexed at 0, so this is the byte passed it
    }
    else
    {
        LOG_DEBUG("got RBB_MAX_LEN bytes back, last byte set to null and possible overflow");
        data[RBB_MAX_LEN - 1] = '\0';
    }

    DEBUGF(DBG_SCOPE, "RES <%s>", data);

    if (error_type != TTY_OK)
    {
        LOGF_DEBUG("Error %d", error_type);
        return error_type;
    }
    return nbytes_read;

}

// copied from LX200_OnStep driver
int LX200_esp32go::flushIO(int fd)
{
    tcflush(fd, TCIOFLUSH);
    int error_type = 0;
    int nbytes_read;
    std::unique_lock<std::mutex> guard(lx200CommsLock);
    tcflush(fd, TCIOFLUSH);
    do
    {
        char discard_data[RBB_MAX_LEN] = {0};
        error_type = tty_nread_section_expanded(fd, discard_data, sizeof(discard_data), '#', 0, 1000, &nbytes_read);
        if (error_type >= 0)
        {
            LOGF_DEBUG("flushIO: Information in buffer: Bytes: %u, string: %s", nbytes_read, discard_data);
        }
        //LOGF_DEBUG("flushIO: error_type = %i", error_type);
    }
    while (error_type > 0);
    return 0;
}

// copied from LX200_OnStep driver
int LX200_esp32go::getCommandSingleCharResponse(int fd, char *data, const char *cmd)
{
    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    flushIO(fd);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read_expanded(fd, data, 1, OSTimeoutSeconds, OSTimeoutMicroSeconds, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    if (error_type != TTY_OK)
        return error_type;

    term = strchr(data, '#');
    if (term)
        *term = '\0';
    if (nbytes_read < RBB_MAX_LEN) //given this function that should always be true, as should nbytes_read always be 1
    {
        data[nbytes_read] = '\0';
    }
    else
    {
        LOG_DEBUG("got RB_MAX_LEN bytes back (which should never happen), last byte set to null and possible overflow");
        data[RBB_MAX_LEN - 1] = '\0';
    }

    DEBUGF(DBG_SCOPE, "RES <%s>", data);

    return nbytes_read;
}


void LX200_esp32go::splitString(const char *str, char array[][100], int *count) {
    const char *start = str;
    const char *end;
    *count = 0;

    while ((end = strchr(start, '\n')) != NULL) {
        size_t length = end - start;
        strncpy(array[*count], start, length);
        array[*count][length] = '\0'; // Null-terminate the string
        (*count)++;
        start = end + 1; // Move past the newline
    }
    // Handle the last line if it doesn't end with a newline
    if (*start) {
        strcpy(array[*count], start);
        (*count)++;
    }
}

bool LX200_esp32go::SetTrackEnabled(bool enabled) //track On/Off events handled by inditelescope
{
    if (enabled)
    {
        sendCommandBlind(":Qw#");
        TrackState = SCOPE_TRACKING;
        LOG_INFO("Tracking enabled");
    }
    else
    {
        sendCommandBlind(":Mh#");
        TrackState = SCOPE_IDLE;
        LOG_INFO("Tracking disabled");
    }
    return true;
}

bool LX200_esp32go::SetTrackMode(uint8_t mode)
{
    if (isSimulation())
        return true;

    // FW not ready yet
/*
    LOG_INFO("SetTrackMode");
    switch(mode)
    {
        case TRACK_SIDEREAL:
            return sendCommandBlind(":TQ#");
            break;
        case TRACK_SOLAR:
            return sendCommandBlind(":TS#");
            break;
        case TRACK_LUNAR:
            return sendCommandBlind(":TL#");
            break;
        case TRACK_KING:
            return sendCommandBlind(":TK#");
            break;
    }
*/
    return true;
}

void LX200_esp32go::getBasicData()
{
    // process parent
    LX200Generic::getBasicData();

    if (!isSimulation())
    {

        char configData[256];
        int error_or_fail = getCommandSingleCharErrorOrLongResponse(PortFD, configData, ":cA#");
        if (error_or_fail > 1)
        {
            LOG_INFO("ESP32go config read");
            //LOG_INFO(configData);
            char lines[26][100];
            int lineCount;
            splitString(configData, lines, &lineCount);
            // az guide rate index 2
            GuideRateNP[0].setValue(atof(lines[2]));
            // az guide rate index 6
            GuideRateNP[1].setValue(atof(lines[6]));
            GuideRateNP.apply();
            // mount mode index 18
            MountTypeSP.reset();
            MountTypeSP[MOUNT_EQ_GEM].setState(ISS_ON);
            if(*lines[18]=='0')
            {
                LOG_INFO("Mount EQ"); // default
            }
            else if(*lines[18]=='1')
            {
                LOG_INFO("Mount AltAz");
                MountTypeSP.reset();
                MountTypeSP[MOUNT_ALTAZ].setState(ISS_ON);
            }
            else if(*lines[18]=='2')
            {
                LOG_INFO("Mount FORK");
                MountTypeSP.reset();
                MountTypeSP[MOUNT_EQ_FORK].setState(ISS_ON);
            }
            else
            {
                LOG_INFO("Mount ?????");
            }
            MountTypeSP.apply();
        }
        else
        {
            LOG_INFO("Cannot check configuration. Activating older/PicGoto mode");
            picgotoMode = true;
            activeFocus = false;
            char MountAlign[2];
            *MountAlign = ACK(PortFD); // get mount type from ACK response
            MountTypeSP.reset();
            MountTypeSP[MOUNT_EQ_GEM].setState(ISS_ON); // Default GEM
            if(MountAlign[0] == 'A')
            {
                LOG_INFO("Mount AltAz");
                MountTypeSP.reset();
                MountTypeSP[MOUNT_ALTAZ].setState(ISS_ON);
            }
            MountTypeSP.apply();
        }
        double version = 0.0;
        if(picgotoMode || getCommandSexa(PortFD, &version, ":GVN#") < 0)
        {
            if(!picgotoMode)
                LOG_INFO("Cannot check FW version. Activating older/PicGoto mode");

            picgotoMode = true;
            activeFocus = false;
        }
        else
        {
            if(version > 5)
            {
                guideUpdate = true;
                //LOG_INFO("Updated version");
            }
            else
            {
                //LOG_INFO("preDrv version");
                LOG_INFO("Pre v06.3 FW. Dectivating guidePulse periodic check");
            }
            char buffer[128] = {0};
            getVersionDate(PortFD, buffer);
            VersionTP[0].setText(buffer);
            getVersionTime(PortFD, buffer);
            VersionTP[1].setText(buffer);
            getVersionNumber(PortFD, buffer);
            VersionTP[2].setText(buffer);
            getProductName(PortFD, buffer);
            VersionTP[3].setText(buffer);
            VersionTP.apply();
        }
        // initial tracking state
        char isTracking[4];
        if(getCommandSingleCharResponse(PortFD, isTracking, ":Gk#") < 0)
        {
            LOG_ERROR("Error reading tracking status.");
        }
        else
        {
            if (strcmp(isTracking, "1")==0)
            {
                TrackStateSP[TRACK_ON].setState(ISS_ON);
                TrackStateSP[TRACK_OFF].setState(ISS_OFF);
                TrackState = SCOPE_TRACKING;
            }
            else
            {
                TrackStateSP[TRACK_ON].setState(ISS_OFF);
                TrackStateSP[TRACK_OFF].setState(ISS_ON);
                TrackState = SCOPE_IDLE;
            }
            TrackStateSP.apply();
            LOG_INFO("TrackState read");
        }
    }
    if(activeFocus)
        LOG_INFO("Focuser is active");

}


bool LX200_esp32go::sendCommandBlind(const char *cmd)
{
    int error_type;
    int nbytes_write = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    flushIO(PortFD);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);
    tcflush(PortFD, TCIFLUSH);


    if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK)
    {
        LOGF_ERROR("CHECK CONNECTION: Error sending command %s", cmd);
        return 0; //Fail if we can't write
        //return error_type;
    }

    return 1;
}
// -----------------
// FOCUSER INTERFACE

IPState LX200_esp32go::MoveAbsFocuser (uint32_t targetTicks)
{
    if(!activeFocus)
        return IPS_OK;
    //LOG_INFO("FOCUSER MoveAbsFocuser");
    if (FocusAbsPosNP[0].getMax() >= int(targetTicks) && FocusAbsPosNP[0].getMin() <= int(targetTicks))
    {
        char read_buffer[32];
        snprintf(read_buffer, sizeof(read_buffer), ":FA+%05d#", int(targetTicks));
        //LOG_INFO(read_buffer);
        sendCommandBlind(read_buffer);
        return IPS_BUSY; // Normal case, should be set to normal by update.
    }
    else
    {
        LOG_INFO("Unable to move focuser, out of range");
        return IPS_ALERT;
    }
}

IPState LX200_esp32go::MoveRelFocuser (FocusDirection dir, uint32_t ticks)
{
    if(!activeFocus)
        return IPS_OK;
    //LOG_INFO("FOCUSER MoveRelFocuser");
    char read_buffer[32];
    char s = (dir == FOCUS_INWARD ? '-':'+');
    snprintf(read_buffer, sizeof(read_buffer), ":FP%c%05d#", s, ticks);
    //LOG_INFO(read_buffer);
    sendCommandBlind(read_buffer);
    return IPS_BUSY; // Normal case, should be set to normal by update.
}

bool LX200_esp32go::AbortFocuser ()
{
    LOG_INFO("FOCUSER AbortFocuser");
    //  :FQ#   Stop the focuser
    //         Returns: Nothing
    char cmd[CMDB_MAX_LEN] = {0};
    strncpy(cmd, ":FQ#", sizeof(cmd));
    return sendCommandBlind(cmd);
}

int LX200_esp32go::UpdateFocuser()
{
    if(!activeFocus)
        return false;

    bool busy = false;
    //char value[RBB_MAX_LEN] = {0};
    int value_int;
    if(getCommandInt(PortFD, &value_int, ":FB#") < 0)
    {
        LOG_ERROR("Error reading focuser status.");
        return false;
    }
    if(value_int == 1)
    {
        busy = true;
        FocusAbsPosNP.setState(IPS_BUSY);    
        FocusRelPosNP.setState(IPS_BUSY);
    }
    if(getCommandInt(PortFD, &value_int, ":Fp#") < 0)
    {
        FocusAbsPosNP.apply();
        FocusRelPosNP.apply();
        LOG_ERROR("Error reading focuser position.");
        return false;
    }
    //LOG_INFO("FOCUSER STATUS UPDATED");

    FocusAbsPosNP[0].setValue(value_int);
    //double current = FocusAbsPosNP[0].getValue();
    if(!busy)
    {
        FocusAbsPosNP.setState(IPS_OK);
        FocusRelPosNP.setState(IPS_OK);
    }
    LOGF_DEBUG("Current focuser: %d, %f", value_int, FocusAbsPosNP[0].getValue());

    FocusAbsPosNP.apply();
    FocusRelPosNP.apply();
    FI::updateProperties();

    return 0;
}

