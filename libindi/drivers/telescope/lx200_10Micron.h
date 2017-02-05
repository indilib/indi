/*
    10Micron INDI driver

    Copyright (C) 2017 Hans Lambermont

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

#ifndef LX200_10MICRON_H
#define LX200_10MICRON_H

#include "lx200generic.h"

class LX200_10MICRON : public LX200Generic
{
public:

    LX200_10MICRON(void);
    ~LX200_10MICRON(void) {}

    virtual const char *getDefaultName(void);
    virtual bool Connect(void);
    virtual bool Disconnect(void);
    virtual bool initProperties(void);
    virtual void ISGetProperties (const char *dev);
    virtual bool updateProperties(void);

protected:

    virtual void getBasicData(void);

private:

    // IP Address/Port
    ITextVectorProperty AddressTP;
    IText AddressT[2];

    int sockfd = -1;

    virtual bool ConnectTCP(void);
    bool getMountInfo(void);

    static const int SocketTimeout;
    static const int Pollms;
};

#endif

