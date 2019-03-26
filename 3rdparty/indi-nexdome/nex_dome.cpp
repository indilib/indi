/*******************************************************************************
 NexDome
 Copyright(c) 2017 Rozeware Development Ltd. All rights reserved.
 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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
#include "nex_dome.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <termios.h>
#include <memory>

#include <indicom.h>

#include "config.h"

// We declare an auto pointer to nexDome.
static std::unique_ptr<NexDome> nexDome(new NexDome());

#define DOME_SPEED      2.0             /* 2 degrees per second, constant */
#define SHUTTER_TIMER   5.0             /* Shutter closes/open in 5 seconds */

void ISGetProperties(const char *dev)
{
    nexDome->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    nexDome->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    nexDome->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    nexDome->ISNewNumber(dev, name, values, names, num);
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
    nexDome->ISSnoopDevice(root);
}

NexDome::NexDome()
{
    setVersion(INDI_NEXDOME_VERSION_MAJOR, INDI_NEXDOME_VERSION_MINOR);

    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_ABS_MOVE | DOME_CAN_PARK | DOME_HAS_SHUTTER);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool NexDome::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_AZ);

    //  now fix the park data display
    IUFillNumber(&ParkPositionN[AXIS_AZ], "PARK_AZ", "AZ Degrees", "%5.1f", 0.0, 360.0, 0.0, 0);
    //  lets fix the display for absolute position
    IUFillNumber(&DomeAbsPosN[0], "DOME_ABSOLUTE_POSITION", "Degrees", "%5.1f", 0.0, 360.0, 1.0, 0.0);
    //IUFillNumberVector(&ParkPositionNP,ParkPositionN,1,getDeviceName(),

    ///////////////////////////////////////////////////////////////////////////////
    /// Home Command
    ///////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&HomeS[0], "Home", "", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 1, getDeviceName(), "DOME_HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Calibration Command
    ///////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&CalibrateS[0], "Calibrate", "", ISS_OFF);
    IUFillSwitchVector(&CalibrateSP, CalibrateS, 1, getDeviceName(), "DOME_CALIBRATE", "Calibrate", SITE_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Sync Command
    ///////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&SyncPositionN[0], "SYNC_AZ", "AZ Degrees", "%5.1f", 0.0, 360, 0.0, 0);
    IUFillNumberVector(&SyncPositionNP, SyncPositionN, 1, getDeviceName(), "DOME_SYNC", "Sync", SITE_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Home Position
    ///////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&HomePositionN[0], "HOME_POSITON", "degrees", "%5.1f", 0.0, 360.0, 0.0, 0);
    IUFillNumberVector(&HomePositionNP, HomePositionN, 1, getDeviceName(), "HOME_POS", "Home Az", SITE_TAB, IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Battery
    ///////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&BatteryLevelN[0], "BATTERY_ROTATOR", "Rotator", "%5.2f", 0.0, 16.0, 0.0, 0);
    IUFillNumber(&BatteryLevelN[1], "BATTERY_SHUTTER", "Shutter", "%5.2f", 0.0, 16.0, 0.0, 0);
    IUFillNumberVector(&BatteryLevelNP, BatteryLevelN, 2, getDeviceName(), "BATTERY", "Battery Level", SITE_TAB, IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Firmware Info
    ///////////////////////////////////////////////////////////////////////////////
    IUFillText(&FirmwareVersionT[0], "FIRMWARE_VERSION", "Version", "");
    IUFillTextVector(&FirmwareVersionTP, FirmwareVersionT, 1, getDeviceName(), "FIRMWARE", "Firmware", SITE_TAB, IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Dome Reversal
    ///////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&ReversedS[0], "Disable", "", ISS_OFF);
    IUFillSwitch(&ReversedS[1], "Enable", "", ISS_OFF);
    IUFillSwitchVector(&ReversedSP, ReversedS, 2, getDeviceName(), "DOME_REVERSED", "Reversed", SITE_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    return true;
}

bool NexDome::Handshake()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("v\n", res))
    {
        IUSaveText(&FirmwareVersionT[0], &res[1]);
        return true;
    }

    return false;
}

const char * NexDome::getDefaultName()
{
    return "NexDome";
}

bool NexDome::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        readStartupParameters();

        defineSwitch(&HomeSP);
        defineSwitch(&CalibrateSP);
        defineNumber(&SyncPositionNP);
        defineNumber(&HomePositionNP);
        defineNumber(&BatteryLevelNP);
        defineText(&FirmwareVersionTP);
        defineSwitch(&ReversedSP);
    }
    else
    {
        deleteProperty(HomeSP.name);
        deleteProperty(CalibrateSP.name);
        deleteProperty(SyncPositionNP.name);
        deleteProperty(HomePositionNP.name);
        deleteProperty(BatteryLevelNP.name);
        deleteProperty(FirmwareVersionTP.name);
        deleteProperty(ReversedSP.name);
    }

    return true;
}

bool NexDome::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev, getDeviceName()))
    {
        ///////////////////////////////////////////////////////////////////////////////
        /// Home Command
        ///////////////////////////////////////////////////////////////////////////////
        if(!strcmp(name, HomeSP.name))
        {
            if(AtHome)
            {
                HomeSP.s = IPS_OK;
                LOG_INFO("Already at home.");
            }
            else
            {
                if(!m_MotorPower)
                {
                    HomeSP.s = IPS_ALERT;
                    LOG_ERROR("Cannot home without motor power");
                }
                else
                {
                    HomeSP.s = IPS_BUSY;
                    sendCommand("h\n");
                    LOG_INFO("Dome finding home...");
                }
            }

            IDSetSwitch(&HomeSP, nullptr);
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Calibration Command
        ///////////////////////////////////////////////////////////////////////////////
        if(!strcmp(name, CalibrateSP.name))
        {
            if(AtHome)
            {
                CalibrateSP.s = IPS_BUSY;
                sendCommand("c\n");
                m_Calibrating = true;
                time(&CalStartTime);
                m_HomeAz = -1;
                LOG_INFO("Dome is Calibrating...");
            }
            else
            {
                CalibrateSP.s = IPS_ALERT;
                LOG_ERROR("Cannot calibrate unless dome is at home position.");
            }

            IDSetSwitch(&CalibrateSP, nullptr);
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Reversal
        ///////////////////////////////////////////////////////////////////////////////
        if(!strcmp(name, ReversedSP.name))
        {
            if(states[0] == ISS_OFF)
            {
                ReversedSP.s = IPS_OK;
                ReversedS[0].s = ISS_OFF;
                ReversedS[1].s = ISS_ON;
                sendCommand("y 1\n");
                LOG_INFO("Dome is reversed.");
            }
            else
            {
                ReversedSP.s = IPS_IDLE;
                ReversedS[0].s = ISS_ON;
                ReversedS[1].s = ISS_OFF;
                sendCommand("y 0\n");
                LOG_INFO("Dome is not reversed.");
            }

            IDSetSwitch(&ReversedSP, nullptr);
        }
    }
    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

bool NexDome::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev, getDeviceName()) == 0)
    {
        ///////////////////////////////////////////////////////////////////////////////
        /// Synchronize Position
        ///////////////////////////////////////////////////////////////////////////////
        if(!strcmp(name, SyncPositionNP.name))
        {
            char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
            snprintf(cmd, DRIVER_LEN, "s %4.1f\n", values[0]);
            if (sendCommand(cmd, res))
            {
                SyncPositionN[0].value = values[0];
                float b1 = atof(&res[1]);
                SyncPositionNP.s = IPS_OK;
                LOGF_INFO("Dome sync at %3.0f.", b1);
                //  refetch the new home azimuth
                m_HomeAz = -1;
            }
            else
                SyncPositionNP.s =  IPS_ALERT;

            IDSetNumber(&SyncPositionNP, nullptr);
            return true;
        }
    }
    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

