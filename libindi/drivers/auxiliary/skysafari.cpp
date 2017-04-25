/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

  INDI SkySafar Middleware Driver.

  The driver expects a heartbeat from the client every X minutes. If no heartbeat
  is received, the driver executes the shutdown procedures.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include <memory>
#include <libnova.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>


#include "skysafari.h"
#include "skysafariclient.h"

#define POLLMS  1000

// We declare unique pointer to my lovely German Shephard Tommy (http://indilib.org/images/juli_tommy.jpg)
std::unique_ptr<SkySafari> tommyGoodBoy(new SkySafari());

void ISGetProperties(const char *dev)
{
    tommyGoodBoy->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    tommyGoodBoy->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    tommyGoodBoy->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    tommyGoodBoy->ISNewNumber(dev, name, values, names, num);
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
    tommyGoodBoy->ISSnoopDevice(root);
}

SkySafari::SkySafari()
{
    setVersion(0,1);
    setDriverInterface(AUX_INTERFACE);

    skySafariClient = new SkySafariClient();
}

SkySafari::~SkySafari()
{
    delete(skySafariClient);
}

const char * SkySafari::getDefaultName()
{
    return (char *)"SkySafari";
}

bool SkySafari::Connect()
{
    //bool SkySafari::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)

    // turn on server
    /*
    ISState states[2] = { ISS_ON, ISS_OFF };
    const char *names[2] = { "Enable", "Disable" };
    ISNewSwitch(getDeviceName(), "Server", states, (char **) names, 2);
    */


    bool rc = startServer();
    if (rc)
        SetTimer(POLLMS);

    return rc;
}

bool SkySafari::Disconnect()
{    
    return stopServer();
}

bool SkySafari::initProperties()
{
    INDI::DefaultDevice::initProperties();

    IUFillText(&SettingsT[INDISERVER_HOST],"INDISERVER_HOST","indiserver host","localhost");
    IUFillText(&SettingsT[INDISERVER_PORT],"INDISERVER_PORT","indiserver port","7624");
    IUFillText(&SettingsT[SKYSAFARI_PORT],"SKYSAFARI_PORT","SkySafari port", "9624");
    IUFillTextVector(&SettingsTP,SettingsT,3,getDeviceName(),"WATCHDOG_SETTINGS","Settings",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&ServerControlS[SERVER_ENABLE], "Enable", "", ISS_OFF);
    IUFillSwitch(&ServerControlS[SERVER_DISABLE], "Disable", "", ISS_ON);
    IUFillSwitchVector(&ServerControlSP, ServerControlS, 2, getDeviceName(), "Server", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&ActiveDeviceT[ACTIVE_TELESCOPE],"ACTIVE_TELESCOPE","Telescope","Telescope Simulator");
    IUFillTextVector(&ActiveDeviceTP,ActiveDeviceT,1,getDeviceName(),"ACTIVE_DEVICES","Active devices",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    addDebugControl();

    return true;
}

void SkySafari::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    DefaultDevice::ISGetProperties(dev);

    defineText(&SettingsTP);
    defineText(&ActiveDeviceTP);    
    //defineSwitch(&ServerControlSP);

    loadConfig(true);

    //watchdogClient->setTelescope(ActiveDeviceT[0].text);
    //watchdogClient->setDome(ActiveDeviceT[1].text);
}

bool SkySafari::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(SettingsTP.name, name))
        {
            IUUpdateText(&SettingsTP, texts, names, n);
            SettingsTP.s = IPS_OK;
            IDSetText(&SettingsTP, NULL);
            return true;
        }

        if (!strcmp(ActiveDeviceTP.name, name))
        {
            if (skySafariClient->isBusy())
            {
                ActiveDeviceTP.s = IPS_ALERT;
                IDSetText(&ActiveDeviceTP, NULL);
                DEBUG(INDI::Logger::DBG_ERROR, "Cannot change devices names while already connected to mount...");
                return true;
            }

            IUUpdateText(&ActiveDeviceTP, texts, names, n);
            ActiveDeviceTP.s = IPS_OK;
            IDSetText(&ActiveDeviceTP, NULL);

            //watchdogClient->setTelescope(ActiveDeviceT[0].text);
            //watchdogClient->setDome(ActiveDeviceT[1].text);

            return true;
        }

    }

    return INDI::DefaultDevice::ISNewText(dev,name,texts,names,n);
}

