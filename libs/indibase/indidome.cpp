/*******************************************************************************
 INDI Dome Base Class
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 The code used calculate dome target AZ and ZD is written by Ferran Casarramona, and adapted from code from Markus Wildi.
 The transformations are based on the paper Matrix Method for Coodinates Transformation written by Toshimi Taki (http://www.asahi-net.or.jp/~zs3t-tk).

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

#include "indidome.h"

#include "indicom.h"
#include "indicontroller.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include <libnova/julian_day.h>
#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

#include <cerrno>
#include <cmath>
#include <cstring>
#include <wordexp.h>
#include <pwd.h>
#include <unistd.h>
#include <limits>

#define DOME_SLAVING_TAB "Slaving"
#define DOME_COORD_THRESHOLD \
    0.1 /* Only send debug messages if the differences between old and new values of Az/Alt excceds this value */

namespace INDI
{

Dome::Dome() : ParkDataFileName(GetHomeDirectory() + "/.indi/ParkData.xml")
{
    controller = new Controller(this);

    controller->setButtonCallback(buttonHelper);

    prev_az = prev_alt = prev_ra = prev_dec = 0;
    mountEquatorialCoords.dec = mountEquatorialCoords.ra = -1;
    m_MountState = IPS_ALERT;

    capability = 0;

    m_ShutterState = SHUTTER_UNKNOWN;
    m_DomeState    = DOME_IDLE;

    parkDataType = PARK_NONE;
    ParkdataXmlRoot = nullptr;
}

Dome::~Dome()
{
    delXMLEle(ParkdataXmlRoot);

    delete controller;
    delete serialConnection;
    delete tcpConnection;
}

std::string Dome::GetHomeDirectory() const
{
    // Check first the HOME environmental variable
    const char * HomeDir = getenv("HOME");

    // ...otherwise get the home directory of the current user.
    if (!HomeDir)
    {
        HomeDir = getpwuid(getuid())->pw_dir;
    }
    return (HomeDir ? std::string(HomeDir) : "");
}

bool Dome::initProperties()
{
    DefaultDevice::initProperties(); //  let the base class flesh in what it wants

    // Presets
    IUFillNumber(&PresetN[0], "Preset 1", "", "%6.2f", 0, 360.0, 1.0, 0);
    IUFillNumber(&PresetN[1], "Preset 2", "", "%6.2f", 0, 360.0, 1.0, 0);
    IUFillNumber(&PresetN[2], "Preset 3", "", "%6.2f", 0, 360.0, 1.0, 0);
    IUFillNumberVector(&PresetNP, PresetN, 3, getDeviceName(), "Presets", "", "Presets", IP_RW, 0, IPS_IDLE);

    //Preset GOTO
    IUFillSwitch(&PresetGotoS[0], "Preset 1", "", ISS_OFF);
    IUFillSwitch(&PresetGotoS[1], "Preset 2", "", ISS_OFF);
    IUFillSwitch(&PresetGotoS[2], "Preset 3", "", ISS_OFF);
    IUFillSwitchVector(&PresetGotoSP, PresetGotoS, 3, getDeviceName(), "Goto", "", "Presets", IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    //    IUFillSwitch(&AutoParkS[0], "INDI_ENABLED", "Enable", ISS_OFF);
    //    IUFillSwitch(&AutoParkS[1], "INDI_DISABLED", "Disable", ISS_ON);
    //    IUFillSwitchVector(&AutoParkSP, AutoParkS, 2, getDeviceName(), "DOME_AUTOPARK", "Auto Park", OPTIONS_TAB, IP_RW,
    //                       ISR_1OFMANY, 0, IPS_IDLE);

    // Active Devices
    IUFillText(&ActiveDeviceT[0], "ACTIVE_TELESCOPE", "Telescope", "Telescope Simulator");
    //IUFillText(&ActiveDeviceT[1], "ACTIVE_WEATHER", "Weather", "WunderGround");
    IUFillTextVector(&ActiveDeviceTP, ActiveDeviceT, 1, getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB,
                     IP_RW, 60, IPS_IDLE);

    // Use locking if telescope is unparked
    IUFillSwitch(&MountPolicyS[MOUNT_IGNORED], "MOUNT_IGNORED", "Mount ignored", ISS_ON);
    IUFillSwitch(&MountPolicyS[MOUNT_LOCKS], "MOUNT_LOCKS", "Mount locks", ISS_OFF);
    IUFillSwitchVector(&MountPolicySP, MountPolicyS, 2, getDeviceName(), "MOUNT_POLICY", "Mount Policy", OPTIONS_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    // Shutter Policy
    IUFillSwitch(&ShutterParkPolicyS[SHUTTER_CLOSE_ON_PARK], "SHUTTER_CLOSE_ON_PARK", "Close On Park", ISS_OFF);
    IUFillSwitch(&ShutterParkPolicyS[SHUTTER_OPEN_ON_UNPARK], "SHUTTER_OPEN_ON_PARK", "Open On UnPark", ISS_OFF);
    IUFillSwitchVector(&ShutterParkPolicySP, ShutterParkPolicyS, 2, getDeviceName(), "DOME_SHUTTER_PARK_POLICY", "Shutter",
                       OPTIONS_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // Measurements
    IUFillNumber(&DomeMeasurementsN[DM_DOME_RADIUS], "DM_DOME_RADIUS", "Radius (m)", "%6.2f", 0.0, 50.0, 1.0, 0.0);
    IUFillNumber(&DomeMeasurementsN[DM_SHUTTER_WIDTH], "DM_SHUTTER_WIDTH", "Shutter width (m)", "%6.2f", 0.0, 10.0, 1.0,
                 0.0);
    IUFillNumber(&DomeMeasurementsN[DM_NORTH_DISPLACEMENT], "DM_NORTH_DISPLACEMENT", "N displacement (m)", "%6.2f",
                 -10.0, 10.0, 1.0, 0.0);
    IUFillNumber(&DomeMeasurementsN[DM_EAST_DISPLACEMENT], "DM_EAST_DISPLACEMENT", "E displacement (m)", "%6.2f", -10.0,
                 10.0, 1.0, 0.0);
    IUFillNumber(&DomeMeasurementsN[DM_UP_DISPLACEMENT], "DM_UP_DISPLACEMENT", "Up displacement (m)", "%6.2f", -10,
                 10.0, 1.0, 0.0);
    IUFillNumber(&DomeMeasurementsN[DM_OTA_OFFSET], "DM_OTA_OFFSET", "OTA offset (m)", "%6.2f", -10.0, 10.0, 1.0, 0.0);
    IUFillNumberVector(&DomeMeasurementsNP, DomeMeasurementsN, 6, getDeviceName(), "DOME_MEASUREMENTS", "Measurements",
                       DOME_SLAVING_TAB, IP_RW, 60, IPS_OK);

    IUFillSwitch(&OTASideS[0], "DM_OTA_SIDE_EAST", "East", ISS_OFF);
    IUFillSwitch(&OTASideS[1], "DM_OTA_SIDE_WEST", "West", ISS_OFF);
    IUFillSwitchVector(&OTASideSP, OTASideS, 2, getDeviceName(), "DM_OTA_SIDE", "Meridian side", DOME_SLAVING_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_OK);

    IUFillSwitch(&DomeAutoSyncS[0], "DOME_AUTOSYNC_ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&DomeAutoSyncS[1], "DOME_AUTOSYNC_DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&DomeAutoSyncSP, DomeAutoSyncS, 2, getDeviceName(), "DOME_AUTOSYNC", "Slaving", DOME_SLAVING_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_OK);

    IUFillNumber(&DomeSpeedN[0], "DOME_SPEED_VALUE", "RPM", "%6.2f", 0.0, 10, 0.1, 1.0);
    IUFillNumberVector(&DomeSpeedNP, DomeSpeedN, 1, getDeviceName(), "DOME_SPEED", "Speed", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_OK);

    IUFillNumber(&DomeSyncN[0], "DOME_SYNC_VALUE", "Az", "%.2f", 0.0, 360, 10, 0.0);
    IUFillNumberVector(&DomeSyncNP, DomeSyncN, 1, getDeviceName(), "DOME_SYNC", "Sync", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_OK);

    IUFillSwitch(&DomeMotionS[0], "DOME_CW", "Dome CW", ISS_OFF);
    IUFillSwitch(&DomeMotionS[1], "DOME_CCW", "Dome CCW", ISS_OFF);
    IUFillSwitchVector(&DomeMotionSP, DomeMotionS, 2, getDeviceName(), "DOME_MOTION", "Motion", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 60, IPS_OK);

    // Driver can define those to clients if there is support
    IUFillNumber(&DomeAbsPosN[0], "DOME_ABSOLUTE_POSITION", "Degrees", "%6.2f", 0.0, 360.0, 1.0, 0.0);
    IUFillNumberVector(&DomeAbsPosNP, DomeAbsPosN, 1, getDeviceName(), "ABS_DOME_POSITION", "Absolute Position",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_OK);

    IUFillNumber(&DomeRelPosN[0], "DOME_RELATIVE_POSITION", "Degrees", "%6.2f", -180, 180.0, 10.0, 0.0);
    IUFillNumberVector(&DomeRelPosNP, DomeRelPosN, 1, getDeviceName(), "REL_DOME_POSITION", "Relative Position",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_OK);

    IUFillSwitch(&AbortS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&AbortSP, AbortS, 1, getDeviceName(), "DOME_ABORT_MOTION", "Abort Motion", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillNumber(&DomeParamN[0], "AUTOSYNC_THRESHOLD", "Autosync threshold (deg)", "%6.2f", 0.0, 360.0, 1.0, 0.5);
    IUFillNumberVector(&DomeParamNP, DomeParamN, 1, getDeviceName(), "DOME_PARAMS", "Params", DOME_SLAVING_TAB, IP_RW,
                       60, IPS_OK);

    IUFillSwitch(&ParkS[0], "PARK", "Park(ed)", ISS_OFF);
    IUFillSwitch(&ParkS[1], "UNPARK", "UnPark(ed)", ISS_OFF);
    IUFillSwitchVector(&ParkSP, ParkS, 2, getDeviceName(), "DOME_PARK", "Parking", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY,
                       60, IPS_OK);

    // Backlash Compensation
    IUFillSwitch(&DomeBacklashS[INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&DomeBacklashS[INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&DomeBacklashSP, DomeBacklashS, 2, getDeviceName(), "DOME_BACKLASH_TOGGLE", "Backlash", OPTIONS_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);


    // Backlash Compensation Value
    IUFillNumber(&DomeBacklashN[0], "DOME_BACKLASH_VALUE", "Steps", "%.f", 0, 1e6, 100, 0);
    IUFillNumberVector(&DomeBacklashNP, DomeBacklashN, 1, getDeviceName(), "DOME_BACKLASH_STEPS", "Backlash",
                       OPTIONS_TAB, IP_RW, 60, IPS_OK);


    IUFillSwitch(&DomeShutterS[0], "SHUTTER_OPEN", "Open", ISS_OFF);
    IUFillSwitch(&DomeShutterS[1], "SHUTTER_CLOSE", "Close", ISS_ON);
    IUFillSwitchVector(&DomeShutterSP, DomeShutterS, 2, getDeviceName(), "DOME_SHUTTER", "Shutter", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_OK);

    IUFillSwitch(&ParkOptionS[0], "PARK_CURRENT", "Current", ISS_OFF);
    IUFillSwitch(&ParkOptionS[1], "PARK_DEFAULT", "Default", ISS_OFF);
    IUFillSwitch(&ParkOptionS[2], "PARK_WRITE_DATA", "Write Data", ISS_OFF);
    IUFillSwitchVector(&ParkOptionSP, ParkOptionS, 3, getDeviceName(), "DOME_PARK_OPTION", "Park Options", SITE_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    addDebugControl();

    controller->mapController("Dome CW", "CW/Open", Controller::CONTROLLER_BUTTON, "BUTTON_1");
    controller->mapController("Dome CCW", "CCW/Close", Controller::CONTROLLER_BUTTON, "BUTTON_2");

    controller->initProperties();

    IDSnoopDevice(ActiveDeviceT[0].text, "EQUATORIAL_EOD_COORD");
    IDSnoopDevice(ActiveDeviceT[0].text, "GEOGRAPHIC_COORD");
    IDSnoopDevice(ActiveDeviceT[0].text, "TELESCOPE_PARK");
    if (CanAbsMove())
        IDSnoopDevice(ActiveDeviceT[0].text, "TELESCOPE_PIER_SIDE");

    //IDSnoopDevice(ActiveDeviceT[1].text, "WEATHER_STATUS");

    setDriverInterface(DOME_INTERFACE);

    if (domeConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(serialConnection);
    }

    if (domeConnection & CONNECTION_TCP)
    {
        tcpConnection = new Connection::TCP(this);
        tcpConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(tcpConnection);
    }

    return true;
}

void Dome::ISGetProperties(const char * dev)
{
    //  First we let our parent populate
    DefaultDevice::ISGetProperties(dev);

    defineText(&ActiveDeviceTP);
    loadConfig(true, "ACTIVE_DEVICES");

    ISState isMountIgnored = ISS_OFF;
    if (IUGetConfigSwitch(getDeviceName(), MountPolicySP.name, MountPolicyS[MOUNT_IGNORED].name, &isMountIgnored) == 0)
    {
        MountPolicyS[MOUNT_IGNORED].s = isMountIgnored;
        MountPolicyS[MOUNT_LOCKS].s = (isMountIgnored == ISS_ON) ? ISS_OFF : ISS_ON;
    }
    defineSwitch(&MountPolicySP);

    controller->ISGetProperties(dev);
    return;
}

bool Dome::updateProperties()
{
    if (isConnected())
    {
        if (HasShutter())
        {
            defineSwitch(&DomeShutterSP);
            defineSwitch(&ShutterParkPolicySP);
        }

        //  Now we add our Dome specific stuff
        defineSwitch(&DomeMotionSP);

        if (HasVariableSpeed())
        {
            defineNumber(&DomeSpeedNP);
            //defineNumber(&DomeTimerNP);
        }
        if (CanRelMove())
            defineNumber(&DomeRelPosNP);
        if (CanAbsMove())
            defineNumber(&DomeAbsPosNP);
        if (CanAbort())
            defineSwitch(&AbortSP);
        if (CanAbsMove())
        {
            defineNumber(&PresetNP);
            defineSwitch(&PresetGotoSP);

            defineSwitch(&DomeAutoSyncSP);
            defineSwitch(&OTASideSP);
            defineNumber(&DomeParamNP);
            defineNumber(&DomeMeasurementsNP);
        }
        if (CanSync())
            defineNumber(&DomeSyncNP);

        if (CanPark())
        {
            defineSwitch(&ParkSP);
            if (parkDataType != PARK_NONE)
            {
                defineNumber(&ParkPositionNP);
                defineSwitch(&ParkOptionSP);
            }
        }

        if (HasBacklash())
        {
            defineSwitch(&DomeBacklashSP);
            defineNumber(&DomeBacklashNP);
        }

        //defineSwitch(&AutoParkSP);
    }
    else
    {
        if (HasShutter())
        {
            deleteProperty(DomeShutterSP.name);
            deleteProperty(ShutterParkPolicySP.name);
        }

        deleteProperty(DomeMotionSP.name);

        if (HasVariableSpeed())
        {
            deleteProperty(DomeSpeedNP.name);
            //deleteProperty(DomeTimerNP.name);
        }
        if (CanRelMove())
            deleteProperty(DomeRelPosNP.name);
        if (CanAbsMove())
            deleteProperty(DomeAbsPosNP.name);
        if (CanAbort())
            deleteProperty(AbortSP.name);
        if (CanAbsMove())
        {
            deleteProperty(PresetNP.name);
            deleteProperty(PresetGotoSP.name);

            deleteProperty(DomeAutoSyncSP.name);
            deleteProperty(OTASideSP.name);
            deleteProperty(DomeParamNP.name);
            deleteProperty(DomeMeasurementsNP.name);
        }

        if (CanSync())
            deleteProperty(DomeSyncNP.name);

        if (CanPark())
        {
            deleteProperty(ParkSP.name);
            if (parkDataType != PARK_NONE)
            {
                deleteProperty(ParkPositionNP.name);
                deleteProperty(ParkOptionSP.name);
            }
        }

        if (HasBacklash())
        {
            deleteProperty(DomeBacklashSP.name);
            deleteProperty(DomeBacklashNP.name);
        }

        //deleteProperty(AutoParkSP.name);
    }

    controller->updateProperties();
    return true;
}

bool Dome::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, PresetNP.name))
        {
            IUUpdateNumber(&PresetNP, values, names, n);
            PresetNP.s = IPS_OK;
            IDSetNumber(&PresetNP, nullptr);

            return true;
        }
        // Dome Sync
        else if (!strcmp(name, DomeSyncNP.name))
        {
            if (Sync(values[0]))
            {
                IUUpdateNumber(&DomeSyncNP, values, names, n);
                DomeSyncNP.s = IPS_OK;
                DomeAbsPosN[0].value = values[0];
                IDSetNumber(&DomeAbsPosNP, nullptr);
            }
            else
            {
                DomeSyncNP.s = IPS_ALERT;
            }

            IDSetNumber(&DomeSyncNP, nullptr);
            return true;
        }
        else if (!strcmp(name, DomeParamNP.name))
        {
            IUUpdateNumber(&DomeParamNP, values, names, n);
            DomeParamNP.s = IPS_OK;
            IDSetNumber(&DomeParamNP, nullptr);
            return true;
        }
        else if (!strcmp(name, DomeSpeedNP.name))
        {
            double newSpeed = values[0];
            Dome::SetSpeed(newSpeed);
            return true;
        }
        else if (!strcmp(name, DomeAbsPosNP.name))
        {
            double newPos = values[0];
            Dome::MoveAbs(newPos);
            return true;
        }
        else if (!strcmp(name, DomeRelPosNP.name))
        {
            double newPos = values[0];
            Dome::MoveRel(newPos);
            return true;
        }
        else if (!strcmp(name, DomeMeasurementsNP.name))
        {
            IUUpdateNumber(&DomeMeasurementsNP, values, names, n);
            DomeMeasurementsNP.s = IPS_OK;
            IDSetNumber(&DomeMeasurementsNP, nullptr);

            return true;
        }
        else if (strcmp(name, ParkPositionNP.name) == 0)
        {
            IUUpdateNumber(&ParkPositionNP, values, names, n);
            ParkPositionNP.s = IPS_OK;

            Axis1ParkPosition = ParkPositionN[AXIS_RA].value;
            IDSetNumber(&ParkPositionNP, nullptr);
            return true;
        }
        ////////////////////////////////////////////
        // Backlash value
        ////////////////////////////////////////////
        else if (!strcmp(name, DomeBacklashNP.name))
        {
            if (DomeBacklashS[INDI_ENABLED].s != ISS_ON)
            {
                DomeBacklashNP.s = IPS_IDLE;
                DEBUGDEVICE(dev, Logger::DBG_WARNING, "Dome backlash must be enabled first.");
            }
            else
            {
                uint32_t steps = static_cast<uint32_t>(values[0]);
                if (SetBacklash(steps))
                {
                    DomeBacklashN[0].value = values[0];
                    DomeBacklashNP.s = IPS_OK;
                }
                else
                    DomeBacklashNP.s = IPS_ALERT;
            }
            IDSetNumber(&DomeBacklashNP, nullptr);
            return true;
        }
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool Dome::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        ////////////////////////////////////////////
        // GOTO Presets
        ////////////////////////////////////////////
        if (!strcmp(PresetGotoSP.name, name))
        {
            if (m_DomeState == DOME_PARKED)
            {
                DEBUGDEVICE(getDeviceName(), Logger::DBG_ERROR,
                            "Please unpark before issuing any motion commands.");
                PresetGotoSP.s = IPS_ALERT;
                IDSetSwitch(&PresetGotoSP, nullptr);
                return false;
            }

            IUUpdateSwitch(&PresetGotoSP, states, names, n);
            int index  = IUFindOnSwitchIndex(&PresetGotoSP);
            IPState rc = Dome::MoveAbs(PresetN[index].value);
            if (rc == IPS_OK || rc == IPS_BUSY)
            {
                PresetGotoSP.s = IPS_OK;
                LOGF_INFO("Moving to Preset %d (%.2f degrees).", index + 1, PresetN[index].value);
                IDSetSwitch(&PresetGotoSP, nullptr);
                return true;
            }

            PresetGotoSP.s = IPS_ALERT;
            IDSetSwitch(&PresetGotoSP, nullptr);
            return false;
        }
        ////////////////////////////////////////////
        // Dome Auto Sync
        ////////////////////////////////////////////
        else if (!strcmp(name, DomeAutoSyncSP.name))
        {
            IUUpdateSwitch(&DomeAutoSyncSP, states, names, n);
            DomeAutoSyncSP.s = IPS_OK;

            if (DomeAutoSyncS[0].s == ISS_ON)
            {
                IDSetSwitch(&DomeAutoSyncSP, "Dome will now be synced to mount azimuth position.");
                //UpdateAutoSync();
                m_HorizontalUpdateTimerID = IEAddTimer(10, &Dome::updateMountCoordsHelper, this);
            }
            else
            {
                IDSetSwitch(&DomeAutoSyncSP, "Dome is no longer synced to mount azimuth position.");
                if (m_HorizontalUpdateTimerID > 0)
                {
                    IERmTimer(m_HorizontalUpdateTimerID);
                    m_HorizontalUpdateTimerID = -1;
                }

                if (DomeAbsPosNP.s == IPS_BUSY || DomeRelPosNP.s == IPS_BUSY /* || DomeTimerNP.s == IPS_BUSY*/)
                    Dome::Abort();
            }

            return true;
        }
        ////////////////////////////////////////////
        // OTA Side
        ////////////////////////////////////////////
        else if (!strcmp(name, OTASideSP.name))
        {
            IUUpdateSwitch(&OTASideSP, states, names, n);
            OTASideSP.s = IPS_OK;

            if (OTASideS[0].s == ISS_ON)
            {
                IDSetSwitch(&OTASideSP, "Dome will be synced for telescope been at east of meridian");
                UpdateAutoSync();
            }
            else
            {
                IDSetSwitch(&OTASideSP, "Dome will be synced for telescope been at west of meridian");
                UpdateAutoSync();
            }

            return true;
        }
        ////////////////////////////////////////////
        // Dome Motion
        ////////////////////////////////////////////
        else if (!strcmp(name, DomeMotionSP.name))
        {
            // Check if any switch is ON
            for (int i = 0; i < n; i++)
            {
                if (states[i] == ISS_ON)
                {
                    if (!strcmp(DomeMotionS[DOME_CW].name, names[i]))
                        Dome::Move(DOME_CW, MOTION_START);
                    else
                        Dome::Move(DOME_CCW, MOTION_START);

                    return true;
                }
            }

            // All switches are off, so let's turn off last motion
            int current_direction = IUFindOnSwitchIndex(&DomeMotionSP);
            // Shouldn't happen
            if (current_direction < 0)
            {
                DomeMotionSP.s = IPS_IDLE;
                IDSetSwitch(&DomeMotionSP, nullptr);
                return false;
            }

            Dome::Move(static_cast<DomeDirection>(current_direction), MOTION_STOP);

            return true;
        }
        ////////////////////////////////////////////
        // Abort Motion
        ////////////////////////////////////////////
        else if (!strcmp(name, AbortSP.name))
        {
            Dome::Abort();
            return true;
        }
        ////////////////////////////////////////////
        // Shutter
        ////////////////////////////////////////////
        else if (!strcmp(name, DomeShutterSP.name))
        {
            // Check if any switch is ON
            for (int i = 0; i < n; i++)
            {
                if (states[i] == ISS_ON)
                {
                    // Open
                    if (!strcmp(DomeShutterS[0].name, names[i]))
                    {
                        return (Dome::ControlShutter(SHUTTER_OPEN) != IPS_ALERT);
                    }
                    else
                    {
                        return (Dome::ControlShutter(SHUTTER_CLOSE) != IPS_ALERT);
                    }
                }
            }
        }
        ////////////////////////////////////////////
        // Parking Switch
        ////////////////////////////////////////////
        else if (!strcmp(name, ParkSP.name))
        {
            // Check if any switch is ON
            for (int i = 0; i < n; i++)
            {
                if (states[i] == ISS_ON)
                {
                    if (!strcmp(ParkS[0].name, names[i]))
                    {
                        if (m_DomeState == DOME_PARKING)
                            return false;

                        return (Dome::Park() != IPS_ALERT);
                    }
                    else
                    {
                        if (m_DomeState == DOME_UNPARKING)
                            return false;

                        return (Dome::UnPark() != IPS_ALERT);
                    }
                }
            }
        }
        ////////////////////////////////////////////
        // Parking Option
        ////////////////////////////////////////////
        else if (!strcmp(name, ParkOptionSP.name))
        {
            IUUpdateSwitch(&ParkOptionSP, states, names, n);
            ISwitch * sp = IUFindOnSwitch(&ParkOptionSP);
            if (!sp)
                return false;

            bool rc = false;

            IUResetSwitch(&ParkOptionSP);

            if (!strcmp(sp->name, "PARK_CURRENT"))
            {
                rc = SetCurrentPark();
            }
            else if (!strcmp(sp->name, "PARK_DEFAULT"))
            {
                rc = SetDefaultPark();
            }
            else if (!strcmp(sp->name, "PARK_WRITE_DATA"))
            {
                rc = WriteParkData();
                if (rc)
                    DEBUG(Logger::DBG_SESSION, "Saved Park Status/Position.");
                else
                    LOG_WARN( "Can not save Park Status/Position.");
            }

            ParkOptionSP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&ParkOptionSP, nullptr);

            return true;
        }
        ////////////////////////////////////////////
        // Auto Park
        ////////////////////////////////////////////
#if 0
        else if (!strcmp(name, AutoParkSP.name))
        {
            IUUpdateSwitch(&AutoParkSP, states, names, n);

            AutoParkSP.s = IPS_OK;

            if (AutoParkS[0].s == ISS_ON)
                LOG_WARN( "Warning: Auto park is enabled. If weather conditions are in the "
                          "danger zone, the dome will be automatically parked. Only enable this "
                          "option is parking the dome at any time will not cause damage to any "
                          "equipment.");
            else
                DEBUG(Logger::DBG_SESSION, "Auto park is disabled.");

            IDSetSwitch(&AutoParkSP, nullptr);

            return true;
        }
#endif
        ////////////////////////////////////////////
        // Telescope Parking Policy
        ////////////////////////////////////////////
        else if (!strcmp(name, MountPolicySP.name))
        {
            IUUpdateSwitch(&MountPolicySP, states, names, n);
            MountPolicySP.s = IPS_OK;
            if (MountPolicyS[MOUNT_IGNORED].s == ISS_ON)
                LOG_INFO("Mount Policy set to: Mount ignored. Dome can park regardless of mount parking state.");
            else
                LOG_WARN("Mount Policy set to: Mount locks. This prevents the dome from parking when mount is unparked.");
            IDSetSwitch(&MountPolicySP, nullptr);
            triggerSnoop(ActiveDeviceT[0].text, "TELESCOPE_PARK");
            return true;
        }
        ////////////////////////////////////////////
        // Shutter Parking Policy
        ////////////////////////////////////////////
        else if (!strcmp(name, ShutterParkPolicySP.name))
        {
            IUUpdateSwitch(&ShutterParkPolicySP, states, names, n);
            ShutterParkPolicySP.s = IPS_OK;
            IDSetSwitch(&ShutterParkPolicySP, nullptr);
            return true;
        }
        ////////////////////////////////////////////
        // Backlash enable/disable
        ////////////////////////////////////////////
        else if (strcmp(name, DomeBacklashSP.name) == 0)
        {
            int prevIndex = IUFindOnSwitchIndex(&DomeBacklashSP);
            IUUpdateSwitch(&DomeBacklashSP, states, names, n);
            const bool enabled = IUFindOnSwitchIndex(&DomeBacklashSP) == INDI_ENABLED;

            if (SetBacklashEnabled(enabled))
            {
                IUUpdateSwitch(&DomeBacklashSP, states, names, n);
                DomeBacklashSP.s = IPS_OK;
                LOGF_INFO("Dome backlash is %s.", (enabled ? "enabled" : "disabled"));
            }
            else
            {
                IUResetSwitch(&DomeBacklashSP);
                DomeBacklashS[prevIndex].s = ISS_ON;
                DomeBacklashSP.s = IPS_ALERT;
                LOG_ERROR("Failed to set trigger Dome backlash.");
            }

            IDSetSwitch(&DomeBacklashSP, nullptr);
            return true;
        }
    }

    controller->ISNewSwitch(dev, name, states, names, n);

    //  Nobody has claimed this, so, ignore it
    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool Dome::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, ActiveDeviceTP.name) == 0)
        {
            ActiveDeviceTP.s = IPS_OK;
            IUUpdateText(&ActiveDeviceTP, texts, names, n);
            IDSetText(&ActiveDeviceTP, nullptr);

            IDSnoopDevice(ActiveDeviceT[0].text, "EQUATORIAL_EOD_COORD");
            IDSnoopDevice(ActiveDeviceT[0].text, "TARGET_EOD_COORD");
            IDSnoopDevice(ActiveDeviceT[0].text, "GEOGRAPHIC_COORD");
            IDSnoopDevice(ActiveDeviceT[0].text, "TELESCOPE_PARK");
            if (CanAbsMove())
                IDSnoopDevice(ActiveDeviceT[0].text, "TELESCOPE_PIER_SIDE");
            //IDSnoopDevice(ActiveDeviceT[1].text, "WEATHER_STATUS");

            return true;
        }
    }

