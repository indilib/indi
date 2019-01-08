#if 0
Celestron GPS
Copyright (C) 2003-2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

Version with experimental pulse guide support. GC 04.12.2015

#endif

#include "celestrongps.h"

#include "indicom.h"

#include <libnova/transform.h>

#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>

// Simulation Parameters
#define GOTO_RATE       5        // slew rate, degrees/s
#define SLEW_RATE       0.5      // slew rate, degrees/s
#define FINE_SLEW_RATE  0.1      // slew rate, degrees/s
#define GOTO_LIMIT      5.5      // Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees
#define SLEW_LIMIT      1        // Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees
#define FINE_SLEW_LIMIT 0.5      // Move at FINE_SLEW_RATE until distance from target is FINE_SLEW_LIMIT degrees

#define MOUNTINFO_TAB "Mount Info"

static std::unique_ptr<CelestronGPS> telescope(new CelestronGPS());

void ISGetProperties(const char *dev)
{
    telescope->ISGetProperties(dev);
}
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    telescope->ISNewSwitch(dev, name, states, names, n);
}
void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    telescope->ISNewText(dev, name, texts, names, n);
}
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    telescope->ISNewNumber(dev, name, values, names, n);
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
    telescope->ISSnoopDevice(root);
}

CelestronGPS::CelestronGPS()
{
    setVersion(3, 2);


    fwInfo.Version           = "Invalid";
    fwInfo.controllerVersion = 0;
    fwInfo.controllerVariant = ISNEXSTAR;
    fwInfo.isGem = false;

    INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    currentRA  = 0;
    currentDEC = 90;
    currentAZ  = 0;
    currentALT = 0;
    targetAZ   = 0;
    targetALT  = 0;
}

bool CelestronGPS::checkMinVersion(float minVersion, const char *feature)
{
    if (((fwInfo.controllerVariant == ISSTARSENSE) &&
         (fwInfo.controllerVersion < MINSTSENSVER)) ||
        ((fwInfo.controllerVariant == ISNEXSTAR) &&
         (fwInfo.controllerVersion < minVersion)))
    {
        LOGF_WARN("Firmware v%3.1f does not support %s. Minimun required version is %3.1f",
                fwInfo.controllerVersion, feature, minVersion);
        return false;
    }
    return true;
}

const char *CelestronGPS::getDefaultName()
{
    return static_cast<const char *>("Celestron GPS");
}

