/*

LX200_TeenAstro 

Based on LX200_OnStep and others
François Desvallées https://github.com/fdesvallees

Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "lx200_TeenAstro.h"
#include <libnova/sidereal_time.h>

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <cerrno>

#include <cmath>
#include <memory>
#include <cstring>
#include <mutex>


/* Simulation Parameters */
#define SLEWRATE 1        /* slew rate, degrees/s */
#define SIDRATE  0.004178 /* sidereal rate, degrees/s */

#define FIRMWARE_TAB "Firmware data"

#define ONSTEP_TIMEOUT  3

// Our telescope auto pointer
static std::unique_ptr<LX200_TeenAstro> teenAstro(new LX200_TeenAstro());
extern std::mutex lx200CommsLock;

/*
 * LX200 TeenAstro constructor
 */
LX200_TeenAstro::LX200_TeenAstro()
{
    setVersion(1, 2);           // don't forget to update drivers.xml

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    SetTelescopeCapability(
        TELESCOPE_CAN_GOTO | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT |
        TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_PIER_SIDE |
        TELESCOPE_HAS_TRACK_MODE  | TELESCOPE_CAN_CONTROL_TRACK);

    LOG_DEBUG("Initializing from LX200 TeenAstro device...");
}

void LX200_TeenAstro::debugTriggered(bool enable)
{
    INDI_UNUSED(enable);
    setLX200Debug(getDeviceName(), DBG_SCOPE);
}

const char *LX200_TeenAstro::getDriverName()
{
    return getDefaultName();
}

const char *LX200_TeenAstro::getDefaultName()
{
    return "LX200 TeenAstro";
}

bool LX200_TeenAstro::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    SetParkDataType(PARK_RA_DEC);

    // ============== MAIN_CONTROL_TAB

    // Tracking Mode
     AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
     AddTrackMode("TRACK_SOLAR", "Solar");
     AddTrackMode("TRACK_LUNAR", "Lunar");

    // Error Status
    IUFillText(&ErrorStatusT[0], "Error code", "", "");
    IUFillTextVector(&ErrorStatusTP, ErrorStatusT, 1, getDeviceName(), "Mount Status", "", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);


    // ============== MOTION_TAB
    // Motion speed of axis when pressing NSWE buttons
    IUFillSwitch(&SlewRateS[0], "Guide", "Guide Speed", ISS_OFF);
    IUFillSwitch(&SlewRateS[1], "Slow", "Slow", ISS_OFF);
    IUFillSwitch(&SlewRateS[2], "Medium", "Medium", ISS_OFF);
    IUFillSwitch(&SlewRateS[3], "Fast", "Fast", ISS_ON);
    IUFillSwitch(&SlewRateS[4], "Max", "Max", ISS_OFF);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 5, getDeviceName(), "TELESCOPE_SLEW_RATE", "Centering Rate", 
                        MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // ============== GUIDE_TAB
    // Motion speed of axis when guiding
    IUFillSwitch(&GuideRateS[0], "25", "0.25x", ISS_OFF);
    IUFillSwitch(&GuideRateS[1], "50", "0.5x", ISS_ON);
    IUFillSwitch(&GuideRateS[2], "100", "1.0x", ISS_OFF);
    IUFillSwitchVector(&GuideRateSP, GuideRateS, 3, getDeviceName(), "TELESCOPE_GUIDE_RATE", "Guide Rate", GUIDE_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    initGuiderProperties(getDeviceName(), GUIDE_TAB);

    // ============== OPTIONS_TAB
    // Slew threshold
    IUFillNumber(&SlewAccuracyN[0], "SlewRA", "RA (arcmin)", "%10.6m", 0., 60., 1., 3.0);   // min,max,step,current
    IUFillNumber(&SlewAccuracyN[1], "SlewDEC", "Dec (arcmin)", "%10.6m", 0., 60., 1., 3.0);
    IUFillNumberVector(&SlewAccuracyNP, SlewAccuracyN, NARRAY(SlewAccuracyN), getDeviceName(), "Slew Accuracy", "",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // ============== SITE_TAB
    IUFillSwitch(&SiteS[0], "Site 1", "", ISS_OFF);
    IUFillSwitch(&SiteS[1], "Site 2", "", ISS_OFF);
    IUFillSwitch(&SiteS[2], "Site 3", "", ISS_OFF);
    IUFillSwitch(&SiteS[3], "Site 4", "", ISS_OFF);
    IUFillSwitchVector(&SiteSP, SiteS, 4, getDeviceName(), "Sites", "", SITE_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&SiteNameT[0], "Name", "", "");
    IUFillTextVector(&SiteNameTP, SiteNameT, 1, getDeviceName(), "Site Name", "", SITE_TAB, IP_RO, 0, IPS_IDLE);


    // ============== FIRMWARE_TAB
    IUFillText(&VersionT[0], "Date", "", "");
    IUFillText(&VersionT[1], "Time", "", "");
    IUFillText(&VersionT[2], "Number", "", "");
    IUFillText(&VersionT[3], "Name", "", "");
    IUFillTextVector(&VersionTP, VersionT, 4, getDeviceName(), "Firmware Info", "", FIRMWARE_TAB, IP_RO, 0, IPS_IDLE);

    addAuxControls();
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

#if 0
    // dont' need to get from config file - mount already has location
    double longitude = 0, latitude = 90;
    // Get value from config file if it exists.
    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude);
    currentRA  = get_local_sidereal_time(longitude);
    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude);
    currentDEC = latitude > 0 ? 90 : -90;
