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

#include <string>
#include <cstring>

namespace INDI
{

/**
 * \class INDI::WidgetView
 * \brief Provides decorator for Low-Level INDI widgets
 */
template <typename>
struct WidgetView;

template <>
struct WidgetView<IText>: public IText
{
    using Type = IText;

public:
    WidgetView()                                  { memset(this, 0, sizeof(*this)); }
    ~WidgetView()                                 { free(this->text); }
    void clear()                                  { free(this->text); memset(this, 0, sizeof(*this)); }

public: // setters
    void setParent(ITextVectorProperty *parent)   { this->tvp = parent; }

    void setName(const char *name)                { strncpy(this->name, name, MAXINDINAME); }
    void setName(const std::string &name)         { setName(name.data()); }

    void setLabel(const char *label)              { strncpy(this->label, label, MAXINDILABEL); }
    void setLabel(const std::string &label)       { setLabel(label.data()); }

    void setText(const char *text)                { free(this->text); this->text = strndup(text, strlen(text)); }
    void setText(const std::string &text)         { setText(text.data()); }

    void setAux(void *user)                       { this->aux0 = user; }
    // don't use any other aux!

public: //getters
    const char *getName()  const                  { return this->name; }
    const char *getLabel() const                  { return this->label; }
    const char *getText()  const                  { return this->text; }

    void *getAux() const                          { return this->aux0; }
};

template <>
struct WidgetView<INumber>: public INumber
{
    using Type = INumber;

public:
    WidgetView()                                  { memset(this, 0, sizeof(*this)); }
    ~WidgetView()                                 { }
    void clear()                                  { memset(this, 0, sizeof(*this)); }

public: // setters
    void setParent(INumberVectorProperty *parent) { this->nvp = parent; }

    void setName(const char *name)                { strncpy(this->name, name, MAXINDINAME); }
    void setName(const std::string &name)         { setName(name.data()); }

    void setLabel(const char *label)              { strncpy(this->label, label, MAXINDILABEL); }
    void setLabel(const std::string &label)       { setLabel(label.data()); }

    void setFormat(const char *format)            { strncpy(this->format, format, MAXINDIFORMAT); }
    void setFormat(const std::string &format)     { setLabel(format.data()); }

    void setMin(double min)                       { this->min = min; }
    void setMax(double max)                       { this->max = max; }
    void setStep(double step)                     { this->step = step; }
    void setValue(double value)                   { this->value = value; }

    void setAux(void *user)                       { this->aux0 = user; }
    // don't use any other aux!

public: //getters
    const char *getName()   const                 { return this->name; }
    const char *getLabel()  const                 { return this->label; }
    const char *getFormat() const                 { return this->format; }

    double getMin()   const                       { return this->min; }
    double getMax()   const                       { return this->max; }
    double getStep()  const                       { return this->step; }
    double getValue() const                       { return this->value; }

    void *getAux() const                          { return this->aux0; }
};

template <>
struct WidgetView<ISwitch>: public ISwitch
{
    using Type = ISwitch;

public:
    WidgetView()                                  { memset(this, 0, sizeof(*this)); }
    ~WidgetView()                                 { }
    void clear()                                  { memset(this, 0, sizeof(*this)); }

public: // setters
    void setParent(ISwitchVectorProperty *parent) { this->svp = parent; }

    void setName(const char *name)                { strncpy(this->name, name, MAXINDINAME); }
    void setName(const std::string &name)         { setName(name.data()); }

    void setLabel(const char *label)              { strncpy(this->label, label, MAXINDILABEL); }
    void setLabel(const std::string &label)       { setLabel(label.data()); }

    void setState(const ISState &state)           { this->s = state; }
    bool setState(const std::string &state)       { return crackISState(state.data(), &this->s) == 0; }

    void setAux(void *user)                       { this->aux = user; }
    // don't use any other aux!

public: //getters
    const char *getName()   const                 { return this->name; }
    const char *getLabel()  const                 { return this->label; }

    ISState getState() const                      { return this->s; }

    void *getAux() const                          { return this->aux; }
};

template <>
struct WidgetView<ILight>: public ILight
{
    using Type = ILight;

public:
    WidgetView()                                  { memset(this, 0, sizeof(*this)); }
    ~WidgetView()                                 { }
    void clear()                                  { memset(this, 0, sizeof(*this)); }

public: // setters
    void setParent(ILightVectorProperty *parent)  { this->lvp = parent; }

    void setName(const char *name)                { strncpy(this->name, name, MAXINDINAME); }
    void setName(const std::string &name)         { setName(name.data()); }

    void setLabel(const char *label)              { strncpy(this->label, label, MAXINDILABEL); }
    void setLabel(const std::string &label)       { setLabel(label.data()); }

    void setState(const IPState &state)           { this->s = state; }
    bool setState(const std::string &state)       { return crackIPState(state.data(), &this->s) == 0; }

    void setAux(void *user)                       { this->aux = user; }
    // don't use any other aux!

public: //getters
    const char *getName()   const                 { return this->name; }
    const char *getLabel()  const                 { return this->label; }

    IPState getState() const                      { return this->s; }

    void *getAux() const                          { return this->aux; }
};

template <>
struct WidgetView<IBLOB>: public IBLOB
{
    using Type = IBLOB;

public:
    WidgetView()                                  { memset(this, 0, sizeof(*this)); }
    ~WidgetView()                                 { free(this->blob); }
    void clear()                                  { free(this->blob); memset(this, 0, sizeof(*this)); }

public: // setters
    void setParent(IBLOBVectorProperty *parent)   { this->bvp = parent; }

    void setName(const char *name)                { strncpy(this->name, name, MAXINDINAME); }
    void setName(const std::string &name)         { setName(name.data()); }

    void setLabel(const char *label)              { strncpy(this->label, label, MAXINDILABEL); }
    void setLabel(const std::string &label)       { setLabel(label.data()); }

    void setFormat(const char *format)            { strncpy(this->format, format, MAXINDIBLOBFMT); }
    void setFormat(const std::string &format)     { setLabel(format.data()); }

    // TODO
    //void setBlob(...);
    
    void setAux(void *user)                       { this->aux0 = user; }
    // don't use any other aux!

public: //getters
    const char *getName()   const                 { return this->name; }
    const char *getLabel()  const                 { return this->label; }
    const char *getFormat() const                 { return this->format; }

    void *getAux() const                          { return this->aux0; }
};

}
