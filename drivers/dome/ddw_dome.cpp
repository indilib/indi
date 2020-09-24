/*******************************************************************************
 DDW Dome INDI Driver

 Copyright(c) 2020 Jarno Paananen. All rights reserved.

 based on:

 ScopeDome Dome INDI Driver

 Copyright(c) 2017-2020 Jarno Paananen. All rights reserved.

 and

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

#include "ddw_dome.h"

#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>
#include <wordexp.h>
#include <unistd.h>

#define DDW_TIMEOUT 2

// We declare an auto pointer to DDW.
std::unique_ptr<DDW> ddw(new DDW());

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
    ddw->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    ddw->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    ddw->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    ddw->ISNewNumber(dev, name, values, names, n);
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
    ddw->ISSnoopDevice(root);
}

DDW::DDW()
{
    setVersion(1, 0);
    m_ShutterState     = SHUTTER_UNKNOWN;

    status        = DOME_UNKNOWN;
    targetShutter = SHUTTER_CLOSE;

    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_ABS_MOVE | DOME_CAN_PARK | DOME_HAS_SHUTTER);
}

bool DDW::initProperties()
{
    INDI::Dome::initProperties();

    IUFillNumber(&FirmwareVersionN[0], "VERSION", "Version", "%2.0f", 0.0, 99.0, 1.0, 0.0);
    IUFillNumberVector(&FirmwareVersionNP, FirmwareVersionN, 2, getDeviceName(), "FIRMWARE",
                       "Firmware", INFO_TAB, IP_RO, 60, IPS_IDLE);

    SetParkDataType(PARK_AZ);

    addAuxControls();

    // Set serial parameters
    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);

    setPollingPeriodRange(1000, 3000); // Device doesn't like too long interval
    setDefaultPollingPeriod(1000);
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::SetupParms()
{
    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking
        // values.
        SetAxis1ParkDefault(0);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is
        // found.
        SetAxis1Park(0);
        SetAxis1ParkDefault(0);
    }

    FirmwareVersionN[0].value = fwVersion;
    FirmwareVersionNP.s       = IPS_OK;
    IDSetNumber(&FirmwareVersionNP, nullptr);
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::Handshake()
{
    // Send GINF command and check if the controller responds
    int rc = writeCmd("GINF");
    if (rc != TTY_OK)
        return false;

    std::string response;
    rc = readStatus(response);
    if (rc != TTY_OK)
        return false;

    LOGF_DEBUG("Initial response: %s", response.c_str());

    // Response should start with V
    if(response[0] != 'V')
        return false;
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
const char *DDW::getDefaultName()
{
    return (const char *)"DDW Dome";
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        defineNumber(&FirmwareVersionNP);
        SetupParms();
    }
    else
    {
        deleteProperty(FirmwareVersionNP.name);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

bool DDW::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

int DDW::writeCmd(const char* cmd)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error writing command: %s. Cmd: %s", errstr, cmd);
    }    
    return rc;
}

int DDW::readStatus(std::string& status)
{
    int nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    // Read response and parse it
    char response[256] = {0};

    // Read buffer
    if ((rc = tty_nread_section(PortFD, response, sizeof(response), '\r', DDW_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error reading: %s.", errstr);
    }

    status = response;
    return rc;
}

/************************************************************************************
 *
* ***********************************************************************************/
void DDW::TimerHit()
{
    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    // Check state machine where we are going currently
    switch(status)
    {
        case DOME_READY:
            break;
        default:
            break;
    }
#if 0
    else if (DomeAbsPosNP.s == IPS_BUSY)
    {
        if ((currentStatus & STATUS_MOVING) == 0)
        {
            // Rotation idle, are we close enough?
            double azDiff = targetAz - DomeAbsPosN[0].value;

            if (azDiff > 180)
            {
                azDiff -= 360;
            }
            if (azDiff < -180)
            {
                azDiff += 360;
            }
            if (!refineMove || fabs(azDiff) <= DomeParamN[0].value)
            {
                if (refineMove)
                    DomeAbsPosN[0].value = targetAz;
                DomeAbsPosNP.s = IPS_OK;
                LOG_INFO("Dome reached requested azimuth angle.");

                if (getDomeState() == DOME_PARKING)
                {
                    if (ParkShutterS[0].s == ISS_ON && getInputState(IN_CLOSED1) == ISS_OFF)
                    {
                        ControlShutter(SHUTTER_CLOSE);
                    }
                    else
                    {
                        SetParked(true);
                    }
                }
                else if (getDomeState() == DOME_UNPARKING)
                    SetParked(false);
                else
                    setDomeState(DOME_SYNCED);
            }
            else
            {
                // Refine azimuth
                MoveAbs(targetAz);
            }
        }

        IDSetNumber(&DomeAbsPosNP, nullptr);
    }
    else
        IDSetNumber(&DomeAbsPosNP, nullptr);
#endif
    SetTimer(POLLMS);
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState DDW::MoveAbs(double az)
{
    LOGF_DEBUG("MoveAbs (%f)", az);

    // Construct move command
    return IPS_BUSY;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState DDW::Park()
{
    // First move to park position and then optionally close shutter
    IPState s = MoveAbs(GetAxis1Park());
    if (s == IPS_OK && ShutterParkPolicyS[SHUTTER_CLOSE_ON_PARK].s == ISS_ON)
    {
        // Already at home, just close if needed
        return ControlShutter(SHUTTER_CLOSE);
    }
    return s;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState DDW::UnPark()
{
    if (ShutterParkPolicyS[SHUTTER_OPEN_ON_UNPARK].s == ISS_ON)
    {
        return ControlShutter(SHUTTER_OPEN);
    }
    return IPS_OK;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState DDW::ControlShutter(ShutterOperation operation)
{
    LOGF_INFO("Control shutter %d", (int)operation);
    targetShutter = operation;
    if (operation == SHUTTER_OPEN)
    {
        LOG_INFO("Opening shutter");
//        if (getInputState(IN_OPEN1))
        {
            LOG_INFO("Shutter already open");
            return IPS_OK;
        }
//        setOutputState(OUT_CLOSE1, ISS_OFF);
//        setOutputState(OUT_OPEN1, ISS_ON);
    }
    else
    {
        LOG_INFO("Closing shutter");
//        if (getInputState(IN_CLOSED1))
        {
            LOG_INFO("Shutter already closed");
            return IPS_OK;
        }
//        setOutputState(OUT_OPEN1, ISS_OFF);
//        setOutputState(OUT_CLOSE1, ISS_ON);
    }

    m_ShutterState = SHUTTER_MOVING;
    return IPS_BUSY;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::Abort()
{
//    writeCmd(Stop);
    status = DOME_READY;
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::saveConfigItems(FILE *fp)
{
    return INDI::Dome::saveConfigItems(fp);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosN[0].value);
    return true;
}
/************************************************************************************
 *
* ***********************************************************************************/

bool DDW::SetDefaultPark()
{
    // By default set position to 90
    SetAxis1Park(90);
    return true;
}
