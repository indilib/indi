/*
 * Standard LX200 implementation.

   Copyright (C) 2003 - 2018 Jasem Mutlaq (mutlaqja@ikarustech.com)

   This library is free software;
   you can redistribute it and / or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation;
   either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY;
   without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library;
   if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA
 */

#include "lx200telescope.h"

#include "indicom.h"
#include "lx200driver.h"

#include <libnova/sidereal_time.h>

#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <time.h>

/* Simulation Parameters */
#define LX200_GENERIC_SLEWRATE 5        /* slew rate, degrees/s */
#define SIDRATE  0.004178 /* sidereal rate, degrees/s */

LX200Telescope::LX200Telescope() : FI(this)
{
}

void LX200Telescope::debugTriggered(bool enable)
{
    INDI_UNUSED(enable);
    setLX200Debug(getDeviceName(), DBG_SCOPE);
}

const char *LX200Telescope::getDriverName()
{
    return getDefaultName();
}

const char *LX200Telescope::getDefaultName()
{
    return static_cast<const char *>("Standard LX200");
}


bool LX200Telescope::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    IUFillSwitch(&AlignmentS[0], "Polar", "", ISS_ON);
    IUFillSwitch(&AlignmentS[1], "AltAz", "", ISS_OFF);
    IUFillSwitch(&AlignmentS[2], "Land", "", ISS_OFF);
    IUFillSwitchVector(&AlignmentSP, AlignmentS, 3, getDeviceName(), "Alignment", "", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

#if 0
    IUFillSwitch(&SlewRateS[SLEW_GUIDE], "SLEW_GUIDE", "Guide", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_CENTERING], "SLEW_CENTERING", "Centering", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_FIND], "SLEW_FIND", "Find", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_MAX], "SLEW_MAX", "Max", ISS_ON);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 4, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
#endif

#if 0
    IUFillSwitch(&TrackModeS[0], "TRACK_SIDEREAL", "Sidereal", ISS_ON);
    IUFillSwitch(&TrackModeS[1], "TRACK_SOLAR", "Solar", ISS_OFF);
    IUFillSwitch(&TrackModeS[2], "TRACK_LUNAR", "Lunar", ISS_OFF);
    IUFillSwitch(&TrackModeS[3], "TRACK_CUSTOM", "Custom", ISS_OFF);
    IUFillSwitchVector(&TrackModeSP, TrackModeS, 4, getDeviceName(), "TELESCOPE_TRACK_MODE", "Track Mode", MOTION_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
#endif

    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    if (genericCapability & LX200_HAS_PRECISE_TRACKING_FREQ)
    {
        IUFillNumber(&TrackFreqN[0], "trackFreq", "Freq", "%g", 55, 65, 0.00001, 60.16427);
    }
    else
    {
        IUFillNumber(&TrackFreqN[0], "trackFreq", "Freq", "%g", 56.4, 60.1, 0.1, 60.1);
    }
    IUFillNumberVector(&TrackingFreqNP, TrackFreqN, 1, getDeviceName(), "Tracking Frequency", "", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    IUFillSwitch(&UsePulseCmdS[0], "Off", "", ISS_OFF);
    IUFillSwitch(&UsePulseCmdS[1], "On", "", ISS_ON);
    IUFillSwitchVector(&UsePulseCmdSP, UsePulseCmdS, 2, getDeviceName(), "Use Pulse Cmd", "", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&SiteS[0], "Site 1", "", ISS_ON);
    IUFillSwitch(&SiteS[1], "Site 2", "", ISS_OFF);
    IUFillSwitch(&SiteS[2], "Site 3", "", ISS_OFF);
    IUFillSwitch(&SiteS[3], "Site 4", "", ISS_OFF);
    IUFillSwitchVector(&SiteSP, SiteS, 4, getDeviceName(), "Sites", "", SITE_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&SiteNameT[0], "Name", "", "");
    IUFillTextVector(&SiteNameTP, SiteNameT, 1, getDeviceName(), "Site Name", "", SITE_TAB, IP_RW, 0, IPS_IDLE);

    if (genericCapability & LX200_HAS_FOCUS)
    {
        FI::SetCapability(FOCUSER_CAN_ABORT | FOCUSER_CAN_REVERSE | FOCUSER_HAS_VARIABLE_SPEED);
        FI::initProperties(FOCUS_TAB);

        //        IUFillSwitch(&FocusModeS[0], "FOCUS_HALT", "Halt", ISS_ON);
        //        IUFillSwitch(&FocusModeS[1], "FOCUS_SLOW", "Slow", ISS_OFF);
        //        IUFillSwitch(&FocusModeS[2], "FOCUS_FAST", "Fast", ISS_OFF);
        //        IUFillSwitchVector(&FocusModeSP, FocusModeS, 3, getDeviceName(), "FOCUS_MODE", "Mode", FOCUS_TAB, IP_RW,
        //                           ISR_1OFMANY, 0, IPS_IDLE);

        // Classical speeds slow or fast
        FocusSpeedN[0].min = 1;
        FocusSpeedN[0].max = 2;
        FocusSpeedN[0].value = 1;
    }

    TrackState = SCOPE_IDLE;

    initGuiderProperties(getDeviceName(), GUIDE_TAB);

    /* Add debug/simulation/config controls so we may debug driver if necessary */
    addAuxControls();

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    double longitude = 0, latitude = 90;
    // Get value from config file if it exists.
    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude);
    currentRA  = get_local_sidereal_time(longitude);
    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude);
    currentDEC = latitude > 0 ? 90 : -90;

    return true;
}

