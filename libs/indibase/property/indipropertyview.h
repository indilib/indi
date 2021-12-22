/*
    Copyright (C) 2021 by Pawel Soja <kernel32.pl@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

#include "indiapi.h"
#include "indidevapi.h"
#include "indiwidgettraits.h"

#include "indidriver.h" // IUSaveConfigXXX (indicom.c)

#include "indiwidgetview.h"

#include <string>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <type_traits>

namespace INDI
{

template <typename> struct WidgetView;
template <typename> struct PropertyView;

#define PROPERTYVIEW_BASE_ACCESS public
// don't use direct access to low-level property
//#define PROPERTYVIEW_BASE_ACCESS protected // future

/**
 * \class INDI::PropertyView and INDI::WidgetView
 * \brief Provides decorator for Low-Level IXXXVectorProperty/IXXX
 *
 * INDI::PropertyView
 *
 * A class that will allow a easy transition to the new widget handling interface (future).
 * - Use PropertyView<IText>   instead of ITextVectorProperty
 * - Use PropertyView<INumber> instead of INumberVectorProperty
 * - Use PropertyView<ISwitch> instead of ISwitchVectorProperty
 * - Use PropertyView<ILight>  instead of ILightVectorProperty
 * - Use PropertyView<IBLOB>   instead of IBLOBVectorProperty
 *
 * The PropertyView<IXXX> class is compatible with low-level IXXXVectorProperty structures.
 *
 * INDI::WidgetView
 *
 * A class that will allow a easy transition to the new widget handling interface (future).
 * - Use WidgetView<IText>   instead of IText
 * - Use WidgetView<INumber> instead of INumber
 * - Use WidgetView<ISwitch> instead of ISwitch
 * - Use WidgetView<ILight>  instead of ILight
 * - Use WidgetView<IBLOB>   instead of IBLOB
 *
 * The WidgetView<IXXX> class is compatible with low-level IXXX structures.
 */

template <typename T>
struct PropertyView: PROPERTYVIEW_BASE_ACCESS WidgetTraits<T>::PropertyType
{
    using Type = T;
    using PropertyType = typename WidgetTraits<T>::PropertyType;
    using WidgetType = WidgetView<T>;

    friend class Property;
    friend class PropertyPrivate;
    friend class BaseDevice;
    friend class DefaultDevice;
    template <typename>
    friend struct WidgetView;

    template <typename X, typename Needed>
    using enable_if_is_same_t = typename std::enable_if<std::is_same<X, Needed>::value, bool>::type;
public:
    PropertyView();

    // #PS: do not delete items, they may be on the stack.
    //~PropertyView()                                        { for(auto &p: *this) {p.clear();} free(widget()); }

public:
    void setDeviceName(const char *name);                  /* outside implementation */
    void setDeviceName(const std::string &name);           /* outside implementation */

    void setName(const char *name);                        /* outside implementation */
    void setName(const std::string &name);                 /* outside implementation */

    void setLabel(const char *label);                      /* outside implementation */
    void setLabel(const std::string &label);               /* outside implementation */

    void setGroupName(const char *name);                   /* outside implementation */
    void setGroupName(const std::string &name);            /* outside implementation */

    void setPermission(IPerm permission);                  /* outside implementation */
    void setTimeout(double timeout);                       /* outside implementation */
    void setState(IPState state);

    void setTimestamp(const char *timestamp);              /* outside implementation */
    void setTimestamp(const std::string &timestamp);       /* outside implementation */

    void setAux(void *user);                               /* outside implementation */
    void setWidgets(WidgetType *w, size_t count);          /* outside implementation */

    template <size_t N>
    void setWidgets(WidgetType (&w)[N]);                   /* outside implementation */

public: // only for ISwitch
    void setRule(ISRule rule);                             /* outside implementation */
    bool setRule(const std::string &rule);                 /* outside implementation */

    template <typename X = T, enable_if_is_same_t<X, ISwitch> = true>
    void reset()                                           { IUResetSwitch(this); }

    template <typename X = T, enable_if_is_same_t<X, ISwitch> = true>
    WidgetType *findOnSwitch() const                       { return static_cast<WidgetType *>(IUFindOnSwitch(this)); }

