/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 Connection Plugin Interface

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

#ifndef CONNECTIONSERIAL_H
#define CONNECTIONSERIAL_H

#include "connectioninterface.h"

namespace Connection
{

class Serial : public Interface
{

public:

    Serial(INDI::DefaultDevice *dev);
    virtual ~Serial();

    virtual bool Connect();

    virtual bool Disconnect();

    virtual void Activated();

    virtual void Deactivated();

    virtual const std::string name() { return "CONNECTION_SERIAL"; }

    virtual const std::string label() { return "Serial"; }

    virtual const char *port() { return PortT[0].text; }
    virtual const uint32_t baud();

    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool saveConfigItems(FILE *fp);

    void setDefaultPort(const char *defaultPort);
    void setDefaultBaudIndex(int newIndex);
    void setCandidatePorts(std::vector<std::string> ports) { m_Ports = ports; }    

    const int getPortFD() const { return PortFD; }

protected:

    /** \brief Connect to serial port device. Default parameters are 8 bits, 1 stop bit, no parity. Override if different from default.
      \param port Port to connect to.
      \param baud Baud rate
      \return True if connection is successful, false otherwise
      \warning Do not call this function directly, it is called by Connection::Serial Connect() function.
    */
    virtual bool Connect(const char *port, uint32_t baud);

    // Device physical port
    ITextVectorProperty PortTP;
    IText PortT[1];

    ISwitch BaudRateS[6];
    ISwitchVectorProperty BaudRateSP;

    // Should serial connection attempt to try connecting with the candiate ports (enabled) or just fail when
    // connection to current port fails (disabled)
    ISwitch AutoSearchS[6];
    ISwitchVectorProperty AutoSearchSP;

    int PortFD=-1;

    std::vector<std::string> m_Ports;
};

}

#endif
