#include <system_error>
#include <sys/wait.h>

#include "utils.h"
#include "IndiServerController.h"

void IndiServerController::start(const char * * args) {
    int argCount = 0;
    if (!args) {
        argCount = 0;
    } else {
        while(args[argCount]) {
            argCount++;
        }
    }
    const char * fullArgs[argCount + 2];
    fullArgs[0] = "indiserver";
    for(int i = 0; i < argCount ; i++) {
        fullArgs[i + 1] = args[i];
    }
    fullArgs[1 + argCount] = nullptr;

    // Do fork & exec for the indiserver binary
    indiServerPid = fork();
    if (indiServerPid == -1) {
        throw std::runtime_error("fork error");
    }
    if (indiServerPid == 0) {
        execv("../indiserver", (char * const *) fullArgs);
        // Child goes here....
        perror("indiserver");
        ::exit(1);
    }
}

void IndiServerController::waitProcessEnd(int exitCode) {
    int wstatus;
    pid_t ret = waitpid(indiServerPid, &wstatus, 0);
    if (ret == -1) {
        throw std::system_error(errno, std::generic_category(), "waitpid");
    }
    if (!WIFEXITED(wstatus)) {
        throw std::runtime_error("unclean exit of indiserver");
    }
    fprintf(stderr, "indiserver exit code: %d\n", WEXITSTATUS(wstatus));
    if (WEXITSTATUS(wstatus) != exitCode) {
        throw std::runtime_error("Wrong exit code of indiserver");
    }
}