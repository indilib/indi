/* INDI Server for protocol version 1.7.
 * Copyright (C) 2007 Elwood C. Downey <ecdowney@clearskyinstitute.com>
                 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>
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

#include "config.h"
#include <set>
#include <string>
#include <list>

#include "indiapi.h"
#include "indidevapi.h"
#include "libs/lilxml.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>

extern "C" { 
#include <ev.h>
}

#include <ev++.h>

#define INDIPORT      7624    /* default TCP/IP port to listen */
#define MAXSBUF       512
#define MAXRBUF       49152 /* max read buffering here */
#define MAXWSIZ       49152 /* max bytes/write */
#define SHORTMSGSIZ   2048  /* buf size for most messages */
#define DEFMAXQSIZ    128   /* default max q behind, MB */
#define DEFMAXSSIZ    5     /* default max stream behind, MB */
#define DEFMAXRESTART 10    /* default max restarts */

#ifdef OSX_EMBEDED_MODE
#define LOGNAME  "/Users/%s/Library/Logs/indiserver.log"
#define FIFONAME "/tmp/indiserverFIFO"
#endif

static ev::default_loop loop;

class Peer;
class RawPeerRef {
    friend class Peer;
    Peer * value;
    RawPeerRef * prev, * next;
    
    void clear();

protected:
    RawPeerRef(Peer * p);
    ~RawPeerRef();

    Peer * getRaw() const { return value; }

public:

    bool alive() const { return value != nullptr; }
};

template<class P> class PeerRef: public RawPeerRef
{
public:
    PeerRef(P* value): RawPeerRef((P*)value) {}
    P * get() const { return (PeerRef*)getRaw(); }
};

/** Object that can vanish */
class Peer {
    friend class RawPeerRef;
private:
    RawPeerRef * first, * last;

protected:
    Peer() {
        first = nullptr;
        last = nullptr;
    }

    virtual ~Peer() {
        clearRefs();
    }

    void clearRefs() {
        while(first) {
            first->clear();
        }
    }
};

class Msg {
public:
    int count;         /* number of consumers left */
    unsigned long cl;  /* content length */
    char *cp;          /* content: malloced */

    Msg();
    ~Msg();
    void alloc(unsigned long cl);

    /* save str as content in mp. */
    void setFromStr(const char * str);

    /* print root as content in mp.*/
    void setFromXMLEle(XMLEle *root);
};

class MsgQueue: public Peer{
    void unrefMsg(Msg * msg);
    int rFd, wFd;
    LilXML * lp;         /* XML parsing context */
    ev::io   rio, wio;   /* Event loop io events */
    void ioCb(ev::io &watcher, int revents);

    std::list<Msg*> msgq;           /* Msg queue */
    unsigned int nsent; /* bytes of current Msg sent so far */

    void readFromFd();
    void writeToFd();

protected:
    int getRFd() const { return rFd; }
    int getWFd() const { return wFd; }

    void traceMsg(const std::string & log, XMLEle *root);

    /* Close the client. (May be restarted later depending on logic) */
    virtual void close() = 0;
    /* Handle a message. will be freed by caller */
    virtual void onMessage(XMLEle *root) = 0;

    /* convert the string value of enableBLOB to our B_ state value.
     * no change if unrecognized
     */
    static void crackBLOB(const char *enableBLOB, BLOBHandling *bp);

public:

    MsgQueue();
    virtual ~MsgQueue();

    void pushMsg(Msg * msg);

    /* return storage size of all Msqs on the given q */
    unsigned long msgQSize() const;

    Msg * headMsg() const;
    void consumeHeadMsg();

    /* Remove all messages from queue */
    void clearMsgQueue();

    void setFds(int rFd, int wFd);

    // FIXME: move to protected
    virtual void log(const std::string & log) const = 0;
};

/* device + property name */
class Property {
public:
    std::string dev;
    std::string name;
    BLOBHandling blob = B_NEVER; /* when to snoop BLOBs */

    Property(const std::string & dev, const std::string & name): dev(dev), name(name) {}
};


class Fifo {
    std::string name; /* Path to FIFO for dynamic startups & shutdowns of drivers */

    char buffer[1024];
    int bufferPos = 0;
    int fd = -1;
    ev::io fdev;

    void close();
    void open();
    void processLine(const char * line);
    void read();
    void ioCb(ev::io &watcher, int revents);
public:
    Fifo(const std::string & name);
    void listen() { open(); }
};

static Fifo * fifo = nullptr;


class DvrInfo;

/* info for each connected client */
class ClInfo: public MsgQueue {

protected:
    virtual void onMessage(XMLEle *root);

    void crackBLOBHandling(const std::string & dev, const std::string & name, const char *enableBLOB);

public:
    std::list<Property*> props; /* props we want */
    int allprops = 0;       /* saw getProperties w/o device */
    BLOBHandling blob = B_NEVER;  /* when to send setBLOBs */

    ClInfo();
    virtual ~ClInfo();

    /* return 0 if cp may be interested in dev/name else -1
     */
    int findDevice(const std::string & dev, const std::string & name) const;

    /* add the given device and property to the props[] list of client if new.
    */
    void addDevice(const std::string & dev, const std::string & name, int isblob);

    // FIXME: no need for public once fully converted to object
    virtual void close();
    virtual void log(const std::string & log) const;


    /* put Msg mp on queue of each chained server client, except notme.
    */
    static void q2Servers(DvrInfo *me, Msg *mp, XMLEle *root);

    /* put Msg mp on queue of each client interested in dev/name, except notme.
    * if BLOB always honor current mode.
    */
    static void q2Clients(ClInfo *notme, int isblob, const std::string & dev, const std::string & name, Msg *mp, XMLEle *root);
};
static std::set<ClInfo*> clients;

/* info for each connected driver */
class DvrInfo: public MsgQueue
{
    /* add dev/name to this device's snooping list.
     * init with blob mode set to B_NEVER.
     */
    void addSDevice(const std::string & dev, const std::string & name);

public:
    /* return Property if dp is this driver is snooping dev/name, else NULL.
    */
    Property *findSDevice(const std::string & dev, const std::string & name) const;

protected:
    virtual void onMessage(XMLEle *root);

