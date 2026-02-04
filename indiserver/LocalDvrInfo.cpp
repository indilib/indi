/* INDI Server for protocol version 1.7.
 * Copyright (C) 2007 Elwood C. Downey <ecdowney@clearskyinstitute.com>
                 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>
                 2022 Ludovic Pollet
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "LocalDrvInfo.hpp"
#include "Utils.hpp"
#include "Msg.hpp"
#include "Constants.hpp"
#include "CommandLineArgs.hpp"

#include "Fifo.hpp"
#include <sys/socket.h>
#include <fcntl.h>
#include <libgen.h>
#include <unistd.h>

/* start the given local INDI driver process.
 * exit if trouble.
 */
void LocalDvrInfo::start()
{
    Msg *mp;
    int rp[2], wp[2], ep[2];
    int ux[2];
    int pid;

#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "STARTING \"%s\"\n", name.c_str());
    fflush(stderr);
#endif

    /* build three pipes: r, w and error*/
    if (useSharedBuffer)
    {
        // FIXME: lots of FD are opened by indiserver. FD_CLOEXEC is a must + check other fds
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, ux) == -1)
        {
            log(fmt("socketpair: %s\n", strerror(errno)));
            Bye();
        }
    }
    else
    {
        if (pipe(rp) < 0)
        {
            log(fmt("read pipe: %s\n", strerror(errno)));
            Bye();
        }
        if (pipe(wp) < 0)
        {
            log(fmt("write pipe: %s\n", strerror(errno)));
            Bye();
        }
    }
    if (pipe(ep) < 0)
    {
        log(fmt("stderr pipe: %s\n", strerror(errno)));
        Bye();
    }

    /* fork&exec new process */
    pid = fork();
    if (pid < 0)
    {
        log(fmt("fork: %s\n", strerror(errno)));
        Bye();
    }
    if (pid == 0)
    {
        /* child: exec name */
        int fd;

        /* rig up pipes */
        if (useSharedBuffer)
        {
            // For unix socket, the same socket end can be used for both read & write
            dup2(ux[0], 0); /* driver stdin reads from ux[0] */
            dup2(ux[0], 1); /* driver stdout writes to ux[0] */
            ::close(ux[0]);
            ::close(ux[1]);
        }
        else
        {
            dup2(wp[0], 0); /* driver stdin reads from wp[0] */
            dup2(rp[1], 1); /* driver stdout writes to rp[1] */
        }
        dup2(ep[1], 2); /* driver stderr writes to e[]1] */
        for (fd = 3; fd < 100; fd++)
        {
            (void)::close(fd);
        }
        if (!envDev.empty())
        {
            setenv("INDIDEV", envDev.c_str(), 1);
        }
        /* Only reset environment variable in case of FIFO */
        else if (fifoHandle)
        {
            unsetenv("INDIDEV");
        }
        if (!envConfig.empty())
        {
            setenv("INDICONFIG", envConfig.c_str(), 1);
        }
        else if (fifoHandle)
        {
            unsetenv("INDICONFIG");
        }
        if (!envSkel.empty())
        {
            setenv("INDISKEL", envSkel.c_str(), 1);
        }
        else if (fifoHandle)
        {
            unsetenv("INDISKEL");
        }
        std::string executable;
        if (!envPrefix.empty())
        {
            setenv("INDIPREFIX", envPrefix.c_str(), 1);
#if defined(OSX_EMBEDED_MODE)
            executable = envPrefix + "/Contents/MacOS/" + name;
#elif defined(__APPLE__)
            executable = envPrefix + "/" + name;
#else
            executable = envPrefix + "/bin/" + name;
#endif

            fprintf(stderr, "%s\n", executable.c_str());

            execlp(executable.c_str(), name.c_str(), NULL);
        }
        else
        {
            execlp(name.c_str(), name.c_str(), NULL);
        }

#ifdef OSX_EMBEDED_MODE
        fprintf(stderr, "FAILED \"%s\"\n", name.c_str());
        fflush(stderr);
#endif
        log(fmt("execlp %s: %s\n", executable.c_str(), strerror(errno)));
        _exit(1); /* parent will notice EOF shortly */
    }

    if (useSharedBuffer)
    {
        /* don't need child's other socket end */
        ::close(ux[0]);

        /* record pid, io channels, init lp and snoop list */
        setFds(ux[1], ux[1]);
        rp[0] = ux[1];
        wp[1] = ux[1];
    }
    else
    {
        /* don't need child's side of pipes */
        ::close(wp[0]);
        ::close(rp[1]);

        /* record pid, io channels, init lp and snoop list */
        setFds(rp[0], wp[1]);
    }

    ::close(ep[1]);

    // Watch pid
    this->pid = pid;
    this->pidwatcher.set(pid);
    this->pidwatcher.start();

    // Watch input on efd
    this->efd = ep[0];
    fcntl(this->efd, F_SETFL, fcntl(this->efd, F_GETFL, 0) | O_NONBLOCK);
    this->eio.start(this->efd, ev::READ);

    /* first message primes driver to report its properties -- dev known
     * if restarting
     */
    if (userConfigurableArguments->verbosity > 0)
        log(fmt("pid=%d rfd=%d wfd=%d efd=%d\n", pid, rp[0], wp[1], ep[0]));

    XMLEle *root = addXMLEle(NULL, "getProperties");
    addXMLAtt(root, "version", TO_STRING(INDIV));
    mp = new Msg(nullptr, root);

    // pushmsg can kill mp. do at end
    pushMsg(mp);
}

