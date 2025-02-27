/*******************************************************************************
  Copyright(c) 2016 Radek Kaczorek  <rkaczorek AT gmail DOT com>

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

#include "quantum_wheel.h"

#include "connectionplugins/connectionserial.h"

#include <memory>
#include <cstring>
#include <termios.h>
#include <unistd.h>

#include "indicom.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 3

#define QUANTUM_TIMEOUT 5

std::unique_ptr<QFW> qfw(new QFW());

QFW::QFW()
{
    setDeviceName(QFW::getDefaultName());
    setVersion(VERSION_MAJOR, VERSION_MINOR);
    setFilterConnection(CONNECTION_SERIAL | CONNECTION_TCP);
}

void QFW::debugTriggered(bool enable)
{
    INDI_UNUSED(enable);
}

void QFW::simulationTriggered(bool enable)
{
    INDI_UNUSED(enable);
}

const char *QFW::getDefaultName()
{
    return (const char *)"Quantum Wheel";
}

bool QFW::Handshake()
{
    if (isSimulation())
    {
        IDMessage(getDeviceName(), "Simulation: connected");
        PortFD = 1;
        return true;
    }
    // check serial connection
    if (PortFD < 0 || isatty(PortFD) == 0)
    {
        IDMessage(getDeviceName(), "Device /dev/ttyACM0 is not available\n");
        return false;
    }
    // read description
    char cmd[10] = {"SN\r\n"};
    char resp[255] = {0};
    if (send_command(PortFD, cmd, resp) < 2)
        return false;

    // SN should respond SN<number>, this identifies a Quantum wheel
    return strncmp(cmd, resp, 2) == 0;
}

bool QFW::initProperties()
{
    INDI::FilterWheel::initProperties();
    addDebugControl();
    addSimulationControl();

    serialConnection->setDefaultPort("/dev/ttyACM0");

    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(7);
    CurrentFilter      = 1;

    return true;
}

//bool QFW::updateProperties()
//{
//    INDI::FilterWheel::updateProperties();

//    if (isConnected())
//    {
//        // read number of filters
//        char cmd[10] = {"EN\r\n"};
//        char resp[255]={0};
//        if (send_command(PortFD, cmd, resp) < 2)
//            return false;
//        int numFilters = resp[1] - '0';
//        //FilterNameTP->ntp = numFilters;
//        for (int i = 0; i < numFilters; i++)
//        {
//            sprintf(cmd, "F%1d\r\n", i);
//            if (send_command(PortFD, cmd, resp) < 3)
//                return false;
//            char name[64];
//            int n = strlen(resp);
//            strncpy(name, &resp[2], n - 4);
//            name[n] = 0;
//            IUFillText(&FilterNameT[i], name, name, name);
//        }
//    }

//    return true;
//}

void QFW::ISGetProperties(const char *dev)
{
    INDI::FilterWheel::ISGetProperties(dev);
}

int QFW::QueryFilter()
{
    return CurrentFilter;
}

bool QFW::SelectFilter(int position)
{
    // count from 0 to 6 for positions 1 to 7
    position = position - 1;

    if (position < 0 || position > 6)
        return false;

    if (isSimulation())
    {
        CurrentFilter = position + 1;
        SelectFilterDone(CurrentFilter);
        return true;
    }

    // goto
    char targetpos[255] = {0};
    char curpos[255] = {0};
    char dmp[255];
    int err;
    int nbytes;

    // format target position G[0-6]
    sprintf(targetpos, "G%d\r\n ", position);

    // write command
    //int len = strlen(targetpos);

    err = tty_write_string(PortFD, targetpos, &nbytes);
    if (err)
    {
        char errmsg[255];
        tty_error_msg(err, errmsg, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errmsg);
        return false;
    }
    //res = write(PortFD, targetpos, len);
    dump(dmp, targetpos);
    LOGF_DEBUG("CMD: %s", dmp);

    // format target marker P[0-6]
    sprintf(targetpos, "P%d", position);

    // check current position
    do
    {
        usleep(100 * 1000);
        //res         = read(PortFD, curpos, 255);
        err = tty_read_section(PortFD, curpos, '\n', QUANTUM_TIMEOUT, &nbytes);
        if (err)
        {
            char errmsg[255];
            tty_error_msg(err, errmsg, MAXRBUF);
            LOGF_ERROR("Serial read error: %s", errmsg);
            return false;
        }
        curpos[nbytes] = 0;
        dump(dmp, curpos);
        LOGF_DEBUG("REP: %s", dmp);
    }
    while (strncmp(targetpos, curpos, 2) != 0);

    // return current position to indi
    CurrentFilter = position + 1;
    SelectFilterDone(CurrentFilter);
    LOGF_DEBUG("CurrentFilter set to %d", CurrentFilter);

    return true;
}

void QFW::dump(char *buf, const char *data)
{
    int i = 0;
    int n = 0;
    while(data[i] != 0)
    {
        if (isprint(data[i]))
        {
            buf[n] = data[i];
            n++;
        }
        else
        {
            sprintf(buf + n, "[%02X]", data[i]);
            n += 4;
        }
        i++;
    }
}

// Send a command to the mount. Return the number of bytes received or 0 if
// case of error
// commands are null terminated, replies end with /n
int QFW::send_command(int fd, const char* cmd, char *resp)
{
    int err;
    int nbytes = 0;
    char errmsg[MAXRBUF];
    int cmd_len = strlen(cmd);
    char dmp[255];

    dump(dmp, cmd);
    LOGF_DEBUG("CMD <%s>", dmp);

    tcflush(fd, TCIOFLUSH);
    if ((err = tty_write(fd, cmd, cmd_len, &nbytes)) != TTY_OK)
    {
        tty_error_msg(err, errmsg, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errmsg);
        return 0;
    }

    err = tty_read_section(fd, resp, '\n', QUANTUM_TIMEOUT, &nbytes);
    if (err)
    {
        tty_error_msg(err, errmsg, MAXRBUF);
        LOGF_ERROR("Serial read error: %s", errmsg);
        return 0;
    }

    resp[nbytes] = 0;
    dump(dmp, resp);
    LOGF_DEBUG("RES <%s>", dmp);
    return nbytes;
}