void LX200Telescope::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    INDI::Telescope::ISGetProperties(dev);

    /*
    if (isConnected())
    {
        if (genericCapability & LX200_HAS_ALIGNMENT_TYPE)
            defineSwitch(&AlignmentSP);

        if (genericCapability & LX200_HAS_TRACKING_FREQ)
            defineNumber(&TrackingFreqNP);

        if (genericCapability & LX200_HAS_PULSE_GUIDING)
            defineSwitch(&UsePulseCmdSP);

        if (genericCapability & LX200_HAS_SITES)
        {
            defineSwitch(&SiteSP);
            defineText(&SiteNameTP);
        }

        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);

        if (genericCapability & LX200_HAS_FOCUS)
        {
            defineSwitch(&FocusMotionSP);
            defineNumber(&FocusTimerNP);
            defineSwitch(&FocusModeSP);
        }
    }
    */
}

bool LX200Telescope::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        if (genericCapability & LX200_HAS_ALIGNMENT_TYPE)
            defineSwitch(&AlignmentSP);

        if (genericCapability & LX200_HAS_TRACKING_FREQ)
            defineNumber(&TrackingFreqNP);

        if (genericCapability & LX200_HAS_PULSE_GUIDING)
            defineSwitch(&UsePulseCmdSP);

        if (genericCapability & LX200_HAS_SITES)
        {
            defineSwitch(&SiteSP);
            defineText(&SiteNameTP);
        }

        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);

        if (genericCapability & LX200_HAS_FOCUS)
        {
            FI::updateProperties();
            //defineSwitch(&FocusModeSP);
        }

        getBasicData();
    }
    else
    {
        if (genericCapability & LX200_HAS_ALIGNMENT_TYPE)
            deleteProperty(AlignmentSP.name);

        if (genericCapability & LX200_HAS_TRACKING_FREQ)
            deleteProperty(TrackingFreqNP.name);

        if (genericCapability & LX200_HAS_PULSE_GUIDING)
            deleteProperty(UsePulseCmdSP.name);

        if (genericCapability & LX200_HAS_SITES)
        {
            deleteProperty(SiteSP.name);
            deleteProperty(SiteNameTP.name);
        }

        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);

        if (genericCapability & LX200_HAS_FOCUS)
        {
            FI::updateProperties();
            //deleteProperty(FocusModeSP.name);
        }
    }

    return true;
}

bool LX200Telescope::checkConnection()
{
    if (isSimulation())
        return true;

    return (check_lx200_connection(PortFD) == 0);
}

bool LX200Telescope::Handshake()
{
    return checkConnection();
}

bool LX200Telescope::isSlewComplete()
{
    return (::isSlewComplete(PortFD) == 1);
}

bool LX200Telescope::ReadScopeStatus()
{
    if (!isConnected())
        return false;

    if (isSimulation())
    {
        mountSim();
        return true;
    }

    //if (check_lx200_connection(PortFD))
    //return false;

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if LX200 is done slewing
        if (isSlewComplete())
        {
            // Set slew mode to "Centering"
            IUResetSwitch(&SlewRateSP);
            SlewRateS[SLEW_CENTERING].s = ISS_ON;
            IDSetSwitch(&SlewRateSP, nullptr);

            TrackState = SCOPE_TRACKING;
            LOG_INFO("Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (isSlewComplete())
        {
            SetParked(true);
        }
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading RA/DEC.");
        return false;
    }

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool LX200Telescope::Goto(double ra, double dec)
{
    const struct timespec timeout = {0, 100000000L};

    targetRA  = ra;
    targetDEC = dec;
    char RAStr[64] = {0}, DecStr[64] = {0};
    int fracbase = 0;

    switch (getLX200Format())
    {
        case LX200_LONGER_FORMAT:
            fracbase = 360000;
            break;
        case LX200_LONG_FORMAT:
        case LX200_SHORT_FORMAT:
        default:
            fracbase = 3600;
            break;
    }

    fs_sexa(RAStr, targetRA, 2, fracbase);
    fs_sexa(DecStr, targetDEC, 2, fracbase);

    // If moving, let's stop it first.
    if (EqNP.s == IPS_BUSY)
    {
        if (!isSimulation() && abortSlew(PortFD) < 0)
        {
            AbortSP.s = IPS_ALERT;
            IDSetSwitch(&AbortSP, "Abort slew failed.");
            return false;
        }

        AbortSP.s = IPS_OK;
        EqNP.s    = IPS_IDLE;
        IDSetSwitch(&AbortSP, "Slew aborted.");
        IDSetNumber(&EqNP, nullptr);

        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
        {
            MovementNSSP.s = MovementWESP.s = IPS_IDLE;
            EqNP.s                          = IPS_IDLE;
            IUResetSwitch(&MovementNSSP);
            IUResetSwitch(&MovementWESP);
            IDSetSwitch(&MovementNSSP, nullptr);
            IDSetSwitch(&MovementWESP, nullptr);
        }

        // sleep for 100 mseconds
        nanosleep(&timeout, nullptr);
    }

    if (!isSimulation())
    {
        if (setObjectRA(PortFD, targetRA) < 0 || (setObjectDEC(PortFD, targetDEC)) < 0)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error setting RA/DEC.");
            return false;
        }

        int err = 0;

        /* Slew reads the '0', that is not the end of the slew */
        if ((err = Slew(PortFD)))
        {
            LOGF_ERROR("Error Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
            slewError(err);
            return false;
        }
    }

    TrackState = SCOPE_SLEWING;
    EqNP.s     = IPS_BUSY;

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);

    return true;
}

bool LX200Telescope::Sync(double ra, double dec)
{
    char syncString[256] = {0};

    if (!isSimulation() && (setObjectRA(PortFD, ra) < 0 || (setObjectDEC(PortFD, dec)) < 0))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error setting RA/DEC. Unable to Sync.");
        return false;
    }

    if (!isSimulation() && ::Sync(PortFD, syncString) < 0)
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Synchronization failed.");
        return false;
    }

    currentRA  = ra;
    currentDEC = dec;

    LOG_INFO("Synchronization successful.");

    EqNP.s     = IPS_OK;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool LX200Telescope::Park()
{
    const struct timespec timeout = {0, 100000000L};
    if (!isSimulation())
    {
        // If scope is moving, let's stop it first.
        if (EqNP.s == IPS_BUSY)
        {
            if (!isSimulation() && abortSlew(PortFD) < 0)
            {
                AbortSP.s = IPS_ALERT;
                IDSetSwitch(&AbortSP, "Abort slew failed.");
                return false;
            }

            AbortSP.s = IPS_OK;
            EqNP.s    = IPS_IDLE;
            IDSetSwitch(&AbortSP, "Slew aborted.");
            IDSetNumber(&EqNP, nullptr);

            if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
            {
                MovementNSSP.s = MovementWESP.s = IPS_IDLE;
                EqNP.s                          = IPS_IDLE;
                IUResetSwitch(&MovementNSSP);
                IUResetSwitch(&MovementWESP);

                IDSetSwitch(&MovementNSSP, nullptr);
                IDSetSwitch(&MovementWESP, nullptr);
            }

            // sleep for 100 msec
            nanosleep(&timeout, nullptr);
        }

        if (!isSimulation() && slewToPark(PortFD) < 0)
        {
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, "Parking Failed.");
            return false;
        }
    }

    ParkSP.s   = IPS_BUSY;
    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking telescope in progress...");
    return true;
}

