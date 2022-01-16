#ifndef SERVER_MOCK_H_
#define SERVER_MOCK_H_ 1

#include <string>
#include <unistd.h>

class IndiClientMock;

/**
 * Instanciate a fake indi server
 */
class ServerMock {
    int fd;

public:
    ServerMock();
    virtual ~ServerMock();

    // Start the listening socket that will receive driver upon their starts
    void listen(int tcpPort);
    void listen(const std::string & unixPath);

    void accept(IndiClientMock & into);

    void close();
};


#endif // SERVER_MOCK_H_