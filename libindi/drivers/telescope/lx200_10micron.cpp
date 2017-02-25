/*
    10micron INDI driver
    GM1000HPS GM2000QCI GM2000HPS GM3000HPS GM4000QCI GM4000HPS AZ2000
    Mount Command Protocol 2.14.11

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

#include <math.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


#include "indicom.h"
#include "lx200_10micron.h"
#include "lx200driver.h"

LX200_10MICRON::LX200_10MICRON(void)
  : LX200Generic()
{
    setVersion(1, 0);
}

const char *LX200_10MICRON::getDefaultName(void)
{
    return (const char *) "10micron";
}

bool LX200_10MICRON::initProperties(void) {
    const bool result = LX200Generic::initProperties();    
    return result;
}

void LX200_10MICRON::ISGetProperties (const char *dev)
{
    LX200Generic::ISGetProperties(dev);
}

bool LX200_10MICRON::updateProperties(void) {
    bool result = LX200Generic::updateProperties();
    return result;
}

bool LX200_10MICRON::getMountInfo(void)
{
    return false;
}

void LX200_10MICRON::getBasicData(void)
{
    getMountInfo();
}

