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
#include "indiutility.h"

#include "indipropertyview.h"

#include "indidriver.h"

#include <memory>
#include <cstdarg>

#define INDI_PROPERTY_BACKWARD_COMPATIBILE
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
    const char *getTypeAsString() const;
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
    const char *getStateAsString() const;
    IPerm getPermission() const;

public:
    bool isEmpty() const;
    bool isValid() const;

    bool isNameMatch(const char *otherName) const;
    bool isNameMatch(const std::string &otherName) const;

    bool isLabelMatch(const char *otherLabel) const;
    bool isLabelMatch(const std::string &otherLabel) const;

public:
    void save(FILE *fp) const;

public:
    void apply(const char *format, ...) const ATTRIBUTE_FORMAT_PRINTF(2, 3);
    void define(const char *format, ...) const ATTRIBUTE_FORMAT_PRINTF(2, 3);

    void apply() const  { apply(nullptr);  }
    void define() const { define(nullptr); }

public:
    INDI::PropertyView<INumber> *getNumber() const;
    INDI::PropertyView<IText>   *getText() const;
    INDI::PropertyView<ISwitch> *getSwitch() const;
    INDI::PropertyView<ILight>  *getLight() const;
    INDI::PropertyView<IBLOB>   *getBLOB() const;

public:
#ifdef INDI_PROPERTY_BACKWARD_COMPATIBILE
    INDI::Property* operator->();
    const INDI::Property* operator->() const;

    operator INDI::Property *();
    operator const INDI::Property *() const;
#endif

protected:
    std::shared_ptr<PropertyPrivate> d_ptr;
    Property(std::shared_ptr<PropertyPrivate> dd);
    Property(PropertyPrivate &dd);
};

} // namespace INDI

#ifdef QT_CORE_LIB
#include <QMetaType>
Q_DECLARE_METATYPE(INDI::Property)
static int indi_property_metatype_id = QMetaTypeId< INDI::Property >::qt_metatype_id();
#endif
