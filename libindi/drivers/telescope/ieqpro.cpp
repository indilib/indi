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
#include <memory>

#include "indicom.h"
#include "ieqpro.h"

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
    currentRA=0;
    currentDEC=0;

    scopeInfo.gpsStatus     = GPS_OFF;
    scopeInfo.systemStatus  = ST_STOPPED;
    scopeInfo.trackRate     = TR_SIDEREAL;
    scopeInfo.slewRate      = SR_1;
    scopeInfo.timeSource    = TS_RS232;
    scopeInfo.hemisphere    = HEMI_NORTH;

    controller = new INDI::Controller(this);
    controller->setJoystickCallback(joystickHelper);
    controller->setButtonCallback(buttonHelper);

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    TelescopeCapability cap;

    cap.canPark = true;
    cap.canSync = true;
    cap.canAbort = true;

    SetTelescopeCapability(&cap);

}

IEQPro::~IEQPro()
{
    delete(controller);
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

    /* Park Coords */
    IUFillNumber(&ParkN[RA_AXIS],"RA_PARK","RA (hh:mm:ss)","%010.6m",0,24,0,0.);
    IUFillNumber(&ParkN[DEC_AXIS],"DEC_PARK","DEC (dd:mm:ss)","%010.6m",-90,90,0,0.);
    IUFillNumberVector(&ParkNP, ParkN,2,getDeviceName(),"PARK_COORDS","Park Coords",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    /* Slew Rate */
    IUFillSwitch(&SlewRateS[SR_1], "SLEW_GUIDE", "1x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_2], "2x", "2x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_3], "SLEW_CENTERING", "3x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_4], "4x", "4x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_5], "SLEW_FIND", "5x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_6], "6x", "6x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_7], "7x", "7x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_8], "8x", "8x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_MAX], "SLEW_MAX", "Max", ISS_ON);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 9, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

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
    IUFillSwitch(&HomeS[0], "Find Home", "", ISS_OFF);
    IUFillSwitch(&HomeS[1], "Set current as Home", "", ISS_OFF);
    IUFillSwitch(&HomeS[2], "Go to Home", "", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 3, getDeviceName(), "HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[0], "GUIDE_RATE", "x Sidereal", "%g", 0.1, 0.9, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 1, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    TrackState=SCOPE_IDLE;

    /*controller->mapController("NSWE Control","NSWE Control", INDI::Controller::CONTROLLER_JOYSTICK, "JOYSTICK_1");
    controller->mapController("SLEW_MAX", "Slew Max", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_1");
    controller->mapController("SLEW_FIND","Slew Find", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_2");
    controller->mapController("SLEW_CENTERING", "Slew Centering", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_3");
    controller->mapController("SLEW_GUIDE", "Slew Guide", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_4");
    controller->mapController("Abort Motion", "Abort Motion", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_5");

    controller->initProperties();*/

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    setInterfaceDescriptor(getInterfaceDescriptor() | GUIDER_INTERFACE);

    addAuxControls();

    return true;
}

bool IEQPro::updateProperties()
{    
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineNumber(&ParkNP);
        defineSwitch(&HomeSP);

        defineSwitch(&TrackModeSP);
        defineSwitch(&SlewRateSP);
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
        deleteProperty(ParkNP.name);
        deleteProperty(HomeSP.name);

        deleteProperty(TrackModeSP.name);
        deleteProperty(SlewRateSP.name);
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

}



bool IEQPro::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp (dev, getDeviceName()))
    {


    }

    controller->ISNewText(dev, name, texts, names, n);

    return INDI::Telescope::ISNewText (dev, name, texts, names, n);
}


bool IEQPro::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{

    if (!strcmp (dev, getDeviceName()))
    {

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

        if (!strcmp(name, ParkNP.name))
        {
           double ra=-1;
           double dec=-100;

           for (int x=0; x<n; x++)
           {
               INumber *eqp = IUFindNumber (&ParkNP, names[x]);
               if (eqp == &ParkN[RA_AXIS])
                   ra = values[x];
               else if (eqp == &ParkN[DEC_AXIS])
                   dec = values[x];
           }

           if ((ra>=0)&&(ra<=24)&&(dec>=-90)&&(dec<=90))
           {
               parkRA  = ra;
               parkDEC = dec;
               IUUpdateNumber(&ParkNP, values, names, n);
               ParkNP.s = IPS_OK;
           }
           else
               ParkNP.s = IPS_ALERT;

           IDSetNumber(&ParkNP, NULL);
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

        if (!strcmp(name, SlewRateSP.name))
        {
            IUUpdateSwitch(&SlewRateSP, states, names, n);

            IEQ_SLEW_RATE rate = (IEQ_SLEW_RATE) IUFindOnSwitchIndex(&SlewRateSP);

            if (setSlewRate(rate))
                SlewRateSP.s = IPS_OK;
            else
                SlewRateSP.s = IPS_ALERT;

            IDSetSwitch(&SlewRateSP, NULL);
            return true;

        }
    }

    controller->ISNewSwitch(dev, name, states, names, n);

    return INDI::Telescope::ISNewSwitch (dev, name, states, names,  n);
}

bool IEQPro::ReadScopeStatus()
{
    bool rc = false;

    IEQInfo newInfo;

    rc = get_ieqpro_status(PortFD, &newInfo);

    if (rc)
    {
        IUResetSwitch(&GPSStatusSP);
        GPSStatusS[newInfo.gpsStatus].s = ISS_ON;
        IDSetSwitch(&GPSStatusSP, NULL);

        IUResetSwitch(&SlewRateSP);
        SlewRateS[newInfo.slewRate].s = ISS_ON;
        IDSetSwitch(&SlewRateSP, NULL);

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
            case ST_PARKED:
            case ST_HOME:
                TrackModeSP.s = IPS_IDLE;
                break;

            default:
                TrackModeSP.s = IPS_BUSY;

        }

        IUResetSwitch(&TrackModeSP);
        TrackModeS[trackMode].s = ISS_ON;
        IDSetSwitch(&TrackModeSP, NULL);

        scopeInfo = newInfo;

    }


    return rc;
}