    template <typename X = T, enable_if_is_same_t<X, ISwitch> = true>
    int findOnSwitchIndex() const                          { return IUFindOnSwitchIndex(this); }

public: // only for INumber
    template <typename X = T, enable_if_is_same_t<X, INumber> = true>
    void updateMinMax();                                   /* outside implementation - only driver side, see indipropertyview_driver.cpp */

public: //getters
    const char *getDeviceName() const                      { return this->device; }
    const char *getName()       const                      { return this->name; }
    const char *getLabel()      const                      { return this->label; }
    const char *getGroupName()  const                      { return this->group; }

    IPerm getPermission()       const;                     /* outside implementation */
    const char *getPermissionAsString() const              { return permStr(getPermission()); }

    ISRule getRule()            const;                     /* outside implementation */
    const char * getRuleAsString() const                   { return ruleStr(getRule()); }

    double getTimeout()         const;                     /* outside implementation */
    IPState getState()          const                      { return this->s; }
    const char *getStateAsString() const                   { return pstateStr(getState()); }

    const char *getTimestamp()  const                      { return this->timestamp; }
    void *getAux()              const                      { return this->aux; }

    int count() const;                                     /* outside implementation */
    WidgetType *widget() const;                            /* outside implementation */

    WidgetType *findWidgetByName(const char *name) const;  /* outside implementation */

public: //tests
    bool isEmpty() const                                   { return widget() == nullptr || count() == 0; }

    bool isNameMatch(const char *otherName) const          { return !strcmp(getName(), otherName); }
    bool isNameMatch(const std::string &otherName) const   { return getName() == otherName; }

    bool isLabelMatch(const char *otherLabel) const        { return !strcmp(getLabel(), otherLabel); }
    bool isLabelMatch(const std::string &otherLabel) const { return getLabel() == otherLabel; }

public: // only driver side
    void save(FILE *f) const;                              /* outside implementation */

    void vapply(const char *format, va_list args) const;   /* outside implementation - only driver side, see indipropertyview_driver.cpp */
    void vdefine(const char *format, va_list args) const;  /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    void apply(const char *format, ...) const ATTRIBUTE_FORMAT_PRINTF(2, 3);  /* outside implementation - only driver side, see indipropertyview_driver.cpp */
    void define(const char *format, ...) const ATTRIBUTE_FORMAT_PRINTF(2, 3); /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    void apply() const                                     { apply(nullptr);  }
    void define() const                                    { define(nullptr); }

public:
    template <typename X = T, enable_if_is_same_t<X, IText> = true>
    void fill(
        const char *device, const char *name, const char *label, const char *group,
        IPerm permission, double timeout, IPState state
    ); /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    template <typename X = T, enable_if_is_same_t<X, INumber> = true>
    void fill(
        const char *device, const char *name, const char *label, const char *group,
        IPerm permission, double timeout, IPState state
    ); /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    template <typename X = T, enable_if_is_same_t<X, ISwitch> = true>
    void fill(
        const char *device, const char *name, const char *label, const char *group,
        IPerm permission, ISRule rule, double timeout, IPState state
    ); /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    template <typename X = T, enable_if_is_same_t<X, ILight> = true>
    void fill(
        const char *device, const char *name, const char *label, const char *group,
        IPState state
    ); /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    template <typename X = T, enable_if_is_same_t<X, IBLOB> = true>
    void fill(
        const char *device, const char *name, const char *label, const char *group,
        IPerm permission, double timeout, IPState state
    ); /* outside implementation - only driver side, see indipropertyview_driver.cpp */

public:
    template <typename X = T, enable_if_is_same_t<X, IText> = true>
    bool update(const char * const texts[], const char * const names[], int n);
    /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    template <typename X = T, enable_if_is_same_t<X, INumber> = true>
    bool update(const double values[], const char * const names[], int n);
    /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    template <typename X = T, enable_if_is_same_t<X, ISwitch> = true>
    bool update(const ISState states[], const char * const names[], int n);
    /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    /*
    template <typename X = T, enable_if_is_same_t<X, ILight> = true>
    bool update(..., const char * const names[], int n);
    */

    template <typename X = T, enable_if_is_same_t<X, IBLOB> = true>
    bool update(
        const int sizes[], const int blobsizes[], const char * const blobs[], const char * const formats[],
        const char * const names[], int n
    ); /* outside implementation - only driver side, see indipropertyview_driver.cpp */


public:
    WidgetType *begin() const                              { return widget();           }
    WidgetType *end()   const                              { return widget() + count(); }

