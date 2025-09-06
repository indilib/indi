/*******************************************************************************
  Copyright(c) 2011 Gerry Rozema, Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "inditelescope.h"

#include "indicom.h"
#include "indicontroller.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

#include <cmath>
#include <cerrno>
#include <pwd.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <wordexp.h>
#include <limits>

namespace INDI
{

Telescope::Telescope()
    : DefaultDevice(), ParkDataFileName(GetHomeDirectory() + "/.indi/ParkData.xml")
{
    controller = new Controller(this);
    controller->setJoystickCallback(joystickHelper);
    controller->setAxisCallback(axisHelper);
    controller->setButtonCallback(buttonHelper);

    currentPierSide = PIER_EAST;
    lastPierSide    = PIER_UNKNOWN;

    currentPECState = PEC_OFF;
    lastPECState    = PEC_UNKNOWN;
}

Telescope::~Telescope()
{
    if (ParkdataXmlRoot)
        delXMLEle(ParkdataXmlRoot);

    delete (controller);
}

bool Telescope::initProperties()
{
    DefaultDevice::initProperties();

    // Since this property was created in setTelescopeCapability before a device name is defined.
    // Then we need to set the device name here
    HomeSP.setDeviceName(getDeviceName());

    // Active Devices
    // @INDI_STANDARD_PROPERTY@
    ActiveDeviceTP[ACTIVE_GPS].fill("ACTIVE_GPS", "GPS", "GPS Simulator");
    ActiveDeviceTP[ACTIVE_DOME].fill("ACTIVE_DOME", "DOME", "Dome Simulator");
    ActiveDeviceTP.fill(getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    ActiveDeviceTP.load();

    // Use locking if dome is closed (and or) park scope if dome is closing
    // @INDI_STANDARD_PROPERTY@
    DomePolicySP[DOME_IGNORED].fill("DOME_IGNORED", "Dome ignored", ISS_ON);
    DomePolicySP[DOME_LOCKS].fill("DOME_LOCKS", "Dome locks", ISS_OFF);
    DomePolicySP.fill(getDeviceName(), "DOME_POLICY", "Dome Policy",  OPTIONS_TAB, IP_RW,
                      ISR_1OFMANY, 60, IPS_IDLE);

    // @INDI_STANDARD_PROPERTY@
    EqNP[AXIS_RA].fill("RA", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    EqNP[AXIS_DE].fill("DEC", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    EqNP.fill(getDeviceName(), "EQUATORIAL_EOD_COORD", "Eq. Coordinates", MAIN_CONTROL_TAB,
              IP_RW, 60, IPS_IDLE);
    lastEqState = IPS_IDLE;

    // @INDI_STANDARD_PROPERTY@
    TargetNP[AXIS_RA].fill("RA", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    TargetNP[AXIS_DE].fill("DEC", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    TargetNP.fill(getDeviceName(), "TARGET_EOD_COORD", "Slew Target", MOTION_TAB, IP_RO, 60,
                  IPS_IDLE);

    // @INDI_STANDARD_PROPERTY@
    ParkOptionSP[PARK_CURRENT].fill("PARK_CURRENT", "Current", ISS_OFF);
    ParkOptionSP[PARK_DEFAULT].fill("PARK_DEFAULT", "Default", ISS_OFF);
    ParkOptionSP[PARK_WRITE_DATA].fill("PARK_WRITE_DATA", "Write Data", ISS_OFF);
    ParkOptionSP[PARK_PURGE_DATA].fill("PARK_PURGE_DATA", "Purge Data", ISS_OFF);
    ParkOptionSP.fill(getDeviceName(), "TELESCOPE_PARK_OPTION", "Park Options", SITE_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // @INDI_STANDARD_PROPERTY@
    TimeTP[UTC].fill("UTC", "UTC Time", nullptr);
    TimeTP[OFFSET].fill("OFFSET", "UTC Offset", nullptr);
    TimeTP.fill(getDeviceName(), "TIME_UTC", "UTC", SITE_TAB, IP_RW, 60, IPS_IDLE);

    // @INDI_STANDARD_PROPERTY@
    LocationNP[LOCATION_LATITUDE].fill("LAT", "Lat (dd:mm:ss.s)", "%012.8m", -90, 90, 0, 0.0);
    LocationNP[LOCATION_LONGITUDE].fill("LONG", "Lon (dd:mm:ss.s)", "%012.8m", 0, 360, 0, 0.0);
    LocationNP[LOCATION_ELEVATION].fill("ELEV", "Elevation (m)", "%g", -200, 10000, 0, 0);
    LocationNP.fill(getDeviceName(), "GEOGRAPHIC_COORD", "Scope Location", SITE_TAB,
                    IP_RW, 60, IPS_IDLE);

    // Pier Side
    // @INDI_STANDARD_PROPERTY@
    PierSideSP[PIER_WEST].fill("PIER_WEST", "West (pointing east)", ISS_OFF);
    PierSideSP[PIER_EAST].fill("PIER_EAST", "East (pointing west)", ISS_OFF);
    PierSideSP.fill(getDeviceName(), "TELESCOPE_PIER_SIDE", "Pier Side", MAIN_CONTROL_TAB,
                    IP_RO, ISR_ATMOST1, 60, IPS_IDLE);

    // Pier Side Simulation
    // @INDI_STANDARD_PROPERTY@
    SimulatePierSideSP[SIMULATE_YES].fill("SIMULATE_YES", "Yes", ISS_OFF);
    SimulatePierSideSP[SIMULATE_NO].fill("SIMULATE_NO", "No", ISS_ON);
    SimulatePierSideSP.fill(getDeviceName(), "SIMULATE_PIER_SIDE", "Simulate Pier Side",
                            MAIN_CONTROL_TAB,
                            IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // PEC State
    // @INDI_STANDARD_PROPERTY@
    PECStateSP[PEC_OFF].fill("PEC OFF", "PEC OFF", ISS_ON);
    PECStateSP[PEC_ON].fill("PEC ON", "PEC ON", ISS_OFF);
    PECStateSP.fill(getDeviceName(), "PEC", "PEC Playback", MOTION_TAB, IP_RW, ISR_1OFMANY, 0,
                    IPS_IDLE);

    // Mount Type
    // @INDI_STANDARD_PROPERTY@
    MountTypeSP[MOUNT_ALTAZ].fill("ALTAZ", "ALTAZ", ISS_OFF);
    MountTypeSP[MOUNT_EQ_FORK].fill("EQ_FORK", "Fork (Eq)", ISS_OFF);
    MountTypeSP[MOUNT_EQ_GEM].fill("EQ_GEM", "GEM", ISS_ON);
    MountTypeSP.fill(getDeviceName(), "TELESCOPE_MOUNT_TYPE", "Mount Type", MOTION_TAB, IP_RO, ISR_1OFMANY, 60, IPS_IDLE);

    // Track Mode. Child class must call AddTrackMode to add members
    // @INDI_STANDARD_PROPERTY@
    TrackModeSP.fill(getDeviceName(), "TELESCOPE_TRACK_MODE", "Track Mode", MAIN_CONTROL_TAB,
                     IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Track State
    // @INDI_STANDARD_PROPERTY@
    TrackStateSP[TRACK_ON].fill("TRACK_ON", "On", ISS_OFF);
    TrackStateSP[TRACK_OFF].fill("TRACK_OFF", "Off", ISS_ON);
    TrackStateSP.fill(getDeviceName(), "TELESCOPE_TRACK_STATE", "Tracking", MAIN_CONTROL_TAB,
                      IP_RW, ISR_1OFMANY, 0,
                      IPS_IDLE);

    // Track Rate
    // @INDI_STANDARD_PROPERTY@
    TrackRateNP[AXIS_RA].fill("TRACK_RATE_RA", "RA (arcsecs/s)", "%.6f", -16384.0, 16384.0, 0.000001,
                              TRACKRATE_SIDEREAL);
    TrackRateNP[AXIS_DE].fill("TRACK_RATE_DE", "DE (arcsecs/s)", "%.6f", -16384.0, 16384.0, 0.000001, 0.0);
    TrackRateNP.fill(getDeviceName(), "TELESCOPE_TRACK_RATE", "Track Rates", MAIN_CONTROL_TAB,
                     IP_RW, 60, IPS_IDLE);

    // On Coord Set
    // @INDI_STANDARD_PROPERTY@
    // Note: Member elements can be TRACK, SLEW, SYNC, and FLIP
    CoordSP.fill(getDeviceName(), "ON_COORD_SET", "On Set", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    generateCoordSet();

    // @INDI_STANDARD_PROPERTY@
    if (nSlewRate >= 4)
        SlewRateSP.fill(getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate",
                        MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    if (CanTrackSatellite())
    {
        // @INDI_STANDARD_PROPERTY@
        TLEtoTrackTP[0].fill("TLE", "TLE", "");
        TLEtoTrackTP.fill(getDeviceName(), "SAT_TLE_TEXT", "Orbit Params", SATELLITE_TAB,
                          IP_RW, 60, IPS_IDLE);

        char curTime[32] = {0};
        std::time_t t = std::time(nullptr);
        struct std::tm *utctimeinfo = std::gmtime(&t);
        strftime(curTime, sizeof(curTime), "%Y-%m-%dT%H:%M:%S", utctimeinfo);

        // @INDI_STANDARD_PROPERTY@
        SatPassWindowTP[SAT_PASS_WINDOW_END].fill("SAT_PASS_WINDOW_END", "End UTC", curTime);
        SatPassWindowTP[SAT_PASS_WINDOW_START].fill("SAT_PASS_WINDOW_START", "Start UTC", curTime);
        SatPassWindowTP.fill(getDeviceName(), "SAT_PASS_WINDOW", "Pass Window", SATELLITE_TAB, IP_RW, 60, IPS_IDLE);

        // @INDI_STANDARD_PROPERTY@
        TrackSatSP[SAT_TRACK].fill("SAT_TRACK", "Track", ISS_OFF);
        TrackSatSP[SAT_HALT].fill("SAT_HALT", "Halt", ISS_ON);
        TrackSatSP.fill(getDeviceName(), "SAT_TRACKING_STAT",
                        "Sat tracking", SATELLITE_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    }

    // @INDI_STANDARD_PROPERTY@
    ParkSP[PARK].fill("PARK", "Park(ed)", ISS_OFF);
    ParkSP[UNPARK].fill("UNPARK", "UnPark(ed)", ISS_OFF);
    ParkSP.fill(getDeviceName(), "TELESCOPE_PARK", "Parking", MAIN_CONTROL_TAB, IP_RW,
                ISR_1OFMANY, 60, IPS_IDLE);

    // @INDI_STANDARD_PROPERTY@
    AbortSP[0].fill("ABORT", "Abort", ISS_OFF);
    AbortSP.fill(getDeviceName(), "TELESCOPE_ABORT_MOTION", "Abort Motion", MAIN_CONTROL_TAB,
                 IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // @INDI_STANDARD_PROPERTY@
    MovementNSSP[DIRECTION_NORTH].fill("MOTION_NORTH", "North", ISS_OFF);
    MovementNSSP[DIRECTION_SOUTH].fill("MOTION_SOUTH", "South", ISS_OFF);
    MovementNSSP.fill(getDeviceName(), "TELESCOPE_MOTION_NS", "Motion N/S", MOTION_TAB,
                      IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // @INDI_STANDARD_PROPERTY@
    MovementWESP[DIRECTION_WEST].fill("MOTION_WEST", "West", ISS_OFF);
    MovementWESP[DIRECTION_EAST].fill("MOTION_EAST", "East", ISS_OFF);
    MovementWESP.fill(getDeviceName(), "TELESCOPE_MOTION_WE", "Motion W/E", MOTION_TAB,
                      IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Reverse NS or WE
    // @INDI_STANDARD_PROPERTY@
    ReverseMovementSP[REVERSE_NS].fill("REVERSE_NS", "North/South", ISS_OFF);
    ReverseMovementSP[REVERSE_WE].fill("REVERSE_WE", "West/East", ISS_OFF);
    ReverseMovementSP.fill(getDeviceName(), "TELESCOPE_REVERSE_MOTION", "Reverse", MOTION_TAB, IP_RW, ISR_NOFMANY, 60,
                           IPS_IDLE);

    controller->initProperties();

    // Joystick motion control
    // @INDI_STANDARD_PROPERTY@
    MotionControlModeTP[MOTION_CONTROL_MODE_JOYSTICK].fill("MOTION_CONTROL_MODE_JOYSTICK", "4-Way Joystick", ISS_ON);
    MotionControlModeTP[MOTION_CONTROL_MODE_AXES].fill("MOTION_CONTROL_MODE_AXES", "Two Separate Axes", ISS_OFF);
    MotionControlModeTP.fill(getDeviceName(), "MOTION_CONTROL_MODE", "Motion Control",
                             "Joystick", IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Lock Axis
    // @INDI_STANDARD_PROPERTY@
    LockAxisSP[LOCK_AXIS_1].fill("LOCK_AXIS_1", "West/East", ISS_OFF);
    LockAxisSP[LOCK_AXIS_2].fill("LOCK_AXIS_2", "North/South", ISS_OFF);
    LockAxisSP.fill(getDeviceName(), "JOYSTICK_LOCK_AXIS", "Lock Axis", "Joystick", IP_RW,
                    ISR_ATMOST1, 60, IPS_IDLE);

    TrackState = SCOPE_IDLE;

    setDriverInterface(TELESCOPE_INTERFACE);

    if (telescopeConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(serialConnection);
    }

    if (telescopeConnection & CONNECTION_TCP)
    {
        tcpConnection = new Connection::TCP(this);
        tcpConnection->registerHandshake([&]()
        {
            return callHandshake();
        });

        registerConnection(tcpConnection);
    }

    IDSnoopDevice(ActiveDeviceTP[ACTIVE_GPS].getText(), "GEOGRAPHIC_COORD");
    IDSnoopDevice(ActiveDeviceTP[ACTIVE_GPS].getText(), "TIME_UTC");

    IDSnoopDevice(ActiveDeviceTP[ACTIVE_DOME].getText(), "DOME_PARK");
    IDSnoopDevice(ActiveDeviceTP[ACTIVE_DOME].getText(), "DOME_SHUTTER");

    addPollPeriodControl();

    double longitude = 0, latitude = 0, elevation = 0;
    // Get value from config file if it exists.
    if (IUGetConfigNumber(getDeviceName(), LocationNP.getName(), LocationNP[LOCATION_LONGITUDE].getName(), &longitude) == 0)
    {
        LocationNP[LOCATION_LONGITUDE].setValue(longitude);
        m_Location.longitude = longitude;
    }
    if (IUGetConfigNumber(getDeviceName(), LocationNP.getName(), LocationNP[LOCATION_LATITUDE].getName(), &latitude) == 0)
    {
        LocationNP[LOCATION_LATITUDE].setValue(latitude);
        m_Location.latitude = latitude;
    }
    if (IUGetConfigNumber(getDeviceName(), LocationNP.getName(), LocationNP[LOCATION_ELEVATION].getName(), &elevation) == 0)
    {
        LocationNP[LOCATION_ELEVATION].setValue(elevation);
        m_Location.elevation = elevation;
    }

    return true;
}

void Telescope::ISGetProperties(const char *dev)
{
    DefaultDevice::ISGetProperties(dev);

    if (CanGOTO())
    {
        defineProperty(ActiveDeviceTP);

        ISState isDomeIgnored = ISS_OFF;
        if (IUGetConfigSwitch(getDeviceName(), DomePolicySP.getName(), DomePolicySP[DOME_IGNORED].getName(), &isDomeIgnored) == 0)
        {
            DomePolicySP[DOME_IGNORED].setState(isDomeIgnored);
            DomePolicySP[DOME_LOCKS].setState((isDomeIgnored == ISS_ON) ? ISS_OFF : ISS_ON);
        }
        defineProperty(DomePolicySP);
    }

    if (CanGOTO())
        controller->ISGetProperties(dev);
}

bool Telescope::updateProperties()
{
    if (isConnected())
    {
        controller->mapController("MOTIONDIR", "N/S/W/E Control", Controller::CONTROLLER_JOYSTICK, "JOYSTICK_1");
        controller->mapController("MOTIONDIRNS", "N/S Control", Controller::CONTROLLER_AXIS, "AXIS_8");
        controller->mapController("MOTIONDIRWE", "W/E Control", Controller::CONTROLLER_AXIS, "AXIS_7");

        if (nSlewRate >= 4)
        {
            controller->mapController("SLEWPRESET", "Slew Rate", Controller::CONTROLLER_JOYSTICK, "JOYSTICK_2");
            controller->mapController("SLEWPRESETUP", "Slew Rate Up", Controller::CONTROLLER_BUTTON, "BUTTON_5");
            controller->mapController("SLEWPRESETDOWN", "Slew Rate Down", Controller::CONTROLLER_BUTTON,
                                      "BUTTON_6");
        }
        if (CanAbort())
            controller->mapController("ABORTBUTTON", "Abort", Controller::CONTROLLER_BUTTON, "BUTTON_1");
        if (CanPark())
        {
            controller->mapController("PARKBUTTON", "Park", Controller::CONTROLLER_BUTTON, "BUTTON_2");
            controller->mapController("UNPARKBUTTON", "UnPark", Controller::CONTROLLER_BUTTON, "BUTTON_3");
        }

        //  Now we add our telescope specific stuff
        if (CanGOTO() || CanSync())
            defineProperty(CoordSP);
        defineProperty(EqNP);
        if (CanAbort())
            defineProperty(AbortSP);

        if (HasTrackMode() && TrackModeSP)
            defineProperty(TrackModeSP);
        if (CanControlTrack())
            defineProperty(TrackStateSP);
        if (HasTrackRate())
            defineProperty(TrackRateNP);
        if (CanHomeFind() || CanHomeSet() || CanHomeGo())
            defineProperty(HomeSP);

        if (CanGOTO())
        {
            defineProperty(MovementNSSP);
            defineProperty(MovementWESP);
            defineProperty(ReverseMovementSP);
            if (nSlewRate >= 4)
                defineProperty(SlewRateSP);
            defineProperty(TargetNP);
        }

        defineProperty(MountTypeSP);

        if (HasTime())
            defineProperty(TimeTP);
        if (HasLocation())
            defineProperty(LocationNP);
        if (CanPark())
        {
            defineProperty(ParkSP);
            if (parkDataType != PARK_NONE)
            {
                defineProperty(ParkPositionNP);
                defineProperty(ParkOptionSP);
            }
        }

        if (HasPierSide())
            defineProperty(PierSideSP);

        if (HasPierSideSimulation())
        {
            defineProperty(SimulatePierSideSP);
            ISState value;
            if (IUGetConfigSwitch(getDefaultName(), "SIMULATE_PIER_SIDE", "SIMULATE_YES", &value) )
                setSimulatePierSide(value == ISS_ON);
        }

        if (CanTrackSatellite())
        {
            defineProperty(TLEtoTrackTP);
            defineProperty(SatPassWindowTP);
            defineProperty(TrackSatSP);
        }

        if (HasPECState())
            defineProperty(PECStateSP);
    }
    else
    {
        if (CanGOTO() || CanSync())
            deleteProperty(CoordSP);
        deleteProperty(EqNP);
        if (CanAbort())
            deleteProperty(AbortSP);
        if (HasTrackMode() && TrackModeSP)
            deleteProperty(TrackModeSP);
        if (HasTrackRate())
            deleteProperty(TrackRateNP);
        if (CanControlTrack())
            deleteProperty(TrackStateSP);
        if (CanHomeFind() || CanHomeSet() || CanHomeGo())
            deleteProperty(HomeSP);

        if (CanGOTO())
        {
            deleteProperty(MovementNSSP);
            deleteProperty(MovementWESP);
            deleteProperty(ReverseMovementSP.getName());
            if (nSlewRate >= 4)
                deleteProperty(SlewRateSP);
            deleteProperty(TargetNP);
        }

        deleteProperty(MountTypeSP);

        if (HasTime())
            deleteProperty(TimeTP);
        if (HasLocation())
            deleteProperty(LocationNP);

        if (CanPark())
        {
            deleteProperty(ParkSP);
            if (parkDataType != PARK_NONE)
            {
                deleteProperty(ParkPositionNP);
                deleteProperty(ParkOptionSP);
            }
        }

        if (HasPierSide())
            deleteProperty(PierSideSP);

        if (HasPierSideSimulation())
        {
            deleteProperty(SimulatePierSideSP);
            if (getSimulatePierSide() == true)
                deleteProperty(PierSideSP);
        }

        if (CanTrackSatellite())
        {
            deleteProperty(TLEtoTrackTP);
            deleteProperty(SatPassWindowTP);
            deleteProperty(TrackSatSP);
        }

        if (HasPECState())
            deleteProperty(PECStateSP);
    }

    if (CanGOTO())
    {
        controller->updateProperties();

        auto useJoystick = getSwitch("USEJOYSTICK");
        if (useJoystick)
        {
            if (isConnected())
            {
                if (useJoystick[0].getState() == ISS_ON)
                {
                    defineProperty(MotionControlModeTP);
                    loadConfig(true, "MOTION_CONTROL_MODE");
                    defineProperty(LockAxisSP);
                    loadConfig(true, "LOCK_AXIS");
                }
                else
                {
                    deleteProperty(MotionControlModeTP);
                    deleteProperty(LockAxisSP);
                }
            }
            else
            {
                deleteProperty(MotionControlModeTP);
                deleteProperty(LockAxisSP);
            }
        }
    }

    return true;
}

bool Telescope::ISSnoopDevice(XMLEle *root)
{
    controller->ISSnoopDevice(root);

    XMLEle *ep           = nullptr;
    const char *propName = findXMLAttValu(root, "name");
    auto deviceName = std::string(findXMLAttValu(root, "device"));

    if (isConnected())
    {
        if (HasLocation() && !strcmp(propName, "GEOGRAPHIC_COORD") && deviceName == ActiveDeviceTP[ACTIVE_GPS].getText())
        {
            // Only accept IPS_OK state
            if (strcmp(findXMLAttValu(root, "state"), "Ok"))
                return false;

            double longitude = -1, latitude = -1, elevation = -1;

            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                const char *elemName = findXMLAttValu(ep, "name");

                if (!strcmp(elemName, "LAT"))
                    latitude = atof(pcdataXMLEle(ep));
                else if (!strcmp(elemName, "LONG"))
                    longitude = atof(pcdataXMLEle(ep));
                else if (!strcmp(elemName, "ELEV"))
                    elevation = atof(pcdataXMLEle(ep));
            }

            return processLocationInfo(latitude, longitude, elevation);
        }
        else if (HasTime() && !strcmp(propName, "TIME_UTC") && deviceName == ActiveDeviceTP[ACTIVE_GPS].getText())
        {
            // Only accept IPS_OK state
            if (strcmp(findXMLAttValu(root, "state"), "Ok"))
                return false;

            char utc[MAXINDITSTAMP], offset[MAXINDITSTAMP];

            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                const char *elemName = findXMLAttValu(ep, "name");

                if (!strcmp(elemName, "UTC"))
                    strncpy(utc, pcdataXMLEle(ep), MAXINDITSTAMP);
                else if (!strcmp(elemName, "OFFSET"))
                    strncpy(offset, pcdataXMLEle(ep), MAXINDITSTAMP);
            }

            return processTimeInfo(utc, offset);
        }
        else if (!strcmp(propName, "DOME_PARK") && deviceName == ActiveDeviceTP[ACTIVE_DOME].getText())
        {
            // This is handled by Watchdog driver.
            // Mount shouldn't park due to dome closing in INDI::Telescope
#if 0
            if (strcmp(findXMLAttValu(root, "state"), "Ok"))
            {
                // Dome options is dome parks or both and dome is parking.
                if ((DomeClosedLockT[2].s == ISS_ON || DomeClosedLockT[3].s == ISS_ON) && !IsLocked && !IsParked)
                {
                    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
                    {
                        const char * elemName = findXMLAttValu(ep, "name");
                        if (( (!strcmp(elemName, "SHUTTER_CLOSE") || !strcmp(elemName, "PARK"))
                                && !strcmp(pcdataXMLEle(ep), "On")))
                        {
                            RememberTrackState = TrackState;
                            Park();
                            LOG_INFO("Dome is closing, parking mount...");
                        }
                    }
                }
            } // Dome is changing state and Dome options is lock or both. d
            else
#endif
                if (!strcmp(findXMLAttValu(root, "state"), "Ok"))
                {
                    bool prevState = IsLocked;
                    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
                    {
                        const char *elemName = findXMLAttValu(ep, "name");

                        if (!IsLocked && (!strcmp(elemName, "PARK")) && !strcmp(pcdataXMLEle(ep), "On"))
                            IsLocked = true;
                        else if (IsLocked && (!strcmp(elemName, "UNPARK")) && !strcmp(pcdataXMLEle(ep), "On"))
                            IsLocked = false;
                    }
                    if (prevState != IsLocked && (DomePolicySP[DOME_LOCKS].getState() == ISS_ON))
                        LOGF_INFO("Dome status changed. Lock is set to: %s", IsLocked ? "locked" : "unlock");
                }
            return true;
        }
    }

    return DefaultDevice::ISSnoopDevice(root);
}

void Telescope::triggerSnoop(const char *driverName, const char *snoopedProp)
{
    LOGF_DEBUG("Active Snoop, driver: %s, property: %s", driverName, snoopedProp);
    IDSnoopDevice(driverName, snoopedProp);
}

uint8_t Telescope::getTelescopeConnection() const
{
    return telescopeConnection;
}

void Telescope::setTelescopeConnection(const uint8_t &value)
{
    uint8_t mask = CONNECTION_SERIAL | CONNECTION_TCP | CONNECTION_NONE;

    if (value == 0 || (mask & value) == 0)
    {
        DEBUGF(Logger::DBG_ERROR, "Invalid connection mode %d", value);
        return;
    }

    telescopeConnection = value;
}

bool Telescope::saveConfigItems(FILE *fp)
{
    DefaultDevice::saveConfigItems(fp);

    ActiveDeviceTP.save(fp);
    DomePolicySP.save(fp);

    // Ensure that we only save valid locations
    if (HasLocation() && (LocationNP[LOCATION_LONGITUDE].getValue() != 0 || LocationNP[LOCATION_LATITUDE].getValue() != 0))
        LocationNP.save(fp);

    if (CanGOTO())
        ReverseMovementSP.save(fp);

    if (SlewRateSP.isValid())
        SlewRateSP.save(fp);
    if (HasPECState())
        PECStateSP.save(fp);
    if (HasTrackMode())
        TrackModeSP.save(fp);
    if (HasTrackRate())
        TrackRateNP.save(fp);

    controller->saveConfigItems(fp);
    MotionControlModeTP.save(fp);
    LockAxisSP.save(fp);
    SimulatePierSideSP.save(fp);

    return true;
}

void Telescope::NewRaDec(double ra, double dec)
{
    switch (TrackState)
    {
        case SCOPE_PARKED:
        case SCOPE_IDLE:
            EqNP.setState(IPS_IDLE);
            break;

        case SCOPE_SLEWING:
        case SCOPE_PARKING:
            EqNP.setState(IPS_BUSY);
            break;

        case SCOPE_TRACKING:
            EqNP.setState(IPS_OK);
            break;
    }

    if (TrackState != SCOPE_TRACKING && CanControlTrack() && TrackStateSP[TRACK_ON].getState() == ISS_ON)
    {
        TrackStateSP.setState(IPS_IDLE);
        TrackStateSP[TRACK_ON].setState(ISS_OFF);
        TrackStateSP[TRACK_OFF].setState(ISS_ON);
        TrackStateSP.apply();
    }
    else if (TrackState == SCOPE_TRACKING && CanControlTrack() && TrackStateSP[TRACK_OFF].getState() == ISS_ON)
    {
        TrackStateSP.setState(IPS_BUSY);
        TrackStateSP[TRACK_ON].setState(ISS_ON);
        TrackStateSP[TRACK_OFF].setState(ISS_OFF);
        TrackStateSP.apply();
    }

    // RA is in hours, so change the arc-second threshold accordingly.
    constexpr double RA_NOTIFY_THRESHOLD = EQ_NOTIFY_THRESHOLD/15.0;
    if (std::abs(EqNP[AXIS_RA].getValue() - ra) > RA_NOTIFY_THRESHOLD ||
            std::abs(EqNP[AXIS_DE].getValue() - dec) > EQ_NOTIFY_THRESHOLD ||
            EqNP.getState() != lastEqState)
    {
        EqNP[AXIS_RA].setValue(ra);
        EqNP[AXIS_DE].setValue(dec);
        lastEqState        = EqNP.getState();
        EqNP.apply();
    }
}

bool Telescope::Sync(double ra, double dec)
{
    INDI_UNUSED(ra);
    INDI_UNUSED(dec);
    //  if we get here, our mount doesn't support sync
    DEBUG(Logger::DBG_ERROR, "Telescope does not support Sync.");
    return false;
}

bool Telescope::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    INDI_UNUSED(dir);
    INDI_UNUSED(command);
    DEBUG(Logger::DBG_ERROR, "Telescope does not support North/South motion.");
    MovementNSSP.reset();
    MovementNSSP.setState(IPS_IDLE);
    MovementNSSP.apply();
    return false;
}

bool Telescope::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    INDI_UNUSED(dir);
    INDI_UNUSED(command);
    DEBUG(Logger::DBG_ERROR, "Telescope does not support West/East motion.");
    MovementWESP.reset();
    MovementWESP.setState(IPS_IDLE);
    MovementWESP.apply();
    return false;
}

/**************************************************************************************
** Process Text properties
***************************************************************************************/
bool Telescope::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (TimeTP.isNameMatch(name))
        {
            int utcindex    = IUFindIndex("UTC", names, n);
            int offsetindex = IUFindIndex("OFFSET", names, n);

            return processTimeInfo(texts[utcindex], texts[offsetindex]);
        }

        if (ActiveDeviceTP.isNameMatch(name))
        {
            ActiveDeviceTP.setState(IPS_OK);
            ActiveDeviceTP.update(texts, names, n);
            //  Update client display
            ActiveDeviceTP.apply();

            IDSnoopDevice(ActiveDeviceTP[ACTIVE_GPS].getText(), "GEOGRAPHIC_COORD");
            IDSnoopDevice(ActiveDeviceTP[ACTIVE_GPS].getText(), "TIME_UTC");

            IDSnoopDevice(ActiveDeviceTP[ACTIVE_DOME].getText(), "DOME_PARK");
            IDSnoopDevice(ActiveDeviceTP[ACTIVE_DOME].getText(), "DOME_SHUTTER");

            saveConfig(ActiveDeviceTP);
            return true;
        }
    }

    controller->ISNewText(dev, name, texts, names, n);

    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
