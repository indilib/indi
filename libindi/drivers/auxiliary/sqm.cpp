/*******************************************************************************
  Copyright(c) 2016 Jasem Mutlaq. All rights reserved.

  INDI Sky Quality Meter Driver

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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <memory>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>

#include "sqm.h"

// We declare an auto pointer to SQM.
std::unique_ptr<SQM> sqm(new SQM());

#define SQM_TIMEOUT    3
#define POLLMS          1000

void ISGetProperties(const char *dev)
{
    sqm->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    sqm->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    sqm->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    sqm->ISNewNumber(dev, name, values, names, num);
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
    sqm->ISSnoopDevice(root);
}

SQM::SQM()
{
    setVersion(1,0);    
}

SQM::~SQM()
{

}

bool SQM::initProperties()
{
    INDI::DefaultDevice::initProperties();   

    addDebugControl();

    return true;
}

void SQM::ISGetProperties (const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineText(&AddressTP);
    loadConfig(true, "IPADDRESS_PORT");
}

bool SQM::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {


    }
    else
    {

    }

    return true;
}


const char * SQM::getDefaultName()
{
    return (char *)"SQM";
}

bool SQM::Connect()
{
    if (sockfd != -1)
        close(sockfd);

    struct timeval ts;
    ts.tv_sec = 3;
    ts.tv_usec =0;

    struct sockaddr_in serv_addr;
    struct hostent *hp;
    int ret = 0;

    /* lookup host address */
    hp = gethostbyname(AddressT[0].text);
    if (!hp)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to lookup IP Address or hostname.");
        return false;
    }

    /* create a socket to the INDI server */
    (void) memset ((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    serv_addr.sin_port = htons(atoi(AddressT[1].text));
    if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to create socket.");
        return false;
    }

    /* connect */
    if ( (ret = ::connect (sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))) <0)
    {
        if (errno != EINPROGRESS)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Failed to connect to SQM server.");
            close(sockfd);
            return false;
        }
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "Connected successfuly to %s.", getDeviceName());

    SetTimer(POLLMS);

    return true;
}

bool SQM::Disconnect()
{
    close(sockfd);
    sockfd=-1;

    DEBUGF(INDI::Logger::DBG_SESSION,"%s is offline.", getDeviceName());

    return true;
}

bool SQM::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{    
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool SQM::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(AddressTP.name, name))
        {
            IUUpdateText(&AddressTP, texts, names, n);
            AddressTP.s = IPS_OK;
            IDSetText(&AddressTP, NULL);
            return true;
        }        
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool SQM::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {        
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool SQM::ISSnoopDevice (XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool SQM::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &AddressTP);
    return true;
}

bool SQM::getStatus()
{        

    return true;
}

bool SQM::getDeviceInfo()
{    
    return true;
}

void SQM::TimerHit()
{
    if (isConnected() == false)
        return;

    getStatus();

    SetTimer(POLLMS);
}
