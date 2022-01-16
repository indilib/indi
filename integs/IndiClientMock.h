#ifndef INDI_CLIENT_MOCK_H_
#define INDI_CLIENT_MOCK_H_ 1

#include <string>

#include "ConnectionMock.h"

/**
 * Interface to a mocked connection to indi server
 */
class IndiClientMock {
    int fd;
public:
    ConnectionMock cnx;
    IndiClientMock();
    virtual ~IndiClientMock();
    void connectUnix(const std::string & path= "/tmp/indiserver");
    void connectTcp(const std::string & host = "127.0.0.1", int port = 7624);
    void associate(int fd);

    // This ensure that previous orders were received
    void ping();

    void close();
};


#endif // INDI_CLIENT_MOCK_H_