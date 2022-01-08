#ifndef CONNECTION_MOCK_H_
#define CONNECTION_MOCK_H_ 1

#include <string>

/**
 * Interface to a mocked connection
 */
class ConnectionMock {
    int fds[2];
    char readChar(const std::string & expected) const;
public:
    ConnectionMock();
    void setFds(int rd, int wr);

    void expect(const std::string & content);
    void expectXml(const std::string & xml);
    void send(const std::string & content);
};


#endif // CONNECTION_MOCK_H_