    controller->ISNewText(dev, name, texts, names, n);

    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool Dome::ISSnoopDevice(XMLEle * root)
{
    XMLEle * ep           = nullptr;
    const char * propName = findXMLAttValu(root, "name");

    // Check TARGET
    if (!strcmp("TARGET_EOD_COORD", propName))
    {
        int rc_ra = -1, rc_de = -1;
        double ra = 0, de = 0;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * elemName = findXMLAttValu(ep, "name");

            LOGF_DEBUG("Snooped Target RA-DEC: %s", pcdataXMLEle(ep));
            if (!strcmp(elemName, "RA"))
                rc_ra = f_scansexa(pcdataXMLEle(ep), &ra);
            else if (!strcmp(elemName, "DEC"))
                rc_de = f_scansexa(pcdataXMLEle(ep), &de);
        }
        //  Dont start moving the dome till the mount has initialized all the variables
        if (HaveRaDec && CanAbsMove())
        {
            if (rc_ra == 0 && rc_de == 0)
            {
                //  everything parsed ok, so lets start the dome to moving
                //  If this slew involves a meridian flip, then the slaving calcs will end up using
                //  the wrong OTA side.  Lets set things up so our slaving code will calculate the side
                //  for the target slew instead of using mount pier side info
                OTASideSP.s = IPS_IDLE;
                IDSetSwitch(&OTASideSP, nullptr);
                //  and see if we can get there at the same time as the mount
                mountEquatorialCoords.ra  = ra * 15.0;
                mountEquatorialCoords.dec = de;
                LOGF_DEBUG("Calling Update mount to anticipate goto target: %g - DEC: %g",
                           mountEquatorialCoords.ra, mountEquatorialCoords.dec);
                UpdateMountCoords();
            }
        }

        return true;
    }
    // Check EOD
    if (!strcmp("EQUATORIAL_EOD_COORD", propName))
    {
        int rc_ra = -1, rc_de = -1;
        double ra = 0, de = 0;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * elemName = findXMLAttValu(ep, "name");

            if (!strcmp(elemName, "RA"))
                rc_ra = f_scansexa(pcdataXMLEle(ep), &ra);
            else if (!strcmp(elemName, "DEC"))
                rc_de = f_scansexa(pcdataXMLEle(ep), &de);
        }