    /* Construct an instance that will start the same driver */
    DvrInfo(const DvrInfo & model);

public:
    std::string name; /* persistent name */
    
    std::set<std::string> dev;      /* device served by this driver */
    std::list<Property*>sprops;     /* props we snoop */
    int restarts;                   /* times process has been restarted */
    bool restart = true;            /* Restart on shutdown */
    DvrInfo();
    virtual ~DvrInfo();

    bool isHandlingDevice(const std::string & dev) const;

    /* start the INDI driver process or connection.
    * exit if trouble.
    */
    virtual void start() = 0;
    virtual void close();
    /* Allocate an instance that will start the same driver */
    virtual DvrInfo * clone() const = 0;
    virtual void log(const std::string & log) const;

    virtual const std::string remoteServerUid() const = 0;

    /* put Msg mp on queue of each driver responsible for dev, or all drivers
    * if dev empty.
    */
    static void q2RDrivers(const std::string & dev, Msg *mp, XMLEle *root);

    /* put Msg mp on queue of each driver snooping dev/name.
    * if BLOB always honor current mode.
    */
    static void q2SDrivers(DvrInfo *me, int isblob, const std::string & dev, const std::string & name, Msg *mp, XMLEle *root);
};
static std::set<DvrInfo*> drivers;

class LocalDvrInfo: public DvrInfo {
    char errbuff[1024];     /* buffer for stderr pipe. line too long will be clipped */
    int errbuffpos = 0;     /* first free pos in buffer */ 
    ev::io     eio;         /* Event loop io events */
    ev::child  pidwatcher;
    void onEfdEvent(ev::io &watcher, int revents); /* callback for data on efd */
    void onPidEvent(ev::child & watcher, int revents);

    int pid = 0;            /* process id or 0 for N/A (not started/terminated) */
    int efd = -1;           /* stderr from driver, or -1 when N/A */

    void closeEfd();
    void closePid();
protected:
    LocalDvrInfo(const LocalDvrInfo & model);

public:
    std::string envDev;
    std::string envConfig;
    std::string envSkel;
    std::string envPrefix;

    virtual void start();

    LocalDvrInfo();
    virtual ~LocalDvrInfo();

    virtual LocalDvrInfo * clone() const;

    virtual const std::string remoteServerUid() const { return ""; }
};

class RemoteDvrInfo: public DvrInfo {
    int openINDIServer();

    void extractRemoteId(const std::string & name, std::string & o_host, int & o_port, std::string & o_dev) const;

protected:
    RemoteDvrInfo(const RemoteDvrInfo & model);

public:
    std::string host;
    int port;

    virtual void start();

    RemoteDvrInfo();
    virtual ~RemoteDvrInfo();

    virtual RemoteDvrInfo * clone() const;

    virtual const std::string remoteServerUid() const 
    { 
        return std::string(host) + ":" + std::to_string(port);
    }
};

class TcpServer {
    int port;
    int sfd = -1;
    ev::io sfdev;

    /* prepare for new client arriving on socket.
    * exit if trouble.
    */
    void accept();
    void ioCb(ev::io &watcher, int revents);
public:
    TcpServer(int port);

    void listen();
};


static void log(const std::string & log);
/** Turn a printf format into std::string */
static std::string fmt(const char * fmt, ...) __attribute__ ((format (printf, 1, 0)));

static char *indi_tstamp(char *s);

static const char *me;                                 /* our name */
static int port = INDIPORT;                            /* public INDI port */
static int verbose;                                    /* chattiness */
static char *ldir;                                     /* where to log driver messages */
static unsigned int maxqsiz  = (DEFMAXQSIZ * 1024 * 1024); /* kill if these bytes behind */
static unsigned int maxstreamsiz  = (DEFMAXSSIZ * 1024 * 1024); /* drop blobs if these bytes behind while streaming*/
static int maxrestarts   = DEFMAXRESTART;

static void logStartup(int ac, char *av[]);
static void usage(void);
static void noSIGPIPE(void);
static char *indi_tstamp(char *s);
static void logDMsg(XMLEle *root, const char *dev);
static void Bye(void);

int main(int ac, char *av[])
{
    /* log startup */
    logStartup(ac, av);

    /* save our name */
    me = av[0];

#ifdef OSX_EMBEDED_MODE

    char logname[128];
    snprintf(logname, 128, LOGNAME, getlogin());
    fprintf(stderr, "switching stderr to %s", logname);
    freopen(logname, "w", stderr);

    fifo = new Fifo();
    fifo->name = FIFONAME;
    verbose   = 1;
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
                    ldir = *++av;
                    ac--;
                    break;
                case 'm':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-m requires max MB behind\n");
                        usage();
                    }
                    maxqsiz = 1024 * 1024 * atoi(*++av);
                    ac--;
                    break;
                case 'p':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-p requires port value\n");
                        usage();
                    }
                    port = atoi(*++av);
                    ac--;
                    break;
                case 'd':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-d requires max stream MB behind\n");
                        usage();
                    }
                    maxstreamsiz = 1024 * 1024 * atoi(*++av);
                    ac--;
                    break;
                case 'f':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-f requires fifo node\n");
                        usage();
                    }
                    fifo = new Fifo(*++av);
                    ac--;
                    break;
                case 'r':
                    if (ac < 2)
                    {
                        fprintf(stderr, "-r requires number of restarts\n");
                        usage();
                    }
                    maxrestarts = atoi(*++av);
                    if (maxrestarts < 0)
                        maxrestarts = 0;
                    ac--;
                    break;
                case 'v':
                    verbose++;
                    break;
                default:
                    usage();
            }
    }
#endif

    /* at this point there are ac args in av[] to name our drivers */
    if (ac == 0 && !fifo)
        usage();

    /* take care of some unixisms */
    noSIGPIPE();

    /* start each driver */
    while (ac-- > 0)
    {
        std::string dvrName = *av++;
        DvrInfo * dr;
        if (dvrName.find('@') != std::string::npos) {
            dr = new RemoteDvrInfo();
        } else {
            dr = new LocalDvrInfo();
        }
        dr->name = dvrName;
        dr->start();
    }

    /* announce we are online */
    (new TcpServer(port))->listen();

    /* Load up FIFO, if available */
    if (fifo) fifo->listen();

    /* handle new clients and all io */
    loop.loop();

    /* will not happen unless no more listener left ! */
    log("unexpected return from event loop\n");
    return (1);
}

