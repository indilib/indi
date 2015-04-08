/*******************************************************************************
  Copyright(c) 2011 Jasem Mutlaq. All rights reserved.

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

#ifndef INDI_INDIPROPERTY_H
#define INDI_INDIPROPERTY_H

#include "indibase.h"



namespace INDI
{

class BaseDevice;

class Property
{
public:
    Property();
    ~Property();

    void setProperty(void *);
    void setType(INDI_PROPERTY_TYPE t);
    void setRegistered(bool r);
    void setDynamic(bool d);
    void setBaseDevice(BaseDevice *idp);

    void *getProperty() { return pPtr; }
    INDI_PROPERTY_TYPE getType() { return pType; }
    bool getRegistered() { return pRegistered; }
    bool isDynamic() { return pDynamic; }
    BaseDevice *getBaseDevice() { return dp; }

    // Convenience Functions
    const char *getName();
    const char *getLabel();
    const char *getGroupName();
    const char *getDeviceName();
    IPState getState();
    IPerm getPermission();

    INumberVectorProperty *getNumber();
    ITextVectorProperty   *getText();
    ISwitchVectorProperty *getSwitch();
    ILightVectorProperty  *getLight();
    IBLOBVectorProperty   *getBLOB();

private:
    void *pPtr;
    BaseDevice *dp;
    INDI_PROPERTY_TYPE pType;
    bool pRegistered;
    bool pDynamic;
};

} // namespace INDI

#endif // INDI_INDIPROPERTY_H
