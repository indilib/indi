#include <string>

void setupSigPipe();

int unixSocketListen(const std::string & path);
int unixSocketConnect(const std::string & unixAddr, bool failAllowed = false);
void unixSocketSendFds(int fd, int count, int * fds);
void unixSocketRecvFds(int fd, int count, int * fds);

int tcpSocketListen(int port);
int tcpSocketConnect(const std::string & unixAddr, int port, bool failAllowed = false);

int socketAccept(int fd);

std::string getTestExePath(const std::string & basename);