bool LX200Telescope::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    int current_move = (dir == DIRECTION_NORTH) ? LX200_NORTH : LX200_SOUTH;

    switch (command)
    {
        case MOTION_START:
            if (!isSimulation() && MoveTo(PortFD, current_move) < 0)
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
                LOGF_DEBUG("Moving toward %s.",
                           (current_move == LX200_NORTH) ? "North" : "South");
            break;

        case MOTION_STOP:
            if (!isSimulation() && HaltMovement(PortFD, current_move) < 0)
            {
                LOG_ERROR("Error stopping N/S motion.");
                return false;
            }
            else
                LOGF_DEBUG("Movement toward %s halted.",
                           (current_move == LX200_NORTH) ? "North" : "South");
            break;
    }

    return true;
}

bool LX200Telescope::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    int current_move = (dir == DIRECTION_WEST) ? LX200_WEST : LX200_EAST;

    switch (command)
    {
        case MOTION_START:
            if (!isSimulation() && MoveTo(PortFD, current_move) < 0)
            {
                LOG_ERROR("Error setting W/E motion direction.");
                return false;
            }
            else
                LOGF_DEBUG("Moving toward %s.", (current_move == LX200_WEST) ? "West" : "East");
            break;

        case MOTION_STOP:
            if (!isSimulation() && HaltMovement(PortFD, current_move) < 0)
            {
                LOG_ERROR("Error stopping W/E motion.");
                return false;
            }
            else
                LOGF_DEBUG("Movement toward %s halted.",
                           (current_move == LX200_WEST) ? "West" : "East");
            break;
    }

    return true;
}

bool LX200Telescope::Abort()
{
    if (!isSimulation() && abortSlew(PortFD) < 0)
    {
        LOG_ERROR("Failed to abort slew.");
        return false;
    }

    if (GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY)
    {
        GuideNSNP.s = GuideWENP.s = IPS_IDLE;
        GuideNSN[0].value = GuideNSN[1].value = 0.0;
        GuideWEN[0].value = GuideWEN[1].value = 0.0;

        if (GuideNSTID)
        {
            IERmTimer(GuideNSTID);
            GuideNSTID = 0;
        }

        if (GuideWETID)
        {
            IERmTimer(GuideWETID);
            GuideNSTID = 0;
        }

        LOG_INFO("Guide aborted.");
        IDSetNumber(&GuideNSNP, nullptr);
        IDSetNumber(&GuideWENP, nullptr);

        return true;
    }

    return true;
}

bool LX200Telescope::setLocalDate(uint8_t days, uint8_t months, uint16_t years)
{
    return (setCalenderDate(PortFD, days, months, years) == 0);
}

bool LX200Telescope::setLocalTime24(uint8_t hour, uint8_t minute, uint8_t second)
{
    return (setLocalTime(PortFD, hour, minute, second) == 0);
}

bool LX200Telescope::setUTCOffset(double offset)
{
    return (::setUTCOffset(PortFD, (offset * -1.0)) == 0);
}

bool LX200Telescope::updateTime(ln_date *utc, double utc_offset)
{
    struct ln_zonedate ltm;

    if (isSimulation())
        return true;

    ln_date_to_zonedate(utc, &ltm, utc_offset * 3600.0);

    JD = ln_get_julian_day(utc);

    LOGF_DEBUG("New JD is %.2f", JD);

    // Meade defines UTC Offset as the offset ADDED to local time to yield UTC, which
    // is the opposite of the standard definition of UTC offset!
    if (setUTCOffset(utc_offset) == false)
    {
        LOG_ERROR("Error setting UTC Offset.");
        return false;
    }

    // Set Local Time
    if (setLocalTime24(ltm.hours, ltm.minutes, ltm.seconds) == false)
    {
        LOG_ERROR("Error setting local time.");
        return false;
    }

    if (setLocalDate(ltm.days, ltm.months, ltm.years) == false)
    {
        LOG_ERROR("Error setting local date.");
        return false;
    }

    LOG_INFO("Time updated, updating planetary data...");
    return true;
}

