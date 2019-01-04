//
// Created by not7cd on 04/01/19.
//

#include "connectioncurl.h"

#include <cstring>

namespace Connection {
    Curl::Curl(INDI::DefaultDevice *dev) : Interface(dev, CONNECTION_CUSTOM) {
        curl_global_init(CURL_GLOBAL_ALL);

        IUFillText(&AddressT[0], "ADDRESS", "Address", "");
        IUFillText(&AddressT[1], "PORT", "Port", "");
        IUFillTextVector(&AddressTP, AddressT, 2, getDeviceName(), "DEVICE_ADDRESS", "Server", CONNECTION_TAB,
                         IP_RW, 60, IPS_IDLE);
    }

    Curl::~Curl() {
        curl_global_cleanup();
    }

    bool Curl::Connect() {
        if (AddressT[0].text == nullptr || AddressT[0].text[0] == '\0' || AddressT[1].text == nullptr ||
            AddressT[1].text[0] == '\0') {
            LOG_ERROR("Error! Server address is missing or invalid.");
            return false;
        }

        const char *hostname = AddressT[0].text;
        const char *port = AddressT[1].text;

        LOGF_INFO("Creating HTTP handle for %s@%s", hostname, port);
        if (handle != nullptr) {
            LOG_WARN("Found old handle, recreating");
            curl_easy_cleanup(handle);
        }
        handle = curl_easy_init();
        if (handle == nullptr) {
            LOG_ERROR("Can't create HTTP handle");
            return false;
        }

        LOG_DEBUG("Handle creation successful, attempting handshake...");
        bool rc = Handshake();

        if (rc) {
            LOGF_INFO("%s is online.", getDeviceName());
//            m_Device->saveConfig(true, "DEVICE_ADDRESS");
        } else
            LOG_DEBUG("Handshake failed.");

        return rc;
    }

    bool Curl::Disconnect() {
        curl_easy_cleanup(handle);
        return true;
    }

    void Curl::Activated() {
        m_Device->defineText(&AddressTP);
//        m_Device->loadConfig(true, "DEVICE_ADDRESS");
    }

    void Curl::Deactivated() {
        m_Device->deleteProperty(AddressTP.name);
    }

    bool Curl::saveConfigItems(FILE *fp) {
        IUSaveConfigText(fp, &AddressTP);
        return true;
    }

    void Curl::setDefaultHost(const char *addressHost) {
        IUSaveText(&AddressT[0], addressHost);
    }

    void Curl::setDefaultPort(uint32_t addressPort) {
        char portStr[8];
        snprintf(portStr, 8, "%d", addressPort);
        IUSaveText(&AddressT[1], portStr);
    }

    bool Curl::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) {
        if (!strcmp(dev, m_Device->getDeviceName())) {
            if (!strcmp(name, AddressTP.name)) {
                IUUpdateText(&AddressTP, texts, names, n);
                AddressTP.s = IPS_OK;
                IDSetText(&AddressTP, nullptr);
                return true;
            }
        }

        return false;
    }
}