bool CelestronGPS::initProperties()
{
    INDI::Telescope::initProperties();

    // Firmware
    IUFillText(&FirmwareT[FW_MODEL], "Model", "", nullptr);
    IUFillText(&FirmwareT[FW_VERSION], "Version", "", nullptr);
    IUFillText(&FirmwareT[FW_GPS], "GPS", "", nullptr);
    IUFillText(&FirmwareT[FW_RA], "RA", "", nullptr);
    IUFillText(&FirmwareT[FW_DEC], "DEC", "", nullptr);
    IUFillTextVector(&FirmwareTP, FirmwareT, 5, getDeviceName(), "Firmware Info", "", MOUNTINFO_TAB, IP_RO, 0,
                     IPS_IDLE);

    AddTrackMode("TRACK_ALTAZ", "Alt/Az");
    AddTrackMode("TRACK_EQN", "Eq North", true);
    AddTrackMode("TRACK_EQS", "Eq South");

    IUFillSwitch(&UseHibernateS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&UseHibernateS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&UseHibernateSP, UseHibernateS, 2, getDeviceName(), "Hibernate", "", OPTIONS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    //GUIDE Define "Use Pulse Cmd" property (Switch).
    IUFillSwitch(&UsePulseCmdS[0], "Off", "", ISS_OFF);
    IUFillSwitch(&UsePulseCmdS[1], "On", "", ISS_ON);
    IUFillSwitchVector(&UsePulseCmdSP, UsePulseCmdS, 2, getDeviceName(), "Use Pulse Cmd", "", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    SetParkDataType(PARK_AZ_ALT);

    //GUIDE Initialize guiding properties.
    initGuiderProperties(getDeviceName(), GUIDE_TAB);

    addAuxControls();

    //GUIDE Set guider interface.
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    return true;
}

void CelestronGPS::ISGetProperties(const char *dev)
{
    static bool configLoaded = false;

    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    INDI::Telescope::ISGetProperties(dev);

    defineSwitch(&UseHibernateSP);
    if (configLoaded == false)
    {
        configLoaded = true;
        loadConfig(true, "Hibernate");
    }

    /*
    if (isConnected())
    {
        //defineNumber(&HorizontalCoordsNP);
        defineSwitch(&SlewRateSP);
        //defineSwitch(&TrackSP);

        //GUIDE Define guiding properties
        defineSwitch(&UsePulseCmdSP);
        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);

        if (fwInfo.Version != "Invalid")
            defineText(&FirmwareTP);
    }
    */
}

bool CelestronGPS::updateProperties()
{
    if (isConnected())
    {
        uint32_t cap = TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT;

        if (driver.get_firmware(&fwInfo))
        {
            IUSaveText(&FirmwareT[FW_MODEL], fwInfo.Model.c_str());
            IUSaveText(&FirmwareT[FW_VERSION], fwInfo.Version.c_str());
            IUSaveText(&FirmwareT[FW_GPS], fwInfo.GPSFirmware.c_str());
            IUSaveText(&FirmwareT[FW_RA], fwInfo.RAFirmware.c_str());
            IUSaveText(&FirmwareT[FW_DEC], fwInfo.DEFirmware.c_str());

            //usePreciseCoords = (fwInfo.controllerVersion > 2.2);
            usePreciseCoords = (checkMinVersion(2.2, "usePreciseCoords"));
        }
        else
        {
            fwInfo.Version = "Invalid";
            LOG_WARN("Failed to retrive firmware information.");
        }

        // Since issues have been observed with Starsense, enabe parking only with Nexstar controller
//        if (fwInfo.controllerVariant == ISSTARSENSE)
//        {
//            if (fwInfo.controllerVersion >= MINSTSENSVER)
//                LOG_INFO("Starsense controller detected.");
//            else
//                LOGF_WARN("Starsense controller detected, but firmware is too old. "
//                        "Current version is %4.2f, but minimum required version is %4.2f. "
//                        "Please update your Starsense firmware.",
//                        fwInfo.controllerVersion, MINSTSENSVER);
//        }
//        else
//            cap |= TELESCOPE_CAN_PARK;

        // JM 2018-09-28: According to user reports in this thread:
        // http://www.indilib.org/forum/mounts/2208-celestron-avx-mount-and-starsense.html
        // Parking is also supported fine with StarSense
        if (checkMinVersion(2.3, "park"))
            cap |= TELESCOPE_CAN_PARK;

        if (checkMinVersion(4.1, "sync"))
            cap |= TELESCOPE_CAN_SYNC;

        if (checkMinVersion(2.3, "updating time and location settings"))
            cap |= TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION;

        // StarSense supports track mode
        if (checkMinVersion(2.3, "track control"))
            cap |= TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK;
        else
            LOG_WARN("Mount firmware does not support track mode.");

        if (fwInfo.isGem  && checkMinVersion(4.15, "Pier Side"))
            cap |= TELESCOPE_HAS_PIER_SIDE;
        else
            LOG_WARN("Mount firmware does not support getting pier side.");

        SetTelescopeCapability(cap, 9);

        INDI::Telescope::updateProperties();

        if (fwInfo.Version != "Invalid")
            defineText(&FirmwareTP);

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationN[LOCATION_LATITUDE].value);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis2Park(LocationN[LOCATION_LATITUDE].value);
            SetAxis1ParkDefault(LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationN[LOCATION_LATITUDE].value);
        }

        //GUIDE Update properties.
        defineSwitch(&UsePulseCmdSP);
        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);

        // Track Mode (t) is only supported for 2.3+
        if (checkMinVersion(2.3, "track mode"))
        {
            CELESTRON_TRACK_MODE mode;
            if (isSimulation())
            {
                if (isParked())
                    driver.set_sim_track_mode(TRACKING_OFF);
                else
                    driver.set_sim_track_mode(TRACK_EQN);
            }
            if (driver.get_track_mode(&mode))
            {
                if (mode != TRACKING_OFF)
                {
                    //IUResetSwitch(&TrackSP);
                    IUResetSwitch(&TrackModeSP);
                    TrackModeS[mode-1].s = ISS_ON;
                    TrackModeSP.s      = IPS_OK;

                    // If tracking is ON then mount is NOT parked
                    if (isParked())
                        SetParked(false);

                    TrackState = SCOPE_TRACKING;
                }
                else
                {
                    LOG_INFO("Mount tracking is off.");
                    TrackState = isParked() ? SCOPE_PARKED : SCOPE_IDLE;
                }
            }
            else
                TrackModeSP.s = IPS_ALERT;

            IDSetSwitch(&TrackModeSP, nullptr);
        }

        // JM 2014-04-14: User (davidw) reported AVX mount serial communication times out issuing "h" command with firmware 5.28
        // JM 2018-09-27: User (suramara) reports that it works with AVX mount with Star Sense firmware version 1.19
        //if (fwInfo.controllerVersion >= 2.3 && fwInfo.Model != "AVX" && fwInfo.Model != "CGE Pro")
        if (checkMinVersion(2.3, "date and time setting"))
        {
            double utc_offset;
            int yy, dd, mm, hh, minute, ss;
            if (driver.get_utc_date_time(&utc_offset, &yy, &mm, &dd, &hh, &minute, &ss))
            {
                char isoDateTime[32];
                char utcOffset[8];

                snprintf(isoDateTime, 32, "%04d-%02d-%02dT%02d:%02d:%02d", yy, mm, dd, hh, minute, ss);
                snprintf(utcOffset, 8, "%4.2f", utc_offset);

                IUSaveText(IUFindText(&TimeTP, "UTC"), isoDateTime);
                IUSaveText(IUFindText(&TimeTP, "OFFSET"), utcOffset);

                LOGF_INFO("Mount UTC offset is %s. UTC time is %s", utcOffset, isoDateTime);
                LOGF_DEBUG("Mount UTC offset is %s. UTC time is %s", utcOffset, isoDateTime);

                TimeTP.s = IPS_OK;
                IDSetText(&TimeTP, nullptr);
            }
        }
        else
            LOG_WARN("Mount does not support retrieval of date and time settings.");


        // Sometimes users start their mount when it is NOT yet aligned and then try to proceed to use it
        // So we check issue and issue error if not aligned.
        checkAlignment();
    }
    else
    {
        INDI::Telescope::updateProperties();

        //GUIDE Delete properties.
        deleteProperty(UsePulseCmdSP.name);
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);

        //deleteProperty(TrackSP.name);
        if (fwInfo.Version != "Invalid")
            deleteProperty(FirmwareTP.name);
    }

    return true;
}

