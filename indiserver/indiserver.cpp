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

 * argv lists names of Driver programs to run or sockets to connect for Devices.
 * Drivers are restarted if they exit or connection closes.
 * Each local Driver's stdin/out are assumed to provide INDI traffic and are
 *   connected here via pipes. Local Drivers' stderr are connected to our
 *   stderr with date stamp and driver name prepended.
 * We only support Drivers that advertise support for one Device. The problem
 *   with multiple Devices in one Driver is without a way to know what they
 *   _all_ are there is no way to avoid sending all messages to all Drivers.
 * Outbound messages are limited to Devices and Properties seen inbound.
 *   Messages to Devices on sockets always include Device so the chained
 *   indiserver will only pass back info from that Device.
 * All newXXX() received from one Client are echoed to all other Clients who
 *   have shown an interest in the same Device and property.
 *
 * 2017-01-29 JM: Added option to drop stream blobs if client blob queue is
 * higher than maxstreamsiz bytes
 *
 * Implementation notes:
 *
 * We fork each driver and open a server socket listening for INDI clients.
 * Then forever we listen for new clients and pass traffic between clients and
 * drivers, subject to optimizations based on sniffing messages for matching
 * Devices and Properties. Since one message might be destined to more than
 * one client or device, they are queued and only removed after the last
 * consumer is finished. XMLEle are converted to linear strings before being
 * sent to optimize write system calls and avoid blocking to slow clients.
 * Clients that get more than maxqsiz bytes behind are shut down.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE // needed for siginfo_t and sigaction
#endif

#include "Fifo.hpp"
#include "ClInfo.hpp"
#include "DvrInfo.hpp"
#include "LocalDrvInfo.hpp"
#include "RemoteDvrInfo.hpp"
#include "TcpServer.hpp"
#include "UnixServer.hpp"
#include "Utils.hpp"
#include "Constants.hpp"
#include "CommandLineArgs.hpp"

#include "config.h"
#include <string>

#include <assert.h>

#include "indiapi.h"

#include <memory>
#include <fcntl.h>
#include <libgen.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/un.h>
#ifdef MSG_ERRQUEUE
#include <linux/errqueue.h>
#endif

#include <ev++.h>
using namespace indiserver::constants;

static ev::default_loop loop;

CommandLineArgs* userConfigurableArguments{nullptr};
Fifo* fifoHandle{nullptr};

/* print usage message and exit (2) */
void usage(void)
{
    fprintf(stderr, "Usage: %s [options] driver [driver ...]\n", userConfigurableArguments->binaryName.c_str());
    fprintf(stderr, "Purpose: server for local and remote INDI drivers\n");
    fprintf(stderr, "INDI Library: %s\nCode %s. Protocol %g.\n", CMAKE_INDI_VERSION_STRING, GIT_TAG_STRING, INDIV);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -l d     : log driver messages to <d>/YYYY-MM-DD.islog\n");
    fprintf(stderr, " -m m     : kill client if gets more than this many MB behind, default %d\n", defaultMaxQueueSizeMB);
    fprintf(stderr,
            " -d m     : drop streaming blobs if client gets more than this many MB behind, default %d. 0 to disable\n",
            defaultMaxStreamSizeMB);
#ifdef ENABLE_INDI_SHARED_MEMORY
    fprintf(stderr, " -u path  : Path for the local connection socket (abstract), default %s\n", INDIUNIXSOCK);
#endif
    fprintf(stderr, " -p p     : alternate IP port, default %d\n", indiPortDefault);
    fprintf(stderr, " -r r     : maximum driver restarts on error, default %d\n", defaultMaximumRestarts);
    fprintf(stderr, " -f path  : Path to fifo for dynamic startup and shutdown of drivers.\n");
    fprintf(stderr, " -v       : show key events, no traffic\n");
    fprintf(stderr, " -vv      : -v + key message content\n");
    fprintf(stderr, " -vvv     : -vv + complete xml\n");
    fprintf(stderr, "driver    : executable or [device]@host[:port]\n");

    exit(2);
}