#endif

    return true;
}
void LX200_TeenAstro::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    INDI::Telescope::ISGetProperties(dev);
}

bool LX200_TeenAstro::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        // Delete inherited controls - too confusing
        deleteProperty("USEJOYSTICK");
        deleteProperty("ACTIVE_DEVICES");
        deleteProperty("DOME_POLICY");
        deleteProperty("TELESCOPE_HAS_TRACK_RATE");
        // Main Control
        defineProperty(&SlewAccuracyNP);
        defineProperty(&ErrorStatusTP);
        // Connection
        // Options
        // Motion Control
        defineProperty(&SlewRateSP);
        defineProperty(&GuideRateSP);

        // Site Management
        defineProperty(&ParkOptionSP);
        defineProperty(&SetHomeSP);

        defineProperty(&SiteSP);
        defineProperty(&SiteNameTP);

        // Guide
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);

        // Firmware Data
        defineProperty(&VersionTP);
        getBasicData();
    }
    else
    {
        // Main Control
        deleteProperty(SlewAccuracyNP.name);
        deleteProperty(ErrorStatusTP.name);
      // Connection
        // Options
        // Motion Control
        deleteProperty(SlewRateSP.name);
        deleteProperty(GuideRateSP.name);
        deleteProperty(SiteSP.name);
        deleteProperty(SiteNameTP.name);

        // Site Management
        deleteProperty(ParkOptionSP.name);
        deleteProperty(SetHomeSP.name);
        // Guide
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        // Firmware Data
        deleteProperty(VersionTP.name);
    }
    return true;
}


bool LX200_TeenAstro::Handshake()
{
    if (isSimulation())
    {
        LOG_INFO("Simulated Connection.");
        return true;
    }

    if (getLX200RA(PortFD, &currentRA) != 0)
    {
        LOG_ERROR("Error communicating with telescope.");
        return false;
    }
    LOG_INFO("TeenAstro is Connected");
    return true;
}

/*
 * ReadScopeStatus
 * Called when polling the mount about once per second
 */
bool LX200_TeenAstro::ReadScopeStatus()
{
    if (isSimulation())
    {
        mountSim();
        return true;
    }
    if (!isConnected())
        return false;

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.s = IPS_ALERT;
        return false;
    }

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if LX200 is done slewing
        if (isSlewComplete())
        {
            TrackState = SCOPE_TRACKING;
            LOG_INFO("Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        LOG_INFO("Parking");
    }

    // update mount status
    getCommandString(PortFD, OSStat, statusCommand);       // returns a string containg controller status
    if (OSStat[15] != '0')
    {
        updateMountStatus(OSStat[15]);              // error
    }
    if (strcmp(OSStat, OldOSStat) != 0)             // if status changed
    {
        handleStatusChange();
        snprintf(OldOSStat, sizeof(OldOSStat), "%s", OSStat);
    }

    NewRaDec(currentRA, currentDEC);

    return true;
}

