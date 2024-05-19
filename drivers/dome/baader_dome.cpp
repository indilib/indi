/*******************************************************************************
 Baader Planetarium Dome INDI Driver

 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 Baader Dome INDI Driver

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

#include "baader_dome.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>

// We declare an auto pointer to BaaderDome.
std::unique_ptr<BaaderDome> baaderDome(new BaaderDome());

#define DOME_CMD          9    /* Dome command in bytes */
#define DOME_BUF          16   /* Dome command buffer */
#define DOME_TIMEOUT      3    /* 3 seconds comm timeout */

#define SIM_SHUTTER_TIMER 5.0 /* Simulated Shutter closes/open in 5 seconds */
#define SIM_FLAP_TIMER    5.0 /* Simulated Flap closes/open in 3 seconds */
#define SIM_DOME_HI_SPEED 5.0 /* Simulated dome speed 5.0 degrees per second, constant */
#define SIM_DOME_LO_SPEED 0.5 /* Simulated dome speed 0.5 degrees per second, constant */

BaaderDome::BaaderDome()
{
    targetAz         = 0;
    m_ShutterState     = SHUTTER_UNKNOWN;
    flapStatus       = FLAP_UNKNOWN;
    simShutterStatus = SHUTTER_CLOSED;
    simFlapStatus    = FLAP_CLOSED;

    status           = DOME_UNKNOWN;
    targetShutter    = SHUTTER_CLOSE;
    targetFlap       = FLAP_CLOSE;
    calibrationStage = CALIBRATION_UNKNOWN;

    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_ABS_MOVE | DOME_CAN_REL_MOVE | DOME_CAN_PARK | DOME_HAS_SHUTTER |
                      DOME_HAS_VARIABLE_SPEED);
}