bool IEQPro::Goto(double r,double d)
{
    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    TrackState = SCOPE_SLEWING;

    IDMessage(getDeviceName(), "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool IEQPro::Abort()
{
    return abort_ieqpro(PortFD);
}

bool IEQPro::Park()
{    
    if (park_ieqpro(PortPD))
    {
        har RAStr[64], DecStr[64];
        fs_sexa(RAStr, parkRA, 2, 3600);
        fs_sexa(DecStr, parkDEC, 2, 3600);

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
        TrackState = SCOPE_IDLE;
        DEBUG(INDI::Logger::DBG_SESSION, "Telescope Unparked");
        return true;
    }
    else
        return false;
}

bool IEQPro::Connect(const char *port)
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
    else if (tty_connect(port, 9600, 8, 0, 1, &PortFD) != TTY_OK)
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

bool IEQPro::Sync(double ra, double dec)
{

    TrackState = SCOPE_IDLE;
    EqNP.s    = IPS_OK;

    NewRaDec(currentRA, currentDEC);

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
     if (set_ieqpro_local_date(PortFD, ltm.years, ltm.months, ltm.days) < 0)
     {
          DEBUG(INDI::Logger::DBG_ERROR, "Error setting local date.");
          return false;
     }

    // Meade defines UTC Offset as the offset ADDED to local time to yield UTC, which
    // is the opposite of the standard definition of UTC offset!
    if (setUTCOffset(PortFD, (utc_offset * -1.0)) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting UTC Offset.");
        return false;
    }

   DEBUG(INDI::Logger::DBG_SESSION, "Time updated, updating planetary data...");

   timeUpdated = true;

   return true;
}

bool IEQPro::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (isSimulation())
        return true;    

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

bool IEQPro::MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Please unpark the mount before issuing any motion commands.");
        return false;
    }

    switch (command)
    {
        case MOTION_START:
        if (start_ieqpro_motion(PortFD, (dir == MOTION_NORTH ? IEQ_N : IEQ_S)) == false)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error setting N/S motion direction.");
            return false;
        }
        else
           DEBUGF(INDI::Logger::DBG_SESSION,"Moving toward %s.", (dir == MOTION_NORTH) ? "North" : "South");
        break;

        case MOTION_STOP:
        if (stop_ieqpro_motion(PortFD, (dir == MOTION_NORTH ? IEQ_N : IEQ_S)) < 0)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error stopping N/S motion.");
            return false;
        }
        else
            DEBUGF(INDI::Logger::DBG_SESSION, "%s motion stopped.", (dir == MOTION_NORTH) ? "North" : "South");
        break;
    }

    return true;
}

bool IEQPro::MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Please unpark the mount before issuing any motion commands.");
        return false;
    }

    switch (command)
    {
        case MOTION_START:
        if (start_ieqpro_motion(PortFD, (dir == MOTION_WEST ? IEQ_W : IEQ_E)) == false)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error setting N/S motion direction.");
            return false;
        }
        else
           DEBUGF(INDI::Logger::DBG_SESSION,"Moving toward %s.", (dir == MOTION_WEST) ? "West" : "East");
        break;

        case MOTION_STOP:
        if (stop_ieqpro_motion(PortFD, (dir == MOTION_WEST ? IEQ_W : IEQ_E)) < 0)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error stopping W/E motion.");
            return false;
        }
        else
            DEBUGF(INDI::Logger::DBG_SESSION, "%s motion stopped.", (dir == MOTION_WEST) ? "West" : "East");
        break;
    }

    return true;
}

bool IEQPro::GuideNorth(float ms)
{

    return true;
}

bool IEQPro::GuideSouth(float ms)
{

    return true;

}

bool IEQPro::GuideEast(float ms)
{


    return true;

}

bool IEQPro::GuideWest(float ms)
{
    return true;
}

bool IEQPro::setSlewRate(IEQ_SLEW_RATE rate)
{
    return set_ieqpro_slew_rate(PortFD, rate);
}

