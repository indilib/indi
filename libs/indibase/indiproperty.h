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

#pragma once

#include "indibase.h"

namespace INDI
{
class BaseDevice;

/**
 * \class INDI::Property
   \brief Provides generic container for INDI properties

\author Jasem Mutlaq
*/
class Property
{
public:
    Property();
    Property(void *property, INDI_PROPERTY_TYPE type);
    Property(INumberVectorProperty *property);
    Property(ITextVectorProperty   *property);
    Property(ISwitchVectorProperty *property);
    Property(ILightVectorProperty  *property);
    Property(IBLOBVectorProperty   *property);

    ~Property();

public:
    void setProperty(void *);
    void setType(INDI_PROPERTY_TYPE t);
    void setRegistered(bool r);
    void setDynamic(bool d);
    void setBaseDevice(BaseDevice *idp);

public:
    void *getProperty() const;
    INDI_PROPERTY_TYPE getType() const;
    bool getRegistered() const;
    bool isDynamic() const;
    BaseDevice *getBaseDevice() const;

public: // Convenience Functions
    void setName(const char *name);
    void setLabel(const char *label);
    void setGroupName(const char *groupName);
    void setDeviceName(const char *deviceName);
    void setTimestamp(const char *timestamp);
    void setState(IPState state);
    void setPermission(IPerm permission);

public: // Convenience Functions
    const char *getName() const;
    const char *getLabel() const;
    const char *getGroupName() const;
    const char *getDeviceName() const;
    const char *getTimestamp() const;
    IPState getState() const;
    IPerm getPermission() const;

public:
    INumberVectorProperty *getNumber() const;
    ITextVectorProperty *getText() const;
    ISwitchVectorProperty *getSwitch() const;
    ILightVectorProperty *getLight() const;
    IBLOBVectorProperty *getBLOB() const;

private:
    void *pPtr = nullptr;
    BaseDevice *dp = nullptr;
    INDI_PROPERTY_TYPE pType = INDI_UNKNOWN;
    bool pRegistered = false;
    bool pDynamic = false;
};

} // namespace INDI
