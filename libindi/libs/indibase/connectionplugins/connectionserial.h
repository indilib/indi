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

#pragma once

#include "connectioninterface.h"

#include <string>

namespace Connection
{
class Serial : public Interface
{
  public:
    /**
     * \typedef BaudRate
     * \brief Supported baud rates
     * \note: Default baud rate is 9600. To change default baud rate, use setDefaultBaudrate(..) function.
     */
    typedef enum { B_9600, B_19200, B_38400, B_57600, B_115200, B_230400 } BaudRate;

    Serial(INDI::DefaultDevice *dev);
    virtual ~Serial();

    virtual bool Connect();

    virtual bool Disconnect();

    virtual void Activated();

    virtual void Deactivated();

    virtual std::string name() { return "CONNECTION_SERIAL"; }

    virtual std::string label() { return "Serial"; }

    /**
     * @return Currently active device port
     */
    virtual const char *port() { return PortT[0].text; }

    /**
     * @return Currently active baud rate raw value (e.g. 9600, 19200..etc)
     */
    virtual uint32_t baud();

    /**
     * @brief setDefaultPort Set default port. Call this function in initProperties() of your driver
     * if you want to change default port.
     * @param defaultPort Name of desired default port
     */
    void setDefaultPort(const char *defaultPort);

    /**
     * @brief setDefaultBaudRate Set default baud rate. The default baud rate is 9600 unless
     * otherwise changed by this function. Call this function in initProperties() of your driver.
     * @param newRate Desired new rate
     */
    void setDefaultBaudRate(BaudRate newRate);

    /**
     * @return Return port file descriptor. If connection is successful, PortFD is a positive
     * integer otherwise it is set to -1
     */
    int getPortFD() const { return PortFD; }

    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool saveConfigItems(FILE *fp);

    /**
     * Refresh the list of system ports
     */
    bool Refresh(bool silent = false);

  protected:
    /**
     * \brief Connect to serial port device. Default parameters are 8 bits, 1 stop bit, no parity.
     * Override if different from default.
     * \param port Port to connect to.
     * \param baud Baud rate
     * \return True if connection is successful, false otherwise
     * \warning Do not call this function directly, it is called by Connection::Serial Connect() function.
     */
    virtual bool Connect(const char *port, uint32_t baud);

    virtual bool processHandshake();

    // Device physical port
    ITextVectorProperty PortTP;
    IText PortT[1];

    ISwitch BaudRateS[6];
    ISwitchVectorProperty BaudRateSP;

    ISwitch AutoSearchS[2];
    ISwitchVectorProperty AutoSearchSP;

    ISwitch *SystemPortS = nullptr;
    ISwitchVectorProperty SystemPortSP;

    ISwitch RefreshS[1];
    ISwitchVectorProperty RefreshSP;

    int PortFD = -1;
};
}