    WidgetType *at(size_t index) const                     { return widget() + index;   }
public:
    void clear()
    {
        for(auto &p: *this) { p.clear(); }
        //free(widget()); // #PS: do not delete items, they may be on the stack.
        memset(this, 0, sizeof(*this));
    }
};

template <typename>
struct WidgetView;

template <>
struct WidgetView<IText>: PROPERTYVIEW_BASE_ACCESS IText
{
    using Type = IText;
    template <typename> friend struct PropertyView;

public:
    WidgetView()                                           { memset(this, 0, sizeof(*this)); }
    WidgetView(const WidgetView &other): Type(other)       { this->text = nullptr; setText(other.text); }
    WidgetView(WidgetView &&other): Type(other)            { memset(static_cast<Type*>(&other), 0, sizeof(other)); }
    WidgetView &operator=(const WidgetView &other)         { return *this = WidgetView(other); }
    WidgetView &operator=(WidgetView &&other)              { std::swap(static_cast<Type&>(other), static_cast<Type&>(*this)); return *this; }
    ~WidgetView()                                          { free(this->text); }
    void clear()                                           { free(this->text); memset(this, 0, sizeof(*this)); }
    // bool isNull() const                                    { return reinterpret_cast<const void*>(this) == nullptr; }

public: // setters
    void setParent(ITextVectorProperty *parent)            { this->tvp = parent; }
    void setParent(PropertyView<Type> *parent)             { this->tvp = static_cast<ITextVectorProperty*>(parent); }

    void setName(const char *name)                         { strncpy(this->name, name, MAXINDINAME); }
    void setName(const std::string &name)                  { setName(name.data()); }

    void setLabel(const char *label)                       { strncpy(this->label, label, MAXINDILABEL); }
    void setLabel(const std::string &label)                { setLabel(label.data()); }

    //void setText(const char *text)                         { free(this->text); this->text = strndup(text, strlen(text)); }
    void setText(const char *text, size_t size)            { this->text = strncpy(static_cast<char*>(realloc(this->text, size + 1)), text, size); this->text[size] = '\0'; }
    void setText(const char *text)                         { setText(text, strlen(text)); }
    void setText(const std::string &text)                  { setText(text.data(), text.size()); }

    void setAux(void *user)                                { this->aux0 = user; }
    // don't use any other aux!

public: //getters
    const char *getName()  const                           { return this->name; }
    const char *getLabel() const                           { return this->label; }
    const char *getText()  const                           { return this->text; }

    void *getAux() const                                   { return this->aux0; }

public: //tests
    bool isNameMatch(const char *otherName) const          { return !strcmp(getName(), otherName); }
    bool isNameMatch(const std::string &otherName) const   { return getName() == otherName; }

    bool isLabelMatch(const char *otherLabel) const        { return !strcmp(getLabel(), otherLabel); }
    bool isLabelMatch(const std::string &otherLabel) const { return getLabel() == otherLabel; }

public:
    void fill(const char *name, const char *label, const char *initialText)
    ; /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    void fill(const std::string &name, const std::string &label, const std::string &initialText)
    { fill(name.c_str(), label.c_str(), initialText.c_str()); }
};

template <>
struct WidgetView<INumber>: PROPERTYVIEW_BASE_ACCESS INumber
{
    using Type = INumber;
    template <typename> friend struct PropertyView;

public:
    WidgetView()                                           { memset(this, 0, sizeof(*this)); }
    WidgetView(const WidgetView &other): Type(other)       { }
    WidgetView(WidgetView &&other): Type(other)            { memset(static_cast<Type*>(&other), 0, sizeof(other)); }
    WidgetView &operator=(const WidgetView &other)         { return *this = WidgetView(other); }
    WidgetView &operator=(WidgetView &&other)              { std::swap(static_cast<Type&>(other), static_cast<Type&>(*this)); return *this; }
    ~WidgetView()                                          { }
    void clear()                                           { memset(this, 0, sizeof(*this)); }
    // bool isNull() const                                    { return reinterpret_cast<const void*>(this) == nullptr; }

public: // setters
    void setParent(INumberVectorProperty *parent)          { this->nvp = parent; }
    void setParent(PropertyView<Type> *parent)             { this->nvp = static_cast<INumberVectorProperty*>(parent); }

