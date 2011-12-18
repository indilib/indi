/*
    Filter Interface
    Copyright (C) 2011 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef INDIFILTERINTERFACE_H
#define INDIFILTERINTERFACE_H

#include "indibase.h"

class INDI::FilterInterface
{

public:

    virtual int QueryFilter() = 0;
    virtual bool SelectFilter(int) = 0;
    virtual bool SetFilterNames() = 0;
    virtual bool initFilterNames(const char *deviceName, const char* groupName) = 0;

    void SelectFilterDone(int);


protected:

    FilterInterface();
    ~FilterInterface();

    void initFilterProperties(const char *deviceName, const char* groupName);

    INumberVectorProperty *FilterSlotNP;   //  A number vector for filter slot
    INumber FilterSlotN[1];

    ITextVectorProperty *FilterNameTP; //  A text vector that stores out physical port name
    IText *FilterNameT;

    int MinFilter;
    int MaxFilter;
    int CurrentFilter;
    int TargetFilter;
};

#endif // INDIFILTERINTERFACE_H