bool CelestronGPS::Goto(double ra, double dec)
{
    targetRA  = ra;
    targetDEC = dec;

    if (EqNP.s == IPS_BUSY || MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
    {
        driver.abort();
        // sleep for 500 mseconds
        usleep(500000);
    }

    if (driver.slew_radec(targetRA, targetDEC, usePreciseCoords) == false)
    {
        LOG_ERROR("Failed to slew telescope in RA/DEC.");
        return false;
    }

    //HorizontalCoordsNP.s = IPS_BUSY;

    TrackState = SCOPE_SLEWING;

    char RAStr[32], DecStr[32];
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);
    LOGF_INFO("Slewing to JNOW RA %s - DEC %s", RAStr, DecStr);

    return true;
}

bool CelestronGPS::Sync(double ra, double dec)
{
    if (!checkMinVersion(4.1, "sync"))
        return false;

    if (driver.sync(ra, dec, usePreciseCoords) == false)
    {
        LOG_ERROR("Sync failed.");
        return false;
    }

    currentRA  = ra;
    currentDEC = dec;

    LOG_INFO("Sync successful.");

    return true;
}

/*
bool CelestronGPS::GotoAzAlt(double az, double alt)
{
    if (isSimulation())
    {
        ln_hrz_posn horizontalPos;
        // Libnova south = 0, west = 90, north = 180, east = 270
        horizontalPos.az = az + 180;
        if (horizontalPos.az >= 360)
             horizontalPos.az -= 360;
        horizontalPos.alt = alt;

        ln_lnlat_posn observer;

        observer.lat = LocationN[LOCATION_LATITUDE].value;
        observer.lng = LocationN[LOCATION_LONGITUDE].value;

        if (observer.lng > 180)
            observer.lng -= 360;

        ln_equ_posn equatorialPos;
        ln_get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);

        targetRA  = equatorialPos.ra/15.0;
        targetDEC = equatorialPos.dec;
    }

    if (driver.slew_azalt(LocationN[LOCATION_LATITUDE].value, az, alt) == false)
    {
        LOG_ERROR("Failed to slew telescope in Az/Alt.");
        return false;
    }

    targetAZ = az;
    targetALT= alt;

    TrackState = SCOPE_SLEWING;

    HorizontalCoordsNP.s = IPS_BUSY;

    char AZStr[16], ALTStr[16];
    fs_sexa(AZStr, targetAZ, 3, 3600);
    fs_sexa(ALTStr, targetALT, 2, 3600);
    LOGF_INFO("Slewing to Az %s - Alt %s", AZStr, ALTStr);

    return true;
}
*/

bool CelestronGPS::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    CELESTRON_DIRECTION move;

    if (currentPierSide == PIER_WEST)
        move = (dir == DIRECTION_NORTH) ? CELESTRON_N : CELESTRON_S;
    else
        move = (dir == DIRECTION_NORTH) ? CELESTRON_S : CELESTRON_N;

    CELESTRON_SLEW_RATE rate = static_cast<CELESTRON_SLEW_RATE>(IUFindOnSwitchIndex(&SlewRateSP));

    switch (command)
    {
        case MOTION_START:
            if (driver.start_motion(move, rate) == false)
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s.", (move == CELESTRON_N) ? "North" : "South");
            break;

        case MOTION_STOP:
            if (driver.stop_motion(move) == false)
            {
                LOG_ERROR("Error stopping N/S motion.");
                return false;
            }
            else
                LOGF_INFO("Movement toward %s halted.", (move == CELESTRON_N) ? "North" : "South");
            break;
    }

    return true;
}

bool CelestronGPS::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    CELESTRON_DIRECTION move = (dir == DIRECTION_WEST) ? CELESTRON_W : CELESTRON_E;
    CELESTRON_SLEW_RATE rate = static_cast<CELESTRON_SLEW_RATE>(IUFindOnSwitchIndex(&SlewRateSP));

    switch (command)
    {
        case MOTION_START:
            if (driver.start_motion(move, rate) == false)
            {
                LOG_ERROR("Error setting W/E motion direction.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s.", (move == CELESTRON_W) ? "West" : "East");
            break;

        case MOTION_STOP:
            if (driver.stop_motion(move) == false)
            {
                LOG_ERROR("Error stopping W/E motion.");
                return false;
            }
            else
                LOGF_INFO("Movement toward %s halted.", (move == CELESTRON_W) ? "West" : "East");
            break;
    }

    return true;
}

