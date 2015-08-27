/*
    INDI IEQ Pro driver

    Copyright (C) 2015 Jasem Mutlaq

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <memory>

#include "indicom.h"
#include "ieqpro.h"

/* Simulation Parameters */
#define	SLEWRATE	1		/* slew rate, degrees/s */
#define SIDRATE		0.004178	/* sidereal rate, degrees/s */

#define MOUNTINFO_TAB   "Mount Info"

// We declare an auto pointer to IEQPro.
std::auto_ptr<IEQPro> scope(0);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(scope.get() == 0) scope.reset(new IEQPro());
}

void ISGetProperties(const char *dev)
{
        ISInit();
        scope->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        scope->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        scope->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        scope->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
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
void ISSnoopDevice (XMLEle *root)
{
   scope->ISSnoopDevice(root);
}

/* Constructor */
IEQPro::IEQPro()
{
    timeUpdated = locationUpdated = false;

    set_ieqpro_device(getDeviceName());

    //ctor
    currentRA=ln_get_apparent_sidereal_time(ln_get_julian_from_sys());
    currentDEC=90;

    scopeInfo.gpsStatus     = GPS_OFF;
    scopeInfo.systemStatus  = ST_STOPPED;
    scopeInfo.trackRate     = TR_SIDEREAL;
    scopeInfo.slewRate      = SR_1;
    scopeInfo.timeSource    = TS_RS232;
    scopeInfo.hemisphere    = HEMI_NORTH;

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    TelescopeCapability cap;

    cap.canPark = true;
    cap.canSync = true;
    cap.canAbort = true;
    cap.hasLocation = true;
    cap.hasTime  = true;
    cap.nSlewRate= 9;

    SetTelescopeCapability(&cap);
}

IEQPro::~IEQPro()
{
}

const char *IEQPro::getDefaultName()
{
    return (const char *) "iEQ";
}

bool IEQPro::initProperties()
{
    INDI::Telescope::initProperties();

    /* Firmware */
    IUFillText(&FirmwareT[FW_MODEL], "Model", "", 0);
    IUFillText(&FirmwareT[FW_BOARD], "Board", "", 0);
    IUFillText(&FirmwareT[FW_CONTROLLER], "Controller", "", 0);
    IUFillText(&FirmwareT[FW_RA], "RA", "", 0);
    IUFillText(&FirmwareT[FW_DEC], "DEC", "", 0);
    IUFillTextVector(&FirmwareTP, FirmwareT, 5, getDeviceName(), "Firmware Info", "", MOUNTINFO_TAB, IP_RO, 0, IPS_IDLE);

    /* Tracking Mode */
    IUFillSwitch(&TrackModeS[TRACK_SIDEREAL], "TRACK_SIDEREAL", "Sidereal", ISS_ON);
    IUFillSwitch(&TrackModeS[TRACK_SOLAR], "TRACK_SOLAR", "Solar", ISS_OFF);
    IUFillSwitch(&TrackModeS[TRACK_LUNAR], "TRACK_LUNAR", "Lunar", ISS_OFF);
    IUFillSwitch(&TrackModeS[TRACK_CUSTOM], "TRACK_CUSTOM", "Custom", ISS_OFF);
    IUFillSwitchVector(&TrackModeSP, TrackModeS, 4, getDeviceName(), "TELESCOPE_TRACK_RATE", "Tracking Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Custom Tracking Rate */
    IUFillNumber(&CustomTrackRateN[0],"CUSTOM_RATE","Rate","%g",-0.0100, 0.0100, 0.005, 0);
    IUFillNumberVector(&CustomTrackRateNP, CustomTrackRateN,1,getDeviceName(),"CUSTOM_RATE","Custom Track",MOTION_TAB,IP_RW,60,IPS_IDLE);

    /* GPS Status */
    IUFillSwitch(&GPSStatusS[GPS_OFF], "Off", "", ISS_ON);
    IUFillSwitch(&GPSStatusS[GPS_ON], "On", "", ISS_OFF);
    IUFillSwitch(&GPSStatusS[GPS_DATA_OK], "Data OK", "", ISS_OFF);
    IUFillSwitchVector(&GPSStatusSP, GPSStatusS, 3, getDeviceName(), "GPS_STATUS", "GPS", MOUNTINFO_TAB, IP_RO, ISR_1OFMANY, 0, IPS_IDLE);

    /* Time Source */
    IUFillSwitch(&TimeSourceS[TS_RS232], "RS232", "", ISS_ON);
    IUFillSwitch(&TimeSourceS[TS_CONTROLLER], "Controller", "", ISS_OFF);
    IUFillSwitch(&TimeSourceS[TS_GPS], "GPS", "", ISS_OFF);
    IUFillSwitchVector(&TimeSourceSP, TimeSourceS, 3, getDeviceName(), "TIME_SOURCE", "Time Source", MOUNTINFO_TAB, IP_RO, ISR_1OFMANY, 0, IPS_IDLE);

    /* Hemisphere */
    IUFillSwitch(&HemisphereS[HEMI_SOUTH], "South", "", ISS_OFF);
    IUFillSwitch(&HemisphereS[HEMI_NORTH], "North", "", ISS_ON);
    IUFillSwitchVector(&HemisphereSP, HemisphereS, 2, getDeviceName(), "HEMISPHERE", "Hemisphere", MOUNTINFO_TAB, IP_RO, ISR_1OFMANY, 0, IPS_IDLE);

    /* Home */
    IUFillSwitch(&HomeS[IEQ_FIND_HOME], "FindHome", "Find Home", ISS_OFF);
    IUFillSwitch(&HomeS[IEQ_SET_HOME], "SetCurrentAsHome", "Set current as Home", ISS_OFF);
    IUFillSwitch(&HomeS[IEQ_GOTO_HOME], "GoToHome", "Go to Home", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 3, getDeviceName(), "HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[0], "GUIDE_RATE", "x Sidereal", "%g", 0.1, 0.9, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 1, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    TrackState=SCOPE_IDLE;

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    setInterfaceDescriptor(getInterfaceDescriptor() | GUIDER_INTERFACE);

    SetParkDataType(PARK_RA_DEC);

    addAuxControls();

    return true;
}

bool IEQPro::updateProperties()
{    
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineSwitch(&HomeSP);

        defineSwitch(&TrackModeSP);
        defineNumber(&CustomTrackRateNP);

        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);
        defineNumber(&GuideRateNP);

        defineText(&FirmwareTP);
        defineSwitch(&GPSStatusSP);
        defineSwitch(&TimeSourceSP);
        defineSwitch(&HemisphereSP);

        getStartupData();
    }
    else
    {
        deleteProperty(HomeSP.name);

        deleteProperty(TrackModeSP.name);
        deleteProperty(CustomTrackRateNP.name);

        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP.name);

        deleteProperty(FirmwareTP.name);
        deleteProperty(GPSStatusSP.name);
        deleteProperty(TimeSourceSP.name);
        deleteProperty(HemisphereSP.name);
    }

    return true;
}

