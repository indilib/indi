/*******************************************************************************
  Copyright (C) 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <tr1/functional>

#include <indiapi.h>
#include <defaultdevice.h>

namespace  INDI
{

class Controller
{
    public:

    typedef enum { CONTROLLER_JOYSTICK, CONTROLLER_AXIS, CONTROLLER_BUTTON, CONTROLLER_UNKNOWN } ControllerType;

    typedef std::tr1::function<void (const char * joystick_n, double mag, double angle)> joystickFunc;
    typedef std::tr1::function<void (const char * axis_n, double value)> axisFunc;
    typedef std::tr1::function<void (const char * button_n, ISState state)> buttonFunc;

    Controller(INDI::DefaultDevice *cdevice);
    virtual ~Controller();

    virtual void ISGetProperties(const char *dev);
    virtual bool initProperties();
    virtual bool updateProperties();
    virtual bool ISSnoopDevice(XMLEle *root);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool saveConfigItems(FILE *fp);

    void mapController(const char *propertyName, const char *propertyLabel, ControllerType type, const char *initialValue);

    void setJoystickCallback(joystickFunc joystickCallback);
    void setAxisCallback(axisFunc axisCallback);
    void setButtonCallback(buttonFunc buttonCallback);

    ControllerType getControllerType(const char *name);
    const char *getControllerSetting(const char *name);

    protected:

    static void joystickEvent(const char * joystick_n, double mag, double angle);
    static void axisEvent(const char * axis_n, int value);
    static void buttonEvent(const char * button_n, int value);

    void enableJoystick();
    void disableJoystick();


    joystickFunc joystickCallbackFunc;
    buttonFunc buttonCallbackFunc;
    axisFunc axisCallbackFunc;


    INDI::DefaultDevice *device;

    private:

    /* Joystick Support */
    ISwitchVectorProperty UseJoystickSP;
    ISwitch UseJoystickS[2];

    ITextVectorProperty JoystickSettingTP;
    IText *JoystickSettingT;


};


}

#endif /* CONTROLLER_H */