bool BaaderDome::initProperties()
{
    INDI::Dome::initProperties();

    CalibrateSP[0].fill("Start", "", ISS_OFF);
    CalibrateSP.fill(getDeviceName(), "Calibrate", "", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    DomeFlapSP[FLAP_OPEN].fill("FLAP_OPEN", "Open", ISS_OFF);
    DomeFlapSP[FLAP_CLOSE].fill("FLAP_CLOSE", "Close", ISS_ON);
    DomeFlapSP.fill(getDeviceName(), "DOME_FLAP", "Flap", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_OK);

    SetParkDataType(PARK_AZ);

    addAuxControls();

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::SetupParms()
{
    targetAz = 0;

    if (UpdatePosition())
        DomeAbsPosNP.apply();

    if (UpdateShutterStatus())
        DomeShutterSP.apply();

    if (UpdateFlapStatus())
        DomeFlapSP.apply(nullptr);

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

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::Handshake()
{
    return Ack();
}

/************************************************************************************
 *
* ***********************************************************************************/
const char *BaaderDome::getDefaultName()
{
    return (const char *)"Baader Dome";
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        defineProperty(DomeFlapSP);
        defineProperty(CalibrateSP);

        SetupParms();
    }
    else
    {
        deleteProperty(DomeFlapSP);
        deleteProperty(CalibrateSP);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (CalibrateSP.isNameMatch(name))
        {
            CalibrateSP.reset();

            if (status == DOME_READY)
            {
                CalibrateSP.setState(IPS_OK);
                LOG_INFO("Dome is already calibrated.");
                CalibrateSP.apply();
                return true;
            }

            if (CalibrateSP.getState() == IPS_BUSY)
            {
                Abort();
                LOG_INFO("Calibration aborted.");
                status        = DOME_UNKNOWN;
                CalibrateSP.setState(IPS_IDLE);
                CalibrateSP.apply();
                return true;
            }

            status = DOME_CALIBRATING;

            LOG_INFO("Starting calibration procedure...");

            calibrationStage = CALIBRATION_STAGE1;

            calibrationStart = DomeAbsPosNP[0].getValue();

            // Goal of procedure is to reach south point to hit sensor
            calibrationTarget1 = calibrationStart + 179;
            if (calibrationTarget1 > 360)
                calibrationTarget1 -= 360;

            if (MoveAbs(calibrationTarget1) == IPS_IDLE)
            {
                CalibrateSP.setState(IPS_ALERT);
                LOG_ERROR("Calibration failure due to dome motion failure.");
                status = DOME_UNKNOWN;
                CalibrateSP.apply();
                return false;
            }

            DomeAbsPosNP.setState(IPS_BUSY);
            CalibrateSP.setState(IPS_BUSY);
            LOGF_INFO("Calibration is in progress. Moving to position %g.", calibrationTarget1);
            CalibrateSP.apply();
            return true;
        }

        if (DomeFlapSP.isNameMatch(name))
        {
            int ret        = 0;
            int prevStatus = DomeFlapSP.findOnSwitchIndex();
            DomeFlapSP.update(states, names, n);
            int FlapDome = DomeFlapSP.findOnSwitchIndex();

            // No change of status, let's return
            if (prevStatus == FlapDome)
            {
                DomeFlapSP.setState(IPS_OK);
                DomeFlapSP.apply();
            }

            // go back to prev status in case of failure
            DomeFlapSP.reset();
            DomeFlapSP[prevStatus].setState(ISS_ON);

            if (FlapDome == 0)
                ret = ControlDomeFlap(FLAP_OPEN);
            else
                ret = ControlDomeFlap(FLAP_CLOSE);

            if (ret == 0)
            {
                DomeFlapSP.setState(IPS_OK);
                DomeFlapSP.reset();
                DomeFlapSP[FlapDome].s = ISS_ON;
                LOGF_INFO("Flap is %s", (FlapDome == 0 ? "open" : "closed"));
                DomeFlapSP.apply();
                return true;
            }
            else if (ret == 1)
            {
                DomeFlapSP.setState(IPS_BUSY);
                DomeFlapSP.reset();
                DomeFlapSP[FlapDome].setState(ISS_ON);
                LOGF_INFO("Flap is %s", (FlapDome == 0 ? "opening" : "closing"));
                DomeFlapSP.apply();
                return true;
            }

            DomeFlapSP.setState(IPS_ALERT);
            LOGF_INFO("Flap failed to %s", (FlapDome == 0 ? "open" : "close"));
            DomeFlapSP.apply();
            return false;
        }
    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::Ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[DOME_BUF];
    char status[DOME_BUF];

    sim = isSimulation();

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, "d#getflap", DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("d#getflap Ack error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (d#getflap)");

    if (sim)
    {
        strncpy(resp, "d#flapclo", DOME_BUF);
        nbytes_read = DOME_CMD;
    }
    else if ((rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Ack error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, "d#%s", status);

    if (rc > 0)
        return true;

    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::UpdateShutterStatus()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[DOME_BUF];
    char status[DOME_BUF];

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, "d#getshut", DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("d#getshut UpdateShutterStatus error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (d#getshut)");

    if (sim)
    {
        if (simShutterStatus == SHUTTER_CLOSED)
            strncpy(resp, "d#shutclo", DOME_CMD + 1);
        else if (simShutterStatus == SHUTTER_OPENED)
            strncpy(resp, "d#shutope", DOME_CMD + 1);
        else if (simShutterStatus == SHUTTER_MOVING)
            strncpy(resp, "d#shutrun", DOME_CMD + 1);
        nbytes_read = DOME_CMD;
    }
    else if ((rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("UpdateShutterStatus error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, "d#shut%s", status);

    if (rc > 0)
    {
        DomeShutterSP.setState(IPS_OK);
        DomeShutterSP.reset();

        if (strcmp(status, "ope") == 0)
        {
            if (m_ShutterState == SHUTTER_MOVING && targetShutter == SHUTTER_OPEN)
                LOGF_INFO("%s", GetShutterStatusString(SHUTTER_OPENED));

            m_ShutterState                 = SHUTTER_OPENED;
            DomeShutterSP[SHUTTER_OPEN].setState(ISS_ON);
        }
        else if (strcmp(status, "clo") == 0)
        {
            if (m_ShutterState == SHUTTER_MOVING && targetShutter == SHUTTER_CLOSE)
                LOGF_INFO("%s", GetShutterStatusString(SHUTTER_CLOSED));

            m_ShutterState                  = SHUTTER_CLOSED;
            DomeShutterSP[SHUTTER_CLOSE].setState(ISS_ON);
        }
        else if (strcmp(status, "run") == 0)
        {
            m_ShutterState = SHUTTER_MOVING;
            DomeShutterSP.setState(IPS_BUSY);
        }
        else
        {
            m_ShutterState = SHUTTER_UNKNOWN;
            DomeShutterSP.setState(IPS_ALERT);
            LOGF_ERROR("Unknown Shutter status: %s.", resp);
        }
        return true;
    }
    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::UpdatePosition()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[DOME_BUF];
    unsigned short domeAz = 0;

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, "d#getazim", DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("d#getazim UpdatePosition error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (d#getazim)");

    if (sim)
    {
        if (status == DOME_READY || calibrationStage == CALIBRATION_COMPLETE)
            snprintf(resp, DOME_BUF, "d#azr%04d", MountAzToDomeAz(DomeAbsPosNP[0].getValue()));
        else
            snprintf(resp, DOME_BUF, "d#azi%04d", MountAzToDomeAz(DomeAbsPosNP[0].getValue()));
        nbytes_read = DOME_CMD;
    }
    else if ((rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("UpdatePosition error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, "d#azr%hu", &domeAz);

    if (rc > 0)
    {
        if (calibrationStage == CALIBRATION_UNKNOWN)
        {
            status           = DOME_READY;
            calibrationStage = CALIBRATION_COMPLETE;
            LOG_INFO("Dome is calibrated.");
            CalibrateSP.setState(IPS_OK);
            CalibrateSP.apply(nullptr);
        }
        else if (status == DOME_CALIBRATING)
        {
            status           = DOME_READY;
            calibrationStage = CALIBRATION_COMPLETE;
            LOG_INFO("Calibration complete.");
            CalibrateSP.setState(IPS_OK);
            CalibrateSP.apply();
        }

        DomeAbsPosNP[0].setValue(DomeAzToMountAz(domeAz));
        return true;
    }
    else
    {
        rc = sscanf(resp, "d#azi%hu", &domeAz);
        if (rc > 0)
        {
            DomeAbsPosNP[0].setValue(DomeAzToMountAz(domeAz));
            return true;
        }
    }
    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
unsigned short BaaderDome::MountAzToDomeAz(double mountAz)
{
    int domeAz = 0;

    domeAz = (mountAz) * 10.0 - 1800;

    if (mountAz >= 0 && mountAz <= 179.9)
        domeAz += 3600;

    if (domeAz > 3599)
        domeAz = 3599;
    else if (domeAz < 0)
        domeAz = 0;

    return ((unsigned short)(domeAz));
}

/************************************************************************************
 *
* ***********************************************************************************/
double BaaderDome::DomeAzToMountAz(unsigned short domeAz)
{
    double mountAz = 0;

    mountAz = ((double)(domeAz + 1800)) / 10.0;

    if (domeAz >= 1800)
        mountAz -= 360;

    if (mountAz > 360)
        mountAz -= 360;
    else if (mountAz < 0)
        mountAz += 360;

    return mountAz;
}

/************************************************************************************
 *
* ***********************************************************************************/
void BaaderDome::TimerHit()
{
    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    UpdatePosition();

    if (DomeAbsPosNP.getState() == IPS_BUSY)
    {
        if (sim)
        {
            double speed = 0;
            auto currentPosition = DomeAbsPosNP[0].getValue();
            if (fabs(targetAz - currentPosition) > SIM_DOME_HI_SPEED)
                speed = SIM_DOME_HI_SPEED;
            else
                speed = SIM_DOME_LO_SPEED;

            if (DomeRelPosNP.getState() == IPS_BUSY)
            {
                // CW
                if (DomeMotionSP[0].getState() == ISS_ON)
                    currentPosition += speed;
                // CCW
                else
                    currentPosition -= speed;
            }
            else
            {
                if (targetAz > DomeAbsPosNP[0].getValue())
                {
                    currentPosition += speed;
                }
                else if (targetAz < DomeAbsPosNP[0].getValue())
                {
                    currentPosition -= speed;
                }
            }

            currentPosition = range360(speed);
            DomeAbsPosNP[0].setValue(currentPosition);
        }

        if (std::abs(targetAz - DomeAbsPosNP[0].getValue()) < DomeParamNP[0].getValue())
        {
            DomeAbsPosNP[0].setValue(targetAz);
            LOG_INFO("Dome reached requested azimuth angle.");

            if (status != DOME_CALIBRATING)
            {
                if (getDomeState() == DOME_PARKING)
                    SetParked(true);
                else if (getDomeState() == DOME_UNPARKING)
                    SetParked(false);
                else
                    setDomeState(DOME_SYNCED);
            }

            if (status == DOME_CALIBRATING)
            {
                if (calibrationStage == CALIBRATION_STAGE1)
                {
                    LOG_INFO("Calibration stage 1 complete. Starting stage 2...");
                    calibrationTarget2 = DomeAbsPosNP[0].getValue() + 2;
                    calibrationStage   = CALIBRATION_STAGE2;
                    MoveAbs(calibrationTarget2);
                    DomeAbsPosNP.setState(IPS_BUSY);
                }
                else if (calibrationStage == CALIBRATION_STAGE2)
                {
                    LOGF_INFO("Calibration stage 2 complete. Returning to initial position %g...", calibrationStart);
                    calibrationStage = CALIBRATION_STAGE3;
                    MoveAbs(calibrationStart);
                    DomeAbsPosNP.setState(IPS_BUSY);
                }
                else if (calibrationStage == CALIBRATION_STAGE3)
                {
                    calibrationStage = CALIBRATION_COMPLETE;
                    LOG_INFO("Dome reached initial position.");
                }
            }
        }

        DomeAbsPosNP.apply();
    }
    else
        DomeAbsPosNP.apply();

    UpdateShutterStatus();

    if (sim && DomeShutterSP.getState() == IPS_BUSY)
    {
        if (simShutterTimer-- <= 0)
        {
            simShutterTimer  = 0;
            simShutterStatus = (targetShutter == SHUTTER_OPEN) ? SHUTTER_OPENED : SHUTTER_CLOSED;
        }
    }
    else
        DomeShutterSP.apply();

    UpdateFlapStatus();

    if (sim && DomeFlapSP.getState() == IPS_BUSY)
    {
        if (simFlapTimer-- <= 0)
        {
            simFlapTimer  = 0;
            simFlapStatus = (targetFlap == FLAP_OPEN) ? FLAP_OPENED : FLAP_CLOSED;
        }
    }
    else
        DomeFlapSP.apply();

    SetTimer(getCurrentPollingPeriod());
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState BaaderDome::MoveAbs(double az)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[DOME_BUF];
    char resp[DOME_BUF];

    if (status == DOME_UNKNOWN)
    {
        LOG_WARN("Dome is not calibrated. Please calibrate dome before issuing any commands.");
        return IPS_ALERT;
    }

    targetAz = az;

    snprintf(cmd, DOME_BUF, "d#azi%04d", MountAzToDomeAz(targetAz));

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, cmd, DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s MoveAbsDome error: %s.", cmd, errstr);
        return IPS_ALERT;
    }

    LOGF_DEBUG("CMD (%s)", cmd);

    if (sim)
    {
        strncpy(resp, "d#gotmess", DOME_CMD + 1);
        nbytes_read = DOME_CMD;
    }
    else if ((rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("MoveAbsDome error: %s.", errstr);
        return IPS_ALERT;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    if (strcmp(resp, "d#gotmess") == 0)
        return IPS_BUSY;

    return IPS_ALERT;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState BaaderDome::MoveRel(double azDiff)
{
    targetAz = range360(DomeAbsPosNP[0].getValue() + azDiff);
    return MoveAbs(targetAz);
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState BaaderDome::Park()
{
    targetAz = GetAxis1Park();
    return MoveAbs(targetAz);
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState BaaderDome::UnPark()
{
    return IPS_OK;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState BaaderDome::ControlShutter(ShutterOperation operation)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[DOME_BUF];
    char resp[DOME_BUF];

    memset(cmd, 0, sizeof(cmd));

    if (operation == SHUTTER_OPEN)
    {
        targetShutter = operation;
        strncpy(cmd, "d#opeshut", DOME_CMD + 1);
    }
    else
    {
        targetShutter = operation;
        strncpy(cmd, "d#closhut", DOME_CMD + 1);
    }

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, cmd, DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s ControlDomeShutter error: %s.", cmd, errstr);
        return IPS_ALERT;
    }

    LOGF_DEBUG("CMD (%s)", cmd);

    if (sim)
    {
        simShutterTimer = SIM_SHUTTER_TIMER;
        strncpy(resp, "d#gotmess", DOME_CMD + 1);
        nbytes_read = DOME_CMD;
    }
    else if ((rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("ControlDomeShutter error: %s.", errstr);
        return IPS_ALERT;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    if (strcmp(resp, "d#gotmess") == 0)
    {
        m_ShutterState = simShutterStatus = SHUTTER_MOVING;
        return IPS_BUSY;
    }
    return IPS_ALERT;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::Abort()
{
    LOGF_INFO("Attempting to abort dome motion by stopping at %g", DomeAbsPosNP[0].getValue());
    MoveAbs(DomeAbsPosNP[0].getValue());
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
const char *BaaderDome::GetFlapStatusString(FlapStatus status)
{
    switch (status)
    {
        case FLAP_OPENED:
            return "Flap is open.";
            break;
        case FLAP_CLOSED:
            return "Flap is closed.";
            break;
        case FLAP_MOVING:
            return "Flap is in motion.";
            break;
        case FLAP_UNKNOWN:
        default:
            return "Flap status is unknown.";
            break;
    }
}

/************************************************************************************
 *
* ***********************************************************************************/
int BaaderDome::ControlDomeFlap(FlapOperation operation)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[DOME_BUF];
    char resp[DOME_BUF];

    memset(cmd, 0, sizeof(cmd));

    if (operation == FLAP_OPEN)
    {
        targetFlap = operation;
        strncpy(cmd, "d#opeflap", DOME_CMD + 1);
    }
    else
    {
        targetFlap = operation;
        strncpy(cmd, "d#cloflap", DOME_CMD + 1);
    }

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, cmd, DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s ControlDomeFlap error: %s.", cmd, errstr);
        return -1;
    }

    LOGF_DEBUG("CMD (%s)", cmd);

    if (sim)
    {
        simFlapTimer = SIM_FLAP_TIMER;
        strncpy(resp, "d#gotmess", DOME_CMD + 1);
        nbytes_read = DOME_CMD;
    }
    else if ((rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("ControlDomeFlap error: %s.", errstr);
        return -1;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    if (strcmp(resp, "d#gotmess") == 0)
    {
        flapStatus = simFlapStatus = FLAP_MOVING;
        return 1;
    }
    return -1;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::UpdateFlapStatus()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[DOME_BUF];
    char status[DOME_BUF];

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, "d#getflap", DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("d#getflap UpdateflapStatus error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (d#getflap)");

    if (sim)
    {
        if (simFlapStatus == FLAP_CLOSED)
            strncpy(resp, "d#flapclo", DOME_CMD + 1);
        else if (simFlapStatus == FLAP_OPENED)
            strncpy(resp, "d#flapope", DOME_CMD + 1);
        else if (simFlapStatus == FLAP_MOVING)
            strncpy(resp, "d#flaprun", DOME_CMD + 1);
        nbytes_read = DOME_CMD;
    }
    else if ((rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("UpdateflapStatus error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, "d#flap%s", status);

    if (rc > 0)
    {
        DomeFlapSP.setState(IPS_OK);
        DomeFlapSP.reset();

        if (strcmp(status, "ope") == 0)
        {
            if (flapStatus == FLAP_MOVING && targetFlap == FLAP_OPEN)
                LOGF_INFO("%s", GetFlapStatusString(FLAP_OPENED));

            flapStatus             = FLAP_OPENED;
            DomeFlapSP[FLAP_OPEN].setState(ISS_ON);
        }
        else if (strcmp(status, "clo") == 0)
        {
            if (flapStatus == FLAP_MOVING && targetFlap == FLAP_CLOSE)
                LOGF_INFO("%s", GetFlapStatusString(FLAP_CLOSED));

            flapStatus              = FLAP_CLOSED;
            DomeFlapSP[FLAP_CLOSE].setState(ISS_ON);
        }
        else if (strcmp(status, "run") == 0)
        {
            flapStatus   = FLAP_MOVING;
            DomeFlapSP.setState(IPS_BUSY);
        }
        else
        {
            flapStatus   = FLAP_UNKNOWN;
            DomeFlapSP.setState(IPS_ALERT);
            LOGF_ERROR("Unknown flap status: %s.", resp);
        }
        return true;
    }
    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::SaveEncoderPosition()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[DOME_BUF];
    char resp[DOME_BUF];

    strncpy(cmd, "d#encsave", DOME_CMD + 1);

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, cmd, DOME_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s SaveEncoderPosition error: %s.", cmd, errstr);
        return false;
    }

    LOGF_DEBUG("CMD (%s)", cmd);

    if (sim)
    {
        strncpy(resp, "d#gotmess", DOME_CMD + 1);
        nbytes_read = DOME_CMD;
    }
    else if ((rc = tty_read(PortFD, resp, DOME_CMD, DOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("SaveEncoderPosition error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    return strcmp(resp, "d#gotmess") == 0;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::saveConfigItems(FILE *fp)
{
    // Only save if calibration is complete
    if (calibrationStage == CALIBRATION_COMPLETE)
        SaveEncoderPosition();

    return INDI::Dome::saveConfigItems(fp);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool BaaderDome::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosNP[0].getValue());
    return true;
}
/************************************************************************************
 *
* ***********************************************************************************/

bool BaaderDome::SetDefaultPark()
{
    // By default set position to 90
    SetAxis1Park(90);
    return true;
}