    void setName(const char *name)                         { strncpy(this->name, name, MAXINDINAME); }
    void setName(const std::string &name)                  { setName(name.data()); }

    void setLabel(const char *label)                       { strncpy(this->label, label, MAXINDILABEL); }
    void setLabel(const std::string &label)                { setLabel(label.data()); }

    void setFormat(const char *format)                     { strncpy(this->format, format, MAXINDIFORMAT); }
    void setFormat(const std::string &format)              { setLabel(format.data()); }

    void setMin(double min)                                { this->min = min; }
    void setMax(double max)                                { this->max = max; }
    void setMinMax(double min, double max)                 { setMin(min); setMax(max); }
    void setStep(double step)                              { this->step = step; }
    void setValue(double value)                            { this->value = value; }

    void setAux(void *user)                                { this->aux0 = user; }
    // don't use any other aux!

public: //getters
    const char *getName()   const                          { return this->name; }
    const char *getLabel()  const                          { return this->label; }
    const char *getFormat() const                          { return this->format; }

    double getMin()   const                                { return this->min; }
    double getMax()   const                                { return this->max; }
    double getStep()  const                                { return this->step; }
    double getValue() const                                { return this->value; }

    void *getAux() const                                   { return this->aux0; }

public: //tests
    bool isNameMatch(const char *otherName) const          { return !strcmp(getName(), otherName); }
    bool isNameMatch(const std::string &otherName) const   { return getName() == otherName; }

    bool isLabelMatch(const char *otherLabel) const        { return !strcmp(getLabel(), otherLabel); }
    bool isLabelMatch(const std::string &otherLabel) const { return getLabel() == otherLabel; }

public:
    void fill(const char *name, const char *label, const char *format,
              double min, double max, double step, double value)
    ; /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    void fill(const std::string &name, const std::string &label, const std::string &format,
              double min, double max, double step, double value)
    { fill(name.c_str(), label.c_str(), format.c_str(), min, max, step, value); }
};

template <>
struct WidgetView<ISwitch>: PROPERTYVIEW_BASE_ACCESS ISwitch
{
    using Type = ISwitch;
    template <typename> friend struct PropertyView;

public:
    WidgetView()                                           { memset(this, 0, sizeof(*this)); }
    WidgetView(const WidgetView &other): Type(other)       { }
    WidgetView(WidgetView &&other): Type(other)            { memset(static_cast<Type*>(&other), 0, sizeof(other)); }
    WidgetView &operator=(const WidgetView &other)         { return *this = WidgetView(other); }
    WidgetView &operator=(WidgetView &&other)              { std::swap(static_cast<Type&>(other), static_cast<Type&>(*this)); return *this; }
    ~WidgetView()                                          { }
    void clear()                                           { memset(this, 0, sizeof(*this)); }
    // bool isNull() const                                    { return reinterpret_cast<const void*>(this) == nullptr; }

public: // setters
    void setParent(ISwitchVectorProperty *parent)          { this->svp = parent; }
    void setParent(PropertyView<Type> *parent)             { this->svp = static_cast<ISwitchVectorProperty*>(parent); }

    void setName(const char *name)                         { strncpy(this->name, name, MAXINDINAME); }
    void setName(const std::string &name)                  { setName(name.data()); }

    void setLabel(const char *label)                       { strncpy(this->label, label, MAXINDILABEL); }
    void setLabel(const std::string &label)                { setLabel(label.data()); }

    void setState(const ISState &state)                    { this->s = state; }
    bool setState(const std::string &state)                { return crackISState(state.data(), &this->s) == 0; }

    void setAux(void *user)                                { this->aux = user; }
    // don't use any other aux!

public: //getters
    const char *getName()   const                          { return this->name; }
    const char *getLabel()  const                          { return this->label; }

    ISState getState() const                               { return this->s; }
    const char *getStateAsString() const                   { return sstateStr(getState()); }

    void *getAux() const                                   { return this->aux; }


public: //tests
    bool isNameMatch(const char *otherName) const          { return !strcmp(getName(), otherName); }
    bool isNameMatch(const std::string &otherName) const   { return getName() == otherName; }

