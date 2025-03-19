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
#include "ClInfo.hpp"
#include "Msg.hpp"
#include "DvrInfo.hpp"
#include "Constants.hpp"
#include "Utils.hpp"
#include "Property.hpp"
#include "CommandLineArgs.hpp"

ConcurrentSet<ClInfo> ClInfo::clients;

// root will be released
void ClInfo::onMessage(XMLEle * root, std::list<int> &sharedBuffers)
{
    char *roottag    = tagXMLEle(root);

    const char *dev  = findXMLAttValu(root, "device");
    const char *name = findXMLAttValu(root, "name");
    int isblob       = !strcmp(tagXMLEle(root), "setBLOBVector");

    /* snag interested properties.
     * N.B. don't open to alldevs if seen specific dev already, else
     *   remote client connections start returning too much.
     */
    if (dev[0])
    {
        // Signature for CHAINED SERVER
        // Not a regular client.
        if (dev[0] == '*' && !this->props.size())
            this->allprops = 2;
        else
            addDevice(dev, name, isblob);
    }
    else if (!strcmp(roottag, "getProperties") && !this->props.size() && this->allprops != 2)
        this->allprops = 1;

    /* snag enableBLOB -- send to remote drivers too */
    if (!strcmp(roottag, "enableBLOB"))
        crackBLOBHandling(dev, name, pcdataXMLEle(root));

    if (!strcmp(roottag, "pingRequest"))
    {
        setXMLEleTag(root, "pingReply");

        Msg * mp = new Msg(this, root);
        pushMsg(mp);
        mp->queuingDone();
        return;
    }

    /* build a new message -- set content iff anyone cares */
    Msg* mp = Msg::fromXml(this, root, sharedBuffers);
    if (!mp)
    {
        log("Closing after malformed message\n");
        close();
        return;
    }

    /* send message to driver(s) responsible for dev */
    DvrInfo::q2RDrivers(dev, mp, root);

    /* JM 2016-05-18: Upstream client can be a chained INDI server. If any driver locally is snooping
    * on any remote drivers, we should catch it and forward it to the responsible snooping driver. */
    /* send to snooping drivers. */
    // JM 2016-05-26: Only forward setXXX messages
    if (!strncmp(roottag, "set", 3))
        DvrInfo::q2SDrivers(NULL, isblob, dev, name, mp, root);

    /* echo new* commands back to other clients */
    if (!strncmp(roottag, "new", 3))
    {
        q2Clients(this, isblob, dev, name, mp, root);
    }

    mp->queuingDone();
}

void ClInfo::close()
{
    if (userConfigurableArguments->verbosity > 0)
        log("shut down complete - bye!\n");

    delete(this);

#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "CLIENTS %d\n", clients.size());
    fflush(stderr);
#endif
}

