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

#include "perfectstar.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <memory>

#define POLLMS              1000            /* 1000 ms */
#define PERFECTSTAR_TIMEOUT 1000            /* 1000 ms */

// We declare an auto pointer to PerfectStar.
std::auto_ptr<PerfectStar> perfectStar(0);

void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(perfectStar.get() == 0) perfectStar.reset(new PerfectStar());

}

void ISGetProperties(const char *dev)
{
        ISInit();
        perfectStar->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        perfectStar->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        perfectStar->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        perfectStar->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
    ISInit();
    perfectStar->ISSnoopDevice(root);
}

PerfectStar::PerfectStar()
{
    FocuserCapability cap;
    cap.canAbort=true;
    cap.canAbsMove=true;
    cap.canRelMove=true;
    cap.variableSpeed=false;

    SetFocuserCapability(&cap);
}

PerfectStar::~PerfectStar()
{

}

bool PerfectStar::Connect()
{
    dev=FindDevice(0x14D8, 0xF812,0);

    if(dev==NULL)
     {
         DEBUG(INDI::Logger::DBG_ERROR, "No PerfectStar focuser found.");
         return false;
     }

    int rc = Open();

    if (rc != -1)
        SetTimer(POLLMS);

    return (rc != -1);
}

bool PerfectStar::Disconnect()
{
    return true;
}

const char * PerfectStar::getDefaultName()
{
        return (char *)"PerfectStar";
}

void PerfectStar::ISGetProperties (const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);

    // We don't need port property
    deleteProperty(PortTP.name);

}

bool PerfectStar::updateProperties()
{

    return INDI::Focuser::updateProperties();
}

void PerfectStar::TimerHit()
{
    if (isConnected() == false)
        return;

    SetTimer(POLLMS);
}

IPState PerfectStar::MoveAbsFocuser(uint32_t targetTicks)
{


}

IPState PerfectStar::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t targetTicks = FocusAbsPosN[0].value + (ticks * (dir == FOCUS_INWARD ? -1 : 1));

    return MoveAbsFocuser(targetTicks);
}

bool PerfectStar::setPosition(uint32_t ticks)
{

}

bool PerfectStar::getPosition(uint32_t *ticks)
{

}

bool PerfectStar::setStatus(PS_STATUS targetStatus)
{
    int rc=0;
    unsigned char command[2];
    unsigned char response[3];

    command[0] = 0x10;
    command[1] = (targetStatus == PS_HALT) ? 0xFF : targetStatus;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%#02X %#02X)", command[0], command[1]);

    rc = WriteBulk(command, 2, PERFECTSTAR_TIMEOUT);

    if (rc < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "setStatus: Error writing to device.");
        return false;
    }

    rc = ReadBulk(response, 3, PERFECTSTAR_TIMEOUT);

    if (rc < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "setStatus: Error reading from device.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%#02X %#02X %#02X)", response[0], response[1], response[2]);

    if (response[1] == 0xFF)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "setStatus: Invalid state change.");
        return false;
    }


    return true;
}

bool PerfectStar::getStatus(PS_STATUS *currentStatus)
{
    int rc=0;
    unsigned char command[1];
    unsigned char response[2];

    command[0] = 0x11;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%#02X)", command[0]);

    rc = WriteBulk(command, 1, PERFECTSTAR_TIMEOUT);

    if (rc < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "getStatus: Error writing to device.");
        return false;
    }

    rc = ReadBulk(response, 2, PERFECTSTAR_TIMEOUT);

    if (rc < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "getStatus: Error reading from device.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%#02X %#02X)", response[0], response[1]);

    switch (response[1])
    {
        case 0:
            *currentStatus = PS_HALT;
            break;

        case 1:
            *currentStatus = PS_IN;
            break;

        case 2:
            *currentStatus = PS_OUT;
            break;

        case 5:
            *currentStatus = PS_LOCKED;
            break;

    default:
            DEBUGF(INDI::Logger::DBG_ERROR, "Error: Unknown status (%d)", response[1]);
            return false;
            break;
    }

    return true;

}

