/*******************************************************************************
  Copyright(c) 2013 Jasem Mutlaq. All rights reserved.

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

#include "joystick.h"

#include "joystickdriver.h"
#include "indistandardproperty.h"

#include <memory>
#include <cstring>

// We declare an auto pointer to joystick.
std::unique_ptr<JoyStick> joystick(new JoyStick());

JoyStick::JoyStick()
{
    driver.reset(new JoyStickDriver());
}

const char *JoyStick::getDefaultName()
{
    return "Joystick";
}

bool JoyStick::Connect()
{
    bool rc = driver->Connect();

    if (rc)
    {
        LOG_INFO("Joystick is online.");

        setupParams();
    }
    else
        LOG_INFO("Error: cannot find Joystick device.");

    return rc;
}

bool JoyStick::Disconnect()
{
    LOG_INFO("Joystick is offline.");

    return driver->Disconnect();
}

void JoyStick::setupParams()
{
    char propName[16] = {0}, propLabel[16] = {0};

    if (driver == nullptr)
        return;

    int nAxis      = driver->getNumOfAxes();
    int nJoysticks = driver->getNumOfJoysticks();
    int nButtons   = driver->getNumrOfButtons();

    JoyStickNP = new INumberVectorProperty[nJoysticks];
    JoyStickN  = new INumber[nJoysticks * 2];

    AxisN = new INumber[nAxis];
    DeadZoneN = new INumber[nAxis];

    ButtonS = new ISwitch[nButtons];

    for (int i = 0; i < nJoysticks * 2; i += 2)
    {
        snprintf(propName, 16, "JOYSTICK_%d", i / 2 + 1);
        snprintf(propLabel, 16, "Joystick %d", i / 2 + 1);

        IUFillNumber(&JoyStickN[i], "JOYSTICK_MAGNITUDE", "Magnitude", "%g", -32767.0, 32767.0, 0, 0);
        IUFillNumber(&JoyStickN[i + 1], "JOYSTICK_ANGLE", "Angle", "%g", 0, 360.0, 0, 0);
        IUFillNumberVector(&JoyStickNP[i / 2], JoyStickN + i, 2, getDeviceName(), propName, propLabel, "Monitor", IP_RO,
                           0, IPS_IDLE);
    }

    for (int i = 0; i < nAxis; i++)
    {
        snprintf(propName, 16, "AXIS_%d", i + 1);
        snprintf(propLabel, 16, "Axis %d", i + 1);

        IUFillNumber(&AxisN[i], propName, propLabel, "%.f", -32767.0, 32767.0, 0, 0);
        IUFillNumber(&DeadZoneN[i], propName, propLabel, "%.f", 0, 5000, 500, 5);
    }

    IUFillNumberVector(&AxisNP, AxisN, nAxis, getDeviceName(), "JOYSTICK_AXES", "Axes", "Monitor", IP_RO, 0, IPS_IDLE);

    IUFillNumberVector(&DeadZoneNP, DeadZoneN, nAxis, getDeviceName(), "JOYSTICK_DEAD_ZONE", "Axes", "Dead Zones", IP_RW, 0, IPS_IDLE);

    for (int i = 0; i < nButtons; i++)
    {
        snprintf(propName, 16, "BUTTON_%d", i + 1);
        snprintf(propLabel, 16, "Button %d", i + 1);

        IUFillSwitch(&ButtonS[i], propName, propLabel, ISS_OFF);
    }

    IUFillSwitchVector(&ButtonSP, ButtonS, nButtons, getDeviceName(), "JOYSTICK_BUTTONS", "Buttons", "Monitor", IP_RO,
                       ISR_NOFMANY, 0, IPS_IDLE);
}

bool JoyStick::initProperties()
{
    INDI::DefaultDevice::initProperties();

    IUFillText(&PortT[0], "PORT", "Port", "/dev/input/js0");
    IUFillTextVector(&PortTP, PortT, 1, getDeviceName(), INDI::SP::DEVICE_PORT, "Ports", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    IUFillText(&JoystickInfoT[0], "JOYSTICK_NAME", "Name", "");
    IUFillText(&JoystickInfoT[1], "JOYSTICK_VERSION", "Version", "");
    IUFillText(&JoystickInfoT[2], "JOYSTICK_NJOYSTICKS", "# Joysticks", "");
    IUFillText(&JoystickInfoT[3], "JOYSTICK_NAXES", "# Axes", "");
    IUFillText(&JoystickInfoT[4], "JOYSTICK_NBUTTONS", "# Buttons", "");
    IUFillTextVector(&JoystickInfoTP, JoystickInfoT, 5, getDeviceName(), "JOYSTICK_INFO", "Joystick Info",
                     MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    addDebugControl();

    return true;
}

bool JoyStick::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        char buf[8];
        // Name
        IUSaveText(&JoystickInfoT[0], driver->getName());
        // Version
        snprintf(buf, 8, "%d", driver->getVersion());
        IUSaveText(&JoystickInfoT[1], buf);
        // # of Joysticks
        snprintf(buf, 8, "%d", driver->getNumOfJoysticks());
        IUSaveText(&JoystickInfoT[2], buf);
        // # of Axes
        snprintf(buf, 8, "%d", driver->getNumOfAxes());
        IUSaveText(&JoystickInfoT[3], buf);
        // # of buttons
        snprintf(buf, 8, "%d", driver->getNumrOfButtons());
        IUSaveText(&JoystickInfoT[4], buf);

        defineProperty(&JoystickInfoTP);

        for (int i = 0; i < driver->getNumOfJoysticks(); i++)
            defineProperty(&JoyStickNP[i]);

        defineProperty(&AxisNP);
        defineProperty(&ButtonSP);

        // Dead zones
        defineProperty(&DeadZoneNP);

        // N.B. Only set callbacks AFTER we define our properties above
        // because these calls backs otherwise can be called asynchronously
        // and they mess up INDI XML output
        driver->setJoystickCallback(joystickHelper);
        driver->setAxisCallback(axisHelper);
        driver->setButtonCallback(buttonHelper);
    }
    else
    {
        deleteProperty(JoystickInfoTP.name);

        for (int i = 0; i < driver->getNumOfJoysticks(); i++)
            deleteProperty(JoyStickNP[i].name);

        deleteProperty(AxisNP.name);
        deleteProperty(DeadZoneNP.name);
        deleteProperty(ButtonSP.name);

        delete[] JoyStickNP;
        delete[] JoyStickN;
        delete[] AxisN;
        delete[] DeadZoneN;
        delete[] ButtonS;
    }

    return true;
}

void JoyStick::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineProperty(&PortTP);
    loadConfig(true, INDI::SP::DEVICE_PORT);

    /*
    if (isConnected())
    {
        for (int i = 0; i < driver->getNumOfJoysticks(); i++)
            defineProperty(&JoyStickNP[i]);

        defineProperty(&AxisNP);
        defineProperty(&ButtonSP);
    }
    */
}

