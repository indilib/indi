/*******************************************************************************
  Copyright(c) 2022 RBFocus. All rights reserved.
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

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

#include "Excalibur.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <chrono>
#include <thread>

#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <math.h>

static std::unique_ptr<Excalibur> flatmaster(new Excalibur());

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
Excalibur::Excalibur() : LightBoxInterface(this), DustCapInterface(this)
{
    setVersion(1, 1);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool Excalibur::initProperties()
{
    INDI::DefaultDevice::initProperties();

    DI::initProperties(MAIN_CONTROL_TAB);
    LI::initProperties(MAIN_CONTROL_TAB, CAN_DIM);

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE);

    LightIntensityNP[0].setMin(0);
    LightIntensityNP[0].setMax(3000);
    LightIntensityNP[0].setStep(100);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
    serialConnection->registerHandshake([&]()
    {
        return Ack();
    });


    registerConnection(serialConnection);
    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool Excalibur::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    DI::updateProperties();
    LI::updateProperties();

    if (isConnected())
    {
        getParkingStatus();
        getLightIntensity();
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
const char *Excalibur::getDefaultName()
{
    return "RBF Excalibur";
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool Excalibur::Ack()
{
    PortFD = serialConnection->getPortFD();

    char response[16] = {0};
    // Try up to 3 times before giving up
    for (int i = 0; i < 3; i++)
    {
        if(sendCommand("#", response))
        {
            if(strstr("FLAT.FLAP!#", response) != nullptr)
            {
                LightSP[FLAT_LIGHT_ON].setState(ISS_OFF);
                LightSP[FLAT_LIGHT_OFF].setState(ISS_ON);
                LightSP.apply();
                return true;

            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOG_ERROR("Ack failed.");
    return false;

}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool Excalibur::Disconnect()
{
    EnableLightBox(false);
    return INDI::DefaultDevice::Disconnect();
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool Excalibur::EnableLightBox(bool enable)
{
    char response[20] = {0};
    char cmd[16] = {0};
    snprintf(cmd, 16, "L%d##", enable ? static_cast<int>(LightIntensityNP[0].getValue()) : 0);
    return sendCommand(cmd, response);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool Excalibur::SetLightBoxBrightness(uint16_t value)
{
    // If light box is not on, then we simply accept the value as-is
    // without dispatching the command to the device
    if (value > 0 && LightSP[FLAT_LIGHT_ON].getState() != ISS_ON)
        return true;

    if( ParkCapSP[0].getState() != ISS_ON)
    {
        LOG_ERROR("You must Park eXcalibur first.");
        return false;
    }

    char cmd[DRIVER_RES] = {0};
    snprintf(cmd, 30, "L%d##", value);
    return sendCommand(cmd);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
IPState Excalibur::ParkCap()
{
    return sendCommand("S1#") ? IPS_BUSY : IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
IPState Excalibur::UnParkCap()
{
    return sendCommand("S0#") ? IPS_BUSY : IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void Excalibur::TimerHit()
{
    if (!isConnected())
        return;

    getParkingStatus();

    SetTimer(getCurrentPollingPeriod());
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool Excalibur::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (LI::processNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool Excalibur::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (LI::processText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool Excalibur::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (DI::processSwitch(dev, name, states, names, n))
            return true;
        if (LI::processSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool Excalibur::ISSnoopDevice(XMLEle *root)
{
    LI::snoop(root);
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool Excalibur::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    return LI::saveConfigItems(fp);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void Excalibur::getLightIntensity()
{
    char res[DRIVER_RES] = {0};

    // Get Light Intensity Status
    sendCommand("O#", res);

    int32_t pos;
    int rc = sscanf(res, "%d#", &pos);

    auto previousIntensity = LightIntensityNP[0].getValue();

    if (rc > 0)
    {
        LightIntensityNP[0].setValue(pos);
        // Only update if necessary
        if (previousIntensity != pos)
            LightIntensityNP.apply();
    }

    auto haveLight = pos > 0;

    // If we have light, but switch is off, then turn it on and vice versa
    if ( (haveLight && LightSP[FLAT_LIGHT_OFF].getState() == ISS_ON) || (!haveLight
            && LightSP[FLAT_LIGHT_ON].getState() == ISS_ON) )
    {
        LightSP.reset();
        LightSP[FLAT_LIGHT_ON].setState(haveLight ? ISS_ON : ISS_OFF);
        LightSP[FLAT_LIGHT_OFF].setState(haveLight ? ISS_OFF : ISS_ON);
        LightSP.apply();
    }

}
//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void Excalibur::getParkingStatus()
{
    char res[DRIVER_RES] = {0};

    // Get Dust Cap Status
    sendCommand("P#", res);
    int32_t pos2;
    sscanf(res, "%d#", &pos2);

    auto isClosed = pos2 <= 0;

    if (ParkCapSP.getState() == IPS_BUSY || ParkCapSP.getState() == IPS_IDLE)
    {
        ParkCapSP.setState(IPS_OK);
        ParkCapSP.reset();
        // Parked? If closed, then yes
        ParkCapSP[INDI_ENABLED].setState(isClosed ? ISS_ON : ISS_OFF);
        // Unparked?
        ParkCapSP[INDI_DISABLED].setState(isClosed ? ISS_OFF : ISS_ON);
        ParkCapSP.apply();
    }
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool Excalibur::sendCommand(const char *command, char *res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    char cmd[20] = {0};
    snprintf(cmd, 20, "%s", command);

    LOGF_DEBUG("CMD <%#02X>", cmd[0]);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }
    if (res == nullptr)
        return true;

    if ((rc = tty_read_section(PortFD, res, DRIVER_DEL, 5, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return  false;
    }

    // Remove the #
    res[nbytes_read - 1] = 0;

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}