        if (rc_ra == 0 && rc_de == 0)
        {
            ra *= 15.0;

            // Do not spam log
            if (std::fabs(mountEquatorialCoords.ra - ra) > 0.01 || std::fabs(mountEquatorialCoords.dec - de) > 0.01)
            {
                char RAStr[64] = {0}, DEStr[64] = {0};
                fs_sexa(RAStr, ra / 15.0, 2, 3600);
                fs_sexa(DEStr, de, 2, 3600);

                LOGF_DEBUG("Snooped RA %s DEC %s", RAStr, DEStr);
            }

            mountEquatorialCoords.ra  = ra;
            mountEquatorialCoords.dec = de;
        }

        m_MountState = IPS_ALERT;
        crackIPState(findXMLAttValu(root, "state"), &m_MountState);

        // If the diff > 0.1 then the mount is in motion, so let's wait until it settles before moving the doom
        if (fabs(mountEquatorialCoords.ra - prev_ra) > DOME_COORD_THRESHOLD ||
                fabs(mountEquatorialCoords.dec - prev_dec) > DOME_COORD_THRESHOLD)
        {
            prev_ra  = mountEquatorialCoords.ra;
            prev_dec = mountEquatorialCoords.dec;
            //LOGF_DEBUG("Snooped RA: %g - DEC: %g", mountEquatorialCoords.ra, mountEquatorialCoords.dec);
            //  a mount still initializing will emit 0 and 0 on the first go
            //  we dont want to process 0/0
            if ((mountEquatorialCoords.ra != 0) || (mountEquatorialCoords.dec != 0))
                HaveRaDec = true;
        }
        // else mount stable, i.e. tracking, so let's update mount coords and check if we need to move
        else if (m_MountState == IPS_OK || m_MountState == IPS_IDLE)
            UpdateMountCoords();

