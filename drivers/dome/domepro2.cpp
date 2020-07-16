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

#include <cmath>
#include <cstring>
#include <memory>
#include <regex>
#include <termios.h>

static std::unique_ptr<DomePro2> domepro2(new DomePro2());

void ISGetProperties(const char *dev)
{
    domepro2->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    domepro2->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    domepro2->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    domepro2->ISNewNumber(dev, name, values, names, n);
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
    domepro2->ISSnoopDevice(root);
}

DomePro2::DomePro2()
{
    m_ShutterState     = SHUTTER_UNKNOWN;

    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_ABS_MOVE | DOME_CAN_REL_MOVE | DOME_CAN_PARK | DOME_HAS_SHUTTER |
                      DOME_HAS_VARIABLE_SPEED);
}

bool DomePro2::initProperties()
{
    INDI::Dome::initProperties();

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

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::Handshake()
{
    return true;
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
    }
    else
    {
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

    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}


////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
void DomePro2::TimerHit()
{
    if (!isConnected())
        return;

    SetTimer(POLLMS);
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomePro2::MoveAbs(double az)
{
    INDI_UNUSED(az);
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomePro2::MoveRel(double azDiff)
{
    targetAz = DomeAbsPosN[0].value + azDiff;

    if (targetAz < DomeAbsPosN[0].min)
        targetAz += DomeAbsPosN[0].max;
    if (targetAz > DomeAbsPosN[0].max)
        targetAz -= DomeAbsPosN[0].max;

    // It will take a few cycles to reach final position
    return MoveAbs(targetAz);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool DomePro2::Sync(double az)
{
    INDI_UNUSED(az);
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState DomePro2::Move(DomeDirection dir, DomeMotionCommand operation)
{
    INDI_UNUSED(dir);
    INDI_UNUSED(operation);
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
IPState DomePro2::Park()
{
    targetAz = GetAxis1Park();

    return MoveAbs(targetAz);
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
    INDI_UNUSED(operation);
    return IPS_ALERT;
}

////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////
bool DomePro2::Abort()
{
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
    SetAxis1Park(DomeAbsPosN[0].value);
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
        snprintf(formatted_command, DRIVER_LEN, "%s\r", cmd);
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