bool CelestronGPS::ReadScopeStatus()
{
    if (isSimulation())
        mountSim();

    if (driver.get_radec(&currentRA, &currentDEC, usePreciseCoords) == false)
    {
        LOG_ERROR("Failed to read RA/DEC values.");
        return false;
    }

    /*if (driver.get_coords_azalt(LocationN[LOCATION_LATITUDE].value, &currentAZ, &currentALT) == false)
        LOG_WARN("Failed to read AZ/ALT values.");
    else
    {
        HorizontalCoordsN[AXIS_AZ].value  = currentAZ;
        HorizontalCoordsN[AXIS_ALT].value = currentALT;
    }*/

    switch (TrackState)
    {
        case SCOPE_SLEWING:
            // are we done?
            if (driver.is_slewing() == false)
            {
                LOG_INFO("Slew complete, tracking...");
                TrackState = SCOPE_TRACKING;
                //HorizontalCoordsNP.s = IPS_OK;
            }
            break;

        case SCOPE_PARKING:
            // are we done?
            if (driver.is_slewing() == false)
            {
                if (driver.set_track_mode(TRACKING_OFF))
                    LOG_DEBUG("Mount tracking is off.");

                SetParked(true);
                //HorizontalCoordsNP.s = IPS_OK;

                saveConfig(true);

                // Check if we need to hibernate
                if (UseHibernateS[0].s == ISS_ON)
                {
                    LOG_INFO("Hibernating mount...");
                    if (driver.hibernate())
                        LOG_INFO("Mount hibernated. Please disconnect now and turn off your mount.");
                    else
                        LOG_ERROR("Hibernating mount failed!");
                }
            }
            break;

        default:
            break;
    }

    //IDSetNumber(&HorizontalCoordsNP, nullptr);
    NewRaDec(currentRA, currentDEC);

    if (!HasPierSide())
        return true;

    char sop;
    INDI::Telescope::TelescopePierSide pierSide = PIER_UNKNOWN;
    char psc = 'U';
    if (driver.get_pier_side(&sop))
    {
        // manage version and hemisphere nonsense
        // HC versions less than 5.24 reverse the side of pier if the mount
        // is in the Southern hemisphere.  StarSense doesn't
        if (LocationN[LOCATION_LATITUDE].value < 0)
        {
            if (fwInfo.controllerVersion <= 5.24 && fwInfo.controllerVariant != ISSTARSENSE)
            {
                // swap the char reported
                if (sop == 'E')
                    sop = 'W';
                else if (sop == 'W')
                    sop = 'E';
            }
        }
        // The Celestron and INDI pointing states are opposite
        if (sop == 'W')
        {
            pierSide = PIER_EAST;
            psc = 'E';
        }
        else if (sop == 'E')
        {
            pierSide = PIER_WEST;
            psc = 'W';
        }
    }

    LOGF_DEBUG("latitude %g, sop %c, PierSide %c",
               LocationN[LOCATION_LATITUDE].value,
               sop, psc);
    setPierSide(pierSide);

    return true;
}

bool CelestronGPS::Abort()
{
    driver.stop_motion(CELESTRON_N);
    driver.stop_motion(CELESTRON_S);
    driver.stop_motion(CELESTRON_W);
    driver.stop_motion(CELESTRON_E);

    //GUIDE Abort guide operations.
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
            GuideWETID = 0;
        }

        LOG_INFO("Guide aborted.");
        IDSetNumber(&GuideNSNP, nullptr);
        IDSetNumber(&GuideWENP, nullptr);

        return true;
    }

    return driver.abort();
}

bool CelestronGPS::Handshake()
{
    driver.set_device(getDeviceName());
    driver.set_port_fd(PortFD);

    if (isSimulation())
    {
        driver.set_simulation(true);
        driver.set_sim_slew_rate(SR_5);
        driver.set_sim_ra(0);
        driver.set_sim_dec(90);
    }

    bool parkDataValid = (LoadParkData() == nullptr);
    // Check if we need to wake up IF:
    // 1. Park data exists in ParkData.xml
    // 2. Mount is currently parked
    // 3. Hibernate option is enabled
    if (parkDataValid && isParked() && UseHibernateS[0].s == ISS_ON)
    {
        LOG_INFO("Waking up mount...");
        if (!driver.wakeup())
        {
            LOG_ERROR("Waking up mount failed! Make sure mount is powered and connected. "
                  "Hibernate requires firmware version >= 5.21");
            return false;
        }
    }

    if (driver.check_connection() == false)
    {
        LOG_ERROR("Failed to communicate with the mount, check the logs for details.");
        return false;
    }

    return true;
}

bool CelestronGPS::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(getDeviceName(), dev))
    {
        // Enable/Disable hibernate
        if (!strcmp(name, UseHibernateSP.name))
        {
            IUUpdateSwitch(&UseHibernateSP, states, names, n);
            if (UseHibernateS[0].s == ISS_ON && checkMinVersion(4.22, "hibernation") == false)
            {
                UseHibernateS[0].s = ISS_OFF;
                UseHibernateS[1].s = ISS_ON;
                UseHibernateSP.s = IPS_ALERT;
            }
            else
                UseHibernateSP.s = IPS_OK;
            IDSetSwitch(&UseHibernateSP, nullptr);
            return true;
        }

        //GUIDE Pulse-Guide command support
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

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool CelestronGPS::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //double newAlt=0, newAz=0;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /*if ( !strcmp (name, HorizontalCoordsNP.name) )
        {
            int i=0, nset=0;

              for (nset = i = 0; i < n; i++)
              {
                  INumber *horp = IUFindNumber (&HorizontalCoordsNP, names[i]);
                  if (horp == &HorizontalCoordsN[AXIS_AZ])
                  {
                      newAz = values[i];
                      nset += newAz >= 0. && newAz <= 360.0;

                  } else if (horp == &HorizontalCoordsN[AXIS_ALT])
                  {
                      newAlt = values[i];
                      nset += newAlt >= -90. && newAlt <= 90.0;
                  }
              }

            if (nset == 2)
            {
                char AzStr[16], AltStr[16];
                fs_sexa(AzStr, newAz, 3, 3600);
                fs_sexa(AltStr, newAlt, 2, 3600);

             if (GotoAzAlt(newAz, newAlt) == false)
             {
                 HorizontalCoordsNP.s = IPS_ALERT;
                 LOGF_ERROR("Error slewing to Az: %s Alt: %s", AzStr, AltStr);
                 IDSetNumber(&HorizontalCoordsNP, nullptr);
                 return false;
             }

             return true;

            }
            else
            {
              HorizontalCoordsNP.s = IPS_ALERT;
              LOG_ERROR("Altitude or Azimuth missing or invalid");
              IDSetNumber(&HorizontalCoordsNP, nullptr);
              return false;
            }
        }*/

        //GUIDE process Guider properties.
        processGuiderProperties(name, values, names, n);
    }

    INDI::Telescope::ISNewNumber(dev, name, values, names, n);
    return true;
}

