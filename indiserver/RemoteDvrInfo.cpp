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
#include "RemoteDvrInfo.hpp"
#include "Utils.hpp"
#include "Constants.hpp"
#include "Msg.hpp"
#include "CommandLineArgs.hpp"

#include <cstdio>
#include <netinet/in.h>
#include <netdb.h>

using namespace indiserver::constants;

void RemoteDvrInfo::extractRemoteId(const std::string &name, std::string &o_host, int &o_port, std::string &o_dev) const
{
    char dev[MAXINDIDEVICE] = {0};
    char host[maxStringBufferLength] = {0};

    /* extract host and port from name*/
    int indi_port = indiPortDefault;
    if (sscanf(name.c_str(), "%[^@]@%[^:]:%d", dev, host, &indi_port) < 2)
    {
        // Device missing? Try a different syntax for all devices
        if (sscanf(name.c_str(), "@%[^:]:%d", host, &indi_port) < 1)
        {
            log(fmt("Bad remote device syntax: %s\n", name.c_str()));
            Bye();
        }
    }

    o_host = host;
    o_port = indi_port;
    o_dev = dev;
}

/* start the given remote INDI driver connection.
 * exit if trouble.
 */
void RemoteDvrInfo::start()
{
    int sockfd;
    std::string dev;
    extractRemoteId(name, host, port, dev);

    /* connect */
    sockfd = openINDIServer();

    /* record flag pid, io channels, init lp and snoop list */

    this->setFds(sockfd, sockfd);

    if (userConfigurableArguments->verbosity > 0)
        log(fmt("socket=%d\n", sockfd));

    /* N.B. storing name now is key to limiting outbound traffic to this
     * dev.
     */
    if (!dev.empty())
        this->dev.insert(dev);

    /* Sending getProperties with device lets remote server limit its
     * outbound (and our inbound) traffic on this socket to this device.
     */
    XMLEle *root = addXMLEle(NULL, "getProperties");

    if (!dev.empty())
    {
        addXMLAtt(root, "device", dev.c_str());
        addXMLAtt(root, "version", TO_STRING(INDIV));
    }
    else
    {
        // This informs downstream server that it is connecting to an upstream server
        // and not a regular client. The difference is in how it treats snooping properties
        // among properties.
        addXMLAtt(root, "device", "*");
        addXMLAtt(root, "version", TO_STRING(INDIV));
    }

    Msg *mp = new Msg(nullptr, root);

    // pushmsg can kill this. do at end
    pushMsg(mp);
}

int RemoteDvrInfo::openINDIServer()
{
    struct sockaddr_in serv_addr;
    struct hostent *hp;
    int sockfd;

    /* lookup host address */
    hp = gethostbyname(host.c_str());
    if (!hp)
    {
        log(fmt("gethostbyname(%s): %s\n", host.c_str(), strerror(errno)));
        Bye();
    }

    /* create a socket to the INDI server */
    (void)memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    serv_addr.sin_port        = htons(port);
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        log(fmt("socket(%s,%d): %s\n", host.c_str(), port, strerror(errno)));
        Bye();
    }

    /* connect */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        log(fmt("connect(%s,%d): %s\n", host.c_str(), port, strerror(errno)));
        Bye();
    }

    /* ok */
    return (sockfd);
}

RemoteDvrInfo::RemoteDvrInfo(): DvrInfo(false)
{}

RemoteDvrInfo::RemoteDvrInfo(const RemoteDvrInfo &model):
    DvrInfo(model),
    host(model.host),
    port(model.port)
{}

RemoteDvrInfo::~RemoteDvrInfo()
{}

RemoteDvrInfo * RemoteDvrInfo::clone() const
{
    return new RemoteDvrInfo(*this);
}