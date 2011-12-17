/*
    Guider Interface
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

#ifndef GUIDERINTERFACE_H
#define GUIDERINTERFACE_H

#include "indibase.h"

class INDI::GuiderInterface
{
public:
    GuiderInterface();
    ~GuiderInterface();

    virtual int GuideNorth(float) = 0;
    virtual int GuideSouth(float) = 0;
    virtual int GuideEast(float) = 0;
    virtual int GuideWest(float) = 0;

protected:
    void initGuiderProperties(const char *deviceName, const char* groupName);

    INumber GuideNS[2];
    INumberVectorProperty *GuideNSP;
    INumber GuideEW[2];
    INumberVectorProperty *GuideEWP;
};

#endif // GUIDERINTERFACE_H
