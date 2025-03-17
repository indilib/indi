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
#include "DvrInfo.hpp"
#include "Utils.hpp"
#include "Constants.hpp"
#include "Msg.hpp"
#include "ClInfo.hpp"
#include "Property.hpp"
#include "Fifo.hpp"
#include "CommandLineArgs.hpp"

ConcurrentSet<DvrInfo> DvrInfo::drivers;

void DvrInfo::onMessage(XMLEle * root, std::list<int> &sharedBuffers)
{
    char *roottag    = tagXMLEle(root);
    const char *dev  = findXMLAttValu(root, "device");
    const char *name = findXMLAttValu(root, "name");
    int isblob       = !strcmp(tagXMLEle(root), "setBLOBVector");

    if (userConfigurableArguments->verbosity > 2)
        traceMsg("read ", root);
    else if (userConfigurableArguments->verbosity > 1)
    {
        log(fmt("read <%s device='%s' name='%s'>\n",
                tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name")));
    }

    /* that's all if driver is just registering a snoop */
    /* JM 2016-05-18: Send getProperties to upstream chained servers as well.*/
    if (!strcmp(roottag, "getProperties"))
    {
        this->addSDevice(dev, name);
        Msg *mp = new Msg(this, root);
        /* send to interested chained servers upstream */
        // FIXME: no use of root here
        ClInfo::q2Servers(this, mp, root);
        /* Send to snooped drivers if they exist so that they can echo back the snooped property immediately */
        // FIXME: no use of root here
        q2RDrivers(dev, mp, root);

        mp->queuingDone();

        return;
    }

    /* that's all if driver desires to snoop BLOBs from other drivers */
    if (!strcmp(roottag, "enableBLOB"))
    {
        Property *sp = findSDevice(dev, name);
        if (sp)
            crackBLOB(pcdataXMLEle(root), &sp->blob);
        delXMLEle(root);
        return;
    }

    /* Found a new device? Let's add it to driver info */
    if (dev[0] && !this->isHandlingDevice(dev))
    {
#ifdef OSX_EMBEDED_MODE
        if (this->dev.empty())
            fprintf(stderr, "STARTED \"%s\"\n", dp->name.c_str());
        fflush(stderr);
#endif
        this->dev.insert(dev);
    }

    /* log messages if any and wanted */
    if (userConfigurableArguments->loggingDir)
        logDMsg(root, dev);

    if (!strcmp(roottag, "pingRequest"))
    {
        setXMLEleTag(root, "pingReply");

        Msg * mp = new Msg(this, root);
        pushMsg(mp);
        mp->queuingDone();
        return;
    }

    /* build a new message -- set content iff anyone cares */
    Msg * mp = Msg::fromXml(this, root, sharedBuffers);
    if (!mp)
    {
        close();
        return;
    }

    /* send to interested clients */
    ClInfo::q2Clients(NULL, isblob, dev, name, mp, root);

    /* send to snooping drivers */
    DvrInfo::q2SDrivers(this, isblob, dev, name, mp, root);

    /* set message content if anyone cares else forget it */
    mp->queuingDone();
}

void DvrInfo::closeWritePart()
{
    // Don't want any half-dead drivers
    close();
}



void DvrInfo::close()
{
    // Tell client driver is dead.
    for (auto dev : dev)
    {
        /* Inform clients that this driver is dead */
        XMLEle *root = addXMLEle(NULL, "delProperty");
        addXMLAtt(root, "device", dev.c_str());

        prXMLEle(stderr, root, 0);
        Msg *mp = new Msg(this, root);

        ClInfo::q2Clients(NULL, 0, dev.c_str(), "", mp, root);
        mp->queuingDone();
    }

    bool terminate;
    if (!restart)
    {
        terminate = true;
    }
    else
    {
        if (restarts >= userConfigurableArguments->maxRestartAttempts)
        {
            log(fmt("Terminated after #%d restarts.\n", restarts));
            terminate = true;
        }
        else
        {
            log(fmt("restart #%d\n", restarts));
            ++restarts;
            terminate = false;
        }
    }

#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "STOPPED \"%s\"\n", name.c_str());
    fflush(stderr);
#endif

    // FIXME: we loose stderr from dying driver
    if (terminate)
    {
        delete(this);
        if ((!fifoHandle) && (drivers.ids().empty()))
            Bye();
        return;
    }
    else
    {
        DvrInfo * restarted = this->clone();
        delete(this);
        restarted->start();
    }
}

void DvrInfo::q2RDrivers(const std::string &dev, Msg *mp, XMLEle *root)
{
    char *roottag = tagXMLEle(root);

    /* queue message to each interested driver.
     * N.B. don't send generic getProps to more than one remote driver,
     *   otherwise they all fan out and we get multiple responses back.
     */
    std::set<std::string> remoteAdvertised;
    for (auto dpId : drivers.ids())
    {
        auto dp = drivers[dpId];
        if (dp == nullptr) continue;

        std::string remoteUid = dp->remoteServerUid();
        bool isRemote = !remoteUid.empty();

        /* driver known to not support this dev */
        if ((!dev.empty()) && dev[0] != '*' && !dp->isHandlingDevice(dev))
            continue;

        /* Only send message to each *unique* remote driver at a particular host:port
         * Since it will be propagated to all other devices there */
        if (dev.empty() && isRemote)
        {
            if (remoteAdvertised.find(remoteUid) != remoteAdvertised.end())
                continue;

            /* Retain last remote driver data so that we do not send the same info again to a driver
            * residing on the same host:port */
            remoteAdvertised.insert(remoteUid);
        }

        /* JM 2016-10-30: Only send enableBLOB to remote drivers */
        if (isRemote == 0 && !strcmp(roottag, "enableBLOB"))
            continue;

        /* ok: queue message to this driver */
        if (userConfigurableArguments->verbosity > 1)
        {
            dp->log(fmt("queuing responsible for <%s device='%s' name='%s'>\n",
                        tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name")));
        }

        // pushmsg can kill dp. do at end
        dp->pushMsg(mp);
    }
}

void DvrInfo::q2SDrivers(DvrInfo *me, int isblob, const std::string &dev, const std::string &name, Msg *mp, XMLEle *root)
{
    std::string meRemoteServerUid = me ? me->remoteServerUid() : "";
    for (auto dpId : drivers.ids())
    {
        auto dp = drivers[dpId];
        if (dp == nullptr) continue;

        Property *sp = dp->findSDevice(dev, name);

        /* nothing for dp if not snooping for dev/name or wrong BLOB mode */
        if (!sp)
            continue;
        if ((isblob && sp->blob == B_NEVER) || (!isblob && sp->blob == B_ONLY))
            continue;

        // Do not send snoop data to remote drivers at the same host
        // since they will manage their own snoops remotely
        if ((!meRemoteServerUid.empty()) && dp->remoteServerUid() == meRemoteServerUid)
            continue;

        /* ok: queue message to this device */
        if (userConfigurableArguments->verbosity > 1)
        {
            dp->log(fmt("queuing snooped <%s device='%s' name='%s'>\n",
                        tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name")));
        }

        // pushmsg can kill dp. do at end
        dp->pushMsg(mp);
    }
}

void DvrInfo::addSDevice(const std::string &dev, const std::string &name)
{
    Property *sp;

    /* no dups */
    sp = findSDevice(dev, name);
    if (sp)
        return;

    /* add dev to sdevs list */
    sp = new Property(dev, name);
    sp->blob = B_NEVER;
    sprops.push_back(sp);

    if (userConfigurableArguments->verbosity)
        log(fmt("snooping on %s.%s\n", dev.c_str(), name.c_str()));
}

Property * DvrInfo::findSDevice(const std::string &dev, const std::string &name) const
{
    for(auto sp : sprops)
    {
        if ((sp->dev == dev) && (sp->name.empty() || sp->name == name))
            return (sp);
    }

    return nullptr;
}
DvrInfo::DvrInfo(bool useSharedBuffer) :
    MsgQueue(useSharedBuffer),
    restarts(0)
{
    drivers.insert(this);
}

DvrInfo::DvrInfo(const DvrInfo &model):
    MsgQueue(model.useSharedBuffer),
    name(model.name),
    restarts(model.restarts)
{
    drivers.insert(this);
}

DvrInfo::~DvrInfo()
{
    drivers.erase(this);
    for(auto prop : sprops)
    {
        delete prop;
    }
}

bool DvrInfo::isHandlingDevice(const std::string &dev) const
{
    return this->dev.find(dev) != this->dev.end();
}

void DvrInfo::log(const std::string &str) const
{
    std::string logLine = "Driver ";
    logLine += name;
    logLine += ": ";
    logLine += str;
    ::log(logLine);
}