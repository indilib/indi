/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

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

#include "indicom.h"
#include "indilogger.h"
#include "connectionserial.h"

namespace Connection
{

extern const char *CONNECTION_TAB;

Serial::Serial(INDI::DefaultDevice *dev) : Interface(dev)
{
    IUFillText(&PortT[0],"PORT","Port","/dev/ttyUSB0");
    IUFillTextVector(&PortTP,PortT,1, dev->getDeviceName(),"DEVICE_PORT","Ports",CONNECTION_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&BaudRateS[0], "9600", "", ISS_ON);
    IUFillSwitch(&BaudRateS[1], "19200", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[2], "38400", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[3], "57600", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[4], "115200", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[5], "230400", "", ISS_OFF);
    IUFillSwitchVector(&BaudRateSP, BaudRateS, 6, dev->getDeviceName(),"DEVICE_BAUD_RATE", "Baud Rate", CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
}

Serial::~Serial()
{

}

bool Serial::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(!strcmp(dev,device->getDeviceName()))
    {
        // Serial Port
        if(!strcmp(name,PortTP.name))
        {
            IUUpdateText(&PortTP,texts,names,n);
            PortTP.s=IPS_OK;
            IDSetText(&PortTP,NULL);
            return true;
        }
    }

    return false;
}

bool Serial::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev,device->getDeviceName()))
    {
        if (!strcmp(name, BaudRateSP.name))
        {
            IUUpdateSwitch(&BaudRateSP, states, names, n);
            BaudRateSP.s = IPS_OK;
            IDSetSwitch(&BaudRateSP, NULL);
            return true;
        }
    }

    return false;
}

bool Serial::Connect()
{
    uint32_t baud = atoi(IUFindOnSwitch(&BaudRateSP)->name);
    bool rc = Connect(PortT[0].text, baud);

    if (rc == false)
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "Connection to %s @ %d failed.", PortT[0].text, baud);
        for (std::string onePort : m_Ports)
        {
            DEBUGF(INDI::Logger::DBG_DEBUG, "Trying connection to %s @ %d ...", onePort.c_str(), baud);
            if (rc = Connect(onePort.c_str(), baud))
            {
                IUSaveText(&PortT[0], onePort.c_str());
                IDSetText(&PortTP, NULL);
                device->saveConfig(true, "DEVICE_PORT");
                break;
            }
        }
    }

    if (rc)
    {
        return Handshake();
    }

    return rc;
}

bool Serial::Connect(const char *port, uint32_t baud)
{
    if (device->isSimulation())
        return true;

    int connectrc=0;
    char errorMsg[MAXRBUF];

    DEBUGF(INDI::Logger::DBG_DEBUG, "Connecting to %s",port);

    if ( (connectrc = tty_connect(port, baud, 8, 0, 1, &PortFD)) != TTY_OK)
    {
        tty_error_msg(connectrc, errorMsg, MAXRBUF);

        DEBUGF(INDI::Logger::DBG_ERROR,"Failed to connect to port %s. Error: %s", port, errorMsg);

        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "Port FD %d",PortFD);

    return true;
}

bool Serial::Disconnect()
{
    if (PortFD > 0)
    {
        tty_disconnect(PortFD);
        PortFD = -1;
    }
}

void Serial::Activated()
{
    device->defineText(&PortTP);
    device->loadConfig(true, "DEVICE_PORT");

    device->defineSwitch(&BaudRateSP);
    device->loadConfig(true, "DEVICE_BAUD_RATE");
}

void Serial::Deactivated()
{
    device->deleteProperty(PortTP.name);
    device->deleteProperty(BaudRateSP.name);

}

bool Serial::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &PortTP);
    IUSaveConfigSwitch(fp, &BaudRateSP);

    return true;
}

void Serial::setDefaultPortIndex(int newIndex)
{
    if (newIndex < 0 || newIndex >= BaudRateSP.nsp)
        return;

    IUResetSwitch(&BaudRateSP);
    BaudRateS[newIndex].s = ISS_ON;
}

const uint32_t Serial::baud()
{
    return atoi(IUFindOnSwitch(&BaudRateSP)->name);
}

}
