#pragma once

#include <vector>
#include <map>
#include <list>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <set>
#include <thread>
#include <cstdint>
#include <functional>

#include <lilxml.h>

#ifdef _WINDOWS
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#endif

#include "watchdeviceproperty.h"

#include "abstractbaseclient_p.h"

namespace INDI
{

class BaseDevice;


class BaseClientPrivate : public AbstractBaseClientPrivate
{
    public:
        BaseClientPrivate(BaseClient *parent);
        virtual ~BaseClientPrivate();

    public:
        size_t sendData(const void *data, size_t size) override;
        void sendString(const char *fmt, ...);

    public:
        bool disconnect(int exit_code);

    public:
        void listenINDI();

    public:
        bool establish(const std::string &target);

        // Add an attribute for access to shared blobs
        bool parseAttachedBlobs(const INDI::LilXmlElement &root, std::vector<std::string> &blobs);

    public:
        std::list<int> incomingSharedBuffers; /* During reception, fds accumulate here */
        bool unixSocket {false};

#ifdef _WINDOWS
        SOCKET sockfd;
#else
        int sockfd {-1};
        int receiveFd {-1};
        int sendFd {-1};
#endif

        std::atomic_bool sAboutToClose;
        std::mutex sSocketBusy;
        std::condition_variable sSocketChanged;
        int sExitCode;

};

}