void IEQPro::getStartupData()
{
    DEBUG(INDI::Logger::DBG_DEBUG, "Getting firmware data...");
    if (get_ieqpro_firmware(PortFD, &firmwareInfo))
    {
        IUSaveText(&FirmwareT[0], firmwareInfo.Model.c_str());
        IUSaveText(&FirmwareT[1], firmwareInfo.MainBoardFirmware.c_str());
        IUSaveText(&FirmwareT[2], firmwareInfo.ControllerFirmware.c_str());
        IUSaveText(&FirmwareT[3], firmwareInfo.RAFirmware.c_str());
        IUSaveText(&FirmwareT[4], firmwareInfo.DEFirmware.c_str());

        FirmwareTP.s = IPS_OK;
        IDSetText(&FirmwareTP, NULL);
    }

    DEBUG(INDI::Logger::DBG_DEBUG, "Getting guiding rate...");
    double guideRate=0;
    if (get_ieqpro_guide_rate(PortFD, &guideRate))
    {
        GuideRateN[0].value = guideRate;
        IDSetNumber(&GuideRateNP, NULL);
    }

    double HA = ln_get_apparent_sidereal_time(ln_get_julian_from_sys());
    double DEC = (HemisphereS[HEMI_NORTH].s == ISS_ON) ? 90 : -90;

    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(HA);
        SetAxis2ParkDefault(DEC);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(HA);
        SetAxis2Park(DEC);
        SetAxis1ParkDefault(HA);
        SetAxis2ParkDefault(DEC);
    }

    double utc_offset;
    int yy, dd, mm, hh, minute, ss;
    if (get_ieqpro_utc_date_time(PortFD, &utc_offset, &yy, &mm, &dd, &hh, &minute, &ss))
    {
        char isoDateTime[32];
        char utcOffset[8];

        snprintf(isoDateTime, 32, "%04d-%02d-%02dT%02d:%02d:%02d", yy, mm, dd, hh, minute, ss);
        snprintf(utcOffset, 8, "%4.2f", utc_offset);

        IUSaveText(IUFindText(&TimeTP, "UTC"), isoDateTime);
        IUSaveText(IUFindText(&TimeTP, "OFFSET"), utcOffset);

        DEBUGF(INDI::Logger::DBG_SESSION, "Mount UTC offset is %s. UTC time is %s", utcOffset, isoDateTime);

        IDSetText(&TimeTP, NULL);
    }

    if (isSimulation())
    {
        if (isParked())
            set_sim_system_status(ST_PARKED);
        else
            set_sim_system_status(ST_STOPPED);
    }
}

