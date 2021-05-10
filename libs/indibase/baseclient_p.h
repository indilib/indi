#pragma once

#include <vector>
#include <map>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <set>
#include <thread>
#include <cstdint>

#include <lilxml.h>

#ifdef _WINDOWS
#include <WinSock2.h>
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#endif

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

public:
    /** @brief Dispatch command received from INDI server to respective devices handled by the client */
    int dispatchCommand(XMLEle *root, char *errmsg);

    /** @brief Remove device */
    int deleteDevice(const char *devName, char *errmsg);

    /** @brief Delete property command */
    int delPropertyCmd(XMLEle *root, char *errmsg);

    /** @brief Find and return a particular device */
    INDI::BaseDevice *findDev(const char *devName, char *errmsg);
    /** @brief Add a new device */
    INDI::BaseDevice *addDevice(XMLEle *dep, char *errmsg);
    /** @brief Find a device, and if it doesn't exist, create it if create is set to 1 */
    INDI::BaseDevice *findDev(XMLEle *root, int create, char *errmsg);

    /**  Process messages */
    int messageCmd(XMLEle *root, char *errmsg);

public:
    BaseClient *parent;

#ifdef _WINDOWS
    SOCKET sockfd;
#else
    int sockfd {-1};
    int receiveFd {-1};
    int sendFd {-1};
#endif

    std::vector<INDI::BaseDevice *> cDevices;
    std::vector<std::string> cDeviceNames;
    std::vector<BLOBMode *> blobModes;
    std::map<std::string, std::set<std::string>> cWatchProperties;

    std::string cServer;
    uint32_t cPort;
    std::atomic_bool sConnected;
    std::atomic_bool sAboutToClose;
    std::mutex sSocketBusy;
    std::condition_variable sSocketChanged;
    int sExitCode;
    bool verbose;

    // Parse & FILE buffers for IO

    uint32_t timeout_sec, timeout_us;
};

}
