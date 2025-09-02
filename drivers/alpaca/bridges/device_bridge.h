/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Alpaca Device Bridge Interface

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

#pragma once

#include "basedevice.h"
#include "indiproperty.h"
#include <string>
#include <httplib.h>

class IDeviceBridge
{
    public:
        virtual ~IDeviceBridge() = default;

        // Device information
        virtual std::string getDeviceType() const = 0;
        virtual std::string getDeviceName() const = 0;
        virtual int getDeviceNumber() const = 0;
        virtual std::string getUniqueID() const = 0;

        // Handle Alpaca API request
        virtual void handleRequest(const std::string &method,
                                   const httplib::Request &req,
                                   httplib::Response &res) = 0;

        // Update from INDI property
        virtual void updateProperty(INDI::Property property) = 0;

        // Common Alpaca API methods
        virtual void handleConnected(const httplib::Request &req, httplib::Response &res) = 0;
        virtual void handleName(const httplib::Request &req, httplib::Response &res) = 0;
        virtual void handleDescription(const httplib::Request &req, httplib::Response &res) = 0;
        virtual void handleDriverInfo(const httplib::Request &req, httplib::Response &res) = 0;
        virtual void handleDriverVersion(const httplib::Request &req, httplib::Response &res) = 0;
        virtual void handleInterfaceVersion(const httplib::Request &req, httplib::Response &res) = 0;
};