/*
 * Use OSStat to detect status change - handle each byte separately
 * called by ReadScopeStatus()
 */
void LX200_TeenAstro::handleStatusChange(void)
{
    LOGF_DEBUG ("Status Change: %s", OSStat);        

    if (OSStat[0] != OldOSStat[0])
    {
        if (OSStat[0] == '0')
        {
            TrackState = SCOPE_IDLE;
        }
        else if (OSStat[0] == '1')
        {
            TrackState = SCOPE_TRACKING;
        }
        else if (OSStat[0] == '2' || OSStat[0] == '3' )
        {
            TrackState = SCOPE_SLEWING;
        }
    }


    // Byte 2 is park status
    if (OSStat[2] != OldOSStat[2])
    {
        if (OSStat[2] == 'P')
        {
            SetParked(true);            // defaults to TrackState=SCOPE_PARKED
        }
        else
        {
            SetParked(false);
//            SetTrackEnabled(false);     //disable since TeenAstro enables it by default            
        }
    }
    // Byte 13 is pier side
    if (OSStat[13] != OldOSStat[13])
    {
        setPierSide(OSStat[13] == 'W' ? INDI::Telescope::PIER_WEST : INDI::Telescope::PIER_EAST);
    }
    // Byte 15 is the error status
    if (OSStat[15] != OldOSStat[15])
    {
        updateMountStatus(OSStat[15]);
    }
}

/*
 * Mount Error status
 * 0:ERR_NONE,  1: ERR_MOTOR_FAULT, 2: ERR_ALT, 3: ERR_LIMIT_SENSE 
 * 4: ERR_AXIS2,5: ERR_AZM, 6: ERR_UNDER_POLE, 7: ERR_MERIDIAN, 8: ERR_SYNC
 */
void LX200_TeenAstro::updateMountStatus(char status)
{
    static const char *errCodes[9] = {"ERR_NONE",  "ERR_MOTOR_FAULT", "ERR_ALT", "ERR_LIMIT_SENSE", 
                                "ERR_AXIS2", "ERR_AZM", "ERR_UNDER_POLE", "ERR_MERIDIAN", "ERR_SYNC"};
    
    if (status < '0' || status > '9')
    {
        return;
    }
    if (status == '0')
    {
        ErrorStatusTP.s = IPS_OK;
    }
    else
    {
        ErrorStatusTP.s = IPS_ALERT;
        TrackState = SCOPE_IDLE;     // Tell Ekos mount is not tracking anymore            
    }
    IUSaveText(&ErrorStatusT[0], errCodes[status-'0']);
    IDSetText(&ErrorStatusTP, nullptr);        
}

/*
 *  Goto target 
 *  Use standard lx200driver command (:Sr   #) 
 *  Set state to slewing
 */
bool LX200_TeenAstro::Goto(double r, double d)
{
    targetRA  = r;
    targetDEC = d;
    char RAStr[64] = {0}, DecStr[64] = {0};

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

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

        // sleep for 100 mseconds
        usleep(100000);
    }

    if (!isSimulation())
    {
        if (setObjectRA(PortFD, targetRA) < 0 || (setObjectDEC(PortFD, targetDEC)) < 0)  // standard LX200 command
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error setting RA/DEC.");
            return false;
        }

        int err = 0;

        /* Slew reads the '0', that is not the end of the slew */
        if ((err = Slew(PortFD)))
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
            slewError(err);
            return false;
        }
    }

    TrackState = SCOPE_SLEWING;
    EqNP.s     = IPS_BUSY;

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool LX200_TeenAstro::isSlewComplete()
{
    const double dx = targetRA - currentRA;
    const double dy = targetDEC - currentDEC;
    return fabs(dx) <= (SlewAccuracyN[0].value / (900.0)) && fabs(dy) <= (SlewAccuracyN[1].value / 60.0);
}

bool LX200_TeenAstro::SetTrackMode(uint8_t mode)
{
    if (isSimulation())
        return true;

    bool rc = (selectTrackingMode(PortFD, mode) == 0);

    return rc;
}