        return true;
    }

    // Check Geographic coords
    if (!strcmp("GEOGRAPHIC_COORD", propName))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * elemName = findXMLAttValu(ep, "name");
            if (!strcmp(elemName, "LONG"))
            {
                double indiLong;
                f_scansexa(pcdataXMLEle(ep), &indiLong);
                if (indiLong > 180)
                    indiLong -= 360;
                observer.lng = indiLong;
                HaveLatLong  = true;
            }
            else if (!strcmp(elemName, "LAT"))
                f_scansexa(pcdataXMLEle(ep), &(observer.lat));
        }

        LOGF_DEBUG("Snooped LONG: %g - LAT: %g", observer.lng, observer.lat);

        UpdateMountCoords();

        return true;
    }

    // Check Telescope Park status
    if (!strcmp("TELESCOPE_PARK", propName))
    {
        if (!strcmp(findXMLAttValu(root, "state"), "Ok"))
        {
            bool prevState = IsLocked;
            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                const char * elemName = findXMLAttValu(ep, "name");

                if ((!strcmp(elemName, "PARK") && !strcmp(pcdataXMLEle(ep), "On")))
                    IsMountParked = true;
                else if ((!strcmp(elemName, "UNPARK") && !strcmp(pcdataXMLEle(ep), "On")))
                    IsMountParked = false;

                if (IsLocked && !strcmp(elemName, "PARK") && !strcmp(pcdataXMLEle(ep), "On"))
                    IsLocked = false;
                else if (!IsLocked && !strcmp(elemName, "UNPARK") && !strcmp(pcdataXMLEle(ep), "On"))
                    IsLocked = true;
            }
            if (prevState != IsLocked && MountPolicyS[1].s == ISS_ON)
                LOGF_INFO("Telescope status changed. Lock is set to: %s",
                          IsLocked ? "locked" : "unlocked");
        }
        return true;
    }

    // Weather Status
    // JM 2020-07-16: Weather handling moved to Watchdog driver
#if 0
    if (!strcmp("WEATHER_STATUS", propName))
    {
        weatherState = IPS_ALERT;
        crackIPState(findXMLAttValu(root, "state"), &weatherState);

        if (weatherState == IPS_ALERT)
        {
            if (CanPark())
            {
                if (!isParked())
                {
                    if (AutoParkS[0].s == ISS_ON)
                    {
                        LOG_WARN("Weather conditions in the danger zone! Parking dome...");
                        Dome::Park();
                    }
                    else
                    {
                        LOG_WARN("Weather conditions in the danger zone! AutoPark is disabled. Please park the dome!");
                    }
                }
            }
            else
                LOG_WARN("Weather conditions in the danger zone! Close the dome immediately!");

            return true;
        }
    }
#endif
    if (!strcmp("TELESCOPE_PIER_SIDE", propName))
    {
        // set defaults to say we have no valid information from mount
        bool isEast = false;
        bool isWest = false;
        OTASideS[0].s = ISS_OFF;
        OTASideS[1].s = ISS_OFF;
        OTASideSP.s = IPS_IDLE;
        //  crack the message
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char * elemName = findXMLAttValu(ep, "name");

            if (!strcmp(elemName, "PIER_EAST") && !strcmp(pcdataXMLEle(ep), "On"))
                isEast = true;
            else if (!strcmp(elemName, "PIER_WEST") && !strcmp(pcdataXMLEle(ep), "On"))
                isWest = true;
        }
        //  update the switch
        if(isEast) OTASideS[0].s = ISS_ON;
        if(isWest) OTASideS[1].s = ISS_ON;
        if(isWest || isEast) OTASideSP.s = IPS_OK;
        //  and set it.  If we didn't get valid info, it'll be set to idle and neither 'button' pressed in the ui
        IDSetSwitch(&OTASideSP, nullptr);
        return true;
    }

    controller->ISSnoopDevice(root);

    return DefaultDevice::ISSnoopDevice(root);
}

bool Dome::SetBacklash(int32_t steps)
{
    INDI_UNUSED(steps);
    LOG_ERROR("Dome does not support backlash compensation.");
    return false;
}

bool Dome::SetBacklashEnabled(bool enabled)
{
    // If disabled, set the Domeer backlash to zero.
    if (enabled)
        return SetBacklash(static_cast<int32_t>(DomeBacklashN[0].value));
    else
        return SetBacklash(0);
}

bool Dome::saveConfigItems(FILE * fp)
{
    DefaultDevice::saveConfigItems(fp);

    IUSaveConfigText(fp, &ActiveDeviceTP);
    IUSaveConfigSwitch(fp, &MountPolicySP);
    IUSaveConfigNumber(fp, &PresetNP);
    IUSaveConfigNumber(fp, &DomeParamNP);
    IUSaveConfigNumber(fp, &DomeMeasurementsNP);
    //IUSaveConfigSwitch(fp, &AutoParkSP);
    IUSaveConfigSwitch(fp, &DomeAutoSyncSP);

    if (HasBacklash())
    {
        IUSaveConfigSwitch(fp, &DomeBacklashSP);
        IUSaveConfigNumber(fp, &DomeBacklashNP);
    }

    if (HasShutter())
    {
        IUSaveConfigSwitch(fp, &ShutterParkPolicySP);
    }

    controller->saveConfigItems(fp);

    return true;
}