void CelestronGPS::mountSim()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt, dx, da_ra = 0, da_dec = 0;
    int nlocked;

    // update elapsed time since last poll, don't presume exactly POLLMS
    gettimeofday(&tv, nullptr);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt  = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec) / 1e6;
    ltv = tv;

    if (fabs(targetRA - currentRA) * 15. >= GOTO_LIMIT)
        da_ra = GOTO_RATE * dt;
    else if (fabs(targetRA - currentRA) * 15. >= SLEW_LIMIT)
        da_ra = SLEW_RATE * dt;
    else
        da_ra = FINE_SLEW_RATE * dt;

    if (fabs(targetDEC - currentDEC) >= GOTO_LIMIT)
        da_dec = GOTO_RATE * dt;
    else if (fabs(targetDEC - currentDEC) >= SLEW_LIMIT)
        da_dec = SLEW_RATE * dt;
    else
        da_dec = FINE_SLEW_RATE * dt;

    if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
    {
        int rate = IUFindOnSwitchIndex(&SlewRateSP);

        switch (rate)
        {
            case SLEW_GUIDE:
                da_ra  = FINE_SLEW_RATE * dt * 0.05;
                da_dec = FINE_SLEW_RATE * dt * 0.05;
                break;

            case SLEW_CENTERING:
                da_ra  = FINE_SLEW_RATE * dt * .1;
                da_dec = FINE_SLEW_RATE * dt * .1;
                break;

            case SLEW_FIND:
                da_ra  = SLEW_RATE * dt;
                da_dec = SLEW_RATE * dt;
                break;

            default:
                da_ra  = GOTO_RATE * dt;
                da_dec = GOTO_RATE * dt;
                break;
        }

        switch (MovementNSSP.s)
        {
            case IPS_BUSY:
                if (MovementNSS[DIRECTION_NORTH].s == ISS_ON)
                    currentDEC += da_dec;
                else if (MovementNSS[DIRECTION_SOUTH].s == ISS_ON)
                    currentDEC -= da_dec;
                break;

            default:
                break;
        }

        switch (MovementWESP.s)
        {
            case IPS_BUSY:
                if (MovementWES[DIRECTION_WEST].s == ISS_ON)
                    currentRA += da_ra / 15.;
                else if (MovementWES[DIRECTION_EAST].s == ISS_ON)
                    currentRA -= da_ra / 15.;
                break;

            default:
                break;
        }

        driver.set_sim_ra(currentRA);
        driver.set_sim_dec(currentDEC);

        /*ln_equ_posn equatorialPos;
        equatorialPos.ra  = currentRA * 15;
        equatorialPos.dec = currentDEC;

        ln_lnlat_posn observer;

        observer.lat = LocationN[LOCATION_LATITUDE].value;
        observer.lng = LocationN[LOCATION_LONGITUDE].value;

        if (observer.lng > 180)
            observer.lng -= 360;

        ln_hrz_posn horizontalPos;

        ln_get_hrz_from_equ(&equatorialPos, &observer, ln_get_julian_from_sys(), &horizontalPos);

        // Libnova south = 0, west = 90, north = 180, east = 270
        horizontalPos.az -= 180;
        if (horizontalPos.az < 0)
            horizontalPos.az += 360;

        set_sim_az(horizontalPos.az);
        set_sim_alt(horizontalPos.alt);*/

        NewRaDec(currentRA, currentDEC);
        return;
    }

    // Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly
    switch (TrackState)
    {
        case SCOPE_IDLE:
            currentRA = driver.get_sim_ra() + (TRACKRATE_SIDEREAL/3600.0 * dt) / 15.0;
            currentRA = range24(currentRA);
            break;

        case SCOPE_SLEWING:
        case SCOPE_PARKING:
            // slewing - nail it when both within one pulse @ SLEWRATE
            nlocked = 0;

            dx = targetRA - currentRA;

            // Take shortest path
            if (fabs(dx) > 12)
                dx *= -1;

            if (fabs(dx) <= da_ra)
            {
                currentRA = targetRA;
                nlocked++;
            }
            else if (dx > 0)
                currentRA += da_ra / 15.;
            else
                currentRA -= da_ra / 15.;

            if (currentRA < 0)
                currentRA += 24;
            else if (currentRA > 24)
                currentRA -= 24;

            dx = targetDEC - currentDEC;
            if (fabs(dx) <= da_dec)
            {
                currentDEC = targetDEC;
                nlocked++;
            }
            else if (dx > 0)
                currentDEC += da_dec;
            else
                currentDEC -= da_dec;

            if (nlocked == 2)
            {
                driver.set_sim_slewing(false);
            }

            break;

        default:
            break;
    }

    driver.set_sim_ra(currentRA);
    driver.set_sim_dec(currentDEC);

    /* ln_equ_posn equatorialPos;
     equatorialPos.ra  = currentRA * 15;
     equatorialPos.dec = currentDEC;

     ln_lnlat_posn observer;

     observer.lat = LocationN[LOCATION_LATITUDE].value;
     observer.lng = LocationN[LOCATION_LONGITUDE].value;

     if (observer.lng > 180)
         observer.lng -= 360;

     ln_hrz_posn horizontalPos;

     ln_get_hrz_from_equ(&equatorialPos, &observer, ln_get_julian_from_sys(), &horizontalPos);

     // Libnova south = 0, west = 90, north = 180, east = 270
     horizontalPos.az -= 180;
     if (horizontalPos.az < 0)
         horizontalPos.az += 360;

     set_sim_az(horizontalPos.az);
     set_sim_alt(horizontalPos.alt);*/
}

