/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

 PerfectStar Focuser

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef PERFECTSTAR_H
#define PERFECTSTAR_H

#include "indibase/indifocuser.h"
#include "indibase/indiusbdevice.h"

/*  Some headers we need */
#include <math.h>
#include <sys/time.h>

class PerfectStar : public INDI::Focuser, public INDI::USBDevice
{
public:

    // Perfect Star (PS) status
    typedef enum { PS_NOOP, PS_IN, PS_OUT, PS_GOTO, PS_SETPOS, PS_HALT, PS_LOCKED } PS_STATUS;

    PerfectStar();
    virtual ~PerfectStar();

    const char *getDefaultName();

    virtual void ISGetProperties (const char *dev);
    virtual bool updateProperties();

    bool Connect();
    bool Disconnect();

    void TimerHit();

    virtual IPState MoveAbsFocuser(uint32_t ticks);
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);


private:

    PS_STATUS status;

    bool setPosition(uint32_t ticks);
    bool getPosition(uint32_t *ticks);

    bool setStatus(PS_STATUS targetStatus);
    bool getStatus(PS_STATUS *currentStatus);

};

#endif