/*
 * Sync - synchronizes the telescope with its currently selected database object coordinates 
 */
bool LX200_TeenAstro::Sync(double ra, double dec)
{
    char syncString[256] = {0};

    // goto target
    if (!isSimulation() && (setObjectRA(PortFD, ra) < 0 || (setObjectDEC(PortFD, dec)) < 0))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error setting RA/DEC. Unable to Sync.");
        return false;
    }

    // Use the parent Sync() function (lx200driver.cpp)
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



//======================== Parking =======================
bool LX200_TeenAstro::SetCurrentPark()
{
    char response[RB_MAX_LEN];

    if (isSimulation())
    {
        LOG_DEBUG("SetCurrentPark: CMD <:hQ>");
        return true;
    }

    if (getCommandString(PortFD, response, ":hQ#") < 0)
    {
        LOGF_WARN("===CMD==> Set Park Pos %s", response);
        return false;
    }
    SetAxis1Park(currentRA);
    SetAxis2Park(currentDEC);
    LOG_WARN("Park Value set to current postion");
    return true;
}

bool LX200_TeenAstro::UnPark()
{
    char response[RB_MAX_LEN];

    if (isSimulation())
    {
        LOG_DEBUG("UnPark: CMD <:hR>");
        TrackState = SCOPE_IDLE;
        EqNP.s    = IPS_OK;
        return true;
    }
    if (getCommandString(PortFD, response, ":hR#") < 0)
    {
        return false;
    }
    SetParked(false);

    return true;
}

bool LX200_TeenAstro::Park()
{
    if (isSimulation())
    {
        LOG_DEBUG("SlewToPark: CMD <:hP>");
        TrackState = SCOPE_PARKED;
        EqNP.s    = IPS_OK;
        return true;
    }

    // If scope is moving, let's stop it first.
    if (EqNP.s == IPS_BUSY)
    {
        if (abortSlew(PortFD) < 0)
        {
            Telescope::AbortSP.s = IPS_ALERT;
            IDSetSwitch(&(Telescope::AbortSP), "Abort slew failed.");
            return false;
        }
        Telescope::AbortSP.s = IPS_OK;
        EqNP.s    = IPS_IDLE;
        IDSetSwitch(&(Telescope::AbortSP), "Slew aborted.");
        IDSetNumber(&EqNP, nullptr);

        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
        {
            MovementNSSP.s = IPS_IDLE;
            MovementWESP.s = IPS_IDLE;
            EqNP.s = IPS_IDLE;
            IUResetSwitch(&MovementNSSP);
            IUResetSwitch(&MovementWESP);

            IDSetSwitch(&MovementNSSP, nullptr);
            IDSetSwitch(&MovementWESP, nullptr);
        }
    }
    if (slewToPark(PortFD) < 0)  // slewToPark is a macro (lx200driver.h)
    {
        ParkSP.s = IPS_ALERT;
        IDSetSwitch(&ParkSP, "Parking Failed.");
        return false;
    }
    ParkSP.s   = IPS_BUSY;
    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking is in progress...");

    return true;
}


/*
 *  updateLocation: not used - use hand controller to update
 */
bool LX200_TeenAstro::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(latitude);
    INDI_UNUSED(longitude);
    INDI_UNUSED(elevation);

    return true;
}

/*
 *  getBasicData: standard LX200 commands
 */
