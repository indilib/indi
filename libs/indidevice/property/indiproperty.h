/*******************************************************************************
  Copyright(c) 2011 Jasem Mutlaq. All rights reserved.
               2022 Pawel Soja <kernel32.pl@gmail.com>

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

#include <memory>
#include <cstdarg>
#include <functional>

#define INDI_PROPERTY_BACKWARD_COMPATIBILE
namespace INDI
{
class BaseDevice;
class PropertyNumber;
class PropertyText;
class PropertySwitch;
class PropertyLight;
class PropertyBlob;
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
        ~Property();

    public:
        Property(INDI::PropertyNumber property);
        Property(INDI::PropertyText   property);
        Property(INDI::PropertySwitch property);
        Property(INDI::PropertyLight  property);
        Property(INDI::PropertyBlob   property);

#ifdef INDI_PROPERTY_BACKWARD_COMPATIBILE
    public:
        Property(INumberVectorProperty *property);
        Property(ITextVectorProperty   *property);
        Property(ISwitchVectorProperty *property);
        Property(ILightVectorProperty  *property);
        Property(IBLOBVectorProperty   *property);

    public:
        Property(INDI::PropertyViewNumber *property);
        Property(INDI::PropertyViewText   *property);
        Property(INDI::PropertyViewSwitch *property);
        Property(INDI::PropertyViewLight  *property);
        Property(INDI::PropertyViewBlob   *property);

#endif
    public:
        void setProperty(void *);
        void setType(INDI_PROPERTY_TYPE t);
        void setRegistered(bool r);
        void setDynamic(bool d);

        INDI_DEPRECATED("Use setBaseDevice(BaseDevice).")
        void setBaseDevice(BaseDevice *idp);

        void setBaseDevice(BaseDevice device);

    public:
        void *getProperty() const;
        INDI_PROPERTY_TYPE getType() const;
        const char *getTypeAsString() const;
        bool getRegistered() const;
        bool isDynamic() const;
        BaseDevice getBaseDevice() const;

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
        void onUpdate(const std::function<void()> &callback);

    public:
        void emitUpdate();
        bool hasUpdateCallback() const;

    public:
        void save(FILE *fp) const;

    public:
        void apply(const char *format, ...) const ATTRIBUTE_FORMAT_PRINTF(2, 3);
        void define(const char *format, ...) const ATTRIBUTE_FORMAT_PRINTF(2, 3);

        void apply() const
        {
            apply(nullptr);
        }
        void define() const
        {
            define(nullptr);
        }

    public:
#ifdef INDI_PROPERTY_BACKWARD_COMPATIBILE
        INDI::PropertyViewNumber *getNumber() const;
        INDI::PropertyViewText   *getText() const;
        INDI::PropertyViewSwitch *getSwitch() const;
        INDI::PropertyViewLight  *getLight() const;
        INDI::PropertyViewBlob   *getBLOB() const;
#endif

    public:
#ifdef INDI_PROPERTY_BACKWARD_COMPATIBILE
        INDI_DEPRECATED("Do not use INDI::Property as pointer.")
        INDI::Property* operator->();

        INDI_DEPRECATED("Do not use INDI::Property as pointer.")
        const INDI::Property* operator->() const;

        INDI_DEPRECATED("Do not use INDI::Property as pointer.")
        operator INDI::Property *();

        INDI_DEPRECATED("Do not use INDI::Property as pointer.")
        operator const INDI::Property *() const;

        INDI_DEPRECATED("Do not use INDI::Property as pointer.")
        operator INDI::PropertyViewNumber *() const { return getNumber(); }

        INDI_DEPRECATED("Do not use INDI::Property as pointer.")
        operator INDI::PropertyViewText   *() const { return getText(); }

        INDI_DEPRECATED("Do not use INDI::Property as pointer.")
        operator INDI::PropertyViewSwitch *() const { return getSwitch(); }

        INDI_DEPRECATED("Do not use INDI::Property as pointer.")
        operator INDI::PropertyViewLight  *() const { return getLight(); }

        INDI_DEPRECATED("Do not use INDI::Property as pointer.")
        operator INDI::PropertyViewBlob   *() const { return getBLOB(); }

        INDI_DEPRECATED("Use comparison to true.")
        bool operator != (std::nullptr_t) const     { return  isValid(); }

        INDI_DEPRECATED("Use comparison to false.")
        bool operator == (std::nullptr_t) const     { return !isValid(); }

        operator bool()                   const     { return  isValid(); }
        operator bool()                             { return  isValid(); }
#endif

    protected:
        std::shared_ptr<PropertyPrivate> d_ptr;
        Property(const std::shared_ptr<PropertyPrivate> &dd);
        Property(PropertyPrivate &dd);
        friend class PropertyNumber;
        friend class PropertyText;
        friend class PropertySwitch;
        friend class PropertyLight;
        friend class PropertyBlob;

    protected:
        friend class BaseDevicePrivate;
        INDI::Property *self();
};

} // namespace INDI

#ifdef QT_CORE_LIB
#include <QMetaType>
Q_DECLARE_METATYPE(INDI::Property)
static int indi_property_metatype_id = QMetaTypeId< INDI::Property >::qt_metatype_id();
#endif