bool LX200Telescope::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (isSimulation())
        return true;

    if (!isSimulation() && setSiteLongitude(PortFD, 360.0 - longitude) < 0)
    {
        LOG_ERROR("Error setting site longitude coordinates");
        return false;
    }

    if (!isSimulation() && setSiteLatitude(PortFD, latitude) < 0)
    {
        LOG_ERROR("Error setting site latitude coordinates");
        return false;
    }

    char l[MAXINDINAME] = {0}, L[MAXINDINAME] = {0};
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    LOGF_INFO("Site location updated to Lat %.32s - Long %.32s", l, L);

    return true;
}

bool LX200Telescope::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, SiteNameTP.name))
        {
            if (!isSimulation() && setSiteName(PortFD, texts[0], currentSiteNum) < 0)
            {
                SiteNameTP.s = IPS_ALERT;
                IDSetText(&SiteNameTP, "Setting site name");
                return false;
            }

            SiteNameTP.s = IPS_OK;
            IText *tp    = IUFindText(&SiteNameTP, names[0]);
            IUSaveText(tp, texts[0]);
            IDSetText(&SiteNameTP, "Site name updated");
            return true;
        }
    }

    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool LX200Telescope::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strstr(name, "FOCUS_"))
        {
            return FI::processNumber(dev, name, values, names, n);
        }

        // Update Frequency
        if (!strcmp(name, TrackingFreqNP.name))
        {
            LOGF_DEBUG("Trying to set track freq of: %04.1f", values[0]);
            if (genericCapability & LX200_HAS_PRECISE_TRACKING_FREQ)
            {
                if (!isSimulation() && setPreciseTrackFreq(PortFD, values[0]) < 0)
                {
                    TrackingFreqNP.s = IPS_ALERT;
                    IDSetNumber(&TrackingFreqNP, "Error setting tracking frequency");
                    return false;
                }
                TrackingFreqNP.s           = IPS_OK;
                TrackingFreqNP.np[0].value = values[0];
                IDSetNumber(&TrackingFreqNP, "Tracking frequency set to %8.5f", values[0]);
            }
            else
            {
                //Normal Tracking Frequency
                if (!isSimulation() && setTrackFreq(PortFD, values[0]) < 0)

                    LOGF_DEBUG("Trying to set track freq of: %f\n", values[0]);
                if (!isSimulation() && setTrackFreq(PortFD, values[0]) < 0)
                {
                    LOGF_DEBUG("Trying to set track freq of: %f\n", values[0]);
                    if (!isSimulation() && setTrackFreq(PortFD, values[0]) < 0)
                    {
                        TrackingFreqNP.s = IPS_ALERT;
                        IDSetNumber(&TrackingFreqNP, "Error setting tracking frequency");
                        return false;
                    }
                    TrackingFreqNP.s           = IPS_OK;
                    IDSetNumber(&TrackingFreqNP, "Error setting tracking frequency");
                    return false;
                }

                TrackingFreqNP.s           = IPS_OK;
                TrackingFreqNP.np[0].value = values[0];
                IDSetNumber(&TrackingFreqNP, "Tracking frequency set to %04.1f", values[0]);
            }

            if (trackingMode != LX200_TRACK_MANUAL)
            {
                trackingMode    = LX200_TRACK_MANUAL;
                TrackModeS[0].s = ISS_OFF;
                TrackModeS[1].s = ISS_OFF;
                TrackModeS[2].s = ISS_OFF;
                TrackModeS[3].s = ISS_ON;
                TrackModeSP.s   = IPS_OK;
                selectTrackingMode(PortFD, trackingMode);
                IDSetSwitch(&TrackModeSP, nullptr);
            }

            return true;
        }

        //        if (!strcmp(name, FocusTimerNP.name))
        //        {
        //            // Don't update if busy
        //            if (FocusTimerNP.s == IPS_BUSY)
        //                return true;

        //            IUUpdateNumber(&FocusTimerNP, values, names, n);

        //            FocusTimerNP.s = IPS_OK;

        //            IDSetNumber(&FocusTimerNP, nullptr);

        //            LOGF_DEBUG("Setting focus timer to %.2f", FocusTimerN[0].value);

        //            return true;
        //        }

        processGuiderProperties(name, values, names, n);
    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool LX200Telescope::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Focuser
        if (strstr(name, "FOCUS"))
        {
            return FI::processSwitch(dev, name, states, names, n);
        }

        // Alignment
        if (!strcmp(name, AlignmentSP.name))
        {
            if (IUUpdateSwitch(&AlignmentSP, states, names, n) < 0)
                return false;

            int index = IUFindOnSwitchIndex(&AlignmentSP);

            if (!isSimulation() && setAlignmentMode(PortFD, index) < 0)
            {
                AlignmentSP.s = IPS_ALERT;
                IDSetSwitch(&AlignmentSP, "Error setting alignment mode.");
                return false;
            }

            AlignmentSP.s = IPS_OK;
            IDSetSwitch(&AlignmentSP, nullptr);
            return true;
        }

        // Sites
        if (!strcmp(name, SiteSP.name))
        {
            if (IUUpdateSwitch(&SiteSP, states, names, n) < 0)
                return false;

            currentSiteNum = IUFindOnSwitchIndex(&SiteSP) + 1;

            if (!isSimulation() && selectSite(PortFD, currentSiteNum) < 0)
            {
                SiteSP.s = IPS_ALERT;
                IDSetSwitch(&SiteSP, "Error selecting sites.");
                return false;
            }

            if (isSimulation())
                IUSaveText(&SiteNameTP.tp[0], "Sample Site");
            else
                getSiteName(PortFD, SiteNameTP.tp[0].text, currentSiteNum);

            if (GetTelescopeCapability() & TELESCOPE_HAS_LOCATION)
                sendScopeLocation();

            SiteNameTP.s = SiteSP.s = IPS_OK;

            IDSetText(&SiteNameTP, nullptr);
            IDSetSwitch(&SiteSP, nullptr);

            return false;
        }

        //        // Focus Motion
        //        if (!strcmp(name, FocusMotionSP.name))
        //        {
        //            // If mode is "halt"
        //            if (FocusModeS[0].s == ISS_ON)
        //            {
        //                FocusMotionSP.s = IPS_IDLE;
        //                IDSetSwitch(&FocusMotionSP, "Focus mode is halt. Select slow or fast mode");
        //                return true;
        //            }

        //            int last_motion = IUFindOnSwitchIndex(&FocusMotionSP);

        //            if (IUUpdateSwitch(&FocusMotionSP, states, names, n) < 0)
        //                return false;

        //            index = IUFindOnSwitchIndex(&FocusMotionSP);

        //            // If same direction and we're busy, stop
        //            if (last_motion == index && FocusMotionSP.s == IPS_BUSY)
        //            {
        //                IUResetSwitch(&FocusMotionSP);
        //                FocusMotionSP.s = IPS_IDLE;
        //                setFocuserSpeedMode(PortFD, 0);
        //                IDSetSwitch(&FocusMotionSP, nullptr);
        //                return true;
        //            }

        //            if (!isSimulation() && setFocuserMotion(PortFD, index) < 0)
        //            {
        //                FocusMotionSP.s = IPS_ALERT;
        //                IDSetSwitch(&FocusMotionSP, "Error setting focuser speed.");
        //                return false;
        //            }

        //            // with a timer
        //            if (FocusTimerN[0].value > 0)
        //            {
        //                FocusTimerNP.s  = IPS_BUSY;
        //                FocusMotionSP.s = IPS_BUSY;
        //                IEAddTimer(50, LX200Telescope::updateFocusHelper, this);
        //            }

        //            FocusMotionSP.s = IPS_OK;
        //            IDSetSwitch(&FocusMotionSP, nullptr);
        //            return true;
        //        }

        //        // Focus speed
        //        if (!strcmp(name, FocusModeSP.name))
        //        {
        //            IUResetSwitch(&FocusModeSP);
        //            IUUpdateSwitch(&FocusModeSP, states, names, n);

        //            index = IUFindOnSwitchIndex(&FocusModeSP);

        //            /* disable timer and motion */
        //            if (index == 0)
        //            {
        //                IUResetSwitch(&FocusMotionSP);
        //                FocusMotionSP.s = IPS_IDLE;
        //                FocusTimerNP.s  = IPS_IDLE;
        //                IDSetSwitch(&FocusMotionSP, nullptr);
        //                IDSetNumber(&FocusTimerNP, nullptr);
        //            }

        //            if (!isSimulation())
        //                setFocuserSpeedMode(PortFD, index);
        //            FocusModeSP.s = IPS_OK;
        //            IDSetSwitch(&FocusModeSP, nullptr);
        //            return true;
        //        }

        // Pulse-Guide command support
        if (!strcmp(name, UsePulseCmdSP.name))
        {
            IUResetSwitch(&UsePulseCmdSP);
            IUUpdateSwitch(&UsePulseCmdSP, states, names, n);

            UsePulseCmdSP.s = IPS_OK;
            IDSetSwitch(&UsePulseCmdSP, nullptr);
            usePulseCommand = (UsePulseCmdS[1].s == ISS_ON);
            LOGF_INFO("Pulse guiding is %s.", usePulseCommand ? "enabled" : "disabled");
            return true;
        }
    }

    //  Nobody has claimed this, so pass it to the parent
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool LX200Telescope::SetTrackMode(uint8_t mode)
{
    if (isSimulation())
        return true;

    bool rc = (selectTrackingMode(PortFD, mode) == 0);

    // Only update tracking frequency if it is defined and not deleted by child classes
    // Note, that LX200_HAS_PRECISE_TRACKING_FREQ can use the same get function
    if (rc &&  (genericCapability & LX200_HAS_TRACKING_FREQ))
    {
        getTrackFreq(PortFD, &TrackFreqN[0].value);
        IDSetNumber(&TrackingFreqNP, nullptr);
    }
    return rc;
}

