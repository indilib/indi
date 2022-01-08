#ifndef INDI_SERVER_CONTROLLER_H_
#define INDI_SERVER_CONTROLLER_H_ 1

#include <string>
#include <unistd.h>

/**
 * Interface to the indiserver process.
 *
 * Allows starting it, sending it signals and inspecting exit code
 */
class IndiServerController {

public:
    // pid of the indiserver
    pid_t indiServerPid;

    void start(const char ** args = nullptr);

    // Wait for process end, expecting the given exitcode
    void waitProcessEnd(int exitCode);
};


#endif // INDI_SERVER_CONTROLLER_H_