    bool isLabelMatch(const char *otherLabel) const        { return !strcmp(getLabel(), otherLabel); }
    bool isLabelMatch(const std::string &otherLabel) const { return getLabel() == otherLabel; }

public:
    void fill(const char *name, const char *label, ISState state = ISS_OFF)
    ; /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    void fill(const std::string &name, const std::string &label, ISState state = ISS_OFF)
    { fill(name.c_str(), label.c_str(), state); }
};

template <>
struct WidgetView<ILight>: PROPERTYVIEW_BASE_ACCESS ILight
{
    using Type = ILight;
    template <typename> friend struct PropertyView;

public:
    WidgetView()                                           { memset(this, 0, sizeof(*this)); }
    WidgetView(const WidgetView &other): Type(other)       { }
    WidgetView(WidgetView &&other): Type(other)            { memset(static_cast<Type*>(&other), 0, sizeof(other)); }
    WidgetView &operator=(const WidgetView &other)         { return *this = WidgetView(other); }
    WidgetView &operator=(WidgetView &&other)              { std::swap(static_cast<Type&>(other), static_cast<Type&>(*this)); return *this; }
    ~WidgetView()                                          { }
    void clear()                                           { memset(this, 0, sizeof(*this)); }
    // bool isNull() const                                    { return reinterpret_cast<const void*>(this) == nullptr; }

public: // setters
    void setParent(ILightVectorProperty *parent)           { this->lvp = parent; }
    void setParent(PropertyView<Type> *parent)             { this->lvp = static_cast<ILightVectorProperty*>(parent); }

    void setName(const char *name)                         { strncpy(this->name, name, MAXINDINAME); }
    void setName(const std::string &name)                  { setName(name.data()); }

    void setLabel(const char *label)                       { strncpy(this->label, label, MAXINDILABEL); }
    void setLabel(const std::string &label)                { setLabel(label.data()); }

    void setState(const IPState &state)                    { this->s = state; }
    bool setState(const std::string &state)                { return crackIPState(state.data(), &this->s) == 0; }

    void setAux(void *user)                                { this->aux = user; }
    // don't use any other aux!

public: //getters
    const char *getName()   const                          { return this->name; }
    const char *getLabel()  const                          { return this->label; }

    IPState getState() const                               { return this->s; }
    const char *getStateAsString() const                   { return pstateStr(getState()); }

    void *getAux() const                                   { return this->aux; }


public: //tests
    bool isNameMatch(const char *otherName) const          { return !strcmp(getName(), otherName); }
    bool isNameMatch(const std::string &otherName) const   { return getName() == otherName; }

    bool isLabelMatch(const char *otherLabel) const        { return !strcmp(getLabel(), otherLabel); }
    bool isLabelMatch(const std::string &otherLabel) const { return getLabel() == otherLabel; }

public:
    void fill(const char *name, const char *label, IPState state = IPS_OK)
    ; /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    void fill(const std::string &name, const std::string &&label, IPState state = IPS_OK)
    { fill(name.c_str(), label.c_str(), state); }
};

template <>
struct WidgetView<IBLOB>: PROPERTYVIEW_BASE_ACCESS IBLOB
{
    using Type = IBLOB;
    template <typename> friend struct PropertyView;

public:
    WidgetView()                                           { memset(this, 0, sizeof(*this)); }
    WidgetView(const WidgetView &other): Type(other)       { }
    WidgetView(WidgetView &&other): Type(other)            { memset(static_cast<Type*>(&other), 0, sizeof(other)); }
    WidgetView &operator=(const WidgetView &other)         { return *this = WidgetView(other); }
    WidgetView &operator=(WidgetView &&other)              { std::swap(static_cast<Type&>(other), static_cast<Type&>(*this)); return *this; }
    ~WidgetView()                                          { /* free(this->blob); */ }
    void clear()                                           { /* free(this->blob); */ memset(this, 0, sizeof(*this)); }
    // bool isNull() const                                    { return reinterpret_cast<const void*>(this) == nullptr; }

public: // setters
    void setParent(IBLOBVectorProperty *parent)            { this->bvp = parent; }
    void setParent(PropertyView<Type> *parent)             { this->bvp = static_cast<IBLOBVectorProperty*>(parent); }

    void setName(const char *name)                         { strncpy(this->name, name, MAXINDINAME); }
    void setName(const std::string &name)                  { setName(name.data()); }