bool Telescope::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        ///////////////////////////////////
        // Goto & Sync for Equatorial Coords
        ///////////////////////////////////
        if (strcmp(name, "EQUATORIAL_EOD_COORD") == 0)
        {
            //  this is for us, and it is a goto
            bool rc    = false;
            double ra  = -1;
            double dec = -100;

            for (int x = 0; x < n; x++)
            {
                if (EqNP[AXIS_RA].isNameMatch(names[x]))
                    ra = values[x];
                else if (EqNP[AXIS_DE].isNameMatch(names[x]))
                    dec = values[x];
            }
            if ((ra >= 0) && (ra <= 24) && (dec >= -90) && (dec <= 90))
            {
                // Check if it is already parked.
                if (CanPark())
                {
                    if (isParked())
                    {
                        DEBUG(Logger::DBG_WARNING,
                              "Please unpark the mount before issuing any motion/sync commands.");
                        EqNP.setState(lastEqState = IPS_IDLE);
                        EqNP.apply();
                        return false;
                    }
                }

                // Check if it can sync
                if (CanSync())
                {
                    if (CoordSP.isSwitchOn("SYNC"))
                    {
                        rc = Sync(ra, dec);
                        if (rc)
                            EqNP.setState(lastEqState = IPS_OK);
                        else
                            EqNP.setState(lastEqState = IPS_ALERT);
                        EqNP.apply();
                        return rc;
                    }
                }

                bool doFlip = false;
                if (CanFlip())
                {
                    doFlip = CoordSP.isSwitchOn("FLIP");
                }

                // Remember Track State
                RememberTrackState = TrackState;
                // Issue GOTO/Flip
                if(doFlip)
                {
                    rc = Flip(ra, dec);
                }
                else
                {
                    rc = Goto(ra, dec);
                }
                if (rc)
                {
                    EqNP.setState(lastEqState = IPS_BUSY);
                    //  Now fill in target co-ords, so domes can start turning
                    TargetNP[AXIS_RA].setValue(ra);
                    TargetNP[AXIS_DE].setValue(dec);
                    TargetNP.apply();
                }
                else
                {
                    EqNP.setState(lastEqState = IPS_ALERT);
                }
                EqNP.apply();
            }
            return rc;
        }

        ///////////////////////////////////
        // Geographic Coords
        ///////////////////////////////////
        if (LocationNP.isNameMatch(name))
        {
            int latindex       = IUFindIndex("LAT", names, n);
            int longindex      = IUFindIndex("LONG", names, n);
            int elevationindex = IUFindIndex("ELEV", names, n);

            if (latindex == -1 || longindex == -1 || elevationindex == -1)
            {
                LocationNP.setState(IPS_ALERT);
                LOG_ERROR("Location data missing or corrupted.");
                LocationNP.apply();
            }

            double targetLat  = values[latindex];
            double targetLong = values[longindex];
            double targetElev = values[elevationindex];

            return processLocationInfo(targetLat, targetLong, targetElev);
        }

        ///////////////////////////////////
        // Park Position
        ///////////////////////////////////
        if (ParkPositionNP.isNameMatch(name))
        {
            double axis1 = std::numeric_limits<double>::quiet_NaN(), axis2 = std::numeric_limits<double>::quiet_NaN();
            for (int x = 0; x < n; x++)
            {
                if (ParkPositionNP[AXIS_RA].isNameMatch(names[x]))
                {
                    axis1 = values[x];
                }
                else if (ParkPositionNP[AXIS_DE].isNameMatch(names[x]))
                {
                    axis2 = values[x];
                }
            }

            if (std::isnan(axis1) == false && std::isnan(axis2) == false)
            {
                bool rc = false;

                rc = SetParkPosition(axis1, axis2);

                if (rc)
                {
                    ParkPositionNP.update(values, names, n);
                    Axis1ParkPosition = ParkPositionNP[AXIS_RA].getValue();
                    Axis2ParkPosition = ParkPositionNP[AXIS_DE].getValue();
                }

                ParkPositionNP.setState(rc ? IPS_OK : IPS_ALERT);
            }
            else
                ParkPositionNP.setState(IPS_ALERT);

            ParkPositionNP.apply();
            return true;
        }

        ///////////////////////////////////
        // Track Rate
        ///////////////////////////////////
        if (TrackRateNP.isNameMatch(name))
        {
            double preAxis1 = TrackRateNP[AXIS_RA].getValue(), preAxis2 = TrackRateNP[AXIS_DE].getValue();
            auto currentTrackingMode = TrackModeSP.findOnSwitch();
            if (TrackRateNP.update(values, names, n) == false || currentTrackingMode == nullptr)
            {
                TrackRateNP.setState(IPS_ALERT);
                TrackRateNP.apply();
                return false;
            }

            if (TrackState == SCOPE_TRACKING && currentTrackingMode->isNameMatch("TRACK_CUSTOM"))
            {
                // Check that we do not abruptly change positive tracking rates to negative ones.
                // tracking must be stopped first.
                // Give warning is tracking sign would cause a reverse in direction
                if ( (preAxis1 * TrackRateNP[AXIS_RA].getValue() < 0) || (preAxis2 * TrackRateNP[AXIS_DE].getValue() < 0) )
                {
                    LOG_ERROR("Cannot reverse tracking while tracking is engaged. Disengage tracking then try again.");
                    return false;
                }

                // All is fine, ask mount to change tracking rate
                if (SetTrackRate(TrackRateNP[AXIS_RA].getValue(), TrackRateNP[AXIS_DE].getValue()) == false)
                {
                    TrackRateNP[AXIS_RA].setValue(preAxis1);
                    TrackRateNP[AXIS_DE].setValue(preAxis2);
                    TrackRateNP.setState(IPS_ALERT);
                }
                else
                    TrackRateNP.setState(IPS_OK);
            }

            // If we are already tracking but tracking mode is NOT custom
            // We just inform the user that it must be set to custom for these values to take
            // effect.
            if (TrackState == SCOPE_TRACKING && !currentTrackingMode->isNameMatch("TRACK_CUSTOM"))
            {
                LOG_INFO("Custom tracking rates set. Tracking mode must be set to Custom for these rates to take effect.");
            }

            // If mount is NOT tracking, we simply accept whatever valid values for use when mount tracking is engaged.
            TrackRateNP.apply();
            return true;
        }
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
bool Telescope::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //  This one is for us
        if (CoordSP.isNameMatch(name))
        {
            //  client is telling us what to do with co-ordinate requests
            CoordSP.setState(IPS_OK);
            CoordSP.update(states, names, n);
            //  Update client display
            CoordSP.apply();
            return true;
        }

        ///////////////////////////////////
        // Slew Rate
        ///////////////////////////////////
        if (SlewRateSP.isNameMatch(name))
        {
            int preIndex = SlewRateSP.findOnSwitchIndex();
            SlewRateSP.update(states, names, n);
            int nowIndex = SlewRateSP.findOnSwitchIndex();
            if (SetSlewRate(nowIndex) == false)
            {
                SlewRateSP.reset();
                SlewRateSP[preIndex].setState(ISS_ON);
                SlewRateSP.setState(IPS_ALERT);
            }
            else
                SlewRateSP.setState(IPS_OK);
            SlewRateSP.apply();
            return true;
        }

        ///////////////////////////////////
        // Parking
        ///////////////////////////////////
        if (ParkSP.isNameMatch(name))
        {
            if (TrackState == SCOPE_PARKING)
            {
                ParkSP.reset();
                ParkSP.setState(IPS_ALERT);
                Abort();
                LOG_INFO("Parking/Unparking aborted.");
                ParkSP.apply();
                return true;
            }

            int preIndex = ParkSP.findOnSwitchIndex();
            ParkSP.update(states, names, n);

            bool toPark = (ParkSP[PARK].getState() == ISS_ON);

            if (toPark == false && TrackState != SCOPE_PARKED)
            {
                ParkSP.reset();
                ParkSP[UNPARK].setState(ISS_ON);
                ParkSP.setState(IPS_IDLE);
                LOG_INFO("Telescope already unparked.");
                IsParked = false;
                ParkSP.apply();
                return true;
            }

            if (toPark == false && isLocked())
            {
                ParkSP.reset();
                ParkSP[PARK].setState(ISS_ON);
                ParkSP.setState(IPS_ALERT);
                LOG_WARN("Cannot unpark mount when dome is locking. See: Dome Policy in options tab.");
                IsParked = true;
                ParkSP.apply();
                return true;
            }

            if (toPark && TrackState == SCOPE_PARKED)
            {
                ParkSP.reset();
                ParkSP[PARK].setState(ISS_ON);
                ParkSP.setState(IPS_IDLE);
                LOG_INFO("Telescope already parked.");
                ParkSP.apply();
                return true;
            }

            RememberTrackState = TrackState;

            ParkSP.reset();
            bool rc = toPark ? Park() : UnPark();
            if (rc)
            {
                if (TrackState == SCOPE_PARKING)
                {
                    ParkSP[PARK].setState(toPark ? ISS_ON : ISS_OFF);
                    ParkSP[UNPARK].setState(toPark ? ISS_OFF : ISS_ON);
                    ParkSP.setState(IPS_BUSY);
                }
                else
                {
                    ParkSP[PARK].setState(toPark ? ISS_ON : ISS_OFF);
                    ParkSP[UNPARK].setState(toPark ? ISS_OFF : ISS_ON);
                    ParkSP.setState(IPS_OK);
                }
            }
            else
            {
                ParkSP[preIndex].setState(ISS_ON);
                ParkSP.setState(IPS_ALERT);
            }

            ParkSP.apply();
            return true;
        }

        ///////////////////////////////////
        // NS Motion
        ///////////////////////////////////
        if (MovementNSSP.isNameMatch(name))
        {
            // Check if it is already parked.
            if (CanPark())
            {
                if (isParked())
                {
                    DEBUG(Logger::DBG_WARNING,
                          "Please unpark the mount before issuing any motion/sync commands.");
                    MovementNSSP.setState(IPS_IDLE);
                    MovementNSSP.apply();
                    return false;
                }
            }

            MovementNSSP.update(states, names, n);

            int current_motion = MovementNSSP.findOnSwitchIndex();

            // if same move requested, return
            if (MovementNSSP.getState() == IPS_BUSY && current_motion == last_ns_motion)
                return true;

            // Time to stop motion
            if (current_motion == -1 || (last_ns_motion != -1 && current_motion != last_ns_motion))
            {
                auto targetDirection = last_ns_motion == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH;
                if (ReverseMovementSP[REVERSE_NS].getState() == ISS_ON)
                    targetDirection = targetDirection == DIRECTION_NORTH ? DIRECTION_SOUTH : DIRECTION_NORTH;

                if (MoveNS(targetDirection, MOTION_STOP))
                {
                    MovementNSSP.reset();
                    MovementNSSP.setState(IPS_IDLE);
                    last_ns_motion = -1;
                    // Update Target when stopped so that domes can track
                    TargetNP[AXIS_RA].setValue(EqNP[AXIS_RA].getValue());
                    TargetNP[AXIS_DE].setValue(EqNP[AXIS_DE].getValue());
                    TargetNP.apply();
                }
                else
                    MovementNSSP.setState(IPS_ALERT);
            }
            else
            {
                if (TrackState != SCOPE_SLEWING && TrackState != SCOPE_PARKING)
                    RememberTrackState = TrackState;

                auto targetDirection = current_motion == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH;
                if (ReverseMovementSP[REVERSE_NS].getState() == ISS_ON)
                    targetDirection = targetDirection == DIRECTION_NORTH ? DIRECTION_SOUTH : DIRECTION_NORTH;

                if (MoveNS(targetDirection, MOTION_START))
                {
                    MovementNSSP.setState(IPS_BUSY);
                    last_ns_motion = targetDirection;
                }
                else
                {
                    MovementNSSP.reset();
                    MovementNSSP.setState(IPS_ALERT);
                    last_ns_motion = -1;
                }
            }

            MovementNSSP.apply();

            return true;
        }

        ///////////////////////////////////
        // WE Motion
        ///////////////////////////////////
        if (MovementWESP.isNameMatch(name))
        {
            // Check if it is already parked.
            if (CanPark())
            {
                if (isParked())
                {
                    DEBUG(Logger::DBG_WARNING,
                          "Please unpark the mount before issuing any motion/sync commands.");
                    MovementWESP.setState(IPS_IDLE);
                    MovementWESP.apply();
                    return false;
                }
            }

            MovementWESP.update(states, names, n);

            int current_motion = MovementWESP.findOnSwitchIndex();

            // if same move requested, return
            if (MovementWESP.getState() == IPS_BUSY && current_motion == last_we_motion)
                return true;

            // Time to stop motion
            if (current_motion == -1 || (last_we_motion != -1 && current_motion != last_we_motion))
            {
                auto targetDirection = last_we_motion == 0 ? DIRECTION_WEST : DIRECTION_EAST;
                if (ReverseMovementSP[REVERSE_WE].getState() == ISS_ON)
                    targetDirection = targetDirection == DIRECTION_EAST ? DIRECTION_WEST : DIRECTION_EAST;
                if (MoveWE(targetDirection, MOTION_STOP))
                {
                    MovementWESP.reset();
                    MovementWESP.setState(IPS_IDLE);
                    last_we_motion = -1;
                    // Update Target when stopped so that domes can track
                    TargetNP[AXIS_RA].setValue(EqNP[AXIS_RA].getValue());
                    TargetNP[AXIS_DE].setValue(EqNP[AXIS_DE].getValue());
                    TargetNP.apply();
                }
                else
                    MovementWESP.setState(IPS_ALERT);
            }
            else
            {
                if (TrackState != SCOPE_SLEWING && TrackState != SCOPE_PARKING)
                    RememberTrackState = TrackState;

                auto targetDirection = current_motion == 0 ? DIRECTION_WEST : DIRECTION_EAST;
                if (ReverseMovementSP[REVERSE_WE].getState() == ISS_ON)
                    targetDirection = targetDirection == DIRECTION_EAST ? DIRECTION_WEST : DIRECTION_EAST;

                if (MoveWE(targetDirection, MOTION_START))
                {
                    MovementWESP.setState(IPS_BUSY);
                    last_we_motion = targetDirection;
                }
                else
                {
                    MovementWESP.reset();
                    MovementWESP.setState(IPS_ALERT);
                    last_we_motion = -1;
                }
            }

            MovementWESP.apply();

            return true;
        }

        ///////////////////////////////////
        // WE or NS Reverse Motion
        ///////////////////////////////////
        if (ReverseMovementSP.isNameMatch(name))
        {
            ReverseMovementSP.update(states, names, n);
            ReverseMovementSP.setState(IPS_OK);
            ReverseMovementSP.apply();
            saveConfig(true, ReverseMovementSP.getName());
            return true;
        }

        ///////////////////////////////////
        // Homing
        ///////////////////////////////////
        if (HomeSP.isNameMatch(name))
        {
            auto onSwitchName = IUFindOnSwitchName(states, names, n);
            // No switch selected. Cancel whatever action if any and return
            if (onSwitchName == NULL)
            {
                if (HomeSP.getState() == IPS_BUSY)
                    Abort();
                HomeSP.reset();
                HomeSP.setState(IPS_IDLE);
                HomeSP.apply();
                return true;
            }
            auto actionString = std::string(onSwitchName);
            auto action = HOME_FIND;
            if (actionString == "SET")
                action = HOME_SET;
            else if (actionString == "GO")
                action = HOME_GO;

            if (isParked())
            {
                HomeSP.setState(IPS_ALERT);
                HomeSP.reset();
                HomeSP.apply();
                LOG_ERROR("Cannot home while parked.");
                return true;
            }

            auto state = ExecuteHomeAction(action);
            HomeSP.setState(state);
            if (state == IPS_BUSY)
                HomeSP.update(states, names, n);
            HomeSP.apply();
            return true;
        }

        ///////////////////////////////////
        // Abort Motion
        ///////////////////////////////////
        if (AbortSP.isNameMatch(name))
        {
            AbortSP.reset();

            if (Abort())
            {
                AbortSP.setState(IPS_OK);

                if (ParkSP.getState() == IPS_BUSY)
                {
                    ParkSP.reset();
                    ParkSP.setState(IPS_ALERT);
                    ParkSP.apply();

                    LOG_INFO("Parking aborted.");
                }
                if (EqNP.getState() == IPS_BUSY)
                {
                    EqNP.setState(lastEqState = IPS_IDLE);
                    EqNP.apply();
                    LOG_INFO("Slew/Track aborted.");
                }
                if (MovementWESP.getState() == IPS_BUSY)
                {
                    MovementWESP.reset();
                    MovementWESP.setState(IPS_IDLE);
                    MovementWESP.apply();
                }
                if (MovementNSSP.getState() == IPS_BUSY)
                {
                    MovementNSSP.reset();
                    MovementNSSP.setState(IPS_IDLE);
                    MovementNSSP.apply();
                }

                last_ns_motion = last_we_motion = -1;

                // JM 2017-07-28: Abort shouldn't affect tracking state. It should affect motion and that's it.
                //if (TrackState != SCOPE_PARKED)
                //TrackState = SCOPE_IDLE;
                // For Idle, Tracking, Parked state, we do not change its status, it should remain as is.
                // For Slewing & Parking, state should go back to last remembered state.
                if (TrackState == SCOPE_SLEWING || TrackState == SCOPE_PARKING)
                {
                    TrackState = RememberTrackState;
                }
            }
            else
                AbortSP.setState(IPS_ALERT);

            AbortSP.apply();

            return true;
        }

        ///////////////////////////////////
        // Track Mode
        ///////////////////////////////////
        if (TrackModeSP.isNameMatch(name))
        {
            int prevIndex = TrackModeSP.findOnSwitchIndex();
            TrackModeSP.update(states, names, n);
            int currIndex = TrackModeSP.findOnSwitchIndex();
            // If same as previous index, or if scope is already idle, then just update switch and return. No commands are sent to the mount.
            if (prevIndex == currIndex || TrackState == SCOPE_IDLE)
            {
                TrackModeSP.setState(IPS_OK);
                TrackModeSP.apply(nullptr);
                return true;
            }

            if (TrackState == SCOPE_PARKED)
            {
                DEBUG(Logger::DBG_WARNING, "Telescope is Parked, Unpark before changing track mode.");
                return false;
            }

            if (SetTrackMode(currIndex))
                TrackModeSP.setState(IPS_OK);
            else
            {
                TrackModeSP.reset();
                TrackModeSP[prevIndex].setState(ISS_ON);
                TrackModeSP.setState(IPS_ALERT);
            }
            TrackModeSP.apply();
            return false;
        }

        ///////////////////////////////////
        // Track State
        ///////////////////////////////////
        if (TrackStateSP.isNameMatch(name))
        {
            int previousState = TrackStateSP.findOnSwitchIndex();
            TrackStateSP.update(states, names, n);
            int targetState = TrackStateSP.findOnSwitchIndex();

            if (previousState == targetState)
            {
                TrackStateSP.apply();
                return true;
            }

            if (TrackState == SCOPE_PARKED)
            {
                DEBUG(Logger::DBG_WARNING, "Telescope is Parked, Unpark before tracking.");
                return false;
            }

            if (SetTrackEnabled((targetState == TRACK_ON) ? true : false))
            {
                TrackState = (targetState == TRACK_ON) ? SCOPE_TRACKING : SCOPE_IDLE;
                TrackStateSP.setState((targetState == TRACK_ON) ? IPS_BUSY : IPS_IDLE);
                TrackStateSP[TRACK_ON].setState((targetState == TRACK_ON) ? ISS_ON : ISS_OFF);
                TrackStateSP[TRACK_OFF].setState((targetState == TRACK_ON) ? ISS_OFF : ISS_ON);
            }
            else
            {
                TrackStateSP.setState(IPS_ALERT);
                TrackStateSP.reset();
                TrackStateSP[previousState].setState(ISS_ON);
            }

            TrackStateSP.apply();
            return true;
        }

        ///////////////////////////////////
        // Park Options
        ///////////////////////////////////
        if (ParkOptionSP.isNameMatch(name))
        {
            ParkOptionSP.update(states, names, n);
            auto index = ParkOptionSP.findOnSwitchIndex();
            if (index == -1)
                return false;

            ParkOptionSP.reset();

            bool rc = false;

            if ((TrackState != SCOPE_IDLE && TrackState != SCOPE_TRACKING) || MovementNSSP.getState() == IPS_BUSY ||
                    MovementWESP.getState() == IPS_BUSY)
            {
                LOG_INFO("Can not change park position while slewing or already parked.");
                ParkOptionSP.setState(IPS_ALERT);
                ParkOptionSP.apply();
                return false;
            }

            switch (index)
            {
                case PARK_CURRENT:
                    rc = SetCurrentPark();
                    break;
                case PARK_DEFAULT:
                    rc = SetDefaultPark();
                    break;
                case PARK_WRITE_DATA:
                    rc = WriteParkData();
                    if (rc)
                        LOG_INFO("Saved Park Status/Position.");
                    else
                        DEBUG(Logger::DBG_WARNING, "Can not save Park Status/Position.");
                    break;
                case PARK_PURGE_DATA:
                    rc = PurgeParkData();
                    if (rc)
                        LOG_INFO("Park data purged.");
                    else
                        DEBUG(Logger::DBG_WARNING, "Can not purge Park Status/Position.");
                    break;
            }

            ParkOptionSP.setState(rc ? IPS_OK : IPS_ALERT);
            ParkOptionSP.apply();
            return true;
        }

        ///////////////////////////////////
        // Parking Dome Policy
        ///////////////////////////////////
        if (DomePolicySP.isNameMatch(name))
        {
            DomePolicySP.update(states, names, n);
            if (DomePolicySP[DOME_IGNORED].getState() == ISS_ON)
                LOG_INFO("Dome Policy set to: Dome ignored. Mount can park or unpark regardless of dome parking state.");
            else
                LOG_WARN("Dome Policy set to: Dome locks. This prevents the mount from unparking when dome is parked.");
#if 0
            else if (!strcmp(names[0], DomeClosedLockT[2].name))
                LOG_INFO("Warning: Dome parking policy set to: Dome parks. This tells "
                         "scope to park if dome is parking. This will disable the locking "
                         "for dome parking, EVEN IF MOUNT PARKING FAILS");
            else if (!strcmp(names[0], DomeClosedLockT[3].name))
                LOG_INFO("Warning: Dome parking policy set to: Both. This disallows the "
                         "scope from unparking when dome is parked, and tells scope to "
                         "park if dome is parking. This will disable the locking for dome "
                         "parking, EVEN IF MOUNT PARKING FAILS.");
#endif
            DomePolicySP.setState(IPS_OK);
            DomePolicySP.apply( );
            triggerSnoop(ActiveDeviceTP[ACTIVE_DOME].getText(), "DOME_PARK");
            return true;
        }

        ///////////////////////////////////
        // Simulate Pier Side
        // This ia a major change to the design of the simulated scope, it might not handle changes on the fly
        ///////////////////////////////////
        if (SimulatePierSideSP.isNameMatch(name))
        {
            SimulatePierSideSP.update(states, names, n);
            int index = SimulatePierSideSP.findOnSwitchIndex();
            if (index == -1)
            {
                SimulatePierSideSP.setState(IPS_ALERT);
                LOG_INFO("Cannot determine whether pier side simulation should be switched on or off.");
                SimulatePierSideSP.apply();
                return false;
            }

            bool pierSideEnabled = index == 0;

            LOGF_INFO("Simulating Pier Side %s.", (pierSideEnabled ? "enabled" : "disabled"));

            setSimulatePierSide(pierSideEnabled);
            if (pierSideEnabled)
            {
                // set the pier side from the current Ra
                // assumes we haven't tracked across the meridian
                setPierSide(expectedPierSide(EqNP[AXIS_RA].getValue()));
            }
            return true;
        }

        ///////////////////////////////////
        // Joystick Motion Control Mode
        ///////////////////////////////////
        if (MotionControlModeTP.isNameMatch(name))
        {
            MotionControlModeTP.update(states, names, n);
            MotionControlModeTP.setState(IPS_OK);
            MotionControlModeTP.apply();
            if (MotionControlModeTP[MOTION_CONTROL_MODE_JOYSTICK].getState() == ISS_ON)
                LOG_INFO("Motion control is set to 4-way joystick.");
            else if (MotionControlModeTP[MOTION_CONTROL_MODE_AXES].getState() == ISS_ON)
                LOG_INFO("Motion control is set to 2 separate axes.");
            else
                DEBUGF(Logger::DBG_WARNING, "Motion control is set to unknown value %d!", n);
            return true;
        }

        ///////////////////////////////////
        // Joystick Lock Axis
        ///////////////////////////////////
        if (LockAxisSP.isNameMatch(name))
        {
            LockAxisSP.update(states, names, n);
            LockAxisSP.setState(IPS_OK);
            LockAxisSP.apply();
            if (LockAxisSP[AXIS_RA].getState() == ISS_ON)
                LOG_INFO("Joystick motion is locked to West/East axis only.");
            else if (LockAxisSP[AXIS_DE].getState() == ISS_ON)
                LOG_INFO("Joystick motion is locked to North/South axis only.");
            else
                LOG_INFO("Joystick motion is unlocked.");
            return true;
        }
    }

    bool rc = controller->ISNewSwitch(dev, name, states, names, n);
    if (rc)
    {
        auto useJoystick = getSwitch("USEJOYSTICK");
        if (useJoystick && useJoystick[0].getState() == ISS_ON)
        {
            defineProperty(MotionControlModeTP);
            defineProperty(LockAxisSP);
        }
        else
        {
            deleteProperty(MotionControlModeTP);
            deleteProperty(LockAxisSP);
        }

    }

    //  Nobody has claimed this, so, ignore it
    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool Telescope::callHandshake()
{
    if (telescopeConnection > 0)
    {
        if (getActiveConnection() == serialConnection)
            PortFD = serialConnection->getPortFD();
        else if (getActiveConnection() == tcpConnection)
            PortFD = tcpConnection->getPortFD();
    }

    return Handshake();
}

