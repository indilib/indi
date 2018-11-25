/*******************************************************************************
  Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

  Pegasus Ultimate Power Box Driver.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "pegasus_upb.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <memory>
#include <termios.h>
#include <cstring>
#include <sys/ioctl.h>

// We declare an auto pointer to PegasusUPB.
static std::unique_ptr<PegasusUPB> upb(new PegasusUPB());

#define FLAT_TIMEOUT 3

void ISGetProperties(const char *dev)
{
    upb->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    upb->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    upb->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    upb->ISNewNumber(dev, name, values, names, n);
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
    upb->ISSnoopDevice(root);
}

PegasusUPB::PegasusUPB() : FI(this), WI(this)
{
    setVersion(1, 0);
}

bool PegasusUPB::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | FOCUSER_INTERFACE | WEATHER_INTERFACE);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]() { return Handshake(); });
    registerConnection(serialConnection);

    return true;
}

bool PegasusUPB::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        FI::updateProperties();
        WI::updateProperties();
    }
    else
    {
        FI::updateProperties();
        WI::updateProperties();
    }

    return true;
}

const char *PegasusUPB::getDefaultName()
{
    return "Pegasus UBP";
}

bool PegasusUPB::Handshake()
{    
    PortFD = serialConnection->getPortFD();

    return true;
}

bool PegasusUPB::Disconnect()
{
    //sendCommand("DISCONNECT#");

    return INDI::DefaultDevice::Disconnect();
}

bool PegasusUPB::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (strstr(name, "FOCUS"))
            return FI::processSwitch(dev, name, states, names, n);
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PegasusUPB::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (strstr(name, "FOCUS_"))
            return FI::processNumber(dev, name, values, names, n);

        if (strstr(name, "WEATHER_"))
            return WI::processNumber(dev, name, values, names, n);
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PegasusUPB::sendCommand(const char *cmd, char *res)
{
    int nbytes_read=0, nbytes_written=0, tty_rc = 0;
    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);
    if ( (tty_rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(tty_rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }

    if ( (tty_rc = tty_nread_section(PortFD, res, 0xA, MAXINDINAME, PEGASUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(tty_rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial read error: %s", errorMessage);
        return false;
    }

    res[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", res);

    return true;
}
