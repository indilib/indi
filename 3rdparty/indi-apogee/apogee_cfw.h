/*******************************************************************************
 Apogee Filter Wheel Driver

 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include <iostream>
#include <memory>

#include <indifilterwheel.h>

#include <ApogeeFilterWheel.h>
#include <FindDeviceEthernet.h>
#include <FindDeviceUsb.h>

class ApogeeCFW : public INDI::FilterWheel
{
    public:
        ApogeeCFW();

        const char *getDefaultName() override;

        void ISGetProperties(const char *dev) override;
        bool initProperties() override;
        bool updateProperties() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    protected:

        bool Connect() override;
        bool Disconnect() override;

        void TimerHit() override;

        void debugTriggered(bool enabled) override;
        bool saveConfigItems(FILE *fp) override;

    private:
        std::unique_ptr<ApogeeFilterWheel> ApgCFW;

        ///////////////////////////////////////////////////////////////////////////
        /// Properties
        //////////////////////////////////////////////////////////////////////////

        // USB or Ethernet?
        ISwitchVectorProperty PortTypeSP;
        ISwitch PortTypeS[2];
        enum
        {
            PORT_USB,
            PORT_NETWORK
        };

        // Subnet/Address for Ethernet connections
        ITextVectorProperty NetworkInfoTP;
        IText NetworkInfoT[2] {};
        enum
        {
            NETWORK_SUBNET,
            NETWORK_ADDRESS
        };

        // Filter Information
        ITextVectorProperty FilterInfoTP;
        IText FilterInfoT[2] {};
        enum
        {
          INFO_NAME,
          INFO_FIRMWARE,
        };

        // Filter Type
        ISwitchVectorProperty FilterTypeSP;
        ISwitch FilterTypeS[5];
        enum
        {
            TYPE_UNKNOWN,
            TYPE_FW50_9R,
            TYPE_FW50_7S,
            TYPE_AFW50_10S,
            TYPE_AFW31_17R
        };

        ///////////////////////////////////////////////////////////////////////////
        /// Filter Info
        //////////////////////////////////////////////////////////////////////////
        std::string ioInterface;
        std::string subnet;
        std::string firmwareRev;
        std::string model;
        FindDeviceEthernet look4cam;
        FindDeviceUsb lookUsb;

        ///////////////////////////////////////////////////////////////////////////
        /// Utility
        //////////////////////////////////////////////////////////////////////////
        std::vector<std::string> MakeTokens(const std::string &str, const std::string &separator);
        std::string GetItemFromFindStr(const std::string &msg, const std::string &item);
        std::string GetUsbAddress(const std::string &msg);
        std::string GetEthernetAddress(const std::string &msg);
        std::string GetIPAddress(const std::string &msg);
        std::string GetModel(const std::string &msg);
        bool IsDeviceFilterWheel(const std::string &msg);

        ///////////////////////////////////////////////////////////////////////////
        /// Filter Wheel Query & Set
        //////////////////////////////////////////////////////////////////////////
        virtual int QueryFilter() override;
        virtual bool SelectFilter(int) override;
};