void NexDome::TimerHit()
{
    if(isConnected())
        readDomeStatus();

    SetTimer(POLLMS);
}

void NexDome::readDomeStatus()
{
    if(isConnected() == false)
        return;

    if (!readMotionStatus() || !readPosition())
        return;

    if(!m_InMotion)
    {
        // Dome is not in motion, lets check a few more things
        // Check for home position
        // Check voltage on batteries
        if (!readHomeSensor() || !readBatteryLevel())
            return;

        //  if we have started a calibration cycle
        //  it has finished since we are not in motion
        if(m_Calibrating)
        {
            //  we just calibrated, and are now not moving
            m_Calibrating = false;
            //  lets get our new value for how many steps to turn the dome 360 degrees
            if (!readStepsPerRevolution())
                return;
        }

        //  lets see how far off we were on last home detect
        if (!readHomeError())
            return;

        // If we need to fetch home position
        if (m_HomeAz == -1)
            readHomePosition();

        //  get shutter status
        if (!readShutterStatus())
            return;

        //  get shutter position if shutter is talking to us
        if(m_ShutterState != 0)
        {
            if (!readShutterPosition())
                return;
        }

        //  if the jog switch is set, unset it
        if((DomeMotionS[0].s == ISS_ON) || (DomeMotionS[1].s == ISS_ON))
        {
            DomeMotionS[0].s = ISS_OFF;
            DomeMotionS[1].s = ISS_OFF;
            IDSetSwitch(&DomeMotionSP, nullptr);
        }
    }

    //  Not all mounts update ra/dec constantly if tracking co-ordinates
    //  This is to ensure our alt/az gets updated even if ra/dec isn't being updated
    //  Once every 10 seconds is more than sufficient
    //  with this added, NexDome will now correctly track telescope simulator
    //  which does not emit new ra/dec co-ords if they are not changing
    if(m_TimeSinceUpdate++ > 9)
    {
        m_TimeSinceUpdate = 0;
        UpdateMountCoords();
    }
}