void LX200_TeenAstro::getBasicData()
{
    int currentSiteIndex, slewRateIndex;

    if (!isSimulation())
    {
        checkLX200EquatorialFormat(PortFD);
        char buffer[128];
        getVersionDate(PortFD, buffer);
        IUSaveText(&VersionT[0], buffer);
        getVersionTime(PortFD, buffer);
        IUSaveText(&VersionT[1], buffer);
        getVersionNumber(PortFD, buffer);
        statusCommand = ":GXI#";
        guideSpeedCommand = ":SXR0:%s#";
        IUSaveText(&VersionT[2], buffer);
        getProductName(PortFD, buffer);
        IUSaveText(&VersionT[3], buffer);

        IDSetText(&VersionTP, nullptr);
        SiteNameT[0].text = new char[64];
        sendScopeTime();

        if (getSiteIndex(&currentSiteIndex))
        {
            SiteS[currentSiteIndex].s = ISS_ON;
            currentSiteNum = currentSiteIndex + 1;
            LOGF_INFO("Site number %d", currentSiteNum);
            getSiteName(PortFD, SiteNameTP.tp[0].text, currentSiteNum);
            SiteNameTP.s = IPS_OK;
            SiteSP.s = IPS_OK;
            IDSetText(&SiteNameTP, nullptr);
            IDSetSwitch(&SiteSP, nullptr);
            getLocation();                  // read site from TeenAstro
        }
        else
        {
             LOG_ERROR("Error reading current site number");
        }

        // Get initial state and set switches
        for (unsigned i=0;i<sizeof(OldOSStat);i++)
            OldOSStat[i] = 'x';                         // reset old OS stat to force re-evaluation
        getCommandString(PortFD, OSStat, statusCommand);       // returns a string containing controller status
        handleStatusChange();
        LOGF_INFO("Initial Status: %s", OSStat);

        // get current slew rate
        if (getSlewRate(&slewRateIndex))
        {
            LOGF_INFO("current slew rate : %d", slewRateIndex);
            SlewRateS[slewRateIndex].s = ISS_ON; 
            SlewRateSP.s = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);
        }
        else
        {
             LOG_ERROR("Error reading current slew rate");
        }

        // Turn off tracking. (too much interaction with telescope.cpp if we try to keep the mount's current track state)
        if (TrackState != SCOPE_TRACKING)
        {
            SetTrackEnabled(false);
        }

 
        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            LOG_INFO("=============== Parkdata loaded");
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            LOG_INFO("=============== Parkdata Load Failed");
        }
    }
}

/*
 * ISNewNumber: callback from user interface
 * 
 * when user has entered a number, handle it to store corresponding driver parameter
 *
 */
bool LX200_TeenAstro::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, SlewAccuracyNP.name))
        {
            if (IUUpdateNumber(&SlewAccuracyNP, values, names, n) < 0)
                return false;

            SlewAccuracyNP.s = IPS_OK;

            if (SlewAccuracyN[0].value < 3 || SlewAccuracyN[1].value < 3)
                IDSetNumber(&SlewAccuracyNP, "Warning: Setting the slew accuracy too low may result in a dead lock");

            IDSetNumber(&SlewAccuracyNP, nullptr);
            return true;
        }

        // GUIDE process Guider properties.
        processGuiderProperties(name, values, names, n);
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}



/*
 * ISNewSwitch: callback from user interface
 * 
 * when user has entered a switch, handle it to store corresponding driver parameter
 *
 */
bool LX200_TeenAstro::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Slew button speed
        if (!strcmp(name, SlewRateSP.name))
        {
            IUUpdateSwitch(&SlewRateSP, states, names, n);
            int slewRate = IUFindOnSwitchIndex(&SlewRateSP);

            if (!selectSlewRate(slewRate))
            {
                LOGF_ERROR("Error setting move to rate %d.", slewRate);
                return false;
            }

            SlewRateSP.s = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);
            return true;
        }

        if (!strcmp(name, GuideRateSP.name))
        {
            IUUpdateSwitch(&GuideRateSP, states, names, n);
            int index = IUFindOnSwitchIndex(&GuideRateSP);
            GuideRateSP.s = IPS_OK;
            SetGuideRate(index);
            IDSetSwitch(&GuideRateSP, nullptr);
        }
         // Sites
        if (!strcmp(name, SiteSP.name))
        {
            if (IUUpdateSwitch(&SiteSP, states, names, n) < 0)
                return false;

            currentSiteNum = IUFindOnSwitchIndex(&SiteSP) + 1;
            LOGF_DEBUG("currentSiteNum: %d", currentSiteNum);
            if (!isSimulation() && (!setSite(currentSiteNum)))
            {
                SiteSP.s = IPS_ALERT;
                IDSetSwitch(&SiteSP, "Error selecting sites.");
                return false;
            }

            if (isSimulation())
                IUSaveText(&SiteNameTP.tp[0], "Sample Site");
            else
            {
                LOGF_DEBUG("Site name %s", SiteNameTP.tp[0].text);
                getSiteName(PortFD, SiteNameTP.tp[0].text, currentSiteNum);
            }

            // When user selects a new site, read it from TeenAstro
            getLocation();

            LOGF_INFO("Setting site number %d", currentSiteNum);
            SiteS[currentSiteNum-1].s = ISS_ON;
            SiteNameTP.s = IPS_OK;
            SiteSP.s = IPS_OK;

            IDSetText(&SiteNameTP, nullptr);
            IDSetSwitch(&SiteSP, nullptr);

            return false;
        }
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool LX200_TeenAstro::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, SiteNameTP.name))
        {
            if (!isSimulation() && setSiteName(PortFD, texts[0], currentSiteNum) < 0)
            {
                SiteNameTP.s = IPS_ALERT;
                IDSetText(&SiteNameTP, nullptr);
                return false;
            }

            SiteNameTP.s = IPS_OK;
            IText *tp    = IUFindText(&SiteNameTP, names[0]);
            IUSaveText(tp, texts[0]);
            IDSetText(&SiteNameTP, nullptr);
            return true;
        }
    }

    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}