void CelestronGPS::simulationTriggered(bool enable)
{
    driver.set_simulation(enable);
}

bool CelestronGPS::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (!checkMinVersion(2.3, "updating location"))
        return false;

    return driver.set_location(longitude, latitude);
}

bool CelestronGPS::updateTime(ln_date *utc, double utc_offset)
{
    if (!checkMinVersion(2.3, "updating time"))
        return false;

    return (driver.set_datetime(utc, utc_offset));
}

bool CelestronGPS::Park()
{
    double parkAZ  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Parking to Az (%s) Alt (%s)...", AzStr, AltStr);

    if (driver.slew_azalt(parkAZ, parkAlt, usePreciseCoords))
    {
        TrackState = SCOPE_PARKING;
        LOG_INFO("Parking is in progress...");
        return true;
    }

    return false;

#if 0
    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270
    horizontalPos.az = parkAZ + 180;
    if (horizontalPos.az >= 360)
        horizontalPos.az -= 360;
    horizontalPos.alt = parkAlt;

    ln_lnlat_posn observer;

    observer.lat = LocationN[LOCATION_LATITUDE].value;
    observer.lng = LocationN[LOCATION_LONGITUDE].value;

    if (observer.lng > 180)
        observer.lng -= 360;

    ln_equ_posn equatorialPos;

    ln_get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);

    char RAStr[16], DEStr[16];
    fs_sexa(RAStr, equatorialPos.ra / 15.0, 2, 3600);
    fs_sexa(DEStr, equatorialPos.dec, 2, 3600);
    LOGF_DEBUG("Parking to RA (%s) DEC (%s)...", RAStr, DEStr);

    if (Goto(equatorialPos.ra / 15.0, equatorialPos.dec))
    {
        TrackState = SCOPE_PARKING;
        LOG_INFO("Parking is in progress...");

        return true;
    }
    else
        return false;
#endif
}

bool CelestronGPS::UnPark()
{
    // Set tracking mode to whatever it was stored before
    SetParked(false);
    loadConfig(true, "TELESCOPE_TRACK_MODE");
    return true;

#if 0
    double parkAZ  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Unparking from Az (%s) Alt (%s)...", AzStr, AltStr);

    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270
    horizontalPos.az = parkAZ + 180;
    if (horizontalPos.az >= 360)
        horizontalPos.az -= 360;
    horizontalPos.alt = parkAlt;

    ln_lnlat_posn observer;

    observer.lat = LocationN[LOCATION_LATITUDE].value;
    observer.lng = LocationN[LOCATION_LONGITUDE].value;

    if (observer.lng > 180)
        observer.lng -= 360;

    ln_equ_posn equatorialPos;

    ln_get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);

    char RAStr[16], DEStr[16];
    fs_sexa(RAStr, equatorialPos.ra / 15.0, 2, 3600);
    fs_sexa(DEStr, equatorialPos.dec, 2, 3600);
    LOGF_DEBUG("Syncing to parked coordinates RA (%s) DEC (%s)...", RAStr, DEStr);

    if (Sync(equatorialPos.ra / 15.0, equatorialPos.dec))
    {
        SetParked(false);
        loadConfig(true, "TELESCOPE_TRACK_MODE");
        return true;
    }
    else
        return false;
#endif
}

bool CelestronGPS::SetCurrentPark()
{
    // The Goto Alt-Az and Get Alt-Az menu items have been renamed Goto Axis Postn and Get Axis Postn
    // where Postn is an abbreviation for Position. Since this feature doesn't actually refer
    // to altitude and azimuth when mounted on a wedge, the new designation is more accurate.
    // Source  : NexStarHandControlVersion4UsersGuide.pdf

    /*ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270

    ln_lnlat_posn observer;
    observer.lat = LocationN[LOCATION_LATITUDE].value;
    observer.lng = LocationN[LOCATION_LONGITUDE].value;
    if (observer.lng > 180)
        observer.lng -= 360;

    ln_equ_posn equatorialPos;
    equatorialPos.ra  = currentRA * 15;
    equatorialPos.dec = currentDEC;
    ln_get_hrz_from_equ(&equatorialPos, &observer, ln_get_julian_from_sys(), &horizontalPos);

    double parkAZ = horizontalPos.az - 180;
    if (parkAZ < 0)
        parkAZ += 360;
    double parkAlt = horizontalPos.alt;*/

    if (driver.get_azalt(&currentAZ, &currentALT, usePreciseCoords) == false)
    {
        LOG_ERROR("Failed to read AZ/ALT values.");
        return false;
    }

    double parkAZ = currentAZ;
    double parkAlt = currentALT;

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);

    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr,
           AltStr);

    SetAxis1Park(parkAZ);
    SetAxis2Park(parkAlt);

    return true;
}

