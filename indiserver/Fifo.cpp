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
#include "Fifo.hpp"
#include "Utils.hpp"
#include "Constants.hpp"
#include "DvrInfo.hpp"
#include "LocalDrvInfo.hpp"
#include "RemoteDvrInfo.hpp"
#include "CommandLineArgs.hpp"

#include <fcntl.h>
#include <unistd.h>

using namespace indiserver::constants;

Fifo::Fifo(const std::string &name) : name(name)
{
    fdev.set<Fifo, &Fifo::ioCb>(this);
}

/* Attempt to open up FIFO */
void Fifo::close(void)
{
    if (fd != -1)
    {
        ::close(fd);
        fd = -1;
        fdev.stop();
    }
    bufferPos = 0;
}

void Fifo::open()
{
    /* Open up FIFO, if available */
    fd = ::open(name.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);

    if (fd < 0)
    {
        log(fmt("open(%s): %s.\n", name.c_str(), strerror(errno)));
        Bye();
    }

    fdev.start(fd, EV_READ);
}


/* Handle one fifo command. Start/stop drivers accordingly */
void Fifo::processLine(const char * line)
{

    if (userConfigurableArguments->verbosity)
        log(fmt("FIFO: %s\n", line));

    char cmd[maxStringBufferLength];
    char arg[4][1];
    char var[4][maxStringBufferLength];

    char tDriver[maxStringBufferLength];
    memset(&tDriver[0], 0, sizeof(char) * maxStringBufferLength);

    char tName[maxStringBufferLength];
    memset(&tName[0], 0, sizeof(char) * maxStringBufferLength);

    char envConfig[maxStringBufferLength];
    memset(&envConfig[0], 0, sizeof(char) * maxStringBufferLength);

    char envSkel[maxStringBufferLength];
    memset(&envSkel[0], 0, sizeof(char) * maxStringBufferLength);

    char envPrefix[maxStringBufferLength];
    memset(&envPrefix[0], 0, sizeof(char) * maxStringBufferLength);


    int n = 0;

    bool remoteDriver = !!strstr(line, "@");

    // If remote driver
    if (remoteDriver)
    {
        n = sscanf(line, "%s %511[^\n]", cmd, tDriver);

        // Remove quotes if any
        char *ptr = tDriver;
        int len   = strlen(tDriver);
        while ((ptr = strstr(tDriver, "\"")))
        {
            memmove(ptr, ptr + 1, --len);
            ptr[len] = '\0';
        }
    }
    // If local driver
    else
    {
        n = sscanf(line, "%s %s -%1c \"%511[^\"]\" -%1c \"%511[^\"]\" -%1c \"%511[^\"]\" -%1c \"%511[^\"]\"", cmd,
                   tDriver, arg[0], var[0], arg[1], var[1], arg[2], var[2], arg[3], var[3]);
    }

    int n_args = (n - 2) / 2;

    int j = 0;
    for (j = 0; j < n_args; j++)
    {
        if (arg[j][0] == 'n')
        {
            strncpy(tName, var[j], maxStringBufferLength - 1);
            tName[maxStringBufferLength - 1] = '\0';

            if (userConfigurableArguments->verbosity)
                log(fmt("With name: %s\n", tName));
        }
        else if (arg[j][0] == 'c')
        {
            strncpy(envConfig, var[j], maxStringBufferLength - 1);
            envConfig[maxStringBufferLength - 1] = '\0';

            if (userConfigurableArguments->verbosity)
                log(fmt("With config: %s\n", envConfig));
        }
        else if (arg[j][0] == 's')
        {
            strncpy(envSkel, var[j], maxStringBufferLength - 1);
            envSkel[maxStringBufferLength - 1] = '\0';

            if (userConfigurableArguments->verbosity)
                log(fmt("With skeketon: %s\n", envSkel));
        }
        else if (arg[j][0] == 'p')
        {
            strncpy(envPrefix, var[j], maxStringBufferLength - 1);
            envPrefix[maxStringBufferLength - 1] = '\0';

            if (userConfigurableArguments->verbosity)
                log(fmt("With prefix: %s\n", envPrefix));
        }
    }

    bool startCmd;
    if (!strcmp(cmd, "start"))
        startCmd = 1;
    else
        startCmd = 0;

    if (startCmd)
    {
        if (userConfigurableArguments->verbosity)
            log(fmt("FIFO: Starting driver %s\n", tDriver));

        DvrInfo * dp;
        if (remoteDriver == 0)
        {
            auto * localDp = new LocalDvrInfo();
            dp = localDp;
            //strncpy(dp->dev, tName, MAXINDIDEVICE);
            localDp->envDev = tName;
            localDp->envConfig = envConfig;
            localDp->envSkel = envSkel;
            localDp->envPrefix = envPrefix;
        }
        else
        {
            dp = new RemoteDvrInfo();
        }
        dp->name = tDriver;
        dp->start();
    }
    else
    {
        for (auto dp : DvrInfo::drivers)
        {
            if (dp == nullptr) continue;

            if (dp->name == tDriver)
            {
                /* If device name is given, check against it before shutting down */
                if (tName[0] && !dp->isHandlingDevice(tName))
                    continue;
                if (userConfigurableArguments->verbosity)
                    log(fmt("FIFO: Shutting down driver: %s\n", tDriver));

                dp->restart = false;
                dp->close();
                break;
            }
        }
    }
}

void Fifo::read(void)
{
    int rd = ::read(fd, buffer + bufferPos, sizeof(buffer) - 1 - bufferPos);
    if (rd == 0)
    {
        if (bufferPos > 0)
        {
            buffer[bufferPos] = '\0';
            processLine(buffer);
        }
        close();
        open();
        return;
    }
    if (rd == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;

        log(fmt("Fifo error: %s\n", strerror(errno)));
        close();
        open();
        return;
    }

    bufferPos += rd;

    for(int i = 0; i < bufferPos; ++i)
    {
        if (buffer[i] == '\n')
        {
            buffer[i] = 0;
            processLine(buffer);
            // shift the buffer
            i++;                   /* count including nl */
            bufferPos -= i;        /* remove from nexbuf */
            memmove(buffer, buffer + i, bufferPos); /* slide remaining to front */
            i = -1;                /* restart for loop scan */
        }
    }

    if ((unsigned)bufferPos >= sizeof(buffer) - 1)
    {
        log(fmt("Fifo overflow"));
        close();
        open();
    }
}

void Fifo::ioCb(ev::io &, int revents)
{
    if (EV_ERROR & revents)
    {
        int sockErrno = readFdError(this->fd);
        if (sockErrno)
        {
            log(fmt("Error on fifo: %s\n", strerror(sockErrno)));
            close();
            open();
        }
    }
    else if (revents & EV_READ)
    {
        read();
    }
}