/*
 * getLocalDate() to sendScopeTime() are copied from lx200telescope.cpp 
 */
bool LX200_TeenAstro::getLocalDate(char *dateString)
{
    if (isSimulation())
    {
        time_t now = time(nullptr);
        strftime(dateString, MAXINDINAME, "%F", localtime(&now));
    }
    else
    {
       getCalendarDate(PortFD, dateString);
    }

    return true;
}

bool LX200_TeenAstro::setLocalDate(uint8_t days, uint8_t months, uint16_t years)
{
    return (setCalenderDate(PortFD, days, months, years) == 0);
}

bool LX200_TeenAstro::getLocalTime(char *timeString)
{
    if (isSimulation())
    {
        time_t now = time(nullptr);
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


bool LX200_TeenAstro::getUTFOffset(double *offset)
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

bool LX200_TeenAstro::sendScopeTime()
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
        snprintf(utcStr, sizeof(utcStr), "%.2f", offset);
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
/*
 * Called by INDI - not sure when 
 */ 

bool LX200_TeenAstro::sendScopeLocation()
{
    int dd = 0, mm = 0, elev = 0;
    double ssf = 0.0;

    LOG_INFO("Send location");
    return true;

    if (isSimulation())
    {
        LocationNP.np[LOCATION_LATITUDE].value = 29.5;  // Kuwait - Jasem's home!
        LocationNP.np[LOCATION_LONGITUDE].value = 48.0;
        LocationNP.np[LOCATION_ELEVATION].value = 10;
        LocationNP.s           = IPS_OK;
        IDSetNumber(&LocationNP, nullptr);
        return true;
    }

    if (getSiteLatitude(PortFD, &dd, &mm, &ssf) < 0)
    {
        LOG_WARN("Failed to get site latitude from device.");
        return false;
    }
    else
    {
        if (dd > 0)
            LocationNP.np[LOCATION_LATITUDE].value = dd + mm / 60.0;
        else
            LocationNP.np[LOCATION_LATITUDE].value = dd - mm / 60.0;
    }
    if (getSiteLongitude(PortFD, &dd, &mm, &ssf) < 0)
    {
        LOG_WARN("Failed to get site longitude from device.");
        return false;
    }
    else
    {
        if (dd > 0)
            LocationNP.np[LOCATION_LONGITUDE].value = 360.0 - (dd + mm / 60.0);
        else
            LocationNP.np[LOCATION_LONGITUDE].value = (dd - mm / 60.0) * -1.0;
    }
    LOGF_DEBUG("Mount Controller Latitude: %g Longitude: %g", LocationN[LOCATION_LATITUDE].value, LocationN[LOCATION_LONGITUDE].value);

    if (getSiteElevation(&elev))
    {
        LocationNP.np[LOCATION_ELEVATION].value = elev;
    }
    else
    {
        LOG_ERROR("Error getting site elevation");
    }
    IDSetNumber(&LocationNP, nullptr);
    saveConfig(true, "GEOGRAPHIC_COORD");

    return true;
}

/*
 * getSiteElevation - not in Meade standard
 */
bool LX200_TeenAstro::getSiteElevation(int *elevationP)
{
    if (getCommandInt(PortFD, elevationP, ":Ge#") !=0)
        return false;
    return true;
}

/*
 * getSiteIndex - not in Meade standard
 */
bool LX200_TeenAstro::getSiteIndex(int *ndxP)
{
    if (getCommandInt(PortFD, ndxP, ":W?#") !=0)
        return false;
    return true;
}


/*
 * getSlewRate - not in Meade standard
 */
bool LX200_TeenAstro::getSlewRate(int *ndxP)
{
    if (getCommandInt(PortFD, ndxP, ":GXRD#") !=0)
        return false;
    return true;
}

/*
 * setSite - not in Meade standard
 * argument is the site number (1 to 4)
 * TeenAstro handles numbers 0 to 3
 */
bool LX200_TeenAstro::setSite(int num)
{
    char buf[10];
    snprintf (buf, sizeof(buf), ":W%d#", num-1);
    sendCommand(buf);
    return true;
}

 /*
  * setSiteElevation - not in Meade standard
  */
bool LX200_TeenAstro::setSiteElevation(double elevation)
{
    char buf[20];
    snprintf (buf, sizeof(buf), ":Se%+4d#", static_cast<int>(elevation));
    sendCommand(buf);
    return true;
}       


/*
 * getLocation
 * retrieve from scope, set into user interface
 */
bool LX200_TeenAstro::getLocation()
{
    int dd = 0, mm = 0, elev = 0;
    double ssf = 0.0;

    if (getSiteLatitude(PortFD, &dd, &mm, &ssf) < 0)
    {
        LOG_WARN("Failed to get site latitude from device.");
        return false;
    }
    else
    {
        if (dd > 0)
            LocationNP.np[LOCATION_LATITUDE].value = dd + mm / 60.0;
        else
            LocationNP.np[LOCATION_LATITUDE].value = dd - mm / 60.0;
    }

    if (getSiteLongitude(PortFD, &dd, &mm, &ssf) < 0)
    {
        LOG_WARN("Failed to get site longitude from device.");
        return false;
    }
    else
    {
        if (dd > 0)
            LocationNP.np[LOCATION_LONGITUDE].value = 360.0 - (dd + mm / 60.0);
        else
            LocationNP.np[LOCATION_LONGITUDE].value = (dd - mm / 60.0) * -1.0;
    }
    if (getSiteElevation(&elev))
    {
        LocationNP.np[LOCATION_ELEVATION].value = elev;
    }
    else
    {
        LOG_ERROR("Error getting site elevation");
    }

    IDSetNumber(&LocationNP, nullptr);
    return true;
}

/*
 * Set Guide Rate -  :SXR0:dddd# (v1.2 and above) where ddd is guide rate * 100
 */
bool LX200_TeenAstro::SetGuideRate(int index)
{
    char cmdString[20];

    snprintf (cmdString, sizeof(cmdString), guideSpeedCommand, GuideRateS[index].name);  // GuideRateS is {25,50,100}
    sendCommand(cmdString);

    return true;
}


/*
 *  Guidexxx - use SendPulseCmd function from lx200driver.cpp
 */
IPState LX200_TeenAstro::GuideNorth(uint32_t ms)
{
    SendPulseCmd(LX200_NORTH, ms);
    return IPS_OK;
}

IPState LX200_TeenAstro::GuideSouth(uint32_t ms)
{
    SendPulseCmd(LX200_SOUTH, ms);
    return IPS_OK;
}

IPState LX200_TeenAstro::GuideEast(uint32_t ms)
{
    SendPulseCmd(LX200_EAST, ms);
    return IPS_OK;
}

IPState LX200_TeenAstro::GuideWest(uint32_t ms)
{
    SendPulseCmd(LX200_WEST, ms);
    return IPS_OK;
}

void LX200_TeenAstro::SendPulseCmd(int8_t direction, uint32_t duration_msec)
{
    ::SendPulseCmd(PortFD, direction, duration_msec);
}


/*
 * Abort() calls standard lx200driver command (:Q#)
 */
bool LX200_TeenAstro::Abort()
{
    if (!isSimulation() && abortSlew(PortFD) < 0)
    {
        LOG_ERROR("Failed to abort slew.");
        return false;
    }

    EqNP.s     = IPS_IDLE;
    TrackState = SCOPE_IDLE;
    IDSetNumber(&EqNP, nullptr);

    LOG_INFO("Slew aborted.");
    return true;
}

/*
 * MoveNS and MoveWE call lx200telescope functions
 */
bool LX200_TeenAstro::MoveNS(INDI_DIR_NS dirns, TelescopeMotionCommand cmd)
{
    if (dirns == DIRECTION_NORTH)
        return Move(LX200_NORTH, cmd);
    else
        return Move(LX200_SOUTH, cmd);
}
bool LX200_TeenAstro::MoveWE(INDI_DIR_WE dirwe, TelescopeMotionCommand cmd)
{
    if (dirwe == DIRECTION_WEST)
        return Move(LX200_WEST, cmd);
    else
        return Move(LX200_EAST, cmd);
}

/*
 * Single function for move - use LX200 functions
 */
bool LX200_TeenAstro::Move(TDirection dir, TelescopeMotionCommand cmd)
{
    switch (cmd)
    {
        case MOTION_START: MoveTo(PortFD, dir);         break;
        case MOTION_STOP:  HaltMovement(PortFD, dir);   break;
    }
    return true;
}

/*
 * Override default config saving
 */
bool LX200_TeenAstro::saveConfigItems(FILE *fp)
{
    IUSaveConfigSwitch(fp, &SlewRateSP);
    IUSaveConfigSwitch(fp, &GuideRateSP);

    return INDI::Telescope::saveConfigItems(fp);
}


/*
 * Mount simulation
 */
void LX200_TeenAstro::mountSim()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt, da, dx;
    int nlocked;

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday(&tv, nullptr);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt  = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec) / 1e6;
    ltv = tv;
    da  = SLEWRATE * dt;

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly */
    switch (TrackState)
    {
        case SCOPE_TRACKING:
            /* RA moves at sidereal, Dec stands still */
            currentRA += (SIDRATE * dt / 15.);
            break;

        case SCOPE_SLEWING:
            /* slewing - nail it when both within one pulse @ SLEWRATE */
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
                TrackState = SCOPE_TRACKING;
            }

            break;

        default:
            break;
    }

    NewRaDec(currentRA, currentDEC);
}