bool CelestronGPS::SetDefaultPark()
{
    // The Goto Alt-Az and Get Alt-Az menu items have been renamed Goto Axis Postn and Get Axis Postn
    // where Postn is an abbreviation for Position. Since this feature doesn't actually refer
    // to altitude and azimuth when mounted on a wedge, the new designation is more accurate.
    // Source  : NexStarHandControlVersion4UsersGuide.pdf

    // By default azimuth 90° ( hemisphere doesn't matter)
    SetAxis1Park(90);

    // Altitude = 90° (latitude doesn't matter)
    SetAxis2Park(90);

    return true;
}

bool CelestronGPS::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &UseHibernateSP);
    //IUSaveConfigSwitch(fp, &TrackSP);
    IUSaveConfigSwitch(fp, &UsePulseCmdSP);

    return true;
}

bool CelestronGPS::setTrackMode(CELESTRON_TRACK_MODE mode)
{
    if (driver.set_track_mode(mode))
    {
        TrackState = (mode == TRACKING_OFF) ? SCOPE_IDLE : SCOPE_TRACKING;
        LOGF_DEBUG("Tracking mode set to %s.", TrackModeS[mode - 1].label);
        return true;
    }

    return false;
}

//GUIDE Guiding functions.
IPState CelestronGPS::GuideNorth(uint32_t ms)
{
    LOGF_DEBUG("GUIDE CMD: N %.0f ms", ms);
    if (!usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }

    // If already moving (no pulse command), then stop movement
    if (MovementNSSP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementNSSP);
        MoveNS(dir == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP);
    }

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if (usePulseCommand)
    {
        driver.send_pulse(CELESTRON_N, 50, ms / 10.0);
    }
    else
    {
        MovementNSS[0].s = ISS_ON;
        MoveNS(DIRECTION_NORTH, MOTION_START);
    }

    // Set slew to guiding
    IUResetSwitch(&SlewRateSP);
    SlewRateS[SLEW_GUIDE].s = ISS_ON;
    IDSetSwitch(&SlewRateSP, nullptr);
    guide_direction = CELESTRON_N;
    GuideNSTID      = IEAddTimer(ms, guideTimeoutHelperN, this);
    return IPS_BUSY;
}

IPState CelestronGPS::GuideSouth(uint32_t ms)
{
    LOGF_DEBUG("GUIDE CMD: S %.0f ms", ms);
    if (!usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }

    // If already moving (no pulse command), then stop movement
    if (MovementNSSP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementNSSP);
        MoveNS(dir == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP);
    }

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if (usePulseCommand)
    {
        driver.send_pulse(CELESTRON_S, 50, ms / 10.0);
    }
    else
    {
        MovementNSS[1].s = ISS_ON;
        MoveNS(DIRECTION_SOUTH, MOTION_START);
    }

    // Set slew to guiding
    IUResetSwitch(&SlewRateSP);
    SlewRateS[SLEW_GUIDE].s = ISS_ON;
    IDSetSwitch(&SlewRateSP, nullptr);
    guide_direction = CELESTRON_S;
    GuideNSTID      = IEAddTimer(ms, guideTimeoutHelperS, this);
    return IPS_BUSY;
}

IPState CelestronGPS::GuideEast(uint32_t ms)
{
    LOGF_DEBUG("GUIDE CMD: E %.0f ms", ms);
    if (!usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }

    // If already moving (no pulse command), then stop movement
    if (MovementWESP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementWESP);
        MoveWE(dir == 0 ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP);
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    if (usePulseCommand)
    {
        driver.send_pulse(CELESTRON_E, 50, ms / 10.0);
    }
    else
    {
        MovementWES[1].s = ISS_ON;
        MoveWE(DIRECTION_EAST, MOTION_START);
    }

    // Set slew to guiding
    IUResetSwitch(&SlewRateSP);
    SlewRateS[SLEW_GUIDE].s = ISS_ON;
    IDSetSwitch(&SlewRateSP, nullptr);
    guide_direction = CELESTRON_E;
    GuideWETID      = IEAddTimer(ms, guideTimeoutHelperE, this);
    return IPS_BUSY;
}

IPState CelestronGPS::GuideWest(uint32_t ms)
{
    LOGF_DEBUG("GUIDE CMD: W %.0f ms", ms);
    if (!usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }

    // If already moving (no pulse command), then stop movement
    if (MovementWESP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementWESP);
        MoveWE(dir == 0 ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP);
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    if (usePulseCommand)
    {
        driver.send_pulse(CELESTRON_W, 50, ms / 10.0);
    }
    else
    {
        MovementWES[0].s = ISS_ON;
        MoveWE(DIRECTION_WEST, MOTION_START);
    }

    // Set slew to guiding
    IUResetSwitch(&SlewRateSP);
    SlewRateS[SLEW_GUIDE].s = ISS_ON;
    IDSetSwitch(&SlewRateSP, nullptr);
    guide_direction = CELESTRON_W;
    GuideWETID      = IEAddTimer(ms, guideTimeoutHelperW, this);
    return IPS_BUSY;
}

