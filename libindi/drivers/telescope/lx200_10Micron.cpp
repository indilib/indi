/*
    10Micron INDI driver
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
#include "lx200_10Micron.h"
#include "lx200driver.h"

const int LX200_10MICRON::SocketTimeout = 3; // In seconds.
const int LX200_10MICRON::Pollms = 1000; // In milliseconds.

LX200_10MICRON::LX200_10MICRON(void)
  : LX200Generic()
{
    setVersion(1, 0);
}

const char *LX200_10MICRON::getDefaultName(void)
{
    return (const char *) "10Micron";
}

bool LX200_10MICRON::initProperties(void) {
    const bool result = LX200Generic::initProperties();
    if (result) {

        // Address/Port
        IUFillText(&AddressT[0], "ADDRESS", "Address", "192.168.100.73");
        IUFillText(&AddressT[1], "PORT",    "Port",    "3490");
        IUFillTextVector(&AddressTP, AddressT, 2, getDeviceName(), "IPADDRESS_PORT", "10Micron mount", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

        return true;
    }
    return false;
}

void LX200_10MICRON::ISGetProperties (const char *dev)
{
    LX200Generic::ISGetProperties(dev);

    defineText(&AddressTP);
//    loadConfig(true, "IPADDRESS_PORT");
}

bool LX200_10MICRON::updateProperties(void) {
    bool result = LX200Generic::updateProperties();

    defineText(&AddressTP);

    return result;
}

bool LX200_10MICRON::Connect(void)
{
    bool rc = false;

    if (isConnected() )
        return true;

    // allow connect options like TCP or Serial
    // TODO
    rc = ConnectTCP();

    return rc;
}

bool LX200_10MICRON::ConnectTCP(void)
{
    if (sockfd != -1)
        close(sockfd);

    struct timeval ts;
    ts.tv_sec = SocketTimeout;
    ts.tv_usec=0;

    struct sockaddr_in serv_addr;
    struct hostent *hp = NULL;
    int ret = 0;

    // Lookup host name or IPv4 address
    hp = gethostbyname(AddressT[0].text);
    if (!hp)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to lookup IP Address or hostname.");
        return false;
    }    

    memset (&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    serv_addr.sin_port = htons(atoi(AddressT[1].text));

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to create socket.");
        return false;
    }

    // Connect to the mount
    if ( (ret = ::connect (sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect to mount %s@%s: %s.", AddressT[0].text, AddressT[1].text, strerror(errno));
        close(sockfd);
        return false;
    }

    // Set the socket receiving and sending timeouts
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&ts,sizeof(struct timeval));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&ts,sizeof(struct timeval));

    DEBUGF(INDI::Logger::DBG_SESSION, "Connected successfuly to %s.", getDeviceName());

    SetTimer(Pollms);

    // now let the rest of INDI::Telescope use our socket as if it were a serial port
    PortFD = sockfd;

    return true;
}

bool LX200_10MICRON::Disconnect(void)
{
    // allow connect options like TCP or Serial
    // TODO
    close(sockfd);
    sockfd=-1;
    PortFD = sockfd;

    DEBUGF(INDI::Logger::DBG_SESSION,"%s is offline.", getDeviceName());

    return true;
}

bool LX200_10MICRON::getMountInfo(void)
{
    return false;
}

void LX200_10MICRON::getBasicData(void)
{
    getMountInfo();
}