void ClInfo::q2Clients(ClInfo *notme, int isblob, const std::string &dev, const std::string &name, Msg *mp, XMLEle *root)
{
    /* queue message to each interested client */
    for (auto cpId : clients.ids())
    {
        auto cp = clients[cpId];
        if (cp == nullptr) continue;

        /* cp in use? notme? want this dev/name? blob? */
        if (cp == notme)
            continue;
        if (cp->findDevice(dev, name) < 0)
            continue;

        //if ((isblob && cp->blob==B_NEVER) || (!isblob && cp->blob==B_ONLY))
        if (!isblob && cp->blob == B_ONLY)
            continue;

        if (isblob)
        {
            if (cp->props.size() > 0)
            {
                Property *blobp = nullptr;
                for (auto pp : cp->props)
                {
                    if (pp->dev == dev && pp->name == name)
                    {
                        blobp = pp;
                        break;
                    }
                }

                if ((blobp && blobp->blob == B_NEVER) || (!blobp && cp->blob == B_NEVER))
                    continue;
            }
            else if (cp->blob == B_NEVER)
                continue;
        }

        /* shut down this client if its q is already too large */
        unsigned long ql = cp->msgQSize();
        if (isblob && userConfigurableArguments->maxStreamSizeMB > 0 && ql > userConfigurableArguments->maxStreamSizeMB)
        {
            // Drop frames for streaming blobs
            /* pull out each name/BLOB pair, decode */
            XMLEle *ep      = NULL;
            int streamFound = 0;
            for (ep = nextXMLEle(root, 1); ep; ep = nextXMLEle(root, 0))
            {
                if (strcmp(tagXMLEle(ep), "oneBLOB") == 0)
                {
                    XMLAtt *fa = findXMLAtt(ep, "format");

                    if (fa && strstr(valuXMLAtt(fa), "stream"))
                    {
                        streamFound = 1;
                        break;
                    }
                }
            }
            if (streamFound)
            {
                if (userConfigurableArguments->verbosity > 1)
                    cp->log(fmt("%ld bytes behind. Dropping stream BLOB...\n", ql));
                continue;
            }
        }
        if (ql > userConfigurableArguments->maxQueueSizeMB)
        {
            if (userConfigurableArguments->verbosity)
                cp->log(fmt("%ld bytes behind, shutting down\n", ql));
            cp->close();
            continue;
        }

        if (userConfigurableArguments->verbosity > 1)
            cp->log(fmt("queuing <%s device='%s' name='%s'>\n",
                        tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name")));

        // pushmsg can kill cp. do at end
        cp->pushMsg(mp);
    }

    return;
}

void ClInfo::q2Servers(DvrInfo *me, Msg *mp, XMLEle *root)
{
    int devFound = 0;

    /* queue message to each interested client */
    for (auto cpId : clients.ids())
    {
        auto cp = clients[cpId];
        if (cp == nullptr) continue;

        // Only send the message to the upstream server that is connected specifically to the device in driver dp
        switch (cp->allprops)
        {
            // 0 --> not all props are requested. Check for specific combination
            case 0:
                for (auto pp : cp->props)
                {
                    if (me->dev.find(pp->dev) != me->dev.end())
                    {
                        devFound = 1;
                        break;
                    }
                }
                break;

            // All props are requested. This is client-only mode (not upstream server)
            case 1:
                break;
            // Upstream server mode
            case 2:
                devFound = 1;
                break;
        }

        // If no matching device found, continue
        if (devFound == 0)
            continue;

        /* shut down this client if its q is already too large */
        unsigned long ql = cp->msgQSize();
        if (ql > userConfigurableArguments->maxQueueSizeMB)
        {
            if (userConfigurableArguments->verbosity)
                cp->log(fmt("%ld bytes behind, shutting down\n", ql));
            cp->close();
            continue;
        }

        /* ok: queue message to this client */
        if (userConfigurableArguments->verbosity > 1)
            cp->log(fmt("queuing <%s device='%s' name='%s'>\n",
                        tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name")));

        // pushmsg can kill cp. do at end
        cp->pushMsg(mp);
    }
}
int ClInfo::findDevice(const std::string &dev, const std::string &name) const
{
    if (allprops >= 1 || dev.empty())
        return (0);
    for (auto pp : props)
    {
        if ((pp->dev == dev) && (pp->name.empty() || (pp->name == name)))
            return (0);
    }
    return (-1);
}

void ClInfo::addDevice(const std::string &dev, const std::string &name, int isblob)
{
    if (isblob)
    {
        for (auto pp : props)
        {
            if (pp->dev == dev && pp->name == name)
                return;
        }
    }
    /* no dups */
    else if (!findDevice(dev, name))
        return;

    /* add */
    Property *pp = new Property(dev, name);
    props.push_back(pp);
}

void ClInfo::crackBLOBHandling(const std::string &dev, const std::string &name, const char *enableBLOB)
{
    /* If we have EnableBLOB with property name, we add it to Client device list */
    if (!name.empty())
        addDevice(dev, name, 1);
    else
        /* Otherwise, we set the whole client blob handling to what's passed (enableBLOB) */
        crackBLOB(enableBLOB, &blob);

    /* If whole client blob handling policy was updated, we need to pass that also to all children
       and if the request was for a specific property, then we apply the policy to it */
    for (auto pp : props)
    {
        if (name.empty())
            crackBLOB(enableBLOB, &pp->blob);
        else if (pp->dev == dev && pp->name == name)
        {
            crackBLOB(enableBLOB, &pp->blob);
            return;
        }
    }
}
ClInfo::ClInfo(bool useSharedBuffer) : MsgQueue(useSharedBuffer)
{
    clients.insert(this);
}

ClInfo::~ClInfo()
{
    for(auto prop : props)
    {
        delete prop;
    }

    clients.erase(this);
}

void ClInfo::log(const std::string &str) const
{
    std::string logLine = fmt("Client %d: ", this->getRFd());
    logLine += str;
    ::log(logLine);
}