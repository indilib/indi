//
// Created by not7cd on 04/01/19.
//

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

        CURL *handle = nullptr;
    };

}
