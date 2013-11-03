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

#ifndef INDIJOYSTICK_H
#define INDIJOYSTICK_H

#include <defaultdevice.h>

/* Standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include <sys/time.h>
#include <time.h>

class JoyStickDriver;

/**
 * @brief The JoyStick class provides an INDI driver that displays event data from game pads. The INDI driver can be encapsulated in any other driver
 * via snooping on properties of interesting.
 *
 * The driver enumerates the game pad and provides three types of constrcuts:
 * <ul>
 * <li><b>Joysticks</b>: Each joystick displays a normalized magnitude [0 to 1] and an angle. The angle is measured counter clock wise starting from
 *  the right/east direction [0 to 360]. They are defined as JOYSTICK_# where # is the joystick number.</li>
 * <li><b>Axes</b>: Each joystick has two or more axes. Each axis has a raw value and angle. The raw value ranges from -32767.0 to 32767.0 They are
 * defined as AXIS_# where # is the axis number.</li>
 * <li><b>Buttons</b>: Buttons are either on or off. They are defined as BUTTON_# where # is the button number.</li>
 * </ul>
 *
 * To snoop on buttons, call IDSnoopDevice("Joystick", "JOYSTICK_BUTTONS") from your driver.
 *
 * \note All indexes start from 1. i.e. There is no BUTTON_0 or JOYSTICK_0. Telescope Simulator and INDI EQMod driver implmement joystick snooping.
 */
class JoyStick : public INDI::DefaultDevice
{
    public:
    JoyStick();
    virtual ~JoyStick();

    virtual bool initProperties();
    virtual bool updateProperties();
    virtual void ISGetProperties (const char *dev);
    virtual bool ISSnoopDevice (XMLEle *root);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

    static void joystickHelper(int joystick_n, double mag, double angle);
    static void axisHelper(int axis_n, int value);
    static void buttonHelper(int button_n, int value);

    protected:

    //  Generic indi device entries
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();

    void setupParams();

    void joystickEvent(int joystick_n, double mag, double angle);
    void axisEvent(int axis_n, int value);
    void buttonEvent(int button_n, int value);

    INumberVectorProperty *JoyStickNP;
    INumber *JoyStickN;

    INumberVectorProperty AxisNP;
    INumber *AxisN;

    ISwitchVectorProperty ButtonSP;
    ISwitch *ButtonS;

    ITextVectorProperty PortTP; //  A text vector that stores out physical port name
    IText PortT[1];

    ITextVectorProperty JoystickInfoTP;
    IText JoystickInfoT[5];

    JoyStickDriver *driver;
};

#endif // INDIJOYSTICK_H