bool Telescope::Handshake()
{
    /* Test connection */
    return ReadScopeStatus();
}

void Telescope::TimerHit()
{
    if (isConnected())
    {
        bool rc;

        rc = ReadScopeStatus();

        if (!rc)
        {
            //  read was not good
            EqNP.setState(lastEqState = IPS_ALERT);
            EqNP.apply();
        }

        SetTimer(getCurrentPollingPeriod());
    }
}

bool Telescope::Flip(double ra, double dec)
{
    INDI_UNUSED(ra);
    INDI_UNUSED(dec);

    DEBUG(Logger::DBG_WARNING, "Flip is not supported.");
    return false;
}

bool Telescope::Goto(double ra, double dec)
{
    INDI_UNUSED(ra);
    INDI_UNUSED(dec);

    DEBUG(Logger::DBG_WARNING, "GOTO is not supported.");
    return false;
}

bool Telescope::Abort()
{
    DEBUG(Logger::DBG_WARNING, "Abort is not supported.");
    return false;
}

bool Telescope::Park()
{
    DEBUG(Logger::DBG_WARNING, "Parking is not supported.");
    return false;
}

bool Telescope::UnPark()
{
    DEBUG(Logger::DBG_WARNING, "UnParking is not supported.");
    return false;
}

bool Telescope::SetTrackMode(uint8_t mode)
{
    INDI_UNUSED(mode);
    DEBUG(Logger::DBG_WARNING, "Tracking mode is not supported.");
    return false;
}