bool JoyStick::ISSnoopDevice(XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool JoyStick::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, PortTP.name) == 0)
        {
            PortTP.s = IPS_OK;
            IUUpdateText(&PortTP, texts, names, n);
            //  Update client display
            IDSetText(&PortTP, nullptr);

            driver->setPort(PortT[0].text);

            return true;
        }
    }

    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool JoyStick::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, DeadZoneNP.name))
        {
            IUUpdateNumber(&DeadZoneNP, values, names, n);
            DeadZoneNP.s = IPS_OK;
            IDSetNumber(&DeadZoneNP, nullptr);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

void JoyStick::joystickHelper(int joystick_n, double mag, double angle)
{
    joystick->joystickEvent(joystick_n, mag, angle);
}

void JoyStick::buttonHelper(int button_n, int value)
{
    joystick->buttonEvent(button_n, value);
}

void JoyStick::axisHelper(int axis_n, int value)
{
    joystick->axisEvent(axis_n, value);
}

void JoyStick::joystickEvent(int joystick_n, double mag, double angle)
{
    if (!isConnected())
        return;

    LOGF_DEBUG("joystickEvent[%d]: %g @ %g", joystick_n, mag, angle);

    if (mag == 0)
        JoyStickNP[joystick_n].s = IPS_IDLE;
    else
        JoyStickNP[joystick_n].s = IPS_BUSY;

    JoyStickNP[joystick_n].np[0].value = mag;
    JoyStickNP[joystick_n].np[1].value = angle;

    IDSetNumber(&JoyStickNP[joystick_n], nullptr);
}

void JoyStick::axisEvent(int axis_n, int value)
{
    if (!isConnected())
        return;

    LOGF_DEBUG("axisEvent[%d]: %d", axis_n, value);

    // All values within deadzone is reset to zero.
    if (std::abs(value) <= DeadZoneN[axis_n].value)
        value = 0;

    if (value == 0)
        AxisNP.s = IPS_IDLE;
    else
        AxisNP.s = IPS_BUSY;

    AxisNP.np[axis_n].value = value;

    IDSetNumber(&AxisNP, nullptr);
}

void JoyStick::buttonEvent(int button_n, int value)
{
    if (!isConnected())
        return;

    LOGF_DEBUG("buttonEvent[%d]: %s", button_n, value > 0 ? "On" : "Off");

    ButtonSP.s          = IPS_OK;
    ButtonS[button_n].s = (value == 0) ? ISS_OFF : ISS_ON;

    IDSetSwitch(&ButtonSP, nullptr);
}

bool JoyStick::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    IUSaveConfigText(fp, &PortTP);
    IUSaveConfigNumber(fp, &DeadZoneNP);

    return true;
}
