#include <system_error>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <signal.h>
#include <dirent.h>
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


int ProcessController::getOpenFdCount() {
    if (pid == -1) {
        throw std::runtime_error(cmd + " is done - cannot check open fd count");
    }
#ifdef __linux__
    std::string path = "/proc/" + std::to_string(pid) + "/fd";

    int count = 0;
    auto dirp = opendir(path.c_str());
    if (dirp == nullptr) {
        throw std::system_error(errno, std::generic_category(), "opendir error: " + path);
    }

    for(auto dp = readdir(dirp); dp != nullptr; dp = readdir(dirp)) {
        std::string name = dp->d_name;
        if (name.length() == 0 || name[0] == '.') {
            continue;
        }
        count++;
    }
    closedir(dirp);
    return count;
#else
    return 0;
#endif
}

void ProcessController::checkOpenFdCount(int expected, const std::string & msg) {
#ifdef __linux__
    int count = getOpenFdCount();
    if (count != expected) {
        throw std::runtime_error(msg + " " + cmd + " open file count is " + std::to_string(count) + " - expected: " + std::to_string(expected));
    }
#else
    (void)expected;
    (void)msg;
#endif
}

void ProcessController::start(const std::string & path, const std::vector<std::string>  & args)
{
    if (pid != -1) {
        throw std::runtime_error(cmd + " already running");
    }

    cmd = path;

    int argCount = 0;
    argCount = args.size();
    std::vector<const char *> fullArgs(argCount + 2);
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
        // TODO : Close all file descriptor
        execv(fullArgs[0], (char * const *) fullArgs.data());

        // Child goes here....
        perror(error.c_str());
        ::exit(1);
    }
}

void ProcessController::waitProcessEnd(int exitCode) {
    join();
    expectExitCode(exitCode);
}

void ProcessController::kill() {
    if (pid == -1) {
        return;
    }
    ::kill(pid, SIGKILL);
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

    if (!WIFEXITED(status)) {
        if (WIFSIGNALED(status)) {
            throw std::runtime_error(cmd + " got signal " + strsignal(WTERMSIG(status)));
        }
        // Not sure this is possible at all
        throw std::runtime_error(cmd + " exited abnormally");
    }
    int actual = WEXITSTATUS(status);
    if (actual != e) {
        throw std::runtime_error("Wrong exit code for " + cmd + ": got " + std::to_string(actual) + " - expecting: " + std::to_string(e));
    }
}
