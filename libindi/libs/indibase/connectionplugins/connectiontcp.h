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

#ifndef CONNECTIONTCP_H
#define CONNECTIONTCP_H

#include "connectioninterface.h"

namespace Connection
{

class TCP : public Interface
{

    public:

        TCP(INDI::DefaultDevice * dev);
        virtual ~TCP();

        virtual bool Connect();

        virtual bool Disconnect();

        virtual void Activated();

        virtual void Deactivated();

        virtual const std::string name()
        {
            return "CONNECTION_TCP";
        }

        virtual const std::string label()
        {
            return "Ethernet";
        }

        virtual const char * host()
        {
            return AddressT[0].text;
        }
        virtual const uint32_t port()
        {
            return atoi(AddressT[0].text);
        }

        virtual bool ISNewText (const char * dev, const char * name, char * texts[], char * names[], int n);
        virtual bool saveConfigItems(FILE * fp);

        const int getPortFD() const
        {
            return PortFD;
        }
        void setDefaultHost(const char * addressHost);
        void setDefaultPort(uint32_t addressPort);

    protected:

        // IP Address/Port
        ITextVectorProperty AddressTP;
        IText AddressT[2];

        int sockfd = -1;
        const uint8_t SOCKET_TIMEOUT = 5;

        int PortFD=-1;
};

}

#endif