bool LX200Telescope::SetSlewRate(int index)
{
    // Convert index to Meade format
    index = 3 - index;

    if (!isSimulation() && setSlewMode(PortFD, index) < 0)
    {
        LOG_ERROR("Error setting slew mode.");
        return false;
    }

    return true;
}

bool LX200Telescope::updateSlewRate(int index)
{
    if (IUFindOnSwitchIndex(&SlewRateSP) == index)
        return true;

    if (!isSimulation() && setSlewMode(PortFD, 3 - index) < 0)
    {
        SlewRateSP.s = IPS_ALERT;
        IDSetSwitch(&SlewRateSP, "Error setting slew mode.");
        return false;
    }

    IUResetSwitch(&SlewRateSP);
    SlewRateS[index].s = ISS_ON;
    SlewRateSP.s = IPS_OK;
    IDSetSwitch(&SlewRateSP, nullptr);
    return true;
}

void LX200Telescope::updateFocusHelper(void *p)
{
    static_cast<LX200Telescope *>(p)->updateFocusTimer();
}

void LX200Telescope::updateFocusTimer()
{
    //    switch (FocusTimerNP.s)
    //    {
    //        case IPS_IDLE:
    //            break;

    //        case IPS_BUSY:
    //            //if (isDebug())
    //            //IDLog("Focus Timer Value is %g\n", FocusTimerN[0].value);

    //            FocusTimerN[0].value -= 50;

    //            if (FocusTimerN[0].value <= 0)
    //            {
    //                //if (isDebug())
    //                //IDLog("Focus Timer Expired\n");

    //                if (!isSimulation() && setFocuserSpeedMode(PortFD, 0) < 0)
    //                {
    //                    FocusModeSP.s = IPS_ALERT;
    //                    IDSetSwitch(&FocusModeSP, "Error setting focuser mode.");

    //                    //if (isDebug())
    //                    //IDLog("Error setting focuser mode\n");

    //                    return;
    //                }

    //                FocusMotionSP.s = IPS_IDLE;
    //                FocusTimerNP.s  = IPS_OK;
    //                FocusModeSP.s   = IPS_OK;

    //                IUResetSwitch(&FocusMotionSP);
    //                IUResetSwitch(&FocusModeSP);
    //                FocusModeS[0].s = ISS_ON;

    //                IDSetSwitch(&FocusModeSP, nullptr);
    //                IDSetSwitch(&FocusMotionSP, nullptr);
    //            }

    //            IDSetNumber(&FocusTimerNP, nullptr);

    //            if (FocusTimerN[0].value > 0)
    //                IEAddTimer(50, LX200Telescope::updateFocusHelper, this);
    //            break;

    //        case IPS_OK:
    //            break;

    //        case IPS_ALERT:
    //            break;
    //    }

    AbortFocuser();
    FocusTimerNP.s = IPS_IDLE;
    FocusTimerN[0].value = 0;
    IDSetNumber(&FocusTimerNP, nullptr);
}

