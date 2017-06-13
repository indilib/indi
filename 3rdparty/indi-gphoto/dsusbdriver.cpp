/*
 DSUSB Driver for GPhoto

 Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "dsusbdriver.h"

#include "indilogger.h"

#include <string.h>

DSUSBDriver::DSUSBDriver(const char * device)
{
    strncpy(this->device, device, MAXINDIDEVICE);

    DEBUGDEVICE(device, INDI::Logger::DBG_DEBUG, "Finding DSUSB 0x134A, 0x9021 ...");
    // Try to see if it is DSUSB
    dev = FindDevice(0x134A, 0x9021, 0);
    // DSUSB2?
    if (dev == NULL)
    {
        DEBUGDEVICE(device, INDI::Logger::DBG_DEBUG, "Finding DSUSB 0x134A, 0x9026 ...");
        dev = FindDevice(0x134A, 0x9026, 0);
    }

    if (dev)
    {
        int rc = Open();
        connected = (rc != -1);
        DEBUGDEVICE(device, INDI::Logger::DBG_DEBUG, "Connected to DSUSB!");
        readState();
    }
}

bool DSUSBDriver::readState()
{
    int rc = ReadBulk(&infoByte, 1, 1000);
    DEBUGFDEVICE(device, INDI::Logger::DBG_DEBUG, "RC: %d - Info Byte: %#02X", rc, infoByte);

    return (rc == 1);
}

bool DSUSBDriver::openShutter()
{
    readState();

    DEBUGDEVICE(device, INDI::Logger::DBG_DEBUG, "DSUSB Opening Shutter ...");
    uint8_t command = (infoByte | 0x01);
    DEBUGFDEVICE(device, INDI::Logger::DBG_DEBUG, "CMD <%#02X>", infoByte);
    int rc = WriteBulk(&command, 1, 1000);
    DEBUGFDEVICE(device, INDI::Logger::DBG_DEBUG, "RC: %d", rc);

    return (rc == 1);
}

bool DSUSBDriver::closeShutter()
{
    readState();

    DEBUGDEVICE(device, INDI::Logger::DBG_DEBUG, "DSUSB Opening Shutter ...");
    uint8_t command = (infoByte & 0xFE);
    DEBUGFDEVICE(device, INDI::Logger::DBG_DEBUG, "CMD <%#02X>", infoByte);
    int rc = WriteBulk(&command, 1, 1000);
    DEBUGFDEVICE(device, INDI::Logger::DBG_DEBUG, "RC: %d", rc);

    return (rc == 1);
}