bool NexDome::readMotionStatus()
{
    char res[DRIVER_LEN] = {0};

    if (!sendCommand("m\n", res))
        return false;

    int m = atoi(&res[1]);

    if(m == 0)
    {
        if(getDomeState() == DOME_PARKING)
        {
            SetParked(true);
        }
        m_InMotion = false;
        if(m_MotorPower)
        {
            DomeAbsPosNP.s = IPS_OK;
            DomeMotionSP.s = IPS_OK;
        }
        else
        {
            DomeAbsPosNP.s = IPS_ALERT;
            DomeMotionSP.s = IPS_ALERT;
        }
        IDSetSwitch(&DomeMotionSP, nullptr);
        if(m_Calibrating)
        {
            float delta;
            time_t now;
            time(&now);
            delta = difftime(now, CalStartTime);
            CalibrateSP.s = IPS_OK;
            IDSetSwitch(&CalibrateSP, "Calibration complete %3.0f seconds.", delta);
        }
    }
    else
    {
        m_InMotion = true;
        DomeAbsPosNP.s = IPS_BUSY;
        if(HomeSP.s == IPS_OK)
        {
            HomeSP.s = IPS_IDLE;
            IDSetSwitch(&HomeSP, nullptr);
        }
    }

    return true;
}

bool NexDome::readPosition()
{
    char res[DRIVER_LEN] = {0};

    if (!sendCommand("q\n", res))
        return false;

    DomeAbsPosN[0].value = atof(&res[1]);
    IDSetNumber(&DomeAbsPosNP, nullptr);

    return true;
}

bool NexDome::readHomeSensor()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("z\n", res))
        return false;

    int t = atoi(&res[1]);

    if(t == 1)
    {
        AtHome = true;
        if(HomeSP.s != IPS_OK)
        {
            HomeSP.s = IPS_OK;
            IDSetSwitch(&HomeSP, "Dome is at home.");
        }
    }
    else if(t == 0)
    {
        AtHome = false;
        if(HomeSP.s != IPS_IDLE)
        {
            if(m_MotorPower) HomeSP.s = IPS_IDLE;
            else HomeSP.s = IPS_ALERT;
            IDSetSwitch(&HomeSP, nullptr);
        }
    }
    else if(t == -1)
    {
        if(HomeSP.s != IPS_BUSY)
        {
            if(m_MotorPower)
                HomeSP.s = IPS_BUSY;
            else
                HomeSP.s = IPS_ALERT;

            IDSetSwitch(&HomeSP, "Dome has not been homed.");
        }
    }

    return true;
}

bool NexDome::readBatteryLevel()
{
    char res[DRIVER_LEN] = {0};

    if (!sendCommand("k\n", res))
        return false;

    int b1, b2;
    float b3, b4;
    char a;
    sscanf(res, "%c %d %d", &a, &b1, &b2);
    b3 = b1 / 100.0;
    b4 = b2 / 100.0;
    if((m_BatteryMain != b3) || (m_BatteryShutter != b4))
    {
        m_BatteryMain = b3;
        m_BatteryShutter = b4;
        BatteryLevelN[0].value = m_BatteryMain;
        BatteryLevelN[1].value = m_BatteryShutter;
        if(m_BatteryMain > 7)
        {
            BatteryLevelNP.s = IPS_OK;
            if(!m_MotorPower)
            {
                IDSetNumber(&BatteryLevelNP, "Motor is powered.");
            }
            m_MotorPower = true;
        }
        else
        {
            //  and it's off now
            if(m_MotorPower)
                IDSetNumber(&BatteryLevelNP, "Motor is NOT powered.");
            m_MotorPower = false;
            DomeAbsPosNP.s = IPS_ALERT;
            IDSetNumber(&DomeAbsPosNP, nullptr);
            HomeSP.s = IPS_ALERT;
            IDSetSwitch(&HomeSP, nullptr);
            BatteryLevelNP.s = IPS_ALERT;
        }
        IDSetNumber(&BatteryLevelNP, nullptr);
    }

    return true;
}

