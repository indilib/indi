#ifndef DRIVER_MOCK_H_
#define DRIVER_MOCK_H_ 1

#include <string>
#include <unistd.h>

#include "ConnectionMock.h"

/**
 * Interface to the the fake driver that forward its indi communication pipes
 * to the test process.
 *
 */
class DriverMock {
    std::string abstractPath;
    int serverConnection;
    int driverConnection;

    int driverFds[2];
public:
    DriverMock();
    virtual ~DriverMock();

    // Start the listening socket that will receive driver upon their starts
    void setup();
    void unsetup();

    void waitEstablish();

    void terminateDriver();

    ConnectionMock cnx;
};


#endif // DRIVER_MOCK_H_