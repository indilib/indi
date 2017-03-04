/*
    10micron INDI driver

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

typedef enum {
    GSTAT_TRACKING                    =  0,
    GSTAT_STOPPED                     =  1,
    GSTAT_PARKING                     =  2,
    GSTAT_UNPARKING                   =  3,
    GSTAT_SLEWING_TO_HOME             =  4,
    GSTAT_PARKED                      =  5,
    GSTAT_STOPPING                    =  6,
    GSTAT_NOT_TRACKING_AND_NOT_MOVING =  7,
    GSTAT_MOTORS_TOO_COLD             =  8,
    GSTAT_TRACKING_OUTSIDE_LIMITS     =  9,
    GSTAT_FOLLOWING_SATELLITE         = 10,
    GSTAT_NEED_USEROK                 = 11,
    GSTAT_UNKNOWN_STATUS              = 98,
    GSTAT_ERROR                       = 99
} _10MICRON_GSTAT;

class LX200_10MICRON : public LX200Generic
{
public:

    LX200_10MICRON(void);
    ~LX200_10MICRON(void) {}

    virtual const char *getDefaultName(void);
    virtual bool Connect(const char *port, uint32_t baud);
    virtual bool Connect(const char *hostname, const char *port);
    virtual bool initProperties(void);
    virtual bool updateProperties(void);
    virtual bool ReadScopeStatus(void);
    virtual bool Park(void);
    virtual bool UnPark(void);

    // TODO move this thing elsewhere
    int monthToNumber(const char *monthName);
    int setStandardProcedureWithoutRead(int fd, const char *data);

protected:

    virtual void getBasicData(void);
    virtual bool onConnect(void);

    IText               ProductT[4];
    ITextVectorProperty ProductTP;

private:
    int fd = -1; // short notation for PortFD/sockfd
    bool getMountInfo(void);

    int OldGstat = -1;
    TelescopeStatus OldTrackState;
};

#endif
