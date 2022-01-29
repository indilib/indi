/*
    NexDome Beaver Controller

    Copyright (C) 2021 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Modified 2021 Sifan Kahale (sifan.kahale@gmail.com)

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

#include "nexdome_beaver.h"

#include "indicom.h"
#include "connectionplugins/connectiontcp.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <cassert>
#include <memory>
#include <regex>

#include <termios.h>
#include <unistd.h>
#include "config.h"

static std::unique_ptr<Beaver> dome(new Beaver());

Beaver::Beaver()
{
    setVersion(1, 1);
    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_ABS_MOVE | DOME_CAN_REL_MOVE | DOME_CAN_PARK);

    setDomeConnection(CONNECTION_TCP | CONNECTION_SERIAL);
}

bool Beaver::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_AZ);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    ///////////////////////////////////////////////////////////////////////////////
    /// Main Tab
    ///////////////////////////////////////////////////////////////////////////////
    // Rotator status
    RotatorStatusTP[0].fill("RSTATUS", "Status", "Idle");
    RotatorStatusTP.fill(getDeviceName(), "ROTATORSTATUS", "Dome", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Shutter status
    ShutterStatusTP[0].fill("SSTATUS", "Status", "Idle");
    ShutterStatusTP.fill(getDeviceName(), "SHUTTERSTATUS", "Shutter", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Shutter Voltage
    ShutterVoltsNP[0].fill("SHUTTERvolts", "Volts", "%.2f", 0.00, 15.00, 0.00, 0.00);
    ShutterVoltsNP.fill(getDeviceName(), "SHUTTERVOLTS", "Shutter", MAIN_CONTROL_TAB, IP_RO, 60, IPS_OK);

    // Rotator Home
    GotoHomeSP[0].fill("ROTATOR_HOME_GOTO", "Home", ISS_OFF);
    GotoHomeSP.fill(getDefaultName(), "ROTATOR_GOTO_Home", "Rotator", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Rototor settings tab
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Home position
    HomePositionNP[0].fill("HOME_AZ", "AZ D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
    HomePositionNP.fill(getDeviceName(), "HOME_POSITION", "Home Position", SITE_TAB, IP_RW, 60, IPS_IDLE);

    // Home set options
    HomeOptionsSP[HOMECURRENT].fill("HOME_CURRENT", "Current", ISS_OFF);
    HomeOptionsSP[HOMEDEFAULT].fill("HOME_DEFAULT", "Default (0)", ISS_OFF);
    HomeOptionsSP.fill(getDeviceName(), "DOME_HOME_OPTION", "Home Options", SITE_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Rotator Calibrations
    RotatorCalibrationSP[ROTATOR_HOME_FIND].fill("ROTATOR_HOME_FIND", "Find Home", ISS_OFF);
    RotatorCalibrationSP[ROTATOR_HOME_MEASURE].fill("ROTATOR_HOME_MEASURE", "Measure Home", ISS_OFF);
    RotatorCalibrationSP.fill(getDefaultName(), "ROTATOR_CALIBRATION", "Rotator", ROTATOR_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Rotator Settings
    RotatorSettingsNP[ROTATOR_MAX_SPEED].fill("ROTATOR_MAX_SPEED", "Max Speed (m/s)", "%.f", 1, 1000, 10, 800);
    RotatorSettingsNP[ROTATOR_MIN_SPEED].fill("ROTATOR_MIN_SPEED", "Min Speed (m/s)", "%.f", 1, 1000, 10, 400);
    RotatorSettingsNP[ROTATOR_ACCELERATION].fill("ROTATOR_ACCELERATION", "Acceleration (m/s^2)", "%.f", 1, 1000, 10, 500);
    RotatorSettingsNP[ROTATOR_TIMEOUT].fill("ROTATOR_TIMEOUT", "Timeout (s)", "%.f", 1, 1000, 10, 83);
    RotatorSettingsNP.fill(getDeviceName(), "ROTATOR_SETTINGS", "Settings", ROTATOR_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Shutter settings tab
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Shutter Home (calibrate, reset)
    ShutterCalibrationSP[SHUTTER_HOME_FIND].fill("SHUTTER_HOME_FIND", "AutoCalibrate", ISS_OFF);
    ShutterCalibrationSP.fill(getDeviceName(), "SHUTTER_CALIBRATION", "Shutter", SHUTTER_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Shutter Settings
    ShutterSettingsNP[SHUTTER_MAX_SPEED].fill("SHUTTER_MAX_SPEED", "Max Speed (m/s)", "%.f", 1, 1000, 10, 800);
    ShutterSettingsNP[SHUTTER_MIN_SPEED].fill("SHUTTER_MIN_SPEED", "Min Speed (m/s)", "%.f", 1, 1000, 10, 400);
    ShutterSettingsNP[SHUTTER_ACCELERATION].fill("SHUTTER_ACCELERATION", "Acceleration (m/s^2)", "%.f", 1, 1000, 10, 500);
    ShutterSettingsNP[SHUTTER_SAFE_VOLTAGE].fill("SHUTTER_SAFE_VOLTAGE", "Safe Voltage", "%.1f", 10, 14, .5, 11);
    ShutterSettingsNP.fill(getDeviceName(), "SHUTTER_SETTINGS", "Settings", SHUTTER_TAB, IP_RW, 60, IPS_IDLE);

    ShutterSettingsTimeoutNP[0].fill("SHUTTER_TIMEOUT", "Timeout (s)", "%.f", 1, 1000, 10, 83);
    ShutterSettingsTimeoutNP.fill(getDeviceName(), "SHUTTER_R_SETTINGS", "Settings", SHUTTER_TAB, IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // INFO Tab
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Beaver Firmware Version
    VersionTP[0].fill("CVERSION", "Controller", "");
    VersionTP.fill(getDeviceName(), "DOME_FIRMWARE", "Version", CONNECTION_TAB, IP_RO, 0, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Communication
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // NOTE need to figure out how to get network connection working
    tcpConnection->setDefaultHost("192.168.1.1");
    tcpConnection->setDefaultPort(10000);
    tcpConnection->setConnectionType(Connection::TCP::TYPE_UDP);
    tty_set_generic_udp_format(1);
    addDebugControl();
    return true;
}

bool Beaver::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        double curPark;
        if (!sendCommand("!domerot getpark#", curPark))
            return false;
        if (InitPark())
        {
            SetAxis1ParkDefault(curPark);
        }
        else
        {
            SetAxis1Park(curPark);
            SetAxis1ParkDefault(curPark);
        }
        TimerHit();

        defineProperty(&VersionTP);
        defineProperty(&HomePositionNP);
        defineProperty(&HomeOptionsSP);
        defineProperty(&RotatorCalibrationSP);
        defineProperty(&GotoHomeSP);
        defineProperty(&RotatorSettingsNP);
        defineProperty(&RotatorStatusTP);
        if (shutterOnLine()) {
            defineProperty(&ShutterCalibrationSP);
            defineProperty(&ShutterSettingsNP);
            defineProperty(&ShutterSettingsTimeoutNP);
            defineProperty(&ShutterStatusTP);
            defineProperty(&ShutterVoltsNP);
        }
    }
    else
    {
        deleteProperty(VersionTP.getName());
        deleteProperty(RotatorCalibrationSP.getName());
        deleteProperty(GotoHomeSP.getName());
        deleteProperty(HomePositionNP.getName());
        deleteProperty(HomeOptionsSP.getName());
        deleteProperty(RotatorSettingsNP.getName());
        deleteProperty(ShutterSettingsTimeoutNP.getName());
        deleteProperty(RotatorStatusTP.getName());
        deleteProperty(ShutterCalibrationSP.getName());
        deleteProperty(ShutterSettingsNP.getName());
        deleteProperty(ShutterStatusTP.getName());
        deleteProperty(ShutterVoltsNP.getName());

    }
    return true;
}


//////////////////////////////////////////////////////////////////////////////
///  Handshake
//////////////////////////////////////////////////////////////////////////////
bool Beaver::Handshake()
{
    if (echo()) {
        // Check if shutter is online
        if (shutterOnLine()) {
            LOG_DEBUG("Shutter in online, enabling Dome has shutter property");
            SetDomeCapability(GetDomeCapability() | DOME_HAS_SHUTTER);
            return true;
        }
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
/// Set default name
//////////////////////////////////////////////////////////////////////////////
const char *Beaver::getDefaultName()
{
    return  "NexDome Beaver";
}

//////////////////////////////////////////////////////////////////////////////
/// Extended Handshake
//////////////////////////////////////////////////////////////////////////////
bool Beaver::echo()
{
    double res = 0;

    // retrieve the controller version from the dome
    char result[DRIVER_LEN] = {0};
    if (!sendRawCommand("!seletek tversion#", result)) {
        LOG_ERROR("Error getting version info");
        return false;
    }
    std::string resString = result;
    LOGF_DEBUG("Version string returned %s", resString.c_str());
    std::regex e(R"(.*:\d*:(.*))");
    std::sregex_iterator iter(resString.begin(), resString.end(), e);
    VersionTP[0].setText((*iter)[1]);

    // retrieve the current az from the dome
    if (rotatorGetAz())
        LOGF_INFO("Dome reports az: %.1f", DomeAbsPosN[0].value);
    else
        return false;

    // retrieve the current home offset from the dome
    if (!sendCommand("!domerot gethome#", res))
        return false;
    else {
        HomePositionNP[0].setValue(res);
        LOGF_INFO("Dome reports home offset: %f", res);
    }

    // retrieve the current park position from the dome
    if (!sendCommand("!domerot getpark#", res))
        return false;
    else {
        SetAxis1Park(res);
        LOGF_INFO("Dome reports park: %.1f", res);
    }

    // get current rotator settings
    if (!rotatorGetSettings())
        return false;

    // get current shutter settings
    if (shutterOnLine()) {
        if (!shutterGetSettings())
            return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////
/// Switch field updated
//////////////////////////////////////////////////////////////////////////////
bool Beaver::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////
        // Rotator Calibration (find and measure home)
        /////////////////////////////////////////////
        if (RotatorCalibrationSP.isNameMatch(name))
        {
            RotatorCalibrationSP.update(states, names, n);
            bool rc = false;
            switch (RotatorCalibrationSP.findOnSwitchIndex())
            {
                case ROTATOR_HOME_FIND:
                    rc = rotatorFindHome();
                    RotatorCalibrationSP.setState(rc ? IPS_BUSY : IPS_ALERT);
                    break;

                case ROTATOR_HOME_MEASURE:
                    rc = rotatorMeasureHome();
                    RotatorCalibrationSP.setState(rc ? IPS_BUSY : IPS_ALERT);
                    break;
            }
            RotatorCalibrationSP.apply();
            return true;
        }

        /////////////////////////////////////////////
        // Rotator Go Home
        /////////////////////////////////////////////
        if (GotoHomeSP.isNameMatch(name))
        {
            GotoHomeSP.update(states, names, n);
            bool rc = false;
            rc = rotatorGotoHome();
            GotoHomeSP.setState(rc ? IPS_BUSY : IPS_ALERT);
            GotoHomeSP.apply();
            return true;
        }

        /////////////////////////////////////////////
        // Home options
        /////////////////////////////////////////////
        if (HomeOptionsSP.isNameMatch(name))
        {
            HomeOptionsSP.update(states, names, n);
            bool rc = false;
            double newAz, curHome;
            switch (HomeOptionsSP.findOnSwitchIndex())
            {
                case HOMECURRENT:
                    if (!sendCommand("!domerot getpark#", curHome))
                        return false;
                    newAz = 360.0 - curHome + rotatorGetAz();
                    rc = rotatorSetHome(newAz);
                    LOGF_DEBUG("New home az %.1f (from  360 - %1f + %1f)", newAz, curHome, rotatorGetAz());
                    HomeOptionsSP.setState(rc ? IPS_BUSY : IPS_ALERT);
                    break;

                case HOMEDEFAULT:
                    rc = rotatorSetHome(0.0);
                    HomeOptionsSP.setState(rc ? IPS_OK : IPS_ALERT);
                    break;
            }
            HomeOptionsSP.apply();
            return true;
        }

        /////////////////////////////////////////////
        // Shutter Calibration
        /////////////////////////////////////////////
        if (ShutterCalibrationSP.isNameMatch(name))
        {  //TEST
            ShutterCalibrationSP.update(states, names, n);
            bool rc = shutterFindHome();
            if (rc)
                setShutterState(SHUTTER_MOVING);
            ShutterCalibrationSP.setState(rc ? IPS_BUSY : IPS_ALERT);
            ShutterCalibrationSP.apply();
            return true;
        }
    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

//////////////////////////////////////////////////////////////////////////////
/// Number field updated
//////////////////////////////////////////////////////////////////////////////
bool Beaver::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////
        // Rotator Settings
        /////////////////////////////////////////////
        if (RotatorSettingsNP.isNameMatch(name))
        {
            RotatorSettingsNP.update(values, names, n);
            RotatorSettingsNP.setState(rotatorSetSettings(RotatorSettingsNP[ROTATOR_MAX_SPEED].getValue(),
                                                          RotatorSettingsNP[ROTATOR_MIN_SPEED].getValue(),
                                                          RotatorSettingsNP[ROTATOR_ACCELERATION].getValue(),
                                                          RotatorSettingsNP[ROTATOR_TIMEOUT].getValue()
                                                          ) ? IPS_OK : IPS_ALERT);
            RotatorSettingsNP.apply();
            return true;
        }

        /////////////////////////////////////////////
        // Shutter Settings
        /////////////////////////////////////////////
        if (ShutterSettingsNP.isNameMatch(name))
        {
            ShutterSettingsNP.update(values, names, n);
            ShutterSettingsNP.setState(shutterSetSettings(ShutterSettingsNP[SHUTTER_MAX_SPEED].getValue(),
                                                          ShutterSettingsNP[SHUTTER_MIN_SPEED].getValue(),
                                                          ShutterSettingsNP[SHUTTER_ACCELERATION].getValue(),
                                                          ShutterSettingsNP[SHUTTER_SAFE_VOLTAGE].getValue()
                                                          ) ? IPS_OK : IPS_ALERT);
            ShutterSettingsNP.apply();
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Home Position
        ///////////////////////////////////////////////////////////////////////////////
        if (HomePositionNP.isNameMatch(name))
        {
            HomePositionNP.update(values, names, n);
            if (rotatorSetHome(HomePositionNP[0].getValue())) {
                HomePositionNP.apply();
                return true;
            }
            else
                return false;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Park Position
        ///////////////////////////////////////////////////////////////////////////////
        if (strcmp(name, ParkPositionNP.name) == 0) {
            IUUpdateNumber(&ParkPositionNP, values, names, n);
            if (rotatorSetPark(ParkPositionN[AXIS_RA].value)) {
                ParkPositionNP.s = IPS_OK;
                SetAxis1Park(ParkPositionN[AXIS_RA].value);
                IDSetNumber(&ParkPositionNP, nullptr);
                return true;
            }
            else
                return false;
        }

    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);

}

///////////////////////////////////////////////////////////////////////////
/// Timer hit - update appropriate fields
///////////////////////////////////////////////////////////////////////////
void Beaver::TimerHit()
{
    if (!isConnected()) {
        return;
    }

    // Get Position and sets az pos field
    rotatorGetAz();
    LOGF_DEBUG("Rotator position: %f", DomeAbsPosN[0].value);

    // Query rotator status
    uint16_t domeStatus = 0;
    if (!getDomeStatus(domeStatus)) {
        LOG_ERROR("Could not get dome status");
    }

    ////////////////////////////////////////////
    // Test for general dome errors
    ////////////////////////////////////////////
    if (domeStatus & DOME_STATUS_UNSAFE_CW) {
        LOG_ERROR("CW Unsafe Error");
        setDomeState(DOME_ERROR);
        RotatorStatusTP.apply();
    }
    if (domeStatus & DOME_STATUS_UNSAFE_RG) {
        LOG_ERROR("RGx Unsafe Error");
        setDomeState(DOME_ERROR);
        RotatorStatusTP.apply();
    }

    ////////////////////////////////////////////
    // Test rotator and set status
    ////////////////////////////////////////////

    // Is rotator idle?
    if (!(domeStatus & DOME_STATUS_ROTATOR_MOVING)) {

        // Dome Parked
        if (rotatorIsParked() & (getDomeState() == DOME_PARKING))  {
            SetParked(true);
            std::string rStatus = "Parked";
            RotatorStatusTP[0].setText(rStatus);
            RotatorStatusTP.setState(IPS_OK);
            LOG_DEBUG("Dome is parked.");
        }
        // Measuring Home completed
        else if (!strcmp(RotatorStatusTP[0].getText(), "Measuring Home")) {
            setDomeState(DOME_IDLE);
            RotatorCalibrationSP.setState(IPS_OK);
            RotatorCalibrationSP.apply();
            std::string rStatus = "Home";
            RotatorStatusTP[0].setText(rStatus);
            RotatorStatusTP.setState(IPS_OK);
        }
        // Finding Home completed
        else if (!strcmp(RotatorStatusTP[0].getText(), "Finding Home")) {
            setDomeState(DOME_IDLE);
            RotatorCalibrationSP.setState(IPS_OK);
            RotatorCalibrationSP.apply();
            std::string rStatus = "Home";
            RotatorStatusTP[0].setText(rStatus);
            RotatorStatusTP.setState(IPS_OK);
        }
        // Homing completed
        else if (!strcmp(RotatorStatusTP[0].getText(), "Homing")) {
            setDomeState(DOME_IDLE);
            std::string rStatus = "Home";
            RotatorStatusTP[0].setText(rStatus);
            RotatorStatusTP.setState(IPS_OK);
            GotoHomeSP.setState(IPS_OK);
            GotoHomeSP.apply();
            LOG_DEBUG("Dome at home");
        }
        // Move completed
        else if (getDomeState() == DOME_MOVING) {
            RotatorStatusTP.apply();
            setDomeState(DOME_IDLE);
            RotatorCalibrationSP.setState(IPS_OK);
            RotatorCalibrationSP.apply();
            std::string rStatus = "Idle";
            RotatorStatusTP[0].setText(rStatus);
            RotatorStatusTP.setState(IPS_OK);
            LOG_DEBUG("Dome reached target position.");
        }
        RotatorStatusTP.apply();
    }

    ////////////////////////////////////////////
    // Test Shutter and set status
    ////////////////////////////////////////////
    if (shutterOnLine()) {

        // Test for shutter error
        if (domeStatus & DOME_STATUS_SHUTTER_ERROR) {
            LOG_ERROR("Shutter Mechanical Error");
            std::string rStatus = "Mechanical Error";
            ShutterStatusTP[0].setText(rStatus);
            ShutterStatusTP.apply();
            setShutterState(SHUTTER_ERROR);
        }

        // If moving, set status
        if (getShutterState() == SHUTTER_MOVING) {

            if (domeStatus & DOME_STATUS_SHUTTER_OPENING) {
                setShutterState(SHUTTER_MOVING);
                std::string rStatus = "Opening";
                ShutterStatusTP[0].setText(rStatus);
                LOG_DEBUG("Shutter state set to Opening");
            }
            else if (domeStatus & DOME_STATUS_SHUTTER_CLOSING) {
                setShutterState(SHUTTER_MOVING);
                std::string rStatus = "Closing";
                ShutterStatusTP[0].setText(rStatus);
                LOG_DEBUG("Shutter state set to Closing");
            }
            else if (domeStatus & DOME_STATUS_SHUTTER_MOVING) {
                setShutterState(SHUTTER_MOVING);
                std::string rStatus = "Moving";
                ShutterStatusTP[0].setText(rStatus);
                LOG_DEBUG("Shutter is moving");
            }

        }

        // if stopped, test if opened or closed
        if (domeStatus & DOME_STATUS_SHUTTER_OPENED) {
            setShutterState(SHUTTER_OPENED);
            std::string rStatus = "Open";
            ShutterStatusTP[0].setText(rStatus);
            LOG_DEBUG("Shutter state set to OPEN");
        }
        if (domeStatus & DOME_STATUS_SHUTTER_CLOSED) {
            setShutterState(SHUTTER_CLOSED);
            std::string rStatus = "Closed";
            ShutterStatusTP[0].setText(rStatus);
            LOG_DEBUG("Shutter state set to CLOSED");
        }
        ShutterStatusTP.apply();

        // Update shutter voltage
        double res;
        // ignoring a random get voltage cmd error here and just reporting successful status
        if (sendCommand("!dome getshutterbatvoltage#", res)) {
            LOGF_DEBUG("Shutter voltage currently is: %.2f", res);
            ShutterVoltsNP[0].setValue(res);
            (res < ShutterSettingsNP[SHUTTER_SAFE_VOLTAGE].getValue()) ? ShutterVoltsNP.setState(IPS_ALERT) : ShutterVoltsNP.setState(IPS_OK);
            ShutterVoltsNP.apply();
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

//////////////////////////////////////////////////////////////////////////////
/// Rotator absolute move
//////////////////////////////////////////////////////////////////////////////
IPState Beaver::MoveAbs(double az)
{
    if (rotatorGotoAz(az))
    {
        m_TargetRotatorAz = az;
        setDomeState(DOME_MOVING);
        std::string rStatus = "Moving";
        RotatorStatusTP[0].setText(rStatus);
        RotatorStatusTP.apply();
        return IPS_BUSY;
    }
    return IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////////////
/// Rotator relative move (calc's offset and calles abs move)
//////////////////////////////////////////////////////////////////////////////
IPState Beaver::MoveRel(double azDiff)
{

    azDiff = domeDir * azDiff;
    m_TargetRotatorAz = DomeAbsPosN[0].value + azDiff;

    if (m_TargetRotatorAz < DomeAbsPosN[0].min)
        m_TargetRotatorAz += DomeAbsPosN[0].max;
    if (m_TargetRotatorAz > DomeAbsPosN[0].max)
        m_TargetRotatorAz -= DomeAbsPosN[0].max;
    LOGF_DEBUG("Requested rel move of %.1f", azDiff);
    lastAzDiff = fabs(azDiff);
    return MoveAbs(m_TargetRotatorAz);
}

//////////////////////////////////////////////////////////////////////////////
/// Rotator move (calc's offset and calles abs move)
//////////////////////////////////////////////////////////////////////////////
IPState Beaver::Move(DomeDirection dir, DomeMotionCommand operation)
 {
    LOG_DEBUG("Re-implemented move was called");
     if (operation == MOTION_START)
     {
         if (dir == DOME_CW) {
             domeDir = 1;
         }
         else {
             domeDir = -1;
         }
         MoveRel(lastAzDiff);

     }
     return IPS_OK;
 }

//////////////////////////////////////////////////////////////////////////////
/// open or close the shutter (will not show if shutter is not present)
//////////////////////////////////////////////////////////////////////////////
IPState Beaver::ControlShutter(ShutterOperation operation)
{
    double res = 0;
    if (operation == SHUTTER_OPEN)
    {
        if (sendCommand("!dome openshutter#", res)) {
            setShutterState(SHUTTER_MOVING);
            return IPS_BUSY;
        }
        else
            return IPS_ALERT;
    }
    else if (operation == SHUTTER_CLOSE)
    {
        if (sendCommand("!dome closeshutter#", res)) {
            setShutterState(SHUTTER_MOVING);
            return IPS_BUSY;
        }
        else
            return IPS_ALERT;
    }
    return IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////////////
/// INDI save config
//////////////////////////////////////////////////////////////////////////////
bool Beaver::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// tells rotator to goto az pos
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorGotoAz(double az)
{
    char cmd[DRIVER_LEN] = {0};
    double res = 0;
    snprintf(cmd, DRIVER_LEN, "!dome gotoaz %.2f#", az);
    setDomeState(DOME_MOVING);
    std::string rStatus = "Moving";
    RotatorStatusTP[0].setText(rStatus);
    RotatorStatusTP.apply();
    return sendCommand(cmd, res);
}

/////////////////////////////////////////////////////////////////////////////
/// get rotator az position
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorGetAz()
{
    double res = 0;
    if (sendCommand("!dome getaz#", res))
    {
        DomeAbsPosN[0].value = res;
        IDSetNumber(&DomeAbsPosNP, nullptr);
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Set home offset
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorSetHome(double az)
{
    char cmd[DRIVER_LEN] = {0};
    double res = 0;
    snprintf(cmd, DRIVER_LEN, "!domerot sethome %.2f#", az);
    if (sendCommand(cmd, res)) {
        LOGF_INFO("Home is set to: %.1f", az);
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Rotator park
/////////////////////////////////////////////////////////////////////////////
IPState Beaver::Park()
{
    double res;
    if (sendCommand("!dome gopark#", res)) {
        std::string rStatus = "Parking";
        RotatorStatusTP[0].setText(rStatus);
        RotatorStatusTP.apply();
        // check shutter policy
        if (shutterOnLine() && (ShutterParkPolicyS[SHUTTER_CLOSE_ON_PARK].s == ISS_ON)) {
            if(ControlShutter(SHUTTER_CLOSE)) {
                DomeShutterS[SHUTTER_OPEN].s = ISS_OFF;
                DomeShutterS[SHUTTER_CLOSE].s = ISS_ON;
                setShutterState(SHUTTER_MOVING);
            }
            else
                return IPS_ALERT;
        }
        return IPS_BUSY;
    }
    return IPS_ALERT;
}

/////////////////////////////////////////////////////////////////////////////
/// Rotator unpark
/////////////////////////////////////////////////////////////////////////////
IPState Beaver::UnPark()
{
    std::string rStatus = "Dome Parked";
    RotatorStatusTP[0].setText(rStatus);
    RotatorStatusTP.apply();
    // check shutter policy
    if (shutterOnLine() && (ShutterParkPolicyS[SHUTTER_OPEN_ON_UNPARK].s == ISS_ON)) {
        if(!ControlShutter(SHUTTER_CLOSE)) {
            DomeShutterS[SHUTTER_OPEN].s = ISS_ON;
            DomeShutterS[SHUTTER_CLOSE].s = ISS_OFF;
            setShutterState(SHUTTER_MOVING);
        }
        else
            return IPS_ALERT;
    }
    return IPS_OK;
}

/////////////////////////////////////////////////////////////////////////////
/// Rotator set park position
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorSetPark(double az)
{
    double res = 0;
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "!domerot setpark %.2f#", az);
    if (sendCommand(cmd, res)) {
        LOGF_INFO("Park set to: %.2f", az);
        SetAxis1Park(az);
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Rotator set park position to current
/////////////////////////////////////////////////////////////////////////////
bool Beaver::SetCurrentPark() {
    double res = 0;
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "!domerot setpark %.2f#", DomeAbsPosN[0].value);
    if (sendCommand(cmd, res)) {
        SetAxis1Park(DomeAbsPosN[0].value);
        LOGF_INFO("Park set to current: %.2f", DomeAbsPosN[0].value);
    }
        return true;

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Rotator set park position to default (0 az)
/////////////////////////////////////////////////////////////////////////////
bool Beaver::SetDefaultPark() {
    double res = 0;
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "!domerot setpark %d#", 0);
    if (sendCommand(cmd, res)) {
        SetAxis1Park(0.0);
        LOG_INFO("Park set to default: 0.00");
        return true;
    }

    return false;
}


/////////////////////////////////////////////////////////////////////////////
/// tells rotator to goto home position
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorGotoHome()
{
    double res = 0;
    if (sendCommand("!dome gohome#", res)) {
        setDomeState(DOME_MOVING);
        std::string rStatus = "Homing";
        RotatorStatusTP[0].setText(rStatus);
        RotatorStatusTP.apply();
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// tells the rotator to find and accurate measure the home pos and set all params
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorMeasureHome()
{
    double res = 0;
    if (sendCommand("!dome autocalrot 1#", res)) {
        setDomeState(DOME_MOVING);
        std::string rStatus = "Measuring Home";
        RotatorStatusTP[0].setText(rStatus);
        RotatorStatusTP.apply();
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// tell the rotator to find the home position magnet
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorFindHome()
{
    double res = 0;
    if (sendCommand("!dome autocalrot 0#", res)) {
        setDomeState(DOME_MOVING);
        std::string rStatus = "Finding Home";
        RotatorStatusTP[0].setText(rStatus);
        RotatorStatusTP.apply();
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Rotator at home?
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorIsHome()
{
    double status = 0;
    if (sendCommand("!dome athome#", status)) {
        LOG_ERROR("Error checking home");
        return false;
    }
    LOGF_DEBUG("Rotator Home? %s", (status == 1) ? "true" : "false");
    if (status == 1)
        return true;
    else
        return false;
}


/////////////////////////////////////////////////////////////////////////////
/// Rotator parked?
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorIsParked()
{
    double status = 0;
    if (!sendCommand("!dome atpark#", status)) {
        LOG_ERROR("Error checking park");
        return false;
    }
    LOGF_DEBUG("Rotator Parked? %s", (status == 1) ? "true" : "false");
    if (status == 1)
        return true;
    else
        return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Dome Status
/////////////////////////////////////////////////////////////////////////////
bool Beaver::getDomeStatus(uint16_t &domeStatus)
{
    double res = 0;
    if (!sendCommand("!dome status#", res))  {
        LOG_ERROR("Status cmd errored out");
        return false;
    }
    domeStatus = static_cast<uint16_t>(res);
    LOGF_DEBUG("Dome status: %0x", domeStatus);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::shutterOnLine()
{
    double res = 0;
    uint16_t domeStatus;
    bool shutterIsUp = false;
    // retrieving shutter status
    if (!sendCommand("!dome shutterisup#", res))  {
        LOG_ERROR("Shutter status cmd errored out");
        //failsave, return false/not online
        return false;
    }
    // retrieving comm status
    shutterIsUp = static_cast<bool>(res);
    if (!getDomeStatus(domeStatus)) {
        LOG_ERROR("Shutter status cmd errored out");
        //failsave, return false/not online
        return false;
    }
    LOGF_DEBUG("ShutterIsUp %s  Comms error %s", shutterIsUp ? "true" : "false", (domeStatus & DOME_STATUS_SHUTTER_COMM) ? "true" : "false");
    bool status = (shutterIsUp |  !(domeStatus & DOME_STATUS_SHUTTER_COMM));
    LOGF_DEBUG("ShuttOnLine %s", status ? "true" : "false");
    return status;
}

//////////////////////////////////////////////////////////////////////////////
/// abort everything
//////////////////////////////////////////////////////////////////////////////
bool Beaver::Abort()
{
    return abortAll();
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::abortAll()
{
    double res = 0;
    if (sendCommand("!dome abort 1 1 1#", res)) {
        std::string rStatus = "Idle";
        RotatorStatusTP[0].setText(rStatus);
        RotatorStatusTP.apply();
        if (!rotatorGetAz())
            return false;
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// abort shutter
/////////////////////////////////////////////////////////////////////////////
bool Beaver::shutterAbort()
{
    double res = 0;
    return sendCommand("!dome abort 0 0 1#", res);
}

/////////////////////////////////////////////////////////////////////////////
/// Shutter set settings
/////////////////////////////////////////////////////////////////////////////
bool Beaver::shutterSetSettings(double maxSpeed, double minSpeed, double acceleration, double voltage)
{
    if (shutterOnLine()) {
        char cmd[DRIVER_LEN] = {0};
        double res;

        snprintf(cmd, DRIVER_LEN, "!dome setshuttermaxspeed %.2f#", maxSpeed);
        if (!sendCommand(cmd, res)) {
            LOG_ERROR("Problem setting shutter max speed");
            return false;
        }
        snprintf(cmd, DRIVER_LEN, "!dome setshutterminspeed %.2f#", minSpeed);
        if (!sendCommand(cmd, res)) {
            LOG_ERROR("Problem setting shutter min speed");
            return false;
        }
        snprintf(cmd, DRIVER_LEN, "!dome setshutteracceleration %.2f#", acceleration);
        if (!sendCommand(cmd, res)) {
            LOG_ERROR("Problem setting shutter acceleration");
            return false;
        }
        snprintf(cmd, DRIVER_LEN, "!dome setshuttersafevoltage %.2f#", voltage);
        if (!sendCommand(cmd, res)) {
            LOG_ERROR("Problem setting shutter safe voltage");
            return false;
        }
        if(!sendCommand("!seletek savefs#", res)) {
            LOG_ERROR("Problem setting shutter savefs");
            return false;
        }
        LOG_INFO("Shutter parameters have been updated");
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Shutter get settings
/////////////////////////////////////////////////////////////////////////////
bool Beaver::shutterGetSettings()
{
    double res;
    if (shutterOnLine()) {
        if (!sendCommand("!dome getshuttermaxspeed#", res)) {
            LOG_ERROR("Problem getting shutter max speed");
            return false;
        }
        else {
            ShutterSettingsNP[SHUTTER_MAX_SPEED].setValue(res);
            LOGF_DEBUG("Shutter reports max speed of: %.1f", res);
        }
        if (!sendCommand("!dome getshutterminspeed#", res)) {
            LOG_ERROR("Problem getting shutter min speed");
            return false;
        }
        else {
            ShutterSettingsNP[SHUTTER_MIN_SPEED].setValue(res);
            LOGF_DEBUG("Shutter reports min speed of: %.1f", res);
        }
        if (!sendCommand("!dome getshutteracceleration#", res)) {
            LOG_ERROR("Problem getting shutter acceleration");
            return false;
        }
        else {
            ShutterSettingsNP[SHUTTER_ACCELERATION].setValue(res);
            LOGF_DEBUG("Shutter reports acceleration of: %.1f", res);
        }
        if (!sendCommand("!dome getshuttertimeoutopenclose#", res)) {
            LOG_ERROR("Problem getting shutter timeout");
            return false;
        }
        else {
            ShutterSettingsTimeoutNP[0].setValue(res);
            LOGF_DEBUG("Shutter reports safe timeout of: %.1f", res);
        }
        if (!sendCommand("!dome getshuttersafevoltage#", res)) {
            LOG_ERROR("Problem getting shutter safe voltage");
            return false;
        }
        else {
            ShutterSettingsNP[SHUTTER_SAFE_VOLTAGE].setValue(res);
            LOGF_DEBUG("Shutter reports safe voltage of: %.1f", res);
        }
        ShutterSettingsNP.apply();
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Rotator set settings
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorSetSettings(double maxSpeed, double minSpeed, double acceleration, double timeout)
{
    char cmd[DRIVER_LEN] = {0};
    double res;

    snprintf(cmd, DRIVER_LEN, "!domerot setmaxspeed %.2f#", maxSpeed);
    if (!sendCommand(cmd, res)) {
        LOG_ERROR("Problem setting rotator max speed");
        return false;
    }
    snprintf(cmd, DRIVER_LEN, "!domerot setminspeed %.2f#", minSpeed);
    if (!sendCommand(cmd, res)) {
        LOG_ERROR("Problem setting rotator min speed");
        return false;
    }
    snprintf(cmd, DRIVER_LEN, "!domerot setacceleration %.2f#", acceleration);
    if (!sendCommand(cmd, res)) {
        LOG_ERROR("Problem setting rotator acceleration");
        return false;
    }
    snprintf(cmd, DRIVER_LEN, "!domerot setmaxfullrotsecs %.2f#", timeout);
    if (!sendCommand(cmd, res)) {
        LOG_ERROR("Problem setting rotator full rot secs");
        return false;
    }
    if(!sendCommand("!seletek savefs#", res)) {
        LOG_ERROR("dome could not savefs");
        return false;
    }
    LOG_INFO("Rotator parameters have been updated");

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Rotator get settings
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorGetSettings()
{
    double res;
    if (!sendCommand("!domerot getmaxspeed#", res)) {
        LOG_ERROR("Problem getting rotator max speed");
        return false;
    }
    else {
        RotatorSettingsNP[ROTATOR_MAX_SPEED].setValue(res);
        LOGF_DEBUG("Rotator reports max speed of: %.1f", res);
    }
    if (!sendCommand("!domerot getminspeed#", res)) {
        LOG_ERROR("Problem getting rotator min speed");
        return false;
    }
    else {
        RotatorSettingsNP[ROTATOR_MIN_SPEED].setValue(res);
        LOGF_DEBUG("Rotator reports min speed of: %.1f", res);
    }
    if (!sendCommand("!domerot getacceleration#", res)) {
        LOG_ERROR("Problem getting rotator acceleration");
        return false;
    }
    else {
        RotatorSettingsNP[ROTATOR_ACCELERATION].setValue(res);
        LOGF_DEBUG("Rotator reports acceleration of: %.1f", res);
    }
    if (!sendCommand("!domerot getmaxfullrotsecs#", res)) {
        LOG_ERROR("Problem getting rotator full rot secs");
        return false;
    }
    else {
        RotatorSettingsNP[ROTATOR_TIMEOUT].setValue(res);
        LOGF_DEBUG("Rotator reports timeout(s) of: %.1f", res);
    }
    RotatorSettingsNP.apply();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::shutterFindHome()
{
    if (shutterOnLine()) {
        double res = 0;
        return sendCommand("!dome autocalshutter#", res);
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Raw Command
/////////////////////////////////////////////////////////////////////////////
bool Beaver::sendRawCommand(const char * cmd, char * response)
{
    int rc = TTY_OK;
    for (int i = 0; i < 3; i++)
    {
        int nbytes_written = 0, nbytes_read = 0;
        rc = tty_write_string(PortFD, cmd, &nbytes_written);

        if (rc != TTY_OK)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Serial write error: %s.", errstr);
            return false;
        }

        rc = tty_nread_section(PortFD, response, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

        if (rc != TTY_OK)
        {
            // wait and try again
            usleep(100000);
            continue;
        }

        // Remove extra #
        response[nbytes_read - 1] = 0;
        LOGF_DEBUG("Command Response: %s", response);
        return true;
    }

    // timeouts used up, return error
    if (rc != TTY_OK) {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
    }
    return false;
}
/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool Beaver::sendCommand(const char * cmd, double &res)
{
    char response[DRIVER_LEN] = {0};
    sendRawCommand(cmd, response);

    std::regex rgx(R"(.*:((-*\d+(\.\d*)*)))");
    std::smatch match;
    std::string input(response);

    if (std::regex_search(input, match, rgx)) {
        try {
            res = std::stof(match.str(1));
            return true;
        }
        catch (...) {
            LOGF_ERROR("Failed to process response: %s.", response);
            return false;
        }
    }
    LOGF_DEBUG("Command error: %s  response: %d", cmd, response);
    return false;
}
