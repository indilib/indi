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

#define PRODUCT_TAB   "Product"

LX200_10MICRON::LX200_10MICRON(void)
  : LX200Generic()
{
    // TODO define the telescope capabilities via the TelescopeCapability

    // these things still pop up
    deleteProperty(FocusMotionSP.name);
    deleteProperty(FocusModeSP.name);
    deleteProperty(FocusTimerNP.name);

    setVersion(1, 0);
}

// Called by INDI::DefaultDevice::ISGetProperties
// Note that getDriverName calls ::getDefaultName which returns LX200 Generic
const char *LX200_10MICRON::getDefaultName(void)
{
    return (const char *) "10micron";
}

// Called by ISGetProperties to initialize basic properties that are required all the time
bool LX200_10MICRON::initProperties(void) {
    const bool result = LX200Generic::initProperties();    

    // these things still pop up
    deleteProperty(FocusMotionSP.name);
    deleteProperty(FocusModeSP.name);
    deleteProperty(FocusTimerNP.name);

    // TODO initialize properties additional to INDI::Telescope

    return result;
}

// Called by INDI::Telescope::ISGetProperties to define properties for when the device is in a disconnected state
// This includes new connections, fi an indi_getprop
void LX200_10MICRON::ISGetProperties (const char *dev)
{
    LX200Generic::ISGetProperties(dev);

    // these things still pop up
    deleteProperty(FocusMotionSP.name);
    deleteProperty(FocusModeSP.name);
    deleteProperty(FocusTimerNP.name);
}

// this should move to some generic library
int LX200_10MICRON::monthToNumber(const char *monthName)
{
    struct entry {
        const char *name;
        int id;
    };
    entry month_table[] = {
        { "Jan", 1 },
        { "Feb", 2 },
        { "Mar", 3 },
        { "Apr", 4 },
        { "May", 5 },
        { "Jun", 6 },
        { "Jul", 7 },
        { "Aug", 8 },
        { "Sep", 9 },
        { "Oct", 10 },
        { "Nov", 11 },
        { "Dec", 12 },
        { NULL, 0 }
    };
    entry *p = month_table;
    while (p->name != NULL) {
      if (strcasecmp(p -> name, monthName) == 0)
         return p->id;
      ++p;
    }
    return 0;
}

// Called by INDI::Telescope when connected state changes to add/remove properties
bool LX200_10MICRON::updateProperties(void) {
    bool result = LX200Generic::updateProperties();

    // LX200Generic::updateProperties just created some properties we don't have, so remove them
    // these things still pop up
    deleteProperty(FocusMotionSP.name);
    deleteProperty(FocusModeSP.name);
    deleteProperty(FocusTimerNP.name);

    if (isConnected()) {
        char ProductName[80];
        getCommandString(PortFD, ProductName, "#:GVP#");
        char ControlBox[80];
        getCommandString(PortFD, ControlBox, "#:GVZ#");
        char FirmwareVersion[80];
        getCommandString(PortFD, FirmwareVersion, "#:GVN#");
        char FirmwareDate1[80];
        getCommandString(PortFD, FirmwareDate1, "#:GVD#");
        char FirmwareDate2[80];
        char mon[4];
        int dd, yyyy;
        sscanf(FirmwareDate1, "%s %02d %04d", mon, &dd, &yyyy);
        getCommandString(PortFD, FirmwareDate2, "#:GVT#");
        char FirmwareDate[80];
        snprintf(FirmwareDate, 80, "%04d-%02d-%02dT%s", yyyy, monthToNumber(mon), dd, FirmwareDate2);

        IUFillText(&ProductT[0],"NAME","Product Name",ProductName);
        IUFillText(&ProductT[1],"CONTROL_BOX","Control Box",ControlBox);
        IUFillText(&ProductT[2],"FIRMWARE_VERSION","Firmware Version",FirmwareVersion);
        IUFillText(&ProductT[3],"FIRMWARE_DATE","Firmware Date", FirmwareDate);
        IUFillTextVector(&ProductTP,ProductT,4,getDeviceName(),"PRODUCT_INFO","Product",PRODUCT_TAB,IP_RO,60,IPS_IDLE);

        defineText(&ProductTP);

//          getBasicData();
    } else {
        deleteProperty(ProductTP.name);

        // TODO delete new'ed stuff from getBasicData
    }
    return result;
}

// INDI::Telescope calls ReadScopeStatus() to check the link to the telescope and update its state and position. The child class should call newRaDec() whenever a new value is read from the telescope.</li>
// This still lives in LX200Generic
//bool LX200_10MICRON::ReadScopeStatus()
//{
//  return true;
//}

// Called by updateProperties
void LX200_10MICRON::getBasicData(void)
{
    getMountInfo();
}

// Called by getBasicData
bool LX200_10MICRON::getMountInfo(void)
{
    return true;
}