bool Telescope::SetTrackRate(double raRate, double deRate)
{
    INDI_UNUSED(raRate);
    INDI_UNUSED(deRate);
    DEBUG(Logger::DBG_WARNING, "Custom tracking rates is not supported.");
    return false;
}

bool Telescope::SetTrackEnabled(bool enabled)
{
    INDI_UNUSED(enabled);
    DEBUG(Logger::DBG_WARNING, "Tracking state is not supported.");
    return false;
}

int Telescope::AddTrackMode(const char *name, const char *label, bool isDefault)
{
    INDI::WidgetSwitch node;
    node.fill(name, label, isDefault ? ISS_ON : ISS_OFF);
    TrackModeSP.push(std::move(node));
    return (TrackModeSP.count() - 1);
}

bool Telescope::SetCurrentPark()
{
    DEBUG(Logger::DBG_WARNING, "Parking is not supported.");
    return false;
}

bool Telescope::SetDefaultPark()
{
    DEBUG(Logger::DBG_WARNING, "Parking is not supported.");
    return false;
}

bool Telescope::processTimeInfo(const char *utc, const char *offset)
{
    struct ln_date utc_date;
    double utc_offset = 0;

    if (extractISOTime(utc, &utc_date) == -1)
    {
        TimeTP.setState(IPS_ALERT);
        LOGF_ERROR("Date/Time is invalid: %s.", utc);
        TimeTP.apply();
        return false;
    }

    utc_offset = atof(offset);

    if (updateTime(&utc_date, utc_offset))
    {
        TimeTP[UTC].setText(utc);
        TimeTP[OFFSET].setText(offset);
        TimeTP.setState(IPS_OK);
        TimeTP.apply();
        return true;
    }
    else
    {
        TimeTP.setState(IPS_ALERT);
        TimeTP.apply();
        return false;
    }
}