LocalDvrInfo::LocalDvrInfo(): DvrInfo(true)
{
    eio.set<LocalDvrInfo, &LocalDvrInfo::onEfdEvent>(this);
    pidwatcher.set<LocalDvrInfo, &LocalDvrInfo::onPidEvent>(this);
}

LocalDvrInfo::LocalDvrInfo(const LocalDvrInfo &model):
    DvrInfo(model),
    envDev(model.envDev),
    envConfig(model.envConfig),
    envSkel(model.envSkel),
    envPrefix(model.envPrefix)
{
    eio.set<LocalDvrInfo, &LocalDvrInfo::onEfdEvent>(this);
    pidwatcher.set<LocalDvrInfo, &LocalDvrInfo::onPidEvent>(this);
}

LocalDvrInfo::~LocalDvrInfo()
{
    closeEfd();
    if (pid != 0)
    {
        kill(pid, SIGKILL); /* libev insures there will be no zombies */
        pid = 0;
    }
    closePid();
}

LocalDvrInfo * LocalDvrInfo::clone() const
{
    return new LocalDvrInfo(*this);
}

void LocalDvrInfo::closeEfd()
{
    ::close(efd);
    efd = -1;
    eio.stop();
}

void LocalDvrInfo::closePid()
{
    pid = 0;
    pidwatcher.stop();
}

void LocalDvrInfo::onEfdEvent(ev::io &, int revents)
{
    if (EV_ERROR & revents)
    {
        int sockErrno = readFdError(this->efd);
        if (sockErrno)
        {
            log(fmt("Error on stderr: %s\n", strerror(sockErrno)));
            closeEfd();
        }
        return;
    }

    if (revents & EV_READ)
    {
        ssize_t nr;

        /* read more */
        nr = read(efd, errbuff + errbuffpos, sizeof(errbuff) - errbuffpos);
        if (nr <= 0)
        {
            if (nr < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK) return;

                log(fmt("stderr %s\n", strerror(errno)));
            }
            else
                log("stderr EOF\n");
            closeEfd();
            return;
        }
        errbuffpos += nr;

        for(int i = 0; i < errbuffpos; ++i)
        {
            if (errbuff[i] == '\n')
            {
                log(fmt("%.*s\n", (int)i, errbuff));
                i++;                               /* count including nl */
                errbuffpos -= i;                       /* remove from nexbuf */
                memmove(errbuff, errbuff + i, errbuffpos); /* slide remaining to front */
                i = -1;                            /* restart for loop scan */
            }
        }
    }
}

void LocalDvrInfo::onPidEvent(ev::child &, int revents)
{
    if (revents & EV_CHILD)
    {
        if (WIFEXITED(pidwatcher.rstatus))
        {
            log(fmt("process %d exited with status %d\n", pid, WEXITSTATUS(pidwatcher.rstatus)));
        }
        else if (WIFSIGNALED(pidwatcher.rstatus))
        {
            int signum = WTERMSIG(pidwatcher.rstatus);
            log(fmt("process %d killed with signal %d - %s\n", pid, signum, strsignal(signum)));
        }
        pid = 0;
        this->pidwatcher.stop();
    }
}