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
#pragma once

#include "lilxml.h"

#include <string>
#include <vector>
#include <unordered_map>

void log(const std::string &log);
/* Turn a printf format into std::string */
std::string fmt(const char * fmt, ...) __attribute__ ((format (printf, 1, 0)));

char *indi_tstamp(char *s);
void logStartup(int ac, char *av[]);
void noSIGPIPE(void);
char *indi_tstamp(char *s);
void logDMsg(XMLEle *root, const char *dev);
void Bye(void);
std::vector<XMLEle *> findBlobElements(XMLEle * root);
int readFdError(int
                fd);                       /* Read a pending error condition on the given fd. Return errno value or 0 if none */
void * attachSharedBuffer(int fd, size_t &size);
void dettachSharedBuffer(int fd, void * ptr, size_t size);
bool parseBlobSize(XMLEle * blobWithAttachedBuffer, ssize_t &size);
XMLEle * cloneXMLEleWithReplacementMap(XMLEle * root, const std::unordered_map<XMLEle*, XMLEle*> &replacement);

#define STRINGIFY_TOK(x) #x
#define TO_STRING(x) STRINGIFY_TOK(x)