/* record we have started and our args */
static void logStartup(int ac, char *av[])
{
    int i;

    std::string startupMsg = "startup:";
    for (i = 0; i < ac; i++) {
        startupMsg += " ";
        startupMsg += av[i];
    }
    log(startupMsg);
}

/* print usage message and exit (2) */
static void usage(void)
{
    fprintf(stderr, "Usage: %s [options] driver [driver ...]\n", me);
    fprintf(stderr, "Purpose: server for local and remote INDI drivers\n");
    fprintf(stderr, "INDI Library: %s\nCode %s. Protocol %g.\n", CMAKE_INDI_VERSION_STRING, GIT_TAG_STRING, INDIV);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -l d     : log driver messages to <d>/YYYY-MM-DD.islog\n");
    fprintf(stderr, " -m m     : kill client if gets more than this many MB behind, default %d\n", DEFMAXQSIZ);
    fprintf(stderr,
            " -d m     : drop streaming blobs if client gets more than this many MB behind, default %d. 0 to disable\n",
            DEFMAXSSIZ);
    fprintf(stderr, " -p p     : alternate IP port, default %d\n", INDIPORT);
    fprintf(stderr, " -r r     : maximum driver restarts on error, default %d\n", DEFMAXRESTART);
    fprintf(stderr, " -f path  : Path to fifo for dynamic startup and shutdown of drivers.\n");
    fprintf(stderr, " -v       : show key events, no traffic\n");
    fprintf(stderr, " -vv      : -v + key message content\n");
    fprintf(stderr, " -vvv     : -vv + complete xml\n");
    fprintf(stderr, "driver    : executable or [device]@host[:port]\n");

    exit(2);
}

/* turn off SIGPIPE on bad write so we can handle it inline */
static void noSIGPIPE()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    (void)sigaction(SIGPIPE, &sa, NULL);
}

/* start the given local INDI driver process.
 * exit if trouble.
 */
void LocalDvrInfo::start()
{
    Msg *mp;
    char buf[32];
    int rp[2], wp[2], ep[2];
    int pid;

#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "STARTING \"%s\"\n", name.c_str());
    fflush(stderr);
#endif

    /* build three pipes: r, w and error*/
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
        dup2(wp[0], 0); /* driver stdin reads from wp[0] */
        dup2(rp[1], 1); /* driver stdout writes to rp[1] */
        dup2(ep[1], 2); /* driver stderr writes to e[]1] */
        for (fd = 3; fd < 100; fd++)
            (void)::close(fd);

        if (!envDev.empty())
            setenv("INDIDEV", envDev.c_str(), 1);
        /* Only reset environment variable in case of FIFO */
        else if (fifo)
            unsetenv("INDIDEV");
        if (!envConfig.empty())
            setenv("INDICONFIG", envConfig.c_str(), 1);
        else if (fifo)
            unsetenv("INDICONFIG");
        if (!envSkel.empty())
            setenv("INDISKEL", envSkel.c_str(), 1);
        else if (fifo)
            unsetenv("INDISKEL");
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
            if (fifo)
                unsetenv("INDIPREFIX");
            if (name[0] == '.')
            {
                executable = std::string(dirname((char*)me)) + "/" + name;
                execlp(executable.c_str(), name.c_str(), NULL);
            }
            else
            {
                execlp(name.c_str(), name.c_str(), NULL);
            }
        }

#ifdef OSX_EMBEDED_MODE
        fprintf(stderr, "FAILED \"%s\"\n", name.c_str());
        fflush(stderr);
#endif
        log(fmt("execlp %s: %s\n", executable.c_str(), strerror(errno)));
        _exit(1); /* parent will notice EOF shortly */
    }

    /* don't need child's side of pipes */
    ::close(wp[0]);
    ::close(rp[1]);
    ::close(ep[1]);

    /* record pid, io channels, init lp and snoop list */
    setFds(rp[0], wp[1]);

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
    mp = new Msg();
    snprintf(buf, sizeof(buf), "<getProperties version='%g'/>\n", INDIV);
    mp->setFromStr(buf);
    pushMsg(mp);
    
    if (verbose > 0)
        log(fmt("pid=%d rfd=%d wfd=%d efd=%d\n", pid, rp[0], wp[1], ep[0]));
}

void RemoteDvrInfo::extractRemoteId(const std::string & name, std::string & o_host, int & o_port, std::string & o_dev) const
{
    char dev[MAXINDIDEVICE] = {0};
    char host[MAXSBUF] = {0};

    /* extract host and port from name*/
    int indi_port = INDIPORT;
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
    char buf[MAXSBUF] = {0};
    int sockfd;
    std::string dev;
    extractRemoteId(name, host, port, dev);
    
    /* connect */
    sockfd = openINDIServer();

    /* record flag pid, io channels, init lp and snoop list */

    this->setFds(sockfd, sockfd);

    /* N.B. storing name now is key to limiting outbound traffic to this
     * dev.
     */
    if (!dev.empty())
        this->dev.insert(dev);

    /* Sending getProperties with device lets remote server limit its
     * outbound (and our inbound) traffic on this socket to this device.
     */
    Msg *mp = new Msg();
    if (!dev.empty())
        sprintf(buf, "<getProperties device='%s' version='%g'/>\n", dev.c_str(), INDIV);
    else
        // This informs downstream server that it is connecting to an upstream server
        // and not a regular client. The difference is in how it treats snooping properties
        // among properties.
        sprintf(buf, "<getProperties device='*' version='%g'/>\n", INDIV);
    mp->setFromStr(buf);
    pushMsg(mp);

    if (verbose > 0)
        log(fmt("socket=%d\n", sockfd));
}

/* open a connection to the given host and port or die.
 * return socket fd.
 */
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