bool SkySafari::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{   
    return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool SkySafari::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(ServerControlSP.name, name))
        {
            bool rc = false;

            if (!strcmp(IUFindOnSwitchName(states, names, n), ServerControlS[SERVER_ENABLE].name))
            {
                // If already working, do nothing
                if (ServerControlS[SERVER_ENABLE].s == ISS_ON)
                {
                    ServerControlSP.s = IPS_OK;
                    IDSetSwitch(&ServerControlSP, NULL);
                    return true;
                }

                rc = startServer();
                ServerControlSP.s = (rc ? IPS_OK : IPS_ALERT);
            }
            else
            {
                if (!strcmp(IUFindOnSwitchName(states, names, n), ServerControlS[SERVER_DISABLE].name))
                {
                    // If already working, do nothing
                    if (ServerControlS[SERVER_DISABLE].s == ISS_ON)
                    {
                        ServerControlSP.s = IPS_IDLE;
                        IDSetSwitch(&ServerControlSP, NULL);
                        return true;
                    }

                    rc = stopServer();
                    ServerControlSP.s = (rc ? IPS_IDLE : IPS_ALERT);
                }
            }

            IUUpdateSwitch(&ServerControlSP, states, names, n);
            IDSetSwitch(&ServerControlSP, NULL);
            return true;
        }

    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool SkySafari::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &SettingsTP);
    IUSaveConfigText(fp, &ActiveDeviceTP);    

    return true;
}

void SkySafari::TimerHit()
{
    if (clientFD == -1)
    {
        struct sockaddr_in cli_socket;
        socklen_t cli_len;
        int cli_fd;

        /* get a private connection to new client */
        cli_len = sizeof(cli_socket);
        cli_fd = accept (lsocket, (struct sockaddr *)&cli_socket, &cli_len);
        if(cli_fd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            // Try again later
            SetTimer(POLLMS);
            return;
        }
        else if (cli_fd < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect to SkySafari. %s", strerror(errno));
            SetTimer(POLLMS);
            return;
        }

        clientFD = cli_fd;

        int flags=0;
        // Get socket flags
        if ((flags = fcntl(clientFD, F_GETFL, 0)) < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error connecting to SkySafari. F_GETFL: %s", strerror(errno));
        }

        // Set to Non-Blocking
        if (fcntl(clientFD, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error connecting to SkySafari. F_SETFL: %s", strerror(errno));
        }

        DEBUG(INDI::Logger::DBG_SESSION, "Connected to SkySafari.");
    }
    else
    {
        // Read from SkySafari
        char buffer[64] = { 0 };
        int rc = read(clientFD, buffer, 64);
        if(rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            SetTimer(POLLMS);
            return;
        }
        else if(rc > 0)
        {
            processCommand(buffer, rc);
        }
        else
        {
            DEBUG(INDI::Logger::DBG_ERROR, "SkySafari Disconnected? Reconnect again.");
            close(clientFD);
            clientFD=-1;
        }
    }

    SetTimer(POLLMS);
}

bool SkySafari::startServer()
{
    struct sockaddr_in serv_socket;
    int sfd;
    int flags=0;
    int reuse=1;

    /* make socket endpoint */
    if ((sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error starting server. socket: %s", strerror(errno));
        return false;
    }

    // Get socket flags
    if ((flags = fcntl(sfd, F_GETFL, 0)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error starting server. F_GETFL: %s", strerror(errno));
    }

    // Set to Non-Blocking
    if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error starting server. F_SETFL: %s", strerror(errno));
    }

    /* bind to given port for any IP address */
    memset (&serv_socket, 0, sizeof(serv_socket));
    serv_socket.sin_family = AF_INET;
    serv_socket.sin_addr.s_addr = htonl (INADDR_ANY);
    serv_socket.sin_port = htons ((unsigned short)atoi(SettingsT[SKYSAFARI_PORT].text));
    if (setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error starting server. setsockopt: %s", strerror(errno));
        return false;
    }

    if (bind(sfd,(struct sockaddr*)&serv_socket,sizeof(serv_socket)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error starting server. bind: %s", strerror(errno));
        return false;
    }

    /* willing to accept connections with a backlog of 5 pending */
    if (listen (sfd, 5) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error starting server. listen: %s", strerror(errno));
        return false;
    }

    lsocket = sfd;
    DEBUG(INDI::Logger::DBG_SESSION, "SkySafari Server is running. Connect the App now to this machine using SkySafari LX200 driver.");
}

bool SkySafari::stopServer()
{
    if (clientFD > 0)
        close(clientFD);
    if (lsocket > 0)
        close(lsocket);

    clientFD = lsocket = -1;

    return true;
}

void SkySafari::processCommand(char *command, int len)
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s> LEN <%d>", command, len);
}
