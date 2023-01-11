/*
    Copyright (C) 2022 by Pawel Soja <kernel32.pl@gmail.com>

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

#include "lilxml.h"
#include "indiapi.h"
#include "indibase.h"
#include "indicom.h"
#include "indidevapi.h"

#include <string>
#include <functional>
#include <list>
#include <stdexcept>
#include <memory>

#include <cstring>

namespace INDI
{

// helper
template <typename T>
class safe_ptr
{
        T fake, *ptr;

    public:
        safe_ptr(T *ptr = nullptr): ptr(ptr ? ptr : &fake)
        { }

        T& operator *() { return *ptr; }
        operator bool() const { return true; }
};

class LilXmlValue
{
    public:
        explicit LilXmlValue(const char *value);
        explicit LilXmlValue(const char *value, size_t size);

    public:
        bool isValid() const;
        const void *data()  const;
        size_t size() const;

    public:
        int         toInt(safe_ptr<bool> ok = nullptr) const;
        double      toDoubleSexa(safe_ptr<bool> ok = nullptr) const;
        double      toDouble(safe_ptr<bool> ok = nullptr) const;
        const char *toCString() const;
        std::string toString() const;
        ISRule      toISRule(safe_ptr<bool> ok = nullptr) const;
        ISState     toISState(safe_ptr<bool> ok = nullptr) const;
        IPState     toIPState(safe_ptr<bool> ok = nullptr) const;
        IPerm       toIPerm(safe_ptr<bool> ok = nullptr) const;

    public:
        std::size_t indexOf(const char *needle, size_t from = 0) const;
        std::size_t indexOf(const std::string &needle, size_t from = 0) const;

        std::size_t lastIndexOf(const char *needle, size_t from = 0) const;
        std::size_t lastIndexOf(const std::string &needle, size_t from = 0) const;

        bool startsWith(const char *needle) const;
        bool startsWith(const std::string &needle) const;

        bool endsWith(const char *needle) const;
        bool endsWith(const std::string &needle) const;

    public:
        operator const char *() const { return toCString(); }
        operator double  () const { return toDouble();  }
        operator int     () const { return toInt();     }
        operator ISRule  () const { return toISRule();  }
        operator ISState () const { return toISState(); }
        operator IPState () const { return toIPState(); }
        operator IPerm   () const { return toIPerm();   }
        operator size_t  () const { return toInt();     }

    protected:
        const char *mValue; 
        size_t      mSize;
};

class LilXmlAttribute: public LilXmlValue
{
    public:
        explicit LilXmlAttribute(XMLAtt *a);

    public:
        bool isValid() const;

    public:
        std::string name() const;
        const LilXmlValue &value() const { return *this; }

    public:
        operator bool() const { return isValid(); }

    public:
        XMLAtt *handle() const;

    protected:
        XMLAtt *mHandle;
};

class LilXmlElement
{
    public:
        using Elements = std::list<LilXmlElement>; // or implement custom container/iterators

    public:
        explicit LilXmlElement(XMLEle *e);

    public:
        bool isValid() const;
        std::string tagName() const;

    public:
        Elements getElements() const;
        Elements getElementsByTagName(const char *tagName) const;
        LilXmlAttribute getAttribute(const char *name) const;
        LilXmlAttribute addAttribute(const char *name, const char *value);
        void removeAttribute(const char *name);

        LilXmlValue context() const;
        void setContext(const char *data);

        void print(FILE *f, int level = 0) const;

    public:
        XMLEle *handle() const;

    protected:
        XMLEle *mHandle;
        char errmsg[128];
};

class LilXmlDocument
{
        LilXmlDocument(const LilXmlDocument &) = delete;
        LilXmlDocument &operator=(const LilXmlDocument&) = delete;

    public:
        explicit LilXmlDocument(XMLEle *root);
        LilXmlDocument(LilXmlDocument &&other);
        ~LilXmlDocument() = default;

    public:
        bool isValid() const;
        LilXmlElement root() const;

    protected:
        std::unique_ptr<XMLEle, void(*)(XMLEle*)> mRoot;
};

class LilXmlParser
{
        LilXmlParser(const LilXmlDocument &) = delete;
        LilXmlParser &operator=(const LilXmlDocument&) = delete;
        LilXmlParser(LilXmlDocument &&) = delete;
        LilXmlParser &operator=(LilXmlDocument &&) = delete;

    public:
        LilXmlParser();
        ~LilXmlParser() = default;

    public:
        LilXmlDocument readFromFile(FILE *file);
        LilXmlDocument readFromFile(const char *fileName);
        LilXmlDocument readFromFile(const std::string &fileName);

    public:
        std::list<LilXmlDocument> parseChunk(const char *data, size_t size);

    public:
        bool hasErrorMessage() const;
        const char *errorMessage() const;

    protected:
        std::unique_ptr<LilXML, void(*)(LilXML *)> mHandle;
        char mErrorMessage[MAXRBUF] = {0, };
};



// LilXmlValue Implementation
inline LilXmlValue::LilXmlValue(const char *value)
    : mValue(value)
    , mSize(value ? strlen(value) : 0)
{ }

inline LilXmlValue::LilXmlValue(const char *value, size_t size)
    : mValue(value)
    , mSize(size)
{ }

inline bool LilXmlValue::isValid() const
{
    return mValue != nullptr;
}

inline const void *LilXmlValue::data() const
{
    return mValue;
}

inline size_t LilXmlValue::size() const
{
    return mSize;
}

inline int LilXmlValue::toInt(safe_ptr<bool> ok) const
{
    int result = 0;
    try {
        result = std::stoi(toString());
        *ok = true;
    } catch (...) {
        *ok = false;
    }
    return result;
}

inline double LilXmlValue::toDoubleSexa(safe_ptr<bool> ok) const
{
    double result = 0;
    *ok = (isValid() && f_scansexa(mValue, &result) >= 0);
    return result;
}

inline double LilXmlValue::toDouble(safe_ptr<bool> ok) const
{
    double result = 0;
    try {
        result = std::stod(toString());
        *ok = true;
    } catch (...) {
        *ok = false;
    }
    return result;
}

inline const char *LilXmlValue::toCString() const
{
    return isValid() ? mValue : "";
}

inline std::string LilXmlValue::toString() const
{
    return isValid() ? mValue : "";
}

inline ISRule LilXmlValue::toISRule(safe_ptr<bool> ok) const
{
    ISRule rule = ISR_1OFMANY;
    *ok = (isValid() && crackISRule(mValue, &rule) >= 0);
    return rule;
}

inline ISState LilXmlValue::toISState(safe_ptr<bool> ok) const
{
    ISState state = ISS_OFF;
    *ok = (isValid() && crackISState(mValue, &state) >= 0);
    return state;
}

inline IPState LilXmlValue::toIPState(safe_ptr<bool> ok) const
{
    IPState state = IPS_OK;
    *ok = (isValid() && crackIPState(mValue, &state) >= 0);
    return state;
}

inline IPerm LilXmlValue::toIPerm(safe_ptr<bool> ok) const
{
    IPerm result = IP_RO;
    *ok = (isValid() && crackIPerm(mValue, &result) >= 0);
    return result;
}

inline std::size_t LilXmlValue::indexOf(const char *needle, size_t from) const
{
    return toString().find_first_of(needle, from);
}

inline std::size_t LilXmlValue::indexOf(const std::string &needle, size_t from) const
{
    return toString().find_first_of(needle, from);
}

inline std::size_t LilXmlValue::lastIndexOf(const char *needle, size_t from) const
{
    return toString().find_last_of(needle, from);
}

inline std::size_t LilXmlValue::lastIndexOf(const std::string &needle, size_t from) const
{
    return toString().find_last_of(needle, from);
}

inline bool LilXmlValue::startsWith(const char *needle) const
{
    return indexOf(needle) == 0;
}

inline bool LilXmlValue::startsWith(const std::string &needle) const
{
    return indexOf(needle) == 0;
}

inline bool LilXmlValue::endsWith(const char *needle) const
{
    return lastIndexOf(needle) == (size() - strlen(needle));
}

inline bool LilXmlValue::endsWith(const std::string &needle) const
{
    return lastIndexOf(needle) == (size() - needle.size());
}

// LilXmlAttribute Implementation

inline LilXmlAttribute::LilXmlAttribute(XMLAtt *a)
    : LilXmlValue(a ? valuXMLAtt(a) : nullptr)
    , mHandle(a)
{ }

inline XMLAtt *LilXmlAttribute::handle() const
{
    return mHandle;
}

inline bool LilXmlAttribute::isValid() const
{
    return mHandle != nullptr;
}


inline std::string LilXmlAttribute::name() const
{
    return isValid() ? nameXMLAtt(mHandle) : "";
}

// LilXmlElement Implementation

inline LilXmlElement::LilXmlElement(XMLEle *e)
    : mHandle(e)
{ }

inline XMLEle *LilXmlElement::handle() const
{
    return mHandle;
}

inline std::string LilXmlElement::tagName() const
{
    return tagXMLEle(mHandle);
}

inline LilXmlElement::Elements LilXmlElement::getElements() const
{
    Elements result;
    if (handle() == nullptr)
        return result;

    for (XMLEle *ep = nextXMLEle(mHandle, 1); ep != nullptr; ep = nextXMLEle(mHandle, 0))
        result.push_back(LilXmlElement(ep));
    return result;
}

inline LilXmlElement::Elements LilXmlElement::getElementsByTagName(const char *tagName) const
{
    LilXmlElement::Elements result;
    if (handle() == nullptr)
        return result;

    for (XMLEle *ep = nextXMLEle(mHandle, 1); ep != nullptr; ep = nextXMLEle(mHandle, 0))
    {
        LilXmlElement element(ep);
        if (element.tagName() == tagName)
        {
            result.push_back(element);
        }
    }
    return result;
}

inline LilXmlAttribute LilXmlElement::getAttribute(const char *name) const
{
    return LilXmlAttribute(findXMLAtt(mHandle, name));
}

inline LilXmlAttribute LilXmlElement::addAttribute(const char *name, const char *value)
{
    return LilXmlAttribute(addXMLAtt(mHandle, name, value));
}

inline void LilXmlElement::removeAttribute(const char *name)
{
    rmXMLAtt(mHandle, name);
}

inline LilXmlValue LilXmlElement::context() const
{
    return LilXmlValue(pcdataXMLEle(mHandle), pcdatalenXMLEle(mHandle));
}

inline void LilXmlElement::setContext(const char *data)
{
    editXMLEle(mHandle, data);
}

inline void LilXmlElement::print(FILE *f, int level) const
{
    prXMLEle(f, handle(), level);
}

// LilXmlDocument Implementation

inline LilXmlDocument::LilXmlDocument(XMLEle *root)
    : mRoot(root, [](XMLEle* root) { if (root) delXMLEle(root); })
{ }

inline LilXmlDocument::LilXmlDocument(LilXmlDocument &&other)
    : mRoot(std::move(other.mRoot))
{ }

inline bool LilXmlDocument::isValid() const
{
    return mRoot != nullptr;
}

inline LilXmlElement LilXmlDocument::root() const
{
    return LilXmlElement(mRoot.get());
}

// LilXmlParser Implementation

inline LilXmlParser::LilXmlParser()
    : mHandle(newLilXML(), [](LilXML *handle) { delLilXML(handle); })
{ }

inline LilXmlDocument LilXmlParser::readFromFile(FILE *file)
{
    return LilXmlDocument(readXMLFile(file, mHandle.get(), mErrorMessage));
}

inline LilXmlDocument LilXmlParser::readFromFile(const char *fileName)
{
    FILE *fp = fopen(fileName, "r");
    if (fp == nullptr)
    {
        snprintf(mErrorMessage, sizeof(mErrorMessage), "Error loading file %s", fileName);
        return LilXmlDocument(nullptr);
    }

    LilXmlDocument result = readFromFile(fp);
    fclose(fp);
    return result;
}

inline LilXmlDocument LilXmlParser::readFromFile(const std::string &fileName)
{
    return readFromFile(fileName.c_str());
}

inline std::list<LilXmlDocument> LilXmlParser::parseChunk(const char *data, size_t size)
{
    std::list<LilXmlDocument> result;
    XMLEle ** nodes = parseXMLChunk(mHandle.get(), const_cast<char*>(data), int(size), mErrorMessage);
    if (nodes != nullptr)
    {    
        for (auto it = nodes; *it; ++it)
        {
            result.push_back(LilXmlDocument(*it));
        }
        free(nodes);
    }
    return result;
}

inline bool LilXmlParser::hasErrorMessage() const
{
    return mErrorMessage[0] != '\0';
}

inline const char *LilXmlParser::errorMessage() const
{
    return mErrorMessage;
}

}