void LX200Telescope::mountSim()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt = 0, da = 0, dx = 0;
    int nlocked = 0;

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday(&tv, nullptr);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt  = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec) / 1e6;
    ltv = tv;
    da  = LX200_GENERIC_SLEWRATE * dt;

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly */
    switch (TrackState)
    {

        case SCOPE_IDLE:
            currentRA  += (TRACKRATE_SIDEREAL / 3600.0 * dt / 15.);
            break;

        case SCOPE_TRACKING:
            switch (IUFindOnSwitchIndex(&TrackModeSP))
            {
                case TRACK_SIDEREAL:
                    da = 0;
                    dx = 0;
                    break;

                case TRACK_LUNAR:
                    da = ((TRACKRATE_LUNAR - TRACKRATE_SIDEREAL) / 3600.0 * dt / 15.);
                    dx = 0;
                    break;

                case TRACK_SOLAR:
                    da = ((TRACKRATE_SOLAR - TRACKRATE_SIDEREAL) / 3600.0 * dt / 15.);
                    dx = 0;
                    break;

                case TRACK_CUSTOM:
                    da = ((TrackRateN[AXIS_RA].value - TRACKRATE_SIDEREAL) / 3600.0 * dt / 15.);
                    dx = (TrackRateN[AXIS_DE].value / 3600.0 * dt);
                    break;

            }

            currentRA  += da;
            currentDEC += dx;
            break;

        case SCOPE_SLEWING:
        case SCOPE_PARKING:
            /* slewing - nail it when both within one pulse @ LX200_GENERIC_SLEWRATE */
            nlocked = 0;

            dx = targetRA - currentRA;

            if (fabs(dx) <= da)
            {
                currentRA = targetRA;
                nlocked++;
            }
            else if (dx > 0)
                currentRA += da / 15.;
            else
                currentRA -= da / 15.;

            dx = targetDEC - currentDEC;
            if (fabs(dx) <= da)
            {
                currentDEC = targetDEC;
                nlocked++;
            }
            else if (dx > 0)
                currentDEC += da;
            else
                currentDEC -= da;

            if (nlocked == 2)
            {
                if (TrackState == SCOPE_SLEWING)
                    TrackState = SCOPE_TRACKING;
                else
                    SetParked(true);
            }

            break;

        default:
            break;
    }

    NewRaDec(currentRA, currentDEC);
}

