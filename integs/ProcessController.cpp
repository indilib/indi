#include <system_error>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "ProcessController.h"

ProcessController::ProcessController() {
    pid = -1;
    cmd = "<unstarted>";
    status = 0;
}

ProcessController::~ProcessController() {
}


void ProcessController::start(const std::string & path, const std::vector<std::string>  & args)
{
    if (pid != -1) {
        throw std::runtime_error(cmd + " already running");
    }

    cmd = path;

    int argCount = 0;
    argCount = args.size();
    const char * fullArgs[argCount + 2];
    fullArgs[0] = path.c_str();
    for(int i = 0; i < argCount ; i++) {
        fullArgs[i + 1] = args[i].c_str();
    }
    fullArgs[1 + argCount] = nullptr;
    fprintf(stderr, "Running ");
    for(int i = 0; i < 2+ argCount ; ++i) {
        fprintf(stderr, " %s", fullArgs[i] ? fullArgs[i] : "<null>");
    }
    fprintf(stderr, "\n");
    // Do fork & exec for the indiserver binary
    pid = fork();
    if (pid == -1) {
        throw std::system_error(errno, std::generic_category(), "fork error");
    }
    if (pid == 0) {
        std::string error = "exec " + path;

        execv(fullArgs[0], (char * const *) fullArgs);

        // Child goes here....
        perror(error.c_str());
        ::exit(1);
    }
}

void ProcessController::waitProcessEnd(int exitCode) {
    join();
    expectExitCode(exitCode);
}

void ProcessController::join() {
    if (pid == -1) {
        return;
    }
    pid_t waited = waitpid(pid, &status, 0);
    if (waited == -1) {
        throw std::system_error(errno, std::generic_category(), "waitpid error");
    }
    pid = -1;
}

void ProcessController::expectDone() {
    if (pid == -1) {
        return;
    }

    pid_t waited = waitpid(pid, &status, WNOHANG);
    if (waited == -1) {
        throw std::system_error(errno, std::generic_category(), "waitpid error");
    }
    if (waited == 0) {
        // No child found... Still running
        throw std::runtime_error("Process " + cmd + " not done");
    }

    pid = -1;
}

void ProcessController::expectAlive() {
    if (pid != -1) {
        pid_t waited = waitpid(pid, &status, WNOHANG);
        if (waited == -1) {
            throw std::system_error(errno, std::generic_category(), "waitpid error");
        }
        if (waited != 0) {
            // Process terminated
            pid = -1;
            return;
        }
    }

    throw std::runtime_error("Process terminated unexpectedly");
}

void ProcessController::expectExitCode(int e) {
    expectDone();
    int actual = WEXITSTATUS(status);
    if (actual != e) {
        throw std::runtime_error("Wrong exit code of indiserver: got " + std::to_string(actual) + " - expecting: " + std::to_string(e));
    }
}
