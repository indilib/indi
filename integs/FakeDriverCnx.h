#ifndef FAKEDRIVER_H_
#define FAKEDRIVER_H_ 1

#include <string>
#include <unistd.h>

/**
 * Interface to the fake driver that forward its indi communication pipes
 * to the test process.
 *
 */
class FakeDriverCnx {
    std::string abstractPath;
    int serverConnection;
    int driverConnection;

    int driverFds[2];
public:
    void setup();

    void waitEstablish();

    void terminate();

    void expect(const std::string & content);
    void send(const std::string & content);
};


#endif // FAKEDRIVER_H_