bool Telescope::processLocationInfo(double latitude, double longitude, double elevation)
{
    // Do not update if not necessary
    // JM 2021-05-26: This can sometimes be problematic. Let child driver deals with duplicate requests.
#if 0
    if (latitude == LocationN[LOCATION_LATITUDE].value && longitude == LocationN[LOCATION_LONGITUDE].value &&
            elevation == LocationN[LOCATION_ELEVATION].value)
    {
        LocationNP.s = IPS_OK;
        IDSetNumber(&LocationNP, nullptr);
        return true;
    }
    else
#endif
        if (latitude == 0 && longitude == 0)
        {
            LOG_DEBUG("Silently ignoring invalid latitude and longitude.");
            LocationNP.setState(IPS_IDLE);
            LocationNP.apply();
            return false;
        }

    if (updateLocation(latitude, longitude, elevation))
    {
        LocationNP.setState(IPS_OK);
        LocationNP[LOCATION_LATITUDE].setValue(latitude);
        LocationNP[LOCATION_LONGITUDE].setValue(longitude);
        LocationNP[LOCATION_ELEVATION].setValue(elevation);
        //  Update client display
        LocationNP.apply();

        // Always save geographic coord config immediately.
        saveConfig(true, "GEOGRAPHIC_COORD");

        updateObserverLocation(latitude, longitude, elevation);

        return true;
    }
    else
    {
        LocationNP.setState(IPS_ALERT);
        //  Update client display
        LocationNP.apply();

        return false;
    }
}

