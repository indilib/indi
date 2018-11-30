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

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_REVERSE  |
                      FOCUSER_CAN_SYNC     |
                      FOCUSER_CAN_ABORT);

    addAuxControls();

    ////////////////////////////////////////////////////////////////////////////
    /// Main Control Panel
    ////////////////////////////////////////////////////////////////////////////
    // Cycle all power on/off
    IUFillSwitch(&PowerCycleAllS[POWER_CYCLE_ON], "POWER_CYCLE_ON", "All On", ISS_OFF);
    IUFillSwitch(&PowerCycleAllS[POWER_CYCLE_OFF], "POWER_CYCLE_OFF", "All Off", ISS_OFF);
    IUFillSwitchVector(&PowerCycleAllSP, PowerCycleAllS, 2, getDeviceName(), "POWER_CYCLE", "Cycle Power", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////
    /// Power Group
    ////////////////////////////////////////////////////////////////////////////

    // Turn on/off power
    IUFillSwitch(&PowerControlS[0], "POWER_CONTROL_1", "Port 1", ISS_OFF);
    IUFillSwitch(&PowerControlS[1], "POWER_CONTROL_2", "Port 2", ISS_OFF);
    IUFillSwitch(&PowerControlS[2], "POWER_CONTROL_2", "Port 3", ISS_OFF);
    IUFillSwitch(&PowerControlS[3], "POWER_CONTROL_2", "Port 4", ISS_OFF);
    IUFillSwitchVector(&PowerControlSP, PowerControlS, 4, getDeviceName(), "POWER_CONTROL", "Power Control", POWER_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

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
        // Main Control
        defineSwitch(&PowerCycleAllSP);

        // Power
        defineSwitch(&PowerControlSP);

        FI::updateProperties();
        WI::updateProperties();
    }
    else
    {
        // Main Control
        deleteProperty(PowerCycleAllSP.name);

        // Power
        deleteProperty(PowerControlSP.name);

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

    char res[MAXINDILABEL] = {0};

    bool rc = sendCommand("P#", res);
    if (!rc)
        return false;

    return (!strcmp(res, "UPB_OK"));
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
        // Cycle all power on or off
        if (!strcmp(name, PowerCycleAllSP.name))
        {
            IUUpdateSwitch(&PowerCycleAllSP, states, names, n);

            PowerCycleAllSP.s = IPS_ALERT;
            char cmd[MAXINDIDEVICE]={0},res[MAXINDINAME]={0};
            snprintf(cmd, MAXINDINAME, "PZ:%d", IUFindOnSwitchIndex(&PowerCycleAllSP));
            if (sendCommand(cmd, res))
            {
                PowerCycleAllSP.s = !strcmp(cmd, res) ? IPS_OK : IPS_ALERT;
            }

            IUResetSwitch(&PowerCycleAllSP);
            IDSetSwitch(&PowerCycleAllSP, nullptr);
            return true;
        }

        // Control Power per port
        if (!strcmp(name, PowerControlSP.name))
        {
            bool failed=false;
            for (int i=0; i < n; i++)
            {
                if (!strcmp(names[i], PowerControlS[i].name) && states[i] != PowerControlS[i].s)
                {
                    if (setPowerEnabled(i, states[i] == ISS_ON) == false)
                    {
                        failed = true;
                        break;
                    }
                }
            }

            if (failed)
               PowerControlSP.s = IPS_ALERT;
            else
            {
                PowerControlSP.s = IPS_OK;
                IUUpdateSwitch(&PowerControlSP, states, names, n);
            }

            IDSetSwitch(&PowerControlSP, nullptr);
            return true;
        }

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
    char command[MAXINDINAME]={0};
    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);
    snprintf(command, MAXINDINAME, "%s\n", cmd);
    if ( (tty_rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
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

IPState PegasusUPB::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[MAXINDINAME]={0}, res[MAXINDINAME] = {0};
    snprintf(cmd, MAXINDINAME, "SM:%d", targetTicks);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd) ? IPS_BUSY : IPS_ALERT);
    }

    return IPS_ALERT;
}

IPState PegasusUPB::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    return MoveAbsFocuser(dir == FOCUS_INWARD ? FocusAbsPosN[0].value - ticks : FocusAbsPosN[0].value + ticks);
}

bool PegasusUPB::AbortFocuser()
{
    char res[MAXINDINAME]={0};
    if (sendCommand("SH", res))
    {
        return (!strcmp(res, "H:1"));
    }

    return false;
}

bool PegasusUPB::ReverseFocuser(bool enabled)
{
    char cmd[MAXINDINAME]={0}, res[MAXINDINAME] = {0};
    snprintf(cmd, MAXINDINAME, "SR:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

bool PegasusUPB::SyncFocuser(uint32_t ticks)
{
    char cmd[MAXINDINAME]={0}, res[MAXINDINAME] = {0};
    snprintf(cmd, MAXINDINAME, "SC:%d", ticks);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

bool PegasusUPB::setPowerEnabled(uint8_t port, bool enabled)
{
    char cmd[MAXINDINAME]={0}, res[MAXINDINAME] = {0};
    snprintf(cmd, MAXINDINAME, "P%d:%d", port, enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}