bool IEQPro::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp (dev, getDeviceName()))
    {

        // Custom Tracking Rate
        if (!strcmp(name, CustomTrackRateNP.name))
        {
            if (TrackModeS[TRACK_CUSTOM].s != ISS_ON)
            {
                CustomTrackRateNP.s = IPS_IDLE;
                DEBUG(INDI::Logger::DBG_ERROR, "Can only set tracking rate if tracking mode is set to custom.");
                IDSetNumber(&CustomTrackRateNP, NULL);
                return true;
            }

            IUUpdateNumber(&CustomTrackRateNP, values, names, n);

            if (set_ieqpro_custom_track_rate(PortFD, CustomTrackRateN[0].value))
                CustomTrackRateNP.s = IPS_OK;
            else
                CustomTrackRateNP.s = IPS_ALERT;

            IDSetNumber(&CustomTrackRateNP, NULL);

            return true;

        }

        // Guiding Rate
        if (!strcmp(name, GuideRateNP.name))
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);

            if (set_ieqpro_guide_rate(PortFD, GuideRateN[0].value))
                GuideRateNP.s = IPS_OK;
            else
                GuideRateNP.s = IPS_ALERT;

            IDSetNumber(&GuideRateNP, NULL);

            return true;
        }

        if (!strcmp(name,GuideNSNP.name) || !strcmp(name,GuideWENP.name))
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }
    }

    return INDI::Telescope::ISNewNumber (dev, name, values, names, n);
}

bool IEQPro::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp (getDeviceName(), dev))
    {
        if (!strcmp(name, HomeSP.name))
        {
            IUUpdateSwitch(&HomeSP, states, names, n);

            IEQ_HOME_OPERATION operation = (IEQ_HOME_OPERATION) IUFindOnSwitchIndex(&HomeSP);

            IUResetSwitch(&HomeSP);

            switch (operation)
            {
                case IEQ_FIND_HOME:
                    if (firmwareInfo.Model.find("CEM") == std::string::npos)
                    {
                        HomeSP.s = IPS_IDLE;
                        IDSetSwitch(&HomeSP, NULL);
                        DEBUG(INDI::Logger::DBG_WARNING, "Home search is not supported in this model.");
                        return true;
                    }

                    if (find_ieqpro_home(PortFD) == false)
                    {
                        HomeSP.s = IPS_ALERT;
                        IDSetSwitch(&HomeSP, NULL);
                        return false;

                    }

                    HomeSP.s = IPS_OK;
                    IDSetSwitch(&HomeSP, NULL);
                    DEBUG(INDI::Logger::DBG_SESSION, "Searching for home position...");
                    return true;

                    break;

                case IEQ_SET_HOME:
                    if (set_ieqpro_current_home(PortFD) == false)
                    {
                        HomeSP.s = IPS_ALERT;
                        IDSetSwitch(&HomeSP, NULL);
                        return false;

                    }

                    HomeSP.s = IPS_OK;
                    IDSetSwitch(&HomeSP, NULL);
                    DEBUG(INDI::Logger::DBG_SESSION, "Home position set to current coordinates.");
                    return true;

                    break;

                case IEQ_GOTO_HOME:
                    if (goto_ieqpro_home(PortFD) == false)
                    {
                        HomeSP.s = IPS_ALERT;
                        IDSetSwitch(&HomeSP, NULL);
                        return false;

                    }

                    HomeSP.s = IPS_OK;
                    IDSetSwitch(&HomeSP, NULL);
                    DEBUG(INDI::Logger::DBG_SESSION, "Slewing to home position...");
                    return true;

                    break;
            }

            return true;
        }

        if (!strcmp(name, TrackModeSP.name))
        {
            IUUpdateSwitch(&TrackModeSP, states, names, n);

            TelescopeTrackMode mode = (TelescopeTrackMode) IUFindOnSwitchIndex(&TrackModeSP);

            IEQ_TRACK_RATE rate;

            switch (mode)
            {
                case TRACK_SIDEREAL:
                    rate = TR_SIDEREAL;
                    break;
                case TRACK_SOLAR:
                    rate = TR_SOLAR;
                    break;
                case TRACK_LUNAR:
                    rate = TR_LUNAR;
                    break;
                case TRACK_CUSTOM:
                    rate = TR_CUSTOM;
                    break;
            }

            if (set_ieqpro_track_mode(PortFD, rate))
            {
                if (TrackState == SCOPE_TRACKING)
                    TrackModeSP.s = IPS_BUSY;
                else
                    TrackModeSP.s = IPS_OK;
            }
            else
                TrackModeSP.s = IPS_ALERT;

        }

    }

    return INDI::Telescope::ISNewSwitch (dev, name, states, names,  n);
}