bool Telescope::updateTime(ln_date *utc, double utc_offset)
{
    INDI_UNUSED(utc);
    INDI_UNUSED(utc_offset);
    return true;
}

bool Telescope::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(latitude);
    INDI_UNUSED(longitude);
    INDI_UNUSED(elevation);
    return true;
}

IPState Telescope::ExecuteHomeAction(TelescopeHomeAction action)
{
    INDI_UNUSED(action);
    return IPS_ALERT;
}

void Telescope::updateObserverLocation(double latitude, double longitude, double elevation)
{
    m_Location.longitude = longitude;
    m_Location.latitude  = latitude;
    m_Location.elevation = elevation;
    char lat_str[MAXINDIFORMAT] = {0}, lng_str[MAXINDIFORMAT] = {0};

    // Make display longitude to be in the standard 0 to +180 East, and 0 to -180 West.
    // No need to confuse new users with INDI format.
    double display_longitude = longitude > 180 ? longitude - 360 : longitude;
    fs_sexa(lat_str, m_Location.latitude, 2, 36000);
    fs_sexa(lng_str, display_longitude, 2, 36000);
    // Choose WGS 84, also known as EPSG:4326 for latitude/longitude ordering
    LOGF_INFO("Observer location updated: Latitude %.12s (%.2f) Longitude %.12s (%.2f)", lat_str, m_Location.latitude, lng_str,
              display_longitude);
}

bool Telescope::SetParkPosition(double Axis1Value, double Axis2Value)
{
    INDI_UNUSED(Axis1Value);
    INDI_UNUSED(Axis2Value);
    return true;
}

void Telescope::generateCoordSet()
{
    CoordSP.resize(0);

    INDI::WidgetSwitch node;
    node.fill("TRACK", "Track", ISS_ON);
    CoordSP.push(std::move(node));

    if(CanGOTO())
    {
        INDI::WidgetSwitch node;
        node.fill("SLEW", "Slew", ISS_OFF);
        CoordSP.push(std::move(node));
    }

    if(CanSync())
    {
        INDI::WidgetSwitch node;
        node.fill("SYNC", "Sync", ISS_OFF);
        CoordSP.push(std::move(node));

    }

    if(CanFlip())
    {
        INDI::WidgetSwitch node;
        node.fill("FLIP", "Flip", ISS_OFF);
        CoordSP.push(std::move(node));
    }
}