TcpServer::TcpServer(int port): port(port)
{
    sfdev.set<TcpServer, &TcpServer::ioCb>(this);
}

void TcpServer::ioCb(ev::io &, int revents)
{
    if (revents & EV_ERROR) {
        log("Error on socket\n");
        Bye();        
    }
    if (revents & EV_READ) {
        accept();
    }
}

/* create the public INDI Driver endpoint lsocket on port.
 * return server socket else exit.
 */
void TcpServer::listen()
{
    struct sockaddr_in serv_socket;
    int reuse = 1;

    /* make socket endpoint */
    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        log(fmt("socket: %s\n", strerror(errno)));
        Bye();
    }

    /* bind to given port for any IP address */
    memset(&serv_socket, 0, sizeof(serv_socket));
    serv_socket.sin_family = AF_INET;
#ifdef SSH_TUNNEL
    serv_socket.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#else
    serv_socket.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
    serv_socket.sin_port = htons((unsigned short)port);
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        log(fmt("setsockopt: %s\n", strerror(errno)));
        Bye();
    }
    if (bind(sfd, (struct sockaddr *)&serv_socket, sizeof(serv_socket)) < 0)
    {
        log(fmt("bind: %s\n", strerror(errno)));
        Bye();
    }

    /* willing to accept connections with a backlog of 5 pending */
    if (::listen(sfd, 5) < 0)
    {
        log(fmt("listen: %s\n", strerror(errno)));
        Bye();
    }

    fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK);
    sfdev.start(sfd, EV_READ);

    /* ok */
    if (verbose > 0)
        log(fmt("listening to port %d on fd %d\n", port, sfd));
}

void TcpServer::accept()
{
    struct sockaddr_in cli_socket;
    socklen_t cli_len;
    int cli_fd;

    /* get a private connection to new client */
    cli_len = sizeof(cli_socket);
    cli_fd  = ::accept(sfd, (struct sockaddr *)&cli_socket, &cli_len);
    if (cli_fd < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;

        log(fmt("accept: %s\n", strerror(errno)));
        Bye();
    }

    ClInfo * cp = new ClInfo();

    /* rig up new clinfo entry */
    cp->setFds(cli_fd, cli_fd);

    if (verbose > 0)
    {
        cp->log(fmt("new arrival from %s:%d - welcome!\n",
                inet_ntoa(cli_socket.sin_addr), ntohs(cli_socket.sin_port)));
    }
#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "CLIENTS %d\n", clients.size());
    fflush(stderr);
#endif
}

Fifo::Fifo(const std::string & name) : name(name)
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


/** Handle one fifo command. Start/stop drivers accordingly */
void Fifo::processLine(const char * line)
{

    if (verbose)
        log(fmt("FIFO: %s\n", line));

    char cmd[MAXSBUF], arg[4][1], var[4][MAXSBUF], tDriver[MAXSBUF], tName[MAXSBUF], envConfig[MAXSBUF],
            envSkel[MAXSBUF], envPrefix[MAXSBUF];

    memset(&tDriver[0], 0, sizeof(char) * MAXSBUF);
    memset(&tName[0], 0, sizeof(char) * MAXSBUF);
    memset(&envConfig[0], 0, sizeof(char) * MAXSBUF);
    memset(&envSkel[0], 0, sizeof(char) * MAXSBUF);
    memset(&envPrefix[0], 0, sizeof(char) * MAXSBUF);

    int n = 0;

    bool remoteDriver = !!strstr(line, "@");

    // If remote driver
    if (remoteDriver)
    {
        n = sscanf(line, "%s %512[^\n]", cmd, tDriver);

        // Remove quotes if any
        char *ptr = tDriver;
        int len   = strlen(tDriver);
        while ((ptr = strstr(tDriver, "\"")))
        {
            memmove(ptr, ptr + 1, --len);
            ptr[len] = '\0';
        }

        //fprintf(stderr, "Remote Driver: %s\n", tDriver);
    }
    // If local driver
    else
    {
        n = sscanf(line, "%s %s -%1c \"%512[^\"]\" -%1c \"%512[^\"]\" -%1c \"%512[^\"]\" -%1c \"%512[^\"]\"", cmd,
                    tDriver, arg[0], var[0], arg[1], var[1], arg[2], var[2], arg[3], var[3]);
    }

    int n_args = (n - 2) / 2;

    int j = 0;
    for (j = 0; j < n_args; j++)
    {
        //fprintf(stderr, "arg[%d]: %c\n", i, arg[j][0]);
        //fprintf(stderr, "var[%d]: %s\n", i, var[j]);

        if (arg[j][0] == 'n')
        {
            strncpy(tName, var[j], MAXSBUF - 1);
            tName[MAXSBUF - 1] = '\0';

            if (verbose)
                log(fmt("With name: %s\n", tName));
        }
        else if (arg[j][0] == 'c')
        {
            strncpy(envConfig, var[j], MAXSBUF - 1);
            envConfig[MAXSBUF - 1] = '\0';

            if (verbose)
                log(fmt("With config: %s\n", envConfig));
        }
        else if (arg[j][0] == 's')
        {
            strncpy(envSkel, var[j], MAXSBUF - 1);
            envSkel[MAXSBUF - 1] = '\0';

            if (verbose)
                log(fmt("With skeketon: %s\n", envSkel));
        }
        else if (arg[j][0] == 'p')
        {
            strncpy(envPrefix, var[j], MAXSBUF - 1);
            envPrefix[MAXSBUF - 1] = '\0';

            if (verbose)
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
        if (verbose)
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
        for (auto dp : drivers)
        {
            log(fmt("dp->name: %s - tDriver: %s\n", dp->name.c_str(), tDriver));
            if (dp->name == tDriver)
            {
                log(fmt("name: %s - dp->dev[0]: %s\n", tName, dp->dev.empty() ? "" : dp->dev.begin()->c_str()));

                /* If device name is given, check against it before shutting down */
                //if (tName[0] && strcmp(dp->dev[0], tName))
                if (tName[0] && !dp->isHandlingDevice(tName))
                    continue;
                if (verbose)
                    log(fmt("FIFO: Shutting down driver: %s\n", tDriver));

                //                    for (i = 0; i < dp->ndev; i++)
                //                    {
                //                        /* Inform clients that this driver is dead */
                //                        XMLEle *root = addXMLEle(NULL, "delProperty");
                //                        addXMLAtt(root, "device", dp->dev[i]);

                //                        prXMLEle(stderr, root, 0);
                //                        Msg *mp = newMsg();

                //                        q2Clients(NULL, 0, dp->dev[i], NULL, mp, root);
                //                        if (mp->count > 0)
                //                            setMsgXMLEle(mp, root);
                //                        else
                //                            freeMsg(mp);
                //                        delXMLEle(root);
                //                    }
                dp->restart = false;
                dp->close();
                break;
            }
        }
    }

}

/* Read commands from FIFO and process them. Start/stop drivers accordingly */
void Fifo::read(void)
{
    int rd = ::read(fd, buffer + bufferPos, sizeof(buffer) - 1 - bufferPos);
    if (rd == 0) {
        if (bufferPos > 0)
        {
            buffer[bufferPos] = '\0';
            processLine(buffer);
        }
        close();
        open();
        return;
    }
    if (rd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;

        log(fmt("Fifo error: %s\n", strerror(errno)));
        close();
        open();
        return;
    }

    bufferPos += rd;

    for(int i = 0; i < bufferPos; ++i) {
        if (buffer[i] == '\n') {
            buffer[i] = 0;
            processLine(buffer);
            // shift the buffer
            i++;                   /* count including nl */
            bufferPos -= i;        /* remove from nexbuf */
            memmove(buffer, buffer + i, bufferPos); /* slide remaining to front */
            i = -1;                /* restart for loop scan */
        }
    }

    if ((unsigned)bufferPos >= sizeof(buffer) - 1) {
        log(fmt("Fifo overflow"));
        close();
        open();
    }
}

void Fifo::ioCb(ev::io &, int revents)
{
    if (EV_ERROR & revents) {
        log("got invalid event on fifo");
        close();
        open();
    }
    else if (revents & EV_READ) {
        read();
    }
}

/* read more from the given client, send to each appropriate driver when see
 * xml closure. also send all newXXX() to all other interested clients.
 * return -1 if had to shut down anything, else 0.
 */
void ClInfo::onMessage(XMLEle * root)
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

    /* build a new message -- set content iff anyone cares */
    Msg* mp = new Msg();

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

    /* set message content if anyone cares else forget it */
    if (mp->count > 0)
        mp->setFromXMLEle(root);
    else
        delete mp;
}

