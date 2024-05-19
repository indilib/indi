/*******************************************************************************
 Copyright(c) 2020 Jasem Mutlaq. All rights reserved.

 Astrometric Solutions DomePro2 INDI Driver

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "domepro2.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <inttypes.h>
#include <memory>
#include <regex>
#include <termios.h>

static std::unique_ptr<DomePro2> domepro2(new DomePro2());

DomePro2::DomePro2()
{
    m_ShutterState     = SHUTTER_UNKNOWN;

    SetDomeCapability(DOME_CAN_ABORT |
                      DOME_CAN_ABS_MOVE |
                      DOME_CAN_REL_MOVE |
                      DOME_CAN_PARK |
                      DOME_CAN_SYNC |
                      DOME_HAS_SHUTTER);
}

bool DomePro2::initProperties()
{
    INDI::Dome::initProperties();
    // Firmware & Hardware versions
    VersionTP[VERSION_FIRMWARE].fill("VERSION_FIRMWARE", "Firmware", "NA");
    VersionTP[VERSION_HARDWARE].fill("VERSION_HARDWARE", "Hardware", "NA");
    VersionTP.fill(getDeviceName(), "VERSION", "Version", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Dome & Shutter statuses
    StatusTP[STATUS_DOME].fill("STATUS_DOME", "Dome", "NA");
    StatusTP[STATUS_SHUTTER].fill("STATUS_SHUTTER", "Shutter", "NA");
    StatusTP.fill(getDeviceName(), "STATUS", "Status", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Settings
    SettingsNP[SETTINGS_AZ_CPR].fill("SETTINGS_AZ_CPR", "Az CPR (steps)", "%.f", 0x20, 0x40000000, 0, 0);
    SettingsNP[SETTINGS_AZ_COAST].fill("SETTINGS_AZ_COAST", "Az Coast (deg)", "%.2f", 0, 15, 0, 0);
    SettingsNP[SETTINGS_AZ_HOME].fill("SETTINGS_AZ_HOME", "Az Home (deg)", "%.2f", 0, 360, 0, 0);
    SettingsNP[SETTINGS_AZ_PARK].fill("SETTINGS_AZ_PARK", "Az Park (deg)", "%.2f", 0, 360, 0, 0);
    SettingsNP[SETTINGS_AZ_STALL_COUNT].fill("SETTINGS_AZ_STALL_COUNT", "Az Stall Count (steps)", "%.f", 0, 0x40000000,
                 0, 0);
    SettingsNP.fill(getDeviceName(), "SETTINGS", "Settings", SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    // Home
    HomeSP[HOME_DISCOVER].fill("HOME_DISCOVER", "Discover", ISS_OFF);
    HomeSP[HOME_GOTO].fill("HOME_GOTO", "Goto", ISS_OFF);
    HomeSP.fill(getDeviceName(), "HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    SetParkDataType(PARK_AZ);
    addAuxControls();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::setupInitialParameters()
{
    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(0);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(0);
        SetAxis1ParkDefault(0);
    }

    if (getFirmwareVersion() && getHardwareConfig())
        VersionTP.setState(IPS_OK);

    if (getDomeStatus() && getShutterStatus())
        StatusTP.setState(IPS_OK);

    if (getDomeAzCPR() && getDomeAzCoast() && getDomeHomeAz() && getDomeParkAz() && getDomeAzStallCount())
        SettingsNP.setState(IPS_OK);

    if (getDomeAzPos())
        DomeAbsPosNP.apply();

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::Handshake()
{
    return getFirmwareVersion();
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
const char *DomePro2::getDefaultName()
{
    return "DomePro2";
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        setupInitialParameters();

        defineProperty(VersionTP);
        defineProperty(StatusTP);
        defineProperty(SettingsNP);
        defineProperty(HomeSP);
    }
    else
    {
        deleteProperty(VersionTP);
        deleteProperty(StatusTP);
        deleteProperty(SettingsNP);
        deleteProperty(HomeSP);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Home
        if (HomeSP.isNameMatch(name))
        {
            HomeSP.reset();
            for (int i = 0; i < HomeSP.count(); i++)
            {
                if (states[i] != ISS_ON)
                    continue;

                if (HomeSP[HOME_GOTO].isNameMatch(names[i]))
                {
                    if (!gotoHomeDomeAz())
                    {
                        HomeSP.setState(IPS_ALERT);
                        LOG_ERROR("Failed to go to Home Dome Az.");
                    }
                    else
                    {
                        HomeSP[HOME_GOTO].setState(ISS_ON);
                        HomeSP.setState(IPS_BUSY);
                    }
                }
                else if (HomeSP[HOME_DISCOVER].isNameMatch(names[i]))
                {
                    if (!discoverHomeDomeAz())
                    {
                        HomeSP.setState(IPS_ALERT);
                        LOG_ERROR("Failed to discover Home Dome Az.");
                    }
                    else
                    {
                        HomeSP[HOME_DISCOVER].setState(ISS_ON);
                        HomeSP.setState(IPS_BUSY);
                    }
                }
            }

            HomeSP.apply();
            return true;
        }

    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////

bool DomePro2::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        ////////////////////////////////////////////////////////////
        // Settings
        ////////////////////////////////////////////////////////////
        if (SettingsNP.isNameMatch(name))
        {
            bool allSet = true;
            for (int i = 0; i < SettingsNP.count(); i++)
            {
                if (SettingsNP[SETTINGS_AZ_CPR].isNameMatch(names[i]))
                {
                    if (setDomeAzCPR(static_cast<uint32_t>(values[i])))
                        SettingsNP[SETTINGS_AZ_CPR].setValue(values[i]);
                    else
                    {
                        allSet = false;
                        LOG_ERROR("Failed to set Dome AZ CPR.");
                    }
                }
                else if (SettingsNP[SETTINGS_AZ_COAST].isNameMatch(names[i]))
                {
                    if (setDomeAzCoast(static_cast<uint32_t>(values[i])))
                        SettingsNP[SETTINGS_AZ_COAST].setValue(values[i]);
                    else
                    {
                        allSet = false;
                        LOG_ERROR("Failed to set Dome AZ Coast.");
                    }
                }
                else if (SettingsNP[SETTINGS_AZ_HOME].isNameMatch(names[i]))
                {
                    if (setDomeHomeAz(static_cast<uint32_t>(values[i])))
                        SettingsNP[SETTINGS_AZ_HOME].setValue(values[i]);
                    else
                    {
                        allSet = false;
                        LOG_ERROR("Failed to set Dome AZ Home.");
                    }
                }
                else if (SettingsNP[SETTINGS_AZ_PARK].isNameMatch(names[i]))
                {
                    if (setDomeParkAz(static_cast<uint32_t>(values[i])))
                        SettingsNP[SETTINGS_AZ_PARK].setValue(values[i]);
                    else
                    {
                        allSet = false;
                        LOG_ERROR("Failed to set Dome AZ Park.");
                    }
                }
                else if (SettingsNP[SETTINGS_AZ_STALL_COUNT].isNameMatch(names[i]))
                {
                    if (setDomeAzStallCount(static_cast<uint32_t>(values[i])))
                        SettingsNP[SETTINGS_AZ_STALL_COUNT].setValue(values[i]);
                    else
                    {
                        allSet = false;
                        LOG_ERROR("Failed to set Dome AZ Stall Count.");
                    }
                }

            }

            SettingsNP.setState(allSet ? IPS_OK : IPS_ALERT);
            SettingsNP.apply();
            return true;
        }

    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}
////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
void DomePro2::TimerHit()
{
    if (!isConnected())
        return;

    double currentAz = DomeAbsPosNP[0].getValue();
    if(getDomeAzPos() && std::abs(currentAz - DomeAbsPosNP[0].getValue()) > DOME_AZ_THRESHOLD)
        DomeAbsPosNP.apply();

    std::string domeStatus = StatusTP[STATUS_DOME].getText();
    std::string shutterStatus = StatusTP[STATUS_SHUTTER].getText();
    if(getDomeStatus() && getShutterStatus() && (strcmp(domeStatus.c_str(), StatusTP[STATUS_DOME].getText()) != 0 ||
                                                  strcmp(shutterStatus.c_str(), StatusTP[STATUS_DOME].getText()) != 0))
    {
        if (getDomeState() == DOME_MOVING || getDomeState() == DOME_PARKING)
        {

            if (StatusTP[STATUS_DOME].isNameMatch("Idle"))
            {
                if (getDomeState() == DOME_PARKING)
                    SetParked(true);

                setDomeState(DOME_IDLE);

                if (HomeSP.getState() == IPS_BUSY)
                {
                    HomeSP.reset();
                    HomeSP.setState(IPS_IDLE);
                    HomeSP.apply();
                }
            }
        }

        if (getShutterState() == SHUTTER_MOVING)
        {
            int statusVal = processShutterStatus();
            if (statusVal == 0x00)
                setShutterState(SHUTTER_OPENED);
            else if (statusVal == 0x01)
                setShutterState(SHUTTER_CLOSED);
            else if ((statusVal >= 0x04 && statusVal <= 12) || statusVal >= 15)
                setShutterState(SHUTTER_ERROR);
            else if (statusVal == 0x13)
                setShutterState(SHUTTER_UNKNOWN);
        }

        StatusTP.apply();
    }

    SetTimer(getCurrentPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomePro2::MoveAbs(double az)
{
    INDI_UNUSED(az);
    uint32_t steps = static_cast<uint32_t>(az * (SettingsNP[SETTINGS_AZ_CPR].getValue() / 360));
    if (gotoDomeAz(steps))
        return IPS_BUSY;
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomePro2::MoveRel(double azDiff)
{
    targetAz = DomeAbsPosNP[0].getValue() + azDiff;

    if (targetAz < DomeAbsPosNP[0].getMin())
        targetAz += DomeAbsPosNP[0].getMax();
    if (targetAz > DomeAbsPosNP[0].getMax())
        targetAz -= DomeAbsPosNP[0].getMax();

    // It will take a few cycles to reach final position
    return MoveAbs(targetAz);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool DomePro2::Sync(double az)
{
    //INDI_UNUSED(az);
    if(calibrateDomeAz(az))
        return true;
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState DomePro2::Move(DomeDirection dir, DomeMotionCommand operation)
{
    //    INDI_UNUSED(dir);
    //    INDI_UNUSED(operation);
    if (operation == MOTION_STOP)
    {
        if(killDomeAzMovement())
            return IPS_OK;
    }
    else if (dir == DOME_CCW)
    {
        if(setDomeLeftOn())
            return IPS_BUSY;
    }
    else if (dir == DOME_CW)
    {
        if(setDomeRightOn())
            return IPS_BUSY;
    }

    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomePro2::Park()
{
    //targetAz = GetAxis1Park();
    return (gotoDomePark() ? IPS_BUSY : IPS_ALERT);
    //return MoveAbs(targetAz);
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomePro2::UnPark()
{
    return IPS_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomePro2::ControlShutter(ShutterOperation operation)
{
    if (operation == SHUTTER_OPEN)
    {
        if (openDomeShutters())
            return IPS_BUSY;
    }
    else if (operation == SHUTTER_CLOSE)
    {
        if (closeDomeShutters())
            return IPS_BUSY;
    }
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::Abort()
{
    if (!killDomeAzMovement())
        return false;

    if (getShutterState() == SHUTTER_MOVING && killDomeShutterMovement())
    {
        setShutterState(SHUTTER_UNKNOWN);
    }

    if (ParkSP.getState() == IPS_BUSY)
    {
        SetParked(false);
    }

    if (HomeSP.getState() == IPS_BUSY)
    {
        HomeSP.reset();
        HomeSP.setState(IPS_IDLE);
        HomeSP.apply();
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosNP[0].getValue());
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::SetDefaultPark()
{
    // By default set position to 90
    SetAxis1Park(90);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t DomePro2::processShutterStatus()
{
    if (StatusTP[STATUS_SHUTTER].isNameMatch("Opened"))
        return 0;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("Closed"))
        return 1;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("Opening"))
        return 2;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("Closing"))
        return 3;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("ShutterError"))
        return 4;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter module is not communicating to the azimuth module"))
        return 5;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 1 opposite direction timeout error on open occurred"))
        return 6;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 1 opposite direction timeout error on close occurred"))
        return 7;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 2 opposite direction timeout error on open occurred"))
        return 8;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 2 opposite direction timeout error on close occurred"))
        return 9;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 1 completion timeout error on open occurred"))
        return 10;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 1 completion timeout error on close occurred"))
        return 11;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 2 completion timeout error on open occurred"))
        return 12;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 2 completion timeout error on close occurred"))
        return 13;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 1 limit fault on open occurred"))
        return 14;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 1 limit fault on close occurred"))
        return 15;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 2 limit fault on open occurred"))
        return 16;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 2 limit fault on close occurred"))
        return 17;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("Shutter disabled (Shutter Enable input is not asserted)"))
        return 18;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("Intermediate"))
        return 19;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("GoTo"))
        return 20;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 1 OCP trip on open occurred"))
        return 21;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 1 OCP trip on close occurred"))
        return 22;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 2 OCP trip on open occurred"))
        return 23;
    else if (StatusTP[STATUS_SHUTTER].isNameMatch("shutter 2 OCP trip on close occurred"))
        return 24;
    return 25;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::getFirmwareVersion()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("DGfv", res) == false)
        return false;

    int version = 0;
    if (sscanf(res, "%X", &version) == 1)
    {
        char versionString[DRIVER_LEN] = {0};
        snprintf(versionString, DRIVER_LEN, "%d", version);
        VersionTP[VERSION_FIRMWARE].setText(versionString);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::getHardwareConfig()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("DGhc", res) == false)
        return false;

    uint32_t config = 0;
    if (sscanf(res, "%X", &config) == 1)
    {
        char configString[DRIVER_LEN] = {0};
        uint8_t index = static_cast<uint8_t>(config);
        if (DomeHardware.count(index))
        {
            snprintf(configString, DRIVER_LEN, "%s", DomeHardware.at(index).c_str());
            VersionTP[VERSION_HARDWARE].setText(configString);

        }
        else
        {
            LOGF_WARN("Unknown model detected %d", config);
        }

        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::getDomeStatus()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("DGam", res) == false)
        return false;

    if (DomeStatus.count(res))
    {
        StatusTP[STATUS_DOME].setText(DomeStatus.at(res).c_str());
        return true;
    }
    else
    {
        LOGF_WARN("Unknown dome status detected %d", res);
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::getShutterStatus()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("DGsx", res) == false)
        return false;

    uint32_t status = 0;
    if (sscanf(res, "%X", &status) == 1)
    {
        char statusString[DRIVER_LEN] = {0};
        uint8_t index = static_cast<uint8_t>(status);
        if (ShutterStatus.count(index))
        {
            snprintf(statusString, DRIVER_LEN, "%s", ShutterStatus.at(index).c_str());
            StatusTP[STATUS_SHUTTER].setText(statusString);

        }
        else
        {
            LOGF_WARN("Unknown shutter status detected %d", status);
        }

        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::getDomeAzCPR()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("DGcp", res) == false)
        return false;

    uint32_t cpr = 0;
    if (sscanf(res, "%X", &cpr) == 1)
    {
        SettingsNP[SETTINGS_AZ_CPR].setValue(cpr);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::setDomeAzCPR(uint32_t cpr)
{
    char cmd[DRIVER_LEN] = {0};
    if (cpr < 0x20 || cpr > 0x40000000)
    {
        LOG_ERROR("CPR value out of bounds (32 to 1,073,741,824)");
        return false;
    }
    else if (cpr % 2 != 0)
    {
        LOG_ERROR("CPR value must be an even number");
        return false;
    }
    snprintf(cmd, DRIVER_LEN, "DScp0x%08X", cpr);
    if (sendCommand(cmd) == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::getDomeAzCoast()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("DGco", res) == false)
        return false;

    uint32_t coast = 0;
    if (sscanf(res, "%X", &coast) == 1)
    {
        SettingsNP[SETTINGS_AZ_COAST].setValue(coast * (360 / SettingsNP[SETTINGS_AZ_CPR].getValue()));
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::setDomeAzCoast(uint32_t az)
{
    char cmd[DRIVER_LEN] = {0};
    uint32_t steps = static_cast<uint32_t>(az * (SettingsNP[SETTINGS_AZ_CPR].getValue() / 360));
    //    if (coast > 0x4000)
    //    {
    //        LOG_ERROR("Coast value out of bounds (0 to 16,384)");
    //        return false;
    //    }
    snprintf(cmd, DRIVER_LEN, "DSco0x%08X", steps);
    if (sendCommand(cmd) == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::getDomeHomeAz()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("DGha", res) == false)
        return false;

    uint32_t home = 0;
    if (sscanf(res, "%X", &home) == 1)
    {
        SettingsNP[SETTINGS_AZ_HOME].setValue(home * (360 / SettingsNP[SETTINGS_AZ_CPR].getValue()));
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::setDomeHomeAz(uint32_t az)
{
    char cmd[DRIVER_LEN] = {0};
    uint32_t steps = static_cast<uint32_t>(az * (SettingsNP[SETTINGS_AZ_CPR].getValue() / 360));
    //    if (home >= SettingsN[SETTINGS_AZ_CPR].value)
    //    {
    //        char err[DRIVER_LEN] = {0};
    //        snprintf(err, DRIVER_LEN, "Home value out of bounds (0 to %.f, value has to be less than CPR)",
    //                 SettingsN[SETTINGS_AZ_CPR].value - 1);
    //        LOG_ERROR(err);
    //        return false;
    //    }
    snprintf(cmd, DRIVER_LEN, "DSha0x%08X", steps);
    if (sendCommand(cmd) == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::getDomeParkAz()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("DGpa", res) == false)
        return false;

    uint32_t park = 0;
    if (sscanf(res, "%X", &park) == 1)
    {
        SettingsNP[SETTINGS_AZ_PARK].setValue(park * (360 / SettingsNP[SETTINGS_AZ_CPR].getValue()));
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::setDomeParkAz(uint32_t az)
{
    char cmd[DRIVER_LEN] = {0};
    uint32_t steps = static_cast<uint32_t>(az * (SettingsNP[SETTINGS_AZ_CPR].getValue() / 360));
    //    if (park >= SettingsN[SETTINGS_AZ_CPR].value)
    //    {
    //        char err[DRIVER_LEN] = {0};
    //        snprintf(err, DRIVER_LEN, "Park value out of bounds (0 to %.f, value has to be less than CPR)",
    //                 SettingsN[SETTINGS_AZ_CPR].value - 1);
    //        LOG_ERROR(err);
    //        return false;
    //    }
    snprintf(cmd, DRIVER_LEN, "DSpa0x%08X", steps);
    if (sendCommand(cmd) == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::calibrateDomeAz(double az)
{
    char cmd[DRIVER_LEN] = {0};
    uint32_t steps = static_cast<uint32_t>(az * (SettingsNP[SETTINGS_AZ_CPR].getValue() / 360));
    //    if (steps >= SettingsN[SETTINGS_AZ_CPR].value)
    //    {
    //        char err[DRIVER_LEN] = {0};
    //        snprintf(err, DRIVER_LEN, "Degree value out of bounds");
    //        LOG_ERROR(err);
    //        return false;
    //    }
    snprintf(cmd, DRIVER_LEN, "DSca0x%08X", steps);
    if (sendCommand(cmd) == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::getDomeAzStallCount()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("DGas", res) == false)
        return false;

    uint32_t count = 0;
    if (sscanf(res, "%X", &count) == 1)
    {
        SettingsNP[SETTINGS_AZ_STALL_COUNT].setValue(count);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::setDomeAzStallCount(uint32_t count)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "DSha0x%08X", count);
    if (sendCommand(cmd) == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::getDomeAzPos()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("DGap", res) == false)
        return false;

    uint32_t pos = 0;
    if (sscanf(res, "%X", &pos) == 1)
    {
        DomeAbsPosNP[0].setValue(pos * (360 / SettingsNP[SETTINGS_AZ_CPR].getValue()));
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::gotoDomeAz(uint32_t az)
{
    char cmd[DRIVER_LEN] = {0};
    if (az >= SettingsNP[SETTINGS_AZ_CPR].getValue())
        return false;
    snprintf(cmd, DRIVER_LEN, "DSgo0x%08X", az);
    if (sendCommand(cmd) == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::gotoDomePark()
{
    if (sendCommand("DSgp") == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::killDomeAzMovement()
{
    if (sendCommand("DXxa") == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::killDomeShutterMovement()
{
    if (sendCommand("DXxs") == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::openDomeShutters()
{
    if (sendCommand("DSso") == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::closeDomeShutters()
{
    if (sendCommand("DSsc") == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::gotoHomeDomeAz()
{
    if (sendCommand("DSah") == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::discoverHomeDomeAz()
{
    if (sendCommand("DSdh") == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::setDomeLeftOn()
{
    if (sendCommand("DSol") == false)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::setDomeRightOn()
{
    if (sendCommand("DSor") == false)
        return false;
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool DomePro2::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);

        char formatted_command[DRIVER_LEN] = {0};
        snprintf(formatted_command, DRIVER_LEN, "!%s;", cmd);
        rc = tty_write_string(PortFD, formatted_command, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[DRIVER_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        // Remove extra \r
        res[nbytes_read - 1] = 0;
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void DomePro2::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> DomePro2::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}