int main(int ac, char *av[])
{
    /* log startup */
    logStartup(ac, av);

    std::unique_ptr<CommandLineArgs> argValues = std::make_unique<CommandLineArgs>();
    userConfigurableArguments = argValues.get();

    std::unique_ptr<Fifo> fifoHandleOwner{};

    /* save our name */
    argValues->binaryName = av[0];

#ifdef OSX_EMBEDED_MODE

    char logname[128];
    snprintf(logname, 128, logNamePattern, getlogin());
    fprintf(stderr, "switching stderr to %s", logname);
    freopen(logname, "w", stderr);

    fifoHandleOwner = std::make_unique<Fifo>();
    fifoHandle = fifoHandleOwner.get();
    updatedArgs->fifoHandle->name = fifoName;
    updatedArgs->verbosity   = 1;
    ac        = 0;

#else

    /* crack args */
    while ((--ac > 0) && ((*++av)[0] == '-'))
    {
        char *s;
        for (s = av[0] + 1; *s != '\0'; s++)
            switch (*s)
            {
                case 'l':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-l requires log directory\n");
                        usage();
                    }
                    userConfigurableArguments->loggingDir = *++av;
                    ac--;
                    break;
                case 'm':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-m requires max MB behind\n");
                        usage();
                    }
                    userConfigurableArguments->maxQueueSizeMB = 1024 * 1024 * atoi(*++av);
                    ac--;
                    break;
                case 'p':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-p requires port value\n");
                        usage();
                    }
                    userConfigurableArguments->port = atoi(*++av);
                    ac--;
                    break;
                case 'd':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-d requires max stream MB behind\n");
                        usage();
                    }
                    userConfigurableArguments->maxStreamSizeMB = 1024 * 1024 * atoi(*++av);
                    ac--;
                    break;
#ifdef ENABLE_INDI_SHARED_MEMORY
                case 'u':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-u requires local socket path\n");
                        usage();
                    }
                    UnixServer::unixSocketPath = *++av;
                    ac--;
                    break;
#endif // ENABLE_INDI_SHARED_MEMORY
                case 'f':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-f requires fifo node\n");
                        usage();
                    }
                    assert(!fifoHandle);
                    fifoHandleOwner = std::make_unique<Fifo>(*++av);
                    fifoHandle = fifoHandleOwner.get();
                    ac--;
                    break;
                case 'r':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-r requires number of restarts\n");
                        usage();
                    }
                    userConfigurableArguments->maxRestartAttempts = atoi(*++av);
                    if (userConfigurableArguments->maxRestartAttempts < 0)
                        userConfigurableArguments->maxRestartAttempts = 0;
                    ac--;
                    break;
                case 'v':
                    userConfigurableArguments->verbosity++;
                    break;
                default:
                    usage();
            }
    }
#endif

    /* at this point there are ac args in av[] to name our drivers */
    if (ac == 0 && !fifoHandle)
        usage();

    /* take care of some unixisms */
    noSIGPIPE();

    std::vector<std::unique_ptr<DvrInfo>> drivers(ac);

    /* start each driver */
    while (ac-- > 0)
    {
        std::string dvrName = *av++;
        if (dvrName.find('@') != std::string::npos)
        {
            drivers.push_back(std::make_unique<RemoteDvrInfo>());
        }
        else
        {
            drivers.push_back(std::make_unique<LocalDvrInfo>());
        }
        drivers.back()->name = dvrName;
        drivers.back()->start();
    }

    /* announce we are online */
    const auto tcpServer = std::make_unique<TcpServer>(userConfigurableArguments->port);
    tcpServer->listen();

#ifdef ENABLE_INDI_SHARED_MEMORY
    /* create a new unix server */
    const auto unixServer = std::make_unique<UnixServer>(UnixServer::unixSocketPath);
    unixServer->listen();
#endif
    /* Load up FIFO, if available */
    if (fifoHandle)
    {
        // New started drivers will not inherit server's prefix anymore

        // JM 2022.07.23: This causes an issue on MacOS. Disabled for now until investigated further.
        //unsetenv("INDIPREFIX");
        fifoHandle->listen();
    }

    /* handle new clients and all io */
    loop.loop();

    /* will not happen unless no more listener left ! */
    log("unexpected return from event loop\n");
    return (1);
}