void Dome::triggerSnoop(const char * driverName, const char * snoopedProp)
{
    LOGF_DEBUG("Active Snoop, driver: %s, property: %s", driverName, snoopedProp);
    IDSnoopDevice(driverName, snoopedProp);
}

bool Dome::isLocked()
{
    return MountPolicyS[1].s == ISS_ON && IsLocked;
}

void Dome::buttonHelper(const char * button_n, ISState state, void * context)
{
    static_cast<Dome *>(context)->processButton(button_n, state);
}

void Dome::processButton(const char * button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

    // Dome In
    if (!strcmp(button_n, "Dome CW"))
    {
        if (DomeMotionSP.s != IPS_BUSY)
            Dome::Move(DOME_CW, MOTION_START);
        else
            Dome::Move(DOME_CW, MOTION_STOP);
    }
    else if (!strcmp(button_n, "Dome CCW"))
    {
        if (DomeMotionSP.s != IPS_BUSY)
            Dome::Move(DOME_CCW, MOTION_START);
        else
            Dome::Move(DOME_CCW, MOTION_STOP);
    }
    else if (!strcmp(button_n, "Dome Abort"))
    {
        Dome::Abort();
    }
}

IPState Dome::getMountState() const
{
    return m_MountState;
}

void Dome::setShutterState(const Dome::ShutterState &value)
{
    switch (value)
    {
        case SHUTTER_OPENED:
            IUResetSwitch(&DomeShutterSP);
            DomeShutterS[SHUTTER_OPEN].s = ISS_ON;
            DomeShutterSP.s = IPS_OK;
            break;

        case SHUTTER_CLOSED:
            IUResetSwitch(&DomeShutterSP);
            DomeShutterS[SHUTTER_CLOSE].s = ISS_ON;
            DomeShutterSP.s = IPS_OK;
            break;


        case SHUTTER_MOVING:
            DomeShutterSP.s = IPS_BUSY;
            break;

        case SHUTTER_ERROR:
            DomeShutterSP.s = IPS_ALERT;
            LOG_WARN("Shutter failure.");
            break;

        case SHUTTER_UNKNOWN:
            IUResetSwitch(&DomeShutterSP);
            DomeShutterSP.s = IPS_IDLE;
            LOG_WARN("Unknown shutter status.");
            break;
    }

    IDSetSwitch(&DomeShutterSP, nullptr);
    m_ShutterState = value;
}

void Dome::setDomeState(const Dome::DomeState &value)
{
    switch (value)
    {
        case DOME_IDLE:
            if (DomeMotionSP.s == IPS_BUSY)
            {
                IUResetSwitch(&DomeMotionSP);
                DomeMotionSP.s = IPS_IDLE;
                IDSetSwitch(&DomeMotionSP, nullptr);
            }
            if (DomeAbsPosNP.s == IPS_BUSY)
            {
                DomeAbsPosNP.s = IPS_IDLE;
                IDSetNumber(&DomeAbsPosNP, nullptr);
            }
            if (DomeRelPosNP.s == IPS_BUSY)
            {
                DomeRelPosNP.s = IPS_IDLE;
                IDSetNumber(&DomeRelPosNP, nullptr);
            }
            break;

        case DOME_SYNCED:
            if (DomeMotionSP.s == IPS_BUSY)
            {
                IUResetSwitch(&DomeMotionSP);
                DomeMotionSP.s = IPS_OK;
                IDSetSwitch(&DomeMotionSP, nullptr);
            }
            if (DomeAbsPosNP.s == IPS_BUSY)
            {
                DomeAbsPosNP.s = IPS_OK;
                IDSetNumber(&DomeAbsPosNP, nullptr);
            }
            if (DomeRelPosNP.s == IPS_BUSY)
            {
                DomeRelPosNP.s = IPS_OK;
                IDSetNumber(&DomeRelPosNP, nullptr);
            }
            break;

        case DOME_PARKED:
            if (DomeMotionSP.s == IPS_BUSY)
            {
                IUResetSwitch(&DomeMotionSP);
                DomeMotionSP.s = IPS_IDLE;
                IDSetSwitch(&DomeMotionSP, nullptr);
            }
            if (DomeAbsPosNP.s == IPS_BUSY)
            {
                DomeAbsPosNP.s = IPS_IDLE;
                IDSetNumber(&DomeAbsPosNP, nullptr);
            }
            if (DomeRelPosNP.s == IPS_BUSY)
            {
                DomeRelPosNP.s = IPS_IDLE;
                IDSetNumber(&DomeRelPosNP, nullptr);
            }
            IUResetSwitch(&ParkSP);
            ParkSP.s   = IPS_OK;
            ParkS[0].s = ISS_ON;
            IDSetSwitch(&ParkSP, nullptr);
            IsParked = true;
            break;

        case DOME_PARKING:
            IUResetSwitch(&ParkSP);
            ParkSP.s   = IPS_BUSY;
            ParkS[0].s = ISS_ON;
            IDSetSwitch(&ParkSP, nullptr);
            break;

        case DOME_UNPARKING:
            IUResetSwitch(&ParkSP);
            ParkSP.s   = IPS_BUSY;
            ParkS[1].s = ISS_ON;
            IDSetSwitch(&ParkSP, nullptr);
            break;

        case DOME_UNPARKED:
            IUResetSwitch(&ParkSP);
            ParkSP.s   = IPS_OK;
            ParkS[1].s = ISS_ON;
            IDSetSwitch(&ParkSP, nullptr);
            IsParked = false;
            break;

        case DOME_UNKNOWN:
            IUResetSwitch(&ParkSP);
            ParkSP.s = IPS_IDLE;
            IsParked = false;
            IDSetSwitch(&ParkSP, nullptr);
            break;

        case DOME_ERROR:
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, nullptr);
            break;

        case DOME_MOVING:
            break;
    }

    m_DomeState = value;
}

/*
The problem to get a dome azimuth given a telescope azimuth, altitude and geometry (telescope placement, mount geometry) can be seen as solve the intersection between the optical axis with the dome, that is, the intersection between a line and a sphere.
To do that we need to calculate the optical axis line taking the centre of the dome as origin of coordinates.
*/

// Returns false if it can't solve it due bad geometry of the observatory
// Returns:
// Az : Azimuth required to the dome in order to center the shutter aperture with telescope
// minAz: Minimum azimuth in order to avoid any dome interference to the full aperture of the telescope
// maxAz: Maximum azimuth in order to avoid any dome interference to the full aperture of the telescope
bool Dome::GetTargetAz(double &Az, double &Alt, double &minAz, double &maxAz)
{
    point3D MountCenter, OptCenter, OptVector, DomeIntersect;
    double hourAngle;
    double mu1, mu2;
    int OTASide = 1; /* Side of the telescope with respect of the mount, 1: east, -1: west*/

    if (HaveLatLong == false)
    {
        triggerSnoop(ActiveDeviceT[0].text, "GEOGRAPHIC_COORD");
        LOG_WARN( "Geographic coordinates are not yet defined, triggering snoop...");
        return false;
    }

    double JD  = ln_get_julian_from_sys();
    double MSD = ln_get_mean_sidereal_time(JD);

    LOGF_DEBUG("JD: %g - MSD: %g", JD, MSD);

    MountCenter.x = DomeMeasurementsN[DM_EAST_DISPLACEMENT].value; // Positive to East
    MountCenter.y = DomeMeasurementsN[DM_NORTH_DISPLACEMENT].value;  // Positive to North
    MountCenter.z = DomeMeasurementsN[DM_UP_DISPLACEMENT].value;    // Positive Up

    LOGF_DEBUG("MC.x: %g - MC.y: %g MC.z: %g", MountCenter.x, MountCenter.y, MountCenter.z);

    // Get hour angle in hours
    hourAngle = rangeHA( MSD + observer.lng / 15.0 - mountEquatorialCoords.ra / 15.0);

    LOGF_DEBUG("HA: %g  Lng: %g RA: %g", hourAngle, observer.lng, mountEquatorialCoords.ra);

    //  this will have state OK if the mount sent us information
    //  and it will be IDLE if not
    if(CanAbsMove() && OTASideSP.s == IPS_OK)
    {
        // process info from the mount
        if(OTASideS[0].s == ISS_ON) OTASide = -1;
        else OTASide = 1;
    }
    else
    {
        //  figure out the pier side without help from the mount
        if(hourAngle > 0) OTASide = -1;
        else OTASide = 1;
        //  if we got here because we turned off the PIER_SIDE switches in a target goto
        //  lets try get it back on
        if (CanAbsMove())
            triggerSnoop(ActiveDeviceT[0].text, "TELESCOPE_PIER_SIDE");

    }

    OpticalCenter(MountCenter, OTASide * DomeMeasurementsN[DM_OTA_OFFSET].value, observer.lat, hourAngle, OptCenter);

    LOGF_DEBUG("OTA_SIDE: %d", OTASide);
    LOGF_DEBUG("OTA_OFFSET: %g  Lat: %g", DomeMeasurementsN[DM_OTA_OFFSET].value, observer.lat);
    LOGF_DEBUG("OC.x: %g - OC.y: %g OC.z: %g", OptCenter.x, OptCenter.y, OptCenter.z);

    // To be sure mountHoriztonalCoords is up to date.
    get_hrz_from_equ(&mountEquatorialCoords, &observer, JD, &mountHoriztonalCoords);

    //    mountHoriztonalCoords.az += 180;
    //    if (mountHoriztonalCoords.az >= 360)
    //        mountHoriztonalCoords.az -= 360;
    //    if (mountHoriztonalCoords.az < 0)
    //        mountHoriztonalCoords.az += 360;

    // Get optical axis point. This and the previous form the optical axis line
    OpticalVector(mountHoriztonalCoords.az, mountHoriztonalCoords.alt, OptVector);
    LOGF_DEBUG("Mount Az: %g  Alt: %g", mountHoriztonalCoords.az, mountHoriztonalCoords.alt);
    LOGF_DEBUG("OV.x: %g - OV.y: %g OV.z: %g", OptVector.x, OptVector.y, OptVector.z);

    if (Intersection(OptCenter, OptVector, DomeMeasurementsN[DM_DOME_RADIUS].value, mu1, mu2))
    {
        // If telescope is pointing over the horizon, the solution is mu1, else is mu2
        if (mu1 < 0)
            mu1 = mu2;

        double yx;
        double HalfApertureChordAngle;
        double RadiusAtAlt;

        DomeIntersect.x = OptCenter.x + mu1 * (OptVector.x );
        DomeIntersect.y = OptCenter.y + mu1 * (OptVector.y );
        DomeIntersect.z = OptCenter.z + mu1 * (OptVector.z );

        if (fabs(DomeIntersect.x) > 0.00001)
        {
            yx = DomeIntersect.y / DomeIntersect.x;
            Az = 90 - 180 * atan(yx) / M_PI;
            if (DomeIntersect.x < 0)
            {
                Az = Az + 180;
            }
            if (Az >= 360)
                Az -= 360;
            else if (Az < 0)
                Az += 360;
        }
        else
        {
            // Dome East-West line or zenith
            if (DomeIntersect.y > 0)
                Az = 90;
            else
                Az = 270;
        }

        if ((fabs(DomeIntersect.x) > 0.00001) || (fabs(DomeIntersect.y) > 0.00001))
            Alt = 180 *
                  atan(DomeIntersect.z /
                       sqrt((DomeIntersect.x * DomeIntersect.x) + (DomeIntersect.y * DomeIntersect.y))) /
                  M_PI;
        else
            Alt = 90; // Dome Zenith

        // Calculate the Azimuth range in the given Altitude of the dome
        RadiusAtAlt = DomeMeasurementsN[DM_DOME_RADIUS].value * cos(M_PI * Alt / 180); // Radius alt the given altitude

        if (DomeMeasurementsN[DM_SHUTTER_WIDTH].value < (2 * RadiusAtAlt))
        {
            HalfApertureChordAngle = 180 * asin(DomeMeasurementsN[DM_SHUTTER_WIDTH].value / (2 * RadiusAtAlt)) /
                                     M_PI; // Angle of a chord of half aperture length
            minAz = Az - HalfApertureChordAngle;
            if (minAz < 0)
                minAz = minAz + 360;
            maxAz = Az + HalfApertureChordAngle;
            if (maxAz >= 360)
                maxAz = maxAz - 360;
        }
        else
        {
            minAz = 0;
            maxAz = 360;
        }
        return true;
    }

    return false;
}

