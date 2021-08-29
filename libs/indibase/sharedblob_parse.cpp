#include <map>
#include <sstream>
#include <mutex>
#include <cstdint>

#include <unistd.h>

#include "indidevapi.h"
#include "sharedblob_parse.h"

namespace INDI {

static std::mutex attachedBlobMutex;
static std::map<std::string, int> receivedFds;
static uint64_t idGenerator = rand();


std::string allocateBlobUid(int fd) {
    std::stringstream ss;
    ss << idGenerator;

    std::string id = ss.str();

    std::lock_guard<std::mutex> lock(attachedBlobMutex);
    fprintf(stderr, "received fd: %d\n", fd);
    receivedFds[id] = fd;
    return id;
}

void * attachBlobByUid(const std::string & identifier, size_t size)
{
    int fd;
    {
        std::lock_guard<std::mutex> lock(attachedBlobMutex);
        auto where = receivedFds.find(identifier);
        if (where == receivedFds.end()) {
            return nullptr;
        }
        fd = where->second;
        receivedFds.erase(where);
    }

    return IDSharedBlobAttach(fd, size);
}

void releaseBlobUids(const std::vector<std::string> & blobs)
{
    std::vector<int> toDestroy;
    {
        std::lock_guard<std::mutex> lock(attachedBlobMutex);
        for(auto id : blobs) {
            auto idPos = receivedFds.find(id);
            if (idPos != receivedFds.end()) {
                toDestroy.push_back(idPos->second);
                receivedFds.erase(idPos);
            }
        }
    }

    for(auto fd : toDestroy) {
        ::close(fd);
    }
}

}