/* read more from the given driver, send to each interested client when see
 * xml closure. if driver dies, try restarting.
 * return 0 if ok else -1 if had to shut down anything.
 */
void DvrInfo::onMessage(XMLEle * root)
{
    char *roottag    = tagXMLEle(root);
    const char *dev  = findXMLAttValu(root, "device");
    const char *name = findXMLAttValu(root, "name");
    int isblob       = !strcmp(tagXMLEle(root), "setBLOBVector");

    if (verbose > 2)
        traceMsg("read ", root);
    else if (verbose > 1)
    {
        log(fmt("read <%s device='%s' name='%s'>\n",
                tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name")));
    }

    /* that's all if driver is just registering a snoop */
    /* JM 2016-05-18: Send getProperties to upstream chained servers as well.*/
    if (!strcmp(roottag, "getProperties"))
    {
        this->addSDevice(dev, name);
        Msg *mp = new Msg();
        /* send to interested chained servers upstream */
        ClInfo::q2Servers(this, mp, root);
        /* Send to snooped drivers if they exist so that they can echo back the snooped propertly immediately */
        q2RDrivers(dev, mp, root);

        if (mp->count > 0)
            mp->setFromXMLEle(root);
        else
            delete(mp);
        return;
    }

    /* that's all if driver desires to snoop BLOBs from other drivers */
    if (!strcmp(roottag, "enableBLOB"))
    {
        Property *sp = findSDevice(dev, name);
        if (sp)
            crackBLOB(pcdataXMLEle(root), &sp->blob);
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
    if (ldir)
        logDMsg(root, dev);

    /* build a new message -- set content iff anyone cares */
    Msg * mp = new Msg();

    /* send to interested clients */
    ClInfo::q2Clients(NULL, isblob, dev, name, mp, root);

    /* send to snooping drivers */
    DvrInfo::q2SDrivers(this, isblob, dev, name, mp, root);

    /* set message content if anyone cares else forget it */
    if (mp->count > 0)
        mp->setFromXMLEle(root);
    else
        delete mp;
}

/* close down the given client */
void ClInfo::close()
{
    if (verbose > 0)
        log("shut down complete - bye!\n");

    delete(this);

#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "CLIENTS %d\n", clients.size());
    fflush(stderr);
#endif
}

/* close down the given driver and restart if set*/
void DvrInfo::close()
{
    // Tell client driver is dead.
    for (auto dev : dev)
    {
        /* Inform clients that this driver is dead */
        XMLEle *root = addXMLEle(NULL, "delProperty");
        addXMLAtt(root, "device", dev.c_str());

        prXMLEle(stderr, root, 0);
        Msg *mp = new Msg();

        ClInfo::q2Clients(NULL, 0, dev.c_str(), NULL, mp, root);
        if (mp->count > 0)
            mp->setFromXMLEle(root);
        else
            delete mp;
        delXMLEle(root);
    }

    bool terminate;
    if (!restart) {
        terminate = true;
    } else {
        if (restarts >= maxrestarts) {
            log(fmt("Terminated after #%d restarts.\n", restarts));

            terminate = true;
        }
        else
        {
            log(fmt("restart #%d\n", ++restarts));
        }
    }

#ifdef OSX_EMBEDED_MODE
    fprintf(stderr, "STOPPED \"%s\"\n", name.c_str());
    fflush(stderr);
#endif

    // FIXME: we loose stderr from dying driver
    if (terminate) {
        delete(this);
        if ((!fifo) && (drivers.size() == 0))
            Bye();
        return;
    } else {
        DvrInfo * restarted = this->clone();
        delete(this);
        restarted->start();
    }
}

/* put Msg mp on queue of each driver responsible for dev, or all drivers
 * if dev not specified.
 */
void DvrInfo::q2RDrivers(const std::string & dev, Msg *mp, XMLEle *root)
{
    char *roottag = tagXMLEle(root);

    /* queue message to each interested driver.
     * N.B. don't send generic getProps to more than one remote driver,
     *   otherwise they all fan out and we get multiple responses back.
     */
    std::set<std::string> remoteAdvertised;
    for (auto dp : drivers)
    {
        std::string remoteUid = dp->remoteServerUid();
        bool isRemote = !remoteUid.empty();

        /* driver known to not support this dev */
        if ((!dev.empty()) && dev[0] != '*' && !dp->isHandlingDevice(dev))
            continue;

        /* Only send message to each *unique* remote driver at a particular host:port
         * Since it will be propogated to all other devices there */
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
        dp->pushMsg(mp);
        if (verbose > 1)
        {
            dp->log(fmt("queuing responsible for <%s device='%s' name='%s'>\n",
                    tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name")));
        }
    }
}

void DvrInfo::q2SDrivers(DvrInfo *me, int isblob, const std::string & dev, const std::string & name, Msg *mp, XMLEle *root)
{
    std::string meRemoteServerUid = me ? me->remoteServerUid() : "";
    for (auto dp : drivers)
    {
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
        dp->pushMsg(mp);
        if (verbose > 1)
        {
            dp->log(fmt("queuing snooped <%s device='%s' name='%s'>\n",
                    tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name")));
        }
    }
}

void DvrInfo::addSDevice(const std::string & dev, const std::string & name)
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

    if (verbose)
        log(fmt("snooping on %s.%s\n", dev.c_str(), name.c_str()));
}

Property * DvrInfo::findSDevice(const std::string & dev, const std::string & name) const
{
    for(auto sp : sprops) 
    {
        if ((sp->dev == dev) && (sp->name.empty() || sp->name == name))
            return (sp);
    }

    return nullptr;
}

/* put Msg mp on queue of each client interested in dev/name, except notme.
 * if BLOB always honor current mode.
 * return -1 if had to shut down any clients, else 0.
 */
void ClInfo::q2Clients(ClInfo *notme, int isblob, const std::string & dev, const std::string & name, Msg *mp, XMLEle *root)
{
    /* queue message to each interested client */
    for (auto cp : clients)
    {
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
        if (isblob && maxstreamsiz > 0 && ql > maxstreamsiz)
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
                if (verbose > 1)
                    cp->log(fmt("%ld bytes behind. Dropping stream BLOB...\n", ql));
                continue;
            }
        }
        if (ql > maxqsiz)
        {
            if (verbose)
                cp->log(fmt("%ld bytes behind, shutting down\n", ql));
            cp->close();
            continue;
        }

        /* ok: queue message to this client */
        cp->pushMsg(mp);
        if (verbose > 1)
            cp->log(fmt("queuing <%s device='%s' name='%s'>\n",
                    tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name")));
    }

    return;
}

void ClInfo::q2Servers(DvrInfo *me, Msg *mp, XMLEle *root)
{
    int devFound = 0;

    /* queue message to each interested client */
    //FIXME: this will break if clients is modified during iteration.
    for (auto cp : clients)
    {
        // Only send the message to the upstream server that is connected specfically to the device in driver dp
        switch (cp->allprops)
        {
            // 0 --> not all props are requested. Check for specific combination
            case 0:
                for (auto pp : cp->props)
                {
                    if (me->dev.find(pp->dev) != me->dev.end()) {
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
        if (ql > maxqsiz)
        {
            if (verbose)
                cp->log(fmt("%ld bytes behind, shutting down\n", ql));
            cp->close();
            continue;
        }

        /* ok: queue message to this client */
        cp->pushMsg(mp);
        if (verbose > 1)
            cp->log(fmt("queuing <%s device='%s' name='%s'>\n",
                    tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name")));
    }
}


/* write the next chunk of the current message in the queue to the given
 * client. pop message from queue when complete and free the message if we are
 * the last one to use it. shut down this client if trouble.
 * N.B. we assume we will never be called with cp->msgq empty.
 * return 0 if ok else -1 if had to shut down.
 */
void MsgQueue::writeToFd() {
    ssize_t nsend, nw;
    Msg *mp;

    /* get current message */
    mp = headMsg();
    if (mp == nullptr) {
        log("Unexpected write notification");
        return;
    }

    /* send next chunk, never more than MAXWSIZ to reduce blocking */
    nsend = mp->cl - nsent;
    if (nsend > MAXWSIZ)
        nsend = MAXWSIZ;
    nw = write(wFd, &mp->cp[nsent], nsend);

    /* shut down if trouble */
    if (nw <= 0)
    {
        if (nw == 0)
            log("write returned 0\n");
        else
            log(fmt("write: %s\n", strerror(errno)));
        close();
        return;
    }

    /* trace */
    if (verbose > 2)
    {
        log(fmt("sending msg copy %d nq %ld:\n%.*s\n", mp->count,
                msgq.size(), (int)nw, &mp->cp[nsent]));
    }
    else if (verbose > 1)
    {
        log(fmt("sending %.50s\n", &mp->cp[nsent]));
    }

    /* update amount sent. when complete: free message if we are the last
     * to use it and pop from our queue.
     */
    nsent += nw;
    if (nsent == mp->cl)
        consumeHeadMsg();
}

int ClInfo::findDevice(const std::string & dev, const std::string & name) const
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

void ClInfo::addDevice(const std::string & dev, const std::string & name, int isblob)
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


/* convert the string value of enableBLOB to our B_ state value.
 * no change if unrecognized
 */
void MsgQueue::crackBLOB(const char *enableBLOB, BLOBHandling *bp)
{
    if (!strcmp(enableBLOB, "Also"))
        *bp = B_ALSO;
    else if (!strcmp(enableBLOB, "Only"))
        *bp = B_ONLY;
    else if (!strcmp(enableBLOB, "Never"))
        *bp = B_NEVER;
}

/* Update the client property BLOB handling policy */
void ClInfo::crackBLOBHandling(const std::string & dev, const std::string & name, const char *enableBLOB)
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

/* print key attributes and values of the given xml to stderr.
 */
void MsgQueue::traceMsg(const std::string & logMsg, XMLEle *root)
{
    log(logMsg);

    static const char *prtags[] =
    {
        "defNumber", "oneNumber", "defText", "oneText", "defSwitch", "oneSwitch", "defLight", "oneLight",
    };
    XMLEle *e;
    const char *msg, *perm, *pcd;
    unsigned int i;

    /* print tag header */
    fprintf(stderr, "%s %s %s %s", tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name"),
            findXMLAttValu(root, "state"));
    pcd = pcdataXMLEle(root);
    if (pcd[0])
        fprintf(stderr, " %s", pcd);
    perm = findXMLAttValu(root, "perm");
    if (perm[0])
        fprintf(stderr, " %s", perm);
    msg = findXMLAttValu(root, "message");
    if (msg[0])
        fprintf(stderr, " '%s'", msg);

    /* print each array value */
    for (e = nextXMLEle(root, 1); e; e = nextXMLEle(root, 0))
        for (i = 0; i < sizeof(prtags) / sizeof(prtags[0]); i++)
            if (strcmp(prtags[i], tagXMLEle(e)) == 0)
                fprintf(stderr, "\n %10s='%s'", findXMLAttValu(e, "name"), pcdataXMLEle(e));

    fprintf(stderr, "\n");
}

/* fill s with current UT string.
 * if no s, use a static buffer
 * return s or buffer.
 * N.B. if use our buffer, be sure to use before calling again
 */
static char *indi_tstamp(char *s)
{
    static char sbuf[64];
    struct tm *tp;
    time_t t;

    time(&t);
    tp = gmtime(&t);
    if (!s)
        s = sbuf;
    strftime(s, sizeof(sbuf), "%Y-%m-%dT%H:%M:%S", tp);
    return (s);
}

/* log message in root known to be from device dev to ldir, if any.
 */
static void logDMsg(XMLEle *root, const char *dev)
{
    char stamp[64];
    char logfn[1024];
    const char *ts, *ms;
    FILE *fp;

    /* get message, if any */
    ms = findXMLAttValu(root, "message");
    if (!ms[0])
        return;

    /* get timestamp now if not provided */
    ts = findXMLAttValu(root, "timestamp");
    if (!ts[0])
    {
        indi_tstamp(stamp);
        ts = stamp;
    }

    /* append to log file, name is date portion of time stamp */
    sprintf(logfn, "%s/%.10s.islog", ldir, ts);
    fp = fopen(logfn, "a");
    if (!fp)
        return; /* oh well */
    fprintf(fp, "%s: %s: %s\n", ts, dev, ms);
    fclose(fp);
}

/* log when then exit */
static void Bye()
{
    fprintf(stderr, "%s: good bye\n", indi_tstamp(NULL));
    exit(1);
}

DvrInfo::DvrInfo() {
    drivers.insert(this);
}

DvrInfo::DvrInfo(const DvrInfo & model):
    name(model.name),
    restarts(model.restarts)
{}

DvrInfo::~DvrInfo() {
    drivers.erase(this);
    for(auto prop : sprops) {
        delete prop;
    }
}

bool DvrInfo::isHandlingDevice(const std::string & dev) const {
    return this->dev.find(dev) != this->dev.end();
}

void DvrInfo::log(const std::string & str) const {
    std::string logLine = "Driver ";
    logLine += name;
    logLine += ": ";
    logLine += str;
    ::log(logLine);
}

LocalDvrInfo::LocalDvrInfo() {
    eio.set<LocalDvrInfo, &LocalDvrInfo::onEfdEvent>(this);
    pidwatcher.set<LocalDvrInfo, &LocalDvrInfo::onPidEvent>(this);
}

LocalDvrInfo::LocalDvrInfo(const LocalDvrInfo & model):
    DvrInfo(model),
    envDev(model.envDev),
    envConfig(model.envConfig),
    envSkel(model.envSkel),
    envPrefix(model.envPrefix)
{}


LocalDvrInfo::~LocalDvrInfo() {
    closeEfd();
    if (pid != 0)
    {
        kill(pid, SIGKILL); /* we've insured there are no zombies */
        pid = 0;
    }
    closePid();
}

LocalDvrInfo * LocalDvrInfo::clone() const {
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
    if (EV_ERROR & revents) {
        log("got invalid event on stderr");
        closeEfd();
        return;
    }

    if (revents & EV_READ) {
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
    if (revents & EV_CHILD) {
        if (WIFEXITED(pidwatcher.rstatus)) {
            log(fmt("process %d exited with status %d\n", pid, WEXITSTATUS(pidwatcher.rstatus)));
        } else if (WIFSIGNALED(pidwatcher.rstatus)) {
            int signum = WTERMSIG(pidwatcher.rstatus);
            log(fmt("process %d killed with signal %d - %s\n", pid, signum, strsignal(signum)));
        }
        pid = 0;
        this->pidwatcher.stop();
    }
}

RemoteDvrInfo::RemoteDvrInfo()
{}

RemoteDvrInfo::RemoteDvrInfo(const RemoteDvrInfo & model):
    DvrInfo(model),
    host(model.host),
    port(model.port)
{}

RemoteDvrInfo::~RemoteDvrInfo()
{}

RemoteDvrInfo * RemoteDvrInfo::clone() const {
    return new RemoteDvrInfo(*this);
}

ClInfo::ClInfo() {
    clients.insert(this);
}

ClInfo::~ClInfo() {
    for(auto prop : props) {
        delete prop;
    }
    
    clients.erase(this);
}

void ClInfo::log(const std::string & str) const {
    std::string logLine = fmt("Client %d: ", this->getRFd());
    logLine += str;
    ::log(logLine);
}

Msg::Msg(void): count(0), cl(0), cp(nullptr)
{
}

Msg::~Msg() {
    if (cp) {
        free(cp);
    }
}

void Msg::alloc(unsigned long cl) {
    if (cp) {
        free(cp);
        cp = nullptr;
    }
    cp = (char*)malloc(cl);
    this->cl = cl; 
}

void Msg::setFromStr(const char * str)
{
    alloc(strlen(str));
    memcpy(cp, str, cl);
}

void Msg::setFromXMLEle(XMLEle *root)
{
    /* want cl to only count content, but need room for final \0 */
    alloc(sprlXMLEle(root, 0) + 1);
    sprXMLEle(cp, root, 0);

    /* Drop the last zero */
    cl--;
}

MsgQueue::MsgQueue(): nsent(0) {
    lp = newLilXML();
    rio.set<MsgQueue, &MsgQueue::ioCb>(this);
    wio.set<MsgQueue, &MsgQueue::ioCb>(this);
    rFd = -1;
    wFd = -1;
}

MsgQueue::~MsgQueue() {
    rio.stop();
    wio.stop();

    clearMsgQueue();
    delLilXML(lp);
    lp = nullptr;

    setFds(-1, -1);

    /* decrement and possibly free any unsent messages for this client */
    for(auto mp : msgq)
        unrefMsg(mp);
}

void MsgQueue::setFds(int rFd, int wFd)
{
    if (this->rFd != -1) {
        rio.stop();
        wio.stop();
        ::close(rFd);
        if (rFd != wFd) {
            ::close(wFd);
        }
    }

    this->rFd = rFd;
    this->wFd = wFd;
    this->nsent = 0;
    
    if (rFd != -1) {
        fcntl(rFd, F_SETFL, fcntl(rFd, F_GETFL, 0) | O_NONBLOCK); 
        if (wFd != rFd) {
            fcntl(wFd, F_SETFL, fcntl(wFd, F_GETFL, 0) | O_NONBLOCK); 
        }
        
        rio.set(rFd, ev::READ);
        wio.set(wFd, ev::WRITE);
        rio.start();
    }
}

void MsgQueue::unrefMsg(Msg * mp) {
    if (--mp->count == 0)
        delete(mp);
}

Msg * MsgQueue::headMsg() const {
    if (msgq.empty()) return nullptr;
    return *(msgq.begin());
}

void MsgQueue::consumeHeadMsg() {
    unrefMsg(headMsg());
    msgq.pop_front();
    nsent = 0;
    if (msgq.empty()) {
        wio.stop();
    }
}

void MsgQueue::pushMsg(Msg * mp) {
    mp->count++;
    msgq.push_back(mp);
    // Register for client write
    if (wFd != -1) wio.start();
}

void MsgQueue::clearMsgQueue() {
    for(auto mp: msgq) {
        unrefMsg(mp);
    }
    msgq.clear();

    nsent = 0;
    // Cancel io write events
    wio.stop();
}

unsigned long MsgQueue::msgQSize() const {
    unsigned long l = 0;

    for (auto mp : msgq)
    {
        l += sizeof(Msg);
        l += mp->cl;
    }

    return (l);
}

void MsgQueue::ioCb(ev::io &, int revents)
{
    if (EV_ERROR & revents) {
        log("got invalid event");
        close();
        return;
    }

    if (revents & EV_READ) 
        readFromFd();

    if (revents & EV_WRITE) 
        writeToFd();
}

void MsgQueue::readFromFd()
{
    char buf[MAXRBUF];
    ssize_t nr;

    /* read client */
    nr = read(rFd, buf, sizeof(buf));
    if (nr <= 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        
        if (nr < 0)
            log(fmt("read: %s\n", strerror(errno)));
        else if (verbose > 0)
            log(fmt("read EOF\n"));
        close();
        return;
    }

    /* process XML chunk */
    char err[1024];
    XMLEle **nodes = parseXMLChunk(lp, buf, nr, err);
    if (!nodes)
    {
        log(fmt("XML error: %s\n", err));
        log(fmt("XML read: %.*s\n", (int)nr, buf));
        close();
        return;
    }

    int inode = 0;

    XMLEle *root = nodes[inode];
    PeerRef<MsgQueue> ref(this);

    while (root)
    {
        if (ref.alive()) {
            if (verbose > 2)
                traceMsg("read ", root);
            else if (verbose > 1)
            {
                log(fmt("read <%s device='%s' name='%s'>\n",
                        tagXMLEle(root), findXMLAttValu(root, "device"), findXMLAttValu(root, "name")));
            }

            onMessage(root);
        }
        delXMLEle(root);
        inode++;
        root = nodes[inode];
    }

    free(nodes);
}

static void log(const std::string & log) {
    fprintf(stderr, "%s: ", indi_tstamp(NULL));
    fprintf(stderr, "%s", log.c_str());
}

static std::string fmt(const char *fmt, ...)
{
    char buffer[128];
    int size = sizeof(buffer);
    char *p = buffer;
    va_list ap;

    /* Determine required size */
    va_start(ap, fmt);
    size = vsnprintf(p, size, fmt, ap);
    va_end(ap);

    if (size < 0) {
        perror("vnsprintf");
    }

    if ((unsigned)size < sizeof(buffer)) {
        return std::string(buffer);
    }

    size++;             /* For '\0' */
    p = (char*)malloc(size);
    if (p == NULL) {
        perror("malloc");
        Bye();
    }

    va_start(ap, fmt);
    size = vsnprintf(p, size, fmt, ap);
    va_end(ap);

    if (size < 0) {
        free(p);
        perror("vnsprintf");
    }
    std::string ret(p);
    free(p);
    return ret;
}

void RawPeerRef::clear() {
    if (value == nullptr) {
        return;
    }
    if (prev) {
        prev->next = next;
    } else {
        value->first = next;
    }
    if (next) {
        next->prev = prev;
    } else {
        value->last = prev;
    }
    prev = nullptr;
    next = nullptr;
    value = nullptr;
}

RawPeerRef::RawPeerRef(Peer * p) {
    value = p;
    next = nullptr;
    prev = p->last;
    if (p->last) {
        p->last->next = this;
    } else {
        p->first = this;
    }
    p->last = this;
}

RawPeerRef::~RawPeerRef() {
    clear();
}