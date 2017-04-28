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
#include "indicom.h"

#define POLLMS  100

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
    bool rc = startServer();
    if (rc)
    {
        skySafariClient->setMount(ActiveDeviceT[ACTIVE_TELESCOPE].text);
        skySafariClient->connectServer();
        SetTimer(POLLMS);
    }

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
            IUUpdateText(&ActiveDeviceTP, texts, names, n);
            ActiveDeviceTP.s = IPS_OK;
            IDSetText(&ActiveDeviceTP, NULL);
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
    if (isConnected() == false)
        return;

    if (clientFD == -1)
    {
        struct sockaddr_in cli_socket;
        socklen_t cli_len;
        int cli_fd=-1;

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

        // Only show message first time SkySafari connects
        if (isSkySafariConnected == false)
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Connected to SkySafari.");
            isSkySafariConnected = true;
        }
    }
    else
    {
        // Read from SkySafari
        char buffer[64] = { 0 };
        int rc = read(clientFD, buffer, 64);
        if(rc > 0)
        {
            std::vector<std::string> commands = split(buffer, '#');
            for (std::string cmd : commands)
            {
                // Remove the :
                cmd.erase(0,1);
                processCommand(cmd);
            }
        }
        // EOF
        else if (rc == 0)
        {
            //DEBUG(INDI::Logger::DBG_ERROR, "SkySafari Disconnected? Reconnect again.");
            close(clientFD);
            clientFD=-1;
        }

        // Otherwise EAGAIN so we just try shortly
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

    if (::bind(sfd,(struct sockaddr*)&serv_socket,sizeof(serv_socket)) < 0)
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

void SkySafari::processCommand(std::string cmd)
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd.c_str());

    if (skySafariClient->isConnected() == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Internal client is not connected! Restart driver and try again.");
        return;
    }

    // Set site Latitude
    if (cmd.compare(0, 2, "St") == 0)
    {
        int dd=0, mm=0;
        if (sscanf (cmd.c_str(), "St%d%*c%d", &dd, &mm) == 2)
        {
            haveLatitude = true;
            siteLatitude = dd + mm / 60.0;
        }

        // Always respond with valid
        sendSkySafari("1");

        // Try sending geographic coords if all is available
        sendGeographicCoords();
    }
    // Set site Longitude
    else if (cmd.compare(0, 2, "Sg") == 0)
    {
        int ddd=0, mm=0;
        if (sscanf (cmd.c_str(), "Sg%d%*c%d", &ddd, &mm) == 2)
        {
            haveLongitude = true;
            siteLongitude = ddd + mm / 60.0;

            // Convert to INDI format (0 to 360 Eastwards). Meade is 0 to 360 Westwards.
            siteLongitude = 360 - siteLongitude;
        }

        // Always respond with valid
        sendSkySafari("1");

        // Try sending geographic coords if all is available
        sendGeographicCoords();
    }
    // Get RA
    else if (cmd == "GR")
    {
        INumberVectorProperty *eqCoordsNP = skySafariClient->getEquatorialCoords();
        if (eqCoordsNP == nullptr)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Unable to communicate with mount, is mount turned on and connected?");
            return;
        }

        int hh,mm,ss;
        char output[32] = {0};
        getSexComponents(eqCoordsNP->np[AXIS_RA].value, &hh, &mm, &ss);
        snprintf(output, 32, "%02d:%02d:%02d#", hh, mm, ss);
        sendSkySafari(output);
    }
    // Get DE
    else if (cmd == "GD")
    {
        INumberVectorProperty *eqCoordsNP = skySafariClient->getEquatorialCoords();
        if (eqCoordsNP == nullptr)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Unable to communicate with mount, is mount turned on and connected?");
            return;
        }
        int dd,mm,ss;
        char output[32] = {0};
        getSexComponents(eqCoordsNP->np[AXIS_DE].value, &dd, &mm, &ss);
        snprintf(output, 32, "%+02d:%02d:%02d#", dd, mm, ss);
        sendSkySafari(output);
    }
    // Set RA
    else if (cmd.compare(0, 2, "Sr") == 0)
    {
        int hh=0, mm=0, ss=0;
        if (sscanf (cmd.c_str(), "Sr%d:%d:%d", &hh, &mm, &ss) == 3)
        {
            RA = hh + mm / 60.0 + ss / 3600.0;
        }

        // Always respond with valid
        sendSkySafari("1");
    }
    // Set DE
    else if (cmd.compare(0, 2, "Sd") == 0)
    {
        int dd=0, mm=0, ss=0;
        if (sscanf (cmd.c_str(), "Sd%d*%d:%d", &dd, &mm, &ss) == 3)
        {
            DE = dd + mm / 60.0 + ss / 3600.0;
        }

        // Always respond with valid
        sendSkySafari("1");
    }
    // GOTO
    else if (cmd == "MS")
    {
        ISwitchVectorProperty *gotoModeSP = skySafariClient->getGotoMode();
        if (gotoModeSP == nullptr)
        {
            sendSkySafari("2<Not Supported>#");
            return;
        }

        // Set mode first
        ISwitch *trackSW = IUFindSwitch(gotoModeSP, "TRACK");
        if (trackSW == nullptr)
        {
            sendSkySafari("2<Not Supported>#");
            return;
        }

        IUResetSwitch(gotoModeSP);
        trackSW->s = ISS_ON;
        skySafariClient->sendGotoMode();

        INumberVectorProperty *eqCoordsNP = skySafariClient->getEquatorialCoords();
        eqCoordsNP->np[AXIS_RA].value = RA;
        eqCoordsNP->np[AXIS_DE].value = DE;
        skySafariClient->sendEquatorialCoords();

        sendSkySafari("0");
    }
    // Sync
    else if (cmd == "CM")
    {
        ISwitchVectorProperty *gotoModeSP = skySafariClient->getGotoMode();
        if (gotoModeSP == nullptr)
        {
            sendSkySafari("Not Supported#");
            return;
        }

        // Set mode first
        ISwitch *syncSW = IUFindSwitch(gotoModeSP, "SYNC");
        if (syncSW == nullptr)
        {
            sendSkySafari("Not Supported#");
            return;
        }

        IUResetSwitch(gotoModeSP);
        syncSW->s = ISS_ON;
        skySafariClient->sendGotoMode();

        INumberVectorProperty *eqCoordsNP = skySafariClient->getEquatorialCoords();
        eqCoordsNP->np[AXIS_RA].value = RA;
        eqCoordsNP->np[AXIS_DE].value = DE;
        skySafariClient->sendEquatorialCoords();

        sendSkySafari(" M31 EX GAL MAG 3.5 SZ178.0'#");
    }
    // Abort
    else if (cmd == "Q")
    {
        skySafariClient->abort();
    }
    // RG
    else if (cmd == "RG")
    {
        skySafariClient->setSlewRate(0);
    }
    // RC
    else if (cmd == "RC")
    {
        skySafariClient->setSlewRate(1);
    }
    // RM
    else if (cmd == "RM")
    {
        skySafariClient->setSlewRate(2);
    }
    // RS
    else if (cmd == "RS")
    {
        skySafariClient->setSlewRate(3);
    }
}

