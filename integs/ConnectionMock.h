#ifndef CONNECTION_MOCK_H_
#define CONNECTION_MOCK_H_ 1

#include <string>
#include <list>

class SharedBuffer;

/**
 * Interface to a mocked connection
 */
class ConnectionMock {
    int fds[2];
    std::list<int> receivedFds;
    bool bufferReceiveAllowed;
    ssize_t read(void * buff, size_t count);
    char readChar(const std::string & expected);

    void release();
public:
    ConnectionMock();
    ~ConnectionMock();
    void setFds(int rd, int wr);

    void expect(const std::string & content);
    void expectXml(const std::string & xml);
    void send(const std::string & content);
    void send(const std::string & content, const SharedBuffer & buff);
    void send(const std::string & content, const SharedBuffer ** buffers);

    void allowBufferReceive(bool state);
    void expectBuffer(SharedBuffer & fd);
};


#endif // CONNECTION_MOCK_H_