    void setLabel(const char *label)                       { strncpy(this->label, label, MAXINDILABEL); }
    void setLabel(const std::string &label)                { setLabel(label.data()); }

    void setFormat(const char *format)                     { strncpy(this->format, format, MAXINDIBLOBFMT); }
    void setFormat(const std::string &format)              { setLabel(format.data()); }

    void setBlob(void *blob)                               { this->blob = blob; }
    void setBlobLen(int size)                              { this->bloblen = size; }
    void setSize(int size)                                 { this->size = size; }

    void setAux(void *user)                                { this->aux0 = user; }
    // don't use any other aux!

public: //getters
    const char *getName()   const                          { return this->name; }
    const char *getLabel()  const                          { return this->label; }
    const char *getFormat() const                          { return this->format; }

    const void *getBlob()   const                          { return this->blob; }
    int getBlobLen() const                                 { return this->bloblen; }
    int getSize() const                                    { return this->size; }

    void *getAux() const                                   { return this->aux0; }

public: //tests
    bool isNameMatch(const char *otherName) const          { return !strcmp(getName(), otherName); }
    bool isNameMatch(const std::string &otherName) const   { return getName() == otherName; }

    bool isLabelMatch(const char *otherLabel) const        { return !strcmp(getLabel(), otherLabel); }
    bool isLabelMatch(const std::string &otherLabel) const { return getLabel() == otherLabel; }

public:
    void fill(const char *name, const char *label, const char *format)
    ; /* outside implementation - only driver side, see indipropertyview_driver.cpp */

    void fill(const std::string &name, const std::string &label, const std::string &format)
    { fill(name.c_str(), label.c_str(), format.c_str()); }
};




/* outside implementation */
template <typename T>
inline PropertyView<T>::PropertyView()
{ memset(this, 0, sizeof(*this)); }

template <typename T>
inline void PropertyView<T>::setDeviceName(const char *name)
{ strncpy(this->device, name, MAXINDIDEVICE); }

template <typename T>
inline void PropertyView<T>::setDeviceName(const std::string &name)
{ setDeviceName(name.data()); }

template <typename T>
inline void PropertyView<T>::setName(const char *name)
{ strncpy(this->name, name, MAXINDINAME); }

template <typename T>
inline void PropertyView<T>::setName(const std::string &name)
{ setName(name.data()); }

template <typename T>
inline void PropertyView<T>::setLabel(const char *label)
{ strncpy(this->label, label, MAXINDILABEL); }

template <typename T>
inline void PropertyView<T>::setLabel(const std::string &label)
{ setLabel(label.data()); }

template <typename T>
inline void PropertyView<T>::setGroupName(const char *name)
{ strncpy(this->group, name, MAXINDIGROUP); }

template <typename T>
inline void PropertyView<T>::setGroupName(const std::string &name)
{ setGroupName(name.data()); }

template <typename T>
inline void PropertyView<T>::setState(IPState state)
{ this->s = state; }

template <typename T>
inline void PropertyView<T>::setTimestamp(const char *timestamp)
{ strncpy(this->timestamp, timestamp, MAXINDITSTAMP); }

template <typename T>
inline void PropertyView<T>::setTimestamp(const std::string &timestamp)
{ setTimestamp(timestamp.data()); }

template <typename T>
template <size_t N>
inline void PropertyView<T>::setWidgets(WidgetType (&w)[N])
{ setWidgets(static_cast<WidgetType*>(w), N); }

template <typename T>
inline void PropertyView<T>::setAux(void *user)
{ this->aux = user; }

template <>
inline void PropertyView<IText>::save(FILE *f) const
{ IUSaveConfigText(f, this); }

template <>
inline void PropertyView<INumber>::save(FILE *f) const
{ IUSaveConfigNumber(f, this); }

template <>
inline void PropertyView<ISwitch>::save(FILE *f) const
{ IUSaveConfigSwitch(f, this); }

template <>
inline void PropertyView<ILight>::save(FILE *f) const
{ (void)f; /* IUSaveConfigLight(f, this); */ }

template <>
inline void PropertyView<IBLOB>::save(FILE *f) const
{ IUSaveConfigBLOB(f, this); }

template <typename T>
inline void PropertyView<T>::setTimeout(double timeout)
{ this->timeout = timeout; }

template <>
inline void PropertyView<ILight>::setTimeout(double)
{ }