bool NexDome::readStepsPerRevolution()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("t\n", res))
        return false;

    m_StepsPerDomeTurn = atoi(&res[1]);
    LOGF_INFO("Dome has %d steps per revolution.", m_StepsPerDomeTurn);
    IDSetSwitch(&HomeSP, nullptr);

    return true;
}

bool NexDome::readHomeError()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("o\n", res))
        return false;

    float b1 = atof(&res[1]);
    if(b1 != m_HomeError)
    {
        LOGF_DEBUG("Home error %4.2f.", b1);
        m_HomeError = b1;
    }

    return true;
}

bool NexDome::readHomePosition()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("i\n", res))
        return false;

    float b1 = atof(&res[1]);
    if(b1 != m_HomeAz)
    {
        HomePositionN[0].value = b1;
        LOGF_INFO("Home position is %4.1f degrees.", b1);
        IDSetNumber(&HomePositionNP, nullptr);
        m_HomeAz = b1;
    }

    return true;
}

bool NexDome::readShutterStatus()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("u\n", res))
        return false;

    int b = atoi(&res[1]);

    if(b != m_ShutterState)
    {
        if(b == 0)
        {
            DomeShutterSP.s = IPS_ALERT;
            DomeShutterS[0].s = ISS_OFF;
            DomeShutterS[1].s = ISS_OFF;
            LOG_INFO("Shutter is not connected.");
            IDSetSwitch(&DomeShutterSP, nullptr);
        }

        if(b == 1)
        {
            DomeShutterSP.s = IPS_OK;
            DomeShutterS[0].s = ISS_OFF;
            DomeShutterS[1].s = ISS_OFF;
            LOG_INFO("Shutter is open.");
            IDSetSwitch(&DomeShutterSP, nullptr);
        }

        if(b == 2)
        {
            DomeShutterSP.s = IPS_BUSY;
            DomeShutterS[0].s = ISS_OFF;
            DomeShutterS[1].s = ISS_OFF;
            LOG_INFO("Shutter is opening...");
            IDSetSwitch(&DomeShutterSP, nullptr);
        }

        if(b == 3)
        {
            DomeShutterSP.s = IPS_IDLE;
            DomeShutterS[0].s = ISS_OFF;
            DomeShutterS[1].s = ISS_OFF;
            LOG_INFO("Shutter is closed.");
            IDSetSwitch(&DomeShutterSP, nullptr);
        }

        if(b == 4)
        {
            DomeShutterSP.s = IPS_BUSY;
            DomeShutterS[0].s = ISS_OFF;
            DomeShutterS[1].s = ISS_OFF;
            LOG_INFO("Shutter is closing...");
            IDSetSwitch(&DomeShutterSP, nullptr);
        }

        if(b == 5)
        {
            DomeShutterSP.s = IPS_ALERT;
            DomeShutterS[0].s = ISS_OFF;
            DomeShutterS[1].s = ISS_OFF;
            LOG_INFO("Shutter state undetermined.");
            IDSetSwitch(&DomeShutterSP, nullptr);
        }

        m_ShutterState = b;
    }

    return true;
}

bool NexDome::readShutterPosition()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("b\n", res))
        return false;

    float b1 = atof(&res[1]);

    if((b1 != m_ShutterPosition))
    {
        m_ShutterPosition = b1;
        if(b1 == 90.0)
        {
            shutterState = SHUTTER_OPENED;
            DomeShutterSP.s = IPS_OK;
            DomeShutterS[0].s = ISS_OFF;
            DomeShutterS[1].s = ISS_OFF;
            LOG_INFO("Shutter is open.");
            IDSetSwitch(&DomeShutterSP, nullptr);
        }
        else
        {
            if(b1 == -22.5)
            {
                shutterState = SHUTTER_CLOSED;
                DomeShutterSP.s = IPS_IDLE;
                DomeShutterS[0].s = ISS_OFF;
                DomeShutterS[1].s = ISS_OFF;
                LOG_INFO("Shutter is closed.");
                IDSetSwitch(&DomeShutterSP, nullptr);
            }
            else
            {
                shutterState = SHUTTER_UNKNOWN;
                DomeShutterSP.s = IPS_ALERT;
                DomeShutterS[0].s = ISS_OFF;
                DomeShutterS[1].s = ISS_OFF;
                LOGF_INFO("Shutter Position %4.1f", b1);
                IDSetSwitch(&DomeShutterSP, nullptr);
            }
        }
    }

    return true;
}