bool Dome::Intersection(point3D p1, point3D dp, double r, double &mu1, double &mu2)
{
    double a, b, c;
    double bb4ac;

    a     = dp.x * dp.x + dp.y * dp.y + dp.z * dp.z;
    b     = 2 * (dp.x * p1.x + dp.y * p1.y + dp.z * p1.z);
    c     = 0.0;
    c     = c + p1.x * p1.x + p1.y * p1.y + p1.z * p1.z;
    c     = c - r * r;
    bb4ac = b * b - 4 * a * c;
    if ((fabs(a) < 0.0000001) || (bb4ac < 0))
    {
        mu1 = 0;
        mu2 = 0;
        return false;
    }

    mu1 = (-b + sqrt(bb4ac)) / (2 * a);
    mu2 = (-b - sqrt(bb4ac)) / (2 * a);

    return true;
}

bool Dome::OpticalCenter(point3D MountCenter, double dOpticalAxis, double Lat, double Ah, point3D &OP)
{
    double q, f;
    double cosf, sinf, cosq, sinq;

    // Note: this transformation is a circle rotated around X axis -(90 - Lat) degrees
    q = M_PI * (90 - Lat) / 180;
    f = -M_PI * (180 + Ah * 15) / 180;

    cosf = cos(f);
    sinf = sin(f);
    cosq = cos(q);
    sinq = sin(q);

    OP.x = (dOpticalAxis * cosf + MountCenter.x); // The sign of dOpticalAxis determines de side of the tube
    OP.y = (dOpticalAxis * sinf * cosq + MountCenter.y);
    OP.z = (dOpticalAxis * sinf * sinq + MountCenter.z);

    return true;
}

bool Dome::OpticalVector(double Az, double Alt, point3D &OV)
{
    double q, f;

    q    = M_PI * Alt / 180;
    f    = M_PI * Az / 180;
    OV.x = cos(q) * sin(f);
    OV.y = cos(q) * cos(f);
    OV.z = sin(q);

    return true;
}

double Dome::Csc(double x)
{
    return 1.0 / sin(x);
}

double Dome::Sec(double x)
{
    return 1.0 / cos(x);
}

bool Dome::CheckHorizon(double HA, double dec, double lat)
{
    double sinh_value;

    sinh_value = cos(lat) * cos(HA) * cos(dec) + sin(lat) * sin(dec);

    if (sinh_value >= 0.0)
        return true;

    return false;
}

void Dome::UpdateMountCoords()
{
    if (m_HorizontalUpdateTimerID > 0)
    {
        IERmTimer(m_HorizontalUpdateTimerID);
        m_HorizontalUpdateTimerID = IEAddTimer(HORZ_UPDATE_TIMER, &Dome::updateMountCoordsHelper, this);
    }

    // If not initialized yet, return.
    if (mountEquatorialCoords.ra == -1)
        return;

    //  Dont do this if we haven't had co-ordinates from the mount yet
    if (!HaveLatLong)
        return;
    if (!HaveRaDec)
        return;

    get_hrz_from_equ(&mountEquatorialCoords, &observer, ln_get_julian_from_sys(), &mountHoriztonalCoords);

    //    mountHoriztonalCoords.az += 180;
    //    if (mountHoriztonalCoords.az > 360)
    //        mountHoriztonalCoords.az -= 360;
    //    if (mountHoriztonalCoords.az < 0)
    //        mountHoriztonalCoords.az += 360;

    // Control debug flooding
    if (fabs(mountHoriztonalCoords.az - prev_az) > DOME_COORD_THRESHOLD ||
            fabs(mountHoriztonalCoords.alt - prev_alt) > DOME_COORD_THRESHOLD)
    {
        prev_az  = mountHoriztonalCoords.az;
        prev_alt = mountHoriztonalCoords.alt;
        LOGF_DEBUG("Updated telescope Az: %g - Alt: %g", prev_az, prev_alt);
    }

    // Check if we need to move if mount is unparked.
    if (IsMountParked == false)
        UpdateAutoSync();
}

void Dome::UpdateAutoSync()
{
    if ((m_MountState == IPS_OK || m_MountState == IPS_IDLE) && DomeAbsPosNP.s != IPS_BUSY && DomeAutoSyncS[0].s == ISS_ON)
    {
        if (CanPark())
        {
            if (isParked() == true)
            {
                if (AutoSyncWarning == false)
                {
                    LOG_WARN("Cannot perform autosync with dome parked. Please unpark to enable autosync operation.");
                    AutoSyncWarning = true;
                }
                return;
            }
        }

        AutoSyncWarning = false;
        double targetAz = 0, targetAlt = 0, minAz = 0, maxAz = 0;
        bool res;
        res = GetTargetAz(targetAz, targetAlt, minAz, maxAz);
        if (!res)
        {
            LOGF_DEBUG("GetTargetAz failed %g", targetAz);
            return;
        }
        LOGF_DEBUG("Calculated target azimuth is %.2f. MinAz: %.2f, MaxAz: %.2f", targetAz, minAz,
                   maxAz);

        if (fabs(targetAz - DomeAbsPosN[0].value) > DomeParamN[0].value)
        {
            IPState ret = Dome::MoveAbs(targetAz);
            if (ret == IPS_OK)
                LOGF_DEBUG("Dome synced to position %.2f degrees.", targetAz);
            else if (ret == IPS_BUSY)
                LOGF_DEBUG("Dome is syncing to position %.2f degrees...", targetAz);
            else
                LOG_ERROR("Dome failed to sync to new requested position.");

            DomeAbsPosNP.s = ret;
            IDSetNumber(&DomeAbsPosNP, nullptr);
        }
    }
}

void Dome::SetDomeCapability(uint32_t cap)
{
    capability = cap;

    if (CanAbort())
        controller->mapController("Dome Abort", "Dome Abort", Controller::CONTROLLER_BUTTON, "BUTTON_3");
}

const char * Dome::GetShutterStatusString(ShutterState status)
{
    switch (status)
    {
        case SHUTTER_OPENED:
            return "Shutter is open.";
        case SHUTTER_CLOSED:
            return "Shutter is closed.";
        case SHUTTER_MOVING:
            return "Shutter is moving.";
        case SHUTTER_ERROR:
            return "Shutter has errors.";
        case SHUTTER_UNKNOWN:
        default:
            return "Shutter status is unknown.";
    }
}

