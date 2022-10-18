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

#ifndef _WINDOWS
class EventFd
{
    public:
        EventFd()
        {
            if (socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd) < 0)
            {
                IDLog("Notify pipe: %s\n", strerror(errno));
            }
        }

        ~EventFd()
        {
            close(pipefd[0]);
            close(pipefd[1]);
        }

    public:
        void wakeUp()
        {
            size_t c = 1;
            // wakeup 'select' function
            ssize_t ret = write(pipefd[1], &c, sizeof(c));

            if (ret != sizeof(c))
            {
                IDLog("The socket cannot be woken up.\n");
            }
        }

        int selectFd() const
        {
            return pipefd[0];
        }

    private:
        int pipefd[2] = {-1, -1};
};
#endif

class ClientSharedBlobs
{
    public:
        class Blobs : public std::vector<std::string>
        {
            public:
                ~Blobs();
        };

    public:
        void enableDirectBlobAccess(const char * dev, const char * prop);
        void disableDirectBlobAccess();

        bool parseAttachedBlobs(const INDI::LilXmlElement &root, Blobs &blobs);
        bool isDirectBlobAccess(const std::string &dev, const std::string &prop) const;

        static bool hasDirectBlobAccessEntry(const std::map<std::string, std::set<std::string>> &directBlobAccess,
                                            const std::string &dev, const std::string &prop);

        void addIncomingSharedBuffer(int fd);

        void clear();

    private:
        std::list<int> incomingSharedBuffers;
        std::map<std::string, std::set<std::string>> directBlobAccess;
};

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

    public:
        bool unixSocket {false};

#ifdef _WINDOWS
        SOCKET sockfd;
#else
        int sockfd {-1};
        EventFd eventFd;
#endif

        std::atomic_bool sAboutToClose;
        std::mutex sSocketBusy;
        std::condition_variable sSocketChanged;
        int sExitCode;

        ClientSharedBlobs sharedBlobs;
};

}
