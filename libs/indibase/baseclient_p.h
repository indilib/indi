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

namespace INDI
{

class BaseDevice;

struct BLOBMode
{
    std::string device;
    std::string property;
    BLOBHandling blobMode;
};

class BaseClientPrivate
{
    public:
        BaseClientPrivate(BaseClient *parent);
        virtual ~BaseClientPrivate();

    public:
        bool connect();
        bool disconnect(int exit_code);

        /** @brief Connect/Disconnect to INDI driver
         *  @param status If true, the client will attempt to turn on CONNECTION property within the driver (i.e. turn on the device).
         *                Otherwise, CONNECTION will be turned off.
         *  @param deviceName Name of the device to connect to.
         */
        void setDriverConnection(bool status, const char *deviceName);

        size_t sendData(const void *data, size_t size);
        void sendString(const char *fmt, ...);

    public:
        void listenINDI();
        /** @brief clear Clear devices and blob modes */
        void clear();

    public:
        BLOBMode *findBLOBMode(const std::string &device, const std::string &property);
        void enableDirectBlobAccess(const char * dev = nullptr, const char * prop = nullptr);
    private:
        bool isDirectBlobAccess(const std::string &dev, const std::string &prop) const;

    public:
        /** @brief Dispatch command received from INDI server to respective devices handled by the client */
        int dispatchCommand(const INDI::LilXmlElement &root, char *errmsg);

        /** @brief Remove device */
        int deleteDevice(const char *devName, char *errmsg);

        /** @brief Delete property command */
        int delPropertyCmd(const INDI::LilXmlElement &root, char *errmsg);

        /**  Process messages */
        int messageCmd(const INDI::LilXmlElement &root, char *errmsg);

    private:
        std::list<int> incomingSharedBuffers; /* During reception, fds accumulate here */
        bool unixSocket {false};

        bool establish(const std::string &target);

        // Add an attribute for access to shared blobs
        bool parseAttachedBlobs(const INDI::LilXmlElement &root, std::vector<std::string> &blobs);

    public:
        BaseClient *parent;

#ifdef _WINDOWS
        SOCKET sockfd;
#else
        int sockfd {-1};
        int receiveFd {-1};
        int sendFd {-1};
#endif

        WatchDeviceProperty watchDevice;

        std::list<BLOBMode> blobModes;
        std::map<std::string, std::set<std::string>> directBlobAccess;

        std::string cServer {"localhost"};
        uint32_t cPort      {7624};

        std::atomic_bool sConnected {false};
        std::atomic_bool sAboutToClose;
        std::mutex sSocketBusy;
        std::condition_variable sSocketChanged;
        int sExitCode;
        bool verbose {false};

        // Parse & FILE buffers for IO

        uint32_t timeout_sec {3}, timeout_us {0};
};

}