bool NexDome::readReversedStatus()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("y\n", res))
        return false;

    m_DomeReversed = atoi(&res[1]);
    if(m_DomeReversed == 1)
    {
        ReversedS[0].s = ISS_OFF;
        ReversedS[1].s = ISS_ON;
        ReversedSP.s = IPS_OK;
        IDSetSwitch(&ReversedSP, nullptr);
    }
    else
    {
        ReversedS[0].s = ISS_ON;
        ReversedS[1].s = ISS_OFF;
        ReversedSP.s = IPS_IDLE;
        IDSetSwitch(&ReversedSP, nullptr);
    }

    return true;
}

bool NexDome::readStartupParameters()
{
    bool rc1 = readStepsPerRevolution();
    bool rc2 = readHomePosition();
    bool rc3 = readReversedStatus();

    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(180);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(180);
        SetAxis1ParkDefault(180);
    }

    return (rc1 && rc2 && rc3);
}

IPState NexDome::MoveAbs(double az)
{
    char cmd[DRIVER_LEN] = {0};

    if(!m_MotorPower)
    {
        LOG_ERROR("Cannot move dome without motor power.");
        IDSetNumber(&BatteryLevelNP, nullptr);
        return IPS_ALERT;
    }

    //  Just write the string
    //  Our main reader loop will check any returns
    snprintf(cmd, DRIVER_LEN, "g %3.1f\n", az);

    DomeAbsPosNP.s = sendCommand(cmd) ? IPS_BUSY : IPS_ALERT;
    IDSetNumber(&DomeAbsPosNP, nullptr);
    return DomeAbsPosNP.s;
}

IPState NexDome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    double target;

    if (operation == MOTION_START)
    {
        target = DomeAbsPosN[0].value;
        if(dir == DOME_CW)
        {
            target += 5;
        }
        else
        {
            target -= 5;
        }

        if(target < 0)
            target += 360;
        if(target >= 360)
            target -= 360;
    }
    else
    {
        target = DomeAbsPosN[0].value;
    }

    MoveAbs(target);

    return ((operation == MOTION_START) ? IPS_BUSY : IPS_OK);

}

IPState NexDome::Park()
{
    if(!m_MotorPower)
    {
        LOG_ERROR("Cannot park with motor unpowered.");
        IDSetNumber(&BatteryLevelNP, nullptr);
        return IPS_ALERT;
    }
    MoveAbs(GetAxis1Park());
    return IPS_BUSY;
}

IPState NexDome::UnPark()
{
    if(!m_MotorPower)
    {
        LOG_ERROR("Cannot unpark with motor unpowered.");
        IDSetNumber(&BatteryLevelNP, nullptr);
        return IPS_ALERT;
    }
    return IPS_OK;
}

IPState NexDome::ControlShutter(ShutterOperation operation)
{
    if(m_ShutterState == 0)
    {
        //  we are not talking to the shutter
        //  so return an error
        return IPS_ALERT;
    }

    if(operation == SHUTTER_OPEN)
    {
        if(shutterState == SHUTTER_OPENED)
            return IPS_OK;
        if (sendCommand("d\n"))
            shutterState = SHUTTER_MOVING;
    }
    if(operation == SHUTTER_CLOSE)
    {
        if(shutterState == SHUTTER_CLOSED)
            return IPS_OK;
        if (sendCommand("e\n"))
            shutterState = SHUTTER_MOVING;
    }

    return IPS_BUSY;
}

bool NexDome::Abort()
{
    sendCommand("a\n");
    /*
        // If we abort while in the middle of opening/closing shutter, alert.
        if (DomeShutterSP.s == IPS_BUSY)
        {
            DomeShutterSP.s = IPS_ALERT;
            IDSetSwitch(&DomeShutterSP, "Shutter operation aborted. Status: unknown.");
            return false;
        }
    */
    return true;
}

bool NexDome::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosN[0].value);
    return true;
}

bool NexDome::SetDefaultPark()
{
    // default park position is pointed south
    SetAxis1Park(180);
    return true;
}

bool NexDome::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
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
        rc = tty_write_string(PortFD, cmd, &nbytes_written);
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
    {
        while (true)
        {
            rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);
            if (rc != TTY_OK)
                break;

            // Expected command found, break
            if (toupper(cmd[0]) == res[0])
            {
                break;
            }
        }
        res[nbytes_read - 1] = 0;
    }

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
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

void NexDome::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

bool NexDome::saveConfigItems(FILE * fp)
{
    INDI::Dome::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &ReversedSP);

    return true;
}