bool IEQPro::ReadScopeStatus()
{
    bool rc = false;

    IEQInfo newInfo;

    if (isSimulation())
        mountSim();

    rc = get_ieqpro_status(PortFD, &newInfo);

    if (rc)
    {
        IUResetSwitch(&GPSStatusSP);
        GPSStatusS[newInfo.gpsStatus].s = ISS_ON;
        IDSetSwitch(&GPSStatusSP, NULL);

        IUResetSwitch(&TimeSourceSP);
        TimeSourceS[newInfo.timeSource].s = ISS_ON;
        IDSetSwitch(&TimeSourceSP, NULL);

        IUResetSwitch(&HemisphereSP);
        HemisphereS[newInfo.hemisphere].s = ISS_ON;
        IDSetSwitch(&HemisphereSP, NULL);

        TelescopeTrackMode trackMode;

        switch (newInfo.trackRate)
        {
            case TR_SIDEREAL:
                trackMode = TRACK_SIDEREAL;
                break;
            case TR_SOLAR:
                trackMode = TRACK_SOLAR;
                break;
            case TR_LUNAR:
                trackMode = TRACK_LUNAR;
            case TR_KING:
                trackMode = TRACK_SIDEREAL;
                break;
            case TR_CUSTOM:
                trackMode = TRACK_CUSTOM;
                break;
        }

        switch (newInfo.systemStatus)
        {
            case ST_STOPPED:
                TrackModeSP.s = IPS_IDLE;
                TrackState = SCOPE_IDLE;
                break;
            case ST_PARKED:
                TrackModeSP.s = IPS_IDLE;
                TrackState = SCOPE_PARKED;
                if (isParked() == false)
                    SetParked(true);
                break;
            case ST_HOME:
                TrackModeSP.s = IPS_IDLE;
                TrackState = SCOPE_IDLE;
                break;
             case ST_SLEWING:
             case ST_MERIDIAN_FLIPPING:
                if (TrackState != SCOPE_SLEWING && TrackState != SCOPE_PARKING)
                    TrackState =  SCOPE_SLEWING;
                break;
             case ST_TRACKING_PEC_OFF:
             case ST_TRACKING_PEC_ON:
             case ST_GUIDING:
                TrackModeSP.s = IPS_BUSY;
                TrackState = SCOPE_TRACKING;
                if (scopeInfo.systemStatus == ST_SLEWING)
                    DEBUG(INDI::Logger::DBG_SESSION, "Slew complete, tracking...");
                else if (scopeInfo.systemStatus == ST_MERIDIAN_FLIPPING)
                    DEBUG(INDI::Logger::DBG_SESSION, "Meridian flip complete, tracking...");
                break;
        }

        IUResetSwitch(&TrackModeSP);
        TrackModeS[trackMode].s = ISS_ON;
        IDSetSwitch(&TrackModeSP, NULL);

        scopeInfo = newInfo;

    }

    rc = get_ieqpro_coords(PortFD, &currentRA, &currentDEC);

    if (rc)
        NewRaDec(currentRA, currentDEC);

    return rc;
}

