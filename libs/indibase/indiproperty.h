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
#include <utility>
#include "indiutility.h"
#include <memory>

namespace INDI
{
class BaseDevice;

/**
 * \class INDI::Property
   \brief Provides generic container for INDI properties

\author Jasem Mutlaq
*/
class PropertyPrivate;
class Property
{
    DECLARE_PRIVATE(Property)
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
    void setTimeout(double timeout);

public: // Convenience Functions
    const char *getName() const;
    const char *getLabel() const;
    const char *getGroupName() const;
    const char *getDeviceName() const;
    const char *getTimestamp() const;
    IPState getState() const;
    IPerm getPermission() const;

public:
    void save(FILE *fp);

public:
    template <typename ... Args>
    void apply(const char *format = nullptr, Args &&... args);

    template <typename ... Args>
    void define(const char *format = nullptr, Args &&... args);

public:
    INumberVectorProperty *getNumber() const;
    ITextVectorProperty *getText() const;
    ISwitchVectorProperty *getSwitch() const;
    ILightVectorProperty *getLight() const;
    IBLOBVectorProperty *getBLOB() const;

protected:
    std::shared_ptr<PropertyPrivate> d_ptr;
};






template <typename ... Args>
void Property::apply(const char *format, Args &&... args)
{
    switch (getType())
    {
        case INDI_NUMBER: IDSetNumber(getNumber(), format, std::forward<Args>(args)...); break;
        case INDI_TEXT:   IDSetText(getText(),     format, std::forward<Args>(args)...); break;
        case INDI_SWITCH: IDSetSwitch(getSwitch(), format, std::forward<Args>(args)...); break;
        case INDI_LIGHT:  IDSetLight(getLight(),   format, std::forward<Args>(args)...); break;
        case INDI_BLOB:   IDSetBLOB(getBLOB(),     format, std::forward<Args>(args)...); break;
        default:;;
    }
}

template <typename ... Args>
void Property::define(const char *format, Args &&... args)
{
    switch (getType())
    {
        case INDI_NUMBER: IDDefNumber(getNumber(), format, std::forward<Args>(args)...); break;
        case INDI_TEXT:   IDDefText(getText(),     format, std::forward<Args>(args)...); break;
        case INDI_SWITCH: IDDefSwitch(getSwitch(), format, std::forward<Args>(args)...); break;
        case INDI_LIGHT:  IDDefLight(getLight(),   format, std::forward<Args>(args)...); break;
        case INDI_BLOB:   IDDefBLOB(getBLOB(),     format, std::forward<Args>(args)...); break;
        default:;;
    }
}


} // namespace INDI