void IEQPro::processButton(const char * button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

#if 0
    // Max Slew speed
    if (!strcmp(button_n, "SLEW_MAX"))
    {
        selectAPMoveToRate(PortFD, 0);
        IUResetSwitch(&MoveToRateSP);
        MoveToRateS[0].s = ISS_ON;
        IDSetSwitch(&MoveToRateSP, NULL);
    }
    // Find Slew speed
    else if (!strcmp(button_n, "SLEW_FIND"))
    {
        selectAPMoveToRate(PortFD, 1);
        IUResetSwitch(&MoveToRateSP);
        MoveToRateS[1].s = ISS_ON;
        IDSetSwitch(&MoveToRateSP, NULL);
    }
    // Centering Slew
    else if (!strcmp(button_n, "SLEW_CENTERING"))
    {
        selectAPMoveToRate(PortFD, 2);
        IUResetSwitch(&MoveToRateSP);
        MoveToRateS[2].s = ISS_ON;
        IDSetSwitch(&MoveToRateSP, NULL);

    }
    // Guide Slew
    else if (!strcmp(button_n, "SLEW_GUIDE"))
    {
        selectAPMoveToRate(PortFD, 3);
        IUResetSwitch(&MoveToRateSP);
        MoveToRateS[3].s = ISS_ON;
        IDSetSwitch(&MoveToRateSP, NULL);
    }
    // Abort
    else if (!strcmp(button_n, "Abort Motion"))
    {
        // Only abort if we have some sort of motion going on
        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY || EqNP.s == IPS_BUSY
                || GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY)
        {

            Abort();
        }
    }

#endif
}

void IEQPro::processNSWE(double mag, double angle)
{
    if (mag < 0.5)
    {
        // Moving in the same direction will make it stop
        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
            Abort();
    }
    // Put high threshold
    else if (mag > 0.9)
    {
        // North
        if (angle > 0 && angle < 180)
        {
            // Don't try to move if you're busy and moving in the same direction
            if (MovementNSSP.s != IPS_BUSY || MovementNSS[0].s != ISS_ON)
                MoveNS(MOTION_NORTH, MOTION_START);

            MovementNSSP.s = IPS_BUSY;
            MovementNSSP.sp[MOTION_NORTH].s = ISS_ON;
            MovementNSSP.sp[MOTION_SOUTH].s = ISS_OFF;
            IDSetSwitch(&MovementNSSP, NULL);
        }
        // South
        if (angle > 180 && angle < 360)
        {
            // Don't try to move if you're busy and moving in the same direction
           if (MovementNSSP.s != IPS_BUSY  || MovementNSS[1].s != ISS_ON)
            MoveNS(MOTION_SOUTH, MOTION_START);

            MovementNSSP.s = IPS_BUSY;
            MovementNSSP.sp[MOTION_NORTH].s = ISS_OFF;
            MovementNSSP.sp[MOTION_SOUTH].s = ISS_ON;
            IDSetSwitch(&MovementNSSP, NULL);
        }
        // East
        if (angle < 90 || angle > 270)
        {
            // Don't try to move if you're busy and moving in the same direction
           if (MovementWESP.s != IPS_BUSY  || MovementWES[1].s != ISS_ON)
                MoveWE(MOTION_EAST, MOTION_START);

           MovementWESP.s = IPS_BUSY;
           MovementWESP.sp[MOTION_WEST].s = ISS_OFF;
           MovementWESP.sp[MOTION_EAST].s = ISS_ON;
           IDSetSwitch(&MovementWESP, NULL);
        }

        // West
        if (angle > 90 && angle < 270)
        {

            // Don't try to move if you're busy and moving in the same direction
           if (MovementWESP.s != IPS_BUSY  || MovementWES[0].s != ISS_ON)
                MoveWE(MOTION_WEST, MOTION_START);

           MovementWESP.s = IPS_BUSY;
           MovementWESP.sp[MOTION_WEST].s = ISS_ON;
           MovementWESP.sp[MOTION_EAST].s = ISS_OFF;
           IDSetSwitch(&MovementWESP, NULL);
        }
    }

}

void IEQPro::processJoystick(const char * joystick_n, double mag, double angle)
{
    if (!strcmp(joystick_n, "NSWE Control"))
        processNSWE(mag, angle);
}


void IEQPro::joystickHelper(const char * joystick_n, double mag, double angle, void *context)
{
    static_cast<IEQPro *>(context)->processJoystick(joystick_n, mag, angle);

}

void IEQPro::buttonHelper(const char * button_n, ISState state, void *context)
{
    static_cast<IEQPro *>(context)->processButton(button_n, state);
}

bool IEQPro::saveConfigItems(FILE *fp)
{
    controller->saveConfigItems(fp);

    INDI::Telescope::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &ParkNP);

    return true;
}

bool IEQPro::ISSnoopDevice(XMLEle *root)
{
    controller->ISSnoopDevice(root);

    return INDI::Telescope::ISSnoopDevice(root);
}