bool IEQPro::Goto(double r,double d)
{
    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    if (set_ieqpro_ra(PortFD, r) == false || set_ieqpro_dec(PortFD, d) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting RA/DEC.");
        return false;
    }

    if (slew_ieqpro(PortFD) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to slew.");
        return false;
    }

    TrackState = SCOPE_SLEWING;

    IDMessage(getDeviceName(), "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool IEQPro::Sync(double ra, double dec)
{

    if (set_ieqpro_ra(PortFD, ra) == false || set_ieqpro_dec(PortFD, dec) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting RA/DEC.");
        return false;
    }

    if (sync_ieqpro(PortFD) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to sync.");
    }

    TrackState = SCOPE_IDLE;
    EqNP.s    = IPS_OK;

    currentRA  = ra;
    currentDEC = dec;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool IEQPro::Abort()
{
    return abort_ieqpro(PortFD);
}

bool IEQPro::Park()
{
    targetRA  = GetAxis1Park();
    targetDEC = GetAxis2Park();
    if (set_ieqpro_ra(PortFD, targetRA) == false || set_ieqpro_dec(PortFD, targetDEC) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting RA/DEC.");
        return false;
    }

    if (park_ieqpro(PortFD))
    {
        char RAStr[64], DecStr[64];
        fs_sexa(RAStr, targetRA, 2, 3600);
        fs_sexa(DecStr, targetDEC, 2, 3600);

        TrackState = SCOPE_PARKING;
        DEBUGF(INDI::Logger::DBG_SESSION, "Telescope parking in progress to RA: %s DEC: %s", RAStr, DecStr);
        return true;
    }
    else
        return false;
}

bool IEQPro::UnPark()
{
    if (unpark_ieqpro(PortFD))
    {
        SetParked(false);
        TrackState = SCOPE_IDLE;
        return true;
    }
    else
        return false;
}

bool IEQPro::Connect(const char *port, uint16_t baud)
{
    set_ieqpro_device(getDeviceName());
    sim = isSimulation();

    if (sim)
    {
       set_sim_gps_status(GPS_DATA_OK);
       set_sim_system_status(ST_STOPPED);
       set_sim_track_rate(TR_SIDEREAL);
       set_sim_slew_rate(SR_3);
       set_sim_time_source(TS_GPS);
       set_sim_hemisphere(HEMI_NORTH);
    }
    else if (tty_connect(port, baud, 8, 0, 1, &PortFD) != TTY_OK)
    {
      DEBUGF(INDI::Logger::DBG_ERROR, "Error connecting to port %s. Make sure you have BOTH write and read permission to the port.", port);
      return false;
    }

    if (check_ieqpro_connection(PortFD) == false)
        return false;

    DEBUG(INDI::Logger::DBG_SESSION, "Telescope is online.");

    return true;
}

bool IEQPro::Disconnect()
{
    timeUpdated = false;
    locationUpdated = false;

    // Disconnect
    return true;
}

bool IEQPro::updateTime(ln_date * utc, double utc_offset)
{    
    struct ln_zonedate ltm;

    ln_date_to_zonedate(utc, &ltm, utc_offset*3600.0);

     // Set Local Time
     if (set_ieqpro_local_time(PortFD, ltm.hours, ltm.minutes, ltm.seconds) == false)
     {
            DEBUG(INDI::Logger::DBG_ERROR, "Error setting local time.");
            return false;
     }

     // Send it as YY (i.e. 2015 --> 15)
     ltm.years -= 2000;

     // Set Local date
     if (set_ieqpro_local_date(PortFD, ltm.years, ltm.months, ltm.days) == false)
     {
          DEBUG(INDI::Logger::DBG_ERROR, "Error setting local date.");
          return false;
     }

    // UTC Offset
    if (set_ieqpro_utc_offset(PortFD, utc_offset) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting UTC Offset.");
        return false;
    }

   DEBUG(INDI::Logger::DBG_SESSION, "Time and date updated.");

   timeUpdated = true;

   return true;
}

bool IEQPro::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (longitude > 180)
        longitude -= 360;

    if (set_ieqpro_longitude(PortFD, longitude) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to set longitude.");
        return false;
    }

    if (set_ieqpro_latitude(PortFD, latitude) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to set longitude.");
        return false;
    }

    char l[32], L[32];
    fs_sexa (l, latitude, 3, 3600);
    fs_sexa (L, longitude, 4, 3600);

    IDMessage(getDeviceName(), "Site location updated to Lat %.32s - Long %.32s", l, L);

    locationUpdated = true;

    return true;
}

void IEQPro::debugTriggered(bool enable)
{
   set_ieqpro_debug(enable);
}

void IEQPro::simulationTriggered(bool enable)
{
    set_ieqpro_simulation(enable);
}

bool IEQPro::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Please unpark the mount before issuing any motion commands.");
        return false;
    }

    switch (command)
    {
        case MOTION_START:
        if (start_ieqpro_motion(PortFD, (dir == DIRECTION_NORTH ? IEQ_N : IEQ_S)) == false)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error setting N/S motion direction.");
            return false;
        }
        else
           DEBUGF(INDI::Logger::DBG_SESSION,"Moving toward %s.", (dir == DIRECTION_NORTH) ? "North" : "South");
        break;

        case MOTION_STOP:
        if (stop_ieqpro_motion(PortFD, (dir == DIRECTION_NORTH ? IEQ_N : IEQ_S)) < 0)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error stopping N/S motion.");
            return false;
        }
        else
            DEBUGF(INDI::Logger::DBG_SESSION, "%s motion stopped.", (dir == DIRECTION_NORTH) ? "North" : "South");
        break;
    }

    return true;
}

