#pragma once

#include "abstractbaseclient_p.h"
#include "indililxml.h"

#include <tcpsocket.h>

namespace INDI
{

#ifdef ENABLE_INDI_SHARED_MEMORY
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

class TcpSocketSharedBlobs : public TcpSocket
{
public:
    void readyRead() override;

    ClientSharedBlobs sharedBlobs;
};
#endif

class BaseDevice;


class BaseClientPrivate : public AbstractBaseClientPrivate
{
    public:
        BaseClientPrivate(BaseClient *parent);
        virtual ~BaseClientPrivate();

    public:
        bool connectToHostAndWait(std::string hostname, unsigned short port);

    public:
        size_t sendData(const void *data, size_t size) override;

#ifdef ENABLE_INDI_SHARED_MEMORY
        TcpSocketSharedBlobs clientSocket;
#else
        TcpSocket clientSocket;
#endif
        int exitCode;
        LilXmlParser xmlParser;
};

}