template <typename T>
inline void PropertyView<T>::setPermission(IPerm permission)
{ this->p = permission; }

template <>
inline void PropertyView<ILight>::setPermission(IPerm)
{ }

template <typename T>
inline void PropertyView<T>::setRule(ISRule)
{ }

template <>
inline void PropertyView<ISwitch>::setRule(ISRule rule)
{ this->r = rule; }

template <typename T>
inline bool PropertyView<T>::setRule(const std::string &)
{ return false; }

template <>
inline bool PropertyView<ISwitch>::setRule(const std::string &rule)
{ return crackISRule(rule.data(), &this->r) == 0; }

template <typename T>
inline WidgetView<T> *PropertyView<T>::findWidgetByName(const char *) const
{ return nullptr; }

template <>
inline WidgetView<IText> *PropertyView<IText>::findWidgetByName(const char *name) const
{ return static_cast<WidgetView<IText> *>(IUFindText(this, name)); }

template <>
inline WidgetView<INumber> *PropertyView<INumber>::findWidgetByName(const char *name) const
{ return static_cast<WidgetView<INumber> *>(IUFindNumber(this, name)); }

template <>
inline WidgetView<ISwitch> *PropertyView<ISwitch>::findWidgetByName(const char *name) const
{ return static_cast<WidgetView<ISwitch> *>(IUFindSwitch(this, name)); }

template <>
inline WidgetView<ILight> *PropertyView<ILight>::findWidgetByName(const char *name) const
{ return static_cast<WidgetView<ILight> *>(IUFindLight(this, name)); }

template <>
inline WidgetView<IBLOB> *PropertyView<IBLOB>::findWidgetByName(const char *name) const
{ return static_cast<WidgetView<IBLOB> *>(IUFindBLOB(this, name)); }

template <typename T>
inline IPerm PropertyView<T>::getPermission() const
{ return this->p; }

template <>
inline IPerm PropertyView<ILight>::getPermission() const
{ return IP_RO; }

template <typename T>
inline ISRule PropertyView<T>::getRule() const
{ return ISR_NOFMANY; }

template <>
inline ISRule PropertyView<ISwitch>::getRule() const
{ return this->r; }

template <typename T>
inline double PropertyView<T>::getTimeout() const
{ return this->timeout; }

template <>
inline double PropertyView<ILight>::getTimeout() const
{ return 0; }

template <>
inline void PropertyView<IText>::setWidgets(WidgetType *w, size_t size)
{ this->tp = w; this->ntp = size; }

template <>
inline void PropertyView<INumber>::setWidgets(WidgetType *w, size_t size)
{ this->np = w; this->nnp = size; }

template <>
inline void PropertyView<ISwitch>::setWidgets(WidgetType *w, size_t size)
{ this->sp = w; this->nsp = size; }

template <>
inline void PropertyView<ILight>::setWidgets(WidgetType *w, size_t size)
{ this->lp = w; this->nlp = size; }

template <>
inline void PropertyView<IBLOB>::setWidgets(WidgetType *w, size_t size)
{ this->bp = w; this->nbp = size; }

template <>
inline int PropertyView<IText>::count() const
{ return this->ntp; }

template <>
inline int PropertyView<INumber>::count() const
{ return this->nnp; }

template <>
inline int PropertyView<ISwitch>::count() const
{ return this->nsp; }

template <>
inline int PropertyView<ILight>::count() const
{ return this->nlp; }

template <>
inline int PropertyView<IBLOB>::count() const
{ return this->nbp; }

template <>
inline PropertyView<IText>::WidgetType *PropertyView<IText>::widget() const
{ return static_cast<WidgetType*>(this->tp); }

template <>
inline PropertyView<INumber>::WidgetType *PropertyView<INumber>::widget() const
{ return static_cast<WidgetType*>(this->np); }

template <>
inline PropertyView<ISwitch>::WidgetType *PropertyView<ISwitch>::widget() const
{ return static_cast<WidgetType*>(this->sp); }

template <>
inline PropertyView<ILight>::WidgetType *PropertyView<ILight>::widget() const
{ return static_cast<WidgetType*>(this->lp); }

template <>
inline PropertyView<IBLOB>::WidgetType *PropertyView<IBLOB>::widget() const
{ return static_cast<WidgetType*>(this->bp); }

}