void LX200_TeenAstro::slewError(int slewCode)
{
    EqNP.s = IPS_ALERT;

    if (slewCode == 1)
        IDSetNumber(&EqNP, "Object below horizon.");
    else if (slewCode == 2)
        IDSetNumber(&EqNP, "Object below the minimum elevation limit.");
    else
        IDSetNumber(&EqNP, "Slew failed.");
}
/*
 *  Enable or disable sidereal tracking (events handled by inditelescope) 
 */
bool LX200_TeenAstro::SetTrackEnabled(bool enabled)
{
    LOGF_INFO("TrackEnable %d", enabled);

    if (enabled)
    {
        sendCommand(":Te#");
    }
    else
    {
        sendCommand(":Td#");
    }
    return true;
}

/*
 * selectSlewrate - select among TeenAstro's 5 predefined rates
 */
bool LX200_TeenAstro::selectSlewRate(int index)
{
    char cmd[20];

    snprintf(cmd, sizeof(cmd), ":SXRD:%d#", index);
    sendCommand(cmd);
    return true;
}


/*
 * Used instead of getCommandString when response is not terminated with '#'
 * 
 */
void LX200_TeenAstro::sendCommand(const char *cmd)
{
    char resp;
    int nbytes_read;
    std::unique_lock<std::mutex> guard(lx200CommsLock);
    LOGF_INFO("sendCommand %s", cmd);
    int rc = write(PortFD, cmd, strlen(cmd));
    rc = tty_read(PortFD, &resp, 1, ONSTEP_TIMEOUT, &nbytes_read);
    INDI_UNUSED(rc);
}