void LX200Telescope::getBasicData()
{
    if (!isSimulation())
    {
        checkLX200Format(PortFD);

        if (genericCapability & LX200_HAS_ALIGNMENT_TYPE)
            getAlignment();

        // Only check time format if it is not already initialized by the class
        if ( (GetTelescopeCapability() & TELESCOPE_HAS_TIME) && timeFormat == -1)
        {
            if (getTimeFormat(PortFD, &timeFormat) < 0)
                LOG_ERROR("Failed to retrieve time format from device.");
            else
            {
                int ret = 0;

                timeFormat = (timeFormat == 24) ? LX200_24 : LX200_AM;
                // We always do 24 hours
                if (timeFormat != LX200_24)
                    ret = toggleTimeFormat(PortFD);
            }
        }

        if (genericCapability & LX200_HAS_SITES)
        {
            SiteNameT[0].text = new char[64];

            if (getSiteName(PortFD, SiteNameT[0].text, currentSiteNum) < 0)
                LOG_ERROR("Failed to get site name from device");
            else
                IDSetText(&SiteNameTP, nullptr);
        }


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

void LX200Telescope::slewError(int slewCode)
{
    if (slewCode == 1)
        LOG_ERROR("Object below horizon.");
    else if (slewCode == 2)
        LOG_ERROR("Object below the minimum elevation limit.");
    else
        LOG_ERROR("Slew failed.");

    EqNP.s = IPS_ALERT;
    IDSetNumber(&EqNP, nullptr);
}

void LX200Telescope::getAlignment()
{
    signed char align = ACK(PortFD);
    if (align < 0)
    {
        IDSetSwitch(&AlignmentSP, "Failed to get telescope alignment.");
        return;
    }

    AlignmentS[0].s = ISS_OFF;
    AlignmentS[1].s = ISS_OFF;
    AlignmentS[2].s = ISS_OFF;

    switch (align)
    {
        case 'P':
            AlignmentS[0].s = ISS_ON;
            break;
        case 'A':
            AlignmentS[1].s = ISS_ON;
            break;
        case 'L':
            AlignmentS[2].s = ISS_ON;
            break;
    }

    AlignmentSP.s = IPS_OK;
    IDSetSwitch(&AlignmentSP, nullptr);
}

bool LX200Telescope::getLocalTime(char *timeString)
{
    if (isSimulation())
    {
        time_t now = time (nullptr);
        strftime(timeString, MAXINDINAME, "%T", localtime(&now));
    }
    else
    {
        double ctime = 0;
        int h, m, s;
        getLocalTime24(PortFD, &ctime);
        getSexComponents(ctime, &h, &m, &s);
        snprintf(timeString, MAXINDINAME, "%02d:%02d:%02d", h, m, s);
    }

    return true;
}

bool LX200Telescope::getLocalDate(char *dateString)
{
    if (isSimulation())
    {
        time_t now = time (nullptr);
        strftime(dateString, MAXINDINAME, "%F", localtime(&now));
    }
    else
    {
        getCalendarDate(PortFD, dateString);
    }

    return true;
}

bool LX200Telescope::getUTFOffset(double *offset)
{
    if (isSimulation())
    {
        *offset = 3;
        return true;
    }

    int lx200_utc_offset = 0;
    getUTCOffset(PortFD, &lx200_utc_offset);
    // LX200 TimeT Offset is defined at the number of hours added to LOCAL TIME to get TimeT. This is contrary to the normal definition.
    *offset = lx200_utc_offset * -1;
    return true;
}

bool LX200Telescope::sendScopeTime()
{
    char cdate[MAXINDINAME] = {0};
    char ctime[MAXINDINAME] = {0};
    struct tm ltm;
    struct tm utm;
    time_t time_epoch;

    double offset = 0;
    if (getUTFOffset(&offset))
    {
        char utcStr[8] = {0};
        snprintf(utcStr, 8, "%.2f", offset);
        IUSaveText(&TimeT[1], utcStr);
    }
    else
    {
        LOG_WARN("Could not obtain UTC offset from mount!");
        return false;
    }

    if (getLocalTime(ctime) == false)
    {
        LOG_WARN("Could not obtain local time from mount!");
        return false;
    }

    if (getLocalDate(cdate) == false)
    {
        LOG_WARN("Could not obtain local date from mount!");
        return false;
    }

    // To ISO 8601 format in LOCAL TIME!
    char datetime[MAXINDINAME] = {0};
    snprintf(datetime, MAXINDINAME, "%sT%s", cdate, ctime);

    // Now that date+time are combined, let's get tm representation of it.
    if (strptime(datetime, "%FT%T", &ltm) == nullptr)
    {
        LOGF_WARN("Could not process mount date and time: %s", datetime);
        return false;
    }

    // Get local time epoch in UNIX seconds
    time_epoch = mktime(&ltm);

    // LOCAL to UTC by subtracting offset.
    time_epoch -= static_cast<int>(offset * 3600.0);

    // Get UTC (we're using localtime_r, but since we shifted time_epoch above by UTCOffset, we should be getting the real UTC time)
    localtime_r(&time_epoch, &utm);

    // Format it into the final UTC ISO 8601
    strftime(cdate, MAXINDINAME, "%Y-%m-%dT%H:%M:%S", &utm);
    IUSaveText(&TimeT[0], cdate);

    LOGF_DEBUG("Mount controller UTC Time: %s", TimeT[0].text);
    LOGF_DEBUG("Mount controller UTC Offset: %s", TimeT[1].text);

    // Let's send everything to the client
    TimeTP.s = IPS_OK;
    IDSetText(&TimeTP, nullptr);

    return true;
}

bool LX200Telescope::sendScopeLocation()
{
    int dd = 0, mm = 0;

    if (isSimulation())
    {
        LocationNP.np[LOCATION_LATITUDE].value = 29.5;
        LocationNP.np[LOCATION_LONGITUDE].value = 48.0;
        LocationNP.np[LOCATION_ELEVATION].value = 10;
        LocationNP.s           = IPS_OK;
        IDSetNumber(&LocationNP, nullptr);
        return true;
    }

    if (getSiteLatitude(PortFD, &dd, &mm) < 0)
    {
        LOG_WARN("Failed to get site latitude from device.");
        return false;
    }
    else
    {
        if (dd > 0)
            LocationNP.np[0].value = dd + mm / 60.0;
        else
            LocationNP.np[0].value = dd - mm / 60.0;
    }

    if (getSiteLongitude(PortFD, &dd, &mm) < 0)
    {
        LOG_WARN("Failed to get site longitude from device.");
        return false;
    }
    else
    {
        if (dd > 0)
            LocationNP.np[1].value = 360.0 - (dd + mm / 60.0);
        else
            LocationNP.np[1].value = (dd - mm / 60.0) * -1.0;

    }

    LOGF_DEBUG("Mount Controller Latitude: %g Longitude: %g", LocationN[LOCATION_LATITUDE].value, LocationN[LOCATION_LONGITUDE].value);

    IDSetNumber(&LocationNP, nullptr);

    saveConfig(true, "GEOGRAPHIC_COORD");

    return true;
}

IPState LX200Telescope::GuideNorth(uint32_t ms)
{
    // If we're using pulse command, then MovementXXX should NOT be active at all.
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot pulse guide while manually in motion. Stop first.");
        return IPS_ALERT;
    }

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if (usePulseCommand)
    {
        SendPulseCmd(LX200_NORTH, ms);
    }
    else
    {
        updateSlewRate(SLEW_GUIDE);

        ISState states[] = { ISS_ON, ISS_OFF };
        const char *names[] = { MovementNSS[DIRECTION_NORTH].name, MovementNSS[DIRECTION_SOUTH].name};
        ISNewSwitch(MovementNSSP.device, MovementNSSP.name, states, const_cast<char **>(names), 2);
    }

    guide_direction_ns = LX200_NORTH;
    GuideNSTID      = IEAddTimer(static_cast<int>(ms), guideTimeoutHelperNS, this);
    return IPS_BUSY;
}

IPState LX200Telescope::GuideSouth(uint32_t ms)
{
    // If we're using pulse command, then MovementXXX should NOT be active at all.
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot pulse guide while manually in motion. Stop first.");
        return IPS_ALERT;
    }

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if (usePulseCommand)
    {
        SendPulseCmd(LX200_SOUTH, ms);
    }
    else
    {
        updateSlewRate(SLEW_GUIDE);

        ISState states[] = { ISS_OFF, ISS_ON };
        const char *names[] = { MovementNSS[DIRECTION_NORTH].name, MovementNSS[DIRECTION_SOUTH].name};
        ISNewSwitch(MovementNSSP.device, MovementNSSP.name, states, const_cast<char **>(names), 2);
    }

    guide_direction_ns = LX200_SOUTH;
    GuideNSTID      = IEAddTimer(static_cast<int>(ms), guideTimeoutHelperNS, this);
    return IPS_BUSY;
}

