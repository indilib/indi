/*
 Starbook mount driver

 Copyright (C) 2018 Norbert Szulc (not7cd)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

#pragma once

#include <connectionplugins/connectioninterface.h>
#include <string>
#include <cstdlib>
#include <curl/curl.h>
#include <inditelescope.h>


namespace Connection {
    class Curl : public Interface {
    public:
        explicit Curl(INDI::DefaultDevice *dev);

        ~Curl() override;

        bool Connect() override;

        bool Disconnect() override;

        void Activated() override;

        void Deactivated() override;

        std::string name() override { return "CONNECTION_OTHER"; }

        std::string label() override { return "HTTP"; }

        const char *host() const { return AddressT[0].text; }

        uint32_t port() const { return static_cast<uint32_t>(atoi(AddressT[1].text)); }

        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

        bool saveConfigItems(FILE *fp) override;

        void setDefaultHost(const char *addressHost);

        void setDefaultPort(uint32_t addressPort);

        CURL *getHandle() const { return handle; }

    protected:
        ITextVectorProperty AddressTP;
        IText AddressT[2]{};

        const unsigned long HANDLE_TIMEOUT = 2;

        CURL *handle = nullptr;

        void SetupHandle() const;
    };

}