void Dome::SetParkDataType(Dome::DomeParkData type)
{
    parkDataType = type;

    switch (parkDataType)
    {
        case PARK_NONE:
            strncpy(DomeMotionS[DOME_CW].label, "Open", MAXINDILABEL);
            strncpy(DomeMotionS[DOME_CCW].label, "Close", MAXINDILABEL);
            break;

        case PARK_AZ:
            IUFillNumber(&ParkPositionN[AXIS_AZ], "PARK_AZ", "AZ D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
            IUFillNumberVector(&ParkPositionNP, ParkPositionN, 1, getDeviceName(), "DOME_PARK_POSITION", "Park Position",
                               SITE_TAB, IP_RW, 60, IPS_IDLE);
            break;

        case PARK_AZ_ENCODER:
            IUFillNumber(&ParkPositionN[AXIS_AZ], "PARK_AZ", "AZ Encoder", "%.0f", 0, 16777215, 1, 0);
            IUFillNumberVector(&ParkPositionNP, ParkPositionN, 1, getDeviceName(), "DOME_PARK_POSITION", "Park Position",
                               SITE_TAB, IP_RW, 60, IPS_IDLE);
            break;

        default:
            break;
    }
}

void Dome::SyncParkStatus(bool isparked)
{
    IsParked = isparked;

    setDomeState(DOME_IDLE);

    if (IsParked)
    {
        setDomeState(DOME_PARKED);
        DEBUG(Logger::DBG_SESSION, "Dome is parked.");
    }
    else
    {
        setDomeState(DOME_UNPARKED);
        DEBUG(Logger::DBG_SESSION, "Dome is unparked.");
    }
}

void Dome::SetParked(bool isparked)
{
    SyncParkStatus(isparked);
    WriteParkData();
}

bool Dome::isParked()
{
    return IsParked;
}

bool Dome::InitPark()
{
    const char * loadres = LoadParkData();
    if (loadres)
    {
        LOGF_INFO("InitPark: No Park data in file %s: %s", ParkDataFileName.c_str(), loadres);
        SyncParkStatus(false);
        return false;
    }

    SyncParkStatus(isParked());

    if (parkDataType != PARK_NONE)
    {
        LOGF_DEBUG("InitPark Axis1 %.2f", Axis1ParkPosition);
        ParkPositionN[AXIS_AZ].value = Axis1ParkPosition;
        IDSetNumber(&ParkPositionNP, nullptr);

        // If parked, store the position as current azimuth angle or encoder ticks
        if (isParked() && CanAbsMove())
        {
            DomeAbsPosN[0].value = ParkPositionN[AXIS_AZ].value;
            IDSetNumber(&DomeAbsPosNP, nullptr);
        }
    }

    return true;
}

const char * Dome::LoadParkXML()
{
    wordexp_t wexp;
    FILE * fp;
    LilXML * lp;
    static char errmsg[512];

    XMLEle * parkxml;
    XMLAtt * ap;
    bool devicefound = false;

    ParkDeviceName       = getDeviceName();
    ParkstatusXml        = nullptr;
    ParkdeviceXml        = nullptr;
    ParkpositionXml      = nullptr;
    ParkpositionAxis1Xml = nullptr;

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

    if (!strcmp(tagXMLEle(nextXMLEle(ParkdataXmlRoot, 1)), "parkdata"))
        return "Not a park data file";

    parkxml = nextXMLEle(ParkdataXmlRoot, 1);

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
        return "No park data found for this device";

    ParkdeviceXml        = parkxml;
    ParkstatusXml        = findXMLEle(parkxml, "parkstatus");

    if (parkDataType != PARK_NONE)
    {
        ParkpositionXml      = findXMLEle(parkxml, "parkposition");
        ParkpositionAxis1Xml = findXMLEle(ParkpositionXml, "axis1position");

        if (ParkpositionAxis1Xml == nullptr)
        {
            return "Park position invalid or missing.";
        }
    }
    else if (ParkstatusXml == nullptr)
        return "Park status invalid or missing.";

    return nullptr;
}

const char * Dome::LoadParkData()
{
    IsParked = false;

    const char * result = LoadParkXML();
    if (result != nullptr)
        return result;

    if (!strcmp(pcdataXMLEle(ParkstatusXml), "true"))
        IsParked = true;

    if (parkDataType == PARK_NONE)
        return nullptr;

    double axis1Pos = std::numeric_limits<double>::quiet_NaN();

    int rc = sscanf(pcdataXMLEle(ParkpositionAxis1Xml), "%lf", &axis1Pos);
    if (rc != 1)
    {
        return "Unable to parse Park Position Axis 1.";
    }

    if (std::isnan(axis1Pos) == false)
    {
        Axis1ParkPosition = axis1Pos;
        return nullptr;
    }

    return "Failed to parse Park Position.";
}

bool Dome::WriteParkData()
{
    // We need to refresh parking data in case other devices parking states were updated since we
    // read the data the first time.
    if (LoadParkXML() != nullptr)
        LOG_DEBUG("Failed to refresh parking data.");

    wordexp_t wexp;
    FILE * fp;
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
    if (parkDataType != PARK_NONE)
    {
        if (!ParkpositionXml)
            ParkpositionXml = addXMLEle(ParkdeviceXml, "parkposition");
        if (!ParkpositionAxis1Xml)
            ParkpositionAxis1Xml = addXMLEle(ParkpositionXml, "axis1position");
    }

    editXMLEle(ParkstatusXml, (IsParked ? "true" : "false"));

    if (parkDataType != PARK_NONE)
    {
        snprintf(pcdata, sizeof(pcdata), "%lf", Axis1ParkPosition);
        editXMLEle(ParkpositionAxis1Xml, pcdata);
    }

    prXMLEle(fp, ParkdataXmlRoot, 0);
    fclose(fp);
    wordfree(&wexp);

    return true;
}

double Dome::GetAxis1Park()
{
    return Axis1ParkPosition;
}

double Dome::GetAxis1ParkDefault()
{
    return Axis1DefaultParkPosition;
}

void Dome::SetAxis1Park(double value)
{
    Axis1ParkPosition            = value;
    ParkPositionN[AXIS_RA].value = value;
    IDSetNumber(&ParkPositionNP, nullptr);
}

void Dome::SetAxis1ParkDefault(double value)
{
    Axis1DefaultParkPosition = value;
}

IPState Dome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    // Check if it is already parked.
    if (CanPark())
    {
        if (parkDataType != PARK_NONE && isParked())
        {
            LOG_WARN( "Please unpark the dome before issuing any motion commands.");
            return IPS_ALERT;
        }
    }

    if ((DomeMotionSP.s != IPS_BUSY && (DomeAbsPosNP.s == IPS_BUSY || DomeRelPosNP.s == IPS_BUSY)) ||
            (m_DomeState == DOME_PARKING))
    {
        LOG_WARN( "Please stop dome before issuing any further motion commands.");
        return IPS_ALERT;
    }

    int current_direction = IUFindOnSwitchIndex(&DomeMotionSP);

    // if same move requested, return
    if (DomeMotionSP.s == IPS_BUSY && current_direction == dir && operation == MOTION_START)
        return IPS_BUSY;

    DomeMotionSP.s = Move(dir, operation);

    if (DomeMotionSP.s == IPS_BUSY || DomeMotionSP.s == IPS_OK)
    {
        m_DomeState = (operation == MOTION_START) ? DOME_MOVING : DOME_IDLE;
        IUResetSwitch(&DomeMotionSP);
        if (operation == MOTION_START)
            DomeMotionS[dir].s = ISS_ON;
    }

    IDSetSwitch(&DomeMotionSP, nullptr);

    return DomeMotionSP.s;
}

IPState Dome::MoveRel(double azDiff)
{
    if (CanRelMove() == false)
    {
        LOG_ERROR( "Dome does not support relative motion.");
        return IPS_ALERT;
    }

    if (m_DomeState == DOME_PARKED)
    {
        LOG_ERROR( "Please unpark before issuing any motion commands.");
        DomeRelPosNP.s = IPS_ALERT;
        IDSetNumber(&DomeRelPosNP, nullptr);
        return IPS_ALERT;
    }

    if ((DomeRelPosNP.s != IPS_BUSY && DomeMotionSP.s == IPS_BUSY) || (m_DomeState == DOME_PARKING))
    {
        LOG_WARN( "Please stop dome before issuing any further motion commands.");
        DomeRelPosNP.s = IPS_IDLE;
        IDSetNumber(&DomeRelPosNP, nullptr);
        return IPS_ALERT;
    }

    IPState rc;

    if ((rc = MoveRel(azDiff)) == IPS_OK)
    {
        m_DomeState            = DOME_IDLE;
        DomeRelPosNP.s       = IPS_OK;
        DomeRelPosN[0].value = azDiff;
        IDSetNumber(&DomeRelPosNP, "Dome moved %.2f degrees %s.", azDiff,
                    (azDiff > 0) ? "clockwise" : "counter clockwise");
        if (CanAbsMove())
        {
            DomeAbsPosNP.s = IPS_OK;
            IDSetNumber(&DomeAbsPosNP, nullptr);
        }
        return IPS_OK;
    }
    else if (rc == IPS_BUSY)
    {
        m_DomeState = DOME_MOVING;
        DomeRelPosN[0].value = azDiff;
        DomeRelPosNP.s = IPS_BUSY;
        IDSetNumber(&DomeRelPosNP, "Dome is moving %.2f degrees %s...", azDiff,
                    (azDiff > 0) ? "clockwise" : "counter clockwise");
        if (CanAbsMove())
        {
            DomeAbsPosNP.s = IPS_BUSY;
            IDSetNumber(&DomeAbsPosNP, nullptr);
        }

        DomeMotionSP.s = IPS_BUSY;
        IUResetSwitch(&DomeMotionSP);
        DomeMotionS[DOME_CW].s  = (azDiff > 0) ? ISS_ON : ISS_OFF;
        DomeMotionS[DOME_CCW].s = (azDiff < 0) ? ISS_ON : ISS_OFF;
        IDSetSwitch(&DomeMotionSP, nullptr);
        return IPS_BUSY;
    }

    m_DomeState      = DOME_IDLE;
    DomeRelPosNP.s = IPS_ALERT;
    LOG_WARN("Dome failed to move to new requested position.");
    IDSetNumber(&DomeRelPosNP, nullptr);
    return IPS_ALERT;
}