//GUIDE The timer helper functions.
void CelestronGPS::guideTimeoutHelperN(void *p)
{
    static_cast<CelestronGPS *>(p)->guideTimeout(CELESTRON_N);
}
void CelestronGPS::guideTimeoutHelperS(void *p)
{
    static_cast<CelestronGPS *>(p)->guideTimeout(CELESTRON_S);
}
void CelestronGPS::guideTimeoutHelperW(void *p)
{
    static_cast<CelestronGPS *>(p)->guideTimeout(CELESTRON_W);
}
void CelestronGPS::guideTimeoutHelperE(void *p)
{
    static_cast<CelestronGPS *>(p)->guideTimeout(CELESTRON_E);
}

//GUIDE The timer function

/* Here I splitted the behaviour depending upon the direction
 * of the  guide command which generates the timer;  this was
 * done because the  member variable "guide_direction"  could
 * be  modified by a pulse  command on the  other axis BEFORE
 * the calling pulse command is terminated.
 */

void CelestronGPS::guideTimeout(CELESTRON_DIRECTION calldir)
{
    //LOG_DEBUG(" END-OF-TIMER");
    //LOGF_DEBUG("   usePulseCommand = %i", usePulseCommand);
    //LOGF_DEBUG("   GUIDE_DIRECTION = %i", (int)guide_direction);
    //LOGF_DEBUG("   CALL_DIRECTION = %i", calldir);

//    if (guide_direction == -1)
//    {
//        driver.stop_motion(CELESTRON_N);
//        driver.stop_motion(CELESTRON_S);
//        driver.stop_motion(CELESTRON_E);
//        driver.stop_motion(CELESTRON_W);
//
//        MovementNSSP.s = IPS_IDLE;
//        MovementWESP.s = IPS_IDLE;
//        IUResetSwitch(&MovementNSSP);
//        IUResetSwitch(&MovementWESP);
//        IDSetSwitch(&MovementNSSP, nullptr);
//        IDSetSwitch(&MovementWESP, nullptr);
//        IERmTimer(GuideNSTID);
//        IERmTimer(GuideWETID);
//    } else
    if (!usePulseCommand)
    {
        if (calldir == CELESTRON_N || calldir == CELESTRON_S)
        {
            MoveNS(calldir == CELESTRON_N ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP);

            if (calldir == CELESTRON_N)
                GuideNSNP.np[0].value = 0;
            else
                GuideNSNP.np[1].value = 0;

            GuideNSNP.s = IPS_IDLE;
            IDSetNumber(&GuideNSNP, nullptr);
            MovementNSSP.s = IPS_IDLE;
            IUResetSwitch(&MovementNSSP);
            IDSetSwitch(&MovementNSSP, nullptr);
        }
        if (calldir == CELESTRON_W || calldir == CELESTRON_E)
        {
            MoveWE(calldir == CELESTRON_W ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP);
            if (calldir == CELESTRON_W)
                GuideWENP.np[0].value = 0;
            else
                GuideWENP.np[1].value = 0;

            GuideWENP.s = IPS_IDLE;
            IDSetNumber(&GuideWENP, nullptr);
            MovementWESP.s = IPS_IDLE;
            IUResetSwitch(&MovementWESP);
            IDSetSwitch(&MovementWESP, nullptr);
        }
    }

    //LOG_DEBUG(" CALL SendPulseStatusCmd");

    bool pulseguide_state;

    if (!driver.get_pulse_status(calldir, pulseguide_state))
        LOG_ERROR("PULSE STATUS UNDETERMINED");
    else if (pulseguide_state)
        LOG_WARN("PULSE STILL IN PROGRESS, POSSIBLE MOUNT JAM.");

    if (calldir == CELESTRON_N || calldir == CELESTRON_S)
    {
        GuideNSNP.np[0].value = 0;
        GuideNSNP.np[1].value = 0;
        GuideNSNP.s           = IPS_IDLE;
        GuideNSTID            = 0;
        IDSetNumber(&GuideNSNP, nullptr);
    }
    if (calldir == CELESTRON_W || calldir == CELESTRON_E)
    {
        GuideWENP.np[0].value = 0;
        GuideWENP.np[1].value = 0;
        GuideWENP.s           = IPS_IDLE;
        GuideWETID            = 0;
        IDSetNumber(&GuideWENP, nullptr);
    }

    //LOG_WARN("GUIDE CMD COMPLETED");
}

bool CelestronGPS::SetTrackMode(uint8_t mode)
{
    return setTrackMode(static_cast<CELESTRON_TRACK_MODE>(mode+1));
}

bool CelestronGPS::SetTrackEnabled(bool enabled)
{
    return setTrackMode(enabled ? static_cast<CELESTRON_TRACK_MODE>(IUFindOnSwitchIndex(&TrackModeSP)+1) : TRACKING_OFF);
}

void CelestronGPS::checkAlignment()
{
    ReadScopeStatus();

    //if (currentRA == 12.0 && currentDEC == 0.0)
    if (!driver.check_aligned())
        LOG_WARN("Mount is NOT aligned. You must align the mount first before you can use it. Disconnect, align the mount, and reconnect again.");
}