void Telescope::SetTelescopeCapability(uint32_t cap, uint8_t slewRateCount)
{
    capability = cap;
    nSlewRate  = slewRateCount;

    generateCoordSet();

    if (nSlewRate >= 4)
    {
        SlewRateSP.resize(0);
        INDI::WidgetSwitch node;
        //int step  = nSlewRate / 4;
        for (int i = 0; i < nSlewRate; i++)
        {
            // char name[4];
            auto name = std::to_string(i + 1) + "x";
            // snprintf(name, 4, "%dx", i + 1);
            // IUFillSwitch(SlewRateS + i, name, name, ISS_OFF);
            node.fill(name, name, ISS_OFF);
            SlewRateSP.push(std::move(node));
        }

        // If number of slew rate is EXACTLY 4, then let's use common labels
        if (nSlewRate == 4)
        {
            // strncpy((SlewRateS + (0))->label, "Guide", MAXINDILABEL);
            SlewRateSP[0].setLabel("Guide");
            SlewRateSP[1].setLabel("Centering");
            SlewRateSP[2].setLabel("Find");
            SlewRateSP[3].setLabel("Max");
        }

        // By Default we set current Slew Rate to 0.5 of max
        SlewRateSP[nSlewRate / 2].setState(ISS_ON);

        SlewRateSP.fill(getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate",
                        MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    }

    if (CanHomeFind() || CanHomeSet() || CanHomeGo())
    {
        HomeSP.resize(0);
        if (CanHomeFind())
        {
            INDI::WidgetSwitch node;
            node.fill("FIND", "Find", ISS_OFF);
            HomeSP.push(std::move(node));
        }

        if (CanHomeSet())
        {
            INDI::WidgetSwitch node;
            node.fill("SET", "Set", ISS_OFF);
            HomeSP.push(std::move(node));
        }

        if (CanHomeGo())
        {
            INDI::WidgetSwitch node;
            node.fill("GO", "Go", ISS_OFF);
            HomeSP.push(std::move(node));
        }

        HomeSP.shrink_to_fit();
        HomeSP.fill(getDeviceName(), "TELESCOPE_HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    }
}

void Telescope::SetParkDataType(TelescopeParkData type)
{
    parkDataType = type;

    if (parkDataType != PARK_NONE && parkDataType != PARK_SIMPLE)
    {
        switch (parkDataType)
        {
            case PARK_RA_DEC:
                ParkPositionNP[AXIS_RA].fill("PARK_RA", "RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
                ParkPositionNP[AXIS_DE].fill("PARK_DEC", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
                break;

            case PARK_HA_DEC:
                ParkPositionNP[AXIS_RA].fill("PARK_HA", "HA (hh:mm:ss)", "%010.6m", -12, 12, 0, 0);
                ParkPositionNP[AXIS_DE].fill("PARK_DEC", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
                break;

            case PARK_AZ_ALT:
                ParkPositionNP[AXIS_AZ].fill("PARK_AZ", "AZ D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
                ParkPositionNP[AXIS_ALT].fill("PARK_ALT", "Alt  D:M:S", "%10.6m", -90., 90.0, 0.0, 0);
                break;

            case PARK_RA_DEC_ENCODER:
                ParkPositionNP[AXIS_RA].fill("PARK_RA", "RA Encoder", "%.0f", 0, 16777215, 1, 0);
                ParkPositionNP[AXIS_DE].fill("PARK_DEC", "DEC Encoder", "%.0f", 0, 16777215, 1, 0);
                break;

            case PARK_AZ_ALT_ENCODER:
                ParkPositionNP[AXIS_RA].fill("PARK_AZ", "AZ Encoder", "%.0f", 0, 16777215, 1, 0);
                ParkPositionNP[AXIS_DE].fill("PARK_ALT", "ALT Encoder", "%.0f", 0, 16777215, 1, 0);
                break;

            default:
                break;
        }

        ParkPositionNP.fill(getDeviceName(), "TELESCOPE_PARK_POSITION", "Park Position", SITE_TAB, IP_RW, 60, IPS_IDLE);
    }
}

void Telescope::SyncParkStatus(bool isparked)
{
    IsParked = isparked;
    ParkSP.reset();
    ParkSP.setState(IPS_OK);

    if (IsParked)
    {
        ParkSP[PARK].setState(ISS_ON);
        TrackState = SCOPE_PARKED;
        LOG_INFO("Mount is parked.");
    }
    else
    {
        ParkSP[UNPARK].setState(ISS_ON);
        TrackState = SCOPE_IDLE;
        LOG_INFO("Mount is unparked.");
    }

    ParkSP.apply();
}

void Telescope::SetParked(bool isparked)
{
    SyncParkStatus(isparked);

    if (parkDataType != PARK_NONE)
        WriteParkData();
}

bool Telescope::isParked()
{
    return IsParked;
}

bool Telescope::InitPark()
{
    const char *loadres = LoadParkData();
    if (loadres)
    {
        LOGF_INFO("InitPark: No Park data in file %s: %s", ParkDataFileName.c_str(), loadres);
        SyncParkStatus(false);
        return false;
    }

    SyncParkStatus(isParked());

    if (parkDataType != PARK_SIMPLE)
    {
        LOGF_DEBUG("InitPark Axis1 %.2f Axis2 %.2f", Axis1ParkPosition, Axis2ParkPosition);
        ParkPositionNP[AXIS_RA].setValue(Axis1ParkPosition);
        ParkPositionNP[AXIS_DE].setValue(Axis2ParkPosition);
        ParkPositionNP.apply();
    }

    return true;
}

const char *Telescope::LoadParkXML()
{
    wordexp_t wexp;
    FILE *fp = nullptr;
    LilXML *lp = nullptr;
    static char errmsg[512];

    XMLEle *parkxml = nullptr;
    XMLAtt *ap = nullptr;
    bool devicefound = false;

    ParkDeviceName       = getDeviceName();
    ParkstatusXml        = nullptr;
    ParkdeviceXml        = nullptr;
    ParkpositionXml      = nullptr;
    ParkpositionAxis1Xml = nullptr;
    ParkpositionAxis2Xml = nullptr;

    if (wordexp(ParkDataFileName.c_str(), &wexp, 0))
    {
        wordfree(&wexp);
        return "Badly formed filename.";
    }

    if (!(fp = fopen(wexp.we_wordv[0], "r")))
    {
        wordfree(&wexp);
        return strerror(errno);
    }
    wordfree(&wexp);

    lp = newLilXML();

    if (ParkdataXmlRoot)
        delXMLEle(ParkdataXmlRoot);

    ParkdataXmlRoot = readXMLFile(fp, lp, errmsg);
    fclose(fp);

    delLilXML(lp);
    if (!ParkdataXmlRoot)
        return errmsg;

    parkxml = nextXMLEle(ParkdataXmlRoot, 1);

    if (!parkxml)
        return "Empty park file.";

    if (!strcmp(tagXMLEle(parkxml), "parkdata"))
    {
        delXMLEle(parkxml);
        return "Not a park data file";
    }

    while (parkxml)
    {
        if (strcmp(tagXMLEle(parkxml), "device"))
        {
            parkxml = nextXMLEle(ParkdataXmlRoot, 0);
            continue;
        }
        ap = findXMLAtt(parkxml, "name");
        if (ap && (!strcmp(valuXMLAtt(ap), ParkDeviceName)))
        {
            devicefound = true;
            break;
        }
        parkxml = nextXMLEle(ParkdataXmlRoot, 0);
    }

    if (!devicefound)
    {
        delXMLEle(parkxml);
        return "No park data found for this device";
    }

    ParkdeviceXml        = parkxml;
    ParkstatusXml        = findXMLEle(parkxml, "parkstatus");

    if (parkDataType != PARK_SIMPLE)
    {
        ParkpositionXml      = findXMLEle(parkxml, "parkposition");
        if (ParkpositionXml)
            ParkpositionAxis1Xml = findXMLEle(ParkpositionXml, "axis1position");
        if (ParkpositionXml)
            ParkpositionAxis2Xml = findXMLEle(ParkpositionXml, "axis2position");

        if (ParkstatusXml == nullptr || ParkpositionAxis1Xml == nullptr || ParkpositionAxis2Xml == nullptr)
        {
            return "Park data invalid or missing.";
        }
    }
    else if (ParkstatusXml == nullptr)
    {
        return "Park data invalid or missing.";
    }

    return nullptr;
}

const char *Telescope::LoadParkData()
{
    IsParked = false;

    const char *result = LoadParkXML();
    if (result != nullptr)
        return result;

    if (!strcmp(pcdataXMLEle(ParkstatusXml), "true"))
        IsParked = true;

    if (parkDataType != PARK_SIMPLE)
    {
        double axis1Pos = std::numeric_limits<double>::quiet_NaN();
        double axis2Pos = std::numeric_limits<double>::quiet_NaN();

        int rc = sscanf(pcdataXMLEle(ParkpositionAxis1Xml), "%lf", &axis1Pos);
        if (rc != 1)
        {
            return "Unable to parse Park Position Axis 1.";
        }
        rc = sscanf(pcdataXMLEle(ParkpositionAxis2Xml), "%lf", &axis2Pos);
        if (rc != 1)
        {
            return "Unable to parse Park Position Axis 2.";
        }

        if (std::isnan(axis1Pos) == false && std::isnan(axis2Pos) == false)
        {
            Axis1ParkPosition = axis1Pos;
            Axis2ParkPosition = axis2Pos;
            return nullptr;
        }

        return "Failed to parse Park Position.";
    }

    return nullptr;
}

bool Telescope::PurgeParkData()
{
    // We need to refresh parking data in case other devices parking states were updated since we
    // read the data the first time.
    if (LoadParkXML() != nullptr)
        LOG_DEBUG("Failed to refresh parking data.");

    wordexp_t wexp;
    FILE *fp = nullptr;
    LilXML *lp = nullptr;
    static char errmsg[512];

    XMLEle *parkxml = nullptr;
    XMLAtt *ap = nullptr;
    bool devicefound = false;

    ParkDeviceName = getDeviceName();

    if (wordexp(ParkDataFileName.c_str(), &wexp, 0))
    {
        wordfree(&wexp);
        return false;
    }

    if (!(fp = fopen(wexp.we_wordv[0], "r")))
    {
        wordfree(&wexp);
        LOGF_ERROR("Failed to purge park data: %s", strerror(errno));
        return false;
    }
    wordfree(&wexp);

    lp = newLilXML();

    if (ParkdataXmlRoot)
        delXMLEle(ParkdataXmlRoot);

    ParkdataXmlRoot = readXMLFile(fp, lp, errmsg);
    fclose(fp);

    delLilXML(lp);
    if (!ParkdataXmlRoot)
        return false;

    parkxml = nextXMLEle(ParkdataXmlRoot, 1);

    if (!parkxml)
        return false;

    if (!strcmp(tagXMLEle(parkxml), "parkdata"))
    {
        delXMLEle(parkxml);
        return false;
    }

    while (parkxml)
    {
        if (strcmp(tagXMLEle(parkxml), "device"))
        {
            parkxml = nextXMLEle(ParkdataXmlRoot, 0);
            continue;
        }
        ap = findXMLAtt(parkxml, "name");
        if (ap && (!strcmp(valuXMLAtt(ap), ParkDeviceName)))
        {
            devicefound = true;
            break;
        }
        parkxml = nextXMLEle(ParkdataXmlRoot, 0);
    }

    if (!devicefound)
        return false;

    delXMLEle(parkxml);

    ParkstatusXml        = nullptr;
    ParkdeviceXml        = nullptr;
    ParkpositionXml      = nullptr;
    ParkpositionAxis1Xml = nullptr;
    ParkpositionAxis2Xml = nullptr;

    wordexp(ParkDataFileName.c_str(), &wexp, 0);
    if (!(fp = fopen(wexp.we_wordv[0], "w")))
    {
        wordfree(&wexp);
        LOGF_INFO("WriteParkData: can not write file %s: %s", ParkDataFileName.c_str(), strerror(errno));
        return false;
    }
    prXMLEle(fp, ParkdataXmlRoot, 0);
    fclose(fp);
    wordfree(&wexp);

    return true;
}

bool Telescope::WriteParkData()
{
    // We need to refresh parking data in case other devices parking states were updated since we
    // read the data the first time.
    if (LoadParkXML() != nullptr)
        LOG_DEBUG("Failed to refresh parking data.");

    wordexp_t wexp;
    FILE *fp;
    char pcdata[30];
    ParkDeviceName = getDeviceName();

    if (wordexp(ParkDataFileName.c_str(), &wexp, 0))
    {
        wordfree(&wexp);
        LOGF_INFO("WriteParkData: can not write file %s: Badly formed filename.",
                  ParkDataFileName.c_str());
        return false;
    }

    if (!(fp = fopen(wexp.we_wordv[0], "w")))
    {
        wordfree(&wexp);
        LOGF_INFO("WriteParkData: can not write file %s: %s", ParkDataFileName.c_str(),
                  strerror(errno));
        return false;
    }

    if (!ParkdataXmlRoot)
        ParkdataXmlRoot = addXMLEle(nullptr, "parkdata");

    if (!ParkdeviceXml)
    {
        ParkdeviceXml = addXMLEle(ParkdataXmlRoot, "device");
        addXMLAtt(ParkdeviceXml, "name", ParkDeviceName);
    }

    if (!ParkstatusXml)
        ParkstatusXml = addXMLEle(ParkdeviceXml, "parkstatus");
    editXMLEle(ParkstatusXml, (IsParked ? "true" : "false"));

    if (parkDataType != PARK_SIMPLE)
    {
        if (!ParkpositionXml)
            ParkpositionXml = addXMLEle(ParkdeviceXml, "parkposition");
        if (!ParkpositionAxis1Xml)
            ParkpositionAxis1Xml = addXMLEle(ParkpositionXml, "axis1position");
        if (!ParkpositionAxis2Xml)
            ParkpositionAxis2Xml = addXMLEle(ParkpositionXml, "axis2position");

        snprintf(pcdata, sizeof(pcdata), "%lf", Axis1ParkPosition);
        editXMLEle(ParkpositionAxis1Xml, pcdata);
        snprintf(pcdata, sizeof(pcdata), "%lf", Axis2ParkPosition);
        editXMLEle(ParkpositionAxis2Xml, pcdata);
    }

    prXMLEle(fp, ParkdataXmlRoot, 0);
    fclose(fp);
    wordfree(&wexp);

    return true;
}

double Telescope::GetAxis1Park() const
{
    return Axis1ParkPosition;
}
double Telescope::GetAxis1ParkDefault() const
{
    return Axis1DefaultParkPosition;
}
double Telescope::GetAxis2Park() const
{
    return Axis2ParkPosition;
}
double Telescope::GetAxis2ParkDefault() const
{
    return Axis2DefaultParkPosition;
}

void Telescope::SetAxis1Park(double value)
{
    LOGF_DEBUG("Setting Park Axis1 to %.2f", value);
    Axis1ParkPosition = value;
    ParkPositionNP[AXIS_RA].setValue(value);
    ParkPositionNP.apply();
}

void Telescope::SetAxis1ParkDefault(double value)
{
    LOGF_DEBUG("Setting Default Park Axis1 to %.2f", value);
    Axis1DefaultParkPosition = value;
}

void Telescope::SetAxis2Park(double value)
{
    LOGF_DEBUG("Setting Park Axis2 to %.2f", value);
    Axis2ParkPosition = value;
    ParkPositionNP[AXIS_DE].setValue(value);
    ParkPositionNP.apply();
}

void Telescope::SetAxis2ParkDefault(double value)
{
    LOGF_DEBUG("Setting Default Park Axis2 to %.2f", value);
    Axis2DefaultParkPosition = value;
}

bool Telescope::isLocked() const
{
    return DomePolicySP[DOME_LOCKS].getState() == ISS_ON && IsLocked;
}

bool Telescope::SetSlewRate(int index)
{
    INDI_UNUSED(index);
    return true;
}

void Telescope::processButton(const char *button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

    if (!strcmp(button_n, "ABORTBUTTON"))
    {
        auto trackSW = getSwitch("TELESCOPE_TRACK_MODE");
        // Only abort if we have some sort of motion going on
        if (ParkSP.getState() == IPS_BUSY || MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY
                || EqNP.getState() == IPS_BUSY ||
                (trackSW && trackSW.getState() == IPS_BUSY))
        {
            // Invoke parent processing so that Telescope takes care of abort cross-check
            ISState states[1] = { ISS_ON };
            const char *names[1]    = { AbortSP[0].getName() };
            ISNewSwitch(getDeviceName(), AbortSP.getName(), states, const_cast<char **>(names), 1);
        }
    }
    else if (!strcmp(button_n, "PARKBUTTON"))
    {
        ISState states[2] = { ISS_ON, ISS_OFF };
        const char *names[2]    = { ParkSP[PARK].getName(), ParkSP[UNPARK].getName() };
        ISNewSwitch(getDeviceName(), ParkSP.getName(), states, const_cast<char **>(names), 2);
    }
    else if (!strcmp(button_n, "UNPARKBUTTON"))
    {
        ISState states[2] = { ISS_OFF, ISS_ON };
        const char *names[2]    = { ParkSP[PARK].getName(), ParkSP[UNPARK].getName() };
        ISNewSwitch(getDeviceName(), ParkSP.getName(), states, const_cast<char **>(names), 2);
    }
    else if (!strcmp(button_n, "SLEWPRESETUP"))
    {
        processSlewPresets(1, 270);
    }
    else if (!strcmp(button_n, "SLEWPRESETDOWN"))
    {
        processSlewPresets(1, 90);
    }
}

void Telescope::processJoystick(const char *joystick_n, double mag, double angle)
{
    if (MotionControlModeTP[MOTION_CONTROL_MODE_JOYSTICK].getState() == ISS_ON && !strcmp(joystick_n, "MOTIONDIR"))
    {
        if ((TrackState == SCOPE_PARKING) || (TrackState == SCOPE_PARKED))
        {
            DEBUG(Logger::DBG_WARNING, "Can not slew while mount is parking/parked.");
            return;
        }

        processNSWE(mag, angle);
    }
    else if (!strcmp(joystick_n, "SLEWPRESET"))
        processSlewPresets(mag, angle);
}

void Telescope::processAxis(const char *axis_n, double value)
{
    if (MotionControlModeTP[MOTION_CONTROL_MODE_AXES].getState() == ISS_ON)
    {
        if (!strcmp(axis_n, "MOTIONDIRNS") || !strcmp(axis_n, "MOTIONDIRWE"))
        {
            if ((TrackState == SCOPE_PARKING) || (TrackState == SCOPE_PARKED))
            {
                LOG_WARN("Cannot slew while mount is parking/parked.");
                return;
            }

            if (!strcmp(axis_n, "MOTIONDIRNS"))
            {
                // South
                if (value > 0)
                {
                    motionDirNSValue = -1;
                }
                // North
                else if (value < 0)
                {
                    motionDirNSValue = 1;
                }
                else
                {
                    motionDirNSValue = 0;
                }
            }
            else if (!strcmp(axis_n, "MOTIONDIRWE"))
            {
                // East
                if (value > 0)
                {
                    motionDirWEValue = 1;
                }
                // West
                else if (value < 0)
                {
                    motionDirWEValue = -1;
                }
                else
                {
                    motionDirWEValue = 0;
                }
            }

            float x = motionDirWEValue * sqrt(1 - pow(motionDirNSValue, 2) / 2.0f);
            float y = motionDirNSValue * sqrt(1 - pow(motionDirWEValue, 2) / 2.0f);
            float angle = atan2(y, x) * (180.0 / 3.141592653589);
            float mag = sqrt(pow(y, 2) + pow(x, 2));
            while (angle < 0)
            {
                angle += 360;
            }
            if (mag == 0)
            {
                angle = 0;
            }

            processNSWE(mag, angle);
        }
    }
}

void Telescope::processNSWE(double mag, double angle)
{
    if (mag < 0.5)
    {
        // Moving in the same direction will make it stop
        if (MovementNSSP.getState() == IPS_BUSY)
        {
            if (MoveNS(MovementNSSP[DIRECTION_NORTH].getState() == ISS_ON ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP))
            {
                MovementNSSP.reset();
                MovementNSSP.setState(IPS_IDLE);
                MovementNSSP.apply();
            }
            else
            {
                MovementNSSP.setState(IPS_ALERT);
                MovementNSSP.apply();
            }
        }

        if (MovementWESP.getState() == IPS_BUSY)
        {
            if (MoveWE(MovementWESP[DIRECTION_WEST].getState() == ISS_ON ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP))
            {
                MovementWESP.reset();
                MovementWESP.setState(IPS_IDLE);
                MovementWESP.apply();
            }
            else
            {
                MovementWESP.setState(IPS_ALERT);
                MovementWESP.apply();
            }
        }
    }
    // Put high threshold
    else if (mag > 0.9)
    {
        // Only one axis can move at a time
        if (LockAxisSP[AXIS_RA].getState() == ISS_ON)
        {
            // West
            if (angle >= 90 && angle <= 270)
                angle = 180;
            // East
            else
                angle = 0;
        }
        else if (LockAxisSP[AXIS_DE].getState() == ISS_ON)
        {
            // North
            if (angle >= 0 && angle <= 180)
                angle = 90;
            // South
            else
                angle = 270;
        }

        // Snap angle to x or y direction if close to corresponding axis (i.e. deviation < 15°)
        if (angle > 75 && angle < 105)
        {
            angle = 90;
        }
        if (angle > 165 && angle < 195)
        {
            angle = 180;
        }
        if (angle > 255 && angle < 285)
        {
            angle = 270;
        }
        if (angle > 345 || angle < 15)
        {
            angle = 0;
        }

        // North
        if (angle > 0 && angle < 180)
        {
            // Don't try to move if you're busy and moving in the same direction
            if (MovementNSSP.getState() != IPS_BUSY || MovementNSSP[DIRECTION_NORTH].getState() != ISS_ON)
                MoveNS(DIRECTION_NORTH, MOTION_START);

            MovementNSSP.setState(IPS_BUSY);
            MovementNSSP[DIRECTION_NORTH].setState(ISS_ON);
            MovementNSSP[DIRECTION_SOUTH].setState(ISS_OFF);
            MovementNSSP.apply();
        }
        // South
        if (angle > 180 && angle < 360)
        {
            // Don't try to move if you're busy and moving in the same direction
            if (MovementNSSP.getState() != IPS_BUSY || MovementNSSP[DIRECTION_SOUTH].getState() != ISS_ON)
                MoveNS(DIRECTION_SOUTH, MOTION_START);

            MovementNSSP.setState(IPS_BUSY);
            MovementNSSP[DIRECTION_NORTH].setState(ISS_OFF);
            MovementNSSP[DIRECTION_SOUTH].setState(ISS_ON);
            MovementNSSP.apply();
        }
        // East
        if (angle < 90 || angle > 270)
        {
            // Don't try to move if you're busy and moving in the same direction
            if (MovementWESP.getState() != IPS_BUSY || MovementWESP[DIRECTION_EAST].getState() != ISS_ON)
                MoveWE(DIRECTION_EAST, MOTION_START);

            MovementWESP.setState(IPS_BUSY);
            MovementWESP[DIRECTION_WEST].setState(ISS_OFF);
            MovementWESP[DIRECTION_EAST].setState(ISS_ON);
            MovementWESP.apply();
        }

        // West
        if (angle > 90 && angle < 270)
        {
            // Don't try to move if you're busy and moving in the same direction
            if (MovementWESP.getState() != IPS_BUSY || MovementWESP[DIRECTION_WEST].getState() != ISS_ON)
                MoveWE(DIRECTION_WEST, MOTION_START);

            MovementWESP.setState(IPS_BUSY);
            MovementWESP[DIRECTION_WEST].setState(ISS_ON);
            MovementWESP[DIRECTION_EAST].setState(ISS_OFF);
            MovementWESP.apply();
        }
    }
}

void Telescope::processSlewPresets(double mag, double angle)
{
    // high threshold, only 1 is accepted
    if (mag != 1)
        return;

    size_t currentIndex = SlewRateSP.findOnSwitchIndex();

    // Up
    if (angle > 0 && angle < 180)
    {
        if (currentIndex <= 0)
            return;

        SlewRateSP.reset();
        SlewRateSP[currentIndex - 1].setState(ISS_ON);
        SetSlewRate(currentIndex - 1);
    }
    // Down
    else
    {
        if (currentIndex >= SlewRateSP.count() - 1)
            return;

        SlewRateSP.reset();
        SlewRateSP[currentIndex + 1].setState(ISS_ON);
        SetSlewRate(currentIndex - 1);
    }

    SlewRateSP.apply();
}

void Telescope::joystickHelper(const char *joystick_n, double mag, double angle, void *context)
{
    static_cast<Telescope *>(context)->processJoystick(joystick_n, mag, angle);
}

void Telescope::axisHelper(const char *axis_n, double value, void *context)
{
    static_cast<Telescope *>(context)->processAxis(axis_n, value);
}

void Telescope::buttonHelper(const char *button_n, ISState state, void *context)
{
    static_cast<Telescope *>(context)->processButton(button_n, state);
}

void Telescope::setPierSide(TelescopePierSide side)
{
    // ensure that the scope knows it's pier side or the pier side is simulated
    if (HasPierSide() == false && getSimulatePierSide() == false)
        return;

    currentPierSide = side;

    if (currentPierSide != lastPierSide)
    {
        PierSideSP[PIER_WEST].setState((side == PIER_WEST) ? ISS_ON : ISS_OFF);
        PierSideSP[PIER_EAST].setState((side == PIER_EAST) ? ISS_ON : ISS_OFF);
        PierSideSP.setState(IPS_OK);
        PierSideSP.apply();

        lastPierSide = currentPierSide;
    }
}

/// Simulates pier side using the hour angle.
/// A correct implementation uses the declination axis value, this is only for where this isn't available,
/// such as in the telescope simulator or a GEM which does not provide any pier side or axis information.
/// This last is deeply unsatisfactory, it will not be able to reflect the true pointing state
/// reliably for positions close to the meridian.
Telescope::TelescopePierSide Telescope::expectedPierSide(double ra)
{
    // return unknown if the mount does not have pier side, this will be the case for a fork mount
    // where a pier flip is not required.
    if (!HasPierSide() && !HasPierSideSimulation())
        return INDI::Telescope::PIER_UNKNOWN;

    // calculate the hour angle and derive the pier side
    double lst = get_local_sidereal_time(m_Location.longitude);
    double hourAngle = get_local_hour_angle(lst, ra);

    return hourAngle <= 0 ? INDI::Telescope::PIER_WEST : INDI::Telescope::PIER_EAST;
}

void Telescope::setPECState(TelescopePECState state)
{
    currentPECState = state;

    if (currentPECState != lastPECState)
    {

        PECStateSP[PEC_OFF].setState((state == PEC_ON) ? ISS_OFF : ISS_ON);
        PECStateSP[PEC_ON].setState((state == PEC_ON) ? ISS_ON  : ISS_OFF);
        PECStateSP.setState(IPS_OK);
        PECStateSP.apply();

        lastPECState = currentPECState;
    }
}

std::string Telescope::GetHomeDirectory() const
{
    // Check first the HOME environmental variable
    const char *HomeDir = getenv("HOME");

    // ...otherwise get the home directory of the current user.
    if (!HomeDir)
    {
        HomeDir = getpwuid(getuid())->pw_dir;
    }
    return (HomeDir ? std::string(HomeDir) : "");
}

bool Telescope::CheckFile(const std::string &file_name, bool writable) const
{
    FILE *FilePtr = fopen(file_name.c_str(), (writable ? "a" : "r"));

    if (FilePtr)
    {
        fclose(FilePtr);
        return true;
    }
    return false;
}

void Telescope::sendTimeFromSystem()
{
    char ts[32] = {0};

    std::time_t t = std::time(nullptr);
    struct std::tm *utctimeinfo = std::gmtime(&t);

    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", utctimeinfo);
    TimeTP[UTC].setText(ts);

    struct std::tm *localtimeinfo = std::localtime(&t);
    snprintf(ts, sizeof(ts), "%4.2f", (localtimeinfo->tm_gmtoff / 3600.0));
    TimeTP[OFFSET].setText(ts);

    TimeTP.setState(IPS_OK);

    TimeTP.apply();
}

bool Telescope::getSimulatePierSide() const
{
    return m_simulatePierSide;
}

const char * Telescope::getPierSideStr(TelescopePierSide ps)
{
    switch (ps)
    {
        case PIER_WEST:
            return "PIER_WEST";
        case PIER_EAST:
            return "PIER_EAST";
        default:
            return "PIER_UNKNOWN";
    }
}

void Telescope::setSimulatePierSide(bool simulate)
{
    SimulatePierSideSP.reset();
    SimulatePierSideSP[SIMULATE_YES].setState(simulate ? ISS_ON : ISS_OFF);
    SimulatePierSideSP[SIMULATE_NO].setState(simulate ? ISS_OFF : ISS_ON);
    SimulatePierSideSP.setState(IPS_OK);
    SimulatePierSideSP.apply();

    if (simulate)
    {
        capability |= TELESCOPE_HAS_PIER_SIDE;
        defineProperty(PierSideSP);
    }
    else
    {
        capability &= static_cast<uint32_t>(~TELESCOPE_HAS_PIER_SIDE);
        deleteProperty(PierSideSP);
    }

    m_simulatePierSide = simulate;
}

}
