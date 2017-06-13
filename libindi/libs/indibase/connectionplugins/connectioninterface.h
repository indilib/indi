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

#include "indidevapi.h"

#include <functional>
#include <string>

namespace INDI
{
class DefaultDevice;
}

namespace Connection
{
class Interface
{
  public:
    virtual bool Connect() = 0;

    virtual bool Disconnect() = 0;

    virtual void Activated() = 0;

    virtual void Deactivated() = 0;

    virtual std::string name() = 0;

    virtual std::string label() = 0;

    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);

    virtual bool saveConfigItems(FILE *fp);

    void registerHandshake(std::function<bool()> callback);

  protected:
    Interface(INDI::DefaultDevice *dev);
    virtual ~Interface();

    const char *getDeviceName();
    std::function<bool()> Handshake;
    INDI::DefaultDevice *device = NULL;
};
}
