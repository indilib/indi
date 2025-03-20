/*******************************************************************************
  Copyright(c) 2024 Joe Zhou. All rights reserved.

  iOptron Filter Wheel

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "ioptron_wheel.h"

#include "indicom.h"
#include <connectionplugins/connectionserial.h>
#include <cstring>
#include <memory>
#include <termios.h>

#define iEFW_TIMEOUT 4

/*
command :
:WMnn#          Response:  1
get position ，nn filter num。

:WP#            Response:  nn#  or  -1#
get curr position,nn =curr position，if is moving，return -1。

:WOnnsnnnnn#	Response:  1
set offset。nn =filter num , snnnnn=offset

:WFnn#          Response:     snnnnn#
get offset 。nn = filter num , snnnnn=offset

":DeviceInfo#"		Response: "nnnnnnnnnnnn#"
This command includes 12 digits.
The 7th and 8th digit means model of filter wheel.
Model number  99 is iEFW-15, 98 is iEFW18

:FW1#			Response:  nnnnnnnnnnnn#
get firmware version


*/


static std::unique_ptr<iEFW> iefw_wheel(new iEFW());

iEFW::iEFW()
{
    setVersion(1, 0);
    setFilterConnection(CONNECTION_SERIAL);
}

const char *iEFW::getDefaultName()
{
    return "iOptron Wheel";
}

bool iEFW::initProperties()
{
    INDI::FilterWheel::initProperties();
    //    serialConnection->setDefaultPort("/dev/ttyUSB3");
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(8);
    FirmwareTP[0].fill("FIRMWARE", "Firmware", "240101240101");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_ID", "iEFW Firmware", FILTER_TAB, IP_RO, 60, IPS_IDLE);
    WheelIDTP[0].fill("MODEL", "Model", "iEFW");
    WheelIDTP.fill(getDeviceName(), "MODEL_ID", "iEFW Model", FILTER_TAB, IP_RO, 60, IPS_IDLE);
    return true;
}

bool iEFW::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        defineProperty(HomeSP);
        defineProperty(FirmwareTP);
        defineProperty(WheelIDTP);
        bool rc1 = getiEFWInfo();
        bool rc2 = getiEFWfirmwareInfo();
        getFilterPos();
        return (rc1 && rc2);
    }
    else
    {
        deleteProperty(HomeSP);
        deleteProperty(FirmwareTP);
        deleteProperty(WheelIDTP);
    }
    return true;
}

bool iEFW::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (HomeSP.isNameMatch(name))
        {
            if (getiEFWID())
            {
                LOG_INFO("Filter is at home position");
                HomeSP.setState(IPS_OK);
                HomeSP.apply();
            }
            else
                HomeSP.setState(IPS_ALERT);
            HomeSP.apply();
            return true;
        }
    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

bool iEFW::Handshake()
{

    return getiEFWID();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
bool iEFW::getiEFWInfo()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char resp[16];
    int iefwpos, iefwmodel, iefwlast;
    sleep(2);
    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":DeviceInfo#", strlen(":DeviceInfo#"), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR( "Init send iEFW deviceinfo  error: %s.", errstr);
        return false;
    };

    if ( (rc = tty_read_section(PortFD, resp, '#', iEFW_TIMEOUT * 2, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR( "Init read iEFW deviceinfo error: %s.", errstr);
        return false;
    };
    tcflush(PortFD, TCIOFLUSH);
    resp[nbytes_read] = '\0';
    sscanf(resp, "%6d%2d%4d", &iefwpos, &iefwmodel, &iefwlast);
    if ((iefwmodel == 98) || (iefwmodel == 99))
    {
        if (iefwmodel == 99)
        {
            FilterSlotNP[0].setMax(5);
            WheelIDTP.setState(IPS_OK);
            WheelIDTP[0].setText("iEFW-15");
            WheelIDTP.apply();
        };
        if (iefwmodel == 98)
        {
            FilterSlotNP[0].setMax(8);
            WheelIDTP.setState(IPS_OK);
            WheelIDTP[0].setText("iEFW-18");
            WheelIDTP.apply();

        };
        return true;
    }
    else
    {
        LOGF_ERROR( "iEFW getinfo Response: %s", resp);
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool iEFW::getiEFWfirmwareInfo()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16] = {0};
    char iefwfirminfo[16] = {0};
    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":FW1#", 5, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR( "get iEFW FiremwareInfo error: %s.", errstr);
    }
    if ( (rc = tty_read_section(PortFD, resp, '#', iEFW_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR( "get iEFW FirmwareInfo  error: %s.", errstr);
        return false;
    }
    tcflush(PortFD, TCIOFLUSH);
    resp[nbytes_read] = '\0';
    sscanf(resp, "%12s", iefwfirminfo);
    FirmwareTP.setState(IPS_OK);
    FirmwareTP[0].setText(iefwfirminfo);
    FirmwareTP.apply();
    LOGF_DEBUG("Success, response from iEFW is : %s", iefwfirminfo);
    return true;
}

int iEFW::getFilterPos()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16] = {0};
    int  iefwpos = 1;
    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":WP#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR( "send iEFW filter pos Info error: %s.", errstr);
    }
    if ( (rc = tty_read_section(PortFD, resp, '#', iEFW_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR( "read iEFW filter pos Info  error: %s.", errstr);
        return false;
    }
    tcflush(PortFD, TCIOFLUSH);
    resp[nbytes_read] = '\0';
    LOGF_DEBUG("Success, response from iEFW is : %s", resp);
    rc = sscanf(resp, "%2d", &iefwpos);
    if (rc > 0)
    {
        if (iefwpos >= 0)
        {
            CurrentFilter = iefwpos + 1;
            FilterSlotNP[0].setValue(CurrentFilter);
            FilterSlotNP.setState(IPS_OK);
            return CurrentFilter;
        }
        else
            return -1;
    }
    else
        return 999;

}


bool iEFW::getiEFWID()
{
    return getiEFWInfo();
}

bool iEFW::SelectFilter(int f)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    //  char resp[16] = {0};
    //  char iefwposinfo[16] = {0};
    int  iefwpos = -1;
    char cmd[7] = {0};
    if (CurrentFilter == f)
    {
        SelectFilterDone(CurrentFilter);
        return true;
    }
    f = f - 1;

    if (f < 0 || f > (FilterSlotNP[0].getMax() - 1))
        return false;

    tcflush(PortFD, TCIOFLUSH);
    snprintf(cmd, 7, ":WM0%d#", f);
    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR( "select iEFW send pos Info error: %s.", errstr);
    }
    tcflush(PortFD, TCIOFLUSH);
    // check current position  -1  is moving
    do
    {
        usleep(100 * 1000);
        iefwpos = getFilterPos();
    }
    while (iefwpos == -1);
    // return current position to indi
    CurrentFilter = f + 1;
    SelectFilterDone(CurrentFilter);
    FilterSlotNP.setState(IPS_OK);
    FilterSlotNP.apply("Selected filter position reached");
    LOGF_DEBUG("CurrentFilter set to %d", CurrentFilter);
    return true;
}

int iEFW::QueryFilter()
{
    return CurrentFilter;
}