bool IEQPro::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Please unpark the mount before issuing any motion commands.");
        return false;
    }

    switch (command)
    {
        case MOTION_START:
        if (start_ieqpro_motion(PortFD, (dir == DIRECTION_WEST ? IEQ_W : IEQ_E)) == false)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error setting N/S motion direction.");
            return false;
        }
        else
           DEBUGF(INDI::Logger::DBG_SESSION,"Moving toward %s.", (dir == DIRECTION_WEST) ? "West" : "East");
        break;

        case MOTION_STOP:
        if (stop_ieqpro_motion(PortFD, (dir == DIRECTION_WEST ? IEQ_W : IEQ_E)) < 0)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error stopping W/E motion.");
            return false;
        }
        else
            DEBUGF(INDI::Logger::DBG_SESSION, "%s motion stopped.", (dir == DIRECTION_WEST) ? "West" : "East");
        break;
    }

    return true;
}

IPState IEQPro::GuideNorth(float ms)
{    
    bool rc = start_ieqpro_guide(PortFD, IEQ_N, (int) ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

IPState IEQPro::GuideSouth(float ms)
{
    bool rc = start_ieqpro_guide(PortFD, IEQ_S, (int) ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

IPState IEQPro::GuideEast(float ms)
{
    bool rc = start_ieqpro_guide(PortFD, IEQ_E, (int) ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

IPState IEQPro::GuideWest(float ms)
{
    bool rc = start_ieqpro_guide(PortFD, IEQ_W, (int) ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

bool IEQPro::SetSlewRate(int index)
{
    IEQ_SLEW_RATE rate = (IEQ_SLEW_RATE) index;
    return set_ieqpro_slew_rate(PortFD, rate);
}

bool IEQPro::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &CustomTrackRateNP);

    return true;
}

void IEQPro::mountSim ()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt, da, dx;
    int nlocked;

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday (&tv, NULL);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
    ltv = tv;
    da = SLEWRATE*dt;

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly */
    switch (TrackState)
    {

    case SCOPE_TRACKING:
        /* RA moves at sidereal, Dec stands still */
        currentRA += (SIDRATE*dt/15.);
        break;

    case SCOPE_SLEWING:
    case SCOPE_PARKING:
        /* slewing - nail it when both within one pulse @ SLEWRATE */
        nlocked = 0;

        dx = targetRA - currentRA;

        // Take shortest path
        if (fabs(dx) > 12)
            dx *= -1;

        if (fabs(dx) <= da)
        {
        currentRA = targetRA;
        nlocked++;
        }
        else if (dx > 0)
            currentRA += da/15.;
        else
            currentRA -= da/15.;

        if (currentRA < 0)
            currentRA += 24;
        else if (currentRA > 24)
            currentRA -= 24;

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
                set_sim_system_status(ST_TRACKING_PEC_OFF);
            else
                set_sim_system_status(ST_PARKED);
        }

        break;

    default:
        break;
    }

    set_sim_ra(currentRA);
    set_sim_dec(currentDEC);

}

void IEQPro::SetCurrentPark()
{
    SetAxis1Park(currentRA);
    SetAxis2Park(currentDEC);
}

void IEQPro::SetDefaultPark()
{
    // By default set RA to HA
    SetAxis1Park(ln_get_apparent_sidereal_time(ln_get_julian_from_sys()));

    // Set DEC to 90 or -90 depending on the hemisphere
    SetAxis2Park( (HemisphereS[HEMI_NORTH].s == ISS_ON) ? 90 : -90);

}