IPState Dome::MoveAbs(double az)
{
    if (CanAbsMove() == false)
    {
        LOG_ERROR("Dome does not support MoveAbs(). MoveAbs() must be implemented in the child class.");
        return IPS_ALERT;
    }

    if (m_DomeState == DOME_PARKED)
    {
        LOG_ERROR( "Please unpark before issuing any motion commands.");
        DomeAbsPosNP.s = IPS_ALERT;
        IDSetNumber(&DomeAbsPosNP, nullptr);
        return IPS_ALERT;
    }

    if ((DomeRelPosNP.s != IPS_BUSY && DomeMotionSP.s == IPS_BUSY) || (m_DomeState == DOME_PARKING))
    {
        LOG_WARN( "Please stop dome before issuing any further motion commands.");
        return IPS_ALERT;
    }

    IPState rc;

    if (az < DomeAbsPosN[0].min || az > DomeAbsPosN[0].max)
    {
        LOGF_ERROR( "Error: requested azimuth angle %.2f is out of range.", az);
        DomeAbsPosNP.s = IPS_ALERT;
        IDSetNumber(&DomeAbsPosNP, nullptr);
        return IPS_ALERT;
    }

    if ((rc = MoveAbs(az)) == IPS_OK)
    {
        m_DomeState            = DOME_IDLE;
        DomeAbsPosNP.s       = IPS_OK;
        DomeAbsPosN[0].value = az;
        LOGF_INFO("Dome moved to position %.2f degrees azimuth.", az);
        IDSetNumber(&DomeAbsPosNP, nullptr);

        return IPS_OK;
    }
    else if (rc == IPS_BUSY)
    {
        m_DomeState      = DOME_MOVING;
        DomeAbsPosNP.s = IPS_BUSY;
        LOGF_INFO("Dome is moving to position %.2f degrees azimuth...", az);
        IDSetNumber(&DomeAbsPosNP, nullptr);

        DomeMotionSP.s = IPS_BUSY;
        IUResetSwitch(&DomeMotionSP);
        DomeMotionS[DOME_CW].s  = (az > DomeAbsPosN[0].value) ? ISS_ON : ISS_OFF;
        DomeMotionS[DOME_CCW].s = (az < DomeAbsPosN[0].value) ? ISS_ON : ISS_OFF;
        IDSetSwitch(&DomeMotionSP, nullptr);

        return IPS_BUSY;
    }

    m_DomeState      = DOME_IDLE;
    DomeAbsPosNP.s = IPS_ALERT;
    IDSetNumber(&DomeAbsPosNP, "Dome failed to move to new requested position.");
    return IPS_ALERT;
}

bool Dome::Sync(double az)
{
    INDI_UNUSED(az);
    LOG_WARN("Syncing is not supported.");
    return false;
}

bool Dome::Abort()
{
    if (CanAbort() == false)
    {
        LOG_ERROR( "Dome does not support abort.");
        return false;
    }

    IUResetSwitch(&AbortSP);

    if (Abort())
    {
        AbortSP.s = IPS_OK;

        if (m_DomeState == DOME_PARKING || m_DomeState == DOME_UNPARKING)
        {
            IUResetSwitch(&ParkSP);
            if (m_DomeState == DOME_PARKING)
            {
                DEBUG(Logger::DBG_SESSION, "Parking aborted.");
                // If parking was aborted then it was UNPARKED before
                ParkS[1].s = ISS_ON;
            }
            else
            {
                DEBUG(Logger::DBG_SESSION, "UnParking aborted.");
                // If unparking aborted then it was PARKED before
                ParkS[0].s = ISS_ON;
            }

            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, nullptr);
        }

        setDomeState(DOME_IDLE);
    }
    else
    {
        AbortSP.s = IPS_ALERT;

        // If alert was aborted during parking or unparking, the parking state is unknown
        if (m_DomeState == DOME_PARKING || m_DomeState == DOME_UNPARKING)
        {
            IUResetSwitch(&ParkSP);
            ParkSP.s = IPS_IDLE;
            IDSetSwitch(&ParkSP, nullptr);
        }
    }

    IDSetSwitch(&AbortSP, nullptr);

    return (AbortSP.s == IPS_OK);
}

bool Dome::SetSpeed(double speed)
{
    if (HasVariableSpeed() == false)
    {
        LOG_ERROR( "Dome does not support variable speed.");
        return false;
    }

    if (SetSpeed(speed))
    {
        DomeSpeedNP.s       = IPS_OK;
        DomeSpeedN[0].value = speed;
    }
    else
        DomeSpeedNP.s = IPS_ALERT;

    IDSetNumber(&DomeSpeedNP, nullptr);

    return (DomeSpeedNP.s == IPS_OK);
}

IPState Dome::ControlShutter(ShutterOperation operation)
{
    if (HasShutter() == false)
    {
        LOG_ERROR( "Dome does not have shutter control.");
        return IPS_ALERT;
    }

    //    if (weatherState == IPS_ALERT && operation == SHUTTER_OPEN)
    //    {
    //        LOG_WARN( "Weather is in the danger zone! Cannot open shutter.");
    //        return IPS_ALERT;
    //    }

    int currentStatus = IUFindOnSwitchIndex(&DomeShutterSP);

    // No change of status, let's return
    if (DomeShutterSP.s == IPS_BUSY && currentStatus == operation)
    {
        IDSetSwitch(&DomeShutterSP, nullptr);
        return DomeShutterSP.s;
    }

    DomeShutterSP.s = ControlShutter(operation);

    if (DomeShutterSP.s == IPS_OK)
    {
        IDSetSwitch(&DomeShutterSP, "Shutter is %s.", (operation == SHUTTER_OPEN ? "open" : "closed"));
        setShutterState(operation == SHUTTER_OPEN ? SHUTTER_OPENED : SHUTTER_CLOSED);
        return DomeShutterSP.s;
    }
    else if (DomeShutterSP.s == IPS_BUSY)
    {
        IUResetSwitch(&DomeShutterSP);
        DomeShutterS[operation].s = ISS_ON;
        IDSetSwitch(&DomeShutterSP, "Shutter is %s...", (operation == 0 ? "opening" : "closing"));
        setShutterState(SHUTTER_MOVING);
        return DomeShutterSP.s;
    }

    IDSetSwitch(&DomeShutterSP, "Shutter failed to %s.", (operation == 0 ? "open" : "close"));
    return IPS_ALERT;
}

IPState Dome::Park()
{
    // Not really supposed to get this at all, but just in case.
    if (CanPark() == false)
    {
        LOG_ERROR( "Dome does not support parking.");
        return IPS_ALERT;
    }

    // No need to park if parked already.
    if (m_DomeState == DOME_PARKED)
    {
        IUResetSwitch(&ParkSP);
        ParkS[0].s = ISS_ON;
        LOG_INFO("Dome already parked.");
        IDSetSwitch(&ParkSP, nullptr);
        return IPS_OK;
    }

    // Check if dome is locked due to Mount Policy
    if (isLocked())
    {
        IUResetSwitch(&ParkSP);
        ParkS[1].s = ISS_ON;
        ParkSP.s = IPS_ALERT;
        IDSetSwitch(&ParkSP, nullptr);
        LOG_INFO("Cannot Park Dome when mount is locking. See: Mount Policy in options tab.");
        return IPS_ALERT;
    }

    // Now ask child driver to start the actual parking process.
    ParkSP.s = Park();

    // IPS_OK is when it is immediately parked so realisticly this does not happen
    // unless dome is physically parked but just needed a state change.
    if (ParkSP.s == IPS_OK)
        SetParked(true);
    // Dome is moving to parking position
    else if (ParkSP.s == IPS_BUSY)
    {
        setDomeState(DOME_PARKING);

        if (CanAbsMove())
            DomeAbsPosNP.s = IPS_BUSY;

        IUResetSwitch(&ParkSP);
        ParkS[0].s = ISS_ON;
    }
    else
        IDSetSwitch(&ParkSP, nullptr);

    return ParkSP.s;
}

IPState Dome::UnPark()
{
    if (CanPark() == false)
    {
        LOG_ERROR( "Dome does not support parking.");
        return IPS_ALERT;
    }

    if (m_DomeState != DOME_PARKED)
    {
        IUResetSwitch(&ParkSP);
        ParkS[1].s = ISS_ON;
        DEBUG(Logger::DBG_SESSION, "Dome already unparked.");
        IDSetSwitch(&ParkSP, nullptr);
        return IPS_OK;
    }

    //    if (weatherState == IPS_ALERT)
    //    {
    //        LOG_WARN( "Weather is in the danger zone! Cannot unpark dome.");
    //        ParkSP.s = IPS_OK;
    //        IDSetSwitch(&ParkSP, nullptr);
    //        return IPS_ALERT;
    //    }

    ParkSP.s = UnPark();

    if (ParkSP.s == IPS_OK)
        SetParked(false);
    else if (ParkSP.s == IPS_BUSY)
        setDomeState(DOME_UNPARKING);
    else
        IDSetSwitch(&ParkSP, nullptr);

    return ParkSP.s;
}

bool Dome::SetCurrentPark()
{
    LOG_WARN( "Parking is not supported.");
    return false;
}

bool Dome::SetDefaultPark()
{
    LOG_WARN( "Parking is not supported.");
    return false;
}

bool Dome::Handshake()
{
    return false;
}

bool Dome::callHandshake()
{
    if (domeConnection > 0)
    {
        if (getActiveConnection() == serialConnection)
            PortFD = serialConnection->getPortFD();
        else if (getActiveConnection() == tcpConnection)
            PortFD = tcpConnection->getPortFD();
    }

    return Handshake();
}

uint8_t Dome::getDomeConnection() const
{
    return domeConnection;
}

void Dome::setDomeConnection(const uint8_t &value)
{
    uint8_t mask = CONNECTION_SERIAL | CONNECTION_TCP | CONNECTION_NONE;

    if (value == 0 || (mask & value) == 0)
    {
        LOGF_ERROR( "Invalid connection mode %d", value);
        return;
    }

    domeConnection = value;
}

void Dome::updateMountCoordsHelper(void *context)
{
    static_cast<Dome*>(context)->UpdateMountCoords();
}

}