void SkySafari::sendGeographicCoords()
{
    INumberVectorProperty *geographicCoords = skySafariClient->getGeographiCoords();
    if (geographicCoords && haveLatitude && haveLongitude)
    {
        INumber *latitude  = IUFindNumber(geographicCoords, "LAT");
        INumber *longitude = IUFindNumber(geographicCoords, "LONG");
        if (latitude && longitude)
        {
            latitude->value = siteLatitude;
            longitude->value= siteLongitude;
            skySafariClient->sendGeographicCoords();

            // Reset
            haveLatitude = haveLongitude = false;
        }
    }
}

bool SkySafari::sendSkySafari(const char *message)
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", message);

    int bytesWritten=0, totalBytes=strlen(message);

    while (bytesWritten < totalBytes)
    {
        int bytesSent = write(clientFD, message, totalBytes - bytesWritten);
        if (bytesSent >= 0)
            bytesWritten += bytesSent;
        else
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to SkySafari. %s", strerror(errno));
            return false;
        }
    }

    return true;
}

// Had to get this from stackoverlow, why C++ STL lacks such basic functionality?!!!
template<typename Out> void SkySafari::split(const std::string &s, char delim, Out result)
{
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim))
    {
        *(result++) = item;
    }
}


std::vector<std::string> SkySafari::split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}