IPState LX200Telescope::GuideEast(uint32_t ms)
{
    // If we're using pulse command, then MovementXXX should NOT be active at all.
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot pulse guide while manually in motion. Stop first.");
        return IPS_ALERT;
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    if (usePulseCommand)
    {
        SendPulseCmd(LX200_EAST, ms);
    }
    else
    {
        updateSlewRate(SLEW_GUIDE);

        ISState states[] = { ISS_OFF, ISS_ON };
        const char *names[] = { MovementWES[DIRECTION_WEST].name, MovementWES[DIRECTION_EAST].name};
        ISNewSwitch(MovementWESP.device, MovementWESP.name, states, const_cast<char **>(names), 2);
    }

    guide_direction_we = LX200_EAST;
    GuideWETID      = IEAddTimer(static_cast<int>(ms), guideTimeoutHelperWE, this);
    return IPS_BUSY;
}

IPState LX200Telescope::GuideWest(uint32_t ms)
{
    // If we're using pulse command, then MovementXXX should NOT be active at all.
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot pulse guide while manually in motion. Stop first.");
        return IPS_ALERT;
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    if (usePulseCommand)
    {
        SendPulseCmd(LX200_WEST, ms);
    }
    else
    {
        updateSlewRate(SLEW_GUIDE);

        ISState states[] = { ISS_ON, ISS_OFF };
        const char *names[] = { MovementWES[DIRECTION_WEST].name, MovementWES[DIRECTION_EAST].name};
        ISNewSwitch(MovementWESP.device, MovementWESP.name, states, const_cast<char **>(names), 2);
    }

    guide_direction_we = LX200_WEST;
    GuideWETID      = IEAddTimer(static_cast<int>(ms), guideTimeoutHelperWE, this);
    return IPS_BUSY;
}

int LX200Telescope::SendPulseCmd(int8_t direction, uint32_t duration_msec)
{
    return ::SendPulseCmd(PortFD, direction, duration_msec);
}

void LX200Telescope::guideTimeoutHelperNS(void * p)
{
    static_cast<LX200Telescope *>(p)->guideTimeoutNS();
}

void LX200Telescope::guideTimeoutHelperWE(void * p)
{
    static_cast<LX200Telescope *>(p)->guideTimeoutWE();
}

void LX200Telescope::guideTimeoutWE()
{
    if (usePulseCommand == false)
    {
        ISState states[] = { ISS_OFF, ISS_OFF };
        const char *names[] = { MovementWES[DIRECTION_WEST].name, MovementWES[DIRECTION_EAST].name};
        ISNewSwitch(MovementWESP.device, MovementWESP.name, states, const_cast<char **>(names), 2);
    }

    GuideWENP.np[DIRECTION_WEST].value = 0;
    GuideWENP.np[DIRECTION_EAST].value = 0;
    GuideWENP.s           = IPS_IDLE;
    GuideWETID            = 0;
    IDSetNumber(&GuideWENP, nullptr);
}

void LX200Telescope::guideTimeoutNS()
{
    if (usePulseCommand == false)
    {
        ISState states[] = { ISS_OFF, ISS_OFF };
        const char *names[] = { MovementNSS[DIRECTION_NORTH].name, MovementNSS[DIRECTION_SOUTH].name};
        ISNewSwitch(MovementNSSP.device, MovementNSSP.name, states, const_cast<char **>(names), 2);
    }

    GuideNSNP.np[0].value = 0;
    GuideNSNP.np[1].value = 0;
    GuideNSNP.s           = IPS_IDLE;
    GuideNSTID            = 0;
    IDSetNumber(&GuideNSNP, nullptr);

}

bool LX200Telescope::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    if (genericCapability & LX200_HAS_PULSE_GUIDING)
        IUSaveConfigSwitch(fp, &UsePulseCmdSP);

    if (genericCapability & LX200_HAS_FOCUS)
        FI::saveConfigItems(fp);

    return true;
}

bool LX200Telescope::ReverseFocuser(bool enabled)
{
    INDI_UNUSED(enabled);
    return true;
}

bool LX200Telescope::AbortFocuser()
{
    return SetFocuserSpeed(0);
}

IPState LX200Telescope::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    FocusDirection finalDirection = dir;
    // Reverse final direction if necessary
    if (FocusReverseS[REVERSED_ENABLED].s == ISS_ON)
        finalDirection = (dir == FOCUS_INWARD) ? FOCUS_OUTWARD : FOCUS_INWARD;

    SetFocuserSpeed(speed);

    setFocuserMotion(PortFD, finalDirection);

    IEAddTimer(duration, &LX200Telescope::updateFocusHelper, this);

    return IPS_BUSY;
}

bool LX200Telescope::SetFocuserSpeed(int speed)
{
    return (setFocuserSpeedMode(PortFD, speed) == 0);
}
